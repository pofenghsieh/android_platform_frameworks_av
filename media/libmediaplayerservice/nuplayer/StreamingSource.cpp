/*
 * Copyright (C) 2010 The Android Open Source Project
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
#define LOG_TAG "StreamingSource"
#include <utils/Log.h>

#include "StreamingSource.h"

#ifdef OMAP_ENHANCEMENT
#include "ATSByteParser.h"
#else
#include "ATSParser.h"
#endif
#include "AnotherPacketSource.h"
#include "NuPlayerStreamListener.h"

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>

namespace android {

NuPlayer::StreamingSource::StreamingSource(const sp<IStreamSource> &source)
    : mSource(source),
      mFinalResult(OK) {
}

NuPlayer::StreamingSource::~StreamingSource() {
}

void NuPlayer::StreamingSource::start() {
    mStreamListener = new NuPlayerStreamListener(mSource, 0);

    uint32_t sourceFlags = mSource->flags();

    uint32_t parserFlags = ATSParser::TS_TIMESTAMPS_ARE_ABSOLUTE;
    if (sourceFlags & IStreamSource::kFlagAlignedVideoData) {
        parserFlags |= ATSParser::ALIGNED_VIDEO_DATA;
    }

#ifdef OMAP_ENHANCEMENT
    mTSParser = new ATSByteParser(parserFlags);
#else
    mTSParser = new ATSParser(parserFlags);
#endif

    mStreamListener->start();
}

status_t NuPlayer::StreamingSource::feedMoreTSData() {
#ifdef OMAP_ENHANCEMENT
    status_t err = OK;
#endif

    if (mFinalResult != OK) {
        return mFinalResult;
    }

#ifdef OMAP_ENHANCEMENT
    if (mBuffer == NULL) {
        mBuffer = new ABuffer(188*10);
    }
#endif

    for (int32_t i = 0; i < 50; ++i) {
#ifndef OMAP_ENHANCEMENT
        char buffer[188];
#endif
        sp<AMessage> extra;
#ifdef OMAP_ENHANCEMENT
        err = mStreamListener->dequeueAccessUnit(mBuffer, &extra);
#else
        ssize_t n = mStreamListener->read(buffer, sizeof(buffer), &extra);
#endif

#ifdef OMAP_ENHANCEMENT
        if (err == ERROR_END_OF_STREAM) {
#else
        if (n == 0) {
#endif
            ALOGI("input data EOS reached.");
            mTSParser->signalEOS(ERROR_END_OF_STREAM);
            mFinalResult = ERROR_END_OF_STREAM;
            break;
#ifdef OMAP_ENHANCEMENT
        } else if (err == INFO_DISCONTINUITY) {
#else
        } else if (n == INFO_DISCONTINUITY) {
#endif
            int32_t type = ATSParser::DISCONTINUITY_SEEK;

            int32_t mask;
            if (extra != NULL
                    && extra->findInt32(
                        IStreamListener::kKeyDiscontinuityMask, &mask)) {
                if (mask == 0) {
                    ALOGE("Client specified an illegal discontinuity type.");
                    return ERROR_UNSUPPORTED;
                }

                type = mask;
            }

            mTSParser->signalDiscontinuity(
                    (ATSParser::DiscontinuityType)type, extra);
#ifdef OMAP_ENHANCEMENT
        } else if (err == OK) {
            CHECK(mBuffer != NULL);
            if (mBuffer->data()[0] == 0x00) {
#else
        } else if (n < 0) {
            CHECK_EQ(n, -EWOULDBLOCK);
            break;
        } else {
            if (buffer[0] == 0x00) {
#endif
                // XXX legacy
                mTSParser->signalDiscontinuity(
#ifdef OMAP_ENHANCEMENT
                        mBuffer->data()[1] == 0x00
#else
                        buffer[1] == 0x00
#endif
                            ? ATSParser::DISCONTINUITY_SEEK
                            : ATSParser::DISCONTINUITY_FORMATCHANGE,
                        extra);
            } else {
#ifdef OMAP_ENHANCEMENT
                err = mTSParser->feedTSPackets(mBuffer);
#else
                status_t err = mTSParser->feedTSPacket(buffer, sizeof(buffer));
#endif

                if (err != OK) {
                    ALOGE("TS Parser returned error %d", err);

#ifdef OMAP_ENHANCEMENT
                    if (!(mSource->flags() & IStreamSource::kFlagWfd)) {
                        mTSParser->signalEOS(err);
                        mFinalResult = err;
                    }
#else
                    mTSParser->signalEOS(err);
                    mFinalResult = err;
#endif
                    break;
                }
            }
#ifdef OMAP_ENHANCEMENT
        } else {
            CHECK_EQ(err, EWOULDBLOCK);
            break;
#endif
        }
    }

    return OK;
}

sp<MetaData> NuPlayer::StreamingSource::getFormatMeta(bool audio) {
    ATSParser::SourceType type =
        audio ? ATSParser::AUDIO : ATSParser::VIDEO;

    sp<AnotherPacketSource> source =
        static_cast<AnotherPacketSource *>(mTSParser->getSource(type).get());

    if (source == NULL) {
        return NULL;
    }

    return source->getFormat();
}

status_t NuPlayer::StreamingSource::dequeueAccessUnit(
        bool audio, sp<ABuffer> *accessUnit) {
    ATSParser::SourceType type =
        audio ? ATSParser::AUDIO : ATSParser::VIDEO;

    sp<AnotherPacketSource> source =
        static_cast<AnotherPacketSource *>(mTSParser->getSource(type).get());

    if (source == NULL) {
        return -EWOULDBLOCK;
    }

    status_t finalResult;
    if (!source->hasBufferAvailable(&finalResult)) {
        return finalResult == OK ? -EWOULDBLOCK : finalResult;
    }

    status_t err = source->dequeueAccessUnit(accessUnit);

#if !defined(LOG_NDEBUG) || LOG_NDEBUG == 0
    if (err == OK) {
        int64_t timeUs;
        CHECK((*accessUnit)->meta()->findInt64("timeUs", &timeUs));
        ALOGV("dequeueAccessUnit timeUs=%lld us", timeUs);
    }
#endif

    return err;
}

}  // namespace android

