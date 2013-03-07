/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr.h"
#include "cpr_stdlib.h"
#include "cpr_stdio.h"
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>
#include "thread_monitor.h"
#include "prtypes.h"
#include "mozilla/Assertions.h"

#define ANDROID_MIN_THREAD_PRIORITY (-20)	/* tbd: check MV linux: current val from Larry port */
#define ANDROID_MAX_THREAD_PRIORITY (+19)	/* tbd: check MV linux. current val from Larry port */

void CSFLogRegisterThread(const cprThread_t thread);

/**
 * cprCreateThread
 *
 * @brief Create a thread
 *
 *  The cprCreateThread function creates another execution thread within the
 *  current process. If the input parameter "name" is present, then this is used
 *  for debugging purposes. The startRoutine is the address of the function where
 *  the thread execution begins. The start routine prototype is defined as
 *  follows
 *  @code
 *     int32_t (*cprThreadStartRoutine)(void* data)
 *  @endcode
 *
 * @param[in]  name         - name of the thread created (optional)
 * @param[in]  startRoutine - function where thread execution begins
 * @param[in]  stackSize    - size of the thread's stack
 * @param[in]  priority     - thread's execution priority
 * @param[in]  data         - parameter to pass to startRoutine
 *
 * Return Value: Thread handle or NULL if creation failed.
 */
cprThread_t
cprCreateThread (const char *name,
                 cprThreadStartRoutine startRoutine,
                 uint16_t stackSize,
                 uint16_t priority,
                 void *data)
{
    static const char fname[] = "cprCreateThread";
    static uint16_t id = 0;
    cpr_thread_t *threadPtr;
    pthread_t threadId;
    pthread_attr_t attr;

    CPR_INFO("%s: creating '%s' thread\n", fname, name);

    /* Malloc memory for a new thread */
    threadPtr = (cpr_thread_t *)cpr_malloc(sizeof(cpr_thread_t));
    if (threadPtr != NULL) {
        if (pthread_attr_init(&attr) != 0) {

            CPR_ERROR("%s - Failed to init attribute for thread %s\n",
                      fname, name);
            cpr_free(threadPtr);
            return (cprThread_t)NULL;
        }

        if (pthread_attr_setstacksize(&attr, stackSize) != 0) {
            CPR_ERROR("%s - Invalid stacksize %d specified for thread %s\n",
                      fname, stackSize, name);
            cpr_free(threadPtr);
            return (cprThread_t)NULL;
        }

        if (pthread_create(&threadId, &attr, startRoutine, data) != 0) {
            CPR_ERROR("%s - Creation of thread %s failed: %d\n",
                      fname, name, errno);
            cpr_free(threadPtr);
            return (cprThread_t)NULL;
        }

        /* Assign name to CPR if one was passed in */
        if (name != NULL) {
            threadPtr->name = name;
        }

        /*
         * TODO - It would be nice for CPR to keep a linked
         * list of running threads for debugging purposes
         * such as a show command or walking the list to ensure
         * that an application does not attempt to create
         * the same thread twice.
         */
        threadPtr->u.handleInt = threadId;
        threadPtr->threadId = ++id;
        CSFLogRegisterThread(threadPtr);
        return (cprThread_t)threadPtr;
    }

    /* Malloc failed */
    CPR_ERROR("%s - Malloc for thread %s failed.\n", fname, name);
    errno = ENOMEM;
    return (cprThread_t)NULL;
}

/*
 * cprJoinThread
 *
 * wait for thread termination
 */
void cprJoinThread(cprThread_t thread)
{
    cpr_thread_t *cprThreadPtr;

    cprThreadPtr = (cpr_thread_t *) thread;
    MOZ_ASSERT(cprThreadPtr);
    pthread_join(cprThreadPtr->u.handleInt, NULL);
}

/**
 * cprDestroyThread
 *
 * @brief Destroys the thread passed in.
 *
 * The cprDestroyThread function is called to destroy a thread. The thread
 * parameter may be any valid thread including the calling thread itself.
 *
 * @param[in] thread - thread to destroy.
 *
 * @return CPR_SUCCESS or CPR_FAILURE. errno should be set for FAILURE case.
 *
 * @note In Linux there will never be a success indication as the
 *       calling thread will have been terminated.
 */
cprRC_t
cprDestroyThread (cprThread_t thread)
{
    cpr_thread_t *cprThreadPtr;

    cprThreadPtr = (cpr_thread_t *) thread;
    if (cprThreadPtr) {
        /*
         * Make sure thread is trying to destroy itself.
         */
        if ((pthread_t) cprThreadPtr->u.handleInt == pthread_self()) {
            CPR_INFO("%s: Destroying Thread %d", __FUNCTION__, cprThreadPtr->threadId);
            pthread_exit(NULL);
            return CPR_SUCCESS;
        }

        CPR_ERROR("%s: Thread attempted to destroy another thread, not itself.",
                  __FUNCTION__);
        MOZ_ASSERT(PR_FALSE);
        errno = EINVAL;
        return CPR_FAILURE;
    }

    CPR_ERROR("%s - NULL pointer passed in.", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    errno = EINVAL;
    return CPR_FAILURE;
}

/**
 * cprAdjustRelativeThreadPriority
 *
 * @brief The function sets the relative thread priority up or down by the given value.
 *
 * This function is used pSIPCC to set up the thread priority. The values of the
 * priority range from -20 (Maximum priority) to +19 (Minimum priority).
 *
 * @param[in] relPri - nice value of the thread -20 is MAX and 19 is MIN
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 */
cprRC_t
cprAdjustRelativeThreadPriority (int relPri)
{
    const char *fname = "cprAdjustRelativeThreadPriority";

    if (setpriority(PRIO_PROCESS, 0, relPri) == -1) {
        CPR_ERROR("%s: could not set the nice..err=%d\n",
                  fname, errno);
        return CPR_FAILURE;
    }
    return CPR_SUCCESS;
}

/**
 * @}
 * @addtogroup ThreadInternal Helper functions for implementing threads in CPR
 * @ingroup Threads
 * @brief Helper functions used by CPR for thread implementation
 *
 * @{
 */

/**
 * cprGetThreadId
 *
 * @brief Return the pthread ID for the given CPR thread.
 *
 * @param[in] thread - thread to query
 *
 * @return Thread's Id or zero(0)
 *
 */
pthread_t
cprGetThreadId (cprThread_t thread)
{
    if (thread) {
        return ((cpr_thread_t *)thread)->u.handleInt;
    }
    return 0;
}
/**
  * @}
  */
