/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "sll_lite.h"
#include "cc_constants.h"
#include "cc_types.h"
#include "cc_config.h"
#include "phone_debug.h"
#include "debug.h"
#include "CCProvider.h"
#include "ccapi_call_info.h"
#include "conf_roster.h"
#include "ccapi.h"
#include "ccapp_task.h"

cc_conf_participant_status_t
convertStringToParticipantStatus(const char *data)
{
    if (strcmp(data, "connected") == 0) {
        return CCAPI_CONFPARTICIPANT_CONNECTED;
    } else if (strcmp(data, "alerting") == 0) {
        return CCAPI_CONFPARTICIPANT_ALERTING;
    } else if (strcmp(data, "dialing-out") == 0) {
        return CCAPI_CONFPARTICIPANT_DIALING_OUT;
    } else if (strcmp(data, "on-hold") == 0) {
        return CCAPI_CONFPARTICIPANT_ON_HOLD;
    } else if (strcmp(data, "disconnected") == 0) {
        return CCAPI_CONFPARTICIPANT_DISCONNECTED;
    } else {
        return CCAPI_CONFPARTICIPANT_UNKNOWN;
    }
}

cc_call_security_t
convertStringToParticipantSecurity(const char *data)
{

    if (strcmp(data, "NotAuthenticated") == 0) {
        return CC_SECURITY_NOT_AUTHENTICATED;
    } else if (strcmp(data, "Authenticated") == 0) {
        return CC_SECURITY_AUTHENTICATED;
    } else if (strcmp(data, "Encrypted") == 0) {
        return CC_SECURITY_ENCRYPTED;
    } else if (strcmp(data, "Unknown") == 0) {
        return CC_SECURITY_UNKNOWN;
    } else {
        return CC_SECURITY_NONE;
    }
}


void conf_roster_init_call_conference (cc_call_conference_Info_t *info)
{
    CCAPP_DEBUG(DEB_F_PREFIX"in init_call_conference", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));

    info->participantMax = 0;
    info->participantCount = 0;
    info->myParticipantId = strlib_empty();

    sll_lite_init(&info->currentParticipantsList);
}

void conf_roster_free_call_conference (cc_call_conference_Info_t *confInfo)
{
    cc_call_conferenceParticipant_Info_t *participant;

    CCAPP_DEBUG(DEB_F_PREFIX"in free_call_confrerence", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));

    while((participant=(cc_call_conferenceParticipant_Info_t *)
                sll_lite_unlink_head(&confInfo->currentParticipantsList)) != NULL)
    {
        strlib_free(participant->participantName);
        strlib_free(participant->endpointUri);
        strlib_free(participant->callid);
        strlib_free(participant->participantNumber);

        participant->participantSecurity        = CC_SECURITY_NONE;
        participant->participantStatus          = CCAPI_CONFPARTICIPANT_UNKNOWN;
        participant->canRemoveOtherParticipants = FALSE;

        cpr_free(participant);
        participant = NULL;
    }

    strlib_free(confInfo->myParticipantId);
    conf_roster_init_call_conference(confInfo);
}

void conf_roster_copy_call_conferance (cc_call_conference_Info_t *dest, cc_call_conference_Info_t * src)
{
    cc_call_conferenceParticipant_Info_t *destParticipant;
    cc_call_conferenceParticipant_Info_t *srcParticipant;
    sll_lite_node_t *iterator;
    sll_lite_return_e sll_ret_val;

    CCAPP_DEBUG(DEB_F_PREFIX"in copy_call_confrerence", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));

    iterator = src->currentParticipantsList.head_p;
    conf_roster_init_call_conference(dest);

    dest->participantMax = src->participantMax;
    dest->participantCount = src->participantCount;
    dest->myParticipantId = strlib_copy(src->myParticipantId);

    while (iterator) {
        srcParticipant = (cc_call_conferenceParticipant_Info_t *)iterator;

        destParticipant = cpr_malloc(sizeof(cc_call_conferenceParticipant_Info_t));
        if (destParticipant == NULL) {
            CCAPP_ERROR(DEB_F_PREFIX" Malloc failure for participant", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));
            return;
        } else {
            destParticipant->participantName = strlib_copy(srcParticipant->participantName);
            destParticipant->endpointUri = strlib_copy(srcParticipant->endpointUri);
            destParticipant->callid = strlib_copy(srcParticipant->callid);

            destParticipant->participantNumber          = strlib_copy(srcParticipant->participantNumber);
            destParticipant->participantSecurity        = srcParticipant->participantSecurity;
            destParticipant->participantStatus          = srcParticipant->participantStatus;
            destParticipant->canRemoveOtherParticipants = srcParticipant->canRemoveOtherParticipants;
        }

        sll_ret_val = sll_lite_link_tail(&dest->currentParticipantsList, (sll_lite_node_t *)destParticipant);
        if (sll_ret_val != SLL_LITE_RET_SUCCESS) {
            CCAPP_ERROR(DEB_F_PREFIX" Error while trying to insert in the linked list", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"));
            cpr_free(destParticipant);
            return;
        }

        iterator = iterator->next_p;
    }
}

// -------------------
// API Implementation
// -------------------

/**
* Get Conference Participants
* @param [in] handle - call handle
* @param [in/out] participantHandles - array of participant handles to be returned
* @param [in/out] count - in:  size of array provided in participantHandles; out:  number of entries populated (up to original value provided)
* @return void
*/
void CCAPI_CallInfo_getConfParticipants (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandles[], int* count)
{
   cc_call_conference_ref_t              callConference   = NULL;    // conference reference (from call info)
   cc_call_conference_participant_ref_t  participant      = NULL;    // participant reference
   cc_uint16_t                           participantIndex = 0;       // participant index
   cc_uint16_t                           nodeCount        = 0;       // linked list node count

   CCAPP_DEBUG(DEB_F_PREFIX"Entering:  CCAPI_CallInfo_getConfParticipants", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));

   // get conference reference from the call info
   callConference = getCallConferenceRef(handle);
   if (callConference == NULL)
   {
      CCAPP_ERROR(DEB_F_PREFIX"Unable to get conference handle", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
      *count = 0;
      return;
   }

   nodeCount = SLL_LITE_NODE_COUNT(&(callConference->currentParticipantsList));
   CCAPP_DEBUG(DEB_F_PREFIX"SLL NODE COUNT = [%d]", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"), nodeCount);
   if (nodeCount <= 0)
   {
      *count = 0;
      return;
   }

   participant = (cc_call_conference_participant_ref_t)SLL_LITE_LINK_HEAD(&callConference->currentParticipantsList);
   while (participant != NULL)
   {
      if (participantIndex >= *count)
      {
          CCAPP_ERROR(DEB_F_PREFIX"Not Enough Room Provided To List All Participants.  Listed [%d] of [%d]", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"), *count, nodeCount);
          return;
      }

      // add this participant to our list of particpiants
      participantHandles[participantIndex] = (participant->callid);

      // step to the next stored participant in the list
      participant = (cc_call_conference_participant_ref_t)SLL_LITE_LINK_NEXT_NODE(participant);
      participantIndex++;
   }

   // sanity check
   if (participantIndex != nodeCount)
   {  // did not find the expected number of participants!
       CCAPP_ERROR(DEB_F_PREFIX"Detected mismatch between counted participants [%d] and SLL returned nodecount [%d]", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"),
                participantIndex, nodeCount);
       *count = 0;
       return;
   }

   // return number of participants
   *count = nodeCount;
   return;
}

/**
* Get Maximum Number of Conference Participants ( in case gui wants to show %full conference info )
* @param [in] handle - call handle
* @return maximum number of conference participants
*/
cc_uint16_t CCAPI_CallInfo_getConfParticipantMax (cc_callinfo_ref_t handle)
{  //
   cc_call_conference_ref_t callConference;    // conference reference (from call info)

   CCAPP_DEBUG(DEB_F_PREFIX"Entering:  CCAPI_CallInfo_getConfParticipantMax", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));

   // get conference reference from the call info
   callConference = getCallConferenceRef(handle);
   if (callConference == NULL)
   {
      // no conference reference available
      CCAPP_ERROR(DEB_F_PREFIX"Unable to get conference reference", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
      return (0);
   }

   // return the max
   return (callConference->participantMax);
}

/**
* Get Participant Name
* @param [in] handle - call info handle
* @param [in] participantHandle - specific handle for conference participant
* @return display name of the conference participant
*/
cc_string_t CCAPI_CallInfo_getConfParticipantName (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandle)
{
    cc_call_conference_participant_ref_t participant = getConferenceParticipantRef (handle, participantHandle);
    if (participant == NULL)
    {
        return strlib_empty();
    }

    return (participant->participantName);
}

/**
* Get Participant Number
* @param [in] handle - handle of call
* @param [in] participantHandle - handle of conference participant
* @return display number of the conference participant
*/
cc_string_t CCAPI_CallInfo_getConfParticipantNumber (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandle)
{
    cc_call_conference_participant_ref_t participant = getConferenceParticipantRef (handle, participantHandle);
    if (participant == NULL)
    {
        return strlib_empty();
    }

    return (participant->participantNumber);
}

/**
* Get Conference Participant Status
* @param [in] handle - call handle
* @param [in] participantHandle - handle of conference participant
* @return conference participant status
*/
cc_conf_participant_status_t CCAPI_CallInfo_getConfParticipantStatus (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandle)
{
    cc_call_conference_participant_ref_t participant = getConferenceParticipantRef (handle, participantHandle);
    if (participant == NULL)
    {
        return (CCAPI_CONFPARTICIPANT_UNKNOWN);
    }

    return (participant->participantStatus);
}

/**
* Get Participant Security
* @param [in] handle - call handle
* @param [in] participantHandle - handle of conference participant
* @return security setting of the specific conference participant
*/
cc_call_security_t CCAPI_CallInfo_getConfParticipantSecurity (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandle)
{
    cc_call_conference_participant_ref_t participant = getConferenceParticipantRef (handle, participantHandle);
    if (participant == NULL)
    {
        return (CC_SECURITY_NONE);
    }

    return (participant->participantSecurity);
}

/**
*/
cc_boolean CCAPI_CallInfo_isConfSelfParticipant (cc_callinfo_ref_t handle, cc_participant_ref_t participantHandle)
{
   cc_call_conference_ref_t callConference;    // conference reference (from call info)

   // get conference reference from the call info
   callConference = getCallConferenceRef(handle);
   if (callConference == NULL)
   {
      // error - log
      CCAPP_ERROR(DEB_F_PREFIX"Unable to get conference reference", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
      return (FALSE);
   }

   return (strcmp((callConference->myParticipantId), participantHandle) == 0);
}

/**
*/
cc_participant_ref_t CCAPI_CallInfo_getConfSelfParticipant (cc_callinfo_ref_t handle)
{
   cc_call_conference_ref_t callConference;    // conference reference (from call info)

   // get conference reference from the call info
   callConference = getCallConferenceRef(handle);
   if (callConference == NULL)
   {
      // unexpected error
      CCAPP_ERROR(DEB_F_PREFIX"Unable to get conference reference", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
      return strlib_empty();
   }

   return (callConference->myParticipantId);
}

// -----
/**
 * Get the call conference reference
 * @param [in] handle - call info handle
 * @return cc_call_conference_Info_t
 */
cc_call_conference_ref_t  getCallConferenceRef(cc_callinfo_ref_t handle)
{
  session_data_t *data = (session_data_t *)handle;

  if (!CCAPI_CallInfo_getIsConference(handle))
  {
      CCAPP_ERROR(DEB_F_PREFIX"Conference API Invoked, but Not In Conference Call", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
      return (NULL);
  };

  if (data == NULL)
  {
      return (NULL);
  }

  return (&data->call_conference);
}

// ------------------------------------------------------------------------------------------------------------------
// getConferenceParticipantRef:  returns participant ref (pointer) to a specific participant handle
// ------------------------------------------------------------------------------------------------------------------
cc_call_conference_participant_ref_t getConferenceParticipantRef(cc_callinfo_ref_t handle, cc_participant_ref_t participantHandle)
{
   cc_call_conference_ref_t              callConference;    // conference reference (from call info)
   cc_call_conference_participant_ref_t  participant;

   // get conference reference from the call info
   callConference = getCallConferenceRef(handle);
   if (callConference == NULL)
   {
      // no conference reference available
      CCAPP_ERROR(DEB_F_PREFIX"Unable to get conference reference", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
      return (NULL);
   }

   // see if participantHandle is legit...
   if (participantHandle == NULL)
   {
       CCAPP_DEBUG(DEB_F_PREFIX"Received query for null participant", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
       return (NULL);
   }

   if (SLL_LITE_NODE_COUNT(&(callConference->currentParticipantsList)) <= 0)
   {
      CCAPP_ERROR(DEB_F_PREFIX"Participant list node count is 0, returning NULL", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
      return (NULL);
   }

   participant = (cc_call_conference_participant_ref_t)SLL_LITE_LINK_HEAD(&callConference->currentParticipantsList);
   while (participant != NULL)
   {
      // see if we've found the participant we're looking for
      if (strcmp(participant->callid, participantHandle) == 0)
      {
         return (participant);
      }

      // no match so far, so look at the next item in the list...
      participant = (cc_call_conference_participant_ref_t)SLL_LITE_LINK_NEXT_NODE(participant);
   }

   CCAPP_ERROR(DEB_F_PREFIX"    Did Not Find participant!", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONF"));
   return (NULL);
}
