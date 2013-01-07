/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  @brief CPR layer for interprocess communication
 *
 * The name of this file may be overly broad, rather this file deals
 * with IPC via message queues.  A user may create, destroy and
 * associate a thread with a message queue.  Once established, messages
 * can be delivered and retrieved.
 *
 * The send/get APIs attempt to reliably deliver messages even when
 * under stress.  Two mechanisms have been added to deal with a full
 * message queue.  First, the message queue size may be extended to
 * allow more messages to be handled than supported by an OS.
 * Second, if the queue is indeed full a sleep-and-retry
 * method is used to force a context-switch to allow for other threads
 * to run in hope of clearing some messages off of the queue.  The
 * latter method is always-on by default.  The former method must be
 * enabled by extending the message queue by some size greater than
 * zero (0).
 *
 * @defgroup IPC The Inter Process Communication module
 * @ingroup CPR
 * @brief The module related to IPC abstraction for the pSIPCC
 * @addtogroup MsgQIPCAPIs The Message Queue IPC APIs
 * @ingroup IPC
 * @brief APIs expected by pSIPCC for using message queues
 *
 * @{
 *
 *
 */
#include "cpr.h"
#include "cpr_stdlib.h"
#include <cpr_stdio.h>
#include <errno.h>
#if defined(WEBRTC_GONK)
#include <sys/syscall.h>
#include <unistd.h>
#include <linux/msg.h>
#include <linux/ipc.h>
#else
#include <sys/msg.h>
#include <sys/ipc.h>
#endif
#include "plat_api.h"
#include "CSFLog.h"

static const char *logTag = "cpr_linux_ipc";

#define STATIC static

#if defined(WEBRTC_GONK)
int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
{
  return syscall(__NR_msgsnd, msqid, msgp, msgsz, msgflg);
}

ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg)
{
  return syscall(__NR_msgrcv, msqid, msgp, msgsz, msgtyp, msgflg);
}

int msgctl(int msqid, int cmd, struct msqid_ds *buf)
{
  return syscall(__NR_msgctl, msqid, cmd, buf);
}

int msgget(key_t key, int msgflg)
{
  return syscall(__NR_msgget, key, msgflg);
}
#endif

/* @def The Message Queue depth */
#define OS_MSGTQL 31

/*
 * Internal CPR API
 */
extern pthread_t cprGetThreadId(cprThread_t thread);

/**
 * @struct cpr_msgq_node_s
 * Extended internal message queue node
 *
 * A double-linked list holding the necessary message information
 */
typedef struct cpr_msgq_node_s
{
    struct cpr_msgq_node_s *next;
    struct cpr_msgq_node_s *prev;
    void *msg;
    void *pUserData;
} cpr_msgq_node_t;

/**
 * @struct cpr_msg_queue_s
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
    uint16_t maxCount;
    uint16_t currentCount;
    uint32_t totalCount;
    uint32_t sendErrors;
    uint32_t reTries;
    uint32_t highAttempts;
    uint32_t selfQErrors;
    uint16_t extendedQDepth;
    uint16_t maxExtendedQDepth;
    pthread_mutex_t mutex;       /* lock for managing extended queue     */
    cpr_msgq_node_t *head;       /* extended queue head (newest element) */
    cpr_msgq_node_t *tail;       /* extended queue tail (oldest element) */
} cpr_msg_queue_t;

/**
 * @enum cpr_msgq_post_result_e
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
static cpr_msgq_post_result_e
cprPostExtendedQMsg(cpr_msg_queue_t *msgq, void *msg, void **ppUserData);
static void
cprMoveMsgToQueue(cpr_msg_queue_t *msgq);

/*
 * Functions
 */

/**
 * Creates a message queue
 *
 * @brief The cprCreateMessageQueue function is called to allow the OS to
 * perform whatever work is needed to create a message queue.

 * If the name is present, CPR should assign this name to the message queue to assist in
 * debugging. The message queue depth is the second input parameter and is for
 * setting the desired queue depth. This parameter may not be supported by all OS.
 * Its primary intention is to set queue depth beyond the default queue depth
 * limitation.
 * On any OS where there is no limit on the message queue depth or
 * its queue depth is sufficiently large then this parameter is ignored on that
 * OS.
 *
 * @param[in] name  - name of the message queue (optional)
 * @param[in] depth - the message queue depth, optional field which should
 *                default if set to zero(0)
 *
 * @return Msg queue handle or NULL if init failed, errno should be provided
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
    key_t key;
    static int key_id = 100; /* arbitrary starting number */
    struct msqid_ds buf;

    msgq =(cpr_msg_queue_t *)cpr_calloc(1, sizeof(cpr_msg_queue_t));
    if (msgq == NULL) {
        CPR_ERROR("%s: Malloc failed: %s\n", fname,
                  name ? name : unnamed_string);
        errno = ENOMEM;
        return NULL;
    }

    msgq->name = name ? name : unnamed_string;

    /*
     * Find a unique key
     */
    key = ftok("/proc/self", key_id++);
    CSFLogDebug(logTag, "key = %x\n", key);

    if (key == -1) {
        CPR_ERROR("%s: Key generation failed: %d\n", fname, errno);
        cpr_free(msgq);
        return NULL;
    }

    /*
     * Set creation flag so that OS will create the message queue
     */
    msgq->queueId = msgget(key, (IPC_EXCL | IPC_CREAT | 0666));
    if (msgq->queueId == -1) {
        if (errno == EEXIST) {
            CSFLogDebug(logTag, "Q exists so first remove it and then create again\n");
                /* Remove message queue */
            msgq->queueId = msgget(key, (IPC_CREAT | 0666));
            if (msgctl(msgq->queueId, IPC_RMID, &buf) == -1) {

                CPR_ERROR("%s: Destruction failed: %s: %d\n", fname,
                          msgq->name, errno);

                return NULL;
            }
            msgq->queueId = msgget(key, (IPC_CREAT | 0666));
        }
    } else {
        CSFLogDebug(logTag, "there was no preexisting q..\n");

    }



    if (msgq->queueId == -1) {
        CPR_ERROR("%s: Creation failed: %s: %d\n", fname, name, errno);
        if (errno == EEXIST) {

        }

        cpr_free(msgq);
        return NULL;
    }
    CSFLogDebug(logTag, "create message q with id=%x\n", msgq->queueId);

    /* flush the q before ?? */

    /*
     * Create mutex for extended (overflow) queue
     */
    if (pthread_mutex_init(&msgq->mutex, NULL) != 0) {
        CPR_ERROR("%s: Failed to create msg queue (%s) mutex: %d\n",
                  fname, name, errno);
        (void) msgctl(msgq->queueId, IPC_RMID, &buf);
        cpr_free(msgq);
        return NULL;
    }

    /*
     * Set the extended message queue depth (within bounds)
     */
    if (depth > CPR_MAX_MSG_Q_DEPTH) {
        CPR_INFO("%s: Depth too large (%d) reset to %d\n", fname, depth,
                 CPR_MAX_MSG_Q_DEPTH);
        depth = CPR_MAX_MSG_Q_DEPTH;
    }

    if (depth < OS_MSGTQL) {
        if (depth) {
            CPR_INFO("%s: Depth too small (%d) reset to %d\n", fname, depth, OS_MSGTQL);
        }
        depth = OS_MSGTQL;
    }
    msgq->maxExtendedQDepth = depth - OS_MSGTQL;

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
  * cprDestroyMessageQueue
 * @brief Removes all messages from the queue and then destroy the message queue
 *
 * The cprDestroyMessageQueue function is called to destroy a message queue. The
 * function drains any messages from the queue and the frees the
 * message queue. Any messages on the queue are to be deleted, and not sent to the intended
 * recipient. It is the application's responsibility to ensure that no threads are
 * blocked on a message queue when it is destroyed.
 *
 * @param[in] msgQueue - message queue to destroy
 *
 * @return CPR_SUCCESS or CPR_FAILURE, errno should be provided in this case
 */
cprRC_t
cprDestroyMessageQueue (cprMsgQueue_t msgQueue)
{
    static const char fname[] = "cprDestroyMessageQueue";
    cpr_msg_queue_t *msgq;
    void *msg;
    struct msqid_ds buf;
    CSFLogDebug(logTag, "Destroy message Q called..\n");


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

    /* Remove message queue */
    if (msgctl(msgq->queueId, IPC_RMID, &buf) == -1) {
        CPR_ERROR("%s: Destruction failed: %s: %d\n", fname,
                  msgq->name, errno);
        return CPR_FAILURE;
    }

    /* Remove message queue mutex */
    if (pthread_mutex_destroy(&msgq->mutex) != 0) {
        CPR_ERROR("%s: Failed to destroy msg queue (%s) mutex: %d\n",
                  fname, msgq->name, errno);
    }

    cpr_free(msgq);
    return CPR_SUCCESS;
}


/**
  * cprSetMessageQueueThread
 * @brief Associate a thread with the message queue
 *
 * This method is used by pSIPCC to associate a thread and a message queue.
 * @param[in] msgQueue  - msg queue to set
 * @param[in] thread    - CPR thread to associate with queue
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
  * cprGetMessage
 * @brief Retrieve a message from a particular message queue
 *
 * The cprGetMessage function retrieves the first message from the message queue
 * specified and returns a void pointer to that message.
 *
 * @param[in]  msgQueue    - msg queue from which to retrieve the message. This
 * is the handle returned from cprCreateMessageQueue.
 * @param[in]  waitForever - boolean to either wait forever (TRUE) or not
 *                           wait at all (FALSE) if the msg queue is empty.
 * @param[out] ppUserData  - pointer to a pointer to user defined data. This
 * will be NULL if no user data was present.
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
    struct msgbuffer rcvBuffer = { 0 };
    struct msgbuffer *rcvMsg = &rcvBuffer;
    void *buffer;
    int msgrcvflags;
    cpr_msg_queue_t *msgq;

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
     * until a message is received.
     */
    if (waitForever) {
        msgrcvflags = 0;
    } else {
        msgrcvflags = IPC_NOWAIT;
    }

    if (msgrcv(msgq->queueId, rcvMsg,
        sizeof(struct msgbuffer) - offsetof(struct msgbuffer, msgPtr),
        0, msgrcvflags) == -1) {
    	if (!waitForever && errno == ENOMSG) {
    		CPR_INFO("%s: no message on queue %s (non-blocking receive "
                         " operation), returning\n", fname, msgq->name);
    	} else {
    		CPR_ERROR("%s: msgrcv for queue %s failed: %d\n",
                              fname, msgq->name, errno);
        }
        return NULL;
    }
    CPR_INFO("%s: msgrcv success for queue %s \n",fname, msgq->name);

    (void) pthread_mutex_lock(&msgq->mutex);
    /* Update statistics */
    msgq->currentCount--;
    (void) pthread_mutex_unlock(&msgq->mutex);

    /*
     * Pull out the data
     */
    if (ppUserData) {
        *ppUserData = rcvMsg->usrPtr;
    }
    buffer = rcvMsg->msgPtr;

    /*
     * If there are messages on the extended queue, attempt to
     * push a message back onto the real system queue
     */
    if (msgq->extendedQDepth) {
        cprMoveMsgToQueue(msgq);
    }

    return buffer;
}


/**
  * cprSendMessage
 * @brief Place a message on a particular queue.  Note that caller may
 * block (see comments below)
 *
 * @param[in] msgQueue   - msg queue on which to place the message
 * @param[in] msg        - pointer to the msg to place on the queue
 * @param[in] ppUserData - pointer to a pointer to user defined data
 *
 * @return CPR_SUCCESS or CPR_FAILURE, errno should be provided
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
        (void) pthread_mutex_lock(&msgq->mutex);

        /*
         * If in a queue overflow condition, post message to the
         * extended queue; otherwise, post to normal message queue
         */
        if (msgq->extendedQDepth) {
            /*
             * Check if extended queue is full, if not then
             * attempt to add the message.
             */
            if (msgq->extendedQDepth < msgq->maxExtendedQDepth) {
                rc = cprPostExtendedQMsg(msgq, msg, ppUserData);
                // do under lock to avoid races
                if (rc == CPR_MSGQ_POST_SUCCESS) {
                    cprPegSendMessageStats(msgq, numAttempts);
                } else {
                    msgq->sendErrors++;
                }
                (void) pthread_mutex_unlock(&msgq->mutex);

                if (rc == CPR_MSGQ_POST_SUCCESS) {
                    return CPR_SUCCESS;
                }
                else
                {
                    CPR_ERROR(error_str, fname, msgq->name, "no memory");
                    return CPR_FAILURE;
                }
            }

            /*
             * Even the extended message queue is full, so
             * release the message queue mutex and use the
             * re-try procedure.
             */
            (void) pthread_mutex_unlock(&msgq->mutex);

            /*
             * If attempting to post to the calling thread's
             * own message queue, the re-try procedure will
             * not work.  No options left...fail with an error.
             */
            if (pthread_self() == msgq->thread) {
                msgq->selfQErrors++;
                msgq->sendErrors++;
                CPR_ERROR(error_str, fname, msgq->name, "FULL");
                return CPR_FAILURE;
            }
        } else {
            /*
             * Normal posting of message
             */
            rc = cprPostMessage(msgq, msg, ppUserData);

            /*
             * Before releasing the mutex, check if the
             * return code is 'pending' which means the
             * system message queue is full
             */
            if (rc == CPR_MSGQ_POST_PENDING) {
                /*
                 * If the message queue has enabled the extended queue
                 * support, then attempt to add to the extended queue.
                 */
                if (msgq->maxExtendedQDepth) {
                    rc = cprPostExtendedQMsg(msgq, msg, ppUserData);
                }
            }

            (void) pthread_mutex_unlock(&msgq->mutex);

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
             * Else pending due to a full message queue
             * and the extended queue has not been enabled,
             * so just use the re-try attempts.
             */
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
 * @}
 * @addtogroup MsgQIPCHelper Internal Helper functions for MsgQ
 * @ingroup IPC
 * @brief Helper functions used by CPR to implement the Message Queue IPC APIs
 * @{
 */

/**
 * cprPegSendMessageStats
 * @brief Peg the statistics for successfully posting a message
 *
 * @param[in] msgq        - message queue
 * @param[in] numAttempts - number of attempts to post message to message queue
 *
 * @return none
 *
 * @pre (msgq != NULL)
 */
static void
cprPegSendMessageStats (cpr_msg_queue_t *msgq, uint16_t numAttempts)
{
    /*
     * Collect statistics
     */
    msgq->totalCount++;
    if (msgq->currentCount > msgq->maxCount) {
        msgq->maxCount = msgq->currentCount;
    }

    if (numAttempts > msgq->highAttempts) {
        msgq->highAttempts = numAttempts;
    }
}

/**
 * cprPostMessage
 * @brief Post message to system message queue
 *
 * @param[in] msgq       - message queue
 * @param[in] msg        - message to post
 * @param[in] ppUserData - ptr to ptr to option user data
 *
 * @return the post result which is CPR_MSGQ_POST_SUCCESS,
 *         CPR_MSGQ_POST_FAILURE or CPR_MSGQ_POST_PENDING
 *
 * @pre (msgq != NULL)
 * @pre (msg != NULL)
 */
static cpr_msgq_post_result_e
cprPostMessage (cpr_msg_queue_t *msgq, void *msg, void **ppUserData)
{
    struct msgbuffer mbuf;

    /*
     * Put msg user wants to send into a CNU msg buffer
     * Copy the address of the msg buffer into the mtext
     * portion of the message.
     */
    mbuf.mtype = CPR_IPC_MSG;
    mbuf.msgPtr = msg;

    if (ppUserData != NULL) {
        mbuf.usrPtr = *ppUserData;
    } else {
        mbuf.usrPtr = NULL;
    }

    /*
     * Send message buffer
     */
    if (msgsnd(msgq->queueId, &mbuf,
    		 sizeof(struct msgbuffer) - offsetof(struct msgbuffer, msgPtr),
               IPC_NOWAIT) != -1) {
        msgq->currentCount++;
        return CPR_MSGQ_POST_SUCCESS;
    }

    /*
     * If msgsnd system call would block, handle separately;
     * otherwise a real system error.
     */
    if (errno == EAGAIN) {
        return CPR_MSGQ_POST_PENDING;
    }

    return CPR_MSGQ_POST_FAILED;
}

/**
 * cprPostExtendedQMsg
 * @brief Post message to internal extended message queue
 *
 * @param[in] msgq       - message queue
 * @param[in] msg        - message to post
 * @param[in] ppUserData - ptr to ptr to option user data
 *
 * @return the post result which is CPR_MSGQ_POST_SUCCESS or
 *         CPR_MSGQ_POST_FAILURE if no memory available
 *
 * @pre (msgq != NULL)
 * @pre (msg != NULL)
 * @pre (msgq->mutex has been locked)
 * @pre (msgq->extendedQDepth < msgq->maxExtendedQDepth)
 *
 * @todo Could use cpr_chunk_malloc to pre-allocate all of the nodes
 *       but that does have the consequence of allocating memory that
 *       may not be necessary
 */
static cpr_msgq_post_result_e
cprPostExtendedQMsg (cpr_msg_queue_t *msgq, void *msg, void **ppUserData)
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
    msgq->extendedQDepth++;
    msgq->currentCount++;

    return CPR_MSGQ_POST_SUCCESS;
}


/**
 * cprMoveMsgToQueue
 * @brief Move message from extended internal queue to system message queue
 *
 * @param[in] msgq - the message queue
 *
 * @return none
 *
 * @pre (msgq != NULL)
 * @pre (msgq->extendedQDepth > 0)
 */
static void
cprMoveMsgToQueue (cpr_msg_queue_t *msgq)
{
    static const char *fname = "cprMoveMsgToQueue";
    cpr_msgq_post_result_e rc;
    cpr_msgq_node_t *node;

    (void) pthread_mutex_lock(&msgq->mutex);

    if (!msgq->tail) {
        /* the linked list is bad...ignore it */
        CPR_ERROR("%s: MsgQ (%s) list is corrupt", fname, msgq->name);
        (void) pthread_mutex_unlock(&msgq->mutex);
        return;
    }

    node = msgq->tail;

    rc = cprPostMessage(msgq, node->msg, &node->pUserData);
    if (rc == CPR_MSGQ_POST_SUCCESS) {
        /*
         * Remove node from extended list
         */
        msgq->tail = node->prev;
        if (msgq->tail) {
            msgq->tail->next = NULL;
        }
        if (msgq->head == node) {
            msgq->head = NULL;
        }
        msgq->extendedQDepth--;
        /*
         * Fix increase in the current count which was incremented
         * in cprPostMessage but not really an addition.
         */
        msgq->currentCount--;
    }

    (void) pthread_mutex_unlock(&msgq->mutex);

    if (rc == CPR_MSGQ_POST_SUCCESS) {
        cpr_free(node);
    } else {
        CPR_ERROR("%s: Failed to repost msg on %s queue: %d\n",
                  fname, msgq->name, errno);
    }
}

/**
  * @}
  *
  * @addtogroup MsgQIPCAPIs The Message Queue IPC APIs
  * @{
  */

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
 * @pre (msgQueue != NULL)
 */
uint16_t cprGetDepth (cprMsgQueue_t msgQueue)
{
        cpr_msg_queue_t *msgq;
        msgq = (cpr_msg_queue_t *) msgQueue;
        return msgq->currentCount;
}

