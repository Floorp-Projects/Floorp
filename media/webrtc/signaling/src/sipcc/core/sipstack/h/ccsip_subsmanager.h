/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCSIP_SUBSMANAGER_H_
#define _CCSIP_SUBSMANAGER_H_

#include "cpr_types.h"
#include "cpr_ipc.h"
#include "cpr_timers.h"
#include "cpr_socket.h"
#include "ccsip_core.h"
#include "phone_platform_constants.h"
#include "singly_link_list.h"
#include "ccsip_common_cb.h"

/*
 * States
 */
typedef enum {
    SUBS_STATE_IDLE = 0,        // Initial state of the SCB
    SUBS_STATE_REGISTERED,

    /* out going subscribe SCB states */
    SUBS_STATE_SENT_SUBSCRIBE,  // Sent SUBSCRIBE
    SUBS_STATE_RCVD_NOTIFY,     // Received NOTIFY
    SUBS_STATE_SENT_SUBSCRIBE_RCVD_NOTIFY,  // Sent SUBSCRIBE & Received NOTIFY

    /* incoming subscribe SCB states */
    SUBS_STATE_RCVD_SUBSCRIBE,  // Received SUBSCRIBE
    SUBS_STATE_SENT_NOTIFY,     // Sent NOTIFY
    SUBS_STATE_RCVD_SUBSCRIBE_SENT_NOTIFY, // Received SUBSCRIBE & Sent NOTIFY

    /*
     * No outstanding outgoing/incoming messages to handle.
     * This state is after all outstanding SUBSCRIBE/NOTIFY
     * transactions are complete.
     */
    SUBS_STATE_ACTIVE,

    SUBS_STATE_INVALID
} subsStateType_t;

typedef enum {
    SUBSCRIPTION_NULL = 0,
    SUBSCRIPTION_TERMINATE
} subscriptionState;

/*
 * Subscription ID data type
 */
typedef uint32_t sub_id_t;

#define MAX_EVENT_NAME_LEN           32
#define CCSIP_SUBS_START_CSEQ        1000

/* There may be multiple subscription pending on given call
 * the subcription for KPML, remote-cc, offhook notification  etc.
 */
#define MAX_SCBS                     ((MAX_TEL_LINES * 2) < 32 ? 32 : (MAX_TEL_LINES * 2))
#define LIMIT_SCBS_USAGE             (MAX_SCBS - ((MAX_SCBS * 20) / 100))
#define TMR_PERIODIC_SUBNOT_INTERVAL 5
#define CCSIP_SUBS_INVALID_SUB_ID   (sub_id_t)(-1)
#define MAX_SCB_HISTORY              10

typedef enum {
    SM_REASON_CODE_NORMAL = 0,
    SM_REASON_CODE_ERROR,
    SM_REASON_CODE_SHUTDOWN,
    SM_REASON_CODE_ROLLOVER,
    SM_REASON_CODE_RESET_REG
} ccsip_reason_code_e;


// Application defined callback functions

// Data to app to indicate response to a sent SUBSCRIBE
typedef struct {
    int  status_code;
    long expires;
} ccsip_subs_result_data_t;

// Data to app to indicate a received SUBSCRIBE
typedef struct {
    ccsip_event_data_t *eventData;
    int               expires;
    int               line;
    string_t          from;
    string_t          to;
} ccsip_subs_ind_data_t;

// Data to app to indicate a received NOTIFY
typedef struct {
    ccsip_event_data_t     *eventData;
    sip_subs_state_e        subscription_state; // From the subs-state header
    sip_subs_state_reason_e subscription_state_reason;
    uint32_t                expires;
    uint32_t                retry_after;
    uint32_t                cseq;
    char                    entity[CC_MAX_DIALSTRING_LEN]; // used to store From user for incoming unsolicited NOTIFY.
} ccsip_notify_ind_data_t;

// Data to app to indicate response to a sent NOTIFY
typedef struct {
    int status_code;
} ccsip_notify_result_data_t;

// Data to app to indicate incoming subscription terminated
typedef struct {
    int status_code;
} ccsip_subs_terminate_data_t;

// Containing structure for all stack->app messages
typedef struct ccsip_sub_not_data_t {
    int                 msg_id;
    sub_id_t            sub_id;
    int                 sub_duration;
    cc_subscriptions_t  event;
    line_t              line_id;
    callid_t            gsm_id;
    boolean             norefersub;
    long		request_id;
    ccsip_reason_code_e reason_code;
    union {
        ccsip_subs_ind_data_t       subs_ind_data;
        ccsip_subs_result_data_t    subs_result_data;
        ccsip_notify_ind_data_t     notify_ind_data;
        ccsip_notify_result_data_t  notify_result_data;
        ccsip_subs_terminate_data_t subs_term_data;
    } u;
} ccsip_sub_not_data_t;


// Function to pass an incoming subscribe request
typedef void (*ccsipSubsIndCallbackFn_t)(ccsip_sub_not_data_t *msg_data);

// Function to pass response to a subscribe request
typedef void (*ccsipSubsResultCallbackFn_t)(ccsip_sub_not_data_t *msg_data);

// Function to pass an incoming Notify request
typedef void (*ccsipNotifyIndCallbackFn_t)(ccsip_sub_not_data_t *msg_data);

// Function to pass results of notify request
typedef void (*ccsipNotifyResultCallbackFn_t)(ccsip_sub_not_data_t *msg_data);

// Function to pass general errors
typedef void (*ccsipSubsTerminateCallbackFn_t)(ccsip_sub_not_data_t *msg_data);
typedef void (*ccsipGenericCallbackFn_t)(ccsip_sub_not_data_t *msg_data);



// Return status code
#define SUBSCRIPTION_IN_PROGRESS     1010
#define SUBSCRIPTION_SUCCEEDED       1020
#define SUBSCRIPTION_REJECTED        1030
#define SUBSCRIPTION_FAILED          1040
#define NOTIFY_REQUEST_FAILED        1050
#define SUBSCRIBE_REQUEST_FAILED     1060
#define SUBSCRIBE_FAILED_NORESOURCE  1061
#define SUBSCRIBE_FAILED_BADEVENT    1062
#define SUBSCRIBE_FAILED_BADINFO     1062
#define NETWORK_SUBSCRIPTION_EXPIRED 1070
#define APPLICATION_SUBSCRIPTION_EXPIRED 1080
#define REQUEST_TIMEOUT              1090

// Data passed by app to register for incoming SUBSCRIBE
typedef struct sipspi_subscribe_reg_t_ {
    cc_subscriptions_t eventPackage; // Registering for which event
    ccsipSubsIndCallbackFn_t subsIndCallback; // Callback function
    cc_srcs_t subsIndCallbackTask;   // Callback task
    int       subsIndCallbackMsgID;  // msg_id to use in callback
    ccsipSubsTerminateCallbackFn_t subsTermCallback; // Callback function when remote side terminates subs
    int       subsTermCallbackMsgID; // Terminate msg-id
    long      min_duration; // Min duration for which SUB should be accepted
    long      max_duration; // Max duration for which SUB should be accepted
} sipspi_subscribe_reg_t;

// Data passed by app to initiate a SUBSCRIBE
typedef struct sipspi_subscribe_t_ {
    sub_id_t           sub_id;       // ID for reSubscribe
    cc_subscriptions_t eventPackage; // Event package to send subscribe for
    cc_subscriptions_t acceptPackage; // Accept header
    long               duration;     // subscription duration (0=unsubscribe)
    char               subscribe_uri[CC_MAX_DIALSTRING_LEN];
    char               subscriber_uri[CC_MAX_DIALSTRING_LEN]; // From field
    long		request_id;   // Returned to subscriber in response

    ccsipSubsResultCallbackFn_t subsResultCallback;
    ccsipNotifyIndCallbackFn_t notifyIndCallback;
    ccsipSubsTerminateCallbackFn_t subsTermCallback;

    cc_srcs_t          subsNotCallbackTask;
    int                subsResCallbackMsgID;
    int                subsNotIndCallbackMsgID;
    int                subsTermCallbackMsgID;

    cpr_ip_addr_t      dest_sip_addr; // Destination address
    uint16_t           dest_sip_port; // Destination port

    callid_t           call_id;
    line_t             dn_line;       // Associated line, if any

    boolean            auto_resubscribe; // stack reSubscribes at expiry
    boolean            norefersub;
    ccsip_event_data_t *eventData;    // Determined by the eventPackage value

} sipspi_subscribe_t;

// Data by app to respond to a received SUBSCRIBE
typedef struct sipspi_subscribe_resp_t_ {
    sub_id_t sub_id;         // Subscription id
    uint16_t response_code;  // Response that should be sent
    int      duration;       // Max duration for the subscribe
} sipspi_subscribe_resp_t;

// Data by app to initiate a NOTIFY
typedef struct sipspi_notify_t_ {
    sub_id_t            sub_id;     // Subscription id
    // Info for different notify bodies
    ccsipNotifyResultCallbackFn_t notifyResultCallback;
    int                 subsNotResCallbackMsgID;
    ccsip_event_data_t *eventData;  // Determined by the eventPackage value
    cc_subscriptions_t eventPackage; // Event package
    subscriptionState   subState;
    cc_srcs_t           subsNotCallbackTask; // Opt.: If not already specified
} sipspi_notify_t;

// Data by app to respond to a received NOTIFY
typedef struct sipspi_notify_resp_t_ {
    sub_id_t sub_id;
    int      response_code;
    int      duration;
    uint32_t cseq;
} sipspi_notify_resp_t;

// Data by app to terminate an existing subscription
typedef struct sipspi_subscribe_term_t_ {
    sub_id_t sub_id;
    long     request_id;
    cc_subscriptions_t eventPackage; // Event package
    boolean immediate;
} sipspi_subscribe_term_t;

/*
typedef struct sipspi_remotecc_reg_t_{
} sipspi_remotecc_reg_t;

typedef struct sipspi_remotecc_refer_t_{
} sipspi_remotecc_refer_t;

typedef struct sipspi_remotecc_refer_resp_t_{
} sipspi_remotecc_refer_resp_t;

typedef struct sipspi_remotecc_notify_t_{
} sipspi_remotecc_notify_t;

typedef struct sipspi_remotecc_notify_resp_t_{
} sipspi_remotecc_notify_resp_t;

typedef struct sipspi_remotecc_term_t_{
} sipspi_remotecc_term_t;
*/

typedef struct sipspi_msg_t_ {
    union {
        sipspi_subscribe_reg_t  subs_reg;
        sipspi_subscribe_t      subscribe;
        sipspi_subscribe_resp_t subscribe_resp;
        sipspi_notify_t         notify;
        sipspi_notify_resp_t    notify_resp;
        sipspi_subscribe_term_t subs_term;
        /*
         * sipspi_remotecc_reg_t         remotecc_reg;
         * sipspi_remotecc_refer_t       remotecc_refer;
         * sipspi_remotecc_refer_resp_t  remotecc_refer_resp;
         * sipspi_remotecc_notify_t      remotecc_notify;
         * sipspi_remotecc_notify_resp_t remotecc_notify_resp;
         * sipspi_remotecc_term_t        remotecc_term;
         */
    } msg;
} sipspi_msg_t;

// List of app->stack messages
typedef struct sipspi_msg_list_t_ {
    uint32_t cmd;
    sipspi_msg_t *msg;
    struct sipspi_msg_list_t_ *next;
} sipspi_msg_list_t;

// Subscription Control Block
typedef struct {
    ccsip_common_cb_t       hb; /* this MUST be the first memeber in the struct */

    line_t             line;
    // Subscription ID
    sub_id_t           sub_id;

    // SCB State
    boolean            pendingClean;
    unsigned char      pendingCount;

    // Subscriber details
    boolean            internal; // Internal or external

    // Callback and messaging details
    ccsipSubsIndCallbackFn_t subsIndCallback;
    cc_srcs_t          subsIndCallbackTask;
    cc_srcs_t          subsNotCallbackTask;
    int                subsIndCallbackMsgID;
    ccsipSubsResultCallbackFn_t subsResultCallback;
    int                subsResCallbackMsgID;
    ccsipNotifyIndCallbackFn_t notifyIndCallback;
    int                notIndCallbackMsgID;
    ccsipSubsTerminateCallbackFn_t subsTermCallback;
    int                subsTermCallbackMsgID;
    ccsipNotifyResultCallbackFn_t notifyResultCallback;
    int                notResCallbackMsgID;

    short              sip_socket_handle;
    boolean            useDeviceAddressing;
    callid_t           gsm_id;
    long		request_id;

    // Subscription details
    subsStateType_t    smState;
    subsStateType_t    outstandingIncomingNotifyTrxns; // only used for incoming NOTIFYs
    unsigned long      min_expires;
    unsigned long      max_expires;
    ccsipCCB_t        *ccbp;             /* associated CCB, if any */
    char               event_name[MAX_EVENT_NAME_LEN];
    boolean            auto_resubscribe; /* Resubscribe automatically */
    boolean            norefersub;

    // Messaging details
    uint32_t           last_sent_request_cseq;
    sipMethod_t        last_sent_request_cseq_method;
    uint32_t           last_recv_request_cseq;
    sipMethod_t        last_recv_request_cseq_method;

    // Saved headers
    char               SubURI[MAX_SIP_URL_LENGTH];
    char               SubURIOriginal[MAX_SIP_URL_LENGTH];
    char               SubscriberURI[MAX_SIP_URL_LENGTH];
    string_t           sip_from;
    string_t           sip_to;
    string_t           sip_to_tag;
    string_t           sip_from_tag;
    string_t           sip_contact;
    string_t           cached_record_route;
    sipContact_t      *contact_info;
    sipRecordRoute_t  *record_route_info;
    sll_handle_t       incoming_trxns; // to store via (branch attribute) to track transactions.
    string_t           callingNumber;

    // Subscription headers
    sip_subs_state_e  subscription_state;
    sip_subs_state_reason_e subscription_state_reason;
    uint32_t          retry_after;


    // Linked list of pending app messages
    sipspi_msg_list_t *pendingRequests;
} sipSCB_t;

// Transaction Control Block
typedef struct {
    ccsip_common_cb_t       hb; /* this MUST be the first memeber in the struct */
    char                    full_ruri[MAX_SIP_URL_LENGTH];
    cprTimer_t              timer; /* transaction timer */
    uint32_t                trxn_id;
} sipTCB_t;

// Structure to keep track of recent subscriptions
typedef struct {
    char               last_call_id[MAX_SIP_CALL_ID];
    char               last_from_tag[MAX_SIP_TAG_LENGTH];
    cc_subscriptions_t eventPackage;
} sipSubsHistory_t;

#define MAX_SUB_EVENTS    5
#define MAX_SUB_EVENT_NAME_LEN 16
extern const char eventNames[MAX_SUB_EVENTS][MAX_SUB_EVENT_NAME_LEN];

/*
 * Externally called function headers
 */
// For initializing and shutting down
int sip_subsManager_init();
int sip_subsManager_shut();

// Function to handle subscription requests from applications
int subsmanager_handle_ev_cc_feature_subscribe(sipSMEvent_t *);
int subsmanager_handle_ev_cc_feature_notify(sipSMEvent_t *);

int subsmanager_handle_ev_app_subscribe_register(cprBuffer_t buf);
int subsmanager_handle_ev_app_subscribe(cprBuffer_t buf);
int subsmanager_handle_ev_app_subscribe_response(cprBuffer_t buf);
int subsmanager_handle_ev_app_notify(cprBuffer_t buf);
void subsmanager_handle_ev_app_unsolicited_notify(cprBuffer_t buf, line_t line);
int subsmanager_handle_ev_app_notify_response(cprBuffer_t buf);
int subsmanager_handle_ev_app_subscription_terminated(cprBuffer_t buf);
int subsmanager_handle_retry_timer_expire(int scb_index);
void subsmanager_handle_periodic_timer_expire(void);
int subsmanager_test_start_routine();

// Functions to handle remotecc requests from applications
// int subsmanager_handle_ev_app_remotecc_register();
// int subsmanager_handle_ev_app_remotecc_refer();
// int subsmanager_handle_ev_app_remotecc_refer_response();
// int subsmanager_handle_ev_app_remotecc_notify();
// int subsmanager_handle_ev_app_remotecc_notify_response();
// int subsmanager_handle_ev_app_remotecc_terminated();

// Functions to handle requests from network
int subsmanager_handle_ev_sip_subscribe(sipMessage_t *pSipMessage,
                                        sipMethod_t sipMethod,
                                        boolean in_dialog);
int subsmanager_handle_ev_sip_subscribe_notify(sipMessage_t *pSipMessage);

// Function to handle response from network
int subsmanager_handle_ev_sip_response(sipMessage_t *pSipMessage);

void free_event_data(ccsip_event_data_t *event_data);
int sip_subsManager_rollover(void);
int sip_subsManager_reset_reg(void);
void submanager_update_ccb_addr(ccsipCCB_t *ccb);
boolean add_content(ccsip_event_data_t *eventData, sipMessage_t *request, const char *fname);
void pres_unsolicited_notify_ind(ccsip_sub_not_data_t * msg_data);
sipTCB_t *find_tcb_by_sip_callid(const char *callID_p);
int subsmanager_handle_ev_sip_unsolicited_notify_response(sipMessage_t *pSipMessage, sipTCB_t *tcbp);
void subsmanager_unsolicited_notify_timeout(void *data);

#endif
