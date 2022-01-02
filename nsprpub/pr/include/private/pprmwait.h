/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(_PPRMWAIT_H)
#else
#define _PPRMWAIT_H

#include "prlock.h"
#include "prcvar.h"
#include "prclist.h"
#include "prthread.h"

#define MAX_POLLING_INTERVAL 100
#define _PR_POLL_COUNT_FUDGE 64
#define _PR_DEFAULT_HASH_LENGTH 59

/*
 * Our hash table resolves collisions by open addressing with
 * double hashing.  See Cormen, Leiserson, and Rivest,
 * Introduction to Algorithms, p. 232, The MIT Press, 1990.
 */

#define _MW_HASH(a, m) ((((PRUptrdiff)(a) >> 4) ^ ((PRUptrdiff)(a) >> 10)) % (m))
#define _MW_HASH2(a, m) (1 + ((((PRUptrdiff)(a) >> 4) ^ ((PRUptrdiff)(a) >> 10)) % (m - 2)))
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
    PRUint32 p_timestamp;       /* pseudo-time group had element removed */
    PRPollDesc *polling_list;   /* list poller builds for polling */
    PRIntervalTime last_poll;   /* last time we polled */
    _PRWaiterHash *waiter;      /* pointer to hash table of wait receive objects */

#ifdef WINNT
    /*
     * On NT, idle threads are responsible for getting completed i/o.
     * They need to add completed i/o to the io_ready list.  Since
     * idle threads cannot use nspr locks, we have to use an md lock
     * to protect the io_ready list.
     */
    _MDLock mdlock;             /* protect io_ready, waiter, and wait_list */
    PRCList wait_list;          /* used in place of io_complete.  reuse
                                 * waitQLinks in the PRThread structure. */
#endif /* WINNT */
};

/**********************************************************************
***********************************************************************
******************** Wait group enumerations **************************
***********************************************************************
**********************************************************************/
typedef struct _PRGlobalState
{
    PRCList group_list;         /* master of the group list */
    PRWaitGroup *group;         /* the default (NULL) group */
} _PRGlobalState;

#ifdef WINNT
extern PRStatus NT_HashRemoveInternal(PRWaitGroup *group, PRFileDesc *fd);
#endif

typedef enum {_PR_ENUM_UNSEALED=0, _PR_ENUM_SEALED=0x0eadface} _PREnumSeal;

struct PRMWaitEnumerator
{
    PRWaitGroup *group;       /* group this enumerator is bound to */
    PRThread *thread;               /* thread in midst of an enumeration */
    _PREnumSeal seal;               /* trying to detect deleted objects */
    PRUint32 p_timestamp;           /* when enumeration was (re)started */
    PRRecvWait **waiter;            /* pointer into hash table */
    PRUintn index;                  /* position in hash table */
    void *pad[4];                   /* some room to grow */
};

#endif /* defined(_PPRMWAIT_H) */

/* pprmwait.h */
