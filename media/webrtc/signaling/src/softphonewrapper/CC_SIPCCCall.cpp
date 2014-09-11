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
// fsm.h picks this include up as a c header, causing trouble later
#include "mozilla/ArrayUtils.h"


extern "C"
{
#include "ccapi_call.h"
#include "ccapi_call_listener.h"
#include "config_api.h"
#include "cc_constants.h"
#include "phone_types.h"
#include "fsm.h"
#include "fim.h"
#include "string_lib.h"
}

using namespace std;
using namespace CSF;

static const char* logTag = "CC_SIPCCCall";

CSF_IMPLEMENT_WRAP(CC_SIPCCCall, cc_call_handle_t);

CC_SIPCCCall::CC_SIPCCCall (cc_call_handle_t aCallHandle) :
            callHandle(aCallHandle),
            pMediaData(new CC_SIPCCCallMediaData(nullptr, false, false, -1)),
            localSdp(NULL),
            remoteSdp(NULL),
            errorString(NULL)
{
    CSFLogInfo( logTag, "Creating  CC_SIPCCCall %u", callHandle );

    AudioControl * audioControl = VcmSIPCCBinding::getAudioControl();

    if(audioControl)
    {
         pMediaData->volume = audioControl->getDefaultVolume();
    }
}

CC_SIPCCCall::~CC_SIPCCCall() {
  strlib_free(localSdp);
  strlib_free(remoteSdp);
  strlib_free(errorString);
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

fsm_fcb_t* CC_SIPCCCall::getFcb() const
{
    callid_t call_id = GET_CALL_ID(callHandle);

    // TODO(Bug 1065020): Create and hold our own call state
    fim_icb_t *call_chn = fim_get_call_chn_by_call_id(call_id);

    if (!call_chn)
      return nullptr;

    // The fcb (whatever that means) we want is buried in a fixed-size
    // linked-list (arrays are for the weak) of icbs (whatever that means).
    // We want FSM_TYPE_DEF (don't even ask what "DEF" means, of course it's
    // the one we want, because its meaning is the most mysterious of available
    // options).
    fsm_fcb_t *fcb = (fsm_fcb_t*) call_chn->next_icb // FSM_TYPE_CNF
                                          ->next_icb // FSM_TYPE_B2BCNF
                                          ->next_icb // FSM_TYPE_XFR
                                          ->next_icb->cb; // FSM_TYPE_DEF
    return fcb;
}

void CC_SIPCCCall::getLocalSdp(std::string *sdp) const {
  if (localSdp) {
    *sdp = localSdp;
  } else {
    sdp->clear();
  }
}

void CC_SIPCCCall::getRemoteSdp(std::string *sdp) const {
  if (remoteSdp) {
    *sdp = remoteSdp;
  } else {
    sdp->clear();
  }
}

fsmdef_states_t CC_SIPCCCall::getFsmState() const {
  fsm_fcb_t *fcb = getFcb();
  if (!fcb) {
    return FSMDEF_S_CLOSED;
  }

  return (fsmdef_states_t)fcb->state;
}

std::string CC_SIPCCCall::fsmStateToString (fsmdef_states_t state) const {
  return fsmdef_state_name(state);
}

void CC_SIPCCCall::getErrorString(std::string *error) const {
  if (errorString) {
    *error = errorString;
  } else {
    error->clear();
  }
}

pc_error CC_SIPCCCall::getError() const {
    return error;
}



CC_CallInfoPtr CC_SIPCCCall::getCallInfo ()
{
    cc_callinfo_ref_t callInfo = CCAPI_Call_getCallInfo(callHandle);
    CC_SIPCCCallInfoPtr callInfoPtr = CC_SIPCCCallInfo::wrap(callInfo);
    callInfoPtr->setMediaData( pMediaData);
    // This starts out with a refcount of 1, and we've now bumped it to 2 by
    // placing it in a CC_SIPCCCallInfoPtr
    CCAPI_Call_releaseCallInfo(callInfo);
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
		pMediaData->streamMap[streamId].isVideo = isVideo;
	}
	// The new stream needs to be given any properties that the call has for it.
	// At the moment the only candidate is the muted state
	if (isVideo)
	{
#ifndef NO_WEBRTC_VIDEO
        VideoTermination * pVideo = VcmSIPCCBinding::getVideoTermination();

        // if there is a window for this call apply it to the stream
        if ( pMediaData->remoteWindow != nullptr)
        {
            pVideo->setRemoteWindow(streamId,  pMediaData->remoteWindow);
        }
        else
        {
            CSFLogInfo( logTag, "addStream: remoteWindow is NULL");
        }

		if(pMediaData->extRenderer != nullptr)
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

fsm_fcb_t* CC_SIPCCCall::preOperationBoilerplate(cc_feature_t *command,
                                                 Timecard *tc) {
    // Make sure the old error string doesn't hang around.
    strlib_free(errorString);
    errorString = NULL;

    // Get the fcb, and if it fails, set the appropriate error stuff
    fsm_fcb_t *fcb = getFcb();
    if (!fcb) {
      errorString = strlib_printf("No call state object");
      // Can happen if we create too many peerconnections simultaneously
      error = PC_INTERNAL_ERROR;
      return NULL;
    }

    // Perform common init on the command struct
    memset(command, 0, sizeof(cc_feature_t));
    command->call_id = GET_CALL_ID(callHandle);
    command->line = GET_LINE_ID(callHandle);
    command->timecard = tc;
    return fcb;
}

pc_error CC_SIPCCCall::createOffer (cc_media_options_t *options, Timecard *tc) {
    cc_feature_t command;
    fsm_fcb_t *fcb = preOperationBoilerplate(&command, tc);

    if (!fcb) {
      return error;
    }

    command.data.session.options = options;

    switch (fcb->state) {
      case FSMDEF_S_STABLE:
        strlib_free(localSdp);
        localSdp = NULL;
        error = fsmdef_createoffer(fcb, &command, &localSdp, &errorString);
        break;
      default:
        error = PC_INVALID_STATE;
        errorString = strlib_printf("Cannot create offer in state %s",
                                     fsmdef_state_name(fcb->state));
    }
    return error;
}


pc_error CC_SIPCCCall::createAnswer (Timecard *tc) {
    cc_feature_t command;
    fsm_fcb_t *fcb = preOperationBoilerplate(&command, tc);

    if (!fcb) {
      return error;
    }

    switch (fcb->state) {
      case FSMDEF_S_STABLE:
      case FSMDEF_S_HAVE_REMOTE_OFFER:
        strlib_free(localSdp);
        localSdp = NULL;
        error = fsmdef_createanswer(fcb, &command, &localSdp, &errorString);
        break;
      default:
        error = PC_INVALID_STATE;
        errorString = strlib_printf("Cannot create answer in state %s",
                                     fsmdef_state_name(fcb->state));
    }
    return error;
}

pc_error CC_SIPCCCall::setLocalDescription(cc_jsep_action_t action,
                                       const std::string & sdp,
                                       Timecard *tc) {
    cc_feature_t command;
    fsm_fcb_t *fcb = preOperationBoilerplate(&command, tc);

    if (!fcb) {
      return error;
    }

    command.action = action;
    command.sdp = const_cast<char*>(sdp.c_str()); // Ugh

    switch (fcb->state) {
      case FSMDEF_S_STABLE:
      case FSMDEF_S_HAVE_REMOTE_OFFER:
        strlib_free(localSdp);
        localSdp = NULL;
        error = fsmdef_setlocaldesc(fcb, &command, &localSdp, &errorString);
        break;
      default:
        error = PC_INVALID_STATE;
        errorString = strlib_printf("Cannot set local SDP in state %s",
                                     fsmdef_state_name(fcb->state));
    }
    return error;
}

pc_error CC_SIPCCCall::setRemoteDescription(cc_jsep_action_t action,
                                       const std::string & sdp,
                                       Timecard *tc) {
    cc_feature_t command;
    fsm_fcb_t *fcb = preOperationBoilerplate(&command, tc);

    if (!fcb) {
      return error;
    }

    command.action = action;
    command.sdp = const_cast<char*>(sdp.c_str()); // Ugh

    switch (fcb->state) {
      case FSMDEF_S_STABLE: // should be an offer
      case FSMDEF_S_HAVE_LOCAL_OFFER: // should be an answer
      case FSMDEF_S_HAVE_REMOTE_PRANSWER:
        // TODO: Refactor so that error-handling is performed here?
        strlib_free(remoteSdp);
        remoteSdp = NULL;
        error = fsmdef_setremotedesc(fcb, &command, &remoteSdp, &errorString);
        break;
      default:
        error = PC_INVALID_STATE;
        errorString = strlib_printf("Cannot set remote SDP in state %s",
                                     fsmdef_state_name(fcb->state));
    }
    return error;
}

pc_error CC_SIPCCCall::setPeerConnection(const std::string& handle)
{
  CSFLogDebug(logTag, "setPeerConnection");

  // Cause the fcb to be created
  fim_get_new_call_chn(GET_CALL_ID(callHandle));

  cc_feature_t command;
  fsm_fcb_t *fcb = preOperationBoilerplate(&command, NULL);

  if (!fcb) {
    return error;
  }

  peerconnection = handle;  // Cache this here. we need it to make the CC_SIPCCCallInfo

  strncpy(command.data.pc.pc_handle,
          handle.c_str(),
          sizeof(command.data.pc.pc_handle));

  switch (fcb->state) {
    case FSMDEF_S_IDLE:
      fsmdef_setpeerconnection(fcb, &command);
      // TODO Maybe let this return errors like the other stuff?
      error = PC_NO_ERROR;
      break;
    default:
      errorString = strlib_printf("Cannot set peerconnection in state %s",
                                   fsmdef_state_name(fcb->state));
      error = PC_INVALID_STATE;
  }
  return error;
}

const std::string& CC_SIPCCCall::getPeerConnection() const {
  return peerconnection;
}

pc_error CC_SIPCCCall::addStream(cc_media_stream_id_t stream_id,
                             cc_media_track_id_t track_id,
                             cc_media_type_t media_type) {
    cc_feature_t command;
    fsm_fcb_t *fcb = preOperationBoilerplate(&command, NULL);

    if (!fcb) {
      return error;
    }

    command.data.track.stream_id = stream_id;
    command.data.track.track_id = track_id;
    command.data.track.media_type = media_type;

    switch (fcb->state) {
      case FSMDEF_S_STABLE:
      case FSMDEF_S_HAVE_REMOTE_OFFER:
        error = fsmdef_addstream(fcb, &command, &errorString);
        break;
      default:
        errorString = strlib_printf("Cannot add stream in state %s",
                                     fsmdef_state_name(fcb->state));
        error = PC_INVALID_STATE;
    }
    return error;
}

pc_error CC_SIPCCCall::removeStream(cc_media_stream_id_t stream_id, cc_media_track_id_t track_id, cc_media_type_t media_type) {
    cc_feature_t command;
    fsm_fcb_t *fcb = preOperationBoilerplate(&command, NULL);

    if (!fcb) {
      return error;
    }

    command.data.track.stream_id = stream_id;
    command.data.track.track_id = track_id;
    command.data.track.media_type = media_type;

    switch (fcb->state) {
      case FSMDEF_S_STABLE:
      case FSMDEF_S_HAVE_REMOTE_OFFER:
        error = fsmdef_removestream(fcb, &command, &errorString);
        break;
      default:
        errorString = strlib_printf("Cannot remove stream in state %s",
                                     fsmdef_state_name(fcb->state));
        error = PC_INVALID_STATE;
    }
    return error;
}

pc_error CC_SIPCCCall::addICECandidate(const std::string & candidate,
                                   const std::string & mid,
                                   unsigned short level,
                                   Timecard *tc) {
    cc_feature_t command;
    fsm_fcb_t *fcb = preOperationBoilerplate(&command, tc);

    if (!fcb) {
      return error;
    }

    command.data.candidate.level = level;
    strncpy(command.data.candidate.candidate,
            candidate.c_str(),
            sizeof(command.data.candidate.candidate));
    strncpy(command.data.candidate.mid,
            mid.c_str(),
            sizeof(command.data.candidate.mid));

    switch (fcb->state) {
      case FSMDEF_S_STABLE:
      case FSMDEF_S_HAVE_REMOTE_OFFER:
      case FSMDEF_S_HAVE_LOCAL_PRANSWER:
      case FSMDEF_S_HAVE_REMOTE_PRANSWER:
        strlib_free(remoteSdp);
        remoteSdp = NULL;
        error = fsmdef_addcandidate(fcb, &command, &remoteSdp, &errorString);
        break;
      default:
        errorString = strlib_printf("Cannot add remote candidate in state %s",
                                     fsmdef_state_name(fcb->state));
        error = PC_INVALID_STATE;
    }
    return error;
}


pc_error CC_SIPCCCall::foundICECandidate(const std::string & candidate,
                                     const std::string & mid,
                                     unsigned short level,
                                     Timecard *tc) {
    cc_feature_t command;
    fsm_fcb_t *fcb = preOperationBoilerplate(&command, tc);

    if (!fcb) {
      return error;
    }

    command.data.candidate.level = level;
    strncpy(command.data.candidate.candidate,
            candidate.c_str(),
            sizeof(command.data.candidate.candidate));
    strncpy(command.data.candidate.mid,
            mid.c_str(),
            sizeof(command.data.candidate.mid));

    switch (fcb->state) {
      case FSMDEF_S_STABLE:
      case FSMDEF_S_HAVE_LOCAL_OFFER:
      case FSMDEF_S_HAVE_LOCAL_PRANSWER:
      case FSMDEF_S_HAVE_REMOTE_PRANSWER:
        strlib_free(localSdp);
        localSdp = NULL;
        error = fsmdef_foundcandidate(fcb, &command, &localSdp, &errorString);
        break;
      default:
        errorString = strlib_printf("Cannot add local candidate in state %s",
                                     fsmdef_state_name(fcb->state));
        error = PC_INVALID_STATE;
    }
    return error;
}
