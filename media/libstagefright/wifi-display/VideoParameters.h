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

#ifndef VIDEO_PARAMETERS_H_

#define VIDEO_PARAMETERS_H_

#include <utils/RefBase.h>
#include <utils/List.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/MediaErrors.h>

namespace android {

struct VideoMode : public RefBase {
    bool h264HighProfile;
    int h264Level;
    int width;
    int height;
    int frameRate;
    bool progressive;

    VideoMode();
    VideoMode(const VideoMode &videoMode);
    bool operator==(const VideoMode &other) const;
    bool operator>(const VideoMode &other) const;
    AString toString();
};

struct SimpleVideoMode {
    int width;
    int height;
    int frameRate;
    bool progressive;
};

struct VideoParameters : public RefBase {
private:
    struct VideoTable : public LightRefBase<VideoTable> {
        int table;
        int index;
    };

public:
    // H.264-codec
    struct H264Codec : public LightRefBase<H264Codec> {
        uint32_t profile;
        uint32_t level;
        uint32_t cea;
        uint32_t vesa;
        uint32_t hh;
        uint32_t latency;
        uint32_t minSliceSize;
        uint32_t sliceEncParams;
        uint32_t frameRateControl;
        uint32_t maxHres;
        uint32_t maxVres;
    };

    static sp<VideoParameters> parse(const char *data);
    sp<VideoMode> applyVideoMode(const char *data);
    AString generateVideoFormats();
    AString generateVideoMode(const sp<VideoMode> &mode);
    sp<VideoMode> getBestVideoMode(const sp<VideoParameters> &sinkParams,
            const sp<VideoMode> &desiredMode);
    bool isMatchingVideoMode(const sp<VideoMode> &videoMode);
    bool getVideoFrameRateChangeSupport(const sp<VideoMode> &videoMode);

protected:
    virtual ~VideoParameters();

private:
    uint32_t mNative;
    SimpleVideoMode mNativeMode;
    uint32_t mPrefDispModeSupported;
    List< sp<H264Codec> > mH264Codecs;
    List< sp<VideoMode> > mMatchingModes;

    VideoParameters();
    status_t parseParams(const char *data);
    uint8_t getLevel(int level);
    AString generateH264Format(const sp<H264Codec> &params);
    sp<VideoTable> checkResolution(const sp<VideoMode> &mode);
    List< sp<H264Codec> > *getCodecs();
    void initMatchingModes(const sp<VideoParameters> &sinkParams);
};

}  // namespace android

#endif  // VIDEO_PARAMETERS_H_
