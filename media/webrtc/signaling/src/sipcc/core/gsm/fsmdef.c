/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits.h>
#include "CCProvider.h"
#include "cpr_types.h"
#include "cpr_stdlib.h"
#include "cpr_stdio.h"
#include "cpr_string.h"
#include "cpr_rand.h"
#include "cpr_timers.h"
#include "cpr_errno.h"
#include "phone.h"
#include "lsm.h"
#include "fsm.h"
#include "sm.h"
#include "ccapi.h"
#include "ccsip_cc.h"
#include "phone_debug.h"
#include "fim.h"
#include "config.h"
#include "sdp.h"
#include "ccsip_sdp.h" // Temporary include
#include "rtp_defs.h"
#include "debug.h"
#include "gsm_sdp.h"
#include "vcm.h"
#include "uiapi.h"
#include "gsm.h"
#include "phntask.h"
#include "prot_configmgr.h"
#include "sip_interface_regmgr.h"
#include "dialplanint.h"
#include "subapi.h"
#include "text_strings.h"
#include "platform_api.h"
#include "peer_connection_types.h"
#include "prlog.h"
#include "sessionHash.h"

extern void update_kpmlconfig(int kpmlVal);
extern boolean g_disable_mass_reg_debug_print;
void escalateDeescalate();

#define FSMDEF_NO_NUMBER    (NULL)
#define DIGIT_POUND         ('#')
#define FSMDEF_MAX_DCBS     (LSM_MAX_CALLS)
#define FSMDEF_CC_CALLER_ID ((cc_state_data_t *)(&(dcb->caller_id)))
#define RINGBACK_DELAY      90

// Minimum and maximum hold reversion timer in seconds
#define MIN_HOLD_REVERSION_INTERVAL_TIMER 10
#define MAX_HOLD_REVERSION_INTERVAL_TIMER 1200

fsmdef_dcb_t *fsmdef_dcbs;

static const char *fsmdef_state_names[] = {
    "IDLE",

    /* SIP states */
    "COLLECTING_INFO",
    "CALL_SENT",
    "OUTGOING_PROCEEDING",
    "KPML_COLLECTING_INFO",
    "OUTGOING_ALERTING",
    "INCOMING_ALERTING",
    "CONNECTING",
    "JOINING",
    "CONNECTED",
    "CONNECTED MEDIA PEND",
    "RELEASING",
    "HOLD_PENDING",
    "HOLDING",
    "RESUME_PENDING",
    "PRESERVED",

    /* WebRTC States */
    "STABLE",
    "HAVE_LOCAL_OFFER",
    "HAVE_REMOTE_OFFER",
    "HAVE_REMOTE_PRANSWER",
    "HAVE_LOCAL_PRANSWER",
    "CLOSED"
};


static sm_rcs_t fsmdef_ev_createoffer(sm_event_t *event);
static sm_rcs_t fsmdef_ev_createanswer(sm_event_t *event);
static sm_rcs_t fsmdef_ev_setlocaldesc(sm_event_t *event);
static sm_rcs_t fsmdef_ev_setremotedesc(sm_event_t *event);
static sm_rcs_t fsmdef_ev_setpeerconnection(sm_event_t *event);
static sm_rcs_t fsmdef_ev_addstream(sm_event_t *event);
static sm_rcs_t fsmdef_ev_removestream(sm_event_t *event);
static sm_rcs_t fsmdef_ev_addcandidate(sm_event_t *event);

static sm_rcs_t fsmdef_ev_default(sm_event_t *event);
static sm_rcs_t fsmdef_ev_default_feature_ack(sm_event_t *event);
static sm_rcs_t fsmdef_ev_idle_setup(sm_event_t *event);
static sm_rcs_t fsmdef_ev_idle_feature(sm_event_t *event);
static sm_rcs_t fsmdef_ev_idle_offhook(sm_event_t *event);
static sm_rcs_t fsmdef_ev_idle_dialstring(sm_event_t *event);
static sm_rcs_t fsmdef_ev_onhook(sm_event_t *event);
static sm_rcs_t fsmdef_ev_collectinginfo_release(sm_event_t *event);
static sm_rcs_t fsmdef_ev_collectinginfo_feature(sm_event_t *event);
static sm_rcs_t fsmdef_ev_offhook(sm_event_t *event);
static sm_rcs_t fsmdef_ev_digit_begin(sm_event_t *event);
static sm_rcs_t fsmdef_ev_dialstring(sm_event_t *event);
static sm_rcs_t fsmdef_ev_proceeding(sm_event_t *event);
static sm_rcs_t fsmdef_ev_callsent_release(sm_event_t *event);
static sm_rcs_t fsmdef_ev_callsent_feature(sm_event_t *event);
static sm_rcs_t fsmdef_ev_out_alerting(sm_event_t *event);
static sm_rcs_t fsmdef_ev_inalerting_feature(sm_event_t *event);
static sm_rcs_t fsmdef_ev_inalerting_offhook(sm_event_t *event);
static sm_rcs_t fsmdef_handle_inalerting_offhook_answer(sm_event_t *event);
static sm_rcs_t fsmdef_ev_connected(sm_event_t *event);
static sm_rcs_t fsmdef_ev_connected_line(sm_event_t *event);
static sm_rcs_t fsmdef_ev_connected_ack(sm_event_t *event);
static sm_rcs_t fsmdef_ev_connecting_feature(sm_event_t *event);
static sm_rcs_t fsmdef_ev_connected_feature(sm_event_t *event);
static sm_rcs_t fsmdef_ev_connected_media_pend_feature(sm_event_t *event);
static sm_rcs_t fsmdef_ev_connected_media_pend_feature_ack(sm_event_t *event);
static sm_rcs_t fsmdef_ev_release(sm_event_t *event);
static sm_rcs_t fsmdef_ev_release_complete(sm_event_t *event);
static sm_rcs_t fsmdef_ev_releasing_release(sm_event_t *event);
static sm_rcs_t fsmdef_ev_releasing_feature(sm_event_t *event);
static sm_rcs_t fsmdef_ev_releasing_onhook(sm_event_t *event);
static sm_rcs_t fsmdef_ev_hold_pending_feature(sm_event_t *event);
static sm_rcs_t fsmdef_ev_hold_pending_feature_ack(sm_event_t *event);
static sm_rcs_t fsmdef_ev_holding_release(sm_event_t *event);
static sm_rcs_t fsmdef_ev_holding_feature(sm_event_t *event);
static sm_rcs_t fsmdef_ev_holding_feature_ack(sm_event_t *event);
static sm_rcs_t fsmdef_ev_holding_onhook(sm_event_t *event);
static sm_rcs_t fsmdef_ev_holding_offhook(sm_event_t *event);
static sm_rcs_t fsmdef_ev_session_audit(sm_event_t *event);
static sm_rcs_t fsmdef_ev_resume_pending_feature(sm_event_t *event);
static sm_rcs_t fsmdef_ev_resume_pending_feature_ack(sm_event_t *event);
static sm_rcs_t fsmdef_ev_preserved_feature(sm_event_t *event);
static void fsmdef_ev_join(cc_feature_data_t *data);

static sm_rcs_t fsmdef_cfwd_clear_ccm(fsm_fcb_t *fcb);
static sm_rcs_t fsmdef_process_dialstring_for_callfwd(sm_event_t *event);
static sm_rcs_t fsmdef_process_cfwd_softkey_event(sm_event_t *event);
static sm_rcs_t fsmdef_cfwd_clear_ccm(fsm_fcb_t *fcb);
static sm_rcs_t fsmdef_ev_joining_connected_ack(sm_event_t *event);
static sm_rcs_t fsmdef_ev_joining_offhook(sm_event_t *event);

static void fsmdef_b2bjoin_invoke(fsmdef_dcb_t *dcb,
                                  cc_feature_data_t *join_data);
static void fsmdef_select_invoke(fsmdef_dcb_t *dcb,
                                 cc_feature_data_t *select_data);
static void fsmdef_handle_join_pending(fsmdef_dcb_t *dcb);
static void fsmdef_append_dialstring_to_feature_uri(fsmdef_dcb_t *dcb,
                                                    const char *dialstring);
static boolean fsmdef_is_feature_uri_configured(cc_features_t ftr_id);
static void fsmdef_set_call_info_cc_call_state(fsmdef_dcb_t *dcb,
                                               cc_states_t state,
                                               cc_causes_t cause);
static boolean fsmdef_extract_join_target(sm_event_t *event);
static void fsmdef_ev_notify_feature(cc_feature_t *msg, fsmdef_dcb_t *dcb);
static void fsmdef_notify_hook_event(fsm_fcb_t *fcb, cc_msgs_t msg,
                                     char *global_call_id,
                                     callid_t prim_call_id,
                                     cc_hold_resume_reason_e consult_reason,
                                     monitor_mode_t monitor_mode,
                                     cfwdall_mode_t cfwdall_mode);
static void fsmdef_update_callinfo_security_status(fsmdef_dcb_t *dcb,
                                     cc_feature_data_call_info_t *call_info);
static void fsmdef_update_calltype (fsm_fcb_t *fcb, cc_feature_t *msg);


/*
 * TODO <emannion> Update events for correct JSEP transitions
 *                 Instead of providing events for all states
 */
static sm_function_t fsmdef_function_table[FSMDEF_S_MAX][CC_MSG_MAX] =
{
/* FSMDEF_S_IDLE ------------------------------------------------------------ */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_idle_setup,       // New incoming
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_default,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_release_complete,
    /* CC_MSG_FEATURE          */ fsmdef_ev_idle_feature,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default_feature_ack,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_idle_offhook,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_default,
    /* CC_MSG_LINE             */ fsmdef_ev_idle_offhook,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_idle_dialstring,  // new outgoing
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_session_audit,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_setpeerconnection,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    },

/* FSMDEF_S_COLLECT_INFO ---------------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_collectinginfo_release,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_collectinginfo_feature,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default_feature_ack,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_default,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_digit_begin,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_dialstring,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_session_audit,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    },

/* FSMDEF_S_CALL_SENT ------------------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_proceeding,
    /* CC_MSG_ALERTING         */ fsmdef_ev_out_alerting,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_connected,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_callsent_release,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_callsent_feature,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default_feature_ack,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_offhook,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_session_audit,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    },

/* FSMDEF_S_OUTGOING_PROCEEDING --------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_out_alerting,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_connected,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_callsent_release,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_callsent_feature,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default_feature_ack,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_offhook,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_session_audit,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    },

/* FSMDEF_S_KPML_COLLECT_INFO ----------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_out_alerting,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_connected,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_callsent_release,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_collectinginfo_feature,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default_feature_ack,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_default,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_digit_begin,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_session_audit,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    },

/* FSMDEF_S_OUTGOING_ALERTING ----------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_out_alerting,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_connected,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_callsent_release,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_callsent_feature,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default_feature_ack,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_offhook,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_session_audit,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    },

/* FSMDEF_S_INCOMING_ALERTING ----------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_release,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_inalerting_feature,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default_feature_ack,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_inalerting_offhook,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_inalerting_offhook,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_session_audit,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    },

/* FSMDEF_S_CONNECTING ------------------------------------------------------ */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_connected_ack,
    /* CC_MSG_RELEASE          */ fsmdef_ev_release,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_connecting_feature,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default_feature_ack,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_offhook,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_connected_line,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_session_audit,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    },

/* FSMDEF_S_JOINING --------------------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_joining_connected_ack,
    /* CC_MSG_RELEASE          */ fsmdef_ev_release,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_connecting_feature,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default_feature_ack,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_joining_offhook,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_connected_line,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_session_audit,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    },

/* FSMDEF_S_CONNECTED ------------------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_release,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_connected_feature,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default_feature_ack,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_offhook,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_connected_line,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_session_audit,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    },

/* FSMDEF_S_CONNECTED_MEDIA_PEND  ------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_release,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_connected_media_pend_feature,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_connected_media_pend_feature_ack,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_offhook,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_connected_line,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_session_audit,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    },

/* FSMDEF_S_RELEASING ------------------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_releasing_release,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_release_complete,
    /* CC_MSG_FEATURE          */ fsmdef_ev_releasing_feature,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default_feature_ack,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_default,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_releasing_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_connected_line,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_session_audit,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    },

/* FSMDEF_S_HOLD_PENDING ---------------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_holding_release,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_hold_pending_feature,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_hold_pending_feature_ack,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_default,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_session_audit,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    },

/* FSMDEF_S_HOLDING --------------------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_holding_release,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_holding_feature,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_holding_feature_ack,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_holding_offhook,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_holding_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_session_audit,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    },

/* FSMDEF_S_RESUME_PENDING -------------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_release,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_resume_pending_feature,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_resume_pending_feature_ack,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_default,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_session_audit,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    },

/* FSMDEF_S_PRESERVED  ------------------------------------------------------ */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_release,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_preserved_feature,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_default,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_session_audit,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    },


/* FSMDEF_S_STABLE  --------------------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_default,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_default,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_default,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_default,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_createoffer,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_createanswer,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_setlocaldesc,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_setremotedesc,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_addstream,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_removestream,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_addcandidate
    },

/* FSMDEF_S_HAVE_LOCAL_OFFER  ----------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_default,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_default,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_default,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_default,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_createoffer,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_createanswer,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_setlocaldesc,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_setremotedesc,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default /* Reject lame-duck
                                                       candidates */
    },

/* FSMDEF_S_HAVE_REMOTE_OFFER  ---------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_default,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_default,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_default,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_default,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_createoffer,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_createanswer,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_setlocaldesc,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_setremotedesc,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_addstream,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_removestream,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_addcandidate
    },

/* FSMDEF_S_HAVE_LOCAL_PRANSWER  -------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_default,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_default,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_default,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_default,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_createoffer,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_createanswer,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_setlocaldesc,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default, /* Should not happen */
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_addcandidate
    },

/* FSMDEF_S_HAVE_REMOTE_PRANSWER  ------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_default,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_default,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_default,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_default,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_createoffer,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_createanswer,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default, /* Should not happen */
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_setremotedesc,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_addcandidate
    },

/* FSMDEF_S_CLOSED  --------------------------------------------------------- */
    {
    /* CC_MSG_SETUP            */ fsmdef_ev_default,
    /* CC_MSG_SETUP_ACK        */ fsmdef_ev_default,
    /* CC_MSG_PROCEEDING       */ fsmdef_ev_default,
    /* CC_MSG_ALERTING         */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED        */ fsmdef_ev_default,
    /* CC_MSG_CONNECTED_ACK    */ fsmdef_ev_default,
    /* CC_MSG_RELEASE          */ fsmdef_ev_default,
    /* CC_MSG_RELEASE_COMPLETE */ fsmdef_ev_default,
    /* CC_MSG_FEATURE          */ fsmdef_ev_default,
    /* CC_MSG_FEATURE_ACK      */ fsmdef_ev_default,
    /* CC_MSG_OFFHOOK          */ fsmdef_ev_default,
    /* CC_MSG_ONHOOK           */ fsmdef_ev_onhook,
    /* CC_MSG_LINE             */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_BEGIN      */ fsmdef_ev_default,
    /* CC_MSG_DIGIT_END        */ fsmdef_ev_default,
    /* CC_MSG_DIALSTRING       */ fsmdef_ev_default,
    /* CC_MSG_MWI              */ fsmdef_ev_default,
    /* CC_MSG_SESSION_AUDIT    */ fsmdef_ev_default,
    /* CC_MSG_CREATEOFFER      */ fsmdef_ev_default,
    /* CC_MSG_CREATEANSWER     */ fsmdef_ev_default,
    /* CC_MSG_SETLOCALDESC     */ fsmdef_ev_default,
    /* CC_MSG_SETREMOTEDESC    */ fsmdef_ev_default,
    /* CC_MSG_SETPEERCONNECTION */fsmdef_ev_default,
    /* CC_MSG_ADDSTREAM        */ fsmdef_ev_default,
    /* CC_MSG_REMOVESTREAM     */ fsmdef_ev_default,
    /* CC_MSG_ADDCANDIDATE     */ fsmdef_ev_default
    }
};

static sm_table_t fsmdef_sm_table;
sm_table_t *pfsmdef_sm_table = &fsmdef_sm_table;

/*--------------------------------------------------------------------------
 * Global data
 *--------------------------------------------------------------------------
 */
uint16_t g_numofselected_calls = 0;
boolean  g_b2bjoin_pending = FALSE;
callid_t g_b2bjoin_callid = CC_NO_CALL_ID;

static sdp_direction_e s_default_video_dir = SDP_DIRECTION_SENDRECV;
static sdp_direction_e s_session_video_dir = SDP_MAX_QOS_DIRECTIONS;


void set_default_video_pref(int pref) {
   s_default_video_dir = pref;
}

void set_next_sess_video_pref(int pref) {
   s_session_video_dir = pref;
}

const char *
fsmdef_state_name (int state)
{
    if ((state <= FSMDEF_S_MIN) || (state >= FSMDEF_S_MAX)) {
        return (get_debug_string(GSM_UNDEFINED));
    }

    return (fsmdef_state_names[state]);
}

/*
 * fsmdef_get_dcb_by_call_id
 *
 * return the dcb referenced by the given call_id
 */
fsmdef_dcb_t *
fsmdef_get_dcb_by_call_id (callid_t call_id)
{
    static const char fname[] = "fsmdef_get_dcb_by_call_id";
    fsmdef_dcb_t   *dcb;
    fsmdef_dcb_t   *dcb_found = NULL;

    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
        if (dcb->call_id == call_id) {
            dcb_found = dcb;
            break;
        }
    }

    if (dcb_found) {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_PTR),
                     dcb->call_id, dcb->line, fname, dcb_found);
    }

    return (dcb_found);
}


/*
 * fsmdef_check_if_chaperone_call_exist
 *
 * return the dcb referenced by the given call_id
 */
boolean
fsmdef_check_if_chaperone_call_exist (void)
{
    static const char fname[] = "fsmdef_check_if_chaperone_call_exist";
    fsmdef_dcb_t   *dcb;
    boolean result = FALSE;

    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
    	if(dcb->policy == CC_POLICY_CHAPERONE){
		result = TRUE;
		break;
	}
    }

    if (result) {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_PTR),
                     dcb->call_id, dcb->line, fname, dcb);
    }

    return result;
}


void fsmdef_get_rtp_stat (fsmdef_dcb_t *dcb , cc_kfact_t *kfactor)
{
    static const char fname[] ="fsmdef_get_rtp_stat";

    int      call_stats_flag;
    fsmdef_media_t *media;
    media = gsmsdp_find_audio_media(dcb);

    if (!media) {
        GSM_ERR_MSG(GSM_F_PREFIX"dcb media pointer invalid", fname);
        return;
    }

    memset(kfactor, 0, sizeof(cc_kfact_t));
    config_get_value(CFGID_CALL_STATS, &call_stats_flag, sizeof(call_stats_flag));

    if (call_stats_flag) {
        vcmGetRtpStats(media->cap_index, dcb->group_id,
                          media->refid,
                          lsm_get_ms_ui_call_handle(dcb->line, dcb->call_id, CC_NO_CALL_ID), &(kfactor->rxstats[0]), &(kfactor->txstats[0]));
    }
}

/**
 * The function sets or clears the local hold status to the given media or
 * for all of the media.
 *
 * @param[in]dcb          - pointer to fsmdef_dcb_t.
 * @param[in]media        - specify which media to use. The value of
 *                          NULL indicates for all media.
 * @param[in]set          - set local hold or clear local hold.
 *
 * @return  none
 *
 * @pre     (dcb not_eq NULL)
 */
static void
fsmdef_update_media_hold_status (fsmdef_dcb_t *dcb, fsmdef_media_t *media,
                                 boolean set)
{
    fsmdef_media_t *start_media, *end_media;

    if (media == NULL) {
        /* NULL value of the given media indicates for all media */
        start_media = GSMSDP_FIRST_MEDIA_ENTRY(dcb);
        end_media   = NULL; /* NULL means till the end of the list */
    } else {
        /* given media, uses the provided media */
        start_media = media;
        end_media   = media;
    }

    GSMSDP_FOR_MEDIA_LIST(media, start_media, end_media, dcb) {
        if (GSMSDP_MEDIA_ENABLED(media)) {
            if (set) {
                FSM_SET_FLAGS(media->hold, FSM_HOLD_LCL);
            } else {
                FSM_RESET_FLAGS(media->hold, FSM_HOLD_LCL);
            }
        }
    }
}

/**
 * The function checks to see whether all media streams are
 * in locally held or not.
 *
 * @param[in]dcb          - pointer to fsmdef_dcb_t.
 *
 * @return  TRUE  - all media streams are in local hold.
 *          FALSE - not all media streams are in local hold.
 *
 * @pre     (dcb not_eq NULL)
 */
static boolean
fsmdef_all_media_are_local_hold (fsmdef_dcb_t *dcb)
{
    fsmdef_media_t *media;
    /*
     * Check the local hold status of each media to see if the
     * media is already locally held or not.
     */
    GSMSDP_FOR_ALL_MEDIA(media, dcb) {
        if (!GSMSDP_MEDIA_ENABLED(media)) {
            continue;
        }
        if (!FSM_CHK_FLAGS(media->hold, FSM_HOLD_LCL)) {
            /* found one media that is not on hold */
            return (FALSE);
        }
    }
    /* all media streams are local held */
    return (TRUE);
}

/**
 * The function gets the numbmer of media in local hold.
 *
 * @param[in]dcb          - pointer to fsmdef_dcb_t.
 *
 * @return  uint16_t for the number of media in local hold.
 *
 * @pre     (dcb not_eq NULL)
 */
static unsigned int
fsmdef_num_media_in_local_hold (fsmdef_dcb_t *dcb)
{
    fsmdef_media_t *media;
    unsigned int num_local_hold = 0;

    /* Check the local hold status of the media(s) */
    GSMSDP_FOR_ALL_MEDIA(media, dcb) {
        if (!GSMSDP_MEDIA_ENABLED(media)) {
            continue;
        }
        if (FSM_CHK_FLAGS(media->hold, FSM_HOLD_LCL)) {
            num_local_hold++;
        }
    }
    return (num_local_hold);
}

/**
 * The function is a convenient function to set each local hold in
 * SDP for each media if it is marked as locally held.
 *
 * @param[in]dcb          - pointer to fsmdef_dcb_t.
 *
 * @return  None
 *
 * @pre     (dcb not_eq NULL)
 */
static void
fsmdef_set_per_media_local_hold_sdp (fsmdef_dcb_t *dcb)
{
    fsmdef_media_t *media;

    GSMSDP_FOR_ALL_MEDIA(media, dcb) {
        if (!GSMSDP_MEDIA_ENABLED(media)) {
            continue;
        }
        if (FSM_CHK_FLAGS(media->hold, FSM_HOLD_LCL)) {
            /* set local hold to this media entry */
            gsmsdp_set_local_hold_sdp(dcb, media);
        }
    }
}

/**
 * This function deallocates a constraints structure
 *
 * @param[in]constraints - pointer to cc_media_constraints_t
 * @return None
 */
void
fsmdef_free_constraints(cc_media_constraints_t *constraints) {
    int i;

    if (!constraints) {
       return;
    }

    for (i = 0; i < constraints->constraint_count; i++) {
        cpr_free(constraints->constraints[i]->name);
        cpr_free(constraints->constraints[i]->value);
    }
    cpr_free(constraints->constraints);
    cpr_free(constraints);
}

void
fsmdef_init_dcb (fsmdef_dcb_t *dcb, callid_t call_id,
                 fsmdef_call_types_t call_type,
                 string_t called_number, line_t line, fsm_fcb_t *fcb)
{
    string_t calling_name;
    int      blocking;
    char     name[MAX_LINE_NAME_SIZE];

    dcb->call_id = call_id;
    dcb->line = line;

    dcb->spoof_ringout_requested = FALSE;
    dcb->spoof_ringout_applied = FALSE;

    dcb->log_disp = CC_CALL_LOG_DISP_UNKNWN;

    fsmutil_init_groupid(dcb, call_id, call_type);

    /*
     * Fill in as much of the caller_id data as possible.
     * Different data is available based on the call_type.
     */
    switch (call_type) {
    case FSMDEF_CALL_TYPE_OUTGOING:
        config_get_value(CFGID_CALLERID_BLOCKING, &blocking, sizeof(blocking));
        if (line != 0) {
            sip_config_get_display_name(line, name, sizeof(name));
        }
        if (blocking & 1 || line == 0) {
            calling_name = SIP_HEADER_ANONYMOUS_STR;
        } else {
            calling_name = name;
        }
        dcb->caller_id.calling_name = strlib_update(dcb->caller_id.calling_name,
                                                    calling_name);
        dcb->caller_id.calling_number =
            strlib_update(dcb->caller_id.calling_number, name);

        /*
         * called_xxx data will be set when the fsmdef receives the dialstring.
         */
        dcb->caller_id.called_name   = strlib_empty();
        dcb->caller_id.called_number = strlib_empty();
        dcb->caller_id.orig_rpid_number = strlib_empty();

        dcb->inbound = FALSE;

        break;

    case FSMDEF_CALL_TYPE_INCOMING:
    case FSMDEF_CALL_TYPE_FORWARD:
        /*
         * calling_xxx data will be set when the fsmdef receives the setup.
         */
        dcb->caller_id.calling_name   = strlib_empty();
        dcb->caller_id.calling_number = strlib_empty();

        dcb->caller_id.last_redirect_name = strlib_empty();
        dcb->caller_id.last_redirect_number = strlib_empty();
        dcb->caller_id.orig_called_name = strlib_empty();
        dcb->caller_id.orig_called_number = strlib_empty();
        dcb->caller_id.orig_rpid_number = strlib_empty();

        sip_config_get_display_name(line, name, sizeof(name));
        dcb->caller_id.called_name =
            strlib_update(dcb->caller_id.called_name, name);
        dcb->caller_id.called_number =
            strlib_update(dcb->caller_id.called_number, called_number);

        dcb->inbound = TRUE;

        break;

    case FSMDEF_CALL_TYPE_NONE:
        dcb->caller_id.calling_name   = strlib_empty();
        dcb->caller_id.calling_number = strlib_empty();
        dcb->caller_id.called_name    = strlib_empty();
        dcb->caller_id.called_number  = strlib_empty();
        dcb->caller_id.alt_calling_number = strlib_empty();
        dcb->caller_id.last_redirect_name   = strlib_empty();
        dcb->caller_id.last_redirect_number = strlib_empty();
        dcb->caller_id.orig_called_name     = strlib_empty();
        dcb->caller_id.orig_called_number   = strlib_empty();
        dcb->caller_id.orig_rpid_number = strlib_empty();
        dcb->inbound = FALSE;

        break;
    default:
        break;
    }                           /* switch (call_type) { */

    dcb->caller_id.display_calling_number = TRUE;
    dcb->caller_id.display_called_number = TRUE;

    /* Initially, assume ui update is required for a new call. */
    dcb->ui_update_required = TRUE;
    dcb->placed_call_update_required = TRUE;

    dcb->is_conf_call = FALSE; /*Initially, set to conf call false*/

    dcb->digit_cnt = 0;

    dcb->call_type = call_type;
    dcb->orientation = CC_ORIENTATION_NONE;

    dcb->caller_id.call_instance_id = 0;

    dcb->msgs_sent = FSMDEF_MSG_NONE;
    dcb->msgs_rcvd = FSMDEF_MSG_NONE;

    dcb->send_release = FALSE;

    dcb->inband = FALSE;
    dcb->inband_received = FALSE;
    dcb->outofband = 0;

    dcb->remote_sdp_present = FALSE;
    dcb->remote_sdp_in_ack = FALSE;
    dcb->local_sdp_complete = FALSE;

    dcb->sdp = NULL;
    dcb->src_sdp_version = 0;

    dcb->dial_mode = DIAL_MODE_NUMERIC;

    dcb->hold_reason = CC_REASON_NONE;

    dcb->pd_updated = FALSE;

    dcb->alerting_tone = VCM_NO_TONE;
    dcb->tone_direction = VCM_PLAY_TONE_TO_EAR;

    dcb->alert_info = ALERTING_NONE;

    dcb->dialplan_tone = FALSE;

    dcb->active_tone = VCM_NO_TONE;

    dcb->monrec_tone_action = FSMDEF_MRTONE_NO_ACTION;
    dcb->monitor_tone_direction = VCM_PLAY_TONE_TO_EAR;
    dcb->recorder_tone_direction = VCM_PLAY_TONE_TO_EAR;

    dcb->play_tone_action = FSMDEF_PLAYTONE_NO_ACTION;

    dcb->fcb = fcb;

    dcb->early_error_release = FALSE;

    dcb->active_feature = CC_FEATURE_NONE;

    /* Release transient timers if any have been allocated */
    if (dcb->err_onhook_tmr) {
        (void) cprDestroyTimer(dcb->err_onhook_tmr);
        dcb->err_onhook_tmr = NULL;
    }
    if (dcb->req_pending_tmr) {
        (void) cprDestroyTimer(dcb->req_pending_tmr);
        dcb->req_pending_tmr = NULL;
    }

    FSM_SET_SECURITY_STATUS(dcb, CC_SECURITY_UNKNOWN);
    FSM_SET_POLICY(dcb, CC_POLICY_UNKNOWN);
    dcb->session = PRIMARY;

    dcb->dsp_out_of_resources = FALSE;

    if (dcb->selected) {
        g_numofselected_calls--;
    }

    dcb->selected = FALSE;

    dcb->select_pending = FALSE;
    dcb->call_not_counted_in_mnc_bt = FALSE;
    if (g_disable_mass_reg_debug_print == FALSE) {
        FSM_DEBUG_SM(DEB_L_C_F_PREFIX"call_not_counted_in_mnc_bt = FALSE",
            DEB_L_C_F_PREFIX_ARGS(FSM, line, call_id, "fsmdef_init_dcb"));
    }

    /* clear all bit flags */
    dcb->flags = 0;
    dcb->onhook_received = FALSE;

    dcb->cur_video_avail = SDP_DIRECTION_INACTIVE;

    if ( s_session_video_dir != SDP_MAX_QOS_DIRECTIONS &&
         call_type == FSMDEF_CALL_TYPE_OUTGOING ) {
        dcb->video_pref = s_session_video_dir;
        s_session_video_dir = SDP_MAX_QOS_DIRECTIONS;
    } else {
        dcb->video_pref = s_default_video_dir;
    }

    gsmsdp_init_media_list(dcb);

    dcb->join_call_id = CC_NO_CALL_ID;
    dcb->callref = 0;

    dcb->ice_ufrag = NULL;
    dcb->ice_pwd = NULL;
    dcb->ice_default_candidate_addr[0] = '\0';

    dcb->digest_alg[0] = '\0';
    dcb->digest[0] = '\0';
}


static void
fsmdef_free_dcb (fsmdef_dcb_t *dcb)
{
    if (dcb == NULL) {
        return;
    }

    strlib_free(dcb->caller_id.calling_name);
    strlib_free(dcb->caller_id.calling_number);
    strlib_free(dcb->caller_id.alt_calling_number);
    strlib_free(dcb->caller_id.called_name);
    strlib_free(dcb->caller_id.called_number);

    strlib_free(dcb->caller_id.last_redirect_name);
    strlib_free(dcb->caller_id.last_redirect_number);
    strlib_free(dcb->caller_id.orig_called_name);
    strlib_free(dcb->caller_id.orig_called_number);
    strlib_free(dcb->caller_id.orig_rpid_number);

    /* Cancel any existing error onhook timer */
    if (dcb->err_onhook_tmr) {
        (void) cprCancelTimer(dcb->err_onhook_tmr);
        (void) cprDestroyTimer(dcb->err_onhook_tmr);
        dcb->err_onhook_tmr = NULL;
    }

    /* Cancel any existing request pending timer */
    if (dcb->req_pending_tmr) {
        (void) cprCancelTimer(dcb->req_pending_tmr);
        (void) cprDestroyTimer(dcb->req_pending_tmr);
        dcb->req_pending_tmr = NULL;
    }

    /* Cancel any existing ringback delay timer */
    if (dcb->ringback_delay_tmr) {
        (void) cprCancelTimer(dcb->ringback_delay_tmr);
    }

    // Free the call instance id
    if (dcb->caller_id.call_instance_id != 0) {
        fsmutil_free_ci_id(dcb->caller_id.call_instance_id, dcb->line);
    }

    /* clean media list */
    gsmsdp_clean_media_list(dcb);

    gsmsdp_free(dcb);

    fsmdef_init_dcb(dcb, CC_NO_CALL_ID, FSMDEF_CALL_TYPE_NONE, NULL,
                    LSM_NO_LINE, NULL);

    /*
     * Cache random numbers for SRTP keys
     */
    gsmsdp_cache_crypto_keys();

}

void
fsmdef_free_cb (fim_icb_t *icb, callid_t call_id)
{
    fsm_fcb_t      *fcb = NULL;
    fsmdef_dcb_t   *dcb = NULL;

    if (call_id != CC_NO_CALL_ID) {
        dcb = fsmdef_get_dcb_by_call_id(call_id);
        if (dcb != NULL) {
            fcb = dcb->fcb;
            fsmdef_init_dcb(dcb, CC_NO_CALL_ID, FSMDEF_CALL_TYPE_NONE,
                            NULL, LSM_NO_LINE, NULL);
            /* fsmdef_init_dcb(...,NULL) will always set the fcb ptr to NULL,
               so if fsmdef_free_cb were called on that we'd have fcb==NULL here */
            if (fcb != NULL) {
              fsm_init_fcb(fcb, CC_NO_CALL_ID, FSMDEF_NO_DCB, FSM_TYPE_NONE);
            }
        } else {

            fcb = fsm_get_fcb_by_call_id_and_type(call_id, FSM_TYPE_DEF);
            if (fcb != NULL) {
                fsm_init_fcb(fcb, CC_NO_CALL_ID, FSMDEF_NO_DCB, FSM_TYPE_NONE);
            }
        }
    }
}


/*
 *  ROUTINE:     fsmdef_get_new_dcb
 *
 *  DESCRIPTION: return a new dcb initialized with the given data
 *
 *  PARAMETERS:
 *      call_id:    call_id
 *
 *  RETURNS:
 *      dcb: the new dcb
 *
 *  NOTES:       None
 */
fsmdef_dcb_t *
fsmdef_get_new_dcb (callid_t call_id)
{
    static const char fname[] = "fsmdef_get_new_dcb";
    fsmdef_dcb_t *dcb = NULL;

    /*
     * Get a free dcb.
     */
    if ((dcb = fsmdef_get_dcb_by_call_id(CC_NO_CALL_ID)) == NULL) {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG1), call_id, 0, fname,
                     "no dcbs available");

        return (NULL);
    }

    dcb->call_id = call_id;

    FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_PTR),
                 dcb->call_id, dcb->line, fname, dcb);

    return (dcb);
}


/**
 *
 * Returns dcb related to connected call.
 *
 * @param none
 *
 * @return  dcb of connected call.
 *
 * @pre     none
 */

fsmdef_dcb_t *
fsmdef_get_connected_call (void)
{
    fsmdef_dcb_t *dcb;
    fsm_fcb_t    *fcb;

    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
        if (dcb->call_id != CC_NO_CALL_ID) {
            fcb = dcb->fcb;
            if ((fcb != NULL) && (fcb->state == FSMDEF_S_RESUME_PENDING ||
                                fcb->state == FSMDEF_S_CONNECTED ||
                                fcb->state == FSMDEF_S_CONNECTED_MEDIA_PEND)) {

                return (dcb);
            }
        }
    }

    return (NULL);
}

/**
 *
 * Returns dcb related to alerting out call.
 *
 * @param none
 *
 * @return  dcb of outgoing alerting call.
 *
 * @pre     none
 */
static fsmdef_dcb_t *
fsmdef_get_alertingout_call (void)
{
    fsmdef_dcb_t *dcb;
    fsm_fcb_t    *fcb;

    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
        if (dcb->call_id != CC_NO_CALL_ID) {
            fcb = dcb->fcb;
            if ((fcb != NULL) && (fcb->state == FSMDEF_S_OUTGOING_ALERTING)) {
                return (dcb);
            }
        }
    }

    return (NULL);
}

/*
 * fsmdef_get_active_call_cnt
 *
 * Return the count of active calls aside from the
 * callid passed in.
 */
int
fsmdef_get_active_call_cnt (callid_t callId)
{
    fsmdef_dcb_t *dcb;
    int           cnt = 0;

    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
        if ((dcb->call_id != CC_NO_CALL_ID) && (dcb->call_id != callId)) {
            cnt++;
        }
    }

    return (cnt);
}


/*
 * return the dcbs and count of calls that are ringing and
 * are in error state not including the given call_id
 *
 * @param pointer to dcbs array
 * @param call_id that should be ignored from search
 *
 * @return  int number of ringing or error state calls
 *
 * @pre     none
*/
static int
fsmdef_get_ringing_n_error_call_dcbs (fsmdef_dcb_t **dcbs, callid_t ignore_call_id)
{
    fsmdef_dcb_t *dcb;
    int           cnt = 0;

    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
        if (dcb->spoof_ringout_applied ||
            ((dcb->call_id != CC_NO_CALL_ID) &&
             (dcb->call_id != ignore_call_id) &&
             (dcb->fcb && ((dcb->fcb->state <= FSMDEF_S_CONNECTING) ||
             (dcb->fcb->state == FSMDEF_S_RELEASING))))) {
            dcbs[cnt++] = dcb;
        }
    }

    return (cnt);
}


int
fsmdef_get_dcbs_in_held_state (fsmdef_dcb_t **dcbs, callid_t ignore_call_id)
{
    fsmdef_dcb_t *dcb;
    int           cnt = 0;

    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
        if ((dcb->call_id != CC_NO_CALL_ID) &&
            (dcb->call_id != ignore_call_id) &&
            (dcb->fcb && (dcb->fcb->state == FSMDEF_S_HOLDING ||
             dcb->fcb->state == FSMDEF_S_HOLD_PENDING))) {
            dcbs[cnt++] = dcb;
        }
    }

    return (cnt);
}

void fsmdef_call_cc_state_dialing (fsmdef_dcb_t *dcb, boolean suppress)
{
  cc_state_data_dialing_t data;

  if ( dcb->caller_id.called_number[0] == '\0' ) {
     data.play_dt = TRUE;
  } else {
     data.play_dt = FALSE;
  }

  data.suppress_stutter = suppress;

  cc_call_state(dcb->call_id, dcb->line, CC_STATE_DIALING,
                          (cc_state_data_t *)(&data));
}

/*
 * fsmdef_get_other_dcb_by_line
 *
 * return the dcb of the call that is active on a given line,
 * not including the given call_id
 */
fsmdef_dcb_t *
fsmdef_get_other_dcb_by_line (callid_t call_id, line_t line)
{
    fsmdef_dcb_t *dcb;
    fsmdef_dcb_t *dcb_found = NULL;

    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
        if ((dcb->call_id != CC_NO_CALL_ID) &&
            (dcb->line == line) && (dcb->call_id != call_id)) {
            dcb_found = dcb;
        }
    }

    return (dcb_found);
}

/*
 * fsmdef_are_there_selected_calls_onotherline
 *
 * @param line - line number
 *
 * loop thru the dcbs and check if there are selected calls
 * on a line other than this one
 *
 * @return TRUE, there are
 *         FALSE there are not
 */
boolean fsmdef_are_there_selected_calls_onotherline (line_t line)
{
    fsmdef_dcb_t *dcb;

    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
        if (dcb->selected) {
            if (dcb->line != line) {
                return (TRUE);
            }
        }
    }
    return (FALSE);
}

/*
 * fsmdef_are_join_calls_on_same_line
 *
 * @param line - line number
 *
 * loop thru the dcbs and check if the line passed is the
 * same as where the initial join started
 *
 * @return TRUE, they are on the same line
 *         FALSE not on the same line
 */
boolean fsmdef_are_join_calls_on_same_line (line_t line)
{
    fsmdef_dcb_t *dcb;

    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
        if (dcb->call_id == g_b2bjoin_callid) {
            if (dcb->line != line) {
                return (FALSE);
            } else {
                return (TRUE);
            }
        }
    }
    return (FALSE);
}

/**
 *
 * Handles media capability update feature event from platform when
 * media stream capability changes.
 *
 * @param[in]msg - pointer to the cc_feature_t.
 *
 * @return  None.
 *
 * @pre     (msg not_eq NULL)
 */
void
fsmdef_update_media_cap_feature_event (cc_feature_t *msg)
{
    static const char fname[] = "fsmdef_update_media_cap_feature_event";
    fsmdef_dcb_t      *dcb;
    fsm_fcb_t         *fcb;

    FSM_DEBUG_SM(DEB_L_C_F_PREFIX"", DEB_L_C_F_PREFIX_ARGS(FSM, msg->line, msg->call_id, fname));

    /*
     * Find the connected call to send the media capability update
     * event to. There can be more than one call chains in
     * connected state for an example, a call that participates in
     * local conference, barged, monitored. All calls that
     * are active are sent the update event. Each call leg will handle
     * the event.
     *
     */
    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
        if (dcb->call_id != CC_NO_CALL_ID) {
            fcb = dcb->fcb;
            if ((fcb != NULL) && (fcb->state == FSMDEF_S_RESUME_PENDING ||
                                fcb->state == FSMDEF_S_CONNECTED)) {
                cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id,
                               dcb->line, CC_FEATURE_UPD_MEDIA_CAP, NULL);

            }
        }
    }
}

/**
 *
 * Function to find and hold any connected call.
 *
 * @param call_id        call id of the call
 * @param wait     flag to indicate if the caller has to wait ton invoke the event
 * @param src_id   source id of the caller where to post hold or endcall event.
 *
 * @return  none
 *
 * @pre     (wait == FALSE)
 */

static void
fsmdef_find_and_hold_connected_call (callid_t call_id, boolean *wait,
                                     cc_srcs_t src_id)
{
    fsmdef_dcb_t     *con_dcb;
    fsmcnf_ccb_t     *ccb;
    fsmcnf_ccb_t     *con_ccb;
    fsmxfr_xcb_t     *xcb;
    cc_feature_data_t data;
    callid_t          other_call_id;
    callid_t          other_call_id2;

    *wait = FALSE;

    /*
     * Place the connected call, if there is one, on hold,
     * but there are some restrictions to ignore the hold request:
     * 1. the connected call must be different than the one requesting the hold,
     * 2. the connected call is involved with this call in a conference and
     *    the conference is active.
     * NOTE: for case 2, why do we not send a hold for each of the calls
     *       involved in the conference? Because, the fsmcnf will generate the
     *       second hold for the other leg of the conference when it receives
     *       this hold.
     * 3. the connected call is in a conference with another call that is
     *    involved with a transfer. This is the case when two calls are in a
     *    conference and a bridge decides to transfer one leg of the bridge
     *    using REFER. I.E., cnf between A-B and A-C. B initiates transfer to D.
     *    B holds A-B, sends REFER to A, A holds A-B, initiates A-D, D answers,
     *    A hangs up A-B and connects conference. So, when A initiates A-D,
     *    we do not want to hold the active bridge between A-C.
     */
    con_dcb = fsmdef_get_connected_call();
    if ((con_dcb != NULL) && ((con_dcb->call_id != call_id) ||
                              (con_dcb->spoof_ringout_applied == FALSE))) {
        ccb     = fsmcnf_get_ccb_by_call_id(call_id);
        con_ccb = fsmcnf_get_ccb_by_call_id(con_dcb->call_id);


        if ((ccb == NULL) || (con_ccb == NULL) ||
            ((ccb == con_ccb) && (ccb->active != TRUE)) || (ccb != con_ccb)) {
            other_call_id = fsmcnf_get_other_call_id(con_ccb, con_dcb->call_id);
            xcb = fsmxfr_get_xcb_by_call_id(other_call_id);

            other_call_id2 = fsmxfr_get_other_call_id(xcb, other_call_id);

            if (call_id != other_call_id2) {
                *wait = TRUE;

                data.hold.call_info.type = CC_FEAT_HOLD;
                data.hold.call_info.data.hold_resume_reason =
                    CC_REASON_INTERNAL;
                data.hold.msg_body.num_parts = 0;
                data.hold.call_info.data.call_info_feat_data.swap = FALSE;
                data.hold.call_info.data.call_info_feat_data.protect = FALSE;
                cc_int_feature(src_id, CC_SRC_GSM, con_dcb->call_id,
                               con_dcb->line, CC_FEATURE_HOLD, &data);
            }
        }
    }
}

/*
 * Function  post a event to end the call which are in ringing
 * reorder or busy and connecting state. This would indicate call function to
 * wait on the existing event.
 *
 * @param call_id that should be ignored from search
 * @param pointer to boolean to indicate if the caller has to wait
 *
 * @return  none
 *
 * @pre     none
*/
static void
fsmdef_find_and_handle_ring_connecting_releasing_calls (callid_t call_id, boolean *wait)
{
    int               i;
    int               act_dcb_cnt;
    fsmdef_dcb_t     *act_dcb;
    fsmdef_dcb_t     *act_dcbs[LSM_MAX_CALLS];
    cc_feature_data_t data;

    *wait = FALSE;

    data.endcall.cause         = CC_CAUSE_NORMAL;
    data.endcall.dialstring[0] = '\0';

    act_dcb_cnt = fsmdef_get_ringing_n_error_call_dcbs(act_dcbs, call_id);
    for (i = 0; i < act_dcb_cnt; i++) {
        act_dcb = act_dcbs[i];
        /*
         * Clear all the outgoing ringing lines (if there are any).
         */
        if (act_dcb->call_type == FSMDEF_CALL_TYPE_OUTGOING ||
            act_dcb->spoof_ringout_applied) {
            *wait = TRUE;
            cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, act_dcb->call_id,
                           act_dcb->line, CC_FEATURE_END_CALL, &data);

        }
        else if (act_dcb->fcb->state == FSMDEF_S_CONNECTING) {
            /* If the call is in connecting state, then wait till SIP
             * response to get the call in connected state.
             * The call can be put on hold only when call is connected.
             */
            *wait = TRUE;
        }
    }
}

void
fsmdef_end_call (fsmdef_dcb_t *dcb, cc_causes_t cause)
{
    cc_feature_data_t data;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
    data.endcall.cause         = cause;
    data.endcall.dialstring[0] = '\0';

    cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id, dcb->line,
                   CC_FEATURE_END_CALL, &data);
}

/*
 * fsmdef_clear_preserved_calls
 *
 * Release any calls in the preserved state
 */
static void
fsmdef_clear_preserved_calls (boolean *wait)
{
    fsmdef_dcb_t *dcb;

    *wait = FALSE;
    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
        if ((dcb->call_id != CC_NO_CALL_ID) &&
            (dcb->fcb->state == FSMDEF_S_PRESERVED)) {
            *wait = TRUE;
            fsmdef_end_call(dcb, CC_CAUSE_NORMAL);
        }
    }
}

/**
 *
 * Checks to see if actions are required for existing calls before the new
 * call can be activated.
 *
 * 1. Place the currently active connected call on hold.
 * 2. Clear any ringing calls.
 * 3. Release any call in the preserved state.
 *
 * @param is_newcall  To indicate if this is a new call
 * @param src_id     source of the caller
 * @param call_id    call-id
 * @param line       line number
 * @param feature    feature which is waiting on hodling call.
 * @param feature data
 *
 * @return  TRUE if actions require delay before the new call may begin
 *          FALSE if no actions are required an the new call may begin
 *
 * @pre     (wait, wait2, wait3 all FALSE)
 */
static boolean
fsmdef_wait_to_start_new_call (boolean is_newcall, cc_srcs_t src_id, callid_t call_id,
                               line_t line, cc_features_t feature,
                               cc_feature_data_t *data)
{
    boolean wait  = FALSE;
    boolean wait2 = FALSE;
    boolean wait3 = FALSE;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    fsmdef_find_and_hold_connected_call(call_id, &wait, src_id);

    fsmdef_find_and_handle_ring_connecting_releasing_calls(call_id, &wait2);

    fsmdef_clear_preserved_calls(&wait3);

    /*
     * Requeue the message because we need to wait for call actions to complete
     */
    if ((wait) || (wait2) || (wait3)) {
        cc_int_feature(src_id, CC_SRC_GSM, call_id, line, feature, data);
    }

    return (wait | wait2 | wait3);
}

static cc_causes_t
fsmdef_get_cause (boolean data_valid, cc_feature_data_t *data)
{
    cc_causes_t cause;

    if (data_valid) {
        cause = data->endcall.cause;
    } else {
        cause = CC_CAUSE_NORMAL;
    }

    return (cause);
}


/**
 * common function to release a call given cause code
 * i.e onhooks the given call.
 *
 * @param[in] fcb     The pointer to the fsm_fcb_t structure of this
 *                    call chain.
 * @param[in] cause   release cause code.
 *
 * @param[in] send_release When set to TRUE the function sends release
 *                    and waits for release complete.
 *                         When set to FALSE the function cleans up
 *                    dcb and release fcb.
 *
 * @pre               (fcb not_eq NULL)
 *
 * @return            sm_rcs_t indicates whether the execution of
 *                    next statmachine to end (SM_RC_END) or clean up
 *                    (SM_RC_CLEANUP)
 *
 * @Usage Note:       The function uses send_release flag to
 *                    as an indicator for sending release out or not.
 *                    If release is sent, the function transitions fsmdef's
 *                    state to FSMDEF_S_RELEASING and
 *                    return the SM_RC_END to waite for release complete.
 *
 *                    Otherwise, it cleans up dcb and releases fcb and
 *                    return SM_RC_CLEANUP to the caller.
 *
 *                    If the SM_RC_CLEANUP is returned, the caller should
 *                    terminate any access or perform any operation
 *                    afterward.
 */
sm_rcs_t
fsmdef_release (fsm_fcb_t *fcb, cc_causes_t cause, boolean send_release)
{
    fsmdef_dcb_t   *dcb = fcb->dcb;
    cc_state_data_t state_data;
    cc_kfact_t      kfactor;
    fsmdef_media_t *media;
    char tmp_str[STATUS_LINE_MAX_LEN];
    int             sdpmode = 0;

    if (!dcb) {
      /* Already been released */
      return SM_RC_CLEANUP;
    }

    FSM_DEBUG_SM(DEB_L_C_F_PREFIX"Entered. cause= %s",
		DEB_L_C_F_PREFIX_ARGS(FSM, dcb->line, dcb->call_id, __FUNCTION__), cc_cause_name(cause));

    if (g_dock_undock_event != MEDIA_INTERFACE_UPDATE_NOT_REQUIRED) {
        ui_update_media_interface_change(dcb->line, dcb->call_id, MEDIA_INTERFACE_UPDATE_FAIL);
    }
    memset(&kfactor, 0, sizeof(cc_kfact_t));
    /* Cancel any existing autoanswer timer */
    (void) cprCancelTimer(dcb->autoAnswerTimer);


    /*
     * Let Dialog Manager know that there is ONHOOK event
     */
    fsmdef_notify_hook_event(fcb, CC_MSG_ONHOOK, NULL, CC_NO_CALL_ID,
                             CC_REASON_NONE, CC_MONITOR_NONE,CFWDALL_NONE);

    media = gsmsdp_find_audio_media(dcb);
    if ((media) && (media->direction != SDP_DIRECTION_INACTIVE)) {
        fsmdef_get_rtp_stat(dcb, &kfactor);
    }

    if ( cause == CC_SIP_CAUSE_ANSWERED_ELSEWHERE ) {
        ui_log_disposition(dcb->call_id, CC_CALL_LOG_DISP_IGNORE );
    }

    if ( cause == CC_CAUSE_RESP_TIMEOUT) {
        if ((platGetPhraseText(STR_INDEX_RESP_TIMEOUT,
                                (char *) tmp_str,
                                STATUS_LINE_MAX_LEN - 1)) == CPR_SUCCESS) {
            lsm_ui_display_status(tmp_str, dcb->line, dcb->call_id);
        }
    }

    if (send_release) {
        cc_int_release(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id, dcb->line,
                       cause, NULL, &kfactor);
        /*
         * Wait around for the release_complete.
         */
        fsm_change_state(fcb, __LINE__, FSMDEF_S_RELEASING);

        /*
         * Only move the UI if we changed the UI's state when the call was
         * received.
         */
        if ((dcb->line != LSM_NO_LINE) || (cause != CC_CAUSE_BUSY)) {
            state_data.onhook.caller_id = dcb->caller_id;
            state_data.onhook.local = FALSE;
            state_data.onhook.cause     = CC_CAUSE_NORMAL;
            cc_call_state(dcb->call_id, dcb->line, CC_STATE_ONHOOK,
                          &state_data);
        }
        return (SM_RC_END);
    } else {
        /*
         * Only move the UI if we changed the UI's state when the call was
         * initiated.
         */
        if ((dcb->line != LSM_NO_LINE) || (cause != CC_CAUSE_BUSY)) {
            state_data.onhook.caller_id = dcb->caller_id;
            state_data.onhook.local     = FALSE;
            state_data.onhook.cause     = CC_CAUSE_NORMAL;
            cc_call_state(dcb->call_id, dcb->line, CC_STATE_ONHOOK,
                          &state_data);
        }

        /*
         * Only send a release complete to the remote end if they are waiting
         * for it. This is the case when we have sent a proceeding or we
         * have received a release.
         */
        if (FSM_CHK_FLAGS(dcb->msgs_sent, FSMDEF_MSG_PROCEEDING) ||
            FSM_CHK_FLAGS(dcb->msgs_rcvd, FSMDEF_MSG_RELEASE)) {
            cc_int_release_complete(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id,
                                    dcb->line, cause, &kfactor);
        }

        config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));
        fsm_change_state(fcb, __LINE__,
                         sdpmode ? FSMDEF_S_CLOSED : FSMDEF_S_IDLE);

        fsmdef_free_dcb(dcb);
        fsm_release(fcb, __LINE__, cause);
        /*
         * fsmdef has been released, indiate cleanup FSM chain.
         */
        return (SM_RC_CLEANUP);
    }
}

/*
 * fsmdef_convert_esc_plus
 *
 * replaces an escaped "+" (%2B) with a real "+"
 */
static void
fsmdef_convert_esc_plus (const char *src_number)
{
    int   i, len;
    char *number;

    len = strlen(src_number) - 2;
    number = (char *) src_number;
    number[0] = '+';
    for (i = 1; i < len; i++) {
        number[i] = number[i + 2];
    }
    number[i] = '\0';
}

static boolean
fsmdef_compare_caller_id_string (string_t dest, string_t src)
{
    if ((dest == NULL) && (src == NULL)) {
        /*
         * Strings are same.
         */
        return (FALSE);
    }

    if ((dest == NULL) || (src == NULL)) {
        /*
         * Strings differ.
         */
        return (TRUE);
    }

    if (strncmp(dest, src, FSMDEF_MAX_CALLER_ID_LEN) != 0) {
        /*
         * Strings differ.
         */
        return (TRUE);
    }

    /*
     * Strings are same.
     */
    return (FALSE);
}

static boolean
fsmdef_compare_caller_id (cc_caller_id_t *dest_caller_id,
                          cc_caller_id_t *src_caller_id)
{
    if (fsmdef_compare_caller_id_string(dest_caller_id->calling_name,
                                        src_caller_id->calling_name)) {
        return (TRUE);
    }

    if (fsmdef_compare_caller_id_string(dest_caller_id->calling_number,
                                        src_caller_id->calling_number)) {
        return (TRUE);
    }

    if (fsmdef_compare_caller_id_string(dest_caller_id->called_name,
                                        src_caller_id->called_name)) {
        return (TRUE);
    }

    if (fsmdef_compare_caller_id_string(dest_caller_id->called_number,
                                        src_caller_id->called_number)) {
        return (TRUE);
    }

    if (fsmdef_compare_caller_id_string(dest_caller_id->orig_called_name,
                                        src_caller_id->orig_called_name)) {
        return (TRUE);
    }

    if (fsmdef_compare_caller_id_string(dest_caller_id->orig_called_number,
                                        src_caller_id->orig_called_number)) {
        return (TRUE);
    }

    if (fsmdef_compare_caller_id_string(dest_caller_id->last_redirect_name,
                                        src_caller_id->last_redirect_name)) {
        return (TRUE);
    }

    if (fsmdef_compare_caller_id_string(dest_caller_id->last_redirect_number,
                                        src_caller_id->last_redirect_number)) {
        return (TRUE);
    }

    if (fsmdef_compare_caller_id_string(dest_caller_id->orig_rpid_number,
                                        src_caller_id->orig_rpid_number)) {
        return (TRUE);
    }

    if (dest_caller_id->display_calling_number != src_caller_id->display_calling_number ||
        dest_caller_id->display_called_number  != src_caller_id->display_called_number ||
        dest_caller_id->call_type              != src_caller_id->call_type ||
        dest_caller_id->call_instance_id       != src_caller_id->call_instance_id) {
        return (TRUE);
    }

    return (FALSE);
}

static void
fsmdef_mv_caller_id (fsmdef_dcb_t *dcb, cc_caller_id_t *caller_id)
{
    /*
     * Move the caller ID from the source to the storage in dcb if there
     * is a change in what is already stored in the dcb.
     */
    if (fsmdef_compare_caller_id(&dcb->caller_id, caller_id)) {
        cc_mv_caller_id(&dcb->caller_id, caller_id);
        dcb->ui_update_required = TRUE;
    }
}

static void
fsmdef_update_callinfo (fsm_fcb_t *fcb, cc_feature_t *msg)
{
    static const char fname[]    = "fsmdef_update_callinfo";
    fsmdef_dcb_t      *dcb       = fcb->dcb;
    cc_feature_data_t *feat_data = &(msg->data);
    cc_action_data_t   action_data;
    cc_caller_id_t    *caller_id;

    if (msg->data_valid == FALSE) {
        /* No data to use for update. Just ignore the event. */
        return;
    }

    if ((feat_data->call_info.feature_flag & CC_UI_STATE) &&
        (feat_data->call_info.ui_state == CC_UI_STATE_RINGOUT)) {

        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG1),
                     dcb->call_id, dcb->line, fname,
                     "setting spoof_ringout_requested");

        dcb->spoof_ringout_requested = TRUE;
    } else {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_CLR_SPOOF_RQSTD),
                     dcb->call_id, dcb->line, fname);

        dcb->spoof_ringout_requested = FALSE;
    }

    caller_id = &feat_data->call_info.caller_id;

    if (feat_data->call_info.feature_flag & CC_CALLER_ID) {
        fsmdef_mv_caller_id(dcb, caller_id);
    }

    /*
     * If CCM provides a call instance id and it does not match
     * the current call instance id, free the current call instance
     * id and set the call instance id to the newly provided value.
     */
    if (feat_data->call_info.feature_flag & CC_CALL_INSTANCE &&
        feat_data->call_info.caller_id.call_instance_id != dcb->caller_id.call_instance_id) {
        if (dcb->caller_id.call_instance_id != 0) {
            fsmutil_free_ci_id(dcb->caller_id.call_instance_id, dcb->line);
        }
        dcb->caller_id.call_instance_id =
            feat_data->call_info.caller_id.call_instance_id;
        fsmutil_set_ci_id(dcb->caller_id.call_instance_id, dcb->line);
        dcb->ui_update_required = TRUE;
    }

    /*
     * Update security status
     */
    fsmdef_update_callinfo_security_status(dcb, &feat_data->call_info);

    /*
     * Update call policy
     */
    if (feat_data->call_info.feature_flag & CC_POLICY) {
        if (dcb->policy != feat_data->call_info.policy) {
	    dcb->policy = feat_data->call_info.policy;
	    dcb->ui_update_required = TRUE;
	}
    }

    /*
     * Save orientation so that UI call update can be done at
     * any time that UI needs to be updated.
     */
    if (feat_data->call_info.feature_flag & CC_ORIENTATION) {
        if (dcb->orientation != feat_data->call_info.orientation) {
            dcb->orientation = feat_data->call_info.orientation;
            dcb->ui_update_required = TRUE;
        }
    }

    /*
     * This call info. event may be as part of media effecting
     * signaling event (such as INVITE, re-INVITE, 180 etc.)
     * which will be followed by actual media effected event. It is
     * indicated by SIP stack to improve media cutting through as soonest.
     * If SIP indicates that UI can be delayed then do not update call
     * information now. The UI will be updated as part of media
     * manipulation event that will follow.
     * (Note: call info event is sent separately from the SIP signaling
     *  event currently and it is sent before SIP signaling event).
     */
    if (feat_data->call_info.feature_flag & CC_DELAY_UI_UPDATE) {
        /* Delay UI update */
    } else {
        /*
         * Only perform a UI update if something changed.
         */
        if (dcb->ui_update_required == TRUE
           || dcb->spoof_ringout_requested == TRUE) {

            action_data.update_ui.action = CC_UPDATE_CALLER_INFO;
            action_data.update_ui.data.caller_info = feat_data->call_info;

            (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_UPDATE_UI,
                                 &action_data);

            FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG1), dcb->call_id,
                         dcb->line, fname, "UI update");
        } else {
            FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG1),
                         dcb->call_id, dcb->line, fname, "No UI update");
        }
    }
    /* update callref */
    if ( dcb->callref == 0 ) {
        dcb->callref = feat_data->call_info.callref;
        ui_update_callref(dcb->line, dcb->call_id, feat_data->call_info.callref);
    }
    /* update gcid */
    if (feat_data->call_info.global_call_id[0] != '\0') {
        ui_update_gcid(dcb->line, dcb->call_id, feat_data->call_info.global_call_id);
        /*
         * store the gcid lcb, this is used to prevent short ringing
         * in scenarios where a call is routed to the calling phone.
         */
        lsm_update_gcid(dcb->call_id, feat_data->call_info.global_call_id);
    }
}

/**
 *  Function: fsmdef_set_feature_timer
 *
 *  Description: This function is called to set (start) timer for a
 *    given timer. The context of the timer is set so that upon
 *    expiration the corresponding context (dcb) can be obtained.
 *
 *  Parameters:
 *    dcb      - pointer to the fsmdef_dcb_t. The caller must ensure
 *               dcb is not NULL.
 *    timer    - pointer to cprTimer_t. The caller must ensure that
 *               timer is not NULL.
 *    duration - the time duration for the timer to be set.
 *
 *  Returns:
 *    N/A.
 */
static void
fsmdef_set_feature_timer (fsmdef_dcb_t *dcb, cprTimer_t *timer,
                          uint32_t duration)
{
    static const char fname[] = "fsmdef_set_feature_timer";

    if (cprCancelTimer(*timer) != CPR_SUCCESS) {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_TMR_CANCEL_FAILED),
                     dcb->call_id, dcb->line, fname, "Feature", cpr_errno);

        return;
    }

    if (cprStartTimer(*timer, duration, (void *)(long)dcb->call_id) == CPR_FAILURE) {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_TMR_START_FAILED),
                     dcb->call_id, dcb->line, fname, "Feature", cpr_errno);
    }
}

static void
fsmdef_set_req_pending_timer (fsmdef_dcb_t *dcb)
{
    static const char fname[] = "fsmdef_set_req_pending_timer";
    uint32_t msec;

    if (dcb == NULL) {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_INVALID_DCB), fname);
        return;
    }

    if (!dcb->req_pending_tmr) {
        dcb->req_pending_tmr = cprCreateTimer("Request Pending",
                                              GSM_REQ_PENDING_TIMER,
                                              TIMER_EXPIRATION,
                                              gsm_msgq);

        if (dcb->req_pending_tmr == NULL) {
            FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_TMR_CREATE_FAILED),
                         dcb->call_id, dcb->line, fname, "Request Pending");

            return;
        }
    }

    if (dcb->inbound) {
        // We did not initiate this call, so set timer between 0 and 2000ms
        msec = abs(cpr_rand()) % 2000;
    } else {
        // We initiated this call, so set the timer between 2100 and 4000ms
        msec = abs(cpr_rand()) % 1900 + 2100;
    }

    FSM_DEBUG_SM(DEB_L_C_F_PREFIX"Starting req pending timer for %d ms.",
		DEB_L_C_F_PREFIX_ARGS(FSM, dcb->line, dcb->call_id, fname), msec);

    fsmdef_set_feature_timer(dcb, &dcb->req_pending_tmr, msec);
}

static void
fsmdef_set_ringback_delay_timer (fsmdef_dcb_t *dcb)
{
    static const char fname[] = "fsmdef_set_ringback_delay_timer";

    if (dcb == NULL) {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_INVALID_DCB), fname);
        return;
    }

    FSM_DEBUG_SM(DEB_L_C_F_PREFIX"Starting Ringback Delay timer"
                 " for %d ms.\n",
                 DEB_L_C_F_PREFIX_ARGS(FSM, dcb->line, dcb->call_id, fname), RINGBACK_DELAY);

    fsmdef_set_feature_timer(dcb, &dcb->ringback_delay_tmr, RINGBACK_DELAY);
}

/*******************************************************************
 * event functions
 */
static sm_rcs_t
fsmdef_ev_default (sm_event_t *event)
{
    fsm_fcb_t    *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t *dcb = fcb->dcb;
    cc_feature_t *msg = (cc_feature_t *) event->msg;

    FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SM_DEFAULT_EVENT));

    if (!dcb) {
      return (SM_RC_END);
    }

    /*
     * For WebRTC events, we must send back a message to enable the
     * corresponding error callback to be called. Arguably, these
     * should always be caught by the PeerConnection and thrown as exceptions,
     * but this enusres that the behavior is water-tight.
     */
    switch (event->event) {
      case CC_MSG_CREATEOFFER:
          ui_create_offer(evCreateOfferError, fcb->state, msg->line,
              msg->call_id, dcb->caller_id.call_instance_id, strlib_empty(),
              PC_INVALID_STATE, "Cannot create offer in state %s",
              fsmdef_state_name(fcb->state));
        break;

      case CC_MSG_CREATEANSWER:
          ui_create_answer(evCreateAnswerError, fcb->state, msg->line,
              msg->call_id, dcb->caller_id.call_instance_id, strlib_empty(),
              PC_INVALID_STATE, "Cannot create answer in state %s",
              fsmdef_state_name(fcb->state));
        break;

      case CC_MSG_SETLOCALDESC:
          ui_set_local_description(evSetLocalDescError, fcb->state, msg->line,
              msg->call_id, dcb->caller_id.call_instance_id, strlib_empty(),
              PC_INVALID_STATE, "Cannot set local description in state %s",
              fsmdef_state_name(fcb->state));
        break;

      case CC_MSG_SETREMOTEDESC:
          ui_set_remote_description(evSetRemoteDescError, fcb->state,
              msg->line, msg->call_id, dcb->caller_id.call_instance_id,
              strlib_empty(), PC_INVALID_STATE,
              "Cannot set remote description in state %s",
              fsmdef_state_name(fcb->state));
        break;

      case CC_MSG_ADDCANDIDATE:
          ui_ice_candidate_add(evAddIceCandidateError, fcb->state, msg->line,
              msg->call_id, dcb->caller_id.call_instance_id, strlib_empty(),
              PC_INVALID_STATE, "Cannot add ICE candidate in state %s",
              fsmdef_state_name(fcb->state));
        break;

      case CC_MSG_ADDSTREAM:
      case CC_MSG_REMOVESTREAM:
          /* This shouldn't happen, since PeerConnection should check
           * the state before sending these events to us. The debug message
           * here is to catch the unexpected situation of such an event
           * getting through anyway. */
          FSM_DEBUG_SM(DEB_L_C_F_PREFIX"Cannot add or remove streams "
              "in state %s", DEB_L_C_F_PREFIX_ARGS(FSM, dcb->line,
              msg->call_id, __FUNCTION__), fsmdef_state_name(fcb->state));
        break;

      default:
          cc_call_state(dcb->call_id, dcb->line, CC_STATE_UNKNOWN, NULL);
        break;
    }

    return (SM_RC_END);
}

/*
 * Default event handler for feature_ack event
 */
static sm_rcs_t
fsmdef_ev_default_feature_ack (sm_event_t *event)
{
  static const char fname[] = "fsmdef_ev_default_feature_ack";
    fsm_fcb_t        *fcb    = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t     *dcb    = fcb->dcb;
    cc_feature_ack_t *msg    = (cc_feature_ack_t *) event->msg;
    cc_features_t     ftr_id = msg->feature_id;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, "fsmdef_ev_default_feature_ack"));

    if (ftr_id == CC_FEATURE_SELECT) {
        /* Reeceived the response for select, so turn the flag off */
        dcb->select_pending = FALSE;
        if (dcb->selected) {
            dcb->selected = FALSE;
            g_numofselected_calls--;
            FSM_DEBUG_SM(DEB_L_C_F_PREFIX"call is unselected and number of selected \
                          calls on the phone is %d\n",
						  DEB_L_C_F_PREFIX_ARGS(FSM, dcb->line, msg->call_id, fname),
                          g_numofselected_calls);

        } else {
            dcb->selected = TRUE;
            if ((g_b2bjoin_pending == FALSE) &&
                (dcb->active_feature == CC_FEATURE_B2B_JOIN)) {
                g_b2bjoin_pending = TRUE;
                g_b2bjoin_callid  = dcb->call_id;
            }
            g_numofselected_calls++;
            FSM_DEBUG_SM(DEB_L_C_F_PREFIX"call is selected and number of selected \
                          calls on the phone is %d\n",
						  DEB_L_C_F_PREFIX_ARGS(FSM, dcb->line, dcb->call_id, fname),
                          g_numofselected_calls);
        }
        ui_call_selected(dcb->line, lsm_get_ui_id(dcb->call_id), (dcb->selected)?CC_DIALOG_LOCKED:CC_DIALOG_UNLOCKED);

    } else if (dcb->active_feature != ftr_id) {
        // check if we are getting feature_ack for the active feature
        FSM_DEBUG_SM(DEB_L_C_F_PREFIX"feature_ack rcvd for %s but %s is active",
                     DEB_L_C_F_PREFIX_ARGS(FSM, dcb->line, dcb->call_id, fname),
					 cc_feature_name(ftr_id), cc_feature_name(dcb->active_feature));

    }

    // reset active feature
    dcb->active_feature = CC_FEATURE_NONE;

    return (SM_RC_END);
}


static void
fsmdef_sm_ignore_ftr (fsm_fcb_t *fcb, int fname, cc_features_t ftr_id)
{
    fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
    if (fcb->dcb) {
        cc_call_state(fcb->dcb->call_id, fcb->dcb->line, CC_STATE_UNKNOWN,
                      NULL);
    }
}

static void
fsmdef_sm_ignore_src (fsm_fcb_t *fcb, int fname, cc_srcs_t src_id)
{
    fsm_sm_ignore_src(fcb, __LINE__, src_id);

    if (fcb->dcb) {
        cc_call_state(fcb->dcb->call_id, fcb->dcb->line, CC_STATE_UNKNOWN,
                      NULL);
    }
}

/*
 * fsmdef_error_onhook_timeout
 *
 * Timer is started immediately after the call error. This function is called
 * when there is a timeout event generated by the timer .
 *
 * @param[in] data    The gsm ID (callid_t) of the call onhook timeout
 *                    has occured.
 *
 * @return            N/A
 */
void
fsmdef_error_onhook_timeout (void *data)
{
    static const char fname[] = "fsmdef_error_onhook_timeout";
    fsmdef_dcb_t *dcb;
    callid_t   call_id;

    call_id = (callid_t)(long)data;
    if (call_id == CC_NO_CALL_ID) {
        /* Invalid call id */
        GSM_ERR_MSG(get_debug_string(FSMDEF_DBG1), 0, 0, fname, "invalid data");
        return;
    }

    /* Retrieve dcb from call id */
    dcb = fsmdef_get_dcb_by_call_id(call_id);
    if (dcb == NULL) {
        GSM_ERR_MSG(get_debug_string(FSMDEF_DBG_INVALID_DCB), fname);

        return;
    }

    FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG1),
                 dcb->call_id, dcb->line, fname, "timeout");

    cc_int_onhook(CC_SRC_GSM, CC_SRC_GSM, CC_NO_CALL_ID, CC_REASON_NONE,
                  dcb->call_id, dcb->line, FALSE, FALSE, __FILE__, __LINE__);
}

/**
 *  Function: fsmdef_feature_timer_timeout
 *
 *  Description: This function is called when receives time out
 *    notification. The function then converts the timer event
 *    into the CCAPI event suitable for GSM's call state machine.
 *
 *  Parameters:
 *    feature_id - corresponding feature ID of the timer event.
 *    data       - the opaque data for the caller which is actually
 *                 is the GSM's call id.
 *
 *  Returns:
 *    NULL or
 *    pointer to the cc_feature_t.
 */
void *
fsmdef_feature_timer_timeout (cc_features_t feature_id, void *data)
{
    static const char fname[] = "fsmdef_feature_timer_timeout";
    cc_feature_t     *pmsg;
    callid_t         call_id;
    fsmdef_dcb_t     *dcb;

    FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG1), 0, 0, fname, "timeout");

    call_id = (callid_t)(long)data;
    if (call_id == CC_NO_CALL_ID) {
        /* Invalid call id */
        GSM_ERR_MSG(get_debug_string(FSMDEF_DBG1), 0, 0, fname, "invalid data");
        return NULL;
    }

    dcb = fsmdef_get_dcb_by_call_id(call_id);
    if (dcb == NULL) {
        /* The corresponding dcb for the call ID is not found */
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_INVALID_DCB), fname);
        return (NULL);
    }

    if (dcb->inband_received && feature_id == CC_FEATURE_RINGBACK_DELAY_TIMER_EXP) {
        /* Double check if inbound ringback indication is received*/
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG1), 0, 0, fname, "inband received!");
        return (NULL);
    }

    pmsg = (cc_feature_t *) gsm_get_buffer(sizeof(*pmsg));
    if (!pmsg) {
        GSM_ERR_MSG(get_debug_string(FSMDEF_DBG1),
                    call_id, dcb->line, fname,
                    "failed to allocate feature timer message");
        return NULL;
    }

    memset(pmsg, 0, sizeof(*pmsg));

    pmsg->msg_id     = CC_MSG_FEATURE;
    pmsg->src_id     = CC_SRC_GSM;
    pmsg->call_id    = call_id;
    pmsg->line       = dcb->line;
    pmsg->feature_id = feature_id;
    pmsg->data_valid = FALSE;

    return (void *) pmsg;
}

/**
 *
 * Function handles idle setup request received from the network
 *
 * @sm_eent_t   event
 *
 * @return  SM_RC_END
 *
 * @pre     (called_number and  called_number not NULL)
 * @pre     (event->data not_eq NULL)
 * @pre     (event->msg not_eq NULL)
 */
static sm_rcs_t
fsmdef_ev_idle_setup (sm_event_t *event)
{
    static const char fname[] = "fsmdef_ev_idle_setup";
    fsm_fcb_t        *fcb     = (fsm_fcb_t *) event->data;
    cc_setup_t       *msg     = (cc_setup_t *) event->msg;
    callid_t          call_id = msg->call_id;
    int               temp;
    string_t          called_number  = msg->caller_id.called_number;
    string_t          calling_number = msg->caller_id.calling_number;
    fsmdef_dcb_t     *dcb;
    cc_causes_t       cause;
    fsmxfr_xcb_t     *xcb;
    fsm_fcb_t        *other_fcb;
    callid_t          other_call_id;
    boolean           alerting = TRUE;
    boolean           replaces = msg->replaces;
    int               other_active_calls;
    boolean           transfer_target = FALSE;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    /*
     * Make sure we have a valid called_number.
     */
    if ((called_number == NULL) || (called_number[0] == '\0')) {
        return (SM_RC_CLEANUP);
    }

    /*
     * Check the called/calling number for the E.164 escaped "+"
     * and convert it to a real "+" if present.
     */
    if (cpr_strncasecmp(called_number, "%2B", 3) == 0) {
        fsmdef_convert_esc_plus(called_number);
    }
    if (cpr_strncasecmp(calling_number, "%2B", 3) == 0) {
        fsmdef_convert_esc_plus(calling_number);
    }

    FSM_DEBUG_SM(DEB_L_C_F_PREFIX"called_number= %s calling_number= %s",
		DEB_L_C_F_PREFIX_ARGS(FSM, msg->line, msg->call_id, fname),
		msg->caller_id.called_number, msg->caller_id.calling_number);

    //idle = lsm_is_phone_idle();

    xcb = fsmxfr_get_xcb_by_call_id(call_id);
    if (xcb && replaces) {
        transfer_target = TRUE;
    }

    /*
     * Get a new incoming call context.
     * if we the target of a transfer, request that the line
     * availability be increased by one to account for the third
     * instance of a line to become available to allow completion
     * of transfer.
     */
    cause = fsm_get_new_incoming_call_context(call_id, fcb, called_number,
                                              transfer_target);
    dcb = fcb->dcb;
    if ((msg->call_info.type != CC_FEAT_MONITOR) &&
        (replaces != TRUE)) {
        if (lsm_is_line_available(dcb->line, TRUE) == FALSE) {
            /* increment it to compensate for decrementing while ending the call. */
            lsm_increment_call_chn_cnt(dcb->line);
            fsmdef_end_call(dcb, CC_CAUSE_BUSY);
            return (SM_RC_END);
        }
        lsm_increment_call_chn_cnt(dcb->line);
    }
    else {
        /*
         * join calls (barged, M & R) are not counted by CUCM. so we should not count either
         */
        dcb->call_not_counted_in_mnc_bt = TRUE;
        dcb->join_call_id = msg->call_info.data.join.join_call_id;
    }
    /*
     * Set default orientation for the incoming setup as "from" to
     * avoid updating UI when call info is received in "ACK" which
     * shoule be "from" for typicall incoming call.
     */
    dcb->orientation = CC_ORIENTATION_FROM;

    switch (cause) {
    case CC_CAUSE_OK:
        break;

    case CC_CAUSE_NO_RESOURCE:
        cc_call_state(fcb->dcb->call_id, fcb->dcb->line, CC_STATE_UNKNOWN,
                      NULL);
        return (SM_RC_CLEANUP);

    default:
        fsmdef_end_call(dcb, cause);
        return (SM_RC_END);
    }

    /*
     * Check for Anonymous call blocking. If low bit is set,
     * then do not allow call. Note that we must allow both upper and lowercase
     * ANON strings, hence the use of strcasestr
     */
    config_get_value(CFGID_ANONYMOUS_CALL_BLOCK, &temp, sizeof(temp));
    if (temp & 1) {
        /*
         * We compare the calling name to the hardcoded Anonymous string we use in
         * our SIP headers. This handles the case where calling name was pulled from
         * the From header and was set to Anonymous. We also compare the calling name
         * to the localized string index for Private which is what the calling name will
         * be set to if an RPID header was received.
         */
        char tmp_str[STATUS_LINE_MAX_LEN];
        sstrncpy(tmp_str, platform_get_phrase_index_str(UI_PRIVATE), sizeof(tmp_str));
        if (strcasestr(msg->caller_id.calling_name, SIP_HEADER_ANONYMOUS_STR) ||
            strcasestr(msg->caller_id.calling_name, tmp_str)) {
            fsmdef_end_call(dcb, CC_CAUSE_ANONYMOUS);
            return (SM_RC_END);
        }
    }

    /*
     * Check if Call Waiting is disabled.
     * If call-waiting is disabled and there is another call active
     * then the GSM will return busy.
     * unless this is a barge/monitor target call
     *
     */
    config_get_line_value(CFGID_LINE_CALL_WAITING, &temp, sizeof(temp),
                          dcb->line);
    other_active_calls = fsmdef_get_active_call_cnt(call_id);

    if ((msg->call_info.type != CC_FEAT_MONITOR)) {
        if ((!(temp & 1)) && (other_active_calls > 0) &&
            (!((xcb != NULL) && (xcb->mode == FSMXFR_MODE_TARGET)))) {

            fsmdef_end_call(dcb, CC_CAUSE_BUSY);

            return (SM_RC_END);
        }
    }

    /*
     * If this is a call to replace another call,
     * check that we have a call to replace
     */
    other_call_id = fsmxfr_get_other_call_id(xcb, call_id);
    if (replaces) {
        if ((xcb == NULL) || (other_call_id == CC_NO_CALL_ID)) {
            FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG1),
                         dcb->call_id, dcb->line, "",
                         "No call to replace");

            fsmdef_end_call(dcb, CC_CAUSE_NO_REPLACE_CALL);
            return (SM_RC_END);
        }
    }


    /*
     * The called name and number will not be obtained from
     * the setup. Remove it before getting from the setup message.
     */
    if (msg->caller_id.called_name != NULL) {
        strlib_free(msg->caller_id.called_name);
        msg->caller_id.called_name = NULL;
    }
    if (msg->caller_id.called_number != NULL) {
        strlib_free(msg->caller_id.called_number);
        msg->caller_id.called_number = NULL;
    }
    /* Get the caller ID from the setup message */
    fsmdef_mv_caller_id(dcb, &msg->caller_id);

    if (msg->caller_id.call_type == CC_CALL_FORWARDED) {

        dcb->call_type = FSMDEF_CALL_TYPE_FORWARD;
    }

    /*
     * If a call instance id is provided, use it in the dcb. We will
     * check to see that the call instance id provided differs from
     * what is currently stored in the dcb before assigning it.
     */
    if (msg->call_info.type == CC_FEAT_CALLINFO) {
        cc_feature_data_call_info_t *data;

        data = &msg->call_info.data.call_info_feat_data;
        if (data->feature_flag & CC_CALL_INSTANCE) {
            if (data->caller_id.call_instance_id != 0 &&
                data->caller_id.call_instance_id !=
                               dcb->caller_id.call_instance_id) {
                if (dcb->caller_id.call_instance_id != 0) {
                    fsmutil_free_ci_id(dcb->caller_id.call_instance_id,
                                       dcb->line);
                }
                dcb->caller_id.call_instance_id =
                    data->caller_id.call_instance_id;
                fsmutil_set_ci_id(dcb->caller_id.call_instance_id, dcb->line);
            }
        }

        if (data->feature_flag & CC_SECURITY) {
            FSM_SET_SECURITY_STATUS(dcb, data->security);
        }

        if (data->feature_flag & CC_POLICY) {
            FSM_SET_POLICY(dcb, data->policy);
        }
    }
    dcb->alert_info = msg->alert_info;
    dcb->alerting_ring = msg->alerting_ring;
    dcb->alerting_tone = msg->alerting_tone;

    /*
     * This is an incoming call, so we know we will need to send a
     * RELEASE when we clear the call.
     */
    dcb->send_release = TRUE;

    cause = gsmsdp_negotiate_offer_sdp(fcb, &msg->msg_body, TRUE);
    if (cause != CC_CAUSE_OK) {
        return (fsmdef_release(fcb, cause, dcb->send_release));
    }

    if (transfer_target) {
        /*
         * Send a proceeding event to the transfer call.
         *
         * The proceeding event will end the other call
         * and answer the current call.
         *
         */
        cc_int_proceeding(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id,
                          dcb->line, &(dcb->caller_id));
    }

    cc_int_setup_ack(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id, dcb->line,
                     &(dcb->caller_id), NULL);

    FSM_SET_FLAGS(dcb->msgs_sent, FSMDEF_MSG_SETUP_ACK);



    cc_int_proceeding(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id, dcb->line,
                          &(dcb->caller_id));

    FSM_SET_FLAGS(dcb->msgs_sent, FSMDEF_MSG_PROCEEDING);


    alerting = fsmdef_extract_join_target(event);

    /*
     * This might be the transfer call from the transferee to the target.
     * In such a case we want this new call to match the state of the call
     * that it is replacing.
     */
    if (xcb != NULL) {
        other_fcb = fsm_get_fcb_by_call_id_and_type(other_call_id,
                                                    FSM_TYPE_DEF);
        if (other_fcb && (other_fcb->old_state == FSMDEF_S_CONNECTED ||
            other_fcb->old_state == FSMDEF_S_CONNECTED_MEDIA_PEND ||
            other_fcb->old_state == FSMDEF_S_RESUME_PENDING ||
            other_fcb->state == FSMDEF_S_CONNECTED ||
            other_fcb->state == FSMDEF_S_CONNECTED_MEDIA_PEND ||
            other_fcb->state == FSMDEF_S_RESUME_PENDING)) {
            alerting = FALSE;
        }
    }


    if (alerting == TRUE) {
        /*
         * set the call priority in lcb. This is used by ringer logic.
         */
        if ((msg->call_info.type == CC_FEAT_CALLINFO) &&
            (msg->call_info.data.call_info_feat_data.priority == CC_CALL_PRIORITY_URGENT)) {
            lsm_set_lcb_call_priority(call_id);
        }
        if ((msg->call_info.type == CC_FEAT_CALLINFO) &&
            (msg->call_info.data.call_info_feat_data.dusting == TRUE)) {
            lsm_set_lcb_dusting_call(call_id);
        }

        cc_call_state(dcb->call_id, dcb->line, CC_STATE_ALERTING,
                      FSMDEF_CC_CALLER_ID);
        /*
         * Currently we do not send SDP in the 180 response.
         */
        cc_int_alerting(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id, dcb->line,
                        &(dcb->caller_id), NULL, FALSE);

       /* update callref */
       if ( dcb->callref == 0 ) {
           dcb->callref = msg->call_info.data.call_info_feat_data.callref;
           ui_update_callref(dcb->line, dcb->call_id, msg->call_info.data.call_info_feat_data.callref);
       }
       /* update gcid */
       ui_update_gcid(dcb->line, dcb->call_id, msg->call_info.data.call_info_feat_data.global_call_id);
       /*
        * store the gcid lcb, this is used to prevent short ringing
        * in scenarios where a call is routed to the calling phone.
        */
       lsm_update_gcid(dcb->call_id, msg->call_info.data.call_info_feat_data.global_call_id);
    }

    ui_cc_capability(dcb->line, lsm_get_ui_id(dcb->call_id), msg->recv_info_list);

    FSM_SET_FLAGS(dcb->msgs_sent, FSMDEF_MSG_ALERTING);

    fsm_change_state(fcb, __LINE__, FSMDEF_S_INCOMING_ALERTING);

    return (SM_RC_END);
}


sm_rcs_t
fsmdef_dialstring (fsm_fcb_t *fcb, const char *dialstring,
                   cc_redirect_t *redirect, boolean replace,
                   cc_call_info_t *call_info)
{
    static const char fname[] = "fsmdef_dialstring";
    fsmdef_dcb_t     *dcb = fcb->dcb;
    cc_causes_t       cause;
    cc_msgbody_info_t msg_body;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    if (dialstring) {
        if (strlen(dialstring) > MAX_SIP_URL_LENGTH) {
            FSM_DEBUG_SM(DEB_F_PREFIX"Dial string too long", DEB_F_PREFIX_ARGS(FSM, fname));
            /* Force clean up call without sending release */
            return (fsmdef_release(fcb, CC_CAUSE_INVALID_NUMBER, FALSE));
        }
    }

    /*
     * If there is active feature which is waiting for digit collection
     * then use service URI preceded with dialed number.
     */
    switch (dcb->active_feature) {

    case CC_FEATURE_CFWD_ALL:
        fsmdef_append_dialstring_to_feature_uri(dcb, dialstring);
        break;

    default:
        if (dialstring) {
            dcb->caller_id.called_number =
                strlib_update(dcb->caller_id.called_number, dialstring);
        }
        break;
    }

    cause = gsmsdp_create_local_sdp(dcb, FALSE, TRUE, TRUE, TRUE, TRUE);
    if (cause != CC_CAUSE_OK) {
        FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
        /* Force clean up call without sending release */
        return (fsmdef_release(fcb, cause, FALSE));
    }

    /* Build SDP for sending out */
    cause = gsmsdp_encode_sdp_and_update_version(dcb, &msg_body);
    if (cause != CC_CAUSE_OK) {
        FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
        /* Force clean up call without sending release */
        return (fsmdef_release(fcb, cause, FALSE));
    }

    /*
     * Since we are sending setup to UI we will also have to send
     * release to it to for sip stack to clean up the call
     */
    dcb->send_release = TRUE;

    /*
     * lsm_parse_displaystr will free present called number and return
     * pointer to parsed called number
     */
    dcb->caller_id.called_number =
        lsm_parse_displaystr(dcb->caller_id.called_number);

    /* set default orientation for outgoing call */
    dcb->orientation = CC_ORIENTATION_TO;
    dcb->inbound = FALSE;

    /*
     * Invoke cc_call_state with modified caller_id for features such as
     * pickups
     */
    fsmdef_set_call_info_cc_call_state(dcb, CC_STATE_DIALING_COMPLETED, CC_CAUSE_MIN);

    FSM_SET_FLAGS(dcb->msgs_sent, FSMDEF_MSG_SETUP);

    fsmdef_set_call_info_cc_call_state(dcb, CC_STATE_CALL_SENT, CC_CAUSE_MIN);

    /*
     * Send setup or INVITE out after finishing all UI activities and media
     * preparation. This is done as the final step in order to minimize
     * UI activities/media activities to run concurrently while processing
     * the response from the network. On the platform that uses Java VM to
     * support UI and media, the time to process the UI and media activities
     * may take longer than the network response time to the INVITE (such as
     * voice mail case) and JNI calls may be blocked while invoking the
     * UI or media calls in the LSM.
     */
    cc_int_setup(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id, dcb->line,
                 &(dcb->caller_id), dcb->alert_info, VCM_INSIDE_RING,
                 VCM_INSIDE_DIAL_TONE, redirect, call_info, replace, NULL, &msg_body);
    fsm_change_state(fcb, __LINE__, FSMDEF_S_CALL_SENT);

    return (SM_RC_END);
}


static sm_rcs_t
fsmdef_ev_dialstring (sm_event_t *event)
{
    sm_rcs_t      sm_rc;
    fsm_fcb_t    *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t *dcb = fcb->dcb;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    // handle dialstring event if from callfwdall
    if (fsmdef_process_dialstring_for_callfwd(event) == SM_RC_END) {
        // release the call started to collect callfwd info
        dcb->send_release = FALSE;
        return (fsmdef_release(fcb, CC_CAUSE_NORMAL, dcb->send_release));
    }

    sm_rc = fsmdef_dialstring(fcb, ((cc_dialstring_t *)event->msg)->dialstring,
                              NULL, FALSE, NULL);

    return (sm_rc);
}


/**
 * fsmdef_ev_createoffer
 *
 * Generates Offer SDP
 *
 */
static sm_rcs_t
fsmdef_ev_createoffer (sm_event_t *event) {
    sm_rcs_t            sm_rc;
    fsm_fcb_t           *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t        *dcb = fcb->dcb;
    cc_causes_t         cause = CC_CAUSE_NORMAL;
    cc_msgbody_info_t   msg_body;
    cc_feature_t        *msg = (cc_feature_t *) event->msg;
    line_t              line = msg->line;
    callid_t            call_id = msg->call_id;
    cc_causes_t         lsm_rc;
    int                 sdpmode = 0;
    char                *ufrag = NULL;
    char                *ice_pwd = NULL;
    short               vcm_res;
    session_data_t      *sess_data_p = NULL;
    char                *local_sdp = NULL;
    uint32_t            local_sdp_len = 0;
    boolean             has_stream = FALSE;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));
    if (!sdpmode) {
      /* Force clean up call without sending release */
      return (fsmdef_release(fcb, cause, FALSE));
    }

    if (dcb == NULL) {
      FSM_DEBUG_SM(DEB_F_PREFIX"dcb is NULL.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
      return SM_RC_CLEANUP;
    }

    /* For now, if the local SDP has been set, we don't allow it to be set
       again. This will change when we allow renegotiation of ongoing
       sessions. See bug 840728. */
    if (dcb->local_sdp_complete) {
        FSM_DEBUG_SM(DEB_F_PREFIX"local SDP already created: returning "
        "prevously created SDP.\n", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

        local_sdp = sipsdp_write_to_buf(dcb->sdp->src_sdp, &local_sdp_len);
        if (!local_sdp) {
            ui_create_offer(evCreateOfferError, fcb->state, line, call_id,
                dcb->caller_id.call_instance_id, strlib_empty(),
                PC_INTERNAL_ERROR, "Could not re-create local SDP for offer");
            FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
            return (fsmdef_release(fcb, cause, FALSE));
        }

        ui_create_offer(evCreateOfferSuccess, fcb->state, line, call_id,
            dcb->caller_id.call_instance_id,
            strlib_malloc(local_sdp,-1), PC_NO_ERROR, NULL);
        free(local_sdp);
        return (SM_RC_END);
    }

    dcb->inbound = FALSE;

    if (msg->data.session.constraints) {
       gsmsdp_process_cap_constraints(dcb, msg->data.session.constraints);
       fsmdef_free_constraints(msg->data.session.constraints);
       msg->data.session.constraints = 0;
    }

    if (dcb->media_cap_tbl->cap[CC_VIDEO_1].enabled ||
        dcb->media_cap_tbl->cap[CC_AUDIO_1].enabled ||
        dcb->media_cap_tbl->cap[CC_DATACHANNEL_1].enabled) {
      has_stream = TRUE;
    }

    if (!has_stream) {
      ui_create_offer(evCreateOfferError, fcb->state, line, call_id,
          dcb->caller_id.call_instance_id, strlib_empty(),
          PC_INVALID_STATE, "Cannot create SDP without any streams.");
      return SM_RC_END;
    }

    vcm_res = vcmGetIceParams(dcb->peerconnection, &ufrag, &ice_pwd);
    if (vcm_res) {
    	FSM_DEBUG_SM(DEB_F_PREFIX"vcmGetIceParams returned an error",
            DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
      ui_create_offer(evCreateOfferError, fcb->state, line, call_id,
          dcb->caller_id.call_instance_id, strlib_empty(),
          PC_INTERNAL_ERROR, "Failed to get ICE parameters for local SDP");
      return (fsmdef_release(fcb, cause, FALSE));
    }

    dcb->ice_ufrag = (char *)cpr_malloc(strlen(ufrag) + 1);
    if (!dcb->ice_ufrag)
    	return SM_RC_END;

    sstrncpy(dcb->ice_ufrag, ufrag, strlen(ufrag) + 1);
    free(ufrag);


    dcb->ice_pwd = (char *)cpr_malloc(strlen(ice_pwd) + 1);
    if (!dcb->ice_pwd)
    	return SM_RC_END;

    sstrncpy(dcb->ice_pwd, ice_pwd, strlen(ice_pwd) + 1);
    free(ice_pwd);

    vcm_res = vcmGetDtlsIdentity(dcb->peerconnection,
                    dcb->digest_alg, FSMDEF_MAX_DIGEST_ALG_LEN,
                    dcb->digest, FSMDEF_MAX_DIGEST_LEN);

    if (vcm_res) {
    	FSM_DEBUG_SM(DEB_F_PREFIX"vcmGetDtlsIdentity returned an error", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
        return SM_RC_END;
    }

    cause = gsmsdp_create_local_sdp(dcb, FALSE, TRUE, TRUE, TRUE, TRUE);
    if (cause != CC_CAUSE_OK) {
        ui_create_offer(evCreateOfferError, fcb->state, line, call_id,
            dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INTERNAL_ERROR, "Could not create local SDP for offer;"
                " cause = %s", cc_cause_name(cause));
        FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
        return (fsmdef_release(fcb, cause, FALSE));
    }

    cause = gsmsdp_encode_sdp_and_update_version(dcb, &msg_body);
    if (cause != CC_CAUSE_OK) {
        ui_create_offer(evCreateOfferError, fcb->state, line, call_id,
            dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INTERNAL_ERROR, "Could not encode local SDP for offer;"
                " cause = %s", cc_cause_name(cause));
        FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
        return (fsmdef_release(fcb, cause, FALSE));
    }

    dcb->local_sdp_complete = TRUE;

    /* Pass offer SDP back to UI */
    ui_create_offer(evCreateOfferSuccess, fcb->state, line, call_id,
        dcb->caller_id.call_instance_id,
        strlib_malloc(msg_body.parts[0].body, -1), PC_NO_ERROR, NULL);
    cc_free_msg_body_parts(&msg_body);

    return (SM_RC_END);
}


/**
 * fsmdef_ev_createanswer
 *
 * Generates Answer SDP
 *
 */
static sm_rcs_t
fsmdef_ev_createanswer (sm_event_t *event) {
    sm_rcs_t            sm_rc;
    fsm_fcb_t           *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t        *dcb = fcb->dcb;
    cc_feature_t        *msg = (cc_feature_t *) event->msg;
    cc_causes_t         cause = CC_CAUSE_NORMAL;
    cc_msgbody_info_t   msg_body;
    line_t              line = msg->line;
    callid_t            call_id = msg->call_id;
    line_t              free_line;
    int                 sdpmode = 0;
    const char          *called_number = "1234";
    cc_causes_t         lsm_rc;
    cc_msgbody_t        *part;
    uint32_t            body_length;
    char                *ufrag = NULL;
    char                *ice_pwd = NULL;
    short               vcm_res;
    session_data_t      *sess_data_p;
    boolean             has_audio;
    boolean             has_video;
    boolean             has_data;
    char                *local_sdp = NULL;
    uint32_t            local_sdp_len = 0;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));
    if (!sdpmode) {
        return (fsmdef_release(fcb, cause, FALSE));
    }

    if (dcb == NULL) {
        FSM_DEBUG_SM(DEB_F_PREFIX"dcb is NULL.",
            DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
        return SM_RC_CLEANUP;
    }

    /* For now, if the local SDP has been set, we don't allow it to be set
       again. This will change when we allow renegotiation of ongoing
       sessions. See bug 840728. */
    if (dcb->local_sdp_complete) {
        FSM_DEBUG_SM(DEB_F_PREFIX"local SDP already created: returning "
        "prevously created SDP.\n", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

        local_sdp = sipsdp_write_to_buf(dcb->sdp->src_sdp, &local_sdp_len);
        if (!local_sdp) {
            ui_create_answer(evCreateAnswerError, fcb->state, line, call_id,
                dcb->caller_id.call_instance_id, strlib_empty(),
                PC_INTERNAL_ERROR, "Could not re-create local SDP for answer");
            FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
            return (fsmdef_release(fcb, cause, FALSE));
        }

        ui_create_answer(evCreateAnswerSuccess, fcb->state, line, call_id,
            dcb->caller_id.call_instance_id,
            strlib_malloc(local_sdp,-1), PC_NO_ERROR, NULL);
        free(local_sdp);
        return (SM_RC_END);
    }

    dcb->inbound = TRUE;

    if (msg->data.session.constraints) {
       gsmsdp_process_cap_constraints(dcb, msg->data.session.constraints);
       fsmdef_free_constraints(msg->data.session.constraints);
       msg->data.session.constraints = 0;
    }

    vcm_res = vcmGetIceParams(dcb->peerconnection, &ufrag, &ice_pwd);
    if (vcm_res) {
    	FSM_DEBUG_SM(DEB_F_PREFIX"vcmGetIceParams returned an error",
            DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
      ui_create_answer(evCreateAnswerError, fcb->state, line, call_id,
          dcb->caller_id.call_instance_id, strlib_empty(),
          PC_INTERNAL_ERROR, "Could not get ICE parameters for answer");
      return (fsmdef_release(fcb, cause, FALSE));
    }

    dcb->ice_ufrag = (char *)cpr_malloc(strlen(ufrag) + 1);
    if (!dcb->ice_ufrag)
        return SM_RC_END;

    sstrncpy(dcb->ice_ufrag, ufrag, strlen(ufrag) + 1);
    free(ufrag);


    dcb->ice_pwd = (char *)cpr_malloc(strlen(ice_pwd) + 1);
    if (!dcb->ice_pwd)
        return SM_RC_END;

    sstrncpy(dcb->ice_pwd, ice_pwd, strlen(ice_pwd) + 1);
    free(ice_pwd);

    vcm_res = vcmGetDtlsIdentity(dcb->peerconnection,
                    dcb->digest_alg, FSMDEF_MAX_DIGEST_ALG_LEN,
                    dcb->digest, FSMDEF_MAX_DIGEST_LEN);

    if (vcm_res) {
        FSM_DEBUG_SM(DEB_F_PREFIX"vcmGetDtlsIdentity returned an error", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
        return SM_RC_END;
    }

    /*
     * Determine what media types are offered, used to create matching local SDP
     * for negotiation.
     */
    gsmsdp_get_offered_media_types(fcb, dcb->sdp, &has_audio, &has_video, &has_data);

    /*
     * The sdp member of the dcb has local and remote sdp
     * this next function fills in the local part
     */
    cause = gsmsdp_create_local_sdp(dcb, TRUE, has_audio, has_video, has_data, FALSE);
    if (cause != CC_CAUSE_OK) {
        ui_create_answer(evCreateAnswerError, fcb->state, line, call_id,
            dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INTERNAL_ERROR, "Could not create local SDP for answer;"
                " cause = %s", cc_cause_name(cause));
        FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
        // Force clean up call without sending release
        return (fsmdef_release(fcb, cause, FALSE));
    }

    /* TODO(ekr@rtfm.com): The second true is because we are acting as if we are
       processing an offer. The first, however, is for an initial offer and we may
       want to set that conditionally. */
    cause = gsmsdp_negotiate_media_lines(fcb, dcb->sdp,
            /* initial_offer */       TRUE,
            /* offer */               TRUE,
            /* notify_stream_added */ FALSE,
            /* create_answer */       TRUE);

    if (cause != CC_CAUSE_OK) {
        ui_create_answer(evCreateAnswerError, fcb->state, line, call_id,
            dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INTERNAL_ERROR, "Could not negotiate media lines; cause = %s",
                cc_cause_name(cause));
        return (fsmdef_release(fcb, cause, FALSE));
    }

    cause = gsmsdp_encode_sdp_and_update_version(dcb, &msg_body);
    if (cause != CC_CAUSE_OK) {
        ui_create_answer(evCreateAnswerError, fcb->state, line, call_id,
            dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INTERNAL_ERROR, "Could not encode SDP for answer; cause = %s",
                cc_cause_name(cause));
        FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
        return (fsmdef_release(fcb, cause, FALSE));
    }

    dcb->local_sdp_complete = TRUE;

    /* Pass SDP back to UI */
    ui_create_answer(evCreateAnswerSuccess, fcb->state, line, call_id,
        dcb->caller_id.call_instance_id,
        strlib_malloc(msg_body.parts[0].body, -1), PC_NO_ERROR, NULL);
    cc_free_msg_body_parts(&msg_body);

    return (SM_RC_END);
}


/**
 * SetLocalDescription
 *
 * Because the PeerConnection relies on receiving either a success or
 * an error callback, there must be no paths that return from this
 * function without first calling ui_set_local_description.
 */
static sm_rcs_t
fsmdef_ev_setlocaldesc(sm_event_t *event) {
    fsm_fcb_t           *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t        *dcb = fcb->dcb;
    cc_feature_t        *msg = (cc_feature_t *) event->msg;
    cc_causes_t         cause = CC_CAUSE_NORMAL;
    int                 action = msg->action;
    string_t            sdp = msg->sdp;
    int                 sdpmode = 0;
    callid_t            call_id = msg->call_id;
    line_t              line = msg->line;
    cc_causes_t         lsm_rc;
    char                *local_sdp = NULL;
    uint32_t            local_sdp_len = 0;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    if (dcb == NULL) {
        FSM_DEBUG_SM(DEB_F_PREFIX"dcb is NULL.",
          DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
        fsm_change_state(fcb, __LINE__, FSMDEF_S_CLOSED);
        ui_set_local_description(evSetLocalDescError, fcb->state, line, call_id,
            0, strlib_empty(),
            PC_INTERNAL_ERROR, "Unrecoverable error: dcb is NULL.");
        return (SM_RC_CLEANUP);
    }

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));
    if (!sdpmode) {
        fsm_change_state(fcb, __LINE__, FSMDEF_S_CLOSED);
        ui_set_local_description(evSetLocalDescError, fcb->state, line, call_id,
            dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INTERNAL_ERROR, "'sdpmode' configuration is false. This should "
            "never ever happen. Run for your lives!");
        return (SM_RC_END);
    }

    if (!dcb->sdp) {
        ui_set_local_description(evSetLocalDescError, fcb->state, line, call_id,
           dcb->caller_id.call_instance_id, strlib_empty(),
           PC_INTERNAL_ERROR, "Setting of local SDP before calling "
           "createOffer or createAnswer is not currently supported.");
        return (SM_RC_END);
    }

    switch (action) {

    case JSEP_OFFER:
        if (fcb->state != FSMDEF_S_STABLE &&
            fcb->state != FSMDEF_S_HAVE_LOCAL_OFFER) {
            ui_set_local_description(evSetLocalDescError, fcb->state, line,
                call_id, dcb->caller_id.call_instance_id, strlib_empty(),
                PC_INVALID_STATE, "Cannot set local offer in state %s",
                fsmdef_state_name(fcb->state));
            return (SM_RC_END);
        }
        /* TODO: Parse incoming SDP and act on it. */
        fsm_change_state(fcb, __LINE__, FSMDEF_S_HAVE_LOCAL_OFFER);
        break;

    case JSEP_ANSWER:
        if (fcb->state != FSMDEF_S_HAVE_REMOTE_OFFER &&
            fcb->state != FSMDEF_S_HAVE_LOCAL_PRANSWER) {
            ui_set_local_description(evSetLocalDescError, fcb->state, line,
            call_id, dcb->caller_id.call_instance_id, strlib_empty(),
                PC_INVALID_STATE, "Cannot set local answer in state %s",
                fsmdef_state_name(fcb->state));
            return (SM_RC_END);
        }
        /* TODO: Parse incoming SDP and act on it. */
        FSM_SET_FLAGS(dcb->msgs_sent, FSMDEF_MSG_CONNECTED);

        cc_call_state(dcb->call_id, dcb->line, CC_STATE_ANSWERED,
                      FSMDEF_CC_CALLER_ID);

        /*
         * Now that we have negotiated the media, time to set up ICE.
         * There also needs to be an ICE check in negotiate_media_lines.
         */
        cause = gsmsdp_install_peer_ice_attributes(fcb);
        if (cause != CC_CAUSE_OK) {
            ui_set_local_description(evSetLocalDescError, fcb->state, line,
            call_id, dcb->caller_id.call_instance_id, strlib_empty(),
                PC_INTERNAL_ERROR, "Could not configure local ICE state"
                " from SDP; cause = %s", cc_cause_name(cause));
            return (SM_RC_END);
        }

        /* taken from fsmdef_ev_connected_ack start rx and tx  */
        cc_call_state(dcb->call_id, dcb->line, CC_STATE_CONNECTED,
                  FSMDEF_CC_CALLER_ID);
        /*
         * If DSP is not able to start rx/tx channels, release the call
         */
        if (dcb->dsp_out_of_resources == TRUE) {
            cc_call_state(fcb->dcb->call_id, fcb->dcb->line,
                CC_STATE_UNKNOWN, NULL);
            ui_set_local_description(evSetLocalDescError, fcb->state, line,
                call_id, dcb->caller_id.call_instance_id, strlib_empty(),
                PC_INTERNAL_ERROR, "Cannot start media channels; cause = %s",
                cc_cause_name(cause));
            return (SM_RC_END);
        }
        /* we may want to use the functionality in the following method
         *  to handle media capability changes, needs discussion
         * fsmdef_transition_to_connected(fcb);
         */
        fsm_change_state(fcb, __LINE__, FSMDEF_S_STABLE);
        break;

    case JSEP_PRANSWER:
        if (fcb->state != FSMDEF_S_HAVE_REMOTE_OFFER &&
            fcb->state != FSMDEF_S_HAVE_LOCAL_PRANSWER) {
            ui_set_local_description(evSetLocalDescError, fcb->state, line,
                call_id, dcb->caller_id.call_instance_id, strlib_empty(),
                PC_INVALID_STATE, "Cannot set local pranswer in state %s",
                fsmdef_state_name(fcb->state));
            return (SM_RC_END);
        }
        ui_set_local_description(evSetLocalDescError, fcb->state, msg->line,
            msg->call_id, dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INTERNAL_ERROR, "Provisional answers are not yet supported");
        return (SM_RC_END);

    default:
        ui_set_local_description(evSetLocalDescError, fcb->state, msg->line,
            msg->call_id, dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INTERNAL_ERROR, "Unknown session description type: %d",action);
        return (SM_RC_END);
    }

    /* Encode the current local SDP structure into a char buffer */
    local_sdp = sipsdp_write_to_buf(dcb->sdp->src_sdp, &local_sdp_len);
    if (!local_sdp) {
        ui_set_local_description(evSetLocalDescError, fcb->state, msg->line,
            msg->call_id, dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INTERNAL_ERROR, "Could not encode local SDP for local "
            "description");
        return (SM_RC_END);
    }

    ui_set_local_description(evSetLocalDescSuccess, fcb->state, msg->line,
        msg->call_id, dcb->caller_id.call_instance_id,
        strlib_malloc(local_sdp,-1), PC_NO_ERROR, NULL);

    free(local_sdp);
    return (SM_RC_END);
}

/**
 * SetRemoteDescription
 *
 * Because the PeerConnection relies on receiving either a success or
 * an error callback, there must be no paths that return from this
 * function without first calling ui_set_remote_description.
 */
static sm_rcs_t
fsmdef_ev_setremotedesc(sm_event_t *event) {
    fsm_fcb_t           *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t        *dcb = fcb->dcb;
    cc_feature_t        *msg = (cc_feature_t *) event->msg;
    cc_causes_t         cause = CC_CAUSE_NORMAL;
    int                 action = msg->action;
    int                 sdpmode = 0;
    callid_t            call_id = msg->call_id;
    line_t              line = msg->line;
    cc_causes_t         lsm_rc;
    cc_msgbody_t        *part;
    uint32_t            body_length;
    cc_msgbody_info_t   msg_body;
    boolean             has_audio;
    boolean             has_video;
    boolean             has_data;
    char                *remote_sdp = 0;
    uint32_t            remote_sdp_len = 0;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    if (dcb == NULL) {
        FSM_DEBUG_SM(DEB_F_PREFIX"dcb is NULL.",
          DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
        fsm_change_state(fcb, __LINE__, FSMDEF_S_CLOSED);
        ui_set_remote_description(evSetRemoteDescError, fcb->state, line,
            call_id, 0, strlib_empty(),
            PC_INTERNAL_ERROR, "Unrecoverable error: dcb is NULL.");
        return (SM_RC_CLEANUP);
    }

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));
    if (!sdpmode) {
        fsm_change_state(fcb, __LINE__, FSMDEF_S_CLOSED);
        ui_set_remote_description(evSetRemoteDescError, fcb->state, line,
            call_id, dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INTERNAL_ERROR, "'sdpmode' configuration is false. This should "
            "never ever happen. Run for your lives!");
        return (SM_RC_END);
    }

    // XXX We don't currently support renegotiation. If the remote SDP
    // has already been set, return an error to the application. This is
    // temporary until Bug 840728 is implemented.
    if (dcb->sdp && dcb->sdp->dest_sdp) {
        FSM_DEBUG_SM(DEB_F_PREFIX"Renegotiation not currently supported.",
                     DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
        ui_set_remote_description(evSetRemoteDescError, fcb->state, line,
            call_id, dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INVALID_STATE, "Renegotiation of session description is not "
            "currently supported. See Bug 840728 for status.");
        return (SM_RC_END);
    }

    cc_initialize_msg_body_parts_info(&msg_body);

    /* !!! NOTE !!! The following code sets up the pointers inside
       msg_body.parts[0] to point directly to the buffers from the
       event->msg structure. While this is more efficient than
       copying them, we must take exceptional care not to call
       cc_free_msg_body_parts() on this particular msg_body, since
       doing so would result in the buffers being freed twice. */

    msg_body.num_parts = 1;
    msg_body.content_type = cc_content_type_SDP;
    part = &msg_body.parts[0];
    body_length = strlen(msg->sdp);
    part->body = msg->sdp;
    part->body_length = body_length;
    part->content_type = cc_content_type_SDP;
    part->content_disposition.required_handling = FALSE;
    part->content_disposition.disposition = cc_disposition_session;
    part->content_id = NULL;

    switch (action) {
    case JSEP_OFFER:
        if (fcb->state != FSMDEF_S_STABLE &&
            fcb->state != FSMDEF_S_HAVE_REMOTE_OFFER) {
            ui_set_remote_description(evSetRemoteDescError, fcb->state, line,
            call_id, dcb->caller_id.call_instance_id, strlib_empty(),
                PC_INVALID_STATE, "Cannot set remote offer in state %s",
                fsmdef_state_name(fcb->state));
            return (SM_RC_END);
        }
        cause = gsmsdp_process_offer_sdp(fcb, &msg_body, TRUE);
        if (cause != CC_CAUSE_OK) {
            ui_set_remote_description(evSetRemoteDescError, fcb->state, line,
            call_id, dcb->caller_id.call_instance_id, strlib_empty(),
                PC_INTERNAL_ERROR, "Could not process offer SDP; "
                "cause = %s", cc_cause_name(cause));
            return (SM_RC_END);
        }

        /*
         * Determine what media types are offered, used to create matching
         * local SDP for negotiation.
         */
        gsmsdp_get_offered_media_types(fcb, dcb->sdp, &has_audio,
            &has_video, &has_data);

        /*
         * The sdp member of the dcb has local and remote sdp
         * this next function fills in the local part
         */
        cause = gsmsdp_create_local_sdp(dcb, TRUE, has_audio, has_video,
            has_data, FALSE);
        if (cause != CC_CAUSE_OK) {
            ui_set_remote_description(evSetRemoteDescError, fcb->state, line,
              call_id, dcb->caller_id.call_instance_id, strlib_empty(),
              PC_INTERNAL_ERROR, "Could not create local SDP; cause = %s",
              cc_cause_name(cause));
            FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
            // Force clean up call without sending release
            return (fsmdef_release(fcb, cause, FALSE));
        }

        cause = gsmsdp_negotiate_media_lines(fcb, dcb->sdp,
            TRUE, TRUE, TRUE, FALSE);
        if (cause != CC_CAUSE_OK) {
            ui_set_remote_description(evSetRemoteDescError, fcb->state, line,
            call_id, dcb->caller_id.call_instance_id, strlib_empty(),
              PC_INTERNAL_ERROR, "Could not negotiate media lines; cause = %s",
              cc_cause_name(cause));
            return (fsmdef_release(fcb, cause, FALSE));
        }

        /* Now that the SDP is digested we need to sanity check
           for ICE parameters */
        cause = gsmsdp_check_ice_attributes_exist(fcb);
        if (cause != CC_CAUSE_OK) {
            ui_set_remote_description(evSetRemoteDescError, fcb->state, line,
            call_id, dcb->caller_id.call_instance_id, strlib_empty(),
              PC_INTERNAL_ERROR, "ICE attributes missing; cause = %s",
              cc_cause_name(cause));
            return (SM_RC_END);
        }

        gsmsdp_clean_media_list(dcb);

        fsm_change_state(fcb, __LINE__, FSMDEF_S_HAVE_REMOTE_OFFER);
        break;

    case JSEP_ANSWER:
        if (fcb->state != FSMDEF_S_HAVE_LOCAL_OFFER &&
            fcb->state != FSMDEF_S_HAVE_REMOTE_PRANSWER) {
            ui_set_remote_description(evSetRemoteDescError, fcb->state, line,
            call_id, dcb->caller_id.call_instance_id, strlib_empty(),
                PC_INVALID_STATE, "Cannot set remote answer in state %s",
                fsmdef_state_name(fcb->state));
            return (SM_RC_END);
        }
        cause = gsmsdp_negotiate_answer_sdp(fcb, &msg_body);
        if (cause != CC_CAUSE_OK) {
            ui_set_remote_description(evSetRemoteDescError, fcb->state, line,
                call_id, dcb->caller_id.call_instance_id, strlib_empty(),
                PC_INTERNAL_ERROR, "Could not negotiate answer SDP; cause = %s",
                cc_cause_name(cause));
            return (SM_RC_END);
        }

        /*
         * Now that we have negotiated the media, time to set up ICE.
         * There also needs to be an ICE check in negotiate_media_lines.
         */
        cause = gsmsdp_install_peer_ice_attributes(fcb);
        if (cause != CC_CAUSE_OK) {
            ui_set_remote_description(evSetRemoteDescError, fcb->state, line,
                call_id, dcb->caller_id.call_instance_id, strlib_empty(),
                PC_INTERNAL_ERROR, "Could not configure local ICE state"
                " from SDP; cause = %s", cc_cause_name(cause));
            return (SM_RC_END);
        }

        cc_call_state(dcb->call_id, dcb->line, CC_STATE_CONNECTED,
            FSMDEF_CC_CALLER_ID);

        /* we may want to use the functionality in the following method
         * to handle media capability changes, needs discussion
         * fsmdef_transition_to_connected(fcb);
         */

        fsm_change_state(fcb, __LINE__, FSMDEF_S_STABLE);
        break;

    case JSEP_PRANSWER:
        if (fcb->state != FSMDEF_S_HAVE_LOCAL_OFFER &&
            fcb->state != FSMDEF_S_HAVE_REMOTE_PRANSWER) {
            ui_set_remote_description(evSetRemoteDescError, fcb->state, line,
                call_id, dcb->caller_id.call_instance_id, strlib_empty(),
                PC_INVALID_STATE, "Cannot set remote pranswer in state %s",
                fsmdef_state_name(fcb->state));
            return (SM_RC_END);
        }
        ui_set_local_description(evSetLocalDescError, fcb->state, msg->line,
            msg->call_id, dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INTERNAL_ERROR, "Provisional answers are not yet supported");
        return (SM_RC_END);

    default:
        ui_set_local_description(evSetLocalDescError, fcb->state, msg->line,
            msg->call_id, dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INTERNAL_ERROR, "Unknown session description type: %d",action);
        return (SM_RC_END);
    }

    /* For the sake of accuracy, we regenerate the SDP text from our parsed
       version: if we have any local variation in how we've interpreted the
       received SDP, then remoteDescription will reflect that variation. In
       practice, this shouldn't happen; but, if it does, at least this will
       allow the WebRTC application to figure out what's going on. */

    remote_sdp = sipsdp_write_to_buf(dcb->sdp->dest_sdp, &remote_sdp_len);

    if (!remote_sdp) {
        ui_set_remote_description(evSetRemoteDescError, fcb->state, line,
        call_id, dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INTERNAL_ERROR, "Could not serialize remote description;"
            " cause = %s",  cc_cause_name(cause));
        return (SM_RC_END);
    }

    ui_set_remote_description(evSetRemoteDescSuccess, fcb->state, line, call_id,
        dcb->caller_id.call_instance_id, strlib_malloc(remote_sdp,-1),
        PC_NO_ERROR, NULL);

    free(remote_sdp);

    return (SM_RC_END);
}


static sm_rcs_t
fsmdef_ev_setpeerconnection(sm_event_t *event) {
    fsm_fcb_t           *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t        *dcb = fcb->dcb;
    cc_causes_t         cause = CC_CAUSE_NORMAL;
    cc_feature_t        *msg = (cc_feature_t *) event->msg;
    callid_t            call_id = msg->call_id;
    int                 sdpmode = 0;
    line_t              line = msg->line;
    cc_causes_t         lsm_rc;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));
    if (!sdpmode) {
        FSM_DEBUG_SM(DEB_F_PREFIX"sdpmode is false; cannot set peerconnection.",
            DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
        return (SM_RC_END);
    }

    if (!msg)
      return SM_RC_END;

    if (!msg->data_valid)
      return SM_RC_END;

    if (dcb == NULL) {
      dcb = fsmdef_get_new_dcb(call_id);
      if (dcb == NULL) {
        return SM_RC_ERROR;
      }

      lsm_rc = lsm_get_facility_by_line(call_id, line, FALSE, dcb);
      if (lsm_rc != CC_CAUSE_OK) {
          FSM_DEBUG_SM(DEB_F_PREFIX"lsm_get_facility_by_line failed.",
                       DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
          return SM_RC_END;
      }

      fsmdef_init_dcb(dcb, call_id, FSMDEF_CALL_TYPE_NONE, NULL, line, fcb);

      fsm_set_fcb_dcbs(dcb);
    }

    PR_ASSERT(strlen(msg->data.pc.pc_handle) < PC_HANDLE_SIZE);
    sstrncpy(dcb->peerconnection, msg->data.pc.pc_handle,
             sizeof(dcb->peerconnection));
    dcb->peerconnection_set = TRUE;

    FSM_DEBUG_SM(DEB_F_PREFIX"Setting peerconnection handle for (%d/%d) to %s",
                 DEB_F_PREFIX_ARGS(FSM, __FUNCTION__),
                 line, call_id, dcb->peerconnection);

    /* WebRTC connections always start in state "STABLE". */
    fsm_change_state(fcb, __LINE__, FSMDEF_S_STABLE);

    return (SM_RC_END);
}


static sm_rcs_t
fsmdef_ev_addstream(sm_event_t *event) {
    fsm_fcb_t           *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t        *dcb = fcb->dcb;
    cc_causes_t         cause = CC_CAUSE_NORMAL;
    cc_feature_t        *msg = (cc_feature_t *) event->msg;
    int                 sdpmode = 0;
    cc_causes_t         lsm_rc;
    cc_msgbody_t        *part;
    uint32_t            body_length;
    cc_msgbody_info_t   msg_body;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));
    if (sdpmode == FALSE) {
        return (SM_RC_END);
    }

    if (dcb == NULL) {
        FSM_DEBUG_SM(DEB_F_PREFIX"dcb is NULL.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
        return SM_RC_CLEANUP;
    }

    /*
     * This is temporary code to allow configuration of the two
     * default streams. When multiple streams > 2 are supported this
     * will be re-implemented.
     */
    if (msg->data.track.media_type == VIDEO) {
        dcb->media_cap_tbl->cap[CC_VIDEO_1].enabled = TRUE;
        dcb->media_cap_tbl->cap[CC_VIDEO_1].support_direction = SDP_DIRECTION_SENDRECV;
        dcb->media_cap_tbl->cap[CC_VIDEO_1].pc_stream = msg->data.track.stream_id;
        dcb->media_cap_tbl->cap[CC_VIDEO_1].pc_track = msg->data.track.track_id;
    } else if (msg->data.track.media_type == AUDIO) {
        dcb->media_cap_tbl->cap[CC_AUDIO_1].enabled = TRUE;
        dcb->media_cap_tbl->cap[CC_AUDIO_1].support_direction = SDP_DIRECTION_SENDRECV;
        dcb->media_cap_tbl->cap[CC_AUDIO_1].pc_stream = msg->data.track.stream_id;
        dcb->media_cap_tbl->cap[CC_AUDIO_1].pc_track = msg->data.track.track_id;
    } else if (msg->data.track.media_type == DATA) {
        dcb->media_cap_tbl->cap[CC_DATACHANNEL_1].enabled = TRUE;
        dcb->media_cap_tbl->cap[CC_DATACHANNEL_1].support_direction = SDP_DIRECTION_SENDRECV;
        dcb->media_cap_tbl->cap[CC_DATACHANNEL_1].pc_stream = msg->data.track.stream_id;
        dcb->media_cap_tbl->cap[CC_DATACHANNEL_1].pc_track = msg->data.track.track_id;
    } else {
        return (SM_RC_END);
    }

    return (SM_RC_END);
}

static sm_rcs_t
fsmdef_ev_removestream(sm_event_t *event) {
    fsm_fcb_t           *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t        *dcb = fcb->dcb;
    cc_causes_t         cause = CC_CAUSE_NORMAL;
    cc_feature_t        *msg = (cc_feature_t *) event->msg;
    int                 sdpmode = 0;
    cc_causes_t         lsm_rc;
    cc_msgbody_t        *part;
    uint32_t            body_length;
    cc_msgbody_info_t   msg_body;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));
    if (sdpmode == FALSE) {

        return (SM_RC_END);
    }

    if (dcb == NULL) {
        FSM_DEBUG_SM(DEB_F_PREFIX"dcb is NULL.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
        return SM_RC_CLEANUP;
    }

    /*
     * This is temporary code to allow configuration of the two
     * default streams. When multiple streams > 2 are supported this
     * will be re-implemented.
     */
    if (msg->data.track.media_type == AUDIO) {
        PR_ASSERT(dcb->media_cap_tbl->cap[CC_AUDIO_1].enabled);
        dcb->media_cap_tbl->cap[CC_AUDIO_1].support_direction = SDP_DIRECTION_RECVONLY;
        dcb->video_pref = SDP_DIRECTION_SENDRECV;
    } else if (msg->data.track.media_type == VIDEO) {
        PR_ASSERT(dcb->media_cap_tbl->cap[CC_VIDEO_1].enabled);
        dcb->media_cap_tbl->cap[CC_VIDEO_1].support_direction = SDP_DIRECTION_RECVONLY;
    } else {
        return (SM_RC_END);
    }

    return (SM_RC_END);
}

static sm_rcs_t
fsmdef_ev_addcandidate(sm_event_t *event) {
    fsm_fcb_t           *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t        *dcb = fcb->dcb;
    cc_causes_t         cause = CC_CAUSE_NORMAL;
    cc_feature_t        *msg = (cc_feature_t *) event->msg;
    int                 sdpmode = 0;
    short               vcm_res;
    uint16_t            level;
    line_t              line = msg->line;
    callid_t            call_id = msg->call_id;
    char                *remote_sdp = 0;
    uint32_t            remote_sdp_len = 0;
    char                *candidate = 0;
    char                candidate_tmp[CANDIDATE_SIZE];

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    if (!dcb) {
        FSM_DEBUG_SM(DEB_F_PREFIX"dcb is NULL.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
        ui_ice_candidate_add(evAddIceCandidateError, fcb->state, line, call_id,
            0, strlib_empty(), PC_INTERNAL_ERROR, "DCB has not been created.");
        return SM_RC_CLEANUP;
    }

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));
    if (sdpmode == FALSE) {
        ui_ice_candidate_add(evAddIceCandidateError, fcb->state, line, call_id,
            dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INTERNAL_ERROR, "'sdpmode' configuration is false. This should "
            "never ever happen. Run for your lives!");
        return (SM_RC_END);
    }

    if (!dcb->sdp) {
        FSM_DEBUG_SM(DEB_F_PREFIX"dcb->sdp is NULL. Has the "
            "remote description been set yet?\n",
            DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

        ui_ice_candidate_add(evAddIceCandidateError, fcb->state, line, call_id,
            dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INVALID_STATE, "Cannot add remote ICE candidates before "
                              "setting remote SDP.");

        return SM_RC_END;
    }

    /* Perform level lookup based on mid value */
    /* comment until mid is properly updated
    cause = gsmsdp_find_level_from_mid(dcb, (const char *)msg->data.candidate.mid, &level);
    */

    /* XXX: Bug 833043. Need to sanity check that this is actually a valid candidate */
    /* Update remote SDP with new candidate information */
    candidate = (char *)msg->data.candidate.candidate;
    if (!strncasecmp(candidate, "a=", 2)) {
      char *cr;

      /* Strip off Chrome's inappropriate leading a=.
         TODO(ekr@rtfm.com): remove once Chrome conforms to the specification
         or revisit if the specification changes. */
      sstrncpy(candidate_tmp, candidate + 2, sizeof(candidate_tmp));

      /* Strip off \r\n if it exists */
      cr = strchr(candidate_tmp, '\r');
      if (cr)
        *cr = '\0';

      candidate = candidate_tmp;
    }

    level = msg->data.candidate.level;
    gsmsdp_set_ice_attribute (SDP_ATTR_ICE_CANDIDATE, level,
      dcb->sdp->dest_sdp, candidate);

    vcm_res = vcmSetIceCandidate(dcb->peerconnection, candidate, msg->data.candidate.level);
    if(vcm_res) {
        FSM_DEBUG_SM(DEB_F_PREFIX"failure setting ice candidate.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));
    }

    /* Serialize the updated SDP and inform the PeerConnection of the
       new SDP contents. */

    remote_sdp = sipsdp_write_to_buf(dcb->sdp->dest_sdp, &remote_sdp_len);

    if (!remote_sdp) {
        ui_ice_candidate_add(evAddIceCandidateError, fcb->state, line, call_id,
            dcb->caller_id.call_instance_id, strlib_empty(),
            PC_INTERNAL_ERROR, "Could not serialize new SDP after adding ICE "
            "candidate.");
        return (SM_RC_END);
    }

    ui_ice_candidate_add(evAddIceCandidate, fcb->state, line, call_id,
        dcb->caller_id.call_instance_id, strlib_malloc(remote_sdp,-1),
        PC_NO_ERROR, NULL);

    free(remote_sdp);
    return (SM_RC_END);
}

static void
fsmdef_check_active_feature (fsmdef_dcb_t *dcb, cc_features_t ftr_id)
{
    if ((dcb) && (dcb->active_feature != ftr_id)) {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_FTR_REQ_ACT),
                     dcb->call_id, dcb->line,
                     cc_feature_name(ftr_id),
                     cc_feature_name(dcb->active_feature));
        lsm_ui_display_notify(INDEX_STR_KEY_NOT_ACTIVE, NO_FREE_LINES_TIMEOUT);
    }
}

static sm_rcs_t
fsmdef_ev_idle_feature (sm_event_t *event)
{
    static const char  fname[]    = "fsmdef_ev_idle_feature";
    fsm_fcb_t         *fcb     = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t      *dcb     = fcb->dcb;
    cc_feature_t      *msg     = (cc_feature_t *) event->msg;
    cc_srcs_t          src_id  = msg->src_id;
    cc_features_t      ftr_id  = msg->feature_id;
    cc_feature_data_t *data    = &(msg->data);
    line_t             line    = msg->line;
    cc_causes_t        cause   = CC_CAUSE_NORMAL;
    callid_t           call_id = fcb->call_id;
    boolean            expline;
    sm_rcs_t           sm_rc   = SM_RC_END;
    fsmcnf_ccb_t      *ccb;
    fsmxfr_xcb_t      *xcb;
    char              *global_call_id = NULL;

    fsm_sm_ftr(ftr_id, src_id);

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    switch (src_id) {
    case CC_SRC_UI:
    case CC_SRC_GSM:
        switch (ftr_id) {
        case CC_FEATURE_UPD_SESSION_MEDIA_CAP:
             if (dcb) {
                 dcb->video_pref = data->caps.support_direction;
             }
             break;
        case CC_FEATURE_CFWD_ALL:
            if (fsmdef_is_feature_uri_configured(ftr_id) == FALSE) {
                fsm_display_feature_unavailable();
                fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
                break;
            }

            // handle cfwd event for ccm and non-ccm cases
            // process feature event only if no other active feature
            if ((dcb->active_feature == CC_FEATURE_NONE) &&
                (fsmdef_get_connected_call() == NULL)) {
                dcb->active_feature = ftr_id;
                (void) fsmdef_process_cfwd_softkey_event(event);
            } else {
                fsmdef_check_active_feature(dcb, ftr_id);
                fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            }
            break;
        case CC_FEATURE_NEW_CALL:

            /* fetch the global_call_id from feature data */
            global_call_id = data->newcall.global_call_id;

           /*
             * Set the expanded parameter. This parameter is used when
             * requesting a free line. A transfer can request that the line
             * availability be increased by one to account for the third
             * instance of a line to become available for transfers.
             */
            if (data != NULL) {
                ccb = fsmcnf_get_ccb_by_call_id(call_id);
                xcb = fsmxfr_get_xcb_by_call_id(call_id);
                if ((ccb != NULL) || (xcb != NULL)) {
                    expline = TRUE;
                } else {
                    expline = FALSE;
                }
            } else {
                expline = FALSE;
            }

            /*
             * Get a new outgoing call context if we have not already
             * grabbed one.
             */
            if (fcb->dcb == NULL) {
                cause = fsm_get_new_outgoing_call_context(call_id, line, fcb,
                                                          expline);
                switch (cause) {
                case CC_CAUSE_OK:
                    break;

                case CC_CAUSE_NO_RESOURCE:
                    GSM_ERR_MSG("%s No Resource! Return SM_RC_CLEANUP.", fname);
                    return (SM_RC_CLEANUP);

                default:
                    /*
                     * No free lines.
                     */
                    fsm_display_no_free_lines();

                    /*
                     * Send an endcall message. This behaviour is different
                     * than an offhook or line event because the new_call
                     * feature may have been generated by a transfer (as the
                     * consultation call) and will need this end_call message
                     * so that it can cleanup the transfer. The offhook can
                     * only come from the UI so it can just be cleaned up here
                     * without regard for a transfer.
                     */
                    fsmdef_end_call(fcb->dcb, cause);

                    return (SM_RC_END);
                }

                dcb = fcb->dcb;
                /*
                 * Let Dialog Manager know that there is OFFHOOK event
                 */
                fsmdef_notify_hook_event(fcb, CC_MSG_OFFHOOK, global_call_id,
                                         data->newcall.prim_call_id,
                                         data->newcall.hold_resume_reason,
                                         CC_MONITOR_NONE,CFWDALL_NONE);
            }


            /*
             * The user is attempting to start a new call on a specific line:
             * 1. need to place the connected call (if there is one) on hold,
             * 2. clear any outgoing ringing calls,
             * 3. initiate this call.
             */
            if (fsmdef_wait_to_start_new_call(TRUE, CC_SRC_GSM, call_id, line,
                        ftr_id, data)) {
                return (SM_RC_END);
            }

            //lsm_set_active_call_id(call_id);

            cc_call_state(dcb->call_id, dcb->line, CC_STATE_OFFHOOK,
                          FSMDEF_CC_CALLER_ID);

            if ( data->newcall.cause == CC_CAUSE_CONF ||
                 data->newcall.cause == CC_CAUSE_XFER_LOCAL ) {
              /* suppress stutter dial tone for conf and transfer features */
              fsmdef_call_cc_state_dialing(dcb, TRUE);
            } else {
              fsmdef_call_cc_state_dialing(dcb, FALSE);
            }

            switch (data->newcall.cause) {
            case CC_CAUSE_XFER_REMOTE:
                /*
                 * This newcall feature is really the consultation part of
                 * a local transfer that has been transferred by the
                 * consultation call, so proceed as though this is a
                 * dialstring call since we already have the called_number.
                 */
                if (data->newcall.redirect.redirects[0].number[0] != '\0') {
                    sm_rc = fsmdef_dialstring(fcb, data->newcall.dialstring,
                                              &(data->newcall.redirect), FALSE,
                                              NULL);

                } else if (data->newcall.redirect.redirects[0].redirect_reason
                               == CC_REDIRECT_REASON_DEFLECTION) {
                    /*
                     * CC_REDIRECT_REASON_DEFLECTION shows that transferee is
                     * going to initiate a new call for replacing the call leg
                     * between transferor and target.
                     */

                    memset(data->newcall.redirect.redirects[0].number, 0,
                           sizeof(CC_MAX_DIALSTRING_LEN));
                    sm_rc = fsmdef_dialstring(fcb, data->newcall.dialstring,
                                              &(data->newcall.redirect), FALSE,
                                              NULL);

                } else {
                    sm_rc =
                        fsmdef_dialstring(fcb, data->newcall.dialstring, NULL,
                                          FALSE, NULL);
                }

                return (sm_rc);

            case CC_CAUSE_REDIRECT:
                sm_rc = fsmdef_dialstring(fcb, data->newcall.dialstring,
                                          &(data->newcall.redirect), FALSE,
                                          NULL);
                return (sm_rc);

            case CC_CAUSE_XFER_BY_REMOTE:

                /* CC_REDIRECT_REASON_DEFLECTION shows that transferee is
                 * going to initiate a new call for replacing the call leg
                 * between transferor and target.
                 */

                memset(data->newcall.redirect.redirects[0].number, 0,
                       sizeof(CC_MAX_DIALSTRING_LEN));
                sm_rc = fsmdef_dialstring(fcb, data->newcall.dialstring,
                                          &(data->newcall.redirect), FALSE,
                                          NULL);
                return (sm_rc);

            default:
                fsm_change_state(fcb, __LINE__, FSMDEF_S_COLLECT_INFO);

                return (SM_RC_END);
            }

        case CC_FEATURE_END_CALL:
            cause = fsmdef_get_cause(msg->data_valid, data);

            /*
             * There has to be a dcb to process this event.
             */
            if (fcb->dcb == NULL) {
                // commented out following line due to klocwork error
                // call is dereferencing fcb->dcb when it is NULL (sorry, can't do that!)
                // cc_call_state(fcb->dcb->call_id, fcb->dcb->line, CC_STATE_UNKNOWN, NULL);
                return (SM_RC_CLEANUP);
            }

            if (dcb->call_type == FSMDEF_CALL_TYPE_INCOMING ||
                dcb->call_type == FSMDEF_CALL_TYPE_FORWARD) {
                dcb->send_release = TRUE;
            }

            return (fsmdef_release(fcb, cause, dcb->send_release));

        default:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);

            break;
        }
        break;

    default:
        fsmdef_sm_ignore_src(fcb, __LINE__, src_id);

    }                           /* switch (src_id) { */

    return (sm_rc);
}

sm_rcs_t
fsmdef_offhook (fsm_fcb_t *fcb, cc_msgs_t msg_id, callid_t call_id,
                line_t line, const char *dial_string,
                sm_event_t *event, char *global_call_id,
                callid_t prim_call_id, cc_hold_resume_reason_e consult_reason,
                monitor_mode_t monitor_mode)
{
    boolean     wait  = FALSE;
    boolean     wait2 = FALSE;
    boolean     wait3 = FALSE;
    cc_causes_t cause;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    /*
     * Get a new outgoing call context if we have not already
     * grabbed one.
     */
    if (fcb->dcb == NULL) {
        cause = fsm_get_new_outgoing_call_context(call_id, line, fcb, FALSE);
        switch (cause) {
        case CC_CAUSE_OK:
            break;

        default:
            /*
             * No free lines
             */
            fsm_display_no_free_lines();

            if (fsmdef_get_connected_call() != NULL) {
                lsm_speaker_mode(ON);
            } else {
                lsm_speaker_mode(OFF);
            }
            return (SM_RC_CLEANUP);
        }

        /*
         * Let Dialog Manager know that there is OFFHOOK event
         */
        fsmdef_notify_hook_event(fcb, CC_MSG_OFFHOOK, global_call_id,
                                 prim_call_id, consult_reason, monitor_mode,CFWDALL_NONE);
    }


    /*
     * The user is attempting to start a new call on a specific line:
     * 1. need to place the connected call (if there is one) on hold,
     * 2. clear any outgoing ringing calls, or calls are in reorder/busy state
     * 3. initiate this call.
     */
    fsmdef_find_and_hold_connected_call(call_id, &wait, CC_SRC_GSM);

    fsmdef_find_and_handle_ring_connecting_releasing_calls(call_id, &wait2);

    fsmdef_clear_preserved_calls(&wait3);

    /*
     * Requeue the message if we need to wait for the connected line to
     * hold
     */
    if ((wait == TRUE) || (wait2 == TRUE) || (wait3 == TRUE)) {
        switch (msg_id) {
        case CC_MSG_OFFHOOK:
            cc_int_offhook(CC_SRC_GSM, CC_SRC_GSM, prim_call_id, consult_reason,
                           call_id, line, global_call_id, monitor_mode,CFWDALL_NONE);
            break;

        case CC_MSG_LINE:
            cc_int_line(CC_SRC_GSM, CC_SRC_GSM, call_id, line);
            break;

        case CC_MSG_DIALSTRING:
            cc_int_dialstring(CC_SRC_GSM, CC_SRC_GSM, call_id, line,
                              dial_string, global_call_id, monitor_mode);
            break;

        case CC_MSG_FEATURE:
            if (dial_string != NULL) {
                cc_int_dialstring(CC_SRC_GSM, CC_SRC_GSM, call_id, line,
                                  dial_string, global_call_id, monitor_mode);
                break;
            }

            /*FALLTHROUGH*/
        default:
            cc_call_state(fcb->dcb->call_id, fcb->dcb->line, CC_STATE_UNKNOWN,
                          NULL);
            return (SM_RC_CLEANUP);
        }

        return (SM_RC_END);
    }

    //lsm_set_active_call_id(call_id);

    return (SM_RC_SUCCESS);
}


static sm_rcs_t
fsmdef_ev_idle_offhook (sm_event_t *event)
{
    fsm_fcb_t    *fcb = (fsm_fcb_t *) event->data;
    cc_offhook_t *msg = (cc_offhook_t *) event->msg;
    fsmdef_dcb_t *dcb;
    sm_rcs_t      sm_rc;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    sm_rc = fsmdef_offhook(fcb, msg->msg_id, msg->call_id, msg->line, NULL,
                           event, msg->global_call_id, msg->prim_call_id,
                           msg->hold_resume_reason, msg->monitor_mode);

    if (sm_rc != SM_RC_SUCCESS) {
        return (sm_rc);
    }

    dcb = fcb->dcb;

    cc_call_state(dcb->call_id, dcb->line, CC_STATE_OFFHOOK,
                  FSMDEF_CC_CALLER_ID);

    fsmdef_call_cc_state_dialing(dcb, FALSE);

    fsm_change_state(fcb, __LINE__, FSMDEF_S_COLLECT_INFO);

    return (SM_RC_END);
}


static sm_rcs_t
fsmdef_ev_idle_dialstring (sm_event_t *event)
{
    fsm_fcb_t       *fcb = (fsm_fcb_t *) event->data;
    cc_dialstring_t *msg = (cc_dialstring_t *) event->msg;
    fsmdef_dcb_t    *dcb;
    cc_action_data_t data;
    sm_rcs_t         sm_rc;
    cc_call_info_t   call_info;
    cc_call_info_t  *call_info_p = NULL;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    sm_rc = fsmdef_offhook(fcb, msg->msg_id, msg->call_id, msg->line,
                           msg->dialstring, event, msg->g_call_id,
                           CC_NO_CALL_ID, CC_REASON_NONE, msg->monitor_mode);

    if (sm_rc != SM_RC_SUCCESS) {
        return (sm_rc);
    }

    dcb = fcb->dcb;

    if (msg->dialstring) {
        lsm_set_lcb_dialed_str_flag(dcb->call_id);
    }
    cc_call_state(dcb->call_id, dcb->line, CC_STATE_OFFHOOK,
                  FSMDEF_CC_CALLER_ID);

    data.tone.tone = VCM_INSIDE_DIAL_TONE;
    (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_STOP_TONE, &data);

    dcb->send_release = TRUE;

    /* Set call_info with global call_id, sent in the Initcallreq,
     * If this call is not because Initcallreq, then don't set call_info
     * Also, if this is a monitor call, add the mode to the call_info
     */
    if (msg->g_call_id != NULL) {

        call_info.type = CC_FEAT_INIT_CALL;
        call_info.data.initcall.monitor_mode = msg->monitor_mode;
        sstrncpy(call_info.data.initcall.gcid, msg->g_call_id, CC_GCID_LEN);

        call_info_p = &call_info;
    }

    if ( strncmp(CISCO_BLFPICKUP_STRING, msg->dialstring, strlen(CISCO_BLFPICKUP_STRING)) == 0 ) {
        dcb->log_disp = CC_CALL_LOG_DISP_RCVD;
    }

    sm_rc = fsmdef_dialstring(fcb, msg->dialstring, NULL, FALSE, call_info_p);

    return (sm_rc);
}

static sm_rcs_t
fsmdef_ev_session_audit (sm_event_t *event)
{
    static const char  fname[]    = "fsmdef_ev_session_audit";
    fsm_fcb_t          *fcb       = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t       *dcb       = fcb->dcb;
    cc_audit_sdp_req_t *audit_msg = (cc_audit_sdp_req_t *) event->msg;
    cc_msgbody_info_t   msg_body;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    if (gsmsdp_encode_sdp_and_update_version(dcb, &msg_body) != CC_CAUSE_OK) {
        /*
         * Failed to encode our local sdp. Send ack to SIP stack with
         * no message body. SIP stack will include previously send
         * SDP in response to session audit request.
         */
        FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));

        cc_int_audit_sdp_ack(CC_SRC_GSM, CC_SRC_SIP, audit_msg->call_id,
                             audit_msg->line, NULL);
    } else {
        cc_int_audit_sdp_ack(CC_SRC_GSM, CC_SRC_SIP, audit_msg->call_id,
                             audit_msg->line, &msg_body);
    }

    /*
     * If we are currently performing a spoofed ringout and the current session audit
     * does not indicate that we should continue to do so, go back to connected state.
     * But only change to connected state if not locally held.
     */
    if (dcb->spoof_ringout_applied &&
        !dcb->spoof_ringout_requested) {

        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_CLR_SPOOF_APPLD),
                     dcb->call_id, dcb->line, fname);

        if ((fcb->state != FSMDEF_S_HOLDING) &&
            (fcb->state != FSMDEF_S_HOLD_PENDING)) {
            /*
             * If is at least one media entry that is not in loally held
             * then go to connected state.
             */
            dcb->spoof_ringout_applied = FALSE;
            cc_call_state(dcb->call_id, dcb->line, CC_STATE_CONNECTED,
                          FSMDEF_CC_CALLER_ID);
        }
    }

    return (SM_RC_SUCCESS);
}

static sm_rcs_t
fsmdef_ev_collectinginfo_release (sm_event_t *event)
{
    fsm_fcb_t          *fcb       = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t       *dcb       = fcb->dcb;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

   fsmdef_set_call_info_cc_call_state(dcb, CC_STATE_CALL_FAILED, CC_CAUSE_INVALID_NUMBER);

    // Start onhook timer
    if ( dcb->err_onhook_tmr) {
        (void) cprDestroyTimer(dcb->err_onhook_tmr);
    }
    dcb->err_onhook_tmr = cprCreateTimer("Error Onhook",
                                         GSM_ERROR_ONHOOK_TIMER,
                                         TIMER_EXPIRATION,
                                         gsm_msgq);
    if (dcb->err_onhook_tmr == NULL) {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_TMR_CREATE_FAILED),
                     dcb->call_id, dcb->line, "", "Error Onhook");

        return (SM_RC_CLEANUP);
    }

    if (cprStartTimer(dcb->err_onhook_tmr,
                      FSMDEF_ERR_ONHOOK_TMR_SECS * 1000,
                      (void *)(long)dcb->call_id) == CPR_FAILURE) {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_TMR_START_FAILED),
                     dcb->call_id, dcb->line, "",
                     "Error Onhook", cpr_errno);
		return (SM_RC_CLEANUP);
    }

    return (SM_RC_END);
}


static sm_rcs_t
fsmdef_ev_collectinginfo_feature (sm_event_t *event)
{
    fsm_fcb_t       *fcb    = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t    *dcb    = fcb->dcb;
    cc_feature_t    *msg    = (cc_feature_t *) event->msg;
    cc_srcs_t        src_id = msg->src_id;
    cc_features_t    ftr_id = msg->feature_id;
    cc_action_data_t data;
    sm_rcs_t         sm_rc = SM_RC_END;
    cc_causes_t      cause;
    cc_feature_data_t *feature_data = &(msg->data);

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    fsm_sm_ftr(ftr_id, src_id);

    switch (msg->feature_id) {
    case CC_FEATURE_UPD_SESSION_MEDIA_CAP:
        dcb->video_pref = feature_data->caps.support_direction;
        break;
    case CC_FEATURE_END_CALL:
        cause = fsmdef_get_cause(msg->data_valid, &(msg->data));
        if (fcb->state == FSMDEF_S_KPML_COLLECT_INFO) {
            /* Clean up and send release */
            return (fsmdef_release(fcb, cause, TRUE));
        }
        else {
            /* Clean up without sending release */
            return (fsmdef_release(fcb, cause, FALSE));
        }

    case CC_FEATURE_NUMBER:
    case CC_FEATURE_URL:
        dcb->dial_mode = ((msg->feature_id == CC_FEATURE_NUMBER) ?
                          (DIAL_MODE_NUMERIC) : (DIAL_MODE_URL));

        data.dial_mode.mode = dcb->dial_mode;
        data.dial_mode.digit_cnt = dcb->digit_cnt;
        (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_DIAL_MODE,
                             &data);

        break;

    case CC_FEATURE_CALLINFO:
        fsmdef_update_callinfo(fcb, msg);
        /*
         * lsm_set_lcb_prevent_ringing() will check if there is a RINGIN call
         * with the same GCID. If so, it will set a flag to prevent ringing.
         */
        lsm_set_lcb_prevent_ringing(dcb->call_id);
        break;

    case CC_FEATURE_SELECT:
        fsmdef_select_invoke(dcb, feature_data);
        return (SM_RC_END);

    case CC_FEATURE_CFWD_ALL:
        if (fsmdef_is_feature_uri_configured(msg->feature_id) == FALSE) {
            fsm_set_call_status_feature_unavailable(dcb->call_id, dcb->line);

            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;
        }

        // handle cfwd event for ccm and non-ccm cases
        // process feature event only if no other active feature
        if (dcb->active_feature == CC_FEATURE_NONE) {
            dcb->active_feature = ftr_id;
            (void) fsmdef_process_cfwd_softkey_event(event);
         } else {
            fsmdef_check_active_feature(dcb, ftr_id);
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
        }
        break;

    default:
        dcb->active_feature = CC_FEATURE_NONE;
        fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);

        break;
    }

    return (sm_rc);
}

/*
 *  Function: fsmdef_ev_digit_begin
 *
 *  Parameters: event
 *
 *  Description: This function is called each time a digit is
 *    received from the platform code.  Currently, the platform code
 *    parses the dialplan.  This function simply turns the
 *    dialtone off every time a digit is received and
 *    displays the appropriate keyset.  (Eventually, this
 *    interface should be changed and GSM should have
 *    better knowledge of the dialplan.)
 *
 *  Returns: SM_RC_END
 */
static sm_rcs_t
fsmdef_ev_digit_begin (sm_event_t *event)
{
    static const char  fname[]    = "fsmdef_ev_digit_begin";
    fsm_fcb_t        *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t     *dcb = fcb->dcb;
    cc_digit_begin_t *msg = (cc_digit_begin_t *) event->msg;
    char              digit;
    cc_action_data_t  data;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    digit = lsm_digit2ch(msg->digit);

    FSM_DEBUG_SM(DEB_L_C_F_PREFIX"Digit Received= %c: stopping dial tone..",
		DEB_L_C_F_PREFIX_ARGS(FSM, msg->line, msg->call_id, fname), digit);
    data.tone.tone = VCM_INSIDE_DIAL_TONE;
    (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_STOP_TONE, &data);

    /*
     * Increment digit_cnt so that the proper keyset will be
     * displayed.
     */
    if (dcb->digit_cnt < CC_MAX_DIALSTRING_LEN) {
        dcb->digit_cnt++;
    }

    return (SM_RC_END);
}


static sm_rcs_t
fsmdef_ev_proceeding (sm_event_t *event)
{
    fsm_fcb_t    *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t *dcb = fcb->dcb;

    fcb->dcb->send_release = TRUE;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

#ifdef SAPP_SAPP_GSM
    if ((event->msg != NULL) &&
        (((cc_proceeding_t *)(event->msg))->caller_id.called_name != NULL)) {
        dcb->caller_id.called_name =
            strlib_update(dcb->caller_id.called_name,
                          ((cc_proceeding_t *) (event->msg))->caller_id.
                          called_name);
    }
#endif

    cc_call_state(dcb->call_id, dcb->line, CC_STATE_FAR_END_PROCEEDING,
                  FSMDEF_CC_CALLER_ID);


    fsm_change_state(fcb, __LINE__, FSMDEF_S_OUTGOING_PROCEEDING);

    return (SM_RC_END);
}

static sm_rcs_t
fsmdef_ev_out_alerting (sm_event_t *event)
{
    static const char fname[] = "fsmdef_ev_out_alerting";
    fsm_fcb_t     *fcb   = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t  *dcb   = fcb->dcb;
    cc_alerting_t *msg   = (cc_alerting_t *) event->msg;
    cc_causes_t    cause = CC_CAUSE_ERROR;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    dcb->send_release = TRUE;

    dcb->inband = FALSE;
    if (msg->inband) {
        dcb->inband = TRUE;

        cause = gsmsdp_negotiate_answer_sdp(fcb, &msg->msg_body);
        if (cause != CC_CAUSE_OK) {
            cc_call_state(fcb->dcb->call_id, fcb->dcb->line, CC_STATE_UNKNOWN,
                          NULL);
            return (fsmdef_release(fcb, cause, dcb->send_release));
        }

        /*
         * Record fact that we have successfully negotiated media that may be
         * used for inband ringback.
         */
        dcb->inband_received = TRUE;
        FSM_DEBUG_SM(DEB_F_PREFIX"inband_received, cancel timer.", DEB_F_PREFIX_ARGS(FSM, fname));

        /*
         * If ringback delay timer has been started, cancel it now.
         */
        if (cprCancelTimer(dcb->ringback_delay_tmr) != CPR_SUCCESS) {
            FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_TMR_CANCEL_FAILED),
                         dcb->call_id, dcb->line, fname, "Ringback Delay",
                         cpr_errno);
        }
    } else {
        /*
         * Not inband alerting case. Set ringback delay timer so that local
         * ringback will eventually be played. We delay the ringback for
         * a short time to handle the case where the messages key was pressed.
         * This is because VM server can respond very quickly with RTP, 183,
         * and 200 and we do not want local ringback tone to interfere with
         * the playing of the VM prompt.
         */
        if (!cprIsTimerRunning(dcb->ringback_delay_tmr)) {
            fsmdef_set_ringback_delay_timer(dcb);
        }
    }

    cc_call_state(dcb->call_id, dcb->line, CC_STATE_FAR_END_ALERTING,
                  FSMDEF_CC_CALLER_ID);

    /*
     * If DSP is not able to start rx/tx channels, release the call
     */
    if (dcb->dsp_out_of_resources == TRUE) {
        (void)fsmdef_release(fcb, CC_CAUSE_NO_MEDIA, dcb->send_release);
        cc_call_state(fcb->dcb->call_id, fcb->dcb->line, CC_STATE_UNKNOWN,
                      NULL);
        return (SM_RC_END);
    }
//    fsmdef_update_pd(dcb, FSMDEF_CALL_TYPE_OUTGOING);

    fsm_change_state(fcb, __LINE__, FSMDEF_S_OUTGOING_ALERTING);

    return (SM_RC_END);
}

static sm_rcs_t
fsmdef_ev_callsent_release (sm_event_t *event)
{
    fsm_fcb_t      *fcb    = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t   *dcb    = fcb->dcb;
    cc_release_t   *msg    = (cc_release_t *) event->msg;
    cc_causes_t     cause  = msg->cause;
    cc_srcs_t       src_id = msg->src_id;
    sm_rcs_t        sm_rc  = SM_RC_END;
    char            tmp_str[STATUS_LINE_MAX_LEN];

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    /* if UI_STATE of BUSY in 183 Call-Info is causing the release,
       do not modify dcb->send_release */
    if (cause != CC_CAUSE_UI_STATE_BUSY) {
        dcb->send_release = FALSE;
    } else {
        // CSCti63677
        if ((fcb->state == FSMDEF_S_OUTGOING_ALERTING) &&
            (dcb->inband_received == TRUE) &&
            (dcb->placed_call_update_required)) {

            lsm_update_placed_callinfo(dcb);
            dcb->placed_call_update_required = FALSE;
        }
    }

    FSM_SET_FLAGS(dcb->msgs_rcvd, FSMDEF_MSG_RELEASE);

    /* For 500 response from the CCM, disconnect the call and clear the UI.
     * There are several cases in which CCM sends down 500 response code to
     * clear the call and UI. Some of the cases are CFWDALL, early conference
     * and CTI transfer of ringing call
     *
     * Non-auto pickups do receive 480 response, it is OK release the call.
     */
    if ((cause == CC_CAUSE_REMOTE_SERVER_ERROR) ||
            (((strncmp(dcb->caller_id.called_number, CISCO_BLFPICKUP_STRING,
                       (sizeof(CISCO_BLFPICKUP_STRING) - 1)) == 0)) &&
             ((cause == CC_TEMP_NOT_AVAILABLE) || (cause == CC_CAUSE_CONGESTION) ))) {
        if (cause == CC_CAUSE_CONGESTION) {
            if (platGetPhraseText(STR_INDEX_NO_CALL_FOR_PICKUP, (char *)tmp_str, STATUS_LINE_MAX_LEN - 1) == CPR_SUCCESS)
            {
                ui_set_notification(CC_NO_LINE, CC_NO_CALL_ID, tmp_str, 2, FALSE, DEF_NOTIFY_PRI);
            }
        }
        cause = CC_CAUSE_OK;
    }

    switch (cause) {
        case CC_CAUSE_ERROR:
        case CC_CAUSE_NOT_FOUND:
        case CC_CAUSE_BUSY:
        case CC_CAUSE_CONGESTION:
        case CC_CAUSE_INVALID_NUMBER:
        case CC_CAUSE_PAYLOAD_MISMATCH:
        case CC_CAUSE_REMOTE_SERVER_ERROR:
        case CC_TEMP_NOT_AVAILABLE:
        case CC_CAUSE_UI_STATE_BUSY:
        case CC_CAUSE_NO_USER_ANS:

            fsmdef_set_call_info_cc_call_state(dcb, CC_STATE_CALL_FAILED, cause);

            if (cause != CC_CAUSE_UI_STATE_BUSY) {
                cc_int_release_complete(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id,
                        dcb->line, cause, NULL);
            }
            /* see if the SIP stack has aborted this call early for some reason
             * If SIP brought this down, we are still offhook on the UI, so
             * when we get the release_complete from the 200 for the BYE, we
             * need to ignore it, so that reorder can be played AND when the user
             * hangs up, then the UI will be driven to a clean state.
             */
            if (src_id == CC_SRC_SIP) {
                dcb->early_error_release = TRUE;
            }

            if ( dcb->err_onhook_tmr) {
                (void) cprDestroyTimer(dcb->err_onhook_tmr);
            }
            dcb->err_onhook_tmr = cprCreateTimer("Error Onhook",
                    GSM_ERROR_ONHOOK_TIMER,
                    TIMER_EXPIRATION,
                    gsm_msgq);
            if (dcb->err_onhook_tmr == NULL) {
                FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_TMR_CREATE_FAILED),
                        dcb->call_id, dcb->line, "", "Error Onhook");
                return (SM_RC_CLEANUP);
            }

            if (cprStartTimer(dcb->err_onhook_tmr,
                        FSMDEF_ERR_ONHOOK_TMR_SECS * 1000,
                        (void *)(long)dcb->call_id) == CPR_FAILURE) {

                FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_TMR_START_FAILED),
                        dcb->call_id, dcb->line, "",
                        "Error Onhook", cpr_errno);

                return (SM_RC_CLEANUP);
            }

            break;

        default:
            sm_rc = fsmdef_release(fcb, cause, dcb->send_release);
            if (sm_rc == SM_RC_CLEANUP) {
                /*
                 * FSM release indicates clean up, do not continue
                 * on since fcb and dcb have been freed or re-initialized.
                 */
                return (sm_rc);
            }
    }                           /* switch (cause) */

    /*UI_STATE of BUSY in 183 is causing the release, so
     *don't change state. This is needed to support
     *callback feature. Since the callee is busy, we need
     *update call UI status to "Busy" from "Ringout" to
     *reflect this change.
     */
    if (cause != CC_CAUSE_UI_STATE_BUSY) {
        fsm_change_state(fcb, __LINE__, FSMDEF_S_RELEASING);
    } else {
        cc_action_data_t action_data;
        action_data.update_ui.action = CC_UPDATE_SET_CALL_STATUS;
        action_data.update_ui.data.set_call_status_parms.phrase_str_p = platform_get_phrase_index_str(LINE_BUSY);
        action_data.update_ui.data.set_call_status_parms.timeout = 0;
        action_data.update_ui.data.set_call_status_parms.call_id = dcb->call_id;
        action_data.update_ui.data.set_call_status_parms.line = dcb->line;
        /*Update UI status to "Busy".*/
        (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_UPDATE_UI,
                             &action_data);
    }

    return (sm_rc);
}


static sm_rcs_t
fsmdef_ev_callsent_feature (sm_event_t *event)
{
    fsm_fcb_t       *fcb     = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t    *dcb     = fcb->dcb;
    cc_feature_t    *msg     = (cc_feature_t *) event->msg;
    cc_srcs_t        src_id  = msg->src_id;
    cc_features_t    ftr_id  = msg->feature_id;
    callid_t         call_id = msg->call_id;
    line_t           line    = msg->line;
    cc_causes_t      cause;
    cc_feature_data_redirect_t *data = &(msg->data.redirect);
    cc_action_data_t action_data;
    cc_feature_data_t *select_data = &(msg->data);

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    fsm_sm_ftr(ftr_id, src_id);

    switch (ftr_id) {
    case CC_FEATURE_UPD_SESSION_MEDIA_CAP:
        dcb->video_pref = select_data->caps.support_direction;
        break;
    case CC_FEATURE_NOTIFY:
        if (src_id == CC_SRC_SIP) {
            fsmdef_ev_notify_feature(msg, dcb);
        } else {
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
        }

        break;

    case CC_FEATURE_END_CALL:
        /**
         * In case of earlier attandence, there might a waiting call.
         */
        lsm_remove_lcb_prevent_ringing(dcb->call_id);
        /*
         *  Since user press the end call, no need to wait to play the busy tone.
         *  So, clear the early_error_release and clean the fcb/dcb.
         */
        dcb->early_error_release = FALSE;
        cause = fsmdef_get_cause(msg->data_valid, &(msg->data));

        return (fsmdef_release(fcb, cause, dcb->send_release));

    case CC_FEATURE_REDIRECT:
        /*
         * The outgoing call has been redirected, so we need to:
         * 1. ACK the redirect request,
         * 2. release the current call,
         * 3. start a new call to the redirect number.
         */
        cc_int_feature_ack(CC_SRC_GSM, CC_SRC_SIP, call_id, line,
                           CC_FEATURE_REDIRECT, NULL, CC_CAUSE_REDIRECT);
        /*
         * May need to update an xcb if this call is involved in a transfer.
         */
        //xcb = fsmxfr_get_xcb_by_call_id(call_id);
        // fsmxfr_update_xfr_context(xcb, call_id, redirect_call_id);
        dcb->caller_id.called_number =
            strlib_update(dcb->caller_id.called_number, data->redirect_number);

        cc_call_state(dcb->call_id, dcb->line, CC_STATE_DIALING_COMPLETED,
                      FSMDEF_CC_CALLER_ID);

        break;


    case CC_FEATURE_CALLINFO:
        fsmdef_update_calltype(fcb, msg);
        fsmdef_update_callinfo(fcb, msg);
        /*
         * lsm_set_lcb_prevent_ringing() will check if there is a RINGIN call
         * with the same GCID. If so, it will set a flag to prevent ringing.
         */
        lsm_set_lcb_prevent_ringing(dcb->call_id);
        break;

    case CC_FEATURE_UPDATE:
        /* Simply reply with a 200OK to a received UPDATE */
        cc_int_feature_ack(CC_SRC_GSM, CC_SRC_SIP, call_id, line,
                           CC_FEATURE_UPDATE, NULL, CC_CAUSE_OK);
        break;

    case CC_FEATURE_RINGBACK_DELAY_TIMER_EXP:
        if (!dcb->inband_received) {
            /*
             * Ringback delay timer expired and we have not received
             * a response from the far end indicating that they are
             * playing inband ringback. Start local ringback tone now.
             */
            action_data.tone.tone = VCM_ALERTING_TONE;
            (void)cc_call_action(call_id, line, CC_ACTION_PLAY_TONE,
                                 &action_data);
        }
        break;


    case CC_FEATURE_SELECT:
        fsmdef_select_invoke(dcb, select_data);
        return (SM_RC_END);


    case CC_FEATURE_SUBSCRIBE:
        /* KPML subscription received so collect digits for KPML */
        fsm_change_state(fcb, __LINE__, FSMDEF_S_KPML_COLLECT_INFO);
        break;

    case CC_FEATURE_CFWD_ALL:
        fsm_set_call_status_feature_unavailable(call_id, line);

        fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
        break;

    default:
        fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
        break;
    }                           /* switch (ftr_id) { */

    return (SM_RC_END);
}


static sm_rcs_t
fsmdef_release_call (fsm_fcb_t *fcb, cc_feature_t *msg)
{
    cc_feature_data_t *data = &(msg->data);
    cc_state_data_t    state_data;
    cc_causes_t        cause;
    fsmdef_dcb_t      *dcb  = fcb->dcb;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    cause = fsmdef_get_cause(msg->data_valid, data);

    /*
     * Do things a little different depending on the value of the
     * release cause.
     */
    switch (cause) {
    case CC_CAUSE_XFER_LOCAL:
        /*
         * Send release and then wait for the release_complete.
         */
        cc_int_release(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id,
                       dcb->line, data->endcall.cause,
                       data->endcall.dialstring, NULL);

        fsm_change_state(fcb, __LINE__, FSMDEF_S_RELEASING);

        state_data.onhook.caller_id = dcb->caller_id;
        state_data.onhook.local     = TRUE;
        state_data.onhook.cause     = CC_CAUSE_NORMAL;
        cc_call_state(dcb->call_id, dcb->line, CC_STATE_ONHOOK, &state_data);

        break;

    case CC_CAUSE_XFER_REMOTE:
        /*
         * No need to send release because the remote end initiated
         * the transfer.
         */
        dcb->send_release = FALSE;
        return (fsmdef_release(fcb, cause, dcb->send_release));

    case CC_CAUSE_XFER_CNF:
    case CC_CAUSE_REPLACE:
        /*
         * We are the target of a transfer and this is the consultation
         * call that is being replaced, so we just need to onhook this call
         * but leave the signaling up until the stack notifies the FSM that
         * the transfer is accepted - and then we will release the call.
         * Same has to happen when bridge of conference ends the call. We are
         * initiating transfer in this case so we want signaling to remain
         * up while UI should be cleared up.
         */
        state_data.onhook.caller_id = dcb->caller_id;
        state_data.onhook.local     = TRUE;
        state_data.onhook.cause     = CC_CAUSE_NORMAL;
        cc_call_state(dcb->call_id, dcb->line, CC_STATE_ONHOOK, &state_data);

        fsm_change_state(fcb, __LINE__, FSMDEF_S_HOLDING);

        break;

    default:
        return (fsmdef_release(fcb, cause, dcb->send_release));
    }

    return (SM_RC_END);
}


static sm_rcs_t
fsmdef_ev_inalerting_feature (sm_event_t *event)
{
    fsm_fcb_t     *fcb     = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t  *dcb     = fcb->dcb;
    cc_feature_t  *msg     = (cc_feature_t *) event->msg;
    cc_srcs_t      src_id  = msg->src_id;
    cc_features_t  ftr_id  = msg->feature_id;
    callid_t       call_id = msg->call_id;
    line_t         line    = msg->line;
    cc_feature_data_t *data = &(msg->data);

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    fsm_sm_ftr(ftr_id, src_id);

    switch (src_id) {
    case CC_SRC_UI:
    case CC_SRC_GSM:
        switch (ftr_id) {
        case CC_FEATURE_UPD_SESSION_MEDIA_CAP:
             dcb->video_pref = data->caps.support_direction;
             /* force an update to media cap */
             dcb->media_cap_tbl->id--;
             gsmsdp_update_local_sdp_media_capability(dcb, FALSE, FALSE);
             break;

        case CC_FEATURE_END_CALL:
            return (fsmdef_release_call(fcb, msg));

        case CC_FEATURE_ANSWER:
            /*
             * The user wants to answer this call, so...
             * 1. need to place the connected call (if there is one) on hold,
             * 2. clear all the outgoing ringing lines,
             * 3. answer this call.
             */
            if (fsmdef_wait_to_start_new_call(TRUE, CC_SRC_GSM, dcb->call_id, dcb->line,
                                              CC_FEATURE_ANSWER, NULL)) {

                /*
                 * Inform the LSM that the answering of this call has
                 * been delayed while waiting for other calls to clear.
                 */
                (void)cc_call_action(dcb->call_id, dcb->line,
                                     CC_ACTION_ANSWER_PENDING, NULL);

                return (SM_RC_END);
            }

            return (fsmdef_handle_inalerting_offhook_answer(event));

        default:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);

            break;
        }                       /* switch (ftr_id) { */

        break;

    case CC_SRC_SIP:
        switch (ftr_id) {
        case CC_FEATURE_CALLINFO:
            fsmdef_update_callinfo(fcb, msg);
            break;

        case CC_FEATURE_UPDATE:
            /* Simply reply with a 200 OK to a received UPDATE */
            cc_int_feature_ack(CC_SRC_GSM, CC_SRC_SIP, call_id, line,
                               CC_FEATURE_UPDATE, NULL, CC_CAUSE_OK);
            break;

        default:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);

            break;
        }                       /* switch (ftr_id) { */

        break;

    default:
        fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);

        break;
    }                           /* switch (src_id) { */

    return (SM_RC_END);
}


/*
 * This function contains the common code for fsmdef_ev_inalerting_offhook()
 * and the ANSWER event handling in the fsmdef_ev_inalerting_feature().
 */
static sm_rcs_t
fsmdef_handle_inalerting_offhook_answer (sm_event_t *event)
{
    fsm_fcb_t        *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t     *dcb = fcb->dcb;
    cc_causes_t       cause;
    cc_msgbody_info_t msg_body;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    /* Build our response SDP to include in the connected */
    cause = gsmsdp_encode_sdp_and_update_version(dcb, &msg_body);
    if (cause != CC_CAUSE_OK) {
        FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
        return (fsmdef_release(fcb, cause, dcb->send_release));
    }

    /* For CCM, call_type indicate if the call is forwarded or not
     * for forwarded call display will be shown as "Forward", only
     * during ringing state. Once the call is  connected then the call
     * is shown as normal incoming call "From". so change call type now
     * Do this only if Retain Forward Information is disabled or not configured.
     * If configured/enabled then leave the call type as Forward.
     */

    if (dcb->call_type == FSMDEF_CALL_TYPE_FORWARD) {
        if (!fsmdef_check_retain_fwd_info_state()) {
            dcb->call_type = FSMDEF_CALL_TYPE_INCOMING;
            /*
             * Force us to update the UI so that any possible callinfo received
             * prior to the call is answered takes effect.
             */
            dcb->ui_update_required = TRUE;
        }
    }

    /* Cancel any existing autoanswer timer */
    (void)cprCancelTimer(dcb->autoAnswerTimer);

    cc_int_connected(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id, dcb->line,
                     &(dcb->caller_id), NULL, &msg_body);

    FSM_SET_FLAGS(dcb->msgs_sent, FSMDEF_MSG_CONNECTED);

    cc_call_state(dcb->call_id, dcb->line, CC_STATE_ANSWERED,
                  FSMDEF_CC_CALLER_ID);

    fsm_change_state(fcb, __LINE__, FSMDEF_S_CONNECTING);

    return (SM_RC_END);
}


static sm_rcs_t
fsmdef_ev_inalerting_offhook (sm_event_t *event)
{
    return (fsmdef_handle_inalerting_offhook_answer(event));
}


static sm_rcs_t
fsmdef_ev_connecting_feature (sm_event_t *event)
{
    fsm_fcb_t    *fcb    = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t *dcb    = fcb->dcb;
    cc_feature_t *msg    = (cc_feature_t *) event->msg;
    cc_srcs_t     src_id = msg->src_id;
    cc_features_t ftr_id = msg->feature_id;
    cc_causes_t   cause;
    cc_feature_data_t *data = &(msg->data);

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    fsm_sm_ftr(ftr_id, src_id);

    switch (src_id) {
    case CC_SRC_UI:
        switch (ftr_id) {
        case CC_FEATURE_UPD_SESSION_MEDIA_CAP:
             dcb->video_pref = data->caps.support_direction;
             break;
        case CC_FEATURE_END_CALL:
            cause = fsmdef_get_cause(msg->data_valid, &(msg->data));

            return (fsmdef_release(fcb, cause, dcb->send_release));

        default:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);

            break;
        }

        break;

    case CC_SRC_SIP:
        switch (ftr_id) {
        case CC_FEATURE_CALLINFO:
            fsmdef_update_callinfo(fcb, msg);
            break;

        case CC_FEATURE_CALL_PRESERVATION:
            return (fsmdef_release(fcb, CC_CAUSE_NORMAL, dcb->send_release));

        case CC_FEATURE_NOTIFY:
            fsmdef_ev_notify_feature(msg, dcb);
            break;

        default:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);

            break;
        }

        break;

    case CC_SRC_GSM:
        switch (ftr_id) {
        case CC_FEATURE_END_CALL:
            cause = fsmdef_get_cause(msg->data_valid, &(msg->data));

            return (fsmdef_release(fcb, cause, dcb->send_release));

        default:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);

            break;
        }

        break;

    default:
        fsmdef_sm_ignore_src(fcb, __LINE__, src_id);

        break;
    }

    return (SM_RC_END);
}

/**
 *
 * Function to handle transition to FSMDEF_S_CONNECTED. It checks
 * whether there is any media capability that needs to be updated
 * or not. If there is not then it transition to FSMDEF_S_CONNECTED
 * otherwise it transitions to the FSMDEF_S_CONNECTED_MEDIA_PEND state
 * and sends out the media update request.
 *
 * @param[in] fcb     - The pointer to the fsm_fcb_t structure of this
 *                      call.
 *
 * @return  SM_RC_END or SM_RC_CLEANUP
 *
 * @pre     (fcb not_eq NULL)
 */
static sm_rcs_t
fsmdef_transition_to_connected (fsm_fcb_t *fcb)
{
    fsmdef_dcb_t      *dcb = fcb->dcb;
    cc_feature_data_t feature_data;
    sm_rcs_t          sm_rc = SM_RC_END;
    cc_causes_t       cause;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    if (dcb->req_pending_tmr) {
        /* cancel any request pending timer, just in case */
        (void) cprCancelTimer(dcb->req_pending_tmr);
    }

    /*
     * Update the media capability without effecting the existing media line.
     */
    if (!gsmsdp_update_local_sdp_media_capability(dcb, FALSE, FALSE)) {
        /* not thing is changed, transition to connected state */
        fsm_change_state(fcb, __LINE__, FSMDEF_S_CONNECTED);
        return (sm_rc);
    }


    feature_data.resume.call_info.type = CC_FEAT_NONE;
    feature_data.resume.call_info.data.hold_resume_reason = CC_REASON_NONE;
    feature_data.resume.msg_body.num_parts = 0;
    feature_data.resume.call_info.data.call_info_feat_data.swap = FALSE;
    feature_data.resume.call_info.data.call_info_feat_data.protect = FALSE;
    /* Encode SDP */
    cause = gsmsdp_encode_sdp_and_update_version(dcb,
                                                 &feature_data.resume.msg_body);
    if (cause != CC_CAUSE_OK) {
        FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
        return(fsmdef_release(fcb, cause, dcb->send_release));
    }

    fsmdef_get_rtp_stat(dcb, &(feature_data.resume.kfactor));

    /* Send feature request to SIP */
    cc_int_feature(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id, dcb->line,
                   CC_FEATURE_MEDIA, &feature_data);


    if (g_dock_undock_event == MEDIA_INTERFACE_UPDATE_STARTED) {
        g_dock_undock_event = MEDIA_INTERFACE_UPDATE_IN_PROCESS;
        ui_update_media_interface_change(dcb->line, dcb->call_id, MEDIA_INTERFACE_UPDATE_BEGIN);
    } else if (g_dock_undock_event == MEDIA_INTERFACE_UPDATE_IN_PROCESS) {
        DEF_DEBUG(DEB_F_PREFIX" MEDIA_INTERFACE_UPDATE is already in process. "
            " Ignore another update event.\n", DEB_F_PREFIX_ARGS(FSM, "fsmdef_transition_to_connected"));
    }
    fsm_change_state(fcb, __LINE__, FSMDEF_S_CONNECTED_MEDIA_PEND);
    return (sm_rc);
}

static sm_rcs_t
fsmdef_ev_connected (sm_event_t *event)
{
    static const char fname[] = "fsmdef_ev_connected";
    fsm_fcb_t      *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t   *dcb = fcb->dcb;
    cc_connected_t *msg = (cc_connected_t *) event->msg;
    cc_causes_t     cause;
    sm_rcs_t        sm_rc;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    dcb->send_release = TRUE;

    cause = gsmsdp_negotiate_answer_sdp(fcb, &msg->msg_body);
    if (cause != CC_CAUSE_OK) {

		cc_call_state(fcb->dcb->call_id, fcb->dcb->line, CC_STATE_UNKNOWN,
						NULL);
		return (fsmdef_release(fcb, cause, dcb->send_release));
    }

    // Reset dcb->active_feature flag
    dcb->active_feature = CC_FEATURE_NONE;

    /* Reset spoof ring out in case t was set before going to connected state. */
    FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_CLR_SPOOF_APPLD),
                 dcb->call_id, dcb->line, fname);

    dcb->spoof_ringout_applied = FALSE;

    /*
     * Cancel ringback delay timer
     */
    if (cprCancelTimer(dcb->ringback_delay_tmr) != CPR_SUCCESS) {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_TMR_CANCEL_FAILED),
                     dcb->call_id, dcb->line, fname, "Ringback Delay",
                     cpr_errno);
    }

    cc_call_state(dcb->call_id, dcb->line, CC_STATE_CONNECTED,
                  FSMDEF_CC_CALLER_ID);

    if ( dcb->log_disp != CC_CALL_LOG_DISP_UNKNWN ) {
        ui_log_disposition(dcb->call_id, dcb->log_disp );
    }


    ui_cc_capability(dcb->line, lsm_get_ui_id(dcb->call_id), msg->recv_info_list);

    /*
     * If DSP is not able to start rx/tx channels, release the call
     */
    if (dcb->dsp_out_of_resources == TRUE) {
        (void)fsmdef_release(fcb, CC_CAUSE_NO_MEDIA, dcb->send_release);
        cc_call_state(fcb->dcb->call_id, fcb->dcb->line, CC_STATE_UNKNOWN,
                      NULL);
        return (SM_RC_END);
    }
    cc_int_connected_ack(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id, dcb->line,
                         &(dcb->caller_id), NULL);

    FSM_SET_FLAGS(dcb->msgs_sent, FSMDEF_MSG_CONNECTED_ACK);

//    fsmdef_update_pd(dcb, FSMDEF_CALL_TYPE_OUTGOING);

    /*
     * Handle media capability changes if there is before transition to
     * connected state.
     */
    sm_rc = fsmdef_transition_to_connected(fcb);
    fsmutil_set_shown_calls_ci_element(dcb->caller_id.call_instance_id, dcb->line);

    return (sm_rc);
}


static sm_rcs_t
fsmdef_ev_connected_ack (sm_event_t *event)
{
    fsm_fcb_t          *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t       *dcb = fcb->dcb;
    cc_connected_ack_t *msg = (cc_connected_ack_t *) event->msg;
    cc_causes_t         cause;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    /*
     * Check the remote SDP. The far end may not have included the SDP in an
     * earlier message, which means that the SDP must be in this message.
     */
    if (dcb->remote_sdp_in_ack == TRUE) {
        cause = gsmsdp_negotiate_answer_sdp(fcb, &msg->msg_body);
        if (cause != CC_CAUSE_OK) {
            return (fsmdef_release(fcb, cause, dcb->send_release));
        }
    }

    cc_call_state(dcb->call_id, dcb->line, CC_STATE_CONNECTED,
                  FSMDEF_CC_CALLER_ID);
    /*
     * If DSP is not able to start rx/tx channels, release the call
     */
    if (dcb->dsp_out_of_resources == TRUE) {
        (void)fsmdef_release(fcb, CC_CAUSE_NO_MEDIA, dcb->send_release);
        cc_call_state(fcb->dcb->call_id, fcb->dcb->line, CC_STATE_UNKNOWN,
                      NULL);
        return (SM_RC_END);
    }

//    fsmdef_update_pd(dcb, FSMDEF_CALL_TYPE_INCOMING);

    /*
     * Handle media capability changes if there is before transition to
     * connected state.
     */
    return (fsmdef_transition_to_connected(fcb));
}

/**
 * The function handles local hold event but not sending any hold request
 * out to the remote end. The local SDP is updated by the way.
 *
 * @param[in]fcb          - pointer to fsm_fcb_t
 *
 * @return  SM_RC_END or failrue.
 *
 * @pre     (fcb not_eq NULL)
 */
static sm_rcs_t
fsm_hold_local_only (fsm_fcb_t *fcb)
{
    static const char fname[] = "fsm_hold_local_only";
    cc_state_data_t state_data;
    fsmdef_dcb_t *dcb = fcb->dcb;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    /*
     * Check local hold status, and allow request if the media is not
     * locally held.
     */
    if (fsmdef_all_media_are_local_hold(dcb)) {
        /*
         * a new hold request is not allowed. Ignore the request
         * but we should still ack the request.
         */
        cc_int_feature_ack(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id,
                           dcb->line, CC_FEATURE_HOLD, NULL, CC_CAUSE_NORMAL);
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG1), dcb->call_id, dcb->line,
                     fname, "already hold");

        return (SM_RC_END);
    }

    state_data.hold.caller_id = dcb->caller_id;
    state_data.hold.local     = TRUE;

    /*
     * Update the SDP so that offer indicates hold. Reinitialize the local
     * sdp media to include all available codecs. We do this because our local
     * list has been shortened to the one negotiated codec.
     */
    (void)gsmsdp_update_local_sdp_media_capability(dcb, TRUE, TRUE);

    FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_CLR_SPOOF_APPLD),
                 dcb->call_id, dcb->line, fname);

    dcb->spoof_ringout_applied = FALSE;

    cc_call_state(dcb->call_id, dcb->line, CC_STATE_HOLD, &state_data);

    /* set all the media to local hold */
    fsmdef_update_media_hold_status(dcb, NULL, TRUE);

    fsm_change_state(fcb, __LINE__, FSMDEF_S_HOLDING);

    sipsdp_src_dest_free(CCSIP_DEST_SDP_BIT | CCSIP_SRC_SDP_BIT,
                             &dcb->sdp);

    return (SM_RC_END);
}

/**
 * The function handles local hold event. The function also supports
 * re-sending hold request out such as during a glare condition.
 *
 * @param[in]fcb          - pointer to fsm_fcb_t
 * @param[in]data_p       - pointer to the cc_feature_data_t of the
 *                          hold feature.
 * @param[in]resend       - TRUE indicates to resend hold request.
 *
 * @return  SM_RC_END or failrue.
 *
 * @pre     (fcb not_eq NULL)
 * @pre     (data_p not_eq NULL)
 */
static sm_rcs_t
fsm_hold_local (fsm_fcb_t *fcb, cc_feature_data_t *data_p,
                boolean resend)
{
    static const char fname[] = "fsm_hold_local";
    cc_state_data_t state_data;
    fsmdef_dcb_t   *dcb = fcb->dcb;
    cc_causes_t     cause;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    /*
     * Check local hold status, and allow request if the media is not
     * locally held or the caller indicates that to resend the hold
     * request (such as in glare resolution).
     */
    if (!resend && fsmdef_all_media_are_local_hold(dcb)) {
        /*
         * a new hold request is not allowed. Ignore the request
         * but we should still ack the request.
         */
        cc_int_feature_ack(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id,
                           dcb->line, CC_FEATURE_HOLD, NULL,
                           CC_CAUSE_NORMAL);
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG1), dcb->call_id, dcb->line,
                     fname, "already hold");
        return (SM_RC_END);
    }

    state_data.hold.caller_id = dcb->caller_id;
    state_data.hold.local     = TRUE;
    state_data.hold.reason    = data_p->hold.call_info.data.hold_resume_reason;

    /* Store hold reason in case we need to resend the hold request due to
     * request pending response.
     */
    dcb->hold_reason = data_p->hold.call_info.data.hold_resume_reason;

    FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_CLR_SPOOF_APPLD),
                 dcb->call_id, dcb->line, fname);

    dcb->spoof_ringout_applied = FALSE;

    fsmdef_get_rtp_stat(dcb, &(data_p->hold.kfactor));

    /* put the call on hold before building the SDP  as DSP
     * will then be able to give us a full set of codecs
     * CUCM doesn't like to see a change in codecs on the fly
     * ( i.e. without going to inactive state ) */
    cc_call_state(dcb->call_id, dcb->line, CC_STATE_HOLD, &state_data);

    /*
     * Update the SDP so that offer indicates hold. Reinitialize the local
     * sdp to include all available codecs. We do this because our
     * local list has been shortened to the one negotiated codec.
     */
    (void)gsmsdp_update_local_sdp_media_capability(dcb, TRUE, TRUE);

    /*
     * Do not expect any msg. body from local hold but free them
     * just in case before build new SDP body to send out.
     */
    cc_free_msg_body_parts(&data_p->hold.msg_body);

    /* Build SDP for sending out */
    cause = gsmsdp_encode_sdp_and_update_version(dcb, &data_p->hold.msg_body);
    if (cause != CC_CAUSE_OK) {
        FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
        return (fsmdef_release(fcb, cause, dcb->send_release));
    }

    /* set all the media to local hold */
    fsmdef_update_media_hold_status(dcb, NULL, TRUE);

    cc_int_feature(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id, dcb->line,
                   CC_FEATURE_HOLD, data_p);

    fsm_change_state(fcb, __LINE__, FSMDEF_S_HOLDING);

    sipsdp_src_dest_free(CCSIP_DEST_SDP_BIT | CCSIP_SRC_SDP_BIT,
                             &dcb->sdp);

    return (SM_RC_END);
}

/**
 * Handles hold feature in connected media update pending state.
 *
 * @param[in] fcb     The pointer to the fsm_fcb_t structure of this
 *                    call chain.
 * @param[in] data_p  pointer to the cc_feature_data_t.
 *
 * @return            sm_rsc_t indicates whether the execution of
 *                    next statmachine to end or to clean up.
 */
static sm_rcs_t
fsm_connected_media_pend_local_hold (fsm_fcb_t *fcb, cc_feature_data_t *data_p)
{
    static const char fname[] = "fsm_hold_local_connected_media_pend";
    fsmdef_dcb_t   *dcb = fcb->dcb;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    /*
     * Check local hold status, and allow request if the media is not
     * locally held.
     */
    if (fsmdef_all_media_are_local_hold(dcb)) {
        /*
         * a new hold request is not allowed. Ignore the request
         * but we should still ack the request.
         */
        cc_int_feature_ack(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id,
                           dcb->line, CC_FEATURE_HOLD, NULL,
                           CC_CAUSE_NORMAL);
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG1), dcb->call_id, dcb->line,
                     fname, "already hold");
        return (SM_RC_END);
    }

    if (dcb->req_pending_tmr &&
        cprIsTimerRunning(dcb->req_pending_tmr)) {
        /*
         * Request timer is running, that means we are waiting
         * to re-send media update again due to previously glare.
         * Since the previous offer has not been accepted, we can
         * simply just send hold instead when glare resolution timer
         * expires.
         */

        /* store the reason to resend when glare timer expires */
        dcb->hold_reason = data_p->hold.call_info.data.hold_resume_reason;
        /*
         * reset feature hold pending flag in case that there are
         * multiple hold feature received while waiting for
         * glare resolution to resolve.
         */
        FSM_RESET_FLAGS(dcb->flags, FSMDEF_F_HOLD_REQ_PENDING);
        fsm_change_state(fcb, __LINE__, FSMDEF_S_HOLD_PENDING);
        return (SM_RC_END);
    }

    /*
     * We have sent media capability update out but have not received
     * any response yet. The glare condition may occur but we can only
     * assume that the media update was sent out at this point.
     * We can not send out any more request until the result is
     * known. We can not do any thing now but simply remember
     * to re-send media with the hold feature pending when
     * the result is known.
     */
    FSM_SET_FLAGS(dcb->flags, FSMDEF_F_HOLD_REQ_PENDING);
    return (SM_RC_END);
}

/**
 * common function to handles media feature from remote end.
 *
 * @param[in] fcb     The pointer to the fsm_fcb_t structure of this
 *                    call chain.
 * @param[in] msg     The pointer to cc_feature_t.
 *
 * @return            sm_rsc_t indicates whether the execution of
 *                    next statmachine to end or to clean up.
 */
static sm_rcs_t
fsmdef_remote_media (fsm_fcb_t *fcb, cc_feature_t *msg)
{
    static const char fname[] = "fsmdef_remote_media";
    fsmdef_dcb_t      *dcb  = fcb->dcb;
    cc_feature_data_t *data = &(msg->data);
    cc_feature_data_t  feature_data;
    cc_causes_t        cause;
    boolean            send_ack = TRUE;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    memset(&feature_data, 0 , sizeof(cc_feature_data_t));
    /*
     * Determine what type of RESUME/MEDIA this is:
     * 1. third-party control is just trying to change the media -
     *    midcall-invite with no SDP, so we need to wait for the SDP
     *    in the SIP ACK before we can truly resume the media.
     * 2. remote end wants to resume a held call or just a media
     *    changes.
     *
     * We can distinguish between the two because case 1 will not
     * have any data and case 2 will have data.
     */
    if (msg->data_valid == FALSE) {
        /*
         * Case 1.
         *
         * negotiate offer without SDP will reset all local media entries
         * to have all codecs included. This is to re-advertise the
         * capabilities again.
         */
         (void) gsmsdp_negotiate_offer_sdp(fcb, NULL, FALSE);

        /*
         * Update the media direction based on whether each media
         * stream is locally held or not before sending out the
         * offer SDP.
         */
        fsmdef_set_per_media_local_hold_sdp(dcb);
        (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_STOP_MEDIA,
                                 NULL);
        (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_START_RCV,
                                 NULL);
    } else {
        /*
         * SIP may send MEDIA feature when answer SDP is received in
         * ACK. The secnario is found when remote resumes and the
         * resume INVITE is a delayed media INVITE. We sent an offer in
         * the 200 OK and gets the answer back in the ACK. In this
         * case, SIP will send MEDIA feature to GSM. We need to check
         * whether we are waiting for an answer in ACK or not and
         * use the corresponding offer/answer SDP negotiation function.
         */
        if (dcb->remote_sdp_in_ack) {
            cause = gsmsdp_negotiate_answer_sdp(fcb,
                                               &data->resume.msg_body);
            if (cause != CC_CAUSE_OK) {
                /*
                 * There is some thing wrong the answer SDP for some
                 * reason, can not go on.
                 */
                FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
                return (fsmdef_release(fcb, cause, dcb->send_release));
            }

            /*
             * This is the answer to our previous offer, no need to
             * to ack to SIP this one.
             */
            send_ack = FALSE;
        } else {
            /* This is a new offer */

            /*
             * get k factor to be included in the feature ack. Getting
             * the k factor needs to be done before maniputate media
             * stream by the LSM.
             */
            fsmdef_media_t *media = gsmsdp_find_audio_media(dcb);
            if ((media) && (media->direction != SDP_DIRECTION_INACTIVE)) {
                fsmdef_get_rtp_stat(dcb, &(feature_data.resume.kfactor));
            }

            cause = gsmsdp_negotiate_offer_sdp(fcb,
                                               &data->resume.msg_body, FALSE);
            if (cause != CC_CAUSE_OK) {
                /*
                 * Received a sdp that cannot be accepted.
                 * It should just reject the new sdp offer rather than
                 * tearing down the call.
                 */
                cc_int_feature_ack(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id,
                                   dcb->line, msg->feature_id, NULL, cause);
                return (SM_RC_END);
            }
            /*
             * Update the media based on local hold.
             */
            fsmdef_set_per_media_local_hold_sdp(dcb);
        }

        /*
         * If spoof ringout is not being requested and we are currently
         * playing spoof ringout, transition the LSM from the far end alerting
         * to the connected state.
         */
        if ((!dcb->spoof_ringout_requested) && (dcb->spoof_ringout_applied)) {
            FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_CLR_SPOOF_APPLD),
                         dcb->call_id, dcb->line, fname);

            dcb->spoof_ringout_applied = FALSE;
            cc_call_state(dcb->call_id, dcb->line, CC_STATE_CONNECTED,
                          FSMDEF_CC_CALLER_ID);
        } else {
            (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_MEDIA,
                                 NULL);
        }
    }

    if (send_ack) {
        /* Build SDP from our current SDP */
        cause = gsmsdp_encode_sdp_and_update_version(dcb, &feature_data.resume.msg_body);
        if (cause != CC_CAUSE_OK) {
            FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
            return (fsmdef_release(fcb, cause, dcb->send_release));
        }
        cc_int_feature_ack(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id,
                           dcb->line, msg->feature_id, &feature_data,
                           CC_CAUSE_NORMAL);
    }
    return (SM_RC_END);
}

/**
 *
 * Function to handles connected state feature events. Function handles
 * feature events generated by GSM, UI and SIP stack.
 *
 * @param sm_event_t        event
 *
 * @return  SM_RC_END or SM_RC_CLEANUP
 *
 * @pre     (fcb->dcb not_eq NULL)
 * @pre     (event->data not_eq NULL)
 * @pre     (event->msg not_eq NULL)
 */
static sm_rcs_t
fsmdef_ev_connected_feature (sm_event_t *event)
{
    static const char fname[] = "fsmdef_ev_connected_feature";
    fsm_fcb_t         *fcb    = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t      *dcb    = fcb->dcb;
    cc_feature_t      *msg    = (cc_feature_t *) event->msg;
    cc_srcs_t          src_id = msg->src_id;
    cc_features_t      ftr_id = msg->feature_id;
    cc_feature_data_t *data   = &(msg->data);
    sm_rcs_t           sm_rc;
    cc_feature_data_t  feature_data;
    cc_action_data_t   action_data;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    fsm_sm_ftr(ftr_id, src_id);

    switch (src_id) {
    case CC_SRC_UI:
    case CC_SRC_GSM:
        switch (msg->feature_id) {
        case CC_FEATURE_HOLD:
            /* If the line number is 0xFF, then this request
             * came from GSM during a Transfer. We want to
             * put the call on local hold only. We do not want
             * to send a cc_feature to the SIP stack because
             * that will cause an Invite Hold to go via SIP.
             * We don't want to put the other end on hold, just
             * ourselves.
             */
            if (msg->line == 0xFF) {
                sm_rc = fsm_hold_local_only(fcb);
            } else {
                if (msg->data_valid) {
                    sm_rc = fsm_hold_local(fcb, data, FALSE);
                } else {
                    feature_data.hold.call_info.type = CC_FEAT_HOLD;
                    feature_data.hold.call_info.data.hold_resume_reason =
                        CC_REASON_NONE;
                    feature_data.hold.msg_body.num_parts = 0;
                    feature_data.hold.call_info.data.call_info_feat_data.swap = FALSE;
                    feature_data.hold.call_info.data.call_info_feat_data.protect = FALSE;
                    sm_rc = fsm_hold_local(fcb, &feature_data, FALSE);
                }

            }
            fsmdef_handle_join_pending(dcb);
            return (sm_rc);

        case CC_FEATURE_END_CALL:
            sm_rc = fsmdef_release_call(fcb, msg);

            fsmdef_handle_join_pending(dcb);
            return (sm_rc);

        case CC_FEATURE_JOIN:
            /*
             * Send offhook to the new call that triggers the
             * completion of the setup of the join in call
             */
            fsmdef_ev_join(data);
            break;

        case CC_FEATURE_SELECT:
            if (msg->data_valid == FALSE) {
                fsmdef_select_invoke(dcb, NULL);
            } else {
                fsmdef_select_invoke(dcb, data);
            }
            return (SM_RC_END);

        case CC_FEATURE_B2B_JOIN:
            if (msg->data_valid == FALSE) {
                fsmdef_b2bjoin_invoke(dcb, NULL);
            } else {
                fsmdef_b2bjoin_invoke(dcb, data);
            }
            return (SM_RC_END);

        case CC_FEATURE_DIRTRXFR:
        case CC_FEATURE_UNDEFINED:
            fsm_display_feature_unavailable();

            fsmdef_handle_join_pending(dcb);
            return (SM_RC_END);

        case CC_FEATURE_UPD_SESSION_MEDIA_CAP:
            dcb->video_pref = data->caps.support_direction;
            // Force an re-INVITE by mismatching the id
            dcb->media_cap_tbl->id--;
            /* FALL THRU */
        case CC_FEATURE_UPD_MEDIA_CAP:
            /*
             * Media capability update request, check to see if
             * there is any change in media capability and transition
             * the pending state or stay in the connected state.
             */
            sm_rc = fsmdef_transition_to_connected(fcb);
            return (sm_rc);

        case CC_FEATURE_REQ_PEND_TIMER_EXP:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;

        default:
            fsmdef_handle_join_pending(dcb);
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);

            break;
        }                       /* switch (msg->feature_id) */

        break;

    case CC_SRC_SIP:
        switch (msg->feature_id) {

        case CC_FEATURE_MEDIA:
            /*
             * remote send media update which can be resume or
             * or just media changes.
             */
            sm_rc = fsmdef_remote_media(fcb, msg);
            return (sm_rc);

        case CC_FEATURE_CALLINFO:
            fsmdef_update_callinfo(fcb, msg);
            break;

        case CC_FEATURE_CALL_PRESERVATION:
            action_data.update_ui.action = CC_UPDATE_CALL_PRESERVATION;
            (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_UPDATE_UI,
                                 &action_data);
            fsm_change_state(fcb, __LINE__, FSMDEF_S_PRESERVED);
            break;
        case CC_FEATURE_NOTIFY:
            fsmdef_ev_notify_feature(msg, dcb);
            break;

        case CC_FEATURE_UPDATE:
            /*
             * We only get an UPDATE feature event if we receive a medialess UPDATE.
             * This type of event only conveys UI updates that are processed with
             * a call info event. We do perform one check to see if we are currently
             * spoofing ringout. If we are and the spoof ringout requested flag
             * has been cleared, we tell the LSM to go connected.
             */
            if ((!dcb->spoof_ringout_requested) && (dcb->spoof_ringout_applied)) {
                FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_CLR_SPOOF_APPLD),
                             dcb->call_id, dcb->line, fname);

                dcb->spoof_ringout_applied = FALSE;
                cc_call_state(dcb->call_id, dcb->line, CC_STATE_CONNECTED,
                              FSMDEF_CC_CALLER_ID);
            }

	    /*
	     * For chaperone call, we will update call state here, to update
	     * the related key's status.
	     */
	    if(dcb->policy == CC_POLICY_CHAPERONE){
                cc_call_state(dcb->call_id, dcb->line, CC_STATE_CONNECTED,
                              FSMDEF_CC_CALLER_ID);
	    }
            break;

        case CC_FEATURE_FAST_PIC_UPD:

            vcmMediaControl(CREATE_CALL_HANDLE(dcb->line, dcb->call_id), VCM_MEDIA_CONTROL_PICTURE_FAST_UPDATE);

            break;

        default:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;
        }                       /* switch (msg->feature_id) */
        break;

    default:
        fsmdef_sm_ignore_src(fcb, __LINE__, src_id);
        break;
    }                           /* switch (src_id) */

    return (SM_RC_END);
}

/**
 *
 * Function to handles connected state media update pending feature.
 *
 * @param sm_event_t        event
 *
 * @return  SM_RC_END or SM_RC_CLEANUP
 *
 * @pre     (fcb->dcb not_eq NULL)
 * @pre     (event->data not_eq NULL)
 * @pre     (event->msg not_eq NULL)
 */
static sm_rcs_t
fsmdef_ev_connected_media_pend_feature (sm_event_t *event)
{
    fsm_fcb_t         *fcb    = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t      *dcb    = fcb->dcb;
    cc_feature_t      *msg    = (cc_feature_t *) event->msg;
    cc_srcs_t          src_id = msg->src_id;
    cc_features_t      ftr_id = msg->feature_id;
    cc_feature_data_t *data   = &(msg->data);
    sm_rcs_t           sm_rc = SM_RC_END;
    cc_feature_data_t  feature_data;
    cc_causes_t        cause;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    fsm_sm_ftr(ftr_id, src_id);

    switch (src_id) {
    case CC_SRC_UI:
    case CC_SRC_GSM:
        switch (msg->feature_id) {
        case CC_FEATURE_HOLD:
            /* If the line number is 0xFF, then this request
             * came from GSM during a Transfer. We want to
             * put the call on local hold only. We do not want
             * to send a cc_feature to the SIP stack because
             * that will cause an Invite Hold to go via SIP.
             * We don't want to put the other end on hold, just
             * ourselves.
             */
            if (msg->line == 0xFF) {
                sm_rc = fsm_hold_local_only(fcb);
            } else {
                if (msg->data_valid) {
                    sm_rc = fsm_connected_media_pend_local_hold(fcb, data);
                } else {
                    feature_data.hold.call_info.type = CC_FEAT_HOLD;
                    feature_data.hold.call_info.data.hold_resume_reason =
                        CC_REASON_NONE;
                    feature_data.hold.msg_body.num_parts = 0;
                    feature_data.hold.call_info.data.call_info_feat_data.swap = FALSE;
                    feature_data.hold.call_info.data.call_info_feat_data.protect = FALSE;
                    sm_rc = fsm_connected_media_pend_local_hold(fcb,
                                                                &feature_data);
                }
            }
            fsmdef_handle_join_pending(dcb);
            return (sm_rc);

        case CC_FEATURE_UPD_SESSION_MEDIA_CAP:
            dcb->video_pref = data->caps.support_direction;
            /* FALL THRU */
        case CC_FEATURE_UPD_MEDIA_CAP:
            /* We are already in the media update state */
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            return (SM_RC_END);

        case CC_FEATURE_REQ_PEND_TIMER_EXP:
            /*
             * Glare resolution timer expires.
             */
            if (FSM_CHK_FLAGS(dcb->flags, FSMDEF_F_HOLD_REQ_PENDING)) {
                /* There is a hold feature pending, send out hold instead */
                feature_data.hold.call_info.type = CC_FEAT_HOLD;
                feature_data.hold.call_info.data.hold_resume_reason =
                       dcb->hold_reason;
                feature_data.hold.msg_body.num_parts = 0;
                feature_data.hold.call_info.data.call_info_feat_data.swap = FALSE;
                feature_data.hold.call_info.data.call_info_feat_data.protect = FALSE;
                FSM_RESET_FLAGS(dcb->flags, FSMDEF_F_HOLD_REQ_PENDING);
                return (fsm_hold_local(fcb, &feature_data, FALSE));
            }

            /*
             * Check the possible media capbility changes.
             */
            (void)gsmsdp_update_local_sdp_media_capability(dcb, FALSE, FALSE);
            feature_data.resume.call_info.type = CC_FEAT_NONE;
            feature_data.resume.call_info.data.hold_resume_reason =
                                                            CC_REASON_NONE;
            feature_data.resume.msg_body.num_parts = 0;
            feature_data.resume.call_info.data.call_info_feat_data.swap = FALSE;
            feature_data.resume.call_info.data.call_info_feat_data.protect = FALSE;
            /* Encode SDP */
            cause = gsmsdp_encode_sdp_and_update_version(dcb,
                                                         &feature_data.resume.msg_body);

            if (cause != CC_CAUSE_OK) {
                FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
                return(fsmdef_release(fcb, cause, dcb->send_release));
            }

            /* Send feature request to SIP */
            cc_int_feature(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id, dcb->line,
                           CC_FEATURE_MEDIA, &feature_data);
            return (SM_RC_END);

        default:
            /*
             * The rest of the feature handles the same way as the
             * connected feature handling.
             */
            break;
        }                       /* switch (msg->feature_id) */
        break;

    default:
        /*
         * The rest of the feature handles the same way as the
         * connected feature handling.
         */
        break;
    }

    /*
     * Unhandled features are handled by the normal connected feature
     */
    return (fsmdef_ev_connected_feature(event));
}

/**
 *
 * Function to handles connected media update pending state feature ack
 * events.
 *
 * @param sm_event_t        event
 *
 * @return  SM_RC_END or SM_RC_CLEANUP
 *
 * @pre     (fcb->dcb not_eq NULL)
 * @pre     (event->data not_eq NULL)
 * @pre     (event->msg not_eq NULL)
 */
static sm_rcs_t
fsmdef_ev_connected_media_pend_feature_ack (sm_event_t *event)
{
    static const char fname[] = "fsmdef_ev_connected_media_pend_feature_ack";
    fsm_fcb_t        *fcb    = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t     *dcb    = fcb->dcb;
    cc_feature_ack_t *msg    = (cc_feature_ack_t *) event->msg;
    cc_features_t     ftr_id = msg->feature_id;
    cc_srcs_t         src_id = msg->src_id;
    cc_feature_data_t feature_data;
    sm_rcs_t          sm_rc = SM_RC_END;
    cc_msgbody_info_t *msg_body;
    cc_causes_t        cause;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    fsm_sm_ftr(ftr_id, src_id);
    switch (src_id) {
    case CC_SRC_SIP:
        switch (ftr_id) {
        case CC_FEATURE_MEDIA:
            /* Media update feature ack from SIP */

            if (msg->cause == CC_CAUSE_REQUEST_PENDING) {
                /*
                 * The glare condition occurs from the previously sent
                 * media update. Starts a request pending timer so that
                 * we retry.
                 */
                fsmdef_set_req_pending_timer(dcb);
                if (FSM_CHK_FLAGS(dcb->flags, FSMDEF_F_HOLD_REQ_PENDING)) {
                    /*
                     * Feature hold is pending, abort the media capability
                     * update and retry with hold instead.
                     */
                    FSM_RESET_FLAGS(dcb->flags, FSMDEF_F_HOLD_REQ_PENDING);
                    fsm_change_state(fcb, __LINE__, FSMDEF_S_HOLD_PENDING);
                }
                return (SM_RC_END);
            }

            /* Check for error code reported */
            if ((msg->cause != CC_CAUSE_NORMAL) &&
                (msg->cause != CC_CAUSE_OK)) {
                /* Unable to send media request */
                GSM_ERR_MSG(get_debug_string(FSMDEF_DBG2),
                            dcb->call_id, dcb->line, fname,
                            " Media request failed, cause= ", msg->cause);
                cc_call_state(dcb->call_id, dcb->line, CC_STATE_UNKNOWN, NULL);
                return(fsmdef_release(fcb, CC_CAUSE_ERROR, dcb->send_release));
            }

            msg_body = &msg->data.resume.msg_body;
            cause = gsmsdp_negotiate_answer_sdp(fcb, msg_body);
            if (cause != CC_CAUSE_OK) {
                return (fsmdef_release(fcb, cause, dcb->send_release));
            }

            /*
             * Check to see if we have a feature request pending
             */
            if (FSM_CHK_FLAGS(dcb->flags, FSMDEF_F_HOLD_REQ_PENDING)) {
                /* There is a hold feature pending, send out hold instead */
                feature_data.hold.call_info.type = CC_FEAT_HOLD;
                feature_data.hold.call_info.data.hold_resume_reason =
                    dcb->hold_reason;
                feature_data.hold.msg_body.num_parts = 0;
                feature_data.hold.call_info.data.call_info_feat_data.swap = FALSE;
                feature_data.hold.call_info.data.call_info_feat_data.protect = FALSE;
                FSM_RESET_FLAGS(dcb->flags, FSMDEF_F_HOLD_REQ_PENDING);
                sm_rc = fsm_hold_local(fcb, &feature_data, FALSE);
            } else {
                /*
                 * If spoof ringout is not being requested and we are
                 * currently playing spoof ringout, transition the LSM from
                 * the far end alerting to the connected state.
                 */
                if ((!dcb->spoof_ringout_requested) &&
                    (dcb->spoof_ringout_applied)) {
                    FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_CLR_SPOOF_APPLD),
                                 dcb->call_id, dcb->line, fname);

                    dcb->spoof_ringout_applied = FALSE;
                    cc_call_state(dcb->call_id, dcb->line, CC_STATE_CONNECTED,
                                  FSMDEF_CC_CALLER_ID);
                } else {
                    (void)cc_call_action(dcb->call_id, dcb->line,
                                         CC_ACTION_MEDIA, NULL);
                }

                /*
                 * Check any media capability changes that might occurs
                 * while we were in the middle of the previous transaction.
                 */
                sm_rc = fsmdef_transition_to_connected(fcb);
                if (g_dock_undock_event != MEDIA_INTERFACE_UPDATE_NOT_REQUIRED) {
                    if (is_gsmsdp_media_ip_updated_to_latest(dcb) == TRUE) {
                        ui_update_media_interface_change(dcb->line, dcb->call_id, MEDIA_INTERFACE_UPDATE_SUCCESSFUL);
                    } else {
                        DEF_DEBUG("We must have received another MEDIA_INTERFACE_UPDATE  events "
                            " while current MEDIA_INTERFACE_UPDATE event is in procoess. Sending re-invite again");
                        escalateDeescalate();
                    }
                }
            }
            return (sm_rc);

        default:
            break;
        }
        break;

    default:
        break;
    }

    /* call default feature ack handler to take common/default actions*/
    (void) fsmdef_ev_default_feature_ack(event);
    return (SM_RC_END);
}

static sm_rcs_t
fsmdef_ev_offhook (sm_event_t *event)
{
    fsm_fcb_t       *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t    *dcb = fcb->dcb;
    cc_action_data_t data;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    /*
     * User has gone offhook while using the speaker.
     */
    data.speaker.on = FALSE;
    (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_SPEAKER, &data);

    //lsm_set_active_call_id(dcb->call_id);

    return (SM_RC_END);
}


static sm_rcs_t
fsmdef_ev_connected_line (sm_event_t *event)
{
    fsm_fcb_t    *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t *dcb = fcb->dcb;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    cc_call_state(dcb->call_id, dcb->line, CC_STATE_CONNECTED,
                  FSMDEF_CC_CALLER_ID);

    /*
     * Handle media capability changes if there is before transition to
     * connected state.
     */
    return (fsmdef_transition_to_connected(fcb));
}


static sm_rcs_t
fsmdef_ev_onhook (sm_event_t *event)
{
    fsm_fcb_t       *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t    *dcb = fcb->dcb;
    sm_rcs_t         sm_rc;
    cc_action_data_t data;
    int              sdpmode = 0;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    /* currently this flag is only set by conference case. It signals
     * that onhook has been received, do not process it anymore.
     */
    if (dcb->onhook_received) {
        dcb->onhook_received = FALSE;
        return SM_RC_END;
    }

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));

    if (sdpmode) {
        if(dcb->ice_ufrag)
          cpr_free(dcb->ice_ufrag);

        if(dcb->ice_pwd)
          cpr_free(dcb->ice_pwd);
    }

    /*
     * If the user presses the ENDCALL softkey for an
     * incoming call set the release cause to Busy.
     */
    if (fcb->state == FSMDEF_S_INCOMING_ALERTING) {
        sm_rc = fsmdef_release(fcb, CC_CAUSE_BUSY, dcb->send_release);
    } else  {
        dcb->early_error_release = FALSE;
        sm_rc = fsmdef_release(fcb, CC_CAUSE_NORMAL, dcb->send_release);
    }

    if (sm_rc == SM_RC_CLEANUP) {
        /* This dcb has been cleaned up, do nothing more */
        return (sm_rc);
    } else if (fcb->state == FSMDEF_S_HOLDING ||
               fcb->state == FSMDEF_S_HOLD_PENDING) {
        data.ringer.on = TRUE;
        (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_RINGER, &data);
        sm_rc = SM_RC_END;
    } else {
        sm_rc = SM_RC_END;
    }

    return (sm_rc);
}


static sm_rcs_t
fsmdef_ev_release (sm_event_t *event)
{
    fsm_fcb_t    *fcb = (fsm_fcb_t *) event->data;
    cc_release_t *msg = (cc_release_t *) event->msg;
    fsmdef_dcb_t *dcb = fcb->dcb;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    dcb->send_release = FALSE;

    FSM_SET_FLAGS(dcb->msgs_rcvd, FSMDEF_MSG_RELEASE);

	if (msg->cause == CC_CAUSE_REMOTE_DISCONN_REQ_PLAYTONE) {

		fsmdef_set_call_info_cc_call_state(dcb, CC_STATE_CALL_FAILED, CC_CAUSE_REMOTE_DISCONN_REQ_PLAYTONE);

		/* what to return for return code */
		return(SM_RC_SUCCESS);
	} else {
		return (fsmdef_release(fcb, msg->cause, dcb->send_release));
	}
}


static sm_rcs_t
fsmdef_ev_releasing_release (sm_event_t *event)
{
    fsm_fcb_t    *fcb = (fsm_fcb_t *) event->data;
    cc_release_t *msg = (cc_release_t *) event->msg;
    fsmdef_dcb_t *dcb = fcb->dcb;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    /* see if the SIP stack has aborted this call early for some reason
     * If SIP brought this down, we are still offhook on the UI. We
     * need to ignore it, so that reorder can be played AND when the user
     * hangs up, then the UI will be driven to a clean state.
     */
    if (fcb->dcb->early_error_release == FALSE) {

        cc_int_release_complete(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id, dcb->line,
                                msg->cause, NULL);

        fsm_change_state(fcb, __LINE__, FSMDEF_S_IDLE);

        fsmdef_free_dcb(dcb);

        FSM_SET_FLAGS(dcb->msgs_rcvd, FSMDEF_MSG_RELEASE);

        fsm_release(fcb, __LINE__, msg->cause);

        return (SM_RC_CLEANUP);
    } else {
        FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SM_DEFAULT_EVENT));
        return (SM_RC_END);
    }
}


static sm_rcs_t
fsmdef_ev_releasing_feature (sm_event_t *event)
{
    fsm_fcb_t    *fcb    = (fsm_fcb_t *) event->data;
    cc_feature_t *msg    = (cc_feature_t *) event->msg;
    cc_srcs_t     src_id = msg->src_id;
    cc_features_t ftr_id = msg->feature_id;
    cc_causes_t   cause;
    sm_rcs_t      sm_rc  = SM_RC_END;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    fsm_sm_ftr(ftr_id, src_id);

    switch (ftr_id) {
    case CC_FEATURE_END_CALL:
        cause = fsmdef_get_cause(msg->data_valid, &(msg->data));

        /* Clean up call chain, no release sent */
        return (fsmdef_release(fcb, cause, FALSE));

    default:
        fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
        break;
    }

    return (sm_rc);
}


static sm_rcs_t
fsmdef_ev_releasing_onhook (sm_event_t *event)
{
    fsm_fcb_t *fcb = (fsm_fcb_t *) event->data;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    /* Clean up call chain, no release sent */
    return (fsmdef_release(fcb, CC_CAUSE_NORMAL, FALSE));
}


static sm_rcs_t
fsmdef_ev_release_complete (sm_event_t *event)
{
    fsm_fcb_t *fcb = (fsm_fcb_t *) event->data;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    if (fcb->dcb == NULL) {
        return (SM_RC_CLEANUP);
    }
    /* see if the SIP stack has aborted this call early for some reason
     * If SIP brought this down, we are still offhook on the UI. We
     * need to ignore it, so that reorder can be played AND when the user
     * hangs up, then the UI will be driven to a clean state.
     */
    if (fcb->dcb->early_error_release == FALSE) {

        fsm_change_state(fcb, __LINE__, FSMDEF_S_IDLE);

        fsmdef_free_dcb(fcb->dcb);

        fsm_release(fcb, __LINE__,
                    ((cc_release_complete_t *) (event->msg))->cause);

        return (SM_RC_CLEANUP);

    } else {
        FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SM_DEFAULT_EVENT));
        return (SM_RC_END);
    }
}

static sm_rcs_t
fsmdef_ev_hold_pending_feature (sm_event_t *event)
{
    fsm_fcb_t         *fcb     = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t      *dcb     = fcb->dcb;
    fsmcnf_ccb_t      *ccb     = NULL;
    cc_feature_t      *msg     = (cc_feature_t *) event->msg;
    cc_srcs_t          src_id  = msg->src_id;
    cc_features_t      ftr_id  = msg->feature_id;
    callid_t           call_id = msg->call_id;
    line_t             line    = msg->line;
    cc_feature_data_t *data = &(msg->data);
    cc_feature_data_t  feature_data;
    sm_rcs_t           sm_rc;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    fsm_sm_ftr(ftr_id, src_id);

    switch (src_id) {
    case (CC_SRC_UI):
    case (CC_SRC_GSM):
        switch (ftr_id) {

        case CC_FEATURE_UPD_SESSION_MEDIA_CAP:
             dcb->video_pref = data->caps.support_direction;
             break;

        case CC_FEATURE_RESUME:
            /*
             * We will not be able to resume this call since we are in the
             * hold pending state but we can place any other active call
             * on hold. Find the connected call (if there is one) and place
             * it on hold but not call if it is involved in a conference.
             */
            if (msg->data.resume.cause != CC_CAUSE_CONF) {
                if (fsmdef_wait_to_start_new_call(TRUE, src_id, call_id, line,
                                                  CC_FEATURE_RESUME, NULL)) {
                    ccb = fsmcnf_get_ccb_by_call_id(call_id);
                    if (ccb != NULL) {
                        ccb->cnf_ftr_ack = FALSE;
                    }
                }
            }
            return (SM_RC_END);

        case CC_FEATURE_REQ_PEND_TIMER_EXP:
            feature_data.hold.call_info.type = CC_FEAT_HOLD;
            feature_data.hold.call_info.data.hold_resume_reason =
                dcb->hold_reason;
            feature_data.hold.msg_body.num_parts = 0;
            feature_data.hold.call_info.data.call_info_feat_data.swap = FALSE;
            feature_data.hold.call_info.data.call_info_feat_data.protect = FALSE;
            sm_rc = fsm_hold_local(fcb, &feature_data, TRUE);
            return sm_rc;

        case CC_FEATURE_END_CALL:
            sm_rc = fsmdef_release_call(fcb, msg);
            return (sm_rc);

        default:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);

            break;
        }                       /* switch (ftr_id) */

        break;

    case (CC_SRC_SIP):
        switch (ftr_id) {
        case CC_FEATURE_MEDIA:
            return (fsmdef_remote_media(fcb, msg));

        case CC_FEATURE_CALLINFO:
            fsmdef_update_callinfo(fcb, msg);
            break;

        case CC_FEATURE_CALL_PRESERVATION:
            return (fsmdef_release(fcb, CC_CAUSE_NORMAL, dcb->send_release));

        case CC_FEATURE_NOTIFY:
            fsmdef_ev_notify_feature(msg, dcb);
            break;

        default:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;
        }                       /* switch (ftr_id) */

        break;

    default:
        fsmdef_sm_ignore_src(fcb, __LINE__, src_id);

        break;
    }                           /* switch (src_id) */

    return (SM_RC_END);
}

/**
 * feature ack event handler in hold pending state.
 *
 * @param[in] event   Pointer to sm_event_t structure for feature ack event.
 *
 * @return            Value of type sm_rcs_t to state machine
 *
 * @pre               (event not_eqs NULL) and
 *                    (event->data not_eqs NULL) and
 *                    ((fsm_fcb_t *)(event->data)->dcb not_eqs NULL)
 */
static sm_rcs_t
fsmdef_ev_hold_pending_feature_ack (sm_event_t *event)
{
    fsm_fcb_t         *fcb    = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t      *dcb    = fcb->dcb;
    cc_feature_ack_t  *msg    = (cc_feature_ack_t *) event->msg;
    cc_srcs_t          src_id = msg->src_id;
    cc_features_t      ftr_id = msg->feature_id;
    cc_causes_t        cause;
    cc_msgbody_info_t *msg_body;
    cc_feature_data_t  feature_data;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    fsm_sm_ftr(ftr_id, src_id);

    switch (src_id) {
    case (CC_SRC_SIP):
        switch (ftr_id) {
        case CC_FEATURE_RESUME:
            /*
             * This is feature ack for resume. We received resume
             * feature ack because the hold request was received while
             * we are waiting to resume i.e. was in the
             * resume pending state and resume request has been sent out.
             *
             * If the resume ack indicates glare, then ignore sending the
             * resume and transition to holding (we were in hold and
             * unable to send RESUME). Otherwise send hold out right away
             * if there is no other error.
             */
            fsm_sm_ftr(ftr_id, src_id);
            if (msg->cause == CC_CAUSE_REQUEST_PENDING) {
                /*
                 * The glare condition occurs from the previously sent
                 * resume transition to holding.
                 */
                (void)fsm_hold_local_only(fcb);
                break;
            }

            /* call default feature ack handler to take common/default actions*/
            (void) fsmdef_ev_default_feature_ack(event);

            if ((msg->cause != CC_CAUSE_NORMAL) &&
                (msg->cause != CC_CAUSE_OK)) {
                cc_call_state(dcb->call_id, dcb->line,
                              CC_STATE_UNKNOWN, NULL);
                return(fsmdef_release(fcb, CC_CAUSE_ERROR, dcb->send_release));
            }

            if (msg->data_valid != TRUE) {
                cc_call_state(dcb->call_id, dcb->line,
                              CC_STATE_UNKNOWN, NULL);
                return(fsmdef_release(fcb, CC_CAUSE_ERROR, dcb->send_release));
            }

            msg_body = &msg->data.resume.msg_body;
            cause = gsmsdp_negotiate_answer_sdp(fcb, msg_body);
            if (cause != CC_CAUSE_OK) {
                return(fsmdef_release(fcb, cause, dcb->send_release));
            }

            /*
             * HOLD can be sent now.
             */
            feature_data.hold.call_info.type = CC_FEAT_HOLD;
            feature_data.hold.call_info.data.hold_resume_reason =
                dcb->hold_reason;
            feature_data.hold.msg_body.num_parts = 0;
            feature_data.hold.call_info.data.call_info_feat_data.swap = FALSE;
            feature_data.hold.call_info.data.call_info_feat_data.protect = FALSE;
            fsm_hold_local(fcb, &feature_data, FALSE);
            break;

        default:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;
        }
        break;

    default:
        fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
        break;
    }

    return (SM_RC_END);
}


static sm_rcs_t
fsmdef_ev_holding_release (sm_event_t *event)
{
    cc_release_t *msg = (cc_release_t *) event->msg;
    fsm_fcb_t    *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t *dcb = fcb->dcb;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    if (msg->cause != CC_CAUSE_XFER_LOCAL) {
        fcb->dcb->send_release = FALSE;
    }

    FSM_SET_FLAGS(dcb->msgs_rcvd, FSMDEF_MSG_RELEASE);

    return (fsmdef_release(fcb, msg->cause, fcb->dcb->send_release));
}

static sm_rcs_t
fsmdef_ev_holding_onhook (sm_event_t *event)
{
    cc_onhook_t  *msg = (cc_onhook_t *) event->msg;
    fsm_fcb_t    *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t *dcb = fcb->dcb;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    if (!(msg->softkey)) {
        /* Meaning Hangup, ignore, a held call can't be hung up */
        FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SM_DEFAULT_EVENT));
        return (SM_RC_END);
    }

    /*
     * Meaning EndCall softkey is sent, take down
     * the call, this happens during failover.
     */
    FSM_SET_FLAGS(dcb->msgs_rcvd, FSMDEF_MSG_RELEASE);

    return (fsmdef_release(fcb, CC_CAUSE_NORMAL, dcb->send_release));
}

/**
  *
  * fsmdef_reversion_timeout - Triggers LSM for doing Reversion alerts.
  *
  * @param fsmdef_dcb_t     dcb for this call
  *
  * @return  none
  *
  * @pre     (dcb not_eq NULL)
  */

void fsmdef_reversion_timeout(callid_t call_id)
{

	int ret = CPR_SUCCESS;

	fsmdef_dcb_t *dcb = fsmdef_get_dcb_by_call_id(call_id) ;

    if ( (dcb == NULL ) || (dcb->fcb == NULL)) {
	    return;
    }

    // check that we are in HOLDING state before proceeding
    if ((dcb->fcb->state != FSMDEF_S_HOLDING) &&
        (dcb->fcb->state != FSMDEF_S_HOLD_PENDING)) {
        return;
    }

    if  (dcb->reversionInterval > 0) {
	    ret = cprStartTimer(dcb->revertTimer, dcb->reversionInterval * 1000, (void*)(long)call_id);
	}

	if ( ret == CPR_FAILURE ) {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_TMR_START_FAILED),
              dcb->call_id, dcb->line, "", "Reversion", cpr_errno);
        return;
    }

	cc_call_state(dcb->call_id, dcb->line, CC_STATE_HOLD_REVERT, NULL);

}


/**
  *
  * fsmdef_resume - Performs Resume Operation.
  *
  * @param sm_event_t     event
  *
  * @return  sm_rcs_t     SM_RC_END - indicating the event has been consumed
  *
  * @pre     (event not_eq NULL)
  */

static void
fsmdef_resume (sm_event_t *event)
{

    static const char fname[] = "fsmdef_resume";
    fsm_fcb_t         *fcb     = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t      *dcb     = fcb->dcb;
    fsmcnf_ccb_t      *ccb     = NULL;
    cc_feature_t      *msg     = (cc_feature_t *) event->msg;
    cc_feature_data_t *data    = &(msg->data);
    cc_srcs_t          src_id  = msg->src_id;
    callid_t           call_id = msg->call_id;
    line_t             line    = msg->line;
    cc_feature_data_t feature_data;
    cc_causes_t        cause;
    boolean            req_pending_tmr_running = FALSE;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    /*
     * Ignore the request if local hold is not active.
     */
    if (fsmdef_num_media_in_local_hold(dcb) == 0) {
        /*
         * No media in local held, should not happen here.
         */
        cc_int_feature_ack(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id,
                           dcb->line, CC_FEATURE_RESUME, NULL,
                           CC_CAUSE_NORMAL);
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG1), call_id, dcb->line,
                     fname, "resume media not in hold state\n");
        return;
    }

    (void) cprCancelTimer(dcb->revertTimer);
	dcb->reversionInterval = -1;
    /*
     * Make sure the connected call (if there is one) goes on hold,
     * but do not hold the connected call if it is involved in a
     * conference.
     */
    if (msg->data.resume.cause != CC_CAUSE_CONF) {
        if (fsmdef_wait_to_start_new_call(TRUE, src_id, call_id, line,
                                          CC_FEATURE_RESUME, (msg->data_valid ?
                                        data : NULL))) {

            ccb = fsmcnf_get_ccb_by_call_id(call_id);
            if (ccb != NULL) {
                ccb->cnf_ftr_ack = FALSE;
            }
            return ;
        }
    }

    /* Clear all media holding status */
    fsmdef_update_media_hold_status(dcb, NULL, FALSE);

    if (dcb->req_pending_tmr && cprIsTimerRunning(dcb->req_pending_tmr)) {
        req_pending_tmr_running = TRUE;
    }

    if (!req_pending_tmr_running) {
        /*
         * Reinitialize the local sdp to include all available codecs.
         * We do this because it is possible that our local list
         * may have been shortened to the one negotiated codec. This is
         * the case when we receive an incoming call, we negotiate a
         * single codec, copy it into the local sdp and send it back
         * in the connected msg.
         */
        (void)gsmsdp_update_local_sdp_media_capability(dcb, TRUE, FALSE);

        if (msg->data_valid) {
            feature_data.resume.call_info = data->resume.call_info;
        } else {
            feature_data.resume.call_info.type = CC_FEAT_RESUME;
            feature_data.resume.call_info.data.hold_resume_reason =
                CC_REASON_NONE;
            feature_data.resume.msg_body.num_parts = 0;
            feature_data.resume.call_info.data.call_info_feat_data.swap = FALSE;
            feature_data.resume.call_info.data.call_info_feat_data.protect = FALSE;
        }
        /* Encode SDP */
        cause = gsmsdp_encode_sdp_and_update_version(dcb,
                                                     &feature_data.resume.msg_body);
        if (cause != CC_CAUSE_OK) {
            FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
            (void)fsmdef_release(fcb, cause, dcb->send_release);
            return ;
        }
    }

    /*
     * If spoof ringout is currently requested, transition to the
     * far end alerting state instead of the connected state.
     */
    if (dcb->spoof_ringout_requested) {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG1),
                     dcb->call_id, dcb->line, fname,
                     "setting spoof_ringout_applied");

        dcb->spoof_ringout_applied = TRUE;
        cc_call_state(dcb->call_id, dcb->line,
                      CC_STATE_FAR_END_ALERTING, FSMDEF_CC_CALLER_ID);
    } else {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_CLR_SPOOF_APPLD),
                     dcb->call_id, dcb->line, fname);

        dcb->spoof_ringout_applied = FALSE;
        /* Start receiving but not transmit, before sending resume */
        (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_START_RCV,
                             NULL);
    }

    if (!req_pending_tmr_running) {
        cc_int_feature(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id, dcb->line,
                       CC_FEATURE_RESUME, &feature_data);
    }

    /*
     * We lock the UI until we learn the result of the resume request
     */
    fim_lock_ui(call_id);

    /* Wait for feature ack */
    fsm_change_state(fcb, __LINE__, FSMDEF_S_RESUME_PENDING);

	return ;

}

/**
  *
  * fsmdef_ev_holding_offhook - Handles offhook in for holding state.
  *
  * @param sm_event_t     event
  *
  * @return  sm_rcs_t     SM_RC_END - indicating the event has been consumed
  *
  * @pre     (event not_eq NULL)
  */

static sm_rcs_t
fsmdef_ev_holding_offhook (sm_event_t *event)
{
    fsm_fcb_t         *fcb     = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t      *dcb     = fcb->dcb;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    if (cprIsTimerRunning(dcb->revertTimer)) {
		// Off hook resume calls only if HR is active else ignore this event
		fsmdef_resume(event);
	}

	return  SM_RC_END;

}

static sm_rcs_t
fsmdef_ev_holding_feature (sm_event_t *event)
{
    fsm_fcb_t *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t *dcb = fcb->dcb;
    cc_feature_t *msg = (cc_feature_t *) event->msg;
    cc_srcs_t src_id = msg->src_id;
    cc_features_t ftr_id = msg->feature_id;
    cc_feature_data_t *data = &(msg->data);
    cc_feature_data_t feature_data;
    sm_rcs_t sm_rc;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    fsm_sm_ftr(ftr_id, src_id);

    switch (src_id) {
    case (CC_SRC_UI):
    case (CC_SRC_GSM):
        switch (ftr_id) {

        case CC_FEATURE_UPD_SESSION_MEDIA_CAP:
             dcb->video_pref = data->caps.support_direction;
             break;

        case CC_FEATURE_HOLD:
            if (msg->data_valid) {
                sm_rc = fsm_hold_local(fcb, data, FALSE);
            } else {
                feature_data.hold.call_info.type = CC_FEAT_HOLD;
                feature_data.hold.call_info.data.hold_resume_reason =
                    CC_REASON_NONE;
                feature_data.hold.msg_body.num_parts = 0;
                feature_data.hold.call_info.data.call_info_feat_data.swap = FALSE;
                feature_data.hold.call_info.data.call_info_feat_data.protect = FALSE;
                sm_rc = fsm_hold_local(fcb, &feature_data, FALSE);
            }
            fsmdef_handle_join_pending(dcb);
            return (sm_rc);

        case CC_FEATURE_HOLD_REVERSION:
            // Stop the timer for alert interval
            (void) cprCancelTimer(dcb->revertTimer);
            dcb->reversionInterval = -1;

			// Do not revert if alertInterval is negative
            if ( data->hold_reversion.alertInterval < 0 )
               return SM_RC_END;
			// interval timer of 0 implies continue to revert
			// Clamp the interval to be in MIN/MAX_HOLD_REVERSION_INTERVAL_TIMER
            if ( data->hold_reversion.alertInterval  > 0  &&
				 data->hold_reversion.alertInterval < MIN_HOLD_REVERSION_INTERVAL_TIMER )
               data->hold_reversion.alertInterval = MIN_HOLD_REVERSION_INTERVAL_TIMER;

            if ( data->hold_reversion.alertInterval > MAX_HOLD_REVERSION_INTERVAL_TIMER )
               data->hold_reversion.alertInterval = MAX_HOLD_REVERSION_INTERVAL_TIMER;

            dcb->reversionInterval = data->hold_reversion.alertInterval ;

            fsmdef_reversion_timeout(fcb->dcb->call_id);

            fsmdef_handle_join_pending(dcb);
            return SM_RC_END;

        case CC_FEATURE_RESUME:
            fsmdef_resume(event);
            break;

        case CC_FEATURE_END_CALL:
            sm_rc = fsmdef_release_call(fcb, msg);
            (void) cprCancelTimer(dcb->revertTimer);
            dcb->reversionInterval = -1;
            fsmdef_handle_join_pending(dcb);
            return (sm_rc);

        case CC_FEATURE_SELECT:
            if (msg->data_valid == FALSE) {
                fsmdef_select_invoke(dcb, NULL);
            } else {
                fsmdef_select_invoke(dcb, data);
            }
            return (SM_RC_END);

        case CC_FEATURE_B2B_JOIN:
            if (msg->data_valid == FALSE) {
                fsmdef_b2bjoin_invoke(dcb, NULL);
            } else {
                fsmdef_b2bjoin_invoke(dcb, data);
            }
            return (SM_RC_END);

        case CC_FEATURE_DIRTRXFR:
        case CC_FEATURE_UNDEFINED:
            fsm_display_feature_unavailable();

            fsmdef_handle_join_pending(dcb);
            return (SM_RC_END);

        case CC_FEATURE_REQ_PEND_TIMER_EXP:
        /* Ignore the request pending timer when in holding state */
        default:
            fsmdef_handle_join_pending(dcb);
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);

            break;
        }                       /* switch (ftr_id) */

        break;

    case (CC_SRC_SIP):
        switch (ftr_id) {
        case CC_FEATURE_MEDIA:
            return (fsmdef_remote_media(fcb, msg));

        case CC_FEATURE_CALLINFO:
            fsmdef_update_callinfo(fcb, msg);
            break;

        case CC_FEATURE_CALL_PRESERVATION:
            (void) cprCancelTimer(dcb->revertTimer);
            dcb->reversionInterval = -1;
            return (fsmdef_release(fcb, CC_CAUSE_NORMAL, dcb->send_release));

        case CC_FEATURE_NOTIFY:
            fsmdef_ev_notify_feature(msg, dcb);
            break;

        default:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);

            break;
        }                       /* switch (ftr_id) */

        break;

    default:
        fsmdef_sm_ignore_src(fcb, __LINE__, src_id);

        break;
    }                           /* switch (src_id) */

    return (SM_RC_END);
}


static sm_rcs_t
fsmdef_ev_holding_feature_ack (sm_event_t *event)
{
    static const char fname[] = "fsmdef_ev_holding_feature_ack";
    fsm_fcb_t        *fcb    = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t     *dcb    = fcb->dcb;
    cc_feature_ack_t *msg    = (cc_feature_ack_t *) event->msg;
    cc_srcs_t         src_id = msg->src_id;
    cc_features_t     ftr_id = msg->feature_id;
    cc_causes_t       cause  = msg->cause;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    switch (src_id) {
    case (CC_SRC_SIP):
        switch (ftr_id) {
        case CC_FEATURE_HOLD:
            if (cause == CC_CAUSE_REQUEST_PENDING) {
                /*
                 * If this is feature ack for hold and request is pending,
                 * set a request pending timer so that we retry the hold
                 * feature request.
                 */
                fsmdef_set_req_pending_timer(dcb);
                fsm_change_state(fcb, __LINE__, FSMDEF_S_HOLD_PENDING);
                return (SM_RC_END);
            }

            /* Check for error code reported */
            if ((cause != CC_CAUSE_NORMAL) &&
                (cause != CC_CAUSE_OK)) {
                /* Unable to send hold request */
                GSM_ERR_MSG(get_debug_string(FSMDEF_DBG2),
                            dcb->call_id, dcb->line, fname,
                            "HOLD request failed, cause= ", cause);
                cc_call_state(dcb->call_id, dcb->line, CC_STATE_UNKNOWN, NULL);
                return(fsmdef_release(fcb, CC_CAUSE_ERROR, dcb->send_release));
            }
            // update video_avail as we are not negotiating the answer below
            dcb->cur_video_avail = SDP_DIRECTION_INACTIVE;
            lsm_update_video_avail(dcb->line, dcb->call_id, dcb->cur_video_avail);
            break;

        default:
            fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;
        }
        break;

    default:
        fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
        break;
    }

    /* call default feature ack handler to take common/default actions */
    (void) fsmdef_ev_default_feature_ack(event);

    return (SM_RC_END);
}

static sm_rcs_t
fsmdef_ev_resume_pending_feature (sm_event_t *event)
{
    static const char fname[] = "fsmdef_ev_resume_pending_feature";
    fsm_fcb_t         *fcb     = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t      *dcb     = fcb->dcb;
    cc_feature_t      *msg     = (cc_feature_t *) event->msg;
    cc_srcs_t          src_id  = msg->src_id;
    cc_features_t      ftr_id  = msg->feature_id;
    callid_t           call_id = msg->call_id;
    cc_feature_data_t *data    = &(msg->data);
    cc_feature_data_t  feature_data;
    sm_rcs_t           sm_rc;
    cc_causes_t        cause;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    fsm_sm_ftr(ftr_id, src_id);

    switch (src_id) {
    case (CC_SRC_UI):
        switch (ftr_id) {
        case CC_FEATURE_UPD_SESSION_MEDIA_CAP:
             dcb->video_pref = data->caps.support_direction;
             break;

        case CC_FEATURE_END_CALL:
            fim_unlock_ui(call_id);
            sm_rc = fsmdef_release_call(fcb, msg);
            return (sm_rc);

        case CC_FEATURE_HOLD:
            /*
             * The UI should be locked but the hold event from UI here
             * can be the result of other call chain wants to put this
             * other call that may be in conected state or in resume
             * pending state on hold. One example, the user want to
             * resume a call that is on hold, that call chain will look
             * for any other call in connected state or call in resume
             * pending (about to be connected) and puts this call on hold.
             *
             * There are 2 choices here. If we are in this state and has
             * not sent a resume out to the network, then simply
             * transition to holding state. If resume has already
             * been sent and we are here waiting for resume pending ack,
             * then we can not send out hold immediately. In the later
             * case, go to hold pending state and wait for resume
             * feature ack to send hold out.
             *
             * In either case, UI can be unlocked since we are now
             * about to hold again.
             */
            fim_unlock_ui(dcb->call_id);
            if (dcb->req_pending_tmr &&
                cprIsTimerRunning(dcb->req_pending_tmr)) {
                /*
                 * Request timer is running, that means we are waiting
                 * to send resume or we already have sent one out
                 * but it resulted in glare condition and that we are waiting
                 * to resent it again. In this, just go back to hold state.
                 */
                (void) cprCancelTimer(dcb->req_pending_tmr);

                FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG1),
                             dcb->call_id, dcb->line, fname,
                             "Received Hold while waiting to send resume\n");

                /* Go back to holding without sending any thing out */
                (void)fsm_hold_local_only(fcb);
            } else {
                /*
                 * We have sent resume to the network and wait for
                 * for the feature ack. The glare condition can
                 * occur but we can only assume that resume was sent out
                 * at this point. We can not send out any more request
                 * until the result is known.
                 */
                if (msg->data_valid) {
                    dcb->hold_reason =
                         data->hold.call_info.data.hold_resume_reason;
                } else {
                    dcb->hold_reason = CC_REASON_NONE;
                }
                // since we are going to hold stop the media now
                // else the next new call or resume will result in 2 sets of media ports open
                (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_STOP_MEDIA,
                                 NULL);
                fsm_change_state(fcb, __LINE__, FSMDEF_S_HOLD_PENDING);
            }
            break;

        default:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;
        }
        break;

    case (CC_SRC_GSM):
        switch (ftr_id) {

        case CC_FEATURE_HOLD:
            sm_rc = fsm_hold_local_only(fcb);
            fim_unlock_ui(call_id);
            return (sm_rc);

        case CC_FEATURE_END_CALL:
            fim_unlock_ui(call_id);
            sm_rc = fsmdef_release_call(fcb, msg);
            return (sm_rc);

        case CC_FEATURE_REQ_PEND_TIMER_EXP:

            /*
             * Reinitialize the local sdp to include all available codecs.
             * We do this because it is possible that our local list
             * may have been shortened to the one negotiated codec. This is
             * the case when we receive an incoming call, we negotiate a
             * single codec, copy it into the local sdp and send it back
             * in the connected msg.
             */
            (void)gsmsdp_update_local_sdp_media_capability(dcb, TRUE, FALSE);

            feature_data.resume.call_info.type = CC_FEAT_RESUME;
            feature_data.resume.call_info.data.hold_resume_reason =
                CC_REASON_NONE;
            feature_data.resume.msg_body.num_parts = 0;
            feature_data.resume.call_info.data.call_info_feat_data.swap = FALSE;
            feature_data.resume.call_info.data.call_info_feat_data.protect = FALSE;
            /* Encode SDP */
            cause = gsmsdp_encode_sdp_and_update_version(dcb, &feature_data.resume.msg_body);
            if (cause != CC_CAUSE_OK) {
                FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
                return (fsmdef_release(fcb, cause, dcb->send_release));
            }

            /*
             * We lock the UI until we learn the result of the resume request.
             */
            fim_lock_ui(call_id);

            cc_int_feature(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id, dcb->line,
                           CC_FEATURE_RESUME, &feature_data);
            break;

        default:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;
        }                       /* switch (ftr_id) */

        break;

    case (CC_SRC_SIP):
        switch (ftr_id) {
        case CC_FEATURE_MEDIA:
            return (fsmdef_remote_media(fcb, msg));

        case CC_FEATURE_CALLINFO:
            fsmdef_update_callinfo(fcb, msg);
            break;

        case CC_FEATURE_CALL_PRESERVATION:
            fim_unlock_ui(call_id);
            return (fsmdef_release(fcb, CC_CAUSE_NORMAL, dcb->send_release));

        case CC_FEATURE_NOTIFY:
            fsmdef_ev_notify_feature(msg, dcb);
            break;

        case CC_FEATURE_UPDATE:
            /*
             * We only get an UPDATE feature event if we receive a medialess UPDATE.
             * This type of event only conveys UI updates that are processed with
             * a call info event. We do perform one check to see if we are currently
             * spoofing ringout. If we are and the spoof ringout requested flag
             * has been cleared, we tell the LSM to go connected which halts the
             * spoofed ringout.
             */
            if ((!dcb->spoof_ringout_requested) && (dcb->spoof_ringout_applied)) {
                FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_CLR_SPOOF_APPLD),
                             dcb->call_id, dcb->line, fname);

                dcb->spoof_ringout_applied = FALSE;
                cc_call_state(dcb->call_id, dcb->line, CC_STATE_CONNECTED,
                              FSMDEF_CC_CALLER_ID);
            }
            break;

        default:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;
        }                       /* switch (ftr_id) */

        break;

    default:
        fsmdef_sm_ignore_src(fcb, __LINE__, src_id);
        break;
    }                           /* switch (src_id) */

    return (SM_RC_END);
}

/**
 * feature ack event handler in resume pending state.
 *
 * @param[in] event   Pointer to sm_event_t structure for feature ack event.
 *
 * @return            Value of type sm_rcs_t to state machine
 *
 * @pre               (event not_eqs NULL) and
 *                    (event->data not_eqs NULL) and
 *                    ((fsm_fcb_t *)(event->data)->dcb not_eqs NULL)
 */
static sm_rcs_t
fsmdef_ev_resume_pending_feature_ack (sm_event_t *event)
{
    static const char fname[] = "fsmdef_ev_resume_pending_feature_ack";
    fsm_fcb_t         *fcb    = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t      *dcb    = fcb->dcb;
    cc_feature_ack_t  *msg    = (cc_feature_ack_t *) event->msg;
    cc_srcs_t          src_id = msg->src_id;
    cc_features_t      ftr_id = msg->feature_id;
    cc_causes_t        cause;
    cc_msgbody_info_t *msg_body;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    fsm_sm_ftr(ftr_id, src_id);

    switch (src_id) {
    case (CC_SRC_SIP):
        switch (ftr_id) {
         case CC_FEATURE_HOLD:
             /*
              * This can occur because SIP stack has not sent HOLD out yet
              * but the user has pressed resume since we already moved
              * to HOLD state as soon as the user pressed HOLD. Ignore the
              * HOLD feature ACK except if it indicates error. If we get
              * REQUEST_PENDING, we'll just move to connected state.
              */
             if (msg->cause == CC_CAUSE_REQUEST_PENDING) {
                cc_call_state(dcb->call_id, dcb->line, CC_STATE_CONNECTED,
                              FSMDEF_CC_CALLER_ID);
                fsm_change_state(fcb, __LINE__, FSMDEF_S_CONNECTED);
                return (SM_RC_END);
             }
             if ((msg->cause != CC_CAUSE_NORMAL) &&
                 (msg->cause != CC_CAUSE_OK)) {
                 cc_call_state(dcb->call_id, dcb->line,
                               CC_STATE_UNKNOWN, NULL);
                 return(fsmdef_release(fcb, CC_CAUSE_ERROR, dcb->send_release));
             }
             fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
             break;

        case CC_FEATURE_RESUME:
            /*
             * If this is feature ack for resume and request is pending,
             * set a request pending timer so that we retry the resume
             * feature request. Otherwise, unlock the UI and continue
             * processing.
             */
            fsm_sm_ftr(ftr_id, src_id);
            if ( msg->cause == CC_CAUSE_REQUEST_PENDING ) {
            	fsmdef_set_req_pending_timer(dcb);
                return (SM_RC_END);
            } else {
                fim_unlock_ui(dcb->call_id);
            }
            /* call default feature ack handler to take common/default actions*/
            (void) fsmdef_ev_default_feature_ack(event);

            if ((msg->cause == CC_CAUSE_SERV_ERR_UNAVAIL) &&
                (dcb->hold_reason == CC_REASON_MONITOR_UPDATE)) {
                FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG1),
                             dcb->call_id, dcb->line, fname,
                             "msg->cause == CC_CAUSE_SERV_ERR_UNAVAIL, unable to monitor update\n");
                return (fsmdef_transition_to_connected(fcb));
            }

            if ((msg->cause != CC_CAUSE_NORMAL) &&
                (msg->cause != CC_CAUSE_OK)) {
                cc_call_state(dcb->call_id, dcb->line,
                              CC_STATE_UNKNOWN, NULL);
                return(fsmdef_release(fcb, CC_CAUSE_ERROR, dcb->send_release));
            }

            if (msg->data_valid != TRUE) {
                cc_call_state(dcb->call_id, dcb->line,
                              CC_STATE_UNKNOWN, NULL);
                return(fsmdef_release(fcb, CC_CAUSE_ERROR, dcb->send_release));
            }

            msg_body = &msg->data.resume.msg_body;
            cause = gsmsdp_negotiate_answer_sdp(fcb, msg_body);
            if (cause != CC_CAUSE_OK) {
                return (fsmdef_release(fcb, cause, dcb->send_release));
            }
            if (!dcb->spoof_ringout_applied) {
                cc_call_state(dcb->call_id, dcb->line, CC_STATE_CONNECTED,
                          FSMDEF_CC_CALLER_ID);
            }
            /*
             * Handle media capability changes if there is before transition
             * to connected state to catch any changes during resume
             * transaction.
             */
            return (fsmdef_transition_to_connected(fcb));

        default:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;
        }
        break;

    default:
        fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
        break;
    }

    /* call default feature ack handler to take common/default actions */
    (void) fsmdef_ev_default_feature_ack(event);

    return (SM_RC_END);
}

static sm_rcs_t
fsmdef_ev_preserved_feature (sm_event_t *event)
{
    fsm_fcb_t         *fcb    = (fsm_fcb_t *) event->data;
    cc_feature_t      *msg    = (cc_feature_t *) event->msg;
    cc_srcs_t          src_id = msg->src_id;
    cc_features_t      ftr_id = msg->feature_id;
    sm_rcs_t           sm_rc;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    fsm_sm_ftr(ftr_id, src_id);

    switch (src_id) {
    case CC_SRC_UI:
    case CC_SRC_GSM:
        switch (msg->feature_id) {

        case CC_FEATURE_END_CALL:
            sm_rc = fsmdef_release_call(fcb, msg);
            return (sm_rc);

        default:
            fsmdef_sm_ignore_src(fcb, __LINE__, src_id);
            break;
        }

        break;

    case CC_SRC_SIP:
        switch (msg->feature_id) {
        case CC_FEATURE_HOLD:
        case CC_FEATURE_RESUME:
            sm_rc = fsmdef_release_call(fcb, msg);
            return (sm_rc);

        case CC_FEATURE_MEDIA:
            sm_rc = fsmdef_remote_media(fcb, msg);
            return (sm_rc);

        case CC_FEATURE_CALLINFO:
            fsmdef_update_callinfo(fcb, msg);
            break;

        default:
            fsmdef_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;
        }                       /* switch (msg->feature_id) */

        break;

    default:
        fsmdef_sm_ignore_src(fcb, __LINE__, src_id);
        break;
    }                           /* switch (src_id) */

    return (SM_RC_END);
}

cc_int32_t
fsmdef_show_cmd (cc_int32_t argc, const char *argv[])
{
    fsmdef_dcb_t *dcb;
    fsm_fcb_t    *fcb;
    int           i = 0;
    callid_t      call_id;
    unsigned long strtoul_result;
    char *strtoul_end;

    /*
     * Check if need help.
     */
    if ((argc == 2) && (argv[1][0] == '?')) {
        debugif_printf("show fsmdef [all|rel]\n");
    } else if ((argc == 1) || (strcmp(argv[1], "all") == 0)) {
        debugif_printf("\n-------- FSMDEF dcbs --------");
        debugif_printf("\ni   call_id  dcb         line");
        debugif_printf("\n-----------------------------\n");

        /*
         * Print info for all dcbs.
         */
        FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
            debugif_printf("%-2d  %-7d  0x%8p  %-4d\n",
                           i++, dcb->call_id, dcb, dcb->line);
        }
    } else if (strcmp(argv[1], "rel") == 0) {
        errno = 0;
        strtoul_result = strtoul(argv[2], &strtoul_end, 10);

        if (errno || argv[2] == strtoul_end || strtoul_result > USHRT_MAX) {
            debugif_printf("%s parse error of call_id %s", __FUNCTION__, argv[2]);
            return 0;
        }

        call_id = (callid_t) strtoul_result;

        debugif_printf("\nDEF %-4d/%d: releasing\n", call_id, 0);

        fcb = fsm_get_fcb_by_call_id_and_type(call_id, FSM_TYPE_DEF);
        if (fcb == NULL) {
            return (0);
        }

        (void)fsmdef_release(fcb, CC_CAUSE_NORMAL, fcb->dcb->send_release);
    }
    return (0);
}

/**
 *
 * This function is called to determine if the phone
 * is in a state where autoAnswer is supported. Even
 * if the feature is enabled on the line being called,
 * the phone may be in a state that does not allow it.
 *
 * @param autoAnswerAlt   to indicate what states auto answer enabled.
 * @param myCallId     call_id
 *
 * @return  TRUE or FALSE
 *
 */
static int
fsmdef_check_auto_answer_allowed (int autoAnswerAlt, callid_t myCallId)
{
    fsmdef_dcb_t *dcb;

    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
        if (dcb->call_id != myCallId && dcb->fcb != NULL) {
            /*
             * autoAnswer is disabled if there is an incoming or an
             * outgoing call that has not reached the connected state
             * regardless of the value of autoAnswerAlt.
             */
            if ((dcb->fcb->state == FSMDEF_S_COLLECT_INFO)        ||
                (dcb->fcb->state == FSMDEF_S_CALL_SENT)           ||
                (dcb->fcb->state == FSMDEF_S_OUTGOING_PROCEEDING) ||
                (dcb->fcb->state == FSMDEF_S_KPML_COLLECT_INFO)   ||
                (dcb->fcb->state == FSMDEF_S_CONNECTING)          ||
                (dcb->fcb->state == FSMDEF_S_JOINING)) {
                return (FALSE);
            }

            /*
             * If autoAnswerAlt is 1, then autoAnswer is also
             * disabled if there is a connected call or a call
             * on hold.
             */
            if (autoAnswerAlt == 1) {
                if ((dcb->fcb->state == FSMDEF_S_CONNECTED) ||
                    (dcb->fcb->state == FSMDEF_S_PRESERVED) ||
                    (dcb->fcb->state == FSMDEF_S_RESUME_PENDING) ||
                    (dcb->fcb->state == FSMDEF_S_CONNECTED_MEDIA_PEND) ||
                    (dcb->fcb->state == FSMDEF_S_HOLDING) ||
                    (dcb->fcb->state == FSMDEF_S_OUTGOING_ALERTING) ||
                    (dcb->fcb->state == FSMDEF_S_INCOMING_ALERTING) ||
                    (dcb->fcb->state == FSMDEF_S_HOLD_PENDING)) {

                    return (FALSE);
                }
            } else if (autoAnswerAlt == 0) {
                if ((dcb->fcb->state == FSMDEF_S_CONNECTED) ||
                    (dcb->fcb->state == FSMDEF_S_PRESERVED) ||
                    (dcb->fcb->state == FSMDEF_S_CONNECTED_MEDIA_PEND) ||
                    (dcb->fcb->state == FSMDEF_S_OUTGOING_ALERTING)) {

                    return (FALSE);
                }
            }
        }
    }

    return (TRUE);
}

/**
 *
 * This function is called when a call is received on a
 * line that has auto answer enabled and the autoAnswer
 * timer has expired.
 *
 * @param data   pointer to opaque data
 *
 * @return  none
 *
 * @pre     (fcb and fcb->dcb not_eq NULL)
 * @pre     (data should be fcb data)
 */

void
fsmdef_auto_answer_timeout (void *data)
{
    static const char fname[] = "fsmdef_auto_answer_timeout";
    int        autoAnswerAlternate = 0;
    int        autoAnswerOverride = 0;
    int        headSetActive = 0;
    int        speakerEnabled = 1;
    callid_t   call_id;
    char       autoAnswerMode[MAX_LINE_AUTO_ANS_MODE_SIZE];
    fsmdef_dcb_t *dcb;

    call_id = (callid_t)(long)data;
    if (call_id == CC_NO_CALL_ID) {
        /* Invalid call id */
        GSM_ERR_MSG(get_debug_string(FSMDEF_DBG1), 0, 0, fname, "invalid data");
        return;
    }

    /* Retrieve dcb from call id */
    dcb = fsmdef_get_dcb_by_call_id(call_id);
    if (dcb == NULL) {
        /*
         * The only way this should happen is for the
         * call to be released without cancelling the
         * autoAnswer timer.
         */
        FSM_DEBUG_SM(DEB_F_PREFIX"AutoAnswer timer expired but no dcb was found.", DEB_F_PREFIX_ARGS(FSM, fname));
        return;
    }

    /*
     * The device-based config parameter autoAnswerIdleAlternate
     * determines what state the phone must be in for autoAnswer
     * feature to be enabled.
     */
    config_get_value(CFGID_AUTOANSWER_IDLE_ALTERNATE, &autoAnswerAlternate,
                     sizeof(autoAnswerAlternate));

    if (fsmdef_check_auto_answer_allowed(autoAnswerAlternate, dcb->call_id)) {
        /*
         * Is headset active? No need to check if headset has been disabled
         * on the phone because the headset will never be active if it has.
         */
        headSetActive = platGetAudioDeviceStatus(VCM_AUDIO_DEVICE_HEADSET);

        /*
         * If function call fails, just assume headset is not active
         * and drive on
         */
        if (headSetActive == -1) {
            FSM_DEBUG_SM(DEB_F_PREFIX"platGetAudioDeviceStatus() for headset failed.", DEB_F_PREFIX_ARGS(FSM, fname));
            headSetActive = 0;
        }

        /*
         * Determine where the audio should be sent
         */
        config_get_line_string(CFGID_LINE_AUTOANSWER_MODE, autoAnswerMode,
                               dcb->line, sizeof(autoAnswerMode));

        /*
         * Check to see if the speaker has been disabled
         */
        config_get_value(CFGID_SPEAKER_ENABLED, &speakerEnabled,
                            sizeof(speakerEnabled));

        /*
         * autoAnswer using speaker
         */
        if (strcasestr(autoAnswerMode, "speaker")) {
            /*
             * Speaker has not been disabled, normal processing.
             */
            if (speakerEnabled) {
                /*
                 * Enable audio path to speaker if headset is not being used.
                 */
                if (!headSetActive) {
                    platSetSpeakerMode(TRUE);
                }
                /* Send offhook event to GSM */
                cc_int_feature(CC_SRC_UI, CC_SRC_GSM, dcb->call_id,
                               dcb->line, CC_FEATURE_ANSWER, NULL);
            } else {

               /*
                * Speaker is disabled, but autoAnswer is enabled and says to
                * use the speaker. Check the device-based override
                * parameter to see what action to take. If override is
                * set just return and let normal call processing happen.
                * If not, terminate the call. The best SIP response we
                * could come up in this situation is to send a 480
                * Temporarily Unavailable.
                */
                config_get_value(CFGID_AUTOANSWER_OVERRIDE, &autoAnswerOverride,
                                 sizeof(autoAnswerOverride));
                if (!autoAnswerOverride) {
                    fsmdef_end_call(dcb, CC_TEMP_NOT_AVAILABLE);
                }

            }
        } else if (strcasestr(autoAnswerMode, "headset")) {
            /*
             * autoAnswer using headset. If headset is not enabled just let
             * the phone ring normally.
             */
            if (headSetActive) {
                /* Send offhook event to GSM */
                cc_int_feature(CC_SRC_UI, CC_SRC_GSM, dcb->call_id,
                               dcb->line, CC_FEATURE_ANSWER, NULL);
            }
        } else {
            /*
             * Unknown autoAnswer mode
             */
            FSM_DEBUG_SM(DEB_F_PREFIX"Unknown autoAnswer Mode: %s  AutoAnswer is disabled.",
                         DEB_F_PREFIX_ARGS(FSM, fname), autoAnswerMode);
        }
    }
}

/*
 * fsmdef_b2bjoin_invoke
 *
 * Description:
 *    This function implements the b2bjoin feature invocation.
 *
 * Parameters:
 *    fsmdef_dcb_t *dcb - dcb associated with this call
 *
 * Returns: None
 */
static void
fsmdef_b2bjoin_invoke (fsmdef_dcb_t *dcb, cc_feature_data_t *join_data)
{
    cc_feature_data_t feature_data;
    int join_across_lines;
    cc_uint32_t major_ver;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    // Disable Join if the CUCM doesn't support it
    platGetSISProtocolVer(&major_ver, NULL, NULL, NULL);

    if ( major_ver < SIS_PROTOCOL_MAJOR_VERSION_UNISON ) {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG1), dcb->call_id, dcb->line,
            "fsmdef_b2bjoin_invoke", "Major sis is small than SIS_PROTOCOL_MAJOR_VERSION_UNISON, so, B2BJOIN is disabled");
        fsm_display_feature_unavailable();
        fsmdef_sm_ignore_ftr(dcb->fcb, __LINE__, CC_FEATURE_B2B_JOIN);
        return;
    }

    config_get_value(CFGID_JOIN_ACROSS_LINES,
                     &join_across_lines, sizeof(join_across_lines));
    /*
     * Invoke B2BJoin feature towards SIP. It will send a REFER to CCM
     */
    if (join_data) {
        cc_int_feature(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id,
                       dcb->line, CC_FEATURE_B2B_JOIN, join_data);
    } else {

        if ((g_b2bjoin_pending == FALSE) && (dcb->fcb->state == FSMDEF_S_HOLDING)
            && ((fsmdef_get_connected_call() != NULL) ||
                (fsmdef_get_alertingout_call() != NULL))) {
             /* Single Button Join case
              * If Join is pressed on a held call while there is
              * an active(connected or alerting) call, that completes the Join
              */
             feature_data.b2bjoin.b2bjoin_callid = dcb->call_id;
             feature_data.b2bjoin.b2bjoin_joincallid = dcb->call_id;
             cc_int_feature(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id,
                            dcb->line, CC_FEATURE_B2B_JOIN, &feature_data);
             return;
        }

        if ((g_numofselected_calls == 0) ||
            ((g_b2bjoin_pending == FALSE) && (join_across_lines == JOIN_ACROSS_LINES_DISABLED) &&
            (fsmdef_are_there_selected_calls_onotherline(dcb->line) == TRUE))) {
            dcb->active_feature =  CC_FEATURE_B2B_JOIN;
            feature_data.select.select = TRUE;
            fsmdef_select_invoke(dcb,&feature_data);
            fsm_display_use_line_or_join_to_complete();
            return;
        }
        if (g_b2bjoin_pending) {
	                if (join_across_lines == JOIN_ACROSS_LINES_DISABLED) {
	                    if (fsmdef_are_join_calls_on_same_line(dcb->line) == FALSE) {

	                        fsm_display_use_line_or_join_to_complete();
	                        g_b2bjoin_pending = FALSE;
	                        g_b2bjoin_callid  = CC_NO_CALL_ID;
	                        return;
	                    }
	                }
	                if (dcb->call_id== g_b2bjoin_callid) {
	                    /* If join is pending, pressing Join on the same call will cancel it */
	                    g_b2bjoin_pending = FALSE;
	                    g_b2bjoin_callid  = CC_NO_CALL_ID;
	                    /* Remove the check mark from it*/
	                    cc_int_feature(CC_SRC_UI, CC_SRC_GSM, dcb->call_id,
	                                   dcb->line, CC_FEATURE_SELECT, NULL);
	                    return;
	                }
	                feature_data.b2bjoin.b2bjoin_callid = dcb->call_id;
                        if( g_b2bjoin_callid == CC_NO_CALL_ID ){
                            feature_data.b2bjoin.b2bjoin_joincallid = dcb->call_id;
                        }
                        else{
                            feature_data.b2bjoin.b2bjoin_joincallid = g_b2bjoin_callid;
                        }

	                cc_int_feature(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id,
	                               dcb->line, CC_FEATURE_B2B_JOIN, &feature_data);

        } else {
                  if ((g_numofselected_calls == 1) && (dcb->selected)) {
                    /*
                     * If this is only one selected call, pressing
                     * Join on it will start a new join operation
                     */
	                    g_b2bjoin_pending = TRUE;
	                    g_b2bjoin_callid  = dcb->call_id;
	                    fsm_display_use_line_or_join_to_complete();
	                    return;
                   }
	                feature_data.b2bjoin.b2bjoin_callid = dcb->call_id;
	                feature_data.b2bjoin.b2bjoin_joincallid = dcb->call_id;
	                cc_int_feature(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id,
	                               dcb->line, CC_FEATURE_B2B_JOIN, &feature_data);
        }
    }
    g_b2bjoin_pending = FALSE;
    g_b2bjoin_callid  = CC_NO_CALL_ID;
}

/*
 * fsmdef_select_invoke
 *
 *
 * Description:
 *    This function implements the join feature invocation.
 *
 * Parameters:
 *    fsmdef_dcb_t *dcb - dcb associated with this call
 *
 * Returns: None
 */
static void
fsmdef_select_invoke (fsmdef_dcb_t *dcb, cc_feature_data_t *select_data)
{
    cc_feature_data_t  feature_data;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    if (dcb->select_pending) {
        /* We have sent a select but have not received a
         * response yet, so diallow further selects till then
         */
        return;
    }
    if (select_data) {
        feature_data.select.select = select_data->select.select;
    } else {
        if (dcb->selected == TRUE) {
            feature_data.select.select = FALSE;
        } else {
            feature_data.select.select = TRUE;
        }
    }
    if ((g_b2bjoin_pending) && (dcb->call_id== g_b2bjoin_callid)) {
        /* If join is pending, pressing Select on the same call cancels join*/
         g_b2bjoin_pending = FALSE;
         g_b2bjoin_callid  = CC_NO_CALL_ID;
    }
    dcb->select_pending = TRUE;;
    cc_int_feature(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id,
                   dcb->line, CC_FEATURE_SELECT, &feature_data);
}

/*
 * handle_join_pending
 *
 *
 * Description:
 *    This function checks if the join_pending flag
 *    needs to be cleared.due to the invocation of other features
 *
 * Parameters:
 *    fsmdef_dcb_t *dcb - dcb associated with this call
 *
 * Returns: None
 */
static void fsmdef_handle_join_pending (fsmdef_dcb_t *dcb)
{
    if ((g_b2bjoin_pending) && (dcb->call_id == g_b2bjoin_callid)) {
         /* If join is pending, invoking any other feature cancels join*/
         g_b2bjoin_pending = FALSE;
         g_b2bjoin_callid  = CC_NO_CALL_ID;
    }
}


/*
 * fsmdef_process_dialstring_for_callfwd
 *
 * Description:
 *    This function processes the dialstring event for callfwd.
 *
 * Parameters:
 *    sm_event_t *event - data associated with this call/dialstring
 *
 * Returns: sm_rcs_t
 */
static sm_rcs_t
fsmdef_process_dialstring_for_callfwd (sm_event_t *event)
{
    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    /*
     * For ccm case just return success.
     * It will continue the call by sending INVITE
     */
    return (SM_RC_SUCCESS);
}


/*
 * fsmdef_process_cfwd_softkey_event
 *
 * Description:
 *    This function processes the cfwd softkey press event for callfwd.
 *
 * Parameters:
 *    sm_event_t *event - data associated with this call/dialstring
 *
 * Returns: sm_rcs_t
 */
static sm_rcs_t
fsmdef_process_cfwd_softkey_event (sm_event_t *event)
{
    fsm_fcb_t       *fcb    = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t    *dcb    = fcb->dcb;
    cc_feature_t    *msg    = (cc_feature_t *) event->msg;
    cc_features_t    ftr_id = msg->feature_id;
    cc_feature_data_t *ftr_data = &(msg->data);
    cc_action_data_t cc_data;
    int              skMask[MAX_SOFT_KEYS];

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    // check if callfwd is active (i.e. set previously)
    if (lsm_check_cfwd_all_ccm(dcb->line)) {
        // call fsmdef_dialstring() version of the function specific
        // to cfwd feature
        return (fsmdef_cfwd_clear_ccm(fcb));
    }


    // code from here on is common to both modes/feature_id
    if (fcb->state == FSMDEF_S_IDLE) {

         /*
          * The user is attempting to start a cfwdall call on a specific line:
          * 1. need to place the connected call (if there is one) on hold,
          * 2. clear any outgoing ringing calls,
          * 3. initiate this call.
          */
         if (fsmdef_wait_to_start_new_call(TRUE, CC_SRC_GSM, dcb->call_id, dcb->line, ftr_id, ftr_data))
         {
             dcb->active_feature = CC_FEATURE_NONE;
             return (SM_RC_END);
         }


        // go offhook and then move to dialing state to collect digits
        //only contains <cfwdall-set> in initial NOTIFY to CUCM
        fsmdef_notify_hook_event(fcb,CC_MSG_OFFHOOK,
                               ftr_data->newcall.global_call_id,
                               ftr_data->newcall.prim_call_id,
                               ftr_data->newcall.hold_resume_reason,
                               CC_MONITOR_NONE,
                               (ftr_id == CC_FEATURE_CFWD_ALL) ? CFWDALL_SET:CFWDALL_NONE);
        cc_call_state(dcb->call_id, dcb->line, CC_STATE_OFFHOOK,
                      ((cc_state_data_t *) (&(dcb->caller_id))));

        fsmdef_call_cc_state_dialing(dcb, FALSE);

        // stop the dial tone for parity with SCCP phone behavior
        cc_data.tone.tone = VCM_INSIDE_DIAL_TONE;
        (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_STOP_TONE,
                             &cc_data);

        // give different (zip-zip) tone rather than dial tone. zip_zip
        // is not a permanent tone so it will end on it's own without
        // us telling media termination to stop playing the tone.
        cc_data.tone.tone = VCM_ZIP_ZIP;
        (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_PLAY_TONE,
                             &cc_data);

        // move to collect digit info state
        fsm_change_state(fcb, __LINE__, FSMDEF_S_COLLECT_INFO);
    } else { // we must be in FSMDEF_S_COLLECT_INFO state
        // stop the dial tone for parity with SCCP phone behavior
        cc_data.tone.tone = VCM_INSIDE_DIAL_TONE;
        (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_STOP_TONE,
                             &cc_data);

        // give different (zip-zip) tone rather than dial tone. zip_zip
        // is not a permanent tone so it will end on it's own without
        // us telling media termination to stop playing the tone.
        cc_data.tone.tone = VCM_ZIP_ZIP;
        (void)cc_call_action(dcb->call_id, dcb->line, CC_ACTION_PLAY_TONE,
                             &cc_data);
    }
    ui_control_feature(dcb->line, dcb->call_id, skMask, 1, FALSE);

    return (SM_RC_END);
}

/**
 * This function is a reduced version of the fsmdef_dialstring() function.
 * It is customized for sending INVITE out to CCM to clear CFA state.
 *
 * @param[in] fcb     The pointer to the fsm_fcb_t structure of this
 *                    call chain.
 *
 * @pre               (fcb not_eq NULL)
 *
 * @return            sm_rsc_t indicates whether the execution of
 *                    next statmachine.
 */
static sm_rcs_t
fsmdef_cfwd_clear_ccm (fsm_fcb_t *fcb)
{
    fsmdef_dcb_t     *dcb = fcb->dcb;
    cc_causes_t       cause;
    cc_msgbody_info_t msg_body;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    // to clear CFA in CCM mode... only put service uri... no dialstring.
    fsmdef_append_dialstring_to_feature_uri(dcb, NULL);

    // From here on all we need to do is send INVITE out.
    // Since, its not a real call there is no need to update UI etc.
    // Response to this call will be 5xx so it will be released by the SIP stack.
    cause = gsmsdp_create_local_sdp(dcb, FALSE, TRUE, TRUE, TRUE, TRUE);
    if (cause != CC_CAUSE_OK) {
        FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
        return (fsmdef_release(fcb, cause, dcb->send_release));
    }

    /* Build SDP for sending out */
    cause = gsmsdp_encode_sdp_and_update_version(dcb, &msg_body);
    if (cause != CC_CAUSE_OK) {
        FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
        return (fsmdef_release(fcb, cause, dcb->send_release));
    }
    cc_int_setup(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id, dcb->line,
                 &(dcb->caller_id), dcb->alert_info, VCM_INSIDE_RING,
                 VCM_INSIDE_DIAL_TONE, NULL, NULL, FALSE, NULL, &msg_body);

    /*
     * Since we are sending setup to UI we will also have to send
     * release to it to for sip stack to clean up the call
     */
    dcb->send_release = TRUE;

    FSM_SET_FLAGS(dcb->msgs_sent, FSMDEF_MSG_SETUP);

    fsm_change_state(fcb, __LINE__, FSMDEF_S_CALL_SENT);
    return (SM_RC_END);
}

/*
 * fsmdef_append_dialstring_to_feature_uri
 *
 * Description:
 *    This function appends dialstring to feature URI.
 *
 * Parameters:
 *    fsmdef_dcb_t *dcb, char *dialstring
 *
 * Return Value: none
 *
 * Note: TNP specific implementation.
 */
static void
fsmdef_append_dialstring_to_feature_uri (fsmdef_dcb_t *dcb,
                                        const char *dialstring)
{
    char service_uri[MAX_URL_LENGTH];

    service_uri[0] = '\0';

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    if (dcb == NULL) {
        FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_INVALID_DCB),
                     __FUNCTION__);
        return;
    }

    switch (dcb->active_feature) {
    case CC_FEATURE_CFWD_ALL:
        config_get_string(CFGID_CALL_FORWARD_URI, service_uri,
                          sizeof(service_uri));
        break;
    default:
        // do nothing
        break;
    }

    if (service_uri[0] != NUL) {
        dcb->caller_id.called_number =
            strlib_update(dcb->caller_id.called_number, service_uri);
        if (dialstring && dialstring[0]) {
            dcb->caller_id.called_number =
                strlib_append(dcb->caller_id.called_number, "-");
            dcb->caller_id.called_number =
                strlib_append(dcb->caller_id.called_number, dialstring);
        }
    } else {
        FSM_DEBUG_SM(DEB_F_PREFIX"Configured Feature/Service URI Not Found For Feature[%d]", DEB_F_PREFIX_ARGS(FSM, "fsmdef_append_dialstring_to_feature_uri"), (int)dcb->active_feature);

        if (dialstring && dialstring[0]) {
            dcb->caller_id.called_number =
                strlib_update(dcb->caller_id.called_number, dialstring);
        }
    }
}


/*
 * fsmdef_is_feature_uri_configured
 *
 * Description:
 *    This function checks is a feature URI is configured.
 *
 * Parameters:
 *    cc_features_t ftr_id
 *
 * Return Value: TRUE or FALSE
 *
 * Note: TNP specific implementation.
 */
static boolean
fsmdef_is_feature_uri_configured (cc_features_t ftr_id)
{
    char service_uri[MAX_URL_LENGTH];

    service_uri[0] = '\0';

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    switch (ftr_id) {
    case CC_FEATURE_CFWD_ALL:
        config_get_string(CFGID_CALL_FORWARD_URI, service_uri,
                          sizeof(service_uri));
        break;
    default:
        break;
    }

    if (service_uri[0] != NUL) {
        return TRUE;
    }

    FSM_DEBUG_SM(DEB_F_PREFIX"Configured Feature/Service URI Not Found For Feature[%d]", DEB_F_PREFIX_ARGS(FSM, "fsmdef_is_feature_uri_configured"), (int)ftr_id);
    return FALSE;
}

/*
 * fsmdef_check_if_ok_for_dial_call
 *
 * Description:
 *    This function checks if there is a call in collecting info state
 *    on a given line.
 *
 * Parameters:
 *    line_t line
 *
 * Returns: TRUE or FALSE
 */
boolean
fsmdef_check_if_ok_for_dial_call (line_t line)
{

    fsmdef_dcb_t *dcb;

    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
        if (((dcb->line == line) && (dcb->call_id != CC_NO_CALL_ID)) &&
            (dcb->fcb != NULL) &&
            ((dcb->fcb->state == FSMDEF_S_COLLECT_INFO) ||
             (dcb->fcb->state == FSMDEF_S_CALL_SENT) ||
             (dcb->fcb->state == FSMDEF_S_OUTGOING_PROCEEDING) ||
             (dcb->fcb->state == FSMDEF_S_KPML_COLLECT_INFO))) {
            return (TRUE);
        }
    }
    return (FALSE);
}

/*
 * fsmdef_check_if_ok_to_ans_call
 *
 * Description:
 *    Checks if given call is in ringing state.
 *
 * Parameters:
 *    line
 *    call_id
 *
 * Returns: TRUE or FALSE
 */

boolean
fsmdef_check_if_ok_to_ans_call (line_t line, callid_t call_id)
{
    fsmdef_dcb_t   *dcb;

    dcb = fsmdef_get_dcb_by_call_id(call_id);

    if (dcb == NULL) {
        return (FALSE);
    }

    if ((dcb->line != line) || ((dcb->fcb != NULL) && (dcb->fcb->state != FSMDEF_S_INCOMING_ALERTING))) {
        return (FALSE);
    }

    return (TRUE);
}

/*
 * fsmdef_check_if_ok_to_hold_call
 *
 * Description:
 *    Checks if the requested call is in connected state.
 *
 * Parameters:
 *    line
 *    call_id
 *
 * Returns: TRUE - if there is a connected or resume pending call
 *          FALSE - if the call is not in above state
 */
boolean
fsmdef_check_if_ok_to_hold_call (line_t line, callid_t call_id)
{
    fsmdef_dcb_t *dcb;

    dcb = fsmdef_get_dcb_by_call_id(call_id);

    if (dcb == NULL) {

        return (FALSE);
    }

    if ((dcb->line != line) ||
        ((dcb->fcb != NULL) &&
        ((dcb->fcb->state != FSMDEF_S_CONNECTED) &&
         (dcb->fcb->state != FSMDEF_S_CONNECTED_MEDIA_PEND) &&
         (dcb->fcb->state != FSMDEF_S_RESUME_PENDING)))) {

        return (FALSE);
    }

    return (TRUE);
}

/*
 * fsmdef_check_if_ok_to_resume_call
 *
 * Description:
 *    Checks if the requested call is in held state.
 *
 * Parameters:
 *    line
 *    call_id
 *
 * Returns: TRUE or FALSE
 */
boolean
fsmdef_check_if_ok_to_resume_call (line_t line, callid_t call_id)
{
    fsmdef_dcb_t *dcb;

    dcb = fsmdef_get_dcb_by_call_id(call_id);

    if (dcb == NULL) {
        return (FALSE);
    }

    if ((dcb->line != line) || ((dcb->fcb != NULL) &&(dcb->fcb->state != FSMDEF_S_HOLDING))) {
        return (FALSE);
    }

    return (TRUE);
}

/*
 * fsmdef_check_if_ok_to_run_feature
 *
 * Description:
 *    Checks if it is ok to run features on the call.
 *
 * Parameters:
 *    line
 *    call_id
 *
 * Returns: TRUE or FALSE
 */
boolean
fsmdef_check_if_ok_to_run_feature (line_t line, callid_t call_id)
{
    fsmdef_dcb_t *dcb;

    dcb = fsmdef_get_dcb_by_call_id(call_id);

    if (dcb == NULL) {
        return (FALSE);
    }

    if ((dcb->line != line) ||
        ((dcb->fcb != NULL) &&
         (dcb->fcb->state != FSMDEF_S_CONNECTED) &&
         (dcb->fcb->state != FSMDEF_S_CONNECTED_MEDIA_PEND))) {
        return (FALSE);
    }

    return (TRUE);
}

/*
 * fsmdef_check_if_ok_to_monitor_update_call
 *
 * Description:
 *    Checks if it is ok to update the monitoring call.
 *
 * Parameters:
 *    line
 *    call_id
 *
 * Returns: TRUE or FALSE
 */
boolean
fsmdef_check_if_ok_to_monitor_update_call (line_t line, callid_t call_id)
{
    fsmdef_dcb_t *dcb;

    dcb = fsmdef_get_dcb_by_call_id(call_id);

    if (dcb == NULL) {
        return (FALSE);
    }

    if ((dcb->line != line) ||
        ((dcb->fcb != NULL) &&
         (dcb->fcb->state != FSMDEF_S_CONNECTED))) {
        return (FALSE);
    }

    return (TRUE);
}

/*
 * fsmdef_set_call_info_cc_call_state
 *
 * Description:
 *    Sets the called number to readable localized values
 *    if it is service URI based feature and invokes cc_call_state()
 *
 * Parameters:
 *    dcb
 *    state
 *
 * Returns: none
 */
static void
fsmdef_set_call_info_cc_call_state (fsmdef_dcb_t *dcb, cc_states_t state, cc_causes_t cause)
{
    cc_state_data_t temp_data;
    char           tmp_str[CALL_BUBBLE_STR_MAX_LEN];
    int            rc = CPR_FAILURE;

	tmp_str[0] = '\0';

    switch (dcb->active_feature) {
    case CC_FEATURE_CFWD_ALL:
        rc = platGetPhraseText(STR_INDEX_CALL_FORWARD,
                                      (char *) tmp_str,
                                      CALL_BUBBLE_STR_MAX_LEN);
        break;
    default:
        rc = CPR_FAILURE;
        break;
    }

    switch (state) {

    case CC_STATE_DIALING_COMPLETED:
        temp_data.dialing_completed.caller_id = dcb->caller_id;
        break;

    case CC_STATE_CALL_SENT:
        temp_data.call_sent.caller_id = dcb->caller_id;
        break;

    case CC_STATE_CALL_FAILED:
        temp_data.call_failed.caller_id = dcb->caller_id;
        temp_data.call_failed.cause = cause;
        break;

    default:
        /* Set the call id for other states */
        temp_data.offhook.caller_id = dcb->caller_id;
        break;
    }

    if ((rc == CPR_SUCCESS) && strlen(tmp_str) > 0) {
        temp_data.offhook.caller_id.called_number = tmp_str;
    }

    cc_call_state(dcb->call_id, dcb->line, state, &temp_data);
}

/*
 * fsmdef_get_dcb_by_call_instance_id
 *
 * Description:
 *    This function returns fsmdef DCB that has a match for a given call
 *    instance ID of a given line.
 *
 * Parameters:
 *    line             - the dn line
 *    call_instance_id - for call instance ID.
 *
 * Returns: pointer to fsmdef_dcb_t if a matching dcb is found otherwise
 *          returns NULL
 */
fsmdef_dcb_t *
fsmdef_get_dcb_by_call_instance_id (line_t line, uint16_t call_instance_id)
{
    fsmdef_dcb_t *dcb;

    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
        if ((dcb->caller_id.call_instance_id == call_instance_id) &&
            (dcb->line == line)) {
            return (dcb);
        }
    }
    return (NULL);
}

/*
 * fsmdef_ev_join
 *
 * Description:
 *     The fsmdef_ev_join function sends an offhook event
 *     to the barging call and sets its state to FSMDEF_S_JOINING.
 *
 * Parameters:
 *     data - pointer to the cc_feature_data_t
 *
 * Returns:
 *     none
 */
static void
fsmdef_ev_join (cc_feature_data_t *data)
{
    fsm_fcb_t *fcb = NULL;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    fcb = fsm_get_fcb_by_call_id_and_type(data->newcall.join.join_call_id,
                                          FSM_TYPE_DEF);
    if (fcb) {
        fsmdef_dcb_t *dcb = fcb->dcb;

        cc_int_offhook(CC_SRC_GSM, CC_SRC_GSM, CC_NO_CALL_ID, CC_REASON_NONE,
                       dcb->call_id, dcb->line, NULL, CC_MONITOR_NONE,CFWDALL_NONE);
        fsm_change_state(fcb, __LINE__, FSMDEF_S_JOINING);
    }
}

/*
 * fsmdef_ev_joining_connected_ack
 *
 * Description:
 *     The fsmdef_ev_joining_connected_ack negotiates the sdp in the
 *     ack(if there is one) and sets the fsm/ccapi state to connected
 *
 * Parameters:
 *     event - pointer to the sm_event_t
 *
 * Returns:
 *     sm_rcs_t value
 */
static sm_rcs_t
fsmdef_ev_joining_connected_ack (sm_event_t *event)
{
    fsm_fcb_t          *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t       *dcb = fcb->dcb;
    cc_connected_ack_t *msg = (cc_connected_ack_t *) event->msg;
    cc_causes_t         cause;
    fsmcnf_ccb_t       *ccb;
    fsm_fcb_t          *join_target_fcb;
    cc_feature_data_t   data;
    cc_uint32_t           major_sis_ver = SIS_PROTOCOL_MAJOR_VERSION_SEADRAGON;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    memset(&data, 0, sizeof(data));

    /* Check the remote SDP. The far end may not have included the SDP in an
     * earlier message, which means that the SDP must be in this message.
     */
    if (dcb->remote_sdp_in_ack == TRUE) {
        cause = gsmsdp_negotiate_answer_sdp(fcb, &msg->msg_body);
        if (cause != CC_CAUSE_OK) {
            data.endcall.cause = cause;
            cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id, dcb->line,
                           CC_FEATURE_END_CALL, &data);
            return (SM_RC_END);
        }
    }

    platGetSISProtocolVer(&major_sis_ver, NULL, NULL,NULL);

    ccb = fsmcnf_get_ccb_by_call_id(dcb->call_id);

    if (ccb) {
        join_target_fcb = fsm_get_fcb_by_call_id_and_type(ccb->cnf_call_id,
                                                           FSM_TYPE_CNF);
        if ((gsmsdp_is_media_encrypted(dcb) == FALSE) &&
            (gsmsdp_is_media_encrypted((join_target_fcb!= NULL)?join_target_fcb->dcb:NULL) == TRUE) &&
            (major_sis_ver < SIS_PROTOCOL_MAJOR_VERSION_MUSTER)) {
            /*
             * Target Call was secure, a non-secure is trying to Barge/Monitor,
             * fail it
             */
            data.endcall.cause = CC_CAUSE_SECURITY_FAILURE;
            cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id, dcb->line,
                           CC_FEATURE_END_CALL, &data);
            return (SM_RC_END);
        }
    }

    cc_call_state(dcb->call_id, dcb->line, CC_STATE_CONNECTED,
                  (cc_state_data_t *) &(dcb->caller_id));
    /*
     * If DSP is not able to start rx/tx channels, release the call
     */
    if (dcb->dsp_out_of_resources == TRUE) {
        data.endcall.cause = CC_CAUSE_NO_MEDIA;
        cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id, dcb->line,
                       CC_FEATURE_END_CALL, &data);
        return (SM_RC_END);
    }

    if (ccb) {
        /*
         * Bridge the conference legs for the join call, even though
         * it's done here, ccb should only be accessed from fsmcnf
         */
        ccb->active  = TRUE;
        ccb->bridged = TRUE;
    }

    /*
     * Handle media capability changes if there is before transition to
     * connected state.
     */
    return(fsmdef_transition_to_connected(fcb));
}

/*
 * fsmdef_ev_joining_offhook
 *
 * Description:
 *     The fsmdef_ev_joining_offhook creates the local sdp
 *     for the sip stack to send 200 Ok out
 *
 * Parameters:
 *     event - pointer to the sm_event_t
 *
 * Returns:
 *     sm_rcs_t value
 */
static sm_rcs_t
fsmdef_ev_joining_offhook (sm_event_t *event)
{
    fsm_fcb_t        *fcb = (fsm_fcb_t *) event->data;
    fsmdef_dcb_t     *dcb = fcb->dcb;
    cc_causes_t       cause;
    cc_msgbody_info_t msg_body;
    cc_feature_data_t data;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    /* Build our response SDP to include in the connected */
    cause = gsmsdp_encode_sdp_and_update_version(dcb, &msg_body);
    if (cause != CC_CAUSE_OK) {
        FSM_DEBUG_SM("%s", get_debug_string(FSM_DBG_SDP_BUILD_ERR));
        memset(&data, 0, sizeof(data));
        data.endcall.cause = cause;
        cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id, dcb->line,
                       CC_FEATURE_END_CALL, &data);
        return (SM_RC_END);
    }
    cc_int_connected(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id, dcb->line,
                     &(dcb->caller_id), NULL, &msg_body);

    FSM_SET_FLAGS(dcb->msgs_sent, FSMDEF_MSG_CONNECTED);

    return (SM_RC_END);
}

/*
 * fsmdef_extract_join_target
 *
 * Description:
 *     The fsmdef_extract_join_target extract join target call id from
 *     the setup message and sends a JOIN event to that call
 *
 * Parameters:
 *     event - pointer to the sm_event_t
 *
 * Returns:
 *     TRUE or FALSE
 */
static boolean
fsmdef_extract_join_target (sm_event_t *event)
{
    static const char  fname[]    = "fsmdef_extract_join_target";
    fsm_fcb_t      *fcb     = (fsm_fcb_t *) event->data;
    cc_setup_t     *msg     = (cc_setup_t *) event->msg;
    callid_t        call_id = msg->call_id;
    line_t          line    = msg->line;
    fsmdef_dcb_t   *dcb;
    fsmdef_dcb_t   *join_dcb;
    cc_feature_data_t data;

    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    dcb = fcb->dcb;

    if ((msg->call_info.type == CC_FEAT_MONITOR) ||
        (dcb->session == WHISPER_COACHING))  {

        /* For now, null out the ui id */
        lsm_set_ui_id(dcb->call_id, CC_NO_CALL_ID);
        join_dcb =
            fsmdef_get_dcb_by_call_id(msg->call_info.data.join.join_call_id);
        if (join_dcb) {
            dcb->group_id = join_dcb->group_id;
            memset(&data, 0, sizeof(data));
            data.newcall.join.join_call_id = call_id;

          if (dcb->session == WHISPER_COACHING) {
            	data.newcall.cause = CC_CAUSE_MONITOR;
            } else if (msg->call_info.type == CC_FEAT_MONITOR) {
                data.newcall.cause = CC_CAUSE_MONITOR;
                // set the session leg of this dcb to monitor
                dcb->session = MONITOR;
            }
            FSM_DEBUG_SM(DEB_L_C_F_PREFIX" dcb-session type is = %s",
                DEB_L_C_F_PREFIX_ARGS(FSM, dcb->line, dcb->call_id, fname),
                dcb->session == WHISPER_COACHING ? "WHISPER_COACHING" :
                dcb->session == MONITOR ? "MONITOR" : "PRIMARY");

            cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, join_dcb->call_id, line,
                           CC_FEATURE_JOIN, &data);
        } else {
            FSM_DEBUG_SM(DEB_L_C_F_PREFIX"Unable to find join target dcb",
				DEB_L_C_F_PREFIX_ARGS(FSM, dcb->line, dcb->call_id, fname));
            return (TRUE);
        }
    } else {
        FSM_DEBUG_SM(DEB_L_C_F_PREFIX"Unable to find join target call information",
			DEB_L_C_F_PREFIX_ARGS(FSM, dcb->line, dcb->call_id, fname));
        return (TRUE);
    }
    return (FALSE);
}


/*
 * fsmdef_ev_notify_feature
 *
 * Description:
 *     Handles NOTIFY event in different states.
 *
 * Parameters:
 *     msg - message pointer
 *     dcb - pointer to the fsmdef_dcb_t
 *
 * Returns:
 *     none
 */
static void
fsmdef_ev_notify_feature (cc_feature_t *msg, fsmdef_dcb_t *dcb)
{
    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

}

/*
 * fsmdef_notify_hook_event
 *
 * Description:
 *     Send offhook/onhook events to SIP (DialogManager) task.
 *
 * Parameters:
 *     dcb - pointer to the fsmdef_dcb_t
 *
 * Returns:
 *     none
 */
static void
fsmdef_notify_hook_event (fsm_fcb_t *fcb, cc_msgs_t msg, char *global_call_id,
                          callid_t prim_call_id,
                          cc_hold_resume_reason_e consult_reason,
                          monitor_mode_t monitor_mode,
                          cfwdall_mode_t cfwdall_mode)
{
    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, __FUNCTION__));

    if (msg == CC_MSG_OFFHOOK) {
        cc_int_offhook(CC_SRC_GSM, CC_SRC_SIP, prim_call_id, consult_reason,
                       fcb->dcb->call_id, fcb->dcb->line,
                       global_call_id, monitor_mode,cfwdall_mode);
    } else if (msg == CC_MSG_ONHOOK) {
        cc_int_onhook(CC_SRC_GSM, CC_SRC_SIP, prim_call_id,
                      consult_reason, fcb->dcb->call_id, fcb->dcb->line, FALSE,
                      FALSE, __FILE__, __LINE__);
    }
    return;
}

/*
 * fsmdef_update_callinfo_security_status
 *
 * Description:
 *     The fsmdef_update_callinfo_security_status function updates the call
 *     information that applies to TNP platform.
 *
 * Parameters:
 *     dcb       - pointer to the fsmdef_dcb_t
 *     call_info - pointer to the cc_feature_data_call_info_t
 *
 * Returns:
 *     none
 */
static void
fsmdef_update_callinfo_security_status (fsmdef_dcb_t *dcb,
                                        cc_feature_data_call_info_t *call_info)
{
    /*
     * Update security information
     */
    if (call_info->feature_flag & CC_SECURITY) {
        if (sip_regmgr_get_sec_level(dcb->line) != AUTHENTICATED &&
            sip_regmgr_get_sec_level(dcb->line) != ENCRYPTED ) {
            // Signaling security can't be trusted downgrade security level to NOT_AUTHENTICATED
            call_info->security = CC_SECURITY_NOT_AUTHENTICATED;
        }

        if (dcb->security != call_info->security) {
            FSM_SET_SECURITY_STATUS(dcb, call_info->security);
            if ( call_info->security == CC_SECURITY_ENCRYPTED )
               ui_update_call_security(dcb->line, lsm_get_ui_id(dcb->call_id), CC_SECURITY_ENCRYPTED);
            else
               ui_update_call_security(dcb->line, lsm_get_ui_id(dcb->call_id), CC_SECURITY_UNKNOWN);

            dcb->ui_update_required = TRUE;
        }
    }
}

/*
 * fsmdef_check_retain_fwd_info_state
 *
 * Description:   This function checks the "Retain Forward Information" config
 *                parameter (only for TNP). It returns TRUE if forward info is
 *                to be retained; FALSE otherwise. This config parameter is
 *                is used to change call bubble display from "Forward <DN>" to
 *                "From <DN>" when an incoming call is answered AND the config
 *                parameter value is set to TRUE. For non-TNP, this function
 *                will always return FALSE (i.e. not to retain fwd info).
 *
 * Parameters:    none
 *
 * Return Value:  TRUE or FALSE
 */
boolean
fsmdef_check_retain_fwd_info_state (void)
{
    int retain_fwd_info_cfg = 0;    /* default to Disabled (or 0) for no-op */

    config_get_value(CFGID_RETAIN_FORWARD_INFORMATION,
                     &retain_fwd_info_cfg,
                     sizeof(retain_fwd_info_cfg));

    if (!retain_fwd_info_cfg) {
        return (FALSE);         /* do NOT retain forward information */
    } else {
        return (TRUE);          /* retain forward information */
    }
}

void
fsmdef_init (void)
{
    static const char fname[] = "fsmdef_init";
    fsmdef_dcb_t *dcb;


    /*
     *  Initialize the dcbs.
     */
    fsmdef_dcbs = (fsmdef_dcb_t *)
        cpr_calloc(FSMDEF_MAX_DCBS, sizeof(fsmdef_dcb_t));
    if (fsmdef_dcbs == NULL) {
        FSM_DEBUG_SM(DEB_F_PREFIX"cpr_calloc returned NULL",
                     DEB_F_PREFIX_ARGS(FSM, fname));
        return;
    }

    /* Create free media structure list */
    if (!gsmsdp_create_free_media_list()) {
        FSM_DEBUG_SM(DEB_F_PREFIX"Unable to create free media list",
                     DEB_F_PREFIX_ARGS(FSM, fname));
        return;
    }

    DEF_DEBUG(DEB_F_PREFIX"Disabling mass registration print", DEB_F_PREFIX_ARGS(SIP_REG, fname));
    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
        fsmdef_init_dcb(dcb, CC_NO_CALL_ID, FSMDEF_CALL_TYPE_NONE,
                        FSMDEF_NO_NUMBER, LSM_NO_LINE, NULL);
        /*
         * Allocate ringback delay timer for each dcb
         */
        dcb->ringback_delay_tmr = cprCreateTimer("Ringback Delay",
                                                 GSM_RINGBACK_DELAY_TIMER,
                                                 TIMER_EXPIRATION,
                                                 gsm_msgq);
        if (dcb->ringback_delay_tmr == NULL) {
            FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_TMR_CREATE_FAILED),
                         dcb->call_id, dcb->line, fname, "Ringback Delay");
            return;
        }

        /*
         * Allocate auto answer timer for each dcb
         */
        dcb->autoAnswerTimer = cprCreateTimer("Auto Answer",
                                              GSM_AUTOANSWER_TIMER,
                                              TIMER_EXPIRATION,
                                              gsm_msgq);
        if (dcb->autoAnswerTimer == NULL) {
            FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_TMR_CREATE_FAILED),
                         dcb->call_id, dcb->line, fname, "Auto Answer");
            (void)cprDestroyTimer(dcb->ringback_delay_tmr);
            dcb->ringback_delay_tmr = NULL;
            return;
        }
        dcb->revertTimer = cprCreateTimer("Call Reversion",
                                              GSM_REVERSION_TIMER,
                                              TIMER_EXPIRATION,
                                              gsm_msgq);
		dcb->reversionInterval = -1;
        if (dcb->revertTimer == NULL) {
            FSM_DEBUG_SM(get_debug_string(FSMDEF_DBG_TMR_CREATE_FAILED),
                         dcb->call_id, dcb->line, fname, "Hold Revertion");

            (void)cprDestroyTimer(dcb->ringback_delay_tmr);
            dcb->ringback_delay_tmr = NULL;
            (void)cprDestroyTimer(dcb->autoAnswerTimer);
            dcb->autoAnswerTimer = NULL;
            return;
        }
        if (dcb == fsmdef_dcbs) {
            g_disable_mass_reg_debug_print = TRUE;
        }
    }
    g_disable_mass_reg_debug_print = FALSE;

    /*
     * Initialize the state/event table.
     */
    fsmdef_sm_table.min_state = FSMDEF_S_MIN;
    fsmdef_sm_table.max_state = FSMDEF_S_MAX;
    fsmdef_sm_table.min_event = CC_MSG_MIN;
    fsmdef_sm_table.max_event = CC_MSG_MAX;
    fsmdef_sm_table.table     = (&(fsmdef_function_table[0][0]));
}

void
fsmdef_shutdown (void)
{
    fsmdef_dcb_t *dcb;

    FSM_FOR_ALL_CBS(dcb, fsmdef_dcbs, FSMDEF_MAX_DCBS) {
        if (dcb->req_pending_tmr) {
            (void)cprDestroyTimer(dcb->req_pending_tmr);
        }
        if (dcb->err_onhook_tmr) {
            (void)cprDestroyTimer(dcb->err_onhook_tmr);
        }
        if (dcb->ringback_delay_tmr) {
            (void)cprDestroyTimer(dcb->ringback_delay_tmr);
        }
        if (dcb->autoAnswerTimer) {
            (void)cprDestroyTimer(dcb->autoAnswerTimer);
        }
        if (dcb->revertTimer) {
            (void)cprDestroyTimer(dcb->revertTimer);
        }

        /* clean media list */
        gsmsdp_clean_media_list(dcb);
    }

    /* destroy free media structure list */
    gsmsdp_destroy_free_media_list();

    cpr_free(fsmdef_dcbs);
    fsmdef_dcbs = NULL;
}

static void
fsmdef_update_calltype (fsm_fcb_t *fcb, cc_feature_t *msg) {
    fsmdef_dcb_t      *dcb       = fcb->dcb;
    cc_feature_data_t *feat_data = &(msg->data);
    cc_caller_id_t    *caller_id;

    if (msg->data_valid == FALSE) {
        /* No data to use for update. Just ignore the event. */
        return;
    }

    if (feat_data->call_info.feature_flag & CC_CALLER_ID) {
        caller_id = &feat_data->call_info.caller_id;
        if (caller_id->call_type == CC_CALL_FORWARDED) {
            if (fsmdef_check_retain_fwd_info_state()) {
                dcb->call_type = FSMDEF_CALL_TYPE_FORWARD;
            }
        }
    }
}

