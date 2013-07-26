/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdp_os_defs.h"
#include "sdp.h"
#include "sdp_private.h"
#include "vcm.h"
#include "CSFLog.h"

static const char* logTag = "sdp_main";

/* Note: These *must* be in the same order as the enum types. */
const sdp_tokenarray_t sdp_token[SDP_MAX_TOKENS] =
{
    {"v=", sdp_parse_version,      sdp_build_version },
    {"o=", sdp_parse_owner,        sdp_build_owner },
    {"s=", sdp_parse_sessname,     sdp_build_sessname },
    {"i=", sdp_parse_sessinfo,     sdp_build_sessinfo },
    {"u=", sdp_parse_uri,          sdp_build_uri },
    {"e=", sdp_parse_email,        sdp_build_email },
    {"p=", sdp_parse_phonenum,     sdp_build_phonenum },
    {"c=", sdp_parse_connection,   sdp_build_connection },
    {"b=", sdp_parse_bandwidth,    sdp_build_bandwidth },
    {"t=", sdp_parse_timespec,     sdp_build_timespec },
    {"r=", sdp_parse_repeat_time,  sdp_build_repeat_time },
    {"z=", sdp_parse_timezone_adj, sdp_build_timezone_adj },
    {"k=", sdp_parse_encryption,   sdp_build_encryption },
    {"a=", sdp_parse_attribute,    sdp_build_attribute },
    {"m=", sdp_parse_media,        sdp_build_media }
};


/* Note: These *must* be in the same order as the enum types. */
const sdp_attrarray_t sdp_attr[SDP_MAX_ATTR_TYPES] =
{
    {"bearer", sizeof("bearer"),
     sdp_parse_attr_simple_string, sdp_build_attr_simple_string },
    {"called", sizeof("called"),
     sdp_parse_attr_simple_string, sdp_build_attr_simple_string },
    {"connection_type", sizeof("connection_type"),
     sdp_parse_attr_simple_string, sdp_build_attr_simple_string },
    {"dialed", sizeof("dialed"),
     sdp_parse_attr_simple_string, sdp_build_attr_simple_string },
    {"dialing", sizeof("dialing"),
     sdp_parse_attr_simple_string, sdp_build_attr_simple_string },
    {"direction", sizeof("direction"),
     sdp_parse_attr_comediadir, sdp_build_attr_comediadir },
    {"eecid", sizeof("eecid"),
     sdp_parse_attr_simple_u32, sdp_build_attr_simple_u32 },
    {"fmtp", sizeof("fmtp"),
     sdp_parse_attr_fmtp, sdp_build_attr_fmtp },
    {"framing", sizeof("framing"),
     sdp_parse_attr_simple_string, sdp_build_attr_simple_string },
    {"inactive", sizeof("inactive"),
     sdp_parse_attr_direction, sdp_build_attr_direction },
    {"ptime", sizeof("ptime"),
     sdp_parse_attr_simple_u32, sdp_build_attr_simple_u32 },
    {"qos", sizeof("qos"),
     sdp_parse_attr_qos, sdp_build_attr_qos },
    {"curr", sizeof("curr"),
     sdp_parse_attr_curr, sdp_build_attr_curr },
    {"des", sizeof("des"),
     sdp_parse_attr_des, sdp_build_attr_des},
    {"conf", sizeof("conf"),
     sdp_parse_attr_conf, sdp_build_attr_conf},
    {"recvonly", sizeof("recvonly"),
     sdp_parse_attr_direction, sdp_build_attr_direction },
    {"rtpmap", sizeof("rtpmap"),
     sdp_parse_attr_transport_map, sdp_build_attr_transport_map },
    {"secure", sizeof("secure"),
     sdp_parse_attr_qos, sdp_build_attr_qos },
    {"sendonly", sizeof("sendonly"),
     sdp_parse_attr_direction, sdp_build_attr_direction },
    {"sendrecv", sizeof("sendrecv"),
     sdp_parse_attr_direction, sdp_build_attr_direction },
    {"subnet", sizeof("subnet"),
     sdp_parse_attr_subnet, sdp_build_attr_subnet },
    {"T38FaxVersion", sizeof("T38FaxVersion"),
     sdp_parse_attr_simple_u32, sdp_build_attr_simple_u32 },
    {"T38MaxBitRate", sizeof("T38MaxBitRate"),
     sdp_parse_attr_simple_u32, sdp_build_attr_simple_u32 },
    {"T38FaxFillBitRemoval", sizeof("T38FaxFillBitRemoval"),
     sdp_parse_attr_simple_bool, sdp_build_attr_simple_bool },
    {"T38FaxTranscodingMMR", sizeof("T38FaxTranscodingMMR"),
     sdp_parse_attr_simple_bool, sdp_build_attr_simple_bool },
    {"T38FaxTranscodingJBIG", sizeof("T38FaxTranscodingJBIG"),
     sdp_parse_attr_simple_bool, sdp_build_attr_simple_bool },
    {"T38FaxRateManagement", sizeof("T38FaxRateManagement"),
     sdp_parse_attr_t38_ratemgmt, sdp_build_attr_t38_ratemgmt },
    {"T38FaxMaxBuffer", sizeof("T38FaxMaxBuffer"),
     sdp_parse_attr_simple_u32, sdp_build_attr_simple_u32 },
    {"T38FaxMaxDatagram", sizeof("T38FaxMaxDatagram"),
     sdp_parse_attr_simple_u32, sdp_build_attr_simple_u32 },
    {"T38FaxUdpEC", sizeof("T38FaxUdpEC"),
     sdp_parse_attr_t38_udpec, sdp_build_attr_t38_udpec },
    {"X-cap", sizeof("X-cap"),
     sdp_parse_attr_cap, sdp_build_attr_cap },
    {"X-cpar", sizeof("X-cpar"),
     sdp_parse_attr_cpar, sdp_build_attr_cpar },
    {"X-pc-codec", sizeof("X-pc-codec"),
     sdp_parse_attr_pc_codec, sdp_build_attr_pc_codec },
    {"X-pc-qos", sizeof("X-pc-qos"),
     sdp_parse_attr_qos, sdp_build_attr_qos },
    {"X-qos", sizeof("X-qos"),
     sdp_parse_attr_qos, sdp_build_attr_qos },
    {"X-sqn", sizeof("X-sqn"),
     sdp_parse_attr_simple_u32, sdp_build_attr_simple_u32 },
    {"TMRGwXid", sizeof("TMRGwXid"),
     sdp_parse_attr_simple_bool, sdp_build_attr_simple_bool },
    {"TC1PayloadBytes", sizeof("TC1PayloadBytes"),
     sdp_parse_attr_simple_u32, sdp_build_attr_simple_u32 },
    {"TC1WindowSize", sizeof("TC1WindowSize"),
     sdp_parse_attr_simple_u32, sdp_build_attr_simple_u32 },
    {"TC2PayloadBytes", sizeof("TC2PayloadBytes"),
     sdp_parse_attr_simple_u32, sdp_build_attr_simple_u32 },
    {"TC2WindowSize", sizeof("TC2WindowSize"),
     sdp_parse_attr_simple_u32, sdp_build_attr_simple_u32 },
    {"rtcp", sizeof("rtcp"),
     sdp_parse_attr_simple_u32, sdp_build_attr_simple_u32 },
    {"rtr", sizeof("rtr"),
     sdp_parse_attr_rtr, sdp_build_attr_rtr},
    {"silenceSupp", sizeof("silenceSupp"),
     sdp_parse_attr_silencesupp, sdp_build_attr_silencesupp },
    {"X-crypto", sizeof("X-crypto"),
     sdp_parse_attr_srtpcontext, sdp_build_attr_srtpcontext },
    {"mptime", sizeof("mptime"),
      sdp_parse_attr_mptime, sdp_build_attr_mptime },
    {"X-sidin", sizeof("X-sidin"),
      sdp_parse_attr_x_sidin, sdp_build_attr_x_sidin },
    {"X-sidout", sizeof("X-sidout"),
      sdp_parse_attr_x_sidout, sdp_build_attr_x_sidout },
    {"X-confid", sizeof("X-confid"),
      sdp_parse_attr_x_confid, sdp_build_attr_x_confid },
    {"group", sizeof("group"),
      sdp_parse_attr_group, sdp_build_attr_group },
    {"mid", sizeof("mid"),
      sdp_parse_attr_simple_u32, sdp_build_attr_simple_u32 },
    {"source-filter", sizeof("source-filter"),
      sdp_parse_attr_source_filter, sdp_build_source_filter},
    {"rtcp-unicast", sizeof("rtcp-unicast"),
      sdp_parse_attr_rtcp_unicast, sdp_build_attr_rtcp_unicast},
    {"maxprate", sizeof("maxprate"),
      sdp_parse_attr_maxprate, sdp_build_attr_simple_string},
    {"sqn", sizeof("sqn"),
     sdp_parse_attr_simple_u32, sdp_build_attr_simple_u32 },
    {"cdsc", sizeof("cdsc"),
     sdp_parse_attr_cap, sdp_build_attr_cap },
    {"cpar", sizeof("cpar"),
     sdp_parse_attr_cpar, sdp_build_attr_cpar },
    {"sprtmap", sizeof("sprtmap"),
     sdp_parse_attr_transport_map, sdp_build_attr_transport_map },
    {"crypto", sizeof("crypto"),
     sdp_parse_attr_sdescriptions, sdp_build_attr_sdescriptions },
    {"label", sizeof("label"),
      sdp_parse_attr_simple_string, sdp_build_attr_simple_string },
    {"framerate", sizeof("framerate"),
      sdp_parse_attr_simple_u32, sdp_build_attr_simple_u32 },
    {"candidate", sizeof("candidate"),
      sdp_parse_attr_ice_attr, sdp_build_attr_ice_attr },
    {"ice-ufrag", sizeof("ice-ufrag"),
      sdp_parse_attr_ice_attr, sdp_build_attr_ice_attr },
    {"ice-pwd", sizeof("ice-pwd"),
      sdp_parse_attr_ice_attr, sdp_build_attr_ice_attr},
    {"rtcp-mux", sizeof("rtcp-mux"),
      sdp_parse_attr_rtcp_mux_attr, sdp_build_attr_rtcp_mux_attr},
    {"fingerprint", sizeof("fingerprint"),
      sdp_parse_attr_fingerprint_attr, sdp_build_attr_simple_string},
    {"maxptime", sizeof("maxptime"),
      sdp_parse_attr_simple_u32, sdp_build_attr_simple_u32},
    {"rtcp-fb", sizeof("rtcp-fb"),
      sdp_parse_attr_rtcp_fb, sdp_build_attr_rtcp_fb}
};

/* Note: These *must* be in the same order as the enum types. */
const sdp_namearray_t sdp_media[SDP_MAX_MEDIA_TYPES] =
{
    {"audio",        sizeof("audio")},
    {"video",        sizeof("video")},
    {"application",  sizeof("application")},
    {"data",         sizeof("data")},
    {"control",      sizeof("control")},
    {"nas/radius",   sizeof("nas/radius")},
    {"nas/tacacs",   sizeof("nas/tacacs")},
    {"nas/diameter", sizeof("nas/diameter")},
    {"nas/l2tp",     sizeof("nas/l2tp")},
    {"nas/login",    sizeof("nas/login")},
    {"nas/none",     sizeof("nas/none")},
    {"image",        sizeof("image")},
    {"text",         sizeof("text")}
};


/* Note: These *must* be in the same order as the enum types. */
const sdp_namearray_t sdp_nettype[SDP_MAX_NETWORK_TYPES] =
{
    {"IN",           sizeof("IN")},
    {"ATM",          sizeof("ATM")},
    {"FR",           sizeof("FR")},
    {"LOCAL",        sizeof("LOCAL")}
};


/* Note: These *must* be in the same order as the enum types. */
const sdp_namearray_t sdp_addrtype[SDP_MAX_ADDR_TYPES] =
{
    {"IP4",          sizeof("IP4")},
    {"IP6",          sizeof("IP6")},
    {"NSAP",         sizeof("NSAP")},
    {"EPN",          sizeof("EPN")},
    {"E164",         sizeof("E164")},
    {"GWID",         sizeof("GWID")}
};


/* Note: These *must* be in the same order as the enum type. */
const sdp_namearray_t sdp_transport[SDP_MAX_TRANSPORT_TYPES] =
{
    {"RTP/AVP",      sizeof("RTP/AVP")},
    {"udp",          sizeof("udp")},
    {"udptl",        sizeof("udptl")},
    {"ces10",        sizeof("ces10")},
    {"LOCAL",        sizeof("LOCAL")},
    {"AAL2/ITU",     sizeof("AAL2/ITU")},
    {"AAL2/ATMF",    sizeof("AAL2/ATMF")},
    {"AAL2/custom",  sizeof("AAL2/custom")},
    {"AAL1/AVP",     sizeof("AAL1/AVP")},
    {"udpsprt",      sizeof("udpsprt")},
    {"RTP/SAVP",     sizeof("RTP/SAVP")},
    {"tcp",          sizeof("tcp")},
    {"RTP/SAVPF",    sizeof("RTP/SAVPF")},
    {"DTLS/SCTP",    sizeof("DTLS/SCTP")}
};

/* Note: These *must* be in the same order as the enum type. */
const sdp_namearray_t sdp_encrypt[SDP_MAX_ENCRYPT_TYPES] =
{
    {"clear",        sizeof("clear")},
    {"base64",       sizeof("base64")},
    {"uri",          sizeof("uri")},
    {"prompt",       sizeof("prompt")}
};

/* Note: These *must* be in the same order as the enum type. */
const sdp_namearray_t sdp_payload[SDP_MAX_STRING_PAYLOAD_TYPES] =
{
    {"t38",          sizeof("t38")},
    {"X-tmr",        sizeof("X-tmr")},
    {"T120",         sizeof("T120")}
};

/* Note: These *must* be in the same order as the enum type. */
const sdp_namearray_t sdp_t38_rate[SDP_T38_MAX_RATES] =
{
    {"localTCF",        sizeof("localTCF")},
    {"transferredTCF",  sizeof("transferredTCF")},
    {"unknown",         sizeof("unknown")}
};

/* Note: These *must* be in the same order as the enum type. */
const sdp_namearray_t sdp_t38_udpec[SDP_T38_MAX_UDPEC] =
{
    {"t38UDPRedundancy",  sizeof("t38UDPRedundancy")},
    {"t38UDPFEC",         sizeof("t38UDPFEC")},
    {"unknown",           sizeof("unknown")}
};

/* Note: These *must* be in the same order as the enum type. */
const sdp_namearray_t sdp_qos_strength[SDP_MAX_QOS_STRENGTH] =
{
    {"optional",          sizeof("optional")},
    {"mandatory",         sizeof("mandatory")},
    {"success",           sizeof("success")},
    {"failure",           sizeof("failure")},
    {"none",              sizeof("none")}
};

/* Note: These *must* be in the same order as the enum type. */
const sdp_namearray_t sdp_qos_status_type[SDP_MAX_QOS_STATUS_TYPES] =
{
    {"local",          sizeof("local")},
    {"remote",         sizeof("remote")},
    {"e2e",            sizeof("e2e")}
};

/* Note: These *must* be in the same order as the enum type. */
const sdp_namearray_t sdp_curr_type[SDP_MAX_CURR_TYPES] =
{
    {"qos",            sizeof("qos")},
    {"unknown",         sizeof("unknown")}
};

/* Note: These *must* be in the same order as the enum type. */
const sdp_namearray_t sdp_des_type[SDP_MAX_DES_TYPES] =
{
    {"qos",            sizeof("qos")},
    {"unknown",         sizeof("unknown")}
};

/* Note: These *must* be in the same order as the enum type. */
const sdp_namearray_t sdp_conf_type[SDP_MAX_CONF_TYPES] =
{
    {"qos",            sizeof("qos")},
    {"unknown",         sizeof("unknown")}
};
/* Note: These *must* be in the same order as the enum type. */
const sdp_namearray_t sdp_qos_direction[SDP_MAX_QOS_DIR] =
{
    {"send",              sizeof("send")},
    {"recv",              sizeof("recv")},
    {"sendrecv",          sizeof("sendrecv")},
    {"none",              sizeof("none")}
};

/* Note: These *must* be in the same order as the enum type. */
const sdp_namearray_t sdp_silencesupp_pref[SDP_MAX_SILENCESUPP_PREF] = {
    {"standard",          sizeof("standard")},
    {"custom",            sizeof("custom")},
    {"-",                 sizeof("-")}
};

/* Note: These *must* be in the same order as the enum type. */
const sdp_namearray_t sdp_silencesupp_siduse[SDP_MAX_SILENCESUPP_SIDUSE] = {
    {"No SID",            sizeof("No SID")},
    {"Fixed Noise",       sizeof("Fixed Noise")},
    {"Sampled Noise",     sizeof("Sampled Noise")},
    {"-",                 sizeof("-")}
};

/* Note: These *must* be in the same order as the enum type. */
const sdp_namearray_t sdp_mediadir_role[SDP_MAX_MEDIADIR_ROLES] =
{
    {"passive",       sizeof("passive")},
    {"active",        sizeof("active")},
    {"both",          sizeof("both")},
    {"reuse",         sizeof("reuse")},
    {"unknown",       sizeof("unknown")}
};

/* Note: These *must* be in the same order as the enum type. */
const sdp_namearray_t sdp_fmtp_codec_param[SDP_MAX_FMTP_PARAM] =
{
    {"annexa",              sizeof("annexa")}, /* 0 */
    {"annexb",              sizeof("annexb")}, /* 1 */
    {"bitrate",             sizeof("bitrate")}, /* 2 */
    {"QCIF",                sizeof("QCIF")}, /* 3 */
    {"CIF",                 sizeof("CIF")},  /* 4 */
    {"MAXBR",               sizeof("MAXBR")}, /* 5 */
    {"SQCIF",               sizeof("SQCIF")}, /* 6 */
    {"CIF4",                sizeof("CIF4")}, /* 7 */
    {"CIF16",               sizeof("CIF16")}, /* 8 */
    {"CUSTOM",              sizeof("CUSTOM")}, /* 9 */
    {"PAR",                 sizeof("PAR")}, /* 10 */
    {"CPCF",                sizeof("CPCF")}, /* 11 */
    {"BPP",                 sizeof("BPP")}, /* 12 */
    {"HRD",                 sizeof("HRD")}, /* 13 */
    {"PROFILE",             sizeof("PROFILE")}, /* 14 */
    {"LEVEL",               sizeof("LEVEL")}, /* 15 */
    {"INTERLACE",           sizeof("INTERLACE")}, /* 16 */

    /* H.264 related */
    {"profile-level-id",      sizeof("profile-level-id")}, /* 17 */
    {"sprop-parameter-sets",  sizeof("sprop-parameter-sets")}, /* 18 */
    {"packetization-mode",    sizeof("packetization-mode")}, /* 19 */
    {"sprop-interleaving-depth",    sizeof("sprop-interleaving-depth")}, /* 20 */
    {"sprop-deint-buf-req",   sizeof("sprop-deint-buf-req")}, /* 21 */
    {"sprop-max-don-diff",    sizeof("sprop-max-don-diff")}, /* 22 */
    {"sprop-init-buf-time",   sizeof("sprop-init-buf-time")}, /* 23 */

    {"max-mbps",              sizeof("max-mbps")}, /* 24 */
    {"max-fs",                sizeof("max-fs")}, /* 25 */
    {"max-cpb",               sizeof("max-cpb")}, /* 26 */
    {"max-dpb",               sizeof("max-dpb")}, /* 27 */
    {"max-br",                sizeof("max-br")}, /* 28 */
    {"redundant-pic-cap",     sizeof("redundant-pic-cap")}, /* 29 */
    {"deint-buf-cap",         sizeof("deint-buf-cap")}, /* 30 */
    {"max-rcmd-nalu-size",    sizeof("max-rcmd_nali-size")}, /* 31 */
    {"parameter-add",         sizeof("parameter-add")}, /* 32 */

    /* Annexes - require special handling */
     {"D", sizeof("D")}, /* 33 */
     {"F", sizeof("F")}, /* 34 */
     {"I", sizeof("I")}, /* 35 */
     {"J", sizeof("J")}, /* 36 */
     {"T", sizeof("T")}, /* 37 */
     {"K", sizeof("K")}, /* 38 */
     {"N", sizeof("N")}, /* 39 */
     {"P", sizeof("P")}, /* 40 */

     {"mode",                sizeof("mode")},  /* 41 */
    {"level-asymmetry-allowed",         sizeof("level-asymmetry-allowed")}, /* 42 */
    {"maxaveragebitrate",               sizeof("maxaveragebitrate")}, /* 43 */
    {"usedtx",                          sizeof("usedtx")}, /* 44 */
    {"stereo",                          sizeof("stereo")}, /* 45 */
    {"useinbandfec",                    sizeof("useinbandfec")}, /* 46 */
    {"maxcodedaudiobandwidth",          sizeof("maxcodedaudiobandwidth")}, /* 47 */
    {"cbr",                             sizeof("cbr")}, /* 48 */
    {"streams",                         sizeof("streams")}, /* 49 */
    {"protocol",                        sizeof("protocol")} /* 50 */

} ;

/* Note: These *must* be in the same order as the enum type. */
const sdp_namearray_t sdp_fmtp_codec_param_val[SDP_MAX_FMTP_PARAM_VAL] =
{
    {"yes",                 sizeof("yes")},
    {"no",                  sizeof("no")}
};

const sdp_namearray_t sdp_bw_modifier_val[SDP_MAX_BW_MODIFIER_VAL] =
{
    {"AS",                  sizeof("AS")},
    {"CT",                  sizeof("CT")},
    {"TIAS",                sizeof("TIAS")}
};

const sdp_namearray_t sdp_group_attr_val[SDP_MAX_GROUP_ATTR_VAL] =
{
    {"FID",                 sizeof("FID")},
    {"LS",                  sizeof("LS")},
    {"ANAT",                sizeof("ANAT")}
};

const sdp_namearray_t sdp_srtp_context_crypto_suite[SDP_SRTP_MAX_NUM_CRYPTO_SUITES] =
{
  {"UNKNOWN_CRYPTO_SUITE",    sizeof("UNKNOWN_CRYPTO_SUITE")},
  {"AES_CM_128_HMAC_SHA1_32", sizeof("AES_CM_128_HMAC_SHA1_32")},
  {"AES_CM_128_HMAC_SHA1_80", sizeof("AES_CM_128_HMAC_SHA1_80")},
  {"F8_128_HMAC_SHA1_80", sizeof("F8_128_HMAC_SHA1_80")}
};

/* Maintain the same order as defined in typedef sdp_src_filter_mode_e */
const sdp_namearray_t sdp_src_filter_mode_val[SDP_MAX_FILTER_MODE] =
{
    {"incl", sizeof("incl")},
    {"excl", sizeof("excl")}
};

/* Maintain the same order as defined in typdef sdp_rtcp_unicast_mode_e */
const sdp_namearray_t sdp_rtcp_unicast_mode_val[SDP_RTCP_MAX_UNICAST_MODE] =
{
    {"reflection", sizeof("reflection")},
    {"rsi",        sizeof("rsi")}
};

#define SDP_NAME(x) {x, sizeof(x)}
/* Maintain the same order as defined in typdef sdp_rtcp_fb_type_e */
const sdp_namearray_t sdp_rtcp_fb_type_val[SDP_MAX_RTCP_FB] =
{
    SDP_NAME("ack"),
    SDP_NAME("ccm"),
    SDP_NAME("nack"),
    SDP_NAME("trr-int")
};

/* Maintain the same order as defined in typdef sdp_rtcp_fb_nack_type_e */
const sdp_namearray_t sdp_rtcp_fb_nack_type_val[SDP_MAX_RTCP_FB_NACK] =
{
    SDP_NAME(""),
    SDP_NAME("sli"),
    SDP_NAME("pli"),
    SDP_NAME("rpsi"),
    SDP_NAME("app"),
    SDP_NAME("rai"),
    SDP_NAME("tllei"),
    SDP_NAME("pslei"),
    SDP_NAME("ecn")
};

/* Maintain the same order as defined in typdef sdp_rtcp_fb_ack_type_e */
const sdp_namearray_t sdp_rtcp_fb_ack_type_val[SDP_MAX_RTCP_FB_ACK] =
{
    SDP_NAME("rpsi"),
    SDP_NAME("app")
};

/* Maintain the same order as defined in typdef sdp_rtcp_fb_ccm_type_e */
const sdp_namearray_t sdp_rtcp_fb_ccm_type_val[SDP_MAX_RTCP_FB_CCM] =
{
    SDP_NAME("fir"),
    SDP_NAME("tmmbr"),
    SDP_NAME("tstr"),
    SDP_NAME("vbcm")
};


/*  Maintain same order as defined in typedef sdp_srtp_crypto_suite_t */
const sdp_srtp_crypto_suite_list sdp_srtp_crypto_suite_array[SDP_SRTP_MAX_NUM_CRYPTO_SUITES] =
{
  {SDP_SRTP_UNKNOWN_CRYPTO_SUITE, UNKNOWN_CRYPTO_SUITE, 0, 0},
  {SDP_SRTP_AES_CM_128_HMAC_SHA1_32, AES_CM_128_HMAC_SHA1_32,
      SDP_SRTP_AES_CM_128_HMAC_SHA1_32_KEY_BYTES,
      SDP_SRTP_AES_CM_128_HMAC_SHA1_32_SALT_BYTES},
  {SDP_SRTP_AES_CM_128_HMAC_SHA1_80, AES_CM_128_HMAC_SHA1_80,
      SDP_SRTP_AES_CM_128_HMAC_SHA1_80_KEY_BYTES,
      SDP_SRTP_AES_CM_128_HMAC_SHA1_80_SALT_BYTES},
  {SDP_SRTP_F8_128_HMAC_SHA1_80, F8_128_HMAC_SHA1_80,
      SDP_SRTP_F8_128_HMAC_SHA1_80_KEY_BYTES,
      SDP_SRTP_F8_128_HMAC_SHA1_80_SALT_BYTES}
};

const char* sdp_result_name[SDP_MAX_RC] =
    {"SDP_SUCCESS",
     "SDP_FAILURE",
     "SDP_INVALID_SDP_PTR",
     "SDP_NOT_SDP_DESCRIPTION",
     "SDP_INVALID_TOKEN_ORDERING",
     "SDP_INVALID_PARAMETER",
     "SDP_INVALID_MEDIA_LEVEL",
     "SDP_INVALID_CAPABILITY",
     "SDP_NO_RESOURCE",
     "SDP_UNRECOGNIZED_TOKEN",
     "SDP_NULL_BUF_PTR",
     "SDP_POTENTIAL_SDP_OVERFLOW"};

const char *sdp_get_result_name ( sdp_result_e rc )
{
    if (rc >= SDP_MAX_RC) {
        return ("Invalid SDP result code");
    } else {
        return (sdp_result_name[rc]);
    }
}

const char *sdp_get_attr_name ( sdp_attr_e attr_type )
{
    if (attr_type >= SDP_MAX_ATTR_TYPES) {
        return ("Invalid attribute type");
    } else {
        return (sdp_attr[attr_type].name);
    }
}

const char *sdp_get_media_name ( sdp_media_e media_type )
{
    if (media_type == SDP_MEDIA_UNSUPPORTED) {
        return (SDP_UNSUPPORTED);
    } else if (media_type >= SDP_MAX_MEDIA_TYPES) {
        return ("Invalid media type");
    } else {
        return (sdp_media[media_type].name);
    }
}

const char *sdp_get_network_name ( sdp_nettype_e network_type )
{
    if (network_type == SDP_NT_UNSUPPORTED) {
        return (SDP_UNSUPPORTED);
    } else if (network_type >= SDP_MAX_NETWORK_TYPES) {
        return ("Invalid network type");
    } else {
        return (sdp_nettype[network_type].name);
    }
}

const char *sdp_get_address_name ( sdp_addrtype_e addr_type )
{
    if (addr_type == SDP_AT_UNSUPPORTED) {
        return (SDP_UNSUPPORTED);
    } else if (addr_type >= SDP_MAX_ADDR_TYPES) {
        if (addr_type == SDP_AT_FQDN) {
            return ("*");
        } else {
            return ("Invalid address type");
        }
    } else {
        return (sdp_addrtype[addr_type].name);
    }
}

const char *sdp_get_transport_name ( sdp_transport_e transport_type )
{
    if (transport_type == SDP_TRANSPORT_UNSUPPORTED) {
        return (SDP_UNSUPPORTED);
    } else if (transport_type >= SDP_MAX_TRANSPORT_TYPES) {
        return ("Invalid transport type");
    } else {
        return (sdp_transport[transport_type].name);
    }
}

const char *sdp_get_encrypt_name ( sdp_encrypt_type_e encrypt_type )
{
    if (encrypt_type == SDP_ENCRYPT_UNSUPPORTED) {
        return (SDP_UNSUPPORTED);
    } else if (encrypt_type >= SDP_MAX_ENCRYPT_TYPES) {
        return ("Invalid encryption type");
    } else {
        return (sdp_encrypt[encrypt_type].name);
    }
}

const char *sdp_get_payload_name ( sdp_payload_e payload )
{
    if (payload == SDP_PAYLOAD_UNSUPPORTED) {
        return (SDP_UNSUPPORTED);
    } else if (payload >= SDP_MAX_STRING_PAYLOAD_TYPES) {
        return ("Invalid payload type");
    } else {
        return (sdp_payload[payload].name);
    }
}

const char *sdp_get_t38_ratemgmt_name ( sdp_t38_ratemgmt_e rate )
{
    if (rate >= SDP_T38_MAX_RATES) {
        return ("Invalid rate");
    } else {
        return (sdp_t38_rate[rate].name);
    }
}

const char *sdp_get_t38_udpec_name ( sdp_t38_udpec_e udpec )
{
    if (udpec >= SDP_T38_MAX_UDPEC) {
        return ("Invalid udpec");
    } else {
        return (sdp_t38_udpec[udpec].name);
    }
}

const char *sdp_get_qos_strength_name ( sdp_qos_strength_e strength )
{
    if (strength >= SDP_MAX_QOS_STRENGTH) {
        return ("Invalid qos strength");
    } else {
        return (sdp_qos_strength[strength].name);
    }
}

const char *sdp_get_qos_direction_name ( sdp_qos_dir_e direction )
{
    if (direction >= SDP_MAX_QOS_DIR) {
        return ("Invalid qos direction");
    } else {
        return (sdp_qos_direction[direction].name);
    }
}

const char *sdp_get_qos_status_type_name ( sdp_qos_status_types_e status_type )
{
    if (status_type >= SDP_MAX_QOS_STATUS_TYPES) {
        return ("Invalid qos status type");
    } else {
        return (sdp_qos_status_type[status_type].name);
    }
}

const char *sdp_get_curr_type_name (sdp_curr_type_e curr_type )
{
    if (curr_type >= SDP_MAX_CURR_TYPES) {
        return ("Invalid curr type");
    } else {
        return (sdp_curr_type[curr_type].name);
    }
}

const char *sdp_get_des_type_name (sdp_des_type_e des_type )
{
    if (des_type >= SDP_MAX_DES_TYPES) {
        return ("Invalid des type");
    } else {
        return (sdp_des_type[des_type].name);
    }
}

const char *sdp_get_conf_type_name (sdp_conf_type_e conf_type )
{
    if (conf_type >= SDP_MAX_CONF_TYPES) {
        return ("Invalid conf type");
    } else {
        return (sdp_conf_type[conf_type].name);
    }
}

const char *sdp_get_silencesupp_pref_name (sdp_silencesupp_pref_e pref)
{
    if (pref >= SDP_MAX_SILENCESUPP_PREF) {
        return ("Invalid silencesupp pref");
    } else {
        return (sdp_silencesupp_pref[pref].name);
    }
}

const char *sdp_get_silencesupp_siduse_name (sdp_silencesupp_siduse_e siduse)
{
    if (siduse >= SDP_MAX_SILENCESUPP_SIDUSE) {
        return ("Invalid silencesupp siduse");
    } else {
        return (sdp_silencesupp_siduse[siduse].name);
    }
}

const char *sdp_get_mediadir_role_name (sdp_mediadir_role_e role)
{
    if (role >= SDP_MEDIADIR_ROLE_UNKNOWN) {
        return ("Invalid media direction role");
    } else {
        return (sdp_mediadir_role[role].name);
    }
}


const char *sdp_get_bw_modifier_name (sdp_bw_modifier_e bw_modifier_type)
{
    if (bw_modifier_type == SDP_BW_MODIFIER_UNSUPPORTED) {
        return (SDP_UNSUPPORTED);
    } else if (bw_modifier_type < SDP_BW_MODIFIER_AS ||
            bw_modifier_type >= SDP_MAX_BW_MODIFIER_VAL) {
        return ("Invalid bw modifier type");
    } else {
        return (sdp_bw_modifier_val[bw_modifier_type].name);
    }
}

const char *sdp_get_group_attr_name (sdp_group_attr_e group_attr_type)
{
    if (group_attr_type == SDP_GROUP_ATTR_UNSUPPORTED) {
        return (SDP_UNSUPPORTED);
    } else if (group_attr_type >= SDP_MAX_GROUP_ATTR_VAL) {
        return ("Invalid a=group: attribute type");
    } else {
        return (sdp_group_attr_val[group_attr_type].name);
    }
}

const char *sdp_get_src_filter_mode_name (sdp_src_filter_mode_e type)
{
    if (type >= SDP_MAX_FILTER_MODE) {
        return ("Invalid source filter mode");
    } else {
        return (sdp_src_filter_mode_val[type].name);
    }
}

const char *sdp_get_rtcp_unicast_mode_name (sdp_rtcp_unicast_mode_e type)
{
    if (type >= SDP_RTCP_MAX_UNICAST_MODE) {
        return ("Invalid rtcp unicast mode");
    } else {
        return (sdp_rtcp_unicast_mode_val[type].name);
    }
}

/* Function:    sdp_verify_sdp_ptr
 * Description: Verify the SDP pointer is valid by checking for
 *              the SDP magic number.  If not valid, display an error.
 *              Note that we can't keep a statistic of this because we
 *              track stats inside the config structure which is stored
 *              in the SDP structure.
 * Parameters:	sdp_p    The SDP structure handle.
 * Returns:	TRUE or FALSE.
 */
inline tinybool sdp_verify_sdp_ptr (sdp_t *sdp_p)
{
    if ((sdp_p != NULL) && (sdp_p->magic_num == SDP_MAGIC_NUM)) {
        return (TRUE);
    } else {
        CSFLogError(logTag, "SDP: Invalid SDP pointer.");
        return (FALSE);
    }
}

/* Function:    sdp_init_description
 * Description:	Allocates a new SDP structure that can be used for either
 *              parsing or building an SDP description.  This routine
 *              saves the config pointer passed in the SDP structure so
 *              SDP will know how to parse/build based on the options defined.
 *              An SDP structure must be allocated before parsing or building
 *              since the handle must be passed to these routines.
 * Parameters:  config_p     The config handle returned by sdp_init_config
 * Returns:     A handle for a new SDP structure as a void ptr.
*/
sdp_t *sdp_init_description (const char *peerconnection, void *config_p)
{
    int i;
    sdp_t *sdp_p;
    sdp_conf_options_t *conf_p = (sdp_conf_options_t *)config_p;

    if (sdp_verify_conf_ptr(conf_p) == FALSE) {
        return (NULL);
    }

    sdp_p = (sdp_t *)SDP_MALLOC(sizeof(sdp_t));
    if (sdp_p == NULL) {
	return (NULL);
    }

    sstrncpy(sdp_p->peerconnection, peerconnection, sizeof(sdp_p->peerconnection));

    /* Initialize magic number. */
    sdp_p->magic_num = SDP_MAGIC_NUM;

    sdp_p->conf_p             = conf_p;
    sdp_p->version            = SDP_CURRENT_VERSION;
    sdp_p->owner_name[0]      = '\0';
    sdp_p->owner_sessid[0]    = '\0';
    sdp_p->owner_version[0]   = '\0';
    sdp_p->owner_network_type = SDP_NT_INVALID;
    sdp_p->owner_addr_type    = SDP_AT_INVALID;
    sdp_p->owner_addr[0]      = '\0';
    sdp_p->sessname[0]        = '\0';
    sdp_p->sessinfo_found     = FALSE;
    sdp_p->uri_found          = FALSE;

    sdp_p->default_conn.nettype      = SDP_NT_INVALID;
    sdp_p->default_conn.addrtype     = SDP_AT_INVALID;
    sdp_p->default_conn.conn_addr[0] = '\0';
    sdp_p->default_conn.is_multicast = FALSE;
    sdp_p->default_conn.ttl          = 0;
    sdp_p->default_conn.num_of_addresses = 0;

    sdp_p->bw.bw_data_count   = 0;
    sdp_p->bw.bw_data_list    = NULL;

    sdp_p->timespec_p         = NULL;
    sdp_p->sess_attrs_p       = NULL;
    sdp_p->mca_p              = NULL;
    sdp_p->mca_count          = 0;

    /* Set default debug flags from application config. */
    for (i=0; i < SDP_MAX_DEBUG_TYPES; i++) {
        sdp_p->debug_flag[i] = conf_p->debug_flag[i];
    }

    return (sdp_p);
}


/* Function:    void sdp_debug(sdp_t *sdp_p, sdp_debug_e debug_type,
 *                             tinybool my_bool);
 * Description:	Define the type of debug for this particular SDP structure.
 *              By default, each SDP description has the settings that are
 *              set for the application.
 *              Valid debug types are ERRORS, WARNINGS, and TRACE.  Each
 *              debug type can be turned on/off individually.  The
 *              debug level can be redefined at any time.
 * Parameters:	sdp_ptr    The SDP handle returned by sdp_init_description.
 *              debug_type Specifies the debug type being enabled/disabled.
 *              my_bool    Defines whether the debug should be enabled or not.
 * Returns:	Nothing.
 */
void sdp_debug (sdp_t *sdp_p, sdp_debug_e debug_type, tinybool debug_flag)
{
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return;
    }

    if (debug_type < SDP_MAX_DEBUG_TYPES)  {
        sdp_p->debug_flag[debug_type] = debug_flag;
    }
}


/* Function:    void sdp_set_string_debug(sdp_t *sdp_p, char *debug_str)
 * Description: Define a string to be associated with all debug output
 *              for this SDP. The string will be copied into the SDP
 *              structure and so the library will not be dependent on
 *              the application's memory for this string.
 * Parameters:  sdp_p      The SDP handle returned by sdp_init_description.
 *              debug_str  Pointer to a string that should be printed out
 *                         with every debug msg.
 * Returns:     Nothing.
 */
void sdp_set_string_debug (sdp_t *sdp_p, const char *debug_str)
{
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return;
    }

    sstrncpy(sdp_p->debug_str, debug_str, sizeof(sdp_p->debug_str));
}


/* Function:    sdp_validate_sdp
 * Description: Validate an SDP structure.
 * Parameters:  sdp_p    The SDP handle of the struct to validate.
 * Returns:     A result value indicating if the validation was successful.
 *              If not, what type of error was encountered.
 */
sdp_result_e sdp_validate_sdp (sdp_t *sdp_p)
{
    int i;
    u16 num_media_levels;

    /* Need to validate c= info is specified at session level or
     * at all m= levels.
     */
    if (sdp_connection_valid((void *)sdp_p, SDP_SESSION_LEVEL) == FALSE) {
        num_media_levels = sdp_get_num_media_lines((void *)sdp_p);
        for (i=1; i <= num_media_levels; i++) {
            if (sdp_connection_valid((void *)sdp_p, (unsigned short)i) == FALSE) {
                sdp_parse_error(sdp_p->peerconnection,
                    "%s c= connection line not specified for "
                    "every media level, validation failed.",
                    sdp_p->debug_str);
                return (SDP_FAILURE);
            }
        }
    }

    /* Validate required lines were specified */
    if ((sdp_owner_valid((void *)sdp_p) == FALSE) &&
        (sdp_p->conf_p->owner_reqd == TRUE)) {
        sdp_parse_error(sdp_p->peerconnection,
            "%s o= owner line not specified, validation failed.",
            sdp_p->debug_str);
        return (SDP_FAILURE);
    }

    if ((sdp_session_name_valid((void *)sdp_p) == FALSE) &&
        (sdp_p->conf_p->session_name_reqd == TRUE)) {
        sdp_parse_error(sdp_p->peerconnection,
            "%s s= session name line not specified, validation failed.",
            sdp_p->debug_str);
        return (SDP_FAILURE);
    }

    if ((sdp_timespec_valid((void *)sdp_p) == FALSE) &&
        (sdp_p->conf_p->timespec_reqd == TRUE)) {
        sdp_parse_error(sdp_p->peerconnection,
            "%s t= timespec line not specified, validation failed.",
            sdp_p->debug_str);
        return (SDP_FAILURE);
    }

    return (SDP_SUCCESS);
}

/* Function:    sdp_parse
 * Description: Parse an SDP description in the specified buffer.
 * Parameters:  sdp_p    The SDP handle returned by sdp_init_description
 *              bufp     Pointer to the buffer containing the SDP
 *                       description to parse.
 *              len      The length of the buffer.
 * Returns:     A result value indicating if the parse was successful and
 *              if not, what type of error was encountered.  The
 *              information from the parse is stored in the sdp_p structure.
 */
sdp_result_e sdp_parse (sdp_t *sdp_p, char **bufp, u16 len)
{
    u8           i;
    u16          cur_level = SDP_SESSION_LEVEL;
    char        *ptr;
    char        *next_ptr = NULL;
    char        *line_end;
    sdp_token_e  last_token = SDP_TOKEN_V;
    sdp_result_e result=SDP_SUCCESS;
    tinybool     parse_done = FALSE;
    tinybool     end_found = FALSE;
    tinybool     first_line = TRUE;
    tinybool     unrec_token = FALSE;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if ((bufp == NULL) || (*bufp == NULL)) {
        return (SDP_NULL_BUF_PTR);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Trace SDP Parse:", sdp_p->debug_str);
    }

    next_ptr = *bufp;
    sdp_p->conf_p->num_parses++;

    /* Initialize the last valid capability instance to zero.  Used
     * to help in parsing X-cpar attrs. */
    sdp_p->cap_valid = FALSE;
    sdp_p->last_cap_inst = 0;

    /* We want to try to find the end of the SDP description, even if
     * we find a parsing error.
     */
    while (!end_found) {
	/* If the last char of this line goes beyond the end of the buffer,
	 * we don't parse it.
	 */
        ptr = next_ptr;
        line_end = sdp_findchar(ptr, "\n");
        if (line_end >= (*bufp + len)) {
            sdp_parse_error(sdp_p->peerconnection,
                "%s End of line beyond end of buffer.",
                sdp_p->debug_str);
            end_found = TRUE;
            break;
        }

        /* Print the line if we're tracing. */
        if ((parse_done == FALSE) &&
	  (sdp_p->debug_flag[SDP_DEBUG_TRACE])) {
	    SDP_PRINT("%s ", sdp_p->debug_str);

            SDP_PRINT("%*s", (int)(line_end - ptr), ptr);

        }

        /* Find out which token this line has, if any. */
        for (i=0; i < SDP_MAX_TOKENS; i++) {
            if (strncmp(ptr, sdp_token[i].name, SDP_TOKEN_LEN) == 0) {
                break;
            }
        }
        if (i == SDP_MAX_TOKENS) {
            /* See if the second char on the next line is an '=' char.
             * If so, we note this as an unrecognized token line. */
            if (ptr[1] == '=') {
                unrec_token = TRUE;
            }
            if (first_line == TRUE) {
                sdp_parse_error(sdp_p->peerconnection,
                    "%s Attempt to parse text not recognized as "
                    "SDP text, parse fails.", sdp_p->debug_str);
                    /* If we haven't already printed out the line we
                     * were trying to parse, do it now.
                     */
                if (!sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
                    SDP_PRINT("%s ", sdp_p->debug_str);
                    SDP_PRINT("%*s", (int)(line_end - ptr), ptr);
                }
                sdp_p->conf_p->num_not_sdp_desc++;
                return (SDP_NOT_SDP_DESCRIPTION);
            } else {
                end_found = TRUE;
                break;
            }
        }

        /* This is the beginning of a new SDP description. */
        if ((first_line != TRUE) && (i == SDP_TOKEN_V)) {
            end_found = TRUE;
            break;
        }

        /* Advance the next ptr to one char beyond the end of the line. */
        next_ptr = line_end + 1;
        if (next_ptr >= (*bufp + len)) {
            end_found = TRUE;
        }

        /* If we've finished parsing and are just looking for the end of
         * the SDP description, we don't need to do anything else here.
         */
        if (parse_done == TRUE) {
            continue;
        }

        /* Only certain tokens are valid at the media level. */
        if (cur_level != SDP_SESSION_LEVEL) {
            if ((i != SDP_TOKEN_I) && (i != SDP_TOKEN_C) &&
                (i != SDP_TOKEN_B) && (i != SDP_TOKEN_K) &&
                (i != SDP_TOKEN_A) && (i != SDP_TOKEN_M)) {
                sdp_p->conf_p->num_invalid_token_order++;
                sdp_parse_error(sdp_p->peerconnection,
                    "%s Warning: Invalid token %s found at media level",
                    sdp_p->debug_str, sdp_token[i].name);
                continue;
            }
        }

        /* Verify the token ordering. */
        if (first_line == TRUE) {
            if (i != SDP_TOKEN_V) {
                if (sdp_p->conf_p->version_reqd == TRUE) {
                    sdp_parse_error(sdp_p->peerconnection,
                        "%s First line not v=, parse fails",
                        sdp_p->debug_str);
                    sdp_p->conf_p->num_invalid_token_order++;
                    result = SDP_INVALID_TOKEN_ORDERING;
                    parse_done = TRUE;
                } else {
                    last_token = (sdp_token_e)i;
                }
            } else {
                last_token = (sdp_token_e)i;
            }
            first_line = FALSE;
        } else {
            if (i < last_token) {
                sdp_p->conf_p->num_invalid_token_order++;
                sdp_parse_error(sdp_p->peerconnection,
                    "%s Warning: Invalid token ordering detected, "
                    "token %s found after token %s", sdp_p->debug_str,
                    sdp_token[i].name, sdp_token[last_token].name);
            }
        }

        /* Finally parse the line. */
        ptr += SDP_TOKEN_LEN;
        result = sdp_token[i].parse_func(sdp_p, cur_level, (const char *)ptr);
        last_token = (sdp_token_e)i;
        if (last_token == SDP_TOKEN_M) {
            if (cur_level == SDP_SESSION_LEVEL) {
                cur_level = 1;
            } else {
                cur_level++;
            }
            /* The token ordering can start again at i= */
            last_token = (sdp_token_e)(SDP_TOKEN_I - 1);
        }
        if (result != SDP_SUCCESS) {
            parse_done = TRUE;
        }

        /* Skip the new line char at the end of this line and see if
         * this is the end of the buffer.
         */
        if ((line_end + 1) == (*bufp + len)) {
            end_found = TRUE;
        }
    }

    /* If we found no valid lines, return an error. */
    if (first_line == TRUE) {
        sdp_p->conf_p->num_not_sdp_desc++;
        return (SDP_NOT_SDP_DESCRIPTION);
    }

    /* If no errors were found yet, validate the overall sdp. */
    if (result == SDP_SUCCESS) {
        result = sdp_validate_sdp(sdp_p);
    }
    /* Return the pointer where we left off. */
    *bufp = next_ptr;
    /* If the SDP is valid, but the next line following was an
     * unrecognized <token>= line, indicate this on the return. */
    if ((result == SDP_SUCCESS) && (unrec_token == TRUE)) {
        return (SDP_UNRECOGNIZED_TOKEN);
    } else {
        return (result);
    }
}


/* Function:    sdp_build
 * Description: Build an SDP description in the specified buffer based
 *              on the information in the given SDP structure.
 * Parameters:  sdp_p    The SDP handle returned by sdp_init_description
 *              fs A flex_string where the SDP description should be built.
 * Returns:     A result value indicating if the build was successful and
 *              if not, what type of error was encountered - e.g.,
 *              description was too long for the given buffer.
 */
sdp_result_e sdp_build (sdp_t *sdp_p, flex_string *fs)
{
    int i, j;
    sdp_result_e        result = SDP_SUCCESS;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (!fs) {
        return (SDP_NULL_BUF_PTR);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Trace SDP Build:", sdp_p->debug_str);
    }

    sdp_p->conf_p->num_builds++;

    for (i=0; ((i < SDP_TOKEN_M) &&
               (result == SDP_SUCCESS)); i++) {
        result = sdp_token[i].build_func(sdp_p, SDP_SESSION_LEVEL, fs);
        /* ok not to check buffer space (yet) as the if() checks it */
    }
    /* If the session level was ok, build the media lines. */
    if (result == SDP_SUCCESS) {
        for (i=1; ((i <= sdp_p->mca_count) &&
                   (result == SDP_SUCCESS)); i++) {
            result = sdp_token[SDP_TOKEN_M].build_func(sdp_p, (u16)i, fs);

            /* ok not to check buffer space (yet) as the for() checks it */
            for (j=SDP_TOKEN_I;
                 ((j < SDP_TOKEN_M) && (result == SDP_SUCCESS));
                 j++) {
                if ((j == SDP_TOKEN_U) || (j == SDP_TOKEN_E) ||
                    (j == SDP_TOKEN_P) || (j == SDP_TOKEN_T) ||
                    (j == SDP_TOKEN_R) || (j == SDP_TOKEN_Z)) {
                    /* These tokens not valid at media level. */
                    continue;
                }
                result = sdp_token[j].build_func(sdp_p, (u16)i, fs);
                /* ok not to check buffer space (yet) as the for() checks it */
            }
        }
    }

    return (result);
}

/* Function:    sdp_free_description
 * Description:	Free an SDP description and all memory associated with it.
 * Parameters:  sdp_p  The SDP handle returned by sdp_init_description
 * Returns:     A result value indicating if the free was successful and
 *              if not, what type of error was encountered - e.g., sdp_p
 *              was invalid and didn't point to an SDP structure.
*/
sdp_result_e sdp_free_description (sdp_t *sdp_p)
{
    sdp_timespec_t  *time_p, *next_time_p;
    sdp_attr_t      *attr_p, *next_attr_p;
    sdp_mca_t       *mca_p, *next_mca_p;
    sdp_bw_t        *bw_p;
    sdp_bw_data_t   *bw_data_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    /* Free the config structure */
    if (sdp_p->conf_p) {
        SDP_FREE(sdp_p->conf_p);
    }

    /* Free any timespec structures - should be only one since
     * this is all we currently support.
     */
    time_p = sdp_p->timespec_p;
    while (time_p != NULL) {
	next_time_p = time_p->next_p;
	SDP_FREE(time_p);
	time_p = next_time_p;
    }

    bw_p = &(sdp_p->bw);
    bw_data_p = bw_p->bw_data_list;
    while (bw_data_p != NULL) {
        bw_p->bw_data_list = bw_data_p->next_p;
        SDP_FREE(bw_data_p);
        bw_data_p = bw_p->bw_data_list;
    }

    /* Free any session attr structures */
    attr_p = sdp_p->sess_attrs_p;
    while (attr_p != NULL) {
	next_attr_p = attr_p->next_p;
	sdp_free_attr(attr_p);
	attr_p = next_attr_p;
    }

    /* Free any mca structures */
    mca_p = sdp_p->mca_p;
    while (mca_p != NULL) {
	next_mca_p = mca_p->next_p;

	/* Free any media attr structures */
	attr_p = mca_p->media_attrs_p;
	while (attr_p != NULL) {
	    next_attr_p = attr_p->next_p;
	    sdp_free_attr(attr_p);
	    attr_p = next_attr_p;
	}

        /* Free the media profiles struct if allocated. */
        if (mca_p->media_profiles_p != NULL) {
            SDP_FREE(mca_p->media_profiles_p);
        }

        bw_p = &(mca_p->bw);
        bw_data_p = bw_p->bw_data_list;
        while (bw_data_p != NULL) {
            bw_p->bw_data_list = bw_data_p->next_p;
            SDP_FREE(bw_data_p);
            bw_data_p = bw_p->bw_data_list;
        }

	SDP_FREE(mca_p);
	mca_p = next_mca_p;
    }

    SDP_FREE(sdp_p);

    return (SDP_SUCCESS);
}

/*
 * sdp_parse_error
 * Send SDP parsing errors to log and up to peerconnection
 */
void sdp_parse_error(const char *peerconnection, const char *format, ...) {
    flex_string fs;
    va_list ap;

    flex_string_init(&fs);

    va_start(ap, format);
    flex_string_vsprintf(&fs, format, ap);
    va_end(ap);

    CSFLogError("SDP Parse", "SDP Parse Error %s, pc %s", fs.buffer, peerconnection);
    vcmOnSdpParseError(peerconnection, fs.buffer);

    flex_string_free(&fs);
}
