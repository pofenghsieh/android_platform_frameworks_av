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

#ifndef AUDIOLOOPBACK_H
#define AUDIOLOOPBACK_H

#include <pthread.h>
#include <sys/types.h>
#include <media/AudioRecord.h>
#include <media/AudioTrack.h>

namespace android {

class AudioLoopback : virtual public RefBase {
 public:

    /* Events used by AudioLoopback callback function (callback_t).
     * Keep in sync with frameworks/base/media/java/android/media/HostlessTrack.java NATIVE_EVENT_*.
     */
    enum event_type {
        EVENT_ERROR    = 0,         // Runtime error event.
        EVENT_UNDERRUN = 1,         // AudioTrack Underrun event.
        EVENT_OVERRUN  = 2          // AudioRecord Overrun event.
    };

    /* Callback for receiving events from AudioLoopback.
     *
     * Parameters:
     *
     * event:   type of event notified (see enum AudioLoopback::event_type).
     * user:    Pointer to context for use by the callback receiver.
     * info:    Pointer to optional parameter according to event type:
     *          - EVENT_ERROR: unused.
     *          - EVENT_UNDERRUN: unused.
     *          - EVENT_OVERRUN: unused.
     */

    typedef void (*callback_t)(int event, void* user, void *info);

    /* Constructs an AudioLoopback
     */
    AudioLoopback(audio_source_t sourceType, uint32_t channelConfig,
                  const String16& opPackageName, callback_t callback = NULL,
                  void* user = NULL);

    /* Terminates the AudioLoopback
     */
    virtual ~AudioLoopback();

    /* Result of constructing the AudioLoopback. This must be checked
     * before using any AudioLoopoback APIs, using an uninitialized
     * AudioLoopback produces undefined results.
     *
     * Parameters:
     *  none.
     *
     * Returned value:
     *  NO_ERROR: loopback was successfully created.
     *  BAD_VALUE: loopback's internal pipe construction failed.
     *  Other error codes returned by AudioTrack, AudioRecord.
     */
    status_t initCheck() const { return mStatus; }

    /* Set the volume for all channels.
     *
     * Parameters:
     *
     * volume:  volume to set
     *
     * Returned value:
     *  NO_ERROR: successful setting of volume.
     *  BAD_VALUE: loopback is not initialized properly or the volume setting
     *             isn't valid.
     */
    status_t setVolume(float volume);

    /* Starts the audio loopback
     *
     * Parameters:
     *  none.
     *
     * Returned value:
     *  NO_ERROR: successful loopback start.
     *  BAD_VALUE: loopback is not initialized properly.
     *  INVALID_OPERATION: loopback has already started.
     */
    status_t start();

    /* Stops the audio loopback
     *
     * Parameters:
     *  none.
     *
     * Returned value:
     *  none.
     */
    void stop();

    /* Returns the unique session ID associated with internal audio track.
     *
     * Parameters:
     *  none.
     *
     * Returned value:
     *  AudioTrack session ID.
     */
    int getSessionId() const;

 protected:
    /* 23.2ms at 44.1kHz, 21.2ms at 48kHz */
    static const int kFastTrackFrames = 1024;

    /* Notification period for AudioRecord MORE_DATA event */
    static const int kPreferredNotificationFrames = 256;

    /* AudioHAL period size, this value can't be retrieved through AudioFlinger */
    static const int kAudioHALPeriodFrames = 256;

    /* Audio record warm-up retry count (10 ms per try) */
    static const int kMaxRecordWarmUpRetry = 10;

    sp<AudioRecord> mRecord;
    sp<AudioTrack> mTrack;
    audio_source_t mSourceType;
    audio_stream_type_t mStreamType;
    String16 mOpPackageName;
    status_t mStatus;
    uint32_t mRate;
    audio_format_t mFormat;
    audio_channel_mask_t mInChannels;
    audio_channel_mask_t mOutChannels;
    uint32_t mInFrameSize;
    uint32_t mOutFrameSize;
    bool mRunning;
    bool mRecordWarm;
    bool mRecovering;
    bool mTrackSteady;
    uint32_t mInMinFrameCount;
    uint32_t mOutMinFrameCount;
    uint32_t mInFrames;
    uint32_t mOutFrames;
    uint32_t mNotificationFrames;
    uint32_t mFramesWritten;
    uint32_t mFramesRead;
    callback_t mCbf;
    void *mUserData;
    char *mPrefillBuffer;
    bool mWaitingForStopLock;
    mutable Mutex mLock;

    status_t createTrack();
    status_t createRecord();
    void getRecordPosition(uint32_t &server, uint32_t &client, uint32_t &frames) const;
    void getTrackPosition(uint32_t &server, uint32_t &client, uint32_t &frames) const;
    status_t prefillTrack();
    status_t write(AudioRecord::Buffer *buffer);

 private:
    AudioLoopback(const AudioLoopback& loopback);
    AudioLoopback& operator=(const AudioLoopback& loopback);

    static void trackCallback(int event, void *user, void *info);
    static void recordCallback(int event, void *user, void *info);
};

} // namespace android

#endif
