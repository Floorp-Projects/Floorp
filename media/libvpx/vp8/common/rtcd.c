/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "vpx_config.h"
#define RTCD_C
#include "vpx_rtcd.h"

#if CONFIG_MULTITHREAD && defined(_WIN32)
#include <windows.h>
#include <stdlib.h>
static void once(void (*func)(void))
{
    static CRITICAL_SECTION *lock;
    static LONG waiters;
    static int done;
    void *lock_ptr = &lock;

    /* If the initialization is complete, return early. This isn't just an
     * optimization, it prevents races on the destruction of the global
     * lock.
     */
    if(done)
        return;

    InterlockedIncrement(&waiters);

    /* Get a lock. We create one and try to make it the one-true-lock,
     * throwing it away if we lost the race.
     */

    {
        /* Scope to protect access to new_lock */
        CRITICAL_SECTION *new_lock = malloc(sizeof(CRITICAL_SECTION));
        InitializeCriticalSection(new_lock);
        if (InterlockedCompareExchangePointer(lock_ptr, new_lock, NULL) != NULL)
        {
            DeleteCriticalSection(new_lock);
            free(new_lock);
        }
    }

    /* At this point, we have a lock that can be synchronized on. We don't
     * care which thread actually performed the allocation.
     */

    EnterCriticalSection(lock);

    if (!done)
    {
        func();
        done = 1;
    }

    LeaveCriticalSection(lock);

    /* Last one out should free resources. The destructed objects are
     * protected by checking if(done) above.
     */
    if(!InterlockedDecrement(&waiters))
    {
        DeleteCriticalSection(lock);
        free(lock);
        lock = NULL;
    }
}


#elif CONFIG_MULTITHREAD && HAVE_PTHREAD_H
#include <pthread.h>
static void once(void (*func)(void))
{
    static pthread_once_t lock = PTHREAD_ONCE_INIT;
    pthread_once(&lock, func);
}


#else
/* No-op version that performs no synchronization. vpx_rtcd() is idempotent,
 * so as long as your platform provides atomic loads/stores of pointers
 * no synchronization is strictly necessary.
 */

static void once(void (*func)(void))
{
    static int done;

    if(!done)
    {
        func();
        done = 1;
    }
}
#endif


void vpx_rtcd()
{
    once(setup_rtcd_internal);
}
