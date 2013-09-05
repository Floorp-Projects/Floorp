/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"
#include "timecard.h"

#include "CC_Common.h"

#include "CC_SIPCCCall.h"
#include "CC_SIPCCCallInfo.h"
#include "VcmSIPCCBinding.h"
#include "CSFVideoTermination.h"
#include "CSFAudioTermination.h"
#include "CSFAudioControl.h"

extern "C"
{
#include "ccapi_call.h"
#include "ccapi_call_listener.h"
#include "config_api.h"
}

using namespace std;
using namespace CSF;

static const char* logTag = "CC_SIPCCCall";

CSF_IMPLEMENT_WRAP(CC_SIPCCCall, cc_call_handle_t);

CC_SIPCCCall::CC_SIPCCCall (cc_call_handle_t aCallHandle) :
            callHandle(aCallHandle),
            pMediaData(new CC_SIPCCCallMediaData(NULL,false,false,-1)),
            m_lock("CC_SIPCCCall")
{
    CSFLogInfo( logTag, "Creating  CC_SIPCCCall %u", callHandle );

    AudioControl * audioControl = VcmSIPCCBinding::getAudioControl();

    if(audioControl)
    {
         pMediaData->volume = audioControl->getDefaultVolume();
    }
}


/*
  CCAPI_CALL_EV_CAPABILITY      -- From phone team: "...CCAPI_CALL_EV_CAPABILITY is generated for the capability changes but we decided to
                                   suppress it if it's due to state changes. We found it redundant as a state change implicitly implies a
                                   capability change. This event will still be generated if the capability changes without a state change.
  CCAPI_CALL_EV_CALLINFO        -- From phone team: "...CCAPI_CALL_EV_CALLINFO is generated for any caller id related changes including
                                   called/calling/redirecting name/number etc..."
  CCAPI_CALL_EV_PLACED_CALLINFO -- From phone team: "CCAPI_CALL_EV_PLACED_CALLINFO was a trigger to update the placed call history and
                                   gives the actual number dialed out. I think this event can be deprecated."
*/

/*
 CallState

   REORDER: You get this if you misdial a number.
 */

// This function sets the remote window parameters for the call. Note that it currently only tolerates a single
// video stream on the call and would need to be updated to handle multiple remote video streams for conferencing.
void CC_SIPCCCall::setRemoteWindow (VideoWindowHandle window)
{
    VideoTermination * pVideo = VcmSIPCCBinding::getVideoTermination();
     pMediaData->remoteWindow = window;

    if (!pVideo)
    {
        CSFLogWarn( logTag, "setRemoteWindow: no video provider found");
        return;
    }

    for (StreamMapType::iterator entry =  pMediaData->streamMap.begin(); entry !=  pMediaData->streamMap.end(); entry++)
    {
        if (entry->second.isVideo)
        {
            // first video stream found
            int streamId = entry->first;
            pVideo->setRemoteWindow(streamId,  pMediaData->remoteWindow);

            return;
        }
    }
    CSFLogInfo( logTag, "setRemoteWindow:no video stream found in call %u", callHandle );
}

int CC_SIPCCCall::setExternalRenderer(VideoFormat vFormat, ExternalRendererHandle renderer)
{
    VideoTermination * pVideo = VcmSIPCCBinding::getVideoTermination();
     pMediaData->extRenderer = renderer;
     pMediaData->videoFormat = vFormat;

    if (!pVideo)
    {
        CSFLogWarn( logTag, "setExternalRenderer: no video provider found");
        return -1;
    }

    for (StreamMapType::iterator entry =  pMediaData->streamMap.begin(); entry !=  pMediaData->streamMap.end(); entry++)
    {
        if (entry->second.isVideo)
        {
            // first video stream found
            int streamId = entry->first;
            return pVideo->setExternalRenderer(streamId,  pMediaData->videoFormat, pMediaData->extRenderer);
        }
    }
    CSFLogInfo( logTag, "setExternalRenderer:no video stream found in call %u", callHandle );
	return -1;
}

void CC_SIPCCCall::sendIFrame()
{
    VideoTermination * pVideo = VcmSIPCCBinding::getVideoTermination();

    if (pVideo)
    {
        pVideo->sendIFrame(callHandle);
    }
}

CC_CallInfoPtr CC_SIPCCCall::getCallInfo ()
{
    cc_callinfo_ref_t callInfo = CCAPI_Call_getCallInfo(callHandle);
    CC_SIPCCCallInfoPtr callInfoPtr = CC_SIPCCCallInfo::wrap(callInfo);
    callInfoPtr->setMediaData( pMediaData);
    return callInfoPtr.get();
}


//
// Operations - The following function are actions that can be taken the execute an operation on the Call, ie calls
//              down to pSIPCC to originate a call, end a call etc.
//

bool CC_SIPCCCall::originateCall (cc_sdp_direction_t video_pref, const string & digits)
{
    return (CCAPI_Call_originateCall(callHandle, video_pref, digits.c_str()) == CC_SUCCESS);
}

bool CC_SIPCCCall::answerCall (cc_sdp_direction_t video_pref)
{
    return (CCAPI_Call_answerCall(callHandle, video_pref) == CC_SUCCESS);
}

bool CC_SIPCCCall::hold (cc_hold_reason_t reason)
{
    return (CCAPI_Call_hold(callHandle, reason) == CC_SUCCESS);
}

bool CC_SIPCCCall::resume (cc_sdp_direction_t video_pref)
{
    return (CCAPI_Call_resume(callHandle, video_pref) == CC_SUCCESS);
}

bool CC_SIPCCCall::endCall()
{
    return (CCAPI_Call_endCall(callHandle) == CC_SUCCESS);
}

bool CC_SIPCCCall::sendDigit (cc_digit_t digit)
{
	AudioTermination * pAudio = VcmSIPCCBinding::getAudioTermination();
	mozilla::MutexAutoLock lock(m_lock);

    // Convert public digit (as enum or char) to RFC2833 form.
	int digitId = -1;
	switch(digit)
	{
	case '0':
		digitId = 0;
		break;
	case '1':
		digitId = 1;
		break;
	case '2':
		digitId = 2;
		break;
	case '3':
		digitId = 3;
		break;
	case '4':
		digitId = 4;
		break;
	case '5':
		digitId = 5;
		break;
	case '6':
		digitId = 6;
		break;
	case '7':
		digitId = 7;
		break;
	case '8':
		digitId = 8;
		break;
	case '9':
		digitId = 9;
		break;
	case '*':
		digitId = 10;
		break;
	case '#':
		digitId = 11;
		break;
	case 'A':
		digitId = 12;
		break;
	case 'B':
		digitId = 13;
		break;
	case 'C':
		digitId = 14;
		break;
	case 'D':
		digitId = 15;
		break;
  case '+':
    digitId = 16;
    break;
	}

    for (StreamMapType::iterator entry =  pMediaData->streamMap.begin(); entry !=  pMediaData->streamMap.end(); entry++)
    {
		if (entry->second.isVideo == false)
		{
			// first is the streamId
			if (pAudio->sendDtmf(entry->first, digitId) != 0)
			{
				// We have sent a digit, done.
				break;
			}
			else
			{
				CSFLogWarn( logTag, "sendDigit:sendDtmf returned fail");
			}
		}
    }
    return (CCAPI_Call_sendDigit(callHandle, digit) == CC_SUCCESS);
}

bool CC_SIPCCCall::backspace()
{
    return (CCAPI_Call_backspace(callHandle) == CC_SUCCESS);
}

bool CC_SIPCCCall::redial (cc_sdp_direction_t video_pref)
{
    return (CCAPI_Call_redial(callHandle, video_pref) == CC_SUCCESS);
}

bool CC_SIPCCCall::initiateCallForwardAll()
{
    return (CCAPI_Call_initiateCallForwardAll(callHandle) == CC_SUCCESS);
}

bool CC_SIPCCCall::endConsultativeCall()
{
    return (CCAPI_Call_endConsultativeCall(callHandle) == CC_SUCCESS);
}

bool CC_SIPCCCall::conferenceStart (cc_sdp_direction_t video_pref)
{
    return (CCAPI_Call_conferenceStart(callHandle, video_pref) == CC_SUCCESS);
}

bool CC_SIPCCCall::conferenceComplete (CC_CallPtr otherLeg, cc_sdp_direction_t video_pref)
{
    return (CCAPI_Call_conferenceComplete(callHandle, ((CC_SIPCCCall*)otherLeg.get())->callHandle, video_pref) == CC_SUCCESS);
}

bool CC_SIPCCCall::transferStart (cc_sdp_direction_t video_pref)
{
    return (CCAPI_Call_transferStart(callHandle, video_pref) == CC_SUCCESS);
}

bool CC_SIPCCCall::transferComplete (CC_CallPtr otherLeg,
                                     cc_sdp_direction_t video_pref)
{
    return (CCAPI_Call_transferComplete(callHandle, ((CC_SIPCCCall*)otherLeg.get())->callHandle, video_pref) == CC_SUCCESS);
}

bool CC_SIPCCCall::cancelTransferOrConferenceFeature()
{
    return (CCAPI_Call_cancelTransferOrConferenceFeature(callHandle) == CC_SUCCESS);
}

bool CC_SIPCCCall::directTransfer (CC_CallPtr target)
{
    return (CCAPI_Call_directTransfer(callHandle, ((CC_SIPCCCall*)target.get())->callHandle) == CC_SUCCESS);
}

bool CC_SIPCCCall::joinAcrossLine (CC_CallPtr target)
{
    return (CCAPI_Call_joinAcrossLine(callHandle, ((CC_SIPCCCall*)target.get())->callHandle) == CC_SUCCESS);
}

bool CC_SIPCCCall::blfCallPickup (cc_sdp_direction_t video_pref, const string & speed)
{
    return (CCAPI_Call_blfCallPickup(callHandle, video_pref, speed.c_str()) == CC_SUCCESS);
}

bool CC_SIPCCCall::select()
{
    return (CCAPI_Call_select(callHandle) == CC_SUCCESS);
}

bool CC_SIPCCCall::updateVideoMediaCap (cc_sdp_direction_t video_pref)
{
    return (CCAPI_Call_updateVideoMediaCap(callHandle, video_pref) == CC_SUCCESS);
}

bool CC_SIPCCCall::sendInfo (const string & infopackage, const string & infotype, const string & infobody)
{
    return (CCAPI_Call_sendInfo(callHandle, infopackage.c_str(), infotype.c_str(), infobody.c_str()) == CC_SUCCESS);
}

bool CC_SIPCCCall::muteAudio(void)
{
    return setAudioMute(true);
}

bool CC_SIPCCCall::unmuteAudio()
{
	return setAudioMute(false);
}

bool CC_SIPCCCall::muteVideo()
{
	return setVideoMute(true);
}

bool CC_SIPCCCall::unmuteVideo()
{
	return setVideoMute(false);
}

bool CC_SIPCCCall::setAudioMute(bool mute)
{
	bool returnCode = false;
	AudioTermination * pAudio = VcmSIPCCBinding::getAudioTermination();
	pMediaData->audioMuteState = mute;
	// we need to set the mute status of all audio streams in the map
	{
		mozilla::MutexAutoLock lock(m_lock);
		for (StreamMapType::iterator entry =  pMediaData->streamMap.begin(); entry !=  pMediaData->streamMap.end(); entry++)
	    {
			if (entry->second.isVideo == false)
			{
				// first is the streamId
				if (pAudio->mute(entry->first, mute))
				{
					// We have muted at least one stream
					returnCode = true;
				}
				else
				{
					CSFLogWarn( logTag, "setAudioMute:audio mute returned fail");
				}
			}
	    }
	}

    if  (CCAPI_Call_setAudioMute(callHandle, mute) != CC_SUCCESS)
    {
    	returnCode = false;
    }

	return returnCode;
}

bool CC_SIPCCCall::setVideoMute(bool mute)
{
	bool returnCode = false;
	VideoTermination * pVideo = VcmSIPCCBinding::getVideoTermination();
	pMediaData->videoMuteState = mute;
	// we need to set the mute status of all audio streams in the map
	{
		mozilla::MutexAutoLock lock(m_lock);
		for (StreamMapType::iterator entry =  pMediaData->streamMap.begin(); entry !=  pMediaData->streamMap.end(); entry++)
	    {
			if (entry->second.isVideo == true)
			{
				// first is the streamId
				if (pVideo->mute(entry->first, mute))
				{
					// We have muted at least one stream
					returnCode = true;
				}
				else
				{
					CSFLogWarn( logTag, "setVideoMute:video mute returned fail");
				}
			}
	    }
	}

    if  (CCAPI_Call_setVideoMute(callHandle, mute) != CC_SUCCESS)
    {
    	returnCode = false;
    }

	return returnCode;
}

void CC_SIPCCCall::addStream(int streamId, bool isVideo)
{

	CSFLogInfo( logTag, "addStream: %d video=%s callhandle=%u",
        streamId, isVideo ? "TRUE" : "FALSE", callHandle);
	{
		mozilla::MutexAutoLock lock(m_lock);
		pMediaData->streamMap[streamId].isVideo = isVideo;
	}
	// The new stream needs to be given any properties that the call has for it.
	// At the moment the only candidate is the muted state
	if (isVideo)
	{
#ifndef NO_WEBRTC_VIDEO
        VideoTermination * pVideo = VcmSIPCCBinding::getVideoTermination();

        // if there is a window for this call apply it to the stream
        if ( pMediaData->remoteWindow != NULL)
        {
            pVideo->setRemoteWindow(streamId,  pMediaData->remoteWindow);
        }
        else
        {
            CSFLogInfo( logTag, "addStream: remoteWindow is NULL");
        }

		if(pMediaData->extRenderer != NULL)
		{
			pVideo->setExternalRenderer(streamId, pMediaData->videoFormat, pMediaData->extRenderer);
		}
		else
		{
            CSFLogInfo( logTag, "addStream: externalRenderer is NULL");

		}


        for (StreamMapType::iterator entry =  pMediaData->streamMap.begin(); entry !=  pMediaData->streamMap.end(); entry++)
        {
    		if (entry->second.isVideo == false)
    		{
    			// first is the streamId
    			pVideo->setAudioStreamId(entry->first);
    		}
        }
		if (!pVideo->mute(streamId,  pMediaData->videoMuteState))
		{
			CSFLogError( logTag, "setting video mute state failed for new stream: %d", streamId);
		} else
		{
			CSFLogError( logTag, "setting video mute state SUCCEEDED for new stream: %d", streamId);

		}
#endif
	}
	else
	{
		AudioTermination * pAudio = VcmSIPCCBinding::getAudioTermination();
		if (!pAudio->mute(streamId,  pMediaData->audioMuteState))
		{
			CSFLogError( logTag, "setting audio mute state failed for new stream: %d", streamId);
		}
        if (!pAudio->setVolume(streamId,  pMediaData->volume))
        {
			CSFLogError( logTag, "setting volume state failed for new stream: %d", streamId);
        }
	}
}

void CC_SIPCCCall::removeStream(int streamId)
{
	mozilla::MutexAutoLock lock(m_lock);

	if ( pMediaData->streamMap.erase(streamId) != 1)
	{
		CSFLogError( logTag, "removeStream stream that was never in the streamMap: %d", streamId);
	}
}

bool CC_SIPCCCall::setVolume(int volume)
{
	bool returnCode = false;

    AudioTermination * pAudio = VcmSIPCCBinding::getAudioTermination();
	{
    	mozilla::MutexAutoLock lock(m_lock);
		for (StreamMapType::iterator entry =  pMediaData->streamMap.begin(); entry !=  pMediaData->streamMap.end(); entry++)
	    {
			if (entry->second.isVideo == false)
			{
			    // first is the streamId
                int streamId = entry->first;
			    if (pAudio->setVolume(streamId, volume))
			    {
                    //We have changed the volume on at least one stream,
                    //so persist the new volume as the call volume,
                    //and report success for the volume change, even if it
                    //fails on other streams
                     pMediaData->volume = volume;
                    returnCode = true;
			    }
			    else
			    {
				    CSFLogWarn( logTag, "setVolume:set volume on stream %d returned fail",
                        streamId);
                }
            }
	    }
	}
    return returnCode;
}

CC_SIPCCCallMediaDataPtr CC_SIPCCCall::getMediaData()
{
    return  pMediaData;
}

void CC_SIPCCCall::originateP2PCall (cc_sdp_direction_t video_pref, const std::string & digits, const std::string & ip)
{
    CCAPI_Config_set_server_address(ip.c_str());
    CCAPI_Call_originateCall(callHandle, video_pref, digits.c_str());
}

/*
 * This method works asynchronously, is an onCallEvent with the resulting SDP
 */
void CC_SIPCCCall::createOffer (cc_media_constraints_t *constraints,
                                Timecard *tc) {
    CCAPI_CreateOffer(callHandle, constraints, tc);
}
/*
 * This method works asynchronously, there is onCallEvent with the resulting SDP
 */
void CC_SIPCCCall::createAnswer (cc_media_constraints_t *constraints,
                                 Timecard *tc) {
    CCAPI_CreateAnswer(callHandle, constraints, tc);

}

void CC_SIPCCCall::setLocalDescription(cc_jsep_action_t action,
                                       const std::string & sdp,
                                       Timecard *tc) {
    CCAPI_SetLocalDescription(callHandle, action, sdp.c_str(), tc);
}

void CC_SIPCCCall::setRemoteDescription(cc_jsep_action_t action,
                                       const std::string & sdp,
                                       Timecard *tc) {
    CCAPI_SetRemoteDescription(callHandle, action, sdp.c_str(), tc);
}

void CC_SIPCCCall::setPeerConnection(const std::string& handle)
{
  CSFLogDebug(logTag, "setPeerConnection");

  peerconnection = handle;  // Cache this here. we need it to make the CC_SIPCCCallInfo
  CCAPI_SetPeerConnection(callHandle, handle.c_str());
}

const std::string& CC_SIPCCCall::getPeerConnection() const {
  return peerconnection;
}

void CC_SIPCCCall::addStream(cc_media_stream_id_t stream_id, cc_media_track_id_t track_id, cc_media_type_t media_type) {
  CCAPI_AddStream(callHandle, stream_id, track_id, media_type);
}

void CC_SIPCCCall::removeStream(cc_media_stream_id_t stream_id, cc_media_track_id_t track_id, cc_media_type_t media_type) {
  CCAPI_RemoveStream(callHandle, stream_id, track_id, media_type);
}

void CC_SIPCCCall::addICECandidate(const std::string & candidate,
                                   const std::string & mid,
                                   unsigned short level,
                                   Timecard *tc) {
  CCAPI_AddICECandidate(callHandle, candidate.c_str(), mid.c_str(),
                        (cc_level_t) level, tc);
}
