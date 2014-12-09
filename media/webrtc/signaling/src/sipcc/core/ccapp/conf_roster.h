/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __CONFROSTER_H__
#define __CONFROSTER_H__

#include "sll_lite.h"
#include "cpr_string.h"
#include "cc_constants.h"
#include "cpr_stdio.h"
#include "ccapi_conf_roster.h"

// structure for individual participant/user info
typedef struct cc_call_conferenceParticipant_Info_t_ {
    sll_lite_node_t              node;
    cc_participant_ref_t         callid;
    string_t                     participantName;
    string_t                     participantNumber;
    cc_conf_participant_status_t participantStatus;
    cc_call_security_t           participantSecurity;
    string_t                     endpointUri;
    cc_boolean                   canRemoveOtherParticipants;
} cc_call_conferenceParticipant_Info_t;

// reference to above structure
typedef struct cc_call_conferenceParticipant_Info_t_* cc_call_conference_participant_ref_t;

// main structure (one instance kept per conference (per call))
typedef struct cc_call_conference_Info_t_ {
    int32_t              participantMax;
    int32_t              participantCount;
    cc_participant_ref_t myParticipantId;
    sll_lite_list_t      currentParticipantsList;
} cc_call_conference_Info_t;

// reference to above structure
typedef struct cc_call_conference_Info_t_* cc_call_conference_ref_t;

void conf_roster_init_call_conference (cc_call_conference_Info_t *info);
cc_call_conference_ref_t             getCallConferenceRef(cc_callinfo_ref_t handle);
cc_call_conference_participant_ref_t getConferenceParticipantRef(cc_callinfo_ref_t handle, cc_participant_ref_t participantHandle);
void conf_roster_free_call_conference (cc_call_conference_Info_t *confInfo);
void conf_roster_copy_call_conferance (cc_call_conference_Info_t *dest, cc_call_conference_Info_t * src);

#endif



