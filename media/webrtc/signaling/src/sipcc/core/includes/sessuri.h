/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
