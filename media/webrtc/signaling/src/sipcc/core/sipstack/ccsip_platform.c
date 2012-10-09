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
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX "IP Stack Not Initialized.\n", "sip_platform_init");
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
