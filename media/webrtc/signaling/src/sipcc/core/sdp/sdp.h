/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SDP_H_
#define _SDP_H_

#include "cc_constants.h"
#include "sdp_os_defs.h"
#include "ccsdp.h"

/* SDP Defines */

/* The following defines are used to indicate params that are specified
 * as the choose parameter or parameters that are invalid.  These can
 * be used where the value required is really a u16, but is represented
 * by an int32.
 */
#define SDP_CHOOSE_PARAM           (-1)
#define SDP_SESSION_LEVEL        0xFFFF

#define UNKNOWN_CRYPTO_SUITE              "UNKNOWN_CRYPTO_SUITE"
#define AES_CM_128_HMAC_SHA1_32           "AES_CM_128_HMAC_SHA1_32"
#define AES_CM_128_HMAC_SHA1_80           "AES_CM_128_HMAC_SHA1_80"
#define F8_128_HMAC_SHA1_80               "F8_128_HMAC_SHA1_80"

/*
 * SDP_SRTP_MAX_KEY_SIZE_BYTES
 *  Maximum size for a SRTP Master Key in bytes.
 */
#define SDP_SRTP_MAX_KEY_SIZE_BYTES  16
/*
 * SDP_SRTP_MAX_SALT_SIZE_BYTES
 *  Maximum size for a SRTP Master Salt in bytes.
 */
#define SDP_SRTP_MAX_SALT_SIZE_BYTES 14
/*
 * SDP_SRTP_MAX_MKI_SIZE_BYTES
 *  Maximum size for a SRTP Master Key Index in bytes.
 */
#define SDP_SRTP_MAX_MKI_SIZE_BYTES   4

/* Max number of characters for Lifetime */
#define SDP_SRTP_MAX_LIFETIME_BYTES 16

#define SDP_SDESCRIPTIONS_KEY_SIZE_UNKNOWN      0
#define SDP_SRTP_CRYPTO_SELECTION_FLAGS_UNKNOWN 0

/*
 * SRTP_CONTEXT_SET_*
 *  Set a SRTP Context field flag
 */
#define SDP_SRTP_ENCRYPT_MASK           0x00000001
#define SDP_SRTP_AUTHENTICATE_MASK      0x00000002
#define SDP_SRTCP_ENCRYPT_MASK          0x00000004
#define SDP_SRTCP_SSRC_MASK             0x20000000
#define SDP_SRTCP_ROC_MASK              0x10000000
#define SDP_SRTCP_KDR_MASK              0x08000000
#define SDP_SRTCP_KEY_MASK              0x80000000
#define SDP_SRTCP_SALT_MASK             0x40000000

#define SDP_SRTP_CONTEXT_SET_SSRC(cw)          ((cw) |= SDP_SRTCP_SSRC_MASK)
#define SDP_SRTP_CONTEXT_SET_ROC(cw)           ((cw) |= SDP_SRTCP_ROC_MASK)
#define SDP_SRTP_CONTEXT_SET_KDR(cw)           ((cw) |= SDP_SRTCP_KDR_MASK)
#define SDP_SRTP_CONTEXT_SET_MASTER_KEY(cw)    ((cw) |= SDP_SRTCP_KEY_MASK)
#define SDP_SRTP_CONTEXT_SET_MASTER_SALT(cw)   ((cw) |= SDP_SRTCP_SALT_MASK)
#define SDP_SRTP_CONTEXT_SET_ENCRYPT_AUTHENTICATE(cw) \
     ((cw) |= (SDP_SRTP_ENCRYPT_MASK | SDP_SRTP_AUTHENTICATE_MASK | \
               SDP_SRTCP_ENCRYPT_MASK))

#define SDP_SRTP_CONTEXT_RESET_SSRC(cw)     ((cw) &= ~(SDP_SRTCP_SSRC_MASK))
#define SDP_SRTP_CONTEXT_RESET_ROC(cw)      ((cw) &= ~(SDP_SRTCP_ROC_MASK))
#define SDP_SRTP_CONTEXT_RESET_KDR(cw)      ((cw) &= ~(SDP_SRTCP_KDR_MASK))
#define SDP_CONTEXT_RESET_MASTER_KEY(cw)    ((cw) &= ~(SDP_SRTCP_KEY_MASK))
#define SDP_CONTEXT_RESET_MASTER_SALT(cw)   ((cw) &= ~(SDP_SRTCP_SALT_MASK))

/* SDP Enum Types */

typedef enum {
    SDP_DEBUG_TRACE,
    SDP_DEBUG_WARNINGS,
    SDP_DEBUG_ERRORS,
    SDP_MAX_DEBUG_TYPES
} sdp_debug_e;

typedef enum {
    SDP_CHOOSE_CONN_ADDR,
    SDP_CHOOSE_PORTNUM,
    SDP_MAX_CHOOSE_PARAMS
} sdp_choose_param_e;


/* Token Lines - these must be in the same order they should
 *               appear in an SDP.
 */
typedef enum {
    SDP_TOKEN_V = 0,
    SDP_TOKEN_O,
    SDP_TOKEN_S,
    SDP_TOKEN_I,
    SDP_TOKEN_U,
    SDP_TOKEN_E,
    SDP_TOKEN_P,
    SDP_TOKEN_C,
    SDP_TOKEN_B,
    SDP_TOKEN_T,
    SDP_TOKEN_R,
    SDP_TOKEN_Z,
    SDP_TOKEN_K,
    SDP_TOKEN_A,
    SDP_TOKEN_M,
    SDP_MAX_TOKENS
} sdp_token_e;

/* Media Types */
typedef enum {
    SDP_MEDIA_AUDIO = 0,
    SDP_MEDIA_VIDEO,
    SDP_MEDIA_APPLICATION,
    SDP_MEDIA_DATA,
    SDP_MEDIA_CONTROL,
    SDP_MEDIA_NAS_RADIUS,
    SDP_MEDIA_NAS_TACACS,
    SDP_MEDIA_NAS_DIAMETER,
    SDP_MEDIA_NAS_L2TP,
    SDP_MEDIA_NAS_LOGIN,
    SDP_MEDIA_NAS_NONE,
    SDP_MEDIA_TEXT,
    SDP_MEDIA_IMAGE,
    SDP_MAX_MEDIA_TYPES,
    SDP_MEDIA_UNSUPPORTED,
    SDP_MEDIA_INVALID
} sdp_media_e;


/* Connection Network Type */
typedef enum {
    SDP_NT_INTERNET = 0,              /*  0 -> IP - In SDP "IN" is defined */
			              /*       to mean "Internet"          */
    SDP_NT_ATM,	                      /*  1 -> ATM                         */
    SDP_NT_FR,                        /*  2 -> FRAME RELAY                 */
    SDP_NT_LOCAL,                     /*  3 -> local                       */
    SDP_MAX_NETWORK_TYPES,
    SDP_NT_UNSUPPORTED,
    SDP_NT_INVALID
} sdp_nettype_e;


/* Address Type  */
typedef enum {
    SDP_AT_IP4 = 0,                   /* 0 -> IP Version 4 (IP4)           */
    SDP_AT_IP6,                       /* 1 -> IP Version 6 (IP6)           */
    SDP_AT_NSAP,                      /* 2 -> 20 byte NSAP address         */
    SDP_AT_EPN,                       /* 3 -> 32 bytes of endpoint name    */
    SDP_AT_E164,                      /* 4 -> 15 digit decimal number addr */
    SDP_AT_GWID,                      /* 5 -> Private gw id. ASCII string  */
    SDP_MAX_ADDR_TYPES,
    SDP_AT_UNSUPPORTED,
    SDP_AT_FQDN,
    SDP_AT_INVALID
} sdp_addrtype_e;


/* Transport Types */

#define SDP_MAX_PROFILES 3

typedef enum {
    SDP_TRANSPORT_RTPAVP = 0,
    SDP_TRANSPORT_UDP,
    SDP_TRANSPORT_UDPTL,
    SDP_TRANSPORT_CES10,
    SDP_TRANSPORT_LOCAL,
    SDP_TRANSPORT_AAL2_ITU,
    SDP_TRANSPORT_AAL2_ATMF,
    SDP_TRANSPORT_AAL2_CUSTOM,
    SDP_TRANSPORT_AAL1AVP,
    SDP_TRANSPORT_UDPSPRT,
    SDP_TRANSPORT_RTPSAVP,
    SDP_TRANSPORT_TCP,
    SDP_TRANSPORT_RTPSAVPF,
    SDP_TRANSPORT_DTLSSCTP,
    SDP_MAX_TRANSPORT_TYPES,
    SDP_TRANSPORT_UNSUPPORTED,
    SDP_TRANSPORT_INVALID
} sdp_transport_e;


/* Encryption KeyType */
typedef enum {
    SDP_ENCRYPT_CLEAR,                /* 0 -> Key given in the clear       */
    SDP_ENCRYPT_BASE64,               /* 1 -> Base64 encoded key           */
    SDP_ENCRYPT_URI,                  /* 2 -> Ptr to URI                   */
    SDP_ENCRYPT_PROMPT,               /* 3 -> No key included, prompt user */
    SDP_MAX_ENCRYPT_TYPES,
    SDP_ENCRYPT_UNSUPPORTED,
    SDP_ENCRYPT_INVALID
} sdp_encrypt_type_e;


/* Known string payload types */
typedef enum {
    SDP_PAYLOAD_T38,
    SDP_PAYLOAD_XTMR,
    SDP_PAYLOAD_T120,
    SDP_MAX_STRING_PAYLOAD_TYPES,
    SDP_PAYLOAD_UNSUPPORTED,
    SDP_PAYLOAD_INVALID
} sdp_payload_e;


/* Payload type indicator */
typedef enum {
    SDP_PAYLOAD_NUMERIC,
    SDP_PAYLOAD_ENUM
} sdp_payload_ind_e;


/* Image payload types */
typedef enum {
    SDP_PORT_NUM_ONLY,                  /* <port> or '$'                */
    SDP_PORT_NUM_COUNT,                 /* <port>/<number of ports>     */
    SDP_PORT_VPI_VCI,                   /* <vpi>/<vci>                  */
    SDP_PORT_VCCI,                      /* <vcci>                       */
    SDP_PORT_NUM_VPI_VCI,               /* <port>/<vpi>/<vci>           */
    SDP_PORT_VCCI_CID,                  /* <vcci>/<cid> or '$'/'$'      */
    SDP_PORT_NUM_VPI_VCI_CID,           /* <port>/<vpi>/<vci>/<cid>     */
    SDP_MAX_PORT_FORMAT_TYPES,
    SDP_PORT_FORMAT_INVALID
} sdp_port_format_e;


/* Fmtp attribute format Types */
typedef enum {
    SDP_FMTP_NTE,
    SDP_FMTP_CODEC_INFO,
    SDP_FMTP_MODE,
    SDP_FMTP_DATACHANNEL,
    SDP_FMTP_UNKNOWN_TYPE,
    SDP_FMTP_MAX_TYPE
} sdp_fmtp_format_type_e;


/* T.38 Rate Mgmt Types */
typedef enum {
    SDP_T38_LOCAL_TCF,
    SDP_T38_TRANSFERRED_TCF,
    SDP_T38_UNKNOWN_RATE,
    SDP_T38_MAX_RATES
} sdp_t38_ratemgmt_e;


/* T.38 udp EC Types */
typedef enum {
    SDP_T38_UDP_REDUNDANCY,
    SDP_T38_UDP_FEC,
    SDP_T38_UDPEC_UNKNOWN,
    SDP_T38_MAX_UDPEC
} sdp_t38_udpec_e;

/* Bitmaps for manipulating sdp_direction_e */
typedef enum {
    SDP_DIRECTION_FLAG_SEND=0x01,
    SDP_DIRECTION_FLAG_RECV=0x02
} sdp_direction_flag_e;

/* Media flow direction */
typedef enum {
    SDP_DIRECTION_INACTIVE = 0,
    SDP_DIRECTION_SENDONLY = SDP_DIRECTION_FLAG_SEND,
    SDP_DIRECTION_RECVONLY = SDP_DIRECTION_FLAG_RECV,
    SDP_DIRECTION_SENDRECV = SDP_DIRECTION_FLAG_SEND | SDP_DIRECTION_FLAG_RECV,
    SDP_MAX_QOS_DIRECTIONS
} sdp_direction_e;

#define SDP_DIRECTION_PRINT(arg) \
    (((sdp_direction_e)(arg)) == SDP_DIRECTION_INACTIVE ? "SDP_DIRECTION_INACTIVE " : \
     ((sdp_direction_e)(arg)) == SDP_DIRECTION_SENDONLY ? "SDP_DIRECTION_SENDONLY": \
     ((sdp_direction_e)(arg)) == SDP_DIRECTION_RECVONLY ? "SDP_DIRECTION_RECVONLY ": \
     ((sdp_direction_e)(arg)) == SDP_DIRECTION_SENDRECV ? " SDP_DIRECTION_SENDRECV": "SDP_MAX_QOS_DIRECTIONS")


/* QOS Strength tag */
typedef enum {
    SDP_QOS_STRENGTH_OPT,
    SDP_QOS_STRENGTH_MAND,
    SDP_QOS_STRENGTH_SUCC,
    SDP_QOS_STRENGTH_FAIL,
    SDP_QOS_STRENGTH_NONE,
    SDP_MAX_QOS_STRENGTH,
    SDP_QOS_STRENGTH_UNKNOWN
} sdp_qos_strength_e;


/* QOS direction */
typedef enum {
    SDP_QOS_DIR_SEND,
    SDP_QOS_DIR_RECV,
    SDP_QOS_DIR_SENDRECV,
    SDP_QOS_DIR_NONE,
    SDP_MAX_QOS_DIR,
    SDP_QOS_DIR_UNKNOWN
} sdp_qos_dir_e;

/* QoS Status types */
typedef enum {
    SDP_QOS_LOCAL,
    SDP_QOS_REMOTE,
    SDP_QOS_E2E,
    SDP_MAX_QOS_STATUS_TYPES,
    SDP_QOS_STATUS_TYPE_UNKNOWN
} sdp_qos_status_types_e;

/* QoS Status types */
typedef enum {
    SDP_CURR_QOS_TYPE,
    SDP_CURR_UNKNOWN_TYPE,
    SDP_MAX_CURR_TYPES
} sdp_curr_type_e;

/* QoS Status types */
typedef enum {
    SDP_DES_QOS_TYPE,
    SDP_DES_UNKNOWN_TYPE,
    SDP_MAX_DES_TYPES
} sdp_des_type_e;

/* QoS Status types */
typedef enum {
    SDP_CONF_QOS_TYPE,
    SDP_CONF_UNKNOWN_TYPE,
    SDP_MAX_CONF_TYPES
} sdp_conf_type_e;


/* Named event range result values. */
typedef enum {
    SDP_NO_MATCH,
    SDP_PARTIAL_MATCH,
    SDP_FULL_MATCH
} sdp_ne_res_e;

/* Fmtp attribute parameters for audio/video codec information */
typedef enum {

    /* mainly for audio codecs */
    SDP_ANNEX_A,     /* 0 */
    SDP_ANNEX_B,
    SDP_BITRATE,

    /* for video codecs */
    SDP_QCIF,
    SDP_CIF,
    SDP_MAXBR,
    SDP_SQCIF,
    SDP_CIF4,
    SDP_CIF16,
    SDP_CUSTOM,
    SDP_PAR,
    SDP_CPCF,
    SDP_BPP,
    SDP_HRD,
    SDP_PROFILE,
    SDP_LEVEL,
    SDP_INTERLACE,

    /* H.264 related */
    SDP_PROFILE_LEVEL_ID,     /* 17 */
    SDP_PARAMETER_SETS,
    SDP_PACKETIZATION_MODE,
    SDP_INTERLEAVING_DEPTH,
    SDP_DEINT_BUF_REQ,
    SDP_MAX_DON_DIFF,
    SDP_INIT_BUF_TIME,

    SDP_MAX_MBPS,
    SDP_MAX_FS,
    SDP_MAX_CPB,
    SDP_MAX_DPB,
    SDP_MAX_BR,
    SDP_REDUNDANT_PIC_CAP,
    SDP_DEINT_BUF_CAP,
    SDP_MAX_RCMD_NALU_SIZE,

    SDP_PARAMETER_ADD,

    /* Annexes - begin */
    /* Some require special handling as they don't have token=token format*/
    SDP_ANNEX_D,
    SDP_ANNEX_F,
    SDP_ANNEX_I,
    SDP_ANNEX_J,
    SDP_ANNEX_T,

    /* These annexes have token=token format */
    SDP_ANNEX_K,
    SDP_ANNEX_N,
    SDP_ANNEX_P,

    SDP_MODE,
    SDP_LEVEL_ASYMMETRY_ALLOWED,
    SDP_MAX_AVERAGE_BIT_RATE,
    SDP_USED_TX,
    SDP_STEREO,
    SDP_USE_IN_BAND_FEC,
    SDP_MAX_CODED_AUDIO_BW,
    SDP_CBR,
    SDP_STREAMS,
    SDP_PROTOCOL,
    SDP_MAX_FMTP_PARAM,
    SDP_FMTP_PARAM_UNKNOWN
} sdp_fmtp_codec_param_e;

/* Fmtp attribute parameters values for
   fmtp attribute parameters which convey codec
   information */

typedef enum {
    SDP_YES,
    SDP_NO,
    SDP_MAX_FMTP_PARAM_VAL,
    SDP_FMTP_PARAM_UNKNOWN_VAL
} sdp_fmtp_codec_param_val_e;

/* silenceSupp suppPref */
typedef enum {
    SDP_SILENCESUPP_PREF_STANDARD,
    SDP_SILENCESUPP_PREF_CUSTOM,
    SDP_SILENCESUPP_PREF_NULL, /* "-" */
    SDP_MAX_SILENCESUPP_PREF,
    SDP_SILENCESUPP_PREF_UNKNOWN
} sdp_silencesupp_pref_e;

/* silenceSupp sidUse */
typedef enum {
    SDP_SILENCESUPP_SIDUSE_NOSID,
    SDP_SILENCESUPP_SIDUSE_FIXED,
    SDP_SILENCESUPP_SIDUSE_SAMPLED,
    SDP_SILENCESUPP_SIDUSE_NULL, /* "-" */
    SDP_MAX_SILENCESUPP_SIDUSE,
    SDP_SILENCESUPP_SIDUSE_UNKNOWN
} sdp_silencesupp_siduse_e;

typedef enum {
    SDP_MEDIADIR_ROLE_PASSIVE,
    SDP_MEDIADIR_ROLE_ACTIVE,
    SDP_MEDIADIR_ROLE_BOTH,
    SDP_MEDIADIR_ROLE_REUSE,
    SDP_MEDIADIR_ROLE_UNKNOWN,
    SDP_MAX_MEDIADIR_ROLES,
    SDP_MEDIADIR_ROLE_UNSUPPORTED,
    SDP_MEDIADIR_ROLE_INVALID
} sdp_mediadir_role_e;

typedef enum {
    SDP_GROUP_ATTR_FID,
    SDP_GROUP_ATTR_LS,
    SDP_GROUP_ATTR_ANAT,
    SDP_MAX_GROUP_ATTR_VAL,
    SDP_GROUP_ATTR_UNSUPPORTED
} sdp_group_attr_e;

typedef enum {
    SDP_SRC_FILTER_INCL,
    SDP_SRC_FILTER_EXCL,
    SDP_MAX_FILTER_MODE,
    SDP_FILTER_MODE_NOT_PRESENT
} sdp_src_filter_mode_e;

typedef enum {
    SDP_RTCP_UNICAST_MODE_REFLECTION,
    SDP_RTCP_UNICAST_MODE_RSI,
    SDP_RTCP_MAX_UNICAST_MODE,
    SDP_RTCP_UNICAST_MODE_NOT_PRESENT
} sdp_rtcp_unicast_mode_e;

/* a=rtcp-fb enumerations */

typedef enum {
    SDP_RTCP_FB_ANY = -1,
    SDP_RTCP_FB_ACK = 0,
    SDP_RTCP_FB_CCM,
    SDP_RTCP_FB_NACK,
    SDP_RTCP_FB_TRR_INT,
    SDP_MAX_RTCP_FB,
    SDP_RTCP_FB_UNKNOWN
} sdp_rtcp_fb_type_e;

typedef enum {
    SDP_RTCP_FB_NACK_NOT_FOUND = -1,
    SDP_RTCP_FB_NACK_UNSPECIFIED = 0,
    SDP_RTCP_FB_NACK_SLI,
    SDP_RTCP_FB_NACK_PLI,
    SDP_RTCP_FB_NACK_RPSI,
    SDP_RTCP_FB_NACK_APP,
    SDP_RTCP_FB_NACK_RAI,
    SDP_RTCP_FB_NACK_TLLEI,
    SDP_RTCP_FB_NACK_PSLEI,
    SDP_RTCP_FB_NACK_ECN,
    SDP_MAX_RTCP_FB_NACK,
    SDP_RTCP_FB_NACK_UNKNOWN
} sdp_rtcp_fb_nack_type_e;

typedef enum {
    SDP_RTCP_FB_ACK_NOT_FOUND = -1,
    SDP_RTCP_FB_ACK_RPSI = 0,
    SDP_RTCP_FB_ACK_APP,
    SDP_MAX_RTCP_FB_ACK,
    SDP_RTCP_FB_ACK_UNKNOWN
} sdp_rtcp_fb_ack_type_e;

typedef enum {
    SDP_RTCP_FB_CCM_NOT_FOUND = -1,
    SDP_RTCP_FB_CCM_FIR = 0,
    SDP_RTCP_FB_CCM_TMMBR,
    SDP_RTCP_FB_CCM_TSTR,
    SDP_RTCP_FB_CCM_VBCM,
    SDP_MAX_RTCP_FB_CCM,
    SDP_RTCP_FB_CCM_UNKNOWN
} sdp_rtcp_fb_ccm_type_e;

/*
 * sdp_srtp_fec_order_t
 *  This type defines the order in which to perform FEC
 *  (Forward Error Correction) and SRTP Encryption/Authentication.
 */
typedef enum sdp_srtp_fec_order_t_ {
    SDP_SRTP_THEN_FEC, /* upon sending perform SRTP then FEC */
    SDP_FEC_THEN_SRTP, /* upon sending perform FEC then SRTP */
    SDP_SRTP_FEC_SPLIT /* upon sending perform SRTP Encryption,
			  * then FEC, the SRTP Authentication */
} sdp_srtp_fec_order_t;


/*
 *  sdp_srtp_crypto_suite_t
 *   Enumeration of the crypto suites supported for MGCP SRTP
 *   package.
 */
typedef enum sdp_srtp_crypto_suite_t_ {
    SDP_SRTP_UNKNOWN_CRYPTO_SUITE = 0,
    SDP_SRTP_AES_CM_128_HMAC_SHA1_32,
    SDP_SRTP_AES_CM_128_HMAC_SHA1_80,
    SDP_SRTP_F8_128_HMAC_SHA1_80,
    SDP_SRTP_MAX_NUM_CRYPTO_SUITES
} sdp_srtp_crypto_suite_t;

/*
 * SDP SRTP crypto suite definition parameters
 *
 * SDP_SRTP_<crypto_suite>_KEY_BYTES
 *  The size of a master key for <crypto_suite> in bytes.
 *
 * SDP_SRTP_<crypto_suite>_SALT_BYTES
 *  The size of a master salt for <crypto_suite> in bytes.
 */
#define SDP_SRTP_AES_CM_128_HMAC_SHA1_32_KEY_BYTES	16
#define SDP_SRTP_AES_CM_128_HMAC_SHA1_32_SALT_BYTES	14
#define SDP_SRTP_AES_CM_128_HMAC_SHA1_80_KEY_BYTES	16
#define SDP_SRTP_AES_CM_128_HMAC_SHA1_80_SALT_BYTES	14
#define SDP_SRTP_F8_128_HMAC_SHA1_80_KEY_BYTES		16
#define SDP_SRTP_F8_128_HMAC_SHA1_80_SALT_BYTES	14

/* SDP Defines */

#define SDP_MAX_STRING_LEN      256  /* Max len for SDP string       */
#define SDP_MAX_SHORT_STRING_LEN      12  /* Max len for a short SDP string  */
#define SDP_MAX_PAYLOAD_TYPES   23  /* Max payload types in m= line */
#define SDP_TOKEN_LEN           2   /* Len of <token>=              */
#define SDP_CURRENT_VERSION     0   /* Current default SDP version  */
#define SDP_MAX_PORT_PARAMS     4   /* Max m= port params - x/x/x/x */
#define SDP_MIN_DYNAMIC_PAYLOAD 96  /* Min dynamic payload */
#define SDP_MAX_DYNAMIC_PAYLOAD 127 /* Max dynamic payload */
#define SDP_MIN_CIF_VALUE 1  /* applies to all  QCIF,CIF,CIF4,CIF16,SQCIF */
#define SDP_MAX_CIF_VALUE 32 /* applies to all  QCIF,CIF,CIF4,CIF16,SQCIF */
#define SDP_MAX_SRC_ADDR_LIST  1 /* Max source addrs for which filter applies */


#define SDP_DEFAULT_PACKETIZATION_MODE_VALUE 0 /* max packetization mode for H.264 */
#define SDP_MAX_PACKETIZATION_MODE_VALUE 2 /* max packetization mode for H.264 */

#define SDP_MAX_LEVEL_ASYMMETRY_ALLOWED_VALUE 1 /* max level asymmetry allowed value for H.264 */
#define SDP_DEFAULT_LEVEL_ASYMMETRY_ALLOWED_VALUE 1 /* default level asymmetry allowed value for H.264 */
#define SDP_INVALID_LEVEL_ASYMMETRY_ALLOWED_VALUE 2 /* invalid value for level-asymmetry-allowed param for H.264 */


/* Max number of stream ids that can be grouped together */
#define SDP_MAX_GROUP_STREAM_ID 10


#define SDP_MAGIC_NUM           0xabcdabcd

#define SDP_UNSUPPORTED         "Unsupported"
#define SDP_MAX_LINE_LEN   256 /* Max len for SDP Line */

#define SDP_MAX_PROFILE_VALUE  10
#define SDP_MAX_LEVEL_VALUE    100
#define SDP_MIN_PROFILE_LEVEL_VALUE 0
#define SDP_MAX_TTL_VALUE  255
#define SDP_MIN_MCAST_ADDR_HI_BIT_VAL 224
#define SDP_MAX_MCAST_ADDR_HI_BIT_VAL 239

/* SDP Enum Types */

typedef enum {
    SDP_ERR_INVALID_CONF_PTR,
    SDP_ERR_INVALID_SDP_PTR,
    SDP_ERR_INTERNAL,
    SDP_MAX_ERR_TYPES
} sdp_errmsg_e;

/* SDP Structure Definitions */

/* String names of varios tokens */
typedef struct {
    char                     *name;
    u8                        strlen;
} sdp_namearray_t;

/* c= line info */
typedef struct {
    sdp_nettype_e             nettype;
    sdp_addrtype_e            addrtype;
    char                      conn_addr[SDP_MAX_STRING_LEN+1];
    tinybool                  is_multicast;
    u16                       ttl;
    u16                       num_of_addresses;
} sdp_conn_t;

/* t= line info */
typedef struct sdp_timespec {
    char                      start_time[SDP_MAX_STRING_LEN+1];
    char                      stop_time[SDP_MAX_STRING_LEN+1];
    struct sdp_timespec      *next_p;
} sdp_timespec_t;


/* k= line info */
typedef struct sdp_encryptspec {
    sdp_encrypt_type_e        encrypt_type;
    char              encrypt_key[SDP_MAX_STRING_LEN+1];
} sdp_encryptspec_t;


/* FMTP attribute deals with named events in the range of 0-255 as
 * defined in RFC 2833 */
#define SDP_MIN_NE_VALUE      0
#define SDP_MAX_NE_VALUES     256
#define SDP_NE_BITS_PER_WORD  ( sizeof(u32) * 8 )
#define SDP_NE_NUM_BMAP_WORDS ((SDP_MAX_NE_VALUES + SDP_NE_BITS_PER_WORD - 1)/SDP_NE_BITS_PER_WORD )
#define SDP_NE_BIT_0          ( 0x00000001 )
#define SDP_NE_ALL_BITS       ( 0xFFFFFFFF )

#define SDP_DEINT_BUF_REQ_FLAG   0x1
#define SDP_INIT_BUF_TIME_FLAG   0x2
#define SDP_MAX_RCMD_NALU_SIZE_FLAG   0x4
#define SDP_DEINT_BUF_CAP_FLAG   0x8

#define SDP_FMTP_UNUSED          0xFFFF

typedef struct sdp_fmtp {
    u16                       payload_num;
    u32                       maxval;  /* maxval optimizes bmap search */
    u32                       bmap[ SDP_NE_NUM_BMAP_WORDS ];
    sdp_fmtp_format_type_e    fmtp_format; /* Gives the format type
                                              for FMTP attribute*/
    tinybool                  annexb_required;
    tinybool                  annexa_required;

    tinybool                  annexa;
    tinybool                  annexb;
    u32                       bitrate;
    u32                       mode;

    /* some OPUS specific fmtp params */
    u32                       maxaveragebitrate;
    u16                       usedtx;
    u16                       stereo;
    u16                       useinbandfec;
    char                      maxcodedaudiobandwidth[SDP_MAX_STRING_LEN+1];
    u16                       cbr;

    /* some Data Channel specific fmtp params */
    u16                       streams;   /* Num streams per Data Channel */
    char                      protocol[SDP_MAX_STRING_LEN+1];

    /* BEGIN - All Video related FMTP parameters */
    u16                       qcif;
    u16                       cif;
    u16                       maxbr;
    u16                       sqcif;
    u16                       cif4;
    u16                       cif16;

    u16                       custom_x;
    u16                       custom_y;
    u16                       custom_mpi;
    /* CUSTOM=360,240,4 implies X-AXIS=360, Y-AXIS=240; MPI=4 */
    u16                       par_width;
    u16                       par_height;
    /* PAR=12:11 implies par_width=12, par_height=11 */

    /* CPCF should be a float. IOS does not support float and so it is u16 */
    /* For portable stack, CPCF should be defined as float and the parsing should
     * be modified accordingly */
    u16                       cpcf;
    u16                       bpp;
    u16                       hrd;

    int16                     profile;
    int16                     level;
    tinybool                  is_interlace;

    /* some more H.264 specific fmtp params */
    char              profile_level_id[SDP_MAX_STRING_LEN+1];
    char                      parameter_sets[SDP_MAX_STRING_LEN+1];
    u16                       packetization_mode;
    u16                       level_asymmetry_allowed;
    u16                       interleaving_depth;
    u32                       deint_buf_req;
    u32                       max_don_diff;
    u32                       init_buf_time;

    u32                       max_mbps;
    u32                       max_fs;
    u32                       max_cpb;
    u32                       max_dpb;
    u32                       max_br;
    tinybool                  redundant_pic_cap;
    u32                       deint_buf_cap;
    u32                       max_rcmd_nalu_size;
    tinybool                  parameter_add;

    tinybool                  annex_d;

    tinybool                  annex_f;
    tinybool                  annex_i;
    tinybool                  annex_j;
    tinybool                  annex_t;

    /* H.263 codec requires annex K,N and P to have values */
    u16                       annex_k_val;
    u16                       annex_n_val;

    /* Annex P can take one or more values in the range 1-4 . e.g P=1,3 */
    u16                       annex_p_val_picture_resize; /* 1 = four; 2 = sixteenth */
    u16                       annex_p_val_warp; /* 3 = half; 4=sixteenth */

    u8                        flag;

  /* END - All Video related FMTP parameters */

} sdp_fmtp_t;

/* a=qos|secure|X-pc-qos|X-qos info */
typedef struct sdp_qos {
    sdp_qos_strength_e        strength;
    sdp_qos_dir_e             direction;
    tinybool                  confirm;
    sdp_qos_status_types_e    status_type;
} sdp_qos_t;

/* a=curr:qos status_type direction */
typedef struct sdp_curr {
    sdp_curr_type_e           type;
    sdp_qos_status_types_e    status_type;
    sdp_qos_dir_e             direction;
} sdp_curr_t;

/* a=des:qos strength status_type direction */
typedef struct sdp_des {
    sdp_des_type_e            type;
    sdp_qos_strength_e        strength;
    sdp_qos_status_types_e    status_type;
    sdp_qos_dir_e             direction;
} sdp_des_t;

/* a=conf:qos status_type direction */
typedef struct sdp_conf {
    sdp_conf_type_e           type;
    sdp_qos_status_types_e    status_type;
    sdp_qos_dir_e             direction;
} sdp_conf_t;


/* a=rtpmap or a=sprtmap info */
typedef struct sdp_transport_map {
    u16                       payload_num;
    char                      encname[SDP_MAX_STRING_LEN+1];
    u32                       clockrate;
    u16                       num_chan;
} sdp_transport_map_t;


/* a=rtr info */
typedef struct sdp_rtr {
    tinybool                  confirm;
} sdp_rtr_t;

/* a=subnet info */
typedef struct sdp_subnet {
    sdp_nettype_e             nettype;
    sdp_addrtype_e            addrtype;
    char                      addr[SDP_MAX_STRING_LEN+1];
    int32                     prefix;
} sdp_subnet_t;


/* a=X-pc-codec info */
typedef struct sdp_pccodec {
    u16                       num_payloads;
    ushort                    payload_type[SDP_MAX_PAYLOAD_TYPES];
} sdp_pccodec_t;

/* a=direction info */
typedef struct sdp_comediadir {
    sdp_mediadir_role_e      role;
    tinybool                 conn_info_present;
    sdp_conn_t               conn_info;
    u32                      src_port;
} sdp_comediadir_t;



/* a=silenceSupp info */
typedef struct sdp_silencesupp {
    tinybool                  enabled;
    tinybool                  timer_null;
    u16                       timer;
    sdp_silencesupp_pref_e    pref;
    sdp_silencesupp_siduse_e  siduse;
    tinybool                  fxnslevel_null;
    u8                        fxnslevel;
} sdp_silencesupp_t;


/*
 * a=mptime info */
/* Note that an interval value of zero corresponds to
 * the "-" syntax on the a= line.
 */
typedef struct sdp_mptime {
    u16                       num_intervals;
    ushort                    intervals[SDP_MAX_PAYLOAD_TYPES];
} sdp_mptime_t;

/*
 * a=X-sidin:<val>, a=X-sidout:< val> and a=X-confid: <val>
 * Stream Id,ConfID related attributes to be used for audio/video conferencing
 *
*/

typedef struct sdp_stream_data {
    char                      x_sidin[SDP_MAX_STRING_LEN+1];
    char                      x_sidout[SDP_MAX_STRING_LEN+1];
    char                      x_confid[SDP_MAX_STRING_LEN+1];
    sdp_group_attr_e          group_attr; /* FID or LS */
    u16                       num_group_id;
    u16                       group_id_arr[SDP_MAX_GROUP_STREAM_ID];
} sdp_stream_data_t;

/*
 * a=source-filter:<filter-mode> <filter-spec>
 * <filter-spec> = <nettype> <addrtype> <dest-addr> <src_addr><src_addr>...
 * One or more source addresses to apply filter, for one or more connection
 * address in unicast/multicast environments
 */
typedef struct sdp_source_filter {
   sdp_src_filter_mode_e  mode;
   sdp_nettype_e     nettype;
   sdp_addrtype_e    addrtype;
   char              dest_addr[SDP_MAX_STRING_LEN+1];
   u16               num_src_addr;
   char              src_list[SDP_MAX_SRC_ADDR_LIST+1][SDP_MAX_STRING_LEN+1];
} sdp_source_filter_t;

/*
 * a=rtcp-fb:<payload-type> <feedback-type> [<feedback-parameters>]
 * Defines RTCP feedback parameters
 */
#define SDP_ALL_PAYLOADS         0xFFFF
typedef struct sdp_fmtp_fb {
    u16                          payload_num;    /* can be SDP_ALL_PAYLOADS */
    sdp_rtcp_fb_type_e           feedback_type;
    union {
        sdp_rtcp_fb_ack_type_e   ack;
        sdp_rtcp_fb_ccm_type_e   ccm;
        sdp_rtcp_fb_nack_type_e  nack;
        u32                      trr_int;
    } param;
    char extra[SDP_MAX_STRING_LEN + 1]; /* Holds any trailing information that
                                           cannot be represented by preceding
                                           fields. */
} sdp_fmtp_fb_t;

/*
 * b=<bw-modifier>:<val>
 *
*/
typedef struct sdp_bw_data {
    struct sdp_bw_data       *next_p;
    sdp_bw_modifier_e        bw_modifier;
    int                      bw_val;
} sdp_bw_data_t;

/*
 * This structure houses a linked list of sdp_bw_data_t instances. Each
 * sdp_bw_data_t instance represents one b= line.
 */
typedef struct sdp_bw {
    u16                      bw_data_count;
    sdp_bw_data_t            *bw_data_list;
} sdp_bw_t;

/* Media lines for AAL2 may have more than one transport type defined
 * each with its own payload type list.  These are referred to as
 * profile types instead of transport types.  This structure is used
 * to handle these multiple profile types. Note: One additional profile
 * field is needed because of the way parsing is done.  This is not an
 * error. */
typedef struct sdp_media_profiles {
    u16             num_profiles;
    sdp_transport_e profile[SDP_MAX_PROFILES+1];
    u16             num_payloads[SDP_MAX_PROFILES];
    sdp_payload_ind_e payload_indicator[SDP_MAX_PROFILES][SDP_MAX_PAYLOAD_TYPES];
    u16             payload_type[SDP_MAX_PROFILES][SDP_MAX_PAYLOAD_TYPES];
} sdp_media_profiles_t;


/*
 * sdp_srtp_crypto_context_t
 *  This type is used to hold cryptographic context information.
 *
 */

typedef struct sdp_srtp_crypto_context_t_ {
    int32                   tag;
    unsigned long           selection_flags;
    sdp_srtp_crypto_suite_t suite;
    unsigned char           master_key[SDP_SRTP_MAX_KEY_SIZE_BYTES];
    unsigned char           master_salt[SDP_SRTP_MAX_SALT_SIZE_BYTES];
    unsigned char           master_key_size_bytes;
    unsigned char           master_salt_size_bytes;
    unsigned long           ssrc; /* not used */
    unsigned long           roc;  /* not used */
    unsigned long           kdr;  /* not used */
    unsigned short          seq;  /* not used */
    sdp_srtp_fec_order_t    fec_order; /* not used */
    unsigned char           master_key_lifetime[SDP_SRTP_MAX_LIFETIME_BYTES];
    unsigned char           mki[SDP_SRTP_MAX_MKI_SIZE_BYTES];
    u16                     mki_size_bytes;
    char*                   session_parameters;
} sdp_srtp_crypto_context_t;


/* m= line info and associated attribute list */
/* Note: Most of the port parameter values are 16-bit values.  We set
 * the type to int32 so we can return either a 16-bit value or the
 * choose value. */
typedef struct sdp_mca {
    sdp_media_e               media;
    sdp_conn_t                conn;
    sdp_transport_e           transport;
    sdp_port_format_e         port_format;
    int32                     port;
    int32                     sctpport;
    int32                     num_ports;
    int32                     vpi;
    u32                       vci;  /* VCI needs to be 32-bit */
    int32                     vcci;
    int32                     cid;
    u16                       num_payloads;
    sdp_payload_ind_e         payload_indicator[SDP_MAX_PAYLOAD_TYPES];
    u16                       payload_type[SDP_MAX_PAYLOAD_TYPES];
    sdp_media_profiles_t     *media_profiles_p;
    tinybool                  sessinfo_found;
    sdp_encryptspec_t         encrypt;
    sdp_bw_t                  bw;
    sdp_attr_e                media_direction; /* Either INACTIVE, SENDONLY,
                                                  RECVONLY, or SENDRECV */
    u32                       mid;
    struct sdp_attr          *media_attrs_p;
    struct sdp_mca           *next_p;
} sdp_mca_t;


/* generic a= line info */
typedef struct sdp_attr {
    sdp_attr_e                type;
    union {
        tinybool              boolean_val;
        u32                   u32_val;
        char                  string_val[SDP_MAX_STRING_LEN+1];
        char                  ice_attr[SDP_MAX_STRING_LEN+1];
        sdp_fmtp_t            fmtp;
        sdp_qos_t             qos;
        sdp_curr_t            curr;
        sdp_des_t             des;
        sdp_conf_t            conf;
        sdp_transport_map_t   transport_map;    /* A rtpmap or sprtmap */
        sdp_subnet_t          subnet;
        sdp_t38_ratemgmt_e    t38ratemgmt;
        sdp_t38_udpec_e       t38udpec;
        sdp_pccodec_t         pccodec;
        sdp_silencesupp_t     silencesupp;
        sdp_mca_t            *cap_p;    /* A X-CAP or CDSC attribute */
        sdp_rtr_t             rtr;
        sdp_comediadir_t      comediadir;
        sdp_srtp_crypto_context_t srtp_context;
        sdp_mptime_t          mptime;
        sdp_stream_data_t     stream_data;
        char                  unknown[SDP_MAX_STRING_LEN+1];
        sdp_source_filter_t   source_filter;
        sdp_fmtp_fb_t         rtcp_fb;
    } attr;
    struct sdp_attr          *next_p;
} sdp_attr_t;

typedef struct sdp_srtp_crypto_suite_list_ {
    sdp_srtp_crypto_suite_t crypto_suite_val;
    char * crypto_suite_str;
    unsigned char key_size_bytes;
    unsigned char salt_size_bytes;
} sdp_srtp_crypto_suite_list;

/* Application configuration options */
typedef struct sdp_conf_options {
    u32                       magic_num;
    tinybool                  debug_flag[SDP_MAX_DEBUG_TYPES];
    tinybool                  version_reqd;
    tinybool                  owner_reqd;
    tinybool                  session_name_reqd;
    tinybool                  timespec_reqd;
    tinybool                  media_supported[SDP_MAX_MEDIA_TYPES];
    tinybool                  nettype_supported[SDP_MAX_NETWORK_TYPES];
    tinybool                  addrtype_supported[SDP_MAX_ADDR_TYPES];
    tinybool                  transport_supported[SDP_MAX_TRANSPORT_TYPES];
    tinybool                  allow_choose[SDP_MAX_CHOOSE_PARAMS];
    /* Statistics counts */
    u32                       num_builds;
    u32                       num_parses;
    u32                       num_not_sdp_desc;
    u32                       num_invalid_token_order;
    u32                       num_invalid_param;
    u32                       num_no_resource;
    struct sdp_conf_options  *next_p;
} sdp_conf_options_t;


/* Session level SDP info with pointers to media line info. */
/* Elements here that can only be one of are included directly. Elements */
/* that can be more than one are pointers.                               */
typedef struct {
    char                      peerconnection[PC_HANDLE_SIZE];
    u32                       magic_num;
    sdp_conf_options_t       *conf_p;
    tinybool                  debug_flag[SDP_MAX_DEBUG_TYPES];
    char                      debug_str[SDP_MAX_STRING_LEN+1];
    u32                       debug_id;
    int32                     version; /* version is really a u16 */
    char                      owner_name[SDP_MAX_STRING_LEN+1];
    char                      owner_sessid[SDP_MAX_STRING_LEN+1];
    char                      owner_version[SDP_MAX_STRING_LEN+1];
    sdp_nettype_e             owner_network_type;
    sdp_addrtype_e            owner_addr_type;
    char                      owner_addr[SDP_MAX_STRING_LEN+1];
    char                      sessname[SDP_MAX_STRING_LEN+1];
    tinybool                  sessinfo_found;
    tinybool                  uri_found;
    sdp_conn_t                default_conn;
    sdp_timespec_t           *timespec_p;
    sdp_encryptspec_t         encrypt;
    sdp_bw_t                  bw;
    sdp_attr_t               *sess_attrs_p;

    /* Info to help with building capability attributes. */
    u16                       cur_cap_num;
    sdp_mca_t                *cur_cap_p;
    /* Info to help parsing X-cpar attrs. */
    u16                       cap_valid;
    u16                       last_cap_inst;
    /* Info to help building X-cpar/cpar attrs. */
    sdp_attr_e            last_cap_type;

    /* MCA - Media, connection, and attributes */
    sdp_mca_t                *mca_p;
    ushort                    mca_count;
} sdp_t;


/* Token processing table. */
typedef struct {
    char *name;
    sdp_result_e (*parse_func)(sdp_t *sdp_p, u16 level, const char *ptr);
    sdp_result_e (*build_func)(sdp_t *sdp_p, u16 level, flex_string *fs);
} sdp_tokenarray_t;

/* Attribute processing table. */
typedef struct {
    char *name;
    u16 strlen;
    sdp_result_e (*parse_func)(sdp_t *sdp_p, sdp_attr_t *attr_p,
                               const char *ptr);
    sdp_result_e (*build_func)(sdp_t *sdp_p, sdp_attr_t *attr_p,
                               flex_string *fs);
} sdp_attrarray_t;


/* Prototypes */

/* sdp_config.c */
extern void *sdp_init_config(void);
extern void sdp_appl_debug(void *config_p, sdp_debug_e debug_type,
                           tinybool debug_flag);
extern void sdp_require_version(void *config_p, tinybool version_required);
extern void sdp_require_owner(void *config_p, tinybool owner_required);
extern void sdp_require_session_name(void *config_p,
                                     tinybool sess_name_required);
extern void sdp_require_timespec(void *config_p, tinybool timespec_required);
extern void sdp_media_supported(void *config_p, sdp_media_e media_type,
			 tinybool media_supported);
extern void sdp_nettype_supported(void *config_p, sdp_nettype_e nettype,
			   tinybool nettype_supported);
extern void sdp_addrtype_supported(void *config_p, sdp_addrtype_e addrtype,
			    tinybool addrtype_supported);
extern void sdp_transport_supported(void *config_p, sdp_transport_e transport,
			     tinybool transport_supported);
extern void sdp_allow_choose(void *config_p, sdp_choose_param_e param,
                             tinybool choose_allowed);

/* sdp_main.c */
extern sdp_t *sdp_init_description(const char *peerconnection, void *config_p);
extern void sdp_debug(sdp_t *sdp_ptr, sdp_debug_e debug_type, tinybool debug_flag);
extern void sdp_set_string_debug(sdp_t *sdp_ptr, const char *debug_str);
extern sdp_result_e sdp_parse(sdp_t *sdp_ptr, char **bufp, u16 len);
extern sdp_result_e sdp_build(sdp_t *sdp_ptr, flex_string *fs);
extern sdp_t *sdp_copy(sdp_t *sdp_ptr);
extern sdp_result_e sdp_free_description(sdp_t *sdp_ptr);
extern void sdp_parse_error(const char *peerconnection, const char *format, ...);

extern const char *sdp_get_result_name(sdp_result_e rc);


/* sdp_access.c */
extern tinybool sdp_version_valid(void *sdp_p);
extern int32 sdp_get_version(void *sdp_p);
extern sdp_result_e sdp_set_version(void *sdp_p, int32 version);

extern tinybool sdp_owner_valid(void *sdp_p);
extern const char *sdp_get_owner_username(void *sdp_p);
extern const char *sdp_get_owner_sessionid(void *sdp_p);
extern const char *sdp_get_owner_version(void *sdp_p);
extern sdp_nettype_e sdp_get_owner_network_type(void *sdp_p);
extern sdp_addrtype_e sdp_get_owner_address_type(void *sdp_p);
extern const char *sdp_get_owner_address(void *sdp_p);
extern sdp_result_e sdp_set_owner_username(void *sdp_p, const char *username);
extern sdp_result_e sdp_set_owner_sessionid(void *sdp_p, const char *sessid);
extern sdp_result_e sdp_set_owner_version(void *sdp_p, const char *version);
extern sdp_result_e sdp_set_owner_network_type(void *sdp_p,
                                               sdp_nettype_e network_type);
extern sdp_result_e sdp_set_owner_address_type(void *sdp_p,
                                               sdp_addrtype_e address_type);
extern sdp_result_e sdp_set_owner_address(void *sdp_p, const char *address);

extern tinybool sdp_session_name_valid(void *sdp_p);
extern const char *sdp_get_session_name(void *sdp_p);
extern sdp_result_e sdp_set_session_name(void *sdp_p, const char *sessname);

extern tinybool sdp_timespec_valid(void *sdp_ptr);
extern const char *sdp_get_time_start(void *sdp_ptr);
extern const char *sdp_get_time_stop(void *sdp_ptr);
sdp_result_e sdp_set_time_start(void *sdp_ptr, const char *start_time);
sdp_result_e sdp_set_time_stop(void *sdp_ptr, const char *stop_time);

extern tinybool sdp_encryption_valid(void *sdp_ptr, u16 level);
extern sdp_encrypt_type_e sdp_get_encryption_method(void *sdp_ptr, u16 level);
extern const char *sdp_get_encryption_key(void *sdp_ptr, u16 level);
extern sdp_result_e sdp_set_encryption_method(void *sdp_ptr, u16 level,
                                              sdp_encrypt_type_e method);
extern sdp_result_e sdp_set_encryption_key(void *sdp_ptr, u16 level,
                                           const char *key);

extern tinybool sdp_connection_valid(void *sdp_p, u16 level);
extern tinybool sdp_bw_line_exists(void *sdp_ptr, u16 level, u16 inst_num);
extern tinybool sdp_bandwidth_valid(void *sdp_p, u16 level, u16 inst_num);
extern sdp_nettype_e sdp_get_conn_nettype(void *sdp_p, u16 level);
extern sdp_addrtype_e sdp_get_conn_addrtype(void *sdp_p, u16 level);
extern const char *sdp_get_conn_address(void *sdp_p, u16 level);

extern tinybool sdp_is_mcast_addr (void *sdp_ptr, u16 level);
extern int32 sdp_get_mcast_ttl(void *sdp_ptr, u16 level);
extern int32 sdp_get_mcast_num_of_addresses(void *sdp_ptr, u16 level);

extern sdp_result_e sdp_set_conn_nettype(void *sdp_p, u16 level,
                                  sdp_nettype_e nettype);
extern sdp_result_e sdp_set_conn_addrtype(void *sdp_p, u16 level,
                                   sdp_addrtype_e addrtype);
extern sdp_result_e sdp_set_conn_address(void *sdp_p, u16 level,
                                         const char *address);
extern sdp_result_e sdp_set_mcast_addr_fields(void *sdp_ptr, u16 level,
					     u16 ttl, u16 num_addr);

extern tinybool sdp_media_line_valid(void *sdp_ptr, u16 level);
extern u16 sdp_get_num_media_lines(void *sdp_ptr);
extern sdp_media_e sdp_get_media_type(void *sdp_ptr, u16 level);
extern sdp_port_format_e sdp_get_media_port_format(void *sdp_ptr, u16 level);
extern int32 sdp_get_media_portnum(void *sdp_ptr, u16 level);
extern int32 sdp_get_media_portcount(void *sdp_ptr, u16 level);
extern int32 sdp_get_media_vpi(void *sdp_ptr, u16 level);
extern u32 sdp_get_media_vci(void *sdp_ptr, u16 level);
extern int32 sdp_get_media_vcci(void *sdp_ptr, u16 level);
extern int32 sdp_get_media_cid(void *sdp_ptr, u16 level);
extern sdp_transport_e sdp_get_media_transport(void *sdp_ptr, u16 level);
extern u16 sdp_get_media_num_profiles(void *sdp_ptr, u16 level);
extern sdp_transport_e sdp_get_media_profile(void *sdp_ptr, u16 level,
                                              u16 profile_num);
extern u16 sdp_get_media_num_payload_types(void *sdp_ptr, u16 level);
extern u16 sdp_get_media_profile_num_payload_types(void *sdp_ptr, u16 level,
                                                    u16 profile_num);
extern u32 sdp_get_media_payload_type(void *sdp_ptr, u16 level,
                                u16 payload_num, sdp_payload_ind_e *indicator);
extern u32 sdp_get_media_profile_payload_type(void *sdp_ptr, u16 level,
                  u16 prof_num, u16 payload_num, sdp_payload_ind_e *indicator);
extern sdp_result_e sdp_insert_media_line(void *sdp_ptr, u16 level);
extern void sdp_delete_media_line(void *sdp_ptr, u16 level);
extern sdp_result_e sdp_set_media_type(void *sdp_ptr, u16 level,
                                       sdp_media_e media);
extern sdp_result_e sdp_set_media_port_format(void *sdp_ptr, u16 level,
                                       sdp_port_format_e port_format);
extern sdp_result_e sdp_set_media_portnum(void *sdp_ptr, u16 level,
                                          int32 portnum, int32 sctpport);
extern int32 sdp_get_media_sctp_port(void *sdp_ptr, u16 level);
extern sdp_result_e sdp_set_media_portcount(void *sdp_ptr, u16 level,
                                            int32 num_ports);
extern sdp_result_e sdp_set_media_vpi(void *sdp_ptr, u16 level, int32 vpi);
extern sdp_result_e sdp_set_media_vci(void *sdp_ptr, u16 level, u32 vci);
extern sdp_result_e sdp_set_media_vcci(void *sdp_ptr, u16 level, int32 vcci);
extern sdp_result_e sdp_set_media_cid(void *sdp_ptr, u16 level, int32 cid);
extern sdp_result_e sdp_set_media_transport(void *sdp_ptr, u16 level,
                                            sdp_transport_e transport);
extern sdp_result_e sdp_add_media_profile(void *sdp_ptr, u16 level,
                                           sdp_transport_e profile);
extern sdp_result_e sdp_add_media_payload_type(void *sdp_ptr, u16 level,
                               u16 payload_type, sdp_payload_ind_e indicator);
extern sdp_result_e sdp_add_media_profile_payload_type(void *sdp_ptr,
                               u16 level, u16 prof_num, u16 payload_type,
                               sdp_payload_ind_e indicator);

/* sdp_attr_access.c */
extern sdp_result_e sdp_add_new_attr(void *sdp_ptr, u16 level, u8 cap_num,
                                     sdp_attr_e attr_type, u16 *inst_num);
extern sdp_result_e sdp_copy_attr (void *src_sdp_ptr, void *dst_sdp_ptr,
                                   u16 src_level, u16 dst_level,
                                   u8 src_cap_num, u8 dst_cap_num,
                                   sdp_attr_e src_attr_type, u16 src_inst_num);
extern sdp_result_e sdp_copy_all_attrs(void *src_sdp_ptr, void *dst_sdp_ptr,
                                       u16 src_level, u16 dst_level);
extern sdp_result_e sdp_attr_num_instances(void *sdp_ptr, u16 level,
                         u8 cap_num, sdp_attr_e attr_type, u16 *num_attr_inst);
extern sdp_result_e sdp_get_total_attrs(void *sdp_ptr, u16 level, u8 cap_num,
                                        u16 *num_attrs);
extern sdp_result_e sdp_get_attr_type(void *sdp_ptr, u16 level, u8 cap_num,
                          u16 attr_num, sdp_attr_e *attr_type, u16 *inst_num);
extern sdp_result_e sdp_delete_attr(void *sdp_ptr, u16 level, u8 cap_num,
                             sdp_attr_e attr_type, u16 inst_num);
extern sdp_result_e sdp_delete_all_attrs(void *sdp_ptr, u16 level, u8 cap_num);
extern tinybool sdp_attr_valid(void *sdp_ptr, sdp_attr_e attr_type,
                                u16 level, u8 cap_num, u16 inst_num);
extern const char *sdp_attr_get_simple_string(void *sdp_ptr,
                   sdp_attr_e attr_type, u16 level, u8 cap_num, u16 inst_num);
extern sdp_result_e sdp_attr_set_simple_string(void *sdp_ptr,
                   sdp_attr_e attr_type, u16 level,
                   u8 cap_num, u16 inst_num, const char *string_parm);
extern u32 sdp_attr_get_simple_u32(void *sdp_ptr, sdp_attr_e attr_type,
                                    u16 level, u8 cap_num, u16 inst_num);
extern sdp_result_e sdp_attr_set_simple_u32(void *sdp_ptr,
                                      sdp_attr_e attr_type, u16 level,
                                      u8 cap_num, u16 inst_num, u32 num_parm);
extern tinybool sdp_attr_get_simple_boolean(void *sdp_ptr,
                   sdp_attr_e attr_type, u16 level, u8 cap_num, u16 inst_num);
extern sdp_result_e sdp_attr_set_simple_boolean(void *sdp_ptr,
                   sdp_attr_e attr_type, u16 level, u8 cap_num,
                   u16 inst_num, u32 bool_parm);
extern const char* sdp_attr_get_maxprate(void *sdp_ptr, u16 level,
                                         u16 inst_num);
extern sdp_result_e sdp_attr_set_maxprate(void *sdp_ptr, u16 level,
                                       u16 inst_num, const char *string_parm);
extern sdp_t38_ratemgmt_e sdp_attr_get_t38ratemgmt(void *sdp_ptr, u16 level,
                                                   u8 cap_num, u16 inst_num);
extern sdp_result_e sdp_attr_set_t38ratemgmt(void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num,
                                             sdp_t38_ratemgmt_e t38ratemgmt);
extern sdp_t38_udpec_e sdp_attr_get_t38udpec(void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num);
extern sdp_result_e sdp_attr_set_t38udpec(void *sdp_ptr, u16 level,
                                          u8 cap_num, u16 inst_num,
                                          sdp_t38_udpec_e t38udpec);
extern sdp_direction_e sdp_get_media_direction(void *sdp_ptr, u16 level,
                                               u8 cap_num);
extern sdp_qos_strength_e sdp_attr_get_qos_strength(void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num);
extern sdp_qos_status_types_e sdp_attr_get_qos_status_type (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num);
extern sdp_qos_dir_e sdp_attr_get_qos_direction(void *sdp_ptr, u16 level,
                               u8 cap_num, sdp_attr_e qos_attr, u16 inst_num);
extern tinybool sdp_attr_get_qos_confirm(void *sdp_ptr, u16 level,
                               u8 cap_num, sdp_attr_e qos_attr, u16 inst_num);
extern sdp_curr_type_e sdp_attr_get_curr_type (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num);
extern sdp_des_type_e sdp_attr_get_des_type (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num);
extern sdp_conf_type_e sdp_attr_get_conf_type (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num);
extern sdp_result_e sdp_attr_set_conf_type (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num,
                                sdp_conf_type_e conf_type);
extern sdp_result_e sdp_attr_set_des_type (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num,
                                sdp_des_type_e des_type);
extern sdp_result_e sdp_attr_set_curr_type (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num,
                                sdp_curr_type_e curr_type);
extern sdp_result_e sdp_attr_set_qos_strength(void *sdp_ptr, u16 level,
                                 u8 cap_num, sdp_attr_e qos_attr, u16 inst_num,
                                 sdp_qos_strength_e strength);
extern sdp_result_e sdp_attr_set_qos_direction(void *sdp_ptr, u16 level,
                                 u8 cap_num, sdp_attr_e qos_attr, u16 inst_num,
                                 sdp_qos_dir_e direction);
extern sdp_result_e sdp_attr_set_qos_status_type(void *sdp_ptr, u16 level,
                                 u8 cap_num, sdp_attr_e qos_attr, u16 inst_num,
                                 sdp_qos_status_types_e status_type);
extern sdp_result_e sdp_attr_set_qos_confirm(void *sdp_ptr, u16 level,
                                 u8 cap_num, sdp_attr_e qos_attr, u16 inst_num,
                                 tinybool confirm);
extern sdp_nettype_e sdp_attr_get_subnet_nettype(void *sdp_ptr, u16 level,
                                                 u8 cap_num, u16 inst_num);
extern sdp_addrtype_e sdp_attr_get_subnet_addrtype(void *sdp_ptr, u16 level,
                                                   u8 cap_num, u16 inst_num);
extern const char *sdp_attr_get_subnet_addr(void *sdp_ptr, u16 level,
                                            u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_subnet_prefix(void *sdp_ptr, u16 level,
                                        u8 cap_num, u16 inst_num);
extern sdp_result_e sdp_attr_set_subnet_nettype(void *sdp_ptr, u16 level,
                                                u8 cap_num, u16 inst_num,
                                                sdp_nettype_e nettype);
extern sdp_result_e sdp_attr_set_subnet_addrtype(void *sdp_ptr, u16 level,
                                                 u8 cap_num, u16 inst_num,
                                                 sdp_addrtype_e addrtype);
extern sdp_result_e sdp_attr_set_subnet_addr(void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num,
                                             const char *addr);
extern sdp_result_e sdp_attr_set_subnet_prefix(void *sdp_ptr, u16 level,
                                               u8 cap_num, u16 inst_num,
                                               int32 prefix);
extern tinybool sdp_attr_rtpmap_payload_valid(void *sdp_ptr, u16 level,
                                  u8 cap_num, u16 *inst_num, u16 payload_type);
extern u16 sdp_attr_get_rtpmap_payload_type(void *sdp_ptr, u16 level,
                                            u8 cap_num, u16 inst_num);
extern const char *sdp_attr_get_rtpmap_encname(void *sdp_ptr, u16 level,
                                               u8 cap_num, u16 inst_num);
extern u32 sdp_attr_get_rtpmap_clockrate(void *sdp_ptr, u16 level,
                                         u8 cap_num, u16 inst_num);
extern u16 sdp_attr_get_rtpmap_num_chan(void *sdp_ptr, u16 level,
                                        u8 cap_num, u16 inst_num);
extern sdp_result_e sdp_attr_set_rtpmap_payload_type(void *sdp_ptr, u16 level,
                                                     u8 cap_num, u16 inst_num,
                                                     u16 payload_num);
extern sdp_result_e sdp_attr_set_rtpmap_encname(void *sdp_ptr, u16 level,
                              u8 cap_num, u16 inst_num, const char *encname);
extern sdp_result_e sdp_attr_set_rtpmap_clockrate(void *sdp_ptr, u16 level,
                                                  u8 cap_num, u16 inst_num,
                                                  u32 clockrate);
extern sdp_result_e sdp_attr_set_rtpmap_num_chan(void *sdp_ptr, u16 level,
                                      u8 cap_num, u16 inst_num, u16 num_chan);
extern tinybool sdp_attr_sprtmap_payload_valid(void *sdp_ptr, u16 level,
                                  u8 cap_num, u16 *inst_num, u16 payload_type);
extern u16 sdp_attr_get_sprtmap_payload_type(void *sdp_ptr, u16 level,
                                            u8 cap_num, u16 inst_num);
extern const char *sdp_attr_get_sprtmap_encname(void *sdp_ptr, u16 level,
                                               u8 cap_num, u16 inst_num);
extern u32 sdp_attr_get_sprtmap_clockrate(void *sdp_ptr, u16 level,
                                         u8 cap_num, u16 inst_num);
extern u16 sdp_attr_get_sprtmap_num_chan(void *sdp_ptr, u16 level,
                                        u8 cap_num, u16 inst_num);
extern sdp_result_e sdp_attr_set_sprtmap_payload_type(void *sdp_ptr, u16 level,
                                                     u8 cap_num, u16 inst_num,
                                                     u16 payload_num);
extern sdp_result_e sdp_attr_set_sprtmap_encname(void *sdp_ptr, u16 level,
                              u8 cap_num, u16 inst_num, const char *encname);
extern sdp_result_e sdp_attr_set_sprtmap_clockrate(void *sdp_ptr, u16 level,
                                                  u8 cap_num, u16 inst_num,
                                                  u16 clockrate);
extern sdp_result_e sdp_attr_set_sprtmap_num_chan(void *sdp_ptr, u16 level,
                                      u8 cap_num, u16 inst_num, u16 num_chan);
extern tinybool sdp_attr_fmtp_payload_valid(void *sdp_ptr, u16 level,
                                  u8 cap_num, u16 *inst_num, u16 payload_type);
extern u16 sdp_attr_get_fmtp_payload_type(void *sdp_ptr, u16 level,
                                          u8 cap_num, u16 inst_num);
extern sdp_ne_res_e sdp_attr_fmtp_is_range_set(void *sdp_ptr, u16 level,
                           u8 cap_num, u16 inst_num, u8 low_val, u8 high_val);
extern tinybool sdp_attr_fmtp_valid(void *sdp_ptr, u16 level, u8 cap_num,
				    u16 inst_num, u16 appl_maxval, u32* evt_array);
extern sdp_result_e sdp_attr_set_fmtp_payload_type(void *sdp_ptr, u16 level,
                                                   u8 cap_num, u16 inst_num,
                                                   u16 payload_num);
extern sdp_result_e sdp_attr_get_fmtp_range(void *sdp_ptr, u16 level,
                           u8 cap_num, u16 inst_num, u32 *bmap);
extern sdp_result_e sdp_attr_set_fmtp_range(void *sdp_ptr, u16 level,
                           u8 cap_num, u16 inst_num, u8 low_val, u8 high_val);
extern sdp_result_e sdp_attr_set_fmtp_bitmap(void *sdp_ptr, u16 level,
                           u8 cap_num, u16 inst_num, u32 *bmap, u32 maxval);
extern sdp_result_e sdp_attr_clear_fmtp_range(void *sdp_ptr, u16 level,
                           u8 cap_num, u16 inst_num, u8 low_val, u8 high_val);
extern sdp_ne_res_e sdp_attr_compare_fmtp_ranges(void *src_sdp_ptr,
                           void *dst_sdp_ptr, u16 src_level, u16 dst_level,
                           u8 src_cap_num, u8 dst_cap_num, u16 src_inst_num,
                           u16 dst_inst_num);
extern sdp_result_e sdp_attr_copy_fmtp_ranges(void *src_sdp_ptr,
                           void *dst_sdp_ptr, u16 src_level, u16 dst_level,
                           u8 src_cap_num, u8 dst_cap_num, u16 src_inst_num,
                           u16 dst_inst_num);
extern sdp_result_e sdp_attr_set_fmtp_annexa (void *sdp_ptr, u16 level,
                                              u8 cap_num, u16 inst_num,
                                              tinybool annexa);
extern sdp_result_e sdp_attr_set_fmtp_mode(void *sdp_ptr, u16 level,
                                           u8 cap_num, u16 inst_num,
                                           u32 mode);
extern u32 sdp_attr_get_fmtp_mode_for_payload_type (void *sdp_ptr, u16 level,
                                             u8 cap_num, u32 payload_type);

extern sdp_result_e sdp_attr_set_fmtp_annexb (void *sdp_ptr, u16 level,
                                              u8 cap_num, u16 inst_num,
                                              tinybool annexb);

extern sdp_result_e sdp_attr_set_fmtp_bitrate_type  (void *sdp_ptr, u16 level,
                                                     u8 cap_num, u16 inst_num,
                                                     u32 bitrate);
extern sdp_result_e sdp_attr_set_fmtp_cif  (void *sdp_ptr, u16 level,
                                            u8 cap_num, u16 inst_num,
                                            u16 cif);
extern sdp_result_e sdp_attr_set_fmtp_qcif  (void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num,
                                             u16 qcif);
extern sdp_result_e sdp_attr_set_fmtp_sqcif  (void *sdp_ptr, u16 level,
                                              u8 cap_num, u16 inst_num,
                                              u16 sqcif);
extern sdp_result_e sdp_attr_set_fmtp_cif4  (void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num,
                                             u16 cif4);
extern sdp_result_e sdp_attr_set_fmtp_cif16  (void *sdp_ptr, u16 level,
                                              u8 cap_num, u16 inst_num,
                                              u16 cif16);
extern sdp_result_e sdp_attr_set_fmtp_maxbr  (void *sdp_ptr, u16 level,
                                              u8 cap_num, u16 inst_num,
                                              u16 maxbr);
extern sdp_result_e sdp_attr_set_fmtp_max_average_bitrate (void *sdp_ptr, u16 level,
                                                           u8 cap_num, u16 inst_num,
                                                           u32 maxaveragebitrate);
extern sdp_result_e sdp_attr_set_fmtp_usedtx (void *sdp_ptr, u16 level,
                                              u8 cap_num, u16 inst_num,
                                              tinybool usedtx);
extern sdp_result_e sdp_attr_set_fmtp_stereo (void *sdp_ptr, u16 level,
                                              u8 cap_num, u16 inst_num,
                                              tinybool stereo);
extern sdp_result_e sdp_attr_set_fmtp_useinbandfec (void *sdp_ptr, u16 level,
                                                    u8 cap_num, u16 inst_num,
                                                    tinybool useinbandfec);
extern sdp_result_e sdp_attr_set_fmtp_maxcodedaudiobandwidth (void *sdp_ptr, u16 level,
                                                              u8 cap_num, u16 inst_num,
                                                              const char *maxcodedaudiobandwidth);
extern sdp_result_e sdp_attr_set_fmtp_cbr (void *sdp_ptr, u16 level,
                                           u8 cap_num, u16 inst_num,
                                           tinybool cbr);
extern sdp_result_e sdp_attr_set_fmtp_custom (void *sdp_ptr, u16 level,
                                              u8 cap_num, u16 inst_num,
                                              u16 custom_x, u16 custom_y,
                                              u16 custom_mpi);
extern sdp_result_e sdp_attr_set_fmtp_par (void *sdp_ptr, u16 level,
                                           u8 cap_num, u16 inst_num,
                                           u16 par_width, u16 par_height);
extern sdp_result_e sdp_attr_set_fmtp_bpp  (void *sdp_ptr, u16 level,
                                            u8 cap_num, u16 inst_num,
                                            u16 bpp);
extern sdp_result_e sdp_attr_set_fmtp_hrd  (void *sdp_ptr, u16 level,
                                            u8 cap_num, u16 inst_num,
                                            u16 hrd);

extern sdp_result_e sdp_attr_set_fmtp_h263_num_params (void *sdp_ptr,
                                                       int16 level,
                                                       u8 cap_num,
                                                       u16 inst_num,
                                                       int16 profile,
                                                       u16 h263_level,
                                                       tinybool interlace);

extern sdp_result_e sdp_attr_set_fmtp_profile_level_id (void *sdp_ptr,
                                                       u16 level,
                                                       u8 cap_num,
                                                       u16 inst_num,
                                                       const char *prof_id);

extern sdp_result_e sdp_attr_set_fmtp_parameter_sets (void *sdp_ptr,
                                                      u16 level,
                                                      u8 cap_num,
                                                      u16 inst_num,
                                                      const char *parameter_sets);

extern sdp_result_e sdp_attr_set_fmtp_deint_buf_req (void *sdp_ptr, u16 level,
                                                u8 cap_num, u16 inst_num,
                                                u32 deint_buf_req);

extern sdp_result_e sdp_attr_set_fmtp_init_buf_time (void *sdp_ptr, u16 level,
                                                u8 cap_num, u16 inst_num,
                                                u32 init_buf_time);

extern sdp_result_e sdp_attr_set_fmtp_max_don_diff (void *sdp_ptr, u16 level,
                                                u8 cap_num, u16 inst_num,
                                                u32 max_don_diff);

extern sdp_result_e sdp_attr_set_fmtp_interleaving_depth (void *sdp_ptr, u16 level,
                                                u8 cap_num, u16 inst_num,
                                                u16 interleaving_depth);

extern sdp_result_e sdp_attr_set_fmtp_pack_mode (void *sdp_ptr,
                                                u16 level,
                                                u8 cap_num,
                                                u16 inst_num,
                                                u16 pack_mode);

extern sdp_result_e sdp_attr_set_fmtp_level_asymmetry_allowed (void *sdp_ptr,
                                                u16 level,
                                                u8 cap_num,
                                                u16 inst_num,
                                                u16 level_asymmetry_allowed);

extern sdp_result_e sdp_attr_set_fmtp_redundant_pic_cap (void *sdp_ptr,
						   u16 level,
						   u8 cap_num,
						   u16 inst_num,
						   tinybool redundant_pic_cap);

extern sdp_result_e sdp_attr_set_fmtp_max_mbps (void *sdp_ptr,
						   u16 level,
						   u8 cap_num,
						   u16 inst_num,
						   u32 max_mbps);

extern sdp_result_e sdp_attr_set_fmtp_max_fs (void *sdp_ptr,
					      u16 level,
				              u8 cap_num,
				              u16 inst_num,
					      u32 max_fs);

extern sdp_result_e sdp_attr_set_fmtp_max_cpb (void *sdp_ptr,
					      u16 level,
				              u8 cap_num,
				              u16 inst_num,
					      u32 max_cpb);

extern sdp_result_e sdp_attr_set_fmtp_max_dpb (void *sdp_ptr,
					      u16 level,
				              u8 cap_num,
				              u16 inst_num,
					      u32 max_dpb);

extern sdp_result_e sdp_attr_set_fmtp_max_br (void *sdp_ptr,
					      u16 level,
				              u8 cap_num,
				              u16 inst_num,
					      u32 max_br);

extern sdp_result_e sdp_attr_set_fmtp_max_rcmd_nalu_size (void *sdp_ptr, u16 level,
                                               u8 cap_num, u16 inst_num,
					       u32 max_rcmd_nalu_size);

extern sdp_result_e sdp_attr_set_fmtp_deint_buf_cap (void *sdp_ptr, u16 level,
                                               u8 cap_num, u16 inst_num,
					       u32 deint_buf_cap);

extern sdp_result_e sdp_attr_set_fmtp_h264_parameter_add (void *sdp_ptr,
                                                          u16 level,
                                                          u8 cap_num,
                                                          u16 inst_num,
                                                          tinybool parameter_add);

extern sdp_result_e sdp_attr_set_fmtp_h261_annex_params (void *sdp_ptr,
                                                         u16 level,
                                                         u8 cap_num,
                                                         u16 inst_num,
                                                         tinybool annex_d);

extern sdp_result_e sdp_attr_set_fmtp_h263_annex_params (void *sdp_ptr,
                                                         u16 level,
                                                         u8 cap_num,
                                                         u16 inst_num,
                                                         tinybool annex_f,
                                                         tinybool annex_i,
                                                         tinybool annex_j,
                                                         tinybool annex_t,
                                                         u16 annex_k_val,
                                                         u16 annex_n_val,
                                               u16 annex_p_val_picture_resize,
                                               u16 annex_p_val_warp);


/* get routines */
extern int32 sdp_attr_get_fmtp_bitrate_type (void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num);

extern int32 sdp_attr_get_fmtp_cif (void *sdp_ptr, u16 level,
                                    u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_fmtp_qcif (void *sdp_ptr, u16 level,
                                     u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_fmtp_sqcif (void *sdp_ptr, u16 level,
                                      u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_fmtp_cif4 (void *sdp_ptr, u16 level,
                                     u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_fmtp_cif16 (void *sdp_ptr, u16 level,
                                      u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_fmtp_maxbr (void *sdp_ptr, u16 level,
                                      u8 cap_num, u16 inst_num);
extern sdp_result_e sdp_attr_get_fmtp_max_average_bitrate (void *sdp_ptr, u16 level,
                                                           u8 cap_num, u16 inst_num, u32* val);
extern sdp_result_e sdp_attr_get_fmtp_usedtx (void *sdp_ptr, u16 level,
                                              u8 cap_num, u16 inst_num, tinybool* val);
extern sdp_result_e sdp_attr_get_fmtp_stereo (void *sdp_ptr, u16 level,
                                              u8 cap_num, u16 inst_num, tinybool* val);
extern sdp_result_e sdp_attr_get_fmtp_useinbandfec (void *sdp_ptr, u16 level,
                                                    u8 cap_num, u16 inst_num, tinybool* val);
extern char* sdp_attr_get_fmtp_maxcodedaudiobandwidth (void *sdp_ptr, u16 level,
                                                             u8 cap_num, u16 inst_num);
extern sdp_result_e sdp_attr_get_fmtp_cbr (void *sdp_ptr, u16 level,
                             u8 cap_num, u16 inst_num, tinybool* val);
extern sdp_result_e sdp_attr_set_fmtp_streams (void *sdp_ptr, u16 level,
                             u8 cap_num, u16 inst_num, u32 streams);
extern sdp_result_e sdp_attr_get_fmtp_streams (void *sdp_ptr, u16 level,
                             u8 cap_num, u16 inst_num, u32* val);
extern sdp_result_e sdp_attr_get_fmtp_data_channel_protocol (void *sdp_ptr, u16 level,
                             u8 cap_num, u16 inst_num, char* protocol);
extern sdp_result_e sdp_attr_set_fmtp_data_channel_protocol (void *sdp_ptr, u16 level,
                             u8 cap_num, u16 inst_num,
                             const char *protocol);
extern int32 sdp_attr_get_fmtp_custom_x (void *sdp_ptr, u16 level,
                                         u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_fmtp_custom_y (void *sdp_ptr, u16 level,
                                         u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_fmtp_custom_mpi (void *sdp_ptr, u16 level,
                                           u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_fmtp_par_width (void *sdp_ptr, u16 level,
                                          u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_fmtp_par_height (void *sdp_ptr, u16 level,
                                           u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_fmtp_bpp (void *sdp_ptr, u16 level,
                                    u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_fmtp_hrd (void *sdp_ptr, u16 level,
                                    u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_fmtp_profile (void *sdp_ptr, u16 level,
                                        u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_fmtp_level (void *sdp_ptr, u16 level,
                                      u8 cap_num, u16 inst_num);
extern tinybool sdp_attr_get_fmtp_interlace (void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num);
extern tinybool sdp_attr_get_fmtp_annex_d (void *sdp_ptr, u16 level,
                                           u8 cap_num, u16 inst_num);
extern tinybool sdp_attr_get_fmtp_annex_f (void *sdp_ptr, u16 level,
                                           u8 cap_num, u16 inst_num);
extern tinybool sdp_attr_get_fmtp_annex_i (void *sdp_ptr, u16 level,
                                           u8 cap_num, u16 inst_num);
extern tinybool sdp_attr_get_fmtp_annex_j (void *sdp_ptr, u16 level,
                                           u8 cap_num, u16 inst_num);
extern tinybool sdp_attr_get_fmtp_annex_t (void *sdp_ptr, u16 level,
                                           u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_fmtp_annex_k_val (void *sdp_ptr, u16 level,
                                            u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_fmtp_annex_n_val (void *sdp_ptr, u16 level,
                                            u8 cap_num, u16 inst_num);

extern int32 sdp_attr_get_fmtp_annex_p_picture_resize (void *sdp_ptr, u16 level,
                                                       u8 cap_num, u16 inst_num);
extern int32 sdp_attr_get_fmtp_annex_p_warp (void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num);

/* H.264 codec specific params */

extern const char *sdp_attr_get_fmtp_profile_id(void *sdp_ptr, u16 level,
                                                u8 cap_num, u16 inst_num);
extern const char *sdp_attr_get_fmtp_param_sets(void *sdp_ptr, u16 level,
                                                u8 cap_num, u16 inst_num);
extern sdp_result_e sdp_attr_get_fmtp_pack_mode (void *sdp_ptr, u16 level,
                                          u8 cap_num, u16 inst_num, u16 *val);

extern sdp_result_e sdp_attr_get_fmtp_level_asymmetry_allowed (void *sdp_ptr, u16 level,
                                          u8 cap_num, u16 inst_num, u16 *val);

extern sdp_result_e sdp_attr_get_fmtp_interleaving_depth (void *sdp_ptr, u16 level,
                                                   u8 cap_num, u16 inst_num,
                                                   u16 *val);
extern sdp_result_e sdp_attr_get_fmtp_max_don_diff (void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num,
                                             u32 *val);

/* The following four H.264 parameters that require special handling as
 * the values range from 0 - 4294967295
 */
extern sdp_result_e sdp_attr_get_fmtp_deint_buf_req (void *sdp_ptr, u16 level,
                                                    u8 cap_num, u16 inst_num,
                                                    u32 *val);
extern sdp_result_e sdp_attr_get_fmtp_deint_buf_cap (void *sdp_ptr, u16 level,
                                                    u8 cap_num, u16 inst_num,
                                                    u32 *val);
extern sdp_result_e sdp_attr_get_fmtp_init_buf_time (void *sdp_ptr, u16 level,
                                                    u8 cap_num, u16 inst_num,
                                                    u32 *val);
extern sdp_result_e sdp_attr_get_fmtp_max_rcmd_nalu_size (void *sdp_ptr,
						    u16 level, u8 cap_num,
						    u16 inst_num, u32 *val);


extern sdp_result_e sdp_attr_get_fmtp_max_mbps (void *sdp_ptr, u16 level,
                                         u8 cap_num, u16 inst_num, u32 *val);
extern sdp_result_e sdp_attr_get_fmtp_max_fs (void *sdp_ptr, u16 level,
                                       u8 cap_num, u16 inst_num, u32 *val);
extern sdp_result_e sdp_attr_get_fmtp_max_cpb (void *sdp_ptr, u16 level,
                                        u8 cap_num, u16 inst_num, u32 *val);
extern sdp_result_e sdp_attr_get_fmtp_max_dpb (void *sdp_ptr, u16 level,
                                        u8 cap_num, u16 inst_num, u32 *val);
extern sdp_result_e sdp_attr_get_fmtp_max_br (void *sdp_ptr, u16 level,
                                       u8 cap_num, u16 inst_num, u32 *val);
extern tinybool sdp_attr_fmtp_is_redundant_pic_cap (void *sdp_ptr, u16 level,
                                                    u8 cap_num,
                                                    u16 inst_num);
extern tinybool sdp_attr_fmtp_is_parameter_add (void *sdp_ptr, u16 level,
                                                u8 cap_num,
                                                u16 inst_num);
extern tinybool sdp_attr_fmtp_is_annexa_set (void *sdp_ptr, u16 level,
                                             u8 cap_num,
                                             u16 inst_num);

extern tinybool sdp_attr_fmtp_is_annexb_set (void *sdp_ptr, u16 level,
                                             u8 cap_num,
                                             u16 inst_num);

extern sdp_fmtp_format_type_e  sdp_attr_fmtp_get_fmtp_format (void *sdp_ptr, u16 level, u8 cap_num,
                                                              u16 inst_num);

extern u16 sdp_attr_get_pccodec_num_payload_types(void *sdp_ptr, u16 level,
                                                  u8 cap_num, u16 inst_num);
extern u16 sdp_attr_get_pccodec_payload_type(void *sdp_ptr, u16 level,
                                   u8 cap_num, u16 inst_num, u16 payload_num);
extern sdp_result_e sdp_attr_add_pccodec_payload_type(void *sdp_ptr,
                                               u16 level, u8 cap_num,
                                               u16 inst_num, u16 payload_type);
extern u16 sdp_attr_get_xcap_first_cap_num(void *sdp_ptr, u16 level,
                                            u16 inst_num);
extern sdp_media_e sdp_attr_get_xcap_media_type(void *sdp_ptr, u16 level,
                                                u16 inst_num);
extern sdp_transport_e sdp_attr_get_xcap_transport_type(void *sdp_ptr,
                                         u16 level, u16 inst_num);
extern u16 sdp_attr_get_xcap_num_payload_types(void *sdp_ptr, u16 level,
                                               u16 inst_num);
extern u16 sdp_attr_get_xcap_payload_type(void *sdp_ptr, u16 level,
                                          u16 inst_num, u16 payload_num,
                                          sdp_payload_ind_e *indicator);
extern sdp_result_e sdp_attr_set_xcap_media_type(void *sdp_ptr, u16 level,
                                          u16 inst_num, sdp_media_e media);
extern sdp_result_e sdp_attr_set_xcap_transport_type(void *sdp_ptr, u16 level,
                                      u16 inst_num, sdp_transport_e transport);
extern sdp_result_e sdp_attr_add_xcap_payload_type(void *sdp_ptr, u16 level,
                                      u16 inst_num, u16 payload_type,
                                      sdp_payload_ind_e indicator);
extern u16 sdp_attr_get_cdsc_first_cap_num(void *sdp_ptr, u16 level,
                                            u16 inst_num);
extern sdp_media_e sdp_attr_get_cdsc_media_type(void *sdp_ptr, u16 level,
                                                u16 inst_num);
extern sdp_transport_e sdp_attr_get_cdsc_transport_type(void *sdp_ptr,
                                         u16 level, u16 inst_num);
extern u16 sdp_attr_get_cdsc_num_payload_types(void *sdp_ptr, u16 level,
                                               u16 inst_num);
extern u16 sdp_attr_get_cdsc_payload_type(void *sdp_ptr, u16 level,
                                          u16 inst_num, u16 payload_num,
                                          sdp_payload_ind_e *indicator);
extern sdp_result_e sdp_attr_set_cdsc_media_type(void *sdp_ptr, u16 level,
                                          u16 inst_num, sdp_media_e media);
extern sdp_result_e sdp_attr_set_cdsc_transport_type(void *sdp_ptr, u16 level,
                                      u16 inst_num, sdp_transport_e transport);
extern sdp_result_e sdp_attr_add_cdsc_payload_type(void *sdp_ptr, u16 level,
                                      u16 inst_num, u16 payload_type,
                                      sdp_payload_ind_e indicator);
extern tinybool sdp_media_dynamic_payload_valid (void *sdp_ptr, u16 payload_type,
                                                 u16 m_line);

extern sdp_result_e sdp_attr_set_rtr_confirm (void *, u16 , \
                                              u8 ,u16 ,tinybool );
extern tinybool sdp_attr_get_rtr_confirm (void *, u16, u8, u16);

extern tinybool sdp_attr_get_silencesupp_enabled(void *sdp_ptr, u16 level,
                                                 u8 cap_num, u16 inst_num);
extern u16 sdp_attr_get_silencesupp_timer(void *sdp_ptr, u16 level,
                                          u8 cap_num, u16 inst_num,
                                          tinybool *null_ind);
extern sdp_silencesupp_pref_e sdp_attr_get_silencesupp_pref(void *sdp_ptr,
                                                            u16 level,
                                                            u8 cap_num,
                                                            u16 inst_num);
extern sdp_silencesupp_siduse_e sdp_attr_get_silencesupp_siduse(void *sdp_ptr,
                                                                u16 level,
                                                                u8 cap_num,
                                                                u16 inst_num);
extern u8 sdp_attr_get_silencesupp_fxnslevel(void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num,
                                             tinybool *null_ind);
extern sdp_result_e sdp_attr_set_silencesupp_enabled(void *sdp_ptr, u16 level,
                                                     u8 cap_num, u16 inst_num,
                                                     tinybool enable);
extern sdp_result_e sdp_attr_set_silencesupp_timer(void *sdp_ptr, u16 level,
                                                   u8 cap_num, u16 inst_num,
                                                   u16 value,
                                                   tinybool null_ind);
extern sdp_result_e sdp_attr_set_silencesupp_pref(void *sdp_ptr, u16 level,
                                                  u8 cap_num, u16 inst_num,
                                                  sdp_silencesupp_pref_e pref);
extern sdp_result_e sdp_attr_set_silencesupp_siduse(void *sdp_ptr, u16 level,
                                                    u8 cap_num, u16 inst_num,
                                                    sdp_silencesupp_siduse_e siduse);
extern sdp_result_e sdp_attr_set_silencesupp_fxnslevel(void *sdp_ptr,
                                                       u16 level,
                                                       u8 cap_num,
                                                       u16 inst_num,
                                                       u16 value,
                                                       tinybool null_ind);

extern sdp_mediadir_role_e sdp_attr_get_comediadir_role(void *sdp_ptr,
                                                        u16 level, u8 cap_num,
                                                        u16 inst_num);
extern sdp_result_e sdp_attr_set_comediadir_role(void *sdp_ptr, u16 level,
                                                 u8 cap_num, u16 inst_num,
                                                sdp_mediadir_role_e role);

extern sdp_result_e sdp_delete_all_media_direction_attrs (void *sdp_ptr,
                                                          u16 level);

extern u16 sdp_attr_get_mptime_num_intervals(
    void *sdp_ptr, u16 level, u8 cap_num, u16 inst_num);
extern u16 sdp_attr_get_mptime_interval(
    void *sdp_ptr, u16 level, u8 cap_num, u16 inst_num, u16 interval_num);
extern sdp_result_e sdp_attr_add_mptime_interval(
    void *sdp_ptr, u16 level, u8 cap_num, u16 inst_num, u16 interval);


extern sdp_result_e sdp_delete_bw_line (void *sdp_ptr, u16 level, u16 inst_num);
extern sdp_result_e sdp_copy_all_bw_lines(void *src_sdp_ptr, void *dst_sdp_ptr,
                                          u16 src_level, u16 dst_level);
extern sdp_bw_modifier_e sdp_get_bw_modifier(void *sdp_ptr, u16 level,
                                             u16 inst_num);
extern int32 sdp_get_bw_value(void *sdp_ptr, u16 level, u16 inst_num);
extern int32 sdp_get_num_bw_lines (void *sdp_ptr, u16 level);

extern sdp_result_e sdp_add_new_bw_line(void *sdp_ptr, u16 level,
                                         sdp_bw_modifier_e bw_modifier, u16 *inst_num);
extern sdp_result_e sdp_set_bw(void *sdp_ptr, u16 level, u16 inst_num,
                               sdp_bw_modifier_e value, u32 bw_val);

extern sdp_group_attr_e sdp_get_group_attr(void *sdp_ptr, u16 level,
					   u8 cap_num, u16 inst_num);
extern sdp_result_e sdp_set_group_attr(void *sdp_ptr, u16 level,
                                       u8 cap_num, u16 inst_num,
                                       sdp_group_attr_e value);

extern const char* sdp_attr_get_x_sidout (void *sdp_ptr, u16 level,
                                     u8 cap_num, u16 inst_num);

extern sdp_result_e sdp_attr_set_x_sidout (void *sdp_ptr, u16 level,
                                   u8 cap_num, u16 inst_num,
                                   const char *sidout);

extern const char* sdp_attr_get_x_sidin (void *sdp_ptr, u16 level,
                                     u8 cap_num, u16 inst_num);

extern sdp_result_e sdp_attr_set_x_sidin (void *sdp_ptr, u16 level,
                                   u8 cap_num, u16 inst_num,
                                   const char *sidin);

extern const char* sdp_attr_get_x_confid (void *sdp_ptr, u16 level,
                                     u8 cap_num, u16 inst_num);

extern sdp_result_e sdp_attr_set_x_confid (void *sdp_ptr, u16 level,
                                   u8 cap_num, u16 inst_num,
                                   const char *confid);

extern sdp_result_e sdp_attr_set_ice_candidate(void *sdp_ptr, u16 level,
                              u8 cap_num, u16 inst_num, const char *ice_candidate);

extern u16 sdp_get_group_num_id(void *sdp_ptr, u16 level,
                                u8 cap_num, u16 inst_num);
extern sdp_result_e sdp_set_group_num_id(void *sdp_ptr, u16 level,
                                         u8 cap_num, u16 inst_num,
                                         u16 group_num_id);

extern int32 sdp_get_group_id(void *sdp_ptr, u16 level,
                              u8 cap_num, u16 inst_num, u16 id_num);
extern sdp_result_e sdp_set_group_id (void *sdp_ptr, u16 level,
                                      u8 cap_num, u16 inst_num, u16 group_id);


extern int32 sdp_get_mid_value(void *sdp_ptr, u16 level);
extern sdp_result_e sdp_set_mid_value(void *sdp_ptr, u16 level, u32 mid_val);

extern sdp_result_e sdp_set_source_filter(void *sdp_ptr, u16 level,
                                          u8 cap_num, u16 inst_num,
                                          sdp_src_filter_mode_e mode,
                                          sdp_nettype_e nettype,
                                          sdp_addrtype_e addrtype,
                                          const char *dest_addr,
                                          const char *src_addr);
extern sdp_result_e sdp_include_new_filter_src_addr(void *sdp_ptr, u16 level,
                                                    u8 cap_num, u16 inst_num,
                                                    const char *src_addr);
extern sdp_src_filter_mode_e sdp_get_source_filter_mode(void *sdp_ptr,
                                                  u16 level, u8 cap_num,
                                                  u16 inst_num);
extern sdp_result_e sdp_get_filter_destination_attributes(void *sdp_ptr,
                                                  u16 level, u8 cap_num,
                                                  u16 inst_num,
                                                  sdp_nettype_e *nettype,
                                                  sdp_addrtype_e *addrtype,
                                                  char *dest_addr);
extern int32 sdp_get_filter_source_address_count(void *sdp_ptr, u16 level,
                                                 u8 cap_num, u16 inst_num);
extern sdp_result_e sdp_get_filter_source_address (void *sdp_ptr, u16 level,
                                                   u8 cap_num, u16 inst_num,
                                                   u16 src_addr_id,
                                                   char *src_addr);

extern sdp_result_e sdp_set_rtcp_unicast_mode(void *sdp_ptr, u16 level,
                                              u8 cap_num, u16 inst_num,
                                              sdp_rtcp_unicast_mode_e mode);
extern sdp_rtcp_unicast_mode_e sdp_get_rtcp_unicast_mode(void *sdp_ptr,
                                              u16 level, u8 cap_num,
                                              u16 inst_num);

void sdp_crypto_debug(char *buffer, ulong length_bytes);
char * sdp_debug_msg_filter(char *buffer, ulong length_bytes);

extern int32
sdp_attr_get_sdescriptions_tag(void *sdp_ptr,
                               u16 level,
                               u8 cap_num,
		               u16 inst_num);

extern sdp_srtp_crypto_suite_t
sdp_attr_get_sdescriptions_crypto_suite(void *sdp_ptr,
                                        u16 level,
                                        u8 cap_num,
		                        u16 inst_num);

extern const char*
sdp_attr_get_sdescriptions_key(void *sdp_ptr,
                               u16 level,
                               u8 cap_num,
		               u16 inst_num);

extern const char*
sdp_attr_get_sdescriptions_salt(void *sdp_ptr,
                                u16 level,
                                u8 cap_num,
		                u16 inst_num);

extern const char*
sdp_attr_get_sdescriptions_lifetime(void *sdp_ptr,
                                    u16 level,
                                    u8 cap_num,
		                    u16 inst_num);

extern sdp_result_e
sdp_attr_get_sdescriptions_mki(void *sdp_ptr,
                               u16 level,
                               u8 cap_num,
		               u16 inst_num,
			       const char **mki_value,
			       u16 *mki_length);

extern const char*
sdp_attr_get_sdescriptions_session_params(void *sdp_ptr,
                                          u16 level,
                                          u8 cap_num,
		                          u16 inst_num);

extern unsigned char
sdp_attr_get_sdescriptions_key_size(void *sdp_ptr,
                                    u16 level,
				    u8 cap_num,
				    u16 inst_num);

extern unsigned char
sdp_attr_get_sdescriptions_salt_size(void *sdp_ptr,
                                     u16 level,
				     u8 cap_num,
				     u16 inst_num);

extern unsigned long
sdp_attr_get_srtp_crypto_selection_flags(void *sdp_ptr,
                                         u16 level,
					 u8 cap_num,
					 u16 inst_num);

extern sdp_result_e
sdp_attr_set_sdescriptions_tag(void *sdp_ptr, u16 level,
                               u8 cap_num, u16 inst_num,
                               int32 tag_num);

extern sdp_result_e
sdp_attr_set_sdescriptions_crypto_suite(void *sdp_ptr, u16 level,
                                        u8 cap_num, u16 inst_num,
                                        sdp_srtp_crypto_suite_t crypto_suite);

extern sdp_result_e
sdp_attr_set_sdescriptions_key(void *sdp_ptr, u16 level,
                               u8 cap_num, u16 inst_num,
                               char *key);

extern sdp_result_e
sdp_attr_set_sdescriptions_salt(void *sdp_ptr, u16 level,
                                u8 cap_num, u16 inst_num,
                                char *salt);

extern sdp_result_e
sdp_attr_set_sdescriptions_lifetime(void *sdp_ptr, u16 level,
                                    u8 cap_num, u16 inst_num,
                                    char *lifetime);

extern sdp_result_e
sdp_attr_set_sdescriptions_mki(void *sdp_ptr, u16 level,
                               u8 cap_num, u16 inst_num,
                               char *mki_value,
			       u16 mki_length);

extern sdp_result_e
sdp_attr_set_sdescriptions_key_size(void *sdp_ptr,
                                     u16 level,
				     u8 cap_num,
				     u16 inst_num,
				     unsigned char key_size);

extern sdp_result_e
sdp_attr_set_sdescriptions_salt_size(void *sdp_ptr,
                                     u16 level,
				     u8 cap_num,
				     u16 inst_num,
				     unsigned char salt_size);

sdp_result_e
sdp_attr_get_ice_attribute (void *sdp_ptr, u16 level,
                           u8 cap_num, sdp_attr_e sdp_attr, u16 inst_num,
                           char **out);
sdp_result_e
sdp_attr_set_ice_attribute(void *sdp_ptr, u16 level,
                           u8 cap_num, sdp_attr_e sdp_attr, u16 inst_num, const char *ice_attrib);

sdp_result_e
sdp_attr_get_rtcp_mux_attribute (void *sdp_ptr, u16 level,
                                  u8 cap_num, sdp_attr_e sdp_attr, u16 inst_num,
                                  tinybool *rtcp_mux);

sdp_result_e
sdp_attr_set_rtcp_mux_attribute(void *sdp_ptr, u16 level,
                              u8 cap_num, sdp_attr_e sdp_attr, u16 inst_num, const tinybool rtcp_mux);

sdp_result_e
sdp_attr_get_dtls_fingerprint_attribute (void *sdp_ptr, u16 level,
                                  u8 cap_num, sdp_attr_e sdp_attr, u16 inst_num,
                                  char **out);

sdp_result_e
sdp_attr_set_dtls_fingerprint_attribute(void *sdp_ptr, u16 level,
                              u8 cap_num, sdp_attr_e sdp_attr, u16 inst_num, const char *dtls_fingerprint);

sdp_rtcp_fb_ack_type_e
sdp_attr_get_rtcp_fb_ack(void *sdp_ptr, u16 level, u16 payload_type, u16 inst);

sdp_rtcp_fb_nack_type_e
sdp_attr_get_rtcp_fb_nack(void *sdp_ptr, u16 level, u16 payload_type, u16 inst);

u32
sdp_attr_get_rtcp_fb_trr_int(void *sdp_ptr, u16 level, u16 payload_type,
                             u16 inst);

sdp_rtcp_fb_ccm_type_e
sdp_attr_get_rtcp_fb_ccm(void *sdp_ptr, u16 level, u16 payload_type, u16 inst);

sdp_result_e
sdp_attr_set_rtcp_fb_ack(void *sdp_ptr, u16 level, u16 payload_type, u16 inst,
                         sdp_rtcp_fb_ack_type_e type);

sdp_result_e
sdp_attr_set_rtcp_fb_nack(void *sdp_ptr, u16 level, u16 payload_type, u16 inst,
                          sdp_rtcp_fb_nack_type_e);

sdp_result_e
sdp_attr_set_rtcp_fb_trr_int(void *sdp_ptr, u16 level, u16 payload_type,
                             u16 inst, u32 interval);

sdp_result_e
sdp_attr_set_rtcp_fb_ccm(void *sdp_ptr, u16 level, u16 payload_type, u16 inst,
                         sdp_rtcp_fb_ccm_type_e);

#endif /* _SDP_H_ */
