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

#ifndef _UIAPI_H_
#define _UIAPI_H_

#include "cpr_types.h"
#include "phone_types.h"
#include "string_lib.h"
#include "vcm.h"
#include "ccapi.h"

#include "sessionConstants.h"
typedef enum {
    evMinEvent = 0,
    evOffHook = OFFHOOK,
    evOnHook = ONHOOK,
    evRingOut = RINGOUT,
    evRingIn = RINGIN,
    evProceed = PROCEED,
    evConnected = CONNECTED,
    evHold = HOLD,
    evRemHold = REMHOLD,
    evResume = RESUME,
    evBusy = BUSY,
    evReorder = REORDER,
    evConference = CONFERENCE,
    evRemInUse = REMINUSE,
    evCallPreservation = PRESERVATION,
    evHoldRevert = HOLDREVERT,
    evWhisper = WHISPER,
    evWaitingForDigits = WAITINGFORDIGITS,
    evCreateOffer = CREATEOFFER,
    evCreateAnswer = CREATEANSWER,
    evCreateOfferError = CREATEOFFERERROR,
    evCreateAnswerError = CREATEANSWERERROR,	
    evSetLocalDesc = SETLOCALDESC,
    evSetRemoteDesc = SETREMOTEDESC,    
    evSetLocalDescError = SETLOCALDESCERROR,
    evSetRemoteDescError = SETREMOTEDESCERROR,
    evOnRemoteStreamAdd = REMOTESTREAMADD,
    evMaxEvent
} call_events;

#define MWI_STATUS_YES 1

/* call operations : dialing, state change and info related */
void ui_new_call(call_events event, line_t nLine, callid_t nCallID,
                 int call_attr, uint16_t call_instance_id, boolean dialed_digits);
void ui_set_call_attr(line_t line_id, callid_t call_id, call_attr_t attr);
void ui_call_info(string_t clgName, string_t clgNumber, string_t altClgNumber, boolean dispClgNumber,
                  string_t cldName, string_t cldNumber, boolean dispCldNumber,
                  string_t pOrigCalledNameStr, string_t pOrigCalledNumberStr,
                  string_t pLastRedirectingNameStr,
                  string_t pLastRedirectingNumberStr,
                  calltype_t call_type,
                  line_t line, callid_t call_id, uint16_t call_instance_id,
                  cc_security_e call_security, cc_policy_e call_policy);
void ui_cc_capability(line_t line_id, callid_t call_id,
                      string_t recv_info_list);
void ui_info_received(line_t line_id, callid_t call_id,
                      const char *info_package, const char *content_type,
                      const char *message_body);
void ui_update_placed_call_info(line_t line, callid_t call_id, string_t cldName,
                                string_t cldNumber);
void ui_log_disposition( callid_t nCallID, int logDisp);
void ui_call_state(call_events event, line_t nLine, callid_t nCallID, cc_causes_t cause);
void ui_set_local_hold(line_t line, callid_t call_id);
void ui_delete_last_digit(line_t line_id, callid_t call_id);
void ui_update_label_n_speeddial(line_t line, line_t button_no, string_t
                                 speed_dial, string_t label);
void ui_mnc_reached(line_t line, boolean mnc_reached);

void ui_call_selected(line_t line_id, callid_t call_id, int selected);
void ui_call_in_preservation(line_t line_id, callid_t call_id);
void ui_update_call_security(line_t line, callid_t call_id,
                             cc_security_e call_security);
void ui_update_conf_invoked(line_t line, callid_t call_id,
                             boolean invoked);
void ui_terminate_feature(line_t line, callid_t call_id,
                        callid_t target_call_id);

void ui_update_gcid(line_t line, callid_t call_id, char *gcid);
void ui_update_callref(line_t line, callid_t call_id, unsigned int callref);

/* status line related */
void ui_set_call_status(string_t pString, line_t line, callid_t callID);
char *ui_get_idle_prompt_string(void);
void ui_set_idle_prompt_string(string_t pString, int prompt);
void ui_set_notification(line_t line, callid_t callID,
                         char *promptString, int timeout,
                         boolean notifyProgress, char priority);
void ui_clear_notification();

/* softkey manipulation */
void ui_control_feature(line_t line_id, callid_t call_id, int list[], int len, int enable);
void ui_select_feature_key_set(line_t line_id, callid_t call_id, char *set_name,
                               int sk_list[], int len);
void ui_control_featurekey_bksp(line_t line_id, callid_t call_id,
                                boolean enable);

/* speaker */
void ui_set_speaker_mode(boolean mode);

/* mwi */
void ui_set_mwi(line_t line, boolean status, int type, int newCount, int oldCount, int hpNewCount, int hpOldCount);
void ui_change_mwi_lamp(int status);
boolean ui_line_has_mwi_active(line_t line);

/* call forward */
void ui_cfwd_status(line_t line, boolean cfa, char *cfa_number,
                    boolean lcl_fwd);

/* registration, stack init related */
void ui_sip_config_done(void);
void ui_set_sip_registration_state(line_t line, boolean registered);
void ui_reg_all_failed(void);

void ui_keypad_button(char *digitstr, int direction);

void ui_offhook(line_t line, callid_t call_id);
void ui_dial_call(line_t line, callid_t call_id, char *to,
                  char *global_call_id);
int ui_dial_digits(line_t line, char *digitstr);
int ui_dial_dtmf(line_t line, char *digitstr);
void ui_answer_call(line_t line, callid_t call_id);
void ui_disconnect_call(line_t line, callid_t call_id);
int ui_xfer_setup(line_t line, callid_t call_id, char *to,
                  char *global_call_id, boolean media);
void ui_xfer_complete(line_t line, callid_t call_id, callid_t consult_call_id);
int ui_conf_setup(line_t line, callid_t call_id, char *to,
                  char *global_call_id);
void ui_conf_complete(line_t line, callid_t call_id, callid_t consult_call_id);
int ui_hold_call(line_t line, callid_t call_id);
void ui_hold_retrieve_call(line_t line, callid_t call_id);
void ui_cfwdall_req(unsigned int line);

/* Test Interface */
void ui_execute_uri(char *);

/* Status Message Interface */
void ui_log_status_msg(char *msg);

void ui_init_ccm_conn_status(void);
void ui_update_video_avail (line_t line, callid_t call_id, int avail);
void ui_update_video_offered (line_t line, callid_t call_id, int dir);
void ui_call_stop_ringer(line_t line, callid_t call_id);
void ui_call_start_ringer(vcm_ring_mode_t ringMode, short once, line_t line, callid_t call_id);
void ui_BLF_notification (int request_id, cc_blf_state_t blf_state, int app_id);
void ui_update_media_interface_change(line_t line, callid_t call_id, group_call_event_t event);
void ui_create_offer(call_events event, line_t nLine, callid_t nCallID,
                 	 uint16_t call_instance_id, char* sdp);
void ui_create_answer(call_events event, line_t nLine, callid_t nCallID,
                 	 uint16_t call_instance_id, char* sdp);
void ui_set_local_description(call_events event, line_t nLine, callid_t nCallID,
                 	 uint16_t call_instance_id, char* sdp, cc_int32_t status);
void ui_set_remote_description(call_events event, line_t nLine, callid_t nCallID,
                 	 uint16_t call_instance_id, char* sdp, cc_int32_t status);
void ui_on_remote_stream_added(call_events event, line_t nLine, callid_t nCallID,
                     uint16_t call_instance_id, cc_media_remote_track_table_t media_tracks);


#endif
