/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef prtpool_h___
#define prtpool_h___

#include "prtypes.h"
#include "prthread.h"
#include "prio.h"
#include "prerror.h"

/*
 * NOTE:
 *		THIS API IS A PRELIMINARY VERSION IN NSPR 4.0 AND IS SUBJECT TO
 *		CHANGE
 */

PR_BEGIN_EXTERN_C

typedef struct PRJobIoDesc {
    PRFileDesc *socket;
    PRErrorCode error;
    PRIntervalTime timeout;
} PRJobIoDesc;

typedef struct PRThreadPool PRThreadPool;
typedef struct PRJob PRJob;
typedef void (PR_CALLBACK *PRJobFn) (void *arg);

/* Create thread pool */
NSPR_API(PRThreadPool *)
PR_CreateThreadPool(PRInt32 initial_threads, PRInt32 max_threads,
                          PRUint32 stacksize);

/* queue a job */
NSPR_API(PRJob *)
PR_QueueJob(PRThreadPool *tpool, PRJobFn fn, void *arg, PRBool joinable);

/* queue a job, when a socket is readable */
NSPR_API(PRJob *)
PR_QueueJob_Read(PRThreadPool *tpool, PRJobIoDesc *iod,
							PRJobFn fn, void * arg, PRBool joinable);

/* queue a job, when a socket is writeable */
NSPR_API(PRJob *)
PR_QueueJob_Write(PRThreadPool *tpool, PRJobIoDesc *iod,
								PRJobFn fn, void * arg, PRBool joinable);

/* queue a job, when a socket has a pending connection */
NSPR_API(PRJob *)
PR_QueueJob_Accept(PRThreadPool *tpool, PRJobIoDesc *iod,
									PRJobFn fn, void * arg, PRBool joinable);

/* queue a job, when the socket connection to addr succeeds or fails */
NSPR_API(PRJob *)
PR_QueueJob_Connect(PRThreadPool *tpool, PRJobIoDesc *iod,
			const PRNetAddr *addr, PRJobFn fn, void * arg, PRBool joinable);

/* queue a job, when a timer exipres */
NSPR_API(PRJob *)
PR_QueueJob_Timer(PRThreadPool *tpool, PRIntervalTime timeout,
								PRJobFn fn, void * arg, PRBool joinable);
/* cancel a job */
NSPR_API(PRStatus)
PR_CancelJob(PRJob *job);

/* join a job */
NSPR_API(PRStatus)
PR_JoinJob(PRJob *job);

/* shutdown pool */
NSPR_API(PRStatus)
PR_ShutdownThreadPool(PRThreadPool *tpool);

/* join pool, wait for exit of all threads */
NSPR_API(PRStatus)
PR_JoinThreadPool(PRThreadPool *tpool);

PR_END_EXTERN_C

#endif /* prtpool_h___ */
