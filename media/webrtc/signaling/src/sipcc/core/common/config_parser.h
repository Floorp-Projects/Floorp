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

#ifndef CONFIG_PARSER_H_
#define CONFIG_PARSER_H_

#include <libxml/parser.h>
#include <libxml/tree.h>
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

extern char g_cfg_version_stamp[];

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

void update_sipline_properties(xmlNode *sipline_node, xmlDocPtr doc);

/*
 * config_set_sipline_properties
 *
 */
void config_set_sipline_properties (xmlNode *sipline_node, xmlDocPtr doc, boolean button_configured[] );

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

//void process_ccm_config(xmlNode *ccm_node, xmlDocPtr doc, char **ccm_name, int * sip_port, int *iteration);
void process_ccm_config(xmlNode *ccm_node, xmlDocPtr doc,
						char ccm_name[][MAX_SIP_URL_LENGTH],
						int * sip_port, int * secured_sip_port,
						int *iteration);

/*
 * config_set_ccm_properties
 *
 */
void config_process_ccm_properties  (xmlNode *ccm_node, xmlDocPtr doc);

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
 * config_parse_element
 *
 * Parse the elements in the XML document and call the
 * corressponding set functions
 */
void config_parse_element (xmlNode *cur_node, char *  value, xmlDocPtr doc );


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
 * set ROAP proxy mode on or off
 */
void config_setup_roap_proxy_mode(const cc_boolean is_roap_proxy);

/*
 * set ROAP client mode on or off
 */
void config_setup_roap_client_mode(const cc_boolean is_roap_client);


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
void config_minimum_check_siplines(xmlNode * a_node, xmlDocPtr doc);

boolean is_config_valid(char * config, int from_memory);

void config_minimum_check(xmlNode * a_node, xmlDocPtr doc);

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
