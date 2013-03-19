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

#ifndef CAPTURE_DEVICE_H_
#define CAPTURE_DEVICE_H_

#include <stdint.h>

#include <utils/Errors.h>
#include <media/stagefright/foundation/AHandler.h>

namespace android {

struct ALooper;
struct AMessage;
class GraphicBuffer;
class IDSSWBHal;

struct CaptureDevice : public AHandler {
    struct FrameAvailableListener : RefBase {
        virtual void onFrameAvailable(int index) = 0;
        virtual void onCaptureError(int index, status_t error) = 0;
    };

    CaptureDevice();
    virtual ~CaptureDevice();

    status_t acquire();
    status_t release();

    status_t configure(uint32_t width, uint32_t height);

    void setFrameAvailableListener(const sp<FrameAvailableListener> &listener);

    status_t registerBuffer(int index, const sp<GraphicBuffer> &graphicBuffer);

    status_t queueBuffer(int index);
    status_t dequeueBuffer(int *index);
    status_t cancelBuffer(int *index);

    virtual void onMessageReceived(const sp<AMessage> &msg);

    void postDequeueBuffer(int64_t delayUs = 0);

private:

    enum {
        kWhatWritebackDequeue,
    };

    sp<ALooper> mLooper;
    sp<AMessage> mDequeueMessage;

    sp<FrameAvailableListener> mFrameAvailableListener;

    sp<IDSSWBHal> mWriteback;
    int mHandle;

    DISALLOW_EVIL_CONSTRUCTORS(CaptureDevice);
};

} // namespace android

#endif // CAPTURE_DEVICE_H_
