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

#ifndef _SESSURI_H_
#define  _SESSURI_H_

#define MAX_LEN_SCHEME_INFO 256
#define MAX_STR_LEN_PARAM_TYPE 64
#define MAX_STR_LEN_PARAM_VAL  64

/*
 * Scheme names
 */
#define SCHEME_SIP "sip"
#define SCHEME_FILE "file"
#define SCHEME_RTP  "rtp"
#define SCHEME_RTSP "rtsp"
#define SCHEME_CAPTURE "capture"

/**
 *
 * Different URI types.
 *
 */
typedef enum {
    SCHEME_NONE=0,
    SIP_URI,
    FILE_URI,
    CAPTURE_URI,
    RTP_URI,
    RTSP_URI
} scheme_e;

/**
 * parameter tags/names
 * 
 */

#define LINE_TAG "line"

/**
 * Definitions for file player session
 */
#define CADENCE_TAG       "cadence"
#define MEDIA_TYPE_TAG    "media_type"
#define LOOP_COUNT_TAG    "loop_count"
#define PRIORITY_TAG      "priority"
#define FILEPTYPE_TAG     "type"

/**
 * Definitions for raw rtp session
 */
#define DIRECTION_TAG     "direction"
#define MULTICAST_TAG     "mcast"
#define PAYLOADTYPE_TAG   "payloadtype"
#define FRAMESIZE_TAG     "framesize"
#define VADENABLE_TAG     "vad"
#define PRECEDENCE_TAG    "precedence"
#define MIXINGMODE_TAG    "mode"
#define MIXINGPARTY_TAG   "party"
#define CHANNELTYPE_TAG   "channeltype"
#define LOCALADDRESS_TAG  "localaddress"
#define LOCALPORT_TAG     "localport"
#define ALGORITHM_TAG     "algorithm"

/**
 * Param types required for various URIs.
 *
 */
typedef enum {
    LINE_PARAM=0,
    MEDIA_TYPE_PARAM,
    CADENCE_PARAM,
    LOOP_COUNT_PARAM,
    PRIORITY_PARAM,
    MAX_QUERY_PARAM,
    DIRECTION_PARAM,
    MULTICAST_PARAM,
    PAYLOADTYPE_PARAM,
    FRAMESIZE_PARAM,
    VADENABLE_PARAM,
    PRECEDENCE_PARAM,
    MIXINGMODE_PARAM,
    MIXINGPARTY_PARAM,
    CHANNELTYPE_PARAM,
    LOCALADDRESS_PARAM,
    LOCALPORT_PARAM,
    ALGORITHM_PARAM,
    FILETYPE_PARAM
} param_e;

typedef enum {
    MEDIA_TYPE_AUDIO,
    MEDIA_TYPE_VIDEO,
    MEDIA_TYPE_AUDIO_VIDEO,
} media_type_e;

/**
 * params related to call sessions.
 *
 */
typedef struct {
    int           line_id;
    media_type_e  media_type;
} call_session_param_t;

/**
 * params related to capture sessions.
 *
 */
typedef struct {
    media_type_e  media_type;
} capture_session_param_t;

/**
 *
 * params related file sessions
 */
typedef struct {
    int type;
    int loop_count;
    int cadence;
    int priority;
} file_session_param_t;

typedef struct {
    int direction;
    int multicast;
    int payloadtype;
    int framesize;
    int vadenable;
    int precedence;
    int mixingmode;
    int mixingparty;
    int channeltype;
    int localaddress;
    int localport;
    int algorithm;
} raw_rtp_session_param_t;

/*
 * generic param type
 */
typedef union params {
    call_session_param_t call_session_param;
    file_session_param_t file_session_param;
    raw_rtp_session_param_t raw_session_param;
    capture_session_param_t capture_session_param;
} param_t;


/*
 * URI information. This is output of the Parser.
 *
 */
typedef struct uri_s {
    scheme_e     scheme;
    char         scheme_specific[MAX_LEN_SCHEME_INFO];
    union 
    {
        call_session_param_t call_session_param;
        file_session_param_t file_session_param;   
        raw_rtp_session_param_t raw_session_param;
    }param;
    
} uri_t;

int parse_uri(const char *uri, uri_t *uri_info);

#endif
