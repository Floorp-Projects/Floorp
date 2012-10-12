/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCSIP_CC_H_
#define _CCSIP_CC_H_

#include "ccapi.h"


void sip_cc_setup(int gsm_call_id, int line, string_t calling_name,
                  string_t calling_number, string_t alt_calling_number, boolean display_calling_number,
                  string_t called_name, string_t called_number,
                  boolean display_called_number, string_t orig_called_name,
                  string_t orig_called_number, string_t last_redirect_name,
                  string_t last_redirect_number, cc_call_type_e call_type,
                  cc_alerting_type alert_info, vcm_ring_mode_t alerting_ring,
                  vcm_tones_t alerting_tone, cc_call_info_t *call_info_p,
                  boolean replaces, string_t recv_info_list, sipMessage_t *sip_msg);
void sip_cc_setup_ack(int call_id, int line, cc_msgbody_info_t *msg_body);
void sip_cc_proceeding(int gsm_call_id, int line);
void sip_cc_alerting(int gsm_call_id, int line, sipMessage_t *sip_msg,
                     int inband);
void sip_cc_connected(int gsm_call_id, int line, string_t recv_info_list, sipMessage_t *sip_msg);
void sip_cc_connected_ack(int gsm_call_id, int line, sipMessage_t *sip_msg);
void sip_cc_release(int gsm_call_id, int line, cc_causes_t cause,
                    const char *dialstring);
void sip_cc_release_complete(int gsm_call_id, int line, cc_causes_t cause);
void sip_cc_feature(int call_id, int line, int feature, void *data);
void sip_cc_feature_ack(int call_id, int line, int feature, void *data,
                        cc_causes_t cause);
void sip_cc_mwi(int call_id, int line, boolean on, int type,
                int newCount, int oldCount, int hpNewCount, int hpOldCount);
void sip_cc_mv_msg_body_to_cc_msg(cc_msgbody_info_t *cc_msg,
                                  sipMessage_t *sip_msg);
boolean sip_cc_create_cc_msg_body_from_sip_msg(cc_msgbody_info_t *cc_msg,
                                               sipMessage_t *sip_msg);
void sip_cc_options(callid_t call_id, line_t line, sipMessage_t *pSipMessage);
void sip_cc_audit(callid_t call_id, line_t line, boolean apply_ringout);

#endif
