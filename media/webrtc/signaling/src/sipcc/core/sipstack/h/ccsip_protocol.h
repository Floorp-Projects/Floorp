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

#ifndef _CCSIP_PROTOCOL_H_
#define _CCSIP_PROTOCOL_H_

#include "httpish_protocol.h"

typedef struct sip_header_
{
    char *hname;
    char *c_hname;
} sip_header_t;


#define SIP_VERSION "SIP/2.0"
#define SIP_SCHEMA "SIP/"
#define SIP_SCHEMA_LEN 4  /* length of the SIP_SCHEMA as defined above */

/* Methods */

#define SIP_METHOD_OPTIONS   "OPTIONS"
#define SIP_METHOD_INVITE    "INVITE"
#define SIP_METHOD_BYE       "BYE"
#define SIP_METHOD_CANCEL    "CANCEL"
#define SIP_METHOD_REGISTER  "REGISTER"
#define SIP_METHOD_ACK       "ACK"
#define SIP_METHOD_PRACK     "PRACK"
#define SIP_METHOD_COMET     "COMET"
#define SIP_METHOD_NOTIFY    "NOTIFY"
#define SIP_METHOD_REFER     "REFER"
#define SIP_METHOD_UPDATE    "UPDATE"
#define SIP_METHOD_INFO      "INFO"
#define SIP_METHOD_MESSAGE   "MESSAGE"
#define SIP_METHOD_PUBLISH   "PUBLISH"
#define SIP_METHOD_SUBSCRIBE "SUBSCRIBE"


/* Headers */
/*
 * We maintain a direct pointer to the header value for the following
 * headers.
 * Cached headers : Begin
 */
#define FROM               0
#define TO                 1
#define VIA                2
#define CALLID             3
#define CSEQ               4
#define CONTACT            5
#define CONTENT_LENGTH     6
#define CONTENT_TYPE       7
#define RECORD_ROUTE       8
#define REQUIRE            9
#define ROUTE             10
#define SUPPORTED         11
/* Cached headers : End */

#define SESSION            24
#define EXPIRES            41
#define LOCATION           42
#define TIMESTAMP          43
#define MAX_FORWARDS       44
#define UNSUPPORTED        45
#define ACCEPT             46
#define ACCEPT_ENCODING    47
#define ACCEPT_LANGUAGE    48
#define ALLOW              49
#define USER_AGENT         50
#define SERVER             51
#define DATE               52
#define CISCO_GUID         53
#define DIVERSION          54
#define EVENT              55
#define SESSION            24
#define CALL_INFO          61
#define JOIN               62

/* Session Header Types - indicates the purpose of SDP bodies */
#define SESSION_MEDIA      "Media"
#define SESSION_QOS        "QoS"
#define SESSION_SECURITY   "Security"

/* Content-Disposition Types */
#define SIP_CONTENT_DISPOSITION_UNKNOWN_VALUE 0
#define SIP_CONTENT_DISPOSITION_RENDER_VALUE 1
#define SIP_CONTENT_DISPOSITION_RENDER "render"
#define SIP_CONTENT_DISPOSITION_SESSION_VALUE 2
#define SIP_CONTENT_DISPOSITION_SESSION "session"
#define SIP_CONTENT_DISPOSITION_ICON_VALUE 3
#define SIP_CONTENT_DISPOSITION_ICON "icon"
#define SIP_CONTENT_DISPOSITION_ALERT_VALUE 4
#define SIP_CONTENT_DISPOSITION_ALERT "alert"
#define SIP_CONTENT_DISPOSITION_PRECONDITION_VALUE 5
#define SIP_CONTENT_DISPOSITION_PRECONDITION "precondition"

/* Content-Disposition Handling */
#define DISPOSITION_HANDLING "handling"
#define DISPOSITION_HANDLING_REQUIRED "required"
#define DISPOSITION_HANDLING_OPTIONAL "optional"

/* Some headers common to HTTP .some of the headers can be received
   in compact form ( SIP_C_ .. indicates compact header form ) */

#define SIP_HEADER_CONTENT_LENGTH      HTTPISH_HEADER_CONTENT_LENGTH
#define SIP_C_HEADER_CONTENT_LENGTH    HTTPISH_C_HEADER_CONTENT_LENGTH
#define SIP_HEADER_CONTENT_TYPE        HTTPISH_HEADER_CONTENT_TYPE
#define SIP_C_HEADER_CONTENT_TYPE      "c"
#define SIP_HEADER_CONTENT_ENCODING    HTTPISH_HEADER_CONTENT_ENCODING
#define SIP_C_HEADER_CONTENT_ENCODING  "e"
#define SIP_HEADER_CONTENT_ID          HTTPISH_HEADER_CONTENT_ID

#define SIP_HEADER_USER_AGENT    HTTPISH_HEADER_USER_AGENT
#define SIP_HEADER_SERVER        HTTPISH_HEADER_SERVER
#define SIP_HEADER_DATE          HTTPISH_HEADER_DATE

#define SIP_HEADER_VIA           HTTPISH_HEADER_VIA
#define SIP_C_HEADER_VIA         "v"
#define SIP_HEADER_MAX_FORWARDS  HTTPISH_HEADER_MAX_FORWARDS
#define SIP_HEADER_EXPIRES       HTTPISH_HEADER_EXPIRES
#define SIP_HEADER_LOCATION      HTTPISH_HEADER_LOCATION
#define SIP_C_HEADER_LOCATION    "m"

/* Headers specific to SIP .Some of the headers can be received in
   compact form */

#define SIP_HEADER_FROM        "From"
#define SIP_C_HEADER_FROM      "f"
#define SIP_HEADER_TO          "To"
#define SIP_C_HEADER_TO        "t"
#define SIP_HEADER_CSEQ        "CSeq"
#define SIP_HEADER_TIMESTAMP   "Timestamp"
#define SIP_HEADER_CALLID      "Call-ID"
#define SIP_C_HEADER_CALLID    "i"
#define SIP_HEADER_REQUIRE     "Require"
#define SIP_HEADER_SUPPORTED   "Supported"
#define SIP_C_HEADER_SUPPORTED "k"
#define SIP_HEADER_UNSUPPORTED "Unsupported"
#define SIP_HEADER_CONTACT     "Contact"
#define SIP_C_HEADER_CONTACT   SIP_C_HEADER_LOCATION
#define SIP_HEADER_CISCO_GUID  "Cisco-Guid"
#define SIP_HEADER_WARN        "Warning"

#define SIP_HEADER_ACCEPT      "Accept"
#define SIP_HEADER_ACCEPT_ENCODING "Accept-Encoding"
#define SIP_HEADER_ACCEPT_LANGUAGE "Accept-Language"
#define SIP_HEADER_ALLOW        "Allow"

#define SIP_HEADER_RECORD_ROUTE "Record-Route"
#define SIP_HEADER_ROUTE        "Route"
#define SIP_HEADER_SESSION      "Session"
#define SIP_HEADER_ALSO         "Also"
#define SIP_HEADER_REQUESTED_BY "Requested-By"
#define SIP_HEADER_DIVERSION    "Diversion"
#define SIP_HEADER_CC_DIVERSION "CC-Diversion"
#define SIP_HEADER_CC_REDIRECT  "CC-Redirect"
#define SIP_HEADER_ALERT_INFO   "Alert-Info"

#define SIP_HEADER_RSEQ         "RSeq"
#define SIP_HEADER_RACK         "RAck"
#define SIP_HEADER_CONTENT_DISP "Content-Disposition"

/* call stats */
#define SIP_RX_CALL_STATS       "RTP-RxStat"
#define SIP_TX_CALL_STATS       "RTP-TxStat"

/* Spam Specific */
#define SIP_HEADER_AUTHORIZATION       "Authorization"
#define SIP_HEADER_PROXY_AUTHORIZATION "Proxy-Authorization"
#define SIP_HEADER_PROXY_AUTHENTICATE  "Proxy-Authenticate"
#define SIP_HEADER_WWW_AUTHENTICATE    "WWW-Authenticate"

/* Refer parameters */
#define SIP_HEADER_REFER_TO        "Refer-To"
#define SIP_C_HEADER_REFER_TO      "r"
#define SIP_HEADER_REFERRED_BY     "Referred-By"
#define SIP_C_HEADER_REFERRED_BY   "b"

#define SIP_HEADER_REPLACES          "Replaces"
#define SIP_HEADER_PROXY_AUTH        "Proxy-Authorization"
#define SIP_HEADER_ACCEPT_CONTACT    "Accept-Contact"
#define SIP_C_HEADER_ACCEPT_CONTACT  "a"

#define SIP_HEADER_REMOTE_PARTY_ID   "Remote-Party-ID"

#define SIP_HEADER_EVENT        "Event"
#define SIP_C_HEADER_EVENT        "o"

#define SIP_HEADER_SIPIFMATCH   "SIP-If-Match"
#define SIP_HEADER_SIPETAG       "SIP-ETag"
#define SIP_HEADER_RETRY_AFTER  "Retry-After"
#define SIP_HEADER_MIN_EXPIRES  "Min-Expires"
#define SIP_HEADER_REASON       "Reason"

/* Event values */
#define SIP_EVENT_REFER         "refer"
#define SIP_EVENT_DIALOG        "dialog"
#define SIP_EVENT_KPML          "kpml"
#define SIP_EVENT_PRESENCE      "presence"
#define SIP_EVENT_CONFIG        "sip-profile"
#define SIP_EVENT_MWI           "message-summary"

/* Event param */
#define SIP_EVENT_ID            "id"

/* Subscription states */
#define SIP_HEADER_SUBSCRIPTION_STATE       "Subscription-State"
#define SIP_SUBSCRIPTION_STATE_ACTIVE       "active"
#define SIP_SUBSCRIPTION_STATE_PENDING      "pending"
#define SIP_SUBSCRIPTION_STATE_TERMINATED   "terminated"
/* Subscription state params */
#define SIP_SUBSCRIPTION_STATE_EXPIRES      "expires"
#define SIP_SUBSCRIPTION_STATE_REASON       "reason"
#define SIP_SUBSCRIPTION_STATE_RETRY_AFTER  "retry-after"
/* Subscription state reason values */
#define SIP_SUBSCRIPTION_STATE_REASON_DEACTIVATED "deactivated"
#define SIP_SUBSCRIPTION_STATE_REASON_PROBATION   "probation"
#define SIP_SUBSCRIPTION_STATE_REASON_REJECTED    "rejected"
#define SIP_SUBSCRIPTION_STATE_REASON_TIMEOUT     "timeout"
#define SIP_SUBSCRIPTION_STATE_REASON_GIVEUP      "giveup"
#define SIP_SUBSCRIPTION_STATE_REASON_NORESOURCE  "noresource"

/* Content-Type values */
#define SIP_CONTENT_TYPE_UNKNOWN_VALUE            0
#define SIP_CONTENT_TYPE_UNKNOWN                  "application/unknown"

#define SIP_CONTENT_TYPE_SDP_VALUE                1
#define SIP_CONTENT_TYPE_SDP                      "application/sdp"

#define SIP_CONTENT_TYPE_SIP_VALUE                2
#define SIP_CONTENT_TYPE_SIP                      "application/sip"

#define SIP_CONTENT_TYPE_SIPFRAG_VALUE            3
#define SIP_CONTENT_TYPE_SIPFRAG                  "message/sipfrag"

#define SIP_CONTENT_TYPE_DIALOG_VALUE             4
#define SIP_CONTENT_TYPE_DIALOG                   "application/dialog-info+xml"

#define SIP_CONTENT_TYPE_MWI_VALUE                5
#define SIP_CONTENT_TYPE_MWI                      "application/simple-message-summary"

#define SIP_CONTENT_TYPE_KPML_REQUEST_VALUE       6
#define SIP_CONTENT_TYPE_KPML_REQUEST             "application/kpml-request+xml"

#define SIP_CONTENT_TYPE_KPML_RESPONSE_VALUE      7
#define SIP_CONTENT_TYPE_KPML_RESPONSE            "application/kpml-response+xml"

#define SIP_CONTENT_TYPE_REMOTECC_REQUEST_VALUE   8
#define SIP_CONTENT_TYPE_REMOTECC_REQUEST         "application/x-cisco-remotecc-request+xml"

#define SIP_CONTENT_TYPE_REMOTECC_RESPONSE_VALUE  9
#define SIP_CONTENT_TYPE_REMOTECC_RESPONSE        "application/x-cisco-remotecc-response+xml"

#define SIP_CONTENT_TYPE_CTI_VALUE                10
#define SIP_CONTENT_TYPE_CTI                      "application/x-cisco-cti+xml"

#define SIP_CONTENT_TYPE_CMXML_VALUE              11
#define SIP_CONTENT_TYPE_CMXML                    "application/x-cisco-remotecc-cm+xml"

#define SIP_CONTENT_TYPE_MULTIPART_MIXED_VALUE    12
#define SIP_CONTENT_TYPE_MULTIPART_MIXED          "multipart/mixed"

#define SIP_CONTENT_TYPE_MULTIPART_ALTERNATIVE_VALUE    13
#define SIP_CONTENT_TYPE_MULTIPART_ALTERNATIVE    "multipart/alternative"

#define SIP_CONTENT_TYPE_TEXT_PLAIN_VALUE         14
#define SIP_CONTENT_TYPE_TEXT_PLAIN               "text/plain"

#define SIP_CONTENT_TYPE_PRESENCE_VALUE           15
#define SIP_CONTENT_TYPE_PRESENCE                 "application/pidf+xml"

#define SIP_CONTENT_TYPE_CONFIGAPP_VALUE   	  16
#define SIP_CONTENT_TYPE_CONFIGAPP               "application/x-cisco-config+xml"

#define SIP_CONTENT_TYPE_MEDIA_CONTROL           "application/media_control+xml"

/* Content-Type for multipart-mime */
#define SIP_CONTENT_TYPE_MULTIPART               "multipart/mixed"
#define SIP_CONTENT_BOUNDARY                     "boundary"
#define SIP_CONTENT_BOUNDARY_TEXT                "uniqueBoundary"

/* Content-Encoding */
/* At this time identity is the only one recognize */
#define SIP_CONTENT_ENCODING_IDENTITY            "identity"
#define SIP_CONTENT_ENCODING_IDENTITY_VALUE      0
#define SIP_CONTENT_ENCODING_UNKNOWN_VALUE       1

/* Call-info header */
#define SIP_HEADER_CALL_INFO                     "Call-Info"
#define SIP_CALL_INFO_PURPOSE                    "Purpose"
#define SIP_CALL_INFO_PURPOSE_INFO               "info"
#define SIP_CALL_INFO_PURPOSE_ICON               "icon"
#define SIP_CALL_INFO_PURPOSE_CARD               "card"
#define SIP_CALL_INFO_ORIENTATION                "Orientation"
#define SIP_CALL_INFO_ORIENTATION_TO             "To"
#define SIP_CALL_INFO_ORIENTATION_FROM           "From"
#define SIP_CALL_INFO_SECURITY                   "Security"
#define SIP_CALL_INFO_SECURITY_UNKNOWN           "Unknown"
#define SIP_CALL_INFO_SECURITY_AUTHENTICATED     "Authenticated"
#define SIP_CALL_INFO_SECURITY_NOT_AUTHENTICATED "NotAuthenticated"

/* via-params */
#define MAX_TTL_VAL             255
#define SIP_WELL_KNOWN_PORT     5060
#define SIP_WELL_KNOWN_PORT_STR "5060"
#define VIA_HIDDEN              "hidden"
#define VIA_TTL                 "ttl"
#define VIA_MADDR               "maddr"
#define VIA_RECEIVED            "received"
#define VIA_BRANCH              "branch"
#define VIA_BRANCH_START        "z9hG4bK"

/* CC-Diversion Parameters */
#define DIVERSION_REASON        "reason"
#define DIVERSION_COUNTER       "counter"
#define DIVERSION_LIMIT         "limit"
#define DIVERSION_PRIVACY       "privacy"
#define DIVERSION_SCREEN        "screen"

/* CC-Redirect Parameters */
#define CC_REDIRECT_REASON      "redir-reason"
#define CC_REDIRECT_COUNTER     "redir-counter"
#define CC_REDIRECT_LIMIT       "redir-limit"

/* CC-Diversion Reason Parameter values */
// REASON_UNKNOWN conflicts with winreg.h on Windows.
#ifdef REASON_UNKNOWN
#undef REASON_UNKNOWN
#endif
#define REASON_UNKNOWN          "unknown"
#define REASON_USER_BUSY        "user-busy"
#define REASON_NO_ANSWER        "no-answer"
#define REASON_UNCONDITIONAL    "unconditional"
#define REASON_DEFLECTION       "deflection"
#define REASON_FOLLOW_ME        "follow-me"
#define REASON_OUT_OF_SERVICE   "out-of-service"
#define REASON_TIME_OF_DAY      "time-of-day"
#define REASON_DO_NOT_DISTURB   "do-not-disturb"
#define REASON_UNAVAILABLE      "unavailable"
#define REASON_AWAY             "away"

/* Replaces parameters */
#define SIGNATURE_SCHEME        "scheme"
#define TO_TAG                  "to-tag"
#define FROM_TAG                "from-tag"

/* Remote-Party-Id parameters */
#define RPID_SCREEN             "screen"
#define RPID_PARTY_TYPE         "party"
#define RPID_ID_TYPE            "id-type"
#define RPID_PRIVACY            "privacy"
#define RPID_NP                 "np"

#define RPID_CALLBACK           "x-cisco-callback-number="

#define RPID_SCREEN_LEN         (sizeof(RPID_SCREEN) - 1)
#define RPID_PARTY_TYPE_LEN     (sizeof(RPID_PARTY_TYPE) - 1)
#define RPID_ID_TYPE_LEN        (sizeof(RPID_ID_TYPE) - 1)
#define RPID_PRIVACY_LEN        (sizeof(RPID_PRIVACY) - 1)
#define RPID_NP_LEN             (sizeof(RPID_NP) - 1)
#define RPID_CALLBACK_LEN       (sizeof(RPID_CALLBACK) - 1)

#define KPML_ID_CALLID          "call-id"
#define KPML_ID_FROM_TAG        "from-tag"
#define KPML_ID_TO_TAG          "to-tag"
#define KPML_ID_CALLID_LEN      (sizeof(SIP_HEADER_CALLID)-1)
#define KPML_ID_FROM_TAG_LEN    (sizeof(FROM_TAG)-1)
#define KPML_ID_TO_TAG_LEN      (sizeof(TO_TAG)-1)

#define PRIVACY_OFF        "off"
#define PRIVACY_NAME       "name"
#define PRIVACY_URI        "uri"
#define PRIVACY_FULL       "full"
#define SCREEN_NO          "no"
#define SCREEN_YES         "yes"
#define PARTY_TYPE_CALLING "calling"
#define PARTY_TYPE_CALLED  "called"

#define SIP_HEADER_ALLOW_EVENTS "Allow-Events"
#define SIP_HEADER_MIME_VERSION "MIME-Version"
#define SIP_MIME_VERSION_VALUE  "1.0"

/* Require and Supported header params */
#define REQ_SUPP_PARAM_REPLACES      "replaces"
#define REQ_SUPP_PARAM_100REL        "100rel"
#define REQ_SUPP_PARAM_EARLY_SESSION "early-session"
#define REQ_SUPP_PARAM_JOIN          "join"
#define REQ_SUPP_PARAM_PATH          "path"
#define REQ_SUPP_PARAM_PRECONDITION  "precondition"
#define REQ_SUPP_PARAM_PREF          "pref"
#define REQ_SUPP_PARAM_PRIVACY       "privacy"
#define REQ_SUPP_PARAM_SEC_AGREE     "sec-agree"
#define REQ_SUPP_PARAM_TIMER         "timer"
#define REQ_SUPP_PARAM_NOREFERSUB    "norefersub"
#define REQ_SUPP_PARAM_EXTENED_REFER "extended-refer"
#define REQ_SUPP_PARAM_CISCO_CALLINFO "X-cisco-callinfo"
#define REQ_SUPP_PARAM_CISCO_SERVICEURI      "X-cisco-serviceuri"
#define REQ_SUPP_PARAM_CISCO_ESCAPECODES     "X-cisco-escapecodes"
#define REQ_SUPP_PARAM_CISCO_SERVICE_CONTROL "X-cisco-service-control"
#define REQ_SUPP_PARAM_CISCO_SRTP_FALLBACK   "X-cisco-srtp-fallback"
#define REQ_SUPP_PARAM_CISCO_CONFIG "X-cisco-config"
#define REQ_SUPP_PARAM_SDP_ANAT "sdp-anat"
/* Add defines for the SIP Interfcae Specification (SIS) protocol version tags*/
#define REQ_SUPP_PARAM_CISCO_SISTAG "X-cisco-sis-" 
#define SIS_CURRENT_PROTOCOL_VERSION  "5.2.0"
#define SIS_PROTOCOL_MAJOR_VERSION_SEADRAGON  1
#define SIS_PROTOCOL_MAJOR_VERSION_MUSTER  2
#define SIS_PROTOCOL_MAJOR_VERSION_UNISON  3
#define SIS_PROTOCOL_MAJOR_VERSION_GUILD   4
#define SIS_PROTOCOL_MAJOR_VERSION_ANGELFIRE   5
#define SIS_PROTOCOL_MINOR_VERSION_ANGELFIRE   1
#define SIS_PROTOCOL_MINOR_VERSION_MONTBLANC   1
#define REQ_SUPP_PARAM_CISCO_SIPVER REQ_SUPP_PARAM_CISCO_SISTAG SIS_CURRENT_PROTOCOL_VERSION
#define REQ_SUPP_PARAM_CISCO_MONREC "X-cisco-monrec" 
/* Add define for CME version negotiation */
#define REQ_SUPP_PARAM_CISCO_CME_SISTAG "X-cisco-cme-sis-"

/* Contact parameters */
#define REQ_CONT_PARAM_CISCO_NEWREG          "X-cisco-newreg"

/* Max-forwards header */
#define SIP_MAX_FORWARDS_DEFAULT_STR     "70"
#define SIP_MAX_FORWARDS_DEFAULT_VALUE   70

/* Join header */
#define SIP_HEADER_JOIN               "Join"
#define SIP_HEADER_JOIN_FROM_TAG      "from-tag"
#define SIP_HEADER_JOIN_TO_TAG        "to-tag"

/* Info-Package headers */
#define SIP_HEADER_SEND_INFO    "Send-Info"
#define SIP_HEADER_RECV_INFO    "Recv-Info"
#define SIP_HEADER_INFO_PACKAGE "Info-Package"

/*
 * ALERT : If you are defining a compact header more than 2 bytes long, modify
 * routine compact_hdr_cmp() in file httpish.c
 */

/*
 * Revised SIP drafts say that we use Contact and not Location in the header.
 * But we are supporting both currently for MCI
 */

/*   S T A T U S   C O D E S  */

/* Informational Status Codes 1xx */

#define SIP_1XX_TRYING           100 /*  "Trying"  */
#define SIP_1XX_RINGING          180 /*  "Ringing" */
#define SIP_1XX_CALL_FWD         181 /*  "Call is being forwaded" */
#define SIP_1XX_QUEUED           182 /*  "Queued" */
#define SIP_1XX_SESSION_PROGRESS 183 /* "Session Progress" */

/* Success Status Code 2xx */
#define SIP_SUCCESS_SETUP        200 /* OK/Success */
#define SIP_ACCEPTED             202 /* Accepted */
#define SIP_STATUS_SUCCESS SIP_SUCCESS_SETUP

/* Redirection Status Codes 3xx */
#define SIP_RED_MULT_CHOICES     300 /* Multiple Choices */
#define SIP_RED_MOVED_PERM       301 /* Moved Permanently */
#define SIP_RED_MOVED_TEMP       302 /* Moved Temporarily */
#define SIP_RED_SEE_OTHER        303 /* See Other */
#define SIP_RED_USE_PROXY        305 /* Use Proxy */
#define SIP_RED_ALT_SERVICE      380 /* Alternative Service */

/* Client Error Status Codes 4xx */
#define SIP_CLI_ERR_BAD_REQ      400 /* Bad Request */
#define SIP_CLI_ERR_UNAUTH       401 /* Unauthorized */
#define SIP_CLI_ERR_PAY_REQD     402 /* Payment Required */
#define SIP_CLI_ERR_FORBIDDEN    403 /* Forbidden */
#define SIP_CLI_ERR_NOT_FOUND    404 /* Not Found */
#define SIP_CLI_ERR_NOT_ALLOWED  405 /* Method Not Allowed */
#define SIP_CLI_ERR_NOT_ACCEPT   406 /* Not Acceptable */
#define SIP_CLI_ERR_PROXY_REQD   407 /* Proxy Authentication Required */

#define SIP_CLI_ERR_REQ_TIMEOUT  408 /* Request Timeout */
#define SIP_CLI_ERR_CONFLICT     409 /* Conflict */
#define SIP_CLI_ERR_GONE         410 /* Gone */
#define SIP_CLI_ERR_LEN_REQD     411 /* Length Required */
#define SIP_CLI_ERR_COND_FAIL    412 /* Conditional req failed */
#define SIP_CLI_ERR_LARGE_MSG    413 /* Request Message Body Too Large */
#define SIP_CLI_ERR_LARGE_URI    414 /* Request-URI Too Large */
#define SIP_CLI_ERR_MEDIA        415 /* Unsupported Media Type */
#define SIP_CLI_ERR_EXTENSION    420 /* Bad Extension */
#define SIP_CLI_ERR_INTERVAL_TOO_SMALL 423 /* Duration too small */
#define SIP_CLI_ERR_ANONYMITY_NOT_ALLOWED 433 /* Anonymity Disallowed */
#define SIP_CLI_ERR_NOT_AVAIL    480 /* Temporarily Not Available */
#define SIP_CLI_ERR_CALLEG       481 /* Call Leg/Transaction Does Not Exist */
#define SIP_CLI_ERR_LOOP_DETECT  482 /* Loop Detected */
#define SIP_CLI_ERR_MANY_HOPS    483 /* Too Many Hops */
#define SIP_CLI_ERR_ADDRESS      484 /* Address Incomplete */
#define SIP_CLI_ERR_AMBIGUOUS    485 /* Ambiguous */
#define SIP_CLI_ERR_BUSY_HERE    486 /* Busy here */
#define SIP_CLI_ERR_REQ_CANCEL   487 /* Request Cancelled */
#define SIP_CLI_ERR_NOT_ACCEPT_HERE 488 /* Not Acceptable Here */
#define SIP_CLI_ERR_BAD_EVENT    489 /* Bad Event */
#define SIP_CLI_ERR_REQ_PENDING  491 /* Request Pending */

/* Server Error Status Codes 5xx */
#define SIP_SERV_ERR_INTERNAL    500 /* Internal Server Error */
#define SIP_SERV_ERR_NOT_IMPLEM  501 /* Not Implemented */
#define SIP_SERV_ERR_BAD_GW      502 /* Bad Gateway */
#define SIP_SERV_ERR_UNAVAIL     503 /* Service Unavailable */
#define SIP_SERV_ERR_GW_TIMEOUT  504 /* Gateway Timeout */
#define SIP_SERV_ERR_SIP_VER     505 /* SIP Version not supported */
#define SIP_SERV_ERR_PRECOND_FAILED 580 /* Precondition Failed */

/* Global Failure Status Codes 6xx */
#define SIP_FAIL_BUSY            600 /* Busy */
#define SIP_FAIL_DECLINE         603 /* Decline */
#define SIP_FAIL_NOT_EXIST       604 /* Does not exist anywhere */
#define SIP_FAIL_NOT_ACCEPT      606 /* Not Acceptable */

/* SIP Warning Codes 3xx */
#define SIP_WARN_INCOMPAT_NETWORK_PROT  300 /* Incompatible Network Protocol */
#define SIP_WARN_INCOMPAT_NETWORK_ADDR  301 /* Incompatible Network Address */
#define SIP_WARN_INCOMPAT_TRANS_PROT    302 /* Incompatible Transport Protocol */
#define SIP_WARN_INCOMPAT_BANDW_UNITS   303 /* Incompatible Bandwidth Units */
#define SIP_WARN_MEDIA_TYPE_UNAVAIL     304 /* Media Type(s) Unavailable */
#define SIP_WARN_INCOMPAT_MEDIA_FORMAT  305 /* Incompatible Media Formats */
#define SIP_WARN_UNKNOWN_MEDIA_ATTRIB   306 /* Media Attribute not understood */
#define SIP_WARN_UNKNOWN_SDP_PARAM      307 /* SDP Parameter not understood */
#define SIP_WARN_MISC                   399 /* Miscellaneous */

/* Other warnings */
#define SIP_WARN_PROCESSING_PREVIOUS_REQUEST 901

/*   R E A S O N   P H R A S E S  */

/* Informational  1xx */

#define SIP_1XX_TRYING_PHRASE    "Trying"
#define SIP_1XX_RINGING_PHRASE   "Ringing"
#define SIP_1XX_CALL_FWD_PHRASE  "Call is being forwaded"
#define SIP_1XX_QUEUED_PHRASE    "Queued"
#define SIP_1XX_SESSION_PROGRESS_PHRASE "Session Progress"

/* Success  2xx */
#define SIP_SUCCESS_SETUP_PHRASE  "OK"
#define SIP_ACCEPTED_PHRASE  "Accepted"

/* Redirection  3xx */
#define SIP_RED_MULT_CHOICES_PHRASE  "Multiple Choices"
#define SIP_RED_MOVED_PERM_PHRASE    "Moved Permanently"
#define SIP_RED_MOVED_TEMP_PHRASE    "Moved Temporarily"
#define SIP_RED_SEE_OTHER_PHRASE     "See Other"
#define SIP_RED_USE_PROXY_PHRASE     "Use Proxy"
#define SIP_RED_ALT_SERVICE_PHRASE   "Alternative Service"

/* Client Error  4xx */
#define SIP_CLI_ERR_BAD_REQ_PHRASE     "Bad Request"
#define SIP_CLI_ERR_UNAUTH_PHRASE      "Unauthorized"
#define SIP_CLI_ERR_PAY_REQD_PHRASE    "Payment Required"
#define SIP_CLI_ERR_FORBIDDEN_PHRASE   "Forbidden"
#define SIP_CLI_ERR_NOT_FOUND_PHRASE   "Not Found"
#define SIP_CLI_ERR_NOT_ALLOWED_PHRASE "Method Not Allowed"
#define SIP_CLI_ERR_NOT_ACCEPT_PHRASE  "Not Acceptable"
#define SIP_CLI_ERR_PROXY_REQD_PHRASE  "Proxy Authentication Required"

#define SIP_CLI_ERR_REQ_TIMEOUT_PHRASE "Request Timeout"
#define SIP_CLI_ERR_CONFLICT_PHRASE    "Conflict"
#define SIP_CLI_ERR_GONE_PHRASE        "Gone"
#define SIP_CLI_ERR_LEN_REQD_PHRASE    "Length Required"
#define SIP_CLI_ERR_LARGE_MSG_PHRASE   "Request Message Body Too Large"
#define SIP_CLI_ERR_LARGE_URI_PHRASE   "Request-URI Too Large"
#define SIP_CLI_ERR_MEDIA_PHRASE       "Unsupported Media Type"
#define SIP_CLI_ERR_EXTENSION_PHRASE   "Bad Extension"
#define SIP_CLI_ERR_NOT_AVAIL_PHRASE   "Temporarily Not Available"
#define SIP_CLI_ERR_CALLEG_PHRASE      "Call Leg/Transaction Does Not Exist"
#define SIP_CLI_ERR_SUBS_DOES_NOT_EXIST_PHRASE "Subscription Does Not Exist"
#define SIP_CLI_ERR_LOOP_DETECT_PHRASE "Loop Detected"
#define SIP_CLI_ERR_MANY_HOPS_PHRASE   "Too Many Hops"
#define SIP_CLI_ERR_ADDRESS_PHRASE     "Address Incomplete"
#define SIP_CLI_ERR_AMBIGUOUS_PHRASE   "Ambiguous"
#define SIP_CLI_ERR_BUSY_HERE_PHRASE   "Busy here"
#define SIP_CLI_ERR_REQ_CANCEL_PHRASE  "Request Cancelled"
#define SIP_CLI_ERR_NOT_ACCEPT_HERE_PHRASE "Not Acceptable Here"
#define SIP_CLI_ERR_BAD_EVENT_PHRASE   "Bad Event"
#define SIP_CLI_ERR_INTERVAL_TOO_SMALL_PHRASE "Interval too small"
#define SIP_CLI_ERR_INTERVAL_TOO_LARGE_PHRASE "Interval too large"
#define SIP_CLI_ERR_ANONYMITY_NOT_ALLOWED_PHRASE "Anonymity Disallowed"
#define SIP_CLI_ERR_REQ_PENDING_PHRASE "Request Pending"

/* Server Error  5xx */
#define SIP_SERV_ERR_INTERNAL_PHRASE   "Internal Server Error"
#define SIP_SERV_ERR_NOT_IMPLEM_PHRASE "Not Implemented"
#define SIP_SERV_ERR_BAD_GW_PHRASE     "Bad Gateway"
#define SIP_SERV_ERR_UNAVAIL_PHRASE    "Service Unavailable"
#define SIP_SERV_ERR_GW_TIMEOUT_PHRASE "Gateway Timeout"
#define SIP_SERV_ERR_SIP_VER_PHRASE    "SIP Version not supported"
#define SIP_SERV_ERR_PRECOND_FAILED_PHRASE "Precondition Failed"

/* Global Failure  6xx */
#define SIP_FAIL_BUSY_PHRASE           "Busy"
#define SIP_FAIL_DECLINE_PHRASE        "Decline"
#define SIP_FAIL_NOT_EXIST_PHRASE      "Does not exist anywhere"
#define SIP_FAIL_NOT_ACCEPT_PHRASE     "Not Acceptable"

/* SIP Warning Codes 3xx Phrases */
#define SIP_WARN_INCOMPAT_NETWORK_PROT_PHRASE "\"Incompatible Network Protocol\""
#define SIP_WARN_INCOMPAT_NETWORK_ADDR_PHRASE "\"Incompatible Network Address\""
#define SIP_WARN_INCOMPAT_TRANS_PROT_PHRASE "\"Incompatible Transport Protocol\""
#define SIP_WARN_INCOMPAT_BANDW_UNITS_PHRASE "\"Incompatible Bandwidth Units\""
#define SIP_WARN_MEDIA_TYPE_UNAVAIL_PHRASE "\"Media Type(s) Unavailable\""
#define SIP_WARN_INCOMPAT_MEDIA_FORMAT_PHRASE "\"Incompatible Media Formats\""
#define SIP_WARN_UNKNOWN_MEDIA_ATTRIB_PHRASE "\"Media Attribute not understood\""
#define SIP_WARN_UNKNOWN_SDP_PARAM_PHRASE  "\"SDP Parameter not understood\""
#define SIP_WARN_REFER_AMBIGUOUS_PHRASE  "\"Ambiguous.Multiple Contacts Found.\""

#define SIP_STATUS_PHRASE_NONE         "Unknown"

/* Client Error 400 Bad Request detailed explaination */
#define SIP_CLI_ERR_BAD_REQ_MEDIA_COMP_FAILED     "Bad Request - 'Media info comparison failed'"
#define SIP_CLI_ERR_BAD_REQ_SDP_ERROR             "Bad Request - 'Invalid SDP information'"
#define SIP_CLI_ERR_BAD_REQ_RECORD_ROUTE          "Bad Request - 'Malformed/Missing Record Route'"
#define SIP_CLI_ERR_BAD_REQ_CONTACT_FIELD         "Bad Request - 'Malformed/Missing Contact field'"
#define SIP_CLI_ERR_BAD_REQ_CALLID_ABSENT         "Bad Request - 'Callid absent'"
#define SIP_CLI_ERR_BAD_REQ_CSEQ_FIELD            "Bad Request - 'Error in Cseq field'"
#define SIP_CLI_ERR_BAD_REQ_FROM_OR_TO_FIELD      "Bad Request - 'Error in From and/or To field'"
#define SIP_CLI_ERR_BAD_REQ_MEDIA_NEGOTIATION     "Bad Request - 'Media Negotiation Failed'"
#define SIP_CLI_ERR_BAD_REQ_URL_ERROR             "Bad Request - 'Malformed/Missing URL'"
#define SIP_CLI_ERR_BAD_REQ_ToURL_ERROR           "Bad Request - 'Malformed/Missing TO: field'"
#define SIP_CLI_ERR_BAD_REQ_FROMURL_ERROR         "Bad Request - 'Malformed/Missing FROM: field'"
#define SIP_CLI_ERR_BAD_REQ_CALLID_ERROR          "Bad Request - 'Malformed/Missing CALLID field'"
#define SIP_CLI_ERR_BAD_REQ_REQLINE_ERROR         "Bad Request - 'Malformed/Missing REQUEST LINE'"
#define SIP_CLI_ERR_BAD_REQ_IPADDRESS_ERROR       "Bad Request - 'Invalid IP Address'"
#define SIP_CLI_ERR_BAD_REQ_CONTENT_LENGTH_ERROR  "Bad Request - 'Content length Error'"
#define SIP_CLI_ERR_BAD_REQ_CONTENT_TYPE_ERROR    "Bad Request - 'Malformed/Missing SIP CONTENT TYPE field'"
#define SIP_CLI_ERR_BAD_REQ_PHRASE_DIVERSION      "Bad Request - 'Malformed CC-Diversion Header'"
#define SIP_CLI_ERR_BAD_REQ_VIA_OR_CSEQ           "Bad Request - 'Malformed/Missing VIA OR CSEQ'"
#define SIP_CLI_ERR_BAD_REQ_PHRASE_REFER          "Bad Request - 'Malformed REFER Request'"
#define SIP_CLI_ERR_BAD_REQ_PHRASE_REFER_TO       "Bad Request - 'Malformed Refer-To Header'"
#define SIP_CLI_ERR_BAD_REQ_PHRASE_REFER_BY       "Bad Request - 'Missing   Refer-By Header'"
#define SIP_CLI_ERR_BAD_REQ_PHRASE_REPLACES       "Bad Request - 'Malformed Replaces Header'"
#define SIP_CLI_ERR_BAD_REQ_METHOD_UNKNOWN        "Bad Request - 'Unable to obtain SIP method'"
#define SIP_CLI_ERR_BAD_REQ_BAD_BODY_ENCODING     "Bad Request - 'Unable to decode body'"
#define SIP_CLI_ERR_BAD_REQ_SUBSCRIPTION_DELETED  "Bad Request - 'Subscription Terminated'"
#define SIP_CLI_ERR_BAD_REQ_NO_BODY               "Bad Request - 'Body Expected'"
#define SIP_CLI_ERR_BAD_REQ_NO_SUBSCRIPTION_HEADER "Bad Request - 'Malformed/Missing Subscription-State Header'"
#define SIP_CLI_ERR_BAD_REQ_CONTENT_ID_ERROR      "Bad Request - 'Invalid Content-Id field'"
#define SIP_CLI_ERR_BAD_REQ_REQUIRE_HDR           "Bad Request - 'Malformed/Missing Require header'"             
#endif /*_CCSIP_PROTOCOL_H_*/
