/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
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

#ifndef CAPTURE_SOURCE_H_
#define CAPTURE_SOURCE_H_

#include <stdint.h>

#include <utils/Vector.h>

#include <gui/BufferQueue.h>

#include <media/stagefright/foundation/AHandlerReflector.h>
#include <media/stagefright/MediaSource.h>

namespace android {

struct ALooper;
struct AMessage;
struct CaptureDevice;
class MediaBuffer;
class MetaData;
class SurfaceMediaSource;

struct CaptureSource : public MediaSource {
    CaptureSource(sp<SurfaceMediaSource> mediaSource);

    sp<BufferQueue> getBufferQueue();

    // MediaSource
    virtual status_t start(MetaData *params);
    virtual status_t stop();
    virtual sp<MetaData> getFormat();
    virtual status_t read(MediaBuffer **buffer, const ReadOptions *options);
    virtual status_t pause();
    virtual status_t setBuffers(const Vector<MediaBuffer *> &buffers);

    void onMessageReceived(const sp<AMessage> &msg);

protected:

    virtual ~CaptureSource();

private:

    enum {
        kWhatFrameAvailable,
        kWhatCaptureError,
    };

    struct BufferSlot {
        BufferSlot()
            : mBufferState(FREE),
              mRegistered(false),
              mTimestamp(-1) {
        }

        enum BufferState {
            // Indicates that the buffer is not been used yet. The buffer is owned by
            // SurfaceMediaSource.
            FREE = 0,

            // Indicates that the buffer has been dequeued from SurfaceMediaSource. The buffer
            // is owned by CaptureSource.
            DEQUEUED_FROM_SMS = 1,

            // Indicates that the buffer has been queued to CaptureDevice for capture. The
            // buffer is owned by CaptureDevice.
            QUEUED_TO_CD = 2,

            // Indicates that the buffer has been captured and is about to be queued back
            // to SurfaceMediaSource. The buffer is owned by CaptureSource.
            DEQUEUED_FROM_CD = 3,

            // Indicates that the buffer has been canceled from CaptureDevice without
            // completing the capture. The buffer is owned by CaptureSource.
            CANCELED_FROM_CD = 4,

            // Indicates that the buffer has been queued back to SurfaceMediaSource. The buffer
            // is owned by SurfaceMediaSource.
            QUEUED_TO_SMS = 5
        };

        // The current state of this buffer slot.
        BufferState mBufferState;

        // Indicates that the buffer has been registered with CaptureDevice.
        bool mRegistered;

        sp<GraphicBuffer> mGraphicBuffer;

        int64_t mTimestamp;
    };

    struct CaptureDeviceListener;

    void onFrameAvailable(const sp<AMessage> &msg);

    status_t dequeueMediaSourceBuffer_l(int *index);
    status_t queueMediaSourceBuffer_l(int index);
    void cancelMediaSourceBuffers_l();

    status_t setupCaptureDevice();
    status_t setupBufferQueue();

    void dequeueCaptureDeviceBuffer_l(const sp<AMessage> &msg, int *index);
    status_t queueCaptureDeviceBuffer_l(int index);
    void cancelCaptureDeviceBuffers_l();

    bool mStarted;
    bool mShutdown;
    status_t mError;

    Mutex mLock;
    sp<ALooper> mLooper;
    sp< AHandlerReflector<CaptureSource> > mReflector;

    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mFormat;

    sp<BufferQueue> mFramebufferStub;

    sp<CaptureDevice> mCaptureDevice;

    sp<SurfaceMediaSource> mMediaSource;
    sp<BufferQueue> mBufferQueue;

    BufferSlot mBufferSlots[BufferQueue::NUM_BUFFER_SLOTS];

    DISALLOW_EVIL_CONSTRUCTORS(CaptureSource);
};

} // namespace android

#endif // CAPTURE_SOURCE_H_
