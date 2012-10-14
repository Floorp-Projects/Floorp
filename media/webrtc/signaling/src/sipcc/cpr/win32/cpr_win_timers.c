/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr.h"
#include "cpr_types.h"
#include "cpr_timers.h"
#include "cpr_win_timers.h"
#include "cpr_stdio.h"
#include "cpr_memory.h"
#include "cpr_stdlib.h"
#include "platform_api.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

/* Define timer debug flag.*/
//int32_t TMRDebug = 0; Soya: it has been defined in ccsip_debug.c

#define TIMER_FREE 0x1          /* Indicates timer is free */
#define TIMER_INITIALIZED 0x2   /* Indicates timer is initialized */
#define TIMER_ACTIVE 0x4        /* Indicates timer is in list */

#define CPRTMR_STK 0            /* CPR Timer stack size */
#define CPRTMR_PRIO 0           /* CPR Timer task priority */

extern void cprDisableSwap(void);
extern void cprEnableSwap(void);

/* Linked List of currently running timers */
typedef struct cprTimerDef {
    const char *name;
    cprMsgQueue_t callBackMsgQueue;
    uint16_t applicationTimerId;
    uint16_t applicationMsgId;
    void *data;
    uint32_t expiration_time;
    uint32_t flags;
    struct cprTimerDef *previous;
    struct cprTimerDef *next;
} cprTimerBlk;

/* Local data debug info */
static uint32_t expired_count;  /* Number of timers that expired */
static uint32_t removed_count;  /* Number of timers removed. */
static uint32_t inserted_count; /* Number of timers inserted. */
static uint32_t cpr_timer_init; /* Bool indicating the timer system is initialized */

cprMsgQueue_t tmr_msgq;
cprThread_t tmr_thread;


/*
 * Internal CPR function to fill in data in the sysheader.
 * This is to prevent knowledge of the evil syshdr structure
 * from spreading throughout the CPR code. This thing is
 * like kudzu...
 */
extern void *fillInSysHeader(void *buffer, uint16_t cmd, uint16_t len,
                             void *timerMsg);


static uint32_t ticker;
unsigned long current_time(void);


/*
 * Head of the list of currently running timers
 */
cprTimerBlk *cprTimerPendingList;

/*
 * Just a dummy msg to send to wake up the timer task
 */
static char tmr_msg[] = "WakeUpTimer";


/**
 * cprSleep
 *
 * Suspend the calling thread
 *
 * Parameters: duration - Number of milliseconds the thread should sleep
 *
 * Return Value: Nothing
 */
void
cprSleep (uint32_t duration)
{
    Sleep(duration);
};

/*------------------------------------------------------------------------------
 *  NAME:        timer_event_allocate()
 *
 *  PARAMETERS:
 *   None.
 *
 *  RETURNS:
 *   timer - if timer is available.
 *   0 - otherwise.
 *
 *  DESCRIPTION:
 *   Allocate a timer from free list of timers.
 *
 *----------------------------------------------------------------------------*/
void *
timer_event_allocate (void)
{
    cprTimerBlk *new_timer;

    cprDisableSwap();

    new_timer = (cprTimerBlk *) cpr_malloc(sizeof(cprTimerBlk));
    if (new_timer == NULL) {
        /* Malloc error */
        CPR_ERROR("!timer_event_allocate:  Could not malloc a new timer\n");
    } else {
        new_timer->flags &= ~TIMER_FREE;
    }

    cprEnableSwap();

    return (new_timer);
}


/*------------------------------------------------------------------------------
 *  NAME:                timer_insert(*timer)
 *
 *  PARAMETERS:
 *   timer - Timer to be inserted into timer event list.
 *           Must be initialized and have timeout value set.
 *
 *  RETURNS:
 *   void
 *
 *  DESCRIPTION:
 *   Insert timer entry into doubly linked timer list sorted in
 *   ascending order by timeout expiration.
 *
 *----------------------------------------------------------------------------*/
static void
timer_insert (cprTimerBlk * timer)
{

    cprTimerBlk *element; /* Timer element new timer is inserted in front of */
    cprTimerBlk *pred;    /* Timer element new timer is inserted behind */
    int insert_at_front;  /* Indicates new timer inserted in front of list */

    cprDisableSwap();

    if (cprTimerPendingList == NULL) {
        insert_at_front = 1;
    } else if (timer->expiration_time <= cprTimerPendingList->expiration_time) {
        insert_at_front = 1;
    } else {
        insert_at_front = 0;
    }

    if (insert_at_front) {
        /* Insert timer into front of timer list */
        if (cprTimerPendingList != NULL) {
            cprTimerPendingList->previous = timer;
        }
        timer->next = cprTimerPendingList;
        timer->previous = NULL;
        cprTimerPendingList = timer;
    } else {
        /* Traverse list looking for place to insert */
        pred = cprTimerPendingList;
        element = cprTimerPendingList->next;

        /* Find position for timer insert */
        while (element != NULL) {
            if (timer->expiration_time <= element->expiration_time) {
                /* Found insertion point */
                break;
            } else {
                /* Check next element */
                pred = element;
                element = element->next;
            }
        }

        /* Insert timer in front of element */
        timer->previous = pred;
        timer->next = pred->next;
        pred->next = timer;

        if (element != NULL) {
            element->previous = timer;
        }
    }

    cprEnableSwap();
}



/*------------------------------------------------------------------------------
 *  NAME:        timer_remove(*timer)
 *
 *  PARAMETERS:
 *   timer - Timer to be removed from active timer list.
 *
 *  RETURNS:
 *   void
 *
 *  DESCRIPTION:
 *   Removes specified timer from the doubly linked
 *   timer list to cancel timer.
 *
 *----------------------------------------------------------------------------*/
static void
timer_remove (cprTimerBlk *timer)
{

    cprDisableSwap();
    if (cprTimerPendingList) {
        if (cprTimerPendingList == timer) {
            /* Element is at front of list */
            cprTimerPendingList = timer->next;
            if (timer->next != NULL) {
                timer->next->previous = NULL;
            }
        } else {
            /* Timer is in the middle of the list */
            timer->previous->next = timer->next;
            if (timer->next != NULL) {
                timer->next->previous = timer->previous;
            }
        }
        timer->next = NULL;
        timer->previous = NULL;
    }
    cprEnableSwap();
}



/**
 * cprCreateTimer
 *
 * Initialize a timer
 *
 * Parameters: name               - name of the timer
 *             applicationTimerId - ID for this timer from the application's
 *                                  perspective
 *             applicationMsgId   - ID for syshdr->cmd when timer expire msg
 *                                  is sent
 *             callbackMsgQueue   - where to send a msg when this timer expires
 *
 * Return Value: Timer handle or NULL if creation failed.
 */
cprTimer_t
cprCreateTimer (const char *name,
                uint16_t applicationTimerId,
                uint16_t applicationMsgId,
                cprMsgQueue_t callBackMsgQueue)
{

    cprTimerBlk *cprTimerPtr = NULL;

    if ((cprTimerPtr = (cprTimerBlk*)timer_event_allocate()) != NULL) {
        if (name != NULL) {
            cprTimerPtr->name = name;
        }
        /* Set timer id, msg id and callback msg queue (Mandatory) */
        cprTimerPtr->applicationTimerId = applicationTimerId;
        cprTimerPtr->applicationMsgId = applicationMsgId;
        if (callBackMsgQueue == NULL) {
            CPR_ERROR("Callback msg queue for timer %s is NULL.\n", name);
            return (NULL);
        }
        cprTimerPtr->callBackMsgQueue = callBackMsgQueue;
        cprTimerPtr->flags |= TIMER_INITIALIZED;
    } else {
        CPR_ERROR("Failed to CreateTimer.\n");
    }
    return cprTimerPtr;
}


/**
 * cprStartTimer
 *
 * Start a system timer
 *
 * Parameters: timer     - which timer to start
 *             duration  - how long before timer expires in milliseconds
 *             data      - information to be passed to callback function
 *
 * Return Value: Success or failure indication
 */
cprRC_t
cprStartTimer (cprTimer_t timer, uint32_t duration, void *data)
{
    static const char fname[] = "cprStartTimer";

    cprTimerBlk *cprTimerPtr = (cprTimerBlk*)timer;

    cprTimerPtr = (cprTimerBlk *) timer;
    if (cprTimerPtr != NULL) {
        if (cprTimerPtr->flags & TIMER_FREE) {
            CPR_ERROR("!timer_event_activate: %x %s\n", timer,
                      "Attempting to activate free timer\n");
        } else if (cprTimerPtr->flags & TIMER_ACTIVE) {
            CPR_ERROR("!timer_event_activate: %x %s\n", timer,
                      "Attempting to activate active timer");
        } else if ((cprTimerPtr->flags & TIMER_INITIALIZED) == 0) {
            CPR_ERROR("!timer_event_activate: %x %s\n", timer,
                      "Attempting to activate uninitialized timer");
        } else {
            /* Store information in CPR timer structure */
            cprTimerPtr->data = data;
            cprTimerPtr->expiration_time = current_time() + duration;

            /* Start Timer */
            timer_insert(cprTimerPtr);
            cprTimerPtr->flags |= TIMER_ACTIVE;
        }
        /* Bad application! */
    } else {
        CPR_ERROR("%s - NULL pointer passed in.\n", fname);
        return (CPR_FAILURE);
    }
    return (CPR_SUCCESS);
}



/**
 * cprIsTimerRunning
 *
 * Determine if a timer is active
 *
 * Parameters: timer - which timer to check
 *
 * Return Value: True is timer is active, false otherwise
 */
boolean
cprIsTimerRunning (cprTimer_t timer)
{
    static const char fname[] = "cprIsTimerRunning";
    cprTimerBlk *cprTimerPtr = (cprTimerBlk*)timer;

    if (cprTimerPtr != NULL) {
        return ((uint8_t) (cprTimerPtr->flags & TIMER_ACTIVE));

        /* Bad application! */
    } else {
        CPR_ERROR("%s - NULL pointer passed in.\n", fname);
        return (FALSE);
    }
}


/**
 * cprCancelTimer
 *
 * Cancels a running timer
 *
 * Parameters: index - which timer to cancel
 *
 * Return Value: Success or Failure indication
 */
cprRC_t
cprCancelTimer (cprTimer_t timer)
{
    static const char fname[] = "cprCancelTimer";
    cprTimerBlk *cprTimerPtr = (cprTimerBlk *) timer;

    if (cprTimerPtr != NULL) {
        if (cprTimerPtr->flags & TIMER_ACTIVE) {
            /* Timer is active */
            timer_remove((cprTimerBlk*)timer);
            cprTimerPtr->flags &= ~TIMER_ACTIVE;
        } else {
            /* Timer is not active. Ignore request to cancel it */
        }
        removed_count++;
        return (CPR_SUCCESS);
        /* Bad application! */
    } else {
        CPR_ERROR("%s - NULL pointer passed in.\n", fname);
        return (CPR_FAILURE);
    }
}


/**
 * cprUpdateTimer
 *
 * Updates the expiration time for a running timer
 *
 * Parameters: timer     - which timer to update
 *             duration  - how long before timer expires in milliseconds
 *
 * Return Value: Success or failure indication
 */
cprRC_t
cprUpdateTimer (cprTimer_t timer, uint32_t duration)
{
    void *timerData;
    static const char fname[] = "cprUpdateTimer";
    cprTimerBlk *cprTimerPtr = (cprTimerBlk*)timer;

    if (cprTimerPtr != NULL) {
        /* Grab data before cancelling timer */
        timerData = cprTimerPtr->data;
    } else {
        CPR_ERROR("%s - NULL pointer passed in.\n", fname);
        return (CPR_FAILURE);
    }

    if (cprCancelTimer(timer) == CPR_SUCCESS) {
        if (cprStartTimer(timer, duration, timerData) == CPR_SUCCESS) {
            return (CPR_SUCCESS);
        } else {
            CPR_ERROR("%s - Failed to start timer %s\n", fname,
                      cprTimerPtr->name);
            return (CPR_FAILURE);
        }
    }

    CPR_ERROR("%s - Failed to cancel timer %s\n", fname,
              cprTimerPtr->name);
    return (CPR_FAILURE);
}


/**
 * cprDestroyTimer
 *
 * Destroys a timer. Set all links to NULL
 * and then frees the timer block.
 *
 * Parameters: timer - which timer to destroy
 *
 * Return Value: Success or Failure indication
 */
cprRC_t
cprDestroyTimer (cprTimer_t timer)
{
    static const char fname[] = "cprDestroyTimer";
    cprTimerBlk *cprTimerPtr = (cprTimerBlk *) timer;
    cprRC_t returnCode = CPR_FAILURE;

    cprDisableSwap();

    if (cprTimerPtr != NULL) {
        if (cprTimerPtr->flags & TIMER_FREE) {
            CPR_ERROR("!timer_event_free: %x %s\n", timer,
                      "Attempting to free timer that is already free");
        } else if (cprTimerPtr->flags & TIMER_ACTIVE) {
            CPR_ERROR("!timer_event_free: %x %s\n", timer,
                      "Attempting to free active timer");
        } else {
            /* Timer is ok to free. */
            cprTimerPtr->flags = TIMER_FREE;
            cprTimerPtr->next = NULL;
            cpr_free(cprTimerPtr);
            returnCode = CPR_SUCCESS;
        }

        /* Bad application! */
    } else {
        CPR_ERROR("%s - NULL pointer passed in.\n", fname);
    }

    cprEnableSwap();
    return (returnCode);
}


/**
 * cpr_timer_expired
 *
 * Destroys a timer. Set all links to NULL
 * and then frees the timer block.
 *
 * Parameters: none
 *
 * Return Value: bool indicating there is an expired timer
 */
boolean
cpr_timer_expired (void)
{
    unsigned int now;           /* Current time */
    cprTimerBlk *cprTimerPtr;   /* User to traverse timer list */

    cprDisableSwap();

    now = current_time();

    cprTimerPtr = cprTimerPendingList;

    while (cprTimerPtr != NULL) {
        if (cprTimerPtr->expiration_time <= now) {
            cprEnableSwap();
            return TRUE;
        } else {
            /* No more expired timers. Bail */
            break;
        }
    }
    cprEnableSwap();
    return FALSE;
}



/*------------------------------------------------------------------------------
 *  NAME:        cpr_timer_event_process()
 *
 *  PARAMETERS:
 *   None.
 *
 *  RETURNS:
 *   void
 *
 *  DESCRIPTION:
 *   This is called to process the timer event list.  The current time
 *   is compared with the sorted list of entries in the timer list.
 *   When a timer is found to have expired, it is removed from the list
 *   and its expiration routine is called.
 *
 *----------------------------------------------------------------------------*/
void
cpr_timer_event_process (void)
{
    unsigned int now;           /* Current time */
    cprTimerBlk *timer;         /* User to traverse timer list */
    cprCallBackTimerMsg_t *timerMsg;
    void *syshdr;

    cprDisableSwap();

    now = current_time();

    timer = cprTimerPendingList;

    while (timer != NULL) {
        if (timer->expiration_time <= now) {

            /* Timer has expired. */

            timer_remove(timer);    /* Remove from active list */
            timer->flags &= ~TIMER_ACTIVE;

            expired_count++;    /* Debug counter */

            cprEnableSwap();

            /* Send msg to queue to indicate this timer has expired */
            timerMsg = (cprCallBackTimerMsg_t *)
                    cpr_malloc(sizeof (cprCallBackTimerMsg_t));
            if (timerMsg) {
                timerMsg->expiredTimerName = timer->name;
                timerMsg->expiredTimerId = timer->applicationTimerId;
                timerMsg->usrData = timer->data;
                syshdr = cprGetSysHeader(timerMsg);
                if (syshdr) {
                    fillInSysHeader(syshdr, timer->applicationMsgId,
                                    sizeof(cprCallBackTimerMsg_t), timerMsg);
                    if (cprSendMessage(timer->callBackMsgQueue,
                            timerMsg, (void **) &syshdr) == CPR_FAILURE) {
                        cprReleaseSysHeader(syshdr);
                        cpr_free(timerMsg);
                        CPR_ERROR("Call to cprSendMessage failed.\n");
                        CPR_ERROR("Unable to send timer %s expiration msg.\n",
                                  timer->name);
                    }
                } else {
                    cpr_free(timerMsg);
                    CPR_ERROR("Call to cprGetSysHeader failed.\n");
                    CPR_ERROR("Unable to send timer %s expiration msg.\n",
                              timer->name);
                }
            } else {
                CPR_ERROR("Call to cpr_malloc failed.\n");
                CPR_ERROR("Unable to send timer %s expiration msg.\n",
                          timer->name);
            }

            cprDisableSwap();

            /* Timer expiration function may have manipulated the
             * timer list.  Need to start over at front of timer list looking
             * for expired timers.
             */
            timer = cprTimerPendingList;    /* Start at front of list */
        } else {
            /* No more expired timers. Bail */
            break;
        }
    }
    cprEnableSwap();
}



/*------------------------------------------------------------------------------
 *  NAME:        cpr_timer_tick()
 *
 *  PARAMETERS:
 *   None.
 *
 *  RETURNS:
 *   None
 *
 *  DESCRIPTION:
 *   This routine is called by the timer interrupt routine which on windows
 *   is every 20ms
 *
 *----------------------------------------------------------------------------*/
void
cpr_timer_tick (void)
{

    /* windows is driving this at a 20ms rate */
    ticker += 20;
    if (cpr_timer_init) {
        /* Only kick the timer task if there is a timer which
         * has expired and ready to be processed.
         */
        if (cpr_timer_expired()) {
            /* Timer task is initialized and we have an expired timer...Kick it. */
            cprSendMessage(tmr_msgq, tmr_msg, NULL);
        }
    }
}


/*------------------------------------------------------------------------------
 *  NAME:        current_time()
 *
 *  PARAMETERS:
 *   None.
 *
 *  RETURNS:
 *   timer counter
 *
 *  DESCRIPTION:
 *   This routine returns a monotonically increasing timer counter
 *
 *----------------------------------------------------------------------------*/
unsigned long
current_time (void)
{
    return (ticker);
}


/*------------------------------------------------------------------------------
 *  NAME:        CPRTMRTask()
 *
 *  PARAMETERS:
 *   None.
 *
 *  RETURNS:
 *   SUCCESS
 *
 *  DESCRIPTION:
 *   This is the windows version of the CPR timer task
 *
 *----------------------------------------------------------------------------*/
int32_t
CPRTMRTask (void *data)
{
    void *msg;

    while (cpr_timer_init) {
        /* Wait for tick */
        msg = cprGetMessage(tmr_msgq, TRUE, NULL);
        if (msg) {
            /*
             * No need to free the msg here as the only msg
             * sent to the timer task is a static string to
             * wake it up.
             */
            cpr_timer_event_process();
        }
    }
    return (CPR_SUCCESS);
}




/*------------------------------------------------------------------------------
 *  NAME:        cprTimerSystemInit()
 *
 *  PARAMETERS:
 *   None.
 *
 *  RETURNS:
 *   void
 *
 *  DESCRIPTION:
 *   Initialize timer system.  Initialize lists, call
 *   routine to initialize timer interrupt  ticker.
 *
 *----------------------------------------------------------------------------*/
void
cprTimerSystemInit (void)
{
    /* Create timer lists and free buffer pool */
    cprTimerPendingList = NULL;

    tmr_msgq = cprCreateMessageQueue("TMRQ", 0);

    expired_count = 0;
    removed_count = 0;
    inserted_count = 0;

    ticker = 0;
    cpr_timer_init = 1;

    tmr_thread = cprCreateThread("CprTmrTsk", (cprThreadStartRoutine)CPRTMRTask,
                                 CPRTMR_STK, CPRTMR_PRIO, NULL);
    if (!tmr_thread) {
        CPR_ERROR("Failed to create CPR Timer Task");
    }
    cprSetMessageQueueThread(tmr_msgq, tmr_thread);

    //    bind_show_keyword("timers", display_active_timers);
}


void
cpr_timer_stop (void)
{
    cpr_timer_init = 0;
    /* Kick timer to wake it up */
    cprSendMessage(tmr_msgq, tmr_msg, NULL);
    cprDestroyThread(tmr_thread);
}
