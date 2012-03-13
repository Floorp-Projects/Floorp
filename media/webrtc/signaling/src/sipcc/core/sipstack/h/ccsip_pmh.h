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

#ifndef _CCSIP_PMH_H_
#define _CCSIP_PMH_H_

#include "cpr_types.h"
#include "pmhdefs.h"
#include "httpish.h"
#include "ccsip_protocol.h"
#include "string_lib.h"
#include "ccsip_platform.h"
#include "ccapi.h"

#define MAX_REFER_TO_HEADER_CONTENTS  6
#define MAX_REMOTE_PARTY_ID_HEADERS   10
#define MAX_REPLACES_HEADERS          1
#define MAX_REFER_TO_HEADERS          2
#define MAX_REFER_BY_HEADERS          1
#define MAX_CALL_INFO_HEADERS         4

#define MAX_SUB_STATE_HEADER_SIZE     96
#define TEMP_PARSE_BUFFER_SIZE        10

/*
 * Maximum number of Location/Contact headers we will parse. These
 * specify the locations to try to contact the called party, and I figure
 * the user is not going to wait for ever, and simplicity helps.
 */
#define SIP_MAX_LOCATIONS 6

/* Used when we create Via headers. Does not apply to incoming messages.*/
#define SIP_MAX_VIA_LENGTH 128

/* Broadsoft: Max length of "other" parameters in 3xx messages. */
#define SIP_MAX_OTHER_PARAM_LENGTH 256

#define TWO_POWER_31 2147483648UL

/*
 * Some of this corresponds directly to HTTP/1.1 message structure.
 */
typedef httpish_header    sip_header;
typedef httpishMsg_t      sipMessage_t;
typedef httpishReqLine_t  sipReqLine_t;
typedef httpishRespLine_t sipRespLine_t;

typedef httpishStatusCodeClass_t sipStatusCodeClass_t;

typedef hStatus_t sipRet_t;

typedef enum
{
    sipMethodInvalid = 0,
    sipMethodRegister = 100,
    sipMethodOptions,
    sipMethodInvite,
    sipMethodBye,
    sipMethodCancel,
    sipMethodPrack,
    sipMethodComet,
    sipMethodNotify,
    sipMethodRefer,
    sipMethodAck,
    sipMethodMessage,
    sipMethodSubscribe,
    sipMethodPublish,
    sipMethodUpdate,
    sipMethodResponse,
    sipMethodInfo,
    sipMethodUnknown
} sipMethod_t;

typedef enum
{
    SIP_BASIC = 1,
    SIP_DIGEST,
    SIP_UNKNOWN
} sip_scheme_t;

typedef enum
{
    SIP_SUCCESS,
    SIP_FAILURE,
    SIP_INTERNAL_ERR,
    SIP_UNACCEPTABLE_MEDIA_ERR,
    SIP_PRE_CONDITION_FAIL_ERR,
    SIP_NO_RESOURCE,
    SIP_MSG_CREATE_ERR,
    SIP_MSG_PARSE_ERR,
    SIP_MSG_INCOMPLETE_ERR,
    SIP_SUCCESS_DNS_PENDING,
    SIP_SUCCESS_QOS_PENDING,
    SIP_VIA_DNS_QUERY_REQUIRED,
    SIP_SUCCESS_QOS_DNS_PENDING,
    SIP_SUCCESS_DELAYED_MEDIA,
    SIP_SUCCESS_QOS_DELAYED_MEDIA,
    SIP_ADDR_UNAVAILABLE,
    SIP_MAX_RC
} ccsipRet_e;

/* This is used to store the parsed attribute/value pair */
typedef struct {
    char *attr;
    char *value;
} attr_value_pair_t;

typedef struct
{
    /*
     * Not storing the schema because we don't deal with non-SIP URLs.
     * The rest of the fields do not have any meaning if the URL
     * in question is non-SIP. Our parse routine itself would return
     * failure.
     */
    char    *user;
    char    *password;
    char    *host;
    char    *maddr;
    char    *other;
    char    *method;
    uint16_t port;
    boolean  port_present;
    int16_t  transport;
    uint8_t  is_phone;
    uint8_t  ttl_val;
    char     num_headers;       /* number of headers in the avpair below */
    attr_value_pair_t *headerp; /* everything after the '?' */
    boolean  lr_flag;
    boolean  is_ipv6;
} sipUrl_t;

typedef struct
{
    char *user;
    char *isdn_subaddr;
    char *post_dial;
    char *unparsed_tsp;
    char *future_ext;
} telUrl_t;

typedef enum
{
    URL_TYPE_SIP = 1,
    URL_TYPE_TEL,
    URL_TYPE_CID,
    URL_TYPE_UNKNOWN
} urlType_t;

typedef struct
{
    urlType_t schema;
    boolean   sips;
    char     *str_start;
    char     *phone_context;
    char     *other_params[SIP_MAX_LOCATIONS];
    union {
        sipUrl_t *sipUrl;
        telUrl_t *telUrl;
    } u;
} genUrl_t;

typedef struct
{
    char        *str_start;  /* beginning of duplicated string */
    sip_scheme_t scheme;
    char        *user_pass;
    char        *realm;
    char        *d_username;
    char        *unparsed_uri;
    char        *response;
    char        *algorithm;
    char        *cnonce;
    char        *opaque;
    char        *qop;
    char        *nc_count;
    char        *auth_param; /* for future extension */
    char        *nonce;
} sip_author_t;

typedef struct
{
    /* Could be Basic or PGP */
    char        *str_start;  /* beginning of duplicated string */
    sip_scheme_t scheme;
    char        *user_pass;
    char        *realm;
    char        *version;
    char        *algorithm;
    char        *nonce;
    char        *unparsed_domain;
    char        *opaque;
    char        *stale;
    char        *qop;
} sip_authen_t;

typedef struct _sipLocation_t
{
    char     *loc_start;
    char     *name;
    genUrl_t *genUrl;
    char     *tag;
} sipLocation_t;

typedef sipLocation_t sipFrom_t;
typedef sipLocation_t sipTo_t;

/*
 * Definition of the flags in the sipContactParams_t.
 * The flags in the sipContact_t is used to store boolean
 * parameters (not numerical value).
 */
#define SIP_CONTACT_PARM_X_CISCO_NEWREG  (1 << 0)   /* new reg parameter */
typedef struct
{
    int     action;     /* Proxy or Redirect */
    char    *qval;
    uint32_t expires;
    char    *expires_gmt;
    char    *extn_attr;
    uint32_t flags;
} sipContactParams_t;

typedef struct
{
    sipLocation_t     *locations[SIP_MAX_LOCATIONS];
    sipContactParams_t params[SIP_MAX_LOCATIONS];
    uint16_t           num_locations;
    boolean            new_flag;
} sipContact_t;

typedef struct
{
    sipLocation_t *locations[SIP_MAX_LOCATIONS];
    uint16_t       num_locations;
    boolean        new_flag;
} sipRecordRoute_t;

typedef enum
{
    sipTransportTypeUDP = 2345,
    sipTransportTypeTCP
} sipTransportType_t;

/* DIVERSION HEADER STRUCTURE */

typedef struct
{
    sipLocation_t *locations;
//  char *reason;
    uint32_t limit;
    uint32_t counter;
    char    *privacy;
    char    *screen;
} sipDiversion_t;

typedef struct
{
    string_t orig_called_name;
    string_t orig_called_number;
    string_t last_redirect_name;
    string_t last_redirect_number;
} sipDiversionInfo_t;


typedef struct
{
    /*
     * We are not storing the protocol ( should be SIP always )
     */
    char *version;
    char *transport;
    char *host;
    char *ttl;
    char *maddr;
    char *recd_host;
    char *branch_param;
    /* Remaining Via header(s) if any in the input string */
    char *more_via;
    uint16_t remote_port;
    uint8_t  flags;
    boolean is_ipv6;
#define VIA_IS_HIDDEN    (0x01)
} sipVia_t;

typedef struct
{
    uint32_t    number;
    sipMethod_t method;
} sipCseq_t;


typedef struct
{
    uint32_t    response_num;
    uint32_t    cseq_num;
    sipMethod_t method;
} sipRack_t;

typedef enum
{
    sipSessionTypeInvalid = 0,
    sipSessionTypeMedia,
    sipSessionTypeQoS,
    sipSessionTypeSecurity
} sipSessionType_t;

/* Data structure for Replaces header */
typedef struct
{
    char *str_start;
    char *callid;
    char *toTag;
    char *fromTag;
    char *signature_scheme;
} sipReplaces_t;

/* Data structure for Refer-To header */
typedef struct
{
    char     *ref_to_start;
    genUrl_t *targetUrl;
    char     *sip_replaces_hdr;
    char     *sip_acc_cont;
    char     *sip_proxy_auth;
} sipReferTo_t;

/* Data structure for Refer method */
typedef struct
{
    sipReferTo_t *refer_to_info;    /* Refer-To header */
    const char   *referred_by_info; /* Referred-by header */
} sipRefer_t;

/* Data structures for Remote-Party-Id */
typedef struct
{
    sipLocation_t *loc;
    char          *screen;
    char          *party_type;
    char          *id_type;
    char          *privacy;
    char          *np;
} sipRemotePartyId_t;

typedef struct
{
    unsigned int        num_rpid; /* Number of rpid instantiations present */
    sipRemotePartyId_t *rpid[MAX_REMOTE_PARTY_ID_HEADERS];
} sipRemotePartyIdInfo_t;

typedef enum
{
    SUBSCRIPTION_STATE_INVALID = 0,
    SUBSCRIPTION_STATE_ACTIVE,
    SUBSCRIPTION_STATE_PENDING,
    SUBSCRIPTION_STATE_TERMINATED
} sip_subs_state_e;

typedef enum
{
    SUBSCRIPTION_STATE_REASON_INVALID = 0,
    SUBSCRIPTION_STATE_REASON_DEACTIVATED,
    SUBSCRIPTION_STATE_REASON_PROBATION,
    SUBSCRIPTION_STATE_REASON_REJECTED,
    SUBSCRIPTION_STATE_REASON_TIMEOUT,
    SUBSCRIPTION_STATE_REASON_GIVEUP,
    SUBSCRIPTION_STATE_REASON_NORESOURCE
} sip_subs_state_reason_e;

typedef struct
{
    sip_subs_state_e        state;
    uint32_t                expires;
    sip_subs_state_reason_e reason;
    uint32_t                retry_after;
} sipSubscriptionStateInfo_t;

typedef enum {
    SERVICE_CONTROL_ACTION_INVALID = 0,
    SERVICE_CONTROL_ACTION_RESET,
    SERVICE_CONTROL_ACTION_RESTART,
    SERVICE_CONTROL_ACTION_CHECK_VERSION,
    SERVICE_CONTROL_ACTION_CALL_PRESERVATION,
    SERVICE_CONTROL_ACTION_APPLY_CONFIG
} sip_service_control_action_e;

typedef struct
{
    sip_service_control_action_e action;
    char *registerCallID;
    char *configVersionStamp;
    char *dialplanVersionStamp;
    char *softkeyVersionStamp;
    char *fcpVersionStamp;
    char *cucm_result;
    char *firmwareLoadId;
    char *firmwareInactiveLoadId;
    char *loadServer;
    char *logServer;
    boolean ppid;
} sipServiceControl_t;

typedef struct
{
    char *call_id;
    char *from_tag;
    char *to_tag;
} sipJoinInfo_t;

typedef enum {
    mwiVoiceType = 1,
    mwiFaxType,
    mwiTextMessage
} messageCountType_t;

typedef struct {
    boolean mesg_waiting_on;
    int32_t type;
    int32_t newCount;
    int32_t oldCount;
    int32_t hpNewCount;
    int32_t hpOldCount;
} sipMessageSummary_t;

/*
 * Returns a sipMethod_t value given a character method name.
 * Returns sipMethodUnknown if the method is not on the list of recognized
 * methods.
 */
PMH_EXTERN sipMethod_t sippmh_get_method_code(const char *);

PMH_EXTERN genUrl_t *sippmh_parse_url(char *url, boolean dup_flag);

/*
 * See comment above.
 */
PMH_EXTERN void sippmh_genurl_free(genUrl_t *);

PMH_EXTERN sipLocation_t *sippmh_parse_from_or_to(char *from, boolean dup_flag);

PMH_EXTERN sipLocation_t *sippmh_parse_nameaddr_or_addrspec(char *input_loc_ptr,
                                                            char *start_ptr,
                                                            boolean dup_flag,
                                                            boolean name_addr_only_flag,
                                                            char **more_ptr);

/*
 * See comment above.
 */
PMH_EXTERN void sippmh_free_location(sipLocation_t *);

/*
 * Same comments as parse_location
 */
PMH_EXTERN sipContact_t *sippmh_parse_contact(const char *contact);


/*
 * Same comments as parse_location
 */
PMH_EXTERN sipDiversion_t *sippmh_parse_diversion(const char *diversion,
                                                  char *diversionhead);


/*
 * Same comments as free_location.
 */
PMH_EXTERN void sippmh_free_diversion(sipDiversion_t *);

PMH_EXTERN void sippmh_free_diversion_info(sipDiversionInfo_t *div_info);

/*
 * Same comments as free_location.
 */
PMH_EXTERN void sippmh_free_contact(sipContact_t *);

/*
 * Same comments as parse_location
 */
PMH_EXTERN sipVia_t *sippmh_parse_via(const char *via);

/*
 *  Same comments as free_location.
 */
PMH_EXTERN void sippmh_free_via(sipVia_t *via);

PMH_EXTERN void sippmh_process_via_header(sipMessage_t *sip_message,
                                          cpr_ip_addr_t *source_ip_address);

/*
 * Same comments as parse_location
 */
PMH_EXTERN sipCseq_t *sippmh_parse_cseq(const char *cseq);

PMH_EXTERN boolean sippmh_parse_rseq(const char *rseq, uint32_t *rseq_val);

PMH_EXTERN sipRack_t *sippmh_parse_rack(const char *rack);

/*
 * Does a proper comparison of From: headers.(kind of like
 * a URL comparison.
 * TRUE if equal, FALSE if not.
 */
PMH_EXTERN boolean sippmh_are_froms_equal(sipFrom_t *from1, sipFrom_t *from2);

/*
 * Does a proper comparison of To: headers.(kind of like
 * a URL comparison.
 * TRUE if equal, FALSE if not.
 * At this time, sipTo is really the sipLocation struct, but
 * this is kept different in case things change.
 */
PMH_EXTERN boolean sippmh_are_tos_equal(sipTo_t *to1, sipTo_t *to2);

/*
 * Does a comparison of SIP URLs.
 * TRUE if equal, FALSE if not.
 */
PMH_EXTERN boolean sippmh_are_urls_equal(sipUrl_t *url1, sipUrl_t *url2);

/*
 * Utility to get the callid of a message.
 * Returns NULL if the header is not found.
 * Returned pointer should not be freed.
 */
//PMH_EXTERN const char *sippmh_get_callid(sipMessage_t *msg);

/*
 * Adds a Cseq: header to the message.
 * msg = sipMessage_t struct
 * method, seq  = method, seq in the Cseq eg Cseq: 428 INVITE
 * Assumes that the maximum length of the resulting header is
 * less than 32 characters.
 */
PMH_EXTERN sipRet_t sippmh_add_cseq(sipMessage_t *msg, const char *method,
                                    uint32_t seq);

PMH_EXTERN sipRet_t sippmh_add_rack(sipMessage_t *msg, uint32_t rseq,
                                    uint32_t cseq, const char *method);

/*
 * Utility to check whether the URL is valid.
 * TRUE if yes, FALSE if not.
 */
PMH_EXTERN boolean sippmh_valid_url(genUrl_t *genUrl);

PMH_EXTERN boolean sippmh_valid_reqline_url(genUrl_t *genUrl);

PMH_EXTERN sipRecordRoute_t *sippmh_parse_record_route(const char *);

PMH_EXTERN sipRecordRoute_t *sippmh_copy_record_route (sipRecordRoute_t *rr);

PMH_EXTERN void sippmh_free_record_route(sipRecordRoute_t *);

PMH_EXTERN cc_content_disposition_t *sippmh_parse_content_disposition(const char *input_content_disp);

PMH_EXTERN sipReferTo_t *sippmh_parse_refer_to(char *ref_to);

PMH_EXTERN sipReplaces_t *sippmh_parse_replaces(char *replcs, boolean dup_flag);

PMH_EXTERN void sippmh_free_replaces(sipReplaces_t *repl);

PMH_EXTERN void sippmh_free_refer_to(sipReferTo_t *ref);

PMH_EXTERN void sippmh_free_refer(sipRefer_t *ref);

PMH_EXTERN void sippmh_convertEscCharToChar(const char *inputStr,
                                            size_t inputStrLen,
                                            char *outputStr);

PMH_EXTERN int sippmh_cmpURLStrings(const char *s1, const char *s2,
                                    boolean ignore_case);

PMH_EXTERN sip_author_t *sippmh_parse_authorization(const char *);

PMH_EXTERN char *sippmh_generate_authorization(sip_author_t *);

PMH_EXTERN void sippmh_free_author(sip_author_t *);

PMH_EXTERN sip_authen_t *sippmh_parse_authenticate(const char *);

PMH_EXTERN void sippmh_free_authen(sip_authen_t *);

PMH_EXTERN sip_author_t *sippmh_parse_proxy_author(const char *);

PMH_EXTERN sip_authen_t *sippmh_parse_proxy_authen(const char *);

PMH_EXTERN sipRemotePartyId_t *sippmh_parse_remote_party_id(
                                              const char *input_remote_party_id);

PMH_EXTERN boolean sippmh_parse_kpml_event_id_params(char *params,
                                                     char **call_id,
                                                     char **from_tag,
                                                     char **to_tag);

PMH_EXTERN void sippmh_free_remote_party_id_info(sipRemotePartyIdInfo_t *rpid_info);

PMH_EXTERN char *sippmh_parse_user(char *url);

PMH_EXTERN int sippmh_parse_subscription_state(sipSubscriptionStateInfo_t *subsStateInfo,
                                               const char *subs_state);

PMH_EXTERN int sippmh_add_subscription_state(sipMessage_t *msg,
                                             sipSubscriptionStateInfo_t *subsStateInfo);

string_t sippmh_parse_displaystr(string_t displaystr);

#define sippmh_parse_to(x) sippmh_parse_location(x)

#define sippmh_free_to(x) sippmh_free_location(x)

#define sippmh_parse_from(x) sippmh_parse_location(x)

#define sippmh_free_from(x) sippmh_free_location(x)

PMH_EXTERN int sippmh_add_call_info(sipMessage_t *sipMessage,
                                    cc_call_info_t *callInfo);

PMH_EXTERN uint32_t sippmh_parse_supported_require(const char *header,
                                                   char **punsupported_options);

PMH_EXTERN uint16_t sippmh_parse_allow_header(const char *header);

PMH_EXTERN uint16_t sippmh_parse_accept_header(const char *header);

PMH_EXTERN sipServiceControl_t 
            *sippmh_parse_service_control_body(char *msgBody, int msgLength);

PMH_EXTERN void sippmh_free_service_control_info(sipServiceControl_t *scp);

PMH_EXTERN sipJoinInfo_t *sippmh_parse_join_header(const char *header);

PMH_EXTERN void sippmh_free_join_info(sipJoinInfo_t *join);

PMH_EXTERN sipRet_t sippmh_add_join_header(sipMessage_t *message,
                                           sipJoinInfo_t *join);

PMH_EXTERN int32_t sippmh_parse_max_forwards(const char *max_fwd_hdr);

PMH_EXTERN string_t sippmh_get_url_from_hdr(char *string);

PMH_EXTERN int32_t sippmh_parse_message_summary(sipMessage_t *pSipMessage, sipMessageSummary_t *mesgSummary);
  
/*
 * The following SIP parser functions are the same as corresponding
 * HTTP/1.1 message parser functions.
 */
#define sippmh_message_create()         httpish_msg_create()

#define sippmh_message_free( x )        httpish_msg_free( x )

#define sippmh_is_request( x )          httpish_msg_is_request( x , SIP_SCHEMA, SIP_SCHEMA_LEN)

#define sippmh_is_message_complete( x ) httpish_msg_is_complete( x )

#define sippmh_get_request_line( x )    httpish_msg_get_reqline( x )

#define sippmh_get_response_line( x )   httpish_msg_get_respline( x )

#define sippmh_free_request_line( x )   httpish_msg_free_reqline( x )

#define sippmh_free_response_line( x )  httpish_msg_free_respline( x )

#define sippmh_process_network_message( x, y, z) \
        httpish_msg_process_network_msg( x, y, z)

#define sippmh_get_code_class( x ) httpish_msg_get_code_class( x )

#define sippmh_add_request_line( x, y, a, b ) \
        httpish_msg_add_reqline( x, y, a, b )

#define sippmh_add_response_line( x, y , a, b) \
        httpish_msg_add_respline( x, y , a, b)

#define sippmh_add_text_header( x, y, z ) \
        httpish_msg_add_text_header( x, y, z )

#define sippmh_add_int_header( x, y, z ) \
        httpish_msg_add_int_header( x, y, z )

#define sippmh_add_message_body( x, y, z, a, b, c, d) \
        httpish_msg_add_body(x, y, z, a, b, c, d)

#define sippmh_remove_header( x, y ) \
        httpish_msg_remove_header( x, y )

#define sippmh_msg_header_present( x, y ) \
        httpish_msg_header_present(x, y)

#define sippmh_get_header_val( x, y, c_y ) \
        httpish_msg_get_header_val( x, y, c_y )

#define sippmh_get_cached_header_val(x, y) \
        httpish_msg_get_cached_header_val(x, y)

#define sippmh_get_content_length( x ) \
        httpish_msg_get_content_length( x )

#define sippmh_get_all_headers( x, y ) \
        httpish_msg_get_all_headers( x, y )

#define sippmh_write_to_buf( x, y ) \
        httpish_msg_write_to_buf( x, y )

#define sippmh_write( x, y, z ) \
        httpish_msg_write( x, y, z )

#define sippmh_write_to_string( x ) \
        httpish_msg_write_to_string( x )

#define sippmh_get_num_headers( x ) \
        httpish_msg_get_num_headers( x )

#define sippmh_get_num_particular_headers( a, b, c, d, e ) \
        httpish_msg_get_num_particular_headers( a, b, c, d, e )

#define sippmh_get_header_vals( a, b, c, d, e) \
        httpish_msg_get_header_vals( a, b, c, d, e)


#define SIPPMH_VERSIONS_EQUAL(a, b) \
        (cpr_strcasecmp(a, b) == 0)

/*
 * Utility to compare From: header values, given the values rather
 * than the sipFrom_t struct.
 * Returns TRUE if the same, FALSE if not.
 * Expects NULL terminated strings.
 */
PMH_EXTERN boolean sippmh_compare_fromto(const char *a1, const char *a2);

/*
 * Same as compare_fromto, but for URL strings.
 */
PMH_EXTERN boolean sippmh_compare_urls(const char *u1, const char *u2);

#define PARSE_ERR_NON_SIP_URL        1
#define ERROR_1                      SIP_F_PREFIX"Non-SIP URL\n"

#define PARSE_ERR_NO_MEMORY          2
#define ERROR_2                      SIP_F_PREFIX"Out of memory\n"

#define PARSE_ERR_SYNTAX             3
#define ERROR_3                      SIP_F_PREFIX"Syntax error at %s\n"
#define ERROR_3_1                    SIP_F_PREFIX"Unexpected char %c\n"

#define PARSE_ERR_UNEXPECTED_EOS     4
#define ERROR_4                      SIP_F_PREFIX"Unexpected end of string\n"

#define PARSE_ERR_UNTERMINATED_STRING     5
#define ERROR_5                      SIP_F_PREFIX"Unmatched \"\n"

#define PARSE_ERR_UNMATCHED_BRACKET     6
#define ERROR_6                      SIP_F_PREFIX"Unmatched <>\n"

#define PARSE_ERR_INVALID_TTL_VAL       7
#define ERROR_7                      SIP_F_PREFIX"%d: Invalid ttl value(range 0-255)\n"

#define PARSE_ERR_NULL_PTR              8
#define ERROR_8                      SIP_F_PREFIX"NULL Pointer\n"

#define AT_SIGN                   '@'
#define SEMI_COLON                ';'
#define COLON                     ':'
#define EQUAL_SIGN                '='
#define ESCAPE_CHAR               '\\'
#define TILDA                     '~'
#define PERCENT                   '%'
#define STAR                      '*'
#define UNDERSCORE                '_'
#define PLUS                      '+'
#define SINGLE_QUOTE              '\''
#define DOUBLE_QUOTE              '"'
#define LEFT_ANGULAR_BRACKET      '<'
#define RIGHT_ANGULAR_BRACKET     '>'
#define LEFT_SQUARE_BRACKET       '['
#define RIGHT_SQUARE_BRACKET      ']'
#define LEFT_PARENTHESIS          '('
#define RIGHT_PARENTHESIS         ')'
#define DOLLAR_SIGN               '$'
#define FORWARD_SLASH             '/'
#define DOT                       '.'
#define DASH                      '-'
#define EXCLAMATION               '!'
#define AMPERSAND                 '&'
#define COMMA                     ','
#define QUESTION_MARK             '?'
#define SPACE                     ' '
#define TAB                       '\t'
#define NUMBER_SIGN               '#'
#define LEFT_CURLY_BRACE          '{'
#define RIGHT_CURLY_BRACE         '}'
#define OR_SIGN                   '|'
#define CARET                     '^'
#define OPENING_SINGLE_QUOTE      '`'

#define PROXY                     1
#define REDIRECT                  2
#define TRANSPORT_UNSPECIFIED     0
#define TRANSPORT_UDP             1
#define TRANSPORT_TCP             2
#define TRANSPORT_TLS             3
#define TRANSPORT_SCTP            4

#define MAX_TTL_VAL               255

#define SIPPMH_FREE_REQUEST_LINE(x)  sippmh_free_request_line(x); UTILFREE(x)
#define SIPPMH_FREE_RESPONSE_LINE(x) sippmh_free_response_line(x); UTILFREE(x)
#define SIPPMH_FREE_URL(x)           sippmh_genurl_free(x); UTILFREE(x)
#define SIPPMH_FREE_MESSAGE(x)       sippmh_free_message(x) ; UTILFREE(x)

size_t sippmh_convertURLCharToEscChar(const char *inputStr, size_t inputStrLen,
                                      char *outputStr, size_t outputStrSize,
                                      boolean null_terminate);

size_t sippmh_converQuotedStrToEscStr(const char *inputStr, size_t inputStrLen,
                         char *outputStr, size_t outputStrSize,
                         boolean null_terminate);

ccsipRet_e ccsip_process_network_message(sipMessage_t **sipmsg_p, char **buf,
                                         unsigned long *nbytes_used,
                                         char **display_msg);


#endif /* _CCSIP_PMH_H_ */
