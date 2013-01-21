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

/*
<?xml version="1.0" encoding="utf-8"?>
<WFDSettings>
  <Settings device="Tablet">

    <VideoCodingCap profile="CBP" level="3">
      <VideoMode resolution="640x480p60" native="yes"/>
      <VideoMode resolution="1280x720p30"/>
    </VideoCodingCap>

    <AudioCodingCap audio_format="LPCM">
      <AudioMode freq="48000" channels_num="2">
      <AudioMode freq="48000" channels_num="4">
    </AudioCodingCap>

    <UibcCap>

    </UibcCap>
  </Settings>
</WFDSettings>
*/

#ifndef RTSP_CONFIG_H_
#define RTSP_CONFIG_H_

#include <libxml/parser.h>
#include <utils/RefBase.h>

namespace android {

class RtspConfig : public RefBase {
public:
    static sp<RtspConfig> read(const char *fileName, const char *deviceName);
    AString getVideoCaps();
    AString getAudioCaps();
    AString getUibcCaps();

protected:
    virtual ~RtspConfig();

private:
    enum { kVideoProfileCbp, kVideoProfileChp, kVideoProfilesNum };
    enum { kVideoCea, kVideoVesa, kVideoHh, kVideoNum };
    enum { kAudioLpcm, kAudioAac, kAudioAc3, kAudioFormatsNum };

    struct VideoProfile {
        bool valid;
        int level;
        uint32_t formats[kVideoNum];
        uint32_t latency;
        uint32_t minSliceSize;
        uint32_t sliceEncParams;
        uint32_t frameRateControl;
        int32_t maxHres;
        int32_t maxVres;
    };

    struct AudioFormat {
        bool valid;
        uint32_t formats;
        uint32_t latency;
    };

    VideoProfile mVideoProfiles[kVideoProfilesNum];
    int mNativeVideo;

    AudioFormat mAudioFormats[kAudioFormatsNum];

    RtspConfig();

    bool parseDecField(const xmlNodePtr node, const xmlChar *tag,
            bool mandatory, uint32_t *value);

    bool parseVideoCap(const xmlNodePtr videoCap);
    bool parseVideoProfileField(const xmlNodePtr videoCap, int *videoProfile);
    bool parseVideoLevelField(const xmlNodePtr videoCap, int videoProfile);
    bool parseVideoResolutionField(const xmlNodePtr videoMode, int videoProfileIdx,
            size_t *resIdx, int *resTable);
    bool parseVideoNativeField(const xmlNodePtr videoMode, size_t resIdx, int resTable);
    bool parseVideoMode(const xmlNodePtr videoMode, int videoProfileIdx);

    bool parseAudioCap(const xmlNodePtr videoCap);
    bool parseAudioFormat(const xmlNodePtr audioCap, int *audioFormat);
    bool parseAudioMode(const xmlNodePtr audioMode, int audioTableIdx);

    static AString getDeviceName();

    AString generateVideoProfile(int videoProfileIdx);
    AString generateAudioFormat(int audioFormatIdx);
};

} // namespace android

#endif // RTSP_CONFIG_H_
