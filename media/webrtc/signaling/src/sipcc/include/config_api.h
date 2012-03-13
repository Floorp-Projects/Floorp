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

#ifndef _CONFIG_API_H_
#define _CONFIG_API_H_

#include "cc_types.h"


/**
 * configFetchReq
 * 
 * This function tells the config manager to fetch the CTL file 
 * and then fetch the config  from the CUCM. It is expected that 
 * this will result in processing of
 * the config file after the config managers response is received.
 *
 * The response received for this request is asynchronous and 
 * should be handled via event provided by config manager.
 * The CCAPI_Config_reponse api needs to be called for the 
 * handling of the response to the fetch request
 *
 */
void configFetchReq(int device_handle);

/**
 * configParserError
 *
 * Notify the config manager that the config file has an error
 * and a new config file needs to be downloaded.
 *
 * The error could be XML format error or minimum config not being
 * present in the config file.  It is expected that 
 * this will result in processing of
 * the config file after the config managers response is received.
 *
 */
void configParserError(void);

/**
 *
 * CCAPI_Config_response
 *
 * Application should call this api in response to the request
 * configFetchReq(). This is an asynchronous
 * response to the configFetchReq. 
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
void CCAPI_Config_response (int device_handle, const char *device_name, 
                             const char *cfg, int from_memory);


/**
 * When called this function should register with CUCM without prior device file download.
 */
void CCAPI_Start_response(int device_handle, const char *device_name, const char *sipUser, const char *sipPassword, const char *sipDomain);

/**
 *
 * Application should call this api to check the validity of config file. This 
 * function will check whether config file is parsable and has some minimum configuration
 * information defined for the phone to register.
 *
 * @param device_handle handle of the device, the response is for
 * @param cfg_file_name the config file name and path 
 * @param from_memory boolean flag to indicate if the complete config
 *         is sent. This parameter is meant to indicate if the "cfg" parameter
 *          points to the file name or is a pointer to the complete config in memory.
 * @return 
 */ 

cc_boolean CCAPI_Config_checkValidity (int device_handle, const char *cfg_file_name, int from_memory);

/**
 *
 * Application should call this api in response to the request configApplyConfigNotify().
 *
 * @param device_handle handle of the device, the response is for
 * @param cfg the config file name and path 
 * @param from_memory boolean flag to indicate if the complete config
 *         is sent. This parameter is meant to indicate if the "cfg" parameter
 *          points to the file name or is a pointer to the complete config in memory.
 * @param action specify whether to apply the config changes or compare the config files
 * @return 
 */ 
int CCAPI_Config_compareProperties (int device_handle, const char *cfg_file_name, int from_memory);


/**
 * Inform application that pSipcc stack has receive a NOTIFY message of event
 * type "service-control" and action=apply-config. The arguments passed to this
 * function contains the parameter values received in the message.
 *
 * @param config_version [in] latest version stamp of phone configuration.
 * @param dial_plan_version [in] latest version stamp of the dial plan.
 * @param fcp_version [in] latest version stamp of feature policy control.
 * @param cucm_result  [in] action taken by the cucm for the changes applied by the
 * user to the phone configuration. cucm result could be
 *  @li "no_change" - the new phone configuration changes do not impact Unified CM,
 *  @li "config_applied" - The Unified CM Administration applied the
 *  configuration changes dynamically without requiring phone to re-register,
 *  @li "reregister_needed" - The Unified CM Administration applied the
 *  configuration changes and required the phone to re-register to make them
 *  effective.
 * @param load_id [in] - firmware image name that phone should be running with
 * @param inactive_load_id [in] - firmware image name that phone should be in inactive partition.
 * @param load_server [in] - address of load server to download the  firmware
 * image load_id.
 * @param log_server [in] - log server address where error report need to be
 * sent. Error report, for example, could be related to image download failure
 * or phone configuration file download failure.
 * @param ppid [in] whether peer to peer upgrade is available
 * @return void
 */
void configApplyConfigNotify(cc_string_t config_version,
		cc_string_t dial_plan_version,
		cc_string_t fcp_version,
		cc_string_t cucm_result,
		cc_string_t load_id,
		cc_string_t inactive_load_id,
		cc_string_t load_server,
		cc_string_t log_server,
		cc_boolean ppid);

/**
 * dialPlanFetchReq
 * 
 * This function tells the get file request service to fetch the latest dial
 * plan from the CUCM.
 *
 * @param device_handle [in] handle of the device, the response is for
 * @param dialPlanFileName [in] the name of dialplan file to retrieve 
 * @return cc_boolean indicating success/failure
 *
 */
cc_boolean dialPlanFetchReq(int device_handle, char* dialPlanFileName); 

/**
 * fcpFetchReq
 * 
 * This function tells the get file request service to fetch the latest fcp
 * file from the CUCM.
 *
 * @param device_handle [in] handle of the device, the response is for
 * @param dialPlanFileName [in] the name of fcp file to retrieve 
*  @return cc_boolean indicating success/failure
 *
 */
cc_boolean fcpFetchReq(int device_handle, char* fcpFileName); 


cc_boolean CCAPI_Config_set_server_address(const char *ip_address);
cc_boolean CCAPI_Config_set_transport_udp(const cc_boolean is_udp);
cc_boolean CCAPI_Config_set_local_voip_port(const int port);
cc_boolean CCAPI_Config_set_remote_voip_port(const int port);
int CCAPI_Config_get_local_voip_port();
int CCAPI_Config_get_remote_voip_port();
const char* CCAPI_Config_get_version();
cc_boolean CCAPI_Config_set_p2p_mode(const cc_boolean is_p2p);
cc_boolean CCAPI_Config_set_roap_proxy_mode(const cc_boolean is_roap_proxy);
cc_boolean CCAPI_Config_set_roap_client_mode(const cc_boolean is_roap_client);


#endif  /* _CONFIG_API_H_ */
