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

#define LOG_TAG "UibcParameters"
#include <utils/Log.h>
#include <media/stagefright/foundation/ADebug.h>

#include <stdlib.h>

#include "UibcParameters.h"
#include "ElementaryParser.h"

namespace android {

static const char * kCategory[] = {
    "GENERIC",
    "HIDC",
    "none",
    NULL,
};

enum { kCategoryGeneric, kCategoryHidc, kCategoryNone };

static const char * kInputType[] = {
    "Keyboard",
    "Mouse",
    "SingleTouch",
    "MultiTouch",
    "Joystick",
    "Camera",
    "Gesture",
    "RemoteControl",
    NULL,
};
static const char * kInputPath[] = {
    "Infrared",
    "USB",
    "BT",
    "Zigbee",
    "Wi-Fi",
    "No-SP",
    NULL,
};
static const int kInputPathUndefined = -1;

enum {
    kEndOfLine      = ElementaryParser::kEndOfLine,
    kCommaSpace     = ElementaryParser::kCommaSpace,
    kSemicolonSpace = ElementaryParser::kSemicolonSpace,
    kSlash          = ElementaryParser::kSlash,
    kEqualSign      = ElementaryParser::kEqualSign,
};

sp<UibcParameters> UibcParameters::parse(const char * data) {
    sp<UibcParameters> params = new UibcParameters();
    status_t err = params->parseParams(data);

    if (err != OK) {
        return NULL;
    }

    return params;
}

sp<UibcParameters> UibcParameters::applyUibcParameters(const char *data) {
    if (!mSupported) return NULL;
    // Parse new UIBC parmeters
    sp<UibcParameters> newParams = UibcParameters::parse(data);
    if (newParams == NULL) return NULL;
    // Check UIBC is supported
    if (!newParams->mSupported) return NULL;
    // Check Generic parameters are used and has cross at valid points
    if (isGenericUsed() && newParams->isGenericUsed()) {
        for (int i = 0; i < kNumInputTypes; i++) {
            if (!mGeneric[i] && newParams->mGeneric[i]) {
                ALOGE("Generic type %s is not supported by sink", kInputType[i]);
                return NULL;
            }
        }
    }
    // Check Generic parameters are used and has cross at valid points
    if (isHidcUsed() && newParams->isHidcUsed()) {
        for (int i = 0; i < kNumInputTypes; i++) {
            if (newParams->mHidc[i] != kInputPathUndefined && newParams->mHidc[i] != mHidc[i]) {
                ALOGE("HIDC type or path %s/%s is not supported by sink",
                        kInputType[i], kInputPath[newParams->mHidc[i]]);
                return NULL;
            }
        }
    }
    return newParams;
}

AString UibcParameters::generateUibcCapability() {
    AString s;
    if (!mSupported) {
        s = "none";
        return s;
    }

    // Add input_category_list
    s.append("input_category_list=");
    if (isGenericUsed() && isHidcUsed()) {
        s.append("GENERIC, HIDC; ");
    } else if (isGenericUsed()) {
        s.append("GENERIC; ");
    } else if (isHidcUsed()) {
        s.append("HIDC; ");
    } else {
        s.append("none; ");
    }

    // Add generic_cap_list
    s.append("generic_cap_list=");
    bool hasItems = false;
    for (int i = 0; i < kNumInputTypes; i++) {
        if (mGeneric[i]) {
            if (hasItems) s.append(", ");
            hasItems = true;
            s.append(kInputType[i]);
        }
    }
    if (!hasItems) {
        s.append("none; ");
    } else {
        s.append("; ");
    }

    // Add hidc_cap_list
    s.append("hidc_cap_list=");
    hasItems = false;
    for (int i = 0; i < kNumInputTypes; i++) {
        if (mHidc[i] >= 0) {
            if (hasItems) s.append(", ");
            hasItems = true;
            s.append(kInputType[i]);
            s.append("/");
            s.append(kInputPath[mHidc[i]]);
        }
    }
    if (!hasItems) {
        s.append("none; ");
    } else {
        s.append("; ");
    }

    // Add port number
    s.append("port=");
    if (mPort > 0) {
        s.append(StringPrintf("%d", mPort));
    } else {
        s.append("none");
    }
    return s;
}

sp<UibcParameters> UibcParameters::selectUibcParams(const sp<UibcParameters> &sinkParams) {
    if (sinkParams == NULL || !sinkParams->mSupported) return NULL;

    sp<UibcParameters> newParams = new UibcParameters();

    for (int i = 0; i < kNumInputTypes; i++) {
        if (mGeneric[i] && sinkParams->mGeneric[i]) {
            newParams->mGeneric[i] = true;
        }
    }

    for (int i = 0; i < kNumInputTypes; i++) {
        if (mHidc[i] != kInputPathUndefined && sinkParams->mHidc[i] != kInputPathUndefined) {
            newParams->mHidc[i] = sinkParams->mHidc[i];
        }
    }

    newParams->mPort = mPort;
    newParams->mSupported = true;

    return newParams;
}

UibcParameters::UibcParameters() {
    for (int i = 0; i < kNumInputTypes; i++) {
        mGeneric[i] = false;
        mHidc[i] = kInputPathUndefined;
    }
    mPort = 0;
    mSupported = false;
}

status_t UibcParameters::parseParams(const char *data) {
    ElementaryParser parser(data);

    // none
    if (parser.checkStringField("none", kEndOfLine)) return OK;

    // input_category_list=none;
    // input_category_list=GENERIC;
    // input_category_list=HIDC;
    // input_category_list=GENERIC, HIDC;
    if (!parser.checkStringField("input_category_list", kEqualSign)) {
        parser.printError("Tag \"input_category_list=\" is absent");
        return ERROR_MALFORMED;
    }

    uint32_t value;
    if (!parser.parseStringField(kCategory, kCommaSpace | kSemicolonSpace, &value)) {
        parser.printError("Tags GENERIC, HIDC or none is not found");
        return ERROR_MALFORMED;
    }

    bool genericSupport = false;
    bool hidcSupport = false;
    switch (value) {
        case kCategoryNone:
            if (parser.getLastDelimiter() != kSemicolonSpace) {
                parser.printError("Tag none must be delimited by semicolon");
                return ERROR_MALFORMED;
            }
            break;
        case kCategoryGeneric:
            genericSupport = true;
            break;
        case kCategoryHidc:
            hidcSupport = true;
            break;
    }

    if (value != kCategoryNone && parser.getLastDelimiter() == kCommaSpace) {
        if (!parser.parseStringField(kCategory, kSemicolonSpace, &value) ||
                value == kCategoryNone) {
            parser.printError("Tags GENERIC or HIDC is not found");
            return ERROR_MALFORMED;
        }

        if (value == kCategoryGeneric) {
            if  (genericSupport) {
                parser.printError("Double GENERIC tag has been found");
                return ERROR_MALFORMED;
            } else {
                genericSupport = true;
            }
        }

        if (value == kCategoryHidc) {
            if (hidcSupport) {
                parser.printError("Double HIDC tag has been found");
                return ERROR_MALFORMED;
            } else {
                hidcSupport = true;
            }
        }
    }

    //generic_cap_list=none;
    //generic_cap_list=Mouse, SingleTouch;
    if (!parser.checkStringField("generic_cap_list", kEqualSign)) {
        parser.printError("Tag \"generic_cap_list=\" is absent");
        return ERROR_MALFORMED;
    }

    if (!genericSupport) {
        if (!parser.checkStringField("none", kSemicolonSpace)) {
            parser.printError("Tag \"generic_cap_list=\" must be \"none; \"");
            return ERROR_MALFORMED;
        }
    } else {
        do {
            if (!parser.parseStringField(kInputType, kCommaSpace | kSemicolonSpace, &value)) {
                parser.printError("Generic input type list haven't this type");
                return ERROR_MALFORMED;
            }

            if (mGeneric[value]) {
                parser.printError("Duplicated input type");
                return ERROR_MALFORMED;
            }
            mGeneric[value] = true;

        } while (parser.getLastDelimiter() == kCommaSpace);
    }

    // hidc_cap_list=none;
    // hidc_cap_list=Mouse/BT, RemoteControl/Infrared;
    if (!parser.checkStringField("hidc_cap_list", kEqualSign)) {
        parser.printError("Tag \"hidc_cap_list=\" is absent");
        return ERROR_MALFORMED;
    }

    if (!hidcSupport) {
        if (!parser.checkStringField("none", kSemicolonSpace)) {
            parser.printError("Tag \"hidc_cap_list=\" must be \"none; \"");
            return ERROR_MALFORMED;
        }
    } else {
        do {
            if (!parser.parseStringField(kInputType, kSlash, &value)) {
                parser.printError("HIDC input type list haven't this type");
                return ERROR_MALFORMED;
            }

            if (mHidc[value] != kInputPathUndefined) {
                parser.printError("Duplicated input type");
                return ERROR_MALFORMED;
            }

            uint32_t inputPath;
            if (!parser.parseStringField(kInputPath, kCommaSpace | kSemicolonSpace, &inputPath)) {
                parser.printError("HIDC input type list haven't this type");
                return ERROR_MALFORMED;
            }
            mHidc[value] = inputPath;

        } while (parser.getLastDelimiter() == kCommaSpace);
    }

    // port=1000
    if (!parser.checkStringField("port", kEqualSign)) {
        parser.printError("Tag \"port=\" is absent");
        return ERROR_MALFORMED;
    }

    if (parser.checkStringField("none", kEndOfLine)) {
        mPort = 0;
    } else {
        if (!parser.parseDecValue(1, 65535, kEndOfLine, &mPort)) {
            parser.printError("Invalid port value");
            return ERROR_MALFORMED;
        }
    }

    mSupported = true;
    return OK;
}

bool UibcParameters::isGenericUsed() {
    for (int i = 0; i < kNumInputTypes; i++) {
        if (mGeneric[i]) return true;
    }
    return false;
}

bool UibcParameters::isHidcUsed() {
    for (int i = 0; i < kNumInputTypes; i++) {
        if (mHidc[i] >= 0) return true;
    }
    return false;
}

int UibcParameters::createGenericMask() {
    int mask = 0;
    for (int i = 0; i < kNumInputTypes; i++) {
        if (mGeneric[i]) mask += 1 << i;
    }
    return mask;
}

}  // namespace android
