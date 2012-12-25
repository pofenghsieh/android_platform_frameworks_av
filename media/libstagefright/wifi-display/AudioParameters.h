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

#ifndef AUDIO_PARAMETERS_H_

#define AUDIO_PARAMETERS_H_

#include <utils/RefBase.h>
#include <utils/List.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/MediaErrors.h>

namespace android {

struct AudioMode : public RefBase {
    enum {
        kLpcmAudioFormat,
        kAacAudioFormat,
        kAc3AudioFormat,
    };

    int format;
    int sampleRate;
    int sampleSize;
    int channelNum;

    AString toString();
    bool operator==(const AudioMode &other) const;
    bool operator>(const AudioMode &other) const;
};

struct AudioParameters : public RefBase {
private:

    struct AudioCodec : public LightRefBase<AudioCodec> {
        uint32_t format;
        uint32_t modes;
        uint32_t latency;
    };

    struct AudioTable : public LightRefBase<AudioTable> {
        int table;
        int index;
    };

public:
    static sp<AudioParameters> parse(const char *data);
    sp<AudioMode> applyAudioMode(const char *data);
    AString generateAudioFormats();
    AString generateAudioMode(const sp<AudioMode> &mode);
    sp<AudioMode> getBestAudioMode(const sp<AudioParameters> &sinkParams,
            const sp<AudioMode> &desiredMode);

protected:
    virtual ~AudioParameters();

private:
    List< sp<AudioCodec> > mAudioCodecs;

    AudioParameters();
    status_t parseParams(const char *data);
    AString generateAudioFormat(const sp<AudioCodec> &params);
    sp<AudioParameters::AudioTable> checkMode(const sp<AudioMode> &mode);
};

}  // namespace android

#endif  // AUDIO_PARAMETERS_H_
