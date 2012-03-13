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

#include "ccapi_service.h"
#include "cc_device_manager.h"
#include "cc_service.h"
#include "phone_debug.h"
#include "CCProvider.h"
#include "sessionConstants.h"
#include "ccsip_messaging.h"
#include "ccapp_task.h"
#include "config_api.h"
#include "ccapi_device.h"
#include "ccapi_device_info.h"
#include "cc_device_listener.h"
#include "cc_service_listener.h"
#include "plat_api.h"
#include "util_string.h"

int sendResetUpdates  = 0;         // default is not to send updates

// Global Variables
int g_dev_hdl;
char g_dev_name[64];
char g_cfg_p[256];
int g_compl_cfg;

// Externs
extern void setState(); 
extern void resetReady();
extern void resetNotReady();
extern void ccpro_handleserviceControlNotify();


extern cc_srv_ctrl_cmd_t reset_type;
boolean isServiceStartRequestPending = FALSE;
cc_boolean is_action_to_be_deferred(cc_action_t action);
extern cc_action_t pending_action_type; 
cc_boolean parse_config_properties (int device_handle, const char *device_name, const char *cfg, int from_memory); 



/**
 * Defines the management methods.
 */

/**
 * Pre-initialize the Sipcc stack.
 * @return
 */
cc_return_t CCAPI_Service_create() {
    CCAPP_ERROR("CCAPI_Service_create - calling CC_Service_create \n");

    registration_processEvent(EV_CC_CREATE);
    return (CC_SUCCESS);
    //return (service_processEvent(EV_SRVC_CREATE));
}

/**
 * Gracefully unload the Sipcc stack
 * @return
 */
cc_return_t CCAPI_Service_destroy() {
    CCAPP_ERROR("CCAPI_Service_destroy - calling CC_Service_destroy \n");
    
    if (is_action_to_be_deferred(STOP_ACTION) == TRUE) {
        return CC_SUCCESS; 
    }
    // initialize the config to empty
    init_empty_str(g_cfg_p);
    isServiceStartRequestPending = FALSE;
    registration_processEvent(EV_CC_DESTROY);
    return (CC_SUCCESS);
}

/**
 * Bring up the Sipcc stack in service
 * @return
 */
cc_return_t CCAPI_Service_start() {

    if (isServiceStartRequestPending == TRUE) {
        DEF_DEBUG("CCAPI_Service_start request is already pending. Ignoring this.\n");
        return CC_SUCCESS;
    }

    DEF_DEBUG("CCAPI_Service_start - \n");
    isServiceStartRequestPending = TRUE;
    
    registration_processEvent(EV_CC_START);

    return (CC_SUCCESS);
}

/**
 * Stop Sipcc stack service
 * @return
 */
cc_return_t CCAPI_Service_stop() {

    CCAPP_ERROR("CCAPI_Service_stop - calling registration stop \n");
    if (is_action_to_be_deferred(STOP_ACTION) == TRUE) {
        return CC_SUCCESS; 
    }
    sendResetUpdates  = 0;         // reset to default is not to send updates
    isServiceStartRequestPending = FALSE;
    registration_processEvent(EV_CC_STOP);
    return CC_SUCCESS;
}


/**
 * reregister the Sipcc stack service, without downloading the config file
 * 
 */
cc_return_t CCAPI_Service_reregister(int device_handle, const char *device_name,
                             const char *cfg,
                             int complete_config)
{
    CCAPP_ERROR("CCAPI_Service_reregister - initiate reregister \n");

    if (is_action_to_be_deferred(RE_REGISTER_ACTION) == TRUE) {
        return CC_SUCCESS; 
    }
    if (pending_action_type != NO_ACTION) {
        CCAPP_ERROR("Reset/Restart is pending, reregister Ignored! \n");
        return CC_FAILURE;
    }
    
    if (is_empty_str((char*)cfg)) {
        CCAPP_ERROR("Reregister request with empty config.  Exiting.\n");
        return CC_FAILURE;    
    }

    g_dev_hdl = device_handle;
    strncpy(g_dev_name, device_name, 64);
    strncpy(g_cfg_p, cfg, 256);
    CCAPP_DEBUG("CCAPI_Service_reregister - devce name [%s], cfg [%s] \n", g_dev_name, g_cfg_p);
    g_compl_cfg  = complete_config;

    if (parse_config_properties(g_dev_hdl, g_dev_name, g_cfg_p, g_compl_cfg) == TRUE) {
        registration_processEvent(EV_CC_RE_REGISTER);
    }
    
    return (CC_SUCCESS);
}

/**
 * Reset Manager has request a reset, send the current state and 
 * start sending updates.
 */
void CCAPI_Service_reset_request()  {
    cc_deviceinfo_ref_t handle = 0;
    sendResetUpdates  = 1;  
    if (CCAPI_DeviceInfo_isPhoneIdle(handle) == TRUE) {
        resetReady();
    } else {
        resetNotReady();
    }

}
