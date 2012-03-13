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

/**
 *  @mainpage Portable SIP Stack API
 *
 *  @section intro_sec Introduction
 *  The portable SIP stack is used in multiple SIP endpoints. This document
 *  describes the API's provided by the portable SIP stack that third party
 *  vendors must implement to use the stack. 
 *
 *  @section hlapi NG APIs
 *  This API provides 3 main sets of APIs (device, line. call) and provides ways to
 *  invoke actions on these and get status and even callbacks
 * 
 *  @subsection Management
 *    @li cc_service.h @par
 *
 *  @subsection Call
 *    @li ccapi_call.h @par
 *    @li ccapi_call_info.h @par
 *    @li ccapi_call_listener.h @par to be implemented by the vendor for events related to call
 * 
 *  @subsection Line
 *    @li ccapi_line.h @par
 *    @li ccapi_line_info.h @par
 *    @li ccapi_line_listener.h @par to be implemented by the vendor for events related to line
 * 
 *  @subsection Device
 *    @li ccapi_device.h @par
 *    @li ccapi_device_info.h @par
 *    @li ccapi_device_listener.h @par to be implemented by the vendor for events related to device
 * 
 *  @subsection Misc
 *    @li ccapi_types.h @par
 *    @li cc_types.h @par
 *    @li cc_constants.h @par
 *
 */

#ifndef _CCAPI_TYPES_H_
#define _CCAPI_TYPES_H_

#include "cc_constants.h"

/**
 *  Device ID typedef
 */
typedef unsigned int cc_device_handle_t;
/**
 * File location
 */
typedef const char *file_path_t;
/**
 *  Device info reference typedef
 */
typedef struct cc_device_info_t_* cc_deviceinfo_ref_t;
/**
 *  Line info reference typedef
 */
typedef struct cc_line_info_t_ * cc_lineinfo_ref_t;
/**
 *  Feature info reference typedef
 */
typedef struct cc_feature_info_t_ * cc_featureinfo_ref_t;
/**
 *  Call info reference typedef
 */
typedef struct cc_call_info_t_* cc_callinfo_ref_t;
/**
 *  CAll server info reference typedef
 */
typedef struct cc_call_server_t_* cc_callserver_ref_t;

/**
 *  CallLog reference typedef
 */
typedef struct cc_call_log_t_* cc_calllog_ref_t;

#define CCAPI_MAX_SERVERS 4
/**
 * Call event types
 */
typedef enum {
  CCAPI_CALL_EV_CREATED,
  CCAPI_CALL_EV_STATE,
  CCAPI_CALL_EV_CALLINFO,
  CCAPI_CALL_EV_ATTR,
  CCAPI_CALL_EV_SECURITY,
  CCAPI_CALL_EV_LOG_DISP,
  CCAPI_CALL_EV_PLACED_CALLINFO,
  CCAPI_CALL_EV_STATUS,
  CCAPI_CALL_EV_SELECT,
  CCAPI_CALL_EV_LAST_DIGIT_DELETED,
  CCAPI_CALL_EV_GCID,
  CCAPI_CALL_EV_XFR_OR_CNF_CANCELLED,
  CCAPI_CALL_EV_PRESERVATION,
  CCAPI_CALL_EV_CAPABILITY,
  CCAPI_CALL_EV_VIDEO_AVAIL,
  CCAPI_CALL_EV_VIDEO_OFFERED,
  CCAPI_CALL_EV_RECEIVED_INFO,
  CCAPI_CALL_EV_RINGER_STATE,
  CCAPI_CALL_EV_CONF_PARTICIPANT_INFO,
  CCAPI_CALL_EV_MEDIA_INTERFACE_UPDATE_BEGIN,
  CCAPI_CALL_EV_MEDIA_INTERFACE_UPDATE_SUCCESSFUL,
  CCAPI_CALL_EV_MEDIA_INTERFACE_UPDATE_FAIL
} ccapi_call_event_e;

/**
 * Line event types
 */
typedef enum {
  CCAPI_LINE_EV_CONFIG_CHANGED,
  CCAPI_LINE_EV_REG_STATE,
  CCAPI_LINE_EV_CAPSET_CHANGED,
  CCAPI_LINE_EV_CFWDALL,
  CCAPI_LINE_EV_MWI
} ccapi_line_event_e;

/**
 * Device event types
 */
typedef enum {
  CCAPI_DEVICE_EV_CONFIG_CHANGED,
  CCAPI_DEVICE_EV_STATE, // INS/OOS
  CCAPI_DEVICE_EV_IDLE_SET,
  CCAPI_DEVICE_EV_MWI_LAMP,
  CCAPI_DEVICE_EV_NOTIFYPROMPT,
  CCAPI_DEVICE_EV_SERVER_STATUS,
  CCAPI_DEVICE_EV_BLF,
  CCAPI_DEVICE_EV_CAMERA_ADMIN_CONFIG_CHANGED,
  CCAPI_DEVICE_EV_VIDEO_CAP_ADMIN_CONFIG_CHANGED
} ccapi_device_event_e;

/**
 * Call capability feature IDs
 */
typedef enum {
  CCAPI_CALL_CAP_NEWCALL,
  CCAPI_CALL_CAP_ANSWER,
  CCAPI_CALL_CAP_ENDCALL,
  CCAPI_CALL_CAP_HOLD,
  CCAPI_CALL_CAP_RESUME,
  CCAPI_CALL_CAP_CALLFWD,
  CCAPI_CALL_CAP_DIAL,
  CCAPI_CALL_CAP_BACKSPACE,
  CCAPI_CALL_CAP_SENDDIGIT,
  CCAPI_CALL_CAP_TRANSFER,
  CCAPI_CALL_CAP_CONFERENCE,
  CCAPI_CALL_CAP_SWAP,
  CCAPI_CALL_CAP_BARGE,
  CCAPI_CALL_CAP_REDIAL,
  CCAPI_CALL_CAP_JOIN,
  CCAPI_CALL_CAP_SELECT,
  CCAPI_CALL_CAP_RMVLASTPARTICIPANT,
  CCAPI_CALL_CAP_MAX
} ccapi_call_capability_e;

#endif /* _CCAPIAPI_TYPES_H_ */
