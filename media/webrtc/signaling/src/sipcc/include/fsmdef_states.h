/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _FSMDEF_STATES_H_
#define _FSMDEF_STATES_H_

typedef enum {
    FSMDEF_S_MIN = -1,

    FSMDEF_S_IDLE,

    /* SIP States */
    FSMDEF_S_COLLECT_INFO,
    FSMDEF_S_CALL_SENT,
    FSMDEF_S_OUTGOING_PROCEEDING,
    FSMDEF_S_KPML_COLLECT_INFO,
    FSMDEF_S_OUTGOING_ALERTING,
    FSMDEF_S_INCOMING_ALERTING,
    FSMDEF_S_CONNECTING,
    FSMDEF_S_JOINING,
    FSMDEF_S_CONNECTED,
    FSMDEF_S_CONNECTED_MEDIA_PEND,
    FSMDEF_S_RELEASING,
    FSMDEF_S_HOLD_PENDING,
    FSMDEF_S_HOLDING,
    FSMDEF_S_RESUME_PENDING,
    FSMDEF_S_PRESERVED,

    /* WebRTC States */
    /* MUST be in the same order as PeerConnectionImpl::SignalingState */
    FSMDEF_S_STABLE,
    FSMDEF_S_HAVE_LOCAL_OFFER,
    FSMDEF_S_HAVE_REMOTE_OFFER,
    FSMDEF_S_HAVE_LOCAL_PRANSWER,
    FSMDEF_S_HAVE_REMOTE_PRANSWER,
    FSMDEF_S_CLOSED,

    FSMDEF_S_MAX
} fsmdef_states_t;

const char * fsmdef_state_name (int state);

#endif
