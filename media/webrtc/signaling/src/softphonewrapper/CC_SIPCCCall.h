/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CC_SIPCCCALL_H
#define _CC_SIPCCCALL_H

#include "CC_Call.h"

#include <map>

#if defined(__cplusplus) && __cplusplus >= 201103L
typedef struct Timecard Timecard;
#else
#include "timecard.h"
#endif

#include "common/Wrapper.h"
#include "common/csf_common.h"
#include "mozilla/Mutex.h"
#include "base/lock.h"

namespace CSF
{
    struct StreamInfo
    {
		// a bit of overkill having a structure just for video, but we may have other properties later
    	bool isVideo;
    };
    typedef std::map<int, StreamInfo> StreamMapType;

    DECLARE_NS_PTR(CC_SIPCCCallMediaData);

    class CC_SIPCCCallMediaData
	{
          ~CC_SIPCCCallMediaData() {}
	public:
        NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CC_SIPCCCallMediaData)
		CC_SIPCCCallMediaData():
          remoteWindow(nullptr),
          streamMapMutex("CC_SIPCCCallMediaData"),
          audioMuteState(false),
          videoMuteState(false),
          volume(-1){}
		CC_SIPCCCallMediaData(VideoWindowHandle remoteWindow,
            bool audioMuteState, bool videoMuteState, int volume):
          remoteWindow(remoteWindow),
          streamMapMutex("CC_SIPCCCallMediaData"),
          audioMuteState(audioMuteState),
          videoMuteState(videoMuteState),
          volume(volume) {}
        VideoWindowHandle remoteWindow;
		ExternalRendererHandle extRenderer;
		VideoFormat videoFormat;
        mozilla::Mutex streamMapMutex;
        StreamMapType streamMap;
        bool audioMuteState;
        bool videoMuteState;
        int volume;
    private:
        CC_SIPCCCallMediaData(const CC_SIPCCCallMediaData&);
        CC_SIPCCCallMediaData& operator=(const CC_SIPCCCallMediaData&);
	};

	DECLARE_NS_PTR(CC_SIPCCCall);
    class CC_SIPCCCall : public CC_Call
    {

    CSF_DECLARE_WRAP(CC_SIPCCCall, cc_call_handle_t);
    private:
        cc_call_handle_t callHandle;
        CC_SIPCCCall (cc_call_handle_t aCallHandle);
        CC_SIPCCCallMediaDataPtr  pMediaData;
        std::string peerconnection;  // The peerconnection handle

    public:
        virtual inline std::string toString() {
            std::string result;
            char tmpString[11];
            csf_sprintf(tmpString, sizeof(tmpString), "%X", callHandle);
            result = tmpString;
            return result;
        };

        virtual void setRemoteWindow (VideoWindowHandle window);
        virtual int setExternalRenderer(VideoFormat vFormat, ExternalRendererHandle renderer);
		virtual void sendIFrame();

        virtual CC_CallInfoPtr getCallInfo ();

        virtual bool originateCall (cc_sdp_direction_t video_pref, const std::string & digits);
        virtual bool answerCall (cc_sdp_direction_t video_pref);
        virtual bool hold (cc_hold_reason_t reason);
        virtual bool resume (cc_sdp_direction_t video_pref);
        virtual bool endCall();
        virtual bool sendDigit (cc_digit_t digit);
        virtual bool backspace();
        virtual bool redial (cc_sdp_direction_t video_pref);
        virtual bool initiateCallForwardAll();
        virtual bool endConsultativeCall();
        virtual bool conferenceStart (cc_sdp_direction_t video_pref);
        virtual bool conferenceComplete (CC_CallPtr otherLog, cc_sdp_direction_t video_pref);
        virtual bool transferStart (cc_sdp_direction_t video_pref);
        virtual bool transferComplete (CC_CallPtr otherLeg,
                                       cc_sdp_direction_t video_pref);
        virtual bool cancelTransferOrConferenceFeature();
        virtual bool directTransfer (CC_CallPtr target);
        virtual bool joinAcrossLine (CC_CallPtr target);
        virtual bool blfCallPickup (cc_sdp_direction_t video_pref, const std::string & speed);
        virtual bool select();
        virtual bool updateVideoMediaCap (cc_sdp_direction_t video_pref);
        virtual bool sendInfo (const std::string & infopackage, const std::string & infotype, const std::string & infobody);
        virtual bool muteAudio();
        virtual bool unmuteAudio();
        virtual bool muteVideo();
        virtual bool unmuteVideo();
        virtual void addStream(int streamId, bool isVideo);
        virtual void removeStream(int streamId);
        virtual bool setVolume(int volume);
        virtual void originateP2PCall (cc_sdp_direction_t video_pref, const std::string & digits, const std::string & ip);
        virtual void createOffer(cc_media_constraints_t *constraints, Timecard *);
        virtual void createAnswer(cc_media_constraints_t *constraints, Timecard *);
        virtual void setLocalDescription(cc_jsep_action_t action, const std::string & sdp, Timecard *);
        virtual void setRemoteDescription(cc_jsep_action_t action, const std::string & sdp, Timecard *);
        virtual void setPeerConnection(const std::string& handle);
        virtual const std::string& getPeerConnection() const;
        virtual void addStream(cc_media_stream_id_t stream_id,
                               cc_media_track_id_t track_id,
                               cc_media_type_t media_type,
                               cc_media_constraints_t *constraints);
        virtual void removeStream(cc_media_stream_id_t stream_id, cc_media_track_id_t track_id, cc_media_type_t media_type);
        virtual CC_SIPCCCallMediaDataPtr getMediaData();
        virtual void addICECandidate(const std::string & candidate, const std::string & mid, unsigned short level, Timecard *);

    private:
        virtual bool setAudioMute(bool mute);
        virtual bool setVideoMute(bool mute);

        mozilla::Mutex m_lock;
    };

}


#endif
