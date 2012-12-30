/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SESSIONTYPES_H_
#define _SESSIONTYPES_H_

#include "string_lib.h"
#include "sessionConstants.h"
#include "ccsip_pmh.h"
#include "cc_constants.h"
#include "sip_ccm_transport.h"
#include "plat_api.h"

/*********************** SESSION ID *****************/
typedef unsigned int session_id_t ;

typedef struct {
  unsigned int  reason;
  string_t      reason_info;
} ccSessionProvider_cmd_t;

typedef struct {
  string_t sis_ver_name ;  //could be "cme" now.
  unsigned int  sis_ver_major;
  unsigned int  sis_ver_minor;
  unsigned int  sis_ver_addtnl;
}sis_ver;

typedef struct {
  unsigned int   cause;
  unsigned int   mode;
  sis_ver        sis_ver_info;
} ccProvider_state_t;

typedef struct {
  line_t  line_id;
  string_t dial;
} ccSession_create_param_t;

typedef struct {
  string_t                  info;
  string_t                  info1;
  unsigned int              state;
  cc_jsep_action_t          action;
  cc_media_stream_id_t      stream_id;
  cc_media_track_id_t       track_id;
  cc_media_type_t           media_type;
  cc_level_t                level;
  cc_media_constraints_t *  constraints;
} ccSession_feature_t;

typedef struct {
  int          state;
  int          attr;
  int          inst;
  line_t       line_id;
  int          cause;
  string_t 	   sdp;
  unsigned int media_stream_id;
  unsigned int media_stream_track_id;
} cc_call_state_data_t;
/* CALL_SESSION_CREATED shall use the call_state as data*/

typedef struct
{
  string_t      cldNum;
  string_t      cldName;
} cc_placed_call_info_t;

typedef struct
{
  string_t clgName;
  string_t clgNumber;
  string_t altClgNumber;
  boolean dispClgNumber;
  string_t cldName;
  string_t cldNumber;
  boolean dispCldNumber;
  string_t origCalledName;
  string_t origCalledNumber;
  string_t lastRedirectingName;
  string_t lastRedirectingNumber;
  unsigned short call_type;
  unsigned short instance_id;
  int      security;
  int      policy;
} cc_callinfo_t;

typedef struct {
    string_t     featSet;
    int          featMask[MAX_SOFT_KEYS];
} cc_featurekey_set_t;

typedef struct {
    cc_boolean      start;
    vcm_ring_mode_t mode;
    cc_boolean      once;
} cc_ringer_state_t;

/**
 * Define call status to carry over timeout/priority that might be sent from CUCM.
 * Note: if the values of timeout and priority are zero, then 2 second is the
 *      derfault value for the timeout. It's mostly the application based on UI
 *      design.
 */
typedef struct {
    string_t    status;
    int         timeout;
    int         priority;
} cc_call_status_t;

typedef struct
{
  union {
    cc_call_state_data_t  state_data;
    cc_placed_call_info_t plcd_info;
    cc_callinfo_t         call_info;
    cc_call_status_t      status;
    char                  gcid[CC_MAX_GCID];
    int                   action;
    int                   security;
    cc_featurekey_set_t   feat_set;
    unsigned int          target_sess_id;
    unsigned int          callref;
    string_t              recv_info_list;
    cc_ringer_state_t     ringer;
  } data;
} ccSession_update_t;

typedef struct {
  line_t line;
  unsigned int  info;
} cc_line_data_t;

typedef struct {
  int           state;
  int           info;
} cc_feature_state_t;

typedef struct {
  cc_blf_state_t  state;
  int             request_id;
  int             app_id;
} cc_feature_blf_state_t;

typedef struct {
  int           timeout;
  boolean       notifyProgress;
  char          priority;
  string_t      prompt;
} cc_notification_data_t;

typedef struct {
  line_t        line;
  unsigned char button;
  string_t      speed;
  string_t      label;
} cc_label_n_speed_t;

typedef struct {
  string_t      cfg_ver;
  string_t      dp_ver;
  string_t      softkey_ver;
} cc_cfg_version_t;

typedef struct {
  line_t        line;
  boolean       isFwd;
  boolean       isLocal;
  string_t      cfa_num;
} cc_cfwd_status_t;

typedef struct {
  string_t      addr;
  int           status;
} cc_ccm_conn_t;

typedef struct {
  line_t        line;
  boolean       status;
  int  type;
  int  newCount;
  int  oldCount;
  int  hpNewCount;
  int  hpOldCount;
} cc_mwi_status_t;

typedef struct {
  union {
    cc_line_data_t line_info;     // For line specific features
    cc_feature_state_t state_data; // For device specific feature
    cc_feature_blf_state_t blf_data; // For blf state updates.
    cc_notification_data_t notification;
    cc_label_n_speed_t  cfg_lbl_n_spd;
    cc_cfwd_status_t    cfwd;     // For CFWD ALL feature
    cc_ccm_conn_t       ccm_conn;
    cc_mwi_status_t     mwi_status;
    unsigned int        reset_type;
    cc_cfg_version_t    cfg_ver_data;
  } data;
} ccFeature_update_t;

typedef struct {
  int           data;
} ccSessionCmd_t;

/*********************** STREAM SESSION TYPES *****************/

typedef struct {
  unsigned int  reason;
} scSessionProvider_cmd_t;

typedef struct {
  unsigned int  mode;
} scSession_state_t;

typedef struct {
  line_t    line_id;
} scSession_create_param_t;

typedef struct {
  string_t info;
} scSession_feature_t;

typedef struct {
  string_t info;
} scProvider_state_t;

typedef struct {
  int type;
  int mcap_id;
  int group_id;
  int stream_id;
  int call_id;
  int direction;
  int ref_count;
  int session_handle;//session handle for ms rtp session
} rtp_session_info;

typedef struct {
  int refcount;
} rtp_session_update;

typedef struct {
  union {
    int state;
    rtp_session_info rtp_info;
    rtp_session_update rtp_update;
  } data;
} scSession_update_t;

typedef struct {
  string_t info;
} scFeature_update_t;

typedef struct {
  string_t info;
} scSessionCmd_t;

typedef struct {
  int id;
  int data;
} rcFeature_update_t;

/********************** SESSION TYPES ****************************/


typedef struct {
  unsigned int sessionType;
  unsigned int  cmd;
  union {
    ccSessionProvider_cmd_t ccData;
    scSessionProvider_cmd_t scData;
  } cmdData;
} sessionProvider_cmd_t;

typedef struct {
  unsigned int sessionType;
  unsigned int  state;
  union {
    ccProvider_state_t ccData;
    scProvider_state_t scData;
  } stateData;
} provider_state_t;

typedef struct {
  unsigned int sessionType;
  string_t uri;
  union {
    ccSession_create_param_t ccData;
    scSession_create_param_t scData;
  } createData;
} session_create_param_t;

typedef struct {
  unsigned int session_id;
  unsigned int featureID;
  union {
    ccSession_feature_t ccData;
    scSession_feature_t scData;
  } featData;
} session_feature_t;

typedef struct {
  unsigned int sessionID;
  unsigned int eventID;
  unsigned int sessType;
  union {
    ccSession_update_t ccSessionUpd;
    scSession_update_t scSessionUpd;
  } update;
}session_update_t;

typedef struct {
  unsigned int sessID;
  unsigned int cmd;
  union {
    ccSessionCmd_t ccCmd;
    scSessionCmd_t scCmd;
  } cmdData;
}sessionCmd_t;


typedef struct {
  unsigned int sessionType;
  unsigned int featureID;
  union {
    ccFeature_update_t ccFeatUpd;
    scFeature_update_t scFeatUpd;
    rcFeature_update_t rcFeatUpd;
  } update;
}feature_update_t;


typedef struct {
  string_t config_version_stamp;
  string_t dialplan_version_stamp;
  string_t fcp_version_stamp;
  string_t cucm_result;
  string_t load_id;
  string_t inactive_load_id;
  string_t load_server;
  string_t log_server;
  boolean ppid;
} session_mgmt_config_t;

typedef struct {
    int result;
} session_mgmt_apply_config_result_t;

typedef struct {
  long gmt_time;
} session_mgmt_time_t;

typedef struct {
  int ret_val;
  int ndx;
  char *outstr;
  uint32_t len;
} session_mgmt_phrase_text_t;

typedef struct {
  int unreg_reason;
} session_mgmt_unreg_reason_t;

typedef struct {
  int kpml_val;
} session_mgmt_kpmlconfig_t;

typedef struct {
  int enabled;
  plat_audio_device_t device_type;
} session_mgmt_audio_device_status_t;

typedef struct {
  boolean enabled;
} session_mgmt_speaker_headset_mode_t;

typedef struct {
  boolean ret_val;
  line_t line;
} session_mgmt_line_mwi_active_t;

typedef struct {
  string_t uri;
} session_mgmt_uri_t;

typedef struct {
  session_mgmt_func_e func_id;
  union {
    session_mgmt_config_t config;
    session_mgmt_apply_config_result_t apply_config_result;
    session_mgmt_time_t time;
    session_mgmt_phrase_text_t phrase_text;
    session_mgmt_unreg_reason_t unreg_reason;
    session_mgmt_kpmlconfig_t kpmlconfig;
    session_mgmt_audio_device_status_t audio_device_status;
    session_mgmt_speaker_headset_mode_t speaker_headset_mode;
    session_mgmt_line_mwi_active_t line_mwi_active;
    session_mgmt_uri_t uri;
  } data;
} session_mgmt_t;


typedef struct {
  string_t info_package;
  string_t content_type;
  string_t message_body;
} info_generic_raw_t;

typedef struct {
  unsigned int sessionID;
  info_generic_raw_t generic_raw;
} session_send_info_t;

typedef struct {
  unsigned int sessionID;
  int packageID;
  union {
    info_generic_raw_t generic_raw;
  } info;
} session_rcvd_info_t;

#endif

