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
#include "prerr.h"
#include "prerror.h"

PR_BEGIN_EXTERN_C

typedef struct PRJobIoDesc {
    PRFileDesc *socket;
    PRErrorCode error;
    PRIntervalTime timeout;
} PRJobIoDesc;

typedef struct PRThreadPool PRThreadPool;
typedef struct PRJob PRJob;
typedef void (PR_CALLBACK *JobFn) (void *arg);

/* Create thread pool */
PR_EXTERN(PRThreadPool *)
PR_CreateThreadPool(PRInt32 initial_threads, PRInt32 max_threads,
                          PRSize stacksize);

/* queue a job */
PR_EXTERN(PRJob *)
PR_QueueJob(PRThreadPool *tpool, JobFn fn, void *arg, PRBool joinable);

/* queue a job, when a socket is readable */
PR_EXTERN(PRJob *)
PR_QueueJob_Read(PRThreadPool *tpool, PRJobIoDesc *iod,
							JobFn fn, void * arg, PRBool joinable);

/* queue a job, when a socket is writeable */
PR_EXTERN(PRJob *)
PR_QueueJob_Write(PRThreadPool *tpool, PRJobIoDesc *iod,
								JobFn fn, void * arg, PRBool joinable);

/* queue a job, when a socket has a pending connection */
PR_EXTERN(PRJob *)
PR_QueueJob_Accept(PRThreadPool *tpool, PRJobIoDesc *iod,
									JobFn fn, void * arg, PRBool joinable);

/* queue a job, when a timer exipres */
PR_EXTERN(PRJob *)
PR_QueueJob_Timer(PRThreadPool *tpool, PRIntervalTime timeout,
								JobFn fn, void * arg, PRBool joinable);
/* cancel a job */
PR_EXTERN(PRStatus)
PR_CancelJob(PRJob *job);

/* join a job */
PR_EXTERN(PRStatus)
PR_JoinJob(PRJob *job);

/* shutdown pool */
PR_EXTERN(PRStatus)
PR_ShutdownThreadPool(PRThreadPool *tpool);

/* join pool, wait for exit of all threads */
PR_EXTERN(PRStatus)
PR_JoinThreadPool(PRThreadPool *tpool);

PR_END_EXTERN_C

#endif /* prtpool_h___ */
