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

#ifndef _CCAPI_CONF_ROSTER_H_
#define _CCAPI_CONF_ROSTER_H_

#include "cpr_stdio.h"
#include "ccapi_call.h"
#include "CCProvider.h"
#include "phone_debug.h"

// Basic type definitions for conferencing
typedef string_t cc_participant_ref_t;

/**
* Get Conference Participants
*
* Returns list of conference participant information last received from the UCM.
* Note that the list may include conference participants in various states in addition to Connected.
* [ For exampke, a listed participant may be in the ringing state (not yet on conference), 
* or disconnected state (leaving/left the conference).]  Application should invoke
* getConferenceParticipantStatus for each participant in the list to query exact status.
*
* @param [in] handle - call handle
* @param [in/out] participantHandles - array of participant handles to be returned
* @param [in/out] count - in:  size of array provided in participantHandles; out:  number of entries populated (up to original value provided)
* @return void
*/
void CCAPI_CallInfo_getConfParticipants (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandles[], int* count);


/**
* Get Maximum Number of Conference Participants ( in case gui wants to show %full conference info )
* @param [in] handle - call handle
* @return maximum number of conference participants
*/
cc_uint16_t CCAPI_CallInfo_getConfParticipantMax (cc_callinfo_ref_t handle);


/**
* Get Participant Name
* @param [in] handle - call info handle
* @param [in] participantHandle - specific handle for conference participant
* @return display name of the conference participant
*/
cc_string_t CCAPI_CallInfo_getConfParticipantName (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandle);

/**
* Get Participant Number
* @param [in] handle - handle of call
* @param [in] participantHandle - handle of conference participant
* @return display number of the conference participant
*/
cc_string_t CCAPI_CallInfo_getConfParticipantNumber (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandle);

/**
* Get Conference Participant Status
* @param [in] handle - call handle
* @param [in] participantHandle - handle of conference participant
* @return conference status of specific participant
*/
cc_conf_participant_status_t CCAPI_CallInfo_getConfParticipantStatus (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandleHandle);

/**
* Get Participant Security
* @param [in] handle - call handle
* @param [in] participantHandle - handle of conference participant
* @return security setting of the specific conference participant
*/
cc_call_security_t CCAPI_CallInfo_getConfParticipantSecurity (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandleHandle);

/**
* Able To Remove Others?
* Find out whether this participant is capable of removing another participant
* @param [in] handle - call handle
* @return return code (0=indicates not capable, 1=indicates capable)
*/
cc_boolean CCAPI_CallInfo_selfHasRemoveConfParticipantCapability (cc_callinfo_ref_t handle);

/**
* Check to see if a given conference participant reference is the one using this endpoint
* @param [in] handle - call handle
* @param [in] participantHandle - handle of a conference participant
* @returns cc_boolean (TRUE if yes, FALSE if no)
*/
cc_boolean CCAPI_CallInfo_isConfSelfParticipant (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandle);

/**
* Return our conference participant-id
* @param [in] handle - call handle
* @return participant id of self
*/
cc_participant_ref_t CCAPI_CallInfo_getConfSelfParticipant (cc_callinfo_ref_t handle);


#endif /* _CCAPI_CONF_ROSTER_H_ */
