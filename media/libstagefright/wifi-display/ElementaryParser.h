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

#ifndef ELEMENTARY_PARSER_H_

#define ELEMENTARY_PARSER_H_

namespace android {

struct ElementaryParser {

    enum {
        kErrNoBits    = -1,
        kErrMultiBits = -2,
    };

    enum {kMultiBits, kSingleBit, kSingleBitOrZero};

    enum {
        kEndOfLine      = 0x01,
        kSpace          = 0x02,
        kCommaSpace     = 0x04,
        kSemicolonSpace = 0x08,
        kSlash          = 0x10,
        kEqualSign      = 0x20,
    };

    ElementaryParser(const char *params);

    void printError(const char *errStr);

    static int getBitIndex(uint32_t value, uint32_t mask);

    uint32_t getLastDelimiter();

    bool parseDelimiter(uint32_t delimiterMask);

    bool parseDecValue(int min, int max, uint32_t delimiterMask,
            int *value);

    bool parseHexValue(int length, uint32_t max,
            uint32_t delimiterMask, uint32_t *value);

    bool parseHexBitField(int length, uint32_t mask, int singleBit,
            uint32_t delimiterMask, uint32_t *value);

    bool parseStringField(const char **table, uint32_t delimiterMask,
            uint32_t *value);

    bool checkStringField(const char *str, uint32_t delimiterMask);

private:
    const char *mParams;
    int mOffset;
    int mLastDelimiter;
};

}  // namespace android

#endif  // ELEMENTARY_PARSER_H_
