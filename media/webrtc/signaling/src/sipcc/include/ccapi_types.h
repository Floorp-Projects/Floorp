/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
