/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCSIP_REGISTER_H_
#define _CCSIP_REGISTER_H_

#include "cpr_types.h"
#include "cpr_timers.h"
#include "phone.h"
#include "ccsip_core.h"
#include "ccsip_credentials.h"
#include "platform_api.h"

#define MAX_REG_EXPIRES 3600
#include "ccsip_subsmanager.h"
#include "platform_api.h"

#define MAX_RETRIES_401 3

#define AUTH_HDR(status_code) \
    (((status_code) == SIP_CLI_ERR_UNAUTH) ? \
     (SIP_HEADER_WWW_AUTHENTICATE) : (SIP_HEADER_PROXY_AUTHENTICATE))

#define AUTH_HDR_STR(status_code) \
    (((status_code) == SIP_CLI_ERR_UNAUTH) ? \
     ("WWW-Authenticate") : ("Proxy-Authenticate"))

#define AUTH_BUGINF(status_code) \
    (((status_code) == SIP_CLI_ERR_UNAUTH) ? \
     ("SIP 401 Unauthorized") : ("SIP 407 Proxy Authentication required"))

#define AUTH_NOTIFY(status_code) \
    (((status_code) == SIP_CLI_ERR_UNAUTH) ? \
     ("                      401 <---") : ("                      407 <---"))

#define AUTHOR_HDR(status_code) \
    (((status_code) == SIP_CLI_ERR_UNAUTH) ? \
     (SIP_HEADER_AUTHORIZATION) : (SIP_HEADER_PROXY_AUTHORIZATION))


/*
  These numbers need to match up
  with whats defined on the J-Side
  The Master copy should be here because
  the reason needs to be sent out in the register
  message
*/
#define UNREG_REASON_UNSPECIFIED                       0
//Common with what SCCP uses...need to match with J-Side
//Important! should be defined in plat_api.h for thirdparty application to use.
#define UNREG_REASON_TCP_TIMEOUT                       CC_UNREG_REASON_TCP_TIMEOUT        // 10
#define UNREG_REASON_CM_RESET_TCP                      CC_UNREG_REASON_CM_RESET_TCP       //12
#define UNREG_REASON_CM_ABORTED_TCP                    CC_UNREG_REASON_CM_ABORTED_TCP     //13
#define UNREG_REASON_CM_CLOSED_TCP                     CC_UNREG_REASON_CM_CLOSED_TCP      //14
#define UNREG_REASON_REG_TIMEOUT                       CC_UNREG_REASON_REG_TIMEOUT        //17
#define UNREG_REASON_FALLBACK                          CC_UNREG_REASON_FALLBACK           //18
#define UNREG_REASON_PHONE_KEYPAD                      CC_UNREG_REASON_PHONE_KEYPAD       //20
#define UNREG_REASON_RESET_RESET                       CC_UNREG_REASON_RESET_RESET        //22
#define UNREG_REASON_RESET_RESTART                     CC_UNREG_REASON_RESET_RESTART      //23
#define UNREG_REASON_PHONE_REG_REJ                     CC_UNREG_REASON_PHONE_REG_REJ      //24
#define UNREG_REASON_PHONE_INITIALIZED                 CC_UNREG_REASON_PHONE_INITIALIZED  //25
#define UNREG_REASON_VOICE_VLAN_CHANGED                CC_UNREG_REASON_VOICE_VLAN_CHANGED //26

//sip specific ones...need to match with J-Side
#define UNREG_REASON_VERSION_STAMP_MISMATCH            CC_UNREG_REASON_VERSION_STAMP_MISMATCH          //100
#define UNREG_REASON_VERSION_STAMP_MISMATCH_CONFIG     CC_UNREG_REASON_VERSION_STAMP_MISMATCH_CONFIG   //101
#define UNREG_REASON_VERSION_STAMP_MISMATCH_SOFTKEY    CC_UNREG_REASON_VERSION_STAMP_MISMATCH_SOFTKEY  //102
#define UNREG_REASON_VERSION_STAMP_MISMATCH_DIALPLAN   CC_UNREG_REASON_VERSION_STAMP_MISMATCH_DIALPLAN //103
#define UNREG_REASON_APPLY_CONFIG_RESTART              CC_UNREG_REASON_APPLY_CONFIG_RESTART            //104
#define UNREG_REASON_CONFIG_RETRY_RESTART              CC_UNREG_REASON_CONFIG_RETRY_RESTART            //105
#define UNREG_REASON_TLS_ERROR                         CC_UNREG_REASON_TLS_ERROR                       //106
#define UNREG_REASON_RESET_TO_INACTIVE_PARTITION       CC_UNREG_REASON_RESET_TO_INACTIVE_PARTITION     //107
#define UNREG_REASON_VPN_CONNECTIVITY_LOST             CC_UNREG_REASON_VPN_CONNECTIVITY_LOST           //108


#define PRIMARY_LINE           (1)

typedef enum {
    SIP_REG_ERROR,
    SIP_REG_OK
} sip_reg_return_code;

typedef enum
{
    SIP_REG_INVALID=-1,
    SIP_REG_IDLE,
    SIP_REG_REGISTERING,
    SIP_REG_REGISTERED,
    SIP_REG_UNREGISTERING,
    SIP_REG_PRE_FALLBACK,
    SIP_REG_IN_FAILOVER,
    SIP_REG_POST_FAILOVER,
    SIP_REG_STANDBY_FAILOVER,
    SIP_REG_NO_CC,
    SIP_REG_NO_STANDBY,
    SIP_REG_NO_REGISTER
} ccsip_register_states_t;

typedef enum
{
    E_SIP_REG_NONE = 0,
    SIPSPI_REG_EV_BASE = 1,

    E_SIP_REG_REG_REQ = SIPSPI_REG_EV_BASE,
    E_SIP_REG_CANCEL,
    E_SIP_REG_1xx,
    E_SIP_REG_2xx,
    E_SIP_REG_3xx,
    E_SIP_REG_4xx,
    E_SIP_REG_FAILURE_RESPONSE,
    E_SIP_REG_TMR_ACK,
    E_SIP_REG_TMR_EXPIRE,
    E_SIP_REG_TMR_WAIT,
    E_SIP_REG_TMR_RETRY,
    E_SIP_REG_CLEANUP,
    SIPSPI_REG_EV_END = E_SIP_REG_CLEANUP
} sipRegSMEventType_t;


typedef struct
{
    int     line;
    boolean cancel;
} ccsip_register_msg_t;


int  sip_reg_sm_process_event(sipSMEvent_t *pEvent);
sipRegSMEventType_t ccsip_register_sip2sipreg_event(int sip_event);
int  ccsip_register_init(void);
void ccsip_register_timeout_retry(void *data);
void ccsip_register_all_lines(void);
void ccsip_register_cancel(boolean cancel_reg, boolean backup_proxy);
void ccsip_ccm_register_cancel(boolean cancel_reg);
void ccsip_register_set_state(ccsip_register_states_t state);
ccsip_register_states_t ccsip_register_get_state(void);
ccsip_register_states_t ccsip_register_get_register_state(void);
void ccsip_register_reset_proxy(void);
void cred_get_line_credentials(line_t line, credentials_t *pcredentials,
                               int id_len, int pw_len);
boolean cred_get_user_credentials(line_t line, credentials_t *pcredentials);
boolean cred_get_credentials_r(ccsipCCB_t *ccb, credentials_t *pcredentials);
void ccsip_register_commit(void);
void ccsip_backup_register_commit(void);
void ccsip_register_cleanup(ccsipCCB_t *ccb, boolean start);
void ccsip_register_set_register_state(ccsip_register_states_t state);
int  ccsip_register_send_msg(uint32_t cmd, line_t line);
void ccsip_handle_ev_default(ccsipCCB_t *ccb, sipSMEvent_t *event);
void sip_reg_sm_change_state(ccsipCCB_t *ccb, sipRegSMStateType_t new_state);
boolean ccsip_register_all_unregistered();
void sip_stop_ack_timer(ccsipCCB_t *ccb);
void ccsip_register_shutdown(void);
boolean ccsip_get_ccm_date(char *date_value);
boolean ccsip_is_line_registered(line_t line);
boolean process_retry_after(ccsipCCB_t *ccb, sipMessage_t *response);

#endif
