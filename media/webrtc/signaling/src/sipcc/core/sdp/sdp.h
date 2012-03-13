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

#ifndef _SDP_H_
#define _SDP_H_

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

#define SDP_MAX_LEN                1024

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


/* Media flow direction */
typedef enum {
    SDP_DIRECTION_INACTIVE,
    SDP_DIRECTION_SENDONLY,
    SDP_DIRECTION_RECVONLY,
    SDP_DIRECTION_SENDRECV,
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
    SDP_MODE,

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

    SDP_LEVEL_ASYMMETRY_ALLOWED,
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
extern void *sdp_init_description(void *config_p);
extern void sdp_debug(void *sdp_ptr, sdp_debug_e debug_type, tinybool debug_flag);
extern void sdp_set_string_debug(void *sdp_ptr, char *debug_str);
extern sdp_result_e sdp_parse(void *sdp_ptr, char **bufp, u16 len);
extern sdp_result_e sdp_build(void *sdp_ptr, char **bufp, u16 len);
extern void *sdp_copy(void *sdp_ptr);
extern sdp_result_e sdp_free_description(void *sdp_ptr);

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
                                          int32 portnum);
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


#endif /* _SDP_H_ */
