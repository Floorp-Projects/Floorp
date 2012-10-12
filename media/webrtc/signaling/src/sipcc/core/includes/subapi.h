/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
