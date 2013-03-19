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

#define LOG_TAG "CaptureSource"
#include <utils/Log.h>

#include <stdint.h>

#include <gui/BufferQueue.h>
#include <gui/ConsumerBase.h>
#include <hardware/gralloc.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AHandlerReflector.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/SurfaceMediaSource.h>
#include <utils/Vector.h>

#include "CaptureDevice.h"
#include "CaptureSource.h"

namespace android {

enum {
    HAL_PIXEL_FORMAT_TI_NV12 = 0x100
};

static const uint32_t kInitalQueuedBuffers = 2;

struct VirtualFramebufferStub : public BufferQueue {
public:
    VirtualFramebufferStub(uint32_t width, uint32_t height) {
        setConsumerName(String8("VirtualFramebufferStub"));
        setDefaultBufferSize(width, height);
    }

    virtual int query(int what, int* value) {
        switch (what) {
            case NATIVE_WINDOW_VIRTUAL_FRAMEBUFFER_STUB:
                *value = true;
                return NO_ERROR;
                break;
            default:
                return BufferQueue::query(what, value);
        }
    }
};

struct CaptureSource::CaptureDeviceListener : public CaptureDevice::FrameAvailableListener {
    CaptureDeviceListener(ALooper::handler_id handlerId)
        : mHandlerId(handlerId) {
    }

    virtual void onFrameAvailable(int index) {
        sp<AMessage> msg = new AMessage(kWhatFrameAvailable, mHandlerId);
        msg->setInt32("index", index);
        msg->post();
    }

    virtual void onCaptureError(int index, status_t error) {
        sp<AMessage> msg = new AMessage(kWhatCaptureError, mHandlerId);
        msg->setInt32("index", index);
        msg->setInt32("error", error);
        msg->post();
    }

private:

    ALooper::handler_id mHandlerId;
};

CaptureSource::CaptureSource(sp<SurfaceMediaSource> mediaSource)
    : mStarted(false),
      mShutdown(false),
      mError(OK),
      mWidth(0),
      mHeight(0),
      mFormat(HAL_PIXEL_FORMAT_TI_NV12),
      mMediaSource(mediaSource) {

    sp<MetaData> sourceFormat = mMediaSource->getFormat();

    int32_t temp;
    CHECK(sourceFormat->findInt32(kKeyWidth, &temp));
    mWidth = temp;
    CHECK(sourceFormat->findInt32(kKeyHeight, &temp));
    mHeight = temp;

    mBufferQueue = mMediaSource->getBufferQueue();
    mBufferQueue->setConsumerUsageBits(GRALLOC_USAGE_HW_VIDEO_ENCODER |
            GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_SW_WRITE_RARELY);
    mBufferQueue->setConsumerName(String8("WFD SMS"));
    mBufferQueue->setDefaultBufferFormat(mFormat);

    mFramebufferStub = new VirtualFramebufferStub(mWidth, mHeight);
    mCaptureDevice = new CaptureDevice();
}

CaptureSource::~CaptureSource() {
    CHECK(!mStarted);
}

sp<BufferQueue> CaptureSource::getBufferQueue() {
    return mFramebufferStub;
}

status_t CaptureSource::start(MetaData *params) {
    CHECK(!mStarted);

    status_t err = mMediaSource->start(params);
    if (err != OK) {
        ALOGE("Failed to start SurfaceMediaSource (%d)", err);
        return err;
    }

    err = setupCaptureDevice();
    if (err != OK) {
        mMediaSource->stop();
        return err;
    }

    err = setupBufferQueue();
    if (err != OK) {
        mCaptureDevice->release();
        mMediaSource->stop();
        return err;
    }

    mLooper = new ALooper;
    mLooper->setName("CaptureSource");
    mLooper->start();

    mReflector = new AHandlerReflector<CaptureSource>(this);
    mLooper->registerHandler(mReflector);

    mCaptureDevice->setFrameAvailableListener(new CaptureDeviceListener(mReflector->id()));
    mCaptureDevice->postDequeueBuffer();

    mError = OK;
    mStarted = true;

    return OK;
}

status_t CaptureSource::stop() {
    CHECK(mStarted);

    {
        Mutex::Autolock autoLock(mLock);

        mShutdown = true;

        cancelCaptureDeviceBuffers_l();
    }

    mCaptureDevice->release();

    if (mLooper != NULL) {
        mLooper->stop();
        mLooper.clear();

        mReflector.clear();
    }

    // No need to take mLock since looper thread has been stopped.
    cancelMediaSourceBuffers_l();

    status_t err = mMediaSource->stop();

    mStarted = false;
    mShutdown = false;

    return err;
}

sp<MetaData> CaptureSource::getFormat() {
    return mMediaSource->getFormat();
}

status_t CaptureSource::read(MediaBuffer **buffer, const ReadOptions *options) {
    {
        Mutex::Autolock autoLock(mLock);

        if (mError != OK) {
            return mError;
        }
    }

    return mMediaSource->read(buffer, options);
}

status_t CaptureSource::pause() {
    return mMediaSource->pause();
}

status_t CaptureSource::setBuffers(const Vector<MediaBuffer *> &buffers) {
    return mMediaSource->setBuffers(buffers);
}

void CaptureSource::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatFrameAvailable:
        {
            onFrameAvailable(msg);
            break;
        }

        case kWhatCaptureError:
        {
            Mutex::Autolock autoLock(mLock);

            // A capture may fail if all buffers are canceled during shutdown.
            if (!mShutdown) {
                int err;
                CHECK(msg->findInt32("error", &err));

                ALOGE("Failed to capture buffer (%d)", err);

                // Ignore the error and resubmit the buffer for capture.
                int index;
                dequeueCaptureDeviceBuffer_l(msg, &index);
                mError = queueCaptureDeviceBuffer_l(index);

                if (mError == OK) {
                    mCaptureDevice->postDequeueBuffer();
                }
            }
            break;
        }

        default:
            TRESPASS();
    }
}

void CaptureSource::onFrameAvailable(const sp<AMessage> &msg) {
    Mutex::Autolock autoLock(mLock);

    if (mError == OK) {
        int capturedBuffer;
        dequeueCaptureDeviceBuffer_l(msg, &capturedBuffer);
        mError = queueMediaSourceBuffer_l(capturedBuffer);
    }

    if (mError == OK) {
        int emptyBuffer;
        mError = dequeueMediaSourceBuffer_l(&emptyBuffer);
        if (mError == OK) {
            mError = queueCaptureDeviceBuffer_l(emptyBuffer);
        }
    }

    if (mError == OK && !mShutdown) {
        // TODO the post can be delayed if we want to throttle frame rate
        mCaptureDevice->postDequeueBuffer();
    }
}

status_t CaptureSource::dequeueMediaSourceBuffer_l(int *index) {
    sp<Fence> fence;
    status_t err = mBufferQueue->dequeueBuffer(index, fence, mWidth, mHeight, mFormat, GRALLOC_USAGE_HW_RENDER);
    if (err < 0) {
        ALOGE("Failed to dequeue from SurfaceMediaSource (%d)", err);
        return err;
    }

    BufferSlot *slot = &mBufferSlots[*index];

    CHECK(slot->mBufferState == BufferSlot::FREE || slot->mBufferState == BufferSlot::QUEUED_TO_SMS);

    slot->mBufferState = BufferSlot::DEQUEUED_FROM_SMS;
    slot->mTimestamp = -1;

    if ((err & ISurfaceTexture::BUFFER_NEEDS_REALLOCATION) || slot->mGraphicBuffer == NULL) {
        CHECK(!slot->mRegistered);
        err = mBufferQueue->requestBuffer(*index, &slot->mGraphicBuffer);
        if (err != OK) {
            ALOGE("Failed to get buffer from SurfaceMediaSource (%d)", err);
            return err;
        }
    }

    return err;
}

status_t CaptureSource::queueMediaSourceBuffer_l(int index) {
    BufferSlot *slot = &mBufferSlots[index];
    if (slot->mBufferState == BufferSlot::FREE) {
        ALOGE("Buffer %d is already queued to SurfaceMediaSource", index);
        return ALREADY_EXISTS;
    }

    if (slot->mBufferState != BufferSlot::DEQUEUED_FROM_CD) {
        ALOGW("Queuing empty buffer to SurfaceMediaSource");
    }

    Rect crop(slot->mGraphicBuffer->getWidth(), slot->mGraphicBuffer->getHeight());
    int scalingMode = NATIVE_WINDOW_SCALING_MODE_FREEZE;
    // TODO: Add support for portrait devices
    uint32_t transform = 0;

    ISurfaceTexture::QueueBufferInput input(slot->mTimestamp, crop, scalingMode, transform, Fence::NO_FENCE);
    ISurfaceTexture::QueueBufferOutput output;

    status_t err = mBufferQueue->queueBuffer(index, input, &output);
    if (err != OK) {
        ALOGE("Failed to queue to SurfaceMediaSource (%d)", err);
        return err;
    }

    slot->mBufferState = BufferSlot::QUEUED_TO_SMS;

    return OK;
}

void CaptureSource::cancelMediaSourceBuffers_l() {
    for (int i = 0; i < BufferQueue::NUM_BUFFER_SLOTS; i++) {
        BufferSlot *slot = &mBufferSlots[i];
        if (slot->mBufferState != BufferSlot::FREE && slot->mBufferState != BufferSlot::QUEUED_TO_SMS) {
            mBufferQueue->cancelBuffer(i, Fence::NO_FENCE);
            slot->mBufferState = BufferSlot::FREE;
        }
    }
}

status_t CaptureSource::setupCaptureDevice() {
    status_t err = mCaptureDevice->acquire();
    if (err != OK) {
        ALOGE("Failed to acquire capture device (%d)", err);
        return err;
    }

    err = mCaptureDevice->configure(mWidth, mHeight);
    if (err != OK) {
        ALOGE("Failed to configure capture device (%d)", err);
        mCaptureDevice->release();
        return err;
    }

    return OK;
}

status_t CaptureSource::setupBufferQueue() {
    int minUndequeuedBuffers = 0;
    status_t err = mBufferQueue->query(NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &minUndequeuedBuffers);
    if (err != OK) {
        ALOGE("Failed to get minimal undequeued buffer count (%d)", err);
        return err;
    }

    uint32_t bufferCount = minUndequeuedBuffers + kInitalQueuedBuffers;
    err = mBufferQueue->setBufferCount(bufferCount);
    if (err != OK) {
        ALOGE("Failed to set buffer count (%d)", err);
        return err;
    }

    // Pre-allocate the buffers. No need to take mLock since looper thread has not started yet.
    Vector<int> bufferIndices;
    for (uint32_t i = 0; i < bufferCount; i++) {
        int bufferIndex;
        err = dequeueMediaSourceBuffer_l(&bufferIndex);
        if (err != OK) {
            break;
        }

        bufferIndices.push(bufferIndex);
    }

    // Queue initial frames to DSSWB for capture.
    if (err == OK) {
        for (uint32_t i = 0; i < kInitalQueuedBuffers; i++) {
            err = queueCaptureDeviceBuffer_l(bufferIndices[i]);
            if (err != OK) {
                break;
            }
        }
    }

    // Return remaining buffers back to SMS.
    if (err == OK) {
        for (uint32_t i = kInitalQueuedBuffers; i < bufferCount; i++) {
            BufferSlot *slot = &mBufferSlots[bufferIndices[i]];
            mBufferQueue->cancelBuffer(bufferIndices[i], Fence::NO_FENCE);
            slot->mBufferState = BufferSlot::QUEUED_TO_SMS;
        }
    }

    if (err != OK) {
        cancelCaptureDeviceBuffers_l();
        cancelMediaSourceBuffers_l();
    }

    return err;
}

void CaptureSource::dequeueCaptureDeviceBuffer_l(const sp<AMessage> &msg, int *index) {
    CHECK(msg->findInt32("index", index));

    BufferSlot *slot = &mBufferSlots[*index];

    CHECK(slot->mBufferState == BufferSlot::QUEUED_TO_CD);

    slot->mBufferState = BufferSlot::DEQUEUED_FROM_CD;
    slot->mTimestamp = systemTime(SYSTEM_TIME_MONOTONIC);
}

status_t CaptureSource::queueCaptureDeviceBuffer_l(int index) {
    status_t err;
    BufferSlot *slot = &mBufferSlots[index];

    if (!slot->mRegistered) {
        err = mCaptureDevice->registerBuffer(index, slot->mGraphicBuffer);
        if (err != OK) {
            ALOGE("Failed to register buffer with capture device (%d)", err);
            return err;
        }

        slot->mRegistered = true;
    }

    err = mCaptureDevice->queueBuffer(index);
    if (err != OK) {
        ALOGE("Failed to queue buffer to capture device (%d)", err);
        return err;
    }

    slot->mBufferState = BufferSlot::QUEUED_TO_CD;

    return OK;
}

void CaptureSource::cancelCaptureDeviceBuffers_l() {
    int bufferIndex;
    while (mCaptureDevice->cancelBuffer(&bufferIndex) == OK) {
        mBufferSlots[bufferIndex].mBufferState = BufferSlot::CANCELED_FROM_CD;
    }
}

} // namespace android
