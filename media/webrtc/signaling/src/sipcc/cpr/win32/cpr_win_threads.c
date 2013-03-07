/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_threads.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "cpr_debug.h"
#include "cpr_memory.h"
#include "prtypes.h"
#include "mozilla/Assertions.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <process.h>

void CSFLogRegisterThread(const cprThread_t thread);

typedef struct {
    cprThreadStartRoutine startRoutine;
	void *data;
	HANDLE event;
} startThreadData;

unsigned __stdcall
cprStartThread (void *arg)
{
    startThreadData *startThreadDataPtr = (startThreadData *) arg;

    /* Ensure that a message queue is created for this thread, by calling the PeekMessage */
    MSG msg;
    PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);

    /* Notify the thread that is waiting on the event */
    SetEvent( startThreadDataPtr->event );

    /* Call the original start up function requested by the caller of cprCreateThread() */
    (*startThreadDataPtr->startRoutine)(startThreadDataPtr->data);

    cpr_free(startThreadDataPtr);

    return CPR_SUCCESS;
}

/**
 * cprSuspendThread
 *
 * Suspend a thread until someone is nice enough to call cprResumeThread
 *
 * Parameters: thread - which system thread to suspend
 *
 * Return Value: Success or failure indication
 *
 * Comment: Enda Mannion ported this from MFC to Win32
 *			But it is untested as this is and unused
 *			function similar to cprResumeThread
 */
cprRC_t
cprSuspendThread(cprThread_t thread)
{
    int32_t returnCode;
    static const char fname[] = "cprSuspendThread";
    cpr_thread_t *cprThreadPtr;

    cprThreadPtr = (cpr_thread_t*)thread;
    if (cprThreadPtr != NULL) {

		HANDLE *hThread;
		hThread = (HANDLE *)cprThreadPtr->u.handlePtr;
		if (hThread != NULL) {

			returnCode = SuspendThread( hThread );

			if (returnCode == -1) {
				CPR_ERROR("%s - Suspend thread failed: %d\n",
					fname,GetLastError());
				return(CPR_FAILURE);
			}
			return(CPR_SUCCESS);

			// Bad application!
		}
	}
	CPR_ERROR("%s - NULL pointer passed in.\n", fname);

	return(CPR_FAILURE);
};


/**
 * cprResumeThread
 *
 * Resume execution of a previously suspended thread
 *
 * Parameters: thread - which system thread to resume
 *
 * Return Value: Success or failure indication
 */
cprRC_t
cprResumeThread(cprThread_t thread)
{
    int32_t returnCode;
    static const char fname[] = "cprResumeThread";
    cpr_thread_t *cprThreadPtr;

    cprThreadPtr = (cpr_thread_t*)thread;
    if (cprThreadPtr != NULL) {
		HANDLE *hThread;
		hThread = (HANDLE *)cprThreadPtr->u.handlePtr;

		if (hThread != NULL) {

			returnCode = ResumeThread(hThread);
			if (returnCode == -1) {
				CPR_ERROR("%s - Resume thread failed: %d\n",
					fname, GetLastError());
				return(CPR_FAILURE);
			}
			return(CPR_SUCCESS);
		}
		// Bad application!
    }
	CPR_ERROR("%s - NULL pointer passed in.\n", fname);

    return(CPR_FAILURE);
};



/**
 * cprCreateThread
 *
 * Create a thread
 *
 * Parameters: name         - name of the thread created
 *             startRoutine - function where thread execution begins
 *             stackSize    - size of the thread's stack (IGNORED)
 *             priority     - thread's execution priority (IGNORED)
 *             data         - parameter to pass to startRoutine
 *
 *
 * Return Value: Thread handle or NULL if creation failed.
 */
cprThread_t
cprCreateThread(const char* name,
                cprThreadStartRoutine startRoutine,
                uint16_t stackSize,
                uint16_t priority,
                void* data)
{
    cpr_thread_t* threadPtr;
    static char fname[] = "cprCreateThread";
	unsigned long result;
	HANDLE serialize_lock;
	startThreadData* startThreadDataPtr = 0;
	unsigned int ThreadId;

	/* Malloc memory for a new thread */
    threadPtr = (cpr_thread_t *)cpr_malloc(sizeof(cpr_thread_t));

	/* Malloc memory for start threa data */
	startThreadDataPtr = (startThreadData *) cpr_malloc(sizeof(startThreadData));

    if (threadPtr != NULL && startThreadDataPtr != NULL) {

        /* Assign name to CPR and CNU if one was passed in */
        if (name != NULL) {
            threadPtr->name = name;
        }

        startThreadDataPtr->startRoutine = startRoutine;
        startThreadDataPtr->data = data;

		serialize_lock = CreateEvent (NULL, FALSE, FALSE, NULL);
		if (serialize_lock == NULL)	{
			// Your code to deal with the error goes here.
			CPR_ERROR("%s - Event creation failure: %d\n", fname, GetLastError());
			cpr_free(threadPtr);
			threadPtr = NULL;
		}
		else
		{

			startThreadDataPtr->event = serialize_lock;

			threadPtr->u.handlePtr = (void*)_beginthreadex(NULL, 0, cprStartThread, (void*)startThreadDataPtr, 0, &ThreadId);

			if (threadPtr->u.handlePtr != NULL) {
				threadPtr->threadId = ThreadId;
				result = WaitForSingleObject(serialize_lock, 1000);
				ResetEvent( serialize_lock );
			}
			else
			{
				CPR_ERROR("%s - Thread creation failure: %d\n", fname, GetLastError());
				cpr_free(threadPtr);
				threadPtr = NULL;
			}
			CloseHandle( serialize_lock );
		}
    } else {
        /* Malloc failed */
        CPR_ERROR("%s - Malloc for new thread failed.\n", fname);
    }

    CSFLogRegisterThread(threadPtr);

    return(threadPtr);
};


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
    WaitForSingleObject(cprThreadPtr->u.handlePtr, INFINITE);
}

/*
 * cprDestroyThread
 *
 * Destroys the thread passed in.
 *
 * Parameters: thread  - thread to destroy.
 *
 * Return Value: Success or failure indication.
 *               In CNU there will never be a success
 *               indication as the calling thread will
 *               have been terminated.
 */
cprRC_t
cprDestroyThread(cprThread_t thread)
{
    cpr_thread_t *cprThreadPtr;

    cprThreadPtr = (cpr_thread_t*)thread;
    if (cprThreadPtr) {
        /*
         * Make sure thread is trying to destroy itself.
         */
        if (cprThreadPtr->threadId == GetCurrentThreadId()) {
            CPR_INFO("%s: Destroying Thread %d", __FUNCTION__, cprThreadPtr->threadId);
            ExitThread(0);
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
};

/*
 * cprAdjustRelativeThreadPriority
 *
 * The function sets the relative thread priority up or down by
 * the given value.
 *
 * Parameters: relPri - relative priority to be adjusted. The
 *                      relative priority is increased or decreased
 *                      by the given value.
 *                      For an example: If a thread wants to
 *                      increase its prirority by 2 levels from its
 *                      current priority then the thread should pass the
 *                      value of 2 to this function. If the thread wishes
 *                      to decrese the value of the priority by 1 then
 *                      the -1 should be passed to this function.
 *
 *
 * Return Value: Success or failure indication.
 *
 */
cprRC_t
cprAdjustRelativeThreadPriority (int relPri)
{
    /* Do not have support for adjusting priority on WIN32 yet */
    return (CPR_FAILURE);
}
