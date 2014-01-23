/*
 * Copyright (C) 2015 Texas Instruments
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "AudioLoopback"
// #define LOG_NDEBUG 0

#include <cutils/log.h>
#include <cutils/properties.h>
#include <system/audio.h>
#include <media/AudioBufferProvider.h>

#include "AudioLoopback.h"

namespace android {

AudioLoopback::AudioLoopback(audio_source_t sourceType, uint32_t channelConfig,
                             const String16& opPackageName, callback_t callback,
                             void* user) :
    mSourceType(sourceType), mStreamType(AUDIO_STREAM_MUSIC),
    mOpPackageName(opPackageName), mFormat(AUDIO_FORMAT_PCM_16_BIT),
    mRunning(false), mRecordWarm(false), mPrefillBuffer(NULL),
    mWaitingForStopLock(false)
{
    if (!audio_is_output_channel(channelConfig)) {
        ALOGE("AudioLoopback() invalid channel mask %#x", channelConfig);
        mStatus = BAD_VALUE;
        return;
    }

    mCbf = callback;
    mUserData = user;

    mOutChannels = channelConfig;

    if (mOutChannels == AUDIO_CHANNEL_OUT_STEREO) {
        mInChannels = AUDIO_CHANNEL_IN_STEREO;
    } else if (mOutChannels == AUDIO_CHANNEL_OUT_MONO) {
        mInChannels = AUDIO_CHANNEL_IN_MONO;
    } else {
        // only support mono and stereo for now
        ALOGE("AudioLoopback() only mono and stereo channel config supported");
        mStatus = BAD_VALUE;
        return;
    }

    mInFrameSize = popcount(mInChannels) * audio_bytes_per_sample(mFormat);
    mOutFrameSize = popcount(mOutChannels) * audio_bytes_per_sample(mFormat);

    mStatus = AudioSystem::getOutputSamplingRate(&mRate, mStreamType);
    if (mStatus != NO_ERROR) {
        ALOGE("AudioLoopback() failed to get native output sampling rate");
        return;
    }

    mStatus = AudioRecord::getMinFrameCount(&mInMinFrameCount, mRate,
                                            mFormat, mInChannels);
    if (mStatus != NO_ERROR) {
        ALOGE("AudioLoopback() failed to get AudioRecord min frame count");
        return;
    }

    mStatus = AudioTrack::getMinFrameCount(&mOutMinFrameCount, mStreamType, mRate);
    if (mStatus != NO_ERROR) {
        ALOGE("AudioLoopback() failed to get AudioTrack min frame count");
        return;
    }

    ALOGI("AudioLoopback() AudioRecord %d min frames", mInMinFrameCount);
    ALOGI("AudioLoopback() AudioTrack %d min frames", mOutMinFrameCount);

    mStatus = createTrack();
    if (mStatus != NO_ERROR) {
        ALOGE("AudioLoopback() failed to create the audio track");
        return;
    }

    mStatus = createRecord();
    if (mStatus != NO_ERROR) {
        ALOGE("AudioLoopback() failed to create the audio record");
        return;
    }
}

AudioLoopback::~AudioLoopback()
{
    if (mRunning) {
        stop();
    }

    if (mRecord != NULL) {
        mRecord.clear();
    }

    if (mTrack != NULL) {
        mTrack.clear();
    }

    if (mPrefillBuffer != NULL) {
        delete [] mPrefillBuffer;
    }
}

status_t AudioLoopback::setVolume(float volume)
{
    if (mStatus != NO_ERROR) {
        return mStatus;
    }

    return mTrack->setVolume(volume);
}

status_t AudioLoopback::start()
{
    if (mStatus != NO_ERROR) {
        return mStatus;
    }

    AutoMutex lock(mLock);
    if (mRunning) {
        ALOGE("start() loopback is already running");
        return INVALID_OPERATION;
    }

    ALOGI("start() start the audio loopback");

    mRunning = true;
    mRecovering = false;
    mTrackSteady = false;
    mFramesWritten = 0;
    mFramesRead = 0;

    status_t status = mTrack->start();
    if (status != NO_ERROR) {
        ALOGE("start() failed to start the AudioTrack");
        goto err_track;
    }

    status = prefillTrack();
    if (status != NO_ERROR) {
        ALOGE("start() failed to prefill the AudioTrack");
        goto err_prefill;
    }

    status = mRecord->start();
    if (status != NO_ERROR) {
        ALOGE("start() failed to start the AudioRecord");
        goto err_prefill;
    }

    ALOGV("start() audio loopback has started");

    return NO_ERROR;

 err_prefill:
    mTrack->stop();
 err_track:
    mRunning = false;
    return status;
}

void AudioLoopback::stop()
{
    mWaitingForStopLock = true;
    AutoMutex lock(mLock);
    mWaitingForStopLock = false;

    if (!mRunning) {
        ALOGW("stop() loopback is not running");
        return;
    }

    ALOGI("stop() stop the audio loopback");

    mRecord->stop();

    // AudioTrack flush is handled until the next time the server obtains
    // a buffer. If the flush is handled when the loopback is already
    // re-started (e.g. after recovering from underruns), then the flush
    // itself could cause an additional underrun.
    // The server obtains the next buffer when an AudioHAL buffer has been
    // consumed, so use that size to give the server enough time to process
    // the flush operation.
    mTrack->pause();
    mTrack->stop();
    mTrack->flush();
    usleep((kAudioHALPeriodFrames * 1000000) / mRate);

    mRunning = false;

    ALOGV("stop() audio loopback has stopped");
}

int AudioLoopback::getSessionId() const
{
    return mTrack->getSessionId();
}

status_t AudioLoopback::createTrack()
{
    size_t reqFrames = kFastTrackFrames;
    audio_output_flags_t flags = AUDIO_OUTPUT_FLAG_FAST;

    while (true) {
        // AudioFlinger internally rounds up the requested frame count to a power
        // of 2. If the requested frames are not a power of 2, the requested
        // notification frames may not be honored in a single contiguous buffer,
        // so it is split into two MORE_DATA events.
        // In callback mode, MORE_DATA events could also be split if the notification
        // frames are not multiple of the AudioHAL's buffer.
        mTrack = new AudioTrack(mStreamType, mRate, mFormat, mOutChannels,
                                reqFrames, flags, trackCallback, this,
                                kPreferredNotificationFrames, 0,
                                AudioTrack::TRANSFER_SYNC);
        status_t status = mTrack->initCheck();
        if (status != NO_ERROR) {
            ALOGE("createTrack() failed to create the AudioTrack");
            return status;
        }

        mOutFrames = mTrack->frameCount();
        if (reqFrames != mOutFrames) {
            flags = AUDIO_OUTPUT_FLAG_NONE;
            reqFrames = mOutFrames; // It should succeed in the next iteration
            mTrack.clear();
        } else {
            break;
        }
    }

    ALOGW_IF(mOutFrames != reqFrames,
             "createTrack() AudioTrack got %u frames, requested %u",
             mOutFrames, reqFrames);

    ALOGI("createTrack() %s AudioTrack %u frames",
          (flags == AUDIO_OUTPUT_FLAG_FAST) ? "Fast" : "Normal",
          mOutFrames);

    // Temporary buffer to prefill data during transient state
    mPrefillBuffer = new char[mOutFrames * mOutFrameSize];
    memset(mPrefillBuffer, 0, mOutFrames * mOutFrameSize);

    return NO_ERROR;
}

status_t AudioLoopback::createRecord()
{
    uint32_t reqFrames;

    // The AudioTrack buffer size could be smaller than the AudioRecord's min buffer
    // size, in which case the AudioRecord restriction is honored.
    reqFrames = (mOutFrames > mInMinFrameCount) ? mOutFrames : mInMinFrameCount;
    reqFrames *= 2;

    mRecord = new AudioRecord(mSourceType, mRate, mFormat, mInChannels,
                              mOpPackageName, reqFrames, recordCallback,
                              this, kPreferredNotificationFrames, 0,
                              AudioRecord::TRANSFER_CALLBACK);
    status_t status = mRecord->initCheck();
    if (status != NO_ERROR) {
        ALOGE("createRecord() failed to create the AudioRecord");
        return status;
    }

    mInFrames = mRecord->frameCount();

    mNotificationFrames = kPreferredNotificationFrames;

    ALOGI("createRecord() AudioRecord %u frames, notifications %u frames",
          mInFrames, mNotificationFrames);

    ALOGW_IF(mInFrames != reqFrames,
             "createRecord() AudioRecord got %u frames, requested %u",
             mInFrames, reqFrames);

    ALOGW_IF(kPreferredNotificationFrames != mNotificationFrames,
             "createRecord() AudioRecord got %u notification frames, requested %u",
             mNotificationFrames, kPreferredNotificationFrames);

    // After the AudioRecord creation, it's in a cold state where the first callback
    // with new data (MORE_DATA event) takes longer to run than it does for subsequent
    // events. Since the AudioTrack's new data is written from the AudioRecord's
    // callback, a late callback execution might cause underruns in the AudioTrack even
    // if the track is prefilled at start().
    // Warming the AudioRecord up beforehand removes that first late callback execution.
    // The AudioRecord warm up should take less than 50 ms.
    mRecord->start();

    for (int i = 0; (i < kMaxRecordWarmUpRetry) && !mRecordWarm; i++) {
        usleep(10000);
    }

    if (!mRecordWarm) {
        ALOGW("createRecord() AudioRecord is not warm, transient xruns might occur");
        mRecordWarm = true;
    }

    mRecord->stop();

    return NO_ERROR;
}

void AudioLoopback::getRecordPosition(uint32_t &server,
                                      uint32_t &client,
                                      uint32_t &frames) const
{
    uint32_t s, c;

    // Server's number of frames recorded since start
    mRecord->getPosition(&s);

    // The AudioRecord position is updated when the server releases the buffer.
    // mFramesRead is updated when the read() completes, which occurs before
    // the server releases the buffer.
    // When reading more frames than the amount available in the AudioRecord
    // buffer, mFramesRead will be greater than the server's position until the
    // buffer is released. The frame count during that window has to be adjusted
    // to prevent invalid values.
    c = (s > mFramesRead) ? mFramesRead : s;

    // Server and client positions within the AudioRecord buffer
    frames = s - c;
    server = s % mInFrames;
    client = c % mInFrames;
}

void AudioLoopback::getTrackPosition(uint32_t &server,
                                     uint32_t &client,
                                     uint32_t &frames) const
{
    uint32_t s, c;

    // Server's number of frames played since start
    mTrack->getPosition(&s);

    c = mFramesWritten;

    // Server and client positions within the AudioTrack buffer
    frames = c - s;
    server = s % mOutFrames;
    client = c % mOutFrames;
}

status_t AudioLoopback::prefillTrack()
{
    if (mRecovering) {
        mTrack->stop();

        status_t status = mTrack->start();
        if (status != NO_ERROR) {
            ALOGE("prefillTrack() failed to start the AudioTrack");
            return status;
        }
    }

    // Prefill buffer has been zeroed earlier during track creation
    AudioRecord::Buffer buf;
    buf.raw = mPrefillBuffer;
    buf.frameCount = mNotificationFrames;
    buf.size = buf.frameCount * mTrack->frameSize();

    while (!mTrackSteady) {
        ALOGV("prefillTrack() write %u frames (non-blocking)", buf.frameCount);
        ssize_t bytes = mTrack->write(buf.raw, buf.size, false);
        if (bytes == WOULD_BLOCK) {
            ALOGV("prefillTrack() this write would block");
            mTrackSteady = true;
        } else if (bytes < 0) {
            ALOGE("prefillTrack() transaction failed");
            mFramesWritten = 0;
            return FAILED_TRANSACTION;
        } else {
            size_t written = bytes / mTrack->frameSize();
            if (written == buf.frameCount) {
                ALOGV("prefillTrack() wrote %d frames", buf.frameCount);
            } else {
                ALOGW("prefillTrack() only wrote %d of %d frames",
                      written, buf.frameCount);
            }
            mFramesWritten += written;
        }
    }

    return NO_ERROR;
}

status_t AudioLoopback::write(AudioRecord::Buffer *buffer)
{
    AutoMutex lock(mLock);
    if (!mRunning) {
        buffer->size = 0;
        return NO_ERROR;
    }

    if (!mTrackSteady || mRecovering) {
        status_t status = prefillTrack();
        if (status != NO_ERROR) {
            ALOGE("write() failed to prefill the AudioTrack");
            return status;
        }
        mRecovering = false;
    }

    ALOGW_IF(buffer->frameCount != mNotificationFrames,
             "write() AudioRecord got %u frames, expected %u",
             buffer->frameCount, mNotificationFrames);

    ALOGV("write() write %u frames (blocking)", buffer->frameCount);
    ssize_t bytes = mTrack->write(buffer->raw, buffer->size);
    if (bytes < 0) {
        ALOGE("write() transaction failed");
        return FAILED_TRANSACTION;
    }

    ALOGW_IF(bytes != ssize_t(buffer->size),
             "write() only wrote %d of %d bytes", bytes, buffer->size);
    mFramesWritten += bytes / mTrack->frameSize();

    return NO_ERROR;
}

void AudioLoopback::recordCallback(int event, void *user, void *info)
{
    AudioLoopback *loopback = (AudioLoopback *)user;

    if (!loopback) {
        ALOGE("recordCallback() invalid user data");
        return;
    }

    if (!loopback->mRecordWarm) {
        ALOGV("recordCallback() audio record is warm now");
        loopback->mRecordWarm = true;
        return;
    }

    if (!loopback->mRunning) {
        ALOGV("recordCallback() ignore event %d", event);
        return;
    }

    if (event == AudioTrack::EVENT_MORE_DATA) {
        AudioRecord::Buffer *buffer = (AudioRecord::Buffer *)info;
        if (loopback->write(buffer) != NO_ERROR) {
            ALOGE("recordCallback() failed to read AudioRecord");
            if (loopback->mCbf) {
                loopback->mCbf(AudioLoopback::EVENT_ERROR, loopback->mUserData, 0);
            }
        }

        loopback->mFramesRead += buffer->frameCount;

        if (loopback->mRunning && loopback->mWaitingForStopLock) {
            // sleep to make sure that the stop will get a chance to
            // get the lock and run
            usleep(1);
        }
    } else if (event == AudioRecord::EVENT_OVERRUN) {
        AutoMutex lock(loopback->mLock);
        if (loopback->mRecovering) {
            ALOGW("recordCallback() ignore transient underrun events while recovering");
            return;
        }

        // Apparently, due to an AudioFlinger limitation, overrun events may come late
        ALOGW("recordCallback() PCM buffer overrun");
        loopback->mRecovering = true;
        loopback->mTrackSteady = false;

        if (loopback->mCbf) {
            loopback->mCbf(AudioLoopback::EVENT_OVERRUN, loopback->mUserData, 0);
        }

    }
}

void AudioLoopback::trackCallback(int event, void *user, void *info __unused)
{
    AudioLoopback *loopback = (AudioLoopback *)user;

    if (!loopback) {
        ALOGE("trackCallback() invalid user data");
        return;
    }

    if (!loopback->mRunning) {
        ALOGV("trackCallback() ignore event %d", event);
        return;
    }

    if (event == AudioTrack::EVENT_UNDERRUN) {
        AutoMutex lock(loopback->mLock);
        if (loopback->mRecovering) {
            ALOGW("trackCallback() ignore transient underrun events while recovering");
            return;
        }

        ALOGW("trackCallback() PCM buffer underrun");
        loopback->mRecovering = true;
        loopback->mTrackSteady = false;

        if (loopback->mCbf) {
            loopback->mCbf(AudioLoopback::EVENT_UNDERRUN, loopback->mUserData, 0);
        }
    }
}

} // namespace android
