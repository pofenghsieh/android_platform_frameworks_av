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

#ifndef A_TS_BYTE_PARSER_H_

#define A_TS_BYTE_PARSER_H_

#include <sys/types.h>

#include <media/stagefright/foundation/ABase.h>
#include <media/stagefright/foundation/AMessage.h>
#include <utils/KeyedVector.h>
#include <utils/Vector.h>
#include <utils/RefBase.h>

#include "ATSParser.h"

namespace android {

struct ABitReader;
struct ABuffer;
struct MediaSource;
struct ByteReader;

struct ATSByteParser : public RefBase {

    ATSByteParser(uint32_t flags = 0);

    status_t feedTSPacket(const void *data, size_t size);
    status_t feedTSPackets(sp<ABuffer>);

    void signalDiscontinuity(
            ATSParser::DiscontinuityType type, const sp<AMessage> &extra);

    void signalEOS(status_t finalResult);

    sp<MediaSource> getSource(ATSParser::SourceType type);

    bool PTSTimeDeltaEstablished();

    enum {
        // From ISO/IEC 13818-1: 2000 (E), Table 2-29
        STREAMTYPE_RESERVED             = 0x00,
        STREAMTYPE_MPEG1_VIDEO          = 0x01,
        STREAMTYPE_MPEG2_VIDEO          = 0x02,
        STREAMTYPE_MPEG1_AUDIO          = 0x03,
        STREAMTYPE_MPEG2_AUDIO          = 0x04,
        STREAMTYPE_MPEG2_AUDIO_ADTS     = 0x0f,
        STREAMTYPE_MPEG4_VIDEO          = 0x10,
        STREAMTYPE_H264                 = 0x1b,
        STREAMTYPE_PCM_AUDIO       = 0x83,
    };

protected:
    virtual ~ATSByteParser();

private:
    struct Program;
    struct Stream;
    struct PSISection;

    uint32_t mFlags;
    Vector<sp<Program> > mPrograms;

    // Keyed by PID
    KeyedVector<unsigned, sp<PSISection> > mPSISections;

    int64_t mAbsoluteTimeAnchorUs;

    size_t mNumTSPacketsParsed;

    void parseProgramAssociationTable(ByteReader *br);
    void parseProgramMap(ByteReader *br);
    void parsePES(ByteReader *br);

    status_t parsePID(
        ByteReader *br, unsigned PID,
        unsigned continuity_counter,
        unsigned payload_unit_start_indicator);

    void parseAdaptationField(ByteReader *br, unsigned PID);
    status_t parseTS(ByteReader *br);

    void updatePCR(unsigned PID, uint64_t PCR, size_t byteOffsetFromStart);

    uint64_t mPCR[2];
    size_t mPCRBytes[2];
    int64_t mSystemTimeUs[2];
    size_t mNumPCRs;

    DISALLOW_EVIL_CONSTRUCTORS(ATSByteParser);
};

}  // namespace android

#endif  // A_TS_BYTE_PARSER_H_
