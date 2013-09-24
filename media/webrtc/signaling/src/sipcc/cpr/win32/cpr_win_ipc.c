/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_ipc.h"
#include "cpr_memory.h"
#include "cpr_stdlib.h"
#include "cpr_debug.h"
#include "cpr_stdio.h"
#include "cpr_threads.h"

#include <windows.h>
#include <process.h>
#include <winuser.h>


//#ifndef _JAVA_UI_
//extern cprMsgQueue_t gui_msgq;
//#endif
extern cprMsgQueue_t sip_msgq;
extern cprMsgQueue_t gsm_msgq;
extern cprMsgQueue_t tmr_msgq;

extern void gsm_shutdown();
extern void sip_shutdown();

int sip_regmgr_destroy_cc_conns(void);

static const char unnamed_string[] = "unnamed";


uint16_t cprGetDepth (cprMsgQueue_t msgQueue)
{
    return 0;
}


/*
 * Buffer to hold the messages sent/received by CPR. All
 * CPR does is pass four bytes (CNU msg type) and an unsigned
 * four bytes (pointer to the msg buffer).
 */
static char rcvBuffer[100];
#define MSG_BUF 0xF000


/**
 * cprCreateMessageQueue
 *
 * Creates a message queue
 *
 * Parameters: name  - name of the message queue
 *             depth - message queue depth, optional value
 *                     which will default when set to zero(0)
 *                     This parameter is currently not
 *                     supported on this platform.
 *
 * Return Value: Msg queue handle or NULL if init failed.
 */
cprMsgQueue_t
cprCreateMessageQueue (const char* name, uint16_t depth)
{
    cpr_msg_queue_t* msgQueuePtr;
    static uint32_t id = 0;
    static char fname[] = "cprCreateMessageQueue";

    msgQueuePtr = (cpr_msg_queue_t *)cpr_calloc(1, sizeof(cpr_msg_queue_t));
    if (msgQueuePtr == NULL) {
        CPR_ERROR("Malloc for new msg queue failed.\n");
        return NULL;
    }

    msgQueuePtr->name = name ? name : unnamed_string;
    return msgQueuePtr;
}

/**
 * cprSetMessageQueueThread
 *
 * Associate a thread with the message queue
 *
 * Parameters: msgQueue  - msg queue to set
 *             thread    - thread to associate with queue
 *
 * Returns: CPR_SUCCESS or CPR_FAILURE
 *
 * Comments: no error checking done to prevent overwriting
 *           the thread
 */
cprRC_t
cprSetMessageQueueThread (cprMsgQueue_t msgQueue, cprThread_t thread)
{
    static const char fname[] = "cprSetMessageQueueThread";

    if (!msgQueue) {
        CPR_ERROR("%s - msgQueue is NULL\n", fname);
        return CPR_FAILURE;
    }

    ((cpr_msg_queue_t *)msgQueue)->handlePtr = thread;
    return CPR_SUCCESS;
}


/**
 * cprSendMessage
 *
 * Place a message on a particular queue
 *
 * Parameters: msgQueue  - which queue on which to place the message
 *             msg       - pointer to the msg to place on the queue
 *             usrPtr    - pointer to a pointer to user defined data
 *
 * Return Value: Always returns success
 */
cprRC_t
cprSendMessage (cprMsgQueue_t msgQueue,
                void *msg,
                void **usrPtr)
{
    static const char fname[] = "cprSendMessage";
    struct msgbuffer *sendMsg;
    cpr_thread_t *pCprThread;
	HANDLE *hThread;

    if (!msgQueue) {
        CPR_ERROR("%s - msgQueue is NULL\n", fname);
        return CPR_FAILURE;
    }

    pCprThread = (cpr_thread_t *)(((cpr_msg_queue_t *)msgQueue)->handlePtr);
    if (!pCprThread) {
        CPR_ERROR("%s - msgQueue(%x) not associated with a thread\n", fname,
                msgQueue);
        return CPR_FAILURE;
    }

	hThread = (HANDLE*)(pCprThread->u.handlePtr);
	if (!hThread) {
        CPR_ERROR("%s - msgQueue(%x)'s thread(%x) not assoc. with Windows\n",
                fname, msgQueue, pCprThread);
        return CPR_FAILURE;
    }

    /* Package up the message */
    sendMsg = (struct msgbuffer *)cpr_calloc(1, sizeof(struct msgbuffer));
    if (!sendMsg) {
        CPR_ERROR("%s - No memory\n", fname);
        return CPR_FAILURE;
    }
    sendMsg->mtype = PHONE_IPC_MSG;

    /* Save the address of the message */
    sendMsg->msgPtr = msg;

    /* Allow the usrPtr to be optional */
    if (usrPtr) {
        sendMsg->usrPtr = *usrPtr;
    }

    /* Post the message */
	if ( hThread == NULL || PostThreadMessage(pCprThread->threadId, MSG_BUF, (WPARAM)sendMsg, 0) == 0 ) {
        CPR_ERROR("%s - Msg not sent: %d\n", fname, GetLastError());
        cpr_free(sendMsg);
        return CPR_FAILURE;
    }
	return CPR_SUCCESS;
}

void cjni_exit_thread();

/**
 * cprGetMessage
 *
 * Retrieve a message from a particular message queue
 *
 * Parameters: msgQueue    - which queue from which to retrieve the message
 *             waitForever - boolean to either wait forever (TRUE) or not
 *                           wait at all (FALSE) if the msg queue is empty.
 *             usrPtr      - pointer to a pointer to user defined data [OUT]
 *
 * Return Value: Retrieved message buffer or NULL if a failure occurred
 */
void *
cprGetMessage (cprMsgQueue_t msgQueue,
               boolean waitForever,
               void **usrPtr)
{
    static const char fname[] = "cprGetMessage";
    struct msgbuffer *rcvMsg = (struct msgbuffer *)rcvBuffer;
    void *bufferPtr = 0;
    cpr_msg_queue_t *pCprMsgQueue;
    MSG msg;
    cpr_thread_t *pThreadPtr;

    /* Initialize usrPtr */
    if (usrPtr) {
        *usrPtr = NULL;
    }

    pCprMsgQueue = (cpr_msg_queue_t *)msgQueue;
    if (!pCprMsgQueue) {
        CPR_ERROR("%s - invalid msgQueue\n", fname);
        return NULL;
    }

    memset(&msg, 0, sizeof(MSG));

    if (waitForever == TRUE) {
        if (GetMessage(&msg, NULL, 0, 0) == -1) {
            CPR_ERROR("%s - msgQueue = %x failed: %d\n",
                    fname, msgQueue, GetLastError());
            return NULL;
        }
    } else {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) == 0) {
            /* no message present */
            return NULL;
        }
    }

    switch (msg.message) {
        case WM_CLOSE:
                if (msgQueue == &gsm_msgq)
                {
                    CPR_ERROR("%s - WM_CLOSE GSM msg queue\n", fname);
                    gsm_shutdown();
                }
                else if (msgQueue == &sip_msgq)
                {
                    CPR_ERROR("%s - WM_CLOSE SIP msg queue\n", fname);
                    sip_regmgr_destroy_cc_conns();
			        sip_shutdown();
                }
//#ifndef _JAVA_UI_
//            else if (msgQueue == &gui_msgq)
//            {
//                CPR_ERROR("%s - WM_CLOSE GUI msg queue\n", fname);
//                // DFB FIXME
//            }
//#endif
            else if (msgQueue == &tmr_msgq)
            {
                CPR_ERROR("%s - WM_CLOSE TMR msg queue\n", fname);
                // DFB FIXME
            }

			pThreadPtr=(cpr_thread_t *)pCprMsgQueue->handlePtr;
            if (pThreadPtr)
            {
                CloseHandle(pThreadPtr->u.handlePtr);
            }
			pCprMsgQueue->handlePtr = NULL;	// zap the thread ptr, since the thread is going away now
			_endthreadex(0);
            break;
        case MSG_BUF:
            rcvMsg = (struct msgbuffer *)msg.wParam;
            bufferPtr = rcvMsg->msgPtr;
            if (usrPtr) {
                *usrPtr = rcvMsg->usrPtr;
            }
            cpr_free((void *)msg.wParam);
            break;
        case MSG_ECHO_EVENT:
            {
				HANDLE event;
				event = (HANDLE*)msg.wParam;
				SetEvent( event );
            }
            break;
        case WM_TIMER:
            DispatchMessage(&msg);
            return NULL;
            break;
        default:
            break;
    }
    return bufferPtr;
}

