/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __SIP_COMMON_REGMGR_H__
#define __SIP_COMMON_REGMGR_H__

#include "cpr_types.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "cpr_timers.h"
#include "cpr_string.h"
#include "cpr_memory.h"
#include "ccsip_core.h"
#include "singly_link_list.h"
#include "ccsip_platform.h"
#include "sip_common_transport.h"

#define LINE1  0
#define LINE2  1
#define LINE3  2
#define LINE4  3
#define LINE5  4
#define LINE6  5
#define LINE7  6
#define LINE8  7
#define LINE9  8
#define LINE10 9
#define LINE11 10
#define LINE12 11
#define LINE13 12
#define LINE14 13
#define LINE15 14
#define LINE16 15
#define LINE17 16
#define LINE18 17
#define LINE19 18
#define LINE20 19

#define RSP_START 1
#define RSP_COMPLETE 2
#define FAILOVER_RSP 0
#define MAX_FALLBACK_MONITOR_PERIOD 300
#define TLS_CONNECT_TIME 8

#define TOKEN_REFER_TO  "<urn:X-cisco-remotecc:token-registration>"

typedef enum {
    ACTIVE_FD = 0,
    STANDBY_FD,
    MAX_FALLBACK_FDs
} CC_FDs;

typedef enum {
    RET_SUCCESS = 0,
    RET_NO_STANDBY,
    RET_START_FALLBACK,
    RET_INIT_REBOOT
} RET_CODE;

typedef struct fallback_ccb_t_ {
    ccsipCCB_t *ccb;
    sipPlatformUIExpiresTimer_t WaitTimer;
    sipPlatformUIExpiresTimer_t RetryTimer;
    uint32_t StabilityMsgCount;
    boolean tls_socket_waiting;
} fallback_ccb_t;

typedef struct ccm_fd_table_t_ {
    ti_config_table_t *active_ccm_entry;
    ti_config_table_t *standby_ccm_entry;
} ccm_act_stdby_table_t;

//ccm_act_stdby_table_t CCM_Active_Standby_Table[MAX_TEL_LINES+1];

typedef struct ccm_failover_table_t_ {
    ti_config_table_t *failover_ccm_entry;
    ti_config_table_t *fallback_ccm_entry;
    boolean prime_registered;
    boolean failover_started;
} ccm_failover_table_t;

typedef struct ccm_fallback_table_t_ {
    ti_config_table_t *fallback_ccm_entry;
    boolean is_idle;
    boolean is_resp;
    ccsipCCB_t *ccb;
} ccm_fallback_table_t;

typedef struct {
    uint32_t   ccb_index;   /* ccb index for which this msg is intended for */
    CCM_ID     ccm_id;      /* cucm id */
} ccsip_registration_msg_t;

void notify_register_update(int availableLine);
fallback_ccb_t *sip_regmgr_get_fallback_ccb_by_index(line_t index);
int sip_regmgr_destroy_cc_conns(void);
int sip_regmgr_init(void);
void sip_regmgr_register_lines(boolean prime_only, boolean skip_prime);
void sipRegmgrSendRegisterMsg(uint8_t line, ccsipCCB_t *ccb);
void sip_regmgr_ev_cancel(ccsipCCB_t *ccb, sipSMEvent_t *event);
void sip_regmgr_ev_in_fallback_any_response(ccsipCCB_t *ccb, sipSMEvent_t *event);
void sip_regmgr_ev_failure_response(ccsipCCB_t *ccb, sipSMEvent_t *event);
void sip_regmgr_ev_tmr_ack_retry(ccsipCCB_t *ccb, sipSMEvent_t *event);
void sip_regmgr_ev_tmr_expire_standby(ccsipCCB_t *ccb, sipSMEvent_t *event);

void sip_regmgr_ev_unreg_tmr_ack(ccsipCCB_t *cb);
void sip_regmgr_trigger_fallback_monitor(void);
void sip_regmgr_setup_new_standby_ccb(CCM_ID ccm_id);
void sip_regmgr_free_fallback_ccb(ccsipCCB_t *ccb);
void sip_regmgr_retry_timeout_expire(void *data);
void sip_regmgr_stability_timeout_expire(void *data);
void sip_regmgr_find_fallback_ccb_by_callid(const char *callid,
                                            ccsipCCB_t **ccb_ret);
boolean sip_regmgr_find_fallback_ccb_by_addr_port(cpr_ip_addr_t *ipaddr,
                                                  uint16_t port,
                                                  ccsipCCB_t **ccb_ret);
sll_match_e sip_regmgr_fallback_ccb_find(void *find_by_p, void *data_p);
void sip_regmgr_retry_timer_start(fallback_ccb_t *fallback_ccb);
ti_config_table_t *sip_regmgr_ccm_get_conn(line_t dn,
                                           ti_config_table_t *ccm_entry);
void sip_regmgr_check_and_transition(ccsipCCB_t *ccb);
void sip_regmgr_ev_default(ccsipCCB_t *ccb, sipSMEvent_t *event);
void sip_regmgr_ev_fallback_retry(ccsipCCB_t *ccb, sipSMEvent_t *event);
void sip_regmgr_wait_timeout_expire(void *data);
void sip_regmgr_ev_in_fallback_2xx(ccsipCCB_t *ccb, sipSMEvent_t *event);
void sip_regmgr_ev_stability_check_2xx(ccsipCCB_t *ccb, sipSMEvent_t *event);
void sip_regmgr_ev_stability_check_tmr_stable(ccsipCCB_t *ccb,
                                              sipSMEvent_t *event);
void sip_regmgr_ev_stability_check_tmr_wait(ccsipCCB_t *ccb,
                                            sipSMEvent_t *event);
void sip_regmgr_ev_token_wait_2xx(ccsipCCB_t *ccb, sipSMEvent_t *event);
void sip_regmgr_ev_cleanup(ccsipCCB_t *ccb, sipSMEvent_t *event);
void sip_regmgr_ev_token_wait_4xx_n_5xx(ccsipCCB_t *ccb, sipSMEvent_t *event);
void sip_regmgr_ev_token_wait_tmr_wait(ccsipCCB_t *ccb, sipSMEvent_t *event);
void sip_regmgr_shutdown(void);

void sip_regmgr_rsp(int rsp_id, int rsp_type, boolean waited);

boolean sip_regmgr_check_config_change(void);

void sip_regmgr_process_config_change(void);
void sip_regmgr_send_refer(ccsipCCB_t *ccb);
void sip_regmgr_ccm_restarted(ccsipCCB_t *new_reg_ccb);
void sip_regmgr_notify_timer_callback(void *data);
ccsipCCB_t *sip_regmgr_get_fallback_ccb_list(uint32_t *previous_data_p);
void sip_regmgr_replace_standby(ccsipCCB_t *ccb);
void sip_regmgr_regallfail_timer_callback(void *data);
void sip_regmgr_handle_reg_all_fail(void);
void sip_regmgr_get_config_addr(int ccm_id, char *add_str);

extern boolean g_disable_mass_reg_debug_print;

void regmgr_handle_register_update(line_t last_available_line);


#endif /* __SIP_COMMON_REGMGR_H__ */
