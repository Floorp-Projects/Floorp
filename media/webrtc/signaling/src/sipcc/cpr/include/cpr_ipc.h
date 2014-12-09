/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_IPC_H_
#define _CPR_IPC_H_

#include "cpr_types.h"
#include "cpr_threads.h"

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

/* Enable support for cprSetMessageQueueThread API */
#define CPR_USE_SET_MESSAGE_QUEUE_THREAD

/* Maximum message size allowed by CNU */
#define CPR_MAX_MSG_SIZE  8192

/* Function prototypes */

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
 * cprGetDepth
 *
 * Get depth of a message queue
 */
uint16_t cprGetDepth(cprMsgQueue_t msgQueue);

__END_DECLS

#endif

