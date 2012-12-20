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

#define LOG_TAG "AudioParameters"
#include <utils/Log.h>
#include <media/stagefright/foundation/ADebug.h>

#include "AudioParameters.h"
#include "ElementaryParser.h"

namespace android {

static const int kModesLen = 8;
static const uint32_t kModesMask[] = {
    0x00000003,
    0x0000000F,
    0x00000007,
};

static const int kLatencyLen = 2;
static const uint32_t kLatencyMax = 255;

static const char * kFormatTable[] = {
    "LPCM",
    "AAC",
    "AC3",
    NULL,
};

struct SimpleAudioMode {
    int sampleRate;
    int sampleSize;
    int channelNum;
};

static const SimpleAudioMode kLpcmTable[] = {
    {44100, 16, 2},
    {48000, 16, 2},
};
static const SimpleAudioMode kAacTable[] = {
    {48000, 16, 2},
    {48000, 16, 4},
    {48000, 16, 6},
    {48000, 16, 8},
};
static const SimpleAudioMode kAc3Table[] = {
    {48000, 16, 2},
    {48000, 16, 4},
    {48000, 16, 6},
};

enum { kAudioTableLpcm, kAudioTableAac, kAudioTableAc3 };

static const SimpleAudioMode * kAudioTables[] = {
    kLpcmTable,
    kAacTable,
    kAc3Table,
};
static const int kAudioTableSizes[] = {
    sizeof(kLpcmTable) / sizeof(SimpleAudioMode),
    sizeof(kAacTable) / sizeof(SimpleAudioMode),
    sizeof(kAc3Table) / sizeof(SimpleAudioMode),
};

enum {
    kErrMultiBits = ElementaryParser::kErrMultiBits,
    kErrNoBits    = ElementaryParser::kErrNoBits,
};

static const int kMultiBits = ElementaryParser::kMultiBits;

enum {
    kEndOfLine  = ElementaryParser::kEndOfLine,
    kSpace      = ElementaryParser::kSpace,
    kCommaSpace = ElementaryParser::kCommaSpace,
};

AString AudioMode::toString() {
    AString str = kFormatTable[format];
    str.append(StringPrintf(" %dHz %dbits %dch", sampleRate, sampleSize, channelNum));
    return str;
}

bool AudioMode::operator==(const AudioMode &other) const {
    if (format == other.format && sampleRate == other.sampleRate &&
            sampleSize == other.sampleSize && channelNum == other.channelNum) {
        return true;
    }
    return false;
}

bool AudioMode::operator>(const AudioMode &other) const {
    if (format <= other.format && sampleRate <= other.sampleRate &&
            sampleSize <= other.sampleSize && channelNum <= other.channelNum) {
        return false;
    }
    return true;
}

AudioParameters::AudioParameters() {}

AudioParameters::~AudioParameters() {}

sp<AudioParameters> AudioParameters::parse(const char *data) {
    sp<AudioParameters> params = new AudioParameters();
    status_t err = params->parseParams(data);

    if (err != OK) return NULL;

    return params;
}

status_t AudioParameters::parseParams(const char *data) {
    mAudioCodecs.clear();
    ElementaryParser parser(data);

    do {
        sp<AudioCodec> codec = new AudioCodec();

        // Audio format
        // LPCM 00000003 00
        // ^
        if (!parser.parseStringField(kFormatTable, kSpace, &codec->format)) {
            parser.printError("Invalid wfd-audio-codecs");
            return ERROR_MALFORMED;
        }

        // Audio modes
        // LPCM 00000003 00
        //      ^
        if (!parser.parseHexBitField(kModesLen, kModesMask[codec->format],
                kMultiBits, kSpace, &codec->modes) || codec->modes == 0) {
            parser.printError("Invalid audio modes");
            return ERROR_MALFORMED;
        }

        // Audio latency
        // LPCM 00000003 00
        //               ^
        if (!parser.parseHexValue(kLatencyLen, kLatencyMax,
                kEndOfLine | kCommaSpace, &codec->latency)) {
            parser.printError("Invalid audio latency");
            return ERROR_MALFORMED;
        }

        mAudioCodecs.push_back(codec);
    } while (parser.getLastDelimiter() != kEndOfLine);

    return OK;
}

sp<AudioMode> AudioParameters::applyAudioMode(const char * data) {
    // Parse new audio parmeters
    sp<AudioParameters> newParams = AudioParameters::parse(data);
    if (newParams == NULL) return NULL;

    // We must have only one audio parameters set
    int size = newParams->mAudioCodecs.size();
    if (size > 1) {
        ALOGE("SET_PARAMETER must use only one set of audio parameters for setup");
        return NULL;
    }
    const sp<AudioCodec> &newCodec = *(newParams->mAudioCodecs.begin());

    // Check all set parameters has a correct bitmask
    int idxMode = ElementaryParser::getBitIndex(newCodec->modes, kModesMask[newCodec->format]);
    if (idxMode == kErrMultiBits || idxMode == kErrNoBits) {
        ALOGE("Incorrect Audio mode value, must have one bit set");
        return NULL;
    }

    // Check all supported profiles to find suitable one
    List< sp<AudioCodec> >::iterator it = mAudioCodecs.begin();
    while (it != mAudioCodecs.end()) {
        const sp<AudioCodec> &capCodec = *it++;
        if (capCodec->format != newCodec->format) continue;
        if (!(capCodec->modes & newCodec->modes)) continue;
        break;
    }
    if (it == mAudioCodecs.end()) {
        ALOGD("Suitable audio profile is not found");
        return NULL;
    }

    // Fill audio codec params
    sp<AudioMode> mode = new AudioMode();
    mode->format = newCodec->format;
    mode->sampleRate = kAudioTables[newCodec->format][idxMode].sampleRate;
    mode->sampleSize = kAudioTables[newCodec->format][idxMode].sampleSize;
    mode->channelNum = kAudioTables[newCodec->format][idxMode].channelNum;
    return mode;
}

AString AudioParameters::generateAudioFormat(const sp<AudioCodec> &codec) {
    AString s = StringPrintf("%s %08x %02x", kFormatTable[codec->format],
            codec->modes, codec->latency);
    return s;
}

AString AudioParameters::generateAudioFormats() {
    AString s;

    List< sp<AudioCodec> >::iterator it = mAudioCodecs.begin();
    while (it != mAudioCodecs.end()) {
        s.append(generateAudioFormat(*it++));
        if (it != mAudioCodecs.end()) s.append(", ");
    }

    return s;
}

sp<AudioParameters::AudioTable> AudioParameters::checkMode(const sp<AudioMode> &mode) {
    for (int idx = 0; idx < kAudioTableSizes[mode->format]; idx++) {
        if (mode->sampleRate == kAudioTables[mode->format][idx].sampleRate &&
                mode->sampleSize == kAudioTables[mode->format][idx].sampleSize &&
                mode->channelNum == kAudioTables[mode->format][idx].channelNum) {
            sp<AudioTable> audioTable = new AudioTable();
            audioTable->table = mode->format;
            audioTable->index = idx;
            return audioTable;
        }
    }
    return NULL;
}

AString AudioParameters::generateAudioMode(const sp<AudioMode> &mode) {
    AString s;
    sp<AudioTable> at = checkMode(mode);
    if (at == NULL) {
        ALOGE("Appropriate audio has not been found %s", mode->toString().c_str());
        return s;
    }
    sp<AudioCodec> params = new AudioCodec();
    params->format = mode->format;
    params->modes = 1 << at->index;
    params->latency = 0;
    s.append(generateAudioFormat(params));
    return s;
}

sp<AudioMode> AudioParameters::getBestAudioMode(const sp<AudioParameters> &sinkParams,
        const sp<AudioMode> &desiredMode) {
    if (sinkParams == NULL) return NULL;

    // Create list of all possible audio modes between source and sink
    List< sp<AudioMode> > modeList;
    List< sp<AudioCodec> >::iterator selfCodecsItr = mAudioCodecs.begin();
    while (selfCodecsItr != mAudioCodecs.end()) {
        const sp<AudioCodec> &selfCodec = *selfCodecsItr++;

        List< sp<AudioCodec> >::iterator remoteCodecsItr = sinkParams->mAudioCodecs.begin();
        while (remoteCodecsItr != sinkParams->mAudioCodecs.end()) {
            const sp<AudioCodec> &remoteCodec = *remoteCodecsItr++;
            if (selfCodec->format != remoteCodec->format) continue;

            uint32_t matchingModes = selfCodec->modes & remoteCodec->modes;
            for (int i = 0; matchingModes; ++i, matchingModes >>= 1) {
                if (!(matchingModes & 1)) continue;
                sp<AudioMode> audioMode = new AudioMode();
                audioMode->format = selfCodec->format;
                audioMode->sampleRate = kAudioTables[selfCodec->format][i].sampleRate;
                audioMode->sampleSize = kAudioTables[selfCodec->format][i].sampleSize;
                audioMode->channelNum = kAudioTables[selfCodec->format][i].channelNum;
                modeList.push_back(audioMode);
            }
        }
    }

    // Check if desired audio mode is in list of capable audio modes
    if (desiredMode != NULL) {
        ALOGV("Check if desired audio mode is in list of capable audio modes %s",
                desiredMode->toString().c_str());
        List< sp<AudioMode> >::iterator it = modeList.begin();
        while (it != modeList.end()) {
            const sp<AudioMode> &capableMode = *it++;
            ALOGD("%s", capableMode->toString().c_str());
            if (*capableMode.get() == *desiredMode.get()) {
                ALOGV("Desired and best audio mode %s", capableMode->toString().c_str());
                return desiredMode;
            }
        }
    }

    // Do choice of best video mode
    ALOGV("Do choice of best audio mode");
    List< sp<AudioMode> >::iterator it = modeList.begin();
    sp<AudioMode> &bestMode = *it++;
    while (it != modeList.end()) {
        const sp<AudioMode> &mode = *it++;
        if (*mode.get() > *bestMode.get()) {
            bestMode = mode;
        }
        ALOGV("%s", mode->toString().c_str());
    }
    ALOGV("Best audio mode %s", bestMode->toString().c_str());
    return bestMode;
}

}  // namespace android
