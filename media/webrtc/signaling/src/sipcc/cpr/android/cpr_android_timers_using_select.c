/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  @brief CPR layer for Timers.
 *
 *     This file contains the Cisco Portable Runtime layer for non-blocking
 *     timers. This implementation is for the Linux operating system using
 *     select with a timeout.
 *
 *     Timer Service runs in its own thread and blocks on select
 *     call with a timeout. The value of timeout is set equal to
 *     the duration of the earliest expiring timer. The timer list
 *     is kept sorted by earliest to latest expiration times. Therefore,
 *     timeout value is simply the duration on the head of the list.
 *
 *     For starting or cancelling a timer, the timer library contacts
 *     the timer service using a local socket based IPC. This is done
 *     by bringing up a connection between timer service and the client
 *     during timer init.
 *     Timer Service thread is the only thread that adds or removes
 *     timer blocks from the list. When timer library client wants
 *     to start or cancel a timer, the library sends an IPC message
 *     to the timer service thread on the socket connection. This
 *     unblocks the select call. Select also returns the amount of
 *     time left on the timeout when it gets unblocked. This amount is
 *     first subtracted from the head timer block duration. Then rest
 *     of the processing is carried out based on whether the request
 *     was to add or to remove a timer block and duration
 *     on all timer blocks is adjusted as required.
 *     When the select times out it implies the timers at the head
 *     of the list has expired. The list is scanned for expired timers
 *     and expiry processing such as posting message to the handler
 *     etc. is done.
 *
 *     When there are no timers in the list, service blocks on select
 *     forever waiting for at least one timer to be added.
 *
 */

/**
 * @defgroup Timers The Timer implementation module
 * @ingroup CPR
 * @brief The module related to Timer abstraction for the pSIPCC
 * @addtogroup TimerAPIs The Timer APIs
 * @ingroup Timers
 * @brief APIs expected by pSIPCC for using Timers
 *
 */

#include "cpr.h"
#include "cpr_assert.h"
#include "cpr_socket.h"
#include "cpr_stdlib.h"
#include "cpr_stdio.h"
#include "cpr_threads.h"
#include "cpr_timers.h"
#include "cpr_string.h"
#include "phntask.h"
#include <errno.h>
#include <unistd.h>
#include "cpr_android_timers.h"
#include "platform_api.h"

/*--------------------------------------------------------------------------
 * Local definitions
 *--------------------------------------------------------------------------
 */

typedef struct timer_ipc_cmd_s
{
    cpr_timer_t *timer_ptr;
    void        *user_data_ptr;
    uint32_t    duration;
} timer_ipc_cmd_t;


typedef struct timer_ipc_s
{
    uint32_t  msg_type;
    union
    {
        timer_ipc_cmd_t cmd;
        cprRC_t         result;
    }u;
} timer_ipc_t;

#define TMR_CMD_ADD    1
#define TMR_CMD_REMOVE 2
#define TMR_RESULT     3

#define API_RETURN(_val) \
    {                     \
        pthread_mutex_unlock(&api_mutex); \
        return (_val); \
     }\

#define API_ENTER() \
{\
    pthread_mutex_lock(&api_mutex);\
}\

/* for AF_LOCAL not all implementations return client addr in recvfrom so
 * using explicit path for client too.
 */
#define SERVER_PATH "/tmp/CprTmrServer"
#define CLIENT_PATH "/tmp/CprTmrClient"


/*--------------------------------------------------------------------------
 * Global data
 *--------------------------------------------------------------------------
 */


static timerBlk *timerListHead;

static pthread_t timerThreadId;


static pthread_mutex_t api_mutex;


/* local socket used by timer library */
static int client_sock = INVALID_SOCKET;


/* local socket used by the timer  service */
static int serv_sock = INVALID_SOCKET;

static struct sockaddr_un tmr_serv_addr;
static struct sockaddr_un tmr_client_addr;

static fd_set socks; /* descriptor set */


/*--------------------------------------------------------------------------
 * External data references
 *--------------------------------------------------------------------------
 */


/*--------------------------------------------------------------------------
 * External function prototypes
 *--------------------------------------------------------------------------
 */

/*
 * Internal CPR function to fill in data in the sysheader.
 * This is to prevent knowledge of the evil syshdr structure
 * from spreading to cpr_linux_timers.c This thing is
 * like kudzu...
 */
extern void fillInSysHeader(void *buffer, uint16_t cmd, uint16_t len,
                            void *timerMsg);



/*--------------------------------------------------------------------------
 * Local scope function prototypes
 *--------------------------------------------------------------------------
 */

static void     *timerThread(void *data);
static cprRC_t  start_timer_service_loop();
static void     process_expired_timers();
static void     send_api_result(cprRC_t result, struct sockaddr_un *addr, socklen_t len);


/**
 * @addtogroup TimerAPIs The Timer APIs
 * @ingroup Timers
 * @{
 */

/**
 * cprSleep
 *
 * @brief Suspend the calling thread
 * The cprSleep function blocks the calling thread for the indicated number of
 * milliseconds.
 *
 * @param[in] duration - Number of milliseconds the thread should sleep
 *
 * @return -  none
 */
void
cprSleep (uint32_t duration)
{
    /*
     * usleep() can only support up to one second, so split
     * between sleep and usleep if one second or more
     */
    if (duration >= 1000) {
        (void) sleep(duration / 1000);
        (void) usleep((duration % 1000) * 1000);
    } else {
        (void) usleep(duration * 1000);
    }
}

/**
  * @}
  */

/**
 * @defgroup TimerInternal The Timer internal functions
 * @ingroup Timers
 * @{
 */

/**
 * addTimerToList
 * Send message to timer service to add the timer pointed by cprTimerPtr
 * to the list. This routine is just sending IPC message to timer service
 * but the actual addition is done by timer service using the addTimer function.
 * This function is only called by CPR functions and is not visible to external
 * applications.
 * @param[in] cprTimerPtr  - timer pointer
 * @param[in] duration     - timer duration in msec.
 * @param[in] data         - opaque data
 * @return  - CPR_SUCCESS or CPR_FAILURE
 */
static cprRC_t addTimerToList (cpr_timer_t *cprTimerPtr, uint32_t duration, void *data)
{
#ifdef CPR_TIMERS_ENABLED
    timer_ipc_t tmr_cmd = {0};
    timer_ipc_t tmr_rsp={0};

    API_ENTER();

    //CPR_INFO("%s: cprTimerptr=0x%x dur=%d user_data=%x\n",
    //       __FUNCTION__, cprTimerPtr, duration, data);
    tmr_cmd.msg_type = TMR_CMD_ADD;
    tmr_cmd.u.cmd.timer_ptr = cprTimerPtr;
    tmr_cmd.u.cmd.user_data_ptr = data;
    tmr_cmd.u.cmd.duration = duration;

//CPR_INFO("%s:sending messge of type=%d\n", __FUNCTION__, tmr_cmd.msg_type);
    /* simply post a request here to the timer service.*/
    if (client_sock != -1) {
        if (sendto(client_sock, &tmr_cmd, sizeof(timer_ipc_t), 0,
                   (struct sockaddr *)&tmr_serv_addr, sizeof(tmr_serv_addr)) < 0) {
            CPR_ERROR("Failed to tx IPC msg to timer service, errno = %s %s\n",
                   strerror(errno), __FUNCTION__);
            API_RETURN(CPR_FAILURE);
        }

    } else {
        CPR_ERROR("can not make IPC connection, client_sock is invalid %s\n", __FUNCTION__);
        API_RETURN(CPR_FAILURE);
    }

    /*
     * wait for the timer service to excute the request
     * so that we get result of operation
     */

    if (recvfrom(client_sock, &tmr_rsp, sizeof(timer_ipc_t),0, NULL, NULL) < 0) {
        //CPR_INFO("error in recving the result error=%s\n", strerror(errno));
        API_RETURN(CPR_FAILURE);
    } else {
        //CPR_INFO("received response from the timer result=%d\n", tmr_rsp.u.result);
        API_RETURN(tmr_rsp.u.result);
    }
#else
    cprAssert(FALSE, CPR_FAILURE);
    CPR_ERROR("CPR Timers are disabled! %s\n", __FUNCTION__);
    return CPR_SUCCESS;
#endif
}


/**
 * addTimer
 *
 * Add a timer to the timer linked list.
 * This function is only called by CPR functions and is not visible to external
 * applications.
 *
 * @param[in] cprTimerPtr - pointer to the CPR timer structure
 * @param[in] duration    - how long before timer expires in milliseconds
 * @param[in] data        - information to be passed to callback function
 *
 * @return  - CPR_SUCCESS or CPR_FAILURE
 */
static cprRC_t addTimer (cpr_timer_t *cprTimerPtr, uint32_t duration, void *data)
{
    static const char fname[] = "addTimer";
    timerBlk *timerList;
    timerBlk *newTimerPtr;

    CPR_INFO("%s:adding timer=0x%x timerblk=%x\n", fname,
           cprTimerPtr, cprTimerPtr->u.handlePtr);


    /* Verify the timer has been initialized */
    newTimerPtr = (timerBlk *) cprTimerPtr->u.handlePtr;
    if (newTimerPtr == NULL) {
        CPR_ERROR("%s - Timer %s has not been initialized.\n",
                  fname, cprTimerPtr->name);
        errno = EINVAL;

        return(CPR_FAILURE);
    }

    /* Ensure this timer is not already running */
    if (newTimerPtr->timerActive) {
        CPR_ERROR("%s - Timer %s is already active.\n", fname, cprTimerPtr->name);
        errno = EAGAIN;
        return(CPR_FAILURE);

    }

    /* Sanity tests passed, store the data the application passed in */
    newTimerPtr->duration = duration;
    cprTimerPtr->data = data;

    /*
     * Insert timer into the linked list. The timer code only
     * decrements the first timer in the list to be efficient.
     * Therefore, the timer at the top of the list is the timer
     * that will expire first. Timers are added to the list
     * in ascending order of time left before expiration and
     * ticksLeft is calculated to be the difference between
     * when the timer before them expires and when the newly
     * inserted timer expires.
     */

    /* Check for insertion into an empty list */
    if (timerListHead == NULL) {
        //CPR_INFO("no timer in the list case..\n");
        timerListHead = newTimerPtr;
    } else {

	/* Insert timer into list */
        timerList = timerListHead;
        while (timerList != NULL) {

            /*
             * If the duration on this new timer are less than the
             * timer in the list, insert this new timer before
             * it in the list as it will expire first. In doing so
             * the code must subtract the new timer's duration from
             * the duration of the timer in the in the list to keep the deltas correct.
             */
            if (newTimerPtr->duration < timerList->duration) {
                //CPR_INFO("less than case..\n");
                timerList->duration -= newTimerPtr->duration;
                newTimerPtr->next = timerList;
                newTimerPtr->previous = timerList->previous;
                if (newTimerPtr->previous) {
                    newTimerPtr->previous->next = newTimerPtr;
                }
                timerList->previous = newTimerPtr;

                /* Check for insertion at the head of list */
                if (timerListHead == timerList) {
                    //CPR_INFO("insert at the head case..\n");
                    timerListHead = newTimerPtr;
                }
                break;
            } else {
                /*
                 * Else this new timer expires after the timer in
                 * the list. Therefore subtract the timer's duration
                 * from the new timer's duration. Since only the
                 * first timer is decremented all other timers added
                 * expiration must be calculated as a delta from the
                 * timer in front of them in the list.
                 */
                //CPR_INFO("greater than case..\n");
                newTimerPtr->duration -= timerList->duration;

                /* Check for insertion at the end of list */
                if (timerList->next == NULL) {
                    //CPR_INFO("insert at the end sub case..\n");
                    newTimerPtr->previous = timerList;
                    timerList->next = newTimerPtr;
                    newTimerPtr->next = NULL;
                    break;
                }
                timerList = timerList->next;
            }
        }
    }

    newTimerPtr->timerActive = TRUE;
    return(CPR_SUCCESS);
}


/**
 * removeTimerFromList
 * Send message to timer service to remove the timer pointed by cprTimerPtr
 * from the list. This routine is just sending IPC message to timer service
 * and the actual removal is done by timer service using the removeTimer function..
 * This function is only called by CPR functions and is not visible to external
 * applications.
 *
 * @param[in] cprTimerPtr - pointer to the timer to be removed from the list
 * @return - CPR_SUCCESS or CPR_FAILURE
 *
 */
static cprRC_t
removeTimerFromList (cpr_timer_t *cprTimerPtr)
{

    static const char fname[] = "removeTimerFromList";
    timer_ipc_t tmr_cmd = {0};
    timer_ipc_t tmr_rsp = {0};


    API_ENTER();

    //CPR_INFO("%s:remove timer from list=0x%x\n",fname, cprTimerPtr);
    tmr_cmd.msg_type = TMR_CMD_REMOVE;
    tmr_cmd.u.cmd.timer_ptr = cprTimerPtr;

    //CPR_INFO("sending messge of type=%d\n", tmr_cmd.msg_type);

    /* simply post a request here to the timer service.. */
    if (client_sock != -1) {
        if (sendto(client_sock, &tmr_cmd, sizeof(timer_ipc_t), 0,
                   (struct sockaddr *)&tmr_serv_addr, sizeof(tmr_serv_addr)) < 0) {
            CPR_ERROR("%s:failed to tx IPC Msg to timer service, errno = %s\n",
                      fname, strerror(errno));
            API_RETURN(CPR_FAILURE);
        }
    } else {
        CPR_ERROR("%s:client_sock invalid, no IPC connection \n", fname);
        API_RETURN(CPR_FAILURE);
    }

    /*
     * wait for the timer service to excute the request
     * so that we get result of operation
     */

    if (recvfrom(client_sock, &tmr_rsp, sizeof(timer_ipc_t),0, NULL, NULL) < 0) {
        //CPR_INFO("error in recving the result error=%s\n", strerror(errno));
        API_RETURN(CPR_FAILURE);
    } else {
        //CPR_INFO("received response from the timer result=%d\n", tmr_rsp.u.result);
        API_RETURN(tmr_rsp.u.result);
    }
}


/**
 * removeTimer
 *
 * Remove a timer from the timer linked list. This function is only
 * called by CPR functions and is not visible to applications.
 *
 * @param[in] cprTimerPtr - which timer to cancel
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 */
static cprRC_t
removeTimer (cpr_timer_t *cprTimerPtr)
{
    static const char fname[] = "removeTimer";
    timerBlk *timerList;
    timerBlk *previousTimer;
    timerBlk *nextTimer;
    timerBlk *timerPtr;

    //CPR_INFO("removing timer..0x%x\n", cprTimerPtr);

    /*
     * No need to sanitize the cprTimerPtr data as only
     * internal CPR functions call us and they have already
     * sanitized that data. In addition those functions
     * have already grabbed the timer list mutex so no need
     * to do that here.
     */
    timerPtr = (timerBlk *) cprTimerPtr->u.handlePtr;
    //CPR_INFO("%s: timer ptr=%x\n", fname, timerPtr);

    if (timerPtr != NULL) {
        /* Walk the list looking for this timer to cancel. */
        timerList = timerListHead;
        while (timerList != NULL) {
            if (timerList->cprTimerPtr->cprTimerId == cprTimerPtr->cprTimerId) {
                /* Removing only element in the list */
                if ((timerList->previous == NULL) &&
                    (timerList->next == NULL)) {
                    timerListHead = NULL;

                /* Removing head of the list */
                } else if (timerList->previous == NULL) {
                    nextTimer = timerList->next;
                    nextTimer->previous = NULL;
                    timerListHead = nextTimer;

                /* Removing tail of the list */
                } else if (timerList->next == NULL) {
                    previousTimer = timerList->previous;
                    previousTimer->next = NULL;

                /* Removing from middle of the list */
                } else {
                    nextTimer = timerList->next;
                    previousTimer = timerList->previous;
                    previousTimer->next = nextTimer;
                    nextTimer->previous = previousTimer;
                }

                /* Add time back to next timer in the list */
                if (timerList->next) {
                    timerList->next->duration += timerList->duration;
                }

                /*
                 * Reset timer values
                 */
                timerList->next = NULL;
                timerList->previous = NULL;
                timerList->duration = -1;
                timerList->timerActive = FALSE;
                cprTimerPtr->data = NULL;

                return(CPR_SUCCESS);

            }

            /* Walk the list */
            timerList = timerList->next;
        }

        /*
         * Either the timer was not active or it was marked active, but not
         * found on the timer list. If the timer was inactive then that is OK,
         * but let user know about an active timer not found in the timer list.
         */
        if ((timerPtr->next != NULL) || (timerPtr->previous != NULL)) {
            CPR_ERROR("%s - Timer %s marked as active, "
                      "but was not found on the timer list.\n",
                      fname, cprTimerPtr->name);
            timerPtr->next = NULL;
            timerPtr->previous = NULL;
        }
        timerPtr->duration = -1;
        timerPtr->cprTimerPtr->data = NULL;
        timerPtr->timerActive = FALSE;

        return(CPR_SUCCESS);

    }

    /* Bad application! */
    CPR_ERROR("%s - Timer not initialized.\n", fname);
    errno = EINVAL;
    return(CPR_FAILURE);

}

/**
 * @}
 * @addtogroup TimerAPIs The Timer APIs
 * @ingroup Timers
 * @{
 */

/**
 * cprCreateTimer
 *
 * @brief Initialize a timer
 *
 * The cprCreateTimer function is called to allow the OS to perform whatever
 * work is needed to create a timer. The input name parameter is optional. If present, CPR assigns
 * this name to the timer to assist in debugging. The callbackMsgQueue is the
 * address of a message queue created with cprCreateMsgQueue. This is the
 * queue where the timer expire message will be sent.
 * So, when this timer expires a msg of type "applicationMsgId" will be sent to the msg queue
 * "callbackMsgQueue" indicating that timer applicationTimerId has expired.
 *
 * @param[in]   name               -  name of the timer
 * @param[in]   applicationTimerId - ID for this timer from the application's
 *                                  perspective
 * @param[in]   applicationMsgId   - ID for syshdr->cmd when timer expire msg
 *                                  is sent
 * @param[in]   callBackMsgQueue   - where to send a msg when this timer expires
 *
 * @return  Timer handle or NULL if creation failed.
 */
cprTimer_t
cprCreateTimer (const char *name,
                uint16_t applicationTimerId,
                uint16_t applicationMsgId,
                cprMsgQueue_t callBackMsgQueue)
{
    static const char fname[] = "cprCreateTimer";
    static uint32_t cprTimerId = 0;
    cpr_timer_t *cprTimerPtr;
    timerBlk *timerPtr;

    /*
     * Malloc memory for a new timer. Need to
     * malloc memory for the generic CPR view and
     * one for the CNU specific version.
     */
    cprTimerPtr = (cpr_timer_t *) cpr_malloc(sizeof(cpr_timer_t));
    timerPtr = (timerBlk *) cpr_malloc(sizeof(timerBlk));
    if ((cprTimerPtr != NULL) && (timerPtr != NULL)) {
        /* Assign name (Optional) */
        cprTimerPtr->name = name;

        /* Set timer ids, msg id and callback msg queue (Mandatory) */
        cprTimerPtr->applicationTimerId = applicationTimerId;
        cprTimerPtr->applicationMsgId = applicationMsgId;
        cprTimerPtr->cprTimerId = cprTimerId++;
        if (callBackMsgQueue == NULL) {
            CPR_ERROR("%s - Callback msg queue for timer %s is NULL.\n",
                      fname, name);
            cpr_free(timerPtr);
            cpr_free(cprTimerPtr);
            return NULL;
        }
        cprTimerPtr->callBackMsgQueue = callBackMsgQueue;

        /*
         * Set remaining values in both structures to defaults
         */
        timerPtr->next = NULL;
        timerPtr->previous = NULL;
        timerPtr->duration = -1;
        timerPtr->timerActive = FALSE;
        cprTimerPtr->data = NULL;

        /*
         * TODO - It would be nice for CPR to keep a linked
         * list of active timers for debugging purposes
         * such as a show command or walking the list to ensure
         * that an application does not attempt to create
         * the same timer twice.
         *
         * TODO - It would be nice to initialize of pool of
         * timers at init time and have this function just
         * return a timer from the pool. Then when the
         * timer expired or cancel the code would not free
         * it, but just return it to the pool.
         */
        timerPtr->cprTimerPtr = cprTimerPtr;
        cprTimerPtr->u.handlePtr = timerPtr;
        //CPR_INFO("cprTimerCreate: timer_t=%x blk=%x\n",cprTimerPtr, timerPtr);

        return cprTimerPtr;
    }

    /*
     * If we get here there has been a malloc failure.
     */
    if (timerPtr) {
        cpr_free(timerPtr);
    }
    if (cprTimerPtr) {
        cpr_free(cprTimerPtr);
    }

    /* Malloc failed */
    CPR_ERROR("%s - Malloc for timer %s failed.\n", fname, name);
    errno = ENOMEM;
    return NULL;
}


/**
 * cprStartTimer
 *
 * @brief Start a system timer
 *
 * The cprStartTimer function starts a previously created timer referenced by
 * the parameter timer. CPR timer granularity is 10ms. The "timer" input
 * parameter is the handle returned from a previous successful call to
 * cprCreateTimer.
 *
 * @param[in]  timer    - which timer to start
 * @param[in]  duration - how long before timer expires in milliseconds
 * @param[in]  data     - information to be passed to callback function
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 */
cprRC_t
cprStartTimer (cprTimer_t timer,
               uint32_t duration,
               void *data)
{
    static const char fname[] = "cprStartTimer";
    cpr_timer_t *cprTimerPtr;

    cprTimerPtr = (cpr_timer_t *) timer;
    if (cprTimerPtr != NULL) {
        /* add timer to the list */
        return addTimerToList(cprTimerPtr, duration, data);
    }

    /* Bad application! */
    CPR_ERROR("%s - NULL pointer passed in.\n", fname);
    errno = EINVAL;
    return CPR_FAILURE;
}


/**
 * cprIsTimerRunning
 *
 * @brief Determine if a timer is active
 *
 * This function determines whether the passed in timer is currently active. The
 * "timer" parameter is the handle returned from a previous successful call to
 *  cprCreateTimer.
 *
 * @param[in] timer - which timer to check
 *
 * @return True is timer is active, False otherwise
 */
boolean
cprIsTimerRunning (cprTimer_t timer)
{
    static const char fname[] = "cprIsTimerRunning";
    cpr_timer_t *cprTimerPtr;
    timerBlk *timerPtr;

    //CPR_INFO("istimerrunning(): timer=0x%x\n", timer);

    cprTimerPtr = (cpr_timer_t *) timer;
    if (cprTimerPtr != NULL) {
        timerPtr = (timerBlk *) cprTimerPtr->u.handlePtr;
        if (timerPtr == NULL) {
            CPR_ERROR("%s - Timer %s has not been initialized.\n",
                      fname, cprTimerPtr->name);
            errno = EINVAL;
            return FALSE;
        }

        if (timerPtr->timerActive) {
            return TRUE;
        }
    } else {
        /* Bad application! */
        CPR_ERROR("%s - NULL pointer passed in.\n", fname);
        errno = EINVAL;
    }

    return FALSE;
}


/**
 * cprCancelTimer
 *
 * @brief Cancels a running timer
 *
 * The cprCancelTimer function cancels a previously started timer referenced by
 * the parameter timer.
 *
 * @param[in] timer - which timer to cancel
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 */
cprRC_t
cprCancelTimer (cprTimer_t timer)
{
    static const char fname[] = "cprCancelTimer";
    timerBlk *timerPtr;
    cpr_timer_t *cprTimerPtr;
    cprRC_t rc = CPR_SUCCESS;

    //CPR_INFO("cprCancelTimer: timer ptr=%x\n", timer);

    cprTimerPtr = (cpr_timer_t *) timer;
    if (cprTimerPtr != NULL) {
        timerPtr = (timerBlk *) cprTimerPtr->u.handlePtr;
        if (timerPtr == NULL) {
            CPR_ERROR("%s - Timer %s has not been initialized.\n",
                      fname, cprTimerPtr->name);
            errno = EINVAL;
            return CPR_FAILURE;
        }

        /*
         * Ensure timer is active before trying to remove it.
         * If already inactive then just return SUCCESS.
         */
        if (timerPtr->timerActive) {
            //CPR_INFO("removing timer from the list=%x\n", timerPtr);
            rc = removeTimerFromList(timer);
        }
        return rc;
    }

    /* Bad application! */
    CPR_ERROR("%s - NULL pointer passed in.\n", fname);
    errno = EINVAL;
    return CPR_FAILURE;
}


/**
 * cprUpdateTimer
 *
 * @brief Updates the expiration time for a running timer
 *
 * The cprUpdateTimer function cancels a previously started timer referenced by
 * the parameter timer and then restarts the same timer with the duration passed
 * in.
 *
 * @param[in]   timer    - which timer to update
 * @param[in]   duration - how long before timer expires in milliseconds
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 */
cprRC_t
cprUpdateTimer (cprTimer_t timer, uint32_t duration)
{
    static const char fname[] = "cprUpdateTimer";
    cpr_timer_t *cprTimerPtr;
    void *timerData;

    cprTimerPtr = (cpr_timer_t *) timer;
    if (cprTimerPtr != NULL) {
        /* Grab data before cancelling timer */
        timerData = cprTimerPtr->data;
    } else {
        CPR_ERROR("%s - NULL pointer passed in.\n", fname);
        errno = EINVAL;
        return CPR_FAILURE;
    }

    if (cprCancelTimer(timer) == CPR_SUCCESS) {
        if (cprStartTimer(timer, duration, timerData) == CPR_SUCCESS) {
            return CPR_SUCCESS;
        } else {
            CPR_ERROR("%s - Failed to start timer %s\n",
                      fname, cprTimerPtr->name);
            return CPR_FAILURE;
        }
    }

    CPR_ERROR("%s - Failed to cancel timer %s\n", fname, cprTimerPtr->name);
    return CPR_FAILURE;
}


/**
 * cprDestroyTimer
 *
 * @brief Destroys a timer.
 *
 * This function will cancel the timer and then destroy it. It sets
 * all links to NULL and then frees the timer block.
 *
 * @param[in] timer - which timer to destroy
 *
 * @return  CPR_SUCCESS or CPR_FAILURE
 */
cprRC_t
cprDestroyTimer (cprTimer_t timer)
{
    static const char fname[] = "cprDestroyTimer";
    cpr_timer_t *cprTimerPtr;
    cprRC_t rc;

    //CPR_INFO("cprDestroyTimer:destroying timer=%x\n", timer);

    cprTimerPtr = (cpr_timer_t *) timer;
    if (cprTimerPtr != NULL) {
        rc = cprCancelTimer(timer);
        if (rc == CPR_SUCCESS) {
            cprTimerPtr->cprTimerId = 0;
            cpr_free(cprTimerPtr->u.handlePtr);
            cpr_free(cprTimerPtr);
            return CPR_SUCCESS;
        } else {
            CPR_ERROR("%s - Cancel of Timer %s failed.\n",
                      fname, cprTimerPtr->name);
            return CPR_FAILURE;
        }
    }

    /* Bad application! */
    CPR_ERROR("%s - NULL pointer passed in.\n", fname);
    errno = EINVAL;
    return CPR_FAILURE;
}

/**
 * @}
 * @addtogroup TimerInternal The Timer internal functions
 * @ingroup Timers
 * @{
 */

/**
 * cpr_timer_pre_init
 *
 * @brief Initalize timer service and client IPC
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 */
cprRC_t cpr_timer_pre_init (void)
{
    static const char fname[] = "cpr_timer_pre_init";
    int32_t returnCode;

    /* start the timer service first */
    returnCode = (int32_t)pthread_create(&timerThreadId, NULL, timerThread, NULL);
    if (returnCode == -1) {
        CPR_ERROR("%s: Failed to create Timer Thread : %s\n", fname, strerror(errno));
        return CPR_FAILURE;
    }

    /*
     * wait some time so that timer service thread is up
     * TBD:we should really implement wait on timerthread using condvar.
     */
    cprSleep(1000);

    return CPR_SUCCESS;
}


/**
 * cpr_timer_de_init
 *
 * @brief De-Initalize timer service and client IPC
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 */
cprRC_t cpr_timer_de_init(void)
{
    // close all sockets..
    close(client_sock);
    close(serv_sock);


    // destroy api mutex
    pthread_mutex_destroy(&api_mutex);

    return CPR_SUCCESS;
}


/**
 * Timer service thread
 *
 */
/**
 * timerThread
 *
 * @brief Timer service thread
 *
 * This is the start function for the timer server thread.
 *
 * @param[in] data - The data passed in (UNUSED)
 *
 * @return  This function eventually starts an infinite loop on a "select".
 */
void *timerThread (void *data)
{
    static const char fname[] = "timerThread";

    //CPR_INFO("timerThread:started..\n");
#ifndef HOST
#ifndef PTHREAD_SET_NAME
#define PTHREAD_SET_NAME(s)     do { } while (0)
#endif
    PTHREAD_SET_NAME("CPR Timertask");
#endif

    /*
     * Increase the timer thread priority from default priority.
     * This is required to make sure timers fire with reasonable precision.
     *
     * NOTE: always make sure the priority is higher than sip/gsm threads;
     * otherwise, we must use mutex around the following while loop.
     */
    (void) cprAdjustRelativeThreadPriority(TIMER_THREAD_RELATIVE_PRIORITY);

    /* get ready to listen for timer commands and service them */
    if (start_timer_service_loop() == CPR_FAILURE) {
        CPR_ERROR("%s: timer service loop failed\n", fname);
    }

    return NULL;
}


/**
 * local_bind
 * Function used to do a bind on the local socket.
 *
 * @param[in]  sock  - socket descriptor to bind
 * @param[in]  name  - name to use for binding local socket
 * @return 0 if success, -1 on error (errno will be set)
 *
 */
static int local_bind (int sock, char *name)
{
    struct sockaddr_un addr;

    /* construct the address structure */
    addr.sun_family = AF_LOCAL;
    sstrncpy(addr.sun_path, name, sizeof(addr.sun_path));
    /* make sure file doesn't already exist */
    unlink(addr.sun_path);

    return bind(sock, (struct sockaddr *) &addr, sizeof(addr));
}

/**
 * select_sockets
 *
 * Set the socket descriptors to be used for reading.
 * Only server side uses select.
 *
 * @return The server socket number
 */
static int select_sockets (void)
{
    FD_ZERO(&socks);

    FD_SET(serv_sock, &socks);

    return (serv_sock);
}


/**
 * read_timer_cmd
 * read message received on the IPC from the client
 * the only messages are timer commands {add, remove}
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 */
static cprRC_t read_timer_cmd ()
{
    static const char fname[] = "read_timer_cmd";
    int  rcvlen;
    timer_ipc_t tmr_cmd ={0};
    cprRC_t ret = CPR_FAILURE;



    rcvlen =recvfrom(serv_sock, &tmr_cmd, sizeof(timer_ipc_t), 0,
                     NULL, NULL);

    if (rcvlen > 0) {
        //CPR_INFO("got message type=%d\n", tmr_cmd.msg_type);
        switch(tmr_cmd.msg_type) {
	case TMR_CMD_ADD:
            //CPR_INFO("request to add timer ptr=%x duration=%d datptr=%x\n",
            //       tmr_cmd.u.cmd.timer_ptr, tmr_cmd.u.cmd.duration, tmr_cmd.u.cmd.user_data_ptr);

            ret = addTimer(tmr_cmd.u.cmd.timer_ptr,tmr_cmd.u.cmd.duration,
                     (void *)tmr_cmd.u.cmd.user_data_ptr);

            break;

	case TMR_CMD_REMOVE:
            //CPR_INFO("request to remove timer ptr=%x\n", tmr_cmd.u.cmd.timer_ptr);
            ret = removeTimer(tmr_cmd.u.cmd.timer_ptr);
            break;

        default:
            CPR_ERROR("%s:invalid ipc command = %d\n", tmr_cmd.msg_type);
            ret = CPR_FAILURE;
            break;
        }
    } else {
        CPR_ERROR("%s:while reading serv_sock err =%s: Closing Socket..Timers not operational !!! \n",
                  fname, strerror(errno));
        (void) close(serv_sock);
        serv_sock = INVALID_SOCKET;
        ret = CPR_FAILURE;
    }

    /* send the result back */
    send_api_result(ret, &tmr_client_addr, sizeof(tmr_client_addr));

    return (ret);

}

/**
 * send_api_result back to client via a socket sendto operation
 * @param[in] retVal - value of result
 * @param[in] addr   - address to send the result to
 * @param[in] len    - length of addr
 */
void send_api_result(cprRC_t retVal, struct sockaddr_un *addr, socklen_t len)
{
    static const char fname[] = "send_api_result";
    timer_ipc_t tmr_rsp = {0};

    tmr_rsp.msg_type = TMR_RESULT;
    tmr_rsp.u.result = retVal;
    if (sendto(serv_sock, &tmr_rsp, sizeof(timer_ipc_t),0, (struct sockaddr *)addr, len) < 0) {
        CPR_ERROR("%s: error in sending on serv_sock err=%s\n", fname, strerror(errno));
    }
}

/**
 * Start the timer service loop.
 * Service loop waits for timer commands from timer clients processes them.
 * Waiting is done by issuing a select call with a timeout. This is the main
 * function that implements the timer functionality using the select call.
 * timeout value = duration on the head of the timer list.
 * @return CPR_SUCCESS or CPR_FAILURE
 */
cprRC_t start_timer_service_loop (void)
{
    static const char fname[] = "start_timer_service_loop";
    int lsock = -1;
    struct timeval tv;
    int ret;
    boolean use_timeout;


    /* initialize server and client addresses used for sending.*/
    cpr_set_sockun_addr((cpr_sockaddr_un_t *) &tmr_serv_addr,   SERVER_PATH, getpid());
    cpr_set_sockun_addr((cpr_sockaddr_un_t *) &tmr_client_addr, CLIENT_PATH, getpid());

    /*
     * init mutex and cond var.
     * these are used for making API synchronous etc..
     */
    if (pthread_mutex_init(&api_mutex, NULL) != 0) {
        CPR_ERROR("%s: failed to initialize api_mutex err=%s\n", fname,
                  strerror(errno));
        return CPR_FAILURE;
    }


    /* open a unix datagram socket for client library */
    client_sock = socket(AF_LOCAL, SOCK_DGRAM, 0);
    if (client_sock == INVALID_SOCKET) {
        CPR_ERROR("%s:could not create client socket error=%s\n", fname, strerror(errno));
        return CPR_FAILURE;
    }

    /* bind service name to the socket */
    if (local_bind(client_sock,tmr_client_addr.sun_path) < 0) {
        CPR_ERROR("%s:could not bind local socket:error=%s\n", fname, strerror(errno));
        (void) close(client_sock);
        client_sock = INVALID_SOCKET;
        return CPR_FAILURE;
    }

    /* open another unix datagram socket for timer service */
    serv_sock = socket(AF_LOCAL, SOCK_DGRAM, 0);
    if (serv_sock == INVALID_SOCKET) {
        CPR_ERROR("%s:could not create server socket error=%s\n", fname, strerror(errno));
        serv_sock = INVALID_SOCKET;
        close(client_sock);
        client_sock = INVALID_SOCKET;
        return CPR_FAILURE;
    }

    if (local_bind(serv_sock, tmr_serv_addr.sun_path) < 0) {
        CPR_ERROR("%s:could not bind serv socket:error=%s\n", fname, strerror(errno));
        (void) close(serv_sock);
        (void) close(client_sock);
        client_sock = serv_sock = INVALID_SOCKET;
        return CPR_FAILURE;
    }


    while (1) {

        lsock = select_sockets();

	/* set the timer equal to duration on head */
	if (timerListHead != NULL) {
            tv.tv_sec = (timerListHead->duration)/1000;
            tv.tv_usec = (timerListHead->duration%1000)*1000;
            //CPR_INFO("%s:time duration on head =%d sec:%d usec (or %d msec)\n",
            //       fname, tv.tv_sec, tv.tv_usec,
            //       timerListHead->duration);
            use_timeout = TRUE;
	} else {
            //CPR_INFO("%s:no timer in the list.. will block until there is one\n",
            //       fname);
            use_timeout = FALSE;
	}

        ret = select(lsock + 1, &socks, NULL, NULL, (use_timeout == TRUE) ? &tv:NULL);

        if (ret == -1) {
            CPR_ERROR("%s:error in select err=%s\n", fname,
                      strerror(errno));
            return(CPR_FAILURE);
        } else if (ret == 0) {
            /*
             * this means the head timer has expired..there could be others
             */
            timerListHead->duration = 0;
            process_expired_timers();
        } else {

            if (FD_ISSET(serv_sock, &socks)) {
                //CPR_INFO("Got something on serv_sock..\n");
                /* first reduce the duration of the head by current run time */
                if (timerListHead != NULL) {
                    //CPR_INFO("set head duration to =%d prev was= %d\n",
                    //       tv.tv_sec * 1000 + (tv.tv_usec/1000),
                    //       timerListHead->duration);
                    /* set the head with the remaining duration(tv) as indicated by select */
                    timerListHead->duration = tv.tv_sec * 1000 + (tv.tv_usec/1000);
                }
                /* read the ipc message to remove or add a timer */
                (void) read_timer_cmd();
            }
	}
    }

}

/**
 *
 * Process the timers expired. Generally this is called when head timer
 * has expired.
 * @note we need to process the list as there could be
 * other timers too in the list which have expired.
 *
 */
void process_expired_timers() {
}

/**
  * @}
  */
