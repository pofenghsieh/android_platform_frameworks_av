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

#define LOG_TAG "VideoParameters"

#include <stdlib.h>

#include <utils/Log.h>
#include <media/stagefright/foundation/ADebug.h>

#include "VideoParameters.h"
#include "ElementaryParser.h"
#include "OMX_Video.h"

namespace android {

static const int kNativeLen = 2;
static const uint32_t kNativeMax = 255;
static const uint32_t kNativeTableMask = 0x07;
static const uint32_t kNativeModeOffset = 3;

static const int kPrefDispModeSupportedLen = 2;
static const uint32_t kPrefDispModeSupportedMax = 1;

static const int kProfileLen = 2;
static const uint32_t kProfileMask = 0x03;

static const int kLevelLen = 2;
static const uint32_t kLevelMask = 0x1F;

static const int kCeaLen = 8;
static const uint32_t kCeaMask = 0x0001FFFF;

static const int kVesaLen = 8;
static const uint32_t kVesaMask = 0x3FFFFFFF;

static const int kHhLen = 8;
static const uint32_t kHhMask = 0x00000FFF;

static const int kLatencyLen = 2;
static const uint32_t kLatencyMax = 255;

static const int kMinSliceSizeLen = 4;
static const uint32_t kMinSliceSizeMax = 256 * 256 - 1;

static const int kSliceEncLen = 4;
static const uint32_t kSliceEncMask = 0x1FFF;

static const int kFrameRateControlLen = 2;
static const uint32_t kFrameRateControlMask = 0x1F;

const int kLevelTable[5] = {
    OMX_VIDEO_AVCLevel31,
    OMX_VIDEO_AVCLevel32,
    OMX_VIDEO_AVCLevel4,
    OMX_VIDEO_AVCLevel41,
    OMX_VIDEO_AVCLevel42,
};

const char *kLevelPresentationTable[5] = {
    "3.1", "3.2", "4", "4.1", "4.2",
};

static const SimpleVideoMode kCeaTable[] = {
    { 640,  480, 60}, { 720,  480, 60},
    { 720,  480, 60}, { 720,  576, 50},
    { 720,  576, 50}, {1280,  720, 30},
    {1280,  720, 60}, {1920, 1080, 30},
    {1920, 1080, 60}, {1920, 1080, 60},
    {1280,  720, 25}, {1280,  720, 50},
    {1920, 1080, 25}, {1920, 1080, 50},
    {1920, 1080, 50}, {1280,  720, 24},
    {1920, 1080, 24},
};
static const SimpleVideoMode kVesaTable[] = {
    { 800,  600, 30}, { 800,  600, 60},
    {1024,  768, 30}, {1024,  768, 60},
    {1152,  864, 30}, {1152,  864, 60},
    {1280,  768, 30}, {1280,  768, 60},
    {1280,  800, 30}, {1280,  800, 60},
    {1360,  768, 30}, {1360,  768, 60},
    {1366,  768, 30}, {1366,  768, 60},
    {1280, 1024, 30}, {1280, 1024, 60},
    {1400, 1050, 30}, {1400, 1050, 60},
    {1440,  900, 30}, {1440,  900, 60},
    {1600,  900, 30}, {1600,  900, 60},
    {1600, 1200, 30}, {1600, 1200, 60},
    {1680, 1024, 30}, {1680, 1024, 60},
    {1680, 1050, 30}, {1680, 1050, 60},
    {1920, 1200, 30}, {1920, 1200, 60},
};
static const SimpleVideoMode kHhTable[] = {
    {800, 480, 30}, {800, 480, 60},
    {854, 480, 30}, {854, 480, 60},
    {864, 480, 30}, {864, 480, 60},
    {640, 360, 30}, {640, 360, 60},
    {960, 540, 30}, {960, 540, 60},
    {848, 480, 30}, {848, 480, 60},
};

enum { kVideoTableCea, kVideoTableVesa, kVideoTableHh };

static const SimpleVideoMode * kVideoTables[] = {
    kCeaTable,
    kVesaTable,
    kHhTable,
};
static const size_t kVideoTableSizes[] = {
    sizeof(kCeaTable) / sizeof(SimpleVideoMode),
    sizeof(kVesaTable) / sizeof(SimpleVideoMode),
    sizeof(kHhTable) / sizeof(SimpleVideoMode),
};

enum { kCbp = 1, kChp = 2 };

enum {
    kErrMultiBits = ElementaryParser::kErrMultiBits,
    kErrNoBits    = ElementaryParser::kErrNoBits,
};

enum {
    kMultiBits       = ElementaryParser::kMultiBits,
    kSingleBit       = ElementaryParser::kSingleBit,
    kSingleBitOrZero = ElementaryParser::kSingleBitOrZero,
};

enum {
    kEndOfLine  = ElementaryParser::kEndOfLine,
    kSpace      = ElementaryParser::kSpace,
    kCommaSpace = ElementaryParser::kCommaSpace,
};

VideoParameters::VideoParameters() {}

VideoParameters::~VideoParameters() {}

sp<VideoParameters> VideoParameters::parse(const char *data) {
    sp<VideoParameters> params = new VideoParameters();
    status_t err = params->parseParams(data);

    if (err != OK) return NULL;

    return params;
}

status_t VideoParameters::parseParams(const char *data) {
    mH264Codecs.clear();
    ElementaryParser parser(data);

    // Native resolution/refresh rates bitmap
    // 00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 none none
    // ^
    uint32_t native;
    if (!parser.parseHexValue(kNativeLen, kNativeMax, kSpace, &native)) {
        parser.printError("Invalid wfd-video-formats native resolution field");
        return ERROR_MALFORMED;
    }

    uint32_t idxVideoTable = native & kNativeTableMask;
    uint32_t idxVideoMode = native >> kNativeModeOffset;
    if (idxVideoTable <= kVideoTableHh && idxVideoMode < kVideoTableSizes[idxVideoTable]) {
        mNativeMode = kVideoTables[idxVideoTable][idxVideoMode];
        ALOGV("Native mode %dx%d %dHz", mNativeMode.width, mNativeMode.height, mNativeMode.frameRate);
    } else {
        parser.printError("Invalid wfd-video-formats native resolution field");
        return ERROR_MALFORMED;
    }

    // Preferred display mode supported
    // 00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 none none
    //    ^
    if (!parser.parseHexValue(kPrefDispModeSupportedLen,
            kPrefDispModeSupportedMax, kSpace, &mPrefDispModeSupported)) {
        parser.printError("Invalid wfd-video-formats preferred display mode support field");
        return ERROR_MALFORMED;
    }

    if (mPrefDispModeSupported) {
        ALOGE("We don't support Preferred Display Mode");
        return ERROR_MALFORMED;
    }

    do {
        sp<H264Codec> codec = new H264Codec();

        // H264-codec profile bitmask
        // 00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 none none
        //       ^
        if (!parser.parseHexBitField(kProfileLen, kProfileMask,
                kSingleBit, kSpace, &codec->profile)) {
            parser.printError("Invalid H264-codec profile");
            return ERROR_MALFORMED;
        }

        // H264-codec level bitmask
        // 00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 none none
        //          ^
        if (!parser.parseHexBitField(kLevelLen, kLevelMask,
                kSingleBit, kSpace, &codec->level)) {
            parser.printError("Invalid H264-codec level");
            return ERROR_MALFORMED;
        }

        // H264-codec CEA resolutions bitmask
        // 00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 none none
        //             ^
        if (!parser.parseHexBitField(kCeaLen, kCeaMask,
                kMultiBits, kSpace, &codec->cea)) {
            parser.printError("Invalid H264-codec CEA resolutions");
            return ERROR_MALFORMED;
        }

        // H264-codec VESA resolutions bitmask
        // 00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 none none
        //                      ^
        if (!parser.parseHexBitField(kVesaLen, kVesaMask,
                kMultiBits, kSpace, &codec->vesa)) {
            parser.printError("Invalid H264-codec VESA resolutions");
            return ERROR_MALFORMED;
        }

        // H264-codec HH resolutions bitmask
        // 00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 none none
        //                               ^
        if (!parser.parseHexBitField(kHhLen, kHhMask,
                kMultiBits, kSpace, &codec->hh)) {
            parser.printError("Invalid H264-codec HH resolutions");
            return ERROR_MALFORMED;
        }

        if (codec->cea == 0 && codec->vesa == 0 && codec->hh == 0) {
            parser.printError("No one of CEA, VESA and HH resolutions has been set");
            return ERROR_MALFORMED;
        }

        // H264-codec latency
        // 00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 none none
        //                                        ^
        if (!parser.parseHexValue(kLatencyLen, kLatencyMax,
                kSpace, &codec->latency)) {
            parser.printError("Invalid decoder latency value");
            return ERROR_MALFORMED;
        }

        // H264-codec minimum slice size field
        // 00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 none none
        //                                           ^
        if (!parser.parseHexValue(kMinSliceSizeLen, kMinSliceSizeMax,
                kSpace, &codec->minSliceSize)) {
            parser.printError("Invalid min‐slice‐size value");
            return ERROR_MALFORMED;
        }

        // H264-codec slice encoding parameters
        // 00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 none none
        //                                                ^
        if (!parser.parseHexBitField(kSliceEncLen, kSliceEncMask,
                kMultiBits, kSpace, &codec->sliceEncParams)) {
            parser.printError("Invalid slice encoding parameters bitmap");
            return ERROR_MALFORMED;
        }

        // H264-codec video frame rate control support
        // 00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 none none
        //                                                     ^
        if (!parser.parseHexBitField(kFrameRateControlLen, kFrameRateControlMask,
                kMultiBits, kSpace, &codec->frameRateControl)) {
            parser.printError("Invalid video frame rate control support bitmap");
            return ERROR_MALFORMED;
        }

        // H264-codec MaxHres
        // 00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 none none
        //                                                        ^
        if (!parser.checkStringField("none", kSpace)) {
            parser.printError("Invalid wfd-video-formats codec MaxHres");
            return ERROR_MALFORMED;
        }
        codec->maxHres = 0;

        // H264-codec MaxVres
        // 00 00 01 01 00000001 00000000 00000000 00 0000 0000 00 none none
        //                                                             ^
        if (!parser.checkStringField("none", kEndOfLine | kCommaSpace)) {
            parser.printError("Invalid wfd-video-formats codec MaxVres");
            return ERROR_MALFORMED;
        }
        codec->maxVres = 0;

        mH264Codecs.push_back(codec);
    } while (parser.getLastDelimiter() != kEndOfLine);

    return OK;
}

sp<VideoMode> VideoParameters::applyVideoMode(const char * data) {
    // Parse new video parameters
    sp<VideoParameters> newParams = VideoParameters::parse(data);
    if (newParams == NULL) return NULL;

    // We must have only one video parameters set
    int size = newParams->mH264Codecs.size();
    if (size > 1) {
        ALOGE("SET_PARAMETER must use only one set of video parameters for setup");
        return NULL;
    }
    const sp<H264Codec> &newCodec = *(newParams->mH264Codecs.begin());

    // Need to ignore next fields: native, preferred‐display‐mode‐supported
    // latency, min‐slice‐size, slice‐enc‐params, frame‐rate‐control‐support,
    // max‐hres, max‐vres

    // Check all set parameters has a correct bitmask
    int idxProfile = ElementaryParser::getBitIndex(newCodec->profile, kProfileMask);
    if (idxProfile == kErrMultiBits || idxProfile == kErrNoBits) {
        ALOGE("Incorrect H264 profile value, must have one setted bit");
        return NULL;
    }

    int idxLevel = ElementaryParser::getBitIndex(newCodec->level, kLevelMask);
    if (idxLevel == kErrMultiBits || idxLevel == kErrNoBits) {
        ALOGE("Incorrect H264 level value, must have one setted bit");
        return NULL;
    }

    // Fields CEA‐Support, VESA‐Support and HH-Support must have one
    // bit set bit for all of them
    int idxVideoTable = 0;
    int idxCea = ElementaryParser::getBitIndex(newCodec->cea, kCeaMask);
    if (idxCea == kErrMultiBits) {
        ALOGE("Incorrect CEA‐Support value, must have maximim one setted bit");
        return NULL;
    } else if (idxCea != kErrNoBits) idxVideoTable += 1 << kVideoTableCea;

    int idxVesa = ElementaryParser::getBitIndex(newCodec->vesa, kVesaMask);
    if (idxVesa == kErrMultiBits) {
        ALOGE("Incorrect VESA‐Support value, must have maximim one setted bit");
        return NULL;
    } else if (idxVesa != kErrNoBits) idxVideoTable += 1 << kVideoTableVesa;

    int idxHh = ElementaryParser::getBitIndex(newCodec->hh, kHhMask);
    if (idxHh == kErrMultiBits) {
        ALOGE("Incorrect HH‐Support value, must have maximim one setted bit");
        return NULL;
    } else if (idxHh != kErrNoBits) idxVideoTable += 1 << kVideoTableHh;

    // Check only one resolution set to 1
    idxVideoTable = ElementaryParser::getBitIndex(idxVideoTable, kNativeTableMask);
    if (idxVideoTable == kErrMultiBits || idxVideoTable == kErrNoBits) {
        ALOGE("Incorrect video mode values (CEA, VESA, HH), must have one setted bit");
        return NULL;
    }

    // Check all supported profiles to find suitable one
    List< sp<H264Codec> >::iterator it = mH264Codecs.begin();
    while (it != mH264Codecs.end()) {
        const sp<H264Codec> &capCodec = *it++;
        if (!(capCodec->profile & newCodec->profile)) continue;
        if (!(capCodec->level & newCodec->level)) continue;
        switch (idxVideoTable) {
            case kVideoTableCea:
                if (!(capCodec->cea & newCodec->cea)) continue;
                break;
            case kVideoTableVesa:
                if (!(capCodec->vesa & newCodec->vesa)) continue;
                break;
            case kVideoTableHh:
                if (!(capCodec->hh & newCodec->hh)) continue;
                break;
        }
        break;
    }
    if (it == mH264Codecs.end()) return NULL;

    // Fill video codec params
    sp<VideoMode> mode = new VideoMode();
    mode->h264HighProfile = idxProfile == 1 ? true : false;
    mode->h264Level = kLevelTable[idxLevel];

    int idxVideoMode = 0;
    switch (idxVideoTable) {
        case kVideoTableCea:
            idxVideoMode = idxCea;
            break;
        case kVideoTableVesa:
            idxVideoMode = idxVesa;
            break;
        case kVideoTableHh:
            idxVideoMode = idxHh;
            break;
    }

    mode->width = kVideoTables[idxVideoTable][idxVideoMode].width;
    mode->height = kVideoTables[idxVideoTable][idxVideoMode].height;
    mode->frameRate = kVideoTables[idxVideoTable][idxVideoMode].frameRate;

    return mode;
}

AString VideoParameters::generateH264Format(const sp<H264Codec> &params) {
    AString tmp;
    AString s = StringPrintf("%02x %02x %08x %08x %08x %02x %04x %04x %02x ",
            params->profile, params->level, params->cea, params->vesa,
            params->hh, params->latency, params->minSliceSize,
            params->sliceEncParams, params->frameRateControl);

    if (params->maxHres == 0) {
        tmp = "none ";
    } else {
        tmp = StringPrintf("%04x ", params->maxHres);
    }
    s.append(tmp);

    if (params->maxVres == 0) {
        tmp = "none";
    } else {
        tmp = StringPrintf("%04x", params->maxVres);
    }
    s.append(tmp);

    return s;
}

AString VideoParameters::generateVideoFormats() {
    AString s = StringPrintf("%02x %02x ", mNative, mPrefDispModeSupported ? 1 : 0);

    List< sp<H264Codec> >::iterator it = mH264Codecs.begin();
    while (it != mH264Codecs.end()) {
        sp<H264Codec> &params = *it++;
        s.append(generateH264Format(params));
        if (it != mH264Codecs.end()) s.append(", ");
    }

    return s;
}

uint8_t VideoParameters::getLevel(int level) {
    for (uint i = 0, b = 1; i < sizeof(kLevelTable) / sizeof(int); ++i, b <<= 1) {
        if (kLevelTable[i] == level) return b;
    }
    ALOGE("Not supported H264 level value 0x%2x", level);
    return 0;
}

sp<VideoParameters::VideoTable> VideoParameters::checkResolution(const sp<VideoMode> &mode) {
    // Looking for appropriate video resolution and frame rate
    for (int table = 0; table <= kVideoTableVesa; table++) {
        for (uint32_t idx = 0; idx < kVideoTableSizes[table]; idx++) {
            if (mode->width == kVideoTables[table][idx].width &&
                    mode->height == kVideoTables[table][idx].height &&
                    mode->frameRate == kVideoTables[table][idx].frameRate) {
                sp<VideoTable> videoTable = new VideoTable;
                videoTable->table = table;
                videoTable->index = idx;
                return videoTable;
            }
        }
    }
    return NULL;
}

AString VideoParameters::generateVideoMode(const sp<VideoMode> &mode) {
    AString s;
    sp<VideoTable> vt = checkResolution(mode);
    if (vt == NULL) {
        ALOGE("Appropriate resolution has not been found (%dx%dx%d)",
                mode->width, mode->height, mode->frameRate);
        return s;
    }

    sp<H264Codec> params = new H264Codec();

    params->profile = mode->h264HighProfile ? kChp : kCbp;
    params->level = getLevel(mode->h264Level);
    if (params->level == 0) return s;

    params->cea = 0;
    params->vesa = 0;
    params->hh = 0;

    switch (vt->table) {
        case kVideoTableCea:
            params->cea = 1 << vt->index;
            break;
        case kVideoTableVesa:
            params->vesa = 1 << vt->index;
            break;
        case kVideoTableHh:
            params->hh = 1 << vt->index;
            break;
    }

    params->latency = 0;
    params->minSliceSize = 0;
    params->sliceEncParams = 0;
    params->frameRateControl = 0;
    params->maxHres = 0;
    params->maxVres = 0;

    s = "00 00 ";
    s.append(generateH264Format(params));
    return s;
}

List< sp<VideoParameters::H264Codec> > * VideoParameters::getCodecs() {
    return &mH264Codecs;
}

sp<VideoMode> VideoParameters::getBestVideoMode(
        const sp<VideoParameters> &sinkParams, const sp<VideoMode> &desiredMode) {
    if (sinkParams == NULL) return NULL;

    // Create list of all possible video modes between source and sink
    List< sp<VideoMode> > modeList;
    List< sp<H264Codec> >::iterator itSelf = mH264Codecs.begin();
    while (itSelf != mH264Codecs.end()) {
        const sp<H264Codec> &selfCodec = *itSelf++;

        List< sp<H264Codec> >::iterator itRemote = sinkParams->getCodecs()->begin();
        while (itRemote != sinkParams->getCodecs()->end()) {
            const sp<H264Codec> &remoteCodec = *itRemote++;

            if (!(selfCodec->profile & remoteCodec->profile)) continue;

            uint32_t matchingModes[3];
            matchingModes[kVideoTableCea] = selfCodec->cea & remoteCodec->cea;
            matchingModes[kVideoTableVesa] = selfCodec->vesa & remoteCodec->vesa;
            matchingModes[kVideoTableHh] = selfCodec->hh & remoteCodec->hh;
            if (!matchingModes[kVideoTableCea] &&
                    !matchingModes[kVideoTableVesa] && !matchingModes[kVideoTableHh]) {
                continue;
            }

            // Create list of all possible modes
            for (int table = kVideoTableCea; table <= kVideoTableHh; table++) {
                for (int i = 0; matchingModes[table]; ++i, matchingModes[table] >>= 1) {
                    if (!(matchingModes[table] & 1)) continue;
                    sp<VideoMode> videoMode = new VideoMode();
                    videoMode->h264HighProfile = selfCodec->profile & kChp ? true : false;
                    videoMode->h264Level = selfCodec->level < remoteCodec->level ?
                            selfCodec->level : remoteCodec->level;
                    videoMode->h264Level = kLevelTable[ElementaryParser::getBitIndex(
                            videoMode->h264Level, kLevelMask)];
                    videoMode->width = kVideoTables[table][i].width;
                    videoMode->height = kVideoTables[table][i].height;
                    videoMode->frameRate = kVideoTables[table][i].frameRate;
                    modeList.push_back(videoMode);
                }
            }
        }
    }

    // Check if desired video mode is in list of capable video modes
    if (desiredMode != NULL) {
        ALOGV("Check if desired video mode is in list of capable video modes %s",
                desiredMode->toString().c_str());
        List< sp<VideoMode> >::iterator it = modeList.begin();
        while (it != modeList.end()) {
            sp<VideoMode> capableMode = *it++;
            if (*capableMode.get() == *desiredMode.get()) {
                ALOGV("Desired and best video mode %s", capableMode->toString().c_str());
                return capableMode;
            }
        }
    }

    // Check if sink native video mode is in list of capable video modes
    List< sp<VideoMode> >::iterator it = modeList.begin();
    while (it != modeList.end()) {
        const sp<VideoMode> &capableMode = *it++;
        if ((*capableMode.get()).width == sinkParams->mNativeMode.width &&
                (*capableMode.get()).height == sinkParams->mNativeMode.height &&
                (*capableMode.get()).frameRate == sinkParams->mNativeMode.frameRate) {
            return capableMode;
        }
    }

    // Do choice of best video mode
    it = modeList.begin();
    sp<VideoMode> &bestMode = *it++;
    while (it != modeList.end()) {
        const sp<VideoMode> &mode = *it++;
        if (*mode.get() > *bestMode.get()) {
            bestMode = mode;
        }
    }
    ALOGV("Best video mode %s", bestMode->toString().c_str());
    return bestMode;
}

AString VideoMode::toString() {
    AString str;
    if (h264HighProfile) {
        str.append("CHP");
    } else {
        str.append("CBP");
    }

    for (uint i = 0; i < sizeof(kLevelPresentationTable) / sizeof(char *); i++) {
        if (kLevelTable[i] == h264Level) {
            str.append(StringPrintf(" %s", kLevelPresentationTable[i]));
            break;
        }
    }
    str.append(StringPrintf(" %dx%d %dHz", width, height, frameRate));
    return str;
}

bool VideoMode::operator==(const VideoMode &other) const {
    if (h264HighProfile == other.h264HighProfile &&
            width == other.width &&
            height == other.height &&
            frameRate == other.frameRate) {
        return true;
    }
    return false;
}

bool VideoMode::operator>(const VideoMode &other) const {
    if (width <= other.width &&
            height <= other.height &&
            frameRate <= other.frameRate &&
            (!h264HighProfile || other.h264HighProfile)) {
        return false;
    }
    return true;
}

}  // namespace android
