/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Skip this file if not doing video on the device.
 *
 *  The ccsdp_xxx api's provide a means to query and populate the SDP attributes
 *  for video m lines. These Api are not needed if we are not supporting video on
 *  the platform. For audio the stack will populate the appropriate attributes
 *
 *
 *  These API's can be invoked from the vcmCheckAttrs() and vcmPopulateAttrs()
 *  methods to populate or extract the value of specific attributes in the video SDP.
 *  These api require an handle to the SDP that is passed in the above methods.
 * <pre>
 * sdp_handle     The SDP handle
 * level       The level the attribute is defined.  Can be either
 *             SDP_SESSION_LEVEL or 0-n specifying a media line level.
 * inst_num    The instance number of the attribute.  Multiple instances
 *             of a particular attribute may exist at each level and so
 *             the inst_num determines the particular attribute at that
 *             level that should be accessed.  Note that this is the
 *             instance number of the specified type of attribute, not the
 *             overall attribute number at the level.  Also note that the
 *             instance number is 1-based.  For example:
 *             v=0
 *             o=mhandley 2890844526 2890842807 IN IP4 126.16.64.4
 *             s=SDP Seminar
 *             c=IN IP4 10.1.0.2
 *             t=0 0
 *             m=audio 1234 RTP/AVP 0 101 102
 *             a=foo 1
 *             a=foo 2
 *             a=bar 1   # This is instance 1 of attribute bar.
 *             a=foo 3   # This is instance 3 of attribute foo.
 * cap_num     Almost all of the attributes may be defined as X-cpar
 *             parameters (with the exception of X-sqn, X-cap, and X-cpar).
 *             If the cap_num is set to zero, then the attribute is not
 *             an X-cpar parameter attribute.  If the cap_num is any other
 *             value, it specifies the capability number that the X-cpar
 *             attribute is specified for.
 * </pre>
 */

#ifndef __CCSDP_H__
#define __CCSDP_H__

#include "cpr_types.h"

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
    RTP_AVT          = 101,
    RTP_L16          = 102,
    RTP_H263         = 103,
    RTP_ILBC         = 116, /* used only to make an offer */
    RTP_OPUS         = 109,
    RTP_VP8          = 120,
    RTP_I420         = 124,
    RTP_ISAC         = 124
} rtp_ptype;

/**
 * IANA-registered static payload types for the RTP/AVP profile.
 * See http://www.iana.org/assignments/rtp-parameters/rtp-parameters.xml
 */
typedef enum static_rtp_ptype_
{
    STATIC_RTP_AVP_PCMU_8000_1          = 0,
    STATIC_RTP_AVP_GSM_8000_1           = 3,
    STATIC_RTP_AVP_G723_8000_1          = 4,
    STATIC_RTP_AVP_DVI4_8000_1          = 5,
    STATIC_RTP_AVP_DVI4_16000_1         = 6,
    STATIC_RTP_AVP_LPC_8000_1           = 7,
    STATIC_RTP_AVP_PCMA_8000_1          = 8,
    STATIC_RTP_AVP_G722_8000_1          = 9,
    STATIC_RTP_AVP_L16_44100_2          = 10,
    STATIC_RTP_AVP_L16_44100_1          = 11,
    STATIC_RTP_AVP_QCELP_8000_1         = 12,
    STATIC_RTP_AVP_CN_8000_1            = 13,
    STATIC_RTP_AVP_MPA_90000_1          = 14,
    STATIC_RTP_AVP_G728_8000_1          = 15,
    STATIC_RTP_AVP_DVI4_11025_1         = 16,
    STATIC_RTP_AVP_DVI4_22050_1         = 17,
    STATIC_RTP_AVP_G729_8000_1          = 18,
    STATIC_RTP_AVP_CELB_90000_1         = 25,
    STATIC_RTP_AVP_JPEG_90000_1         = 26,
    STATIC_RTP_AVP_NV_90000_1           = 28,
    STATIC_RTP_AVP_H261_90000_1         = 31,
    STATIC_RTP_AVP_MPV_90000_1          = 32,
    STATIC_RTP_AVP_MP2T_90000_1         = 33,
    STATIC_RTP_AVP_H263_90000_1         = 34
} static_rtp_ptype;

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
    SDP_ATTR_RTCP_MUX,
    SDP_ATTR_DTLS_FINGERPRINT,
    SDP_ATTR_MAXPTIME,
    SDP_MAX_ATTR_TYPES,
    SDP_ATTR_INVALID
} sdp_attr_e;

/**
 * Gets the value of the fmtp attribute- parameter-sets parameter for H.264 codec
 *
 * @param[in]  sdp_handle     The SDP handle
 * @param[in]  level       The level to check for the attribute.
 * @param[in]  cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in]  inst_num    The attribute instance number to check.
 *
 * @return      parameter-sets value.
 */

const char* ccsdpAttrGetFmtpParamSets(void *sdp_handle, uint16_t level,
                                            uint8_t cap_num, uint16_t inst_num);

/**
 * Gets the value of the fmtp attribute- packetization-mode parameter for H.264 codec
 *
 * @param[in]  sdp_handle     The SDP handle
 * @param[in]  level       The level to check for the attribute.
 * @param[in]  cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in]  inst_num    The attribute instance number to check.
 * @param[out] *val        packetization-mode value in the range 0 - 2.
 *
 * @return     sdp_result_e         SDP_SUCCESS = SUCCESS
 */
sdp_result_e ccsdpAttrGetFmtpPackMode(void *sdp_handle, uint16_t level,
                         uint8_t cap_num, uint16_t inst_num, uint16_t *val);
/**
 * Gets the value of the fmtp attribute- level asymmetry allowed parameter for H.264 codec
 *
 * @param[in]  sdp_handle     The SDP handle
 * @param[in]  level       The level to check for the attribute.
 * @param[in]  cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in]  inst_num    The attribute instance number to check.
 * @param[out] *val        level-asymmetry-allowed param value in the range 0 - 1.
 *
 * @return     sdp_result_e         SDP_SUCCESS = SUCCESS
 */
sdp_result_e ccsdpAttrGetFmtpLevelAsymmetryAllowed(void *sdp_handle, uint16_t level,
                         uint8_t cap_num, uint16_t inst_num, uint16_t *val);


/**
 * Gets the value of the fmtp attribute- profile-level-id parameter for H.264 codec
 *
 * @param[in]  sdp_handle     The SDP handle returned by sdp_init_description.
 * @param[in]  level       The level to check for the attribute.
 * @param[in]  cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in]  inst_num    The attribute instance number to check.
 *
 * @return   char *      profile-level-id value.
 */
const char* ccsdpAttrGetFmtpProfileLevelId (void *sdp_handle, uint16_t level,
                                          uint8_t cap_num, uint16_t inst_num);

/**
 * Gets the value of the fmtp attribute- max-mbps parameter for H.264 codec
 *
 * @param[in] sdp_handle     The SDP handle
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param[out] *val        max-mbps value.
 *
 * @return     sdp_result_e         SDP_SUCCESS
 */

sdp_result_e ccsdpAttrGetFmtpMaxMbps (void *sdp_handle, uint16_t level,
                                uint8_t cap_num, uint16_t inst_num, uint32_t *val);

/**
 * Gets the value of the fmtp attribute- max-fs parameter for H.264 codec
 *
 * @param[in] sdp_handle     The SDP handle
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param[out] *val        max-fs value.
 *
 * @return     sdp_result_e         SDP_SUCCESS
 */
sdp_result_e ccsdpAttrGetFmtpMaxFs (void *sdp_handle, uint16_t level,
                             uint8_t cap_num, uint16_t inst_num, uint32_t *val);

/**
 * Gets the value of the fmtp attribute- max-cpb parameter for H.264 codec
 * @param[in] sdp_handle     The SDP handle
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param [out] *val      max-cpb value.
 *
 * @return     sdp_result_e         SDP_SUCCESS
 */
sdp_result_e ccsdpAttrGetFmtpMaxCpb (void *sdp_handle, uint16_t level,
                                 uint8_t cap_num, uint16_t inst_num, uint32_t *val);

/**
 * Gets the value of the fmtp attribute- max-br parameter for H.264 codec
 *
 * @param[in] sdp_handle     The SDP handle
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param [out] *val      max-br value.
 *
 * @return     sdp_result_e         SDP_SUCCESS
 */
sdp_result_e ccsdpAttrGetFmtpMaxBr (void *sdp_handle, uint16_t level,
                             uint8_t cap_num, uint16_t inst_num, uint32_t* val);

/**
 * Returns the bandwidth value parameter from the b= line.
 *
 * @param[in] sdp_handle     The SDP handle
 * @param[in] level       The level from which to get the bw value.
 * @param[in] inst_num    instance number of bw line at the level. The first
 *                          instance has a inst_num of 1 and so on.
 *
 * @return     A valid numerical bw value or SDP_INVALID_VALUE(-2).
 */
int ccsdpGetBandwidthValue (void *sdp_handle, uint16_t level, uint16_t inst_num);

/**
 * Add a new attribute of the specified type at the given level and capability
 * level or base attribute if cap_num is zero.
 *
 * @param[in] sdp_handle     The SDP handle
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in] attr_type   The type of attribute to add.
 * @param[in] inst_num    Pointer to a uint16_t in which to return the instance number of the newly added attribute.
 *
 * @return     sdp_result_e
 * 		SDP_SUCCESS            Attribute was added successfully.
 *              SDP_NO_RESOURCE        No memory avail for new attribute.
 *              SDP_INVALID_PARAMETER  Specified media line is not defined.
 */
sdp_result_e ccsdpAddNewAttr (void *sdp_handle, uint16_t level, uint8_t cap_num,
                               sdp_attr_e attr_type, uint16_t *inst_num);

/**
 * Gets the value of the fmtp attribute- max-dpb parameter for H.264 codec
 *
 * @param[in] sdp_handle     The SDP handle
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param[out] *val       max-dpb value.
 *
 * @return     sdp_result_e
 * 		SDP_SUCCESS            Attribute was added successfully.
 */

sdp_result_e ccsdpAttrGetFmtpMaxDpb (void *sdp_handle, uint16_t level,
                               uint8_t cap_num, uint16_t inst_num, uint32_t *val);


/**
 * Sets the value of the fmtp attribute payload type parameter for the given attribute.
 *
 * @param[in] sdp_handle     The SDP handle
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param[in] payload_num New payload type value.
 *
 * @return     SDP_SUCCESS            Attribute was added successfully.
 *             SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e ccsdpAttrSetFmtpPayloadType (void *sdp_handle, uint16_t level,
                              uint8_t cap_num, uint16_t inst_num, uint16_t payload_num);

/**
 * Sets the value of the packetization mode attribute parameter for the given attribute.
 *
 * @param[in] sdp_handle     The SDP handle returned by sdp_init_description.
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param[in] pack_mode   Packetization mode value
 *
 * @return     SDP_SUCCESS            Attribute was added successfully.
 *             SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */

sdp_result_e ccsdpAttrSetFmtpPackMode (void *sdp_handle, uint16_t level,
                                          uint8_t cap_num, uint16_t inst_num, uint16_t pack_mode);
/**
 * Sets the value of the level-asymmetry-allowed attribute parameter for the given attribute.
 *
 * @param[in] sdp_handle     The SDP handle returned by sdp_init_description.
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param[in] level_asymmetry_allowed   level asymmetry allowed value (0 or 1).
 *
 * @return     SDP_SUCCESS            Attribute was added successfully.
 *             SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */

sdp_result_e ccsdpAttrSetFmtpLevelAsymmetryAllowed (void *sdp_handle, uint16_t level,
                                          uint8_t cap_num, uint16_t inst_num, uint16_t level_asymmetry_allowed);

/**
 * Sets the value of the profile-level-id parameter for the given attribute.
 *
 * @param[in] sdp_handle     The SDP handle returned by sdp_init_description.
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param[in] profile_level_id profile_level_id to be set
 *
 * @return     SDP_SUCCESS            Attribute was added successfully.
 *             SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */

sdp_result_e ccsdpAttrSetFmtpProfileLevelId (void *sdp_handle, uint16_t level,
                               uint8_t cap_num, uint16_t inst_num, const char *profile_level_id);

/**
 * Sets the value of the profile-level-id parameter for the given attribute.
 *
 * @param[in] sdp_handle     The SDP handle returned by sdp_init_description.
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param[in] parameter_sets parameter_sets to be set
 *
 * @return     SDP_SUCCESS            Attribute was added successfully.
 *             SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */

sdp_result_e ccsdpAttrSetFmtpParameterSets (void *sdp_handle, uint16_t level,
                                     uint8_t cap_num, uint16_t inst_num, const char *parameter_sets);

/**
 * Sets the value of the max-br parameter for the given attribute.
 *
 * @param[in] sdp_handle     The SDP handle returned by sdp_init_description.
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param[in] max_br    max_br value to be set
 *
 * @return     SDP_SUCCESS            Attribute was added successfully.
 *             SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */


sdp_result_e ccsdpAttrSetFmtpMaxBr (void *sdp_handle, uint16_t level,
                              uint8_t cap_num, uint16_t inst_num, uint32_t max_br);

/**
 * Sets the value of the fmtp attribute- max-mbps parameter for H.264 codec
 *
 * @param[in]  sdp_handle     The SDP handle returned by sdp_init_description.
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param[in] max_mbps    value of max_mbps to be set
 *
 * @return     SDP_SUCCESS            Attribute was added successfully.
 *             SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */

sdp_result_e ccsdpAttrSetFmtpMaxMbps (void *sdp_handle, uint16_t level,
                              uint8_t cap_num, uint16_t inst_num, uint32_t max_mbps);

/**
 * Sets the value of the fmtp attribute- max-fs parameter for H.264 codec
 * @param[in]  sdp_handle     The SDP handle returned by sdp_init_description.
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param[in] max_fs      max_fs value to be set
 *
 * @return     SDP_SUCCESS            Attribute was added successfully.
 *             SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e ccsdpAttrSetFmtpMaxFs (void *sdp_handle, uint16_t level,
                        uint8_t cap_num, uint16_t inst_num, uint32_t max_fs);
/**
 * Sets the value of the fmtp attribute- max-cbp parameter for H.264 codec
 * @param[in]  sdp_handle     The SDP handle returned by sdp_init_description.
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param[in] max_cpb      max_cbp value to be set
 *
 * @return     SDP_SUCCESS            Attribute was added successfully.
 *             SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */

sdp_result_e ccsdpAttrSetFmtpMaxCpb (void *sdp_handle, uint16_t level,
                            uint8_t cap_num, uint16_t inst_num, uint32_t max_cpb);
/**
 * Sets the value of the fmtp attribute- max-dbp parameter for H.264 codec
 * @param[in]  sdp_handle     The SDP handle returned by sdp_init_description.
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param[in] max_dpb      max_dbp value to be set
 *
 * @return     SDP_SUCCESS            Attribute was added successfully.
 *             SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */

sdp_result_e ccsdpAttrSetFmtpMaxDbp (void *sdp_handle, uint16_t level,
                                  uint8_t cap_num, uint16_t inst_num, uint32_t max_dpb);


/**
 * Sets the value of the fmtp attribute qcif parameter for the given attribute.
 * @param[in]  sdp_handle     The SDP handle returned by sdp_init_description.
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param[in] qcif        Sets the QCIF value for a video codec
 *
 * @return      SDP_SUCCESS       Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e ccsdpAttrSetFmtpQcif  (void *sdp_handle, uint16_t level,
                             uint8_t cap_num, uint16_t inst_num, uint16_t qcif);

/**
 * Sets the value of the fmtp attribute sqcif parameter for the given attribute.
 *
 * @param[in]  sdp_handle     The SDP handle returned by sdp_init_description.
 * @param[in] level       The level to check for the attribute.
 * @param[in] cap_num     The capability number associated with the attribute if any.  If none, should be zero.
 * @param[in] inst_num    The attribute instance number to check.
 * @param[in] sqcif        Sets the SQCIF value for a video codec
 *
 * @return      SDP_SUCCESS       Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e ccsdpAttrSetFmtpSqcif  (void *sdp_handle, uint16_t level,
                            uint8_t cap_num, uint16_t inst_num, uint16_t sqcif);

/**
 *
 * To specify bandwidth parameters at any level, a bw line must first be
 * added at that level using this function. This function returns the instance
 * number of an existing bw_line that matches bw_modifier type, or of a newly
 * created bw_line of type bw_modifier. After this addition, you can set the
 * properties of the added bw line by using sdp_set_bw().
 *
 * Note carefully though, that since there can be multiple instances of bw
 * lines at any level, you must specify the instance number when setting
 * or getting the properties of a bw line at any level.
 *
 * This function returns the inst_num variable, the instance number
 * of the created bw_line at that level. The instance number is 1 based.
 * <pre>
 * For example:
 *             v=0                               :Session Level
 *             o=mhandley 2890844526 2890842807 IN IP4 126.16.64.4
 *             s=SDP Seminar
 *             c=IN IP4 10.1.0.2
 *             t=0 0
 *             b=AS:60                           : instance number 1
 *             b=TIAS:50780                      : instance number 2
 *             m=audio 1234 RTP/AVP 0 101 102    : 1st Media level
 *             b=AS:12                           : instance number 1
 *             b=TIAS:8480                       : instance number 2
 *             m=audio 1234 RTP/AVP 0 101 102    : 2nd Media level
 *             b=AS:20                           : instance number 1
 * </pre>
 * @param[in]  sdp_handle    The SDP handle returned by sdp_init_description.
 * @param[in]  level      The level to create the bw line.
 * @param[in]  bw_modifier The Type of bandwidth, CT, AS or TIAS.
 * @param[out]  inst_num   This memory is set with the instance number of the newly created bw line instance.
 *
 * @return      SDP_SUCCESS       Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e ccsdpAddNewBandwidthLine (void *sdp_handle, uint16_t level, sdp_bw_modifier_e bw_modifier, uint16_t *inst_num);


/**
 * Once a bandwidth line is added under a level, this function can be used to
 * set the properties of that bandwidth line.
 *
 * @param[in]  sdp_handle     The SDP handle returned by sdp_init_description.
 * @param[in]  level       The level to at which the bw line resides.
 * @param[in]  inst_num    The instance number of the bw line that is to be set.
 * @param[in]  bw_modifier The Type of bandwidth, CT, AS or TIAS.
 * @param[in]  bw_val      Numerical bandwidth value.
 *
 * @note Before calling this function to set the bw line, the bw line must
 * be added using sdp_add_new_bw_line at the required level.
 *
 * @return      SDP_SUCCESS       Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e ccsdpSetBandwidth (void *sdp_handle, uint16_t level, uint16_t inst_num,
                         sdp_bw_modifier_e bw_modifier, uint32_t bw_val);

/**
 * Returns a string representation of a codec's name.
 *
 * @param[in]  rtp_ptype      The value taken from the rtp_ptype enumeration
 *
 * @return     A string representing the name of the codec
 */
const char * ccsdpCodecName(rtp_ptype ptype);

#endif
