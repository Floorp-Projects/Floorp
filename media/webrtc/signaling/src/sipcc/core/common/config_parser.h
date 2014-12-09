/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CONFIG_PARSER_H_
#define CONFIG_PARSER_H_

#include "cc_constants.h"
#include "cc_types.h"
#include "cc_config.h"
#include "ccapi.h"
#include "phone_debug.h"
#include "debug.h"
#include "prot_configmgr.h"
#include "call_logger.h"
#include "sip_common_transport.h"
#include "sip_ccm_transport.h"

#define MAX_CFG_VERSION_STAMP_LEN 80

/*
 * This function determine whether the passed config parameter should be used
 * in comparing the new and old config value for apply-config purpose.  Only
 * those config ids on whose change phone needs to restart are part of this
 * function. For remaining parameters, it is assumed that any change can be
 * applied dynamically.
 *
 */
boolean is_cfgid_in_restart_list(int cfgid);

/*
 * This function either compare the new and old config value or set the value
 * for the config_id passed depending upon whether apply-config is true or not.
 */
void compare_or_set_byte_value(int cfgid, unsigned char value, const unsigned char * config_name);

/*
 * This function either compare the new and old config value or set the value
 * for the config_id passed depending upon whether apply-config is true or not.
 */
void compare_or_set_boolean_value(int cfgid, cc_boolean value, const unsigned char * config_name);

/*
 * This function either compare the new and old config value or set the value
 * for the config_id passed depending upon whether apply-config is true or not.
 */
void compare_or_set_int_value(int cfgid, int value, const unsigned char * config_name);

/*
 * This function either compare the new and old config value or set the value
 * for the config_id passed depending upon whether apply-config is true or not.
 */
void compare_or_set_string_value (int cfgid, const char* value, const unsigned char * config_name);

/*
 * config_fetch_dialplan() called to retrieve the dialplan
 *
 */
void config_fetch_dialplan(char *filename);

/*
 * config_fetch_fcp() called to retrieve the fcp
 *
 */
void config_fetch_fcp(char *filename);

/*
 * config_set_autoreg_properties
 *
 */
void config_set_autoreg_properties ();

/*
 * update_security_mode_and_ports
 *
 */
void update_security_mode_and_ports(void);

/*
 * config_get_mac_addr
 *
 * Get the filename that has the mac address and parse the string
 * convert it into an mac address stored in the bytearray maddr
*/
void config_get_mac_addr (char *maddr);

/*
 * Set the IP and MAC address in the config table
 */
void config_set_ccm_ip_mac ();


/*
 * Set up configuration without XML config file.
 */
void config_setup_elements ( const char *sipUser, const char *sipPassword, const char *sipDomain);

/*
 * Set server ip address into config
 * Same ip address is also used to make a P2P call
 */
void config_setup_server_address (const char *sipDomain);

/*
 * set transport protocol, limited to udp or tcp for now
 */
void config_setup_transport_udp(const cc_boolean is_udp);

/*
 * set local voip port defaults to 5060
 */
void config_setup_local_voip_control_port(const int voipControlPort);

/*
 * set remote voip port defaults to 5060
 */
void config_setup_remote_voip_control_port(const int voipControlPort);

/*
 * get local voip port defaults to 5060
 */
int config_get_local_voip_control_port();

/*
 * get remote voip port defaults to 5060
 */
int config_get_remote_voip_control_port();

/*
 * get ikran version
 */
const char* config_get_version();

/*
 * set p2p mode on or off
 */
void config_setup_p2p_mode(const cc_boolean is_p2p);

/*
 * set sdp mode on or off
 */
void config_setup_sdp_mode(const cc_boolean is_sdp);

/*
 * set avp mode (true == RTP/SAVPF,  false = RTP/SAVP)
 */
void config_setup_avp_mode(const cc_boolean is_rtpsavpf);

/**
* config_minimum_check:
*
* @a_node: the initial xml node to consider.
* @doc: The DOM tree of the xml file
*
* Check if minimum set of elements are present in the config file and
* have values that can be used by sipstack
*
*/

/**
 * Parse the file that is passed in,
 * walk down the DOM that is created , and get the
 * xml elements nodes.
 */
int config_parser_main( char *config, int complete_config);

/*
 * Set up configuration without XML config file.
 */
int config_setup_main( const char *sipUser, const char *sipPassword, const char *sipDomain);


#endif /* CONFIG_PARSER_H_ */
