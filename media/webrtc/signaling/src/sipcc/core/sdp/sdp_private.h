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

#ifndef _SDP_PRIVATE_H_
#define _SDP_PRIVATE_H_


#include "sdp.h"

/* SDP Defines */

#define SDP_MAX_STRING_LEN      80  /* Max len for SDP string       */
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
#define SDP_MAX_LINE_LEN   80 /* Max len for SDP Line */

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
    char		      encrypt_key[SDP_MAX_STRING_LEN+1];
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
    char		      profile_level_id[SDP_MAX_STRING_LEN+1];
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
        sdp_fmtp_t            fmtp;
        sdp_qos_t             qos;
        sdp_curr_t            curr;
        sdp_des_t             des;
        sdp_conf_t            conf;
        sdp_transport_map_t   transport_map;	/* A rtpmap or sprtmap */
        sdp_subnet_t          subnet;
        sdp_t38_ratemgmt_e    t38ratemgmt;
        sdp_t38_udpec_e       t38udpec;
        sdp_pccodec_t         pccodec;
        sdp_silencesupp_t     silencesupp;
        sdp_mca_t            *cap_p;	/* A X-CAP or CDSC attribute */	
        sdp_rtr_t             rtr;
	sdp_comediadir_t      comediadir; 
	sdp_srtp_crypto_context_t srtp_context;
        sdp_mptime_t          mptime;
        sdp_stream_data_t     stream_data;
        char                  unknown[SDP_MAX_STRING_LEN+1];
        sdp_source_filter_t   source_filter; 
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
    sdp_attr_e		      last_cap_type;
    
    /* MCA - Media, connection, and attributes */
    sdp_mca_t                *mca_p;
    ushort                    mca_count;
} sdp_t;


/* Token processing table. */
typedef struct {
    char *name;
    sdp_result_e (*parse_func)(sdp_t *sdp_p, u16 level, const char *ptr);
    sdp_result_e (*build_func)(sdp_t *sdp_p, u16 level, char **ptr, u16 len);
} sdp_tokenarray_t;


/* Attribute processing table. */
typedef struct {
    char *name;
    u16 strlen;
    sdp_result_e (*parse_func)(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                               const char *ptr);
    sdp_result_e (*build_func)(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                               char **ptr, u16 len);
} sdp_attrarray_t;


/* Data declarations */

extern const sdp_attrarray_t sdp_attr[];
extern const sdp_namearray_t sdp_media[];
extern const sdp_namearray_t sdp_nettype[];
extern const sdp_namearray_t sdp_addrtype[];
extern const sdp_namearray_t sdp_transport[];
extern const sdp_namearray_t sdp_encrypt[];
extern const sdp_namearray_t sdp_payload[];
extern const sdp_namearray_t sdp_t38_rate[];
extern const sdp_namearray_t sdp_t38_udpec[];
extern const sdp_namearray_t sdp_qos_strength[];
extern const sdp_namearray_t sdp_qos_direction[];
extern const sdp_namearray_t sdp_qos_status_type[];
extern const sdp_namearray_t sdp_curr_type[];
extern const sdp_namearray_t sdp_des_type[];
extern const sdp_namearray_t sdp_conf_type[];
extern const sdp_namearray_t sdp_mediadir_role[];
extern const sdp_namearray_t sdp_fmtp_codec_param[];
extern const sdp_namearray_t sdp_fmtp_codec_param_val[];
extern const sdp_namearray_t sdp_silencesupp_pref[];
extern const sdp_namearray_t sdp_silencesupp_siduse[];
extern const sdp_namearray_t sdp_srtp_context_crypto_suite[];
extern const sdp_namearray_t sdp_bw_modifier_val[];
extern const sdp_namearray_t sdp_group_attr_val[];
extern const sdp_namearray_t sdp_src_filter_mode_val[]; 
extern const sdp_namearray_t sdp_rtcp_unicast_mode_val[];

extern const  sdp_srtp_crypto_suite_list sdp_srtp_crypto_suite_array[];
/* Function Prototypes */

/* sdp_access.c */
extern sdp_mca_t *sdp_find_media_level(sdp_t *sdp_p, u16 level);
extern sdp_bw_data_t* sdp_find_bw_line (void *sdp_ptr, u16 level, u16 inst_num);

/* sdp_attr.c */
extern sdp_result_e sdp_parse_attribute(sdp_t *sdp_p, u16 level, 
                                        const char *ptr);
extern sdp_result_e sdp_parse_attr_simple_string(sdp_t *sdp_p, 
                                     sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_simple_string(sdp_t *sdp_p, 
                                     sdp_attr_t *attr_p, char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_simple_u32(sdp_t *sdp_p, 
                                     sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_simple_u32(sdp_t *sdp_p, 
                                     sdp_attr_t *attr_p, char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_simple_bool(sdp_t *sdp_p, 
                                     sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_simple_bool(sdp_t *sdp_p, 
                                     sdp_attr_t *attr_p, char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_maxprate(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     const char *ptr);
extern sdp_result_e sdp_parse_attr_fmtp(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_fmtp(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_direction(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_direction(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_qos(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_qos(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_curr(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_curr (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_des(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_des (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_conf(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_conf (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_transport_map(sdp_t *sdp_p, 
				     sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_transport_map(sdp_t *sdp_p, 
				     sdp_attr_t *attr_p, char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_subnet(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_subnet(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_t38_ratemgmt(sdp_t *sdp_p, 
                                     sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_t38_ratemgmt(sdp_t *sdp_p, 
                                     sdp_attr_t *attr_p, char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_t38_udpec(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_t38_udpec(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_cap(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_cap(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_cpar(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_cpar(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_pc_codec(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_pc_codec(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_xcap(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                        const char *ptr);
extern sdp_result_e sdp_build_attr_xcap(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                        char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_xcpar(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                         const char *ptr);
extern sdp_result_e sdp_build_attr_xcpar(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                         char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_rtr(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_rtr(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_comediadir(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                              const char *ptr);
extern sdp_result_e sdp_build_attr_comediadir(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                              char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_silencesupp(sdp_t *sdp_p,
                                               sdp_attr_t *attr_p,
                                               const char *ptr);
extern sdp_result_e sdp_build_attr_silencesupp(sdp_t *sdp_p,
                                               sdp_attr_t *attr_p, 
                                               char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_srtpcontext(sdp_t *sdp_p,
                                               sdp_attr_t *attr_p,
                                               const char *ptr);
extern sdp_result_e sdp_build_attr_srtpcontext(sdp_t *sdp_p,
                                               sdp_attr_t *attr_p, 
                                               char **ptr, u16 len);
extern sdp_result_e sdp_parse_attr_mptime(
    sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_mptime(
    sdp_t *sdp_p, sdp_attr_t *attr_p, char **ptr, u16 len);

extern sdp_result_e sdp_parse_attr_x_sidin(
    sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_x_sidin(
    sdp_t *sdp_p, sdp_attr_t *attr_p, char **ptr, u16 len);

extern sdp_result_e sdp_parse_attr_x_sidout(
    sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_x_sidout(
    sdp_t *sdp_p, sdp_attr_t *attr_p, char **ptr, u16 len);

extern sdp_result_e sdp_parse_attr_x_confid(
    sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_x_confid(
    sdp_t *sdp_p, sdp_attr_t *attr_p, char **ptr, u16 len);

extern sdp_result_e sdp_parse_attr_group(
    sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_group(
    sdp_t *sdp_p, sdp_attr_t *attr_p, char **ptr, u16 len);

extern sdp_result_e sdp_parse_attr_source_filter(
    sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_source_filter(
    sdp_t *sdp_p, sdp_attr_t *attr_p, char **ptr, u16 len);

extern sdp_result_e sdp_parse_attr_rtcp_unicast(
    sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_rtcp_unicast(
    sdp_t *sdp_p, sdp_attr_t *attr_p, char **ptr, u16 len);

/* sdp_attr_access.c */
extern void sdp_free_attr(sdp_attr_t *attr_p);
extern sdp_result_e sdp_find_attr_list(sdp_t *sdp_p, u16 level, u8 cap_num, 
                                       sdp_attr_t **attr_p, char *fname);
extern sdp_attr_t *sdp_find_attr(sdp_t *sdp_p, u16 level, u8 cap_num,
                                 sdp_attr_e attr_type, u16 inst_num);
extern sdp_attr_t *sdp_find_capability(sdp_t *sdp_p, u16 level, u8 cap_num);

/* sdp_config.c */
extern tinybool sdp_verify_conf_ptr(sdp_conf_options_t *conf_p);

/* sdp_main.c */
extern const char *sdp_get_attr_name(sdp_attr_e attr_type);
extern const char *sdp_get_media_name(sdp_media_e media_type);
extern const char *sdp_get_network_name(sdp_nettype_e network_type);
extern const char *sdp_get_address_name(sdp_addrtype_e addr_type);
extern const char *sdp_get_transport_name(sdp_transport_e transport_type);
extern const char *sdp_get_encrypt_name(sdp_encrypt_type_e encrypt_type);
extern const char *sdp_get_payload_name(sdp_payload_e payload);
extern const char *sdp_get_t38_ratemgmt_name(sdp_t38_ratemgmt_e rate);
extern const char *sdp_get_t38_udpec_name(sdp_t38_udpec_e udpec);
extern const char *sdp_get_qos_strength_name(sdp_qos_strength_e strength);
extern const char *sdp_get_qos_direction_name(sdp_qos_dir_e direction);
extern const char *sdp_get_qos_status_type_name(sdp_qos_status_types_e status_type);
extern const char *sdp_get_curr_type_name(sdp_curr_type_e curr_type);
extern const char *sdp_get_des_type_name(sdp_des_type_e des_type);
extern const char *sdp_get_conf_type_name(sdp_conf_type_e conf_type);
extern const char *sdp_get_mediadir_role_name (sdp_mediadir_role_e role);
extern const char *sdp_get_silencesupp_pref_name(sdp_silencesupp_pref_e pref);
extern const char *sdp_get_silencesupp_siduse_name(sdp_silencesupp_siduse_e
                                                   siduse);

extern const char *sdp_get_bw_modifier_name(sdp_bw_modifier_e bw_modifier);
extern const char *sdp_get_group_attr_name(sdp_group_attr_e group_attr);
extern const char *sdp_get_src_filter_mode_name(sdp_src_filter_mode_e type);
extern const char *sdp_get_rtcp_unicast_mode_name(sdp_rtcp_unicast_mode_e type);

extern tinybool sdp_verify_sdp_ptr(sdp_t *sdp_p);


/* sdp_tokens.c */
extern sdp_result_e sdp_parse_version(sdp_t *sdp_p, u16 token, 
                                      const char *ptr);
extern sdp_result_e sdp_build_version(sdp_t *sdp_p, u16 token, char **ptr, 
                                      u16 len);
extern sdp_result_e sdp_parse_owner(sdp_t *sdp_p, u16 token, 
                                    const char *ptr);
extern sdp_result_e sdp_build_owner(sdp_t *sdp_p, u16 token, char **ptr, 
                                    u16 len);
extern sdp_result_e sdp_parse_sessname(sdp_t *sdp_p, u16 token, 
                                       const char *ptr);
extern sdp_result_e sdp_build_sessname(sdp_t *sdp_p, u16 token, char **ptr, 
                                       u16 len);
extern sdp_result_e sdp_parse_sessinfo(sdp_t *sdp_p, u16 token, 
                                       const char *ptr);
extern sdp_result_e sdp_build_sessinfo(sdp_t *sdp_p, u16 token, char **ptr, 
                                       u16 len);
extern sdp_result_e sdp_parse_uri(sdp_t *sdp_p, u16 token, const char *ptr);
extern sdp_result_e sdp_build_uri(sdp_t *sdp_p, u16 token, char **ptr, 
                                  u16 len);
extern sdp_result_e sdp_parse_email(sdp_t *sdp_p, u16 token, const char *ptr);
extern sdp_result_e sdp_build_email(sdp_t *sdp_p, u16 token, char **ptr, 
                                    u16 len);
extern sdp_result_e sdp_parse_phonenum(sdp_t *sdp_p, u16 token, 
                                       const char *ptr);
extern sdp_result_e sdp_build_phonenum(sdp_t *sdp_p, u16 token, char **ptr, 
                                       u16 len);
extern sdp_result_e sdp_parse_connection(sdp_t *sdp_p, u16 token, 
                                         const char *ptr);
extern sdp_result_e sdp_build_connection(sdp_t *sdp_p, u16 token, char **ptr, 
                                         u16 len);
extern sdp_result_e sdp_parse_bandwidth(sdp_t *sdp_p, u16 token, 
                                        const char *ptr);
extern sdp_result_e sdp_build_bandwidth(sdp_t *sdp_p, u16 token, char **ptr, 
                                        u16 len);
extern sdp_result_e sdp_parse_timespec(sdp_t *sdp_p, u16 token, 
                                       const char *ptr);
extern sdp_result_e sdp_build_timespec(sdp_t *sdp_p, u16 token, char **ptr, 
                                       u16 len);
extern sdp_result_e sdp_parse_repeat_time(sdp_t *sdp_p, u16 token, 
                                          const char *ptr);
extern sdp_result_e sdp_build_repeat_time(sdp_t *sdp_p, u16 token, char **ptr, 
                                          u16 len);
extern sdp_result_e sdp_parse_timezone_adj(sdp_t *sdp_p, u16 token, 
                                           const char *ptr);
extern sdp_result_e sdp_build_timezone_adj(sdp_t *sdp_p, u16 token, char **ptr,
                                           u16 len);
extern sdp_result_e sdp_parse_encryption(sdp_t *sdp_p, u16 token, 
                                         const char *ptr);
extern sdp_result_e sdp_build_encryption(sdp_t *sdp_p, u16 token, char **ptr, 
                                         u16 len);
extern sdp_result_e sdp_parse_media(sdp_t *sdp_p, u16 token, const char *ptr);
extern sdp_result_e sdp_build_media(sdp_t *sdp_p, u16 token, char **ptr, 
                                    u16 len);
extern sdp_result_e sdp_parse_attribute(sdp_t *sdp_p, u16 token, 
                                        const char *ptr);
extern sdp_result_e sdp_build_attribute(sdp_t *sdp_p, u16 token, char **ptr, 
                                        u16 len);

extern void sdp_parse_payload_types(sdp_t *sdp_p, sdp_mca_t *mca_p, 
                                     const char *ptr);
extern sdp_result_e sdp_parse_multiple_profile_payload_types(sdp_t *sdp_p, 
                                               sdp_mca_t *mca_p, 
                                               const char *ptr);
extern sdp_result_e 
sdp_parse_attr_sdescriptions(sdp_t *sdp_p, sdp_attr_t *attr_p,
                             const char *ptr);
			      
extern sdp_result_e
sdp_build_attr_sdescriptions(sdp_t *sdp_p, sdp_attr_t *attr_p, 
                             char **ptr, u16 len);
			     

/* sdp_utils.c */
extern sdp_mca_t *sdp_alloc_mca(void);
extern tinybool sdp_validate_maxprate(const char *string_parm);
extern char *sdp_findchar(const char *ptr, char *char_list);
extern char *sdp_getnextstrtok(const char *str, char *tokenstr, 
                               char *delim, sdp_result_e *result);
extern char *sdp_getnextstrtok_noskip (const char *str, char *tokenstr, 
                         char *delim, sdp_result_e *result);
extern u32 sdp_getnextnumtok(const char *str, const char **str_end, 
                             char *delim, sdp_result_e *result);
extern u32 sdp_getnextnumtok_or_null(const char *str, const char **str_end, 
                                     char *delim, tinybool *null_ind,
                                     sdp_result_e *result);
extern tinybool sdp_getchoosetok(const char *str, char **str_end, 
                                 char *delim, sdp_result_e *result);

extern 
tinybool verify_sdescriptions_mki(char *buf, char *mkiVal, u16 *mkiLen);

extern
tinybool verify_sdescriptions_lifetime(char *buf);
			     
/* sdp_services_xxx.c */
extern void sdp_log_errmsg(sdp_errmsg_e err_msg, char *str);
extern void sdp_dump_buffer(char *_ptr, int _size_bytes);

tinybool sdp_checkrange(sdp_t *sdp, char *num, ulong* lval);

#endif /* _SDP_PRIVATE_H_ */
