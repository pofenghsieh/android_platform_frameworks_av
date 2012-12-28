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

#define LOG_TAG "CaptureDevice"
#include <utils/Log.h>

#include <stdint.h>

#include <binder/IServiceManager.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>
#include <ui/DisplayInfo.h>
#include <ui/GraphicBuffer.h>

#include <IDSSWBHal.h>

#include "CaptureDevice.h"

namespace android {

static const int kDssWbServiceConnectionAttempts = 10;
static const uint32_t kDssWbServiceWaitUs = 500000; // 0.5 s

CaptureDevice::CaptureDevice()
    : mHandle(-1) {
}

CaptureDevice::~CaptureDevice() {
    if (mHandle != -1) {
        release();
    }
}

status_t CaptureDevice::acquire() {
    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> binder;

    for (int i = 0; i < kDssWbServiceConnectionAttempts; i++) {
        binder = sm->getService(String16("hardware.dsswb"));
        if (binder != NULL) {
            break;
        }
        ALOGW("DSSWB Service not published, waiting...");
        usleep(kDssWbServiceWaitUs);
    }

    if (binder == NULL) {
        return NO_INIT;
    }

    ALOGV("DSSWB binder available");
    mWriteback = interface_cast<IDSSWBHal>(binder);

    // acquire WB handle
    status_t err = mWriteback->acquireWB(&mHandle);
    if (err != OK) {
        return err;
    }

    mLooper = new ALooper;
    mLooper->setName("CaptureSource::Writeback");
    mLooper->start();
    mLooper->registerHandler(this);

    mDequeueMessage = new AMessage(kWhatWritebackDequeue, id());

    return OK;
}

status_t CaptureDevice::release() {
    if (mLooper != NULL) {
        mLooper->stop();
        mLooper.clear();

        mDequeueMessage.clear();
    }

    mFrameAvailableListener.clear();

    status_t err = mWriteback->releaseWB(mHandle);

    mWriteback.clear();
    mHandle = -1;

    return err;
}

status_t CaptureDevice::configure(uint32_t width, uint32_t height) {
    wb_capture_config_t config;

    // TODO: configure transform properly for portrait devices
    config.transform = 0;

    DisplayInfo info;
    sp<IBinder> display = SurfaceComposerClient::getBuiltInDisplay(ISurfaceComposer::eDisplayIdMain);
    CHECK_EQ(SurfaceComposerClient::getDisplayInfo(display, &info), (status_t)OK);

    int screenWidth = info.w;
    int screenHeight = info.h;

    config.sourceCrop.left = 0;
    config.sourceCrop.top = 0;
    config.sourceCrop.right = screenWidth;
    config.sourceCrop.bottom = screenHeight;

    config.captureFrame.left = 0;
    config.captureFrame.top = 0;
    config.captureFrame.right = width;
    config.captureFrame.bottom = height;

    ALOGV("WB config: screen = %dx%d, capture = %dx%d", screenWidth, screenHeight, width, height);

    return mWriteback->setConfig(mHandle, config);
}

void CaptureDevice::setFrameAvailableListener(const sp<FrameAvailableListener> &listener) {
    mFrameAvailableListener = listener;
}

status_t CaptureDevice::registerBuffer(int index, const sp<GraphicBuffer> &graphicBuffer) {
    return mWriteback->registerBuffer(mHandle, index, graphicBuffer->getNativeBuffer()->handle);
}

status_t CaptureDevice::queueBuffer(int index) {
    return mWriteback->queue(mHandle, index);
}

status_t CaptureDevice::dequeueBuffer(int *index) {
    return mWriteback->dequeue(mHandle, index);
}

status_t CaptureDevice::cancelBuffer(int *index) {
    return mWriteback->cancelBuffer(mHandle, index);
}

void CaptureDevice::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatWritebackDequeue:
        {
            int bufferIndex;
            status_t err = dequeueBuffer(&bufferIndex);

            if (mFrameAvailableListener != NULL) {
                if (err == OK) {
                    mFrameAvailableListener->onFrameAvailable(bufferIndex);
                } else {
                    mFrameAvailableListener->onCaptureError(err);
                }
            }
            break;
        }

        default:
            TRESPASS();
    }
}

void CaptureDevice::postDequeueBuffer(int64_t delayUs) {
    mDequeueMessage->post(delayUs);
}

} // namespace android
