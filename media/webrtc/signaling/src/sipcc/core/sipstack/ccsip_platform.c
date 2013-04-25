/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_stdio.h"
#include "cpr_string.h"
#include "phone.h"
#include "phone_debug.h"
#include "ccsip_register.h"
#include "ccsip_task.h"
#include "ccsip_pmh.h"
#include "config.h"
#include "sip_common_transport.h"
#include "sip_csps_transport.h"
#include "uiapi.h"
#include "sip_interface_regmgr.h"

#include "platform_api.h"

extern void platform_sync_cfg_vers(char *cfg_ver, char *dp_ver, char *softkey_ver);
extern void platform_reg_failover_ind(void *data);
extern void platform_reg_fallback_ind(void *data);
extern int platGetUnregReason();
extern void ccsip_add_wlan_classifiers();
void ccsip_remove_wlan_classifiers();

/*
 *  Function: sip_platform_init()
 *
 *  Parameters: None
 *
 *  Description: Performs platform initialization stuff.  Should probably be
 *               renamed or moved to make it more "generic".
 *
 *  Returns: None
 *
 */
void
sip_platform_init (void)
{

    // Since we have all our configuration information now
    // we want to do a final check to see if our network media
    // type has changed

    /* Unregister the phone */
    ccsip_register_cancel(FALSE, TRUE);
    ccsip_register_reset_proxy();

    /*
     * Make sure that the IP stack is up before trying to connect
     */
    if (PHNGetState() > STATE_IP_CFG) {

        ccsip_add_wlan_classifiers();
        /*
         * regmgr - The SIPTaskDisconnectFromSipProxies and
         * SIPTaskConnectToSipProxies calls will be called
         * as part of the transport interface init and regmgr
         * inits.
         */

        ccsip_register_all_lines();
        ui_sip_config_done();
    } else {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX "IP Stack Not Initialized.", "sip_platform_init");
    }
}


/*
 * Send StationReset message
 * Parameters supplied by application:
 *    none
 * Parameters supplied in the message
 *     - reset type (RESTART)
 */
int
sip_platform_ui_restart (void)
{
    phone_reset(DEVICE_RESTART);
    return TRUE;
}


void
sip_platform_handle_service_control_notify (sipServiceControl_t *scp)
{
    switch (scp->action) {

    case SERVICE_CONTROL_ACTION_RESET:
        platform_reset_req(DEVICE_RESET);
        break;

    case SERVICE_CONTROL_ACTION_RESTART:
        platform_reset_req(DEVICE_RESTART);
        break;

    case SERVICE_CONTROL_ACTION_CHECK_VERSION:
        platform_sync_cfg_vers(scp->configVersionStamp,
                               scp->dialplanVersionStamp,
                               scp->softkeyVersionStamp);
        break;

    case SERVICE_CONTROL_ACTION_APPLY_CONFIG:
        // call the function to process apply config NOTIFY message.
        platform_apply_config(scp->configVersionStamp,
                               scp->dialplanVersionStamp,
                               scp->fcpVersionStamp,
                               scp->cucm_result,
                               scp->firmwareLoadId,
                               scp->firmwareInactiveLoadId,
                               scp->loadServer,
                               scp->logServer,
                               scp->ppid);
        break;
    default:
        break;

    }
}
