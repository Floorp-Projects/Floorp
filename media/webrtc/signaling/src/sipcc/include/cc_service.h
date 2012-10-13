/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CC_SERVICE_H_
#define _CC_SERVICE_H_

#include "cc_constants.h"

/**
 * Defines the management methods.
 */
/**
 * The following methods are defined to bring up the pSipcc stack
 */
/**
 * Initialize the pSipcc stack. CC_Service_create() must be called before
 * calling this function.
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_Service_init();

/**
 * This function creates various data module needed for initialization of
 * pSipcc stack. On reboot or after CC_Service_destroy(), application must call
 * first this function followed by CC_Service_init() and then
 * CC_Service_start() to bring pSipcc stack in in-service. This function
 * need to be called only once.
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_Service_create();

/**
 * Gracefully unload the pSipcc stack. To bring up the pSipcc stack again,
 * follow the function calling sequence starting from CC_Service_create().
 * @return SUCCESS
 */
cc_return_t CC_Service_destroy();

/**
 * Bring up the pSipcc stack in service. CC_Service_init() must be called
 * before calling this function.
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_Service_start();

/**
 * Shutdown pSipcc stack for restarting
 * @param mgmt_reason the reason to shutdown pSipcc stack
 * @param reason_string literal string for shutdown
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_Service_shutdown(cc_shutdown_reason_t mgmt_reason, cc_string_t reason_string);

/**
 * Unregister all lines of a phone
 * @param mgmt_reason the reason to bring down the registration
 * @param reason_string the literal string for unregistration
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_Service_unregisterAllLines(cc_shutdown_reason_t mgmt_reason, cc_string_t reason_string);

/**
 * Register all lines for a phone.
 * @param mgmt_reason the reason of registration
 * @param reason_string the literal string of the registration
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_Service_registerAllLines(cc_shutdown_reason_t mgmt_reason, cc_string_t reason_string);

/**
 * Restart pSipcc stack
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_Service_restart();

#endif /* _CC_SERVICE_H_ */
