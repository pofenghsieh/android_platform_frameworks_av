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

//#define LOG_NDEBUG 0
#define LOG_TAG "ATSByteParser"
#include <utils/Log.h>

#include "ATSByteParser.h"

#include "AnotherPacketSource.h"
#include "ESQueue.h"
#include "include/avc_utils.h"

#include <media/stagefright/foundation/ABitReader.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/hexdump.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>
#include <media/IStreamSource.h>
#include <utils/KeyedVector.h>

namespace android {

static const size_t kTSPacketSize = 188;

struct ByteReader {
    ByteReader();
    ByteReader(uint8_t*, size_t);

    uint8_t getByte();
    size_t skipBytes(size_t count);
    uint8_t* getData();
    size_t getSize();

private:
    uint8_t *data;
    size_t size;
    size_t offset;
};

ByteReader::ByteReader()
    : data(NULL),
      size(0),
      offset(0) {
}

ByteReader::ByteReader(uint8_t *input, size_t size)
    : data(input),
      size(size),
      offset(0) {
}

uint8_t ByteReader::getByte() {
    return data[offset];
}

size_t ByteReader::skipBytes(size_t count) {
    CHECK(size >= count);
    offset += count;
    size -= count;
    return count;
}

uint8_t* ByteReader::getData() {
    return data + offset;
}

size_t ByteReader::getSize() {
    return size;
}

template <class U> struct Wrap {
    void read(ByteReader *br);
    U *mFields;
};

template <class U> void Wrap<U>::read(ByteReader *br) {
    mFields = reinterpret_cast<U*>(br->getData());
    br->skipBytes(sizeof(U));
}

#pragma pack (push, 1)

struct TransportPacketHeader {
    uint8_t sync_byte:8;
    uint32_t PID_hi:5;
    bool transport_error_indicator:1;
    bool payload_start_indicator:1;
    bool transport_priority:1;
    uint8_t PID_low:8;
    uint8_t continuity_counter:4;
    uint8_t adaptation_field_control:2;
    uint8_t transport_scrambling_control:2;
};

#pragma pack (pop)

#pragma pack (push, 1)

struct ProgramAssociationTable {
    uint8_t table_id:8;
    uint32_t section_length_hi:4;
    uint8_t reserved1:2;
    bool zero:1;
    bool section_syntax_indicator:1;
    uint8_t section_length_low:8;
    uint8_t transport_stream_id_low:8;
    uint32_t transport_stream_id_hi:8;
    bool current_next_indicator:1;
    uint8_t version_number:5;
    uint8_t reserved2:2;
    uint8_t section_number:8;
    uint8_t last_section_number:8;
};

#pragma pack (pop)

#pragma pack (push, 1)

struct ProgramSection {
    uint32_t program_number_hi:8;
    uint8_t program_number_low:8;
    uint32_t PID_hi:5;
    uint8_t reserved:3;
    uint8_t PID_low:8;
};

#pragma pack (pop)

#pragma pack (push, 1)

struct ProgramMapSection {
    uint8_t table_id:8;
    uint32_t section_length_hi:4;
    uint8_t reserved1:2;
    bool zero:1;
    bool section_syntax_indicator:1;
    uint8_t section_length_low:8;
    uint32_t program_number_hi:8;
    uint8_t program_number_low:8;
    bool current_next_indicator:1;
    uint8_t version_number:5;
    uint8_t reserved2:2;
    uint8_t section_number:8;
    uint8_t last_section_number:8;
    uint32_t PCR_PID_hi:5;
    uint8_t reserved3:3;
    uint8_t PCR_PID_low:8;
    uint32_t program_info_length_hi:4;
    uint8_t reserved4:4;
    uint8_t program_info_length_low:8;
};

#pragma pack (pop)

#pragma pack (push, 1)

struct ProgramInfo {
    uint8_t stream_type:8;
    uint32_t elementary_PID_hi:5;
    uint8_t reserved1:3;
    uint8_t elementary_PID_low:8;
    uint32_t ES_info_length_hi:4;
    uint8_t reserved2:4;
    uint8_t ES_info_length_low:8;
};

#pragma pack (pop)

#pragma pack (push, 1)

struct PES {
    uint32_t packet_start_code_prefix_hi:8;
    uint32_t packet_start_code_prefix_mid:8;
    uint8_t packet_start_code_prefix_low:8;
    uint8_t stream_id:8;
    uint32_t PES_packet_length_hi:8;
    uint8_t PES_packet_length_low:8;
    bool original_or_copy:1;
    bool copyright:1;
    bool data_alignment_indicator:1;
    bool PES_priority:1;
    uint8_t PES_scrambling_control:2;
    uint8_t onezero:2;
    bool PES_extension_flag:1;
    bool PES_CRC_flag:1;
    bool additional_copy_info_flag:1;
    bool DSM_trick_mode_flag:1;
    bool ES_rate_flag:1;
    bool ESCR_flag:1;
    uint8_t PTS_DTS_flags:2;
    uint8_t PES_header_data_length:8;
};

#pragma pack (pop)

#pragma pack (push, 1)

struct PTS {
    bool marker1:1;
    uint32_t PTS_hi:3;
    uint8_t seq:4;
    uint32_t PTS_mid_hi:8;
    bool marker2:1;
    uint32_t PTS_mid_low:7;
    uint32_t PTS_low_hi:8;
    bool marker3:1;
    uint8_t PTS_low_low:7;
};

#pragma pack (pop)

struct ATSByteParser::Program : public RefBase {
    Program(ATSByteParser *parser, unsigned programNumber, unsigned programMapPID);

    bool parsePSISection(
            unsigned pid, ByteReader *br, status_t *err);

    bool parsePID(
            unsigned pid, unsigned continuity_counter,
            unsigned payload_unit_start_indicator,
            ByteReader *br, status_t *err);

    void signalDiscontinuity(
            ATSParser::DiscontinuityType type, const sp<AMessage> &extra);

    void signalEOS(status_t finalResult);

    sp<MediaSource> getSource(ATSParser::SourceType type);

    int64_t convertPTSToTimestamp(uint64_t PTS);

    bool PTSTimeDeltaEstablished() const {
        return mFirstPTSValid;
    }

    unsigned number() const { return mProgramNumber; }

    void updateProgramMapPID(unsigned programMapPID) {
        mProgramMapPID = programMapPID;
    }

    unsigned programMapPID() const {
        return mProgramMapPID;
    }

    uint32_t parserFlags() const {
        return mParser->mFlags;
    }

private:
    ATSByteParser *mParser;
    unsigned mProgramNumber;
    unsigned mProgramMapPID;
    KeyedVector<unsigned, sp<Stream> > mStreams;
    bool mFirstPTSValid;
    uint64_t mFirstPTS;

    status_t parseProgramMap(ByteReader *br);

    DISALLOW_EVIL_CONSTRUCTORS(Program);
};

struct ATSByteParser::Stream : public RefBase {
    Stream(Program *program,
           unsigned elementaryPID,
           unsigned streamType,
           unsigned PCR_PID);

    unsigned type() const { return mStreamType; }
    unsigned pid() const { return mElementaryPID; }
    void setPID(unsigned pid) { mElementaryPID = pid; }

    status_t parse(
            unsigned continuity_counter,
            unsigned payload_unit_start_indicator,
            ByteReader *br);

    void signalDiscontinuity(
            ATSParser::DiscontinuityType type, const sp<AMessage> &extra);

    void signalEOS(status_t finalResult);

    sp<MediaSource> getSource(ATSParser::SourceType type);

protected:
    virtual ~Stream();

private:
    Program *mProgram;
    unsigned mElementaryPID;
    unsigned mStreamType;
    unsigned mPCR_PID;
    int32_t mExpectedContinuityCounter;

    sp<ABuffer> mBuffer;
    sp<AnotherPacketSource> mSource;
    bool mPayloadStarted;

    ElementaryStreamQueue *mQueue;

    status_t flush();
    status_t parsePES(uint8_t *data, size_t size);

    void onPayloadData(
            unsigned PTS_DTS_flags, uint64_t PTS, uint64_t DTS,
            const uint8_t *data, size_t size);

    void extractAACFrames(const sp<ABuffer> &buffer);

    bool isAudio() const;
    bool isVideo() const;

    DISALLOW_EVIL_CONSTRUCTORS(Stream);
};

struct ATSByteParser::PSISection : public RefBase {
    PSISection();

    status_t append(const void *data, size_t size);
    void clear();

    bool isComplete() const;
    bool isEmpty() const;

    const uint8_t *data() const;
    size_t size() const;

protected:
    virtual ~PSISection();

private:
    sp<ABuffer> mBuffer;

    DISALLOW_EVIL_CONSTRUCTORS(PSISection);
};

////////////////////////////////////////////////////////////////////////////////

ATSByteParser::Program::Program(
        ATSByteParser *parser, unsigned programNumber, unsigned programMapPID)
    : mParser(parser),
      mProgramNumber(programNumber),
      mProgramMapPID(programMapPID),
      mFirstPTSValid(false),
      mFirstPTS(0) {
    ALOGV("new program number %u", programNumber);
}

bool ATSByteParser::Program::parsePSISection(
        unsigned pid, ByteReader *br, status_t *err) {
    *err = OK;

    if (pid != mProgramMapPID) {
        return false;
    }

    *err = parseProgramMap(br);

    return true;
}

bool ATSByteParser::Program::parsePID(
        unsigned pid, unsigned continuity_counter,
        unsigned payload_unit_start_indicator,
        ByteReader *br, status_t *err) {
    *err = OK;

    ssize_t index = mStreams.indexOfKey(pid);
    if (index < 0) {
        return false;
    }

    *err = mStreams.editValueAt(index)->parse(
            continuity_counter, payload_unit_start_indicator, br);

    return true;
}

void ATSByteParser::Program::signalDiscontinuity(
        ATSParser::DiscontinuityType type, const sp<AMessage> &extra) {
    for (size_t i = 0; i < mStreams.size(); ++i) {
        mStreams.editValueAt(i)->signalDiscontinuity(type, extra);
    }
}

void ATSByteParser::Program::signalEOS(status_t finalResult) {
    for (size_t i = 0; i < mStreams.size(); ++i) {
        mStreams.editValueAt(i)->signalEOS(finalResult);
    }
}

struct StreamInfo {
    unsigned mType;
    unsigned mPID;
};

status_t ATSByteParser::Program::parseProgramMap(ByteReader *br) {
    Wrap<ProgramMapSection> pm;
    pm.read(br);

    unsigned table_id = pm.mFields->table_id;
    ALOGV("  table_id = %u", table_id);
    CHECK_EQ(table_id, 0x02u);

    unsigned section_syntax_indicator = pm.mFields->section_syntax_indicator;
    ALOGV("  section_syntax_indicator = %u", section_syntax_indicator);
    CHECK_EQ(section_syntax_indicator, 1u);

    unsigned section_length = pm.mFields->section_length_low |
            (pm.mFields->section_length_hi << 8);

    ALOGV("  section_length = %u", section_length);
    CHECK_EQ(section_length & 0xc00, 0u);
    CHECK_LE(section_length, 1021u);

    unsigned PCR_PID = pm.mFields->PCR_PID_low |
            (pm.mFields->PCR_PID_hi << 8);

    unsigned program_info_length = pm.mFields->program_info_length_low |
            (pm.mFields->program_info_length_hi << 8);

    ALOGV("  program_info_length = %u", program_info_length);
    CHECK_EQ(program_info_length & 0xc00, 0u);

    if (program_info_length > 0) {
        br->skipBytes(program_info_length);  // skip descriptors
    }

    Vector<StreamInfo> infos;

    // infoBytesRemaining is the number of bytes that make up the
    // variable length section of ES_infos. It does not include the
    // final CRC.
    size_t infoBytesRemaining = section_length - 9 - program_info_length - 4;

    while (infoBytesRemaining > 0) {
        CHECK_GE(infoBytesRemaining, 5u);
        Wrap <ProgramInfo> pi;
        pi.read(br);

        unsigned streamType = pi.mFields->stream_type;
        ALOGV("    stream_type = 0x%02x", streamType);

        unsigned elementaryPID = pi.mFields->elementary_PID_low |
                (pi.mFields->elementary_PID_hi << 8);
        ALOGV("    elementary_PID = 0x%04x", elementaryPID);

        unsigned ES_info_length = pi.mFields->ES_info_length_low |
                (pi.mFields->ES_info_length_hi << 8);
        ALOGV("    ES_info_length = %u", ES_info_length);
        CHECK_EQ(ES_info_length & 0xc00, 0u);

        CHECK_GE(infoBytesRemaining - 5, ES_info_length);

        StreamInfo info;
        info.mType = streamType;
        info.mPID = elementaryPID;
        infos.push(info);

        br->skipBytes(ES_info_length);

        infoBytesRemaining -= 5 + ES_info_length;
    }

    CHECK_EQ(infoBytesRemaining, 0u);

    bool PIDsChanged = false;
    for (size_t i = 0; i < infos.size(); ++i) {
        StreamInfo &info = infos.editItemAt(i);

        ssize_t index = mStreams.indexOfKey(info.mPID);

        if (index >= 0 && mStreams.editValueAt(index)->type() != info.mType) {
            ALOGI("uh oh. stream PIDs have changed.");
            PIDsChanged = true;
            break;
        }
    }

    if (PIDsChanged) {
        // The only case we can recover from is if we have two streams
        // and they switched PIDs.

        bool success = false;

        if (mStreams.size() == 2 && infos.size() == 2) {
            const StreamInfo &info1 = infos.itemAt(0);
            const StreamInfo &info2 = infos.itemAt(1);

            sp<Stream> s1 = mStreams.editValueAt(0);
            sp<Stream> s2 = mStreams.editValueAt(1);

            bool caseA =
                info1.mPID == s1->pid() && info1.mType == s2->type()
                    && info2.mPID == s2->pid() && info2.mType == s1->type();

            bool caseB =
                info1.mPID == s2->pid() && info1.mType == s1->type()
                    && info2.mPID == s1->pid() && info2.mType == s2->type();

            if (caseA || caseB) {
                unsigned pid1 = s1->pid();
                unsigned pid2 = s2->pid();
                s1->setPID(pid2);
                s2->setPID(pid1);

                mStreams.clear();
                mStreams.add(s1->pid(), s1);
                mStreams.add(s2->pid(), s2);

                success = true;
            }
        }

        if (!success) {
            ALOGI("Stream PIDs changed and we cannot recover.");
            return ERROR_MALFORMED;
        }
    }

    for (size_t i = 0; i < infos.size(); ++i) {
        StreamInfo &info = infos.editItemAt(i);

        ssize_t index = mStreams.indexOfKey(info.mPID);

        if (index < 0) {
            sp<Stream> stream = new Stream(
                    this, info.mPID, info.mType, PCR_PID);

            mStreams.add(info.mPID, stream);
        }
    }

    return OK;
}

sp<MediaSource> ATSByteParser::Program::getSource(ATSParser::SourceType type) {
    size_t index = (type == ATSParser::AUDIO) ? 0 : 0;

    for (size_t i = 0; i < mStreams.size(); ++i) {
        sp<MediaSource> source = mStreams.editValueAt(i)->getSource(type);
        if (source != NULL) {
            if (index == 0) {
                return source;
            }
            --index;
        }
    }

    return NULL;
}

int64_t ATSByteParser::Program::convertPTSToTimestamp(uint64_t PTS) {
    if (!(mParser->mFlags & ATSParser::TS_TIMESTAMPS_ARE_ABSOLUTE)) {
        if (!mFirstPTSValid) {
            mFirstPTSValid = true;
            mFirstPTS = PTS;
            PTS = 0;
        } else if (PTS < mFirstPTS) {
            PTS = 0;
        } else {
            PTS -= mFirstPTS;
        }
    }

    int64_t timeUs = (PTS * 100) / 9;

    if (mParser->mAbsoluteTimeAnchorUs >= 0ll) {
        timeUs += mParser->mAbsoluteTimeAnchorUs;
    }

    return timeUs;
}

////////////////////////////////////////////////////////////////////////////////

ATSByteParser::Stream::Stream(
        Program *program,
        unsigned elementaryPID,
        unsigned streamType,
        unsigned PCR_PID)
    : mProgram(program),
      mElementaryPID(elementaryPID),
      mStreamType(streamType),
      mPCR_PID(PCR_PID),
      mExpectedContinuityCounter(-1),
      mPayloadStarted(false),
      mQueue(NULL) {
    switch (mStreamType) {
        case STREAMTYPE_H264:
            mQueue = new ElementaryStreamQueue(
                    ElementaryStreamQueue::H264,
                    (mProgram->parserFlags() & ATSParser::ALIGNED_VIDEO_DATA)
                        ? ElementaryStreamQueue::kFlag_AlignedData : 0);
            break;
        case STREAMTYPE_MPEG2_AUDIO_ADTS:
            mQueue = new ElementaryStreamQueue(ElementaryStreamQueue::AAC);
            break;
        case STREAMTYPE_MPEG1_AUDIO:
        case STREAMTYPE_MPEG2_AUDIO:
            mQueue = new ElementaryStreamQueue(
                    ElementaryStreamQueue::MPEG_AUDIO);
            break;

        case STREAMTYPE_MPEG1_VIDEO:
        case STREAMTYPE_MPEG2_VIDEO:
            mQueue = new ElementaryStreamQueue(
                    ElementaryStreamQueue::MPEG_VIDEO);
            break;

        case STREAMTYPE_MPEG4_VIDEO:
            mQueue = new ElementaryStreamQueue(
                    ElementaryStreamQueue::MPEG4_VIDEO);
            break;

        case STREAMTYPE_PCM_AUDIO:
            mQueue = new ElementaryStreamQueue(
                    ElementaryStreamQueue::PCM_AUDIO);
            break;

        default:
            break;
    }

    ALOGV("new stream PID 0x%02x, type 0x%02x", elementaryPID, streamType);

    if (mQueue != NULL) {
        mBuffer = new ABuffer(192 * 1024);
        mBuffer->setRange(0, 0);
    }
}

ATSByteParser::Stream::~Stream() {
    delete mQueue;
    mQueue = NULL;
}

status_t ATSByteParser::Stream::parse(
        unsigned continuity_counter,
        unsigned payload_unit_start_indicator, ByteReader *br) {
    if (mQueue == NULL) {
        return OK;
    }

    if (mExpectedContinuityCounter >= 0
            && (unsigned)mExpectedContinuityCounter != continuity_counter) {
        ALOGI("discontinuity on stream pid 0x%04x", mElementaryPID);

        mPayloadStarted = false;
        mBuffer->setRange(0, 0);
        mExpectedContinuityCounter = -1;

        return OK;
    }

    mExpectedContinuityCounter = (continuity_counter + 1) & 0x0f;

    if (payload_unit_start_indicator) {
        if (mPayloadStarted) {
            // Otherwise we run the danger of receiving the trailing bytes
            // of a PES packet that we never saw the start of and assuming
            // we have a a complete PES packet.

            status_t err = flush();

            if (err != OK) {
                return err;
            }
        }

        mPayloadStarted = true;
    }

    if (!mPayloadStarted) {
        return OK;
    }

    size_t neededSize = mBuffer->size() + br->getSize();

    if (mBuffer->capacity() < neededSize) {
        // Increment in multiples of 64K.
        neededSize = (neededSize + 65535) & ~65535;

        ALOGI("resizing buffer to %d bytes", neededSize);

        sp<ABuffer> newBuffer = new ABuffer(neededSize);
        memcpy(newBuffer->data(), mBuffer->data(), mBuffer->size());
        newBuffer->setRange(0, mBuffer->size());
        mBuffer = newBuffer;
    }

    memcpy(mBuffer->data() + mBuffer->size(), br->getData(), br->getSize());
    mBuffer->setRange(0, mBuffer->size() + br->getSize());

    return OK;
}

bool ATSByteParser::Stream::isVideo() const {
    switch (mStreamType) {
        case STREAMTYPE_H264:
        case STREAMTYPE_MPEG1_VIDEO:
        case STREAMTYPE_MPEG2_VIDEO:
        case STREAMTYPE_MPEG4_VIDEO:
            return true;

        default:
            return false;
    }
}

bool ATSByteParser::Stream::isAudio() const {
    switch (mStreamType) {
        case STREAMTYPE_MPEG1_AUDIO:
        case STREAMTYPE_MPEG2_AUDIO:
        case STREAMTYPE_MPEG2_AUDIO_ADTS:
        case STREAMTYPE_PCM_AUDIO:
            return true;

        default:
            return false;
    }
}

void ATSByteParser::Stream::signalDiscontinuity(
        ATSParser::DiscontinuityType type, const sp<AMessage> &extra) {
    mExpectedContinuityCounter = -1;

    if (mQueue == NULL) {
        return;
    }

    mPayloadStarted = false;
    mBuffer->setRange(0, 0);

    bool clearFormat = false;
    if (isAudio()) {
        if (type & ATSParser::DISCONTINUITY_AUDIO_FORMAT) {
            clearFormat = true;
        }
    } else {
        if (type & ATSParser::DISCONTINUITY_VIDEO_FORMAT) {
            clearFormat = true;
        }
    }

    mQueue->clear(clearFormat);

    if (type & ATSParser::DISCONTINUITY_TIME) {
        uint64_t resumeAtPTS;
        if (extra != NULL
                && extra->findInt64(
                    IStreamListener::kKeyResumeAtPTS,
                    (int64_t *)&resumeAtPTS)) {
            int64_t resumeAtMediaTimeUs =
                mProgram->convertPTSToTimestamp(resumeAtPTS);

            extra->setInt64("resume-at-mediatimeUs", resumeAtMediaTimeUs);
        }
    }

    if (mSource != NULL) {
        mSource->queueDiscontinuity(type, extra);
    }
}

void ATSByteParser::Stream::signalEOS(status_t finalResult) {
    if (mSource != NULL) {
        mSource->signalEOS(finalResult);
    }
}

status_t ATSByteParser::Stream::parsePES(uint8_t *data, size_t size) {
    ByteReader br(data, size);

    Wrap<PES> pes;
    pes.read(&br);

    unsigned packet_startcode_prefix = pes.mFields->packet_start_code_prefix_low |
            (pes.mFields->packet_start_code_prefix_mid << 8) |
            (pes.mFields->packet_start_code_prefix_hi << 16);

    ALOGV("packet_startcode_prefix = 0x%08x", packet_startcode_prefix);

    if (packet_startcode_prefix != 1) {
        ALOGV("Supposedly payload_unit_start=1 unit does not start "
             "with startcode.");

        return ERROR_MALFORMED;
    }

    CHECK_EQ(packet_startcode_prefix, 0x000001u);

    unsigned stream_id = pes.mFields->stream_id;
    ALOGV("stream_id = 0x%02x", stream_id);

    unsigned PES_packet_length = pes.mFields->PES_packet_length_low |
            (pes.mFields->PES_packet_length_hi << 8);
    ALOGV("PES_packet_length = %u", PES_packet_length);

    if (stream_id != 0xbc  // program_stream_map
            && stream_id != 0xbe  // padding_stream
            && stream_id != 0xbf  // private_stream_2
            && stream_id != 0xf0  // ECM
            && stream_id != 0xf1  // EMM
            && stream_id != 0xff  // program_stream_directory
            && stream_id != 0xf2  // DSMCC
            && stream_id != 0xf8) {  // H.222.1 type E

        unsigned PTS_DTS_flags = pes.mFields->PTS_DTS_flags;
        ALOGV("PTS_DTS_flags = %u", PTS_DTS_flags);

        unsigned ESCR_flag = pes.mFields->ESCR_flag;
        ALOGV("ESCR_flag = %u", ESCR_flag);

        unsigned ES_rate_flag = pes.mFields->ES_rate_flag;
        ALOGV("ES_rate_flag = %u", ES_rate_flag);

        unsigned DSM_trick_mode_flag = pes.mFields->DSM_trick_mode_flag;
        ALOGV("DSM_trick_mode_flag = %u", DSM_trick_mode_flag);

        unsigned additional_copy_info_flag = pes.mFields->additional_copy_info_flag;
        ALOGV("additional_copy_info_flag = %u", additional_copy_info_flag);

        unsigned PES_header_data_length = pes.mFields->PES_header_data_length;
        ALOGV("PES_header_data_length = %u", PES_header_data_length);

        unsigned optional_bytes_remaining = PES_header_data_length;

        uint64_t Pts = 0, Dts = 0;

        if (PTS_DTS_flags == 2 || PTS_DTS_flags == 3) {
            Wrap<PTS> pts;
            pts.read(&br);

            Pts = pts.mFields->PTS_low_low |
                    (pts.mFields->PTS_low_hi << 7) |
                    (pts.mFields->PTS_mid_low << 15) |
                    (pts.mFields->PTS_mid_hi << 22) |
                    (pts.mFields->PTS_hi << 30);

            optional_bytes_remaining -= 5;

            if (PTS_DTS_flags == 3) {
                Wrap<PTS> dts;
                dts.read(&br);
                Dts = dts.mFields->PTS_low_low |
                        (dts.mFields->PTS_low_hi << 7) |
                        (dts.mFields->PTS_mid_low << 15) |
                        (dts.mFields->PTS_mid_hi << 22) |
                        (dts.mFields->PTS_hi << 30);

                optional_bytes_remaining -= 5;
            }
        }

        if (ESCR_flag) {
            br.skipBytes(6);
            optional_bytes_remaining -= 6;
        }

        if (ES_rate_flag) {
            br.skipBytes(3);
            optional_bytes_remaining -= 3;
        }

        br.skipBytes(optional_bytes_remaining);

        // ES data follows.
        if (PES_packet_length != 0) {
            CHECK_GE(PES_packet_length, PES_header_data_length + 3);

            unsigned dataLength =
                PES_packet_length - 3 - PES_header_data_length;

            if (br.getSize() < dataLength) {
                ALOGE("PES packet does not carry enough data to contain "
                        "payload. (numBitsLeft = %d, required = %d)",
                        br.getSize(), dataLength);

                return ERROR_MALFORMED;
            }

            CHECK_GE(br.getSize(), dataLength);

            onPayloadData(
                    PTS_DTS_flags, Pts, Dts, br.getData(), br.getSize());
        } else {
            onPayloadData(
                    PTS_DTS_flags, Pts, Dts,
                    br.getData(), br.getSize());

            size_t payloadSizeBytes = br.getSize();

            ALOGV("There's %d bytes of payload.", payloadSizeBytes);
        }
    } else if (stream_id == 0xbe) {  // padding_stream
        CHECK_NE(PES_packet_length, 0u);
    } else {
        CHECK_NE(PES_packet_length, 0u);
    }

    return OK;
}

status_t ATSByteParser::Stream::flush() {
    if (mBuffer->size() == 0) {
        return OK;
    }

    ALOGV("flushing stream 0x%04x size = %d", mElementaryPID, mBuffer->size());

    status_t err = parsePES(mBuffer->data(), mBuffer->size());

    mBuffer->setRange(0, 0);

    return err;
}

void ATSByteParser::Stream::onPayloadData(
        unsigned PTS_DTS_flags, uint64_t PTS, uint64_t DTS,
        const uint8_t *data, size_t size) {
    ALOGV("onPayloadData mStreamType=0x%02x", mStreamType);

    int64_t timeUs = 0ll;  // no presentation timestamp available.
    if (PTS_DTS_flags == 2 || PTS_DTS_flags == 3) {
        timeUs = mProgram->convertPTSToTimestamp(PTS);
    }

    status_t err = mQueue->appendData(data, size, timeUs);

    if (err != OK) {
        return;
    }

    sp<ABuffer> accessUnit;
    while ((accessUnit = mQueue->dequeueAccessUnit()) != NULL) {
        if (mSource == NULL) {
            sp<MetaData> meta = mQueue->getFormat();

            if (meta != NULL) {
                ALOGV("Stream PID 0x%08x of type 0x%02x now has data.",
                        mElementaryPID, mStreamType);

                mSource = new AnotherPacketSource(meta);
                mSource->queueAccessUnit(accessUnit);
            }
        } else if (mQueue->getFormat() != NULL) {
            // After a discontinuity we invalidate the queue's format
            // and won't enqueue any access units to the source until
            // the queue has reestablished the new format.

            if (mSource->getFormat() == NULL) {
                mSource->setFormat(mQueue->getFormat());
            }
            mSource->queueAccessUnit(accessUnit);
        }
    }
}

sp<MediaSource> ATSByteParser::Stream::getSource(ATSParser::SourceType type) {
    switch (type) {
        case ATSParser::VIDEO:
        {
            if (isVideo()) {
                return mSource;
            }
            break;
        }

        case ATSParser::AUDIO:
        {
            if (isAudio()) {
                return mSource;
            }
            break;
        }

        default:
            break;
    }

    return NULL;
}

////////////////////////////////////////////////////////////////////////////////

ATSByteParser::ATSByteParser(uint32_t flags)
    : mFlags(flags),
      mAbsoluteTimeAnchorUs(-1ll),
      mNumTSPacketsParsed(0),
      mNumPCRs(0) {
    mPSISections.add(0 /* PID */, new PSISection);
}

ATSByteParser::~ATSByteParser() {
}

status_t ATSByteParser::feedTSPacket(const void *data, size_t size) {
    CHECK_EQ(size, kTSPacketSize);

    ByteReader br((uint8_t *)data, size);
    parseTS(&br);
    return OK;
}

status_t ATSByteParser::feedTSPackets(sp<ABuffer> buffer) {
    uint8_t *data = buffer->data();
    size_t size = buffer->size();
    status_t err = OK;
    while (size >= kTSPacketSize) {
        err = feedTSPacket(data, kTSPacketSize);
        data += kTSPacketSize;
        size -= kTSPacketSize;
        if (err != OK) {
            break;
        }
    }
    return err;
}

void ATSByteParser::signalDiscontinuity(
        ATSParser::DiscontinuityType type, const sp<AMessage> &extra) {
    if (type == ATSParser::DISCONTINUITY_ABSOLUTE_TIME) {
        int64_t timeUs;
        CHECK(extra->findInt64("timeUs", &timeUs));

        CHECK(mPrograms.empty());
        mAbsoluteTimeAnchorUs = timeUs;
        return;
    }

    for (size_t i = 0; i < mPrograms.size(); ++i) {
        mPrograms.editItemAt(i)->signalDiscontinuity(type, extra);
    }
}

void ATSByteParser::signalEOS(status_t finalResult) {
    CHECK_NE(finalResult, (status_t)OK);

    for (size_t i = 0; i < mPrograms.size(); ++i) {
        mPrograms.editItemAt(i)->signalEOS(finalResult);
    }
}

void ATSByteParser::parseProgramAssociationTable(ByteReader *br) {
    Wrap<ProgramAssociationTable> pat;
    pat.read(br);

    unsigned table_id = pat.mFields->table_id;
    ALOGV("  table_id = %u", table_id);
    CHECK_EQ(table_id, 0x00u);

    unsigned section_syntax_indictor = pat.mFields->section_syntax_indicator;
    ALOGV("  section_syntax_indictor = %u", section_syntax_indictor);
    CHECK_EQ(section_syntax_indictor, 1u);

    unsigned section_length = (pat.mFields->section_length_low |
            (pat.mFields->section_length_hi << 8));
    ALOGV("  section_length = %u", section_length);
    CHECK_EQ(section_length & 0xc00, 0u);

    size_t numProgramBytes = (section_length - 5 /* header */ - 4 /* crc */);
    CHECK_EQ((numProgramBytes % 4), 0u);

    for (size_t i = 0; i < numProgramBytes / 4; ++i) {
        Wrap<ProgramSection> program;
        program.read(br);

        unsigned program_number = program.mFields->program_number_low |
                (program.mFields->program_number_hi << 8);
        ALOGV("    program_number = %u", program_number);

        unsigned PID = program.mFields->PID_low |
                (program.mFields->PID_hi << 8);

        if (program_number == 0) {
            ALOGV("    network_PID = 0x%04x", PID);
        } else {
            ALOGV("    program_map_PID = 0x%04x", PID);

            bool found = false;
            for (size_t index = 0; index < mPrograms.size(); ++index) {
                const sp<Program> &program = mPrograms.itemAt(index);

                if (program->number() == program_number) {
                    program->updateProgramMapPID(PID);
                    found = true;
                    break;
                }
            }

            if (!found) {
                mPrograms.push(
                        new Program(this, program_number, PID));
            }

            if (mPSISections.indexOfKey(PID) < 0) {
                mPSISections.add(PID, new PSISection);
            }
        }
    }
}

status_t ATSByteParser::parsePID(
        ByteReader *br, unsigned PID,
        unsigned continuity_counter,
        unsigned payload_unit_start_indicator) {
    ssize_t sectionIndex = mPSISections.indexOfKey(PID);

    if (sectionIndex >= 0) {
        const sp<PSISection> &section = mPSISections.valueAt(sectionIndex);

        if (payload_unit_start_indicator) {
            CHECK(section->isEmpty());

            unsigned skip = br->getByte() + 1;
            br->skipBytes(skip);
        }

        status_t err = section->append(br->getData(), br->getSize());

        if (err != OK) {
            return err;
        }

        if (!section->isComplete()) {
            return OK;
        }

        ByteReader sectionBytes((uint8_t*)(section->data()), section->size());

        if (PID == 0) {
            parseProgramAssociationTable(&sectionBytes);
        } else {
            bool handled = false;
            for (size_t i = 0; i < mPrograms.size(); ++i) {
                status_t err;
                if (!mPrograms.editItemAt(i)->parsePSISection(
                            PID, &sectionBytes, &err)) {
                    continue;
                }

                if (err != OK) {
                    return err;
                }

                handled = true;
                break;
            }

            if (!handled) {
                mPSISections.removeItem(PID);
            }
        }

        section->clear();

        return OK;
    }

    bool handled = false;
    for (size_t i = 0; i < mPrograms.size(); ++i) {
        status_t err;
        if (mPrograms.editItemAt(i)->parsePID(
                    PID, continuity_counter, payload_unit_start_indicator,
                    br, &err)) {
            if (err != OK) {
                return err;
            }

            handled = true;
            break;
        }
    }

    if (!handled) {
        ALOGV("PID 0x%04x not handled.", PID);
    }

    return OK;
}

void ATSByteParser::parseAdaptationField(ByteReader *br, unsigned PID) {
    size_t adaptation_field_length = br->getByte();
    br->skipBytes(1);
    if (adaptation_field_length > 0) {
        unsigned discontinuity = ((br->getByte()) & 0x80);
        if (discontinuity) {
            signalDiscontinuity(ATSParser::DISCONTINUITY_FORMATCHANGE, NULL);
        }
        br->skipBytes(adaptation_field_length);
    }
}

status_t ATSByteParser::parseTS(ByteReader *br) {
    Wrap<TransportPacketHeader> header;
    header.read(br);

    unsigned sync_byte = header.mFields->sync_byte;
    CHECK_EQ(sync_byte, 0x47u);

    unsigned payload_unit_start_indicator = header.mFields->payload_start_indicator;
    ALOGV("payload_unit_start_indicator = %u", payload_unit_start_indicator);

    unsigned PID = (header.mFields->PID_low) | (header.mFields->PID_hi << 8);
    ALOGV("PID = 0x%04x", PID);

    unsigned adaptation_field_control = header.mFields->adaptation_field_control;
    ALOGV("adaptation_field_control = %u", adaptation_field_control);

    unsigned continuity_counter = header.mFields->continuity_counter;
    ALOGV("PID = 0x%04x, continuity_counter = %u", PID, continuity_counter);

    // ALOGI("PID = 0x%04x, continuity_counter = %u", PID, continuity_counter);

    if (adaptation_field_control & 0x2) {
        parseAdaptationField(br, PID);
    }

    status_t err = OK;

    if (adaptation_field_control & 0x1) {
        err = parsePID(
                br, PID, continuity_counter, payload_unit_start_indicator);
    }

    ++mNumTSPacketsParsed;

    return err;
}

sp<MediaSource> ATSByteParser::getSource(ATSParser::SourceType type) {
    int which = -1;  // any

    for (size_t i = 0; i < mPrograms.size(); ++i) {
        const sp<Program> &program = mPrograms.editItemAt(i);

        if (which >= 0 && (int)program->number() != which) {
            continue;
        }

        sp<MediaSource> source = program->getSource(type);

        if (source != NULL) {
            return source;
        }
    }

    return NULL;
}

bool ATSByteParser::PTSTimeDeltaEstablished() {
    if (mPrograms.isEmpty()) {
        return false;
    }

    return mPrograms.editItemAt(0)->PTSTimeDeltaEstablished();
}

void ATSByteParser::updatePCR(
        unsigned PID, uint64_t PCR, size_t byteOffsetFromStart) {
    ALOGV("PCR 0x%016llx @ %d", PCR, byteOffsetFromStart);

    if (mNumPCRs == 2) {
        mPCR[0] = mPCR[1];
        mPCRBytes[0] = mPCRBytes[1];
        mSystemTimeUs[0] = mSystemTimeUs[1];
        mNumPCRs = 1;
    }

    mPCR[mNumPCRs] = PCR;
    mPCRBytes[mNumPCRs] = byteOffsetFromStart;
    mSystemTimeUs[mNumPCRs] = ALooper::GetNowUs();

    ++mNumPCRs;

    if (mNumPCRs == 2) {
        double transportRate =
            (mPCRBytes[1] - mPCRBytes[0]) * 27E6 / (mPCR[1] - mPCR[0]);

        ALOGV("transportRate = %.2f bytes/sec", transportRate);
    }
}

////////////////////////////////////////////////////////////////////////////////

ATSByteParser::PSISection::PSISection() {
}

ATSByteParser::PSISection::~PSISection() {
}

status_t ATSByteParser::PSISection::append(const void *data, size_t size) {
    if (mBuffer == NULL || mBuffer->size() + size > mBuffer->capacity()) {
        size_t newCapacity =
            (mBuffer == NULL) ? size : mBuffer->capacity() + size;

        newCapacity = (newCapacity + 1023) & ~1023;

        sp<ABuffer> newBuffer = new ABuffer(newCapacity);

        if (mBuffer != NULL) {
            memcpy(newBuffer->data(), mBuffer->data(), mBuffer->size());
            newBuffer->setRange(0, mBuffer->size());
        } else {
            newBuffer->setRange(0, 0);
        }

        mBuffer = newBuffer;
    }

    memcpy(mBuffer->data() + mBuffer->size(), data, size);
    mBuffer->setRange(0, mBuffer->size() + size);

    return OK;
}

void ATSByteParser::PSISection::clear() {
    if (mBuffer != NULL) {
        mBuffer->setRange(0, 0);
    }
}

bool ATSByteParser::PSISection::isComplete() const {
    if (mBuffer == NULL || mBuffer->size() < 3) {
        return false;
    }

    unsigned sectionLength = U16_AT(mBuffer->data() + 1) & 0xfff;
    return mBuffer->size() >= sectionLength + 3;
}

bool ATSByteParser::PSISection::isEmpty() const {
    return mBuffer == NULL || mBuffer->size() == 0;
}

const uint8_t *ATSByteParser::PSISection::data() const {
    return mBuffer == NULL ? NULL : mBuffer->data();
}

size_t ATSByteParser::PSISection::size() const {
    return mBuffer == NULL ? 0 : mBuffer->size();
}

}  // namespace android
