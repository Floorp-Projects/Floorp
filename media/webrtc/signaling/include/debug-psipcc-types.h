/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
