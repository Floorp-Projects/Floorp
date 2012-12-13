/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdp.h"
#include "ccapi.h"


const char* ccsdpAttrGetFmtpParamSets(void *sdp_ptr, u16 level,
                                            u8 cap_num, u16 inst_num)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->dest_sdp == NULL ) {
    return NULL;
  }
  return sdp_attr_get_fmtp_param_sets(sdpp->dest_sdp, level, cap_num, inst_num);
}

sdp_result_e ccsdpAttrGetFmtpPackMode(void *sdp_ptr, u16 level,
                         u8 cap_num, u16 inst_num, u16 *val)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->dest_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_get_fmtp_pack_mode(sdpp->dest_sdp, level, cap_num, inst_num, val);
}

sdp_result_e ccsdpAttrGetFmtpLevelAsymmetryAllowed(void *sdp_ptr, u16 level,
                         u8 cap_num, u16 inst_num, u16 *val)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->dest_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_get_fmtp_level_asymmetry_allowed(sdpp->dest_sdp, level, cap_num, inst_num, val);
}

const char* ccsdpAttrGetFmtpProfileLevelId (void *sdp_ptr, u16 level,
                                          u8 cap_num, u16 inst_num)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->dest_sdp == NULL ) {
    return NULL;
  }
  return sdp_attr_get_fmtp_profile_id(sdpp->dest_sdp, level, cap_num, inst_num);
}



sdp_result_e ccsdpAttrGetFmtpMaxMbps (void *sdp_ptr, u16 level,
                                u8 cap_num, u16 inst_num, u32 *val)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->dest_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_get_fmtp_max_mbps(sdpp->dest_sdp, level, cap_num, inst_num, val);
}

sdp_result_e ccsdpAttrGetFmtpMaxFs (void *sdp_ptr, u16 level,
                             u8 cap_num, u16 inst_num, u32 *val)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->dest_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_get_fmtp_max_fs(sdpp->dest_sdp, level, cap_num, inst_num, val);
}

sdp_result_e ccsdpAttrGetFmtpMaxCpb (void *sdp_ptr, u16 level,
                                 u8 cap_num, u16 inst_num, u32 *val)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->dest_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_get_fmtp_max_cpb(sdpp->dest_sdp, level, cap_num, inst_num, val);
}

sdp_result_e ccsdpAttrGetFmtpMaxBr (void *sdp_ptr, u16 level,
                             u8 cap_num, u16 inst_num, u32* val)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->dest_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_get_fmtp_max_br(sdpp->dest_sdp, level, cap_num, inst_num, val);
}

int ccsdpGetBandwidthValue (void *sdp_ptr, u16 level, u16 inst_num)

{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->dest_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_get_bw_value(sdpp->dest_sdp, level, inst_num);
}

sdp_result_e ccsdpAttrGetFmtpMaxDpb (void *sdp_ptr, u16 level,
                               u8 cap_num, u16 inst_num, u32 *val)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->dest_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_get_fmtp_max_dpb(sdpp->dest_sdp, level, cap_num, inst_num, val);
}

sdp_result_e ccsdpAddNewAttr (void *sdp_ptr, u16 level, u8 cap_num,
                               sdp_attr_e attr_type, u16 *inst_num)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->src_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_add_new_attr(sdpp->src_sdp, level, cap_num, attr_type, inst_num);
}

sdp_result_e ccsdpAttrSetFmtpPayloadType (void *sdp_ptr, u16 level,
                              u8 cap_num, u16 inst_num, u16 payload_num)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->src_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_set_fmtp_payload_type(sdpp->src_sdp, level, cap_num, inst_num, payload_num);
}


sdp_result_e ccsdpAttrSetFmtpPackMode (void *sdp_ptr, u16 level,
                                          u8 cap_num, u16 inst_num, u16 pack_mode)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->src_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_set_fmtp_pack_mode(sdpp->src_sdp, level, cap_num, inst_num, pack_mode);
}

sdp_result_e ccsdpAttrSetFmtpLevelAsymmetryAllowed (void *sdp_ptr, u16 level,
                                          u8 cap_num, u16 inst_num, u16 asym_allowed)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->src_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_set_fmtp_level_asymmetry_allowed(sdpp->src_sdp, level, cap_num, inst_num, asym_allowed);
}


sdp_result_e ccsdpAttrSetFmtpProfileLevelId (void *sdp_ptr, u16 level,
                               u8 cap_num, u16 inst_num, const char *profile_level_id)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->src_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_set_fmtp_profile_level_id(sdpp->src_sdp, level, cap_num, inst_num, profile_level_id);
}


sdp_result_e ccsdpAttrSetFmtpParameterSets (void *sdp_ptr, u16 level,
                                     u8 cap_num, u16 inst_num, const char *parameter_sets)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->src_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_set_fmtp_parameter_sets(sdpp->src_sdp, level, cap_num, inst_num, parameter_sets);
}



sdp_result_e ccsdpAttrSetFmtpMaxBr (void *sdp_ptr, u16 level,
                              u8 cap_num, u16 inst_num, u32 max_br)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->src_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_set_fmtp_max_br(sdpp->src_sdp, level, cap_num, inst_num, max_br);
}


sdp_result_e ccsdpAttrSetFmtpMaxMbps (void *sdp_ptr, u16 level,
                              u8 cap_num, u16 inst_num, u32 max_mbps)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->src_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_set_fmtp_max_mbps(sdpp->src_sdp, level, cap_num, inst_num, max_mbps);
}

sdp_result_e ccsdpAttrSetFmtpMaxFs (void *sdp_ptr, u16 level,
                        u8 cap_num, u16 inst_num, u32 max_fs)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->src_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_set_fmtp_max_fs(sdpp->src_sdp, level, cap_num, inst_num, max_fs);
}

sdp_result_e ccsdpAttrSetFmtpMaxCpb (void *sdp_ptr, u16 level,
                            u8 cap_num, u16 inst_num, u32 max_cpb)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->src_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_set_fmtp_max_cpb(sdpp->src_sdp, level, cap_num, inst_num, max_cpb);
}

sdp_result_e ccsdpAttrSetFmtpMaxDbp (void *sdp_ptr, u16 level,
                                  u8 cap_num, u16 inst_num, u32 max_dpb)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->src_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_set_fmtp_max_dpb(sdpp->src_sdp, level, cap_num, inst_num, max_dpb);
}


sdp_result_e ccsdpAttrSetFmtpQcif  (void *sdp_ptr, u16 level,
                             u8 cap_num, u16 inst_num, u16 qcif)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->src_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_set_fmtp_qcif(sdpp->src_sdp, level, cap_num, inst_num, qcif);
}

sdp_result_e ccsdpAttrSetFmtpSqcif  (void *sdp_ptr, u16 level,
                            u8 cap_num, u16 inst_num, u16 sqcif)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->src_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_attr_set_fmtp_sqcif(sdpp->src_sdp, level, cap_num, inst_num, sqcif);
}

sdp_result_e ccsdpAddNewBandwidthLine (void *sdp_ptr, u16 level, sdp_bw_modifier_e bw_modifier, u16 *inst_num)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->src_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_add_new_bw_line(sdpp->src_sdp, level, bw_modifier, inst_num);
}


sdp_result_e ccsdpSetBandwidth (void *sdp_ptr, u16 level, u16 inst_num,
                         sdp_bw_modifier_e bw_modifier, u32 bw_val)
{
  cc_sdp_t *sdpp = sdp_ptr;

  if ( sdpp->src_sdp == NULL ) {
    return SDP_INVALID_PARAMETER;
  }
  return sdp_set_bw(sdpp->src_sdp, level, inst_num, bw_modifier, bw_val);
}

const char * ccsdpCodecName(rtp_ptype ptype)
{
  switch (ptype)
 {
    case RTP_NONE:       return "NONE";
    case RTP_PCMU:       return "PCMU";
    case RTP_CELP:       return "CELP";
    case RTP_G726:       return "G726";
    case RTP_GSM:        return "GSM";
    case RTP_G723:       return "G723";
    case RTP_DVI4:       return "DVI4";
    case RTP_DVI4_II:    return "DVI4_II";
    case RTP_LPC:        return "LPC";
    case RTP_PCMA:       return "PCMA";
    case RTP_G722:       return "G722";
    case RTP_G728:       return "G728";
    case RTP_G729:       return "G729";
    case RTP_JPEG:       return "JPEG";
    case RTP_NV:         return "NV";
    case RTP_H261:       return "H261";
    case RTP_H264_P0:    return "H264_P0";
    case RTP_H264_P1:    return "H264_P1";
    case RTP_AVT:        return "AVT";
    case RTP_L16:        return "L16";
    case RTP_H263:       return "H263";
    case RTP_ILBC:       return "iLBC";
    case RTP_OPUS:       return "OPUS";
    case RTP_VP8:        return "VP8";
    case RTP_I420:       return "I420";
    /* case RTP_ISAC:       return "ISAC"; */
  }
  return "UNKNOWN";
}

