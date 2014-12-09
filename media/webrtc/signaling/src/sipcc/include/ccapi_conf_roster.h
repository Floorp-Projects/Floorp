/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
