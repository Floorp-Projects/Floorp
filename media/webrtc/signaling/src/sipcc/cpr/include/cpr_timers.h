/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
