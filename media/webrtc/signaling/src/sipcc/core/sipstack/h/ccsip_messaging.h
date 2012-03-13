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

#ifndef _CCSIP_MESSAGING_H_
#define _CCSIP_MESSAGING_H_

#include "ccsip_sdp.h"
#include "ccsip_pmh.h"
#include "ccsip_credentials.h"
#include "ccsip_core.h"
#include "ccsip_subsmanager.h"
#include "ccapi.h"

#define SIP_MESSAGING_OK          (0)
#define SIP_MESSAGING_ERROR       (1)
#define SIP_MESSAGING_DUPLICATE   (2)
#define SIP_MESSAGING_NEW_CALLID  (3)
#define SIP_MESSAGING_ERROR_STALE_RESP (4)
#define SIP_MESSAGING_ERROR_UNSUPPORTED_MEDIA (5)
#define SIP_MESSAGING_ERROR_NO_TRX (6)
#define SIP_MESSAGING_NOT_ACCEPTABLE (7)
#define SIP_MESSAGING_ENDPOINT_NOT_FOUND (8)
#define NUM_INITIAL_RECORD_ROUTE_BUFS  4

typedef enum {
    SIP_INVITE_TYPE_INVALID = 0,
    SIP_INVITE_TYPE_NORMAL,
    SIP_INVITE_TYPE_MIDCALL,
    SIP_INVITE_TYPE_TRANSFER,
    SIP_INVITE_TYPE_AUTHORIZATION,
    SIP_INVITE_TYPE_REDIRECTED
} sipInviteType_t;

typedef enum {
    SIP_RTP_INCLUDE_MEDIA_INVALID = 0,
    SIP_RTP_INCLUDE_MEDIA_ALL,
    SIP_RTP_INCLUDE_MEDIA_PREFERRED,
    SIP_RTP_INCLUDE_MEDIA_MATCHING
} sipRTPIncludeMedia_t;

typedef enum {
    SIP_REF_NONE = 0,
    SIP_REF_XFER,
    SIP_REF_DIR_XFER,
    SIP_REF_TOKEN,
    SIP_REF_UNKNOWN
} sipRefEnum_e;

typedef struct {
    unsigned int flags;
    unsigned int extflags;
} sipMessageFlag_t;


#define SIP_HEADER_CONTACT_BIT               1
#define SIP_HEADER_RECORD_ROUTE_BIT          (1<<1)
#define SIP_HEADER_ROUTE_BIT                 (1<<2)
#define SIP_HEADER_SPARE_BIT0                (1<<3)
#define SIP_HEADER_REQUESTED_BY_BIT          (1<<4)
#define SIP_HEADER_DIVERSION_BIT             (1<<5)
#define SIP_HEADER_AUTHENTICATION_BIT        (1<<6)
#define SIP_HEADER_REFER_TO_BIT              (1<<7)
#define SIP_HEADER_REFERRED_BY_BIT           (1<<8)
#define SIP_HEADER_REPLACES_BIT              (1<<9)
#define SIP_HEADER_EVENT_BIT                 (1<<10)
#define SIP_HEADER_EXPIRES_BIT               (1<<11)
#define SIP_HEADER_ACCEPT_BIT                (1<<12)
#define SIP_HEADER_ALLOW_BIT                 (1<<13)
#define SIP_HEADER_CISCO_GUID_BIT            (1<<14)
#define SIP_HEADER_SPARE_BIT1                (1<<15)
#define SIP_HEADER_UNSUPPORTED_BIT           (1<<16)
#define SIP_HEADER_REMOTE_PARTY_ID_BIT       (1<<17)
#define SIP_HEADER_PROXY_AUTH_BIT            (1<<18)
#define SIP_HEADER_REQUIRE_BIT               (1<<19)
#define SIP_HEADER_CALL_INFO_BIT             (1<<20)
#define SIP_HEADER_SUPPORTED_BIT             (1<<21)
#define SIP_HEADER_RETRY_AFTER_BIT           (1<<22)
#define SIP_HEADER_ALLOW_EVENTS_BIT          (1<<23)
#define SIP_HEADER_CONTENT_TYPE_BIT          (1<<24)
#define SIP_HEADER_CONTENT_LENGTH_BIT        (1<<25)
#define SIP_HEADER_JOIN_INFO_BIT             (1<<26)
#define SIP_HEADER_ACCEPT_ENCODING_BIT       (1<<27)
#define SIP_HEADER_ACCEPT_LANGUAGE_BIT       (1<<28)
#define SIP_HEADER_OPTIONS_CONTENT_TYPE_BIT  (1<<29)
#define SIP_HEADER_REASON_BIT                (1<<30)
#define SIP_HEADER_RECV_INFO_BIT             (1U<<31)


#define SIP_RFC_SUPPORTED_TAGS REQ_SUPP_PARAM_REPLACES      ","   \
                               REQ_SUPP_PARAM_JOIN          ","   \
                               REQ_SUPP_PARAM_SDP_ANAT      ","   \
                               REQ_SUPP_PARAM_NOREFERSUB

/*
 * The REQ_SUPP_PARAM_EXTENED_REFER is only used in Cisco CCM environment.
 * The tag is no longer in IANA.
 */
#define SIP_CISCO_SUPPORTED_TAGS REQ_SUPP_PARAM_EXTENED_REFER         "," \
                                 REQ_SUPP_PARAM_CISCO_CALLINFO        "," \
                                 REQ_SUPP_PARAM_CISCO_ESCAPECODES     "," \
                                 REQ_SUPP_PARAM_CISCO_MONREC

#define SIP_CISCO_SUPPORTED_REG_TAGS  \
     SIP_RFC_SUPPORTED_TAGS       "," \
     SIP_CISCO_SUPPORTED_TAGS


boolean sipSPISendInvite(ccsipCCB_t *ccb,
                         sipInviteType_t inviteType,
                         boolean initInvite);
boolean sipSPISendInviteMidCall(ccsipCCB_t *ccb, boolean expires);
void sipSPISendInviteResponse100(ccsipCCB_t *ccb, boolean remove_to_tag);
void sipSPISendInviteResponse180(ccsipCCB_t *ccb);
void sipSPISendInviteResponse200(ccsipCCB_t *ccb);
void sipSPISendInviteResponse302(ccsipCCB_t *ccb);

boolean sipSPISendOptionResponse(ccsipCCB_t *ccb, sipMessage_t *);
boolean sipSPIsendNonActiveOptionResponse(sipMessage_t *msg,
                                          cc_msgbody_info_t *local_msg_body);
void sipSPISendInviteResponse(ccsipCCB_t *ccb,
                              uint16_t statusCode,
                              const char *reason_phrase,
                              uint16_t warnCode,
                              const char *warn_phrase,
                              boolean send_sd,
                              boolean retx);
void sipSPISendBye(ccsipCCB_t *ccb,
                   char *alsoString,
                   sipMessage_t *pForked200);
void sipSPISendCancel(ccsipCCB_t *ccb);
boolean sipSPISendByeOrCancelResponse(ccsipCCB_t *ccb,
                                      sipMessage_t *request,
                                      sipMethod_t sipMethodByeorCancel);
boolean sipSPISendAck(ccsipCCB_t *ccb, sipMessage_t *response);
boolean sipSPISendRegister(ccsipCCB_t *ccb,
                           boolean no_dns_lookup,
                           const char *user,
                           int expires_int);

boolean sipSPISendErrorResponse(sipMessage_t *msg, uint16_t status_code,
                                const char *reason_phrase,
                                uint16_t status_code_warning,
                                const char *reason_phrase_warning,
                                ccsipCCB_t *ccb);
boolean sipSPISendLastMessage(ccsipCCB_t *ccb);
boolean sipSPIGenerateAuthorizationResponse(sip_authen_t * sip_authen,
                                            const char *uri,
                                            const char *method,
                                            const char *user_name,
                                            const char *user_password,
                                            char **author_str,
                                            int *nc_count,
                                            ccsipCCB_t *ccb);
void sipSPIGenerateGenAuthorizationResponse(ccsipCCB_t *ccb,
                                            sipMessage_t *request,
                                            sipRet_t *flag,
                                            char *method);

void sipSPIGenerateTargetUrl(genUrl_t *genUrl, char *sipurlstr);
void sipSPIGenerateSipUrl(sipUrl_t *sipUrl, char *sipurlstr);

boolean sipSPISendRefer(ccsipCCB_t *ccb, char *referto, 
                        sipRefEnum_e referto_type);
boolean sipSPISendReferResponse202(ccsipCCB_t *ccb);
boolean sipSPISendNotify(ccsipCCB_t *ccb, int referto);
boolean sipSPISendInfo(ccsipCCB_t *ccb, const char *info_package,
                       const char *content_type, const char *message_body);
boolean sipSPISendUpdate(ccsipCCB_t *ccb);
boolean sipSPISendUpdateResponse(ccsipCCB_t *ccb,
                                 boolean send_sdp,
                                 cc_causes_t cause,
                                 boolean retx);
boolean sipSPISendNotifyResponse(ccsipCCB_t *ccb, cc_causes_t cause);
boolean sipSPIAddStdHeaders(sipMessage_t *msg,
                            ccsipCCB_t *ccb,
                            boolean isResponse);
sipMessage_t *sipSPIBuildRegisterHeaders(ccsipCCB_t *ccb,                    
                    const char *user,
                    int expires_int);
boolean sipSPIAddLocalVia(sipMessage_t *msg,
                          ccsipCCB_t *ccb,
                          sipMethod_t method);
boolean sipSPIAddRequestVia(ccsipCCB_t *ccb,
                            sipMessage_t *response,
                            sipMessage_t *request,
                            sipMethod_t method);
boolean sipSPIAddCiscoGuid(sipMessage_t *msg, ccsipCCB_t *ccb);
boolean sipSPIAddRouteHeaders(sipMessage_t *msg,
                              ccsipCCB_t *ccb,
                              char *result_route,
                              int result_route_length);
sipRet_t sipAddDateHeader(sipMessage_t *sip_message);

const char *sipGetMethodString(sipMethod_t methodname);

void sip_option_response_rtp_media_get_info(cc_sdp_t *src_sdp);

//sipSdp_t* CreateSDPtext();


sipRet_t sipSPIAddContactHeader(ccsipCCB_t *ccb, sipMessage_t *request);
sipRet_t sipSPIAddReasonHeader (ccsipCCB_t *ccb, sipMessage_t *request);
boolean sipSPIGenerateReferredByHeader(ccsipCCB_t *ccb);
boolean sipSPIGenRequestURI(ccsipCCB_t *ccb,
                            sipMethod_t sipmethod,
                            boolean initInvite);

// Message Factory
sipRet_t sipSPIAddCommonHeaders(ccsipCCB_t *ccb,
                                sipMessage_t *request,
                                boolean isResponse,
                                sipMethod_t method,
                                uint32_t response_cseq_number);
boolean CreateRequest(ccsipCCB_t *ccb,
                      sipMessageFlag_t messageflag,
                      sipMethod_t sipmethod,
                      sipMessage_t *request,
                      boolean initInvite,
                      uint32_t response_cseq_number);
boolean CreateResponse(ccsipCCB_t *ccb,
                       sipMessageFlag_t messageflag,
                       uint16_t status_code,
                       sipMessage_t *response,
                       const char *reason_phrase,
                       uint16_t status_code_warning,
                       const char *reason_phrase_warning,
                       sipMethod_t);
boolean SendRequest(ccsipCCB_t *ccb,
                    sipMessage_t *request,
                    sipMethod_t method,
                    boolean midcall,
                    boolean reTx,
                    boolean timer);
boolean AddGeneralHeaders(ccsipCCB_t *ccb,
                          sipMessageFlag_t messageflag,
                          sipMessage_t *request,
                          sipMethod_t method);
boolean getCSeqInfo(sipMessage_t *request,
                    sipCseq_t **request_cseq_structure);
int sipSPICheckRequest(ccsipCCB_t *ccb, sipMessage_t *request);
int sipSPICheckResponse(ccsipCCB_t *ccb, sipMessage_t *response);
int sipSPICheckContact(const char *pContactStr);

void sipGetRequestMethod(sipMessage_t *pRequest, sipMethod_t *pMethod);
int sipGetResponseMethod(sipMessage_t *pResponse, sipMethod_t *pMethod);
int sipGetResponseCode(sipMessage_t *pResponse, int *pResponseCode);
int sipSPIIncomingCallSDP(ccsipCCB_t *ccb,
                          boolean call_hold,
                          boolean udpate_ver);
char *sipSPIUrlDestination(sipUrl_t *sipUrl);
int sipGetMessageCSeq(sipMessage_t *pMessage, uint32_t *pResultCSeqNumber,
                      sipMethod_t * pResultCSeqMethod);
void sipGetMessageToTag(sipMessage_t *pMessage, char *to_tag,
                        int to_tag_max_length);
boolean sipSPISendByeAuth(sipMessage_t *pResponse,
                          sipAuthenticate_t authen,
                          cpr_ip_addr_t *dest_ipaddr,
                          uint16_t dest_port,
                          uint32_t cseq_number,
                          char *alsoString,
                          char *last_call_route,
                          char *last_call_route_request_uri,
                          line_t previous_call_line);
void free_sip_message(sipMessage_t *message);
void sipSPISendFailureResponseAck(ccsipCCB_t *ccb,
                                  sipMessage_t *response,
                                  boolean prevcall,
                                  line_t previous_call_line);
/* ICMP Unreachable handler routine */
void sip_platform_icmp_unreachable_callback(void *ccyb, uint32_t ipaddr);
cc_disposition_type_t sip2ccdisp(uint8_t type);
cc_content_type_t sip2cctype(uint8_t type);

/* Transaction record manipulation methods */
int16_t get_last_request_trx_index(ccsipCCB_t *ccb, boolean sent);
int16_t get_next_request_trx_index(ccsipCCB_t *ccb, boolean sent);
int16_t get_method_request_trx_index(ccsipCCB_t *ccb, sipMethod_t method,
                                     boolean sent);
void clean_method_request_trx(ccsipCCB_t *ccb, sipMethod_t method, boolean sent);
line_t get_dn_line_from_dn(const char *watcher);
boolean sipSPIGenerateRouteHeaderUAC(sipRecordRoute_t *, char *, int,
                                     boolean *loose_routing);
boolean sipSPIGenerateRouteHeaderUAS(sipRecordRoute_t *, char *, int,
                                     boolean *loose_routing);
boolean sipSPIGenerateContactHeader(sipContact_t *, char *, int);

void get_reason_string(int unreg_reason, char *unreg_reason_str, int len);

#endif
