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
