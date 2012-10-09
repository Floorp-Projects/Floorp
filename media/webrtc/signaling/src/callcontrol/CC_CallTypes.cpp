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
