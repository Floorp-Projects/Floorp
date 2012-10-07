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

#ifndef __CCPROVIDER_H__
#define __CCPROVIDER_H__

#include "cpr_types.h"
#include "cpr_ipc.h"
#include "cpr_socket.h"
#include "cpr_memory.h"
#include "cpr_errno.h"
#include "cpr_in.h"
#include "cpr_rand.h"
#include "cpr_string.h"
#include "cpr_threads.h"
#include "phone_types.h"
#include "session.h"

#include "cc_constants.h"
#include "ccapi_types.h"
#include "conf_roster.h"

#define CC_DEVICE_ID 0
#include "ccapi_service.h"

typedef enum {
    FAILOVER,
    FALLBACK,
    NO_CUCM_SRST_AVAILABLE,
    NONE_AVAIL,
} Cucm_mode_t;

typedef struct
{
	cc_line_reg_state_t state;
    Cucm_mode_t    cucm_mode;
    cc_boolean     inPreservation;
	session_id_t   preservID;
    unsigned int   mode;
    unsigned int   cause;
} CCAppGlobal_t;

typedef struct cc_call_log_t_ {
  string_t localPartyName;
  string_t localPartyNumber;
  string_t remotePartyName[2];
  string_t remotePartyNumber[2];
  string_t altPartyNumber[2];
  cc_log_disposition_t logDisp;
  cc_call_state_t callState;
  time_t startTime;
  cc_uint32_t duration;
} cc_call_log_t;

typedef struct cc_call_info_t_{
    uint32_t     ref_count;
    session_id_t  sess_id;
    line_t        line;
    callid_t      id;
    uint16_t      inst;
    cc_call_state_t    state;
    cc_call_attr_t     attr;
    cc_call_type_t     type;
    cc_call_security_t security;
    cc_call_policy_t   policy;
    unsigned int  callref;
    int           isSelected;
    unsigned int  log_disp;
    string_t      clg_name;
    string_t      clg_number;
    string_t      alt_number;
    string_t      cld_name;
    string_t      cld_number;
    string_t      orig_called_name;
    string_t      orig_called_number;
    string_t      last_redir_name;
    string_t      last_redir_number;
    string_t      plcd_name;
    string_t      plcd_number;
    string_t      status;
    char          gci[CC_MAX_GCID];
    cc_int32_t    cause;
    cc_int32_t    vid_dir;
    cc_int32_t    vid_offer;
    cc_boolean    is_conf;
    cc_boolean    ringer_start;
    cc_boolean    ringer_once;
    cc_int32_t    ringer_mode;
    cc_boolean    allowed_features[CCAPI_CALL_CAP_MAX];
    cc_string_t   info_package;
    cc_string_t   info_type;
    cc_string_t   info_body;
    cc_call_log_t call_log;
    cc_boolean    audio_mute;
    cc_boolean    video_mute;
    cc_call_conference_Info_t call_conference;
    cc_string_t   sdp;
    unsigned int  media_stream_track_id;
    unsigned int  media_stream_id;

} session_data_t;

typedef enum {
    NO_ACTION=0,
    RESET_ACTION,
    RESTART_ACTION,
    RE_REGISTER_ACTION,
    STOP_ACTION,
    DESTROY_ACTION
} cc_action_t;

#define CCAPP_SERVICE_CMD    1
#define CCAPP_CREATE_SESSION 2
#define CCAPP_CLOSE_SESSION  3
#define CCAPP_INVOKE_FEATURE 4
#define CCAPP_SESSION_UPDATE 5
#define CCAPP_FEATURE_UPDATE 6
#define CCAPP_UPDATELINES    7
#define CCAPP_FAILOVER_IND   8
#define CCAPP_FALLBACK_IND   9
#define CCAPP_MODE_NOTIFY    10
#define CCAPP_SHUTDOWN_ACK   11
#define CCAPP_REG_ALL_FAIL   12
#define CCAPP_INVOKEPROVIDER_FEATURE 13
#define CCAPP_SEND_INFO      14
#define CCAPP_RCVD_INFO      15
#define CCAPP_LOGOUT_RESET   16
#define CCAPP_THREAD_UNLOAD  17
#define CCAPP_SESSION_MGMT   18

extern cpr_status_e ccappTaskPostMsg(unsigned int msgId, void * data, uint16_t len, int appId);
extern void ccappSyncSessionMgmt(session_mgmt_t *sessMgmt);
extern void CCApp_task(void * arg);
extern void *findhash(unsigned int key);
extern session_id_t createSessionId(line_t line, callid_t call);
extern void getLineIdAndCallId (line_t *line_id, callid_t *call_id);
extern void ccp_handler(void* msg, int type);
extern session_data_t * getDeepCopyOfSessionData(session_data_t *data);
extern void cleanSessionData(session_data_t *data);
extern cc_call_handle_t ccappGetConnectedCall();

#endif

