/*
 * Copyright 2012, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WIFI_DISPLAY_SINK_H_

#define WIFI_DISPLAY_SINK_H_

#include "ANetworkSession.h"
#include "VideoFormats.h"

#include <gui/Surface.h>
#include <media/stagefright/foundation/AHandler.h>

namespace android {

struct ParsedMessage;
struct RTPSink;

class RtspStateListener : public RefBase {
public:
    virtual ~RtspStateListener() {};
    virtual void onStateChanged(int state) = 0;
};

// Represents the RTSP client acting as a wifi display sink.
// Connects to a wifi display source and renders the incoming
// transport stream using a MediaPlayer instance.
struct WifiDisplaySink : public AHandler {
    WifiDisplaySink(
            const sp<ANetworkSession> &netSession,
            const sp<IGraphicBufferProducer> &producer = NULL);

    status_t start(const char *sourceHost, int32_t sourcePort);
    status_t start(const char *uri);
    status_t postStartMessage(const sp<AMessage> &msg);
    void play();
    void pause();
    void teardown();
    void setRtspStateListener(const sp<RtspStateListener> &listener);
    void removeRtspStateListener();


protected:
    virtual ~WifiDisplaySink();
    virtual void onMessageReceived(const sp<AMessage> &msg);

private:
    enum State {
        UNDEFINED,
        CONNECTING,
        CONNECTED,
        OPTIONS,
        GET_PARAMETER,
        SET_PARAMETER,
        PAUSED,
        PLAYING,
    };

    enum {
        kWhatStart,
        kWhatRTSPNotify,
        kWhatStop,
        kWhatAction,
        kWhatTimeoutM16,
    };

    enum Action {
        kActionPlay,
        kActionPause,
        kActionTeardown,
    };

    struct ResponseID {
        int32_t mSessionID;
        int32_t mCSeq;

        bool operator<(const ResponseID &other) const {
            return mSessionID < other.mSessionID
                || (mSessionID == other.mSessionID
                        && mCSeq < other.mCSeq);
        }
    };

    typedef status_t (WifiDisplaySink::*HandleRTSPResponseFunc)(
            int32_t sessionID, const sp<ParsedMessage> &msg);

    static const bool sUseTCPInterleaving = false;

    static const int kM16DefaultTimeoutSecs = 60;
    static const int kM16MinTimeoutSecs = 10;

    State mState;
    sp<ANetworkSession> mNetSession;
    sp<IGraphicBufferProducer> mProducer;
    AString mSetupURI;
    AString mRTSPHost;
    int32_t mSessionID;

    int32_t mNextCSeq;

    KeyedVector<ResponseID, HandleRTSPResponseFunc> mResponseHandlers;

    sp<RTPSink> mRTPSink;
    AString mPlaybackSessionID;
    int32_t mPlaybackSessionTimeoutSecs;

    VideoFormats mSupportedSinkVideoFormats;
    VideoFormats mSourceRequestedVideoFormats;
    sp<RtspStateListener> mRtspStateListener;
    Mutex mRtspStateListenerLock;
    int32_t mM16TimeoutCounter;

    status_t sendM2(int32_t sessionID);
    status_t sendDescribe(int32_t sessionID, const char *uri);
    status_t sendSetup(int32_t sessionID, const char *uri);
    status_t sendPlay(int32_t sessionID, const char *uri);

    void prepareKeepAliveTimeoutCheck();
    status_t sendPause(int32_t sessionID, const char *uri);
    status_t sendTeardown(int32_t sessionID, const char *uri);
    status_t extractPresentationURL(const char *str);

    status_t onReceiveM2Response(
            int32_t sessionID, const sp<ParsedMessage> &msg);

    status_t onReceiveDescribeResponse(
            int32_t sessionID, const sp<ParsedMessage> &msg);

    status_t onReceiveSetupResponse(
            int32_t sessionID, const sp<ParsedMessage> &msg);

    status_t configureTransport(const sp<ParsedMessage> &msg);

    status_t onReceivePlayResponse(
            int32_t sessionID, const sp<ParsedMessage> &msg);

    status_t onReceivePauseResponse(
            int32_t sessionID, const sp<ParsedMessage> &msg);

    status_t onReceiveTeardownResponse(
            int32_t sessionID, const sp<ParsedMessage> &msg);

    void registerResponseHandler(
            int32_t sessionID, int32_t cseq, HandleRTSPResponseFunc func);

    void onReceiveClientData(const sp<AMessage> &msg);

    void onOptionsRequest(
            int32_t sessionID,
            int32_t cseq,
            const sp<ParsedMessage> &data);

    void onGetParameterRequest(
            int32_t sessionID,
            int32_t cseq,
            const sp<ParsedMessage> &data);

    void onSetParameterRequest(
            int32_t sessionID,
            int32_t cseq,
            const sp<ParsedMessage> &data);

    const char *getSetupURI();
    void sendOK(int32_t sessionID, int32_t cseq);

    status_t sendAction(
            int32_t sessionID,
            const char *action,
            const char *uri,
            HandleRTSPResponseFunc func);

    void sendErrorResponse(
            int32_t sessionID,
            const char *errorDetail,
            int32_t cseq);

    static void AppendCommonResponse(AString *response, int32_t cseq);

    bool ParseURL(
            const char *url, AString *host, int32_t *port, AString *path,
            AString *user, AString *pass);

    void notifyRtspStateListener();

    DISALLOW_EVIL_CONSTRUCTORS(WifiDisplaySink);
};

}  // namespace android

#endif  // WIFI_DISPLAY_SINK_H_
