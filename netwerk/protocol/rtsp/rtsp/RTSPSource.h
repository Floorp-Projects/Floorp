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

#ifndef RTSP_SOURCE_H_

#define RTSP_SOURCE_H_

#include "mozilla/Types.h"
#include <utils/RefBase.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/foundation/AHandlerReflector.h>

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIStreamingProtocolController.h"
#include "nsProxyRelease.h"

namespace android {

struct MOZ_EXPORT MetaData;
struct MOZ_EXPORT ABuffer;
struct MOZ_EXPORT ALooper;
struct MOZ_EXPORT AnotherPacketSource;
struct RtspConnectionHandler;

class RTSPSource : public RefBase
{
public:

    RTSPSource(
            nsIStreamingProtocolListener *aListener,
            const char *url,
            bool uidValid = false,
            uid_t uid = 0);

    void start();
    void stop();

    void play();
    void pause();
    void seek(uint64_t timeUs);
    void resume();
    void suspend();
    void playbackEnded();

    status_t feedMoreTSData();

    sp<MetaData> getFormat(bool audio);
    status_t dequeueAccessUnit(bool audio, sp<ABuffer> *accessUnit);

    status_t getDuration(int64_t *durationUs);
    status_t seekTo(int64_t seekTimeUs);
    bool isSeekable();

    void onMessageReceived(const sp<AMessage> &msg);

protected:
    ~RTSPSource();

private:
    enum {
        kWhatNotify               = 'noti',
        kWhatDisconnect           = 'disc',
        kWhatPerformSeek          = 'seek',
        kWhatPerformPlay          = 'play',
        kWhatPerformPause         = 'paus',
        kWhatPerformResume        = 'resu',
        kWhatPerformSuspend       = 'susp',
        kWhatPerformPlaybackEnded = 'ende',
    };

    enum State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        SEEKING,
        PAUSING,
        PLAYING,
    };

    enum Flags {
        // Don't log any URLs.
        kFlagIncognito = 1,
    };

    struct TrackInfo {
        sp<AnotherPacketSource> mSource;

        int32_t  mTimeScale;
        uint32_t mRTPTime;
        int64_t  mNormalPlaytimeUs;
        bool     mNPTMappingValid;
        bool     mIsAudio;
        uint64_t mLatestReceivedUnit;
        uint64_t mLatestPausedUnit;
    };

    AString mURL;
    bool mUIDValid;
    uid_t mUID;
    State mState;
    status_t mFinalResult;
    uint32_t mDisconnectReplyID;
    uint64_t mLatestPausedUnit;

    sp<ALooper> mLooper;
    sp<AHandlerReflector<RTSPSource> > mReflector;
    sp<RtspConnectionHandler> mHandler;

    Vector<TrackInfo> mTracks;
    sp<AnotherPacketSource> mAudioTrack;
    sp<AnotherPacketSource> mVideoTrack;

    int32_t mSeekGeneration;

    sp<AnotherPacketSource> getSource(bool audio);

    void onConnected(bool isSeekable);
    void onDisconnected(const sp<AMessage> &msg);
    void finishDisconnectIfPossible();

    void performSeek(int64_t seekTimeUs);

    void performPlay(int64_t playTimeUs);

    void performPause();

    void performResume();

    void performSuspend();

    void performPlaybackEnded();

    void onTrackDataAvailable(size_t trackIndex);

    void onTrackEndOfStream(size_t trackIndex);

    nsMainThreadPtrHandle<nsIStreamingProtocolListener> mListener;
    int mPrintCount;

    DISALLOW_EVIL_CONSTRUCTORS(RTSPSource);
};

}  // namespace android

#endif  // RTSP_SOURCE_H_
