/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_WIN_IPC_H_
#define _CPR_WIN_IPC_H_

#include "cpr_threads.h"

/* Enable support for cprSetMessageQueueThread API */
#define CPR_USE_SET_MESSAGE_QUEUE_THREAD

/* Maximum message size allowed by CNU */
#define CPR_MAX_MSG_SIZE  8192

/* Our CNU msgtype */
#define PHONE_IPC_MSG 0xF005

/* Msg buffer layout */
struct msgbuffer {
    int32_t mtype; /* Message type */
    void *msgPtr;  /* Ptr to msg */
    void *usrPtr;  /* Ptr to user data */
};

#endif
