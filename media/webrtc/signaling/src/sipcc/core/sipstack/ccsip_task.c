/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "cpr_string.h"
#include "cpr_socket.h"
#include "cpr_timers.h"
#include "cpr_memory.h"
#include "cpr_in.h"
#include "cpr_ipc.h"
#include "phntask.h"
#include "util_string.h"
#include "task.h"
#include "phone.h"
#include "text_strings.h"
#include "ccsip_task.h"
#include "ccsip_core.h"
#include "ccsip_macros.h"
#include "ccsip_messaging.h"
#include "ccsip_sim.h"
#include "ccsip_platform_udp.h"
#include "ccsip_platform.h"
#include "phone_debug.h"
#include "ccsip_register.h"
#include "debug.h"
#include "ccsip_reldev.h"
#include "ccsip_cc.h"
#include "gsm.h"
#include "fim.h"
#include "ccapi.h"
#include "lsm.h"
#include "config.h"
#include "check_sync.h"
#include "sip_common_transport.h"
#include "uiapi.h"
#include "sip_csps_transport.h"
#include "sip_common_regmgr.h"
#include "sip_platform_task.h"
#include "platform_api.h"
#include "sip_interface_regmgr.h"
#include "ccsip_publish.h"
#include "platform_api.h"
#include "thread_monitor.h"

#ifdef SAPP_SAPP_GSM
#define SAPP_APP_GSM 3
#define SAPP_APP SAPP_APP_GSM
#include "sapp.h"
#endif

#if defined SIP_OS_WINDOWS
#include "../win32/cpr_win_defines.h"
#endif



extern sipSCB_t *find_scb_by_callid(const char *callID, int *scb_index);
extern int platThreadInit(char *);
void sip_platform_handle_service_control_notify(sipServiceControl_t *scp);
short SIPTaskProcessTimerExpiration(void *msg, uint32_t *cmd);
extern cprMsgQueue_t sip_msgq;
extern cprMsgQueue_t gsm_msgq;
extern void ccsip_dump_recv_msg_info(sipMessage_t *pSIPMessage,
                               cpr_ip_addr_t *cc_remote_ipaddr,
                               uint16_t cc_remote_port);
void destroy_sip_thread(void);

// Global variables


/*---------------------------------------------------------
 *
 * Definitions
 *
 */

#define MESSAGE_WAITING_STR_SIZE 5

#define MAC_ADDR_STR_LENGTH         16
#define SIP_HEADER_SERVER_LEN       80
#define SIP_HEADER_USER_AGENT_LEN   80
#define SIP_PHONE_MODEL_NUMBER_LEN  32

/* The maximun number of gsm_msgq depth to allow new incoming call,
 */
/*
 * for RT, the threshold of 60 is not based on testing.
 * As discussed in CSCsz33584 Code Review, it is better to drop the heavy load as it enters the system than let it go deep into the system.
 * So we add the same logic into RT, but RT still cannot pass the test in CSCsz33584.
 * Bottleneck of RT might be ccapp_msgq instead of gsm_msgq.
 * A more complex solution should be studied to prevent RT phone from overload.
 */
#define MAX_DEPTH_OF_GSM_MSGQ_TO_ALLOW_NEW_INCOMING_CALL	60

/* Internal request structure for restart/re-init request from platform */
typedef enum {
    SIP_RESTART_REQ_NONE,
    SIP_RESTART_REQ_RESTART,   /* to restart */
    SIP_RESTART_REQ_REINIT     /* to re-init */
} ccsip_restart_cmd;

typedef enum {
    SIP_SHUTDOWN_REQ_NONE,
    SIP_SHUTDOWN_REQ_SHUT
} ccsip_shutdown_cmd;

typedef struct ccsip_restart_req_t_ {
    ccsip_restart_cmd cmd;     /* restart command */
} ccsip_restart_req;

typedef struct ccsip_shutdown_req_t_ {
    ccsip_shutdown_cmd cmd;    /* shutdown command */
    int action;
    int reason;
} ccsip_shutdown_req_t;


extern cprThread_t sip_thread;


/*---------------------------------------------------------
 *
 * Local Variables
 *
 */
// static uint16_t    nfds = 0; No reference. Should this be removed?
extern fd_set read_fds;
extern fd_set write_fds;
// static cpr_socket_t listenSocket = INVALID_SOCKET;  No reference. Should this be removed?
// static sip_connection_t sipConn; Set in ccsip_task.c but never used. Should this be removed?
// static cprMsgQueue_t sip_msg_queue;  No reference. Should this be removed?
// static cprRegion_t   sip_region;   No reference. Should this be removed?
// static cprPool_t     sip_pool;    No reference. Should this be removed?



/*---------------------------------------------------------
 *
 * Global Variables
 *
 */
sipGlobal_t sip;
boolean sip_mode_quiet = FALSE;
char sipHeaderServer[SIP_HEADER_SERVER_LEN];
char sipHeaderUserAgent[SIP_HEADER_SERVER_LEN];
char sipPhoneModelNumber[SIP_PHONE_MODEL_NUMBER_LEN];
char sipUnregisterReason[MAX_SIP_REASON_LENGTH];

boolean Is794x = FALSE;

/*---------------------------------------------------------
 *
 * External Variables
 * TODO reference through proper header files
 */
extern sipCallHistory_t gCallHistory[];


/*---------------------------------------------------------
 *
 * Function declarations
 *
 */
static void SIPTaskProcessSIPMessage(sipMessage_t *message);
static int  SIPTaskProcessSIPNotify(sipMessage_t *pSipMessage);
static int  SIPTaskProcessSIPNotifyMWI(sipMessage_t *pSipMessage,
                                       line_t dn_line);
static void SIPTaskProcessSIPNotifyCheckSync(sipMessage_t *pSipMessage);
static void SIPTaskProcessSIPPreviousCallByeResponse(sipMessage_t *pSipMessage,
                                             int response_code,
                                             line_t previous_call_index);
static void SIPTaskProcessSIPPreviousCallInviteResponse(sipMessage_t *pResponse,
                                             int response_code,
                                             line_t previous_call_index);
static void SIPTaskProcessSIPNotifyRefer(sipMessage_t *pSipMessage);
static int  SIPTaskProcessSIPNotifyServiceControl(sipMessage_t *pSipMessage);
static void SIPTaskProcessRestart(ccsip_restart_cmd cmd);
static void SIPTaskProcessShutdown(int action, int reason);

/*---------------------------------------------------------
 *
 * Functions
 *
 */
void
get_ua_model_and_device (char sipHdrUserAgent[])
{
    const char fname[] = "get_ua_model_and_device";
    char *model = NULL;

    model = (char *)platGetModel();

    if (model) {
        if (strncmp(model, CSF_MODEL, 3) == 0) {
            sstrncat(sipHdrUserAgent, CCSIP_SIP_CSF_USER_AGENT,
                    SIP_HEADER_SERVER_LEN - strlen(sipHdrUserAgent));
            sstrncpy(sipPhoneModelNumber, PHONE_MODEL_NUMBER_CSF,
                    SIP_PHONE_MODEL_NUMBER_LEN);
        } else if (strcmp(model, PHONE_MODEL) == 0) {
            //if phone model is any of vendor defined, set as is.
            sstrncat(sipHdrUserAgent, CCSIP_SIP_USER_AGENT,
                    SIP_HEADER_SERVER_LEN - strlen(sipHdrUserAgent));
            sstrncpy(sipPhoneModelNumber, PHONE_MODEL_NUMBER,
                    SIP_PHONE_MODEL_NUMBER_LEN);
        } else {
            // Default to 7970
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"unknown model,defaulting to model 7970: %s", fname, model);
            sstrncat(sipHdrUserAgent, CCSIP_SIP_7970_USER_AGENT,
                    SIP_HEADER_SERVER_LEN - strlen(sipHdrUserAgent));
            sstrncpy(sipPhoneModelNumber, PHONE_MODEL_NUMBER_7970,
                    SIP_PHONE_MODEL_NUMBER_LEN);
        }
    } else {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"could not obtain model information", fname);
        sstrncat(sipHdrUserAgent, CCSIP_SIP_7970_USER_AGENT,
                SIP_HEADER_SERVER_LEN - strlen(sipHdrUserAgent));
        sstrncpy(sipPhoneModelNumber, PHONE_MODEL_NUMBER_7970,
                SIP_PHONE_MODEL_NUMBER_LEN);
    }
}

extern void ccsip_debug_init(void);

/**
 *
 * SIPTaskInit
 *
 * Initialize the SIP Task
 *
 * Parameters:   None
 *
 * Return Value: SIP_OK
 *
 */
void
SIPTaskInit (void)
{
    /*
     * Initialize platform specific parameters
     */

    // sipConn is set but never used. Should this be removed?
    // for (i = 0; i < MAX_SIP_CONNECTIONS; i++) {
    //     sipConn.read[i] = INVALID_SOCKET;
    //     sipConn.write[i] = INVALID_SOCKET;
    //}

    /*
     * Initialize cprSelect call parameters
     */
    // XXX already did in sip_platform_task_init(), like, two instructions ago
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    /*
     * Do the debug init right here so that we can enable
     * sip debugs during startup and not wait for sip stack
     * to initialize.
     */
    ccsip_debug_init();

    /************************
    // Move all initialization to sip_sm_init called after config
    if (ccsip_register_init() == SIP_ERROR) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"ccsip_register_init() failed.", fname);
        return SIP_ERROR;
    }

     *
     * Allocate timers for CCBs
     *
    if (sip_platform_timers_init() == SIP_ERROR) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sip_platform_timers_init() failed", fname);
        return SIP_ERROR;
    }
    *************************/
    // Initialize the value of the UA and Server headers
    sipHeaderUserAgent[0] = '\0';
    sipPhoneModelNumber[0] = '\0';
    sipHeaderServer[0] = '\0';

#if defined _COMMUNICATOR_
    sstrncat(sipHeaderUserAgent, CCSIP_SIP_COMMUNICATOR_USER_AGENT,
            sizeof(sipHeaderUserAgent) - strlen(sipHeaderUserAgent));
    sstrncpy(sipPhoneModelNumber, PHONE_MODEL_NUMBER_COMMUNICATOR,
            SIP_PHONE_MODEL_NUMBER_LEN);
#else
    get_ua_model_and_device(sipHeaderUserAgent);
#endif

    // Now add the firmware version
    sstrncat(sipHeaderUserAgent, "/",
            sizeof(sipHeaderUserAgent) - strlen(sipHeaderUserAgent));
    sstrncat(sipHeaderUserAgent, gVersion,
            sizeof(sipHeaderUserAgent) - strlen(sipHeaderUserAgent));
    sstrncpy(sipHeaderServer, sipHeaderUserAgent,
            SIP_HEADER_SERVER_LEN);
}


/**
 *
 * SIPTaskSendMsg (API)
 *
 * The API to send a message to the SIP task
 *
 * Parameters:   cmd - the message type
 *               msg - the message buffer to send
 *               len - length of the message buffer
 *               usr - user pointer TEMPORARY VALUE TO MOVE INTO msg
 *
 * Return Value: SIP_OK or SIP_ERROR
 *
 */
cpr_status_e
SIPTaskSendMsg (uint32_t cmd, void *msg, uint16_t len, void *usr)
{
    phn_syshdr_t *syshdr;

    syshdr = (phn_syshdr_t *) cprGetSysHeader(msg);
    if (!syshdr) {
        return CPR_FAILURE;
    }
    syshdr->Cmd = cmd;
    syshdr->Len = len;
    syshdr->Usr.UsrPtr = usr;

    /*
     * If we send a message to the task too soon the sip variable is not set yet.
     * so just use the global for now. This happens if we create sip thread
     * and immediately send a CFG_DONE message to sip thread from the main thread.
     * This can be solved by waiting for an echo roundtrip from Thread before sending
     * any other message. Will do someday.
     */
    if (cprSendMessage(sip_msgq /*sip.msgQueue */ , (cprBuffer_t)msg, (void **)&syshdr)
        == CPR_FAILURE) {
        cprReleaseSysHeader(syshdr);
        return CPR_FAILURE;
    }
    return CPR_SUCCESS;
}

/**
 *
 * SIPTaskGetBuffer (API)
 *
 * The API to grab a buffer from the SIP buffer pool
 *
 * Parameters:   size - size of the buffer requested
 *
 * Return Value: requested buffer or NULL
 *
 */
cprBuffer_t
SIPTaskGetBuffer (uint16_t size)
{
    return cpr_malloc(size);
}

/**
 *
 * SIPTaskProcessListEvent (Internal API)
 *
 * Process a SIP event/message
 *
 * Parameters:   cmd - the type of event
 *               msg - the event message
 *               pUsr- a user pointer (TEMPORARY FIELD to be put into msg)
 *               len - the length of the message
 *
 * Return Value: None
 *
 */
void
SIPTaskProcessListEvent (uint32_t cmd, void *msg, void *pUsr, uint16_t len)
{
    static const char *fname = "SIPTaskProcessListEvent";
    sipSMEvent_t    sip_sm_event;
    int             idx;
	int	            p2psip = 0;
	int             sdpmode = 0;
    cprCallBackTimerMsg_t *timerMsg;
    line_t          last_available_line;
    CCM_ID ccm_id;

    timerMsg = (cprCallBackTimerMsg_t *) msg;
    CCSIP_DEBUG_TASK(DEB_F_PREFIX"cmd = 0x%x", DEB_F_PREFIX_ARGS(SIP_EVT, fname), cmd);

    /*
     * Loop and wait until we get a TCP_PHN_CFG_TCP_DONE or a RESTART
     * message from the phone before we finish
     * initializing the SIPTask and SIP state machine
     * (note this can happen any time the network stack is re-spun)
     */
    if ((sip.taskInited == FALSE) &&
        ((cmd != TCP_PHN_CFG_TCP_DONE) &&
         (cmd != SIP_RESTART) &&
         (cmd != SIP_TMR_SHUTDOWN_PHASE2) &&
         (cmd != SIP_SHUTDOWN) &&
         (cmd != TIMER_EXPIRATION) &&
         (cmd != THREAD_UNLOAD)) ) {
        cpr_free(msg);
        DEF_DEBUG(DEB_F_PREFIX" !!!sip.taskInited  is false. So not executing cmd=0x%x",
            DEB_F_PREFIX_ARGS(SIP_EVT, fname), cmd);
        return;
    }

    memset(&sip_sm_event, 0, sizeof(sipSMEvent_t));

    /*
     * Before CPR used to call the timer callback function out of its'
     * timer tick so quite a few of the SIP timers would just post
     * a timer expiration event back to its' queue to release the
     * CPR thread. Now CPR just sends a timer expiration event
     * to the SIP task so we don't want to double post a timer
     * expiration event. Therefore check to see if this is a timer
     * expiration event and if so possibly update the command before
     * switching on it. If the function returns false it means all
     * it did was update the command so we need to process the switch
     * statement, if it returns true the timer event has been handled.
     */
    if (cmd == TIMER_EXPIRATION) {
        if (SIPTaskProcessTimerExpiration(msg, &cmd)) {
            cpr_free(msg);
            return;
        }
        /*
         * No need to release the buffer if we overwrote the command
         * as the code that handles the new command below will release
         * the buffer.
         */
    }

	config_get_value(CFGID_P2PSIP, &p2psip, sizeof(p2psip));
	config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));

    switch (cmd) {
        /*
         * See comment above
         */
    case TCP_PHN_CFG_TCP_DONE:
        /*
         * Ignore any TCP_PHN_CFG_TCP_DONE message since it is only used to
         * determine when the SIP sm should be initialized
         */
        cpr_free(msg);

        // If P2P set transport to UDP
        if (p2psip == TRUE)
        	CC_Config_setIntValue(CFGID_TRANSPORT_LAYER_PROT, 2);


        if (sip_sm_init() < 0) {
        	CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sip_sm_init() failed ",  fname);
        	return;
        }

        sip_mode_quiet = FALSE;

        /*
         * If P2P or SDP only do not register with SIP Server
         */
        if (!p2psip && !sdpmode)
        	sip_platform_init();
        else
        	ui_set_sip_registration_state(CC_ALL_LINES, TRUE);

        sip.taskInited = TRUE;
        DEF_DEBUG(SIP_F_PREFIX"sip.taskInited is set to true ",  fname);
#ifdef SAPP_SAPP_GSM
        /*
         * Initialize SAPP.
         */
        if (sapp_init() != 0) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sapp_init() failed.", fname);
            break;
        }
        /*
         * Start SAPP. SAPP will register with the CCM.
         */
        if (sapp_test() != 0) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sapp_test() failed.", fname);
            break;
        }
#endif
        break;

    case SIP_GSM:
        /*
         * GSM CC Events
         */

#ifdef SAPP_SAPP_GSM
        if (sapp_process_cc_event(msg) == 0) {
            return;
        }
#endif

        if (p2psip == TRUE)
        	sipTransportSetSIPServer();

        if (sip_sm_process_cc_event(msg) != SIP_OK) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sip_sm_process_cc_event() "
                              "failed.\n", fname);
        }
        break;

    case SIP_TMR_INV_EXPIRE:
        idx = (long) (timerMsg->usrData);
        sip_sm_event.ccb = sip_sm_get_ccb_by_index((line_t)idx);
        cpr_free(msg);
        if (!sip_sm_event.ccb) {
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"usrData does not point to a valid ccb "
                             "discarding SIP_TMR_INV_EXPIRE event\n", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
            break;
        }
        sip_sm_event.type = E_SIP_INV_EXPIRES_TIMER;
        if (sip_sm_process_event(&sip_sm_event) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(SM_PROCESS_EVENT_ERROR), fname, sip_sm_event.type);
        }
        break;

    case SIP_TMR_SUPERVISION_DISCONNECT:
        idx = (long) (timerMsg->usrData);
        sip_sm_event.ccb = sip_sm_get_ccb_by_index((line_t)idx);
        cpr_free(msg);
        if (!sip_sm_event.ccb) {
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"usrData does not point to a valid ccb "
                             "discarding SIP_TMR_SUPERVISION_DISCONNECT event.\n",
                             DEB_F_PREFIX_ARGS(SIP_EVT, fname));
            break;
        }
        sip_sm_event.type = E_SIP_SUPERVISION_DISCONNECT_TIMER;
        if (sip_sm_process_event(&sip_sm_event) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(SM_PROCESS_EVENT_ERROR), fname, sip_sm_event.type);
        }
        break;

    case SIP_TMR_INV_LOCALEXPIRE:
        idx = (long) (timerMsg->usrData);
        sip_sm_event.ccb = sip_sm_get_ccb_by_index((line_t)idx);
        cpr_free(msg);
        if (!sip_sm_event.ccb) {
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"usrData does not point to valid ccb "
                             "discarding SIP_TMR_INV_LOCALEXPIRE "
                             "event.\n", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
            break;
        }
        sip_sm_event.type = E_SIP_INV_LOCALEXPIRES_TIMER;
        if (sip_sm_process_event(&sip_sm_event) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(SM_PROCESS_EVENT_ERROR), fname, sip_sm_event.type);
        }
        break;

        /*
         * UsrInfo is set to the IP address that bounced when an
         * IMCP Unreachable. -1 means we had a timeout, not a bounce.
         */
    case SIP_TMR_MSG_RETRY:
        idx = (long) (timerMsg->usrData);
        sip_sm_event.ccb = sip_sm_get_ccb_by_index((line_t)idx);
        sip_sm_event.type = E_SIP_TIMER;
        sip_sm_event.u.UsrInfo = ip_addr_invalid;
        cpr_free(msg);
        if (sip_sm_process_event(&sip_sm_event) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(SM_PROCESS_EVENT_ERROR), fname, sip_sm_event.type);
        }
        break;

    case SIP_ICMP_UNREACHABLE:
        {
            sipMethod_t retxMessageType;

            idx = msg ? (line_t) (*((uint32_t *)msg)) : 0;
            sip_sm_event.ccb = sip_sm_get_ccb_by_index((line_t)idx);
            /* IP address that bounced */
            sip_sm_event.u.UsrInfo = (*(cpr_ip_addr_t *)pUsr);
            cpr_free(msg);
            if (!sip_sm_event.ccb) {
                CCSIP_DEBUG_TASK(DEB_F_PREFIX"event does not point to valid ccb "
                                 "SIP_ICMP_UNREACHABLE event.\n", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
                break;
            }
            /*
             * Retrieve message type and cancel outstanding
             * timer and remove ICMP Handler
             */
            CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_ENTRY),
                              sip_sm_event.ccb->index,
                              sip_sm_event.ccb->dn_line,
                              fname, "ICMP_UNREACHABLE");

            /* Do not need to process the ICMP_UNREACHABLE for fallback ccbs */
            if (sip_sm_event.ccb->index <= REG_BACKUP_CCB) {
                retxMessageType =
                    sip_platform_msg_timer_messageType_get(sip_sm_event.ccb->index);
                sip_platform_msg_timer_stop(sip_sm_event.ccb->index);

                if (retxMessageType == sipMethodInvite) {
                    config_get_value(CFGID_SIP_INVITE_RETX,
                                     &sip_sm_event.ccb->retx_counter,
                                     sizeof(sip_sm_event.ccb->retx_counter));
                    if (sip_sm_event.ccb->retx_counter > MAX_INVITE_RETRY_ATTEMPTS) {
                        sip_sm_event.ccb->retx_counter = MAX_INVITE_RETRY_ATTEMPTS;
                    }
                } else {
                    config_get_value(CFGID_SIP_RETX,
                                     &sip_sm_event.ccb->retx_counter,
                                     sizeof(sip_sm_event.ccb->retx_counter));
                    if (sip_sm_event.ccb->retx_counter > MAX_NON_INVITE_RETRY_ATTEMPTS) {
                        sip_sm_event.ccb->retx_counter = MAX_NON_INVITE_RETRY_ATTEMPTS;
                    }
                }

                /*
                 * If REG CCB send into registration state machine else send
                 * into sip call processing machine
                 */
                if (retxMessageType == sipMethodRegister) {
                    /*
                     * UsrInfo is set to the IP address that bounced when an
                     * ICMP Unreachable. -1 means we had a timeout, not a bounce.
                     */
                    sip_sm_event.type = (sipSMEventType_t) E_SIP_REG_TMR_RETRY;
                    if (sip_reg_sm_process_event(&sip_sm_event) < 0) {
                        CCSIP_DEBUG_ERROR(get_debug_string(REG_SM_PROCESS_EVENT_ERROR), fname, sip_sm_event.type);
                    }
                } else {
                    sip_sm_event.type = E_SIP_ICMP_UNREACHABLE;
                    if (sip_sm_process_event(&sip_sm_event) < 0) {
                        CCSIP_DEBUG_ERROR(get_debug_string(SM_PROCESS_EVENT_ERROR), fname, sip_sm_event.type);
                    }
                }
            }
        }
        break;

    case SIP_TMR_REG_EXPIRE:
        idx = (long) (timerMsg->usrData);
        sip_sm_event.ccb = sip_sm_get_ccb_by_index((line_t)idx);
        sip_sm_event.type = (sipSMEventType_t) E_SIP_REG_TMR_EXPIRE;
        cpr_free(msg);
        if (sip_reg_sm_process_event(&sip_sm_event) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(REG_SM_PROCESS_EVENT_ERROR), fname, sip_sm_event.type);
        }
        break;

    case SIP_TMR_REG_ACK:
        idx = (long) (timerMsg->usrData);
        sip_sm_event.ccb = sip_sm_get_ccb_by_index((line_t)idx);
        sip_sm_event.type = (sipSMEventType_t) E_SIP_REG_TMR_ACK;
        cpr_free(msg);
        if (sip_reg_sm_process_event(&sip_sm_event) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(REG_SM_PROCESS_EVENT_ERROR), fname, sip_sm_event.type);
        }
        break;

    case SIP_TMR_REG_WAIT:
        idx = msg ? (line_t) (*((uint32_t *)msg)) : CC_NO_LINE;
        sip_sm_event.ccb = sip_sm_get_ccb_by_index((line_t)idx);
        sip_sm_event.type = (sipSMEventType_t) E_SIP_REG_TMR_WAIT;
        cpr_free(msg);
        if (sip_reg_sm_process_event(&sip_sm_event) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(REG_SM_PROCESS_EVENT_ERROR), fname, sip_sm_event.type);
        }
        break;

        /*
         * UsrInfo is set to the IP address that bounced when an
         * ICMP Unreachable. -1 means we had a timeout, not a bounce.
         */
        /*
         * currently, we only look for ccm_id for SIP_TMR_REG_RETRY. for rest
         * of the REG msg, existing code will work because first field of
         * ccsip_registration_msg_t is uint32_t.
         */
    case SIP_TMR_REG_RETRY:
        idx = msg ? (line_t) ((ccsip_registration_msg_t *)msg)->ccb_index : CC_NO_LINE;
        ccm_id = msg ? ((ccsip_registration_msg_t *)msg)->ccm_id : UNUSED_PARAM;
        sip_sm_event.ccb = sip_sm_get_ccb_by_ccm_id_and_index(ccm_id, (line_t) idx);
        sip_sm_event.type = (sipSMEventType_t) E_SIP_REG_TMR_RETRY;
        sip_sm_event.u.UsrInfo = ip_addr_invalid;
        cpr_free(msg);
        if (sip_sm_event.ccb == NULL || sip_reg_sm_process_event(&sip_sm_event) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(REG_SM_PROCESS_EVENT_ERROR), fname, sip_sm_event.type);
        }
        break;

    case SIP_REG_REQ:
        idx = msg ? (line_t) (*((uint32_t *)msg)) : CC_NO_LINE;
        cpr_free(msg);
        sip_sm_event.ccb = sip_sm_get_ccb_by_index((line_t) idx);
        if (!sip_sm_event.ccb) {
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"event data does not point to a valid ccb"
                             "SIP_REG_REQ event.\n", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
            break;
        }
        (void) sip_sm_ccb_init(sip_sm_event.ccb, (line_t)idx, idx, SIP_REG_STATE_IDLE);
        sip_sm_event.ccb->state = (sipSMStateType_t) SIP_REG_STATE_IDLE;
        sip_sm_event.type = (sipSMEventType_t) E_SIP_REG_REG_REQ;
        if (sip_reg_sm_process_event(&sip_sm_event) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(REG_SM_PROCESS_EVENT_ERROR), fname, sip_sm_event.type);
        }
        break;

    case SIP_REG_CANCEL:
        idx = msg ? (line_t) (*((uint32_t *)msg)) : CC_NO_LINE;
        sip_sm_event.ccb = sip_sm_get_ccb_by_index((line_t) idx);
        sip_sm_event.type = (sipSMEventType_t) E_SIP_REG_CANCEL;
        cpr_free(msg);
        if (sip_reg_sm_process_event(&sip_sm_event) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(REG_SM_PROCESS_EVENT_ERROR), fname, sip_sm_event.type);
        }
        break;

    case SIP_REG_CLEANUP:
        idx = msg ? (line_t) (*((uint32_t *)msg)) : CC_NO_LINE;
        sip_sm_event.ccb = sip_sm_get_ccb_by_index((line_t) idx);
        sip_sm_event.type = (sipSMEventType_t) E_SIP_REG_CLEANUP;
        cpr_free(msg);
        if (sip_reg_sm_process_event(&sip_sm_event) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(REG_SM_PROCESS_EVENT_ERROR), fname, sip_sm_event.type);
        }
        break;

    case SIP_TMR_STANDBY_KEEPALIVE:
        sip_sm_event.ccb = sip_sm_get_ccb_by_index(REG_BACKUP_CCB);
        sip_sm_event.type = (sipSMEventType_t) E_SIP_REG_TMR_WAIT;
        cpr_free(msg);
        if (sip_reg_sm_process_event(&sip_sm_event) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(REG_SM_PROCESS_EVENT_ERROR), fname, sip_sm_event.type);
        }
        break;


        // Subscribe/Notify Events
    case SIPSPI_EV_CC_SUBSCRIBE_REGISTER:
        if (subsmanager_handle_ev_app_subscribe_register(msg) < 0) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"subsmanager_handle_ev_app_subscribe_register() "
                              "returned error.\n", fname);
        }
        cpr_free(msg);
        break;
    case SIPSPI_EV_CC_SUBSCRIBE:
        if (subsmanager_handle_ev_app_subscribe(msg) < 0) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"subsmanager_handle_ev_app_subscribe() "
                              "returned error.\n", fname);
        }
        cpr_free(msg);
        break;
    case SIPSPI_EV_CC_SUBSCRIBE_RESPONSE:
        if (subsmanager_handle_ev_app_subscribe_response(msg) < 0) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"subsmanager_handle_ev_app_subscribe_response() "
                              "returned error.\n", fname);
        }
        cpr_free(msg);
        break;
    case SIPSPI_EV_CC_NOTIFY:
        {
            int ret;

            ret = subsmanager_handle_ev_app_notify(msg);
            if (ret == SIP_ERROR) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"subsmanager_handle_ev_app_notify() "
                                  "returned error.\n", fname);
            } else if (ret == SIP_DEFER) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"subsmanager_handle_ev_app_notify() "
                                  "deferred request.\n", fname);
            }
        }
        cpr_free(msg);
        break;
    case SIPSPI_EV_CC_NOTIFY_RESPONSE:
        if (subsmanager_handle_ev_app_notify_response(msg) < 0) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"subsmanager_handle_ev_app_notify_response() "
                              "returned error.\n", fname);
        }
        cpr_free(msg);
        break;
    case SIPSPI_EV_CC_SUBSCRIPTION_TERMINATED:
        if (subsmanager_handle_ev_app_subscription_terminated(msg) < 0) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"subsmanager_handle_ev_app_subscription_terminated() "
                              "returned error.\n", fname);
        }
        cpr_free(msg);
        break;

    case SIPSPI_EV_CC_PUBLISH_REQ:
        if (publish_handle_ev_app_publish(msg) < 0) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"publish_handle_ev_app_publish() "
                              "returned error.\n", fname);
        }
        cpr_free(msg);
        break;

    case SIP_REG_UPDATE:
        last_available_line = msg ? (line_t) (*((uint32_t *)msg)) : CC_NO_LINE;
        cpr_free(msg);
        regmgr_handle_register_update(last_available_line);
        break;
    case REG_MGR_STATE_CHANGE:

        sip_regmgr_rsp(((cc_regmgr_t *)msg)->rsp_id,
                       ((cc_regmgr_t *)msg)->rsp_type,
                       ((cc_regmgr_t *)msg)->wait_flag);
        cpr_free(msg);
        break;


        // Retry timer expired for Sub/Not
    case SIP_TMR_MSG_RETRY_SUBNOT:
        idx = (long) (timerMsg->usrData);
        cpr_free(msg);
        if (subsmanager_handle_retry_timer_expire(idx) < 0) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"subsmanager_handle_retry_timer_expire() "
                              "returned error.\n", fname);
        }
        break;
        // Sub/Not periodic timer
    case SIP_TMR_PERIODIC_SUBNOT:
        cpr_free(msg);
        subsmanager_handle_periodic_timer_expire();
        publish_handle_periodic_timer_expire();
        break;

    case SIP_RESTART:
        {
            ccsip_restart_req *req = (ccsip_restart_req *) msg;
            ccsip_restart_cmd restartCmd;

            restartCmd = req->cmd;
            // Handle restart event from platform
            cpr_free(msg);

            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Received SIP_RESTART event restartCmd = (%d)",
                DEB_F_PREFIX_ARGS(SIP_EVT, fname), restartCmd);
            SIPTaskProcessRestart(restartCmd);
            break;
        }

    case SIP_TMR_SHUTDOWN_PHASE2:
        {
            // Handle shutdown phase 2 event (after unregistration)
            // Note: boolean is 8 bits, but 32 bits have been allocated for it
            // so we need to dereference msg with 32 bits
            int action;

            if (msg) {
                action = (*((uint32_t *)msg));
            } else {
                action = SIP_INTERNAL;
            }

            cpr_free(msg);

            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Received SIP_TMR_SHUTDOWN_PHASE2 event action= (%d)",
                             DEB_F_PREFIX_ARGS(SIP_EVT, fname),  action);
            sip_shutdown_phase2(action);
        }
        break;

    case SIP_SHUTDOWN:
        {
            ccsip_shutdown_req_t *req = (ccsip_shutdown_req_t *) msg;
            int action;
            int reason;

            action = req->action;
            reason = req->reason;
            cpr_free(msg);
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Received SIP_SHUTDOWN message",
                DEB_F_PREFIX_ARGS(SIP_EVT, fname));
            SIPTaskProcessShutdown(action, reason);
        }
        break;

    case THREAD_UNLOAD:
        {
            thread_ended(THREADMON_SIP);
            destroy_sip_thread();
        }
        break;

    case SIP_TMR_GLARE_AVOIDANCE:
        // Handle glare retry timer expiry
        idx = (long) (timerMsg->usrData);
        sip_sm_event.ccb = sip_sm_get_ccb_by_index((line_t)idx);
        sip_sm_event.type = (sipSMEventType_t) E_SIP_GLARE_AVOIDANCE_TIMER;
        cpr_free(msg);
        if (sip_sm_process_event(&sip_sm_event) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(SM_PROCESS_EVENT_ERROR), fname, sip_sm_event.type);
        }
        break;

    default:
        cpr_free(msg);
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unknown message", fname);
        break;
    }
    return;
}

/**
 * SIPTaskCheckSource
 *
 * Ensure that sender is a trusted source
 *
 * Parameters: from - the source address for the message
 *
 * Return Value: SIP_OK if message can be processed further
 */
int
SIPTaskCheckSource (cpr_sockaddr_storage from)
{
    static const char fname[] = "SIPTaskCheckSource";
    int             regConfigValue;
    line_t          line_index, line_end;
    cpr_ip_addr_t   fromIPAddr;
    int             retval = SIP_ERROR;
    char            fromIPAddrStr[MAX_IPADDR_STR_LEN];
    ccsipCCB_t     *ccb = NULL;
    uint32_t        data, *data_p;

    // If not registered, return ok
    config_get_value(CFGID_PROXY_REGISTER, &regConfigValue, sizeof(regConfigValue));
    if (regConfigValue == 0) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"CFGID_PROXY_REGISTER is false",
            DEB_F_PREFIX_ARGS(SIP_IP_MATCH, fname));
        return SIP_OK;
    }

    line_end = 1;
    line_end += TEL_CCB_END;

    util_extract_ip(&fromIPAddr,&from);
    util_ntohl(&fromIPAddr, &fromIPAddr);

    fromIPAddrStr[0] = '\0';
    ipaddr2dotted(fromIPAddrStr, &fromIPAddr);

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Attempting to recognize \"%s\"",
        DEB_F_PREFIX_ARGS(SIP_IP_MATCH, fname), fromIPAddrStr);
    // Get the proxy configured for all lines and check against those
    for (line_index = REG_CCB_START; line_index <= line_end; line_index++) {
        // Check the binary from-IP address with the ccb->reg.addr value
        if (sip_config_check_line((line_t)(line_index - TEL_CCB_END))) {
            ccb = sip_sm_get_ccb_by_index(line_index);
            if (ccb && util_compare_ip(&(ccb->reg.addr), &fromIPAddr)) {
                CCSIP_DEBUG_TASK(DEB_F_PREFIX"Found server IP match",
                    DEB_F_PREFIX_ARGS(SIP_IP_MATCH, fname));
                retval = SIP_OK;
                break;
            }
        }
    }
    if (retval == SIP_OK) {
        return retval;
    }
    // Not found - continue to check backup CCBs
    ccb = sip_sm_get_ccb_by_index(REG_BACKUP_CCB);
    if (ccb && util_compare_ip(&(ccb->reg.addr), &fromIPAddr)) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Found backup server IP match",
            DEB_F_PREFIX_ARGS(SIP_IP_MATCH, fname));
        retval = SIP_OK;
    }
    if (retval == SIP_OK) {
        return retval;
    }
    // Not found - continue to check with fallback CCBs
    data = 0x00;
    data_p = &data;
    ccb = sip_regmgr_get_fallback_ccb_list(data_p);
    while ((ccb != NULL) && (retval != SIP_OK)) {
        if (util_compare_ip(&(ccb->reg.addr), &fromIPAddr)) {
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Found fallback server IP match",
                DEB_F_PREFIX_ARGS(SIP_IP_MATCH, fname));
            retval = SIP_OK;
        }
        ccb = sip_regmgr_get_fallback_ccb_list(data_p);
    }
    return retval;
}

/**
 *
 * SIPTaskProcessUDPMessage (Internal API)
 *
 * Process the received (via UDP) SIP message
 *
 * Parameters:   msg  - the message buffer
 *               len  - length of the message buffer
 *               from - the source address for the message
 *
 * Return Value: SIP_OK if message could be processed (though this itself
 *               may fail) or SIP_ERROR
 *
 */
int
SIPTaskProcessUDPMessage (cprBuffer_t msg,
                          uint16_t len,
                          cpr_sockaddr_storage from)
{
    static const char *fname = "SIPProcessUDPMessage";
    sipMessage_t   *pSipMessage = NULL;
    static char     buf[SIP_UDP_MESSAGE_SIZE + 1];
    char            remoteIPAddrStr[MAX_IPADDR_STR_LEN];
    uint32_t        bytes_used = 0;
    int             accept_msg = SIP_OK;
    cpr_ip_addr_t   ip_addr;
	int p2psip = 0;
    /*
     * Convert IP address to string, for debugs
     */
    util_extract_ip(&ip_addr, &from);

    util_ntohl(&ip_addr, &ip_addr);

    if (SipDebugMessage) {
        ipaddr2dotted(remoteIPAddrStr, &ip_addr);
    }

    util_extract_ip(&ip_addr, &from);

    if (len > SIP_UDP_MESSAGE_SIZE) {
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Received UDP message from <%s>:<%d>: "
                            "message too big: msg size = %d, max SIP "
                            "pkt size = %d\n", DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname), remoteIPAddrStr,
                             util_get_port(&from), bytes_used, SIP_UDP_MESSAGE_SIZE);
        cpr_free(msg);
        return SIP_ERROR;
    }

    /*
     * Copy message to a local memory and release the system buffer
     */
    memcpy(buf, (char *)msg, len);
    buf[len] = '\0'; /* NULL terminate for debug printing */
    cpr_free(msg);

    /*
     * Print the received UDP packet info
     */
    CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"recv UDP message from <%s>:<%d>, length=<%d>, "
                        "message=\n", DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname), remoteIPAddrStr,
                        util_get_port(&from), len);
    CCSIP_DEBUG_MESSAGE_PKT(buf);

    /*
     * Determine whether we want to process this packet
     * Initially just do this if we are talking to CCM - can be expanded later
     */



    config_get_value(CFGID_P2PSIP, &p2psip, sizeof(p2psip));

    if (p2psip == 0) {
    	if (sip_regmgr_get_cc_mode(1) == REG_MODE_CCM) {
        	accept_msg = SIPTaskCheckSource(from);
        	if (accept_msg != SIP_OK) {
        		CCSIP_DEBUG_ERROR(SIP_F_PREFIX"SIPTaskCheckSource() failed - Sender not "
        				"recognized\n", fname);
            return SIP_ERROR;
        	}
    	}
    }

    /*
     * Convert to SIP representation
     */
    pSipMessage = sippmh_message_create();
    if (!pSipMessage) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sippmh_message_create() failed", fname);
        return SIP_ERROR;
    }

    bytes_used = len;

    if (sippmh_process_network_message(pSipMessage, buf, &bytes_used)
            == STATUS_FAILURE) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sippmh_process_network_message() "
                          "failed. discarding the message.\n", fname);
        free_sip_message(pSipMessage);
        return SIP_ERROR;
    }
    /*
     * Add processing here to append received=<ip_address> to the first Via
     * field if the IP address we received this message from is not the same
     * as the IP address in the Via field or if it contains a domain name
     */
    sippmh_process_via_header(pSipMessage, &ip_addr);

    ccsip_dump_recv_msg_info(pSipMessage, &ip_addr, 0);

    /* Process SIP message */
    SIPTaskProcessSIPMessage(pSipMessage);
    return SIP_OK;
}

/**
 *
 * SIPTaskProcessTCPMessage (Internal API)
 *
 * Process the received (via TCP) SIP message
 *
 * Parameters:   msg  - the message buffer
 *               len  - length of the message buffer
 *               from - the source address for the message
 *
 * Return Value: SIP_OK if message could be processed (though this itself
 *               may fail) or SIP_ERROR
 *
 */
void
SIPTaskProcessTCPMessage (sipMessage_t *pSipMessage, cpr_sockaddr_storage from)
{
    cpr_ip_addr_t   ip_addr;

    CPR_IP_ADDR_INIT(ip_addr);
    /*
     * Convert IP address to string, for debugs
     */
    util_extract_ip(&ip_addr, &from);

    /*
     * Add processing here to append received=<ip_address> to the first Via
     * field if the IP address we received this message from is not the same
     * as the IP address in the Via field or if it contains a domain name
     */
    sippmh_process_via_header(pSipMessage, &ip_addr);

    /* Process SIP message */
    SIPTaskProcessSIPMessage(pSipMessage);
}

int
SIPTaskRetransmitPreviousResponse (sipMessage_t *pSipMessage,
                                  const char *fname,
                                  const char *pCallID,
                                  sipCseq_t *sipCseq,
                                  int response_code,
                                  boolean is_request)
{
    sipRelDevMessageRecord_t *pRequestRecord = NULL;
    int             handle = -1;
    const char     *reldev_to = NULL;
    const char     *reldev_from = NULL;
    sipLocation_t  *reldev_to_loc = NULL;
    sipLocation_t  *reldev_from_loc = NULL;

    pRequestRecord = (sipRelDevMessageRecord_t *)
        cpr_calloc(1, sizeof(sipRelDevMessageRecord_t));
    if (!pRequestRecord) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to allocate "
                          "mem for pRequestRecord.\n", fname);
        return SIP_ERROR;
    }

    // Copy to-tag
    reldev_to = sippmh_get_cached_header_val(pSipMessage, TO);
    if (reldev_to) {
        reldev_to_loc = sippmh_parse_from_or_to((char *)reldev_to, TRUE);
        if ((reldev_to_loc) && (reldev_to_loc->tag)) {
            sstrncpy(pRequestRecord->tag,
                     sip_sm_purify_tag(reldev_to_loc->tag),
                                       MAX_SIP_TAG_LENGTH);
        }

        // Copy to-user
        if (reldev_to_loc) {
            sstrncpy(pRequestRecord->to_user,
                     reldev_to_loc->genUrl->u.sipUrl->user,
                     RELDEV_MAX_USER_NAME_LEN);
            sippmh_free_location(reldev_to_loc);
        }
    }

    // Copy from-user and from-host
    reldev_from = sippmh_get_cached_header_val(pSipMessage, FROM);
    if (reldev_from) {
        reldev_from_loc = sippmh_parse_from_or_to((char *)reldev_from, TRUE);
        if (reldev_from_loc) {
            sstrncpy(pRequestRecord->from_user,
                     reldev_from_loc->genUrl->u.sipUrl->user,
                     RELDEV_MAX_USER_NAME_LEN);
            sstrncpy(pRequestRecord->from_host,
                     reldev_from_loc->genUrl->u.sipUrl->host,
                     RELDEV_MAX_HOST_NAME_LEN);

            sippmh_free_location(reldev_from_loc);
        }
    }

    pRequestRecord->is_request = is_request;
    pRequestRecord->response_code = response_code;

    // Copy Call-id
    sstrncpy(pRequestRecord->call_id, (pCallID) ? pCallID : "",
             MAX_SIP_CALL_ID);

    // Copy CSeq values
    pRequestRecord->cseq_method = sipCseq->method;
    pRequestRecord->cseq_number = sipCseq->number;

    if (sipRelDevMessageIsDuplicate(pRequestRecord, &handle)) {
        cpr_free(pRequestRecord);
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Line filter: Previous "
                         "Call ID. Resending stored "
                         "response...\n", DEB_F_PREFIX_ARGS(SIP_MSG, fname));
        if (sipRelDevCoupledMessageSend(handle) < 0) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sipRelDevCoupledMessageSend(%d)"
                              "returned error.\n", fname, handle);
        }
        return SIP_OK;
    } else {
        cpr_free(pRequestRecord);
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Line filter: Previous Call "
                         "ID. Message not in reTx list.\n", DEB_F_PREFIX_ARGS(SIP_MSG, fname));
    }
    return SIP_ERROR;
}

/**
 *
 * SIPTaskProcessTimerExpiration
 *
 * Process a msg indicating a timer has expired
 *
 * Parameters:   msg - the timer callback msg
 *               cmd - allows us to overwrite the cmd if needed
 *
 * Return Value: boolean, if true return as timer has been processed.
 *
 */
short
SIPTaskProcessTimerExpiration (void *msg, uint32_t *cmd)
{
    static const char fname[] = "SIPTaskProcessTimerExpiration";
    cprCallBackTimerMsg_t *timerMsg;
    boolean returnCode = TRUE;
    uint32_t handle;

    timerMsg = (cprCallBackTimerMsg_t *) msg;
    TMR_DEBUG(DEB_F_PREFIX"Timer %s expired. Id is %d", DEB_F_PREFIX_ARGS(SIP_TIMER, fname),
              timerMsg->expiredTimerName, timerMsg->expiredTimerId);

    /* The REGALLFAIL Timer message could come in before the task has
     * been initialized. Need to handle this timer, in order to restart
     * the system. */
    if ((sip.taskInited == FALSE) && (timerMsg->expiredTimerId != SIP_REGALLFAIL_TIMER)) {
        return returnCode;
    }

    switch (timerMsg->expiredTimerId) {
    case SIP_ACK_TIMER:
        *cmd = SIP_TMR_REG_ACK;
        returnCode = FALSE;
        break;

    case SIP_WAIT_TIMER:
        sip_regmgr_wait_timeout_expire(timerMsg->usrData);
        break;

    case SIP_RETRY_TIMER:
        sip_regmgr_retry_timeout_expire(timerMsg->usrData);
        break;

    case SIP_MSG_TIMER:
        *cmd = SIP_TMR_MSG_RETRY;
        returnCode = FALSE;
        break;

    case SIP_EXPIRES_TIMER:
        *cmd = SIP_TMR_INV_EXPIRE;
        returnCode = FALSE;
        break;

    case SIP_REG_TIMEOUT_TIMER:
        ccsip_register_timeout_retry(timerMsg->usrData);
        break;

    case SIP_REG_EXPIRES_TIMER:
        *cmd = SIP_TMR_REG_EXPIRE;
        returnCode = FALSE;
        break;

    case SIP_LOCAL_EXPIRES_TIMER:
        *cmd = SIP_TMR_INV_LOCALEXPIRE;
        returnCode = FALSE;
        break;

    case SIP_SUPERVISION_TIMER:
        *cmd = SIP_TMR_SUPERVISION_DISCONNECT;
        returnCode = FALSE;
        break;

    case SIP_GLARE_AVOIDANCE_TIMER:
        *cmd = SIP_TMR_GLARE_AVOIDANCE;
        returnCode = FALSE;
        break;

    case SIP_KEEPALIVE_TIMER:
        *cmd = SIP_TMR_STANDBY_KEEPALIVE;
        returnCode = FALSE;
        break;

    case SIP_UNREGISTRATION_TIMER:
        sip_platform_unregistration_callback(timerMsg->usrData);
        break;

    case SIP_SUBNOT_TIMER:
        *cmd = SIP_TMR_MSG_RETRY_SUBNOT;
        returnCode = FALSE;
        break;

    case SIP_SUBNOT_PERIODIC_TIMER:
        sip_platform_subnot_periodic_timer_callback(timerMsg->usrData);
        break;

    case SIP_PUBLISH_RETRY_TIMER:
        handle = (long)(timerMsg->usrData);
        if (publish_handle_retry_timer_expire(handle) < 0) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"publish_handle_retry_timer_expire() "
                              "returned error.\n", fname);
        }
        break;

    case SIP_UNSOLICITED_TRANSACTION_TIMER:
        subsmanager_unsolicited_notify_timeout(timerMsg->usrData);
        break;

    case SIP_NOTIFY_TIMER:
        sip_regmgr_notify_timer_callback(timerMsg->usrData);
        break;

	case SIP_PASSTHROUGH_TIMER:
		CCSIP_DEBUG_ERROR("%s: Pass Through Timer fired !", fname);
		break;

    case SIP_REGALLFAIL_TIMER:
        sip_regmgr_regallfail_timer_callback(timerMsg->usrData);
        break;

    default:
        CSFLogError("sipstack", "%s: unknown timer %s", fname,
           timerMsg->expiredTimerName);
        break;
    }
    return (returnCode);
}


/**
 *
 * SIPTaskProcessSIPMessage
 *
 * Process a received SIP message
 *
 * Parameters:   pSipMessage - the SIP message
 *
 * Return Value: SIP_OK or SIP_ERROR
 *
 */
static void
SIPTaskProcessSIPMessage (sipMessage_t *pSipMessage)
{
    static const char *fname = "SIPTaskProcessSIPMessage";
    sipSMEvent_t    sip_sm_event;
    ccsipCCB_t     *reg_ccb = NULL,*ccb = NULL;
    sipStatusCodeClass_t code_class = codeClassInvalid;
    sipMethod_t     method = sipMethodInvalid;
    const char     *pCallID = NULL;
    line_t          line_index = 1;
    boolean         is_request = FALSE;
    int             regConfigValue, response_code = 0;
    int             requestStatus = SIP_MESSAGING_ERROR;
    char            errortext[MAX_SIP_URL_LENGTH];
    const char     *cseq = NULL;
    sipCseq_t      *sipCseq = NULL;
    uint16_t        result_code = 0;
    const char     *max_fwd_hdr = NULL;
    int32_t         max_fwd_hdr_val;
    int             rc;
    char            tmp_str[STATUS_LINE_MAX_LEN];
    sipSCB_t       *scbp = NULL;
    sipTCB_t       *tcbp = NULL;
    int             scb_index;
    //ccsip_event_data_t * evt_data_ptr = NULL;

    sip_sm_event.u.pSipMessage = pSipMessage;
    /*
     * is_complete will be FALSE if we received fewer bytes than
     * specified in the content_length field.  If that happens, we
     * want to return a 400 Bad Request message.
     */
    if (!sippmh_is_message_complete(pSipMessage)) {
        /* Send 400 error */
        if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                    SIP_CLI_ERR_BAD_REQ_PHRASE,
                                    SIP_WARN_MISC,
                                    SIP_CLI_ERR_BAD_REQ_SDP_ERROR,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        free_sip_message(pSipMessage);
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_is_message_complete()");
        return;
    }

    /*
     * Determine Type, Method, and Call ID of this SIP message
     * If the SIP message is a response, also get the response code.
     */
    is_request = sippmh_is_request(pSipMessage);
    if (is_request) {
        /*
         * Check the value of the max-forwards header - if present
         */
        max_fwd_hdr = sippmh_get_header_val(pSipMessage,
                                            SIP_HEADER_MAX_FORWARDS, NULL);
        if (max_fwd_hdr) {
            max_fwd_hdr_val = sippmh_parse_max_forwards(max_fwd_hdr);
            if (max_fwd_hdr_val < 0) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Invalid Max Fwd Value detected",
                                  fname);
                /* Send 483 error */
                if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_MANY_HOPS,
                                            SIP_CLI_ERR_MANY_HOPS_PHRASE,
                                            SIP_WARN_MISC,
                                            NULL, NULL) != TRUE) {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                      fname, SIP_CLI_ERR_BAD_REQ);
                }
                free_sip_message(pSipMessage);
                return;
            }
        }

        sipGetRequestMethod(pSipMessage, &method);

        switch (method) {
        case sipMethodInvalid:
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sipGetResponseMethod");
            /* Send 400 error */
            if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                        SIP_CLI_ERR_BAD_REQ_PHRASE,
                                        SIP_WARN_MISC,
                                        SIP_CLI_ERR_BAD_REQ_METHOD_UNKNOWN,
                                        NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_CLI_ERR_BAD_REQ);
            }
            free_sip_message(pSipMessage);
            return;

        case sipMethodUnknown:
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"SIP method not implemented", fname);
            // Send 501 error
            if (sipSPISendErrorResponse(pSipMessage, SIP_SERV_ERR_NOT_IMPLEM,
                                        SIP_SERV_ERR_NOT_IMPLEM_PHRASE, 0,
                                        NULL, NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_SERV_ERR_NOT_IMPLEM);
            }
            free_sip_message(pSipMessage);
            return;

        case sipMethodRegister:
        case sipMethodPrack:
        case sipMethodComet:
        case sipMethodMessage:
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"SIP method not allowed", fname);
            // Send 405 error
            if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_NOT_ALLOWED,
                                        SIP_CLI_ERR_NOT_ALLOWED_PHRASE, 0,
                                        NULL, NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_CLI_ERR_NOT_ALLOWED);
            }
            free_sip_message(pSipMessage);
            return;

        default:
            break;
        }
    } else {
        if (sipGetResponseMethod(pSipMessage, &method) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sipGetResponseMethod");
            free_sip_message(pSipMessage);
            return;
        }
        if (sipGetResponseCode(pSipMessage, &response_code) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sipGetResponseCode");
            free_sip_message(pSipMessage);
            return;
        }
        code_class = sippmh_get_code_class((uint16_t) response_code);
    }

    /* Get the CSeq */
    cseq = sippmh_get_cached_header_val(pSipMessage, CSEQ);
    if (!cseq) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to extract "
                          "CSeq from message.\n", fname);
        if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                    SIP_CLI_ERR_BAD_REQ_PHRASE,
                                    SIP_WARN_MISC,
                                    SIP_CLI_ERR_BAD_REQ_VIA_OR_CSEQ,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        free_sip_message(pSipMessage);
        return;
    }
    sipCseq = sippmh_parse_cseq(cseq);
    if (!sipCseq) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to parse "
                          "CSeq from message.\n", fname);
        if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                    SIP_CLI_ERR_BAD_REQ_PHRASE,
                                    SIP_WARN_MISC,
                                    SIP_CLI_ERR_BAD_REQ_VIA_OR_CSEQ,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        free_sip_message(pSipMessage);
        return;
    }

    pCallID = sippmh_get_cached_header_val(pSipMessage, CALLID);
    if (!pCallID) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Cannot obtain SIP Call ID.", fname);
        /*
         * Since we have no Call-ID, we can't create a response;
         * therefore, we drop it.
         */
        cpr_free(sipCseq);
        free_sip_message(pSipMessage);
        return;
    }

    /*
     * Unsolicited NOTIFY processing
     */
    if ((is_request) && (method == sipMethodNotify)) {
        CCSIP_DEBUG_TASK(get_debug_string(DEBUG_SIP_MSG_RECV),
                         fname, SIP_METHOD_NOTIFY);
        rc = SIPTaskProcessSIPNotify(pSipMessage);
        if (rc != SIP_DEFER) {
            if (rc != 0) {
                // This Notify is in response to a previous SUBSCRIBE or REFER
                (void) subsmanager_handle_ev_sip_subscribe_notify(pSipMessage);
            }
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;
        }
    }

    /*
     * SUBSCRIBE processing. Subscription requests received from the network,
     * are processed here and do not interact with the main call processing
     * states. All SUBSCRIBE requests and responses are directed to the
     * subscription manager. However NOTIFY responses need to be testes to
     * see whether the NOTIFY request was generated via CC or SM
     */
    if ((is_request) && (method == sipMethodSubscribe)) {

        config_get_value(CFGID_PROXY_REGISTER, &regConfigValue, sizeof(regConfigValue));

        reg_ccb = sip_sm_get_ccb_by_index(REG_CCB_START);

        if ((regConfigValue == 0) || (reg_ccb && reg_ccb->reg.registered)) {

            CCSIP_DEBUG_TASK(get_debug_string(DEBUG_SIP_MSG_RECV),
                             fname, SIP_METHOD_SUBSCRIBE);
            (void) subsmanager_handle_ev_sip_subscribe(pSipMessage, method, FALSE);
        } else {
            if (sipSPISendErrorResponse(pSipMessage, SIP_SERV_ERR_INTERNAL,
                                        SIP_SERV_ERR_INTERNAL_PHRASE,
                                        0, NULL, NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_SERV_ERR_INTERNAL);
            }

        }
        cpr_free(sipCseq);
        free_sip_message(pSipMessage);
        return;
    }
    if (is_request == FALSE) {
        switch (method) {
        case sipMethodSubscribe:
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Recv Subs Response.",
                DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
            (void) subsmanager_handle_ev_sip_response(pSipMessage);
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;

        case sipMethodNotify:
            scbp = find_scb_by_callid(pCallID, &scb_index);
            if (scbp != NULL) {
                CCSIP_DEBUG_TASK(DEB_F_PREFIX"Recv Notify response",
                    DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
                (void) subsmanager_handle_ev_sip_response(pSipMessage);
                cpr_free(sipCseq);
                free_sip_message(pSipMessage);
                return;
            } else {
                tcbp = find_tcb_by_sip_callid(pCallID);
                if (tcbp != NULL) {
                    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Recv Unsolicited Notify response",
                        DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
                    (void) subsmanager_handle_ev_sip_unsolicited_notify_response(pSipMessage, tcbp);
                    cpr_free(sipCseq);
                    free_sip_message(pSipMessage);
                    return;
                }
            }
            break;

        case sipMethodPublish:
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Recv PUBLISH Response.",
                DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
            (void) publish_handle_ev_sip_response(pSipMessage);
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;

        case sipMethodInfo:
            // XXX FIXME see the comments in sipSPISendInfo()
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Recv INFO Response (silently dropped).",
                DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;

        default:
            break;

        }
    }


    /*
     * Determine which line this SIP message is for.
     */
    result_code = sip_sm_determine_ccb(pCallID, sipCseq, pSipMessage,
                                       is_request, &ccb);
    if (result_code != 0) {
        if (is_request) {
            if (result_code == SIP_CLI_ERR_LOOP_DETECT) {
                /* Send 482 error */
                if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_LOOP_DETECT,
                                            SIP_CLI_ERR_LOOP_DETECT_PHRASE,
                                            SIP_WARN_MISC,
                                            NULL, NULL) != TRUE) {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                      fname, SIP_CLI_ERR_LOOP_DETECT);
                }
            } else {
                /* Send 400 error */
                if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                            SIP_CLI_ERR_BAD_REQ_PHRASE,
                                            SIP_WARN_MISC,
                                            NULL, NULL) != TRUE) {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                      fname, SIP_CLI_ERR_BAD_REQ);
                }
            }
        } else { //is response
            // This may be a response for a request generated via sub/not i/f
            scbp = find_scb_by_callid(pCallID, &scb_index);
            if (scbp != NULL) {
                CCSIP_DEBUG_TASK(DEB_F_PREFIX"Recv Refer Response.",
                    DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
                (void) subsmanager_handle_ev_sip_response(pSipMessage);
            } else {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sip_sm_determine_ccb(): "
                                  "bad response. Dropping message.\n", fname);
            }
        }
        cpr_free(sipCseq);
        free_sip_message(pSipMessage);
        return;
    }

    if (ccb) {
        sip_sm_event.ccb = ccb;
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Line filter: Call ID match:  Destination line = "
                         "<%d/%d>.\n", DEB_F_PREFIX_ARGS(SIP_ID, fname), ccb->index, ccb->dn_line);

    } else {

        boolean is_previous_call_id = FALSE;
        line_t  previous_call_index = 0;

        /*
         * Unsolicited options processing
         */
        if ((is_request) && (method == sipMethodOptions)) {
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"recv SIP OPTIONS (outside of dialog) message.",
                DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname)); //
            /*
             * Send an options request to the gsm for this out of call
             * options request. pSipMessage will be freed on return.
             */
            sip_cc_options(CC_NO_CALL_ID, CC_NO_LINE, pSipMessage);
            cpr_free(sipCseq);
            return;
        }

        /*
         * Unsolicited INFO processing
         */
        if ((is_request) && (method == sipMethodInfo)) {
            CCSIP_DEBUG_ERROR("%s: Error: recv out-of-dialog SIP INFO message.", fname); //
            /* Send 481 Call Leg/Transaction Does Not Exist */
            if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_CALLEG,
                                        SIP_CLI_ERR_CALLEG_PHRASE,
                                        0, NULL, NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                fname, SIP_CLI_ERR_NOT_ALLOWED);
            }
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;
        }


        /* + If this is a request and method is INVITE, obtain
         *   the next available line and forward the request to
         *   that state machine.  This is a new incoming call.
         * + Otherwise, this message is spurious -- reject it.
         *   Do not accept an incoming INVITE for a new call
         *   if in quiet mode
         */
        is_previous_call_id = sip_sm_is_previous_call_id(pCallID,
                                                         &previous_call_index);
        if ((method == sipMethodInvite) && (is_request)) {
            if (sip_mode_quiet) {
                if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_NOT_AVAIL,
                                            SIP_CLI_ERR_NOT_AVAIL_PHRASE, 0,
                                            NULL, NULL) != TRUE) {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                      fname, SIP_CLI_ERR_NOT_AVAIL);
                }
                cpr_free(sipCseq);
                free_sip_message(pSipMessage);
                return;
            }
            else if (cprGetDepth(gsm_msgq) > MAX_DEPTH_OF_GSM_MSGQ_TO_ALLOW_NEW_INCOMING_CALL) {
            	/*
            	 * CSCsz33584
            	 * if gsm_msgq depth is larger than MAX_DEPTH_OF_GSM_MSGQ_TO_ALLOW_NEW_INCOMING_CALL,
            	 * it's risky to accept new incoming call.
            	 * just ignore the INVITE message to save CPU time so that the messages in gsm_msgq can be processed faster.
            	 */
            	CCSIP_DEBUG_ERROR(DEB_F_PREFIX"gsm msgq depth too large, drop incoming INVITEs!!!",
                                     DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
                cpr_free(sipCseq);
                free_sip_message(pSipMessage);
                return;
            }

            ccb = sip_sm_get_ccb_next_available(&line_index);
            if (!ccb) {
                /* All lines are busy.  Return 486 BUSY */
                if (platGetPhraseText(STR_INDEX_NO_FREE_LINES,
                                             (char *)tmp_str,
                                             STATUS_LINE_MAX_LEN - 1) == CPR_SUCCESS) {
                    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Line filter: Call ID match not "
                                     "found: INVITE: %s\n Sending 486 BUSY\n",
                                     DEB_F_PREFIX_ARGS(SIP_ID, fname), tmp_str);
                }
                if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BUSY_HERE,
                                            SIP_CLI_ERR_BUSY_HERE_PHRASE, 0,
                                            NULL, NULL) != TRUE) {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                      fname, SIP_CLI_ERR_BUSY_HERE);
                }
                cpr_free(sipCseq);
                free_sip_message(pSipMessage);
                return;
            }
            sip_sm_event.ccb = ccb;

            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Line filter: Call ID match not "
                             "found: INVITE: free ccb index = %d.\n",
                             DEB_F_PREFIX_ARGS(SIP_ID, fname), line_index);

        } else if (is_previous_call_id && (method != sipMethodAck) &&
                   is_request) {
            /*
             * Detect whether the message is a non-ACK request addressing the
             * previous call. If so, reTx the stored response.
             */
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Line filter: Previous Call ID.",
                DEB_F_PREFIX_ARGS(SIP_ID, fname));
            if (SipRelDevEnabled) {
                if (SIPTaskRetransmitPreviousResponse(pSipMessage, fname,
                            pCallID, sipCseq, 0, TRUE) == SIP_OK) {
                    cpr_free(sipCseq);
                    free_sip_message(pSipMessage);
                    return;
                }
            } else {
                CCSIP_DEBUG_TASK(DEB_F_PREFIX"Line filter: Previous Call ID. "
                                 "Reliable Delivery is OFF.\n", DEB_F_PREFIX_ARGS(SIP_ID, fname));
            }

            if (method == sipMethodBye) {
                (void) sipSPISendErrorResponse(pSipMessage, 200,
                                               SIP_SUCCESS_SETUP_PHRASE,
                                               0, NULL, NULL);
            } else {
                (void) sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_CALLEG,
                                               SIP_CLI_ERR_CALLEG_PHRASE,
                                               0, NULL, NULL);
            }

            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;
        } else if (is_previous_call_id && (method == sipMethodAck) &&
                   is_request) {
            /*
             * Detect whether the message is an ACK addressing the previous
             * call. If so, stop any outstanding reTx timers.
             */
            if (SipRelDevEnabled) {
                CCSIP_DEBUG_TASK(DEB_F_PREFIX"Stopping any outstanding "
                                 "reTx timers...\n", DEB_F_PREFIX_ARGS(SIP_TIMER, fname));
                sip_sm_check_retx_timers(sip_sm_get_ccb_by_index(previous_call_index),
                                         pSipMessage);
            }
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Not forwarding response to SIP SM.",
                DEB_F_PREFIX_ARGS(SIP_FWD, fname));
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;
        } else if (is_previous_call_id && (!is_request)) {
            /*
             * Detect whether the message is a response addressing the previous
             * call. If the request is 200 OK, stop any outstanding reTx timers.
             * This will prevent extraneous reTx of BYE/CANCEL messages at the
             * the end of the call.
             */
            if (SipRelDevEnabled) {
                CCSIP_DEBUG_TASK(DEB_F_PREFIX"Stopping any outstanding "
                                 "reTx timers...\n", DEB_F_PREFIX_ARGS(SIP_TIMER, fname));
                sip_sm_check_retx_timers(sip_sm_get_ccb_by_index(previous_call_index),
                                         pSipMessage);
                // Check for stored responses
                if (SIPTaskRetransmitPreviousResponse(pSipMessage, fname,
                                                      pCallID, sipCseq,
                                                      response_code, FALSE) == SIP_OK) {
                    cpr_free(sipCseq);
                    free_sip_message(pSipMessage);
                    return;
                }
            }
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Not forwarding response to SIP SM.",
                DEB_F_PREFIX_ARGS(SIP_FWD, fname));

            if (method == sipMethodBye) {
                SIPTaskProcessSIPPreviousCallByeResponse(pSipMessage,
                                                         response_code,
                                                         previous_call_index);
            }
            if (method == sipMethodInvite) {
                SIPTaskProcessSIPPreviousCallInviteResponse(pSipMessage,
                                                            response_code,
                                                            previous_call_index);
            }
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;

        } else {
            if (method == sipMethodRefer) {
                if (is_request) {
                        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Received OOD Refer.",
                            DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
                        if (subsmanager_handle_ev_sip_subscribe(pSipMessage, sipMethodRefer, FALSE) != SIP_ERROR) {
                            // Successfully handled
                            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Successfully handled OOD Refer.",
                                DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
                        } else {
                            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Not able to handle OOD Refer.",
                                DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
                        }
                } else {
                    // This is a response to an OOD REFER generated via sub/not
                    // interface
                    scbp = find_scb_by_callid(pCallID, &scb_index);
                    if (scbp != NULL) {
                        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Recv Refer Response",
                            DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
                        (void) subsmanager_handle_ev_sip_response(pSipMessage);
                    }
                }
            } else {
                if (SipRelDevEnabled) {
                    // Check for stored responses
                    if (SIPTaskRetransmitPreviousResponse(pSipMessage, fname,
                                                          pCallID, sipCseq,
                                                          response_code,
                                                          FALSE) == SIP_OK) {
                        cpr_free(sipCseq);
                        free_sip_message(pSipMessage);
                        return;
                    }
                }
                CCSIP_DEBUG_TASK(DEB_F_PREFIX"Line filter: Call ID match not "
                                 "found: Rejecting.\n", DEB_F_PREFIX_ARGS(SIP_ID, fname));
                if (is_request && (method != sipMethodAck)) {
                    /* Send 481 error */
                    if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_CALLEG,
                                                SIP_CLI_ERR_CALLEG_PHRASE,
                                                0, NULL, NULL) != TRUE) {
                        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                          fname, SIP_CLI_ERR_CALLEG);
                    }
                }
            }
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;
        }
    }

    if (is_request) {
        /*
         * Process request
         */
        requestStatus = sipSPICheckRequest(ccb, pSipMessage);
        switch (requestStatus) {
        case SIP_MESSAGING_OK:
            break;

        case SIP_MESSAGING_NEW_CALLID:
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sipSPICheckRequest() returned "
                              "SIP_MESSAGING_NEW_CALLID.\n", fname);
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;

        case SIP_CLI_ERR_FORBIDDEN:
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sipSPICheckRequest() returned "
                              "SIP_CLI_ERR_FORBIDDEN.\n", fname);
            if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_FORBIDDEN,
                                        SIP_CLI_ERR_FORBIDDEN_PHRASE,
                                        0, NULL, NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_CLI_ERR_FORBIDDEN);
            }

            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;

        case SIP_MESSAGING_DUPLICATE:
            if (method == sipMethodInvite) {
                char test_mac[MAC_ADDR_STR_LENGTH];
                uint8_t mac_addr[MAC_ADDRESS_LENGTH];

                platform_get_wired_mac_address(mac_addr);

                /*
                 * See if the call id has our mac address in it.
                 * If so, then we called ourselves. Report back busy.
                 */
                snprintf(test_mac, MAC_ADDR_STR_LENGTH, "%.4x%.4x-%.4x",
                         mac_addr[0] * 256 + mac_addr[1],
                         mac_addr[2] * 256 + mac_addr[3],
                         mac_addr[4] * 256 + mac_addr[5]);
                if ((ccb->state == SIP_STATE_SENT_INVITE) &&
                    (strncmp(test_mac, ccb->sipCallID, 13) == 0)) {
                    get_sip_error_string(errortext, SIP_CLI_ERR_BUSY_HERE);
                    (void) sipSPISendErrorResponse(pSipMessage,
                                                   SIP_CLI_ERR_BUSY_HERE,
                                                   errortext, 0, NULL, NULL);
                }
            }
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;

        case SIP_SERV_ERR_INTERNAL:
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sipSPICheckRequest() returned "
                              "SIP_SERV_ERR_INTERNAL.\n", fname);
            if (sipSPISendErrorResponse(pSipMessage, SIP_SERV_ERR_INTERNAL,
                                        SIP_SERV_ERR_INTERNAL_PHRASE,
                                        0, NULL, NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_SERV_ERR_INTERNAL);
            }
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;

        case SIP_CLI_ERR_BAD_REQ:
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sipSPICheckRequest() returned "
                              "SIP_CLI_ERR_BAD_REQ.\n", fname);
            if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                        SIP_CLI_ERR_BAD_REQ_PHRASE,
                                        0, NULL, NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_CLI_ERR_BAD_REQ);
            }
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;

        case SIP_MESSAGING_ERROR_UNSUPPORTED_MEDIA:
        default:
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sipSPICheckRequest() "
                              "returned error.\n", fname);
            /* Send error response */
            if (method != sipMethodAck) {
                if (requestStatus == SIP_MESSAGING_ERROR_UNSUPPORTED_MEDIA) {
                    requestStatus = SIP_CLI_ERR_MEDIA;
                } else if (requestStatus == SIP_MESSAGING_ENDPOINT_NOT_FOUND) {
                    requestStatus = SIP_CLI_ERR_NOT_FOUND;
                } else if (requestStatus == SIP_MESSAGING_NOT_ACCEPTABLE) {
                    requestStatus = SIP_CLI_ERR_NOT_ACCEPT;
                } else if (requestStatus != SIP_CLI_ERR_CALLEG) {
                    requestStatus = 400;
                }
                get_sip_error_string(errortext, requestStatus);
                if (sipSPISendErrorResponse(pSipMessage, (uint16_t)requestStatus,
                                            errortext, 0, NULL, NULL) != TRUE) {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                      fname, SIP_CLI_ERR_BAD_REQ);
                }
                if (method == sipMethodInvite) {
                    ccb->wait_for_ack = TRUE;
                }
            }
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;
        }

        switch (method) {
        case sipMethodInvite:
            CCSIP_DEBUG_TASK(get_debug_string(DEBUG_SIP_MSG_RECV),
                             fname, SIP_METHOD_INVITE);
            sip_sm_event.type = E_SIP_INVITE;
            break;

        case sipMethodAck:
            CCSIP_DEBUG_TASK(get_debug_string(DEBUG_SIP_MSG_RECV),
                             fname, SIP_METHOD_ACK);
            sip_sm_check_retx_timers(ccb, pSipMessage);
            sip_sm_event.type = E_SIP_ACK;
            break;

        case sipMethodBye:
            CCSIP_DEBUG_TASK(get_debug_string(DEBUG_SIP_MSG_RECV),
                             fname, SIP_METHOD_BYE);
            sip_sm_event.type = E_SIP_BYE;
            break;

        case sipMethodCancel:
            CCSIP_DEBUG_TASK(get_debug_string(DEBUG_SIP_MSG_RECV),
                             fname, SIP_METHOD_CANCEL);
            sip_sm_event.type = E_SIP_CANCEL;
            break;

        case sipMethodSubscribe:
            CCSIP_DEBUG_TASK(get_debug_string(DEBUG_SIP_MSG_RECV),
                             fname, SIP_METHOD_SUBSCRIBE);
            sip_sm_event.type = E_SIP_SUBSCRIBE;
            break;

        case sipMethodNotify:
            CCSIP_DEBUG_TASK(get_debug_string(DEBUG_SIP_MSG_RECV),
                             fname, SIP_METHOD_NOTIFY);
            sip_sm_event.type = E_SIP_NOTIFY;
            break;

        case sipMethodRefer:
            CCSIP_DEBUG_TASK(get_debug_string(DEBUG_SIP_MSG_RECV),
                             fname, SIP_METHOD_REFER);
            sip_sm_event.type = E_SIP_REFER;
            break;
        case sipMethodOptions:
            CCSIP_DEBUG_TASK(get_debug_string(DEBUG_SIP_MSG_RECV),
                             fname, SIP_METHOD_OPTIONS);
            sip_sm_event.type = E_SIP_OPTIONS;
            break;
        case sipMethodUpdate:
            CCSIP_DEBUG_TASK(get_debug_string(DEBUG_SIP_MSG_RECV),
                             fname, SIP_METHOD_UPDATE);
            sip_sm_event.type = E_SIP_UPDATE;
            break;
        case sipMethodInfo:
            CCSIP_DEBUG_TASK(get_debug_string(DEBUG_SIP_MSG_RECV),
                             fname, SIP_METHOD_INFO);
            (void) ccsip_handle_info_package(ccb, pSipMessage);
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;
            break;
        default:
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Received unknown SIP request message.",
                             DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
            /* The message must be deallocated here */
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;
        }
    } else {
        int responseStatus = SIP_MESSAGING_ERROR;

        /*
         * Process response
         */
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Received SIP response.",
            DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
        responseStatus = sipSPICheckResponse(ccb, pSipMessage);
        if (responseStatus != SIP_MESSAGING_OK) {
            if (responseStatus == SIP_MESSAGING_DUPLICATE) {
                CCSIP_DEBUG_TASK(DEB_F_PREFIX"Duplicate response detected. "
                                 "Discarding...\n", DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
            } else if (responseStatus == SIP_MESSAGING_ERROR_STALE_RESP) {
                CCSIP_DEBUG_TASK(DEB_F_PREFIX"Stale response detected. "
                                 "Discarding...\n", DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
            } else if (responseStatus == SIP_MESSAGING_ERROR_NO_TRX) {
                // This may be a response for a request generated via sub/not i/f
                scbp = find_scb_by_callid(pCallID, &scb_index);
                if (scbp != NULL) {
                    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Recv Refer Response.",
                        DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
                    (void) subsmanager_handle_ev_sip_response(pSipMessage);
                }
            } else {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sipSPICheckResponse() "
                                  "returned error. Discarding response\n",
                                  fname);
            }
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;
        }

        /* Response is ok.  Cancel the outstanding reTx timer if any */
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Stopping any outstanding reTx "
                         "timers...\n", DEB_F_PREFIX_ARGS(SIP_TIMER, fname));
        sip_sm_check_retx_timers(ccb, pSipMessage);

        /* got a response, re-set re-transmit flag */
        ccb->retx_flag = FALSE;
        ccb->last_recvd_response_code = response_code;
        /*
         * TEL_CCB_START equates to zero and line_t is an unsigned type,
         * so just check the end condition
         */
        if (ccb->index <= TEL_CCB_END) {
            gCallHistory[ccb->index].last_rspcode_rcvd = code_class;
        }

        switch (code_class) {
        case codeClass1xx:
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Recv 1xx message.",
                DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
            sip_sm_event.type = E_SIP_1xx;
            break;

        case codeClass2xx:
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Recv 2xx message.",
                DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
            sip_sm_event.type = E_SIP_2xx;
            break;

        case codeClass3xx:
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Recv 3xx message.",
                DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
            sip_sm_event.type = E_SIP_3xx;
            break;

        case codeClass4xx:
        case codeClass5xx:
        case codeClass6xx:
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Recv 4xx/5xx/6xx message.",
                DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname));
            sip_sm_event.type = E_SIP_FAILURE_RESPONSE;
            switch (response_code) {
            case SIP_CLI_ERR_UNAUTH:
            case SIP_CLI_ERR_PROXY_REQD:
                if (sipCseq->method == sipMethodAck) {
                    sipSPISendFailureResponseAck(ccb, pSipMessage, FALSE, 0);
                    cpr_free(sipCseq);
                    free_sip_message(pSipMessage);
                    return;
                }
                break;

            default:
                break;
            }
            break;

        default:
            /* unknown response, keep re-transmitting */
            ccb->retx_flag = TRUE;
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unknown response class.", fname);
            cpr_free(sipCseq);
            free_sip_message(pSipMessage);
            return;
        }
        // Change the event type for the UPDATE method since there is only one
        // handler for this
        if (method == sipMethodUpdate) {
            sip_sm_event.type = E_SIP_UPDATE_RESPONSE;
         }
    }

    /*
     * Send event to the SIP SM or the SIP REGISTRATION SM
     */
    if (sip_sm_event.ccb->type == SIP_REG_CCB) {
        sip_sm_event.type = (sipSMEventType_t)
            ccsip_register_sip2sipreg_event(sip_sm_event.type);

        if ((!is_request) && ((code_class == codeClass5xx) ||
            (code_class == codeClass6xx))) {
            sip_sm_event.type = (sipSMEventType_t) E_SIP_REG_FAILURE_RESPONSE;
        }

        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Register response does not have a body %d",
        DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname),
        (pSipMessage->mesg_body[0].msgBody == NULL) ? -1: pSipMessage->mesg_body[0].msgContentTypeValue);

        if (sip_sm_event.type != (int) E_SIP_REG_NONE) {
            if (sip_reg_sm_process_event(&sip_sm_event) < 0) {
                CCSIP_DEBUG_ERROR(get_debug_string(REG_SM_PROCESS_EVENT_ERROR), fname, sip_sm_event.type);
                if (is_request && (method != sipMethodAck)) {
                    /* Send 500 error */
                    if (sipSPISendErrorResponse(pSipMessage, 500,
                                                SIP_SERV_ERR_INTERNAL_PHRASE, 0,
                                                NULL, NULL) != TRUE) {
                        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                          fname, SIP_SERV_ERR_INTERNAL);
                    }
                }
                cpr_free(sipCseq);
                free_sip_message(pSipMessage);
                return;
            } else {
                cpr_free(sipCseq);
                return;
            }
        }
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Ignoring non-register event= %d",
                         DEB_F_PREFIX_ARGS(SIP_EVT, fname), sip_sm_event.type);
        cpr_free(sipCseq);
        free_sip_message(pSipMessage);
        return;
    }

    if (sip_sm_process_event(&sip_sm_event) < 0) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sip_sm_process_event() returned "
                          "error.\n", fname);
        if (is_request && (method != sipMethodAck)) {
            /* Send 500 error */
            if (sipSPISendErrorResponse(pSipMessage, 500,
                                        SIP_SERV_ERR_INTERNAL_PHRASE, 0,
                                        NULL, NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_SERV_ERR_INTERNAL);
            }
        }
        cpr_free(sipCseq);
        free_sip_message(pSipMessage);
        return;
    }

    cpr_free(sipCseq);
}

/**
 *
 * SIPTaskProcessConfigChangeNotify
 *
 * ???
 *
 * Parameters:   ???
 *
 * Return Value: zero(0)
 *
 */
int
SIPTaskProcessConfigChangeNotify (int32_t notify_type)
{
    static const char *fname = "SIPTaskProcessConfigChangeNotify";
    int retval = 0;

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Notify received type=%d",
        DEB_F_PREFIX_ARGS(SIP_NOTIFY, fname), notify_type);

    if (notify_type & AA_RELOAD) {
        if ((PHNGetState() == STATE_CONNECTED) ||
            (PHNGetState() == STATE_UNPROVISIONED)) {
            /*
             * If the phone state isn't in STATE_CONNECTED then this change
             * notify is being called because of bootup and not just a SIP
             * menu configuration hang.  We only want to handle updates
             * to the number of lines if the phone is already connected,
             * the other case, booting, is handled by the boot code
             */
            (void) sipTransportInit();

            /* need to unregister the phone */
            ccsip_register_cancel(FALSE, TRUE);
            ccsip_register_reset_proxy();
            (void) sip_platform_ui_restart();
        } else {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"PHNGetState() is not in STATE_CONNECTED, "
                              "bypassing restart\n", fname);
        }
    } else if (notify_type & AA_REGISTER) {
        ccsip_register_commit();
    } else if (notify_type & AA_BU_REG) {
        (void) sipTransportInit();
        ccsip_backup_register_commit();
    }

    return (retval);
}



/**
 *
 * SIPTaskProcessSIPNotify
 *
 * The function processes the unsolicited NOTIFY.
 *
 * Parameters:  pSipMessage - pointer to sipMessage_t.
 *
 * Return Value: SIP_OK, SIP_ERROR, SIP_DEFER.
 *
 * Note: The parameter pSipMessage is expected by the caller to be
 *       preserved i.e. it can not be freed by this
 *       function or the functions called by this function.
 */
static int
SIPTaskProcessSIPNotify (sipMessage_t *pSipMessage)
{
    static const char *fname = "SIPTaskProcessSIPNotify";
    sipReqLine_t   *requestURI = NULL;
    char           *pRequestURIUserStr = NULL;
    boolean         request_uri_error = TRUE;
    line_t          i = 0;
    line_t          dn_line = 0;
    const char     *event = NULL;
    sipLocation_t  *uri_loc = NULL;
    char            line_name[MAX_LINE_NAME_SIZE];
    char            line_contact[MAX_LINE_CONTACT_SIZE];
    const char     *to = NULL;
    sipLocation_t  *to_loc = NULL;
    sipUrl_t       *sipToUrl = NULL;
    boolean         to_header_error = TRUE;
    char           *pUser = NULL;


    /*
     * Parse the Event header
     */
    event = sippmh_get_header_val(pSipMessage, SIP_HEADER_EVENT,
                                  SIP_C_HEADER_EVENT);

    if (event == NULL) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Missing Event header", fname);
        (void) sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                       SIP_CLI_ERR_BAD_REQ_PHRASE,
                                       0, NULL, NULL);
        return SIP_OK;
    }

    /*
     * Find the destination DN
     */
    requestURI = sippmh_get_request_line(pSipMessage);
    if (requestURI) {
        if (requestURI->url) {
            uri_loc = sippmh_parse_from_or_to(requestURI->url, TRUE);
            if (uri_loc) {
                if (uri_loc->genUrl->schema == URL_TYPE_SIP) {
                    if (uri_loc->genUrl->u.sipUrl->user) {
                        pRequestURIUserStr = uri_loc->genUrl->u.sipUrl->user;
                        request_uri_error = FALSE;
                    } else {
                        sippmh_free_location(uri_loc);
                        SIPPMH_FREE_REQUEST_LINE(requestURI);
                    }
                } else {
                    sippmh_free_location(uri_loc);
                    SIPPMH_FREE_REQUEST_LINE(requestURI);
                }
            } else {
                SIPPMH_FREE_REQUEST_LINE(requestURI);
            }
        } else {
            SIPPMH_FREE_REQUEST_LINE(requestURI);
        }
    }

    if (request_uri_error) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"NOTIFY has error in Req-URI!", fname);
        (void) sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                       SIP_CLI_ERR_BAD_REQ_PHRASE,
                                       SIP_WARN_MISC,
                                       SIP_CLI_ERR_BAD_REQ_REQLINE_ERROR, NULL);
        return SIP_OK;
    }

    if (cpr_strncasecmp(event, "refer", 5) == 0) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"NOTIFY: refer", DEB_F_PREFIX_ARGS(SIP_NOTIFY, fname));
        SIPTaskProcessSIPNotifyRefer(pSipMessage);
        sippmh_free_location(uri_loc);
        SIPPMH_FREE_REQUEST_LINE(requestURI);
        return SIP_OK;
    }

    for (i = 1; i <= 1; i++) {
        if (sip_config_check_line(i)) {
            config_get_string((CFGID_LINE_NAME + i - 1), line_name, sizeof(line_name));
            config_get_string((CFGID_LINE_CONTACT + i - 1), line_contact, sizeof(line_contact));
            if ((sippmh_cmpURLStrings(pRequestURIUserStr, line_name, TRUE) == 0) ||
                (sippmh_cmpURLStrings(pRequestURIUserStr, line_contact, TRUE) == 0)) {
                dn_line = i;
                break;
            }
        }
    }

    if (dn_line == 0) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"NOTIFY has unknown user in Req-URI!",
                          fname);
        (void) sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_NOT_FOUND,
                                       SIP_CLI_ERR_NOT_FOUND_PHRASE,
                                       0, NULL, NULL);
        sippmh_free_location(uri_loc);
        SIPPMH_FREE_REQUEST_LINE(requestURI);
        return SIP_OK;
    }

    sippmh_free_location(uri_loc);
    SIPPMH_FREE_REQUEST_LINE(requestURI);

    /*
     * Sanity check To header by parsing the header. Note it is not used, just sanitized.
     */
    to = sippmh_get_cached_header_val(pSipMessage, TO);
    if (to) {
        to_loc = sippmh_parse_from_or_to((char *)to, TRUE);
        if (to_loc) {
            if (to_loc->genUrl->schema == URL_TYPE_SIP) {
                sipToUrl = to_loc->genUrl->u.sipUrl;
                if (sipToUrl) {
                    if (sipToUrl->user) {
                        pUser = sippmh_parse_user(sipToUrl->user);
                        if (pUser) {
                            if (pUser[0] != '\0') {
                                // To header is good.
                                to_header_error = FALSE;
                            }
                            cpr_free(pUser);
                        }
                    }
                }
                sippmh_free_location(to_loc);
            } else {
                sippmh_free_location(to_loc);
            }
        }
    }

    if (to_header_error) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"NOTIFY has error in To header", fname);
        (void) sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                       SIP_CLI_ERR_BAD_REQ_PHRASE,
                                       SIP_WARN_MISC,
                                       SIP_CLI_ERR_BAD_REQ_ToURL_ERROR, NULL);
        return SIP_OK;
    }

    /*
     * Request-URI and To header check out. Check Event to determine which
     * Notify function to invoke.
     */
    if ((cpr_strcasecmp(event, "message-summary") == 0) ||
        (cpr_strcasecmp(event, "simple-message-summary") == 0)) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"NOTIFY: MWI", DEB_F_PREFIX_ARGS(SIP_NOTIFY, fname));
        if (SIPTaskProcessSIPNotifyMWI(pSipMessage, dn_line) != 0) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Bad MWI NOTIFY!", fname);
            (void) sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                           SIP_CLI_ERR_BAD_REQ_PHRASE,
                                           SIP_WARN_MISC,
                                           "Bad MWI NOTIFY", NULL);
            return SIP_OK;
        }
    } else if (cpr_strcasecmp(event, "check-sync") == 0) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"NOTIFY: check-sync", DEB_F_PREFIX_ARGS(SIP_NOTIFY, fname));
        SIPTaskProcessSIPNotifyCheckSync(pSipMessage);
    } else if (cpr_strcasecmp(event, "service-control") == 0) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"NOTIFY: service-control", DEB_F_PREFIX_ARGS(SIP_NOTIFY, fname));
        return SIPTaskProcessSIPNotifyServiceControl(pSipMessage);
    } else if (cpr_strcasecmp(event, SIP_EVENT_DIALOG) == 0) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"NOTIFY: %s", DEB_F_PREFIX_ARGS(SIP_NOTIFY, fname), SIP_EVENT_DIALOG);
        return (2);
    } else if (cpr_strcasecmp(event, SIP_EVENT_CONFIG) == 0) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"NOTIFY: %s", DEB_F_PREFIX_ARGS(SIP_NOTIFY, fname), SIP_EVENT_CONFIG);
        return (2);
    } else if (cpr_strcasecmp(event, SIP_EVENT_KPML) == 0) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"NOTIFY: %s", DEB_F_PREFIX_ARGS(SIP_NOTIFY, fname), SIP_EVENT_KPML);
        return (2);
    } else if (cpr_strcasecmp(event, SIP_EVENT_PRESENCE) == 0) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"NOTIFY: %s", DEB_F_PREFIX_ARGS(SIP_NOTIFY, fname), SIP_EVENT_PRESENCE);
        return (2);
    } else {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unrecognized Event header", fname);
        (void) sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                       SIP_CLI_ERR_BAD_REQ_PHRASE,
                                       0, NULL, NULL);
    }

    return SIP_OK;
}

/**
 *
 * SIPTaskProcessSIPNotifyMWI
 *
 * ???
 *
 * Parameters:   ???
 *
 * Return Value: SIP_OK or SIP_ERROR
 *
 */
static int
SIPTaskProcessSIPNotifyMWI (sipMessage_t* pSipMessage, line_t dn_line)
{
    sipMessageSummary_t mesgSummary;

    if (!pSipMessage->mesg_body[0].msgBody ||
        (pSipMessage->mesg_body[0].msgContentTypeValue != SIP_CONTENT_TYPE_MWI_VALUE &&
        pSipMessage->mesg_body[0].msgContentTypeValue != SIP_CONTENT_TYPE_TEXT_PLAIN_VALUE)) {
        return SIP_ERROR;
    }

    memset(&mesgSummary, 0, sizeof(sipMessageSummary_t));

    if (sippmh_parse_message_summary(pSipMessage, &mesgSummary) < 0) {
        return SIP_ERROR;
    }

    sip_cc_mwi(CC_NO_CALL_ID, dn_line, mesgSummary.mesg_waiting_on, mesgSummary.type,
               mesgSummary.newCount, mesgSummary.oldCount, mesgSummary.hpNewCount, mesgSummary.hpOldCount);

    /*
     * Send 200 OK back
     */
    (void) sipSPISendErrorResponse(pSipMessage, 200, SIP_SUCCESS_SETUP_PHRASE,
                            0, NULL, NULL);

    return SIP_OK;
}
/**
 *
 * SIPTaskProcessSIPNotifyRefer
 *
 * The function processes NOTIFY refer.
 *
 * Parameters:   pSipMessage - pointer to the sipMessage_t.
 *
 * Return Value: None
 *
 */
static void
SIPTaskProcessSIPNotifyRefer (sipMessage_t *pSipMessage)
{
    static const char *fname = "SIPTaskProcessSIPNotifyRefer";
    ccsipCCB_t *ccb = NULL;
    const char *pCallID = NULL;
    sipSCB_t   *scbp = NULL;
    int         scb_index;

    pCallID = sippmh_get_cached_header_val(pSipMessage, CALLID);
    if (!pCallID) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Cannot obtain SIP Call ID.", fname);
        return;
    }

    /*
     * Determine which line this SIP message is for.
     */
    ccb = sip_sm_get_ccb_by_callid(pCallID);
    if (ccb) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Line filter: Call ID match:  Destination line = "
                         "<%d/%d>.\n", DEB_F_PREFIX_ARGS(SIP_ID, fname), ccb->index, ccb->dn_line);
    } else {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"ccb is NULL", fname);
        // Check if this should be sent to the SM by looking up its callid
        // If found, the we return from here
        scbp = find_scb_by_callid(pCallID, &scb_index);
        if (scbp != NULL) {
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Recv Notify.", DEB_F_PREFIX_ARGS(SIP_NOTIFY, fname));
            (void) subsmanager_handle_ev_sip_subscribe_notify(pSipMessage);
            return;
        }
    }
    (void) sipSPISendErrorResponse(pSipMessage, 200, SIP_SUCCESS_SETUP_PHRASE,
                                   0, NULL, NULL);

    if (NULL != ccb) {
        cc_feature_data_t data;

        /* for some reason pingtel decided they were going to send
         * Notify messages with call progression in the body. Let's
         * just send the 200 back for these and not process them.
         * We should only process a Notify with a final response
         * in the body (ie. 200, 3xx, 4xx, 5xx, 6xx).
         */
        if (pSipMessage->mesg_body[0].msgBody == NULL) {
            data.notify.cause = CC_CAUSE_OK;

            if (fsmxfr_get_xcb_by_call_id(ccb->gsm_id) &&
                (fsmxfr_get_xfr_type(ccb->gsm_id) == FSMXFR_TYPE_BLND_XFR)) {
                data.notify.cause = CC_CAUSE_ERROR;
                data.notify.method = CC_XFER_METHOD_REFER;
            }
        } else if ((strstr(pSipMessage->mesg_body[0].msgBody, "100")) ||
                   (strstr(pSipMessage->mesg_body[0].msgBody, "180")) ||
                   (strstr(pSipMessage->mesg_body[0].msgBody, "181")) ||
                   (strstr(pSipMessage->mesg_body[0].msgBody, "182")) ||
                   (strstr(pSipMessage->mesg_body[0].msgBody, "183"))) {
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Ignoring Notify w/Progression",
                              DEB_F_PREFIX_ARGS(SIP_NOTIFY, fname));
            return;
        } else if (strstr(pSipMessage->mesg_body[0].msgBody, "200")) {
            data.notify.cause = CC_CAUSE_OK;
            data.notify.method = CC_XFER_METHOD_REFER;
        } else {
            data.notify.cause = CC_CAUSE_ERROR;
            data.notify.method = CC_XFER_METHOD_REFER;
        }
        data.notify.subscription = CC_SUBSCRIPTIONS_XFER;
        data.notify.blind_xferror_gsm_id = 0;
        sip_cc_feature(ccb->gsm_id, ccb->dn_line, CC_FEATURE_NOTIFY,
                       (void *)&data);
    }
}


/**
 *
 * SIPTaskProcessSIPNotifyCheckSync
 *
 * ???
 *
 * Parameters:   pSipMessage -
 *
 * Return Value: None
 *
 */
static void
SIPTaskProcessSIPNotifyCheckSync (sipMessage_t *pSipMessage)
{
    /*
     * Send 200 OK back
     */
    (void) sipSPISendErrorResponse(pSipMessage, 200, SIP_SUCCESS_SETUP_PHRASE,
                                   0, NULL, NULL);
}

/**
 *
 * SIPTaskProcessSIPNotifyServiceControl
 *
 * Handles an incoming unsolicited NOTIFY service control message
 *
 * Parameters:   Incoming pSipMessage
 *
 * Return Value: SIP_OK if request processed.
 *               SIP_DEFER if request is deferred for processing
 *               by ccsip_core.c.
 *
 */
static int
SIPTaskProcessSIPNotifyServiceControl (sipMessage_t *pSipMessage)
{
    const char *fname = "SIPTaskProcessSIPNotifyServiceControl";
    sipServiceControl_t *scp;
    int rc = SIP_OK;

    scp = ccsip_get_notify_service_control(pSipMessage);

    if (scp != NULL) {
        // The platform code should not alter scp or its fields
        if (scp->action == SERVICE_CONTROL_ACTION_CALL_PRESERVATION) {
            rc = SIP_DEFER;
        } else {
            if (sipSPISendErrorResponse(pSipMessage, 200,
                                        SIP_SUCCESS_SETUP_PHRASE,
                                        0, NULL, NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_SUCCESS_SETUP);
            }

            // Hand over the event to platform
            sip_platform_handle_service_control_notify(scp);
        }
        sippmh_free_service_control_info(scp);
    }

    return rc;
}

/**
 *
 * SIPTaskProcessSIPPreviousCallByeResponse
 *
 * ???
 *
 * Parameters:   pResponse           -
 *               response_code       -
 *               previous_call_index -
 *
 * Return Value: None
 *
 */
static void
SIPTaskProcessSIPPreviousCallByeResponse (sipMessage_t *pResponse,
                                          int response_code,
                                          line_t previous_call_index)
{
    static const char *fname = "SIPTaskProcessSIPPreviousCallByeResponse";
    uint32_t        responseCSeqNumber = 0;
    sipMethod_t     responseCSeqMethod = sipMethodInvalid;
    boolean         bad_authentication = FALSE;
    credentials_t   credentials;
    sipAuthenticate_t authen;
    const char     *authenticate = NULL;
    int             nc_count = 0;

    memset(&authen, 0, sizeof(authen)); // Initialize

    switch (response_code) {
    case SIP_CLI_ERR_UNAUTH:
    case SIP_CLI_ERR_PROXY_REQD:
        /* Check CSeq */
        if (sipGetMessageCSeq(pResponse, &responseCSeqNumber,
                    &responseCSeqMethod) < 0) {
            return;
        }
        if (responseCSeqNumber !=
                gCallHistory[previous_call_index].last_bye_cseq_number) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"CSeq# mismatch: BYE CSeq=%d, "
                              "%d CSeq:%d\n", fname,
                              gCallHistory[previous_call_index].last_bye_cseq_number,
                              response_code, responseCSeqNumber);
            return;
        }

        /* Obtain credentials */
        cred_get_line_credentials(gCallHistory[previous_call_index].dn_line,
                                  &credentials,
                                  sizeof(credentials.id),
                                  sizeof(credentials.pw));
        /* Compute Authentication information */
        authenticate = sippmh_get_header_val(pResponse,
                                             AUTH_HDR(response_code), NULL);
        if (authenticate != NULL) {
            sip_authen_t *sip_authen = NULL;

            sip_authen = sippmh_parse_authenticate(authenticate);
            if (sip_authen) {
                char *author_str = NULL;

                if (sipSPIGenerateAuthorizationResponse(sip_authen,
                                                        gCallHistory[previous_call_index].
                                                        last_route_request_uri,
                                                        SIP_METHOD_BYE,
                                                        credentials.id,
                                                        credentials.pw,
                                                        &author_str,
                                                        &nc_count, NULL)) {
                    if (author_str != NULL)
                    {
                        authen.authorization = (char *)
                            cpr_malloc(strlen(author_str) * sizeof(char) + 1);
                        if (authen.authorization != NULL) {
                            sstrncpy(authen.authorization, author_str,
                                     strlen(author_str) * sizeof(char) + 1);
                            authen.status_code = response_code;
                        } else {
                            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"malloc() failed "
                                              "for authen.authorization\n", fname);
                            bad_authentication = TRUE;
                        }
                        cpr_free(author_str);
                    } else {
                        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"author_str returned by sipSPIGenerateAuthorizationResponse"
                                          "is NULL", fname);
                        bad_authentication = TRUE;
                    }
                } else {
                    CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sipSPIGenerateAuthorizationResponse()"
                                      " returned null.\n", fname);
                    bad_authentication = TRUE;
                }
                sippmh_free_authen(sip_authen);
            } else {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sippmh_parse_authenticate() "
                                  "returned null.\n", fname);
                bad_authentication = TRUE;
            }
        } else {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sippmh_get_header_val(AUTH_HDR) "
                              "returned null.\n", fname);
            bad_authentication = TRUE;
        }

        if (bad_authentication) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Bad authentication header "
                              "in %d\n", fname, response_code);
            if (authen.authorization)
                cpr_free(authen.authorization);
            return;
        }

        /* Resend BYE with authorization */
        (void) sipSPISendByeAuth(pResponse,
                                 authen,
                                 &(gCallHistory[previous_call_index].last_bye_dest_ipaddr),
                                 gCallHistory[previous_call_index].last_bye_dest_port,
                                 ++gCallHistory[previous_call_index].last_bye_cseq_number,
                                 gCallHistory[previous_call_index].last_bye_also_string,
                                 gCallHistory[previous_call_index].last_route,
                                 gCallHistory[previous_call_index].last_route_request_uri,
                                 previous_call_index);
        if (authen.authorization) {
            cpr_free(authen.authorization);
        }
        break;

    default:
        break;
    }
}

/**
 *
 * SIPTaskProcessSIPPreviousCallInviteResponse
 *
 * ???
 *
 * Parameters:   pResponse           -
 *               response_code       -
 *               previous_call_index -
 *
 * Return Value: None
 *
 */
static void
SIPTaskProcessSIPPreviousCallInviteResponse (sipMessage_t *pResponse,
                                             int response_code,
                                             line_t previous_call_index)
{
    static const char *fname = "SIPTaskProcessSIPPreviousCallInviteResponse";
    uint32_t responseCSeqNumber = 0;
    sipMethod_t responseCSeqMethod = sipMethodInvalid;

    if ((response_code >= SIP_CLI_ERR_BAD_REQ) && (response_code < 700)) {
        /* Check CSeq */
        if (sipGetMessageCSeq(pResponse, &responseCSeqNumber,
                              &responseCSeqMethod) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                              fname, "Invalid CSeq");
            return;
        }
        if (responseCSeqNumber !=
                gCallHistory[previous_call_index].last_bye_cseq_number) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Last Bye CSeq=%d, Failure Code = %d, "
                              "CSeq:%d\n", fname,
                              gCallHistory[previous_call_index].last_bye_cseq_number,
                              response_code, responseCSeqNumber);
            return;
        }

        /* Send ACK */
        sipSPISendFailureResponseAck(NULL, pResponse, TRUE, previous_call_index);

    }
}

void
SIPTaskPostRestart (boolean restart)
{
    ccsip_restart_req *msg;
    static const char fname[] = "SIPTaskPostRestart";

    msg = (ccsip_restart_req *) SIPTaskGetBuffer(sizeof(ccsip_restart_req));
    if (msg == NULL) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"failed to allocate IPC msg ccip_restart_req", fname);
        return;
    }
    if (restart) {
        /* This is a restart request from the platform */
        msg->cmd = SIP_RESTART_REQ_RESTART;
    } else {
        /* This is a re-init request from the platform */
        msg->cmd = SIP_RESTART_REQ_REINIT;
    }
    /* send a restart message to the SIP Task */
    if (SIPTaskSendMsg(SIP_RESTART, (cprBuffer_t)msg,
                       sizeof(ccsip_restart_req), NULL) == CPR_FAILURE) {
        cpr_free(msg);
    }
    return;
}

static void
SIPTaskProcessRestart (ccsip_restart_cmd cmd)
{
    static const char fname[] = "SIPTaskProcessRestart";

    if (cmd == SIP_RESTART_REQ_RESTART) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Starting Restart Process",
            DEB_F_PREFIX_ARGS(SIP_TASK, fname));

        /* Restart SIP task */
        sip_restart();
    } else if (cmd == SIP_RESTART_REQ_REINIT) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Starting Re-init Process",
            DEB_F_PREFIX_ARGS(SIP_TASK, fname));

        // Re-initialize the various components
        SIPTaskReinitialize(FALSE);

      /* Clear the quite mode */
        sip_mode_quiet = FALSE;
    }
}

/*
 * If checkConfig is 0/FALSE, process as a config change without checking
 */
void
SIPTaskReinitialize (boolean checkConfig)
{
    static const char fname[] = "SIPTaskReinitialize";

    // Initialize all GSM modules
    cc_fail_fallback_gsm(CC_SRC_SIP, CC_RSP_COMPLETE, CC_REG_FAILOVER_RSP);

    // If the config has changed, or if the check is being bypassed, re-init reg-mgr
    if ( !checkConfig || sip_regmgr_check_config_change() ) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Config change detected: Restarting",
            DEB_F_PREFIX_ARGS(SIP_TASK, fname));
        sip_regmgr_process_config_change();
        return;
    } else {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"No config change detected",
            DEB_F_PREFIX_ARGS(SIP_TASK, fname));
    }
}

void
SIPTaskPostShutdown (int action, int reason, const char *reasonInfo)
{
    ccsip_shutdown_req_t *msg;
    static const char fname[] = "SIPTaskPostShutdown";

    msg = (ccsip_shutdown_req_t *) SIPTaskGetBuffer(sizeof(ccsip_shutdown_req_t));
    if (msg == NULL) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"failed to allocate IPC msg SIP_SHUTDOWN_REQ_SHUT", fname);
        return;
    }
    msg->cmd = SIP_SHUTDOWN_REQ_SHUT;
    msg->action = action;
    msg->reason = reason;
    if (reasonInfo) {
        sstrncpy(sipUnregisterReason, reasonInfo, MAX_SIP_REASON_LENGTH);
    }

    /* send a restart message to the SIP Task */
    if (SIPTaskSendMsg(SIP_SHUTDOWN, (cprBuffer_t)msg,
                       sizeof(ccsip_shutdown_req_t), NULL) == CPR_FAILURE) {
        cpr_free(msg);
    }
    return;
}

static void
SIPTaskProcessShutdown (int action, int reason)
{
    static const char fname[] = "SIPTaskProcessShutdown";

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Starting Shutdown Process", DEB_F_PREFIX_ARGS(SIP_TASK, fname));
    sip_mode_quiet = TRUE;
    /* Shutdown SIP components */
    sip_shutdown_phase1(action, reason);
}
/*
 *  Function: destroy_sip_thread
 *  Description:  kill sip msgQ and sip thread
 *  Parameters:   none
 *  Returns: none
 */
void destroy_sip_thread()
{
    static const char fname[] = "destroy_sip_thread";
    DEF_DEBUG(DEB_F_PREFIX"Unloading SIP and destroying sip thread",
        DEB_F_PREFIX_ARGS(SIP_CC_INIT, fname));

    /* kill msgQ thread first, then itself */
    (void) cprDestroyThread(sip_thread);
}
