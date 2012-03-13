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

#include "CC_Common.h"

#include "CC_SIPCCCallInfo.h"
#include "CC_SIPCCLine.h"

extern "C"
{
#include "ccapi_call.h"
#include "ccapi_call_info.h"
}

#include "CSFLogStream.h"
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
    return CC_SIPCCLine::wrap(lineId);
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
    CSFLogErrorS(logTag, "CCAPI_CallInfo_getCapabilitySet() NOT IMPLEMENTED IN PSIPCC.");
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
    if( pMediaData  != NULL)
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
		CSFLogErrorS( logTag, "State " << getCallState() << " not handled in generateCapabilities()");
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

