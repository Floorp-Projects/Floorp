/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCAPI_SNAPSHOT_H_
#define _CCAPI_SNAPSHOT_H_

#include "ccsip_platform.h"
#include "prot_configmgr.h"
#include "ccapi_line.h"

/*
 * MWI info
 */
typedef struct cc_mwi_info_t_ {
  cc_uint32_t     status;
  cc_uint32_t     type;
  cc_uint32_t     new_count;
  cc_uint32_t     old_count;
  cc_uint32_t     pri_new_count;
  cc_uint32_t     pri_old_count;
} cc_mwi_info_t;

/*
 * line reference data structure
 */
typedef struct cc_line_info_t_ {
  cc_uint32_t     ref_count;
  cc_uint32_t     line_id;
  cc_uint32_t     line_type;
  cc_int32_t     button;
  cc_boolean      reg_state;
  cc_boolean      isCFWD;
  cc_boolean      isLocalCFWD;
  cc_mwi_info_t   mwi;
  cc_string_t     name;
  cc_string_t     dn;
  cc_string_t     cfwd_dest;
  cc_boolean    allowed_features[CCAPI_CALL_CAP_MAX];
  cc_string_t     externalNumber;
  cc_boolean      fwd_caller_name_display;
  cc_boolean      fwd_caller_number_display;
  cc_boolean      fwd_redirected_number_display;
  cc_boolean      fwd_dialed_number_display;
} cc_line_info_t;

typedef struct cc_feature_info_t_ {
  cc_int32_t     feature_id;
  cc_int32_t     button;
  cc_string_t     speedDialNumber;
  cc_string_t     contact;
  cc_string_t     name;
  cc_string_t     retrievalPrefix;
  cc_uint32_t     featureOptionMask;
  cc_blf_state_t  blf_state;
} cc_feature_info_t;

typedef struct cc_call_server_t_ {
  cc_string_t     name;
  cc_ccm_status_t status;
  cc_int32_t      type;
} cc_call_server_t;

typedef struct cc_device_info_t_ {
  cc_uint32_t     ref_count;
  cc_string_t     not_prompt;
  char            registration_ip_addr[MAX_IPADDR_STR_LEN];
  cc_int32_t      not_prompt_prio;
  cc_boolean      not_prompt_prog;
  cc_boolean      mwi_lamp;
  cc_cucm_mode_t  cucm_mode;
  cc_service_state_t  ins_state;
  cc_service_cause_t  ins_cause;
  long long                reg_time;
  cc_call_server_t    ucm[CCAPI_MAX_SERVERS];
} cc_device_info_t;

typedef enum {
    ACCSRY_CFGD_CFG,   //accessory last configured by configuration file.
    ACCSRY_CFGD_APK,   //accessory last configured by another application.
} accsry_cfgd_by_t;

typedef struct accessory_cfg_info_t_ {
    accsry_cfgd_by_t camera;
    accsry_cfgd_by_t video;
} accessory_cfg_info_t;

extern accessory_cfg_info_t g_accessoryCfgInfo;
extern cc_device_info_t g_deviceInfo;
extern cc_line_info_t lineInfo[MAX_CONFIG_LINES+1];

cc_line_info_t* ccsnap_getLineInfo(int lineID);
cc_line_info_t* ccsnap_getLineInfoFromBtn(int btnID);
void ccsnap_line_init();
void ccsnap_line_free();
void ccsnap_device_init();
void ccsnap_device_free();
void ccsnap_gen_deviceEvent(ccapi_device_event_e event, cc_device_handle_t handle);
void ccsnap_gen_lineEvent(ccapi_line_event_e event, cc_lineid_t handle);
void ccsnap_gen_callEvent(ccapi_call_event_e event, cc_call_handle_t handle);
void ccsnap_update_ccm_status(cc_string_t addr, cc_ccm_status_t status);
void ccsnap_handle_mnc_reached (cc_line_info_t *line_info,
		 cc_boolean mnc_reached, cc_cucm_mode_t mode);
cc_feature_info_t* ccsnap_getFeatureInfo(int featureIndex);
void ccsnap_gen_blfFeatureEvent(cc_blf_state_t state, int appId);
cc_string_t ccsnap_EscapeStrToLocaleStr(cc_string_t destination, cc_string_t source, int len);
void ccsnap_set_phone_services_provisioning(boolean misd, boolean plcd, boolean rcvd);
boolean ccsnap_isMissedCallLoggingEnabled();
boolean ccsnap_isReceivedCallLoggingEnabled();
boolean ccsnap_isPlacedCallLoggingEnabled();
void ccsnap_set_line_label(int btn, cc_string_t label);
cc_string_t ccsnap_get_line_label(int btn);

#endif

