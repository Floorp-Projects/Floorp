/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"

#include "CC_Common.h"

#include "CC_SIPCCCallInfo.h"
#include "CC_SIPCCLine.h"

extern "C"
{
#include "ccapi_call.h"
#include "ccapi_call_info.h"
#include "fsmdef_states.h"
}

static const char* logTag = "CC_SIPCCCallInfo";

using namespace std;
using namespace CSF;

CC_SIPCCCallInfo::CC_SIPCCCallInfo (cc_callinfo_ref_t callinfo) : callinfo_ref(callinfo)
{
    CCAPI_Call_retainCallInfo(callinfo);
}

CSF_IMPLEMENT_WRAP(CC_SIPCCCallInfo, cc_callinfo_ref_t);

CC_SIPCCCallInfo::~CC_SIPCCCallInfo()
{
    CCAPI_Call_releaseCallInfo(callinfo_ref);
}

bool CC_SIPCCCallInfo::hasCapability (CC_CallCapabilityEnum::CC_CallCapability capability)
{
	generateCapabilities();
	return (caps.find(capability) != caps.end());
}

set<CC_CallCapabilityEnum::CC_CallCapability> CC_SIPCCCallInfo::getCapabilitySet()
{
	generateCapabilities();
    set<CC_CallCapabilityEnum::CC_CallCapability> callCaps(caps);
    return callCaps;
}

/*
CC_LinePtr CC_SIPCCCallInfo::getLine ()
{
}
*/

cc_call_state_t CC_SIPCCCallInfo::getCallState()
{
    return CCAPI_CallInfo_getCallState(callinfo_ref);
}

fsmdef_states_t CC_SIPCCCallInfo::getFsmState() const
{
    return CCAPI_CallInfo_getFsmState(callinfo_ref);
}

std::string CC_SIPCCCallInfo::fsmStateToString (fsmdef_states_t state) const
{
  return fsmdef_state_name(state);
}

std::string CC_SIPCCCallInfo::callStateToString (cc_call_state_t state)
{
  std::string statestr = "";

    switch(state) {
      case OFFHOOK:
        statestr = "OFFHOOK";
        break;
      case ONHOOK:
        statestr = "ONHOOK";
        break;
      case RINGOUT:
        statestr = "RINGOUT";
        break;
      case RINGIN:
        statestr = "RINGIN";
        break;
      case PROCEED:
        statestr = "PROCEED";
        break;
      case CONNECTED:
        statestr = "CONNECTED";
        break;
      case HOLD:
        statestr = "ONHOOK";
        break;
      case REMHOLD:
        statestr = "REMHOLD";
        break;
      case RESUME:
        statestr = "RESUME";
        break;
      case BUSY:
        statestr = "BUSY";
        break;
      case REORDER:
        statestr = "REORDER";
        break;
      case CONFERENCE:
        statestr = "CONFERENCE";
        break;
      case DIALING:
        statestr = "DIALING";
        break;
      case REMINUSE:
        statestr = "REMINUSE";
        break;
      case HOLDREVERT:
        statestr = "HOLDREVERT";
        break;
      case WHISPER:
        statestr = "WHISPER";
        break;
      case PRESERVATION:
        statestr = "PRESERVATION";
        break;
      case WAITINGFORDIGITS:
        statestr = "WAITINGFORDIGITS";
        break;
      default:
        break;
    }

    return statestr;
}

std::string CC_SIPCCCallInfo::callEventToString (ccapi_call_event_e callEvent)
{
  std::string statestr = "";

    switch(callEvent) {
      case CCAPI_CALL_EV_CREATED:
        statestr = "CCAPI_CALL_EV_CREATED";
        break;
      case CCAPI_CALL_EV_STATE:
        statestr = "CCAPI_CALL_EV_STATE";
        break;
      case CCAPI_CALL_EV_CALLINFO:
        statestr = "CCAPI_CALL_EV_CALLINFO";
        break;
      case CCAPI_CALL_EV_ATTR:
        statestr = "CCAPI_CALL_EV_ATTR";
        break;
      case CCAPI_CALL_EV_SECURITY:
        statestr = "CCAPI_CALL_EV_SECURITY";
        break;
      case CCAPI_CALL_EV_LOG_DISP:
        statestr = "CCAPI_CALL_EV_LOG_DISP";
        break;
      case CCAPI_CALL_EV_PLACED_CALLINFO:
        statestr = "CCAPI_CALL_EV_PLACED_CALLINFO";
        break;
      case CCAPI_CALL_EV_STATUS:
        statestr = "CCAPI_CALL_EV_STATUS";
        break;
      case CCAPI_CALL_EV_SELECT:
        statestr = "CCAPI_CALL_EV_SELECT";
        break;
      case CCAPI_CALL_EV_LAST_DIGIT_DELETED:
        statestr = "CCAPI_CALL_EV_LAST_DIGIT_DELETED";
        break;
      case CCAPI_CALL_EV_GCID:
        statestr = "CCAPI_CALL_EV_GCID";
        break;
      case CCAPI_CALL_EV_XFR_OR_CNF_CANCELLED:
        statestr = "CCAPI_CALL_EV_XFR_OR_CNF_CANCELLED";
        break;
      case CCAPI_CALL_EV_PRESERVATION:
        statestr = "CCAPI_CALL_EV_PRESERVATION";
        break;
      case CCAPI_CALL_EV_CAPABILITY:
        statestr = "CCAPI_CALL_EV_CAPABILITY";
        break;
      case CCAPI_CALL_EV_VIDEO_AVAIL:
        statestr = "CCAPI_CALL_EV_VIDEO_AVAIL";
        break;
      case CCAPI_CALL_EV_VIDEO_OFFERED:
        statestr = "CCAPI_CALL_EV_VIDEO_OFFERED";
        break;
      case CCAPI_CALL_EV_RECEIVED_INFO:
        statestr = "CCAPI_CALL_EV_RECEIVED_INFO";
        break;
      case CCAPI_CALL_EV_RINGER_STATE:
        statestr = "CCAPI_CALL_EV_RINGER_STATE";
        break;
      case CCAPI_CALL_EV_CONF_PARTICIPANT_INFO:
        statestr = "CCAPI_CALL_EV_CONF_PARTICIPANT_INFO";
        break;
      case CCAPI_CALL_EV_MEDIA_INTERFACE_UPDATE_BEGIN:
        statestr = "CCAPI_CALL_EV_MEDIA_INTERFACE_UPDATE_BEGIN";
        break;
      case CCAPI_CALL_EV_MEDIA_INTERFACE_UPDATE_SUCCESSFUL:
        statestr = "CCAPI_CALL_EV_MEDIA_INTERFACE_UPDATE_SUCCESSFUL";
        break;
      case CCAPI_CALL_EV_MEDIA_INTERFACE_UPDATE_FAIL:
        statestr = "CCAPI_CALL_EV_MEDIA_INTERFACE_UPDATE_FAIL";
        break;
      default:
        break;
    }

    return statestr;
}

bool CC_SIPCCCallInfo::getRingerState()
{
    if (CCAPI_CallInfo_getRingerState(callinfo_ref))
    {
    	return true;
    }
    else
    {
    	return false;
    }
}

cc_call_attr_t CC_SIPCCCallInfo::getCallAttr()
{
    return CCAPI_CallInfo_getCallAttr(callinfo_ref);
}

cc_call_type_t CC_SIPCCCallInfo::getCallType()
{
    return CCAPI_CallInfo_getCallType(callinfo_ref);
}

string CC_SIPCCCallInfo::getCalledPartyName()
{
    return CCAPI_CallInfo_getCalledPartyName(callinfo_ref);
}

string CC_SIPCCCallInfo::getCalledPartyNumber()
{
    return CCAPI_CallInfo_getCalledPartyNumber(callinfo_ref);
}

string CC_SIPCCCallInfo::getCallingPartyName()
{
    return CCAPI_CallInfo_getCallingPartyName(callinfo_ref);
}

string CC_SIPCCCallInfo::getCallingPartyNumber()
{
    return CCAPI_CallInfo_getCallingPartyNumber(callinfo_ref);
}

string CC_SIPCCCallInfo::getAlternateNumber()
{
    return CCAPI_CallInfo_getAlternateNumber(callinfo_ref);
}

CC_LinePtr CC_SIPCCCallInfo::getline ()
{
    cc_lineid_t lineId = CCAPI_CallInfo_getLine(callinfo_ref);
    return CC_SIPCCLine::wrap(lineId).get();
}

string CC_SIPCCCallInfo::getOriginalCalledPartyName()
{
    return CCAPI_CallInfo_getOriginalCalledPartyName(callinfo_ref);
}

string CC_SIPCCCallInfo::getOriginalCalledPartyNumber()
{
    return CCAPI_CallInfo_getOriginalCalledPartyNumber(callinfo_ref);
}

string CC_SIPCCCallInfo::getLastRedirectingPartyName()
{
    return CCAPI_CallInfo_getLastRedirectingPartyName(callinfo_ref);
}

string CC_SIPCCCallInfo::getLastRedirectingPartyNumber()
{
    return CCAPI_CallInfo_getLastRedirectingPartyNumber(callinfo_ref);
}

string CC_SIPCCCallInfo::getPlacedCallPartyName()
{
    return CCAPI_CallInfo_getPlacedCallPartyName(callinfo_ref);
}

string CC_SIPCCCallInfo::getPlacedCallPartyNumber()
{
    return CCAPI_CallInfo_getPlacedCallPartyNumber(callinfo_ref);
}

cc_int32_t CC_SIPCCCallInfo::getCallInstance()
{
    return CCAPI_CallInfo_getCallInstance(callinfo_ref);
}

string CC_SIPCCCallInfo::getStatus()
{
    return CCAPI_CallInfo_getStatus(callinfo_ref);
}

cc_call_security_t CC_SIPCCCallInfo::getSecurity()
{
    return CCAPI_CallInfo_getSecurity(callinfo_ref);
}

cc_int32_t CC_SIPCCCallInfo::getSelectionStatus()
{
    return CCAPI_CallInfo_getSelectionStatus(callinfo_ref);
}

string CC_SIPCCCallInfo::getGCID()
{
    return CCAPI_CallInfo_getGCID(callinfo_ref);
}

bool CC_SIPCCCallInfo::getIsRingOnce()
{
    return (CCAPI_CallInfo_getIsRingOnce(callinfo_ref) != 0);
}

int CC_SIPCCCallInfo::getRingerMode()
{
    return CCAPI_CallInfo_getRingerMode(callinfo_ref);
}

cc_int32_t CC_SIPCCCallInfo::getOnhookReason()
{
    return CCAPI_CallInfo_getOnhookReason(callinfo_ref);
}

bool CC_SIPCCCallInfo::getIsConference()
{
    return (CCAPI_CallInfo_getIsConference(callinfo_ref) != 0);
}

set<cc_int32_t> CC_SIPCCCallInfo::getStreamStatistics()
{
    CSFLogError(logTag, "CCAPI_CallInfo_getCapabilitySet() NOT IMPLEMENTED IN PSIPCC.");
    set<cc_int32_t> stats;
    return stats;
}

bool CC_SIPCCCallInfo::isCallSelected()
{
    return (CCAPI_CallInfo_isCallSelected(callinfo_ref) != 0);
}

string CC_SIPCCCallInfo::getINFOPack()
{
    return CCAPI_CallInfo_getINFOPack(callinfo_ref);
}

string CC_SIPCCCallInfo::getINFOType()
{
    return CCAPI_CallInfo_getINFOType(callinfo_ref);
}

string CC_SIPCCCallInfo::getINFOBody()
{
    return CCAPI_CallInfo_getINFOBody(callinfo_ref);
}

cc_calllog_ref_t  CC_SIPCCCallInfo::getCallLogRef()
{
    return CCAPI_CallInfo_getCallLogRef(callinfo_ref);
}

cc_sdp_direction_t CC_SIPCCCallInfo::getVideoDirection()
{
    return CCAPI_CallInfo_getVideoDirection(callinfo_ref);
}

int CC_SIPCCCallInfo::getVolume()
{
    if( pMediaData != nullptr)
    {
        return  pMediaData->volume;
    }
    else
    {
        return -1;
    }
}

bool CC_SIPCCCallInfo::isAudioMuted()
{
    return (CCAPI_CallInfo_isAudioMuted(callinfo_ref) != 0);
}

bool CC_SIPCCCallInfo::isVideoMuted()
{
    return (CCAPI_CallInfo_isVideoMuted(callinfo_ref) != 0);
}

bool CC_SIPCCCallInfo::isMediaStateAvailable()
{
    // for softphone it will always be possible to query the mute state and video direction
    return true;
}


#define PRINT_IF_CC_CAP_TRUE(cap) ((hasFeature(cap)) ? string(#cap) + ",": "")
void CC_SIPCCCallInfo::generateCapabilities()
{
	// If caps are already generated, no need to repeat the exercise.
	if(!caps.empty())
		return;
/*
	CSFLogDebugS( logTag, "generateCapabilities() state=" << getCallState() <<
			" underlyingCaps=" <<
			PRINT_IF_CC_CAP_TRUE(CCAPI_CALL_CAP_NEWCALL) <<
			PRINT_IF_CC_CAP_TRUE(CCAPI_CALL_CAP_ANSWER) <<
			PRINT_IF_CC_CAP_TRUE(CCAPI_CALL_CAP_ENDCALL) <<
			PRINT_IF_CC_CAP_TRUE(CCAPI_CALL_CAP_HOLD) <<
			PRINT_IF_CC_CAP_TRUE(CCAPI_CALL_CAP_RESUME) <<
			PRINT_IF_CC_CAP_TRUE(CCAPI_CALL_CAP_CALLFWD) <<
			PRINT_IF_CC_CAP_TRUE(CCAPI_CALL_CAP_DIAL) <<
			PRINT_IF_CC_CAP_TRUE(CCAPI_CALL_CAP_BACKSPACE) <<
			PRINT_IF_CC_CAP_TRUE(CCAPI_CALL_CAP_SENDDIGIT) <<
			PRINT_IF_CC_CAP_TRUE(CCAPI_CALL_CAP_TRANSFER) <<
			PRINT_IF_CC_CAP_TRUE(CCAPI_CALL_CAP_CONFERENCE) <<
			PRINT_IF_CC_CAP_TRUE(CCAPI_CALL_CAP_SWAP) <<
			PRINT_IF_CC_CAP_TRUE(CCAPI_CALL_CAP_REDIAL) <<
			PRINT_IF_CC_CAP_TRUE(CCAPI_CALL_CAP_JOIN) <<
			PRINT_IF_CC_CAP_TRUE(CCAPI_CALL_CAP_SELECT) <<
			PRINT_IF_CC_CAP_TRUE(CCAPI_CALL_CAP_RMVLASTPARTICIPANT) );
*/
	switch(getCallState())
	{
	case OFFHOOK:
		if(hasFeature(CCAPI_CALL_CAP_NEWCALL)) {
			caps.insert(CC_CallCapabilityEnum::canOriginateCall);
		}
		if(hasFeature(CCAPI_CALL_CAP_ENDCALL)) {
			caps.insert(CC_CallCapabilityEnum::canEndCall);
		}
		break;
	case ONHOOK:
		break;
	case DIALING:
	case PROCEED:
	case RINGOUT:
		if(hasFeature(CCAPI_CALL_CAP_ENDCALL)) {
			caps.insert(CC_CallCapabilityEnum::canEndCall);
		}
		if(hasFeature(CCAPI_CALL_CAP_SENDDIGIT)) caps.insert(CC_CallCapabilityEnum::canSendDigit);
		break;
	case RINGIN:
		if(hasFeature(CCAPI_CALL_CAP_ANSWER)) caps.insert(CC_CallCapabilityEnum::canAnswerCall);
		break;
	case CONNECTED:
		if(hasFeature(CCAPI_CALL_CAP_ENDCALL)) {
			caps.insert(CC_CallCapabilityEnum::canEndCall);
		}
		caps.insert(CC_CallCapabilityEnum::canSendDigit);
		if(hasFeature(CCAPI_CALL_CAP_HOLD)) caps.insert(CC_CallCapabilityEnum::canHold);

        caps.insert(CC_CallCapabilityEnum::canSetVolume);
		if(isAudioMuted())
        {
            caps.insert(CC_CallCapabilityEnum::canUnmuteAudio);
        }
        else
        {
            caps.insert(CC_CallCapabilityEnum::canMuteAudio);
        }

        if ((CCAPI_CallInfo_getVideoDirection(callinfo_ref) == CC_SDP_DIRECTION_SENDRECV) ||
            (CCAPI_CallInfo_getVideoDirection(callinfo_ref) == CC_SDP_DIRECTION_SENDONLY))
        {
            // sending video so video mute is possible
            if (isVideoMuted())
            {
                caps.insert(CC_CallCapabilityEnum::canUnmuteVideo);
            }
            else
            {
                caps.insert(CC_CallCapabilityEnum::canMuteVideo);
            }
        }
        caps.insert(CC_CallCapabilityEnum::canUpdateVideoMediaCap);
		break;
	case HOLD:
	case REMHOLD:
        caps.insert(CC_CallCapabilityEnum::canResume);
		break;

	case BUSY:
	case REORDER:
		if(hasFeature(CCAPI_CALL_CAP_ENDCALL)) {
			caps.insert(CC_CallCapabilityEnum::canEndCall);
		}
		break;
	case PRESERVATION:
		if(hasFeature(CCAPI_CALL_CAP_ENDCALL)) {
			caps.insert(CC_CallCapabilityEnum::canEndCall);
		}
		break;

	// Not worrying about these states yet.
	case RESUME:
	case CONFERENCE:
	case REMINUSE:
	case HOLDREVERT:
	case WHISPER:
	case WAITINGFORDIGITS:
	default:
		CSFLogError( logTag, "State %d not handled in generateCapabilities()",
      getCallState());
		break;
	}
}

bool CC_SIPCCCallInfo::hasFeature(ccapi_call_capability_e cap)
{
	return (CCAPI_CallInfo_hasCapability(callinfo_ref, (cc_int32_t) cap) != 0);
}

void CC_SIPCCCallInfo::setMediaData(CC_SIPCCCallMediaDataPtr  pMediaData)
{
    this-> pMediaData =  pMediaData;
}

