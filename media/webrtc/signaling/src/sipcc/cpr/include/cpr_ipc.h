/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_IPC_H_
#define _CPR_IPC_H_

#include "cpr_types.h"

__BEGIN_DECLS

/**
 * Define handle for message queues
 */
typedef void* cprMsgQueue_t;

/*
 * A timeout value of -1 means wait forever when
 * attempting to get a message. Just #define something
 * so the application code is easier to read.
 */
#define WAIT_FOREVER -1

#if defined SIP_OS_LINUX
#include "../linux/cpr_linux_ipc.h"
#elif defined SIP_OS_WINDOWS
#include "../win32/cpr_win_ipc.h"
#elif defined SIP_OS_OSX
#include "../darwin/cpr_darwin_ipc.h"
#endif

/* Function prototypes */
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
cprCreateMessageQueue(const char *name, uint16_t depth);


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
cprDestroyMessageQueue(cprMsgQueue_t msgQueue);

#ifdef CPR_USE_SET_MESSAGE_QUEUE_THREAD
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
cprSetMessageQueueThread(cprMsgQueue_t msgQueue, cprThread_t thread);
#endif


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
cprGetMessage(cprMsgQueue_t msgQueue,
              boolean waitForever,
              void** usrPtr);

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
cprSendMessage(cprMsgQueue_t msgQueue,
               void* msg,
               void** usrPtr);

__END_DECLS

#endif

