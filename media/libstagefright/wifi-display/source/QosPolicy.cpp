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
#define LOG_TAG "QosPolicy"
#include <utils/Log.h>

#include "QosPolicy.h"

#include <cutils/properties.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>

namespace android {

static const int32_t kLowBufferingWatermarkDefault = 100000;
static const int32_t kHighBufferingWatermarkDefault = 10000000;
static const int64_t kMinEstimationIntervalUs = 1200000;
static const int64_t kMinEmergencyEstimationIntervalUs = 600000;
static const int64_t kDataRateEstimationIntervalUs = 3000000;
static const int32_t kBitRateCheckingPeriod = 500000;
static const int32_t kTargetBitRateDefault = 5000000;
static const int32_t kIgnoreStartupPtsCount = 3;

enum {
    kDebugEnableBufferingPolicy = 0x01,
    kDebugEnableBitRatePolicy   = 0x02,
};

static const char *kDebugEnableDefault = "3";
static const char *kDebugLoggingDefault = "0";

////////////////////////////////////////////////////////////////////////////////////////////////////

struct QosPolicy::BufferingPolicy : public RefBase {
    BufferingPolicy(sp<AMessage> notify);

    enum {
        kReasonRise,
        kReasonFall,
    };

    void setPolicy(uint32_t lowWatermark, uint32_t highWatermark);
    void checkPolicy(uint32_t reason, int32_t queueLength);

private:
    sp<AMessage> mNotify;

    int32_t mLowWatermark;
    int32_t mHighWatermark;

    bool mOverflow;

    bool mDebugEnable;
    bool mDebugLogging;
    int32_t mDebugLoggingThrottle;
    bool mDebugPrintHeader;
    bool mDebugLastOverflow;

    void dump(uint32_t reason, int32_t queueLength);
};

QosPolicy::BufferingPolicy::BufferingPolicy(sp<AMessage> notify)
    : mNotify(notify),
      mOverflow(false),
      mDebugLastOverflow(false)
{
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.wfd.qos.policy.enable", value, kDebugEnableDefault);
    mDebugEnable = (atoi(value) & kDebugEnableBufferingPolicy) != 0;
    property_get("debug.wfd.qos.policy.logging", value, kDebugLoggingDefault);
    mDebugLogging = atoi(value) != 0;
    mDebugPrintHeader = true;
    mDebugLoggingThrottle = 0;
}

void QosPolicy::BufferingPolicy::setPolicy(uint32_t lowWatermark, uint32_t highWatermark) {
    mLowWatermark = lowWatermark;
    mHighWatermark = highWatermark;
}

void QosPolicy::BufferingPolicy::checkPolicy(uint32_t reason, int32_t queueLength) {
    if (reason == kReasonRise) {
        if (!mOverflow && queueLength > mHighWatermark) {
            mOverflow = true;

            if (mDebugEnable) {
                sp<AMessage> notify = mNotify->dup();
                notify->setInt32("what", kWhatPauseVideo);
                notify->post();
            }
        }
    } else if (reason == kReasonFall) {
        if (mOverflow && queueLength <= mLowWatermark) {
            mOverflow = false;

            if (mDebugEnable) {
                sp<AMessage> notify = mNotify->dup();
                notify->setInt32("what", kWhatResumeVideo);
                notify->post();
            }
        }
    } else {
        TRESPASS();
    }

    if (mDebugLogging) {
        dump(reason, queueLength);
    }
}

void QosPolicy::BufferingPolicy::dump(uint32_t reason, int32_t queueLength) {
    if (mDebugPrintHeader) {
        ALOGI("BufferingPolicy,time,queueLength x10,overflow");
        mDebugPrintHeader = false;
    }

    if ((reason == kReasonRise && --mDebugLoggingThrottle < 0) || mDebugLastOverflow != mOverflow) {
        static const int32_t kFlagScale = 7300000;
        int64_t timeUs = ALooper::GetNowUs();

        if (mDebugLastOverflow != mOverflow) {
            ALOGI("BufferingPolicy,%lld,%d,%d", timeUs - 10, queueLength * 10, mDebugLastOverflow * kFlagScale);
            mDebugLastOverflow = mOverflow;
        }

        ALOGI("BufferingPolicy,%lld,%d,%d", timeUs, queueLength * 10, mOverflow * kFlagScale);

        mDebugLoggingThrottle = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct QosPolicy::BitRatePolicy : public RefBase {
    BitRatePolicy(QosPolicy *qosPolicy, sp<AMessage> notify);

    void setTargetBitRate(uint32_t bitRate);
    void setPolicy(uint32_t lowWatermark, uint32_t highWatermark);
    void checkPolicy(int32_t queueLength, int32_t dataRate);

private:
    enum {
        kActionNone,
        kActionDowngrade,
        kActionUpgrade,
    };

    struct SteadyStateTracker {
        SteadyStateTracker();

        void reset();
        void update(uint32_t action, int32_t bitRate);
        int32_t getAverageBitRate();

    private:
        static const int32_t kBufferLength = 32;

        int32_t mSteadyStateCount;
        int32_t mBitRate[kBufferLength];
        int32_t mBitRateCount;
        int32_t mWriteIndex;
    };

    QosPolicy *mQosPolicy;
    sp<AMessage> mNotify;

    int32_t mTargetBitRate;
    int32_t mCurrentBitRate;

    int32_t mAgressiveDowngradeWatermark;
    int32_t mAgressiveUpgradeWatermark;
    int32_t mDowngradeWatermark;
    int32_t mUpgradeWatermark;

    uint32_t mAction;
    int32_t mBitRateStep;

    bool mDebugEnable;
    bool mDebugLogging;
    bool mDebugPrintHeader;

    SteadyStateTracker mSteadyStateTracker;

    int32_t getBitRateStep(uint32_t action, int32_t dataRateDelta = -1);
};

QosPolicy::BitRatePolicy::BitRatePolicy(QosPolicy *qosPolicy, sp<AMessage> notify)
    : mQosPolicy(qosPolicy),
      mNotify(notify),
      mTargetBitRate(kTargetBitRateDefault),
      mCurrentBitRate(kTargetBitRateDefault)
{
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.wfd.qos.policy.enable", value, kDebugEnableDefault);
    mDebugEnable = (atoi(value) & kDebugEnableBitRatePolicy) != 0;
    property_get("debug.wfd.qos.policy.logging", value, kDebugLoggingDefault);
    mDebugLogging = atoi(value) != 0;
    mDebugPrintHeader = true;
}

void QosPolicy::BitRatePolicy::setTargetBitRate(uint32_t bitRate) {
    mTargetBitRate = bitRate;
}

void QosPolicy::BitRatePolicy::setPolicy(uint32_t lowWatermark, uint32_t highWatermark) {
    mAgressiveDowngradeWatermark = static_cast<int>(highWatermark * 0.5f);
    mAgressiveUpgradeWatermark = static_cast<int>(lowWatermark * 0.5f);
    mDowngradeWatermark = static_cast<int>(highWatermark * 0.4f);
    mUpgradeWatermark = lowWatermark;

    mAction = kActionNone;
    mBitRateStep = 0;

    mSteadyStateTracker.reset();
}

void QosPolicy::BitRatePolicy::checkPolicy(int32_t queueLength, int32_t dataRate) {
    static const float kDataRateThreshold = 0.05f;
    static const int32_t kMinimalBitRate = 1000000;

    int32_t newBitRate = mCurrentBitRate;
    int32_t dataRateDelta = mCurrentBitRate - dataRate;
    bool dataRateAction = dataRateDelta > (mCurrentBitRate * kDataRateThreshold);
    bool latencyAction = queueLength < mAgressiveUpgradeWatermark || queueLength > mAgressiveDowngradeWatermark;
    int32_t bitRateStep = 0;

    if (dataRateAction || latencyAction) {
        if (dataRateAction && queueLength > mDowngradeWatermark) {
            bitRateStep = getBitRateStep(kActionDowngrade, dataRateDelta);
        } else if (queueLength > mAgressiveDowngradeWatermark) {
            bitRateStep = getBitRateStep(kActionDowngrade);
        } else if (queueLength < mUpgradeWatermark && mCurrentBitRate < mTargetBitRate) {
            bitRateStep = getBitRateStep(kActionUpgrade);
        } else {
            mAction = kActionNone;
        }

        switch (mAction) {
            case kActionDowngrade:
                newBitRate -= bitRateStep;

                if (newBitRate < kMinimalBitRate) {
                    newBitRate = kMinimalBitRate;
                }
                break;

            case kActionUpgrade:
                newBitRate += bitRateStep;

                if (newBitRate > mTargetBitRate) {
                    newBitRate = mTargetBitRate;
                }
                break;
        }

    } else {
        mAction = kActionNone;
    }

    mSteadyStateTracker.update(mAction, mCurrentBitRate);

    if (mDebugLogging) {
        if (mDebugPrintHeader) {
            ALOGI("BitRatePolicy,time,target,dataRate,dataRateAction,queueLength x10,latencyAction,currentBitRate,newBitRate,actionDelta,actualDelta");
            mDebugPrintHeader = false;
        }
        ALOGI("BitRatePolicy,%lld,%d,%d,%d,%d,%d,%d,%d,%d,%d", ALooper::GetNowUs(), mTargetBitRate,
                dataRate, dataRateAction, queueLength * 10, latencyAction, mCurrentBitRate, newBitRate,
                mTargetBitRate + ((mAction == kActionDowngrade) ? -bitRateStep : bitRateStep),
                mTargetBitRate + newBitRate - mCurrentBitRate);
    }

    if (mDebugEnable && newBitRate != mCurrentBitRate) {
        sp<AMessage> notify = mNotify->dup();
        notify->setInt32("what", kWhatChangeBitRate);
        notify->setInt32("bitrate", newBitRate);
        notify->post();

        mQosPolicy->resetStreamStatistics();

        mCurrentBitRate = newBitRate;
    }
}

int32_t QosPolicy::BitRatePolicy::getBitRateStep(uint32_t action, int32_t dataRateDelta) {
    static const int32_t kDowngradeStep = 800000;
    static const int32_t kUpgradeStep = 400000;
    static const int32_t kUpgradeAboveSteadyStep = 150000;
    static const float kDowngradeAttenuationFactor = 0.6f;
    static const float kUpgradeAttenuationFactor = 0.7f;
    static const float kDowngradeFactor = 0.8f;
    static const int32_t kMinimalStep = 50000;

    if (action != mAction) {
        int32_t steadyBitRate = mSteadyStateTracker.getAverageBitRate();

        switch (action) {
            case kActionDowngrade:
                mBitRateStep = kDowngradeStep;
                break;
            case kActionUpgrade:
                if (steadyBitRate > 0 && (mCurrentBitRate + kUpgradeStep) > steadyBitRate) {
                    mBitRateStep = kUpgradeAboveSteadyStep;
                } else {
                    mBitRateStep = kUpgradeStep;
                }
                break;
        }

        mAction = action;
    } else {
        float attenuationFactor = 0.0f;

        switch (action) {
            case kActionDowngrade:
                attenuationFactor = kDowngradeAttenuationFactor;
                break;
            case kActionUpgrade:
                attenuationFactor = kUpgradeAttenuationFactor;
                break;
        }

        mBitRateStep = static_cast<int>(attenuationFactor * mBitRateStep);

        if (mBitRateStep < kMinimalStep) {
            mBitRateStep = kMinimalStep;
        }
    }

    int32_t bitRateStep = mBitRateStep;

    if (bitRateStep < dataRateDelta) {
        bitRateStep = static_cast<int>(dataRateDelta * kDowngradeFactor);
    }

    return bitRateStep;
}

QosPolicy::BitRatePolicy::SteadyStateTracker::SteadyStateTracker() {
    reset();
}

void QosPolicy::BitRatePolicy::SteadyStateTracker::reset() {
    mSteadyStateCount = 0;
    mBitRateCount = 0;
    mWriteIndex = 0;
}

void QosPolicy::BitRatePolicy::SteadyStateTracker::update(uint32_t action, int32_t bitRate) {
    if (action == kActionNone) {
        if (++mSteadyStateCount > 3) {
            mBitRate[mWriteIndex] = bitRate;
            mBitRateCount++;

            if (++mWriteIndex == kBufferLength) {
                mWriteIndex = 0;
            }
        }
    } else {
        mSteadyStateCount = 0;
    }
}

int32_t QosPolicy::BitRatePolicy::SteadyStateTracker::getAverageBitRate() {
    if (mBitRateCount > 0) {
        int32_t count = mBitRateCount;

        if (count > kBufferLength) {
            count = kBufferLength;
        }

        int64_t accumulated = 0;

        for (int32_t i = 0; i < count; i++) {
            accumulated += mBitRate[i];
        }

        return accumulated / count;
    } else {
        return 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QosPolicy::QosPolicy(const sp<AMessage> &notify)
    : mNotify(notify),
      mQueueCount(0),
      mLastQueuedPts(-kIgnoreStartupPtsCount),
      mAverageFrameDuration(20000),
      mBufferingPolicy(new BufferingPolicy(notify)),
      mBitRatePolicy(new BitRatePolicy(this, notify))
{
    setPolicy(kLowBufferingWatermarkDefault, kHighBufferingWatermarkDefault);
    resetStreamStatistics();
}

QosPolicy::~QosPolicy() {}

void QosPolicy::setTargetBitRate(uint32_t bitRate) {
    mBitRatePolicy->setTargetBitRate(bitRate);
}

void QosPolicy::setPolicy(uint32_t lowBufferingWatermark, uint32_t highBufferingWatermark) {
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.wfd.qos.policy.override", value, "");
    int32_t lowWatermark;
    int32_t highWatermark;
    if (sscanf(value, "%d:%d", &lowWatermark, &highWatermark) == 2) {
        lowBufferingWatermark = lowWatermark;
        highBufferingWatermark = highWatermark;
    }

    mEmergencyWatermark = static_cast<int>(highBufferingWatermark * 0.75f);

    mBufferingPolicy->setPolicy(lowBufferingWatermark, highBufferingWatermark);
    mBitRatePolicy->setPolicy(lowBufferingWatermark, highBufferingWatermark);
}

void QosPolicy::resetStreamStatistics() {
    mDataRate.clear();
    mQueueLengthAverageStart = -1;
    mQueueLengthAverage = 0;
    mLastBitRateCheckUs = ALooper::GetNowUs();
    mEmergencyCount = 0;
}

void QosPolicy::reportQueuedPacket(int64_t eventUs, int64_t pts) {
    mQueueCount++;

    updateFrameDuration(pts);

    checkBufferingPolicy(BufferingPolicy::kReasonRise);
}

void QosPolicy::reportSentPacket(int64_t eventUs, int64_t pts, uint32_t packetSize) {
    mQueueCount--;

    updateDataRate(eventUs, packetSize);
    updateQueueLengthAverage();
    checkForEmergency();

    checkBufferingPolicy(BufferingPolicy::kReasonFall);
    checkBitRatePolicy();
}

void QosPolicy::updateFrameDuration(int64_t pts) {
    if (mLastQueuedPts < 0) {
        if (++mLastQueuedPts < 0)
            return;
    } else {
        int32_t duration = pts - mLastQueuedPts;
        int32_t delta = duration - mAverageFrameDuration;

        if (delta < 0) {
            delta = -delta;
        }

        // The average duration is calculated using running average algorithm:
        //    avg(n) = avg(n-1) * (f - 1) / f + new / f
        // The factor f is adjusted based on difference between current average and new value. If
        // new value is close to average the factor is minimal (16). As the difference grows, so
        // does the factor (up to 1024). This is done to minimize effect of disruptions in PTS --
        // big PTS jumps are given smaller weight.
        int32_t factor = delta / (2 * mAverageFrameDuration) + 4;

        if (factor > 10) {
            factor = 10;
        }

        mAverageFrameDuration = ((int64_t)mAverageFrameDuration * ((1 << factor) - 1) + duration) >> factor;
    }

    mLastQueuedPts = pts;
}

int32_t QosPolicy::getQueueLength() {
    // Ignore frame that is currently being sent
    int32_t queueLength = (mQueueCount - 1) * mAverageFrameDuration;

    if (queueLength < 0) {
        queueLength = 0;
    }

    return queueLength;
}

void QosPolicy::updateQueueLengthAverage() {
    int32_t queueLength = getQueueLength();

    if (mQueueLengthAverageStart == -1) {
        mQueueLengthAverageStart = ALooper::GetNowUs();
        mQueueLengthAverage = queueLength;
    } else {
        if (queueLength > mQueueLengthAverage) {
            mQueueLengthAverage = (mQueueLengthAverage * 11 + queueLength * 5) / 16;
        } else {
            mQueueLengthAverage = (mQueueLengthAverage * 15 + queueLength) / 16;
        }
    }
}

int32_t QosPolicy::getQueueLengthAverage() {
    int64_t interval = 0;

    if (mQueueLengthAverageStart != -1) {
        interval = ALooper::GetNowUs() - mQueueLengthAverageStart;
    }

    if (isEmergency() || interval >= kMinEstimationIntervalUs) {
        return mQueueLengthAverage;
    } else {
        return -1;
    }
}

void QosPolicy::updateDataRate(int64_t eventTimeUs, uint32_t packetSize) {
    DataRateInfo info;

    info.timeUs = eventTimeUs;
    info.packetSize = packetSize;

    mDataRate.push_back(info);

    DataRateIterator newBegin = mDataRate.begin();

    // Find first value that gives interval < maxInterval
    while (info.timeUs - (*newBegin).timeUs >= kDataRateEstimationIntervalUs) {
        ++newBegin;
    }

    // Move newBegin iterator to the last value that gives interval >= maxInterval
    if (newBegin != mDataRate.begin()) {
        --newBegin;
    }

    for (DataRateIterator it = mDataRate.begin(); it != newBegin; ) {
        it = mDataRate.erase(it);
    }
}

int32_t QosPolicy::getDataRate() {
    DataRateIterator begin = mDataRate.begin();
    DataRateIterator end = mDataRate.end();

    int64_t interval = (*(--end)).timeUs - (*begin).timeUs;

    if ((isEmergency() && interval > 0) || interval >= kMinEstimationIntervalUs) {
        int64_t accumulated = 0;

        for (DataRateIterator it = mDataRate.begin(); it != mDataRate.end(); ++it) {
            accumulated += (*it).packetSize;
        }

        return accumulated * 8 * 1000000 / interval;
    } else {
        return -1;
    }
}

void QosPolicy::checkForEmergency() {
    if (mQueueLengthAverage > mEmergencyWatermark) {
        if (mEmergencyCount == 0) {
            if (ALooper::GetNowUs() - mQueueLengthAverageStart >= kMinEmergencyEstimationIntervalUs) {
                mEmergencyCount++;
            }
        } else {
            mEmergencyCount++;
        }
    } else {
        mEmergencyCount = 0;
    }
}

bool QosPolicy::isEmergency() {
    return mEmergencyCount == 1;
}

void QosPolicy::checkBufferingPolicy(uint32_t reason) {
    mBufferingPolicy->checkPolicy(reason, getQueueLength());
}

void QosPolicy::checkBitRatePolicy() {
    int64_t timeUs = ALooper::GetNowUs();

    if (isEmergency() || timeUs - mLastBitRateCheckUs >= kBitRateCheckingPeriod) {
        int32_t queueLength = getQueueLengthAverage();
        int32_t dataRate = getDataRate();

        if (queueLength >= 0 && dataRate >= 0) {
            mBitRatePolicy->checkPolicy(queueLength, dataRate);
        }

        mLastBitRateCheckUs = timeUs;
    }
}

}  // namespace android
