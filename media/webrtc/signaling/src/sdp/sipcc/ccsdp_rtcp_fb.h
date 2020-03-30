/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __CCSDP_RTCP_FB_H__
#define __CCSDP_RTCP_FB_H__

/* a=rtcp-fb enumerations */

typedef enum {
    SDP_RTCP_FB_ANY = -1,
    SDP_RTCP_FB_ACK = 0,
    SDP_RTCP_FB_CCM,
    SDP_RTCP_FB_NACK,
    SDP_RTCP_FB_TRR_INT,
    // from https://www.ietf.org/archive/id/draft-alvestrand-rmcat-remb-03.txt
    SDP_RTCP_FB_REMB,
    // from https://tools.ietf.org/html/draft-holmer-rmcat-transport-wide-cc-extensions-01
    SDP_RTCP_FB_TRANSPORT_CC,
    SDP_MAX_RTCP_FB,
    SDP_RTCP_FB_UNKNOWN
} sdp_rtcp_fb_type_e;

typedef enum {
    SDP_RTCP_FB_NACK_NOT_FOUND = -1,
    SDP_RTCP_FB_NACK_BASIC = 0,
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

// Codec Control Messages - defined by RFC 5104
typedef enum {
    SDP_RTCP_FB_CCM_NOT_FOUND = -1,
    SDP_RTCP_FB_CCM_FIR = 0,
    SDP_RTCP_FB_CCM_TMMBR,
    SDP_RTCP_FB_CCM_TSTR,
    SDP_RTCP_FB_CCM_VBCM,
    SDP_MAX_RTCP_FB_CCM,
    SDP_RTCP_FB_CCM_UNKNOWN
} sdp_rtcp_fb_ccm_type_e;

#ifdef __cplusplus
static_assert(SDP_MAX_RTCP_FB_NACK +
              SDP_MAX_RTCP_FB_ACK +
              SDP_MAX_RTCP_FB_CCM < 32,
              "rtcp-fb Bitmap is larger than 32 bits");
#endif

#endif
