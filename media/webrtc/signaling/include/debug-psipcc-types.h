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

#ifndef T1_DEBUG_H
#define T1_DEBUG_H

#include "CC_Common.h"

extern "C"
{
#include <ccapi_service.h>
#include "ccapi_types.h"

ECC_API cc_string_t device_event_getname(ccapi_device_event_e);
ECC_API cc_string_t call_event_getname(ccapi_call_event_e);
ECC_API cc_string_t line_event_getname(ccapi_line_event_e);

ECC_API cc_string_t digit_getname(cc_digit_t);
ECC_API cc_string_t cucm_mode_getname(cc_cucm_mode_t);
ECC_API cc_string_t line_feature_getname(cc_line_feature_t);
ECC_API cc_string_t feature_option_mask_getname(cc_feature_option_mask_t);
ECC_API cc_string_t service_cause_getname(cc_service_cause_t);
ECC_API cc_string_t service_state_getname(cc_service_state_t);
ECC_API cc_string_t ccm_status_getname(cc_ccm_status_t);
ECC_API cc_string_t line_reg_state_getname(cc_line_reg_state_t);
ECC_API cc_string_t shutdown_reason_getname(cc_shutdown_reason_t);
ECC_API cc_string_t kpml_config_getname(cc_kpml_config_t);
ECC_API cc_string_t upgrade_getname(cc_upgrade_t);
ECC_API cc_string_t sdp_direction_getname(cc_sdp_direction_t);
ECC_API cc_string_t blf_state_getname(cc_blf_state_t);
ECC_API cc_string_t blf_feature_mask_getname(cc_blf_feature_mask_t);
ECC_API cc_string_t call_state_getname(cc_call_state_t);
ECC_API cc_string_t call_attr_getname(cc_call_attr_t);
ECC_API cc_string_t hold_reason_getname(cc_hold_reason_t);
ECC_API cc_string_t call_type_getname(cc_call_type_t);
ECC_API cc_string_t call_security_getname(cc_call_security_t);
ECC_API cc_string_t call_policy_getname(cc_call_policy_t);
ECC_API cc_string_t log_disposition_getname(cc_log_disposition_t);
ECC_API cc_string_t call_priority_getname(cc_call_priority_t);
ECC_API cc_string_t call_selection_getname(cc_call_selection_t);
ECC_API cc_string_t call_capability_getname(ccapi_call_capability_e);
ECC_API cc_string_t srv_ctrl_req_getname(cc_srv_ctrl_req_t);
ECC_API cc_string_t srv_ctrl_cmd_getname(cc_srv_ctrl_cmd_t);
ECC_API cc_string_t message_type_getname(cc_message_type_t);
ECC_API cc_string_t lamp_state_getname(cc_lamp_state_t);
ECC_API cc_string_t cause_getname(cc_cause_t);
ECC_API cc_string_t subscriptions_ext_getname(cc_subscriptions_ext_t);
ECC_API cc_string_t apply_config_result_getname(cc_apply_config_result_t);

}

#endif /* T1_DEBUG_H */
