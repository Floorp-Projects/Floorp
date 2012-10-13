/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _RESET_API_H_
#define _RESET_API_H_


/**
 * resetRequest
 *
 * This function tells the reset Manager that the SIPCC module
 * wants to do a HARD RESET. This is most likely because of a request
 * from the CUCM.
 *
 * The response received for this request is asynchronous and
 * should be handled via event provided by reset manager.
 * The CCAPI_Service_shutdown api needs to be called for the
 * handling of the response to the reset request
 *
 */
void resetRequest();


/**
 * resetReady
 *
 * This function tells the reset manager that call control is
 * ready for reset. This is called whenever the call control
 * determines that it is idle
 *
 * The resetManager will keep track of events can initate
 * reset when it has received ready.
 *
 */
void resetReady();

/**
 * resetNotReady
 *
 * This function tells the reset manager that call control is
 * NOT ready for reset. This is called whenever the call control
 * is not idle
 *
 * The resetManager will keep track of events  and it CANNOT initate
 * reset until a resetReady event is received
 *
 */
void resetNotReady();

#endif  /* _RESET_API_H_ */
