/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
        CCAPP_ERROR(DEB_F_PREFIX" invalid registration details user=%s, domain=%s\n", DEB_F_PREFIX_ARGS(CC_API, fname), sipUser, sipDomain);
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
