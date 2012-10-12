/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <afx.h>
#include <afxwin.h>
#include <afxmt.h>
#include "cpr_types.h"
#include "cpr_threads.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "cpr_debug.h"
#include "cpr_memory.h"


/**
 * cprSuspendThread
 *
 * Suspend a thread until someone is nice enough to call cprResumeThread
 *
 * Parameters: thread - which system thread to suspend
 *
 * Return Value: Success or failure indication
 */
cprRC_t
cprSuspendThread(cprThread_t thread)
{
    int32_t returnCode;
    static const char fname[] = "cprSuspendThread";
    cpr_thread_t *cprThreadPtr;

    cprThreadPtr = (cpr_thread_t*)thread;
    if (cprThreadPtr != NULL) {

		CWinThread *pCWinThread;
		pCWinThread = (CWinThread *)cprThreadPtr->u.handlePtr;
		if (pCWinThread != NULL) {
			returnCode = pCWinThread->SuspendThread();
			if (returnCode == -1) {
				CPR_ERROR("%s - Suspend thread failed: %d\n",
					fname,GetLastError());
				return(CPR_FAILURE);
			}
			return(CPR_SUCCESS);

			/* Bad application! */
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
		CWinThread *pCWinThread;
		pCWinThread = (CWinThread *)cprThreadPtr->u.handlePtr;
		if (pCWinThread != NULL) {

			returnCode = pCWinThread->ResumeThread();
			if (returnCode == -1) {
				CPR_ERROR("%s - Resume thread failed: %d\n",
					fname, GetLastError());
				return(CPR_FAILURE);
			}
			return(CPR_SUCCESS);
		}
		/* Bad application! */
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
	CEvent serialize_lock;

    /* Malloc memory for a new thread */
    threadPtr = (cpr_thread_t *)cpr_malloc(sizeof(cpr_thread_t));
    if (threadPtr != NULL) {

        /* Assign name to CPR and CNU if one was passed in */
        if (name != NULL) {
            threadPtr->name = name;
        }

		threadPtr->u.handlePtr = AfxBeginThread((AFX_THREADPROC)startRoutine, data, priority, stackSize);

        if (threadPtr->u.handlePtr != NULL) {
			PostThreadMessage(((CWinThread *)(threadPtr->u.handlePtr))->m_nThreadID, MSG_ECHO_EVENT, (unsigned long)&serialize_lock, 0);
			result = WaitForSingleObject(serialize_lock, 1000);
			serialize_lock.ResetEvent();
		}
		else
		{
			CPR_ERROR("%s - Thread creation failure: %d\n", fname, GetLastError());
			cpr_free(threadPtr);
            threadPtr = NULL;

        }
    } else {
        /* Malloc failed */
        CPR_ERROR("%s - Malloc for new thread failed.\n", fname);
    }
	return(threadPtr);
};


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
	cprRC_t retCode = CPR_FAILURE;
    static const char fname[] = "cprDestroyThread";
    cpr_thread_t *cprThreadPtr;

    cprThreadPtr = (cpr_thread_t*)thread;
    if (cprThreadPtr != NULL) {
		CWinThread * pCWinThread;
		uint32_t result = 0;
		uint32_t waitrc = WAIT_FAILED;
		pCWinThread = (CWinThread *)((cpr_thread_t *)thread)->u.handlePtr;
		if (pCWinThread !=NULL) {
			result = pCWinThread->PostThreadMessage(WM_CLOSE, 0, 0);
			if(result) {
				waitrc = WaitForSingleObject(pCWinThread->m_hThread, 60000);
			}
		}
		if (result == 0) {
			CPR_ERROR("%s - Thread exit failure %d\n", fname, GetLastError());
            retCode = CPR_FAILURE;
		}
        retCode = CPR_SUCCESS;
    /* Bad application! */
    } else {
        CPR_ERROR("%s - NULL pointer passed in.\n", fname);
        retCode = CPR_FAILURE;
    }
	cpr_free(cprThreadPtr);
	return (retCode);
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
