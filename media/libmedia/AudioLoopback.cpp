/*
 * Copyright (C) 2014 Texas Instruments
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
#include <system/audio.h>
#include <media/AudioBufferProvider.h>
#include <media/AudioLoopback.h>

namespace android {

AudioLoopback::AudioLoopback(audio_source_t sourceType, uint32_t channelConfig,
                             callback_t callback, void* user) :
    mRecord(NULL), mTrack(NULL), mPipe(NULL), mPipeReader(NULL),
    mFormat(AUDIO_FORMAT_PCM_16_BIT), mPipeFrames(0), mRunning(false)
{
    audio_stream_type_t streamType = AUDIO_STREAM_MUSIC;
    size_t inFrameCount;
    size_t outFrameCount;
    status_t status;

    if (!audio_is_output_channel(channelConfig)) {
        ALOGE("AudioLoopback() Invalid channel mask %#x", channelConfig);
        mStatus = BAD_VALUE;
        return;
    }

    mCbf = callback;
    mUserData = user;

    mOutChannels = channelConfig;

    if (mOutChannels == AUDIO_CHANNEL_OUT_STEREO) {
        mInChannels = AUDIO_CHANNEL_IN_STEREO;
    }
    else if (mOutChannels == AUDIO_CHANNEL_OUT_MONO) {
        mInChannels = AUDIO_CHANNEL_IN_MONO;
    }
    else {
        // only support mono and stereo for now
        ALOGE("AudioLoopback() only mono and stereo channel config supported");
        mStatus = BAD_VALUE;
        return;
    }

    mChannelCount = popcount(channelConfig);

    status = AudioSystem::getOutputSamplingRate(&mRate, streamType);
    if (status != NO_ERROR) {
        ALOGE("AudioLoopback() failed to get native output sampling rate");
        mStatus = status;
        return;
    }

    status = AudioRecord::getMinFrameCount(&inFrameCount, mRate, mFormat,
                                           mInChannels);
    if (status != NO_ERROR) {
        ALOGE("AudioLoopback() failed to get AudioRecord min frame count");
        mStatus = status;
        return;
    }

    status = AudioTrack::getMinFrameCount(&outFrameCount, streamType, mRate);
    if (status != NO_ERROR) {
        ALOGE("AudioLoopback() failed to get AudioTrack min frame count");
        mStatus = status;
        return;
    }

    inFrameCount *= 4;
    outFrameCount *= 2;

    ALOGV("AudioRecord %d min frames", inFrameCount);
    ALOGV("AudioTrack  %d min frames", outFrameCount);

    // AudioFlinger requests one buffer (needed to fill the hw buffer, composed
    // of outFrameCount frames), one period (step() called outside AF?) and one
    // more period each time a write to the hw completes. The AudioTrack callback
    // is called each time for those requests.
    // Before reaching steady state, write() calls to the audio hw will go through
    // right away until the hw buffer gets full (steady state is reached). This
    // will cause a total of data requests = 2 * buffer size + 1 * period size.
    // This condition can draw all the data in the pipe very quickly and possibly
    // causing an underrun if the pipe is not deep enough. In order to prevent
    // having the pipe too large, 0's are written into the AudioTrack until steady
    // state is reached (see setMarkerPosition() note)
    // This pipe size along with the head start delay in start() allows AudioRecord
    // to put up to 2.25 times outFrameCount, maintaining ~50% of pipe usage after
    // steady state is reached.
    mPipeFrames = (inFrameCount > outFrameCount) ? inFrameCount : outFrameCount;
    mPipeFrames *= 3;

    mRecord = new AudioRecord(sourceType, mRate, mFormat, mInChannels,
                              inFrameCount, AudioRecordCallback, this,
                              inFrameCount / 4, 0);
    mStatus = mRecord->initCheck();
    if (mStatus != NO_ERROR) {
        ALOGE("AudioLoopback() failed to create AudioRecord");
        return;
    }

    mTrack = new AudioTrack(streamType, mRate, mFormat, mOutChannels,
                            outFrameCount / 2, AUDIO_OUTPUT_FLAG_NONE,
                            AudioTrackCallback, this, outFrameCount / 4, 0);
    mStatus = mTrack->initCheck();
    if (mStatus != NO_ERROR) {
        ALOGE("AudioLoopback() failed to create AudioTrack");
        return;
    }

    // Used as steady state mark
    // AudioTrack sends the EVENT_MARKER callback when frameCount + markerPosition
    // has been reached, when marker position is set using
    // AudioTrack::setMarkerPosition. However, if start is called again after a
    // stop, the EVENT_MARKER callback will not come unless the marker position
    // is set again. And even if it is set again, the AudioTrack position is not
    // reset, so the EVENT_MARKER callback happens immediately. This does not
    // give sufficient time for the pipe to reach the steady state required. So,
    // instead of using the marker position callback to determine steady state,
    // keep track internally using mFramesRead and mMarkerPosition, setting
    // mMarkerPosition to the same value that it would take for the EVENT_MARKER
    // callback to happen if markerPosition were set to outFrameCount / 2.
    mMarkerPosition = outFrameCount;

    NBAIO_Format format = Format_from_SR_C(mRate, mChannelCount);
    NBAIO_Format offers[1] = {format};
    size_t numCounterOffers = 0;

    mPipe = new MonoPipe(mPipeFrames, format, false);
    ssize_t index = mPipe->negotiate(offers, 1, NULL, numCounterOffers);
    if (index != 0) {
        ALOGE("AudioLoopback() failed to negotiate MonoPipe parameters");
        mStatus = BAD_VALUE;
        return;
    }

    mPipeReader = new MonoPipeReader(mPipe);
    numCounterOffers = 0;
    index = mPipeReader->negotiate(offers, 1, NULL, numCounterOffers);
    if (index != 0) {
        ALOGE("AudioLoopback() failed to negotiate MonoPipeReader parameters");
        mStatus = BAD_VALUE;
    }
}

AudioLoopback::~AudioLoopback()
{
    if (mRecord) {
        if (!mRecord->stopped()) {
            mRecord->stop();
        }
        delete mRecord;
    }

    if (mTrack) {
        if (!mTrack->stopped()) {
            mTrack->stop();
        }
        mTrack->flush();
        delete mTrack;
    }

    if (mPipe) {
        delete mPipe;
    }

    if (mPipeReader) {
        delete mPipeReader;
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
    status_t ret;

    if (mStatus != NO_ERROR) {
        return mStatus;
    }

    AutoMutex lock(mLock);

    if (!mRunning) {
        ALOGI("start() loopback is running now");
        ret = mRecord->start();
        if (ret == NO_ERROR) {
            // Give record stream a head start over playback
            uint32_t delayMs = (mPipeFrames * 1000);
            delayMs /= (2 * mRate);
            usleep(delayMs * 1000);
            mSteady = false;
            mFramesRead = 0;
            mTrack->start();

            mRunning = true;
        }
    } else {
        ALOGE("start() loopback is already running");
        ret = INVALID_OPERATION;
    }

    return ret;
}

void AudioLoopback::stop()
{
    AutoMutex lock(mLock);

    if (mRunning) {
        ALOGI("stop() loopback is stopping now");
        mRecord->stop();
        mTrack->pause();
        mTrack->flush();
        flushPipe();
        mRunning = false;
    }
}

int AudioLoopback::getSessionId() const
{
    return mTrack->getSessionId();
}

uint32_t AudioLoopback::pipeUsage() const
{
    return (100 * mPipeReader->availableToRead()) / mPipeFrames;
}

status_t AudioLoopback::writeToPipe(AudioRecord::Buffer *buffer)
{
    size_t frames = buffer->frameCount;
    status_t status = NO_ERROR;

    if (mStatus != NO_ERROR) {
        return mStatus;
    }

    // Write to the mono pipe - blocks if pipe is full
    ssize_t written = mPipe->write(buffer->raw, frames);
    if (written < 0) {
        ALOGE("writeToPipe() pipe write failed %d", written);
        buffer->size = 0;
        status = FAILED_TRANSACTION;
    } else if (written != (ssize_t)frames) {
        ALOGW("writeToPipe() dropped %d frames", frames - written);
        buffer->size = written * mTrack->frameSize();
    }

    ALOGV(" writeToPipe() written %4ld req %4u (used %3u%%)",
          written, frames, pipeUsage());

    return status;
}

status_t AudioLoopback::readFromPipe(AudioTrack::Buffer *buffer)
{
    size_t frames = buffer->frameCount;
    status_t status = NO_ERROR;
    ssize_t read;

    if (mStatus != NO_ERROR) {
        return mStatus;
    }

    if (mSteady) {
        read = mPipeReader->read(buffer->raw, frames,
                                 AudioBufferProvider::kInvalidPTS);
    } else {
        memset(buffer->raw, 0, buffer->size);
        read = frames;
        mFramesRead += read;
        if (mFramesRead >= mMarkerPosition) {
            ALOGV("readFromPipe() reached steady state %u", mMarkerPosition);
            mSteady = true;
        }
    }

    if (read < 0) {
        ALOGE("readFromPipe() pipe read failed %d", read);
        buffer->size = 0;
        status = FAILED_TRANSACTION;
    } else if (read != (ssize_t)frames) {
        ALOGW("readFromPipe() dropped %d frames", frames - read);
        buffer->size = read * mRecord->frameSize();
    }

    ALOGV("readFromPipe()    read %4ld req %4u (used %3u%%)",
          read, frames, pipeUsage());

    return status;
}

status_t AudioLoopback::flushPipe()
{
    status_t status = NO_ERROR;
    ssize_t read;
    uint32_t bytes = audio_bytes_per_sample(mFormat);

    size_t frames = mPipeReader->availableToRead();

    ALOGV("flushPipe() pipe flushing %d frames", frames);

    uint8_t buffer[4096];
    while (frames) {
        read = mPipeReader->read(buffer, sizeof(buffer) / mChannelCount / bytes,
                                 AudioBufferProvider::kInvalidPTS);
        if (read < 0) {
            ALOGE("flushPipe() pipe read failed %d", read);
            status = FAILED_TRANSACTION;
            break;
        }
        frames -= read;
    }

    return status;
}

void AudioLoopback::AudioRecordCallback(int event, void *user, void *info)
{
    AudioLoopback *loopback = (AudioLoopback *)user;
    if (!loopback) {
        ALOGE("AudioRecordCallback() invalid user data");
        return;
    }

    if (event == AudioRecord::EVENT_MORE_DATA) {
        AudioRecord::Buffer *buffer = (AudioRecord::Buffer *)info;

        if (loopback->writeToPipe(buffer) != NO_ERROR) {
            ALOGE("AudioRecordCallback() failed to read AudioRecord");
            if (loopback->mCbf) {
                loopback->mCbf(AudioLoopback::EVENT_ERROR, loopback->mUserData, 0);
            }
        }
    } else if (event == AudioRecord::EVENT_OVERRUN) {
        ALOGW("AudioRecordCallback() PCM buffer overrun");
        if (loopback->mCbf) {
            loopback->mCbf(AudioLoopback::EVENT_OVERRUN, loopback->mUserData, 0);
        }
    }
}

void AudioLoopback::AudioTrackCallback(int event, void *user, void *info)
{
    AudioLoopback *loopback = (AudioLoopback *)user;
    if (!loopback) {
        ALOGE("AudioTrackCallback() invalid user data");
        return;
    }

    if (event == AudioTrack::EVENT_MORE_DATA) {
        AudioTrack::Buffer *buffer = (AudioTrack::Buffer *)info;

        if (loopback->readFromPipe(buffer) != NO_ERROR) {
            ALOGE("AudioTrackCallback() failed to write AudioTrack");
            if (loopback->mCbf) {
                loopback->mCbf(AudioLoopback::EVENT_ERROR, loopback->mUserData, 0);
            }
        }
    } else if (event == AudioTrack::EVENT_MARKER) {
        ALOGV("AudioTrackCallback() reached steady state %u", *((uint32_t *)info));
    } else if (event == AudioTrack::EVENT_UNDERRUN) {
        ALOGW("AudioTrackCallback() PCM buffer underrun");
        if (loopback->mCbf) {
            loopback->mCbf(AudioLoopback::EVENT_UNDERRUN, loopback->mUserData, 0);
        }
    }
}

} // namespace android
