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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * This is a regression test for the bug that the interval timer
 * is not initialized when _PR_CreateCPU calls PR_IntervalNow.
 * The bug would make this test program finish prematurely,
 * when the SHORT_TIMEOUT period expires.  The correct behavior
 * is for the test to finish when the LONG_TIMEOUT period expires.
 */

#include "prlock.h"
#include "prcvar.h"
#include "prthread.h"
#include "prinrval.h"
#include "prlog.h"
#include <stdio.h>

/* The timeouts, in milliseconds */
#define SHORT_TIMEOUT 1000
#define LONG_TIMEOUT 3000

PRLock *lock1, *lock2;
PRCondVar *cv1, *cv2;

void ThreadFunc(void *arg)
{
    PR_Lock(lock1);
    PR_WaitCondVar(cv1, PR_MillisecondsToInterval(SHORT_TIMEOUT));
    PR_Unlock(lock1);
}

int main()
{
    PRThread *thread;
    PRIntervalTime start, end;
    PRUint32 elapsed_ms;

    lock1 = PR_NewLock();
    PR_ASSERT(NULL != lock1);
    cv1 = PR_NewCondVar(lock1);
    PR_ASSERT(NULL != cv1);
    lock2 = PR_NewLock();
    PR_ASSERT(NULL != lock2);
    cv2 = PR_NewCondVar(lock2);
    PR_ASSERT(NULL != cv2);
    start = PR_IntervalNow();
    thread = PR_CreateThread(
            PR_USER_THREAD,
            ThreadFunc,
            NULL,
            PR_PRIORITY_NORMAL,
            PR_LOCAL_THREAD,
            PR_JOINABLE_THREAD,
            0);
    PR_ASSERT(NULL != thread);
    PR_Lock(lock2);
    PR_WaitCondVar(cv2, PR_MillisecondsToInterval(LONG_TIMEOUT));
    PR_Unlock(lock2);
    PR_JoinThread(thread);
    end = PR_IntervalNow();
    elapsed_ms = PR_IntervalToMilliseconds((PRIntervalTime)(end - start));
    /* Allow 100ms imprecision */
    if (elapsed_ms < LONG_TIMEOUT - 100 || elapsed_ms > LONG_TIMEOUT + 100) {
        printf("Elapsed time should be %u ms but is %u ms\n",
                LONG_TIMEOUT, elapsed_ms);
        printf("FAIL\n");
        exit(1);
    }
	printf("Elapsed time: %u ms, expected time: %u ms\n",
               LONG_TIMEOUT, elapsed_ms);
    printf("PASS\n");
    return 0;
}
