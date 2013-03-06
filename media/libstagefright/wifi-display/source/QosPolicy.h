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

#ifndef QOS_POLICY_H_

#define QOS_POLICY_H_

#include <stdint.h>

#include <media/stagefright/foundation/ABase.h>
#include <utils/List.h>
#include <utils/RefBase.h>

namespace android {

struct AMessage;

struct QosPolicy : public RefBase {
    QosPolicy(const sp<AMessage> &notify);

    enum {
        kWhatChangeBitRate,
        kWhatPauseVideo,
        kWhatResumeVideo,
    };

    void setTargetBitRate(uint32_t bitRate);
    void setPolicy(uint32_t lowBufferingWatermark, uint32_t highBufferingWatermark);

    void resetStreamStatistics();

    void reportQueuedPacket(int64_t eventUs, int64_t pts);
    void reportSentPacket(int64_t eventUs, int64_t pts, uint32_t packetSize);

protected:
    virtual ~QosPolicy();

private:
    struct DataRateInfo {
        int64_t timeUs;
        uint32_t packetSize;
    };

    typedef List<DataRateInfo> DataRateList;
    typedef DataRateList::iterator DataRateIterator;

    struct BufferingPolicy;
    struct BitRatePolicy;

    sp<AMessage> mNotify;

    uint32_t mQueueCount;
    int64_t mLastQueuedPts;
    uint32_t mAverageFrameDuration;
    int64_t mQueueLengthAverageStart;
    int32_t mQueueLengthAverage;

    DataRateList mDataRate;

    int32_t mEmergencyWatermark;
    int32_t mEmergencyCount;

    sp<BufferingPolicy> mBufferingPolicy;

    int64_t mLastBitRateCheckUs;
    sp<BitRatePolicy> mBitRatePolicy;

    void updateFrameDuration(int64_t pts);
    int32_t getQueueLength();
    void updateQueueLengthAverage();
    int32_t getQueueLengthAverage();

    void updateDataRate(int64_t eventTimeUs, uint32_t packetSize);
    int32_t getDataRate();

    void checkForEmergency();
    bool isEmergency();

    void checkBufferingPolicy(uint32_t reason);
    void checkBitRatePolicy();

    DISALLOW_EVIL_CONSTRUCTORS(QosPolicy);
};

}  // namespace android

#endif  // QOS_POLICY_H_
