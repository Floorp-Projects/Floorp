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
