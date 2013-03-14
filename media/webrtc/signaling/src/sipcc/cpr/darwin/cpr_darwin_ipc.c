/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr.h"
#include "cpr_stdlib.h"
#include <cpr_stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <plat_api.h>
#include "cpr_string.h"

/*
 * If building with debug test interface,
 * allow access to internal CPR functions
 */
#define STATIC static

#define OS_MSGTQL 31 /* need to check number for MV linux and put here */

/*
 * Internal CPR API
 */
extern pthread_t cprGetThreadId(cprThread_t thread);

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
    pthread_t thread;
    int32_t queueId;
    uint16_t currentCount;
    uint32_t totalCount;
    uint32_t sendErrors;
    uint32_t reTries;
    uint32_t highAttempts;
    uint32_t selfQErrors;
    uint16_t extendedQDepth;
    uint16_t maxExtendedQDepth;
    pthread_mutex_t mutex;       /* lock for managing extended queue     */
	pthread_cond_t cond;		 /* signal for queue/dequeue */
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
 * Mutex to manage message queue list
 */
pthread_mutex_t msgQueueListMutex;

/*
 * String to represent message queue name when it is not provided
 */
static const char unnamed_string[] = "unnamed";


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


/*
 * Prototype declarations
 */
static cpr_msgq_post_result_e
cprPostMessage(cpr_msg_queue_t *msgq, void *msg, void **ppUserData);
static void
cprPegSendMessageStats(cpr_msg_queue_t *msgq, uint16_t numAttempts);


/*
 * Functions
 */

/**
 * Creates a message queue
 *
 * @param name  - name of the message queue
 * @param depth - the message queue depth, optional field which will
 *                default if set to zero(0)
 *
 * @return Msg queue handle or NULL if init failed, errno provided
 *
 * @note the actual message queue depth will be bounded by the
 *       standard system message queue depth and CPR_MAX_MSG_Q_DEPTH.
 *       If 'depth' is outside of the bounds, the value will be
 *       reset automatically.
 */
cprMsgQueue_t
cprCreateMessageQueue (const char *name, uint16_t depth)
{
    static const char fname[] = "cprCreateMessageQueue";
    cpr_msg_queue_t *msgq;
    static int key_id = 100; /* arbitrary starting number */

    msgq = cpr_calloc(1, sizeof(cpr_msg_queue_t));
    if (msgq == NULL) {
        printf("%s: Malloc failed: %s\n", fname,
                  name ? name : unnamed_string);
        errno = ENOMEM;
        return NULL;
    }

    msgq->name = name ? name : unnamed_string;
	msgq->queueId = key_id++;

	pthread_cond_t _cond = PTHREAD_COND_INITIALIZER;
	msgq->cond = _cond;
	pthread_mutex_t _lock = PTHREAD_MUTEX_INITIALIZER;
	msgq->mutex = _lock;

    /*
     * Add message queue to list for statistics reporting
     */
    pthread_mutex_lock(&msgQueueListMutex);
    msgq->next = msgQueueList;
    msgQueueList = msgq;
    pthread_mutex_unlock(&msgQueueListMutex);

    return msgq;
}


/**
 * Removes all messages from the queue and then destroy the message queue
 *
 * @param msgQueue - message queue to destroy
 *
 * @return CPR_SUCCESS or CPR_FAILURE, errno provided
 */
cprRC_t
cprDestroyMessageQueue (cprMsgQueue_t msgQueue)
{
    static const char fname[] = "cprDestroyMessageQueue";
    cpr_msg_queue_t *msgq;
    void *msg;

    msgq = (cpr_msg_queue_t *) msgQueue;
    if (msgq == NULL) {
        /* Bad application! */
        CPR_ERROR("%s: Invalid input\n", fname);
        errno = EINVAL;
        return CPR_FAILURE;
    }

    /* Drain message queue */
    msg = cprGetMessage(msgQueue, FALSE, NULL);
    while (msg != NULL) {
        cpr_free(msg);
        msg = cprGetMessage(msgQueue, FALSE, NULL);
    }

    /* Remove message queue from list */
    pthread_mutex_lock(&msgQueueListMutex);
    if (msgq == msgQueueList) {
        msgQueueList = msgq->next;
    } else {
        cpr_msg_queue_t *msgql = msgQueueList;

        while ((msgql->next != NULL) && (msgql->next != msgq)) {
            msgql = msgql->next;
        }
        if (msgql->next == msgq) {
            msgql->next = msgq->next;
        }
    }
    pthread_mutex_unlock(&msgQueueListMutex);

    /* Remove message queue mutex */
    if (pthread_mutex_destroy(&msgq->mutex) != 0) {
        CPR_ERROR("%s: Failed to destroy msg queue (%s) mutex: %d\n",
                  fname, msgq->name, errno);
    }

    cpr_free(msgq);
    return CPR_SUCCESS;
}


/**
 * Associate a thread with the message queue
 *
 * @param msgQueue  - msg queue to set
 * @param thread    - CPR thread to associate with queue
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 *
 * @note Nothing is done to prevent overwriting the thread ID
 *       when the value has already been set.
 */
cprRC_t
cprSetMessageQueueThread (cprMsgQueue_t msgQueue, cprThread_t thread)
{
    static const char fname[] = "cprSetMessageQueueThread";
    cpr_msg_queue_t *msgq;

    if ((!msgQueue) || (!thread)) {
        CPR_ERROR("%s: Invalid input\n", fname);
        return CPR_FAILURE;
    }

    msgq = (cpr_msg_queue_t *) msgQueue;
    if (msgq->thread != 0) {
        CPR_ERROR("%s: over-writing previously msgq thread name for %s",
                  fname, msgq->name);
    }

    msgq->thread = cprGetThreadId(thread);
    return CPR_SUCCESS;
}


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
    static const char fname[] = "cprGetMessage";

    void *buffer = 0;
    cpr_msg_queue_t *msgq;
    cpr_msgq_node_t *node;
	struct timespec timeout;
	struct timeval tv;
	// On the iPhone, there is a DarwinAlias problem with "timezone"
	struct _timezone {
		int     tz_minuteswest; /* of Greenwich */
		int     tz_dsttime;     /* type of dst correction to apply */
	} tz;

    /* Initialize ppUserData */
    if (ppUserData) {
        *ppUserData = NULL;
    }

    msgq = (cpr_msg_queue_t *) msgQueue;
    if (msgq == NULL) {
        /* Bad application! */
        CPR_ERROR("%s: Invalid input\n", fname);
        errno = EINVAL;
        return NULL;
    }

    /*
     * If waitForever is set, block on the message queue
     * until a message is received, else return after
	 * 25msec of waiting
     */
	pthread_mutex_lock(&msgq->mutex);

	if (!waitForever)
	{
		// We'll wait till 25uSec from now
		gettimeofday(&tv, &tz);
		timeout.tv_nsec = (tv.tv_usec * 1000) + 25000;
		timeout.tv_sec = tv.tv_sec;

		pthread_cond_timedwait(&msgq->cond, &msgq->mutex, &timeout);

	}
	else
	{
		while(msgq->tail==NULL)
		{
			pthread_cond_wait(&msgq->cond, &msgq->mutex);
		}
	}

	// If there is a message on the queue, de-queue it
	if (msgq->tail)
	{
		node = msgq->tail;
		msgq->tail = node->prev;
		if (msgq->tail) {
			msgq->tail->next = NULL;
		}
		if (msgq->head == node) {
			msgq->head = NULL;
		}
		msgq->currentCount--;
		/*
		 * Pull out the data
		 */
		if (ppUserData) {
			*ppUserData = node->pUserData;
		}
		buffer = node->msg;

	}

	pthread_mutex_unlock(&msgq->mutex);

    return buffer;
}


/**
 * Place a message on a particular queue.  Note that caller may
 * block (see comments below)
 *
 * @param msgQueue   - msg queue on which to place the message
 * @param msg        - pointer to the msg to place on the queue
 * @param ppUserData - pointer to a pointer to user defined data
 *
 * @return CPR_SUCCESS or CPR_FAILURE, errno provided
 *
 * @note 1. Messages queues are set to be non-blocking, those cases
 *       where the system call fails with a would-block error code
 *       (EAGAIN) the function will attempt other mechanisms described
 *       below.
 * @note 2. If enabled with an extended message queue, either via a
 *       call to cprCreateMessageQueue with depth value or a call to
 *       cprSetExtendMessageQueueDepth() (when unit testing), the message
 *       will be added to the extended message queue and the call will
 *       return successfully.  When room becomes available on the
 *       system's message queue, those messages will be added.
 * @note 3. If the message queue becomes full and no space is availabe
 *       on the extended message queue, then the function will attempt
 *       to resend the message up to CPR_ATTEMPTS_TO_SEND and the
 *       calling thread will *BLOCK* CPR_SND_TIMEOUT_WAIT_INTERVAL
 *       milliseconds after each failed attempt.  If unsuccessful
 *       after all attempts then EGAIN error code is returned.
 * @note 4. This applies to all CPR threads, including the timer thread.
 *       So it is possible that the timer thread would be forced to
 *       sleep which would have the effect of delaying all active
 *       timers.  The work to fix this rare situation is not considered
 *       worth the effort to fix....so just leaving as is.
 */
cprRC_t
cprSendMessage (cprMsgQueue_t msgQueue, void *msg, void **ppUserData)
{
    static const char fname[] = "cprSendMessage";
    static const char error_str[] = "%s: Msg not sent to %s queue: %s\n";
    cpr_msgq_post_result_e rc;
    cpr_msg_queue_t *msgq;
    int16_t attemptsToSend = CPR_ATTEMPTS_TO_SEND;
    uint16_t numAttempts   = 0;

    /* Bad application? */
    if (msgQueue == NULL) {
        CPR_ERROR(error_str, fname, "undefined", "invalid input");
        errno = EINVAL;
        return CPR_FAILURE;
    }

    msgq = (cpr_msg_queue_t *) msgQueue;

    /*
     * Attempt to send message
     */
    do {

		/*
		 * Post the message to the Queue
		 */
		rc = cprPostMessage(msgq, msg, ppUserData);

		if (rc == CPR_MSGQ_POST_SUCCESS) {
			cprPegSendMessageStats(msgq, numAttempts);
			return CPR_SUCCESS;
		} else if (rc == CPR_MSGQ_POST_FAILED) {
			CPR_ERROR("%s: Msg not sent to %s queue: %d\n",
					  fname, msgq->name, errno);
			msgq->sendErrors++;
			/*
			 * If posting to calling thread's own queue,
			 * then peg the self queue error.
			 */
			if (pthread_self() == msgq->thread) {
				msgq->selfQErrors++;
			}

			return CPR_FAILURE;
		}


        /*
         * Did not succeed in sending the message, so continue
         * to attempt up to the CPR_ATTEMPTS_TO_SEND.
         */
        attemptsToSend--;
        if (attemptsToSend > 0) {
            /*
             * Force a context-switch of the thread attempting to
             * send the message, in order to help the case where
             * the msg queue is full and the owning thread may get
             * a a chance be scheduled so it can drain it (Note:
             * no guarantees, more of a "last-ditch effort" to
             * recover...especially when temporarily over-whelmed).
             */
            cprSleep(CPR_SND_TIMEOUT_WAIT_INTERVAL);
            msgq->reTries++;
            numAttempts++;
        }
    } while (attemptsToSend > 0);

    CPR_ERROR(error_str, fname, msgq->name, "FULL");
    msgq->sendErrors++;
    return CPR_FAILURE;
}

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

	pthread_mutex_lock(&msgq->mutex);

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

	pthread_cond_signal(&msgq->cond);
	pthread_mutex_unlock(&msgq->mutex);

	return CPR_MSGQ_POST_SUCCESS;

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
        cpr_msg_queue_t *msgq;
        msgq = (cpr_msg_queue_t *) msgQueue;
        return msgq->currentCount;
}

