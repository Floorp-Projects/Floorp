/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "phntask.h"
#include "phone_types.h"
#include "phone_debug.h"
#include "util_string.h"
#include "dns_utils.h"
#include "cpr_socket.h"
#include "util_string.h"
#include "sip_common_transport.h"
#include "sip_ccm_transport.h"
#include "ccsip_messaging.h"
#include "ccsip_register.h"
#include "sip_common_transport.h"
#include "sip_common_regmgr.h"
#include "singly_link_list.h"
#include "uiapi.h"
#include "text_strings.h"
#include "ccsip_callinfo.h"
#include "sip_interface_regmgr.h"
#include "ccsip_task.h"
#include "cpr_rand.h"
#include "ccsip_platform_tcp.h"
#include "ccsip_subsmanager.h"
#include "ccsip_publish.h"
#include "ccsip_platform_tls.h"

#define REGALL_FAIL_TIME       100
boolean regall_fail_attempt = FALSE;
boolean registration_reject = FALSE;

int retry_times = 0;

boolean config_update_required = FALSE;
void *new_standby_available = NULL;
sll_handle_t fallback_ccb_list;
static boolean wan_failure = FALSE;
extern ti_config_table_t CCM_Device_Specific_Config_Table[MAX_CCM];
extern ccm_act_stdby_table_t CCM_Active_Standby_Table;
extern uint16_t ccm_config_id_addr_str[MAX_CCM];
#ifdef IPV6_STACK_ENABLED

extern uint16_t ccm_config_id_ipv6_addr_str[MAX_CCM];
#endif

extern boolean sip_reg_all_failed;
extern void sip_platform_handle_service_control_notify(sipServiceControl_t * scp);
extern void ui_update_registration_state_all_lines(boolean registered);
extern int phone_local_tcp_port[UNUSED_PARAM];

ccm_failover_table_t CCM_Failover_Table;
ccm_fallback_table_t CCM_Fallback_Table;
cc_config_table_t CC_Config_Table[MAX_REG_LINES + 1];
typedef struct fallback_line_num_t_ {
    line_t line;
    boolean available;
} fallback_line_num_t;

static fallback_line_num_t fallback_lines_available[MAX_CCM - 1] =
{
    {MAX_CCBS,     TRUE},
    {MAX_CCBS + 1, TRUE},
};

static void sip_regmgr_update_call_ccb(void);
void sip_regmgr_fallback_generic_timer_stop(cprTimer_t timer);
boolean sip_regmgr_find_fallback_ccb_by_ccmid (CCM_ID ccm_id, ccsipCCB_t **ccb_ret);

void sip_regmgr_clean_standby_ccb(ccsipCCB_t *ccb);
static void sip_regmgr_tls_retry_timer_start (fallback_ccb_t *fallback_ccb);
static void
sip_regmgr_generic_timer_start_failure (fallback_ccb_t *fallback_ccb,
                                        uint32_t event)
{

}

static void
sip_regmgr_generic_message_post_failure (fallback_ccb_t *fallback_ccb,
                                         uint32_t event)
{

}

/** sip_regmgr_get_fallback_ccb_list
 *
 * PARAMETERS: NULL, or fallback_ccb
 *
 * DESCRIPTION: Returns the ccb and fallback_ccb from linked list
 *              The first call to this function should pass in NULL
 *              Subsequent calls should pass the opaque (for the caller)
 *              token so that the next ccb is returned
 *
 * RETURNS:     fallback_ccb as the opaque token, ccb as the return value
 *
 */
ccsipCCB_t *
sip_regmgr_get_fallback_ccb_list (uint32_t *previous_data_p)
{
    fallback_ccb_t *this_fallback_ccb = NULL;
    uint32_t data;

    data = (uint32_t) (*previous_data_p);
    this_fallback_ccb = (fallback_ccb_t *)
        sll_next(fallback_ccb_list, (void *)(long) (data));
    if (this_fallback_ccb) {
        *previous_data_p = (long) (this_fallback_ccb);
        return (this_fallback_ccb->ccb);
    }
    return NULL;
}

/*
 ** sip_regmgr_get_fallback_ccb_by_index
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: line number
 *
 *  DESCRIPTION: Searches the linked list of fallback ccb's and
 *               returns the ccb that matches the line number.
 *
 *  RETURNS: fallback_ccb if match, NULL if not.
 *
 */
fallback_ccb_t *
sip_regmgr_get_fallback_ccb_by_index (line_t ndx)
{
    return (fallback_ccb_t *)(sll_find(fallback_ccb_list, (void *)(long)ndx));
}

/*
 ** sip_regmgr_find_fallback_ccb
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: find_by_p is the dn line number that is the used to
 *              match with the fallback ccb's. data_p is the ccb that
 *              from the linked list that is checked to see if it has
 *              the dn number that is being searched for.
 *
 *  DESCRIPTION: finds the fallback ccb that matched a dn line value
 *               This is the match routine provided while the singly
 *               linked list for fallback ccb's is created. sll_find()
 *               used this routine to perform the find.
 *
 *  RETURNS: Indicates if match was found or not.
 *
 */
sll_match_e
sip_regmgr_find_fallback_ccb (void *find_by_p, void *data_p)
{
    int to_find_ccb_index = (long) find_by_p;
    fallback_ccb_t *fallback_ccb = (fallback_ccb_t *) data_p;
    ccsipCCB_t *list_ccb = (ccsipCCB_t *) fallback_ccb->ccb;

    if (to_find_ccb_index == list_ccb->index) {
        return (SLL_MATCH_FOUND);
    } else {
        return (SLL_MATCH_NOT_FOUND);
    }
}

/*
 ** sip_regmgr_find_fallback_ccb_by_callid
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: Callid string and the container for the matching
 *              ccb, if found.
 *
 *  DESCRIPTION: Searches the linked list of fallback ccb's and
 *               returns the ccb that matches the callid.
 *
 *  RETURNS: void
 *
 */
void
sip_regmgr_find_fallback_ccb_by_callid (const char *callid,
                                        ccsipCCB_t **ccb_ret)
{
    const char fname[] = "sip_regmgr_find_fallback_ccb_by_callid";
    fallback_ccb_t *fallback_ccb = NULL;
    ccsipCCB_t *list_ccb = NULL;

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Trying to find match for %s\n",
                          DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname), callid);
    while ((fallback_ccb = (fallback_ccb_t *)sll_next(fallback_ccb_list,
                                                      fallback_ccb)) != NULL) {
        list_ccb = (ccsipCCB_t *) fallback_ccb->ccb;
        if (strcmp(callid, list_ccb->sipCallID) == 0) {
            *ccb_ret = list_ccb;
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Found ccb to match callid"
                                  " line %d/%d\n", DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname),
                                  list_ccb->index,
                                  list_ccb->dn_line);
            break;
        }
    }
}

/*
 ** sip_regmgr_find_fallback_ccb_by_addr_port
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: ipaddr and port to match and container for the ccb
 *              if found.
 *
 *  DESCRIPTION: finds the fallback ccb that matched a addr:port value
 *
 *  RETURNS: Indicates if match was found or not.
 *
 */
boolean
sip_regmgr_find_fallback_ccb_by_addr_port (cpr_ip_addr_t *ipaddr, uint16_t port,
                                           ccsipCCB_t **ccb_ret)
{
    fallback_ccb_t *fallback_ccb = NULL;
    ccsipCCB_t *list_ccb = NULL;
    ti_config_table_t *cfg_table_entry;
    ti_common_t *ti_common;
    boolean found_ccb = FALSE;

    while ((fallback_ccb = (fallback_ccb_t *)sll_next(fallback_ccb_list,
                                                      fallback_ccb)) != NULL) {
        list_ccb = (ccsipCCB_t *) fallback_ccb->ccb;
        cfg_table_entry = (ti_config_table_t *)
            list_ccb->cc_cfg_table_entry;
        ti_common = &cfg_table_entry->ti_common;
        if (util_compare_ip(&(ti_common->addr), ipaddr) && ti_common->port == port) {
            *ccb_ret = list_ccb;
            found_ccb = TRUE;
            break;
        }
    }
    return (found_ccb);
}

/**
 ** sip_regmgr_find_fallback_ccb_by_ccmid
 *  finds the fallback ccb that matched a ccm id
 *
 *  @param  ccm_id ccm id to match and ccb_ret - ccb if found
 *
 *  @return TRUE if found; else  FALSE
 *
 */
boolean
sip_regmgr_find_fallback_ccb_by_ccmid (CCM_ID ccm_id, ccsipCCB_t **ccb_ret)
{
    fallback_ccb_t *fallback_ccb = NULL;
    ccsipCCB_t *list_ccb = NULL;
    ti_config_table_t *cfg_table_entry;
    boolean found_ccb = FALSE;

    while ((fallback_ccb = (fallback_ccb_t *)sll_next(fallback_ccb_list,
                                                      fallback_ccb)) != NULL) {
        list_ccb = (ccsipCCB_t *) fallback_ccb->ccb;
        if (list_ccb) {
            cfg_table_entry = (ti_config_table_t *)
                              list_ccb->cc_cfg_table_entry;
            if (cfg_table_entry &&
               (cfg_table_entry->ti_specific.ti_ccm.ccm_id == ccm_id)) {
				if(ccb_ret != NULL){
					*ccb_ret = list_ccb;
				}
                found_ccb = TRUE;
                break;
            }
        }
    }
    return (found_ccb);
}

/*
 ** sip_regmgr_get_fallback_line_num
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: void
 *
 *  DESCRIPTION: returns the fallback line number to use during
 *               failover while creating fallback ccb's
 *
 *  RETURNS: line number to use.
 *
 */
line_t
sip_regmgr_get_fallback_line_num ()
{
    const char fname[] = "sip_regmgr_get_fallback_line_num";
    int ndx;
    line_t line = 0;

    for (ndx = 0; ndx < (MAX_CCM - 1); ndx++) {
        if (fallback_lines_available[ndx].available) {
            fallback_lines_available[ndx].available = FALSE;
            line = fallback_lines_available[ndx].line;
            break;
        }
    }
    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Allocated fallback line %d at index %d\n",
                          DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname), line, (int) (line - MAX_CCBS));
    return (line);
}

/*
 ** sip_regmgr_return_fallback_line_num
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: void
 *
 *  DESCRIPTION: returns the fallback line number for reuse during
 *               failover while creating fallback ccb's
 *
 *  RETURNS: line number to use.
 *
 */
void
sip_regmgr_return_fallback_line_num (line_t num)
{
    const char fname[] = "sip_regmgr_return_fallback_line_num";

    if (((num - MAX_CCBS) > -1) &&
        ((num - MAX_CCBS) < (MAX_CCM - 1))) {
        fallback_lines_available[(int)(num - MAX_CCBS)].available = TRUE;
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Returned fallback line %d at index %d\n",
                              DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname), num, (int) (num - MAX_CCBS));
    } else {
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Invalid index for fallback_lines_available %d",
                              DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname), (int) (num - MAX_CCBS));
    }
}

/*
 ** sip_regmgr_clean_fallback_ccb
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: pointer to the fallback ccb
 *
 *  DESCRIPTION: Clean fallback CCBs
 *
 *  RETURNS:
 *
 */
void
sip_regmgr_clean_fallback_ccb (fallback_ccb_t *fallback_ccb)
{
    if (fallback_ccb == NULL) {
        return;
    }

    if (fallback_ccb->ccb) {
        sip_regmgr_return_fallback_line_num(fallback_ccb->ccb->index);
    }
    (void) cprCancelTimer(fallback_ccb->WaitTimer.timer);
    (void) cprDestroyTimer(fallback_ccb->WaitTimer.timer);
    fallback_ccb->WaitTimer.timer = NULL;
    fallback_ccb->tls_socket_waiting = FALSE;

    (void) cprCancelTimer(fallback_ccb->RetryTimer.timer);
    (void) cprDestroyTimer(fallback_ccb->RetryTimer.timer);
    fallback_ccb->RetryTimer.timer = NULL;

    if (fallback_ccb->ccb) {
        sip_sm_call_cleanup(fallback_ccb->ccb);
        cpr_free(fallback_ccb->ccb);
        fallback_ccb->ccb = NULL;
    }
}

/*
 ** sip_regmgr_free_fallback_ccb
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: The fallback ccb to be deleted.
 *
 *  DESCRIPTION: Delete a fallback ccb and its associated timers
 *               and removes it from the linked list
 *
 *  RETURNS: none
 */
void
sip_regmgr_free_fallback_ccb (ccsipCCB_t *ccb)
{
    const char fname[] = "sip_regmgr_free_fallback_ccb";
    fallback_ccb_t *fallback_ccb;

    fallback_ccb = sip_regmgr_get_fallback_ccb_by_index(ccb->index);

    if (!fallback_ccb) {
        return;
    }
    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Freed fallback ccb for %s:%d\n",
                          DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname), ccb->reg.proxy, ccb->reg.port);
    sip_regmgr_clean_fallback_ccb(fallback_ccb);
    if (sll_remove(fallback_ccb_list, fallback_ccb) != SLL_RET_SUCCESS) {
        CCSIP_DEBUG_ERROR("%s: sll_remove error for fallback_ccb\n", fname);
    }
    cpr_free(fallback_ccb);
}

/*
 ** sip_regmgr_cleanup_fallback_ccb_list
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: none
 *
 *  DESCRIPTION: Delete fallback ccbs and its associated timers
 *               and removes it from the linked list
 *
 *  RETURNS: none
 */
void
sip_regmgr_free_fallback_ccb_list ()
{
    fallback_ccb_t *fallback_ccb = NULL;

    while ((fallback_ccb = (fallback_ccb_t *)sll_next(fallback_ccb_list, NULL))
           != NULL) {
        sip_regmgr_clean_fallback_ccb(fallback_ccb);
        (void) sll_remove(fallback_ccb_list, fallback_ccb);
        cpr_free(fallback_ccb);
    }
    sll_destroy(fallback_ccb_list);
    fallback_ccb_list = NULL;
}

/*
 ** sip_regmgr_create_fallback_ccb
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: ccm_id, dn_line of the new fallback
 *              ccb to be created.
 *
 *  DESCRIPTION: Creates a new fallback ccb and its associated timers
 *               and adds it to the linked list.
 *
 *  RETURNS: Indicates if the fallback ccb was created successfully
 *           or not.
 *
 */
boolean
sip_regmgr_create_fallback_ccb (CCM_ID ccm_id, line_t dn_line)
{
    const char fname[] = "sip_regmgr_create_fallback_ccb";
    ccsipCCB_t *ccb = NULL;
    boolean ccb_created = FALSE;
    fallback_ccb_t *fallback_ccb;
    static const char sipWaitTimerName[] = "sipWait";
    static const char sipRegRetryTimerName[] = "sipRetry";
    line_t fallback_line;

    if (((int)dn_line < 1) || ((int)dn_line > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, dn_line);
        return(FALSE);
    }

    if (ccm_id >= MAX_CCM) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"ccm id <%d> out of bounds.\n",
                          fname, ccm_id);
        return(FALSE);
    }
    /* Check if fallback ccb exists. If so, return */
    if (sip_regmgr_find_fallback_ccb_by_ccmid(ccm_id, NULL)) {
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"fallback ccb exists  for ccmid %d\n",
                              DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname), ccm_id);
        return(TRUE);
    }
    fallback_line = sip_regmgr_get_fallback_line_num();
    if (!fallback_line) {
        /* Couldn't get fallback line number */
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"couldn't get fallback line for ccmid %d\n",
                              DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname), ccm_id);
        return(FALSE);
    }
    fallback_ccb = (fallback_ccb_t *) cpr_calloc(1, sizeof(fallback_ccb_t));
    if (fallback_ccb) {
        fallback_ccb->WaitTimer.timer = cprCreateTimer(sipWaitTimerName,
                                                       SIP_WAIT_TIMER,
                                                       TIMER_EXPIRATION,
                                                       sip_msgq);

        fallback_ccb->RetryTimer.timer = cprCreateTimer(sipRegRetryTimerName,
                                                        SIP_RETRY_TIMER,
                                                        TIMER_EXPIRATION,
                                                        sip_msgq);
        fallback_ccb->tls_socket_waiting = FALSE;
        if (!fallback_ccb->WaitTimer.timer || !fallback_ccb->RetryTimer.timer) {
            CCSIP_DEBUG_ERROR("%s: failed to create one or more"
                              " UISM timers\n", fname);
            if (fallback_ccb->WaitTimer.timer) {
                (void) cprCancelTimer(fallback_ccb->WaitTimer.timer);
                (void) cprDestroyTimer(fallback_ccb->WaitTimer.timer);
                fallback_ccb->WaitTimer.timer = NULL;
            }
            if (fallback_ccb->RetryTimer.timer) {
                (void) cprCancelTimer(fallback_ccb->RetryTimer.timer);
                (void) cprDestroyTimer(fallback_ccb->RetryTimer.timer);
                fallback_ccb->RetryTimer.timer = NULL;
            }
            ccb_created = FALSE;
        }
        ccb = (ccsipCCB_t *) cpr_calloc(1, sizeof(ccsipCCB_t));
        if (ccb != NULL) {
            (void) sip_sm_ccb_init(ccb, fallback_line, dn_line,
                                   SIP_REG_STATE_IN_FALLBACK);
            ccb->cc_type = CC_CCM;
            ccb->cc_cfg_table_entry = CCM_Config_Table[dn_line - 1][ccm_id];
            sstrncpy(ccb->reg.proxy,
                     CCM_Config_Table[dn_line - 1][ccm_id]->ti_common.addr_str,
                     MAX_IPADDR_STR_LEN);
            ccb->reg.addr = CCM_Config_Table[dn_line - 1][ccm_id]->ti_common.addr;
            ccb->reg.port = (uint16_t)
                CCM_Config_Table[dn_line - 1][ccm_id]->ti_common.port;
            ccb->dest_sip_addr =
                CCM_Config_Table[dn_line - 1][ccm_id]->ti_common.addr;
            ccb->dest_sip_port =
                CCM_Config_Table[dn_line - 1][ccm_id]->ti_common.port;
            ccb->local_port = CCM_Config_Table[dn_line - 1][ccm_id]->ti_common.listen_port;
            fallback_ccb->ccb = ccb;
            (void) sll_append(fallback_ccb_list, fallback_ccb);
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Created fallback ccb for %s:%d with line %d\n",
                                  DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname), ccb->reg.proxy, ccb->reg.port,
                                  fallback_line);
            ccb_created = TRUE;
        } else {
            CCSIP_DEBUG_ERROR(DEB_F_PREFIX"Memalloc failed for ccb for CCM-id %d\n",
                                  DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname), ccm_id);
            sip_regmgr_clean_fallback_ccb(fallback_ccb);
            if (fallback_ccb) {
                cpr_free(fallback_ccb);
            }
        }
    } else {
        CCSIP_DEBUG_ERROR(DEB_F_PREFIX"Memalloc failed for fallback ccb for CCM-id %d\n",
                              DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname), ccm_id);
        sip_regmgr_return_fallback_line_num(fallback_line);
    }
    return (ccb_created);
}

/*
 ** sip_regmgr_trigger_fallback_monitor
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:void
 *
 *  DESCRIPTION: Walks through the linked list of fallback ccbs
 *               and starts the retry timer on them and starts
 *               sending keepalive messages to monitor the failed
 *               ccms.
 *
 *  RETURNS: void.
 *
 */
void
sip_regmgr_trigger_fallback_monitor (void)
{
    const char fname[] = "sip_regmgr_trigger_fallback_monitor";
    fallback_ccb_t *fallback_ccb = NULL;
    ccsipCCB_t *ccb = NULL;

    /*
     * Go through the linked list of failed ccm's and
     * start monitoring them.
     */

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Looking to trigger fallback "
                          "if any available\n", DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname));
    do {
        fallback_ccb = (fallback_ccb_t *) sll_next(fallback_ccb_list,
                                                   (void *)fallback_ccb);
        if (fallback_ccb) {
            ti_config_table_t *ccm_table_entry;
            ccb = fallback_ccb->ccb;
            if (ccb->state == (int) SIP_REG_PRE_FALLBACK) {
                char user[MAX_LINE_NAME_SIZE];

                /*
                 * If state is TokenWait,
                 * Then transition back into InFallback state.
                 * Else send out a keepalive message to the registration server.
                 */
                sip_util_get_new_call_id(ccb);
                ccb->authen.cred_type = 0;
                ccb->retx_counter     = 0;
                ccb->reg.tmr_expire   = 0;
                ccb->reg.act_time     = 0;
                config_get_line_string(CFGID_LINE_NAME, user, ccb->dn_line, sizeof(user));
                sip_reg_sm_change_state(ccb, SIP_REG_STATE_IN_FALLBACK);
                ccm_table_entry = (ti_config_table_t *) ccb->cc_cfg_table_entry;
                if (ccm_table_entry->ti_common.handle != INVALID_SOCKET) {
                    (void) sipSPISendRegister(ccb, 0, user, 0);
                }

                /*
                 * Start the ack, retry timer
                 */
                sip_regmgr_retry_timer_start(fallback_ccb);
                CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Started monitoring %s:%d\n",
                                      DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname),
                                      ccb->reg.proxy, ccb->reg.port);
            } else {
                CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"fallback is in progress ccb idx=%d",
                                      DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname),ccb->index);
            }
        }
    } while (fallback_ccb);
}

void set_active_ccm(ti_config_table_t *cfg_table_entry) {
    CCM_Active_Standby_Table.active_ccm_entry = cfg_table_entry;
    if (cfg_table_entry != NULL) {
        DEF_DEBUG("set_active_ccm: ccm=%s  port=%d",
        CCM_ID_PRINT(cfg_table_entry->ti_specific.ti_ccm.ccm_id),
        phone_local_tcp_port[cfg_table_entry->ti_specific.ti_ccm.ccm_id]);
    } else {
        DEF_DEBUG("set_active_ccm: ccm=PRIMARY  port=-1");
    }
}


/*
 ** sip_regmgr_setup_new_active_ccb
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: pointer to the cfg table entry of the call manager
 *              that is going to become the new active ccm.
 *
 *  DESCRIPTION: Sets up the reg ccb's for all the lines to point
 *               to the new call manager during failover/fallback.
 *
 *  RETURNS: void.
 *
 */
void
sip_regmgr_setup_new_active_ccb (ti_config_table_t *cfg_table_entry)
{
    const char fname[] = "sip_regmgr_setup_new_active_ccb";
    line_t ndx;
    ccsipCCB_t *line_ccb;

    for (ndx = REG_CCB_START; ndx < REG_CCB_END; ndx++) {
        line_ccb = sip_sm_get_ccb_by_index(ndx);
        ui_set_sip_registration_state(line_ccb->dn_line, FALSE);

        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"cancelling timers, line= %d\n",
            DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname), line_ccb->index);
        (void) sip_platform_register_expires_timer_stop(line_ccb->index);
        sip_stop_ack_timer(line_ccb);
        line_ccb->reg.registered = 0;
        sip_reg_sm_change_state(line_ccb, SIP_REG_STATE_IDLE);
        sip_sm_call_cleanup(line_ccb);
        // Since we are going to point this CCB to a new CCM, get a new call-id
        line_ccb->sipCallID[0] = '\0';
        sip_util_get_new_call_id(line_ccb);
        line_ccb->cc_cfg_table_entry = (void *) cfg_table_entry;

        if (cfg_table_entry == NULL) {
            CCSIP_DEBUG_REG_STATE("%s: param cfg_table_entry is NULL!!!\n", fname);
            continue;
        }
        else {
            sstrncpy(line_ccb->reg.proxy, cfg_table_entry->ti_common.addr_str,
                 MAX_IPADDR_STR_LEN);
            line_ccb->dest_sip_addr = cfg_table_entry->ti_common.addr;
            line_ccb->dest_sip_port = (uint16_t) cfg_table_entry->ti_common.port;
            line_ccb->reg.addr = cfg_table_entry->ti_common.addr;
            line_ccb->reg.port = (uint16_t) cfg_table_entry->ti_common.port;
            /*
             * Modify destination fields in call back timer struct
             */
            (void) sip_platform_msg_timer_update_destination(line_ccb->index,
                                                         &(line_ccb->reg.addr),
                                                         line_ccb->reg.port);
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Updated active to %s:%d\n",
                              DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname),
                              line_ccb->reg.proxy, line_ccb->reg.port);
        }
    }
    set_active_ccm(cfg_table_entry);
}

/*
 ** sip_regmgr_setup_new_standby_ccb
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: pointer to the cfg table entry of the call manager
 *              that is going to become the new standby ccm.
 *
 *  DESCRIPTION: Sets up the reg backup ccb for all the lines to point
 *               to the new call manager during failover/fallback.
 *
 *  RETURNS: void.
 *
 */
void
sip_regmgr_setup_new_standby_ccb (CCM_ID ccm_index)
{
    const char fname[] = "sip_regmgr_setup_new_standby_ccb";
    ccsipCCB_t *ccb;
    ti_config_table_t *cfg_table_entry;


    ccb = sip_sm_get_ccb_by_index(REG_BACKUP_CCB);
    if (((int)ccb->dn_line < 1) || ((int)ccb->dn_line > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, ccb->dn_line);
        return;
    }
    if (ccm_index >= MAX_CCM) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"ccm id <%d> out of bounds.\n",
                          fname, ccm_index);
        return;
    }
    cfg_table_entry = CCM_Config_Table[ccb->dn_line - 1][ccm_index];

    ccsip_register_cleanup(ccb, FALSE);
    // Since we are going to point this CCB to a new CCM, get a new call-id
    ccb->sipCallID[0] = '\0';
    sip_util_get_new_call_id(ccb);
    sip_reg_sm_change_state(ccb, SIP_REG_STATE_UNREGISTERING);
    ccb->cc_cfg_table_entry = (void *) cfg_table_entry;
    sstrncpy(ccb->reg.proxy, cfg_table_entry->ti_common.addr_str,
             MAX_IPADDR_STR_LEN);
    ccb->reg.addr = cfg_table_entry->ti_common.addr;
    ccb->reg.port = (uint16_t)
        cfg_table_entry->ti_common.port;
    ccb->dest_sip_addr = cfg_table_entry->ti_common.addr;
    ccb->dest_sip_port = cfg_table_entry->ti_common.port;
    ccb->local_port = cfg_table_entry->ti_common.listen_port;
    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"For ccb_index=REG_BACKUP_CCB=%d, updated standby to %s:%d\n",
                          DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname),
                         REG_BACKUP_CCB, ccb->reg.proxy, ccb->reg.port);
    CCM_Active_Standby_Table.standby_ccm_entry = cfg_table_entry;
}

/*
 ** sip_regmgr_ccm_get_next
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: ccb pointer
 *
 *  DESCRIPTION: Gets the next ccm in the list for failover.
 *
 *  RETURNS: pointer to the config table of the next ccm in line.
 *
 */
ti_config_table_t *
sip_regmgr_ccm_get_next (ccsipCCB_t *ccb, CC_POSITION from_cc)
{
    /*
     * If from_cc is ACTIVE_CC, then check to see if the current
     * standby is valid. If it is then set that to be the ACTIVE_CC,
     * and return that. If there is no current standby, then see if
     * we have WAN failure.
     * Add the failed units in the fallback ccm list and trigger
     * fallback algorithm on them.
     */
    const char fname[] = "sip_regmgr_ccm_get_next";
    ti_config_table_t *ccm_table_ptr = NULL,
                      *ccm_table_active_entry = NULL,
                      *ccm_table_standby_entry = NULL;
    CCM_ID ccm_id, ccm_index;
    CC_POSITION from_cc_save = NONE_CC;


    ccm_table_ptr = (ti_config_table_t *) ccb->cc_cfg_table_entry;

    if (!ccm_table_ptr) {
        return (NULL);
    }
    ccm_id = (ccm_table_ptr->ti_specific.ti_ccm.ccm_id);

    /*
     * Update the cfg table to invalidate the current ccm that
     * has gone down in the case of tcp and create the fallback
     * ccb.
     * Already taken care of in the ccsip_platform_tcp.c function
     * sip_tcp_createconnfailed_to_spi()
     */
    if (ccm_table_ptr->ti_common.conn_type != CONN_UDP) {
        int connid;

        connid = sip_tcp_fd_to_connid(ccm_table_ptr->ti_common.handle);
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"clear the socket and port for "
            "current active cucm_id=%d connid=%d\n",
             DEB_F_PREFIX_ARGS(SIP_FAILOVER, fname),ccm_id, connid );
        sip_tcp_purge_entry(connid);
        sipTransportSetServerHandleAndPort(INVALID_SOCKET, 0,
                                           ccm_table_ptr);
    }

    // Update CCM status

    ui_set_ccm_conn_status(ccm_table_ptr->ti_common.addr_str, CCM_STATUS_NONE);


    (void) sip_regmgr_create_fallback_ccb(ccm_id, ccb->dn_line);

    if (from_cc == ACTIVE_CC) {
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Came here from cucm\n", DEB_F_PREFIX_ARGS(SIP_FAILOVER, fname));
        if (CCM_Active_Standby_Table.standby_ccm_entry) {
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"old ccm_id=%d new_ccm_id=%d Standby=NULL\n",
                DEB_F_PREFIX_ARGS(SIP_FAILOVER, fname), ccm_id,
                CCM_Active_Standby_Table.standby_ccm_entry->ti_specific.ti_ccm.ccm_id);
            ccm_table_ptr = CCM_Active_Standby_Table.standby_ccm_entry;
            ccm_id = (ccm_table_ptr->ti_specific.ti_ccm.ccm_id);
            /*
             * The current standby is going to be tried for the ACTIVE CC
             * role, so fill the standby_ccm_entry with the next valid cc
             * info.
             */
            set_active_ccm(ccm_table_ptr);
            CCM_Active_Standby_Table.standby_ccm_entry = NULL;
            /*
             * Save the ccm_table_ptr to return the correct value
             */
            ccm_table_active_entry = ccm_table_ptr;
            /*
             * We have used the entry of the current standby for our
             * active cc. So go ahead and find the new standby entry
             */
            ccb = sip_sm_get_ccb_by_index(REG_BACKUP_CCB);

            ccm_table_ptr = NULL;
            from_cc_save = ACTIVE_CC;
        } else {
            /*
             * There is no Standby available and the Acitve has
             * failed. No CC available. Initiate Reboot !
             */
            ccsip_register_set_register_state(SIP_REG_NO_CC);
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"NO CC AVAILABLE. NEED TO REBOOT !\n",
                                  DEB_F_PREFIX_ARGS(SIP_FAILOVER, fname));
            set_active_ccm(NULL);

            return (NULL);
        }
    }

    for (ccm_index = (CCM_ID) (ccm_id + 1); ccm_index < MAX_CCM; ccm_index++) {
        ti_ccm_t *ti_ccm = &CCM_Config_Table[ccb->dn_line - 1][ccm_index]->
                               ti_specific.ti_ccm;

        if (ti_ccm->is_valid) {
            ccm_table_standby_entry = sip_regmgr_ccm_get_conn(ccb->dn_line,
                (ti_config_table_t *) CCM_Config_Table[ccb->dn_line - 1][ccm_index]);
            if (ccm_table_standby_entry == NULL) {
                /*
                 * Create a fallback_ccm ccb and put it in a queue to use
                 * later (POST_FAILOVER)
                 */
                CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Could not set transport"
                    "connection to this standby. Ignore this. Continue search \n",
                    DEB_F_PREFIX_ARGS(SIP_FAILOVER, fname) );

                (void) sip_regmgr_create_fallback_ccb(ccm_index, ccb->dn_line);
            } else {
                /*
                 * Will always be REG_BACKUP_CCB
                 * Found our 'next' ccm
                 */
                CCM_Active_Standby_Table.standby_ccm_entry =
                    ccm_table_standby_entry;
                CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"new standby ccm id %d ip=%s ccm_id=%d\n",
                    DEB_F_PREFIX_ARGS(SIP_FAILOVER, fname), ccm_index,
                    ccm_table_standby_entry->ti_common.addr_str, ti_ccm->ccm_id);
                break;
            }
        }
    }

    /*
     * We can wait until the active line re-registers to trigger
     * this but doing it as soon as we find there is no backup
     * available
     */
    if (ccm_table_standby_entry == NULL) {
        /*
         * regmgr - FAILOVER Trigger fallback monitoring
         */
        ccsip_register_set_register_state(SIP_REG_NO_STANDBY);
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Setting register state to SIP_REG_NO_STANDBY \n",
            DEB_F_PREFIX_ARGS(SIP_FAILOVER, fname) );

        ccb = sip_sm_get_ccb_by_index(REG_BACKUP_CCB);

        sip_sm_call_cleanup(ccb);
        // No new standby ccm. Cleanup standby ccm address, port and timers
        sip_regmgr_clean_standby_ccb(ccb);
    }
    if (from_cc_save == ACTIVE_CC) {
        ccm_table_ptr = ccm_table_active_entry;
    } else {
        ccm_table_ptr = ccm_table_standby_entry;
    }
    return (ccm_table_ptr);
}

/*
 ** sip_regmgr_tls_retry_timer_start
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: fallback ccb pointer
 *
 *  DESCRIPTION: Starts the tls retry timer on the fallback ccb
 *  Normally secd takes 3-6 secs to establish a connection. Start
 *  timer to check the status after 8s. Restart keep alive timer for the
 *  remaining time if socket is invalid after 8s
 *
 *  RETURNS: void
 *
 */
static void
sip_regmgr_tls_retry_timer_start (fallback_ccb_t *fallback_ccb)
{
    const char fname[] = "sip_regmgr_tls_retry_timer_start";
    ccsipCCB_t *ccb = NULL;
    int timeout;

    if (!fallback_ccb) {
        return;
    }
    ccb = fallback_ccb->ccb;
    if (fallback_ccb->tls_socket_waiting) {
        /* socket invalid after TLS_CONNECT_TIME(8s)
         * Restart timer for the remaining time(eg:120-8 s)
         */
        timeout = sip_config_get_keepalive_expires();

        if (timeout > MAX_FALLBACK_MONITOR_PERIOD) {
            timeout = MAX_FALLBACK_MONITOR_PERIOD;
        }
        if (timeout > TLS_CONNECT_TIME) {
            timeout -= TLS_CONNECT_TIME;
        }
        fallback_ccb->tls_socket_waiting = FALSE;
    } else {
        /* start timer to monitor the socket status */
        timeout = TLS_CONNECT_TIME;
        fallback_ccb->tls_socket_waiting = TRUE;;
    }
    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Starting TLS timer (%d sec)\n",
                          DEB_L_C_F_PREFIX_ARGS(SIP_FALLBACK, ccb->index, ccb->dn_line, fname), timeout);
    if (cprStartTimer(fallback_ccb->RetryTimer.timer, timeout * 1000,
                fallback_ccb) == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          ccb->index, 0, fname, "cprStartTimer");
        sip_regmgr_generic_timer_start_failure(fallback_ccb, SIP_TMR_REG_RETRY);
    }
}

/*
 ** sip_regmgr_retry_timer_start
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: fallback ccb pointer
 *
 *  DESCRIPTION: Starts the retry timer on the fallback ccb
 *
 *  RETURNS: void
 *
 */
void
sip_regmgr_retry_timer_start (fallback_ccb_t *fallback_ccb)
{
    const char fname[] = "sip_regmgr_retry_timer_start";
    ccsipCCB_t *ccb = NULL;
    int timeout;

    if (!fallback_ccb) {
        return;
    }

    ccb = fallback_ccb->ccb;

    timeout = sip_config_get_keepalive_expires();

    if (timeout > MAX_FALLBACK_MONITOR_PERIOD) {
       timeout = MAX_FALLBACK_MONITOR_PERIOD;
    }

    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Starting fallback timer (%d sec)\n",
                          DEB_L_C_F_PREFIX_ARGS(SIP_FALLBACK, ccb->index, ccb->dn_line, fname), timeout);

    ccb->retx_flag = TRUE;
    if (cprStartTimer(fallback_ccb->RetryTimer.timer, timeout * 1000,
                fallback_ccb) == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          ccb->index, 0, fname, "cprStartTimer");
        sip_regmgr_generic_timer_start_failure(fallback_ccb, SIP_TMR_REG_RETRY);
        ccb->retx_flag = FALSE;
    }
}

/*
 ** sip_regmgr_retry_timeout_expire
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: void *data (fallback ccb)
 *
 *  DESCRIPTION: Is the callback function for retry timer
 *
 *  RETURNS: void
 *
 */
void
sip_regmgr_retry_timeout_expire (void *data)
{
    static const char fname[] = "sip_regmgr_retry_timeout_expire";
    fallback_ccb_t *fallback_ccb;
    ccsipCCB_t *ccb;

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"\n", DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname));
    fallback_ccb = (fallback_ccb_t *) data;
    if (!fallback_ccb) {
        return;
    }
    ccb = (ccsipCCB_t *) fallback_ccb->ccb;
    if (ccsip_register_send_msg(SIP_TMR_REG_RETRY, ccb->index) != SIP_REG_OK) {
        sip_regmgr_generic_message_post_failure(fallback_ccb,
                                                SIP_TMR_REG_RETRY);
    }
}

/*
 ** sip_regmgr_set_stability_total_msgs
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: fallback ccb pointer and a boolean indicating if
 *              it is in a state where we are waiting for the
 *              phone to get idle before transitioning into token
 *              wait state.
 *
 *  DESCRIPTION: Starts the stability timer on the fallback ccb
 *
 *  RETURNS: void
 *
 */
void
sip_regmgr_set_stability_total_msgs (fallback_ccb_t *fallback_ccb)
{
    const char fname[] = "sip_regmgr_set_stability_total_msgs";
    ccsipCCB_t *ccb = NULL;
    int timer_keepalive_expires;
    int connection_mode_duration;

    if (!fallback_ccb) {
        return;
    }
    ccb = fallback_ccb->ccb;

    config_get_value(CFGID_CONN_MONITOR_DURATION,
                     &connection_mode_duration, sizeof(connection_mode_duration));

    timer_keepalive_expires = sip_config_get_keepalive_expires();

    /* Stability count is used to wait to make sure that wan is not flapping */
    fallback_ccb->StabilityMsgCount = connection_mode_duration / timer_keepalive_expires;
    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Starting stability msg count as %d\n",
                          DEB_L_C_F_PREFIX_ARGS(SIP_FALLBACK, ccb->index, ccb->dn_line, fname),
                          fallback_ccb->StabilityMsgCount);
}

/*
 ** sip_regmgr_wait_timer_start
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: fallback ccb pointer
 *
 *  DESCRIPTION: Starts the wait timer on the fallback ccb
 *
 *  RETURNS: void
 *
 */
void
sip_regmgr_wait_timer_start (fallback_ccb_t *fallback_ccb)
{
    const char fname[] = "sip_regmgr_wait_timer_start";
    ccsipCCB_t *ccb = NULL;
    int timeout;

    if (!fallback_ccb) {
        return;
    }
    ccb = fallback_ccb->ccb;

    timeout = sip_config_get_keepalive_expires();

    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Starting wait timer (%d sec)\n",
                          DEB_L_C_F_PREFIX_ARGS(SIP_FALLBACK, ccb->index, ccb->dn_line, fname), timeout);

    if (cprStartTimer(fallback_ccb->WaitTimer.timer, timeout * 1000,
                fallback_ccb) == CPR_FAILURE) {
        CCSIP_DEBUG_REG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                              ccb->index, 0, fname, "cprStartTimer");
        sip_regmgr_generic_timer_start_failure(fallback_ccb, SIP_TMR_REG_WAIT);
    }
}

/*
 ** sip_regmgr_wait_timeout_expire
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: void *data (fallback ccb)
 *
 *  DESCRIPTION: Is the callback function for wait timer
 *
 *  RETURNS: void
 *
 */
void
sip_regmgr_wait_timeout_expire (void *data)
{
    static const char fname[] = "sip_regmgr_wait_timeout_expire";
    fallback_ccb_t *fallback_ccb;
    ccsipCCB_t *ccb;

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"\n", DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname));
    fallback_ccb = (fallback_ccb_t *) data;
    ccb = (ccsipCCB_t *) fallback_ccb->ccb;

    sip_regmgr_fallback_generic_timer_stop(fallback_ccb->WaitTimer.timer);

    if (ccsip_register_send_msg(SIP_TMR_REG_WAIT, ccb->index) != SIP_REG_OK) {
        sip_regmgr_generic_message_post_failure(fallback_ccb, SIP_TMR_REG_WAIT);
    }
}

void
sip_regmgr_fallback_generic_timer_stop (cprTimer_t timer)
{
    static const char fname[] = "sip_regmgr_fallback_generic_timer_stop";

    if (cprCancelTimer(timer) == CPR_FAILURE) {
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"%s failed!\n", DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname), "cprCancelTimer");
    }

    return;
}


/*
 ** sip_regmgr_ev_fallback_retry
 *
 *
 *  PARAMETERS: ccb and event
 *
 *  DESCRIPTION: Event handler for 4xx, 5xx, 6xx responses in fallback state
 *               Start retry timer to monitor the fallback ccm
 *               Wait for the timer to pop to send the next keepalive.
 *
 *
 *  RETURNS: void
 *
 */
void
sip_regmgr_ev_fallback_retry (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    const char *fname = "sip_regmgr_ev_fallback_retry";
    fallback_ccb_t *fallback_ccb = NULL;

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Recd retry event for LINE %d/%d in state %d\n",
                          DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname), ccb->index, ccb->dn_line, ccb->state);

    sip_stop_ack_timer(ccb);
    fallback_ccb = sip_regmgr_get_fallback_ccb_by_index(ccb->index);
    if (fallback_ccb) {
        sip_regmgr_retry_timer_start(fallback_ccb);
    }
    free_sip_message(event->u.pSipMessage);
}

void
sip_regmgr_ev_default (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    const char *fname = "sip_regmgr_ev_default";

    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Received a default event in state %d\n",
                          DEB_L_C_F_PREFIX_ARGS(SIP_EVT, ccb->index, ccb->dn_line, fname), ccb->state);

    sip_reg_sm_change_state(ccb, SIP_REG_STATE_IN_FALLBACK);
    sip_regmgr_ev_tmr_ack_retry(ccb, event);
    /* only free SIP messages, timeouts are internal */
    if (event->type < (int) E_SIP_REG_TMR_ACK) {
        free_sip_message(event->u.pSipMessage);
    }
}

/*
 ** sip_regmgr_ev_tmr_ack_retry
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: ccb and event
 *
 *  DESCRIPTION: Event handler for ack and retry timers. Please
 *               see inline comments for the description.
 *
 *  RETURNS: void
 *
 */
void
sip_regmgr_ev_tmr_ack_retry (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    const char *fname = "sip_regmgr_ev_tmr_ack_retry";
    ti_config_table_t *ccm_table_ptr;
    fallback_ccb_t *fallback_ccb;
    CCM_ID current_standby_ccm_id, fallback_ccm_id;

    /*
     * If state is REGISTERING, set state to IN_FAILOVER.
     * Mark all lines as unregistered and IDLE state.
     * Create fallback ccb for the failed ccm and put it in
     * a queue to be triggered post failover.
     * Find the "next" ccm and set the values in the ccb.
     * Need change state to IDLE and post REG_REQ, make sure
     * we set the no_dns_lookup parameter to 1.
     * If state is IN_FALLBACK, send register message
     * If state is STABILITY_CHECK, transition back to IN_FALLBACK
     * and send register with expire 0.
     * If state is TOKEN_WAIT, transition back to IN_FALLBACK
     * and send register with expire 0.
     */
    ccb->retx_counter = 0;
    switch ((sipRegSMStateType_t) ccb->state) {
    case SIP_REG_STATE_REGISTERED:
        /*
         * A send failure on a tcp for active cc could have caused
         * this message.
         * Fall through and do the same as what is done when in
         * REGISTERING State.
         */
    case SIP_REG_STATE_REGISTERING:
        /*
         * Get the next ccm to use as active.
         */
        ccm_table_ptr = (ti_config_table_t *) ccb->cc_cfg_table_entry;
        if (ccm_table_ptr != CCM_Active_Standby_Table.active_ccm_entry) {
            break;
        }
        ccm_table_ptr = NULL;

        ccm_table_ptr = sip_regmgr_ccm_get_next(ccb, ACTIVE_CC);

        if (ccm_table_ptr == NULL) {
				/*
             	 * Send indication of REG_ALLFAIL so platform can
             	 * initiate a reboot.
             	 */
            	CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Unable to get next ccm!\n", DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname));
            	// Cleanup fallback CCB list
            	sip_regmgr_free_fallback_ccb_list();
            	sip_reg_all_failed = TRUE;
            	sip_regmgr_handle_reg_all_fail();
            	break;
        }

		CCSIP_DEBUG_REG_STATE("%s: ccb information: ccb->dn_line=%d, ccb->index=%d, retry_times=%d\n",
							   fname, ccb->dn_line, ccb->index, retry_times);

		CCM_Failover_Table.failover_ccm_entry = ccm_table_ptr;
        CCM_Failover_Table.failover_started = TRUE;
        CCM_Failover_Table.prime_registered = FALSE;
        CCM_Fallback_Table.fallback_ccm_entry = NULL;
        (void) sip_platform_register_expires_timer_stop(ccb->index);
        sip_stop_ack_timer(ccb);
        sip_platform_msg_timer_stop(ccb->index);
        sip_platform_failover_ind(ccm_table_ptr->ti_specific.ti_ccm.ccm_id);

        /*
        *   Notes for case: (CSCsx60672)
        *   We should change the info in REG_BACKUP_CCB for the case that:
        *   Active CCB failover
        *   previous standby CCM is set as CCM_Active_Standby_Table.active_ccm_entry (#1 IPAddr A)
        *   Third CCM would be set as  CCM_Active_Standby_Table.standby_ccm_entry (#2 IPAddr B)
        *
        *   if (#2) failover before Jphone callback,
        *   The REG_BACKUP_CCB would still store the #1's info
        *   So the call flows would prompt the wrong info(#1 IPAddr A) to Jphone, in this case, (#2 IPAddr B)
        *      should be prompted
        *
        *   We need to update the REG_BACKUP_CCB's info here
        */
        if (CCM_Active_Standby_Table.standby_ccm_entry)
        {
            ti_ccm_t *ti_ccm = &(CCM_Active_Standby_Table.standby_ccm_entry->ti_specific.ti_ccm);
            sip_regmgr_setup_new_standby_ccb(ti_ccm->ccm_id);
            CCSIP_DEBUG_REG_STATE("%s: Setup new standby ccb, ccm_id is %d\n", fname, ti_ccm->ccm_id);
        }

        break;
    case SIP_REG_STATE_UNREGISTERING:

        if (ccb->index == REG_BACKUP_CCB) {
            sip_stop_ack_timer(ccb);
            sip_platform_msg_timer_stop(ccb->index);
            ccm_table_ptr = sip_regmgr_ccm_get_next(ccb, STANDBY_CC);
            if (ccm_table_ptr) {
                /*
                 * New standby, start monitoring it.
                 * Change the ip addr and port to point to the new ccm
                 * Set the CC_CONFIG_TABLE entry for all the lines to
                 * point to the new ccm.
                 * Set the ccb->cc_cfg_table_entry with the new entry.
                 */
                ti_ccm_t *ti_ccm;

                ti_ccm = &ccm_table_ptr->ti_specific.ti_ccm;
                sip_regmgr_setup_new_standby_ccb(ti_ccm->ccm_id);
                CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Start monitoring new standby cc\n",
                                      DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname));
                (void) ccsip_register_send_msg(SIP_REG_CANCEL, ccb->index);

                ui_set_ccm_conn_status(ccm_table_ptr->ti_common.addr_str,
                                       CCM_STATUS_STANDBY);

            } else {
                CCM_Active_Standby_Table.standby_ccm_entry = NULL;
                CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Unable to get next standby ccm !\n",
                                      DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname));
            }
            sip_regmgr_trigger_fallback_monitor();
        } else {
            /*
             * Received a timeout when the active tried to unregister
             * after the expires timer.
             */
            sip_reg_sm_change_state(ccb, SIP_REG_STATE_IDLE);

            if (ccsip_register_all_unregistered() == TRUE) {
                ccsip_register_set_register_state(SIP_REG_IDLE);
            }

            /*
             * Always check how sip_sm_call_cleanup is implemented.
             * The function has a tendency to change.
             */
            sip_sm_call_cleanup(ccb);
        }

        break;
    case SIP_REG_STATE_STABILITY_CHECK:
        /*
         * We getting here means that the wan link could have gone
         * down again. We stop the timers associated with the
         * carefree algorithm and transition back to the
         * IN_FALLBACK state.
         */
        fallback_ccb = sip_regmgr_get_fallback_ccb_by_index(ccb->index);
        if (fallback_ccb) {
            sip_regmgr_fallback_generic_timer_stop(fallback_ccb->WaitTimer.timer);
        }
        /*
         * FALL THROUGH ALERT to the next state.
         */
        /*sa_ignore FALL_THROUGH*/
    case SIP_REG_STATE_TOKEN_WAIT:
        sip_reg_sm_change_state(ccb, SIP_REG_STATE_IN_FALLBACK);
        /*
         * FALL THROUGH ALERT ! get into the next case and
         * start sending of keepalives to the newly failed
         * ccm.
         */
         /*sa_ignore FALL_THROUGH*/
    case SIP_REG_STATE_IN_FALLBACK:
        /*
         * Send Register msg with expiry set to zero (poll)
         */
        {
            char user[MAX_LINE_NAME_SIZE];
            fallback_ccb_t *fallback_ccb2;
            ti_config_table_t *ccm_table_entry;

            fallback_ccb2 = (fallback_ccb_t *) sll_find(fallback_ccb_list,
                                                       (void *)(long)ccb->index);
            ccm_table_entry = (ti_config_table_t *) ccb->cc_cfg_table_entry;
            /*
             * Check here to see if we still need this fallback ccb
             * to be polling its ccm. The reason is that one of the
             * ccms that has a higher precedence to this ccm could have
             * come back online, in which case we can stop monitoring
             * this ccm.
             */
            if (CCM_Active_Standby_Table.standby_ccm_entry) {
                current_standby_ccm_id = CCM_Active_Standby_Table.
                    standby_ccm_entry->ti_specific.ti_ccm.ccm_id;
                fallback_ccm_id = ccm_table_entry->ti_specific.ti_ccm.ccm_id;

                if (current_standby_ccm_id < fallback_ccm_id) {
                    /*
                     * Clean the current fallback ccb and free it
                     */
                    DEF_DEBUG(DEB_F_PREFIX"Freeing the fallback ccb for %d ccm as current standby"
                                " is %d ccm!\n",
                                DEB_F_PREFIX_ARGS(SIP_REG_FREE_FALLBACK, fname),
                                fallback_ccm_id, current_standby_ccm_id);
                    sip_regmgr_free_fallback_ccb(ccb);
                    break;
                }
            }

            /*
             * For the TCP case where a connection has gone down,
             * We need to first try and setup the connection as
             * part of the recovery process. After that is when we
             * can start sending the keepalive messages. The following
             * 'if' block handles that case. The socket handle of the
             * config table is inited to INVALID_SOCKET when the connection
             * goes down.
             */
            if ((ccb->cc_type == CC_CCM)
                && (ccm_table_entry->ti_common.handle == INVALID_SOCKET)) {
                ti_common_t *ti_common;
                sipSPIMessage_t sip_msg;
                uint16_t listener_port;
                ti_ccm_t *ti_ccm;
                CONN_TYPE conn_type = CONN_NONE;
                cpr_socket_t server_conn_handle = INVALID_SOCKET;

                ti_common = &ccm_table_entry->ti_common;
                sip_msg.createConnMsg.addr = ti_common->addr;
                sip_msg.createConnMsg.port = ti_common->port;
                sip_msg.context = NULL;
                ti_ccm = &ccm_table_entry->ti_specific.ti_ccm;

                if (((ti_ccm->sec_level == AUTHENTICATED) ||
                     (ti_ccm->sec_level == ENCRYPTED)) &&
                    (ti_common->conn_type == CONN_TLS)) {
                    sip_msg.context = NULL;
                    if (fallback_ccb2 && (fallback_ccb2->tls_socket_waiting)) {
                        /* socket invalid after TLS_CONN_TIME
                         * Restart timer for the remaining time
                         */
                        sip_regmgr_tls_retry_timer_start(fallback_ccb2);
                        break;
                    }
                    server_conn_handle = sip_tls_create_connection(&sip_msg,
                                                                   FALSE,
                                                                   ti_ccm->sec_level);
                    conn_type = CONN_TLS;
                } else {
                    server_conn_handle = sip_tcp_create_connection(&sip_msg);
                }
                phone_local_tcp_port[ccm_table_entry->ti_specific.ti_ccm.ccm_id] =
                    sip_msg.createConnMsg.local_listener_port;
                if (server_conn_handle != INVALID_SOCKET) {
                    listener_port = sip_msg.createConnMsg.local_listener_port;
                    sipTransportSetServerHandleAndPort(server_conn_handle,
                                                       listener_port,
                                                       ccm_table_entry);
                    ccb->local_port = listener_port;
                    if ((conn_type == CONN_TLS) && fallback_ccb2) {
                        /* start timer to monitor the tls connect status */
                        sip_regmgr_tls_retry_timer_start(fallback_ccb2);
                        break;
                    }
                } else {
                    CCSIP_DEBUG_ERROR("%s: Error: "
                                      "sip_platform_tcp_channel_create("
                                      "server addr=%s, server port=%d) "
                                      "failed.\n", fname, ti_common->addr_str,
                                      ti_common->port);
                    if (fallback_ccb2) {
                        sip_regmgr_retry_timer_start(fallback_ccb2);
                    }
                    break;
                }
            }
            /*
             * Send a keepalive message and hope to get a reply back
             */
            clean_method_request_trx(ccb, sipMethodRegister, TRUE);
            sip_util_get_new_call_id(ccb);
            ccb->authen.cred_type = 0;
            ccb->retx_counter     = 0;
            ccb->reg.tmr_expire   = 0;
            ccb->reg.act_time     = 0;
            config_get_line_string(CFGID_LINE_NAME, user, ccb->dn_line, sizeof(user));
            /*
             * Start the retry timer
             */
            (void) sipSPISendRegister(ccb, 0, user, 0);
            if (fallback_ccb2) {
                sip_regmgr_retry_timer_start(fallback_ccb2);
            }
            break;
        }
    default:
        break;
    }
}

/*
 ** sip_regmgr_ev_in_fallback_2xx
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION: Event handler for the all responses
 *
 *  RETURNS:
 *
 */
void
sip_regmgr_ev_in_fallback_2xx (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    const char *fname = "sip_regmgr_ev_in_fallback_2xx";
    sipMessage_t *response = event->u.pSipMessage;
    int status_code = 0;
    fallback_ccb_t *fallback_ccb;

    clean_method_request_trx(ccb, sipMethodRegister, TRUE);

    if (sipGetResponseCode(response, &status_code) < 0) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_REG_SIP_RESP_CODE), ccb->index,
                          ccb->dn_line, fname);
        free_sip_message(response);
        return;
    }

    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Received a %d\n",
                          DEB_L_C_F_PREFIX_ARGS(SIP_FALLBACK, ccb->index, ccb->dn_line, fname), status_code);

    sip_stop_ack_timer(ccb);
    fallback_ccb = sip_regmgr_get_fallback_ccb_by_index(ccb->index);
    if (fallback_ccb) {
        sip_regmgr_fallback_generic_timer_stop(fallback_ccb->RetryTimer.timer);
    }
    sip_regmgr_check_and_transition(ccb);
    free_sip_message(response);
}

/*
 ** sip_regmgr_ev_stability_check_2xx
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION: Event handler for the all responses
 *
 *  RETURNS:
 *
 */
void
sip_regmgr_ev_stability_check_2xx (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    const char *fname = "sip_regmgr_ev_stability_check_2xx";
    fallback_ccb_t *fallback_ccb;
    sipMessage_t *response = event->u.pSipMessage;
    int status_code = 0;

    clean_method_request_trx(ccb, sipMethodRegister, TRUE);

    if (sipGetResponseCode(response, &status_code) < 0) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_REG_SIP_RESP_CODE), ccb->index,
                          ccb->dn_line, fname);
        free_sip_message(response);
        return;
    }

    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Received a %d\n",
                          DEB_L_C_F_PREFIX_ARGS(SIP_FALLBACK, ccb->index, ccb->dn_line, fname), status_code);

    fallback_ccb = sip_regmgr_get_fallback_ccb_by_index(ccb->index);
    if (!fallback_ccb) {
        free_sip_message(response);
        return;
    }
    sip_regmgr_fallback_generic_timer_stop(fallback_ccb->RetryTimer.timer);
    /*
     * Update stats here ?
     */
    if (fallback_ccb->StabilityMsgCount > 0) {
        fallback_ccb->StabilityMsgCount--;
    }
    if (fallback_ccb->StabilityMsgCount) {
        sip_regmgr_wait_timer_start(fallback_ccb);
    } else {
        wan_failure = FALSE;
        sip_regmgr_check_and_transition(ccb);
    }
    free_sip_message(response);
}

/*
 ** sip_regmgr_ev_stability_check_tmr_wait
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION:
 *
 *  RETURNS:
 *
 */
void
sip_regmgr_ev_stability_check_tmr_wait (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    const char *fname = "sip_regmgr_ev_stability_check_tmr_wait";
    fallback_ccb_t *fallback_ccb;

    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Received event\n",
             DEB_L_C_F_PREFIX_ARGS(SIP_EVT, ccb->index, ccb->dn_line, fname));
    fallback_ccb = sip_regmgr_get_fallback_ccb_by_index(ccb->index);
    (void) ccsip_register_send_msg(SIP_REG_CANCEL, ccb->index);
    if (fallback_ccb) {
        sip_regmgr_retry_timer_start(fallback_ccb);
    }
}

/*
 ** sip_regmgr_ev_token_wait_2xx
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: ccb and event
 *
 *  DESCRIPTION: TOKEN_WAIT state, set expire timer and transition
 *               to IDLE state.
 *
 *  RETURNS: none
 */
void
sip_regmgr_ev_token_wait_2xx (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    static const char fname[] = "sip_regmgr_ev_token_wait_2xx";
    fallback_ccb_t *fallback_ccb;
    sipMessage_t *response;
    int status_code = 0;
    CCM_ID fallback_ccm_id;
    ti_config_table_t *fallback_ccb_entry;

    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Received event\n",
                          DEB_L_C_F_PREFIX_ARGS(SIP_EVT, ccb->index, ccb->dn_line, fname));
    response = event->u.pSipMessage;
    clean_method_request_trx(ccb, sipMethodRegister, TRUE);

    if (sipGetResponseCode(response, &status_code) < 0) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_REG_SIP_RESP_CODE), ccb->index,
                          ccb->dn_line, fname);
        free_sip_message(response);
        return;
    }

    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Received a %d\n",
                          DEB_L_C_F_PREFIX_ARGS(SIP_EVT, ccb->index, ccb->dn_line, fname), status_code);

    sip_stop_ack_timer(ccb);
    fallback_ccb = sip_regmgr_get_fallback_ccb_by_index(ccb->index);
    if (fallback_ccb) {
        sip_regmgr_fallback_generic_timer_stop(fallback_ccb->WaitTimer.timer);

    }
    fallback_ccb_entry = (ti_config_table_t *) ccb->cc_cfg_table_entry;
    fallback_ccm_id    = fallback_ccb_entry->ti_specific.ti_ccm.ccm_id;

    if (CCM_Fallback_Table.fallback_ccm_entry == NULL) {
        CCM_Fallback_Table.fallback_ccm_entry = (ti_config_table_t *)
            ccb->cc_cfg_table_entry;
        CCM_Fallback_Table.is_idle = FALSE;
        CCM_Fallback_Table.is_resp = FALSE;
        CCM_Fallback_Table.ccb = ccb;
        sip_platform_fallback_ind(fallback_ccm_id);

    } else {
        sip_reg_sm_change_state(ccb, SIP_REG_STATE_IN_FALLBACK);
        if (fallback_ccb) {
            sip_regmgr_retry_timer_start(fallback_ccb);
        }
    }

    free_sip_message(response);
}

/*
 ** sip_regmgr_ev_cleanup
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: ccb and event
 *
 *  DESCRIPTION: cleanup primary
 *
 *  RETURNS: none
 */
void
sip_regmgr_ev_cleanup (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    static const char fname[] = "sip_regmgr_ev_cleanup";
    CCM_ID ccm_id;
    ti_config_table_t *cfg_table_entry;
    ccsipCCB_t *line_ccb = NULL;
    ti_common_t *ti_common;

    CCSIP_DEBUG_REG_STATE("%s:\n", fname);
    line_ccb = sip_sm_get_ccb_by_index(REG_CCB_START);
    if (ccb == NULL || line_ccb == NULL) {
        CCSIP_DEBUG_REG_STATE("%s: invalid ccb or line_ccb\n", fname);
        return;
    }

    cfg_table_entry = (ti_config_table_t *) line_ccb->cc_cfg_table_entry;
    /*
     * Setup the new active cc
     */
    sip_regmgr_setup_new_active_ccb((ti_config_table_t *)
                                    ccb->cc_cfg_table_entry);
    sip_regmgr_free_fallback_ccb(ccb);
    /*
     * Initiate registering with the new active cc
     */
    if (CCM_Fallback_Table.is_resp) {
        CCSIP_DEBUG_REG_STATE("%s: Register all lines\n", fname);
        ccsip_register_all_lines();
        CCM_Fallback_Table.fallback_ccm_entry = NULL;
    } else {
        CCSIP_DEBUG_REG_STATE("%s: Register prime line\n", fname);
        sip_regmgr_register_lines(TRUE, FALSE);
        CCM_Failover_Table.prime_registered = TRUE;
    }
    //set status of  current standby to NONE
    if (CCM_Active_Standby_Table.standby_ccm_entry) {
        ti_config_table_t *standby_table_entry;

        standby_table_entry = CCM_Active_Standby_Table.standby_ccm_entry;

        ui_set_ccm_conn_status(standby_table_entry->ti_common.addr_str,
                               CCM_STATUS_NONE);

    }
    /*
     * Setup the new standby.
     * CCM will close the TCP connection after unregistering from the
     * current active ccm if connection type is TCP/TLS
     * Close the TCP connection if one exists and open a new one
     */
    if (cfg_table_entry != NULL) {
        ti_common = &cfg_table_entry->ti_common;
        if (ti_common->conn_type != CONN_UDP) {
            if (ti_common->handle != INVALID_SOCKET) {
                int connid;

                CCSIP_DEBUG_REG_STATE("%s: Close the TCP connection\n", fname);
                connid = sip_tcp_fd_to_connid(ti_common->handle);
                sip_tcp_purge_entry(connid);
                ti_common->handle = INVALID_SOCKET;
            }
            CCSIP_DEBUG_REG_STATE("%s: Open a new connection\n", fname);
            (void) sip_regmgr_ccm_get_conn(line_ccb->dn_line, cfg_table_entry);
        }
        ccm_id = cfg_table_entry->ti_specific.ti_ccm.ccm_id;
        sip_regmgr_setup_new_standby_ccb(ccm_id);
    }
    /*
     * Trigger monitoring the new standby
     */
    (void) ccsip_register_send_msg(SIP_REG_CANCEL, REG_BACKUP_CCB);

    sip_platform_set_ccm_status();
}

/*
 ** sip_regmgr_ev_token_wait_4xx_n_5xx
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: ccb and event
 *
 *  DESCRIPTION: TOKEN_WAIT state, set expire timer and transition
 *               to IDLE state.
 *
 *  RETURNS: none
 */
void
sip_regmgr_ev_token_wait_4xx_n_5xx (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    static const char fname[] = "sip_regmgr_ev_token_wait_4xx_n_5xx";
    fallback_ccb_t *fallback_ccb;
    sipMessage_t   *response;
    int             status_code = 0;
    uint32_t        retry_after = 0;
    const char     *msg_ptr = NULL;
    int             timeout = 0;


    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Received event\n",
                          DEB_L_C_F_PREFIX_ARGS(SIP_EVT, ccb->index, ccb->dn_line, fname));
    response = event->u.pSipMessage;
    clean_method_request_trx(ccb, sipMethodRegister, TRUE);

    if (sipGetResponseCode(response, &status_code) < 0) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_REG_SIP_RESP_CODE), ccb->index,
                          ccb->dn_line, fname);
        free_sip_message(response);
        return;
    }

    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Received a %d\n",
                          DEB_L_C_F_PREFIX_ARGS(SIP_EVT, ccb->index, ccb->dn_line, fname), status_code);

    sip_stop_ack_timer(ccb);
    fallback_ccb = sip_regmgr_get_fallback_ccb_by_index(ccb->index);
    if (!fallback_ccb) {
        return;
    }
    sip_regmgr_fallback_generic_timer_stop(fallback_ccb->WaitTimer.timer);
    // Look for retry-after tag
    if (status_code == SIP_CLI_ERR_BUSY_HERE ||
        status_code == SIP_SERV_ERR_UNAVAIL) {
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Received a 486/503 response!\n",
                              DEB_F_PREFIX_ARGS(SIP_RESP, fname));
        msg_ptr = sippmh_get_header_val(response,
                                        (const char *)SIP_HEADER_RETRY_AFTER,
                                        NULL);
        if (msg_ptr) {
            retry_after = strtoul(msg_ptr, NULL, 10);
        }
        /*
         * Start timer with the retry-after value
         */
        if (retry_after > 0) {
            timeout = retry_after;
        } else {
            // use keep alive timer if retry_after in 486 is missing or 0
            timeout = sip_config_get_keepalive_expires();
        }
        if (cprStartTimer(fallback_ccb->WaitTimer.timer, timeout * 1000,
                    fallback_ccb) == CPR_FAILURE) {
            CCSIP_DEBUG_REG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                                  ccb->index, 0, fname, "cprStartTimer");
            sip_regmgr_generic_timer_start_failure(fallback_ccb,
                                                   SIP_TMR_REG_WAIT);
        }
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Started timer with retry-after for %d secs\n",
                              DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname), timeout);
    } else {
        /*
         * Unhandled 4xx message. start monitoring fallback ccb
         */
        sip_reg_sm_change_state(ccb, SIP_REG_STATE_IN_FALLBACK);
        sip_regmgr_retry_timer_start(fallback_ccb);
    }
}

void
sip_regmgr_ev_token_wait_tmr_wait (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    static const char fname[] = "sip_regmgr_ev_token_wait_tmr_wait";
    fallback_ccb_t *fallback_ccb;

    /*
     * Send a REFER message requesting for registering with
     * the callmanager that has come back up. Cleanup any
     * pending REFER transaction as new REFER is created for
     * next transmission.
     */
    clean_method_request_trx(ccb, sipMethodRefer, TRUE);
    if (sipSPISendRefer(ccb, TOKEN_REFER_TO, SIP_REF_TOKEN)) {
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Successfully sent a REFER"
                              " for token registration!\n", DEB_F_PREFIX_ARGS(SIP_MSG_SEND, fname));
    } else {
        CCSIP_DEBUG_ERROR(DEB_F_PREFIX"Error while trying to send"
                              " REFER for token registration!\n", DEB_F_PREFIX_ARGS(SIP_MSG_SEND, fname));
    }
    fallback_ccb = sip_regmgr_get_fallback_ccb_by_index(ccb->index);
    if (fallback_ccb) {
        sip_regmgr_retry_timer_start(fallback_ccb);
    }
}

/*
 ** sip_regmgr_check_and_transition
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION: Check and transition states
 *
 *  RETURNS: void
 *
 */
void
sip_regmgr_check_and_transition (ccsipCCB_t *ccb)
{
    static const char fname[] = "sip_regmgr_check_and_transition";

    /*
     * This routine gets called from IN_FALLBACK state when we
     * receive a response to a keepalive message -or- from the
     * STABILITY Check state when the Carefree algorithm successfully
     * completes.
     * If state is IN_FALLBACK and wan_failure is TRUE, transition
     * to STABILITY_CHECK state and start the CAREFREE Algorithm.
     * Else transition to the TOKEN_WAIT State.and send the refer
     * message requesting for a token to register.
     */
    if (!ccb) {
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Received event for invalid ccb\n", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
        return;
    }
    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Received event\n",
                          DEB_L_C_F_PREFIX_ARGS(SIP_EVT, ccb->index, ccb->dn_line, fname));
    if (wan_failure) {
        /*
         * Kick off the Carefree algorithm. Transition to STABILITY CHECK
         * state
         */
        sip_reg_sm_change_state(ccb, SIP_REG_STATE_STABILITY_CHECK);
        sip_regmgr_set_stability_total_msgs(
                sip_regmgr_get_fallback_ccb_by_index(ccb->index));
        sip_regmgr_wait_timer_start(
                sip_regmgr_get_fallback_ccb_by_index(ccb->index));
    } else {
        /*
         * Check to see if the ccm coming backup is going to
         * replace the current active, if not just replace the
         * current Standby
         */
        ti_config_table_t *ccm_table_entry, *fallback_ccb_entry;
        CCM_ID current_ccm_id, fallback_ccm_id;
        fallback_ccb_t *fallback_ccb;

        fallback_ccb = sip_regmgr_get_fallback_ccb_by_index(ccb->index);
        ccm_table_entry = CCM_Active_Standby_Table.active_ccm_entry;
        current_ccm_id = ccm_table_entry->ti_specific.ti_ccm.ccm_id;

        fallback_ccb_entry = (ti_config_table_t *) ccb->cc_cfg_table_entry;
        fallback_ccm_id = fallback_ccb_entry->ti_specific.ti_ccm.ccm_id;
        /*
         * Check to see if the current ccm coming back up is going to
         * replace the current active ccm
         */
        if (current_ccm_id > fallback_ccm_id) {
            if ((sip_platform_is_phone_idle()) &&
                (CCM_Fallback_Table.fallback_ccm_entry == NULL)) {
                /*
                 * Send a REFER message requesting for registering with
                 * the callmanager that has come back up. Cleanup any
                 * existing REFER transactions before creating a new
                 * REFER
                 */
                clean_method_request_trx(ccb, sipMethodRefer, TRUE);
                if (sipSPISendRefer(ccb, TOKEN_REFER_TO, SIP_REF_TOKEN)) {
                    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Successfully sent a REFER"
                                          " for token registration!\n", DEB_F_PREFIX_ARGS(SIP_MSG_SEND, fname));
                    sip_reg_sm_change_state(ccb, SIP_REG_STATE_TOKEN_WAIT);
                    if (fallback_ccb) {
                        sip_regmgr_retry_timer_start(fallback_ccb);
                    }
                } else {
                    CCSIP_DEBUG_ERROR(DEB_F_PREFIX"Error while trying to send"
                                          " REFER for token registration!\n",
                                          DEB_F_PREFIX_ARGS(SIP_MSG_SEND, fname));
                }
            } else {
                CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"phone not idle or fallback ccm entry non NULL \n",
                                      DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname));
                // Send keep alive if phone not idle
                sip_reg_sm_change_state(ccb, SIP_REG_STATE_IN_FALLBACK);
                if (fallback_ccb) {
                    sip_regmgr_retry_timer_start(fallback_ccb);
                }
                return;
            }
        } else if (current_ccm_id == fallback_ccm_id) {
            CCSIP_DEBUG_REG_STATE("%s: Current ccm coming back up is the current active ccm\n", fname);
            // Do nothing
        } else {
            /*
             * Falling back as the standby.
             * Any CCM coming back up will either replace the
             * active or standby ccm. In this case it is not the
             * active, so replace the standby ccm.
             */
            ccsipCCB_t *backup_ccb;
            ti_config_table_t *ccm_table_entry2;
            ti_ccm_t *ti_ccm;
            CCM_ID ccm_id;

            ccm_table_entry2 = (ti_config_table_t *)
                ccb->cc_cfg_table_entry;
            ti_ccm = &ccm_table_entry2->ti_specific.ti_ccm;
            ccm_id = ti_ccm->ccm_id;

            backup_ccb = sip_sm_get_ccb_by_index(REG_BACKUP_CCB);
            if (CCM_Active_Standby_Table.standby_ccm_entry) {
                int16_t trx_index;
                ti_config_table_t *standby_ccm_entry = NULL;
                CCM_ID standby_ccmid;

                standby_ccm_entry = CCM_Active_Standby_Table.standby_ccm_entry;
                standby_ccmid = standby_ccm_entry->ti_specific.ti_ccm.ccm_id;

                // Check to see if the ccm coming back up is going to
                // replace the current standby ccm.
                if (standby_ccmid < ccm_id) {
                    /*
                     * Clean the current fallback ccb and free it
                     */
                    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Freeing the fallback ccb"
                                          " for %d ccm as current standby"
                                          " is %d ccm!\n", DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname),
                                          ccm_id, standby_ccmid);
                    sip_regmgr_free_fallback_ccb(ccb);
                    return;
                }
                /*
                 * Check to see if there is a transaction pending.
                 * If so wait for the final response, else proceed.
                 */
                trx_index = get_method_request_trx_index(backup_ccb,
                                                         sipMethodRegister,
                                                         TRUE);
                if (trx_index >= 0) {
                    if (new_standby_available == NULL) {
                        new_standby_available = (void *) ccm_table_entry2;
                    } else {
                        /*
                         * There is already a ccm waiting to
                         * transition into being the standby ccm.
                         * Check current ccm id with the one
                         * waiting to see who is a better fit.
                         */
                        ti_config_table_t *waiting_standby;
                        CCM_ID this_ccm_id, waiting_ccm_id;

                        waiting_standby = (ti_config_table_t *)
                            new_standby_available;
                        waiting_ccm_id = waiting_standby->
                            ti_specific.ti_ccm.ccm_id;
                        this_ccm_id = ccm_table_entry2->
                            ti_specific.ti_ccm.ccm_id;
                        if (waiting_ccm_id > this_ccm_id) {
                            new_standby_available = (void *)
                                ccm_table_entry2;
                        }
                    }
                } else {

                    ti_config_table_t *standby_ccm_entry2;

                    standby_ccm_entry2 = (ti_config_table_t *)
                        backup_ccb->cc_cfg_table_entry;
                    // Update CCM Status
                    ui_set_ccm_conn_status(standby_ccm_entry2->ti_common.addr_str,
                                           CCM_STATUS_NONE);
                    if (standby_ccm_entry2->ti_common.conn_type == CONN_TCP) {
                        /*
                         * Clean up the socket of the current standby
                         * if tcp.
                         */
                        int connid;

                        connid = sip_tcp_fd_to_connid(standby_ccm_entry2->
                                                      ti_common.handle);
                        sip_tcp_purge_entry(connid);
                        sipTransportSetServerHandleAndPort(INVALID_SOCKET, 0,
                                (ti_config_table_t *)(backup_ccb->cc_cfg_table_entry));
                    }
                    ui_set_ccm_conn_status(ccm_table_entry2->ti_common.addr_str,
                                           CCM_STATUS_STANDBY);

                    sip_regmgr_setup_new_standby_ccb(ccm_id);
                    sip_regmgr_free_fallback_ccb(ccb);
                    /*
                     * New standby, start monitoring it.
                     */
                    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Start monitoring new"
                                          " standby cc \n", DEB_F_PREFIX_ARGS(SIP_STANDBY, fname));
                    (void) ccsip_register_send_msg(SIP_REG_CANCEL,
                                                   backup_ccb->index);
                }
            } else {
                /*
                 * Here there is no current standby, so simply
                 * make the current ccm the standby ccm and start
                 * monitoring it.
                 */

                ui_set_ccm_conn_status(ccm_table_entry2->ti_common.addr_str,
                                       CCM_STATUS_STANDBY);

                sip_regmgr_setup_new_standby_ccb(ccm_id);
                sip_regmgr_free_fallback_ccb(ccb);
                CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Start monitoring new"
                                      " standby cc \n", DEB_F_PREFIX_ARGS(SIP_STANDBY, fname));
                (void) ccsip_register_send_msg(SIP_REG_CANCEL,
                                               backup_ccb->index);
            }
        }
    }
}

/*
 ** sip_regmgr_ev_failure_response
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION:
 *
 *  RETURNS:
 *
 */
void
sip_regmgr_ev_failure_response (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    const char     *fname = "sip_regmgr_ev_failure_response";
    int             timeout;

    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Received event\n",
                          DEB_L_C_F_PREFIX_ARGS(SIP_EVT, ccb->index, ccb->dn_line, fname));
    if (ccb->index == REG_BACKUP_CCB) {
        /*
         * Keep monitoring.
         * Stay in unregistering state and wait for the
         * expires timer to pop to send the next keepalive.
         */

        timeout = sip_config_get_keepalive_expires();

        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Starting keep alive timer %d sec\n",
                              DEB_F_PREFIX_ARGS(SIP_TIMER, fname), timeout);
        (void) sip_platform_standby_keepalive_timer_start(timeout * 1000);

    } else {
        // Do not Initiate failover; Need not wait for the the ack_timer
        if (sip_regmgr_get_cc_mode(1) == REG_MODE_CCM) {
            config_update_required = TRUE;
        }

        /*
         * Send indication of REG_ALLFAIL so platform can
         * initiate a reboot.
        */
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Registration rejected.\n", DEB_F_PREFIX_ARGS(SIP_REG, fname));
         // Cleanup fallback CCB list
         sip_regmgr_free_fallback_ccb_list();
         sip_reg_all_failed = TRUE;
         sip_regmgr_handle_reg_all_fail();

    }
}

/*
 ** sip_regmgr_ev_cancel
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION:
 *
 *  RETURNS:
 *
 */
void
sip_regmgr_ev_cancel (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    const char *fname = "sip_regmgr_ev_cancel";
    char user[MAX_LINE_NAME_SIZE];

    /*
     * If state is TokenWait,
     * Then transition back into InFallback state.
     * Else send out a keepalive message to the registration server.
     * Note: Need to rename the event ???
     */
    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Received event\n",
                          DEB_L_C_F_PREFIX_ARGS(SIP_EVT, ccb->index, ccb->dn_line, fname));
    sip_util_get_new_call_id(ccb);
    ccb->authen.cred_type = 0;
    ccb->retx_counter     = 0;
    ccb->reg.tmr_expire   = 0;
    ccb->reg.act_time     = 0;
    config_get_line_string(CFGID_LINE_NAME, user, ccb->dn_line, sizeof(user));

    (void) sipSPISendRegister(ccb, 0, user, 0);
}

/*
 ** sip_regmgr_ccm_get_conn
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION:
 *
 *  RETURNS:
 *
 */
ti_config_table_t *
sip_regmgr_ccm_get_conn (line_t dn, ti_config_table_t *ccm_entry)
{
    /*
     * Just check for the validity of the cpr_socket
     * and return the entry.
     */
    if (ccm_entry->ti_common.handle != INVALID_SOCKET) {
        return (ccm_entry);
    } else {
        /*
         * Create a socket to the ccm
         */
        conn_create_status_t conn_status;

        conn_status = sip_transport_setup_cc_conn(dn, ccm_entry->
                                                  ti_specific.ti_ccm.ccm_id);
        if (conn_status == CONN_SUCCESS) {
            return (ccm_entry);
        } else {
            return (NULL);
        }
    }
}

/*
 ** sip_regmgr_setup_cc_conns
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: void
 *
 *  DESCRIPTION: Replaces SIPTaskConnectToSipProxies
 *
 *  RETURNS:
 *
 */
static int
sip_regmgr_setup_cc_conns ()
{
    const char *fname = "sip_regmgr_setup_cc_conns";
    RET_CODE ret_code = RET_SUCCESS;
    CCM_ID cc_index;
    cpr_socket_t active_fd = INVALID_SOCKET,
                 standby_fd = INVALID_SOCKET;
    line_t line;
    conn_create_status_t conn_status;

    /*
     * We assume only one type of call control in the phone
     * either ccm / csps. That is the reason for only one check
     * When we support both ccm and csps on the same phone (on
     * multiple lines we will need to check each line's cc_type
     * and take the appropriate action.
     */
    if (CC_Config_Table[0].cc_type == CC_CCM) {
        /*
         * Find the active and standby connections for
         * ccm.
         */
        for (cc_index = PRIMARY_CCM; cc_index < MAX_CCM; cc_index++) {
            line = 1;

            /*
             * Note: The opening of sockets will fail only in the
             * case of tcp/tls connections. In the case of udp,
             * the socket will get opened and the failure to reach
             * the destination will come in the form of timeouts/
             * icmp unreachable messages.
             */
            conn_status = sip_transport_setup_cc_conn(line, cc_index);
            if (conn_status == CONN_SUCCESS) {
                if (active_fd == INVALID_SOCKET) {
                    /*
                     * Found Active fd and active_fd is not set
                     * Save it for all lines.
                     */
                    active_fd = CCM_Config_Table[line - 1][cc_index]->ti_common.handle;
                    set_active_ccm(CCM_Config_Table[line - 1][cc_index]);
                } else {
                    /*
                     * Found Standby fd. Save it in the local table.
                     * and break out of loop.
                     */
                    standby_fd = CCM_Config_Table[line - 1]
                        [cc_index]->ti_common.handle;
                    CCM_Active_Standby_Table.standby_ccm_entry =
                        CCM_Config_Table[line - 1][cc_index];
                    break;
                }
            } else if (conn_status == CONN_FAILURE) {
                /*
                 * We can  get this return code when connecting to
                 * a server that uses UDP such as Asterisk. If that is the
                 * case we then change the connection type to UDP and
                 * attempt reconnection.
                 *
                 */

            	// change trans layer protocol to UDP
            	CC_Config_setIntValue(CFGID_TRANSPORT_LAYER_PROT, 2);
            	CCSIP_DEBUG_ERROR("%s: Attempting reconnection using UDP\n", fname);

            	// attempt re-connection
            	sipTransportInit();

            	conn_status = sip_transport_setup_cc_conn(line, cc_index);
                if (conn_status == CONN_SUCCESS) {
                    if (active_fd == INVALID_SOCKET) {
                        /*
                         * Found Active fd and active_fd is not set
                         * Save it for all lines.
                         */
                        active_fd = CCM_Config_Table[line - 1][cc_index]->ti_common.handle;
                        set_active_ccm(CCM_Config_Table[line - 1][cc_index]);
                    } else {
                        /*
                         * Found Standby fd. Save it in the local table.
                         * and break out of loop.
                         */
                        standby_fd = CCM_Config_Table[line - 1]
                            [cc_index]->ti_common.handle;
                        CCM_Active_Standby_Table.standby_ccm_entry =
                            CCM_Config_Table[line - 1][cc_index];
                        break;
                    }
                } else if (conn_status == CONN_FAILURE) {

                	/*
                	 * We get this return code when we have the address
                 	 * configured and we are not able to setup the
                 	 * connection to the ccm.
                 	 * Candidate for fallback monitoring. Note it in the
                 	 * ccm_config_table and add it to a fallback monitor
                 	 * table.
                 	 *
                	 */
                	CCSIP_DEBUG_ERROR("%s: Socket open failure: DN <%d> CCM <%d>\n",
                			fname, line, cc_index);
                	(void) sip_regmgr_create_fallback_ccb(cc_index, line);
                	ret_code = RET_START_FALLBACK;
                }
            }
        }
        if (active_fd == INVALID_SOCKET) {
            /*
             * No CC present for any calls from this phone.
             */
            CCSIP_DEBUG_ERROR("%s: NO CALL CONTROL AVAILABLE! Init a reboot!\n",
                              fname);
            set_active_ccm(&CCM_Dummy_Entry);

            /*
             * RegMgr Interface call, ALL_FAIL
             */
            ret_code = RET_INIT_REBOOT;
        } else if (standby_fd == INVALID_SOCKET) {
            /*
             * No Standby CC present for this phone.
             */
            CCSIP_DEBUG_ERROR("%s: NO VALID STANDBY CALL CONTROL AVAILABLE!\n",
                              fname);
            ret_code = RET_NO_STANDBY;
        }
    } else {
        /*
         * Non CCM Case
         */
        for (line = 1; line <= MAX_REG_LINES; line++) {
            conn_status = sip_transport_setup_cc_conn(line, UNUSED_PARAM);
        }
    }
    return (ret_code);
}

/*
 ** sip_regmgr_destroy_cc_conns
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION: Replaces SIPTaskDisconnectFromSipProxies
 *
 *  RETURNS:
 *
 */
int
sip_regmgr_destroy_cc_conns (void)
{
    const char *fname = "sip_regmgr_destroy_cc_conns";
    line_t dn, max_iteration;

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Destroying connections\n", DEB_F_PREFIX_ARGS(SIP_CC_CONN, fname));
    if (CC_Config_Table[LINE1].cc_type == CC_CCM) {
        /*
         * The first iteration of calling sip_transport_destroy_cc_conn
         * will close all the connections entries for pri, sec, tertiary
         * connections.
         */
        max_iteration = 1;
    } else {
        max_iteration = MAX_REG_LINES;
    }

    for (dn = 1; dn <= max_iteration; dn++) {
        /*
         * Not checking if it is the active ccm connection that is getting torn down.
         * Here we are killing everything so does'nt matter.
         * Something else has triggered the closing of the tcp conns.
         * So the unreg reason should be set by now.
         * In theory we should never see this getting set.
         */
        sip_transport_destroy_cc_conn(dn, PRIMARY_CCM);
    }
    return (0);
}

/*
 ** sip_regmgr_init
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION:
 *
 *  RETURNS:
 *
 */
int
sip_regmgr_init (void)
{
    RET_CODE ret_code = RET_SUCCESS;

    /*
     * Create the fallback ccb link list, that will hold
     * the fallback ccb's of the failed ccm's
     */
	fallback_ccb_list = sll_create(sip_regmgr_find_fallback_ccb);
    ret_code = (RET_CODE) sip_regmgr_setup_cc_conns();
    if (ret_code == RET_START_FALLBACK || ret_code == RET_NO_STANDBY) {
        sip_regmgr_trigger_fallback_monitor();
    } else if (ret_code == RET_INIT_REBOOT) {
        /*
         * Send indication of REG_ALLFAIL so platform can
         * initiate a reboot.
         */
        // Cleanup fallback CCB list
        sip_regmgr_free_fallback_ccb_list();
        sip_reg_all_failed = TRUE;
        sip_regmgr_handle_reg_all_fail();
        return (SIP_ERROR);
    }
    CCM_Fallback_Table.fallback_ccm_entry = NULL;
    CCM_Fallback_Table.is_idle = FALSE;
    CCM_Fallback_Table.is_resp = FALSE;
    CCM_Failover_Table.failover_started = FALSE;
    sip_reg_all_failed = FALSE;
    retry_times = 0;

    return (SIP_OK);
}


/*
 ** sip_regmgr_shutdown
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION: Unregister all lines and clean CCBs
 *
 *  RETURNS: none
 *
 */
void
sip_regmgr_shutdown (void)
{
    const char *fname = "sip_regmgr_shutown";
    fallback_ccb_t *fallback_ccb = NULL;
    line_t ndx = 0;
    ccsipCCB_t *ccb;

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"\n", DEB_F_PREFIX_ARGS(SIP_REG, fname));


    // Shutdown registration
    ccsip_register_shutdown();

    // Cleanup fallback CCB list
    while ((fallback_ccb = (fallback_ccb_t *)sll_next(fallback_ccb_list,
                    NULL)) != NULL) {
        sip_regmgr_clean_fallback_ccb(fallback_ccb);
        (void) sll_remove(fallback_ccb_list, fallback_ccb);
        cpr_free(fallback_ccb);
    }
    sll_destroy(fallback_ccb_list);
    fallback_ccb_list = NULL;

    for (ndx = REG_CCB_START; ndx <= REG_BACKUP_CCB; ndx++) {
        ccb = sip_sm_get_ccb_by_index(ndx);
        if (ccb != NULL) {
            ccb->sipCallID[0] = '\0';
        }
    }

    retry_times = 0;
    set_active_ccm(NULL);
    CCM_Active_Standby_Table.standby_ccm_entry = NULL;
}

/*
 ** sip_regmgr_failover_rsp_start
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: none
 *
 *  DESCRIPTION:
 *
 *  RETURNS: none
 *
 */
void
sip_regmgr_failover_rsp_start (void)
{
    const char *fname = "sip_regmgr_failover_rsp_start";

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"\n", DEB_F_PREFIX_ARGS(SIP_REG, fname));
    /*
     * Received response start
     * Change the ip addr and port to point to the new ccm
     * Set the CC_CONFIG_TABLE entry for all the lines to
     * point to the new ccm.
     * Set the ccb->cc_cfg_table_entry with the new entry.
     */
    sip_regmgr_setup_new_active_ccb(CCM_Failover_Table.failover_ccm_entry);
    /*
     * Monitoring of standby node will start when we get a 200 ok for the new
     * active cucm, or, in case new active fails, then the stanby will be moved
     * up to become new active cucm, and will come to this function again.
     */
    if (ccsip_register_get_register_state() == SIP_REG_NO_STANDBY) {
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Unable to get new standby ccm !\n", DEB_F_PREFIX_ARGS(SIP_STANDBY, fname));
    }

    sip_regmgr_register_lines(TRUE, FALSE);
    // start notify timer
    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"START TIMER \n", DEB_F_PREFIX_ARGS(SIP_TIMER, fname));
    (void) sip_platform_notify_timer_start(5000);
    CCM_Failover_Table.prime_registered = TRUE;
}

/*
 ** sip_regmgr_failover_rsp_complete
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION:
 *
 *  RETURNS: none
 *
 */
void
sip_regmgr_failover_rsp_complete (void)
{
    const char *fname = "sip_regmgr_failover_complete";

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"\n", DEB_F_PREFIX_ARGS(SIP_FAILOVER, fname));
    /*
     * Received response complete
     */
    // stop notify timer
    (void) sip_platform_notify_timer_stop();
    CCM_Failover_Table.failover_started = FALSE;
    sip_platform_cc_mode_notify();
    sip_regmgr_register_lines(FALSE, FALSE);
    sip_regmgr_update_call_ccb();
    sip_platform_set_ccm_status();
    sip_regmgr_trigger_fallback_monitor();
    CCM_Failover_Table.prime_registered = FALSE;
}

void
sip_regmgr_register_lines (boolean prime_only, boolean skip_prime)
{
    static const char fname[] = "sip_regmgr_register_lines";
    ccsipCCB_t *ccb = 0;
    line_t ndx;
    line_t line_end;
    line_t line_start;
    char address[MAX_IPADDR_STR_LEN];
    ti_config_table_t *standby_ccm;

    line_end = 1;
    if ((!CCM_Failover_Table.prime_registered && !skip_prime) || prime_only) {
        line_start = REG_CCB_START;
    } else {
        line_start = REG_CCB_START + 1;
    }
    if (prime_only) {
        line_end = REG_CCB_START;
    } else {
        line_end += TEL_CCB_END;
        ccb = sip_sm_get_ccb_by_index(REG_CCB_START);
        if (ccb->reg.registered ) {
            ui_set_sip_registration_state(ccb->dn_line, TRUE);
        }
    }

    if (line_start == REG_CCB_START) {
        ccsip_register_set_register_state(SIP_REG_REGISTERING);
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"registering prime line \n", DEB_F_PREFIX_ARGS(SIP_REG, fname));
        /*
         * regmgr - Set the Backup (standby ccm) info.
         */
        standby_ccm = CCM_Active_Standby_Table.standby_ccm_entry;
        if (standby_ccm) {
            ti_ccm_t *ti_ccm = &standby_ccm->ti_specific.ti_ccm;

            sip_regmgr_setup_new_standby_ccb(ti_ccm->ccm_id);
        } else {
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"ERROR: Standby ccm entry is NULL\n",
                                  DEB_F_PREFIX_ARGS(SIP_STANDBY, fname));
        }
    }

    for (ndx = line_start; ndx <= line_end; ndx++) {
        if (sip_config_check_line((line_t) (ndx - TEL_CCB_END))) {
            ccb = sip_sm_get_ccb_by_index(ndx);
            if (ccb) {
                CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"%d, %p",
                                      DEB_L_C_F_PREFIX_ARGS(SIP_REG, ccb->index, ccb->dn_line, fname),
                                      ndx, ccb);

                ui_set_sip_registration_state(ccb->dn_line, FALSE);

                sip_sm_call_cleanup(ccb);
                sipTransportGetPrimServerAddress(ccb->dn_line, address);

                sstrncpy(ccb->reg.proxy, address, MAX_IPADDR_STR_LEN);

                ccb->reg.addr = ccb->dest_sip_addr;
                ccb->reg.port = (uint16_t) ccb->dest_sip_port;

                if (ccb->index == REG_CCB_START) {
                   ui_update_registration_state_all_lines(FALSE);
                   ccb->send_reason_header = TRUE;
                } else {
                    ccb->send_reason_header = FALSE;
                }

                if (ccsip_register_send_msg(SIP_REG_REQ, ndx) != SIP_REG_OK) {
                    ccsip_register_cleanup(ccb, TRUE);
                }
            }
        }
    }
    sip_platform_set_ccm_status();
}

/*
 ** sip_regmgr_phone_idle
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION:
 *
 *  RETURNS:
 *
 */
void
sip_regmgr_phone_idle (boolean waited)
{
    const char *fname = "sip_regmgr_phone_idle";
    ccsipCCB_t *ccb;

    /*
     * Received response
     */
    CCM_Fallback_Table.is_idle = TRUE;
    // if waited = TRUE send refer and change to token wait
    // and send ui unlock
    // otherwise start fallback
    if (waited) {
        platform_reg_fallback_cfm();
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX" waited TRUE\n", DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname));
        CCM_Fallback_Table.fallback_ccm_entry = NULL;
        sip_regmgr_send_refer(CCM_Fallback_Table.ccb);

    } else {
        /*
         * Cancel reg from current active ccm
         */
        ccsip_register_cancel(TRUE, FALSE);

        ccb = CCM_Fallback_Table.ccb;
        if (ccsip_register_send_msg(SIP_REG_CLEANUP, ccb->index) != SIP_REG_OK) {
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"failed to send SIP_REG_CLEANUP\n", DEB_F_PREFIX_ARGS(SIP_REG, fname));
        }
        (void) sip_platform_notify_timer_start(5000);
    }
}

/*
 ** sip_regmgr_fallback_rsp
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION:
 *
 *  RETURNS: none
 *
 */
void
sip_regmgr_fallback_rsp (void)
{
    const char *fname = "sip_regmgr_fallback_rsp";

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Entered\n", DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname));
    /*
     * Received response
     */
    (void) sip_platform_notify_timer_stop();
    CCM_Fallback_Table.is_resp = TRUE;
    sip_platform_cc_mode_notify();
    if (CCM_Fallback_Table.fallback_ccm_entry) {
        sip_regmgr_register_lines(FALSE, FALSE);
        CCM_Fallback_Table.fallback_ccm_entry = NULL;
    }
    sip_regmgr_update_call_ccb();
    CCM_Failover_Table.prime_registered = FALSE;
}

void
sip_regmgr_rsp (int rsp_id, int rsp_type, boolean waited)
{

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"rsp id=%s rsp type=%s waited=%d\n",
        DEB_F_PREFIX_ARGS(SIP_RESP, "sip_regmgr_rsp"),
        rsp_id == FAILOVER_RSP ? "FAILOVER_RSP" : "FALLBACK_RSP",
        rsp_type == RSP_START ? "RSP_START" : "RSP_COMPLETE", waited);

    if (rsp_type == RSP_START) {
        if (rsp_id == FAILOVER_RSP) {
            sip_regmgr_failover_rsp_start();
            // Inform the Sub/Not manager that platform is about to failover.
            (void) sip_subsManager_rollover();
            publish_reset();
        } else {
            sip_regmgr_phone_idle(waited);
            /*
             * Inform the Sub/Not manager that platform is about to fallback.
             * so that applications can clean up before re-register with the
             * primary CCM.
             */
            (void) sip_subsManager_rollover();
            publish_reset();
        }
    } else if (rsp_type == RSP_COMPLETE) {

        SIPTaskReinitialize(TRUE);

        if (rsp_id == FAILOVER_RSP) {
            sip_regmgr_failover_rsp_complete();
        } else {
            sip_regmgr_fallback_rsp();
        }
    }
}

/*
 *  Get the config address based on call manager id
 *
 *  @param ccm_id : callmanager idex
 *         addr_str: address of allocated space to return
 *                   the address in string format.
 *
 *  @return  none
 *
 *  @pre     (addr_str != NULL) (1< ccm_id < 4)
 *
 */

void sip_regmgr_get_config_addr (int ccm_id, char *addr_str) {

#ifdef IPV6_STACK_ENABLED
    int     ip_mode = CPR_IP_MODE_IPV4;



    config_get_value(CFGID_IP_ADDR_MODE,
                         &ip_mode, sizeof(ip_mode));
    if (ip_mode == CPR_IP_MODE_IPV4) {
#endif
        config_get_value(ccm_config_id_addr_str[ccm_id], addr_str,
                        MAX_IPADDR_STR_LEN);
#ifdef IPV6_STACK_ENABLED
    } else if (ip_mode == CPR_IP_MODE_IPV6) {

        config_get_value(ccm_config_id_ipv6_addr_str[ccm_id], addr_str,
                             MAX_IPADDR_STR_LEN);
    } else if (ip_mode == CPR_IP_MODE_DUAL) {

        config_get_value(ccm_config_id_ipv6_addr_str[ccm_id], addr_str,
                             MAX_IPADDR_STR_LEN);
        /* Both mode get IPv6 first if not available get ipv4 */
        if (addr_str[0] == NUL) {
            config_get_value(ccm_config_id_addr_str[ccm_id], addr_str,
                             MAX_IPADDR_STR_LEN);
        }
    }
#endif
}

/*
 ** sip_regmgr_check_config_change
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:None
 *
 *  DESCRIPTION: Check if there is a config change in transport protocol
 *  or CCM address
 *
 *  RETURNS: TRUE if config changed
 *
 */
boolean
sip_regmgr_check_config_change (void)
{
    const char *fname = "sip_regmgr_check_config_change";
    ti_common_t ti_common;
    CCM_ID ccm_id;
    ti_common_t *active_ti_common;
#ifdef IPV6_STACK_ENABLED
    int        ip_mode = CPR_IP_MODE_IPV4;

    config_get_value(CFGID_IP_ADDR_MODE,
                     &ip_mode, sizeof(ip_mode));
#endif

    {
        uint32_t  transport_prot;
        CONN_TYPE configured_conn = CONN_NONE;

        config_get_value(CFGID_TRANSPORT_LAYER_PROT, &transport_prot,
                         sizeof(transport_prot));
        configured_conn = CCM_Device_Specific_Config_Table[PRIMARY_CCM].ti_common.configured_conn_type;
        if (transport_prot != (uint32_t) configured_conn) {
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"protocol%d conn type %d\n", DEB_F_PREFIX_ARGS(SIP_CONFIG, fname),
                                  transport_prot, configured_conn);
            return (TRUE);
        }
    }
    for (ccm_id = PRIMARY_CCM; ccm_id < MAX_CCM; ccm_id++) {

        sip_regmgr_get_config_addr(ccm_id, ti_common.addr_str);

        active_ti_common = &CCM_Device_Specific_Config_Table[ccm_id].ti_common;
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"CCM config%s active %s\n", DEB_F_PREFIX_ARGS(SIP_CONFIG, fname),
                              ti_common.addr_str, active_ti_common->addr_str);
        if (strncmp(ti_common.addr_str, active_ti_common->addr_str,
                    strlen(active_ti_common->addr_str)) != 0) {
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"CCM changed\n", DEB_F_PREFIX_ARGS(SIP_CONFIG, fname));
            return (TRUE);
        }

    }
    return (FALSE);
}

/*
 ** sip_regmgr_process_config_change
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:None
 *
 *  DESCRIPTION: Shut down and re-init sip
 *
 *  RETURNS: None
 *
 */
void
sip_regmgr_process_config_change (void)
{

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"!!!Process_config_change\n",
                          DEB_F_PREFIX_ARGS(SIP_CONFIG, "sip_regmgr_process_config_change"));

    /* Register manager configuration changes need to restart sip task */
    sip_shutdown_phase1(FALSE, UNREG_NO_REASON);
}

/*
 ** sip_regmgr_clean_standby_ccb
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: None
 *
 *  DESCRIPTION: Clean standby CCB
 *
 *  RETURNS: None
 *
 */
void
sip_regmgr_clean_standby_ccb (ccsipCCB_t *ccb)
{

    if (ccb) {
        ccb->reg.proxy[0] = '\0';
        (void) sip_platform_register_expires_timer_stop(ccb->index);
        sip_stop_ack_timer(ccb);
        ccb->reg.port = 0;
    }
}

/*
 ** sip_regmgr_send_refer
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:ccb
 *
 *  DESCRIPTION:Send a REFER message requesting for registering with the CCM
 *
 *  RETURNS: None
 *
 */
void
sip_regmgr_send_refer (ccsipCCB_t *ccb)
{
    static const char fname[] = "sip_regmgr_send_refer";

    /*
     * Send a REFER message requesting for registering with
     * the callmanager. Cleanup any
     * pending REFER transaction as new REFER is created for
     * next transmission.
     */
    clean_method_request_trx(ccb, sipMethodRefer, TRUE);
    if (sipSPISendRefer(ccb, TOKEN_REFER_TO, SIP_REF_TOKEN)) {
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Successfully sent a REFER"
                              " for token registration!\n", DEB_F_PREFIX_ARGS(SIP_MSG_SEND, fname));
        sip_reg_sm_change_state(ccb, SIP_REG_STATE_TOKEN_WAIT);
    } else {
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Error while trying to send"
                              " REFER for token registration!\n", DEB_F_PREFIX_ARGS(SIP_MSG_SEND, fname));
    }
}

/*
 ** sip_regmgr_update_call_ccb
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:ccb
 *
 *  DESCRIPTION:Update call ccb after failover/fallback
 *
 *  RETURNS: None
 *
 */
static void
sip_regmgr_update_call_ccb (void)
{
    line_t i;
    ccsipCCB_t *ccb = NULL;

    for (i = TEL_CCB_START; i <= TEL_CCB_END; i++) {
        ccb = sip_sm_get_ccb_by_index(i);
        if (ccb) {
            ccb->local_port = sipTransportGetListenPort(ccb->dn_line, NULL);
            sipTransportGetServerIPAddr(&(ccb->dest_sip_addr), ccb->dn_line);
            ccb->dest_sip_port = sipTransportGetPrimServerPort(ccb->dn_line);
        }
    }
}

/*
 *  Function: sip_regmgr_ccm_restarted
 *
 *  Parameters:
 *      new_reg_ccb  - pointer to ccsipCCB_t of REG CCB that has seen
 *                     by CCM as a new REG.
 *
 *  Description:
 *      The function handles CCM has restarted condition.
 *  The phone needs to clean up internal state that may be left
 *  before CCM has gone down and come right up. This condition can
 *  be seen when using UDP for transport and the CCM has gone down
 *  between REG refresh cycles.
 *
 *  Returns:
 *      None
 */
void
sip_regmgr_ccm_restarted (ccsipCCB_t *new_reg_ccb)
{
    const char *fname = "sip_regmgr_ccm_restarted";
    ccsipCCB_t *ccb;
    line_t ndx, line_end;

    if ((new_reg_ccb == NULL) || (new_reg_ccb->index == REG_BACKUP_CCB)) {
        /* No ccb or it is back up CCB, ignore this one */
        return;
    }

    /* Inform all applications that hat may subscribe to the CCM/Proxy */
    (void) sip_subsManager_reset_reg();
    /*
     * If there are more than one line registered to this CCM/Proxy,
     * start re-register the rest of the lines quickly.
     */
    line_end = 1;
    /*
     * REG CCBs start after the TEL CCBs.
     * So, line_end equals the number of lines and then add the TEL_CCB_END
     * to get the ending of the REG CCBs
     */
    line_end += TEL_CCB_END;

    for (ndx = REG_CCB_START; ndx <= line_end; ndx++) {
        ccb = sip_sm_get_ccb_by_index(ndx);
        if (!sip_config_check_line((line_t)(ndx - TEL_CCB_END)) ||
            !ccb || (ccb == new_reg_ccb) ||
            (ccb->state != (int) SIP_REG_STATE_REGISTERED) ||
            (util_compare_ip(&(ccb->reg.addr), &(new_reg_ccb->reg.addr)) == FALSE)) {
            /*
             * Skip the CCB for the line that
             * 1) is not configured or
             * 2) is the same as the one that detects restarts or
             * 3) is not registered or
             * 4) the proxy/CCM address is not the same one as the
             *    one that is seeing new registration indication.
             *    (do not restart other that is not register to the
             *    same server).
             */
            continue;
        }

        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Re-register %d\n", DEB_F_PREFIX_ARGS(SIP_REG, fname), ccb->dn_line);
        /*
         * This line still registers before the CCM went down and came back
         * up. Do minimum resetting to the ccb just enough for it to
         * send REG out to the CCM.
         */

        /* Set state back to idle */
        sip_reg_sm_change_state(ccb, SIP_REG_STATE_IDLE);

        /* Clear Registered flag */
        ccb->reg.registered = 0;

        /* Restart the register timer */
        (void) sip_platform_register_expires_timer_start(ccb->reg.tmr_expire *
                                                         1000, ccb->index);

        /* Update UI to indicates that this line is not register */
        ui_set_sip_registration_state(ccb->dn_line, FALSE);

        /* Send REG out to the CCM */
        if (ccsip_register_send_msg(SIP_REG_REQ, ndx) != SIP_REG_OK) {
            ccsip_register_cleanup(ccb, TRUE);
        }
    }
}

/*
 ** sip_regmgr_notify_timer_callback
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS: None
 *
 *  DESCRIPTION: Missed notify from CCM during failover/fallback
 *
 *  RETURNS: None
 *
 */
void
sip_regmgr_notify_timer_callback (void *data)
{
    const char *fname = "sip_regmgr_notify_timer_callback";
    ccsipCCB_t *ccb;
    sipServiceControl_t *scp = NULL;
    const char *versionStamp = "0";
    int versionStampLen;

    ccb = sip_sm_get_ccb_by_index(REG_CCB_START);
    if (ccb->reg.registered) {
        // Prime line in registered state. Missed notify from ccm.
        // fake a notify message to unlock UI, Use value 0 so that
        // platform will download the config
        scp = (sipServiceControl_t *)
            cpr_calloc(1, sizeof(sipServiceControl_t));
        if (scp) {
            versionStampLen = strlen(versionStamp);
            scp->action = SERVICE_CONTROL_ACTION_CHECK_VERSION;
            scp->configVersionStamp = (char *)
                cpr_calloc(1, versionStampLen + 1);
            scp->dialplanVersionStamp = (char *)
                cpr_calloc(1, versionStampLen + 1);
            scp->softkeyVersionStamp = (char *)
                cpr_calloc(1, versionStampLen + 1);

            if (!scp->configVersionStamp ||
                !scp->dialplanVersionStamp ||
                !scp->softkeyVersionStamp) {
                CCSIP_DEBUG_ERROR("%s: malloc failed\n", fname);
            } else {
                sstrncpy(scp->configVersionStamp, versionStamp, versionStampLen + 1);
                sstrncpy(scp->dialplanVersionStamp, versionStamp, versionStampLen + 1);
                sstrncpy(scp->softkeyVersionStamp, versionStamp, versionStampLen + 1);
                sip_platform_handle_service_control_notify(scp);
                CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Fake NOTIFY TO Platform\n",
                                      DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname));
            }
            sippmh_free_service_control_info(scp);
        }
    } else {
        // We should not get here. If so then some problem
        // Add debug message for the time being
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"PRIME LINE UNREGISTRED STATE, UI LOCK!!!\n",
                              DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname));
    }
}


/*
 ** sip_regmgr_replace_standby
 *
 *  PARAMETERS: ccb
 *
 *  DESCRIPTION: Replace current standby with new one
 *
 *  RETURNS: None
 *
 */
void
sip_regmgr_replace_standby (ccsipCCB_t *ccb)
{
    const char *fname = "sip_regmgr_replace_standby";
    ti_config_table_t *cfg_table_entry;
    ti_common_t *ti_common;
    ccsipCCB_t *ccb_of_fallback;
    ti_config_table_t *standby_ccm_entry;

    if (!new_standby_available) {
        return;
    }
    /*
     * Will get in here if a fallback of standby
     * occurs when the current standby has an
     * outstanding transaction in process. We wait
     * till the transaction is complete.
     */

    // Update CCM Status
    standby_ccm_entry = (ti_config_table_t *) ccb->cc_cfg_table_entry;

    ui_set_ccm_conn_status(standby_ccm_entry->ti_common.addr_str,
                           CCM_STATUS_NONE);

    cfg_table_entry = (ti_config_table_t *) new_standby_available;
    ti_common = &cfg_table_entry->ti_common;
    sip_regmgr_setup_new_standby_ccb(cfg_table_entry->ti_specific.ti_ccm.ccm_id);
    if (sip_regmgr_find_fallback_ccb_by_addr_port(&(ti_common->addr),
                ti_common->port, &ccb_of_fallback)) {
        sip_regmgr_free_fallback_ccb(ccb_of_fallback);
    } else {
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Unable to find fallback"
                              " ccb to free\n", DEB_F_PREFIX_ARGS(SIP_FALLBACK, fname));
    }
    /*
     * New standby, start monitoring it.
     */

    ui_set_ccm_conn_status(cfg_table_entry->ti_common.addr_str,
                           CCM_STATUS_STANDBY);

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Start monitoring new standby ccb\n", DEB_F_PREFIX_ARGS(SIP_STANDBY, fname));
    (void) ccsip_register_send_msg(SIP_REG_CANCEL, ccb->index);
    new_standby_available = NULL;
}
/**
 ** sip_regmgr_regallfail_timer_callback
 *  Callback function called when the reg-all-fail timer expires
 *
 *  @param  void*  data
 *
 *  @return void
 *
 */
void
sip_regmgr_regallfail_timer_callback (void *data)
{
    const char *fname = "sip_regmgr_regallfail_timer_callback";
    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Registration Failed. Restarting the System now!\n",
                          DEB_F_PREFIX_ARGS(SIP_REG, fname));
    sip_regmgr_send_status(REG_SRC_SIP, REG_ALL_FAIL);
}

/**
 ** sip_regmgr_handle_reg_all_fail
 *  Handles the scenario when all the Registration attempts fail.
 *
 *  @param  none
 *
 *  @return void
 *
 */
void
sip_regmgr_handle_reg_all_fail (void)
{
    const char *fname = "sip_regmgr_handle_reg_all_fail";
    line_t     line_end, ndx;
    CCM_ID     ccm_index;
    ccsipCCB_t *ccb;
    unsigned long msec;
    unsigned long high_regfailtime = 180000;
    unsigned long low_regfailtime = 120000;
    char          tmp_str[STATUS_LINE_MAX_LEN];

  CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"All registration attempts failed.\n", DEB_F_PREFIX_ARGS(SIP_REG, fname));

    /* Start the timer and update UI only if no calls are in progress on the
     * phone.*/
    if (sip_platform_is_phone_idle()) {
        for (ccm_index = PRIMARY_CCM; ccm_index < MAX_CCM; ccm_index++) {
            if (0 != strcmp (CCM_Config_Table[0][ccm_index]->ti_common.addr_str,
                              ""))  {
                ui_set_ccm_conn_status(CCM_Config_Table[0][ccm_index]->
                                       ti_common.addr_str, CCM_STATUS_NONE);
            }
        }
        line_end = 1;
        line_end += TEL_CCB_END;
        for (ndx = REG_CCB_START; ndx <= line_end; ndx++){
            if (sip_config_check_line((line_t) (ndx - TEL_CCB_END))) {
                ccb = sip_sm_get_ccb_by_index(ndx);
                if (!ccb) {
                    continue;
                }
                ui_set_sip_registration_state(ccb->dn_line, FALSE);
            }
        }

        /*
         * Start regallfial with 100 msec timer once,
         * After that get a Random time between high (3 mins)
         * and low (2 mins) and start the timer with that time.
         */
        if (!regall_fail_attempt) {
           	msec = REGALL_FAIL_TIME;
        } else {
           	msec = cpr_rand()%(high_regfailtime - low_regfailtime);
           	msec += low_regfailtime;
        }

        regall_fail_attempt = TRUE;

        sip_platform_reg_all_fail_timer_start(msec);
        if (platGetPhraseText(STR_INDEX_REGISTERING,
                              (char *)tmp_str,
                              STATUS_LINE_MAX_LEN - 1) == CPR_SUCCESS) {
           	ui_set_notification(CC_NO_LINE, CC_NO_CALL_ID, tmp_str, msec/1000, TRUE, DEF_NOTIFY_PRI);
        }
    } else {
        /* Send the Indication to the platform immediately */
        sip_regmgr_send_status(REG_SRC_SIP, REG_ALL_FAIL);
    }
}

/*
 ** notify_register_update
 *
 *  PARAMETERS: last_available_line - last available line button on the phone
 *
 *  DESCRIPTION: This function is used to notify the sip task to handle
 *               any registration updates when sidecars are plugged in or
 *               removed
 *
 *  RETURNS: None
 *
 */
void notify_register_update(int last_line_available)
{
    static const char fname[] = "notify_register_update";

    if (ccsip_register_send_msg(SIP_REG_UPDATE, (line_t)last_line_available) != SIP_REG_OK) {
        CCSIP_DEBUG_ERROR("%s : Unable to send register update message\n", fname);
    } else {
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"last_available_line: %d\n",
                              DEB_F_PREFIX_ARGS(SIP_REG, fname), last_line_available);
    }
}

/*
 ** update_ui_line_reg_state
 *
 *  PARAMETERS:
 *       start_line - first line whose reg status need to be set
 *       end_line - last line whose reg status needs to be set
 *       registered - registered/unregistered
 *
 *  DESCRIPTION: This function is used to set the ui line reg status
 *
 *  RETURNS: None
 *
 */
void update_ui_line_reg_state(int start_line, int end_line, boolean registered)
{
    line_t      line_index = 0;
    ccsipCCB_t *line_ccb = NULL;

    for (line_index = (TEL_CCB_END + (line_t)start_line + 1);
         line_index <= (TEL_CCB_END + end_line);
         line_index++) {
         line_ccb = sip_sm_get_ccb_by_index(line_index);
         if (line_ccb) {
             if (sip_config_check_line(line_ccb->dn_line)) {
                 ui_set_sip_registration_state(line_ccb->dn_line, registered);
             }
          }
    }
}


/*
 ** regmgr_handle_register_update
 *
 *  PARAMETERS: last_available_line - last available line button on the phone
 *
 *  DESCRIPTION: This function is used to handle any registration updates needed
 *               when a sidecar is plugged in or unplugged
 *
 *  RETURNS: None
 *
 */
void regmgr_handle_register_update(line_t last_available_line)
{
    static const char fname[] = "regmgr_handle_register_update";
    ccsipCCB_t *line_ccb;
    line_t      line_index = 0;
    char address[MAX_IPADDR_STR_LEN];
    int         last_line_button_present;
    //boolean     reg_update_needed = TRUE;

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"last_available_line: %d\n",
                          DEB_F_PREFIX_ARGS(SIP_REG, fname), last_available_line);

    if (last_available_line == 1) {
        return;
    }

    last_line_button_present = 1;

	/* added by cangchen fix CSCsz91640*/
    if (last_line_button_present > last_available_line) {
			for (line_index = (TEL_CCB_END + (line_t)last_available_line + 1);
                 line_index <= (TEL_CCB_END + 1) &&
                 line_index <= (TEL_CCB_END + (line_t)(last_line_button_present));
                 line_index++) {
                 line_ccb = sip_sm_get_ccb_by_index(line_index);
                 if (line_ccb) {
                    if (sip_config_check_line(line_ccb->dn_line)) {
                        CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"%d: %p",
                                              DEB_L_C_F_PREFIX_ARGS(SIP_CONFIG, line_ccb->index, line_ccb->dn_line, fname),
                                              line_index, line_ccb);
                   }
                }
            }
	}

        if (last_available_line > last_line_button_present) {
            //sidecar added
            for (line_index = (TEL_CCB_END + (line_t)last_line_button_present + 1);
                 line_index <= (TEL_CCB_END + 1) &&
                 line_index <= (TEL_CCB_END + (line_t)(last_available_line));
                 line_index++) {
                 //sidecar added.register all the lines that have been newly added
                 line_ccb = sip_sm_get_ccb_by_index(line_index);
                 if (line_ccb) {
                    if (sip_config_check_line(line_ccb->dn_line)) {
                        CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"%d: %p",
                                              DEB_L_C_F_PREFIX_ARGS(SIP_CONFIG, line_ccb->index, line_ccb->dn_line, fname),
                                              line_index, line_ccb);

                        ui_set_sip_registration_state(line_ccb->dn_line, FALSE);

                        sip_sm_call_cleanup(line_ccb);
                        sipTransportGetPrimServerAddress(line_ccb->dn_line, address);

                        sstrncpy(line_ccb->reg.proxy, address, MAX_IPADDR_STR_LEN);

                        line_ccb->reg.addr = line_ccb->dest_sip_addr;
                        line_ccb->reg.port = (uint16_t) line_ccb->dest_sip_port;

                        if (ccsip_register_send_msg(SIP_REG_REQ, line_index) != SIP_REG_OK) {
                            ccsip_register_cleanup(line_ccb, TRUE);
                        }
                   }
                }
            }
        } else if (last_line_button_present > last_available_line) {
            for (line_index = (TEL_CCB_END + (line_t)last_available_line + 1);
                 line_index <= (TEL_CCB_END + 1) &&
                 line_index <= (TEL_CCB_END + (line_t)(last_line_button_present));
                 line_index++) {
                 //sidecar removed.unregister al the lines that were
                 //previously registered
                 line_ccb = sip_sm_get_ccb_by_index(line_index);
                 if (line_ccb) {
                        if (sip_config_check_line(line_ccb->dn_line)) {
                        ui_set_sip_registration_state(line_ccb->dn_line, FALSE);

                        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"cancelling timers, line= %d\n",
                                  DEB_F_PREFIX_ARGS(SIP_TIMER, fname), line_ccb->index);
                        (void) sip_platform_register_expires_timer_stop(line_ccb->index);
                        sip_stop_ack_timer(line_ccb);

                        if (ccsip_register_send_msg(SIP_REG_CANCEL, line_index) != SIP_REG_OK) {
                            ccsip_register_cleanup(line_ccb, TRUE);
                        }
                   }
                }
            }
        }


}









