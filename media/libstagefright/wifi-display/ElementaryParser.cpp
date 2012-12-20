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

#define LOG_TAG "ElementaryParser"

#include <stdlib.h>

#include <utils/Log.h>
#include <media/stagefright/foundation/ADebug.h>

#include "ElementaryParser.h"

namespace android {

const char *kDelimiterList[] = {
    " ", ", ", "; ", "/", "=",
};

ElementaryParser::ElementaryParser(const char *params)
    : mParams(params),
      mOffset(0) {
}

void ElementaryParser::printError(const char *errStr) {
    if (errStr == NULL) return;
    char fmt[20];
    sprintf(fmt, "%%s\n%%s\n%%%ds", mOffset + 1);
    ALOGE(fmt, errStr, mParams, "^");
}

int ElementaryParser::getBitIndex(uint32_t value, uint32_t mask) {
    // The bit field (value) must contain max one bit set to 1.
    // Returns index of bit which has value equal 1, and kErrMultiBits
    // if bit field has more than one bit with 1. If bit field hasn't
    // bit set to 1 the return value will be equal kErrNoBits.
    value &= mask;

    for (int pos = 0; value > 0; pos++, value >>= 1) {
        if (value & 1) {
            if (value == 1) {
                return pos;
            } else {
                return kErrMultiBits;
            }
        }
    }

    return kErrNoBits;
}

uint32_t ElementaryParser::getLastDelimiter() {
    return mLastDelimiter;
}

bool ElementaryParser::parseDelimiter(uint32_t delimiterMask) {
    if ((delimiterMask & kEndOfLine) && mParams[mOffset] == '\0') {
        mLastDelimiter = kEndOfLine;
        return true;
    }
    uint32_t pos, mask;
    for (pos = 0, mask = kSpace; mask <= kEqualSign; pos++, mask <<= 1) {
        if (delimiterMask & mask) {
            if (!strncmp(&mParams[mOffset], kDelimiterList[pos],
                    strlen(kDelimiterList[pos]))) {
                mLastDelimiter = mask;
                mOffset += strlen(kDelimiterList[pos]);
                return true;
            }
        }
    }
    return false;
}

bool ElementaryParser::parseDecValue(int min, int max,
        uint32_t delimiterMask, int *value) {
    char *endPtr;
    *value = strtol(&mParams[mOffset], &endPtr, 10);
    if (*value < min || *value > max) {
        return false;
    }
    mOffset = endPtr - mParams;

    return parseDelimiter(delimiterMask);
}

bool ElementaryParser::parseHexValue(int length, uint32_t max,
        uint32_t delimiterMask, uint32_t *value) {
    char *endPtr;
    *value = strtoul(&mParams[mOffset], &endPtr, 16);
    if (endPtr - &mParams[mOffset] != length || *value > max) {
        return false;
    }
    mOffset += length;

    return parseDelimiter(delimiterMask);
}

bool ElementaryParser::parseHexBitField(int length, uint32_t mask,
        int bitsLimit, uint32_t delimiterMask, uint32_t *value) {
    char *endPtr;
    *value = strtoul(&mParams[mOffset], &endPtr, 16);
    if (endPtr - &mParams[mOffset] != length || *value & ~mask) {
        return false;
    }

    if (bitsLimit == kSingleBit || bitsLimit == kSingleBitOrZero) {
        int pos = getBitIndex(*value, mask);
        if (pos == kErrMultiBits ||
                (bitsLimit == kSingleBit && pos == kErrNoBits)) {
            return false;
        }
    }
    mOffset += length;

    return parseDelimiter(delimiterMask);
}

bool ElementaryParser::parseStringField(const char **table,
        uint32_t delimiterMask, uint32_t *value) {
    uint32_t i;
    for (i = 0; table[i] != NULL; i++) {
        if (!strncmp(&mParams[mOffset], table[i], strlen(table[i]))) {
            *value = i;
            mOffset += strlen(table[i]);
            break;
        }
    }

    if (table[i] == NULL) {
        return false;
    }

    return parseDelimiter(delimiterMask);
}

bool ElementaryParser::checkStringField(const char *str,
        uint32_t delimiterMask) {
    if (strncmp(&mParams[mOffset], str, strlen(str))) {
        return false;
    }
    mOffset += strlen(str);

    return parseDelimiter(delimiterMask);
}


}   // namespace android
