/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _CC_SIPCCCALL_H
#define _CC_SIPCCCALL_H

#include "CC_Call.h"

#include <map>
#include <iomanip>
#include <sstream>

#include "common/Wrapper.h"
#include "base/lock.h"

namespace CSF
{
    struct StreamInfo
    {
		// a bit of overkill having a structure just for video, but we may have other properties later
    	bool isVideo;                    
    };
    typedef std::map<int, StreamInfo> StreamMapType;

    DECLARE_PTR(CC_SIPCCCallMediaData);

    class CC_SIPCCCallMediaData
	{
	public:
		CC_SIPCCCallMediaData(): remoteWindow(NULL), audioMuteState(false), videoMuteState(false), volume(-1){}
		CC_SIPCCCallMediaData(VideoWindowHandle remoteWindow,
            bool audioMuteState, bool videoMuteState, int volume): remoteWindow(remoteWindow),
            audioMuteState(audioMuteState), videoMuteState(videoMuteState), volume(volume) {}
        VideoWindowHandle remoteWindow; 
		ExternalRendererHandle extRenderer;
		VideoFormat videoFormat;	
        Lock streamMapMutex;
        StreamMapType streamMap;
        bool audioMuteState;
        bool videoMuteState; 
        int volume;        
    private:
        CC_SIPCCCallMediaData(const CC_SIPCCCallMediaData&);
        CC_SIPCCCallMediaData& operator=(const CC_SIPCCCallMediaData&);
	};

	DECLARE_PTR(CC_SIPCCCall);
    class CC_SIPCCCall : public CC_Call
    {

    CSF_DECLARE_WRAP(CC_SIPCCCall, cc_call_handle_t);
    private:
        cc_call_handle_t callHandle;
        CC_SIPCCCall (cc_call_handle_t aCallHandle);
        CC_SIPCCCallMediaDataPtr  pMediaData;

    public:
        virtual inline std::string toString() {
        	std::stringstream sstream;
            sstream << "0x" << std::setw( 5 ) << std::setfill( '0' ) << std::hex << callHandle;
            return sstream.str();
        };

        virtual void setRemoteWindow (VideoWindowHandle window);
        virtual int setExternalRenderer(VideoFormat vFormat, ExternalRendererHandle renderer);
		virtual void sendIFrame();

        virtual CC_CallInfoPtr getCallInfo ();

        virtual bool originateCall (cc_sdp_direction_t video_pref, const std::string & digits, char* sdp, int audioPort, int videoPort);
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
        virtual bool originateP2PCall (cc_sdp_direction_t video_pref, const std::string & digits, const std::string & ip);

        virtual CC_SIPCCCallMediaDataPtr getMediaData();

    private:
        virtual bool setAudioMute(bool mute);
        virtual bool setVideoMute(bool mute);

        Lock m_lock;
    };

};


#endif
