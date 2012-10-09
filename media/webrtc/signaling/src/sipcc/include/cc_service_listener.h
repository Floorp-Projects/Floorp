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

#ifndef _CC_SERVICE_LISTENER_H_
#define _CC_SERVICE_LISTENER_H_

#include "cc_constants.h"

/**
 * The following section defines all callback method that the pSipcc stack will provide
 * for configuration purpose.
 */
/**
 * Notify appliation that the pSipcc stack is out of service.
 * Application can, for example, use this notification to inform user that
 * device is currently out of service.
 * @param cucm_mode - the cucm mode of call manager to which device was
 * connected before device went out of service.
 * @param service_cause [out]  the cause of out of service.
 * @return void
 */
void CC_ServiceListener_outOfService(cc_cucm_mode_t cucm_mode, cc_service_cause_t service_cause);

/**
 * Notify application that the pSipcc stack is up in service. Application can,
 * for example, use this notification to inform user that device is currently
 * in service.
 * @param cucm_mode - the cucm mode of call manager to which device is
 * connected.
 * @param service_cause [out] - the cause of INS
 * @param sis_ver_name version name
 * @param major_number the major version number
 * @param minor_number the minor version number
 * @param additional_number the additional version number
 * @return void
 */
void CC_ServiceListener_inService(cc_cucm_mode_t cucm_mode, 
        cc_service_cause_t service_cause, 
        cc_string_t sis_ver_name,
        cc_uint32_t major_number,
        cc_uint32_t minor_number,
        cc_uint32_t additional_number);


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
 * @param inactive_load_d [in] firmaware image name for the inactive load image 
 * @param load_server [in] - address of load server to download the  firmware
 * image load_id.
 * @param log_server [in] - log server address where error report need to be
 * sent. Error report, for example, could be related to image download failure
 * or phone configuration file download failure.
 * @param ppid [in] whether peer to peer upgrade is available
 * @return void
 */
void CC_ServiceListener_applyConfigNotify(cc_string_t config_version,
		cc_string_t dial_plan_version,
		cc_string_t fcp_version,
		cc_string_t cucm_result,
		cc_string_t load_id,
		cc_string_t inactive_load_id,
		cc_string_t load_server,
		cc_string_t log_server,
		cc_boolean ppid);

/**
 * Notify the server time. Application can use this function to adjust the
 * device time.
 * @param time - time in second
 * @return void
 */
void CC_ServiceListener_serverTimeNotify(long time);

/**
 * Update the phone registration state.
 * @param line the line
 * @param reg_state the registration state
 * @return void
 */
void CC_ServiceListener_registrationStateChanged(cc_lineid_t line, cc_line_reg_state_t reg_state);

/**
 * Update cucm connection status.
 * @param address the cucm address
 * @param reg_ccm_state the connection state for the cucm.
 * @return void
 */
void CC_ServiceListener_cucmConnectionStatusChanged(cc_string_t address, cc_ccm_status_t reg_ccm_state);

/**
 * Sync up configuration version.
 * @param config_version the configuration version
 * @param dial_plan_version the dial plan version
 * @param softkey_version the softkey set version
 * @return void
 */
void CC_ServiceListener_configVersionNotify(cc_string_t config_version,
		cc_string_t dial_plan_version,
		cc_string_t softkey_version);

/**
 * Notifies incoming service control command from CUCM
 * @param srv_ctrl_type request a reset or restart.
 * @return void
 */
void CC_ServiceListener_serviceControlNotify(cc_srv_ctrl_cmd_t srv_ctrl_type);


#endif /* _CC_SERVICE_LISTENER_H_ */
