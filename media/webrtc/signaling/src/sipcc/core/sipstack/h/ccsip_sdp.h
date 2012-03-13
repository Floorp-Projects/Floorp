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

#ifndef _CCSIP_SDP_H_
#define _CCSIP_SDP_H_


#include "cpr_types.h"
#include "pmhutils.h"
#include "sdp.h"
#include "ccapi.h"

// RAMC-start
#define CCSIP_SDP_BUF_SIZE      2048

/* SDP bitmask values */
#define CCSIP_SRC_SDP_BIT       0x1
#define CCSIP_DEST_SDP_BIT      0x2

/*
 * Create a description or a SIP SDP (sip_info) with
 * appropriate values initialized
 */
PMH_EXTERN boolean sip_sdp_init(void);
PMH_EXTERN void *sipsdp_create(void);
PMH_EXTERN cc_sdp_t *sipsdp_info_create(void);
PMH_EXTERN void sipsdp_src_dest_free(uint16_t flags, cc_sdp_t **sdp_info);
PMH_EXTERN void sipsdp_src_dest_create(uint16_t flags, cc_sdp_t **sdp_info);
PMH_EXTERN void sipsdp_free(cc_sdp_t **sip_sdp);

/*
 * Stream related sdp utility functions
 */
//PMH_EXTERN sip_sdp_stream_t *sipsdp_stream_create(void);
//PMH_EXTERN void sipsdp_add_stream_to_list(sip_sdp_stream_t *stream,
//                                     sip_sdp_t *sip_sdp);
//PMH_EXTERN void sipsdp_remove_stream_from_list(sip_sdp_stream_t **stream);
//PMH_EXTERN void sipsdp_remove_streams_list(sip_sdp_stream_t **stream_list);

#define SIPSDP_MAX_SESSION_VERSION_LENGTH 32

/*
 * Create a SDP structure from a packet got from the network. This
 * also parses the message and fills in the values of address, port etc.
 * Memory is allocated. To free it properly, use sipsdp_free_internal(),
 * followed by free()
 * buf = raw message.
 * nbytes = number of bytes in buf.
 *
 */
PMH_EXTERN cc_sdp_t *sipsdp_create_from_buf(char *buf, uint32_t nbytes,
                                            cc_sdp_t *sdp);

/*
 * Standard session-level parameters
 */
#define SIPSDP_VERSION              0
// RAMC_DEBUG #define SIPSDP_ORIGIN_USERNAME      "CiscoSystemsSIP-GW-UserAgent"
#define SIPSDP_ORIGIN_USERNAME      "Cisco-SIPUA"
#define SIPSDP_SESSION_NAME         "SIP Call"

/* Possible encoding names fo static payload types*/
#define SIPSDP_ATTR_ENCNAME_PCMU      "PCMU"
#define SIPSDP_ATTR_ENCNAME_PCMA      "PCMA"
#define SIPSDP_ATTR_ENCNAME_G729      "G729"
#define SIPSDP_ATTR_ENCNAME_G723      "G723"
#define SIPSDP_ATTR_ENCNAME_G726      "G726-32"
#define SIPSDP_ATTR_ENCNAME_G728      "G728"
#define SIPSDP_ATTR_ENCNAME_GSM       "GSM"
#define SIPSDP_ATTR_ENCNAME_CN        "CN"
#define SIPSDP_ATTR_ENCNAME_G722      "G722"
#define SIPSDP_ATTR_ENCNAME_ILBC      "iLBC"
#define SIPSDP_ATTR_ENCNAME_H263v2    "H263-1998"
#define SIPSDP_ATTR_ENCNAME_H264      "H264"
#define SIPSDP_ATTR_ENCNAME_L16_256K  "L16"
#define SIPSDP_ATTR_ENCNAME_ISAC      "ISAC"

/* Possible encoding names for DTMF tones dynamic payload types */
#define SIPSDP_ATTR_ENCNAME_TEL_EVENT "telephone-event"
#define SIPSDP_ATTR_ENCNAME_FRF_DIGIT "frf-dialed-digit"

/* Possible encoding names for other dynamic payload types */
#define SIPSDP_ATTR_ENCNAME_CLEAR_CH  "X-CCD"
#define SIPSDP_ATTR_ENCNAME_G726R16   "G726-16"
#define SIPSDP_ATTR_ENCNAME_G726R24   "G726-24"
#define SIPSDP_ATTR_ENCNAME_GSMEFR    "GSM-EFR"

/* RTPMAP encoding names added from MGCP for compatibility with MGCP
 * These could be coming in from Cisco MGCP gateway's SDP via a
 * softswitch and SIP GW must interoperate.
 */

#define SIPSDP_ATTR_ENCNAME_G729_A_STR_DOTTED                 "G.729a"
#define SIPSDP_ATTR_ENCNAME_G729_B_STR_DOTTED                 "G.729b"
#define SIPSDP_ATTR_ENCNAME_G729_B_LOW_COMPLEXITY_STR_DOTTED  "G.729b-L"
#define SIPSDP_ATTR_ENCNAME_G729_A_B_STR_DOTTED               "G.729ab"

#define SIPSDP_ATTR_ENCNAME_G7231_HIGH_RATE_STR_DOTTED        "G.723.1-H"
#define SIPSDP_ATTR_ENCNAME_G7231_A_HIGH_RATE_STR_DOTTED      "G.723.1a-H"
#define SIPSDP_ATTR_ENCNAME_G7231_LOW_RATE_STR_DOTTED         "G.723.1-L"
#define SIPSDP_ATTR_ENCNAME_G7231_A_LOW_RATE_STR_DOTTED       "G.723.1a-L"

/*
 * NSE/XNSE encoding names
 */
#define SIPSDP_ATTR_ENCNAME_XNSE       "X-NSE"
#define SIPSDP_ATTR_ENCNAME_NSE        "NSE"


/* Possible clock rates */
#define RTPMAP_CLOCKRATE  8000
#define RTPMAP_VIDEO_CLOCKRATE  90000
#define RTPMAP_L16_CLOCKRATE  16000
#define RTPMAP_ISAC_CLOCKRATE  16000


#define SIPSDP_CONTENT_TYPE         "application/sdp"

#define MAX_RTP_PAYLOAD_TYPES        7

#define BITRATE_5300_BPS  5300
#define BITRATE_6300_BPS  6300

/*
 * T.38 attribute parameters
 */
#define SIPSDP_ATTR_T38_VERSION_DEF             0
#define SIPSDP_ATTR_T38_FILL_BIT_REMOVAL_DEF    FALSE
#define SIPSDP_ATTR_T38_TRANSCODING_MMR_DEF     FALSE
#define SIPSDP_ATTR_T38_TRANSCODING_JBIG_DEF    FALSE
/* the following two definitions are from VSIToH245BuildFastStartT38OLC() */
#define SIPSDP_ATTR_T38_MAX_BUFFER_DEF          200
#define SIPSDP_ATTR_T38_MAX_DATAGRAM_DEF        72

/*
 * Supported NTE range. Note that if the supported NTEs ever becomes a
 * non-contiguous set of values, then it will have to be stored as an
 * sdp. See the definition of negotiated_nte_sdp for an example. Further,
 * where Sdp_ne_cmp_range() is invoked, Sdp_ne_cmp_list() will have to be
 * used instead.
 */
#define SIPSDP_NTE_SUPPORTED_LOW      0  /* Min value of DTMF event table */
#define SIPSDP_FRF_SUPPORTED_HIGH     15 /* for dtmf-relay cisco-rtp */
#define SIPSDP_NTE_SUPPORTED_HIGH     16 /* for dtmf-relay rtp-nse */

/*
 * Create the "on-wire" version, i.e. ready to put into a SIP message.
 * Memory is allocated and should be freed by the user when done
 * Returns NULL on failure.
 */
PMH_EXTERN char *sipsdp_write_to_buf(cc_sdp_t *, uint32_t *);

#define SIPSDP_FREE(x) \
if (x) \
{ \
    sdp_free_description(x); \
}

#define SIPSDP_MAX_PAYLOAD_TYPES 15
#define MAX_RTP_MEDIA_TYPES   6
#define SIPSDP_NTE_DTMF_MIN   0  /* Min value of DTMF event table */
#define SIPSDP_NTE_DTMF_MAX  15  /* Max DTMF event value supported here */


#endif /*_CCSIP_SDP_H_*/
