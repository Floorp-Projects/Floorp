/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SIP_INTERFACE_REGMGR_H_
#define _SIP_INTERFACE_REGMGR_H_

#include "cpr_types.h"
#include "phone_types.h"
#include "sip_common_transport.h"
#include "regmgrapi.h"

typedef enum reg_rcs_t_ {
    REG_RC_SUCCESS,
    REG_RC_ERROR,
    REG_RC_MAX
} reg_rcs_t;

typedef enum reg_srcs_t_ {
    REG_SRC_GSM,
    REG_SRC_SIP,
    REG_SRC_MAX
} reg_srcs_t;

typedef enum reg_status_t_ {
    REG_FAIL,
    REG_ALL_FAIL
} reg_status_t;

typedef struct reg_status_msg_t_ {
    reg_srcs_t src_id;
    reg_status_t msg_id;
} reg_status_msg_t;

typedef enum {
    CCM_STATUS_NONE = 0,
    CCM_STATUS_STANDBY,
    CCM_STATUS_ACTIVE
} reg_ccm_status;

void sip_regmgr_send_status(reg_srcs_t src_id, reg_status_t msg_id);
sec_level_t sip_regmgr_get_sec_level(line_t line);
boolean sip_regmgr_srtp_fallback_enabled(line_t line);
void sip_platform_set_ccm_status();
void sip_platform_cc_mode_notify(void);
void sip_platform_failover_ind(CCM_ID ccm_id);
void sip_platform_fallback_ind(CCM_ID ccm_id);
extern CCM_ID sip_regmgr_get_ccm_id(ccsipCCB_t *ccb);
boolean sip_platform_is_phone_idle(void);
extern void platform_reg_failover_ind(void *to_id);
extern void platform_reg_fallback_ind(void *from_id);
extern void sip_regmgr_phone_idle(boolean waited);
extern void sip_regmgr_fallback_rsp();
extern void sip_regmgr_failover_rsp_start();
extern void sip_regmgr_failover_rsp_complete();
extern void ui_set_ccm_conn_status(const char *addr_str, int status);
extern void platform_cc_mode_notify(int mode);
extern void ui_reg_all_failed(void);
extern void platform_reg_fallback_cfm(void);
extern void platform_regallfail_ind(void *);
extern void sip_platform_logout_reset_req(void);
extern void platform_logout_reset_req (void);

#endif /* _SIP_INTERFACE_REGMGR_H_ */
