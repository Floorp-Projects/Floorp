/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_TIMERS_H_
#define _CPR_TIMERS_H_

#include "cpr_types.h"
#include "cpr_ipc.h"

__BEGIN_DECLS

/**
 * Define handle for timers
 */
typedef void *cprTimer_t;

/**
 * System timer information needed to hide OS differences in implementation.
 * To use timers, the application code may pass in a name to the
 * create function for timers. CPR does not use this field, it is
 * solely for the convenience of the application and to aid in debugging.
 * Upon an application calling the init routine, CPR will malloc the
 * memory for a timer, set the handlePtr or handleInt as appropriate
 * and return a pointer to the timer structure.
 */

/**
 * Information CPR needs to send back to the application
 * when a timer expires.
 */
typedef struct {
    const char *expiredTimerName;
    uint16_t expiredTimerId;
    void *usrData;
} cprCallBackTimerMsg_t;


/* Function prototypes */

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
void cprSleep(uint32_t duration);


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
cprTimer_t cprCreateTimer(const char * name,
                          uint16_t applicationTimerId,
                          uint16_t applicationMsgId,
                          cprMsgQueue_t callBackMsgQueue);

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
cprRC_t cprStartTimer(cprTimer_t timer, uint32_t duration, void *data);


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
boolean cprIsTimerRunning(cprTimer_t timer);


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
cprRC_t cprDestroyTimer(cprTimer_t timer);


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
cprRC_t cprCancelTimer(cprTimer_t timer);


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
cprRC_t cprUpdateTimer(cprTimer_t timer, uint32_t duration);

__END_DECLS

#endif
