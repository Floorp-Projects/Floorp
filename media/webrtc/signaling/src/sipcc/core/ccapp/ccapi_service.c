/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
char g_dev_name[G_DEV_NAME_SIZE];
char g_cfg_p[G_CFG_P_SIZE];
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
//cc_boolean parse_config_properties (int device_handle, const char *device_name, const char *cfg, int from_memory);



/**
 * Defines the management methods.
 */

/**
 * Pre-initialize the Sipcc stack.
 * @return
 */
cc_return_t CCAPI_Service_create() {
    CCAPP_ERROR("CCAPI_Service_create - calling CC_Service_create");

    registration_processEvent(EV_CC_CREATE);
    return (CC_SUCCESS);
    //return (service_processEvent(EV_SRVC_CREATE));
}

/**
 * Gracefully unload the Sipcc stack
 * @return
 */
cc_return_t CCAPI_Service_destroy() {
    CCAPP_ERROR("CCAPI_Service_destroy - calling CC_Service_destroy");

 //   if (is_action_to_be_deferred(STOP_ACTION) == TRUE) {
 //       return CC_SUCCESS;
 //   }
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
        DEF_DEBUG("CCAPI_Service_start request is already pending. Ignoring this.");
        return CC_SUCCESS;
    }

    DEF_DEBUG("CCAPI_Service_start -");
    isServiceStartRequestPending = TRUE;

    registration_processEvent(EV_CC_START);

    return (CC_SUCCESS);
}

/**
 * Stop Sipcc stack service
 * @return
 */
cc_return_t CCAPI_Service_stop() {

	int  sdpmode = 0;

    CCAPP_ERROR("CCAPI_Service_stop - calling registration stop");

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));
    if (!sdpmode) {
        if (is_action_to_be_deferred(STOP_ACTION) == TRUE) {
            return CC_SUCCESS;
        }
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
    CCAPP_ERROR("CCAPI_Service_reregister - initiate reregister");

    if (is_action_to_be_deferred(RE_REGISTER_ACTION) == TRUE) {
        return CC_SUCCESS;
    }
    if (pending_action_type != NO_ACTION) {
        CCAPP_ERROR("Reset/Restart is pending, reregister Ignored!");
        return CC_FAILURE;
    }

    if (is_empty_str((char*)cfg)) {
        CCAPP_ERROR("Reregister request with empty config.  Exiting.");
        return CC_FAILURE;
    }

    g_dev_hdl = device_handle;
    sstrncpy(g_dev_name, device_name, sizeof(g_dev_name));
    sstrncpy(g_cfg_p, cfg, sizeof(g_cfg_p));
    CCAPP_DEBUG("CCAPI_Service_reregister - devce name [%s], cfg [%s]", g_dev_name, g_cfg_p);
    g_compl_cfg  = complete_config;

        registration_processEvent(EV_CC_RE_REGISTER);

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
