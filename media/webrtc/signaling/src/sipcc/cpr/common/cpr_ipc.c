/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr.h"
#include "cpr_stdlib.h"
#include "cpr_stdio.h"
#include "plat_api.h"
#include "cpr_string.h"

#include <errno.h>

#define OS_MSGTQL 31 /* need to check number for MV linux and put here */

/*
 * Extended internal message queue node
 *
 * A double-linked list holding the nessasary message information
 */
typedef struct cpr_msgq_node_s
{
    struct cpr_msgq_node_s *next;
    struct cpr_msgq_node_s *prev;
    void *msg;
    void *pUserData;
} cpr_msgq_node_t;

/*
 * Msg queue information needed to hide OS differences in implementation.
 * To use msg queues, the application code may pass in a name to the
 * create function for msg queues. CPR does not use this field, it is
 * solely for the convenience of the application and to aid in debugging.
 *
 * Note: Statistics are not protected by a mutex; therefore, there exists
 * the possibility that the results may not be accurate.
 *
 * Note:if the depth supplied by OS is insufficient,a message queue owner may
 * increase the message queue depth via cprCreateMessageQueue's depth
 * parameter where the value can range from MSGTQL to CPR_MAX_MSG_Q_DEPTH.
 */
typedef struct cpr_msg_queue_s
{
    struct cpr_msg_queue_s *next;
    const char *name;
    int32_t queueId;
    uint16_t currentCount;
    uint32_t totalCount;
    uint32_t sendErrors;
    uint32_t reTries;
    uint32_t highAttempts;
    uint32_t selfQErrors;
    uint16_t extendedQDepth;
    uint16_t maxExtendedQDepth;
    cpr_msgq_node_t *head;       /* extended queue head (newest element) */
    cpr_msgq_node_t *tail;       /* extended queue tail (oldest element) */
} cpr_msg_queue_t;

/*
 * A enumeration used to report the result of posting a message to
 * a message queue
 */
typedef enum
{
    CPR_MSGQ_POST_SUCCESS,
    CPR_MSGQ_POST_FAILED,
    CPR_MSGQ_POST_PENDING
} cpr_msgq_post_result_e;


/*
 * Head of list of message queues
 */
static cpr_msg_queue_t *msgQueueList = NULL;


/*
 * CPR_MAX_MSG_Q_DEPTH
 *
 * The maximum queue depth supported by the CPR layer.  This value
 * is arbitrary though the purpose is to limit the memory usage
 * by CPR and avoid (nearly) unbounded situations.
 *
 * Note: This value should be greater than MSGTQL which is currently
 *       defined as 31
 */
#define CPR_MAX_MSG_Q_DEPTH 256

/*
 * CPR_SND_TIMEOUT_WAIT_INTERVAL
 *
 * The interval of time to wait in milliseconds between attempts to
 * send a message to the message queue
 *
 * Note: 20 ms. to avoid less than a tick wake up since on most
 *       OSes 10ms is one 1 tick
 *       this should really be OS_TICK_MS * 2 or OS_TICK_MS + X
 */
#define CPR_SND_TIMEOUT_WAIT_INTERVAL 20

/*
 * CPR_ATTEMPTS_TO_SEND
 *
 * The number of attempts made to send a message when the message
 * would otherwise be blocked.  Note in this condition the thread
 * will sleep the timeout interval to allow the msg queue to be
 * drained.
 *
 * Note: 25 attempts for upto .5 seconds at the interval of
 *       CPR_SND_TIMEOUT_WAIT_INTERVAL worst case.
 */
#define CPR_ATTEMPTS_TO_SEND 25

/*
 * Also, important to note that the total timeout interval must be
 * greater than the SIP's select call timeout value which is 25msec.
 * This is necessary to cover the case where the SIP message queue
 * is full and the select timeout occurs.
 *
 * Total timeout interval = CPR_SND_TIMEOUT_WAIT_INTERVAL *
 *                          CPR_ATTEMPTS_TO_SEND;
 */


/**
 * Peg the statistics for successfully posting a message
 *
 * @param msgq        - message queue
 * @param numAttempts - number of attempts to post message to message queue
 *
 * @return none
 *
 * @pre (msgq not_eq NULL)
 */
static void
cprPegSendMessageStats (cpr_msg_queue_t *msgq, uint16_t numAttempts)
{
    /*
     * Collect statistics
     */
    msgq->totalCount++;

    if (numAttempts > msgq->highAttempts) {
        msgq->highAttempts = numAttempts;
    }
}

/**
 * Post message to system message queue
 *
 * @param msgq       - message queue
 * @param msg        - message to post
 * @param ppUserData - ptr to ptr to option user data
 *
 * @return the post result which is CPR_MSGQ_POST_SUCCESS,
 *         CPR_MSGQ_POST_FAILURE or CPR_MSGQ_POST_PENDING
 *
 * @pre (msgq not_eq NULL)
 * @pre (msg not_eq NULL)
 */
static cpr_msgq_post_result_e
cprPostMessage (cpr_msg_queue_t *msgq, void *msg, void **ppUserData)
{
    cpr_msgq_node_t *node;

    /*
     * Allocate new message queue node
     */
    node = cpr_malloc(sizeof(*node));
    if (!node) {
        errno = ENOMEM;
        return CPR_MSGQ_POST_FAILED;
    }

    /*
     * Fill in data
     */
    node->msg = msg;
    if (ppUserData != NULL) {
        node->pUserData = *ppUserData;
    } else {
        node->pUserData = NULL;
    }

    /*
     * Push onto list
     */
    node->prev = NULL;
    node->next = msgq->head;
    msgq->head = node;

    if (node->next) {
        node->next->prev = node;
    }

    if (msgq->tail == NULL) {
        msgq->tail = node;
    }
    msgq->currentCount++;

    return CPR_MSGQ_POST_SUCCESS;

}

/*
 * Functions
 */

/**
 * Retrieve a message from a particular message queue
 *
 * @param[in]  msgQueue    - msg queue from which to retrieve the message
 * @param[in]  waitForever - boolean to either wait forever (TRUE) or not
 *                           wait at all (FALSE) if the msg queue is empty.
 * @param[out] ppUserData  - pointer to a pointer to user defined data
 *
 * @return Retrieved message buffer or NULL if failure occurred or
 *         the waitForever flag was set to false and no messages were
 *         on the queue.
 *
 * @note   If ppUserData is defined, the value will be initialized to NULL
 */
void *
cprGetMessage (cprMsgQueue_t msgQueue, boolean waitForever, void **ppUserData)
{
    return NULL;
}

/**
 * cprGetDepth
 *
 * @brief get depth of a message queue
 *
 * The pSIPCC uses this API to look at the depth of a message queue for internal
 * routing and throttling decision
 *
 * @param[in] msgQueue - message queue
 *
 * @return depth of msgQueue
 *
 * @pre (msgQueue not_eq NULL)
 */
uint16_t cprGetDepth (cprMsgQueue_t msgQueue)
{
    return 0;
}

