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

#ifndef _CCAPI_SERVICE_H_
#define _CCAPI_SERVICE_H_
#include "cc_constants.h"
/**
 * Defines the management methods.
 */

/**
 * The following methods are defined to bring up the pSipcc stack
 */

/**
 * This function creates various data module needed for initialization of
 * Sipcc stack. On reboot or after CCAPI_Service_destroy(), application must call
 * first this function followed by CC_Service_start() 
 *  to bring Sipcc stack in in-service. This function
 * need to be called only once.
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Service_create();

/**
 * Gracefully unload the Sipcc stack. To bring up the pSipcc stack again,
 * follow the function calling sequence starting from CCAPI_Service_create().
 * @return SUCCESS
 */
cc_return_t CCAPI_Service_destroy();

/**
 * Bring up the Sipcc stack in service. 
 * 
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Service_start();

/**
 * Stop Sipcc stack 
 * 
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Service_stop();

/**
 * CCAPI_Service_reregister
 *
 * This API will result in stopping the stip stack (i.e unregister) followed 
 * by parsing of the current config followed by a 
 * start (i.e register) of the SIP stack, without the download of a new config 
 * file. This API is used in the APPLY config case  
 *
 * @param device_handle handle of the device, the response is for
 * @param device_name 
 * @param cfg the config file name and path  or the complete configuration
 *         in memory.
 * @param from_memory boolean flag to indicate if the complete config
 *         is sent. This parameter is meant to indicate if the "cfg" parameter
 *          points to the file name or is a pointer to the complete config in memory.
 * @return 
 */
cc_return_t CCAPI_Service_reregister (int device_handle, const char *device_name,
                             const char *cfg, int from_memory);

/**
 * Reset request from the Reset Manager
 * 
 * @return void
 */
void CCAPI_Service_reset_request();

#endif /* _CCAPI_SERVICE_H_ */
