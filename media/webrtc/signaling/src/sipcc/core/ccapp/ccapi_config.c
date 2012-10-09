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

#include "phone_debug.h"
#include "CCProvider.h"
#include "sessionConstants.h"
#include "prot_configmgr.h"
#include "cc_types.h"
#include "config_parser.h"
#include "config_api.h"
#include "ccapi_snapshot.h"
#include "ccapi_device.h"
#include "ccapi_device_info.h"
#include "cc_device_manager.h"
#include "ccapi_service.h"
#include "util_string.h"

extern boolean apply_config;
extern cc_apply_config_result_t apply_config_result;
cc_boolean parse_setup_properties (int device_handle, const char *device_name, const char *sipUser, const char *sipPassword, const char *sipDomain);

/**
 * 
 * @return
 */

void CCAPI_Start_response(int device_handle, const char *device_name, const char *sipUser, const char *sipPassword, const char *sipDomain) {
    static const char fname[] = "CCAPI_Start_response";

    if (is_empty_str((char*)sipUser) || is_empty_str((char*)sipDomain)) {
        CCAPP_ERROR(DEB_F_PREFIX" invalid registration details user=%x, domain=%x\n", DEB_F_PREFIX_ARGS(CC_API, fname), sipUser, sipDomain);
        return;
    }

    g_dev_hdl = device_handle;
    sstrncpy(g_dev_name, device_name, sizeof(g_dev_name));

    if (is_phone_registered() == FALSE) {

        if (parse_setup_properties(device_handle, device_name, sipUser, sipPassword, sipDomain)) {
            registration_processEvent(EV_CC_CONFIG_RECEIVED);
        }
        return;
    }

 }

/*  New Function
    Register without using config file downloaded from cucm
 */
cc_boolean parse_setup_properties (int device_handle, const char *device_name, const char *sipUser, const char *sipPassword, const char *sipDomain) {
    CC_Config_setStringValue(CFGID_DEVICE_NAME, device_name);

    config_setup_main(sipUser, sipPassword, sipDomain);

    ccsnap_device_init();
    ccsnap_line_init();
    ccsnap_gen_deviceEvent(CCAPI_DEVICE_EV_CONFIG_CHANGED, CC_DEVICE_ID);
    return TRUE;
}

cc_boolean CCAPI_Config_set_server_address(const char *ip_address) {
	config_setup_server_address(ip_address);
	return TRUE;
}

cc_boolean CCAPI_Config_set_transport_udp(const cc_boolean is_udp) {
	config_setup_transport_udp(is_udp);
	return TRUE;
}

cc_boolean CCAPI_Config_set_local_voip_port(const int port) {
	config_setup_local_voip_control_port(port);
	return TRUE;
}

cc_boolean CCAPI_Config_set_remote_voip_port(const int port) {
	config_setup_remote_voip_control_port(port);
	return TRUE;
}

int CCAPI_Config_get_local_voip_port() {
	return config_get_local_voip_control_port();
}

int CCAPI_Config_get_remote_voip_port() {
	return config_get_remote_voip_control_port();
}

const char* CCAPI_Config_get_version() {
	return config_get_version();
}

cc_boolean CCAPI_Config_set_p2p_mode(const cc_boolean is_p2p) {
	config_setup_p2p_mode(is_p2p);
	return TRUE;
}

cc_boolean CCAPI_Config_set_sdp_mode(const cc_boolean is_sdp) {
	config_setup_sdp_mode(is_sdp);
	return TRUE;
}

cc_boolean CCAPI_Config_set_avp_mode(const cc_boolean is_rtpsavpf) {
	config_setup_avp_mode(is_rtpsavpf);
	return TRUE;
}
