/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#if defined(_PPRMWAIT_H)
#else
#define _PPRMWAIT_H

#include "prlock.h"
#include "prcvar.h"
#include "prclist.h"
#include "prthread.h"

#define _PR_HASH_OFFSET 75013
#define MAX_POLLING_INTERVAL 100
#define _PR_POLL_COUNT_FUDGE 64
#define MAX_POLLING_INTERVAL 100
#define _PR_DEFAULT_HASH_LENGTH 59

#define _MW_REHASH(a, i, m) _MW_HASH((PRUint32)(a) + (i) + _PR_HASH_OFFSET, m)
#define _MW_HASH(a, m) ((((PRUint32)(a) >> 4) ^ ((PRUint32)(a) >> 10)) % (m))
#define _MW_ABORTED(_rv) \
    ((PR_FAILURE == (_rv)) && (PR_PENDING_INTERRUPT_ERROR == PR_GetError()))

typedef enum {_prmw_success, _prmw_rehash, _prmw_error} _PR_HashStory;

typedef struct _PRWaiterHash
{
    PRUint16 count;             /* current number in hash table */
    PRUint16 length;            /* current size of the hash table */
    PRRecvWait *recv_wait;      /* hash table of receive wait objects */
} _PRWaiterHash;

typedef enum {_prmw_running, _prmw_stopping, _prmw_stopped} PRMWGroupState;

struct PRWaitGroup
{
    PRCList group_link;         /* all groups are linked to each other */
    PRCList io_ready;           /* list of I/O requests that are ready */
    PRMWGroupState state;       /* state of this group (so we can shut down) */

    PRLock *ml;                 /* lock for synchronizing this wait group */
    PRCondVar *io_taken;        /* calling threads notify when they take I/O */
    PRCondVar *io_complete;     /* calling threads wait here for completions */
    PRCondVar *new_business;    /* polling thread waits here more work */
    PRCondVar *mw_manage;       /* used to manage group lists */
    PRThread* poller;           /* thread that's actually doing the poll() */
    PRUint16 waiting_threads;   /* number of threads waiting for recv */
    PRUint16 polling_count;     /* number of elements in the polling list */
    PRPollDesc *polling_list;   /* list poller builds for polling */
    PRIntervalTime last_poll;   /* last time we polled */
    _PRWaiterHash *waiter;      /* pointer to hash table of wait receive objects */
};

typedef struct _PRGlobalState
{
    PRCList group_list;         /* master of the group list */
    PRWaitGroup *group;         /* the default (NULL) group */
} _PRGlobalState;

#endif /* defined(_PPRMWAIT_H) */

/* pprmwait.h */
