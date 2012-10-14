/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCAPI_SERVICE_H_
#define _CCAPI_SERVICE_H_
#include "cc_constants.h"

/**
 * Globals defined in ccapi_service.c
 */
extern int g_dev_hdl;
#define G_DEV_NAME_SIZE 64
extern char g_dev_name[G_DEV_NAME_SIZE];
#define G_CFG_P_SIZE 256
extern char g_cfg_p[G_CFG_P_SIZE];
extern int g_compl_cfg;

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
