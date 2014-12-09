/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CC_CallTypes.h"

using namespace std;

namespace CSF
{
namespace CC_CallCapabilityEnum
{

std::string toString(CC_CallCapability cap)
{
	switch(cap)
	{
	case canSetRemoteWindow:
		return "canSetRemoteWindow";
	case canSetLocalWindow:
		return "canSetLocalWindow";
	case canSendIFrame:
		return "canSendIFrame";
	case canOriginateCall:
		return "canOriginateCall";
	case canAnswerCall:
		return "canAnswerCall";
	case canHold:
		return "canHold";
	case canResume:
		return "canResume";
	case canEndCall:
		return "canEndCall";
	case canSendDigit:
		return "canSendDigit";
	case canBackspace:
		return "canBackspace";
	case canRedial:
		return "canRedial";
	case canInitiateCallForwardAll:
		return "canInitiateCallForwardAll";
	case canEndConsultativeCall:
		return "canEndConsultativeCall";
	case canConferenceStart:
		return "canConferenceStart";
	case canConferenceComplete:
		return "canConferenceComplete";
	case canTransferStart:
		return "canTransferStart";
	case canTransferComplete:
		return "canTransferComplete";
	case canCancelTransferOrConferenceFeature:
		return "canCancelTransferOrConferenceFeature";
	case canDirectTransfer:
		return "canDirectTransfer";
	case canJoinAcrossLine:
		return "canJoinAcrossLine";
	case canBlfCallPickup:
		return "canBlfCallPickup";
	case canSelect:
		return "canSelect";
	case canUpdateVideoMediaCap:
		return "canUpdateVideoMediaCap";
	case canSendInfo:
		return "canSendInfo";
	case canMuteAudio:
		return "canMuteAudio";
    case canUnmuteAudio:
        return "canUnmuteAudio";
	case canMuteVideo:
		return "canMuteVideo";
    case canUnmuteVideo:
        return "canUnmuteVideo";
    case canSetVolume:
        return "canSetVolume";
	default:
		return "";
	}
}

std::string toString(std::set<CC_CallCapability>& caps)
{
	string result;
	for(std::set<CC_CallCapability>::iterator it = caps.begin(); it != caps.end(); it++)
	{
		if(!result.empty())
			result += ",";
		result += toString(*it);
	}
	return result;
}

}
}
