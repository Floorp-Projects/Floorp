/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __CCSDP_H__
#define __CCSDP_H__

#include "cpr_types.h"
#include "ccsdp_rtcp_fb.h"

#define SIPSDP_ILBC_MODE20 20

/**
 * Return codes for sdp helper APIs
 */
typedef enum rtp_ptype_
{
    RTP_NONE         = -1,
    RTP_PCMU         = 0,
    RTP_CELP         = 1,
    RTP_G726         = 2,
    RTP_GSM          = 3,
    RTP_G723         = 4,
    RTP_DVI4         = 5,
    RTP_DVI4_II      = 6,
    RTP_LPC          = 7,
    RTP_PCMA         = 8,
    RTP_G722         = 9,
    RTP_G728         = 15,
    RTP_G729         = 18,
    RTP_JPEG         = 26,
    RTP_NV           = 28,
    RTP_H261         = 31,
    RTP_H264_P0      = 97,
    RTP_H264_P1      = 126,
    RTP_TELEPHONE_EVENT = 101,
    RTP_L16          = 102,
    RTP_H263         = 103,
    RTP_ILBC         = 116, /* used only to make an offer */
    RTP_OPUS         = 109,
    RTP_VP8          = 120,
    RTP_VP9          = 121,
    RTP_RED          = 122,
    RTP_ULPFEC       = 123,
    RTP_I420         = 124,
    RTP_ISAC         = 124
} rtp_ptype;

typedef struct {
    const char *name;
    int         value;
} ccsdp_key_table_entry_t;

typedef enum max_coded_audio_bandwidth_ {
    opus_nb  = 0,    /* Narrowband */
    opus_mb  = 1,    /* Mediumband */
    opus_wb  = 2,    /* Wideband */
    opus_swb = 3,    /* Super-wideband */
    opus_fb  = 4   /* Fullband */
} max_coded_audio_bandwidth;

static const ccsdp_key_table_entry_t max_coded_audio_bandwidth_table[] = {
    {"nb",         opus_nb},
    {"mb",         opus_mb},
    {"wb",         opus_wb},
    {"swb",        opus_swb},
    {"fb",         opus_fb}
};

typedef enum {
    SDP_SUCCESS, /**< Success */
    SDP_FAILURE,
    SDP_INVALID_SDP_PTR,
    SDP_NOT_SDP_DESCRIPTION,
    SDP_INVALID_TOKEN_ORDERING,
    SDP_INVALID_PARAMETER,
    SDP_INVALID_MEDIA_LEVEL,
    SDP_INVALID_CAPABILITY,
    SDP_NO_RESOURCE,
    SDP_UNRECOGNIZED_TOKEN,
    SDP_NULL_BUF_PTR,
    SDP_POTENTIAL_SDP_OVERFLOW,
    SDP_EMPTY_TOKEN,
    SDP_MAX_RC
} sdp_result_e;

/**
 * Indicates invalid bandwidth value
 */
#define SDP_INVALID_VALUE          (-2)

/**
 * Bandwidth modifier type for b= SDP line
 */
typedef enum {
    SDP_BW_MODIFIER_INVALID = -1,
    SDP_BW_MODIFIER_AS, /** < b=AS: */
    SDP_BW_MODIFIER_CT, /** < b=CT: */
    SDP_BW_MODIFIER_TIAS, /** < b=TIAS: */
    SDP_MAX_BW_MODIFIER_VAL,
    SDP_BW_MODIFIER_UNSUPPORTED
} sdp_bw_modifier_e;

/**
 *  SDP attribute types
 */
/* Attribute Types */
typedef enum {
    SDP_ATTR_BEARER = 0,
    SDP_ATTR_CALLED,
    SDP_ATTR_CONN_TYPE,
    SDP_ATTR_DIALED,
    SDP_ATTR_DIALING,
    SDP_ATTR_DIRECTION,
    SDP_ATTR_EECID,
    SDP_ATTR_FMTP,
    SDP_ATTR_SCTPMAP,
    SDP_ATTR_FRAMING,
    SDP_ATTR_INACTIVE,
    SDP_ATTR_PTIME,
    SDP_ATTR_QOS,
    SDP_ATTR_CURR,
    SDP_ATTR_DES,
    SDP_ATTR_CONF,
    SDP_ATTR_RECVONLY,
    SDP_ATTR_RTPMAP,
    SDP_ATTR_SECURE,
    SDP_ATTR_SENDONLY,
    SDP_ATTR_SENDRECV,
    SDP_ATTR_SUBNET,
    SDP_ATTR_T38_VERSION,
    SDP_ATTR_T38_MAXBITRATE,
    SDP_ATTR_T38_FILLBITREMOVAL,
    SDP_ATTR_T38_TRANSCODINGMMR,
    SDP_ATTR_T38_TRANSCODINGJBIG,
    SDP_ATTR_T38_RATEMGMT,
    SDP_ATTR_T38_MAXBUFFER,
    SDP_ATTR_T38_MAXDGRAM,
    SDP_ATTR_T38_UDPEC,
    SDP_ATTR_X_CAP,
    SDP_ATTR_X_CPAR,
    SDP_ATTR_X_PC_CODEC,
    SDP_ATTR_X_PC_QOS,
    SDP_ATTR_X_QOS,
    SDP_ATTR_X_SQN,
    SDP_ATTR_TMRGWXID,
    SDP_ATTR_TC1_PAYLOAD_BYTES,
    SDP_ATTR_TC1_WINDOW_SIZE,
    SDP_ATTR_TC2_PAYLOAD_BYTES,
    SDP_ATTR_TC2_WINDOW_SIZE,
    SDP_ATTR_RTCP,
    SDP_ATTR_RTR,
    SDP_ATTR_SILENCESUPP,
    SDP_ATTR_SRTP_CONTEXT, /* version 2 sdescriptions */
    SDP_ATTR_MPTIME,
    SDP_ATTR_X_SIDIN,
    SDP_ATTR_X_SIDOUT,
    SDP_ATTR_X_CONFID,
    SDP_ATTR_GROUP,
    SDP_ATTR_MID,
    SDP_ATTR_SOURCE_FILTER,
    SDP_ATTR_RTCP_UNICAST,
    SDP_ATTR_MAXPRATE,
    SDP_ATTR_SQN,
    SDP_ATTR_CDSC,
    SDP_ATTR_CPAR,
    SDP_ATTR_SPRTMAP,
    SDP_ATTR_SDESCRIPTIONS,  /* version 9 sdescriptions */
    SDP_ATTR_LABEL,
    SDP_ATTR_FRAMERATE,
    SDP_ATTR_ICE_CANDIDATE,
    SDP_ATTR_ICE_UFRAG,
    SDP_ATTR_ICE_PWD,
    SDP_ATTR_ICE_LITE,
    SDP_ATTR_RTCP_MUX,
    SDP_ATTR_DTLS_FINGERPRINT,
    SDP_ATTR_MAXPTIME,
    SDP_ATTR_RTCP_FB,  /* RFC 4585 */
    SDP_ATTR_SETUP,
    SDP_ATTR_CONNECTION,
    SDP_ATTR_EXTMAP,  /* RFC 5285 */
    SDP_ATTR_IDENTITY,
    SDP_ATTR_MSID,
    SDP_ATTR_MSID_SEMANTIC,
    SDP_ATTR_BUNDLE_ONLY,
    SDP_ATTR_END_OF_CANDIDATES,
    SDP_ATTR_ICE_OPTIONS,
    SDP_ATTR_SSRC,
    SDP_ATTR_IMAGEATTR,
    SDP_ATTR_SIMULCAST,
    SDP_ATTR_RID,
    SDP_ATTR_DTLS_MESSAGE,
    SDP_ATTR_SCTPPORT,
    SDP_ATTR_MAXMESSAGESIZE,
    SDP_ATTR_SSRC_GROUP,
    SDP_MAX_ATTR_TYPES,
    SDP_ATTR_INVALID
} sdp_attr_e;

typedef enum {
    SDP_SETUP_NOT_FOUND = -1,
    SDP_SETUP_ACTIVE = 0,
    SDP_SETUP_PASSIVE,
    SDP_SETUP_ACTPASS,
    SDP_SETUP_HOLDCONN,
    SDP_MAX_SETUP,
    SDP_SETUP_UNKNOWN
} sdp_setup_type_e;

#endif
