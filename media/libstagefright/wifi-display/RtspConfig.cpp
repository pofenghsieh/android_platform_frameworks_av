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

#define LOG_TAG "RtspConfig"

#include <utils/Log.h>
#include <media/stagefright/foundation/ADebug.h>
#include <libxml/xmlmemory.h>
#include <cutils/properties.h>

#include "RtspConfig.h"

namespace android {

static const int kInvalidValue = -1;

static const xmlChar kRoot[] = "WFDSettings";
static const xmlChar kSettings[] = "Settings";
static const xmlChar kDevice[] = "device";

static const xmlChar kVideoCap[] = "VideoCodingCap";
static const xmlChar kProfile[] = "profile";
static const xmlChar kLevel[] = "level";
static const xmlChar kDecoderLatency[] = "decoder_latency";
static const xmlChar kMinSliceSize[] = "min_slice_size";
static const xmlChar kSliceEncPar[] = "slice_enc_params";
static const xmlChar kFrameRateCtl[] = "frame_rate_control_support";
static const xmlChar kMaxHres[] = "max_hres";
static const xmlChar kMaxVres[] = "max_vres";

static const xmlChar kVideoMode[] = "VideoMode";
static const xmlChar kResolution[] = "resolution";
static const xmlChar kNative[] = "native";

static const xmlChar kAudioCap[] = "AudioCodingCap";
static const xmlChar kAudioMode[] = "AudioMode";
static const xmlChar kAudioFormat[] = "audio_format";
static const xmlChar kSampleRate[] = "sample_rate";
static const xmlChar kChannelsNum[] = "channels_num";
static const xmlChar kLatency[] = "latency";

static const xmlChar kUibcCap[] = "UibcCap";

static const char *kCeaResolution[] = {
    "640x480p60",
    "720x480p60",
    "720x480i60",
    "720x576p50",
    "720x576i50",
    "1280x720p30",
    "1280x720p60",
    "1920x1080p30",
    "1920x1080p60",
    "1920x1080i60",
    "1280x720p25",
    "1280x720p50",
    "1920x1080p25",
    "1920x1080p50",
    "1920x1080i50",
    "1280x720p24",
    "1920x1080p24",
};
static const char *kVesaResolution[] = {
    "800x600p30",
    "800x600p60",
    "1024x768p30",
    "1024x768p60",
    "1152x864p30",
    "1152x864p60",
    "1280x768p30",
    "1280x768p60",
    "1280x800p30",
    "1280x800p60",
    "1360x768p30",
    "1360x768p60",
    "1366x768p30",
    "1366x768p60",
    "1280x1024p30",
    "1280x1024p60",
    "1400x1050p30",
    "1400x1050p60",
    "1440x900p30",
    "1440x900p60",
    "1600x900p30",
    "1600x900p60",
    "1600x1200p30",
    "1600x1200p60",
    "1680x1024p30",
    "1680x1024p60",
    "1680x1050p30",
    "1680x1050p60",
    "1920x1200p30",
    "1920x1200p60",
};
static const char *kHhResolution[] = {
    "800x480p30",
    "800x480p60",
    "854x480p30",
    "854x480p60",
    "864x480p30",
    "864x480p60",
    "640x360p30",
    "640x360p60",
    "960x540p30",
    "960x540p60",
    "848x480p30",
    "848x480p60",
};
static const char **kVideoResolutions[] = {
    kCeaResolution,
    kVesaResolution,
    kHhResolution,
};
static const size_t kVideoResolutionSizes[] = {
    sizeof(kCeaResolution) / sizeof(kCeaResolution[0]),
    sizeof(kVesaResolution) / sizeof(kVesaResolution[0]),
    sizeof(kHhResolution) / sizeof(kVesaResolution[0]),
};
static const char kCbp[] = "CBP";
static const char kChp[] = "CHP";
static const char *kLevels[] = {
    "3.1", "3.2", "4", "4.1", "4.2",
};
static const size_t kLevelsSize = sizeof(kLevels) / sizeof(kLevels[0]);

struct AudioMode {
    uint32_t sampleRate;
    uint32_t channelsNum;
};
static const AudioMode kLpcmModes[] = {
    {44100, 2},
    {48000, 2},
};
static const AudioMode kAacModes[] = {
    {48000, 2},
    {48000, 4},
    {48000, 6},
    {48000, 8},
};
static const AudioMode kAc3Modes[] = {
    {48000, 2},
    {48000, 4},
    {48000, 6},
};
static const AudioMode *kAudioModes[] = {
    kLpcmModes,
    kAacModes,
    kAc3Modes,
};
static const size_t kAudioModeSizes[] = {
    sizeof(kLpcmModes) / sizeof(kLpcmModes[0]),
    sizeof(kAacModes) / sizeof(kAacModes[0]),
    sizeof(kAc3Modes) / sizeof(kAc3Modes[0]),
};
static const char *kAudioCodecNames[] = {
    "LPCM", "AAC", "AC3",
};

RtspConfig::RtspConfig()
    : mNativeVideo(kInvalidValue)
{
    for (int i = 0; i < kVideoProfilesNum; i++) {
        mVideoProfiles[i].valid = false;
        mVideoProfiles[i].level = kInvalidValue;
        mVideoProfiles[i].formats[kVideoCea] = 0;
        mVideoProfiles[i].formats[kVideoVesa] = 0;
        mVideoProfiles[i].formats[kVideoHh] = 0;
        mVideoProfiles[i].latency = 0;
        mVideoProfiles[i].minSliceSize = 0;
        mVideoProfiles[i].sliceEncParams = 0;
        mVideoProfiles[i].frameRateControl = 0;
        mVideoProfiles[i].maxHres = kInvalidValue;
        mVideoProfiles[i].maxVres = kInvalidValue;
    }

    for (int i = 0; i < kAudioFormatsNum; i++) {
        mAudioFormats[i].valid = false;
        mAudioFormats[i].formats = 0;
        mAudioFormats[i].latency = 0;
    }
}

RtspConfig::~RtspConfig() {}

bool RtspConfig::parseDecField(const xmlNodePtr node, const xmlChar *tag,
        bool mandatory, uint32_t *value) {
    bool res = true;
    char *strValue = reinterpret_cast<char *>(xmlGetProp(node, tag));
    if (strValue == NULL) {
        if (mandatory) {
            ALOGE("Mandatory \"%s\" field is absent", tag);
            res = false;
        }
    } else {
        char *endPtr;
        uint32_t tmp = strtoul(strValue, &endPtr, 10);
        if (*endPtr == '\0') {
            *value = tmp;
        } else {
            ALOGE("%s \"%s\" field value is invalid (%s)",
                    mandatory ? "Mandatory" : "Optional",  tag, strValue);
            res = false;
        }
        xmlFree(strValue);
    }
    return res;
}

bool RtspConfig::parseVideoCap(const xmlNodePtr videoCap) {
    // Extract mandatory "profile" field
    int profile;
    if (!parseVideoProfileField(videoCap, &profile)) return false;
    if (mVideoProfiles[profile].valid) {
        ALOGE("Duplicated video profile %s is not acceptable",
                profile == kVideoProfileCbp ? kCbp : kChp);
        return false;
    }

    // Extract mandatory "level" field
    if (!parseVideoLevelField(videoCap, profile)) return false;

    // Extract optional fields
    if (!parseDecField(videoCap, kDecoderLatency, false,
            &mVideoProfiles[profile].latency))
        return false;

    if (!parseDecField(videoCap, kMinSliceSize, false,
            &mVideoProfiles[profile].minSliceSize))
        return false;

    if (!parseDecField(videoCap, kSliceEncPar, false,
            &mVideoProfiles[profile].sliceEncParams))
        return false;

    if (!parseDecField(videoCap, kFrameRateCtl, false,
            &mVideoProfiles[profile].frameRateControl))
        return false;

    if (!parseDecField(videoCap, kMaxHres, false,
            reinterpret_cast<uint32_t *>(&mVideoProfiles[profile].maxHres)))
        return false;

    if (!parseDecField(videoCap, kMaxVres, false,
            reinterpret_cast<uint32_t *>(&mVideoProfiles[profile].maxHres)))
        return false;

    // Extract video modes
    xmlNodePtr videoMode = videoCap->xmlChildrenNode;
    for(; videoMode != NULL; videoMode = videoMode->next) {
        if (xmlStrcmp(videoMode->name, kVideoMode)) continue;
        if (!parseVideoMode(videoMode, profile)) return false;
    }

    for (int idx = 0; idx < kVideoNum; idx++) {
        if(mVideoProfiles[profile].formats[idx]) {
            mVideoProfiles[profile].valid = true;
            break;
        }
    }
    return true;
}

bool RtspConfig::parseVideoProfileField(const xmlNodePtr videoCap, int *videoProfile) {
    bool res = true;
    char *strValue = reinterpret_cast<char *>(xmlGetProp(videoCap, kProfile));
    if (strValue == NULL) {
        ALOGE("Mandatory \"%s\" field is absent", kProfile);
        return false;
    }

    if (!strcasecmp(strValue, kCbp)) {
        *videoProfile = kVideoProfileCbp;
    } else if (!strcasecmp(strValue, kChp)) {
        *videoProfile = kVideoProfileChp;
    } else {
        ALOGE("Mandatory \"%s\" field value is invalid (%s)", kProfile, strValue);
        res = false;
    }

    xmlFree(strValue);
    return res;
}

bool RtspConfig::parseVideoLevelField(const xmlNodePtr videoCap, int videoProfile) {
    bool res = true;
    char *strValue = reinterpret_cast<char *>(xmlGetProp(videoCap, kLevel));
    if (strValue == NULL) {
        ALOGE("Mandatory \"%s\" field is absent", kLevel);
        return false;
    }

    for (size_t idx = 0; idx < kLevelsSize; idx++) {
        if (!strcasecmp(kLevels[idx], strValue)) {
            mVideoProfiles[videoProfile].level = 1 << idx;
            break;
        }
    }

    if (mVideoProfiles[videoProfile].level == kInvalidValue) {
        ALOGE("Mandatory \"%s\" field value is invalid (%s)", kLevel, strValue);
        res = false;
    }

    xmlFree(strValue);
    return res;
}

bool RtspConfig::parseVideoResolutionField(const xmlNodePtr videoMode,
        int videoProfileIdx, size_t *resIdx, int *resTable) {
    bool res = true;
    char *strValue = reinterpret_cast<char *>(xmlGetProp(videoMode, kResolution));
    if (strValue == NULL) {
        ALOGE("Mandatory \"%s\" field is absent", kResolution);
        return false;
    }

    *resTable = kInvalidValue;

    for (int idxTable = 0; *resTable == kInvalidValue && idxTable < kVideoNum; idxTable++) {
        for (*resIdx = 0; *resIdx < kVideoResolutionSizes[idxTable]; (*resIdx)++) {
            if (!strcasecmp(strValue, kVideoResolutions[idxTable][*resIdx])) {
                *resTable = idxTable;
                mVideoProfiles[videoProfileIdx].formats[idxTable] |= 1 << *resIdx;
                break;
            }
        }
    }

    if (*resTable == kInvalidValue) {
        ALOGE("Mandatory \"%s\" field value is invalid (%s)", kResolution, strValue);
        res = false;
    }
    xmlFree(strValue);

    return res;
}

bool RtspConfig::parseVideoNativeField(const xmlNodePtr videoMode,
        size_t resIdx, int resTable) {
    bool res = true;
    char *strValue = reinterpret_cast<char *>(xmlGetProp(videoMode, kNative));
    if (strValue != NULL) {
        if (!strcasecmp(strValue, "yes")) {
            if (mNativeVideo == kInvalidValue) {
                mNativeVideo = (resIdx << 3) + resTable;
            } else {
                ALOGE("Optional \"%s\" field must have \"yes\" mark only one time", kNative);
                res = false;
            }
        }
        xmlFree(strValue);
    }
    return res;
}

bool RtspConfig::parseVideoMode(const xmlNodePtr videoMode, int videoProfileIdx) {
    // Extract mandatory "resolution" field
    size_t resIdx;
    int resTable;
    if (!parseVideoResolutionField(videoMode, videoProfileIdx, &resIdx, &resTable))
        return false;

    // Looking for optional "native" field
    if (!parseVideoNativeField(videoMode, resIdx, resTable)) return false;
    return true;
}

bool RtspConfig::parseAudioCap(const xmlNodePtr audioCap) {
    // Extract mandatory "audio_format" field
    int format;
    if (!parseAudioFormat(audioCap, &format)) return false;

    // Extract optional "latency" field
    if (!parseDecField(audioCap, kLatency, false, &mAudioFormats[format].latency))
        return false;

    // Extract audio modes
    xmlNodePtr audioMode = audioCap->xmlChildrenNode;
    for(; audioMode != NULL; audioMode = audioMode->next) {
        if (xmlStrcmp(audioMode->name, kAudioMode)) continue;
        if (!parseAudioMode(audioMode, format)) return false;
    }

    if (mAudioFormats[format].formats) {
        mAudioFormats[format].valid = true;
    }
    return true;
}

bool RtspConfig::parseAudioFormat(const xmlNodePtr audioCap, int *audioFormat) {
    bool res = true;
    char *strValue = reinterpret_cast<char *>(xmlGetProp(audioCap, kAudioFormat));
    if (strValue == NULL) {
        ALOGE("Mandatory \"%s\" field is absent", kAudioFormat);
        return false;
    }

    if (!strcasecmp(strValue, kAudioCodecNames[kAudioLpcm])) {
        *audioFormat = kAudioLpcm;
    } else if (!strcasecmp(strValue, kAudioCodecNames[kAudioAac])) {
        *audioFormat = kAudioAac;
    } else if (!strcasecmp(strValue, kAudioCodecNames[kAudioAc3])) {
        *audioFormat = kAudioAc3;
    } else {
        ALOGE("Mandatory \"%s\" field value is invalid (%s)", kAudioFormat, strValue);
        res = false;
    }
    xmlFree(strValue);

    if (res && mAudioFormats[*audioFormat].valid) {
        ALOGE("Duplicated audio format %s is not acceptable", kAudioCodecNames[*audioFormat]);
        res = false;
    }
    return res;
}

bool RtspConfig::parseAudioMode(const xmlNodePtr audioMode, int audioTableIdx) {
    // Extract mandatory "sample_rate" field
    uint32_t sampleRate;
    if (!parseDecField(audioMode, kSampleRate, true, &sampleRate)) return false;

    // Extract mandatory "channels_num" field
    uint32_t channelsNum;
    if (!parseDecField(audioMode, kChannelsNum, true, &channelsNum)) return false;

    uint32_t format = 0;
    for (size_t idx = 0; idx < kAudioModeSizes[audioTableIdx]; idx++) {
        if (kAudioModes[audioTableIdx][idx].sampleRate == sampleRate &&
                kAudioModes[audioTableIdx][idx].channelsNum == channelsNum) {
            format = 1 << idx;
            break;
        }
    }

    if (format == 0) {
        ALOGE("Unsupported audio format %d %dch.", sampleRate, channelsNum);
        return false;
    }

    mAudioFormats[audioTableIdx].formats |= format;
    return true;
}

//static
sp<RtspConfig> RtspConfig::read(const char *fileName, const char *deviceName) {
    sp<RtspConfig> rtspConfig = new RtspConfig();

    AString actualDeviceName;
    if (deviceName == NULL) {
        actualDeviceName = getDeviceName();
    } else {
        actualDeviceName.setTo(deviceName);
    }
    ALOGV("device name=\"%s\"", actualDeviceName.c_str());

    if (actualDeviceName.empty()) {
        ALOGE("Failed to determine device name");
        return NULL;
    }

    xmlDocPtr doc = xmlParseFile(fileName);

    if (doc == NULL) {
        ALOGE("Could not parse XML config file %s\n", fileName);
        return NULL;
    }

    bool res = true;
    xmlNodePtr root = xmlDocGetRootElement(doc);
    for (; root != NULL && res; root = root->next) {

        if (xmlStrcmp(root->name, kRoot)) continue;

        xmlNodePtr settings = root->xmlChildrenNode;
        for (; settings != NULL && res; settings = settings->next) {
            if (xmlStrcmp(settings->name, kSettings)) continue;

            char *value = reinterpret_cast<char *>(xmlGetProp(settings, kDevice));
            if (value == NULL) continue;

            if (!strcmp(value, actualDeviceName.c_str())) {
                ALOGV("Device \"%s\" has been found!\n", actualDeviceName.c_str());

                // Get video coding capabilities
                xmlNodePtr videoCap = settings->xmlChildrenNode;
                for (; videoCap != NULL && res; videoCap = videoCap->next) {

                    if (xmlStrcmp(videoCap->name, kVideoCap)) continue;

                    if (!rtspConfig->parseVideoCap(videoCap)) res = false;
                }

                // Get audio coding capabilities
                xmlNodePtr audioCap = settings->xmlChildrenNode;
                for (; audioCap != NULL && res; audioCap = audioCap->next) {

                    if (xmlStrcmp(audioCap->name, kAudioCap)) continue;

                    if (!rtspConfig->parseAudioCap(audioCap)) res = false;
                }
            }
            xmlFree(value);
        }
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();

    if (!res) return NULL;

    return rtspConfig;
}

// static
AString RtspConfig::getDeviceName() {
    AString name;
    char val[PROPERTY_VALUE_MAX];
    if (property_get("ro.product.device", val, NULL)) {
        if (!strcasecmp("blaze", val)) {
            name = "Blaze";
        } else if (!strcasecmp("blaze_tablet", val)) {
            FILE *fp = fopen("/sys/class/graphics/fb0/virtual_size", "r");
            if (fp != NULL) {
                size_t len = fread(val, sizeof(char), 10, fp);
                fclose(fp);
                val[len-1] = '\0';
                if (!strcmp(val, "1024,768")) {
                    name = "Tablet1";
                } else if (!strcmp(val, "1280,800")) {
                    name = "Tablet2";
                } else if (!strcmp(val, "1920,1080")) {
                    name = "Tablet2.5";
                }
            }
        } else if (!strcasecmp("panda5", val)) {
            name = "Panda5";
        }
    }
    return name;
}

AString RtspConfig::generateVideoProfile(int videoProfileIdx) {
    AString profile;
    if (mVideoProfiles[videoProfileIdx].valid) {
        profile = StringPrintf("%02X %02X %08X %08X %08X %02X %04X %04X %02X",
                videoProfileIdx == kVideoProfileCbp ? 1 : 2,
                mVideoProfiles[videoProfileIdx].level & 0x0FF,
                mVideoProfiles[videoProfileIdx].formats[kVideoCea],
                mVideoProfiles[videoProfileIdx].formats[kVideoVesa],
                mVideoProfiles[videoProfileIdx].formats[kVideoHh],
                mVideoProfiles[videoProfileIdx].latency & 0x0FF,
                mVideoProfiles[videoProfileIdx].minSliceSize & 0x0FFFF,
                mVideoProfiles[videoProfileIdx].sliceEncParams & 0x0FFFF,
                mVideoProfiles[videoProfileIdx].frameRateControl & 0x0FF);

        if (mVideoProfiles[videoProfileIdx].maxHres == kInvalidValue) {
            profile.append(" none");
        } else {
            profile.append(StringPrintf(" %04x", mVideoProfiles[videoProfileIdx].maxHres & 0x0FFFF));
        }

        if (mVideoProfiles[videoProfileIdx].maxVres == kInvalidValue) {
            profile.append(" none");
        } else {
            profile.append(StringPrintf(" %04x", mVideoProfiles[videoProfileIdx].maxVres & 0x0FFFF));
        }
    }
    return profile;
}

AString RtspConfig::getVideoCaps() {
    AString capabilities;
    if (!mVideoProfiles[kVideoProfileCbp].valid && !mVideoProfiles[kVideoProfileChp].valid) {
        ALOGW("XML video capabilities are empty");
        return capabilities;
    }

    capabilities = StringPrintf("%02X 00 ", mNativeVideo == kInvalidValue ? 0 : mNativeVideo);

    AString profiles;
    for (int idx = 0; idx < kVideoProfilesNum; idx++) {
        if (mVideoProfiles[idx].valid) {
            if (!profiles.empty()) profiles.append(", ");
            profiles.append(generateVideoProfile(idx));
        }
    }
    capabilities.append(profiles);

    ALOGV("XML video capabilities \"%s\"", capabilities.c_str());
    return capabilities;
}

AString RtspConfig::generateAudioFormat(int audioFormatIdx) {
    AString format;
    if (mAudioFormats[audioFormatIdx].valid) {
         format.append(StringPrintf("%s %08X %02X",
                kAudioCodecNames[audioFormatIdx],
                mAudioFormats[audioFormatIdx].formats,
                mAudioFormats[audioFormatIdx].latency));
    }
    return format;
}

AString RtspConfig::getAudioCaps() {
    AString capabilities;
    if (!mAudioFormats[kAudioLpcm].valid && !mAudioFormats[kAudioAac].valid &&
            !mAudioFormats[kAudioAc3].valid) {
        ALOGV("XML audio capabilities are empty");
        return capabilities;
    }

    for (int idx = 0; idx < kAudioFormatsNum; idx++) {
        if (mAudioFormats[idx].valid) {
            if (!capabilities.empty()) capabilities.append(", ");
            capabilities.append(generateAudioFormat(idx));
        }
    }

    ALOGV("XML audio capabilities \"%s\"", capabilities.c_str());
    return capabilities;
}

AString RtspConfig::getUibcCaps() {
    AString capabilities;
    return capabilities;
}

} // namespace android
