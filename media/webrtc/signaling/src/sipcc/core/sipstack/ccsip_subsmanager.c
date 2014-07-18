/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_in.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "cpr_string.h"
#include "cpr_rand.h"
#include "ccsip_subsmanager.h"
#include "util_string.h"
#include "ccapi.h"
#include "phone_debug.h"
#include "ccsip_messaging.h"
#include "ccsip_platform_udp.h"
#include "ccsip_macros.h"
#include "ccsip_task.h"
#include "sip_common_transport.h"
#include "ccsip_platform_timers.h"
#include "platform_api.h"
#include "ccsip_register.h"
#include "ccsip_reldev.h"
#include "phntask.h"
#include "subapi.h"
#include "debug.h"
#include "ccsip_callinfo.h"
#include "regmgrapi.h"
#include "text_strings.h"
#include "configapp.h"
#include "kpmlmap.h"

/*
 *  Global Variables
 */
sipSCB_t subsManagerSCBS[MAX_SCBS]; // Array of SCBS
sipSubsHistory_t gSubHistory[MAX_SCB_HISTORY];
sipTimerCallbackFn_t callbackFunctionSubNot = sip_platform_subnot_msg_timer_callback;
sipTimerCallbackFn_t callbackFunctionPeriodic = sip_platform_subnot_periodic_timer_callback;
extern sipPlatformUITimer_t sipPlatformUISMSubNotTimers[]; // Array of timers
const char kpmlRequestAcceptHeader[]      = SIP_CONTENT_TYPE_KPML_REQUEST;
const char kpmlResponseAcceptHeader[]     = SIP_CONTENT_TYPE_KPML_RESPONSE;
const char dialogAcceptHeader[]           = SIP_CONTENT_TYPE_DIALOG;
const char presenceAcceptHeader[]         = "application/cpim-pidf+xml";
const char remoteccResponseAcceptHeader[] = SIP_CONTENT_TYPE_REMOTECC_RESPONSE;
const char remoteccRequestAcceptHeader[]  = SIP_CONTENT_TYPE_REMOTECC_REQUEST;

static sll_handle_t s_TCB_list = NULL; // signly linked list handle of TCBs

// Externs
extern int dns_error_code; // Global DNS error code

// Status and statistics for the Subscription Manager
int subsManagerRunning = 0;
int internalRegistrations = 0;

int incomingSubscribes = 0;
int incomingRefers = 0;
int incomingNotifies = 0;
int incomingUnsolicitedNotifies = 0;
int incomingSubscriptions = 0;

int outgoingSubscribes = 0;
int outgoingNotifies = 0;
int outgoingUnsolicitedNotifies = 0;
int outgoingSubscriptions = 0;

int currentScbsAllocated = 0;
int maxScbsAllocated = 0;

#define MAX_SUB_EVENTS    5
#define MAX_SUB_EVENT_NAME_LEN 16
/*
 * sub_id format:
 *
 * sub_id is a 32 bit quantity. The bit 32-16 holds a unique ID and
 * the bits 15-0 holds a SCB index. The following definitions help
 * support this formation.
 *
 * Assumption: The above format assumes that SCB index never be
 *             larger than 16 bit unsigned value.
 */
#define SUB_ID_UNIQUE_ID_POSITION 16     /* shift position of unique id part*/
#define SUB_IDSCB_INDEX_MASK      0xffff /* mask for obtaining scb index    */
#define GET_SCB_INDEX_FROM_SUB_ID(sub_id) \
     (sub_id & SUB_IDSCB_INDEX_MASK)

/*
 *  Event names
 */
const char eventNames[MAX_SUB_EVENTS][MAX_SUB_EVENT_NAME_LEN] =
{
    "dialog",
    "sip-profile",
    "kpml",
    "presence",
    "refer"
};

/*
 * Forward Function Declarations
 */
boolean sipSPISendSubscribe(sipSCB_t *scbp, boolean renew, boolean authen);
boolean sipSPISendSubNotify(ccsip_common_cb_t *cbp, boolean authen);
boolean sipSPISendSubscribeNotifyResponse(sipSCB_t *scbp,
                                          uint16_t response_code,
                                          uint32_t cseq);
boolean sipSPISendSubNotifyResponse(sipSCB_t *scbp, int response_code);
void free_scb(int scb_index, const char *fname);

// External Declarations
extern uint32_t IPNameCk(char *name, char *addr_error);
extern void kpml_init(void);
extern void kpml_shutdown(void);


extern void sip_platform_handle_service_control_notify(sipServiceControl_t * scp);
cc_int32_t show_subsmanager_stats(cc_int32_t argc, const char *argv[]);
static void show_scbs_inuse(void);
static void tcb_reset(void);

typedef struct {
   unsigned long  cseq;
   char           *via;
} sub_not_trxn_t;

/*
 * Function: find_matching_trxn()
 *
 * Parameters: key - key to find the matching node.
 *             data - node data.
 *
 * Descriotion: is invoked by sll_find() to find the matching node based on the key.
 *
 * Returns: either SLL_MATCH_FOUND or SLL_MATCH_NOT_FOUND.
 */
static sll_match_e
find_matching_trxn (void *key, void *data)
{
    unsigned long cseq = *((unsigned long *)key);
    sub_not_trxn_t *trxn_p = (sub_not_trxn_t *) data;

    if (cseq == trxn_p->cseq) {
        return SLL_MATCH_FOUND;
    }

    return SLL_MATCH_NOT_FOUND;
}

/*
 * Function: store_incoming_trxn()
 *
 * Description: stores the via header in incoming_trxns
 *
 * Parameters: via, cseq and scbp
 *
 * Returns: TRUE if the via header is successfully stored,
 *          FALSE otherwise.
 */
static boolean
store_incoming_trxn (const char *via, unsigned long cseq, sipSCB_t *scbp)
{
    static const char *fname = "store_incoming_trxn";
    size_t size = 0;

    sub_not_trxn_t *sub_not_trxn_p;

    if (scbp->incoming_trxns == NULL) {
        scbp->incoming_trxns = sll_create(find_matching_trxn);
        if (scbp->incoming_trxns == NULL) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sll_create() failed", fname);
            return FALSE;
        }
    }
    sub_not_trxn_p = (sub_not_trxn_t *)cpr_malloc(sizeof(sub_not_trxn_t));
    if (sub_not_trxn_p == NULL) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"malloc failed", fname);
        return FALSE;
    }
    sub_not_trxn_p->cseq = cseq;
    size = strlen(via) + 1;
    sub_not_trxn_p->via = (char *) cpr_malloc(size);
    if (sub_not_trxn_p->via) {
        sstrncpy(sub_not_trxn_p->via, via, size);
    }
    (void) sll_append(scbp->incoming_trxns, sub_not_trxn_p);

    return TRUE;
}

void
free_event_data (ccsip_event_data_t *event_data)
{
    ccsip_event_data_t *tmp;

    if (event_data == NULL) {
        return;
    }

    while (event_data != NULL) {
        tmp = event_data->next;
        if (event_data->type == EVENT_DATA_RAW) {
            if (event_data->u.raw_data.data)
                cpr_free(event_data->u.raw_data.data);
        }
        cpr_free(event_data);
        event_data = tmp;
    }
}

void
append_event_data (ccsip_event_data_t * event_data,
                   ccsip_event_data_t *new_data)
{
    while (event_data->next != NULL) {
        event_data = event_data->next;
    }
    event_data->next = new_data;
    new_data->next = NULL;
}

void
free_pending_requests (sipspi_msg_list_t *pendingRequests)
{
    sipspi_msg_list_t *tmp;

    while (pendingRequests) {
        switch (pendingRequests->cmd) {
        case SIPSPI_EV_CC_NOTIFY:
            {
                sipspi_notify_t *notify = &(pendingRequests->msg->msg.notify);

                free_event_data(notify->eventData);
                cpr_free(pendingRequests->msg);
            }
            break;
        case SIPSPI_EV_CC_SUBSCRIBE_REGISTER:
            cpr_free(&pendingRequests->msg->msg.subs_reg);
            break;
        case SIPSPI_EV_CC_SUBSCRIBE:
            {
                sipspi_subscribe_t *subs = &(pendingRequests->msg->msg.subscribe);

                free_event_data(subs->eventData);
                cpr_free(pendingRequests->msg);
            }
            break;
        case SIPSPI_EV_CC_SUBSCRIBE_RESPONSE:
            cpr_free(pendingRequests->msg);
            break;
        case SIPSPI_EV_CC_NOTIFY_RESPONSE:
            cpr_free(pendingRequests->msg);
            break;
        case SIPSPI_EV_CC_SUBSCRIPTION_TERMINATED:
            cpr_free(pendingRequests->msg);
            break;
        default:
            break;
        }
        tmp = pendingRequests;
        pendingRequests = pendingRequests->next;
        cpr_free(tmp);
    }
}

boolean
append_pending_requests (sipSCB_t *scbp,
                         sipspi_msg_t *newRequest,
                         uint32_t cmd)
{
    static const char *fname = "append_pending_requests";
    sipspi_msg_list_t *pendingRequest = NULL;
    sipspi_msg_list_t *tmp = NULL;

    if (!scbp)
        return (FALSE);

    pendingRequest = (sipspi_msg_list_t *)
        cpr_malloc(sizeof(sipspi_msg_list_t));
    if (!pendingRequest) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"malloc failed", fname);
        return (FALSE);
    }
    pendingRequest->cmd  = cmd;
    pendingRequest->msg  = newRequest;
    pendingRequest->next = NULL;

    if (scbp->pendingRequests == NULL) {
        scbp->pendingRequests = pendingRequest;
        return (TRUE);
    }

    tmp = scbp->pendingRequests;
    while (tmp->next != NULL) {
        tmp = tmp->next;
    }
    tmp->next = pendingRequest;
    return (TRUE);
}

void
handle_pending_requests (sipSCB_t *scbp)
{
    sipspi_msg_list_t *pendingRequest = NULL;

    if (scbp->pendingRequests) {
        pendingRequest = scbp->pendingRequests;
        scbp->pendingRequests = pendingRequest->next;
        switch (pendingRequest->cmd) {
        case SIPSPI_EV_CC_NOTIFY:
            {
                sipspi_msg_t *notify = pendingRequest->msg;

                cpr_free(pendingRequest);
                (void) subsmanager_handle_ev_app_notify(notify);
                cpr_free(notify);
            }
            break;
        case SIPSPI_EV_CC_SUBSCRIBE_REGISTER:
            cpr_free(pendingRequest->msg);
            cpr_free(pendingRequest);
            break;
        case SIPSPI_EV_CC_SUBSCRIBE:
            {
                sipspi_msg_t *subs = pendingRequest->msg;

                cpr_free(pendingRequest);
                (void) subsmanager_handle_ev_app_subscribe(subs);
                cpr_free(subs);
            }
            break;
        case SIPSPI_EV_CC_SUBSCRIBE_RESPONSE:
            {
                sipspi_msg_t *subs_resp = pendingRequest->msg;

                cpr_free(pendingRequest);
                (void) subsmanager_handle_ev_app_subscribe_response(subs_resp);
                cpr_free(subs_resp);
            }
            break;
        case SIPSPI_EV_CC_NOTIFY_RESPONSE:
            {
                sipspi_msg_t *not_resp = pendingRequest->msg;

                cpr_free(pendingRequest);
                (void) subsmanager_handle_ev_app_notify_response(not_resp);
                cpr_free(not_resp);
            }
            break;
        case SIPSPI_EV_CC_SUBSCRIPTION_TERMINATED:
            {
                sipspi_msg_t *term = pendingRequest->msg;

                cpr_free(pendingRequest);
                (void) subsmanager_handle_ev_app_subscription_terminated(term);
                cpr_free(term);
            }
            break;
        default:
            if (pendingRequest->msg) {
                cpr_free(pendingRequest->msg);
            }
            cpr_free(pendingRequest);
            break;
        }
    }
}

///////////////////////////////////////////////////////////// Test Only !!

static void
ccsip_api_subscribe_result (ccsip_sub_not_data_t * msg_data)
{
    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Received Subscribe Response: request_id=%ld, sub_id=%x",
                     DEB_F_PREFIX_ARGS(SIP_SUB_RESP, "ccsip_api_subscribe_result"),
                     msg_data->request_id, msg_data->sub_id);
    if (msg_data->u.subs_result_data.status_code == REQUEST_TIMEOUT) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Request timed out", DEB_F_PREFIX_ARGS(SIP_SUB_RESP, "ccsip_api_subscribe_result"));
    }
}

static void
print_event_data (ccsip_event_data_t * eventDatap)
{
    static const char *fname = "print_event_data";

    while (eventDatap) {
        switch (eventDatap->type) {
        case EVENT_DATA_INVALID:
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Invalid Data Received", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
            break;
        case EVENT_DATA_KPML_REQUEST:
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"KPML Request Event Data Received", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
            break;
        case EVENT_DATA_KPML_RESPONSE:
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"KPML Response Event Data Received", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
            break;
        case EVENT_DATA_PRESENCE:
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Presence Event Data Received", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
            break;
        case EVENT_DATA_DIALOG:
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Dialog Event Data Received", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
            break;
        case EVENT_DATA_RAW:
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Raw Event Data Received", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
            break;
        default:
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Event Data Type Not Understood", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
            break;
        }
        eventDatap = eventDatap->next;
    }
}

static void
ccsip_api_notify_result (ccsip_sub_not_data_t *msg_data)
{
    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Received Notify Response", DEB_F_PREFIX_ARGS(SIP_SUB_RESP, "ccsip_api_notify_result"));
}

static void
ccsip_api_notify_ind (ccsip_sub_not_data_t *msg)
{
    static const char *fname = "ccsip_api_notify_ind";
    sipspi_msg_t not_resp;

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Received Notify, request_id=%ld, sub_id=%x",DEB_F_PREFIX_ARGS(SIP_SUB_RESP, fname),
                     msg->request_id, msg->sub_id);

    // Check out what's there in the notify indication
    if (msg->u.notify_ind_data.eventData) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Event Data Received", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
        print_event_data(msg->u.notify_ind_data.eventData);
        free_event_data(msg->u.notify_ind_data.eventData);
    } else {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"No event data received", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
    }

    // Respond with a 200OK
    memset(&not_resp, 0, sizeof(sipspi_msg_t));
    not_resp.msg.notify_resp.sub_id = msg->sub_id;
    not_resp.msg.notify_resp.response_code = SIP_SUCCESS_SETUP;
    not_resp.msg.notify_resp.duration = 3600;

    (void) subsmanager_handle_ev_app_notify_response(&not_resp);
}

static void
ccsip_api_subscribe_terminate (ccsip_sub_not_data_t *msg_data)
{
	sipspi_msg_t terminate;

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Received Terminate notice", DEB_F_PREFIX_ARGS(SIP_SUB, "ccsip_api_subscribe_terminate"));
	if (msg_data->u.subs_term_data.status_code == NETWORK_SUBSCRIPTION_EXPIRED) {
        terminate.msg.subs_term.sub_id = msg_data->sub_id;
        terminate.msg.subs_term.immediate = TRUE;
        (void) subsmanager_handle_ev_app_subscription_terminated(&terminate);
	}
}

static void
ccsip_api_subscribe_ind (ccsip_sub_not_data_t *msg)
{
    static const char *fname = "ccsip_api_subscribe_ind";
    sipspi_msg_t subs_resp, notify, terminate;
    ccsip_event_data_t *eventData;
    char *junkdata;

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Received Subscription Request", DEB_F_PREFIX_ARGS(SIP_SUB, fname));

    // Check out what's there in the subs indication
    if (msg->u.subs_ind_data.eventData) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Event Data Received", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
        print_event_data(msg->u.subs_ind_data.eventData);
        free_event_data(msg->u.subs_ind_data.eventData);
    } else {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"No event data received", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
    }

    // Accept the subscription with a 200 OK response
    subs_resp.msg.subscribe_resp.duration = msg->u.subs_ind_data.expires;
    subs_resp.msg.subscribe_resp.response_code = SIP_SUCCESS_SETUP;
    subs_resp.msg.subscribe_resp.sub_id = msg->sub_id;
    (void) subsmanager_handle_ev_app_subscribe_response(&subs_resp);

    // Now send a NOTIFY
    notify.msg.notify.notifyResultCallback = ccsip_api_notify_result;
    notify.msg.notify.sub_id = msg->sub_id;
    notify.msg.notify.eventData = NULL;

    // Now fill in some data in the kpml structure
    eventData = (ccsip_event_data_t *) cpr_malloc(sizeof(ccsip_event_data_t));
    if (eventData == NULL) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Malloc of ccsip event data structure failed.", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
        return;
    }

    memset(eventData, 0, sizeof(ccsip_event_data_t));
    sstrncpy(eventData->u.kpml_request.pattern.regex.regexData, "012", 32);
    sstrncpy(eventData->u.kpml_request.version, "1.0", 16);
    eventData->type = EVENT_DATA_KPML_REQUEST;
    notify.msg.notify.eventData = eventData;

    // Now fill in some other junk data
    eventData = (ccsip_event_data_t *) cpr_malloc(sizeof(ccsip_event_data_t));
    if (eventData == NULL) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Malloc of ccsip event structure failed.", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
        cpr_free(notify.msg.notify.eventData);
        return;
    }

    junkdata = (char *) cpr_malloc(20);
    if (junkdata == NULL) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Malloc of junk data structure failed.", DEB_F_PREFIX_ARGS(SIP_EVT, fname));
        cpr_free(eventData);
        cpr_free(notify.msg.notify.eventData);
        return;
    }

    memset(eventData, 0, sizeof(ccsip_event_data_t));
    memset(junkdata, 0, 20);
    sstrncpy(junkdata, "Hello", 20);

    eventData->u.raw_data.data = junkdata;
    eventData->u.raw_data.length = strlen(junkdata);
    eventData->type = EVENT_DATA_RAW;
    notify.msg.notify.eventData->next = eventData;
    (void) subsmanager_handle_ev_app_notify(&notify);

    // If expires is 0, terminate the subscription
    if (msg->u.subs_ind_data.expires == 0) {
        terminate.msg.subs_term.sub_id = msg->sub_id;
        terminate.msg.subs_term.immediate = TRUE;
        (void) subsmanager_handle_ev_app_subscription_terminated(&terminate);
    }
}

static void
test_send_subscribe ()
{
    sipspi_msg_t subscribe;
    ccsip_event_data_t *eventData;

    memset(&subscribe.msg.subscribe, 0, sizeof(sipspi_subscribe_t));

    subscribe.msg.subscribe.eventPackage = CC_SUBSCRIPTIONS_KPML;
    subscribe.msg.subscribe.duration = 15;
    sstrncpy(subscribe.msg.subscribe.subscribe_uri, "19921", CC_MAX_DIALSTRING_LEN);
    sstrncpy(subscribe.msg.subscribe.subscriber_uri, "12345", CC_MAX_DIALSTRING_LEN);
    subscribe.msg.subscribe.request_id = 1003;
    subscribe.msg.subscribe.dn_line = 2;
    subscribe.msg.subscribe.sub_id = CCSIP_SUBS_INVALID_SUB_ID;
    subscribe.msg.subscribe.subsResultCallback = ccsip_api_subscribe_result;
    subscribe.msg.subscribe.subsTermCallback = ccsip_api_subscribe_terminate;
    subscribe.msg.subscribe.notifyIndCallback = ccsip_api_notify_ind;
    subscribe.msg.subscribe.auto_resubscribe = TRUE;

    // Now fill in some data in the kpml structure
    /*
     * eventData = (ccsip_event_data_t *) cpr_malloc(sizeof(ccsip_event_data_t));
     * if (eventData == NULL) {
     * CCSIP_DEBUG_TASK("Malloc of ccsip event data failed.");
     * return;
     * }
     *
     * memset(eventData, 0, sizeof(ccsip_event_data_t));
     * sstrncpy(eventData->u.kpml_request.pattern.regex.regexData, "012", sizeof(eventData->u.kpml_request.pattern.regex.regexData));
     * sstrncpy(eventData->u.kpml_request.version, "1.0", sizeof(eventData->u.kpml_request.version));
     * eventData->type = EVENT_DATA_KPML_REQUEST;
     * subscribe.msg.subscribe.eventData = eventData;
     */
    // Now fill in some other junk data
    eventData = (ccsip_event_data_t *) cpr_malloc(sizeof(ccsip_event_data_t));
    if (eventData == NULL) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Malloc of ccsip event data failed.", DEB_F_PREFIX_ARGS(SIP_EVT, "test_send_subscribe"));
        return;
    }

    memset(eventData, 0, sizeof(ccsip_event_data_t));
    eventData->type = EVENT_DATA_RAW;
    eventData->u.raw_data.data = (char *) cpr_malloc(150);
    if (eventData->u.raw_data.data) {
        eventData->u.raw_data.length = 150;
        memset(eventData->u.raw_data.data, 'V', 150);
    } else {
        eventData->u.raw_data.length = 0;
    }
    eventData->next = NULL;
    subscribe.msg.subscribe.eventData = eventData;

    (void) subsmanager_handle_ev_app_subscribe(&subscribe);
}

static void
test_send_register ()
{
    sipspi_msg_t register_msg;

    memset(&register_msg, 0, sizeof(sipspi_msg_t));
    register_msg.msg.subs_reg.eventPackage = CC_SUBSCRIPTIONS_PRESENCE;
    register_msg.msg.subs_reg.subsIndCallback = ccsip_api_subscribe_ind;
	register_msg.msg.subs_reg.subsTermCallback = ccsip_api_subscribe_terminate;
    (void) subsmanager_handle_ev_app_subscribe_register(&register_msg);
}

int
subsmanager_test_start_routine ()
{
    static int subscribe_sent = 0;
    static int register_sent = 0;

    if (subscribe_sent == 0) {
        test_send_subscribe();
        subscribe_sent = 1;
    }

    if (register_sent == 0) {
        test_send_register();
        register_sent = 1;
    }
    return 0;

}

/************************************************************
 * Send local error to the calling application
 * This function reuses the callback interface to pass the
 * (bad) result of requests from the application
 ************************************************************/
void
sip_send_error_message (ccsip_sub_not_data_t *msg_data,
                        cc_srcs_t dest_task,
                        int msgid,
                        ccsipGenericCallbackFn_t callbackFn,
                        const char *fname)
{
    if (!msg_data) {
        return;
    }
    if (callbackFn) {
        (*callbackFn) (msg_data);
    } else if (dest_task != CC_SRC_MIN) {
        (void) sip_send_message(msg_data, dest_task, msgid);
    }
}


/********************************************************
 * Shutdown the Subscription Manager
 ********************************************************/
int
sip_subsManager_shut ()
{
    const char *fname = "sip_subsManager_shut";
    int i;
    sipSCB_t *scbp = NULL;
    ccsip_sub_not_data_t error_data;

    if (subsManagerRunning == 0) {
        return (0);
    }
    error_data.reason_code = SM_REASON_CODE_SHUTDOWN;
    // Send indication of subscription ended to internal apps
    // then clean and free up subscriptions and SCB
    for (i = 0; i < MAX_SCBS; i++) {
        scbp = &(subsManagerSCBS[i]);
        if (scbp->smState == SUBS_STATE_IDLE) {
            continue;
        }

        error_data.sub_id = scbp->sub_id;
        error_data.request_id = scbp->request_id;
        error_data.sub_duration = 0;
        error_data.event = scbp->hb.event_type;
        error_data.msg_id = scbp->subsTermCallbackMsgID;
        error_data.line_id = scbp->hb.dn_line;
        error_data.gsm_id = scbp->gsm_id;
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Sending shutdown notification for scb=%d"
                         " sub_id=%x\n", DEB_F_PREFIX_ARGS(SIP_SUB, fname), i, scbp->sub_id);

        sip_send_error_message(&error_data, scbp->subsNotCallbackTask,
                               scbp->subsTermCallbackMsgID,
                               scbp->subsTermCallback, fname);

        free_scb(i, fname);
    }

    // Shut down the periodic timer
    (void) sip_platform_subnot_periodic_timer_stop();

    // Mark subsManager as stopped running
    subsManagerRunning = 0;

    tcb_reset();

    return (0);
}

/********************************************************
 * Common code notifying application for a failover/fallback
 * or CCM new registration (reset) event. Send a notification to
 * applications that have an active subscription
 ********************************************************/
static int
sip_subsManager_reg_failure_common (ccsip_reason_code_e reason)
{
    const char *fname = "sip_subsManager_reg_failure_common";
    int i;
    sipSCB_t *scbp = NULL;
    ccsip_sub_not_data_t error_data;

    if (subsManagerRunning == 0) {
        return (0);
    }
    error_data.reason_code = reason;
    // Send indication of subscription ended to internal apps
    // then clean and free up subscriptions and SCB
    for (i = 0; i < MAX_SCBS; i++) {
        scbp = &(subsManagerSCBS[i]);
        if (scbp->smState == SUBS_STATE_IDLE ||
            scbp->smState == SUBS_STATE_REGISTERED) {
            // Update addr and port after rollover/ccm reset
            scbp->hb.local_port = sipTransportGetListenPort(1, NULL);
            sipTransportGetServerIPAddr(&(scbp->hb.dest_sip_addr),1);
            scbp->hb.dest_sip_port = sipTransportGetPrimServerPort(1);
            continue;
        }

        error_data.sub_id = scbp->sub_id;
        error_data.line_id = scbp->hb.dn_line;
        error_data.request_id = scbp->request_id;
        error_data.sub_duration = 0;
        error_data.event = scbp->hb.event_type;
        error_data.msg_id = scbp->subsTermCallbackMsgID;

        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Sending reg failure notification for "
                         "scb=%d sub_id=%x reason=%d\n", DEB_F_PREFIX_ARGS(SIP_SUB, fname), i,
                         scbp->sub_id, reason);

        sip_send_error_message(&error_data, scbp->subsNotCallbackTask,
                               scbp->subsTermCallbackMsgID,
                               scbp->subsTermCallback, fname);

        if (scbp->internal) {
            outgoingSubscriptions--;
        } else {
            incomingSubscriptions--;
        }
        free_scb(i, fname);
    }

    sipRelDevAllMessagesClear();
    return (0);
}

/********************************************************
 * Handle a failover/fallback event
 * to handle this event, send a notification to all
 * applications that have an active subscription
 ********************************************************/
int
sip_subsManager_rollover ()
{
    tcb_reset();
    return (sip_subsManager_reg_failure_common(SM_REASON_CODE_ROLLOVER));
}

/********************************************************
 * Handle a (CCM or Proxy) server down and up event^M
 * to handle this event, send a notification to all^M
 * applications that have an active subscription^M
 ********************************************************/
int
sip_subsManager_reset_reg (void)
{
    return (sip_subsManager_reg_failure_common(SM_REASON_CODE_RESET_REG));
}


/***********************************************************
 * Send a protocol error message to application. The application
 * decides whether to terminate and restart the subscription
 * or otherwise
 ***********************************************************/
void
sip_subsManager_send_protocol_error (sipSCB_t *scbp, int scb_index,
                                     boolean terminate)
{
    const char *fname = "sip_subsManager_send_protocol_error";
    ccsip_sub_not_data_t error_data;

    CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Protocol Error for scb=%d sub_id=%x", fname,
                      scb_index, scbp->sub_id);

    error_data.reason_code = SM_REASON_CODE_ERROR;
    error_data.sub_id = scbp->sub_id;
    error_data.request_id = scbp->request_id;
    error_data.sub_duration = 0;
    error_data.event = scbp->hb.event_type;
    error_data.msg_id = scbp->subsTermCallbackMsgID;
    error_data.line_id = scbp->hb.dn_line;

    sip_send_error_message(&error_data, scbp->subsNotCallbackTask,
                           scbp->subsTermCallbackMsgID, scbp->subsTermCallback,
                           fname);

    if (terminate) {
        free_scb(scb_index, fname);
    }

}

/********************************************************
 * Start and initialize the Subscription Manager
 ********************************************************/
void
initialize_scb (sipSCB_t *scbp)
{
    int nat_enable = 0;

    if (!scbp) {
        return;
    }
    memset(scbp, 0, sizeof(sipSCB_t));

    scbp->sub_id = CCSIP_SUBS_INVALID_SUB_ID;
    scbp->pendingClean = FALSE;
    scbp->pendingCount = 0;
    scbp->internal = FALSE;
    scbp->subsIndCallback = NULL;
    scbp->subsResultCallback = NULL;
    scbp->notifyIndCallback = NULL;
    scbp->notifyResultCallback = NULL;
    scbp->subsTermCallback = NULL;
    scbp->notIndCallbackMsgID = 0;
    scbp->notResCallbackMsgID = 0;
    scbp->subsIndCallbackMsgID = 0;
    scbp->subsResCallbackMsgID = 0;
    scbp->subsTermCallbackMsgID = 0;
    scbp->subsIndCallbackTask = CC_SRC_MIN;
    scbp->subsNotCallbackTask = CC_SRC_MIN;

    config_get_value(CFGID_NAT_ENABLE, &nat_enable, sizeof(nat_enable));
    if (nat_enable == 0) {
        sip_config_get_net_device_ipaddr(&(scbp->hb.src_addr));
    } else {
        sip_config_get_nat_ipaddr(&(scbp->hb.src_addr));
    }
    scbp->hb.cb_type = SUBNOT_CB;
    scbp->hb.dn_line = 1;
    scbp->hb.local_port = sipTransportGetListenPort(scbp->hb.dn_line, NULL);
    sipTransportGetServerIPAddr(&(scbp->hb.dest_sip_addr),1);
    scbp->hb.dest_sip_port = sipTransportGetPrimServerPort(1);

    scbp->hb.sipCallID[0] = '\0';
    scbp->smState = SUBS_STATE_IDLE;
    scbp->SubURI[0] = '\0';
    scbp->SubscriberURI[0] = '\0';
    scbp->sip_from = strlib_empty();
    scbp->sip_to = strlib_empty();
    scbp->sip_to_tag = strlib_empty();
    scbp->sip_from_tag = strlib_empty();
    scbp->sip_contact = strlib_empty();
    scbp->cached_record_route = strlib_empty();
    scbp->callingNumber = strlib_empty();
    scbp->subscription_state = SUBSCRIPTION_STATE_INVALID;
    scbp->norefersub = FALSE;
    scbp->request_id = -1;
    scbp->hb.authen.cred_type = 0;
    scbp->hb.authen.authorization = NULL;
    scbp->hb.authen.status_code = 0;
    scbp->hb.authen.nc_count = 0;
    scbp->hb.authen.new_flag = FALSE;
    scbp->hb.event_data_p = NULL;
    scbp->pendingRequests = NULL;
}

int
sip_subsManager_init ()
{
    // Initialize SCBS array
    const char *fname = "sip_subsManager_init";
    line_t   i = 0;
    sipSCB_t *scbp;

    if (subsManagerRunning == 1) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Subscription Manager already running!!", fname);
        return SIP_OK;
    }

    for (i = 0; i < MAX_SCBS; i++) {
        scbp = &(subsManagerSCBS[i]);
        initialize_scb(scbp);
        scbp->line = i;
    }

    for (i = 0; i < MAX_SCB_HISTORY; i++) {
        gSubHistory[i].last_call_id[0] = '\0';
        gSubHistory[i].last_from_tag[0] = '\0';
        gSubHistory[i].eventPackage = CC_SUBSCRIPTIONS_NONE;
    }

    // reset status and stats
    internalRegistrations = 0;

    incomingSubscribes = 0;
    incomingRefers = 0;
    incomingNotifies = 0;
    incomingUnsolicitedNotifies = 0;
    incomingSubscriptions = 0;

    outgoingSubscribes = 0;
    outgoingNotifies = 0;
    outgoingUnsolicitedNotifies = 0;
    outgoingSubscriptions = 0;

    currentScbsAllocated = 0;
    maxScbsAllocated = 0;

    // Start periodic timer
    (void) sip_platform_subnot_periodic_timer_start(TMR_PERIODIC_SUBNOT_INTERVAL * 1000);

    subsManagerRunning = 1;

    // Print out the size of the SCB
    // CCSIP_DEBUG_ERROR("SCB Size=%d", sizeof(sipSCB_t));
    // CCSIP_DEBUG_ERROR("CCB Size=%d", sizeof(ccsipCCB_t));
    // Kick-off the test routine
    // subsmanager_test_start_routine();

    /* Initialize modules which uses SUB/MANAGER */
    kpml_init();
    configapp_init();

    return SIP_OK;
}


/********************************************************
 * Functions to allocate, locate, free, and update SCBs
 ********************************************************/

/*
 *  Function: find_scb_by_callid
 *
 *  Parameters:
 *      callID    - pointer to const. char for the target call ID.
 *      scb_index - pointer to int where the SCB index of the result
 *                  SCB to be stored.
 *
 *  Description:
 *      The find_scb_by_callid function searches for an SCB that
 *  contains the matching callID. It is possible to have multiple
 *  SCB that matches the given callID. The function only returns
 *  the first SCB that has the matching callID.
 *
 *  Returns:
 *     Pointer to SCB and its index if the SCB is found. Otherwise
 *  it returns NULL and the index is undefined.
 */
sipSCB_t *
find_scb_by_callid (const char *callID, int *scb_index)
{
    int       i;
    int       num_scb = currentScbsAllocated;
    sipSCB_t *scbp;

    if (num_scb == 0) {
        /* No active subscription */
        return (NULL);
    }
    scbp = &subsManagerSCBS[0];
    for (i = 0; (i < MAX_SCBS) && num_scb; i++, scbp++) {
        if (scbp->smState != SUBS_STATE_IDLE) {
            if ((scbp->smState != SUBS_STATE_REGISTERED) &&
                (strcmp(callID, scbp->hb.sipCallID) == 0)) {
                *scb_index = i;
                return (scbp);
            }
            num_scb--;
        }
    }

    return (NULL);
}

/*
 *  Function: find_req_scb
 *
 *  Parameters:
 *      callID    - pointer to const. char for the target call ID.
 *      method    - SIP method to match.
 *      cseq      - CSEQ value to match.
 *      scb_index - pointer to int where the SCB index of the result
 *                  SCB to be stored.
 *
 *  Description:
 *      The find_req_scb function searches for an SCB that contains
 *  the matching callID, last request method and last cseq sent. If
 *  the match SCB is found, the function returns the pointer to the
 *  sipSCB_t along with the corresponding index.
 *
 *  Returns:
 *     Pointer to SCB and its index if the SCB is found. Otherwise
 *  returns NULL and the index will be set to MAX_SCBS.
 */
static sipSCB_t *
find_req_scb (const char *callID, sipMethod_t method,
              uint32_t cseq, int *scb_index)
{
    int       idx, num_scb;
    sipSCB_t *scbp;

    scbp = &subsManagerSCBS[0];
    num_scb = currentScbsAllocated;
    /*
     * Search SCB tables for all allocated SCBs.
     */
    for (idx = 0; (idx < MAX_SCBS) && num_scb; idx++, scbp++) {
        if (scbp->smState != SUBS_STATE_IDLE) {
            if ((scbp->smState != SUBS_STATE_REGISTERED) &&
                (scbp->last_sent_request_cseq_method == method) &&
                (scbp->last_sent_request_cseq == cseq) &&
                (strcmp(callID, scbp->hb.sipCallID) == 0)) {
                /* Found the matching scb */
                *scb_index = idx;
                return (scbp);
            }
            num_scb--;
        }
    }
    *scb_index = MAX_SCBS;
    return (NULL);
}

/*
 *  Function: find_scb_by_sub_id
 *
 *  Parameters:
 *      sub_id    - the sub_id_t parameter from which scb index
 *                  to be obtained.
 *      scb_index - pointer to int for index of the SCB entry to
 *                  be returned if the pointer is provided (not
 *                  NULL).
 *
 *  Description:
 *      The function finds the SCB for the given sub_id.
 *
 *  Returns:
 *      1) pointer to sipSCB_t or NULL if finding fails.
 *      2) the index into the SCB array is also returned if the
 *         scb_index parameter is provided.
 */
static sipSCB_t *
find_scb_by_sub_id (sub_id_t sub_id, int *scb_index)
{
    int idx, ret_idx = MAX_SCBS;
    sipSCB_t *scbp = NULL;

    /*
     * Use index part of the sub_id to find the SCB
     */
    idx = GET_SCB_INDEX_FROM_SUB_ID(sub_id);
    if (idx < MAX_SCBS) {
        /*
         * SCB index is within a valid range, get the SCB by index.
         * Match the SCB's sub_id and the one provided.
         */
        if (subsManagerSCBS[idx].sub_id == sub_id) {
            /* The correct SCB is found */
            scbp    = &(subsManagerSCBS[idx]);
            ret_idx = idx;
        }
    }

    if (scb_index != NULL) {
        *scb_index = ret_idx;
    }
    return (scbp);
}

sipSCB_t *
find_scb_by_registration (cc_subscriptions_t event, int *scb_index)
{
    int i;

    for (i = 0; i < MAX_SCBS; i++) {
        if ((subsManagerSCBS[i].hb.event_type == event) &&
            (subsManagerSCBS[i].smState == SUBS_STATE_REGISTERED)) {
            *scb_index = i;
            return &(subsManagerSCBS[i]);
        }
    }

    return (NULL);
}

sipSCB_t *
find_scb_by_subscription (cc_subscriptions_t event, int *scb_index,
                          const char *callID)
{
    int i;

    for (i = 0; i < MAX_SCBS; i++) {
        if (cpr_strcasecmp(subsManagerSCBS[i].hb.sipCallID, callID) == 0) {
            *scb_index = i;
            return &(subsManagerSCBS[i]);
        }
    }
    return (NULL);
}

/*
 *  Function: new_sub_id
 *
 *  Parameters:
 *      scb_index - the SCB index.
 *
 *  Description:
 *      The function allocates a new unique sub_id and return
 *  it to the caller.
 *
 *  NOTE: the scb_index can not exceed 16 bit unsigned value.
 *
 *  Returns:
 *     sub_id.
 */
static sub_id_t
new_sub_id (int scb_index)
{
    static sub_id_t unique_id = 0;
    sub_id_t sub_id;

    /*
     * Form the sub_id is encoded as the following:
     *       bit 31 - bit 16 contains unique ID
     *       bit 15 - bit  0 contains scb index.
     */
    sub_id = (unique_id << SUB_ID_UNIQUE_ID_POSITION) |
             (sub_id_t)(scb_index & SUB_IDSCB_INDEX_MASK);
    unique_id++;       /* next unique sub id */
    if (sub_id == CCSIP_SUBS_INVALID_SUB_ID) {
        /* sub_id becomes the invalid value marker, re-calcualte new id  */
        sub_id = (unique_id << SUB_ID_UNIQUE_ID_POSITION) |
                 (sub_id_t)(scb_index & SUB_IDSCB_INDEX_MASK);
        unique_id++;
    }
    return (sub_id);
}

sipSCB_t *
allocate_scb (int *scb_index)
{
    int i;

    for (i = 0; i < MAX_SCBS; i++) {
        if (subsManagerSCBS[i].smState == SUBS_STATE_IDLE) {
            *scb_index = i;
            currentScbsAllocated++;
            if (currentScbsAllocated > maxScbsAllocated) {
                maxScbsAllocated = currentScbsAllocated;
            }
            /*
             * assigned sub_id to the allocated SCB.
             */
            subsManagerSCBS[i].sub_id = new_sub_id(i);
            CCSIP_DEBUG_TASK("allocate_scb scb_index: %d, currentScbsAllocated: %d, "
                    "maxScbsAllocated: %d, sub_id: %x", *scb_index,
                    currentScbsAllocated, maxScbsAllocated, subsManagerSCBS[i].sub_id);

            /*
             * local port may have changed because of failover/fallback.
             * So update it with current info so that the Via & Contact headers
             * in SUB/NOT are generated correctly.
             */
            subsManagerSCBS[i].hb.local_port =
                sipTransportGetListenPort(subsManagerSCBS[i].hb.dn_line, NULL);
            return &(subsManagerSCBS[i]);
        }
    }
    return (NULL);
}

boolean
is_previous_sub (const char *pCallID,
                 char *pFromTag,
                 cc_subscriptions_t event)
{
    int i;

    if (!pCallID || !pFromTag) {
        return FALSE;
    }

    for (i = 0; i < MAX_SCB_HISTORY; i++) {
        if (strncmp(gSubHistory[i].last_call_id, pCallID, MAX_SIP_CALL_ID) == 0) {
            if (strncmp(gSubHistory[i].last_from_tag, pFromTag, MAX_SIP_TAG_LENGTH) == 0) {
                if (gSubHistory[i].eventPackage == event) {
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

void
clean_scb (sipSCB_t *scbp)
{
    sub_not_trxn_t *trxn_p;

    if (scbp) {
        strlib_free(scbp->sip_from);
        strlib_free(scbp->sip_to);
        strlib_free(scbp->sip_to_tag);
        strlib_free(scbp->sip_from_tag);
        strlib_free(scbp->callingNumber);
        strlib_free(scbp->sip_contact);
        strlib_free(scbp->cached_record_route);
        if (scbp->contact_info)
            sippmh_free_contact(scbp->contact_info);
        if (scbp->record_route_info)
            sippmh_free_record_route(scbp->record_route_info);
        if (scbp->hb.event_data_p)
            free_event_data(scbp->hb.event_data_p);
        if (scbp->pendingRequests) {
            free_pending_requests(scbp->pendingRequests);
        }

        scbp->hb.authen.cnonce[0] = '\0';
        if (scbp->hb.authen.authorization != NULL) {
            cpr_free(scbp->hb.authen.authorization);
            scbp->hb.authen.authorization = NULL;
        }
        if (scbp->hb.authen.sip_authen != NULL) {
            sippmh_free_authen(scbp->hb.authen.sip_authen);
            scbp->hb.authen.sip_authen = NULL;
        }

        /* free all transactions */
        if (scbp->incoming_trxns) {
            while ((trxn_p = (sub_not_trxn_t *)
                sll_next(scbp->incoming_trxns, NULL)) != NULL) {
                (void) sll_remove(scbp->incoming_trxns, (void *)trxn_p);
                cpr_free(trxn_p->via);
                cpr_free(trxn_p);
            }
            sll_destroy(scbp->incoming_trxns);
            scbp->incoming_trxns = NULL;
        }

    }
}
void
store_scb_history (sipSCB_t *scbp)
{
    // Copy SCB parameters in the next available history array
    static int next_history = 0;

    next_history++;
    if (next_history == MAX_SCB_HISTORY) {
        next_history = 0;
    }
    sstrncpy(gSubHistory[next_history].last_call_id, scbp->hb.sipCallID, MAX_SIP_CALL_ID);
    sstrncpy(gSubHistory[next_history].last_from_tag, scbp->sip_from_tag, MAX_SIP_TAG_LENGTH);
    gSubHistory[next_history].eventPackage = scbp->hb.event_type;
}

/**
 *
 * frees the SCB. It will cleanup the scb (ie, free the memory allocated for any sub fields).
 * It will also mark the SCB as free (smState = SUBS_STATE_IDLE).
 * Please note this function may be invoked can be invoked even when smState == SUBS_STATE_IDLE.
 * An example case is when parse_body() fails.
 *
 * @param[in] scb_index - indext of the SCB into SCB array.
 * @param[in] fname -  name of the function calling this function.
 *
 * @return  none
 *
 * @pre     (scb_index >= 0) and (scb_index < MAX_SCBS)
 * @post    (subsManagerSCBS[scb_index].smState equals FALSE)
 */
void
free_scb (int scb_index, const char *fname)
{
    sipSCB_t *scbp = NULL;

    if (scb_index >= MAX_SCBS || scb_index < 0) {
        CCSIP_DEBUG_ERROR("%s Trying to free an invalid scb_index. Return.", fname);
        return;
    }
    scbp = &(subsManagerSCBS[scb_index]);


    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Freeing SCB: scb=%d sub_id=%x",
                     DEB_F_PREFIX_ARGS(SIP_SUB, "free_scb"), scb_index, scbp->sub_id);

    if (scbp->smState != SUBS_STATE_IDLE) {
        currentScbsAllocated--;
        /*
         * This condition should never happen. If occurs, it is a strong indication
         * something has gone wrong in our code logic and should be investigated.
         */
        if (currentScbsAllocated < 0) {
            CCSIP_DEBUG_ERROR("%s: Error somewhere in scb accounting which results"
                       "in negative currentScbsAllocated. Set it to 0.\n", fname);
            currentScbsAllocated = 0;
        }
    }
    // If this was an incoming subscription, store values in history first
    if ((scbp->internal == FALSE) && (scbp->smState != SUBS_STATE_REGISTERED)) {
        store_scb_history(scbp);
    }

    clean_scb(scbp);

    // Stop associated message retry timer
    if (sipPlatformUISMSubNotTimers[scb_index].outstanding) {
        sip_platform_msg_timer_subnot_stop(&sipPlatformUISMSubNotTimers[scb_index]);
    }

    // Re-initialize
    initialize_scb(scbp);
    scbp->line = (line_t) scb_index;
}

/**
 * This function will free up the TCB resources and removes it from the TCB list.
 *
 * @param[in] tcbp -  pointer to a TCB.
 *
 * @return  none
 *
 *  @pre    (tcbp != NULL)
 */
static void free_tcb (sipTCB_t *tcbp)
{
    if (tcbp->hb.authen.authorization != NULL) {
        cpr_free(tcbp->hb.authen.authorization);
    }
    if (tcbp->hb.authen.sip_authen != NULL) {
        sippmh_free_authen(tcbp->hb.authen.sip_authen);
    }

    (void)cprDestroyTimer(tcbp->timer);
    free_event_data(tcbp->hb.event_data_p);
    (void)sll_remove(s_TCB_list, (void *)tcbp);
    cpr_free(tcbp);
}

/**
 * This function will find matching TCB by the SIP Call-ID in the TCB list.
 *
 * @param[in] callID_p - SIP Call-ID
 *
 * @return  NULL if there is no matching TCB
 *          Otherwise, pointer to the found TCB is returned.
 *
 *  @pre    (callID_p != NULL)
 */
sipTCB_t *find_tcb_by_sip_callid (const char *callID_p)
{
    sipTCB_t *tcb_p;

    tcb_p = (sipTCB_t *)sll_next(s_TCB_list, NULL);
    while (tcb_p != NULL) {
        if (strncmp(callID_p, tcb_p->hb.sipCallID, (sizeof(tcb_p->hb.sipCallID) -1)) == 0) {
            return tcb_p;
        }
        tcb_p = (sipTCB_t *)sll_next(s_TCB_list, tcb_p);
    }
    return NULL;
}

/**
 * This function will free up TCBs when
 * 1. restarting or
 * 2. failing over/ falling back
 *
 * @param[in] none
 *
 * @return none
 */
static void tcb_reset (void)
{
    sipTCB_t *tcb_p;

    tcb_p = (sipTCB_t *)sll_next(s_TCB_list, NULL);
    while (tcb_p != NULL) {
        free_tcb(tcb_p);
        tcb_p = (sipTCB_t *)sll_next(s_TCB_list, NULL);
    }
}

/*
 * Function is called to remove ccb pointer from scb
 * When the dialog is cleared then ccb associated with
 * that dialog is cleared as well. Remove that from
 * scb parameters as well
 */
void
submanager_update_ccb_addr (ccsipCCB_t *ccb)
{
    sipSCB_t *scbp = NULL;
    int       scb_index = 0;
    int       num_scb;

    if ((ccb == NULL) || (currentScbsAllocated == 0)) {
        /* The CCB is NULL or no active SCBs */
        return;
    }

    /* Once the dialog is cleared i.e ccb is removed, the subnot
     * should continue to use from its last sequence number. So
     * re-assign last sequence value to scbp.
     *
     * There are possibility of multiple subscriptions associate
     * with a call dialog. Search all of the SCB for the given CCB.
     */
    scbp    = &subsManagerSCBS[0];
    num_scb = currentScbsAllocated;
    for (scb_index = 0; (scb_index < MAX_SCBS) && num_scb; scb_index++, scbp++) {
        if (scbp->smState != SUBS_STATE_IDLE) {
            if ((scbp->smState != SUBS_STATE_REGISTERED) &&
                (scbp->ccbp == ccb)) {
                scbp->last_sent_request_cseq = ccb->last_used_cseq;
                scbp->ccbp = NULL;
            }
            num_scb--;
        }
    }

}


/*
 * Takes care of all the parsing requirements
 */
static int
parse_body (cc_subscriptions_t event_type, char *msgBody, int msgLength,
            ccsip_event_data_t **eventDatapp,
            const char *fname)
{
    const char     *fname1 = "parse_body";
    ccsip_event_data_type_e type = EVENT_DATA_INVALID;

    if (!msgBody) {
        return SIP_ERROR;
    }

    switch (event_type) {
        case CC_SUBSCRIPTIONS_KPML:
            type = EVENT_DATA_KPML_REQUEST;
        break;
        case CC_SUBSCRIPTIONS_CONFIGAPP:
            type = EVENT_DATA_CONFIGAPP_REQUEST;
        break;
        default:
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"%s: unknown event type %d", fname1, fname, type);
        return SIP_ERROR;
    }

    return SIP_OK;
}

boolean
add_content (ccsip_event_data_t *eventData, sipMessage_t *request, const char *fname)
{
    return FALSE;

/* This function requires XML handling */
#if 0
    const char     *fname1 = "add_content";
    uint32_t        len;
    char           *eventBody = NULL;

    while (eventData) {
        /* Encode eventData into eventBody here */
        len = strlen(eventBody);

        switch (eventData->type) {
        case EVENT_DATA_RAW:
            // Assume body is of type CMXML for now
            (void) sippmh_add_message_body(request, eventBody, len,
                                           SIP_CONTENT_TYPE_CMXML,
                                           SIP_CONTENT_DISPOSITION_SESSION_VALUE, TRUE, NULL);
            break;
        case EVENT_DATA_KPML_REQUEST:
            (void) sippmh_add_message_body(request, eventBody, len,
                                           SIP_CONTENT_TYPE_KPML_REQUEST,
                                           SIP_CONTENT_DISPOSITION_SESSION_VALUE, TRUE, NULL);
            break;
        case EVENT_DATA_KPML_RESPONSE:
            (void) sippmh_add_message_body(request, eventBody, len,
                                           SIP_CONTENT_TYPE_KPML_RESPONSE,
                                           SIP_CONTENT_DISPOSITION_SESSION_VALUE, TRUE, NULL);
            break;
        default:
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"%s: Data type not supported", fname1, fname);
            cpr_free(eventBody);
            break;
        }

        eventData = eventData->next;
    }
    return (TRUE);
#endif
}

// Functions to handle requests from internal applications

/************************************************************
 * Applications Register to Receive an incoming subscribe(s)
 ************************************************************/
int
subsmanager_handle_ev_app_subscribe_register (cprBuffer_t buf)
{
    const char     *fname = "subsmanager_handle_ev_app_register";
    sipspi_subscribe_reg_t *reg_datap;
    sipSCB_t       *scbp = NULL;
    int             scb_index;
    sipspi_msg_t   *pSIPSPIMsg = NULL;


    pSIPSPIMsg = (sipspi_msg_t *) buf;

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Processing a new subscription registration", DEB_F_PREFIX_ARGS(SIP_SUB, fname));

    if (!subsManagerRunning) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Subscription Manager Not Initialized!", fname);
        return SIP_ERROR;
    }

    reg_datap = &(pSIPSPIMsg->msg.subs_reg);
    if (reg_datap->subsIndCallback == NULL &&
        reg_datap->subsIndCallbackMsgID == 0) {
        return SIP_ERROR;
    }

    scbp = find_scb_by_registration(reg_datap->eventPackage, &scb_index);
    if (scbp) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Duplicate registration!", fname);
        return SIP_ERROR;
    } else {
        scbp = allocate_scb(&scb_index);
        if (!scbp) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Subscription control block allocation failed", fname);
            return SIP_ERROR;
        }
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Allocated SCB for App Registration,"
                         " event=%d, scb=%d, sub_id=%x\n", DEB_F_PREFIX_ARGS(SIP_SUB, fname),
                         reg_datap->eventPackage,
                         GET_SCB_INDEX_FROM_SUB_ID(scbp->sub_id),
                         scbp->sub_id);
    }

    scbp->hb.dn_line = 1;
    scbp->hb.event_type = reg_datap->eventPackage;
    if (reg_datap->eventPackage - CC_SUBSCRIPTIONS_DIALOG > -1 &&
        reg_datap->eventPackage - CC_SUBSCRIPTIONS_DIALOG < 5) {
        sstrncpy(scbp->event_name, eventNames[reg_datap->eventPackage - CC_SUBSCRIPTIONS_DIALOG], MAX_EVENT_NAME_LEN);
    }

    // Get callback information (either event or function)
    scbp->subsIndCallback = reg_datap->subsIndCallback;
    scbp->subsIndCallbackTask = reg_datap->subsIndCallbackTask;
    scbp->subsNotCallbackTask = reg_datap->subsIndCallbackTask;
    scbp->subsIndCallbackMsgID = reg_datap->subsIndCallbackMsgID;
    scbp->subsTermCallback = reg_datap->subsTermCallback;
    scbp->subsTermCallbackMsgID = reg_datap->subsTermCallbackMsgID;
    scbp->subsTermCallback = reg_datap->subsTermCallback;
    scbp->subsTermCallbackMsgID = reg_datap->subsTermCallbackMsgID;

    scbp->smState = SUBS_STATE_REGISTERED;
    internalRegistrations++;
    return (0);
}

/********************************************************
 * Application Generated Subscribe Request
 ********************************************************/
int
subsmanager_handle_ev_app_subscribe (cprBuffer_t buf)
{
    const char     *fname = "subsmanager_handle_ev_app_subscribe";
    sipspi_subscribe_t *sub_datap;
    sipSCB_t       *scbp = NULL;
    int             scb_index;
    ccsip_sub_not_data_t subs_result_data;
    boolean         reSubscribe = FALSE;
    ccsipCCB_t     *ccbp = NULL;
    sipspi_msg_t   *pSIPSPIMsg = NULL;
    int             subscription_expires;

    pSIPSPIMsg = (sipspi_msg_t *) buf;

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Processing a new App subscription request", DEB_F_PREFIX_ARGS(SIP_SUB, fname));

    if (!subsManagerRunning) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Subscription Manager Not Initialized!", fname);
        return SIP_ERROR;
    }

    // Get Data
    sub_datap = &(pSIPSPIMsg->msg.subscribe);

    /* Steps:
     * * Extract subscription details from the event information
     * * Check data to ensure we support the event package asked for
     * * Allocate an SCB to hold subscription details
     * * Create body of the SUBSCRIBE message, if needed
     * * Prep the message by adding fields to the SCB
     * * Call sipSPISendSubscribe to send the message out
     */

    // Get ready for any failure
    subs_result_data.u.subs_result_data.expires = 0;
    subs_result_data.u.subs_result_data.status_code = SUBSCRIBE_REQUEST_FAILED;
    subs_result_data.sub_id = CCSIP_SUBS_INVALID_SUB_ID;
    subs_result_data.request_id = sub_datap->request_id;
    subs_result_data.msg_id = sub_datap->subsResCallbackMsgID;

    /*
     * Finding SCB by sub_id or request id and eventPakage if the
     * sub_id is not known.
     */
    if (sub_datap->sub_id == CCSIP_SUBS_INVALID_SUB_ID) {
        /*
         * find scb based on request_id and event package.
         * This scenario is possible if application decides to terminate a
         * subscription before subsmanager provides app with sub_id.
         */
        for (scb_index = 0; scb_index < MAX_SCBS; scb_index++) {
            if ((subsManagerSCBS[scb_index].request_id == sub_datap->request_id) &&
                (subsManagerSCBS[scb_index].hb.event_type == sub_datap->eventPackage) &&
                (!subsManagerSCBS[scb_index].pendingClean)) {
                scbp = &(subsManagerSCBS[scb_index]);
                break;
            }
        }
    } else {
        /* Find SCB from sub_id */
        scbp = find_scb_by_sub_id(sub_datap->sub_id, &scb_index);
    }
    if (scbp == NULL) {
        // Process new subscription
        if ((sub_datap->eventPackage != CC_SUBSCRIPTIONS_DIALOG) &&
            (sub_datap->eventPackage != CC_SUBSCRIPTIONS_KPML) &&
            (sub_datap->eventPackage != CC_SUBSCRIPTIONS_PRESENCE)) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Event %d not supported!",
                              fname, sub_datap->eventPackage);
            subs_result_data.u.subs_result_data.status_code = SUBSCRIBE_FAILED_BADEVENT;
            sip_send_error_message(&subs_result_data, sub_datap->subsNotCallbackTask,
                                   sub_datap->subsResCallbackMsgID, sub_datap->subsResultCallback,
                                   fname);
            return SIP_ERROR;
        }

        // Reject this if there is no way to get back to the subscriber
        if (!((sub_datap->subsResultCallback) ||
              ((sub_datap->subsNotCallbackTask != CC_SRC_MIN) &&
               sub_datap->subsResCallbackMsgID))) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"No callback info provided by the App", fname);
            subs_result_data.u.subs_result_data.status_code =
                SUBSCRIBE_FAILED_BADINFO;
            sip_send_error_message(&subs_result_data,
                                   sub_datap->subsNotCallbackTask,
                                   sub_datap->subsResCallbackMsgID,
                                   sub_datap->subsResultCallback,
                                   fname);
            return SIP_ERROR;
        }

        // If this is a presence request - check if sufficient SCBs will still
        // be available for other "more important" functions
        if (sub_datap->eventPackage == CC_SUBSCRIPTIONS_PRESENCE) {
            if (currentScbsAllocated >= LIMIT_SCBS_USAGE) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"reached Presence SCBs threshold", fname);
                subs_result_data.u.subs_result_data.status_code =
                    SUBSCRIBE_FAILED_NORESOURCE;
                sip_send_error_message(&subs_result_data,
                                       sub_datap->subsNotCallbackTask,
                                       sub_datap->subsResCallbackMsgID,
                                       sub_datap->subsResultCallback, fname);
                return SIP_ERROR;
            }
        }

        // Allocate SCB and copy needed parameters
        scbp = allocate_scb(&scb_index);
        if (!scbp) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"ran out of SCBs", fname);
            subs_result_data.u.subs_result_data.status_code =
                SUBSCRIBE_FAILED_NORESOURCE;
            sip_send_error_message(&subs_result_data,
                                   sub_datap->subsNotCallbackTask,
                                   sub_datap->subsResCallbackMsgID,
                                   sub_datap->subsResultCallback,
                                   fname);
            show_scbs_inuse();
            return SIP_ERROR;
        }
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Allocated SCB for Sending Subscribe,"
                         " event=%d scb=%d sub_id=%x\n",
                         DEB_F_PREFIX_ARGS(SIP_SUB, fname), sub_datap->eventPackage,
                         scb_index, scbp->sub_id);
        if (sub_datap->dn_line == 0 || sub_datap->dn_line > MAX_REG_LINES) {
            // By not giving a DN line, the app is asking us to use
            // device addressing and not line addressing
            scbp->hb.dn_line = 1;
            scbp->useDeviceAddressing = TRUE;
        } else {
            scbp->hb.dn_line = sub_datap->dn_line;
        }

        scbp->gsm_id = sub_datap->call_id;
        if (scbp->gsm_id != 0) {
            ccbp = sip_sm_get_ccb_by_gsm_id(scbp->gsm_id);
        } else {
            ccbp = NULL;
        }
        scbp->ccbp = ccbp;

        scbp->hb.event_type = sub_datap->eventPackage;
        scbp->hb.accept_type = sub_datap->acceptPackage;
        if (sub_datap->eventPackage - CC_SUBSCRIPTIONS_DIALOG > -1 &&
            sub_datap->eventPackage - CC_SUBSCRIPTIONS_DIALOG < 5) {
            sstrncpy(scbp->event_name, eventNames[sub_datap->eventPackage - CC_SUBSCRIPTIONS_DIALOG], MAX_EVENT_NAME_LEN);
        }
        sstrncpy(scbp->SubURIOriginal, sub_datap->subscribe_uri,
                 sizeof(scbp->SubURIOriginal));
        sstrncpy(scbp->SubscriberURI, sub_datap->subscriber_uri,
                 sizeof(scbp->SubscriberURI));
        scbp->subsResultCallback    = sub_datap->subsResultCallback;
        scbp->notifyIndCallback     = sub_datap->notifyIndCallback;
        scbp->subsTermCallback      = sub_datap->subsTermCallback;
        scbp->subsNotCallbackTask   = sub_datap->subsNotCallbackTask;
        scbp->subsResCallbackMsgID  = sub_datap->subsResCallbackMsgID;
        scbp->notIndCallbackMsgID   = sub_datap->subsNotIndCallbackMsgID;
        scbp->subsTermCallbackMsgID = sub_datap->subsTermCallbackMsgID;

        scbp->hb.dest_sip_addr = sub_datap->dest_sip_addr;
        scbp->hb.dest_sip_port = sub_datap->dest_sip_port;

        scbp->auto_resubscribe = sub_datap->auto_resubscribe;
        scbp->norefersub = sub_datap->norefersub;
        scbp->request_id = sub_datap->request_id;

        // Set default value of subscribe duration, if not specified
        if (sub_datap->duration < 0) {
            config_get_value(CFGID_TIMER_SUBSCRIBE_EXPIRES, &subscription_expires,
                             sizeof(subscription_expires));
            sub_datap->duration = subscription_expires;
        }
        scbp->internal = TRUE;
    } else {
        /* SCB exists, it is a re-subscribe */
        reSubscribe = TRUE;
    }

    scbp->hb.expires = sub_datap->duration;
    scbp->hb.orig_expiration = sub_datap->duration;

    if (scbp->hb.event_data_p) {
        free_event_data(scbp->hb.event_data_p);
        scbp->hb.event_data_p = NULL;
    }

    // Copy any body received - it will be framed later
    if (sub_datap->eventData) {
        scbp->hb.event_data_p = sub_datap->eventData;
        sub_datap->eventData = NULL;
    }

    //re-initialize cred_type
    scbp->hb.authen.cred_type = 0;

    if (sipSPISendSubscribe(scbp, reSubscribe, FALSE /* auth */)) {
        if (scbp->smState == SUBS_STATE_RCVD_NOTIFY) {
            scbp->smState = SUBS_STATE_SENT_SUBSCRIBE_RCVD_NOTIFY;
        } else {
            scbp->smState = SUBS_STATE_SENT_SUBSCRIBE;
        }
        outgoingSubscribes++;
        if (!reSubscribe) {
            outgoingSubscriptions++;
        }
        return (0);
    }
    // If unable to send subscribe, return error immediately
    // and return scb to the pool if not resubscribing
    CCSIP_DEBUG_ERROR(SIP_F_PREFIX"failed to send SUBSCRIBE message", fname);
    sip_send_error_message(&subs_result_data, scbp->subsNotCallbackTask,
                           scbp->subsResCallbackMsgID,
                           scbp->subsResultCallback, fname);

    if (!reSubscribe) {
        free_scb(scb_index, fname);
    }
    return SIP_ERROR;
}

/***********************************************************
 * Handle Application Response to a Remote Subscribe Request
 ***********************************************************/
int
subsmanager_handle_ev_app_subscribe_response (cprBuffer_t buf)
{
    const char     *fname = "subsmanager_handle_ev_app_subscribe_response";
    sipspi_subscribe_resp_t *subres_datap;
    sipSCB_t       *scbp;
    sipspi_msg_t   *pSIPSPIMsg = NULL;


    pSIPSPIMsg = (sipspi_msg_t *) buf;

    subres_datap = &(pSIPSPIMsg->msg.subscribe_resp);

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Processing an app subscribe response for"
                     " sub_id=%x\n", DEB_F_PREFIX_ARGS(SIP_SUB, fname), subres_datap->sub_id);
    /*
     * Find SCB from the sub_id.
     */
    scbp = find_scb_by_sub_id(subres_datap->sub_id, NULL);
    if (scbp == NULL) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"no SCB for sub_id=%x found",
                          fname, subres_datap->sub_id);
        return SIP_ERROR;
    }

    scbp->hb.expires = subres_datap->duration;
    if (sipSPISendSubscribeNotifyResponse
        (scbp, subres_datap->response_code, scbp->last_recv_request_cseq)) {
        if (scbp->smState == SUBS_STATE_RCVD_SUBSCRIBE_SENT_NOTIFY) {
            scbp->smState = SUBS_STATE_SENT_NOTIFY;
        } else {
            scbp->smState = SUBS_STATE_ACTIVE;
        }
    } else {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"failed to send SUBSCRIBE Response", fname);
        return SIP_ERROR;
    }
    return (0);
}

/********************************************************
 * Handle Application Generated NOTIFY
 ********************************************************/
int
subsmanager_handle_ev_app_notify (cprBuffer_t buf)
{
    const char     *fname = "subsmanager_handle_ev_app_notify";
    sipspi_notify_t *not_datap;
    sipSCB_t       *scbp;
    ccsip_sub_not_data_t notify_result_data;
    sipspi_msg_t   *pSIPSPIMsg = NULL;
    sipspi_msg_t   *temp_SIPSPIMsg = NULL;

    pSIPSPIMsg = (sipspi_msg_t *) buf;

    not_datap = &(pSIPSPIMsg->msg.notify);

    // Fill in the return data structure in case we encounter any problems
    notify_result_data.u.notify_result_data.status_code = NOTIFY_REQUEST_FAILED;
    notify_result_data.msg_id = not_datap->subsNotResCallbackMsgID;
    notify_result_data.sub_id = not_datap->sub_id;

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Processing an app notify request for"
                     " sub_id=%x\n", DEB_F_PREFIX_ARGS(SIP_SUB, fname),
                     not_datap->sub_id);
    /*
     * Find SCB from the sub_id.
     */
    scbp = find_scb_by_sub_id(not_datap->sub_id, NULL);
    if (scbp == NULL) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"no SCB for sub_id=%x found", fname,
                          not_datap->sub_id);
        free_event_data(not_datap->eventData);
        sip_send_error_message(&notify_result_data,
                               not_datap->subsNotCallbackTask,
                               not_datap->subsNotResCallbackMsgID,
                               not_datap->notifyResultCallback,
                               fname);
        return SIP_ERROR;
    }

    notify_result_data.line_id = scbp->hb.dn_line;

    // Check state to see if we need to queue this request
    if ((scbp->smState == SUBS_STATE_SENT_NOTIFY) ||
        (scbp->smState == SUBS_STATE_RCVD_SUBSCRIBE_SENT_NOTIFY)) {
        // This means we have sent a NOTIFY but have not received a response
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Queueing request for later transmission", DEB_F_PREFIX_ARGS(SIP_SUB, fname));
        temp_SIPSPIMsg = (sipspi_msg_t *) cpr_malloc(sizeof(sipspi_msg_t));
        if (temp_SIPSPIMsg) {
            /* Copy the content so that we do not touch the pSIPSPImsg */
            (*temp_SIPSPIMsg) = (*pSIPSPIMsg);

            /* Append the request */
            if (append_pending_requests(scbp, temp_SIPSPIMsg,
                SIPSPI_EV_CC_NOTIFY)) {
                return SIP_DEFER;
            }
            cpr_free(temp_SIPSPIMsg);
        }
        /* We either do not have buffer or failed to append msg. */
        free_event_data(not_datap->eventData);
        sip_send_error_message(&notify_result_data,
                               not_datap->subsNotCallbackTask,
                               not_datap->subsNotResCallbackMsgID,
                               not_datap->notifyResultCallback,
                               fname);
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to queue request", fname);
        return SIP_ERROR;
    }

    // Check state to see if we are in a position to send this NOTIFY
    if (scbp->smState == SUBS_STATE_IDLE) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Bad SCB State: %d", fname, scbp->smState);
        free_event_data(not_datap->eventData);
        sip_send_error_message(&notify_result_data,
                               not_datap->subsNotCallbackTask,
                               not_datap->subsNotResCallbackMsgID,
                               not_datap->notifyResultCallback, fname);
        return SIP_ERROR;
    }

    // Copy the callback function, if not already copied
    if (not_datap->notifyResultCallback == NULL &&
        not_datap->subsNotResCallbackMsgID == 0) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"No callback event or function", fname);
        // Can't really send back any error ...
        free_event_data(not_datap->eventData);
        return SIP_ERROR;
    } else {
        scbp->notifyResultCallback = not_datap->notifyResultCallback;
        scbp->notResCallbackMsgID = not_datap->subsNotResCallbackMsgID;
    }

    if (scbp->hb.event_data_p) {
        free_event_data(scbp->hb.event_data_p);
        scbp->hb.event_data_p = NULL;
    }

    // Copy any body received - it will be framed later
    if (not_datap->eventData) {
        scbp->hb.event_data_p = not_datap->eventData;
        not_datap->eventData = NULL;
    }

    // Find out if app wants to terminate this subscription
    if (not_datap->subState == SUBSCRIPTION_TERMINATE) {
        scbp->hb.expires = 0;
    }

    //re-initialize cred_type
    scbp->hb.authen.cred_type = 0;

    if (sipSPISendSubNotify((ccsip_common_cb_t *)scbp, FALSE) != TRUE) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"failed to send Notify Message", fname);
        sip_send_error_message(&notify_result_data, scbp->subsNotCallbackTask,
                               scbp->notResCallbackMsgID,
                               scbp->notifyResultCallback, fname);
        return SIP_ERROR;
    }

    if (scbp->smState == SUBS_STATE_RCVD_SUBSCRIBE) {
        scbp->smState = SUBS_STATE_RCVD_SUBSCRIBE_SENT_NOTIFY;
    } else {
        scbp->smState = SUBS_STATE_SENT_NOTIFY;
    }
    outgoingNotifies++;

    return (0);
}

/**
  * This function will handle Application Generated unsolicited NOTIFY
  *
  * @param[in] buf - pointer to sipspi_msg_t
  * @param[in] line - line id.
  *
  * @return none
  */
void subsmanager_handle_ev_app_unsolicited_notify (cprBuffer_t buf, line_t line)
{
    const char     *fname = "subsmanager_handle_ev_app_unsolicited_notify";
    sipspi_msg_t   *pSIPSPIMsg = NULL;
    sipspi_notify_t *not_datap;
    sipTCB_t       *tcbp;
    int nat_enable = 0;
    static uint32_t trxn_id = 1;

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Processing an outgoing unsolicited notify request",
                     DEB_F_PREFIX_ARGS(SIP_SUB, fname));

    pSIPSPIMsg = (sipspi_msg_t *) buf;
    not_datap = &(pSIPSPIMsg->msg.notify);

    /*
     * If TCB list is not created yet, create the list.
     */
    if (s_TCB_list == NULL) {
        s_TCB_list = sll_create(NULL);
        if (s_TCB_list == NULL) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"malloc of TCBList failed", fname);
            free_event_data(not_datap->eventData);
            return;
        }
    }

    /*
     *  create a TCB (transaction control block)
     */
    tcbp = cpr_malloc(sizeof(sipTCB_t));
    if (tcbp == NULL) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"malloc of TCB failed", fname);
        free_event_data(not_datap->eventData);
        return;
    }
    memset(tcbp, 0, sizeof(sipTCB_t));
    tcbp->trxn_id = trxn_id;
    trxn_id++;
    if (trxn_id == 0) {
        trxn_id = 1;
    }
    tcbp->timer = cprCreateTimer("Unsolicited transaction timer",
                                 SIP_UNSOLICITED_TRANSACTION_TIMER,
                                 TIMER_EXPIRATION,
                                 sip_msgq);
    if (tcbp->timer == NULL) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"failed to create a timer", fname);
        free_event_data(not_datap->eventData);
        cpr_free(tcbp);
        return;
    }
    tcbp->hb.cb_type = UNSOLICIT_NOTIFY_CB;
    config_get_value(CFGID_NAT_ENABLE, &nat_enable, sizeof(nat_enable));
    if (nat_enable == 0) {
        sip_config_get_net_device_ipaddr(&(tcbp->hb.src_addr));
    } else {
        sip_config_get_nat_ipaddr(&(tcbp->hb.src_addr));
    }
    tcbp->hb.dn_line = line;
    tcbp->hb.local_port = sipTransportGetListenPort(tcbp->hb.dn_line, NULL);

    tcbp->hb.event_type = not_datap->eventPackage;

    // Copy any body received - it will be framed later
    if (not_datap->eventData) {
        tcbp->hb.event_data_p = not_datap->eventData;
        not_datap->eventData = NULL;
    }
    (void) sll_append(s_TCB_list, tcbp);

    if (sipSPISendSubNotify((ccsip_common_cb_t *)tcbp, FALSE) != TRUE) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"failed to send Notify Message", fname);
        free_tcb(tcbp);
        return;
    }

    outgoingUnsolicitedNotifies++;

    return;
}

/**
  * This function will handle network generated response to unsolicited NOTIFY
  *
  * @param[in] pSipMessage - pointer to sipMessage_t
  * @param[in] tcbp -  pointer to associated trnsaction control block.
  *
  * @returns SIP_OK/SIP_ERROR
  */
int subsmanager_handle_ev_sip_unsolicited_notify_response (sipMessage_t *pSipMessage, sipTCB_t *tcbp)
{
    int             response_code = 0;
    const char     *fname = "subsmanager_handle_ev_sip_unsolicited_notify_response";

    // Parse the return code
    (void) sipGetResponseCode(pSipMessage, &response_code);

    /*
     * if the response is < 200, do nothing.
     */
    if (response_code < 200) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"received %d response", DEB_F_PREFIX_ARGS(SIP_SUB, fname), response_code);
        return SIP_OK;
    }

    if ((response_code == SIP_CLI_ERR_UNAUTH) ||
        (response_code == SIP_CLI_ERR_PROXY_REQD)) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Authentication Required", DEB_F_PREFIX_ARGS(SIP_SUB, fname));
        if (ccsip_common_util_generate_auth(pSipMessage, &tcbp->hb, SIP_METHOD_NOTIFY,
                                            response_code, tcbp->full_ruri) == TRUE) {
            if (sipSPISendSubNotify((ccsip_common_cb_t *)tcbp, TRUE) == TRUE) {
                CCSIP_DEBUG_TASK(DEB_F_PREFIX"sent request with Auth header", DEB_F_PREFIX_ARGS(SIP_SUB, fname));
                return SIP_OK;
             }
        }
        free_tcb (tcbp);
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"failed to respond to auth challenge", fname);
        return SIP_ERROR;
    }

    free_tcb(tcbp);
    CCSIP_DEBUG_TASK(DEB_F_PREFIX"received %d response", DEB_F_PREFIX_ARGS(SIP_SUB, fname), response_code);
    return SIP_OK;
}

/**
  * This function will handle outgoing unsolicited NOTIFY transaction timeout.
  *
  * @param[in] data - pointer to an id that identifies the TCB.
  *
  * @returns none.
  */
void subsmanager_unsolicited_notify_timeout (void *data)
{
    const char     *fname = "subsmanager_unsolicited_notify_timeout";
    uint32_t trxn_id = (long)data;
    sipTCB_t *temp_tcbp = NULL;

    /*
     * make sure that the TCB still exists.
     */
    temp_tcbp = (sipTCB_t *)sll_next(s_TCB_list, NULL);
    while (temp_tcbp != NULL) {
        if (temp_tcbp->trxn_id == trxn_id) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"unsolicited notify transaction timedout", fname);
            free_tcb(temp_tcbp);
            return;
        }
        temp_tcbp = (sipTCB_t *)sll_next(s_TCB_list, temp_tcbp);
    }
}

/**********************************************************
 * Handle Application Response to a received Notify request
 **********************************************************/
int
subsmanager_handle_ev_app_notify_response (cprBuffer_t buf)
{
    sipspi_notify_resp_t *notify_resp;
    sipSCB_t       *scbp = NULL;
    sipspi_msg_t   *pSIPSPIMsg = NULL;
    uint32_t        cseq;
    const char     *fname = "subsmanager_handle_ev_app_notify_response";

    pSIPSPIMsg = (sipspi_msg_t *) buf;

    notify_resp = &(pSIPSPIMsg->msg.notify_resp);

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Processing an app notify response for"
                     " sub_id=%x\n", DEB_F_PREFIX_ARGS(SIP_SUB, fname),
                     notify_resp->sub_id);

    // Retrieve response parameters
    /*
     * Find SCB from the sub_id.
     */
    scbp = find_scb_by_sub_id(notify_resp->sub_id, NULL);
    if (scbp == NULL) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"no SCB for sub_id=%x found",
                          fname, notify_resp->sub_id);
        return SIP_ERROR;
    }

    if (notify_resp->cseq == 0) {
        cseq = scbp->last_recv_request_cseq;
    } else {
        cseq = notify_resp->cseq;
    }
    // Call function to make and send the response
    if (sipSPISendSubscribeNotifyResponse(scbp,
                (uint16_t)(notify_resp->response_code), cseq)) {
        /*
         * if the outstanding NOTIFY transaction is only one,
         * then update the scbp->smState.
         */
        if (scbp->outstandingIncomingNotifyTrxns == 1) {
            if (scbp->smState == SUBS_STATE_SENT_SUBSCRIBE_RCVD_NOTIFY) {
                scbp->smState = SUBS_STATE_SENT_SUBSCRIBE;
            } else {
                scbp->smState = SUBS_STATE_ACTIVE;
            }
        }
        scbp->outstandingIncomingNotifyTrxns -= 1;
        return (0);
    }
    return SIP_ERROR;
}

/********************************************************
 * Handle Application Request to Terminate Subscription
 ********************************************************/
int
subsmanager_handle_ev_app_subscription_terminated (cprBuffer_t buf)
{
    /*
     * This function is used by the application to clean up a subscription
     * whether internally or externally initiated. The application should have
     * taken care of all the protocol related messaging before calling this
     * function.
     */
    const char     *fname = "subsmanager_handle_ev_app_subscription_terminated";
    sipspi_subscribe_term_t *subs_term;
    int             scb_index;
    sipSCB_t       *scbp;
    sipspi_msg_t   *pSIPSPIMsg = NULL;


    pSIPSPIMsg = (sipspi_msg_t *) buf;

    subs_term = &(pSIPSPIMsg->msg.subs_term);

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Processing terminate request for sub_id=%x",
                     DEB_F_PREFIX_ARGS(SIP_SUB, fname), subs_term->sub_id);
    /*
     * Find SCB from the sub_id and allow matching with request ID and
     * eventPackage if sub id is not known.
     */
    if (subs_term->sub_id == CCSIP_SUBS_INVALID_SUB_ID) {
        /*
         * find scb based on request_id and event package.
         * This scenario is possible if application decides to terminate a
         * subscription before subsmanager provides app with sub_id.
         */
        for (scb_index = 0; scb_index < MAX_SCBS; scb_index++) {
            if ((subsManagerSCBS[scb_index].request_id == subs_term->request_id) &&
                (subsManagerSCBS[scb_index].hb.event_type == subs_term->eventPackage) &&
                (!subsManagerSCBS[scb_index].pendingClean)) {
                break;
            }
        }
        if (scb_index >= MAX_SCBS) {
            scbp = NULL;
        } else {
            scbp = &(subsManagerSCBS[scb_index]);
        }
    } else {
        /* Find SCB by sub_id */
        scbp = find_scb_by_sub_id(subs_term->sub_id, &scb_index);
    }
    if (scbp == NULL) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"no SCB for sub_id=%x or request id %ld"
                          " and eventPackage %d found", fname,
                          subs_term->sub_id, subs_term->request_id,
                          subs_term->eventPackage);
        return SIP_ERROR;
    }
    if (scbp->smState == SUBS_STATE_IDLE || scbp->pendingClean) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"SCB: scb=%d sub_id=%x has already been"
                          " cleaned up\n", fname,
                          scb_index, subs_term->sub_id);
        return 0;
    }

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Cleaning out subscription for SCB: scb=%d sub_id=%x",
                     DEB_F_PREFIX_ARGS(SIP_SUB, fname), scb_index, scbp->sub_id);

    if (scbp->internal) {
        outgoingSubscriptions--;
    } else {
        incomingSubscriptions--;
    }

    // There could still be messages left to receive and flush, so if not
    // immediate, we mark this SCB as pending deletion and we will clean it up
    // later
    if (subs_term->immediate) {
        free_scb(scb_index, fname);
    } else {
        scbp->pendingClean = TRUE;
        if (scbp->pendingRequests)
            scbp->pendingCount = 2 * TMR_PERIODIC_SUBNOT_INTERVAL;
        else
            scbp->pendingCount = 1 * TMR_PERIODIC_SUBNOT_INTERVAL;
    }

    return (0);
}


/********************************************************
 * Handle network response to a Sub/Not request
 ********************************************************/
int
subsmanager_handle_ev_sip_response (sipMessage_t *pSipMessage)
{
    const char     *fname = "subsmanager_handle_ev_sip_response";
    sipSCB_t       *scbp;
    int             scb_index;
    ccsip_sub_not_data_t sub_not_result_data;

    // currently not used  SysHdr *pSm = NULL;
    int             response_code = 0;
    const char     *pCallID = NULL;
    const char     *rsp_method = NULL;
    sipMethod_t     method = sipMethodInvalid;
    sipStatusCodeClass_t code_class = codeClassInvalid;
    const char     *expires = NULL;
    long            expiry_time;
    const char     *to = NULL, *from = NULL;
    const char     *record_route = NULL;
    sipLocation_t  *to_loc = NULL;
    sipCseq_t      *resp_cseq_structure = NULL;
    uint32_t        cseq;

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Processing a response", DEB_F_PREFIX_ARGS(SIP_SUB, fname));

    if (sipGetResponseMethod(pSipMessage, &method) < 0) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sipGetResponseMethod");
        return SIP_ERROR;
    }
    if (method == sipMethodSubscribe) {
        rsp_method = SIP_METHOD_SUBSCRIBE;
    } else if (method == sipMethodRefer) {
        rsp_method = SIP_METHOD_REFER;
    } else {
        rsp_method = SIP_METHOD_NOTIFY;
    }

    pCallID = sippmh_get_cached_header_val(pSipMessage, CALLID);
    if (!pCallID) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Cannot obtain SIP Call ID.", fname);
        return SIP_ERROR;
    }

    if (!getCSeqInfo(pSipMessage, &resp_cseq_structure)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Cannot obtain SIP CSEQ.", fname);
        return SIP_ERROR;
    }
    cseq = resp_cseq_structure->number;
    cpr_free(resp_cseq_structure);

    // Locate request scbp from that match the response
    scbp = find_req_scb(pCallID, method, cseq, &scb_index);
    if (!scbp) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"No matching request found", fname);
        return SIP_ERROR;
    }
    // Cancel any outstanding retry timer
    if (scbp->hb.retx_flag == TRUE) {
        sip_platform_msg_timer_subnot_stop(&sipPlatformUISMSubNotTimers[scb_index]);
        scbp->hb.retx_flag = FALSE;
    }
    // Parse the return code
    (void) sipGetResponseCode(pSipMessage, &response_code);
    code_class = sippmh_get_code_class((uint16_t) response_code);

    /*
     * If it is a response (18x/2xx) to a dialog initiating SUBSCRIBE,
     * parse the Record-Route header and set up Route set for this dialog.
     * If the sip_to_tag is not yet populated and if there is no associated
     * CCB, then this can be considered as a response to dialog
     * initating SUBSCRIBE.
     */
    if ((strcmp(rsp_method, SIP_METHOD_SUBSCRIBE) == 0) &&
        (scbp->ccbp == NULL) && (scbp->sip_to_tag[0] == '\0')) {
        if (((response_code >= 180) && (response_code <= 189)) ||
            ((response_code >= 200) && (response_code <= 299))) {
            record_route = sippmh_get_cached_header_val(pSipMessage,
                                                        RECORD_ROUTE);
            if (record_route) {
                if (scbp->record_route_info) {
                    sippmh_free_record_route(scbp->record_route_info);
                }
                scbp->record_route_info = sippmh_parse_record_route(record_route);
                if (scbp->record_route_info == NULL) {
                    CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sippmh_parse_record_route() failed",
                                      fname);
                    return SIP_ERROR;
                }
            }
        }
    }

    if ((response_code == SIP_CLI_ERR_UNAUTH) ||
        (response_code == SIP_CLI_ERR_PROXY_REQD)) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Authentication Required", DEB_F_PREFIX_ARGS(SIP_SUB, fname));
        if (ccsip_common_util_generate_auth(pSipMessage, (ccsip_common_cb_t *)scbp, rsp_method,
                                            response_code, scbp->SubURI) == TRUE) {
            // Send Sub/Not message again - this time with authorization
            if ((method == sipMethodSubscribe) || (method == sipMethodRefer)) {
                (void) sipSPISendSubscribe(scbp, TRUE, TRUE);
            } else {
                (void) sipSPISendSubNotify((ccsip_common_cb_t *)scbp, TRUE);
            }
        } else {
            // Return error to caller
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"failed to generate Auth header", fname);
            return SIP_ERROR;
        }
        return (0);
    }

    // If this subscription has been cleaned by the application, then
    // return SCB to IDLE state and discard this response
    if (scbp->pendingClean) {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Recd msg for terminated sub", DEB_F_PREFIX_ARGS(SIP_SUB, fname));
        if (scbp->pendingRequests) {
            scbp->pendingCount += TMR_PERIODIC_SUBNOT_INTERVAL;
            scbp->smState = SUBS_STATE_ACTIVE;
            handle_pending_requests(scbp);
        } else {
            free_scb(scb_index, fname);
        }
        return (0);
    }

    /*
     * if response code is 423, grab Min-Expires and let app know so
     * that it can be used for subsequent subscriptions.
     */
    if ((response_code == SIP_CLI_ERR_INTERVAL_TOO_SMALL) &&
        (strcmp(rsp_method, SIP_METHOD_SUBSCRIBE) == 0)) {
        expires = sippmh_get_header_val(pSipMessage,
                                        (const char *)SIP_HEADER_MIN_EXPIRES,
                                        NULL);
        if (expires) {
            expiry_time = strtoul(expires, NULL, 10);
            //ensure new Min-Expires is > what we set before in Expires
            if ((long) expiry_time > scbp->hb.expires) {
                scbp->hb.expires = expiry_time;
            }
        }
    } else {
        // Get the expires header value and add to scb
        expires = sippmh_get_header_val(pSipMessage, SIP_HEADER_EXPIRES, NULL);
        if (expires) {
            expiry_time = strtoul(expires, NULL, 10);
            scbp->hb.expires = expiry_time;
        }
    }

    // update to and from headers to capture the tag
    if (strcmp(rsp_method, SIP_METHOD_SUBSCRIBE) == 0) {
        to = sippmh_get_cached_header_val(pSipMessage, TO);
        from = sippmh_get_cached_header_val(pSipMessage, FROM);
        scbp->sip_to = strlib_update(scbp->sip_to, to);
        // grab the to-tag if present, and if this is a response to a SUBSCRIBE
        to_loc = sippmh_parse_from_or_to((char *) to, TRUE);
        if (to_loc != NULL) {
            if (to_loc->tag != NULL) {
                scbp->sip_to_tag = strlib_update(scbp->sip_to_tag,
                                                 sip_sm_purify_tag(to_loc->tag));
            }
            sippmh_free_location(to_loc);
        }
        scbp->sip_from = strlib_update(scbp->sip_from, from);
    }

    // Delete body, if any, since it is no longer needed
    if (code_class > codeClass1xx) {
        if (scbp->hb.event_data_p) {
            free_event_data(scbp->hb.event_data_p);
            scbp->hb.event_data_p = NULL;
        }
    }

    if ((scbp->smState == SUBS_STATE_SENT_SUBSCRIBE_RCVD_NOTIFY) ||
        (scbp->smState == SUBS_STATE_SENT_SUBSCRIBE)) {
        sub_not_result_data.u.subs_result_data.expires = scbp->hb.expires;
        sub_not_result_data.u.subs_result_data.status_code = response_code;
        sub_not_result_data.request_id = scbp->request_id;
        sub_not_result_data.sub_id = scbp->sub_id;
        sub_not_result_data.msg_id = scbp->subsResCallbackMsgID;
        sub_not_result_data.gsm_id = scbp->gsm_id;
        sub_not_result_data.line_id = scbp->hb.dn_line;

        if (code_class > codeClass1xx) {
            if (scbp->smState == SUBS_STATE_SENT_SUBSCRIBE_RCVD_NOTIFY) {
                scbp->smState = SUBS_STATE_RCVD_NOTIFY;
            } else {
                scbp->smState = SUBS_STATE_ACTIVE;
            }
        }

        if (scbp->subsResultCallback) {
            (scbp->subsResultCallback) (&sub_not_result_data);
        } else if (scbp->subsNotCallbackTask != CC_SRC_MIN) {
            (void) sip_send_message(&sub_not_result_data,
                                    scbp->subsNotCallbackTask,
                                    scbp->subsResCallbackMsgID);
        }
    } else if ((scbp->smState == SUBS_STATE_SENT_NOTIFY) ||
               (scbp->smState == SUBS_STATE_RCVD_SUBSCRIBE_SENT_NOTIFY)) {
        sub_not_result_data.u.notify_result_data.status_code = response_code;
        sub_not_result_data.request_id = scbp->request_id;
        sub_not_result_data.sub_id = scbp->sub_id;
        sub_not_result_data.msg_id = scbp->notResCallbackMsgID;
        sub_not_result_data.gsm_id = scbp->gsm_id;
        sub_not_result_data.line_id = scbp->hb.dn_line;

        if (code_class > codeClass1xx) {
            if (scbp->smState == SUBS_STATE_RCVD_SUBSCRIBE_SENT_NOTIFY) {
                scbp->smState = SUBS_STATE_RCVD_SUBSCRIBE;
            } else {
                scbp->smState = SUBS_STATE_ACTIVE;
            }
        }

        if (scbp->notifyResultCallback) {
            (scbp->notifyResultCallback) (&sub_not_result_data);
        } else if (scbp->subsNotCallbackTask != CC_SRC_MIN) {
            (void) sip_send_message(&sub_not_result_data,
                                    scbp->subsNotCallbackTask,
                                    scbp->notResCallbackMsgID);
        }
        // If there are pending requests - handle them now
        if (scbp->pendingRequests) {
            handle_pending_requests(scbp);
        }
    } else {
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Incorrect SCB State", DEB_F_PREFIX_ARGS(SIP_SUB, fname));
        return SIP_ERROR;
    }

    return (0);
}

/********************************************************
 * Handle Network Received Subscribe Request
 ********************************************************/
int
subsmanager_handle_ev_sip_subscribe (sipMessage_t *pSipMessage,
                                     sipMethod_t sipMethod,
                                     boolean in_dialog)
{
    const char     *fname = "subsmanager_handle_ev_sip_subscribe";
    const char     *event = NULL;
    cc_subscriptions_t eventPackage = CC_SUBSCRIPTIONS_NONE;
    sipSCB_t       *scbpReg = NULL, *scbp = NULL;
    const char     *callID = NULL, *via = NULL;
    const char     *contact = NULL;
    const char     *record_route = NULL;
    const char     *expires = NULL;
    const char     *require = NULL, *supported = NULL;
    sipCseq_t      *request_cseq_structure = NULL;
    unsigned long   request_cseq_number = 0, expiry_time = 0;

    // currently not used  unsigned long diff_time = 0;
    sipMethod_t     request_cseq_method = sipMethodInvalid;
    unsigned int    content_length = 0;
    line_t          dn_line = 0;
    boolean         request_uri_error = FALSE;
    sipReqLine_t   *requestURI = NULL;

    // currently not used  sipLocation_t *uri_loc = NULL;
    genUrl_t       *genUrl = NULL;
    const char     *sip_from = NULL;
    const char     *sip_to = NULL;
    sipLocation_t  *to_loc = NULL;
    sipLocation_t  *from_loc = NULL;
    sipUrl_t       *sipUriUrl = NULL, *sipFromUrl = NULL;
    char           *pUser = NULL;
    char           *sip_to_tag_temp, *sip_to_temp;
    char           *kpml_call_id = NULL, *from_tag, *to_tag;
    ccsip_event_data_t *subDatap = NULL;
    int             scb_index;
    boolean         reSubscribe = FALSE;
    ccsip_sub_not_data_t subs_ind_data;
    ccsipCCB_t     *ccb = NULL;
    char           *referToString = NULL;
    uint32_t        tags = 0;
    int             result;
    int             requestStatus = SIP_MESSAGING_ERROR;
    uint8_t         i;
    char            line_contact[MAX_LINE_CONTACT_SIZE];
    char            line_name[MAX_LINE_NAME_SIZE];
    int             noOfReferTo = 0;
    sipServiceControl_t *scp=NULL;

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Processing a new SIP subscription request", DEB_F_PREFIX_ARGS(SIP_SUB, fname));

    if (!subsManagerRunning) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Subscription Manager Not Initialized!", fname);
        return SIP_ERROR;
    }

    /* Steps:
     * * Check request for support by a registered app
     * * If not, reject the request as unsupported
     * * Decode the body, if any, and if understood
     * * Allocate and fill in SCB
     * * Call the callback function by registered application
     */
    if (!in_dialog) {
        requestStatus = sipSPICheckRequest(NULL, pSipMessage);
        if (requestStatus != SIP_MESSAGING_OK) {
            if (requestStatus == SIP_MESSAGING_DUPLICATE) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Recieved duplicate request", fname);
            } else {
                if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                            SIP_CLI_ERR_BAD_REQ_PHRASE,
                                            SIP_WARN_MISC, NULL, NULL) != TRUE) {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                      fname, SIP_CLI_ERR_BAD_REQ);
                }
            }
            return SIP_ERROR;
        }
    }

    memset(&subs_ind_data, 0, sizeof(ccsip_sub_not_data_t));

    // Get relevant parameters from the SIP message
    // These are: Event, Subscription-State, Accept, Content-Length besides
    // To, From, Via, Call-ID, CSeq, and Contact

    event = sippmh_get_header_val(pSipMessage, SIP_HEADER_EVENT,
                                  SIP_C_HEADER_EVENT);

    if (event) {
        if (cpr_strcasecmp(event, "dialog") == 0) {
            eventPackage = CC_SUBSCRIPTIONS_DIALOG;
        } else if (cpr_strncasecmp(event, "kpml", 4) == 0) {
            eventPackage = CC_SUBSCRIPTIONS_KPML;
        } else if (cpr_strcasecmp(event, "presence") == 0) {
            eventPackage = CC_SUBSCRIPTIONS_PRESENCE;
        } else {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unsupported event=%s", fname, event);
            if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_EVENT,
                                        SIP_CLI_ERR_BAD_EVENT_PHRASE,
                                        0, NULL, NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_CLI_ERR_BAD_EVENT);
            }
            return SIP_ERROR;
        }
    } else if (sipMethod == sipMethodRefer) {

	} else if (sipMethod == sipMethodBye) {

            return SIP_ERROR;

    } else {
        // Reached here in error
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"No event header", fname);
        if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_EVENT,
                                    SIP_CLI_ERR_BAD_EVENT_PHRASE,
                                    SIP_WARN_MISC,
                                    NULL,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_EVENT);
        }
        return SIP_ERROR;
    }


    // Parse Call-ID
    callID = sippmh_get_cached_header_val(pSipMessage, CALLID);
    if (!callID) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to obtain request's "
                          "Call-ID header.\n", fname);
        if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                    SIP_CLI_ERR_BAD_REQ_PHRASE,
                                    SIP_WARN_MISC,
                                    SIP_CLI_ERR_BAD_REQ_CALLID_ABSENT,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        return SIP_ERROR;
    }
    // Check content
    content_length = sippmh_get_content_length(pSipMessage);

    if (pSipMessage->raw_body) {
        if (content_length != strlen(pSipMessage->raw_body)) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"\n Mismatched Content length and \
                             Actual message body length:content length=%u \
                             \n and message as %s \
                             \n and strlenof messagebody = %zu", fname,
                             content_length, pSipMessage->raw_body,
                             strlen(pSipMessage->raw_body));
            if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                        SIP_CLI_ERR_BAD_REQ_PHRASE,
                                        SIP_WARN_MISC,
                                        SIP_CLI_ERR_BAD_REQ_CONTENT_LENGTH_ERROR,
                                        NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_CLI_ERR_BAD_REQ);
            }
            return SIP_ERROR;
        }
    } else if (content_length != 0) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"\n Mismatched Content length and \
                             Actual message body length:content length=%d \
                             \n and message is EMPTY \
                             \n and strlenof messagebody = 0\n", fname,
                             content_length);
        if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                    SIP_CLI_ERR_BAD_REQ_PHRASE,
                                    SIP_WARN_MISC,
                                    SIP_CLI_ERR_BAD_REQ_CONTENT_LENGTH_ERROR,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        return SIP_ERROR;
    }
    // Parse CSEQ
    if (getCSeqInfo(pSipMessage, &request_cseq_structure) == FALSE) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to obtain or parse request's CSeq "
                          "header.\n", fname);
        if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                    SIP_CLI_ERR_BAD_REQ_PHRASE,
                                    SIP_WARN_MISC,
                                    SIP_CLI_ERR_BAD_REQ_CSEQ_FIELD,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        return SIP_ERROR;
    }

    request_cseq_number = request_cseq_structure->number;
    request_cseq_method = request_cseq_structure->method;
    cpr_free(request_cseq_structure);

    // Parse From
    sip_from = sippmh_get_cached_header_val(pSipMessage, FROM);
    from_loc = sippmh_parse_from_or_to((char *) sip_from, TRUE);
    if (!from_loc) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname,
                          get_debug_string(DEBUG_FUNCTIONNAME_SIPPMH_PARSE_FROM));
        if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                    SIP_CLI_ERR_BAD_REQ_PHRASE,
                                    SIP_WARN_MISC,
                                    SIP_CLI_ERR_BAD_REQ_FROM_OR_TO_FIELD,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        return SIP_ERROR;
    }
    // Parse To
    sip_to = sippmh_get_cached_header_val(pSipMessage, TO);
    to_loc = sippmh_parse_from_or_to((char *) sip_to, TRUE);
    if (!to_loc) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname,
                          get_debug_string(DEBUG_FUNCTIONNAME_SIPPMH_PARSE_TO));
        if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                    SIP_CLI_ERR_BAD_REQ_PHRASE,
                                    SIP_WARN_MISC,
                                    SIP_CLI_ERR_BAD_REQ_FROM_OR_TO_FIELD,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        sippmh_free_location(from_loc);
        return SIP_ERROR;
    }
    // Parse Req-URI
    requestURI = sippmh_get_request_line(pSipMessage);
    if (requestURI) {
        if (requestURI->url) {
            genUrl = sippmh_parse_url(requestURI->url, TRUE);
            if (genUrl) {
                if (genUrl->schema != URL_TYPE_SIP) {
                    request_uri_error = TRUE;
                }
            } else {
                request_uri_error = TRUE;
            }
        } else {
            request_uri_error = TRUE;
        }

    } else {
        request_uri_error = TRUE;
    }
    if (request_uri_error) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Invalid Request URI" "failed.", fname);
        if (to_loc)
            sippmh_free_location(to_loc);
        if (from_loc)
            sippmh_free_location(from_loc);
        if (genUrl)
            sippmh_genurl_free(genUrl);
        if (requestURI)
            SIPPMH_FREE_REQUEST_LINE(requestURI);
        if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                    SIP_CLI_ERR_BAD_REQ_PHRASE,
                                    SIP_WARN_MISC,
                                    SIP_CLI_ERR_BAD_REQ_URL_ERROR,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        return SIP_ERROR;
    }
    // Parse Expires header
    expires = sippmh_get_header_val(pSipMessage, SIP_HEADER_EXPIRES, NULL);
    if (expires) {
        expiry_time = strtoul(expires, NULL, 10);
    } else {
        // No expires header, use default
        expiry_time = 3600;
    }

    scbp = find_scb_by_subscription(eventPackage, &scb_index, callID);
    if (scbp && eventPackage == scbp->hb.event_type) {
        // SCB is already there - must be a re-subscribe
        reSubscribe = TRUE;
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Received a reSubscribe Message", DEB_F_PREFIX_ARGS(SIP_SUB, fname));
        if (scbp->pendingClean) {
            // Oops, the application has already terminated this subscription
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Subscribe received for subscription "
                              "already terminated by application.\n", fname);
            if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                        SIP_CLI_ERR_BAD_REQ_PHRASE,
                                        SIP_WARN_MISC,
                                        SIP_CLI_ERR_BAD_REQ_SUBSCRIPTION_DELETED,
                                        NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_CLI_ERR_BAD_REQ);
            }
            sippmh_free_location(to_loc);
            sippmh_free_location(from_loc);
            sippmh_genurl_free(genUrl);
            SIPPMH_FREE_REQUEST_LINE(requestURI);
            return SIP_ERROR;
        }
        // Check continuity of CSeq numbers
        if (request_cseq_number <= scbp->last_recv_request_cseq) {
            // Return 500 Internal Server Error
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Out of order CSeq number received",
                              fname);
            if (sipSPISendErrorResponse(pSipMessage, SIP_SERV_ERR_INTERNAL,
                                        SIP_SERV_ERR_INTERNAL_PHRASE,
                                        SIP_WARN_MISC,
                                        SIP_CLI_ERR_BAD_REQ_CSEQ_FIELD,
                                        NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_SERV_ERR_INTERNAL);
            }
            sippmh_free_location(to_loc);
            sippmh_free_location(from_loc);
            sippmh_genurl_free(genUrl);
            SIPPMH_FREE_REQUEST_LINE(requestURI);
            return SIP_ERROR;
        }

        sippmh_free_location(to_loc);
        sippmh_free_location(from_loc);
        sippmh_genurl_free(genUrl);
        SIPPMH_FREE_REQUEST_LINE(requestURI);
    } else {
        // Check if this is a SUBSCRIBE request for an already terminated
        // subscription.  If this subscription is associated with an
        // existing dialog, let it go
        ccb = sip_sm_get_ccb_by_callid(callID);
        if ((ccb == NULL) &&
            is_previous_sub(callID, from_loc->tag, eventPackage)) {
            // Return 481 Internal Server Error
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"SUBSCRIBE received for terminated "
                              "subscription\n", fname);
            if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_CALLEG,
                                        SIP_CLI_ERR_CALLEG_PHRASE,
                                        SIP_WARN_MISC,
                                        SIP_CLI_ERR_BAD_REQ_SUBSCRIPTION_DELETED,
                                        NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_CLI_ERR_CALLEG);
            }
            sippmh_free_location(to_loc);
            sippmh_free_location(from_loc);
            sippmh_genurl_free(genUrl);
            SIPPMH_FREE_REQUEST_LINE(requestURI);
            return SIP_ERROR;
        }
        // This is a valid new subscription - get all the params
        scbpReg = find_scb_by_registration(eventPackage, &scb_index);
        if (!scbpReg) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"No application registered "
                              "to accept this event.\n", fname);
            if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_EVENT,
                                        SIP_CLI_ERR_BAD_EVENT_PHRASE,
                                        0, NULL, NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_CLI_ERR_BAD_EVENT);
            }
            sippmh_free_location(to_loc);
            sippmh_free_location(from_loc);
            sippmh_genurl_free(genUrl);
            SIPPMH_FREE_REQUEST_LINE(requestURI);
            return SIP_ERROR;
        }

        scbp = allocate_scb(&scb_index);
        if (!scbp) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"No SCB Available "
                              "to accept this event.\n", fname);
            if (sipSPISendErrorResponse(pSipMessage, SIP_SERV_ERR_INTERNAL,
                                        SIP_SERV_ERR_INTERNAL_PHRASE,
                                        0, NULL, NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_CLI_ERR_BAD_REQ);
            }
            sippmh_free_location(to_loc);
            sippmh_free_location(from_loc);
            sippmh_genurl_free(genUrl);
            SIPPMH_FREE_REQUEST_LINE(requestURI);
            show_scbs_inuse();
            return SIP_ERROR;
        }
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Allocated SCB for Received Subscribe, event=%d,"
                         " scb=%d sub_id=%x\n", DEB_F_PREFIX_ARGS(SIP_SUB, fname), scbpReg->hb.event_type,
                         scb_index, scbp->sub_id);
        scbp->hb.event_type = scbpReg->hb.event_type;
        scbp->subsIndCallback = scbpReg->subsIndCallback;
        scbp->subsIndCallbackTask = scbpReg->subsIndCallbackTask;
        scbp->subsNotCallbackTask = scbpReg->subsNotCallbackTask;
        scbp->subsIndCallbackMsgID = scbpReg->subsIndCallbackMsgID;
        scbp->subsTermCallback = scbpReg->subsTermCallback;
        scbp->subsTermCallbackMsgID = scbpReg->subsTermCallbackMsgID;
        sstrncpy(scbp->event_name, scbpReg->event_name, MAX_EVENT_NAME_LEN);

        scbp->internal = FALSE;

        /* If the event package is KPML then check if the ID values
         * are associated with it
         */
        if (eventPackage == CC_SUBSCRIPTIONS_KPML) {
            /*
             * At this point from_tag and to_tag is never used.  Since
             * kpml_call_id, from_tag and to_tag just point to the
             * appropriate location and does not allocate memory by itself.
             */
            (void) sippmh_parse_kpml_event_id_params((char *)event,
                                                     &kpml_call_id,
                                                     &from_tag,
                                                     &to_tag);
        }

        if (kpml_call_id) {
            ccb = sip_sm_get_ccb_by_callid(kpml_call_id);
        } else {
            ccb = sip_sm_get_ccb_by_callid(callID);
            scbp->ccbp = ccb;
        }
        if (ccb) {
            scbp->gsm_id = ccb->gsm_id;
            scbp->hb.dn_line = ccb->dn_line;
        } else {
            scbp->gsm_id = 0;
            scbp->hb.dn_line = 0;
        }
        // Parse Supported and Required headers to see if there is the
        // norefersub option tag specified
        require = sippmh_get_cached_header_val(pSipMessage, REQUIRE);
        if (require) {
            tags = sippmh_parse_supported_require(require, NULL);
            if (tags & norefersub_tag) {
                scbp->norefersub = TRUE;
            }
         }
        if ((eventPackage == CC_SUBSCRIPTIONS_CONFIGAPP) &&
             (scbp->norefersub == FALSE)) {
             //noReferSub tag is required for OOD Refer for config change
             CCSIP_DEBUG_ERROR(SIP_F_PREFIX"noReferSub missing from Require header.",
                               fname);
             if (sipSPISendErrorResponse(pSipMessage,
                                   SIP_CLI_ERR_BAD_REQ,
                                   SIP_CLI_ERR_BAD_REQ_PHRASE,
                                   SIP_WARN_MISC,
                                   SIP_CLI_ERR_BAD_REQ_REQUIRE_HDR,
				  NULL) != TRUE) {
                 CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                       fname, SIP_CLI_ERR_BAD_REQ);
             }
             free_scb(scb_index, fname);
             sippmh_free_location(to_loc);
             sippmh_free_location(from_loc);
             sippmh_genurl_free(genUrl);
             SIPPMH_FREE_REQUEST_LINE(requestURI);
             return SIP_ERROR;
         }
        supported = sippmh_get_cached_header_val(pSipMessage, SUPPORTED);
        if (supported) {
            tags = sippmh_parse_supported_require(supported, NULL);
            if (tags & norefersub_tag) {
                scbp->norefersub = TRUE;
            }
        }
        // Check to see if the expires value is within the acceptable range,
        // if any has been given to us
        if (scbp->min_expires != 0) {
            if (expiry_time < scbp->min_expires && expiry_time < 3600) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Too small expiry time: %lu; "
                                  "Min acceptable: %lu.", fname,
                                  expiry_time, scbp->min_expires);

                if (sipSPISendErrorResponse(pSipMessage,
                                            SIP_CLI_ERR_INTERVAL_TOO_SMALL,
                                            SIP_CLI_ERR_INTERVAL_TOO_SMALL_PHRASE,
                                            0, NULL, NULL) != TRUE) {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                      fname, SIP_CLI_ERR_INTERVAL_TOO_SMALL);
                }
                free_scb(scb_index, fname);
                sippmh_free_location(to_loc);
                sippmh_free_location(from_loc);
                sippmh_genurl_free(genUrl);
                SIPPMH_FREE_REQUEST_LINE(requestURI);
                return SIP_ERROR;
            }
        }
        if (scbp->max_expires != 0) {
            if (expiry_time > scbp->max_expires) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Too large expiry time: %lu; "
                                  "Max acceptable: %lu.", fname,
                                  expiry_time, scbp->max_expires);
                // There doesn't seem to be any particular error code for
                // maximum expiry time so just return the generic error
                if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                            SIP_CLI_ERR_BAD_REQ_PHRASE,
                                            SIP_WARN_MISC,
                                            SIP_CLI_ERR_INTERVAL_TOO_LARGE_PHRASE,
                                            NULL) != TRUE) {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                      fname, SIP_CLI_ERR_BAD_REQ);
                }
                CCSIP_DEBUG_TASK(DEB_F_PREFIX"Freeing SCB: scb=%d sub_id=%x",
                                 DEB_F_PREFIX_ARGS(SIP_SUB, fname), scb_index, scbp->sub_id);
                free_scb(scb_index, fname);
                sippmh_free_location(to_loc);
                sippmh_free_location(from_loc);
                sippmh_genurl_free(genUrl);
                SIPPMH_FREE_REQUEST_LINE(requestURI);
                return SIP_ERROR;
            }
        }
        sipUriUrl = genUrl->u.sipUrl;
        if (sipUriUrl) {
            pUser = sippmh_parse_user(sipUriUrl->user);
            if (pUser) {
                sstrncpy(scbp->SubURI, pUser, sizeof(scbp->SubURI));
                cpr_free(pUser);
            } else {
                /* An error occurred, copy the whole thing.. */
                sstrncpy(scbp->SubURI, sipUriUrl->user, sizeof(scbp->SubURI));
            }
        }
        sippmh_genurl_free(genUrl);
        SIPPMH_FREE_REQUEST_LINE(requestURI);

        scbp->sip_from = strlib_update(scbp->sip_from, sip_from);
        if (from_loc->tag) {
            scbp->sip_from_tag = strlib_update(scbp->sip_from_tag,
                                               sip_sm_purify_tag(from_loc->tag));
        }

        if (from_loc->genUrl->schema == URL_TYPE_SIP) {
            sipFromUrl = from_loc->genUrl->u.sipUrl;
        }
        if (sipFromUrl) {
            if (sipFromUrl->user) {
                char    *pUserTemp, *target = NULL, addr_error;
                uint32_t sip_address = 0;

                pUserTemp = sippmh_parse_user(sipFromUrl->user);
                if (pUserTemp) {
                    scbp->callingNumber = strlib_update(scbp->callingNumber,
                                                        pUserTemp);
                    cpr_free(pUserTemp);
                } else {
                    scbp->callingNumber = strlib_update(scbp->callingNumber,
                                                        sipFromUrl->user);
                }

                target = cpr_strdup(sipFromUrl->host);
                if (!target) {
                    sip_address = 0;
                } else {
//CPR TODO: need reference for
//Should replace with a boolean util_check_ip_addr(char *addr) call
                    sip_address = IPNameCk(target, &addr_error);
                    cpr_free(target);
                }

                if ((from_loc->genUrl->schema == URL_TYPE_SIP) &&
                    (!sip_address)) {
                    if (scbp->callingNumber) {
                        scbp->callingNumber = strlib_append(scbp->callingNumber, "@");
                        scbp->callingNumber = strlib_append(scbp->callingNumber, sipFromUrl->host);
                    }
                }
            } else {
                scbp->callingNumber = strlib_update(scbp->callingNumber,
                                                    "Unknown Number");
            }
        }

        scbp->sip_to = strlib_update(scbp->sip_to, sip_to);
        if (to_loc->tag == NULL) {
            // Create To tag
            sip_to_tag_temp = strlib_open(scbp->sip_to_tag, MAX_SIP_TAG_LENGTH);
            if (sip_to_tag_temp) {
                sip_util_make_tag(sip_to_tag_temp);
            }
            scbp->sip_to_tag = strlib_close(sip_to_tag_temp);
            sip_to_temp = strlib_open(scbp->sip_to, MAX_SIP_URL_LENGTH);
            if (sip_to_temp) {
                sstrncat(sip_to_temp, ";tag=",
                        MAX_SIP_URL_LENGTH - strlen(sip_to_temp));
                if (scbp->sip_to_tag) {
                    sstrncat(sip_to_temp, scbp->sip_to_tag,
                            MAX_SIP_URL_LENGTH - strlen(sip_to_temp));
                }
            }
            scbp->sip_to = strlib_close(sip_to_temp);
        }

        sippmh_free_location(to_loc);
        sippmh_free_location(from_loc);

        sstrncpy(scbp->hb.sipCallID, callID, MAX_SIP_CALL_ID);

        // Parse Contact info
        contact = sippmh_get_cached_header_val(pSipMessage, CONTACT);
        if (contact) {
            if (scbp->contact_info) {
                sippmh_free_contact(scbp->contact_info);
            }
            scbp->contact_info = sippmh_parse_contact(contact);

            if ((scbp->contact_info == NULL) || // contact in msg, parse error
                (sipSPICheckContact(contact) < 0)) { // If contact is invalid
                if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                            SIP_CLI_ERR_BAD_REQ_PHRASE,
                                            SIP_WARN_MISC,
                                            SIP_CLI_ERR_BAD_REQ_CONTACT_FIELD,
                                            NULL) != TRUE) {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                      fname, SIP_CLI_ERR_BAD_REQ);
                }
                free_scb(scb_index, fname);
                return SIP_ERROR;
            }
        }

        if ((sipMethod == sipMethodSubscribe) && (scbp->ccbp == NULL)) {
            record_route = sippmh_get_cached_header_val(pSipMessage,
                                                        RECORD_ROUTE);
            if (record_route) {
                if (scbp->record_route_info) {
                    sippmh_free_record_route(scbp->record_route_info);
                }
                scbp->record_route_info = sippmh_parse_record_route(record_route);
                if (scbp->record_route_info == NULL) {
                    CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sippmh_parse_record_route() failed",
                                      fname);
                    free_scb(scb_index, fname);
                    return SIP_ERROR;
                }
                /*
                 * Store the Record-Route header value to be used
                 * in 18x or 2xx response
                 */
                scbp->cached_record_route = strlib_update(scbp->cached_record_route,
                                                          record_route);
            }
        }

    } // End of populating a new SCB

    scbp->hb.expires = expiry_time;
    scbp->hb.orig_expiration = expiry_time;
    scbp->last_recv_request_cseq = request_cseq_number;
    scbp->last_recv_request_cseq_method = request_cseq_method;

    // Parse Refer-To
    if (sipMethod == sipMethodRefer) {
        sipReferTo_t   *referto = NULL;

        noOfReferTo = sippmh_get_num_particular_headers(pSipMessage,
                                                        SIP_HEADER_REFER_TO,
                                                        SIP_C_HEADER_REFER_TO,
                                                        &referToString,
                                                        MAX_REFER_TO_HEADERS);

        if ((noOfReferTo == 0) || (noOfReferTo > 1)) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                              scb_index, scbp->hb.dn_line, fname,
                              "Incorrect number of Refer-To headers\n");
            (void) sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                           SIP_CLI_ERR_BAD_REQ_PHRASE,
                                           SIP_WARN_MISC,
                                           SIP_CLI_ERR_BAD_REQ_PHRASE_REFER_TO,
                                           NULL);
            return SIP_ERROR;
        }
        // Note that the refer-to header is not parsed for CCM mode since it was
        // malformed in older versions of CCM
        if (sip_regmgr_get_cc_mode(1) != REG_MODE_CCM) {
            referto = sippmh_parse_refer_to(referToString);
            if (referto == NULL) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                                  scb_index, scbp->hb.dn_line, fname,
                                  "Refer-To header could not be parsed\n");
                (void) sipSPISendErrorResponse(pSipMessage,
                                               SIP_CLI_ERR_BAD_REQ,
                                               SIP_CLI_ERR_BAD_REQ_PHRASE,
                                               SIP_WARN_MISC,
                                               SIP_CLI_ERR_BAD_REQ_PHRASE_REFER_TO,
                                               NULL);
                return SIP_ERROR;
            } else {
                sippmh_free_refer_to(referto);
            }
        }
    }

    // Parse and store Via Header
    via = sippmh_get_cached_header_val(pSipMessage, VIA);
    if (via) {
        /* store the via header */
        if (store_incoming_trxn(via, request_cseq_number,  scbp) == FALSE) {
            (void) sipSPISendErrorResponse(pSipMessage,
                                               SIP_SERV_ERR_INTERNAL,
                                               SIP_SERV_ERR_INTERNAL_PHRASE,
                                               0,
                                               NULL,
                                               NULL);
            return SIP_ERROR;
        }
    }

    // See if can determine dn_line from any of the parameters
    // 1st try the REQ-URI
    // CCM should send us
    // SUBSCRIBE sip:<subscribed-line-pkid>@<phone-ip> SIP/2.0
    // and we should be able to get the line from its pkid
    if (scbp->hb.dn_line == 0) {
        for (dn_line = 1; dn_line <= MAX_REG_LINES; dn_line++) {
            if (sip_config_check_line(dn_line) == FALSE) {
                continue;
            }
            config_get_line_string(CFGID_LINE_CONTACT, line_contact,
                                   dn_line, sizeof(line_contact));
            if (cpr_strcasecmp(line_contact, UNPROVISIONED) != 0) {
                if (cpr_strcasecmp(scbp->SubURI, line_contact) == 0) {
                    scbp->hb.dn_line = dn_line;
                    break;
                }
            }
        }
    }
    if (scbp->hb.dn_line == 0) {
        // If no match there, check against the DN number
        for (dn_line = 1; dn_line <= MAX_REG_LINES; dn_line++) {
            config_get_line_string(CFGID_LINE_NAME, line_name,
                                   dn_line, sizeof(line_name));
            if (!cpr_strcasecmp(scbp->SubURI, line_name)) {
                scbp->hb.dn_line = dn_line;
                break;
            }
        }
    }
    // If unable to determine dn_line, the SUBSCRIBE is directed to device
    // Could make sure and check against the MAC address here
    if (scbp->hb.dn_line == 0) {
        scbp->useDeviceAddressing = TRUE;
    }
    // Parse Body
    subs_ind_data.u.subs_ind_data.eventData = NULL;
    i = 0;
    while (i < HTTPISH_MAX_BODY_PARTS && pSipMessage->mesg_body[i].msgBody
            != NULL) {
        if (pSipMessage->mesg_body[i].msgContentTypeValue != SIP_CONTENT_TYPE_DIALOG_VALUE &&
            pSipMessage->mesg_body[i].msgContentTypeValue != SIP_CONTENT_TYPE_KPML_REQUEST_VALUE &&
            pSipMessage->mesg_body[i].msgContentTypeValue != SIP_CONTENT_TYPE_REMOTECC_REQUEST_VALUE &&
            pSipMessage->mesg_body[i].msgContentTypeValue != SIP_CONTENT_TYPE_REMOTECC_RESPONSE_VALUE &&
            pSipMessage->mesg_body[i].msgContentTypeValue != SIP_CONTENT_TYPE_CONFIGAPP_VALUE &&
            pSipMessage->mesg_body[i].msgContentTypeValue != SIP_CONTENT_TYPE_PRESENCE_VALUE) {

            if (pSipMessage->mesg_body[i].msgContentTypeValue == SIP_CONTENT_TYPE_CMXML_VALUE) {
                // Body can not be parsed - send it up as it is
                subDatap = (ccsip_event_data_t *) cpr_malloc(sizeof(ccsip_event_data_t));
                if (subDatap == NULL) {
                    CCSIP_DEBUG_ERROR(SIP_F_PREFIX"malloc of subDatap failed.",
                                      fname);
                    return (0);
                }

                subDatap->u.raw_data.data = pSipMessage->mesg_body[i].msgBody;
                subDatap->u.raw_data.length = pSipMessage->mesg_body[i].msgLength;
                pSipMessage->mesg_body[i].msgBody = NULL;
                pSipMessage->mesg_body[i].msgLength = 0;
                subDatap->type = EVENT_DATA_RAW;
                subDatap->next = NULL;
            } else {

                /* There are other applications that has been handled like syncCheck
                 */

                scp = sippmh_parse_service_control_body(pSipMessage->mesg_body[i].msgBody,
                                       pSipMessage->mesg_body[i].msgLength);

                if (scp != NULL) {
                    // Hand over the event to platform
                    sip_platform_handle_service_control_notify(scp);

                    sippmh_free_service_control_info(scp);

                }

                /* If this is the only body then have to send the response out as
                 * no other application will be requesting to send response
                 */
                if (i== 0 && pSipMessage->mesg_body[i+1].msgBody == NULL) {

                    if (sipSPISendErrorResponse(pSipMessage, 200, SIP_SUCCESS_SETUP_PHRASE,
                                                    0, NULL, NULL) != TRUE) {
                        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                          fname, SIP_SUCCESS_SETUP);
                    }
                    if (!reSubscribe) {
                        free_scb(scb_index, fname);
                    }
                    return(0);
                }
            }

        } else {
            result = parse_body(scbp->hb.event_type, pSipMessage->mesg_body[i].msgBody,
                                pSipMessage->mesg_body[i].msgLength, &subDatap, fname);
            if (result == SIP_ERROR) {
                if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                            SIP_CLI_ERR_BAD_REQ_PHRASE,
                                            SIP_WARN_MISC,
                                            SIP_CLI_ERR_BAD_REQ_BAD_BODY_ENCODING, NULL) != TRUE) {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                      fname, SIP_CLI_ERR_BAD_REQ);
                }
                if (!reSubscribe) {
                    free_scb(scb_index, fname);
                }
                free_event_data(subDatap);
                free_event_data(subs_ind_data.u.subs_ind_data.eventData);
                return SIP_ERROR;
            }
        }
        if (subs_ind_data.u.subs_ind_data.eventData == NULL &&
            subDatap != NULL) {
            subs_ind_data.u.subs_ind_data.eventData = subDatap;
            subDatap->next = NULL;
        } else if (subs_ind_data.u.subs_ind_data.eventData != NULL &&
            subDatap != NULL){
            append_event_data(subs_ind_data.u.subs_ind_data.eventData,
                              subDatap);
        }
        i++;
    }

    // Prepare subscription indication data
    subs_ind_data.event = eventPackage;
    subs_ind_data.u.subs_ind_data.expires = scbp->hb.expires;
    subs_ind_data.u.subs_ind_data.from = scbp->sip_from;
    subs_ind_data.u.subs_ind_data.to = scbp->sip_to;
    subs_ind_data.u.subs_ind_data.line = scbp->hb.dn_line;
    subs_ind_data.sub_duration = scbp->hb.expires;
    subs_ind_data.sub_id = scbp->sub_id;
    subs_ind_data.msg_id = scbp->subsIndCallbackMsgID;
    subs_ind_data.gsm_id = scbp->gsm_id;
    subs_ind_data.line_id = scbp->hb.dn_line;
    subs_ind_data.norefersub = scbp->norefersub;
    subs_ind_data.request_id = -1;

    // Eventually let the subscribing application know
    if (scbp->subsIndCallback) {
        (scbp->subsIndCallback) (&subs_ind_data);
    } else if ((scbp->subsIndCallbackTask != CC_SRC_MIN) && (scbp->subsIndCallbackMsgID != 0)) {
        (void) sip_send_message(&subs_ind_data, scbp->subsIndCallbackTask,
                                scbp->subsIndCallbackMsgID);
    }

    if (scbp->smState == SUBS_STATE_SENT_NOTIFY) {
        scbp->smState = SUBS_STATE_RCVD_SUBSCRIBE_SENT_NOTIFY;
    } else {
        scbp->smState = SUBS_STATE_RCVD_SUBSCRIBE;
    }
    incomingSubscribes++;
    if (!reSubscribe) {
        incomingSubscriptions++;
    }
    return (0);
}

/**
  * This function will decode the xml bodies.
  *
  * @param[in] event_type - event type.
  * @param[in] pSipMessage - pointer to sipMessage_t
  * @param[out] dataPP -  pointer to pointer to decoded data.
  *
  * @returns TRUE/FALSE
  *
  * @pre (pSipMessage != NULL) && (dataPP != NULL)
  */
static boolean
decode_message_body (cc_subscriptions_t event_type, sipMessage_t *pSipMessage, ccsip_event_data_t **dataPP)
{
    const char     *fname = "decode_message_body";
    uint8_t         i = 0;
    ccsip_event_data_t *notDatap = NULL;
    int             result;

    // Decode the body, if any
    while (i < HTTPISH_MAX_BODY_PARTS && pSipMessage->mesg_body[i].msgBody
            != NULL) {
        if (pSipMessage->mesg_body[0].msgContentTypeValue != SIP_CONTENT_TYPE_DIALOG_VALUE &&
            pSipMessage->mesg_body[0].msgContentTypeValue != SIP_CONTENT_TYPE_KPML_REQUEST_VALUE &&
            pSipMessage->mesg_body[0].msgContentTypeValue != SIP_CONTENT_TYPE_REMOTECC_REQUEST_VALUE &&
            pSipMessage->mesg_body[0].msgContentTypeValue != SIP_CONTENT_TYPE_REMOTECC_RESPONSE_VALUE &&
            pSipMessage->mesg_body[0].msgContentTypeValue != SIP_CONTENT_TYPE_PRESENCE_VALUE) {

            // Body can not be parsed - send it up as it is
            notDatap = (ccsip_event_data_t *) cpr_malloc(sizeof(ccsip_event_data_t));
            if (notDatap == NULL) {
                CCSIP_DEBUG_ERROR("%s: Error - malloc of notDatap failed.",
                                  fname);
                return FALSE;
            }

            notDatap->u.raw_data.data = pSipMessage->mesg_body[0].msgBody;
            notDatap->u.raw_data.length = pSipMessage->mesg_body[0].msgLength;
            pSipMessage->mesg_body[0].msgBody = NULL;
            pSipMessage->mesg_body[0].msgLength = 0;
            notDatap->type = EVENT_DATA_RAW;
            notDatap->next = NULL;

        } else {

            result = parse_body(event_type, pSipMessage->mesg_body[i].msgBody,
                                pSipMessage->mesg_body[i].msgLength, &notDatap, fname);
            if (result == SIP_ERROR) {
                free_event_data(notDatap);
                free_event_data(*dataPP);
                return FALSE;
            }
        }
        if ((*dataPP) == NULL) {
            (*dataPP) = notDatap;
            notDatap->next = NULL;
        } else {
            append_event_data((*dataPP), notDatap);
        }
        i++;
    }
    return TRUE;
}

/********************************************************
 * Handle Network Received Notify Request
 ********************************************************/
int
subsmanager_handle_ev_sip_subscribe_notify (sipMessage_t *pSipMessage)
{
    const char     *fname = "subsmanager_handle_ev_sip_subscribe_notify";
    cc_subscriptions_t eventPackage;
    sipSCB_t       *scbp = NULL;
    const char     *callID = NULL;
    const char     *event = NULL;
    int             scb_index;
    sipCseq_t      *request_cseq_structure = NULL;
    unsigned long   request_cseq_number = 0;
    sipMethod_t     request_cseq_method = sipMethodInvalid;
    ccsip_sub_not_data_t notify_ind_data;
    int16_t         requestStatus = SIP_MESSAGING_ERROR;
    const char     *from = NULL, *to = NULL;
    const char     *subs_state = NULL;
    boolean         subs_header_found = FALSE;
    sipLocation_t  *to_loc = NULL;
    const char     *via = NULL;

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Processing a network generated NOTIFY", DEB_F_PREFIX_ARGS(SIP_SUB, fname));

    memset(&notify_ind_data, 0, sizeof(notify_ind_data));
    requestStatus = (uint16_t) sipSPICheckRequest(NULL, pSipMessage);
    if (requestStatus != SIP_MESSAGING_OK) {
        if (requestStatus == SIP_MESSAGING_DUPLICATE) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Received duplicate request", fname);
        } else {
            if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                        SIP_CLI_ERR_BAD_REQ_PHRASE,
                                        SIP_WARN_MISC, NULL, NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_CLI_ERR_BAD_REQ);
            }
        }
        return SIP_ERROR;
    }
    // Get fields from NOTIFY
    event = sippmh_get_header_val(pSipMessage, SIP_HEADER_EVENT,
                                  SIP_C_HEADER_EVENT);
    if (event) {
        if (cpr_strcasecmp(event, SIP_EVENT_DIALOG) == 0) {
            eventPackage = CC_SUBSCRIPTIONS_DIALOG;
        } else if (cpr_strcasecmp(event, SIP_EVENT_KPML) == 0) {
            eventPackage = CC_SUBSCRIPTIONS_KPML;
        } else if (cpr_strcasecmp(event, SIP_EVENT_CONFIG) == 0) {
            eventPackage = CC_SUBSCRIPTIONS_CONFIG;
        } else if (cpr_strcasecmp(event, SIP_EVENT_PRESENCE) == 0) {
            eventPackage = CC_SUBSCRIPTIONS_PRESENCE;
        } else if ((cpr_strcasecmp(event, SIP_EVENT_REFER) == 0) &&
            (pSipMessage->mesg_body[0].msgContentTypeValue == SIP_CONTENT_TYPE_DIALOG_VALUE)) {
            eventPackage = CC_SUBSCRIPTIONS_DIALOG;
        } else {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unsupported event=%s", fname, event);
            if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_EVENT,
                                        SIP_CLI_ERR_BAD_EVENT_PHRASE,
                                        0, NULL, NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_CLI_ERR_BAD_EVENT);
            }
            return SIP_ERROR;
        }
    } else {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Missing Event header", fname);
        if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_EVENT,
                                    SIP_CLI_ERR_BAD_EVENT_PHRASE,
                                    0, NULL, NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_EVENT);
        }
        return SIP_ERROR;
    }

    callID = sippmh_get_cached_header_val(pSipMessage, CALLID);
    if (!callID) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to obtain request's "
                          "Call-ID header.\n", fname);
        if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                    SIP_CLI_ERR_BAD_REQ_PHRASE,
                                    SIP_WARN_MISC,
                                    SIP_CLI_ERR_BAD_REQ_CALLID_ABSENT,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        return SIP_ERROR;
    }

        // Find the proper SCB
        scbp = find_scb_by_subscription(eventPackage, &scb_index, callID);
        if (!scbp) {
            /*
             * check if it is an unsolicited dialog/presence event NOTIFY
             * check if it has a to-tag. if so, then it is not considered unsolicited NOTIFY.
             * Presence of to-tag could be because it is a final NOTIFY of a subscription which
             * we have just terminated.
             */
            to = sippmh_get_cached_header_val(pSipMessage, TO);
            to_loc = sippmh_parse_from_or_to((char *) to, TRUE);
            if ((to_loc == NULL) || (to_loc->tag == NULL)) {
                if (eventPackage == CC_SUBSCRIPTIONS_DIALOG) {
                    notify_ind_data.line_id = 1;
                    /* decode the body */
                    if (decode_message_body(CC_SUBSCRIPTIONS_DIALOG, pSipMessage,
                                            &(notify_ind_data.u.notify_ind_data.eventData)) == FALSE) {
                        if (sipSPISendErrorResponse(pSipMessage, SIP_SERV_ERR_INTERNAL,
                                                    SIP_SERV_ERR_INTERNAL_PHRASE,
                                                    0, NULL, NULL) != TRUE) {
                            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                              fname, SIP_CLI_ERR_BAD_REQ);
                            return SIP_ERROR;
                        }
                    }
                   if (sipSPISendErrorResponse(pSipMessage, SIP_STATUS_SUCCESS,
                                                SIP_SUCCESS_SETUP_PHRASE,
                                                0, NULL, NULL) != TRUE) {
                        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                          fname, SIP_STATUS_SUCCESS);
                    }

                    incomingUnsolicitedNotifies++;
                    sippmh_free_location(to_loc);
                    return (0);
                } else if (eventPackage == CC_SUBSCRIPTIONS_PRESENCE) {
                    /* decode the body */
                    if (decode_message_body(CC_SUBSCRIPTIONS_PRESENCE, pSipMessage,
                                            &(notify_ind_data.u.notify_ind_data.eventData)) == FALSE) {
                        if (sipSPISendErrorResponse(pSipMessage, SIP_SERV_ERR_INTERNAL,
                                                    SIP_SERV_ERR_INTERNAL_PHRASE,
                                                    0, NULL, NULL) != TRUE) {
                            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                              fname, SIP_CLI_ERR_BAD_REQ);
                            return SIP_ERROR;
                        }
                    }
                    pres_unsolicited_notify_ind(&notify_ind_data);
                    if (sipSPISendErrorResponse(pSipMessage, SIP_STATUS_SUCCESS,
                                                SIP_SUCCESS_SETUP_PHRASE,
                                                0, NULL, NULL) != TRUE) {
                        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                          fname, SIP_STATUS_SUCCESS);
                    }

                    incomingUnsolicitedNotifies++;
                    sippmh_free_location(to_loc);
                    return (0);
                }
            }
            sippmh_free_location(to_loc);
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"No prior subscription", fname);
            if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_CALLEG,
                                        SIP_CLI_ERR_SUBS_DOES_NOT_EXIST_PHRASE,
                                        0, NULL, NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_CLI_ERR_CALLEG);
            }
            return SIP_ERROR;
        }


    if (scbp->pendingClean) {
        // Oops, the application has already terminated this subscription
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Notify received for subscription "
                         "already terminated by application.\n", DEB_F_PREFIX_ARGS(SIP_SUB, fname));
        if (sipSPISendErrorResponse(pSipMessage, SIP_SUCCESS_SETUP,
                                    SIP_SUCCESS_SETUP_PHRASE,
                                    0, NULL, NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_SUCCESS_SETUP);
        }
        return (0);
    }
    // Check SCB state
    if (scbp->smState == SUBS_STATE_SENT_SUBSCRIBE) {
        // Have just sent a SUBSCRIBE but have not received a response
        // This could happen if the NOTIFY from the remote side comes in
        // before its 2xx response to SUBSCRIBE does
        // Should still process this as if received a 2xx response
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Bad subscription state: "
                         "But still processing out-of-turn NOTIFY.\n", DEB_F_PREFIX_ARGS(SIP_SUB, fname));
    }
    // Copy stuff from the message into the SCB - the parts that we
    // need to keep to respond to this request. This includes: Via&Branch
    // (From and To & Tags, CallID are simply inverse of the ones in
    // SUBSCRIBE), and CSeq

    // Parse subscription state header
    subs_state = sippmh_get_header_val(pSipMessage,
                                       SIP_HEADER_SUBSCRIPTION_STATE,
                                       SIP_HEADER_SUBSCRIPTION_STATE);
    if (subs_state) {
        sipSubscriptionStateInfo_t subsStateInfo;

        memset(&subsStateInfo, 0, sizeof(sipSubscriptionStateInfo_t));
        // Get the state, expires, retry-after and reason
        if (sippmh_parse_subscription_state(&subsStateInfo, subs_state) == 0) {
            // Store values in SCB
            scbp->hb.expires = subsStateInfo.expires;
            scbp->subscription_state = subsStateInfo.state;
            scbp->retry_after = subsStateInfo.retry_after;
            scbp->subscription_state_reason = subsStateInfo.reason;
            subs_header_found = TRUE;
        }
    }
    if (!subs_header_found) {
        // Subscription-State header not present - reject
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to obtain or parse request's "
                          "Subs-state header.\n", fname);
        if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                    SIP_CLI_ERR_BAD_REQ_PHRASE,
                                    SIP_WARN_MISC,
                                    SIP_CLI_ERR_BAD_REQ_NO_SUBSCRIPTION_HEADER,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        // Send message to the app and let it decide what it wants
        // to do with a bad NOTIFY
        sip_subsManager_send_protocol_error(scbp, scb_index, FALSE);
        return SIP_ERROR;
    }
    // Parse CSEQ
    if (getCSeqInfo(pSipMessage, &request_cseq_structure) == FALSE) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to obtain or parse request's CSeq "
                          "header.\n", fname);
        if (sipSPISendErrorResponse(pSipMessage, SIP_CLI_ERR_BAD_REQ,
                                    SIP_CLI_ERR_BAD_REQ_PHRASE,
                                    SIP_WARN_MISC,
                                    SIP_CLI_ERR_BAD_REQ_CSEQ_FIELD,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        // Send message to the app and let it decide what it wants
        // to do with a bad NOTIFY
        sip_subsManager_send_protocol_error(scbp, scb_index, FALSE);
        return SIP_ERROR;
    }

    request_cseq_number = request_cseq_structure->number;
    request_cseq_method = request_cseq_structure->method;
    cpr_free(request_cseq_structure);

    // Check continuity of CSeq numbers
    if (request_cseq_number <= scbp->last_recv_request_cseq) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Out of order CSeq number received",
                          fname);
    } else {
        scbp->last_recv_request_cseq = request_cseq_number;
        scbp->last_recv_request_cseq_method = request_cseq_method;
    }

    // Parse and store Via Header
    via = sippmh_get_cached_header_val(pSipMessage, VIA);
    if (via) {
        /* store the via header */
        if (store_incoming_trxn(via, request_cseq_number,  scbp) == FALSE) {
            (void) sipSPISendErrorResponse(pSipMessage,
                                               SIP_SERV_ERR_INTERNAL,
                                               SIP_SERV_ERR_INTERNAL_PHRASE,
                                               0,
                                               NULL,
                                               NULL);
            return SIP_ERROR;
        }
    }

    // Update from/to headers - note this is reversed
    // as we generated the initial SUBSCRIBE
    to = sippmh_get_cached_header_val(pSipMessage, TO);
    from = sippmh_get_cached_header_val(pSipMessage, FROM);

    if (to) {
        scbp->sip_from = strlib_update(scbp->sip_from, to);
    }
    if (from) {
        scbp->sip_to = strlib_update(scbp->sip_to, from);
    }

    if ((scbp->smState == SUBS_STATE_SENT_SUBSCRIBE) ||
        (scbp->smState == SUBS_STATE_SENT_SUBSCRIBE_RCVD_NOTIFY)) {
        scbp->smState = SUBS_STATE_SENT_SUBSCRIBE_RCVD_NOTIFY;
    } else {
        scbp->smState = SUBS_STATE_RCVD_NOTIFY;
    }

    // Decode the body, if any
    if (decode_message_body(eventPackage, pSipMessage, &(notify_ind_data.u.notify_ind_data.eventData)) == FALSE) {
        if (sipSPISendErrorResponse(pSipMessage, SIP_SERV_ERR_INTERNAL,
                                    SIP_SERV_ERR_INTERNAL_PHRASE,
                                    0, NULL, NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        // Send message to the app and let it decide what it
        // wants to do with a bad NOTIFY
        sip_subsManager_send_protocol_error(scbp, scb_index, FALSE);
        return SIP_ERROR;
    }

    // Fill out the return structure and call the callback routine
    notify_ind_data.event = eventPackage;
    notify_ind_data.sub_id = scbp->sub_id;
    notify_ind_data.msg_id = scbp->notIndCallbackMsgID;
    notify_ind_data.gsm_id = scbp->gsm_id;
    notify_ind_data.line_id = scbp->hb.dn_line;
    notify_ind_data.request_id = scbp->request_id;
    notify_ind_data.u.notify_ind_data.subscription_state =
        scbp->subscription_state;
    notify_ind_data.u.notify_ind_data.expires = scbp->hb.expires;
    notify_ind_data.u.notify_ind_data.retry_after = scbp->retry_after;
    notify_ind_data.u.notify_ind_data.subscription_state_reason =
        scbp->subscription_state_reason;
    notify_ind_data.u.notify_ind_data.cseq = request_cseq_number;

    if (scbp->notifyIndCallback) {
        (scbp->notifyIndCallback) (&notify_ind_data);
    } else if (scbp->subsNotCallbackTask != CC_SRC_MIN) {
        (void) sip_send_message(&notify_ind_data, scbp->subsNotCallbackTask,
                                scbp->notIndCallbackMsgID);
    }

    scbp->outstandingIncomingNotifyTrxns += 1;
    incomingNotifies++;
    return (0);
}

static sipRet_t
sm_add_contact (ccsip_common_cb_t *cbp, sipMessage_t *msg)
{

    char    src_addr_str[MAX_IPADDR_STR_LEN];
    uint8_t mac_address[MAC_ADDRESS_LENGTH];
    char    contact[MAX_LINE_CONTACT_SIZE];
    char    contact_str[MAX_SIP_URL_LENGTH];
    size_t  escaped_char_str_len;
    sipSCB_t *scbp = (sipSCB_t *)cbp;

    if (!cbp || !msg) {
        return HSTATUS_FAILURE;
    }

    ipaddr2dotted(src_addr_str, &cbp->src_addr);

    if ((cbp->cb_type == SUBNOT_CB) && (scbp->useDeviceAddressing)) {
        platform_get_wired_mac_address(mac_address);
        snprintf(contact_str, MAX_SIP_URL_LENGTH, "<sip:%.4x%.4x%.4x@%s:%d>",
                 mac_address[0] * 256 + mac_address[1],
                 mac_address[2] * 256 + mac_address[3],
                 mac_address[4] * 256 + mac_address[5],
                 src_addr_str, scbp->hb.local_port);
    } else {
        snprintf(contact_str, 6, "<sip:");
        contact[0] = '\0';
        config_get_line_string(CFGID_LINE_CONTACT, contact,
                               cbp->dn_line, sizeof(contact));
        if ((cpr_strcasecmp(contact, UNPROVISIONED) == 0) ||
            (contact[0] == '\0')) {
            // pk-id has not been provisioned, use line name instead
            config_get_line_string(CFGID_LINE_NAME, contact,
                                   cbp->dn_line, sizeof(contact));
        }
        escaped_char_str_len = sippmh_convertURLCharToEscChar(contact,
                                                              strlen(contact),
                                                              contact_str + 5,
                                                              (MAX_SIP_URL_LENGTH - 5),
                                                              FALSE);
        snprintf(contact_str + 5 + escaped_char_str_len,
                 sizeof(contact_str) - 5 - escaped_char_str_len,
                 "@%s:%d;transport=%s>", src_addr_str, cbp->local_port,
                 sipTransportGetTransportType(cbp->dn_line, TRUE, NULL));
    }

    return sippmh_add_text_header(msg, SIP_HEADER_CONTACT, contact_str);
}

static sipRet_t
sm_add_cseq (sipSCB_t *scbp, sipMethod_t method, sipMessage_t *msg)
{
    uint32_t cseq_number;

    if (!scbp || !msg) {
        return HSTATUS_FAILURE;
    }
    if (scbp->ccbp) {
        // Use cseq from CCB
        cseq_number = ++(scbp->ccbp->last_used_cseq);
        /*
         * Remember CSEQ for the request aside from ccb so that
         * CSEQ can be match when response is received (can not.
         * depend on last CSEQ stored on CCB since it can be
         * changed).
         */
        scbp->last_sent_request_cseq = cseq_number;
    } else if (scbp->last_sent_request_cseq == 0) {
        cseq_number = scbp->last_sent_request_cseq = CCSIP_SUBS_START_CSEQ;
    } else {
        cseq_number = ++(scbp->last_sent_request_cseq);
    }

    return (sippmh_add_cseq(msg, sipGetMethodString(method), cseq_number));
}

static boolean
sipSPIAddRouteHeadersToSubNot (sipMessage_t *msg, sipSCB_t * scbp,
                               char *result_route, int result_route_length)
{
    const char     *fname = "sipSPIAddRouteHeadersToSubNot";
    static char     route[MAX_SIP_HEADER_LENGTH * NUM_INITIAL_RECORD_ROUTE_BUFS];
    static char     Contact[MAX_SIP_HEADER_LENGTH];
    boolean         lr = FALSE;
    sipRecordRoute_t *rr_info;

    /* Check args */
    if (!msg) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT),
                          fname, "msg");
        return (FALSE);
    }
    if (!scbp) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT),
                          fname, "scbp");
        return (FALSE);
    }

    if (scbp->ccbp) {
        rr_info = scbp->ccbp->record_route_info;
    } else {
        rr_info = scbp->record_route_info;
    }
    if (!rr_info) {
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Route info not available; will not"
                            " add Route header.\n", DEB_F_PREFIX_ARGS(SIP_SUB, fname));
        return (TRUE);
    }

    memset(route, 0, MAX_SIP_HEADER_LENGTH * NUM_INITIAL_RECORD_ROUTE_BUFS);
    memset(Contact, 0, MAX_SIP_HEADER_LENGTH);

    if (scbp->internal == FALSE) {  /* incoming subscription */
        /*
         * For Incoming dialog (UAS), Copy the RR headers as it is
         * If Contact is present, append it at the end
         */
        if (sipSPIGenerateRouteHeaderUAS(rr_info, route, sizeof(route), &lr)
                == FALSE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sipSPIGenerateRouteHeaderUAS()");
            return (FALSE);
        }
    } else {
        /*
         * For Outgoing dialog (UAC), Copy the RR headers in the reverse
         * order. If Contact is present, append it at the end
         */
        if (sipSPIGenerateRouteHeaderUAC(rr_info, route, sizeof(route), &lr)
                == FALSE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sipSPIGenerateRouteHeaderUAC()");
            return (FALSE);
        }
    }
    /*
     * If loose_routing is TRUE, then the contact header is NOT appended
     * to the Routeset but is instead used in the Req-URI^M
     */
    if (!lr) {
        Contact[0] = '\0';
        if (sipSPIGenerateContactHeader(scbp->contact_info, Contact,
                    sizeof(Contact)) == FALSE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sipSPIGenerateContactHeader()");
            return (FALSE);
        }
        /* Append Contact to the Route Header, if Contact is available */
        if (Contact[0] != '\0') {
            if (route[0] != '\0') {
                sstrncat(route, ", ", sizeof(route) - strlen(route));
            }
            sstrncat(route, Contact, MIN((sizeof(route) - strlen(route)), sizeof(Contact)));
        }
    }

    if (route[0] != '\0') {
        if (STATUS_SUCCESS == sippmh_add_text_header(msg, SIP_HEADER_ROUTE,
                    route)) {
            CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Adding route = %s", DEB_F_PREFIX_ARGS(SIP_SUB, fname), route);
            if (result_route) {
                sstrncpy(result_route, route, result_route_length);
            }
        } else {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sippmh_add_text_header(ROUTE)");
            return (FALSE);
        }
    } else {
        /* Having nothing in Route header is a legal case.
         * This would happen when the Record-Route header has
         * a single entry and Contact was NULL
         */
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Not adding route", DEB_F_PREFIX_ARGS(SIP_SUB, fname));
    }

    return (TRUE);
}

/**************************************************************
 * Format and Send an application generated SUBSCRIBE
 **************************************************************/
boolean
sipSPISendSubscribe (sipSCB_t *scbp, boolean renew, boolean authen)
{
    const char     *fname = "SIPSPISendSubscribe";
    sipMessage_t   *request = NULL;
    sipMethod_t     method = sipMethodInvalid;
    sipRet_t        flag = STATUS_SUCCESS;
    char            src_addr_str[MAX_IPADDR_STR_LEN];
    char            dest_sip_addr_str[MAX_IPADDR_STR_LEN];
    char            addr[MAX_IPADDR_STR_LEN];
    char           *sip_from_temp, *sip_from_tag, *sip_to_temp;
    char            via[SIP_MAX_VIA_LENGTH];
    char            display_name[MAX_LINE_NAME_SIZE];
    char            line_name[MAX_LINE_NAME_SIZE];
    uint8_t         mac_address[MAC_ADDRESS_LENGTH];
    static uint16_t count = 1;
    int             timeout = 0, max_forwards_value = 70;
#define CID_LENGTH 9
    char            cid[CID_LENGTH];
    char            tmp_header[MAX_SIP_URL_LENGTH + 2];
    char           *remvtag = NULL;
    char           *domainloc = NULL;
    char            allow[MAX_SIP_HEADER_LENGTH];

    /*
     * This function builds and sends a SUBSCRIBE message.
     * Specific subscription information including the encoded body, if any,
     * is taken from the SCB pointer passed. The renew flag indicates that the
     * SUBSCRIBE message is to be sent to renew a previous subscription
     */
    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_REQUEST), fname, "SUBSCRIBE");

    if (!scbp) {
        return FALSE;
    }
    // Fill in source and destination addresses
    // Destination address should be the current CCM that is serving us
    // In the future, this part of the code will be using the tables provided
    // by the connection manager

    // First check if the application gave us any forwarding address
    if (scbp->hb.dest_sip_addr.type == CPR_IP_ADDR_INVALID) {
        sipTransportGetPrimServerAddress(scbp->hb.dn_line, addr);
        dns_error_code = sipTransportGetServerAddrPort(addr,
                                                       &scbp->hb.dest_sip_addr,
                                                       (uint16_t *)&scbp->hb.dest_sip_port,
                                                       &scbp->hb.SRVhandle,
                                                       FALSE);
        if (dns_error_code == 0) {
            util_ntohl(&(scbp->hb.dest_sip_addr), &(scbp->hb.dest_sip_addr));
        } else {
            sipTransportGetServerIPAddr(&(scbp->hb.dest_sip_addr), scbp->hb.dn_line);
        }
        scbp->hb.dest_sip_port = ((dns_error_code == 0) &&
                               (scbp->hb.dest_sip_port)) ?
            ntohs((uint16_t) scbp->hb.dest_sip_port) : (sipTransportGetPrimServerPort(scbp->hb.dn_line));
    }
    ipaddr2dotted(src_addr_str, &scbp->hb.src_addr);
    ipaddr2dotted(dest_sip_addr_str, &scbp->hb.dest_sip_addr);

    // Get the MAC address once - since need it in several places
    platform_get_wired_mac_address(mac_address);

    // If there is a related dialog, use the From header + tag,
    // To header + tag, and the call-id from that dialog
    if (scbp->ccbp) {
        if (scbp->ccbp->flags & INCOMING) {
            // If the call was incoming - reverse the headers for an
            // outgoing request
            scbp->sip_to = strlib_copy(scbp->ccbp->sip_from);
            scbp->sip_to_tag = strlib_copy(scbp->ccbp->sip_from_tag);
            scbp->sip_from = strlib_copy(scbp->ccbp->sip_to);
            scbp->sip_from_tag = strlib_copy(scbp->ccbp->sip_to_tag);
        } else {
            // Otherwise copy then in order
            scbp->sip_from = strlib_copy(scbp->ccbp->sip_from);
            scbp->sip_from_tag = strlib_copy(scbp->ccbp->sip_from_tag);
            scbp->sip_to = strlib_copy(scbp->ccbp->sip_to);
            scbp->sip_to_tag = strlib_copy(scbp->ccbp->sip_to_tag);
        }

        if (scbp->ccbp &&
            (scbp->ccbp->state >= SIP_STATE_SENT_INVITE_CONNECTED)) {
            sstrncpy(scbp->SubURI, scbp->ccbp->ReqURI, MAX_SIP_URL_LENGTH);
        } else {
            sstrncpy(scbp->SubURI, "sip:", MAX_SIP_URL_LENGTH);
            domainloc = scbp->SubURI + strlen(scbp->SubURI);
            sstrncpy(domainloc, dest_sip_addr_str,
                     MAX_SIP_URL_LENGTH - (domainloc - (scbp->SubURI)));
        }
        sstrncpy(scbp->hb.sipCallID, scbp->ccbp->sipCallID, MAX_SIP_CALL_ID);
    } else if (!renew) {
        // Create the From header
        sip_from_temp = strlib_open(scbp->sip_from, MAX_SIP_URL_LENGTH);
        if (sip_from_temp) {
            // Use the subscriberURI, if present
            if (scbp->SubscriberURI[0] != '\0') {
                if (scbp->hb.src_addr.type == CPR_IP_ADDR_IPV6) {
                    snprintf(sip_from_temp, MAX_SIP_URL_LENGTH, "<sip:%s@[%s]>",
                             scbp->SubscriberURI, src_addr_str);
                } else {
                    snprintf(sip_from_temp, MAX_SIP_URL_LENGTH, "<sip:%s@%s>",
                             scbp->SubscriberURI, src_addr_str);
                }
            } else if (scbp->useDeviceAddressing) {
                if (scbp->hb.src_addr.type == CPR_IP_ADDR_IPV6) {

                    snprintf(sip_from_temp, MAX_SIP_URL_LENGTH,
                             "<sip:%.4x%.4x%.4x@[%s]>",
                             mac_address[0] * 256 + mac_address[1],
                            mac_address[2] * 256 + mac_address[3],
                            mac_address[4] * 256 + mac_address[5],
                            src_addr_str);
                } else {
                    snprintf(sip_from_temp, MAX_SIP_URL_LENGTH,
                             "<sip:%.4x%.4x%.4x@%s>",
                             mac_address[0] * 256 + mac_address[1],
                            mac_address[2] * 256 + mac_address[3],
                            mac_address[4] * 256 + mac_address[5],
                            src_addr_str);
                }
            } else {
                sip_config_get_display_name(scbp->hb.dn_line, display_name,
                                            sizeof(display_name));
                config_get_line_string(CFGID_LINE_NAME, line_name,
                                       scbp->hb.dn_line, sizeof(line_name));
                if (scbp->hb.src_addr.type == CPR_IP_ADDR_IPV6) {

                    snprintf(sip_from_temp, MAX_SIP_URL_LENGTH,
                             "\"%s\" <sip:%s@[%s]>", display_name,
                             line_name, src_addr_str);
                } else {
                    snprintf(sip_from_temp, MAX_SIP_URL_LENGTH,
                             "\"%s\" <sip:%s@%s>", display_name,
                             line_name, src_addr_str);
                }
            }

            // Add tag to the From header (need to allocate and fill in scbp->sip_from_tag
            sip_from_tag = strlib_open(scbp->sip_from_tag, MAX_SIP_URL_LENGTH);
            if (sip_from_tag) {
                sip_util_make_tag(sip_from_tag);
                sstrncat(sip_from_temp, ";tag=",
                        MAX_SIP_URL_LENGTH - strlen(sip_from_temp));
                sstrncat(sip_from_temp, sip_from_tag,
                        MAX_SIP_URL_LENGTH - strlen(sip_from_temp));
            }
            scbp->sip_from_tag = strlib_close(sip_from_tag);
        }
        scbp->sip_from = strlib_close(sip_from_temp);

        // Create the Call-ID header - For now create own call-id
        count++;

        snprintf(scbp->hb.sipCallID, MAX_SIP_CALL_ID,
                 "%.4x%.4x-%.4x%.4x-%.8x-%.8x@%s",
                 mac_address[0] * 256 + mac_address[1],
                 mac_address[2] * 256 + mac_address[3],
                 mac_address[4] * 256 + mac_address[5], count,
                 (unsigned int) cpr_rand(),
                 (unsigned int) cpr_rand(),
                 src_addr_str);

        // Create the ReqURI
        sstrncpy(scbp->SubURI, "sip:", MAX_SIP_URL_LENGTH);

        /* Indialog requests should contain suburi only
         */
        sstrncat(scbp->SubURI, scbp->SubURIOriginal, MAX_SIP_URL_LENGTH - sizeof("sip:"));
        domainloc = strchr(scbp->SubURI, '@');
        if (domainloc == NULL) {
            domainloc = scbp->SubURI + strlen(scbp->SubURI);
            if ((domainloc - scbp->SubURI) < (MAX_SIP_URL_LENGTH - 1)) {
                /* Do not include @ when there is no user part */
                if (scbp->SubURIOriginal[0] != '\0') {
                    *domainloc++ = '@';
                }
                sstrncpy(domainloc, dest_sip_addr_str,
                         MAX_SIP_URL_LENGTH - (domainloc - (scbp->SubURI)));
            }
        }
        // Create the To header
        sip_to_temp = strlib_open(scbp->sip_to, MAX_SIP_URL_LENGTH);
        if (sip_to_temp) {
            snprintf(sip_to_temp, MAX_SIP_URL_LENGTH, "<%s>", scbp->SubURI);
            scbp->sip_to = strlib_close(sip_to_temp);
        }
    }

    method = sipMethodSubscribe;


    scbp->last_sent_request_cseq_method = method;

    // Get a blank SIP message template and fill it in
    request = GET_SIP_MESSAGE();
    if (!request) {
        return FALSE;
    }
    // Can't use CreateRequest to fill in the message since not using CCB. So
    // do the following:
    // 1. Add request line (sipSPIAddRequestLine).
    if (HSTATUS_SUCCESS != sippmh_add_request_line(request,
                                                   sipGetMethodString(method),
                                                   scbp->SubURI, SIP_VERSION)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Request line", fname);
        free_sip_message(request);
        return (FALSE);
    }
    // 2. Add local Via (sipSPIAddLocalVia)
    snprintf(via, sizeof(via), "SIP/2.0/%s %s:%d;%s=%s%.8x",
             sipTransportGetTransportType(scbp->hb.dn_line, TRUE, NULL),
             src_addr_str, scbp->hb.local_port, VIA_BRANCH,
             VIA_BRANCH_START, (unsigned int) cpr_rand());
    if (HSTATUS_SUCCESS != sippmh_add_text_header(request, SIP_HEADER_VIA, via)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding VIA header", fname);
        free_sip_message(request);
        return (FALSE);
    }

    // 3. Add common headers (sipSPIAddCommonHeaders)
    if ((HSTATUS_SUCCESS != sippmh_add_text_header(request, SIP_HEADER_FROM, scbp->sip_from)) ||
        (HSTATUS_SUCCESS != sippmh_add_text_header(request, SIP_HEADER_TO, scbp->sip_to)) ||
        (HSTATUS_SUCCESS != sippmh_add_text_header(request, SIP_HEADER_CALLID, scbp->hb.sipCallID)) ||
        (HSTATUS_SUCCESS != sipAddDateHeader(request)) ||
        (HSTATUS_SUCCESS != sm_add_cseq(scbp, method, request))) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding either From, To, CallID, "
                          "Date or Cseq header\n", fname);
        free_sip_message(request);
        return (FALSE);
    }
    // 4. Add text headers (sippmh_add_text_header)
    (void) sippmh_add_text_header(request, SIP_HEADER_USER_AGENT,
                                  sipHeaderUserAgent);

    // 5. Add general headers (AddGeneralHeaders)
    // Event header is not needed for Refer msg
    if (method != sipMethodRefer) {
        if (HSTATUS_SUCCESS != sippmh_add_text_header(request, SIP_HEADER_EVENT,
                                scbp->event_name)) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Event header", fname);
            free_sip_message(request);
            return (FALSE);
        }
    }

    if (scbp->hb.accept_type == CC_SUBSCRIPTIONS_DIALOG) {
        flag = sippmh_add_text_header(request, SIP_HEADER_ACCEPT,
                                      dialogAcceptHeader);
    } else if (scbp->hb.event_type == CC_SUBSCRIPTIONS_KPML) {
        flag = sippmh_add_text_header(request, SIP_HEADER_ACCEPT,
                                      kpmlResponseAcceptHeader);
    } else if (scbp->hb.event_type == CC_SUBSCRIPTIONS_DIALOG) {
        flag = sippmh_add_text_header(request, SIP_HEADER_ACCEPT,
                                      dialogAcceptHeader);
    } else if (scbp->hb.event_type == CC_SUBSCRIPTIONS_PRESENCE) {
        flag = sippmh_add_text_header(request, SIP_HEADER_ACCEPT,
                                      presenceAcceptHeader);
    }
    if (HSTATUS_SUCCESS != flag) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Accept header", fname);
        free_sip_message(request);
        return (FALSE);
    }

    if (HSTATUS_SUCCESS != sippmh_add_int_header(request, SIP_HEADER_EXPIRES,
                scbp->hb.expires)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Expires header", fname);
        free_sip_message(request);
        return (FALSE);
    }
    // Add max-forwards header
    config_get_value(CFGID_SIP_MAX_FORWARDS, &max_forwards_value,
                     sizeof(max_forwards_value));
    if (HSTATUS_SUCCESS !=
        sippmh_add_int_header(request, SIP_HEADER_MAX_FORWARDS,
            max_forwards_value)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Max-Forwards header", fname);
        free_sip_message(request);
        return (FALSE);
    }
    // Add Contact header
    if (HSTATUS_SUCCESS != sm_add_contact(&(scbp->hb), request)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Contact header", fname);
        free_sip_message(request);
        return (FALSE);
    }

    if (authen) {
        if (HSTATUS_SUCCESS != sippmh_add_text_header(request, AUTHOR_HDR(scbp->hb.authen.status_code),
                                                      scbp->hb.authen.authorization)) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Authorization header", fname);
            free_sip_message(request);
            return (FALSE);
        }
    }

    if (scbp->norefersub) {
        if (HSTATUS_SUCCESS != sippmh_add_text_header(request, SIP_HEADER_REQUIRE, "norefersub")) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Require header", fname);
            free_sip_message(request);
            return (FALSE);
        }
    }

    if (method == sipMethodRefer) {
        // Add Refer-To, Referred-By, and Content-ID headers
        sstrncpy(tmp_header, (const char *) (scbp->sip_from), MAX_SIP_URL_LENGTH + 1);
        remvtag = strstr(tmp_header, ";tag=");
        if (remvtag) {
            *remvtag = '\0';
        }
        if (HSTATUS_SUCCESS != sippmh_add_text_header(request, SIP_HEADER_REFERRED_BY, tmp_header)) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Referred-By header", fname);
            free_sip_message(request);
            return (FALSE);
        }

        snprintf(cid, sizeof(cid), "%.8x", (unsigned int)cpr_rand());
        snprintf(tmp_header, sizeof(tmp_header), "cid:%s@%s", cid, src_addr_str);
        if (HSTATUS_SUCCESS != sippmh_add_text_header(request, SIP_HEADER_REFER_TO, tmp_header)) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Refer-To header", fname);
            free_sip_message(request);
            return (FALSE);
        }

        snprintf(tmp_header, sizeof(tmp_header), "<%s@%s>", cid, src_addr_str);
        if (HSTATUS_SUCCESS != sippmh_add_text_header(request, SIP_HEADER_CONTENT_ID, tmp_header)) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Content-ID header", fname);
            free_sip_message(request);
            return (FALSE);
        }
    }

    // Add Allow
    snprintf(allow, MAX_SIP_HEADER_LENGTH, "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s",
             SIP_METHOD_ACK, SIP_METHOD_BYE, SIP_METHOD_CANCEL,
             SIP_METHOD_INVITE, SIP_METHOD_NOTIFY, SIP_METHOD_OPTIONS,
             SIP_METHOD_REFER, SIP_METHOD_REGISTER, SIP_METHOD_UPDATE,
             SIP_METHOD_SUBSCRIBE);
    if (HSTATUS_SUCCESS != sippmh_add_text_header(request, SIP_HEADER_ALLOW, allow)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Allow header", fname);
        free_sip_message(request);
        return (FALSE);
    }

    /*
     * Add route header if needed.
     */
    if (FALSE == sipSPIAddRouteHeadersToSubNot(request, scbp, NULL, 0)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Route header", fname);
        free_sip_message(request);
        return (FALSE);
    }
    // Add content, if any
    if (scbp->hb.event_data_p) {
        if (add_content(scbp->hb.event_data_p, request, fname) == FALSE) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Content", fname);
            free_sip_message(request);
            return (FALSE);
        }
    } else {
        if (HSTATUS_SUCCESS != sippmh_add_int_header(request, SIP_HEADER_CONTENT_LENGTH, 0)) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Content-Len", fname);
            free_sip_message(request);
            return (FALSE);
        }
    }

    ccsip_common_util_set_retry_settings((ccsip_common_cb_t *)scbp, &timeout);
    if (sipTransportCreateSendMessage(NULL, request, method,
                                      &(scbp->hb.dest_sip_addr),
                                      (int16_t) scbp->hb.dest_sip_port,
                                      FALSE, TRUE, timeout, scbp,
                                      RELDEV_NO_STORED_MSG) < 0) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"failed to send message", fname);
        return (FALSE);
    }

    return (TRUE);
}

/**************************************************************
 * Format and send a response to a network received SUBSCRIBE
 **************************************************************/
boolean
sipSPISendSubscribeNotifyResponse (sipSCB_t *scbp,
                                   uint16_t response_code,
                                   uint32_t cseq)
{
    const char     *fname = "sipSPISendSubscribeNotifyResponse";
    sipMessage_t   *response = NULL;
    sipVia_t       *via = NULL;
    sub_not_trxn_t *trxn_p;
    int            status_code = 0;

    // currently not used  const char *request_callid = NULL;
    cpr_ip_addr_t   cc_remote_ipaddr;
    uint16_t        cc_remote_port = 0;

    // currently not used  int timeout = 0;
    char           *pViaHeaderStr = NULL;
    char           *dest_ip_addr_str = 0;
    char            src_addr_str[MAX_IPADDR_STR_LEN];
    char            response_text[MAX_SIP_URL_LENGTH];
    boolean         port_present = FALSE;
    int             reldev_stored_msg;

    sipRet_t        tflag = HSTATUS_SUCCESS;

    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_RESPONSE), fname, response_code);

    memset(&cc_remote_ipaddr, 0, sizeof(cpr_ip_addr_t));
    cc_remote_ipaddr = ip_addr_invalid;
    if (!scbp) {
        return FALSE;
    }

    /*
     * In the response to Subscribe:
     * The Via header gets a ";received" addition (...;received: 192.168.0.7)
     * The From header is the same as in the subscribe request
     * The To header is the same as in the subscribe request but has a tag
     * The call-id is the same as in the request
     * Content Length is set to 0
     */
    response = GET_SIP_MESSAGE();
    if (!response) {
        return FALSE;
    }

    get_sip_error_string(response_text, response_code);

    tflag = sippmh_add_response_line(response, SIP_VERSION, response_code,
                                     response_text);
    if (tflag != HSTATUS_SUCCESS) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding response line", fname);
        free_sip_message(response);
        return FALSE;
    }

    /*
     * find the transaction and fetch the via header
     */
    trxn_p = sll_find(scbp->incoming_trxns, &cseq);
    if (trxn_p) {
        pViaHeaderStr = trxn_p->via;
        /* remove the transaction  because we are done with it */
        (void)sll_remove(scbp->incoming_trxns, trxn_p);
        cpr_free(trxn_p);
    }
    if (pViaHeaderStr) {
        via = sippmh_parse_via(pViaHeaderStr);
    }
    if (!via || !pViaHeaderStr) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "No or Bad Via Header present in Message!");
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in parsing Via header", fname);
        if (via)
            sippmh_free_via(via);
        free_sip_message(response);
        cpr_free(pViaHeaderStr);
        return (FALSE);
    }
    if (STATUS_SUCCESS != sippmh_add_text_header(response, SIP_HEADER_VIA, pViaHeaderStr)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Via header", fname);
        sippmh_free_via(via);
        free_sip_message(response);
        cpr_free(pViaHeaderStr);
        return (FALSE);
    }
    /* free the via header because we will not need it anymore */
    cpr_free(pViaHeaderStr);

    // Add common headers: From, To, CallID, Date, CSeq, Contact
    // Note: If this is a response to a NOTIFY, then from and to headers
    // must be reversed
    if (scbp->last_recv_request_cseq_method == sipMethodNotify) {
        if (STATUS_SUCCESS != sippmh_add_text_header(response, SIP_HEADER_FROM, scbp->sip_to)) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding From header", fname);
            sippmh_free_via(via);
            free_sip_message(response);
            return (FALSE);
        }

        if (STATUS_SUCCESS != sippmh_add_text_header(response, SIP_HEADER_TO, scbp->sip_from)) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding To header", fname);
            sippmh_free_via(via);
            free_sip_message(response);
            return (FALSE);
        }
    } else {
        if (STATUS_SUCCESS != sippmh_add_text_header(response, SIP_HEADER_FROM, scbp->sip_from)) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding From header", fname);
            sippmh_free_via(via);
            free_sip_message(response);
            return (FALSE);
        }

        if (STATUS_SUCCESS != sippmh_add_text_header(response, SIP_HEADER_TO, scbp->sip_to)) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding To header", fname);
            sippmh_free_via(via);
            free_sip_message(response);
            return (FALSE);
        }
    }

    if (STATUS_SUCCESS != sippmh_add_text_header(response, SIP_HEADER_CALLID, scbp->hb.sipCallID)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding CallID header", fname);
        sippmh_free_via(via);
        free_sip_message(response);
        return (FALSE);
    }

    if (HSTATUS_SUCCESS != sipAddDateHeader(response)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Date header", fname);
        sippmh_free_via(via);
        free_sip_message(response);
        return (FALSE);
    }

        if (HSTATUS_SUCCESS != sippmh_add_cseq(response,
                                               sipGetMethodString(scbp->
                                               last_recv_request_cseq_method),
                                               cseq)) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding CSeq header", fname);
            sippmh_free_via(via);
            free_sip_message(response);
            return (FALSE);
        }

    tflag = sippmh_add_text_header(response, SIP_HEADER_SERVER, sipHeaderServer);
    if (tflag != HSTATUS_SUCCESS) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Server header", fname);
        sippmh_free_via(via);
        free_sip_message(response);
        return (FALSE);
    }
    ipaddr2dotted(src_addr_str, &scbp->hb.src_addr);

    tflag = sm_add_contact(&(scbp->hb), response);

    if (tflag != HSTATUS_SUCCESS) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Contact header", fname);
        sippmh_free_via(via);
        free_sip_message(response);
        return (FALSE);
    }
    // Add expires header - only for a SUBSCRIBE response
    if (scbp->last_recv_request_cseq_method == sipMethodSubscribe) {
        if (sippmh_add_int_header(response, SIP_HEADER_EXPIRES, scbp->hb.expires) != 0) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Expires header", fname);
            sippmh_free_via(via);
            free_sip_message(response);
            return (FALSE);
        }
        /*
         * add Record-Route Header, if it is a response (18x/2xx) to
         * dialog initiating SUBSCRIBE.
         */
        if ((scbp->cached_record_route[0] != '\0') &&
            (((response_code >= 180) && (response_code <= 189)) ||
             ((response_code >= 200) && (response_code <= 299)))) {
            tflag = sippmh_add_text_header(response, SIP_HEADER_RECORD_ROUTE,
                                           scbp->cached_record_route);
            if (tflag != HSTATUS_SUCCESS) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Record-Route header",
                                  fname);
                sippmh_free_via(via);
                free_sip_message(response);
                return (FALSE);
            }
        }
        /* free the cached record_route, once the final resp is sent */
        if (response_code >= 200) {
            strlib_free(scbp->cached_record_route);
            scbp->cached_record_route = strlib_empty();
        }
    }
    // Add other headers such as content-length, content-type
    if (HSTATUS_SUCCESS != sippmh_add_int_header(response, SIP_HEADER_CONTENT_LENGTH, 0)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Content-Length header", fname);
        sippmh_free_via(via);
        free_sip_message(response);
        return (FALSE);
    }
    // Send Response
    // Get information on where to send the response from via headers
    if (via->remote_port) {
        cc_remote_port = via->remote_port;
        port_present = TRUE;
    } else {
        /* Use default 5060 if via does not have port */
        cc_remote_port = SIP_WELL_KNOWN_PORT;
    }

    CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Via host: %s, port: %d, rcvd host: %s",
                      fname,  via->host, via->remote_port, via->recd_host);

    if (via->maddr) {
        if (!port_present) {
            dns_error_code = sipTransportGetServerAddrPort(via->maddr,
                                                           &cc_remote_ipaddr,
                                                           &cc_remote_port,
                                                           NULL, FALSE);
        } else {
            dns_error_code = dnsGetHostByName(via->maddr, &cc_remote_ipaddr,
                                               100, 1);
        }

        if (dns_error_code != 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname,
                              "sipTransportGetServerAddrPort or dnsGetHostByName()");
        } else {
            util_ntohl(&cc_remote_ipaddr, &cc_remote_ipaddr);
        }
    }
    // If all else fails send the response to where we got the request from
    if (cc_remote_ipaddr.type == CPR_IP_ADDR_INVALID) {
        if (via->recd_host) {
            dest_ip_addr_str = via->recd_host;
        } else {
            dest_ip_addr_str = via->host;
        }

        if (!port_present) {
            dns_error_code = sipTransportGetServerAddrPort(dest_ip_addr_str,
                                                           &cc_remote_ipaddr,
                                                           &cc_remote_port,
                                                           NULL, FALSE);
        } else {
            dns_error_code = dnsGetHostByName(dest_ip_addr_str,
                                               &cc_remote_ipaddr, 100, 1);
        }

        if (dns_error_code != 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname,
                              "sipTransportGetServerAddrPort or dnsGetHostByName()");
            // cpr_free(request_cseq_structure);
            sippmh_free_via(via);
            free_sip_message(response);
            return (FALSE);
        } else {
            util_ntohl(&cc_remote_ipaddr, &cc_remote_ipaddr);
        }
    }
    // Store cc_remote_ipaddr and cc_remote_port values in SCB for later use
    // in the corresponding NOTIFY, if any
    scbp->hb.dest_sip_addr = cc_remote_ipaddr;
    scbp->hb.dest_sip_port = cc_remote_port;

    reldev_stored_msg = sipRelDevCoupledMessageStore(response, scbp->hb.sipCallID,
                                                     cseq,
                                                     scbp->last_recv_request_cseq_method,
                                                     FALSE,
                                                     status_code,
                                                     &cc_remote_ipaddr,
                                                     cc_remote_port, TRUE);

    sippmh_free_via(via);

    scbp->hb.retx_flag = FALSE;
    if (sipTransportCreateSendMessage(NULL, response, sipMethodSubscribe,
                                      &cc_remote_ipaddr, cc_remote_port,
                                      FALSE, TRUE, 0, scbp, reldev_stored_msg) != 0) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"failed to send message", fname);
        return (FALSE);
    }

    return (TRUE);
}

/**************************************************************
 * Format and send a locally generated NOTIFY
 **************************************************************/
boolean
sipSPISendSubNotify (ccsip_common_cb_t *cbp, boolean authen)
{
    const char     *fname = "SIPSPISendSubNotify";
    sipMessage_t   *request = NULL;
    static uint32_t   cseq = 0;

    // currently not used  char *message_body = NULL;
    // currently not used  sipRet_t flag = STATUS_SUCCESS;
    char            src_addr_str[MAX_IPADDR_STR_LEN];
    char            dest_sip_addr_str[MAX_IPADDR_STR_LEN];
    char            addr[MAX_IPADDR_STR_LEN];

    // currently not used  uint32_t nbytes = SIP_UDP_MESSAGE_SIZE;
    char            via[SIP_MAX_VIA_LENGTH];

    // currently not used  short send_to_proxy_handle = INVALID_SOCKET;
    static char     ReqURI[MAX_SIP_URL_LENGTH];
    static char     sip_temp_str[MAX_SIP_URL_LENGTH];
    static char     sip_temp_tag[MAX_SIP_URL_LENGTH];
    sipSubscriptionStateInfo_t subs_state;
    int             timeout = 0, max_forwards_value = 70;
    char            allow[MAX_SIP_HEADER_LENGTH];
    sipSCB_t *scbp = (sipSCB_t *)cbp;
    sipTCB_t *tcbp = (sipTCB_t *)cbp;

    /*
     * This function builds and sends a NOTIFY message as a follow up to
     * a previously received SUBSCRIBE
     */
    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_REQUEST), fname, "NOTIFY");

    if (!cbp) {
        return FALSE;
    }

    if (util_check_if_ip_valid(&cbp->dest_sip_addr)== FALSE) {
        sipTransportGetPrimServerAddress(scbp->hb.dn_line, addr);

        dns_error_code = sipTransportGetServerAddrPort(addr,
                                                       &cbp->dest_sip_addr,
                                                       (uint16_t *)&cbp->dest_sip_port,
                                                       &cbp->SRVhandle,
                                                       FALSE);

        if (dns_error_code == 0) {
            util_ntohl(&(cbp->dest_sip_addr), &(cbp->dest_sip_addr));
        } else {
            sipTransportGetServerIPAddr(&(cbp->dest_sip_addr), cbp->dn_line);
        }

        cbp->dest_sip_port = ((dns_error_code == 0) &&
                               (cbp->dest_sip_port)) ?
            ntohs(cbp->dest_sip_port) : (sipTransportGetPrimServerPort(cbp->dn_line));

    }

    ipaddr2dotted(src_addr_str, &cbp->src_addr);
    ipaddr2dotted(dest_sip_addr_str, &cbp->dest_sip_addr);

    // Create the request
    request = GET_SIP_MESSAGE();

    /*
     * Determine the Request-URI
     */
    memset(ReqURI, 0, sizeof(ReqURI));

    if (cbp->cb_type == SUBNOT_CB) {
        if (scbp->contact_info) {
            // Build Req-URI from contact info
            sipContact_t *response_contact_info;
            sipUrl_t *sipUrl = NULL;
            cpr_ip_addr_t request_uri_addr;
            uint16_t request_uri_port = 0;

            request_uri_addr = ip_addr_invalid;

            response_contact_info = scbp->contact_info;
            if (response_contact_info->locations[0]->genUrl->schema == URL_TYPE_SIP) {
                sipUrl = response_contact_info->locations[0]->genUrl->u.sipUrl;
            } else {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"URL is not SIP", fname);
                free_sip_message(request);
                return (FALSE);
            }

            request_uri_port = sipUrl->port;
            dns_error_code = sipTransportGetServerAddrPort(sipSPIUrlDestination(sipUrl),
                                                           &request_uri_addr,
                                                           &request_uri_port, NULL, FALSE);
            if (dns_error_code == 0) {
                util_ntohl(&request_uri_addr, &request_uri_addr);
            } else {
                request_uri_addr = ip_addr_invalid;
            }

            if (sipUrl->user != NULL) {
                if (sipUrl->password) {
                    snprintf(ReqURI, sizeof(ReqURI),
                             sipUrl->is_phone ?
                             "sip:%s:%s@%s:%d;user=phone" :
                             "sip:%s:%s@%s:%d",
                             sipUrl->user, sipUrl->password,
                             sipUrl->host, sipUrl->port);
                } else {
                    snprintf(ReqURI, sizeof(ReqURI),
                             sipUrl->is_phone ?
                             "sip:%s@%s:%d;user=phone" :
                             "sip:%s@%s:%d",
                             sipUrl->user, sipUrl->host,
                             sipUrl->port);
                }
            } else {
                snprintf(ReqURI, sizeof(ReqURI),
                         sipUrl->is_phone ?
                         "sip:%s:%d;user=phone" :
                         "sip:%s:%d",
                         sipUrl->host, sipUrl->port);
            }
        } else {
            // Build Req-URI from FROM field
            sipLocation_t  *request_uri_loc = NULL;
            char            sip_from[MAX_SIP_URL_LENGTH];
            sipUrl_t       *request_uri_url = NULL;

            sstrncpy(sip_from, scbp->sip_from, MAX_SIP_URL_LENGTH);
            request_uri_loc = sippmh_parse_from_or_to(sip_from, TRUE);
            if (!request_uri_loc) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                                  fname, "sippmh_parse_from_or_to(TO)");
                free_sip_message(request);
                return (FALSE);
            }

            if (!sippmh_valid_url(request_uri_loc->genUrl)) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                                  fname, "sippmh_valid_url()");
                sippmh_free_location(request_uri_loc);
                free_sip_message(request);
                return (FALSE);
            }

            CCSIP_DEBUG_STATE(DEB_F_PREFIX"Forming Req-URI (Caller): using original "
                              "Req-URI\n", DEB_F_PREFIX_ARGS(SIP_SUB, fname));
            /*
             * if (request_uri_loc->name) {
             * if (request_uri_loc->name[0]) {
             * sstrncat(ReqURI, "\"", sizeof(ReqURI)-strlen(ReqURI));
             * sstrncat(ReqURI, request_uri_loc->name,
             * sizeof(ReqURI)-strlen(ReqURI));
             * sstrncat(ReqURI, "\" ", sizeof(ReqURI)-strlen(ReqURI));
             * }
             * }
             */
            sstrncat(ReqURI, "sip:", sizeof(ReqURI) - strlen(ReqURI));
            if (request_uri_loc->genUrl->schema == URL_TYPE_SIP) {
                request_uri_url = request_uri_loc->genUrl->u.sipUrl;
            } else {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"URL is not SIP", fname);
                sippmh_free_location(request_uri_loc);
                free_sip_message(request);
                return (FALSE);
            }

            if (request_uri_url->user) {
                sstrncat(ReqURI, request_uri_url->user,
                        sizeof(ReqURI) - strlen(ReqURI));
                sstrncat(ReqURI, "@", sizeof(ReqURI) - strlen(ReqURI));
            }
            if (request_uri_url->is_phone) {
                sstrncat(ReqURI, ";user=phone",
                        sizeof(ReqURI) - strlen(ReqURI));
            }
            sstrncat(ReqURI, request_uri_url->host,
                    sizeof(ReqURI) - strlen(ReqURI));
            // sstrncat(ReqURI, ">", sizeof(ReqURI)-strlen(ReqURI));
            sippmh_free_location(request_uri_loc);
        }
    } else { //Unsolicited NOTIFY
        char               line_name[MAX_LINE_NAME_SIZE];
        config_get_line_string(CFGID_LINE_NAME, line_name, cbp->dn_line, sizeof(line_name));
        snprintf(ReqURI, MAX_SIP_URL_LENGTH, "sip:%s@%s", line_name, dest_sip_addr_str);
        sstrncpy(tcbp->full_ruri, ReqURI, MAX_SIP_URL_LENGTH);
    }
    (void) sippmh_add_request_line(request,
                                   sipGetMethodString(sipMethodNotify),
                                   ReqURI, SIP_VERSION);

    // The Via header has the local URI and a new branch
    snprintf(via, sizeof(via), "SIP/2.0/%s %s:%d;%s=%s%.8x",
             sipTransportGetTransportType(cbp->dn_line, TRUE, NULL),
             src_addr_str, cbp->local_port, VIA_BRANCH,
             VIA_BRANCH_START, (unsigned int) cpr_rand());

    if (STATUS_SUCCESS != sippmh_add_text_header(request, SIP_HEADER_VIA, via)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding VIA header", fname);
        free_sip_message(request);
        return (FALSE);
    }

    // The To field including the tag is the same as the From field in the
    // response to SUBSCRIBE
    if (cbp->cb_type != SUBNOT_CB) {
        snprintf(sip_temp_str, MAX_SIP_URL_LENGTH, "<%s>", ReqURI);
    }
    if (STATUS_SUCCESS != sippmh_add_text_header(request, SIP_HEADER_TO,
                                                 ((cbp->cb_type == SUBNOT_CB) ? scbp->sip_from : sip_temp_str))) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding To header", fname);
        free_sip_message(request);
        return (FALSE);
    }

    // The From field including the tag is the same as the To field in the
    // response to SUBSCRIBE
    if (cbp->cb_type != SUBNOT_CB) {
        sstrncat(sip_temp_str, ";tag=", MAX_SIP_URL_LENGTH - strlen(sip_temp_str));
        sip_util_make_tag(sip_temp_tag);
        sstrncat(sip_temp_str, sip_temp_tag, MAX_SIP_URL_LENGTH - strlen(sip_temp_str));
    }
    if (STATUS_SUCCESS != sippmh_add_text_header(request, SIP_HEADER_FROM,
                                                 ((cbp->cb_type == SUBNOT_CB) ?  scbp->sip_to : sip_temp_str))) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding From header", fname);
        free_sip_message(request);
        return (FALSE);
    }
    // The call-id used is the same as in the SCB
    if (cbp->sipCallID[0] == 0) {
        snprintf(tcbp->hb.sipCallID, sizeof(tcbp->hb.sipCallID), "%.8x-%.8x@%s", // was MAX_SIP_URL_LENGTH
                 (unsigned int) cpr_rand(),
                 (unsigned int) cpr_rand(),
                 src_addr_str);
    }
    if (STATUS_SUCCESS != sippmh_add_text_header(request, SIP_HEADER_CALLID, cbp->sipCallID)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding CallID header", fname);
        free_sip_message(request);
        return (FALSE);
    }

    if (HSTATUS_SUCCESS != sipAddDateHeader(request)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Date header", fname);
        free_sip_message(request);
        return (FALSE);
    }

    if (cbp->cb_type == SUBNOT_CB) {
        if (sm_add_cseq(scbp, sipMethodNotify, request) != HSTATUS_SUCCESS) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding CSeq header", fname);
            free_sip_message(request);
            return (FALSE);
        }
    } else {
        cseq++;
        if (cseq == 0) {
            cseq = 1;
        }
        if (HSTATUS_SUCCESS != sippmh_add_cseq(request, sipGetMethodString(sipMethodNotify), cseq)) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding CSEQ header", fname);
            free_sip_message(request);
            return (FALSE);
        }
    }

    // Event is the event name
    // The subscription-state header is active
    if (cbp->event_type - CC_SUBSCRIPTIONS_DIALOG > -1 &&
        cbp->event_type - CC_SUBSCRIPTIONS_DIALOG < 5) {
        if (HSTATUS_SUCCESS != sippmh_add_text_header(request, SIP_HEADER_EVENT,
                                                      eventNames[cbp->event_type - CC_SUBSCRIPTIONS_DIALOG])) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Event header", fname);
            free_sip_message(request);
            return (FALSE);
        }
    }

    subs_state.expires = cbp->expires;
    if (cbp->cb_type == SUBNOT_CB) {
        if (subs_state.expires > 0) {
            subs_state.state = SUBSCRIPTION_STATE_ACTIVE;
        } else {
            subs_state.state = SUBSCRIPTION_STATE_TERMINATED;
        }
    }  else {
        subs_state.state = SUBSCRIPTION_STATE_ACTIVE;
    }
    if (sippmh_add_subscription_state(request, &subs_state) != 0) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Subscription-State header", fname);
        free_sip_message(request);
        return (FALSE);
    }
    // Add max-forwards header
    config_get_value(CFGID_SIP_MAX_FORWARDS, &max_forwards_value,
                     sizeof(max_forwards_value));
    (void) sippmh_add_int_header(request, SIP_HEADER_MAX_FORWARDS,
                                 max_forwards_value);

    // Add contact header
    if (sm_add_contact(cbp, request) != HSTATUS_SUCCESS) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Contact header", fname);
        free_sip_message(request);
        return (FALSE);
    }

    if (authen) {
        (void) sippmh_add_text_header(request,
                                      AUTHOR_HDR(scbp->hb.authen.status_code),
                                      scbp->hb.authen.authorization);
    }
    // Add Allow
    snprintf(allow, MAX_SIP_HEADER_LENGTH, "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s",
             SIP_METHOD_ACK, SIP_METHOD_BYE, SIP_METHOD_CANCEL,
             SIP_METHOD_INVITE, SIP_METHOD_NOTIFY, SIP_METHOD_OPTIONS,
             SIP_METHOD_REFER, SIP_METHOD_REGISTER, SIP_METHOD_UPDATE,
             SIP_METHOD_SUBSCRIBE);
    (void) sippmh_add_text_header(request, SIP_HEADER_ALLOW, allow);

    if (cbp->cb_type == SUBNOT_CB) {
        /* keep the request method to be sent */
        scbp->last_sent_request_cseq_method = sipMethodNotify;
        /*
         * Add route header if needed.
         */
        if (FALSE == sipSPIAddRouteHeadersToSubNot(request, scbp, NULL, 0)) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in adding Route header", fname);
            free_sip_message(request);
            return (FALSE);
        }
        ccsip_common_util_set_retry_settings((ccsip_common_cb_t *)scbp, &timeout);
    }

    // Add content, if any
    if (cbp->event_data_p) {
        if (add_content(cbp->event_data_p, request, fname) == FALSE) {
            free_sip_message(request);
            return (FALSE);
        }
    } else {
        (void) sippmh_add_int_header(request, SIP_HEADER_CONTENT_LENGTH, 0);
    }

    if (sipTransportCreateSendMessage(NULL, request, sipMethodNotify,
                                      &(scbp->hb.dest_sip_addr),
                                      (int16_t) scbp->hb.dest_sip_port,
                                      FALSE, TRUE, timeout, cbp,
                                      RELDEV_NO_STORED_MSG) != 0) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in sending message", fname);
        return (FALSE);
    }

    return (TRUE);
}

/**************************************************************
 * Handle message retries
 *************************************************************/
int
subsmanager_handle_retry_timer_expire (int scb_index)
{
    const char     *fname = "subsmanager_handle_retry_timer_expire";
    sipSCB_t       *scbp = NULL;
    uint32_t        max_retx = 0;
    ccsip_sub_not_data_t sub_not_result_data;
    uint32_t        time_t1 = 0;
    uint32_t        time_t2 = 0;
    uint32_t        timeout = 0;

    CCSIP_DEBUG_TASK("Entering %s. scb_index: %d", fname, scb_index);

    if (scb_index < 0 || scb_index >= MAX_SCBS) {
        return (-1);
    }
    scbp = &(subsManagerSCBS[scb_index]);

    if (scbp->hb.retx_flag == TRUE) {
        config_get_value(CFGID_SIP_RETX, &max_retx, sizeof(max_retx));
        if (max_retx > MAX_NON_INVITE_RETRY_ATTEMPTS) {
            max_retx = MAX_NON_INVITE_RETRY_ATTEMPTS;
        }
        if (scbp->hb.retx_counter < max_retx) {
            config_get_value(CFGID_TIMER_T1, &time_t1, sizeof(time_t1));
            scbp->hb.retx_counter++;
            timeout = time_t1 * (1 << scbp->hb.retx_counter);
            config_get_value(CFGID_TIMER_T2, &time_t2, sizeof(time_t2));
            if (timeout > time_t2) {
                timeout = time_t2;
            }
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Resending message #%d",
                             DEB_F_PREFIX_ARGS(SIP_SUB, fname), scbp->hb.retx_counter);
            if (sipTransportSendMessage(NULL,
                                        sipPlatformUISMSubNotTimers[scb_index].message_buffer,
                                        sipPlatformUISMSubNotTimers[scb_index].message_buffer_len,
                                        sipPlatformUISMSubNotTimers[scb_index].message_type,
                                        &(sipPlatformUISMSubNotTimers[scb_index].ipaddr),
                                        sipPlatformUISMSubNotTimers[scb_index].port,
                                        FALSE, TRUE, timeout, scbp) < 0) {
                return (-1);
            }
        } else {
            // Should we terminate this dialog? At the very least - stop
            // the timer block, display error, and send a message to the app
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Either exceeded max retries for UDP"
                              " or Timer F fired for TCP\n", fname);
            sip_platform_msg_timer_subnot_stop(&sipPlatformUISMSubNotTimers[scb_index]);
            scbp->hb.retx_flag = FALSE;
            scbp->hb.retx_counter = 0;

            memset(&sub_not_result_data, 0, sizeof(sub_not_result_data));
            sub_not_result_data.request_id = scbp->request_id;
            sub_not_result_data.sub_id = scbp->sub_id;
            sub_not_result_data.gsm_id = scbp->gsm_id;
            sub_not_result_data.line_id = scbp->hb.dn_line;

            if ((scbp->last_sent_request_cseq_method == sipMethodSubscribe) ||
                (scbp->last_sent_request_cseq_method == sipMethodRefer)) {
                sub_not_result_data.u.subs_result_data.status_code = REQUEST_TIMEOUT;
                sip_send_error_message(&sub_not_result_data,
                                       scbp->subsNotCallbackTask,
                                       scbp->subsResCallbackMsgID,
                                       scbp->subsResultCallback, fname);
            } else {
                sub_not_result_data.u.notify_result_data.status_code = REQUEST_TIMEOUT;
                // Set the state to ACTIVE, so we can go on appropriately. Note that
                // this assignment below should be placed before sip_send_error_message() so
                // that in case of callback running on same thread freed the scb, we will not
                // set it back to active and try to use it later.
                scbp->smState = SUBS_STATE_ACTIVE;
                sip_send_error_message(&sub_not_result_data,
                                       scbp->subsNotCallbackTask,
                                       scbp->notResCallbackMsgID,
                                       scbp->notifyResultCallback, fname);
            }
            // If there are any pending requests, execute them now
            if (scbp->pendingRequests) {
                handle_pending_requests(scbp);
            }
        }
    }

    return (0);
}

/**************************************************************
 * Handle periodic timer
 *************************************************************/
void
subsmanager_handle_periodic_timer_expire (void)
{
    const char     *fname = "subsmanager_handle_periodic_timer_expire";
    sipSCB_t       *scbp = NULL;
    int             scb_index;
    ccsip_sub_not_data_t subs_term_data;
    sipspi_msg_t    subscribe;
    int             subscription_delta = 0;

    // static char          count = 0;
    /*
     * Go through the list of SCBs and pick out the ones for
     * which some action needs to be taken. This action may include:
     * 1. If an incoming SUBSCRIBE has expired, inform the application
     * 2. If auto-subscribe is set on an outgoing subscribe, send
     *    a re-SUBSCRIBE message
     */
    config_get_value(CFGID_TIMER_SUBSCRIBE_DELTA, &subscription_delta,
                     sizeof(subscription_delta));
    for (scb_index = 0; scb_index < MAX_SCBS; scb_index++) {
        scbp = &(subsManagerSCBS[scb_index]);
        if (scbp->pendingClean) {
            if (scbp->pendingCount > 0) {
                scbp->pendingCount -= TMR_PERIODIC_SUBNOT_INTERVAL;
            } else {
                free_scb(scb_index, fname);
            }
            continue;
        }
        if (scbp->smState == SUBS_STATE_REGISTERED) {
            continue;
        }
        if (scbp->smState != SUBS_STATE_IDLE) {
            if (scbp->hb.expires > 0) {
                scbp->hb.expires -= TMR_PERIODIC_SUBNOT_INTERVAL;
            }
            if (scbp->hb.expires > (subscription_delta + TMR_PERIODIC_SUBNOT_INTERVAL)) {
                continue;
            }

            if (scbp->internal) {
                // Subscription was internally generated
                if (scbp->auto_resubscribe) {
                    if (scbp->smState != SUBS_STATE_SENT_SUBSCRIBE) {
                        // Send re-SUBSCRIBE message - but not if we just sent one
                        CCSIP_DEBUG_TASK(DEB_F_PREFIX"Auto reSubscribing:"
                                         " scb=%d sub_id=%x\n",
                                         DEB_F_PREFIX_ARGS(SIP_SUB, fname), scb_index, scbp->sub_id);
                        memset(&subscribe, 0, sizeof(sipspi_msg_t));
                        subscribe.msg.subscribe.sub_id = scbp->sub_id;
                        subscribe.msg.subscribe.duration = scbp->hb.orig_expiration;
                        (void) subsmanager_handle_ev_app_subscribe(&subscribe);
                    }
                } else {
                    // Let app know its own SUBSCRIBE is about to expire
                    // Application SHOULD renew its own subscription
                    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Notifying App of internal"
                                     " expiry: scb=%d sub_id=%x\n",
                                     DEB_F_PREFIX_ARGS(SIP_SUB, fname), scb_index, scbp->sub_id);
                    subs_term_data.sub_id = scbp->sub_id;
                    subs_term_data.event = scbp->hb.event_type;
                    subs_term_data.msg_id = scbp->subsTermCallbackMsgID;
                    subs_term_data.request_id = scbp->request_id;
                    subs_term_data.reason_code = SM_REASON_CODE_NORMAL;
                    subs_term_data.u.subs_term_data.status_code =
                        APPLICATION_SUBSCRIPTION_EXPIRED;
                    if (scbp->subsTermCallback) {
                        scbp->subsTermCallback(&subs_term_data);
                    } else if (scbp->subsIndCallbackTask != CC_SRC_MIN) {
                        (void) sip_send_message(&subs_term_data, scbp->subsIndCallbackTask,
                                                scbp->subsTermCallbackMsgID);
                    }
                }

            } else {
                // Subscription was received from network
                if (scbp->hb.expires <= 0) {
                    // Let app know this SUBSCRIBE has expired
                    // App SHOULD send final NOTIFY and terminate the session
                    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Notifying App of external"
                                     " expiry: scb=%d sub_id=%x\n",
                                     DEB_F_PREFIX_ARGS(SIP_SUB, fname), scb_index, scbp->sub_id);
                    subs_term_data.sub_id = scbp->sub_id;
                    subs_term_data.event = scbp->hb.event_type;
                    subs_term_data.msg_id = scbp->subsTermCallbackMsgID;
                    subs_term_data.line_id = scbp->hb.dn_line;
                    subs_term_data.gsm_id = scbp->gsm_id;
                    subs_term_data.reason_code = SM_REASON_CODE_NORMAL;
                    subs_term_data.u.subs_term_data.status_code =
                        NETWORK_SUBSCRIPTION_EXPIRED;
                    if (scbp->subsTermCallback) {
                        scbp->subsTermCallback(&subs_term_data);
                    } else if (scbp->subsIndCallbackTask != CC_SRC_MIN) {
                        (void) sip_send_message(&subs_term_data, scbp->subsIndCallbackTask,
                                                scbp->subsTermCallbackMsgID);
                    }
                }
            }
        }
    }
    // Re-start periodic timer
    (void) sip_platform_subnot_periodic_timer_start(TMR_PERIODIC_SUBNOT_INTERVAL * 1000);

    /*
     * Comment out the periodic show since now we have a show-subscription-statistics CLI
     * Do not want to completely remove this code since the CLI is not available in
     * IP-Communicator where this could be enabled for debugging
     */
    /*
     * if (count == 50) {
     * // Show the stats for the Subsmanager
     * CCSIP_DEBUG_TASK("Reg - RecdSubs RecdNots ActiveExtSubs - SentSubs SentNots ActiveIntSubs - CurSCBs MaxSCBs");
     * CCSIP_DEBUG_TASK("-----------------------------------------------------------------------------------");
     * CCSIP_DEBUG_TASK("%d - %d  %d  %d - %d  %d  %d - %d  %d",
     * internalRegistrations, incomingSubscribes,
     * incomingNotifies, incomingSubscriptions,
     * outgoingSubscribes, outgoingNotifies,
     * outgoingSubscriptions, currentScbsAllocated,
     * maxScbsAllocated);
     * count = 0;
     * } else {
     * count ++;
     * }
     */
}

/**************************************************************
 * Format and send a response to a network received NOTIFY
 * Currently this action is handled together with the response
 * to SUBSCRIBE
 **************************************************************/
boolean
sipSPISendSubNotifyResponse (sipSCB_t *scbp, int response_code)
{
    return TRUE;
}

static void
show_scbs_inuse ()
{
    int i;
    sipSCB_t *scbp = NULL;

    if (subsManagerRunning == 0) {
        return;
    }

    debugif_printf("---------SCB DUMP----------\n");
    for (i = 0; i < MAX_SCBS; i++) {
        scbp = &(subsManagerSCBS[i]);
        if (scbp->smState == SUBS_STATE_IDLE) {
            debugif_printf("SCB# %d, State = %d (IDLE)\n", i, scbp->smState);
            continue;
        }
        if (scbp->smState == SUBS_STATE_REGISTERED) {
            debugif_printf("SCB# %d, State = %d (REGISTERED) sub_id=%x\n",
                    i, scbp->smState, scbp->sub_id);
            debugif_printf("SCB# %d, eventPackage=%d\n",
                    i, scbp->hb.event_type);
            continue;
        }
        debugif_printf("SCB# %d, State = %d sub_id=%x\n", i, scbp->smState,
                       scbp->sub_id);
        debugif_printf("SCB# %d, pendingClean=%d, internal=%d, eventPackage=%d, "
                       "norefersub=%d, subscriptionState=%d, expires=%ld", i,
                       scbp->pendingClean, scbp->internal, scbp->hb.event_type,
                       scbp->norefersub, scbp->subscription_state, scbp->hb.expires);
        debugif_printf("-----------------------------\n");
    }
}

cc_int32_t
show_subsmanager_stats (cc_int32_t argc, const char *argv[])
{
    debugif_printf("------ Current Subsmanager Statistics ------\n");
    debugif_printf("Internal Registrations: %d\n", internalRegistrations);
    debugif_printf("Total Incoming Subscribes: %d\n", incomingSubscribes);
    debugif_printf("Total Incoming Notifies: %d\n", incomingNotifies);
    debugif_printf("Total Incoming Unsolicited Notifies: %d\n", incomingUnsolicitedNotifies);
    debugif_printf("Active Incoming Subscriptions: %d\n", incomingSubscriptions);
    debugif_printf("Total Outgoing Subscribes: %d\n", outgoingSubscribes);
    debugif_printf("Total Outgoing Notifies: %d\n", outgoingNotifies);
    debugif_printf("Total Outgoing Unsolicited Notifies: %d\n", outgoingUnsolicitedNotifies);
    debugif_printf("Active Outgoing Subscriptions: %d\n", outgoingSubscriptions);
    debugif_printf("Current SCBs Allocated: %d\n", currentScbsAllocated);
    debugif_printf("Total Maximum SCBs Ever Allocated: %d\n", maxScbsAllocated);
    debugif_printf("------ End of Subsmanager Statistics ------\n");

    // Now print out the status of all SCBs
    show_scbs_inuse();
    return 0;
}

