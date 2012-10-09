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

#ifndef _SUBAPI_H_
#define _SUBAPI_H_

#include "ccsip_subsmanager.h"

cc_rcs_t sub_int_subnot_register(cc_srcs_t src_id, cc_srcs_t dst_id,
                                 cc_subscriptions_t evt_pkg, void *callback_fun,
                                 cc_srcs_t dest_task, int msg_id,
                                 void *term_callback, int term_msg_id,
                                 long min_duration, long max_duration);

cc_rcs_t sub_int_subscribe(sipspi_msg_t *msg_p);

cc_rcs_t sub_int_subscribe_ack(cc_srcs_t src_id, cc_srcs_t dst_id, 
                               sub_id_t sub_id, uint16_t response_code,
                               int duration);

cc_rcs_t sub_int_notify(cc_srcs_t src_id, cc_srcs_t dst_id, sub_id_t sub_id,
                        ccsipNotifyResultCallbackFn_t notifyResultCallback,
                        int subsNotResCallbackMsgID,
                        ccsip_event_data_t *eventData,
                        subscriptionState subState);

cc_rcs_t sub_int_notify_ack(sub_id_t sub_id, uint16_t response_code, 
                            uint32_t cseq);

cc_rcs_t sub_int_subscribe_term(sub_id_t sub_id, boolean immediate, 
                                int request_id,
                                cc_subscriptions_t event_package);

cc_rcs_t sip_send_message(ccsip_sub_not_data_t *msg_data,
                          cc_srcs_t dest_task, int msg_id);

cc_rcs_t app_send_message(void *msg_data, int msg_len, cc_srcs_t dest_id,
                          int msg_id);
extern cc_rcs_t
sub_send_msg (cprBuffer_t buf, uint32_t cmd, uint16_t len, cc_srcs_t dst_id);

#endif
