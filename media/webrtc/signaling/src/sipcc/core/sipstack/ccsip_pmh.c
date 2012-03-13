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

/*
 * Routines that parse SIP messages into individual components
 * defined in ccsip_pmh.h. Used by the code that receives the
 * messages to decipher them and then effect callstate as
 * appropriate.
 * Some SIP messages are exactly the same as HTTP/1.1 messages,
 * and these are parsed using wrapper functions to those in
 * httpish.c
 */
#include "cpr_types.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "cpr_string.h"
#include "cpr_in.h"
#include "cpr_memory.h"
#include "ccsip_pmh.h"
#include "phone_debug.h"
#include "ccapi.h"
#include "text_strings.h"
#include "util_string.h"
#include "ccsip_spi_utils.h"
#include "ccsip_callinfo.h"
#include "ccsip_core.h"

/* Skip linear whitespace */
#define SKIP_LWS(p)    while (*p == SPACE || *p == TAB) { \
                           p++; \
                       }

#define SKIP_WHITE_SPACE(p)    while (*p == SPACE || *p == TAB || \
                                      *p == '\n') { \
                                      p++; \
                               }

#define AUTHENTICATION_BASIC      "Basic"
#define AUTHENTICATION_DIGEST     "Digest"
#define AUTHENTICATION_REALM      "realm"
#define AUTHENTICATION_NONCE      "nonce"
#define AUTHENTICATION_URI        "uri"
#define AUTHENTICATION_DOMAIN     "domain"
#define AUTHENTICATION_ALGORITHM  "algorithm"
#define AUTHENTICATION_OPAQUE     "opaque"
#define AUTHENTICATION_USERNAME   "username"
#define AUTHENTICATION_RESPONSE   "response"


/* Trim trailing space from end of string */
static void
trim_right (char *pstr)
{
    char *pend;

    if (!pstr) {
        return;
    }

    pend = (pstr + (strlen(pstr) - 1));
    while (pend > pstr) {
        if (!isspace((int) *pend)) {
            break;
        }
        *pend = '\0';
        pend--;
    }
}

/*
 *  Remove square brackets from IPv6 address
 *
 *  @param pstr  address string
 *
 *  @return  none
 *  
 *  @pre     none
 *
 */

static void trim_ipv6_host(char *pstr)
{
    if (!pstr) {
        return;
    }

    if (*pstr == '[') {
        pstr++;
    }

    if (*(pstr + strlen(pstr)-1) == ']') {
        *(pstr + strlen(pstr)-1) = '\0';
    }
}

int parse_errno = 0;

/* The order of these error messages corresponds to parse error codes
 * defined in ccsip_pmh.h
 */
static char *parse_errors[] = { "",
    ERROR_1,
    ERROR_2,
    ERROR_3,
    ERROR_4,
    ERROR_5,
    ERROR_6,
    ERROR_7,
    ERROR_8
};

/*
 * These headers are defined as per the values of Header indexes ( in
 * ccsip_protocol.h. To include a header in the cached list, increase
 * the HEADER_CACHE_SIZE in httpish.h and add its entry here. Its index
 * should be consistent with the index value in ccsip_protocol.h.
 * For example, #define VIA 2, implies sip_cached_headers[2] has Via header
 * related stuff.
 */
sip_header_t sip_cached_headers[] = {
/* 0 */  {"From", "f"},
/* 1 */  {"To", "t"},
/* 2 */  {HTTPISH_HEADER_VIA, "v"},
/* 3 */  {"Call-ID", "i"},
/* 4 */  {"CSeq", NULL},
/* 5 */  {"Contact", "m"},
/* 6 */  {HTTPISH_HEADER_CONTENT_LENGTH, HTTPISH_C_HEADER_CONTENT_LENGTH},
/* 7 */  {HTTPISH_HEADER_CONTENT_TYPE, "c"},
/* 8 */  {"Record-Route", NULL},
/* 9 */  {"Require", NULL},
/* 10 */ {"Route", NULL},
/* 11 */ {"Supported", "k"}
};

static boolean
is_dtmf_or_pause (char *digit)
{
    if (!(*digit)) {
        return FALSE;
    }
    switch (*digit) {
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case '*':
    case '#':
    case 'p':
    case 'w':
        return TRUE;
    default:
        return FALSE;
    }
}

/* Inefficient ? Somewhat... */
sipMethod_t
sippmh_get_method_code (const char *method)
{
    sipMethod_t ret = sipMethodInvalid;

    if (method) {

        ret = sipMethodUnknown;

        if (strcmp(method, SIP_METHOD_INVITE) == 0)
            ret = sipMethodInvite;
        else if (strcmp(method, SIP_METHOD_BYE) == 0)
            ret = sipMethodBye;
        else if (strcmp(method, SIP_METHOD_ACK) == 0)
            ret = sipMethodAck;
        else if (strcmp(method, SIP_METHOD_PRACK) == 0)
            ret = sipMethodPrack;
        else if (strcmp(method, SIP_METHOD_COMET) == 0)
            ret = sipMethodComet;
        else if (strcmp(method, SIP_METHOD_OPTIONS) == 0)
            ret = sipMethodOptions;
        else if (strcmp(method, SIP_METHOD_CANCEL) == 0)
            ret = sipMethodCancel;
        else if (strcmp(method, SIP_METHOD_NOTIFY) == 0)
            ret = sipMethodNotify;
        else if (strcmp(method, SIP_METHOD_REFER) == 0)
            ret = sipMethodRefer;
        else if (strcmp(method, SIP_METHOD_SUBSCRIBE) == 0)
            ret = sipMethodSubscribe;
        else if (strcmp(method, SIP_METHOD_REGISTER) == 0)
            ret = sipMethodRegister;
        else if (strcmp(method, SIP_METHOD_UPDATE) == 0)
            ret = sipMethodUpdate;
        else if (strcmp(method, SIP_METHOD_INFO) == 0)
            ret = sipMethodInfo;
        else if (strcmp(method, SIP_METHOD_PUBLISH) == 0)
            ret = sipMethodPublish;
        else if (strcmp(method, SIP_METHOD_MESSAGE) == 0)
            ret = sipMethodMessage;
        else if (strcmp(method, SIP_METHOD_INFO) == 0)
            ret = sipMethodInfo;
    }

    return ret;
}

#define SKIP_SIP_TOKEN(inp_str) \
    while (isalnum((int)*inp_str) || *inp_str == DASH || \
           *inp_str == DOT || *inp_str == EXCLAMATION || \
           *inp_str == PERCENT || *inp_str == STAR || \
           *inp_str == UNDERSCORE || *inp_str == PLUS || \
           *inp_str == '`' || *inp_str == SINGLE_QUOTE || \
           *inp_str == COLON || *inp_str == TILDA || \
           *inp_str == AT_SIGN) { \
        inp_str++; \
    }

static char *
parse_generic_param (char *param, char **param_val)
{
    boolean match_found;
    char   *tok_start;

    /*
     * param is pointing to the first character after the name of the
     * parameter. It could be
     * white space   or
     * equal sign    or
     * semi-colon    or
     * comma         or
     * end of string
     */
    if (*param == SPACE || *param == TAB) {
        *param++ = 0;
        SKIP_LWS(param);
    }

    if (*param != EQUAL_SIGN) {
        /*
         * Parameter exists but its value is empty
         */
        *param_val = "";
        return param;
    }

    *param++ = 0;
    SKIP_LWS(param);
    *param_val = param;
    if (*param == DOUBLE_QUOTE) {
        param++;
        match_found = FALSE;
        while (*param) {
            if (*param == DOUBLE_QUOTE && *(param - 1) != ESCAPE_CHAR) {
                param++;
                match_found = TRUE;
                break;
            }
            param++;
        }
        if (match_found == FALSE) {
            return NULL;
        }
    } else {
        tok_start = param;
        SKIP_SIP_TOKEN(param);
        if (param == tok_start) {
            return NULL;
        }
    }
    return param;
}

/*
 * The syntax of other-param in SIP URL is same as extension-attribute in
 * Contact header. A pointer to the start of the token=value or
 * token="value" string is returned in other_param_value. This allows
 * the calling code to store this value if needed such as in the
 * case of supporting contact-extensions. The function returns
 * a pointer to the next character to be parsed.
 */
static char *
parse_other_param (char *inp_str, char **other_param_value)
{
    char  temp;
    char *token_string;


    SKIP_LWS(inp_str);
    token_string = inp_str;
    SKIP_SIP_TOKEN(inp_str);
    switch (*inp_str) {
    case COMMA:
    case '\0':
    case SEMI_COLON:
        /*
         * Either we have encountered end of string or another
         * parameter follows this other-param. In either case we
         * are done with parsing other-param. Since this is
         * not in the form of token=value or token="value" discard
         * it.
         */
        *other_param_value = NULL;

        /* check for loose routing specifier */
        temp = *inp_str;
        *inp_str = 0;
        if (!cpr_strcasecmp(token_string, "lr")) {
            *other_param_value = (char *) cpr_malloc(4);
            if (*other_param_value != NULL) {
                strcpy(*other_param_value, token_string);
            }
        }
        *inp_str = temp;
        break;

    case EQUAL_SIGN:
        /* It is either token=token form or token=quoted-string form */
        inp_str++;
        if (*inp_str == DOUBLE_QUOTE) {
            /* quoted-string follows */
            inp_str++; /* skip the " char */
            while (*inp_str) {
                if (*inp_str == DOUBLE_QUOTE && *(inp_str - 1) != ESCAPE_CHAR) {
                    inp_str++;
                    break;
                }
                inp_str++;
            }
        } else {
            /* token follows */
            // SKIP_SIP_TOKEN(inp_str);
            // Continue until another semicolon is found or end of string is
            // reached
            while (*inp_str != '\0' && *inp_str != SEMI_COLON) {
                inp_str++;
            }
        }

        /*
         * Terminate the string that token_string is pointing
         * to while keeping the value of *inp_str unchanged.
         * Then assign the null terminated string to other_param_value
         */
        *other_param_value = (char *) cpr_malloc(SIP_MAX_OTHER_PARAM_LENGTH + 1 * sizeof(char));
        if (*other_param_value != NULL) {
            temp = *inp_str;
            *inp_str = 0;
            sstrncpy(*other_param_value, token_string, SIP_MAX_OTHER_PARAM_LENGTH);
            *inp_str = temp;
        }
        break;

    default:
        CCSIP_DEBUG_ERROR(ERROR_3, "parse_other_param", inp_str);
        *other_param_value = NULL;
        return NULL;
    }
    return inp_str;
}

/*
 * This function creates an array of url headers. Each
 * element of the array has two member pointers:
 *      char pointer to header name,
 *      char pointer to header value
 */
static int
url_add_headers_to_list (char *url_strp, sipUrl_t *sip_url)
{
    char *tmp_ptr = NULL;
    char  num_head = 1;
    char *lasts = NULL;

    if (!url_strp) {
        return (PARSE_ERR_NULL_PTR);
    }

    tmp_ptr = strchr(url_strp, AMPERSAND);
    while (tmp_ptr != NULL) {
        num_head++;
        tmp_ptr++;
        tmp_ptr = strchr(tmp_ptr, AMPERSAND);
    }

    sip_url->headerp = (attr_value_pair_t *)
        cpr_malloc(sizeof(attr_value_pair_t) * num_head);
    if (!sip_url->headerp) {
        return (PARSE_ERR_NO_MEMORY);
    }

    sip_url->num_headers = num_head;
    num_head = 0;
    url_strp = cpr_strtok(url_strp, "&?", &lasts);  
    while ((url_strp != NULL) && (num_head < sip_url->num_headers)) {

        tmp_ptr = strchr(url_strp, EQUAL_SIGN);
        if (!tmp_ptr) {
            return (PARSE_ERR_SYNTAX);
        }
        *tmp_ptr++ = 0;

        sip_url->headerp[(uint16_t)num_head].attr  = url_strp;
        sip_url->headerp[(uint16_t)num_head].value = tmp_ptr;

        num_head++;
        url_strp = cpr_strtok(NULL, "&", &lasts);
    }

    return 0;
}

static int
parseUrlParams (char *url_param, sipUrl_t *sipUrl, genUrl_t *genUrl)
{
    static const char fname[] = "parseUrlParams";
    char *param_val;
    char *url_other_param = NULL;
    uint16_t i;
    uint32_t ttl_val;


    /*
     * url-parameters  = *( ";" url-parameter )
     * url-parameter   = transport-param | user-param | method-param
     *            | ttl-param | maddr-param | other-param
     * transport-param = "transport=" ( "udp" | "tcp" | "sctp" | "tls"
     *                                   | other-transport )
     * ttl-param       = "ttl=" ttl
     * ttl             = 1*3DIGIT       ; 0 to 255
     * maddr-param     = "maddr=" host
     * user-param      = "user=" ( "phone" | "ip" )
     * method-param    = "method=" Method
     * tag-param       = "tag=" UUID
     * lr-param        = "lr"
     * UUID            = 1*( hex | "-" )
     * other-param     = ( token | ( token "=" ( token | quoted-string )))
     * other-transport = token
     */

    /*
     * url_param is pointing to the first character after the ';'
     * This routine only prints error message for SYNTAX error, since we
     * want to pinpoint the error location. For other errors it simply
     * returns the error code.
     */
    SKIP_LWS(url_param);
    if (*url_param == 0) {
        return PARSE_ERR_UNEXPECTED_EOS;
    }
    while (1) {
        if (cpr_strncasecmp(url_param, "transport=", 10) == 0) {
            url_param += 10;
            SKIP_LWS(url_param);
            if (cpr_strncasecmp(url_param, "tcp", 3) == 0) {
                sipUrl->transport = TRANSPORT_TCP;
                url_param += 3;
            } else if (cpr_strncasecmp(url_param, "tls", 3) == 0) {
                sipUrl->transport = TRANSPORT_TLS;
                url_param += 3;
            } else if (cpr_strncasecmp(url_param, "sctp", 4) == 0) {
                sipUrl->transport = TRANSPORT_SCTP;
                url_param += 4;
            } else if (cpr_strncasecmp(url_param, "udp", 3) == 0) {
                sipUrl->transport = TRANSPORT_UDP;
                url_param += 3;
            } else {
                SKIP_SIP_TOKEN(url_param);
                SKIP_LWS(url_param);
                if (*url_param != SEMI_COLON && *url_param != 0) {
                    CCSIP_DEBUG_ERROR(ERROR_3, fname, url_param);                    
                    return PARSE_ERR_SYNTAX;
                }
            }
        } else if (cpr_strncasecmp(url_param, "user=", 5) == 0) {
            url_param += 5;
            SKIP_LWS(url_param);
            if (cpr_strncasecmp(url_param, "phone", 5) == 0) {
                sipUrl->is_phone = 1;
                url_param += 5;
            } else if (cpr_strncasecmp(url_param, "ip", 2) == 0) {
                url_param += 2;
            } else {
                CCSIP_DEBUG_ERROR(ERROR_3, fname, url_param);                
                return PARSE_ERR_SYNTAX;
            }
        } else if (cpr_strncasecmp(url_param, "method=", 7) == 0) {
            url_param += 7;
            SKIP_LWS(url_param);
            param_val = url_param;
            while (isalpha((int) *url_param)) {
                url_param++;
            }
            if (param_val == url_param) {
                return PARSE_ERR_UNEXPECTED_EOS;
            }
            sipUrl->method = param_val;
            /* Note that we have not terminated the method str yet */
        } else if (cpr_strncasecmp(url_param, "ttl=", 4) == 0) {
            char save_ch;

            url_param += 4;
            SKIP_LWS(url_param);
            param_val = url_param;
            while (isdigit((int) *url_param)) {
                url_param++;
                /* Atmost 3 digits allowed in ttl value */
                if ((url_param - param_val) > 3) {
                    CCSIP_DEBUG_ERROR(ERROR_3, fname, url_param);                    
                    return PARSE_ERR_SYNTAX;
                }
            }
            if (url_param == param_val) {
                /* Did not find any digit after "ttl=" */
                return PARSE_ERR_UNEXPECTED_EOS;
            }
            save_ch = *url_param;
            *url_param = 0;  /* Terminate the string for atoi */
            ttl_val = (uint32_t) atoi(param_val);
            if (ttl_val > MAX_TTL_VAL) {
                CCSIP_DEBUG_ERROR(ERROR_7, fname, sipUrl->ttl_val);
                return PARSE_ERR_INVALID_TTL_VAL;
            }
            sipUrl->ttl_val = (unsigned char) ttl_val;
            *url_param = save_ch;  /* Restore string state */
        } else if (cpr_strncasecmp(url_param, "maddr=", 6) == 0) {
            url_param += 6;
            SKIP_LWS(url_param);
            param_val = url_param; /* maddr now points to a host */
            while (isalnum((int) *url_param) || *url_param == DOT ||
                   *url_param == DASH) {
                url_param++;
            }
            if (url_param == param_val) {
                /* Empty value of maddr */
                return PARSE_ERR_UNEXPECTED_EOS;
            }
            sipUrl->maddr = param_val;
        } else if (cpr_strncasecmp(url_param, "phone-context=", 14) == 0) {
            url_param += 14;
            SKIP_LWS(url_param);
            param_val = url_param;

            while (isalpha((int) *url_param)) {
                url_param++;
            }
            if (param_val == url_param) {
                return PARSE_ERR_SYNTAX;
            }

            /* If the next character is a space, we want to zero the
             * string, otherwise the next character is EOF or ; in
             * which case the code below will handle these cases.
             */
            if (isspace((int) *url_param)) {
                *url_param++ = 0;
            }
            genUrl->phone_context = param_val;
        } else if (cpr_strncasecmp(url_param, "lr", 2) == 0) {
            // skip to the following SEMI_COLON or end
            while (*url_param && *url_param != SEMI_COLON) {
                url_param++;
            }
            sipUrl->lr_flag = TRUE;
        } else {
            /* other-param */
            url_param = parse_other_param(url_param, &url_other_param);

            if (url_param == NULL) {
                return PARSE_ERR_SYNTAX;
            }

            if (url_other_param != NULL) {
                /* Store it in first free slot */
                i = 0;
                while (i < SIP_MAX_LOCATIONS) {
                    if (genUrl->other_params[i] == NULL) {
                        break;
                    }
                    i++;
                }

                if (i == SIP_MAX_LOCATIONS) {
                    cpr_free(url_other_param);
                    CCSIP_DEBUG_ERROR(SIP_F_PREFIX"parseUrlParams: Too many unknown parameters"
                               " in URL of Contact header", fname);                    
                } else {
                    genUrl->other_params[i] = url_other_param;
                    url_other_param = NULL;
                }
            }
        }

        SKIP_LWS(url_param);
        /* We either expect - end of string or ';' */
        if (*url_param == 0) {
            return 0;
        }
        if (*url_param != SEMI_COLON) {
            CCSIP_DEBUG_ERROR(ERROR_3, fname, url_param);            
            return PARSE_ERR_SYNTAX;
        }
        *url_param++ = 0; /* Zero the ';' and advance pointer */
        SKIP_LWS(url_param);
        if (*url_param == 0) {
            return PARSE_ERR_UNEXPECTED_EOS;
        }
    }
}




/*
 * This routine is based on a very simple and valid assumption. SIP URL can be
 * of the following forms (before the parameters, which start with ';')
 * My understanding is that it is NOT a context-free grammar
 * sip:host
 * sip:host:port
 * sip:user@host
 * sip:user@host:port
 * sip:user:password@host
 * sip:user:password@host:port
 * sip:user:password@[ipv6host]:port
 *
 * Parse the SIP URL and zero the separators ( @ : ; etc)
 * Point fields of sipUrl to appropriate places in the duplicated string
 * This saves multiple mallocs for different fields of the sipUrl
 * For a SIP URL of the form sip:user:password@host:port;params...
 * loc_ptr[0] = Beginning of user part
 * loc_ptr[1] = Beginning of password
 * loc_ptr[2] = Beginning of host
 * loc_ptr[3] = Beginning of port
 * loc_ptr[4] = Beginning of parameters
 */
static int
parseSipUrl (char *url_start, genUrl_t *genUrl)
{
    static const char fname[] = "parseSipUrl";
    sipUrl_t *sipUrl = genUrl->u.sipUrl;
    char ch = 0;
    int token_cnt, separator_cnt;
    char *url_main;
    char *tokens[4];
    char separator[4];
    char *endptr;
    uint16_t port;
    boolean parsing_user_part;
    char *temp_url;
    boolean ipv6_addr = FALSE;

    //strcpy(url_start, "1218@[2001:db8:c18:1:211:11ff:feb1:fb65]");

    /* initializing separator */
    separator[0] = '\0';

    /*
     * SIP-URL         = "sip:" [ userinfo "@" ] hostport
     *              url-parameters [ headers ]
     * userinfo        = user [ ":" password ]
     * user            = *( unreserved | escaped
     *            | "&" | "=" | "+" | "$" | "," )
     * password        = *( unreserved | escaped
     *            | "&" | "=" | "+" | "$" | "," )
     * hostport        = host [ ":" port ]
     * host            = hostname | IPv4address | IPv6reference
     * IPv6reference = "[" IPv6address "]"
     * IPv6address = hexpart [ ":" IPv4address ]
     * hexpart = hexseq / hexseq "::" [ hexseq ] / "::" [ hexseq ]
     * hexseq = hex4 *( ":" hex4)
     * hex4 = 1*4HEXDIG
     * port = 1*DIGIT
     * hostname        = *( domainlabel "." ) toplabel [ "." ]
     * domainlabel     = alphanum | alphanum *( alphanum | "-" ) alphanum
     * toplabel        = alpha | alpha *( alphanum | "-" ) alphanum
     * IPv4address     = 1*digit "." 1*digit "." 1*digit "." 1*digit
     * port            = *digit
     */

    /* Ignore any leading white spaces too, */
    SKIP_LWS(url_start);

    /* we are pointing at whatever is after "sip:" */
    url_main = url_start;

    tokens[0] = url_main;
    token_cnt = 0;
    separator_cnt = 0;

    /* Scan the string for : and @ and store their pointers. Terminate
     * the search when you reach end of string or beginning of parameters
     * indicated by ;. However the bis02 version of the spec allows ; in
     * the user part of the URL. Thus the code must differentiate between
     * a ; in the user part and a ; indicating the start of URL params.
     */
    while (1) {
        if (*url_main == 0 || *url_main == SEMI_COLON ||
            *url_main == QUESTION_MARK) {
            /* Either end of string or beginning of SIP URL parameters
             * or a ; encountered in the user part
             */
            parsing_user_part = FALSE;
            temp_url = url_main;
            while (*temp_url != 0) {
                if (*temp_url == AT_SIGN) {
                    parsing_user_part = TRUE;
                    break;
                } else if (*temp_url == QUESTION_MARK) {
                    /* overloaded headers in url */
                    break;
                }
                temp_url++;
            }
            temp_url = NULL;

            if (!parsing_user_part) {
                ch = *url_main;
                *url_main++ = 0;  /* Terminate the current token */
                token_cnt++;
                break;
            }
        }

        /* For IPv6 address colon present so skip that */
        if ((*url_main == COLON && ipv6_addr == FALSE)|| *url_main == AT_SIGN || *url_main == SPACE ||
            *url_main == TAB) {

            if (*url_main == SPACE || *url_main == TAB) {
                *url_main++ = 0;  /* Terminate current token */
                SKIP_LWS(url_main);
            }
            /*
             * Now we should be pointing to end of string or a separator
             * (even ';' - beginning of parameters
             */
            if (*url_main == COLON || *url_main == AT_SIGN) {
                if (separator_cnt == 3) {
                    /* Too many separators */
                    CCSIP_DEBUG_ERROR(ERROR_3_1, fname, *url_main);                    
                    return PARSE_ERR_SYNTAX;
                }
                separator[separator_cnt++] = *url_main;
                *url_main++ = 0;  /* Zero the separator */
                SKIP_LWS(url_main);
                
                if (*url_main == LEFT_SQUARE_BRACKET) {
                    *url_main++ = 0; /* Must be IPv6 address */
                    ipv6_addr = TRUE;
                }
                tokens[++token_cnt] = url_main;
                if (*url_main == 0) {
                    break;
                }
            }

        } else if (*url_main == RIGHT_SQUARE_BRACKET && ipv6_addr == TRUE) {

            *url_main++ = 0; /* Found complete IPv6 address*/
        } else {
            url_main++;
        }
    }

    sipUrl->port = SIP_WELL_KNOWN_PORT;
    sipUrl->port_present = FALSE;

    /* token_cnt contains the number of entries in the tokens array */
    switch (token_cnt) {

    case 1:
        /* sip:host */
        sipUrl->host = tokens[0];
        break;

    case 2:
        if (separator[0] == AT_SIGN) {
            /* sip:user@host */
            sipUrl->user = tokens[0];
            sipUrl->host = tokens[1];
            sipUrl->is_ipv6 = ipv6_addr;
        } else {
            /* sip:host:port */
            sipUrl->host = tokens[0];
            port = (uint16_t) strtol(tokens[1], &endptr, 10);
            if (*endptr == 0) {
                sipUrl->port = port;
                sipUrl->port_present = TRUE;
            } else {
                sipUrl->port = 0;
            }
        }
        break;

    case 3:
        if (separator[0] == separator[1]) {
            /* Cannot have 2 successive entries of : or @ */
            CCSIP_DEBUG_ERROR(ERROR_3_1, fname, separator[1]);            
            return PARSE_ERR_SYNTAX;
        }
        if (separator[0] == AT_SIGN) {
            /* sip:user@host:port */
            sipUrl->user = tokens[0];
            sipUrl->host = tokens[1];
            port = (uint16_t) strtol(tokens[2], &endptr, 10);
            if (*endptr == 0) {
                sipUrl->port = port;
                sipUrl->port_present = TRUE;
            } else {
                sipUrl->port = 0;
            }
        } else {
            /* sip:user:password@host */
            sipUrl->user = tokens[0];
            sipUrl->password = tokens[1];
            sipUrl->host = tokens[2];
        }
        break;

    case 4:
        /* sip:user:password@host:port */

        if (separator[0] == COLON && separator[1] == AT_SIGN &&
            separator[2] == COLON) {
            sipUrl->user = tokens[0];
            sipUrl->password = tokens[1];
            sipUrl->host = tokens[2];
            port = (uint16_t) strtol(tokens[3], &endptr, 10);
            if (*endptr == 0) {
                sipUrl->port = port;
                sipUrl->port_present = TRUE;
            } else {
                sipUrl->port = 0;
            }
        } else {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Bad separator sequence", fname);            
            return PARSE_ERR_SYNTAX;
        }
        break;

    }

    /* Remove [ and ] from IPv6 address*/
    trim_ipv6_host(sipUrl->host);

    /* Verify the Port */
    if (sipUrl->port == 0) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Received Bad Port", fname);        
        return PARSE_ERR_SYNTAX;
    }

    /* Verify the host portion */
    if (sipSPI_validate_ip_addr_name(sipUrl->host) == FALSE) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Received Bad Host", fname);        
        return PARSE_ERR_SYNTAX;
    }

    /* Set default transport to UNSPECIFIED */
    sipUrl->transport = TRANSPORT_UNSPECIFIED;
    if (ch == SEMI_COLON) { /* Process URL parameters */
        return parseUrlParams(url_main, sipUrl, genUrl);
    } else if (ch == QUESTION_MARK) { /* Process URL headers */
        return url_add_headers_to_list(url_main, sipUrl);
    }
    return 0;  /* SUCCESS */
}

/*
 * This routine is based on a very simple and valid assumption. TEL URL can be
 * of the following forms (before the parameters, which start with ';')
 * tel:user
 *
 * Parse the TEL URL and zero the separators (;)
 * Point fields of sipUrl to appropriate places in the duplicated string
 * This saves multiple mallocs for different fields of the telUrl
 * For a TEL URL of the form tel:user;params...
 */
static int
parseTelUrl (char *url_start, genUrl_t *genUrl)
{
    static const char fname[] = "parseTelUrl";
    char *url_main;
    telUrl_t *telUrl = genUrl->u.telUrl;
    boolean is_local_subscriber = FALSE;

    /*
     * TEL-URL               =  telephone-scheme ":" telephone-subscriber
     * telephone-scheme      =  "tel"
     * telephone-subscriber  =  global-phone-number/local-phone-number
     * global-phone-number   =  "+" base-phone-number [isdn-subaddress]
     *                          [post-dial] *(area-specifier/service-provider/
     *                          future-extension)
     * base-phone-number     =  1*phonedigit
     * local-phone-number    =  1*(phonedigit / dtmf-digit /pause-character)
     *                          [isdn-subaddress] [post-dial] area-specifier
     *                          *(are-specifier/service-provider/
     *                          future-extension)
     * isdn-subaddress       =  ";isub=" 1*phonedigit
     * post-dial             =  ";postd=" 1*(phonedigit / dtmf-digit /
     *                          pause-character)
     * area-specifier        =  ";" phone-context-tag "=" phone-context-ident
     * phone-context-tag     =  "phone-context"
     * phone-context-ident   =  network-prefix / private-prefix
     * network-prefix        =  global-network-prefix / local-network-prefix
     * global-network-prefix =  "+" 1*phonedigit
     * local-network-prefix  =  1*(phonedigit / dtmf-digit / pause-character)
     * private-prefix        =  (%x21-22 / %x24-27 / %x2C / %x2F / %x3A /
     *                           %x3C-40 /%x45-4F / %x51-56 / %x58-60 /
     *                           %x65-6F / %x71-76 / %x78-7E /) *(%x21-3A /
     *                           %x3C-7E)
     * service-provider      =  ";" provider-tag "=" provider-hostname
     * provider-tag          =  "tsp"
     * provider-hostname     =  domain ; <domain> is defined in [RFC1035]
     * future-extension      =  ";" token ["=" token]
     * phonedigit            =  DIGIT / visual-separator
     * visual-separator      =  "-" / "." / "(" / ")"
     * pause-character       =  "p" / "w"
     * dtmf-digit            =  "*" / "#" / "A" / "B" / "C" / "D"
     */

    if (!url_start) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Missing user field", fname);        
        return PARSE_ERR_SYNTAX;
    }

    url_main = url_start;
    SKIP_LWS(url_main);
    /*
     * User field is the only mandatory field.  The rest are optional
     */

    if (*url_main == PLUS) {
        /* global-phone number */
        url_main++;
        telUrl->user = url_main;

        while ((url_main) && ((*url_main == DASH) ||
               (isdigit((int)*url_main)) || (*url_main == DOT))) {
            url_main++;
        }
    } else {
        /* local-phone-number */
        telUrl->user = url_main;
        is_local_subscriber = TRUE;

        while ((url_main) && ((is_dtmf_or_pause(url_main)) ||
               (*url_main == DASH) || (isdigit((int)*url_main)) ||
               (*url_main == DOT))) {
            url_main++;
        }
    }
    if (url_main) {
        SKIP_LWS(url_main);
    }
    if ((url_main) && (!(*url_main))) {
        if (!is_local_subscriber) {
            return 0;  /* no fields to parse */
        } else {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"local-phone-number must have area-specifier",
                       fname);            
            return PARSE_ERR_SYNTAX;
        }
    }
    if (url_main) { 
        if ((*url_main) && (*url_main == SEMI_COLON)) {
            *url_main++ = 0;  /* skip separator */
        } else {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Need ';' before parameters", fname);            
            return PARSE_ERR_SYNTAX;
        }
    }

    while (url_main) {
        SKIP_LWS(url_main);
        if (cpr_strncasecmp(url_main, "isub=", 5) == 0) {
            url_main += 5;
            SKIP_LWS(url_main);
            telUrl->isdn_subaddr = url_main;
        } else if (cpr_strncasecmp(url_main, "postd=", 6) == 0) {
            url_main += 6;
            SKIP_LWS(url_main);
            telUrl->post_dial = url_main;
        } else if (cpr_strncasecmp(url_main, "phone-context", 13) == 0) {
            url_main += 13;
            SKIP_LWS(url_main);
            if (*url_main != EQUAL_SIGN) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Bad syntax in phone_context field", fname);                
                return PARSE_ERR_SYNTAX;
            }
            *url_main++ = 0;  /* skip "=" */
            SKIP_LWS(url_main);
            genUrl->phone_context = url_main;
        } else if (cpr_strncasecmp(url_main, "tsp", 3) == 0) {
            url_main += 3;
            SKIP_LWS(url_main);
            if (*url_main != EQUAL_SIGN) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Bad syntax in service-provider field", fname);                
                return PARSE_ERR_SYNTAX;
            }
            *url_main++ = 0;  /* skip "=" */
            SKIP_LWS(url_main);
            telUrl->unparsed_tsp = url_main;
        } else {
            /* future extension */
            if (telUrl->future_ext) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Only one future extension allowed", fname);                
                return PARSE_ERR_SYNTAX;
            }
            SKIP_LWS(url_main);
            url_main = strchr(url_main, EQUAL_SIGN);
            if (!url_main) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Bad field value", fname);                
                return PARSE_ERR_SYNTAX;
            } else {
                *url_main++ = 0;
                SKIP_LWS(url_main);
                telUrl->future_ext = url_main;
            }
        }
        url_main = strchr(url_main, SEMI_COLON);
        if (url_main) {
            *url_main++ = 0;  /* skip and zero separator */
        }
    }
    if ((is_local_subscriber) && (!genUrl->phone_context)) {
        /* local-phone-number must have area-specifier */
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"local-phone-number requires area-specifier", fname);        
        return PARSE_ERR_SYNTAX;
    }
    return 0;  /* Success */
}

/*
 * Parses a URL
 * (eg. "sip:14085266593@sip.cisco.com;transport=UDP;user=phone")
 * (eg. "tel:+1-555-555-5555")
 * into a usable genUrl_t struct. Memory is created by this function.
 * To free first free internal members(sippmh_genurl_free(genUrl)).
 * Returns NULL if there is a parse error or malloc fails. Components/tokens
 * not found in the URL are set to NULL, unless they have a default value
 * defined by the protocol ( for example port 5060 is default)
 * Parse error will set parse_errno variable to indicate the error.
 * Expects a NULL terminated string in url.
 * See comment block before parseSipUrl() and parseTelUrl()
 * for some other details.
 *
 * url - Pointer to URL to be parsed
 * dup_flag - Should this URL be duplicated and modified ??
 */
genUrl_t *
sippmh_parse_url (char *url, boolean dup_flag)
{
    genUrl_t *genUrl;
    char     *url_main;
    uint16_t  i;
    uint16_t  skipValue;

    if (url == NULL) {
        return NULL;
    }

    genUrl = (genUrl_t *) cpr_calloc(1, sizeof(genUrl_t));
    if (genUrl == NULL) {
        return NULL;
    }

    /*
     * Ensure clean other params pointers
     */
    i = 0;
    while (i < SIP_MAX_LOCATIONS) {
        genUrl->other_params[i] = NULL;
        i++;
    }

    if (dup_flag) {
        url_main = cpr_strdup(url);
        if (!url_main) {
            cpr_free(genUrl);
            return NULL;
        }
        genUrl->str_start = url_main;
    } else {
        url_main = url;
    }

    SKIP_LWS(url_main);
    if (!cpr_strncasecmp(url_main, "sips", 4)) {
        genUrl->schema = URL_TYPE_SIP;
        genUrl->sips = TRUE;
        skipValue = 4;
    } else if (!cpr_strncasecmp(url_main, "sip", 3)) {
        genUrl->schema = URL_TYPE_SIP;
        skipValue = 3;
    } else if (!cpr_strncasecmp(url_main, "tel", 3)) {
        genUrl->schema = URL_TYPE_TEL;
        skipValue = 3;
    } else if (!cpr_strncasecmp(url_main, "cid", 3)) {
        genUrl->schema = URL_TYPE_CID;
        skipValue = 3;
    } else {
        genUrl->schema = URL_TYPE_UNKNOWN;
        skipValue = 0;
    }
    url_main += skipValue;  /* skip "sip", "sips", cid, or "tel" */
    if (*url_main != COLON) {
        if (dup_flag) {
            cpr_free(genUrl->str_start);
        }
        cpr_free(genUrl);
        return NULL;
    }
    *url_main++ = 0;        /* terminate ":" for schema */
    switch (genUrl->schema) {
    case URL_TYPE_CID:
    case URL_TYPE_SIP:
        genUrl->u.sipUrl = (sipUrl_t *) cpr_calloc(1, sizeof(sipUrl_t));
        if (!genUrl->u.sipUrl) {
            if (dup_flag) {
                cpr_free(genUrl->str_start);
            }
            cpr_free(genUrl);
            return NULL;
        }
        parse_errno = parseSipUrl(url_main, genUrl);
        if (parse_errno == 0) {
            //remove_visual_separators(genUrl);
            if (genUrl->u.sipUrl->port == 0) {
                /* If no port specified in SIP-URL, then use 5060 */
                genUrl->u.sipUrl->port = SIP_WELL_KNOWN_PORT;
                genUrl->u.sipUrl->port_present = FALSE;
            }
            return genUrl;
        }
        /* For syntax error, we would already have printed the error msg */
        if (parse_errno != PARSE_ERR_SYNTAX) {
            if (parse_errno == PARSE_ERR_NO_MEMORY) {
                parse_errno = 0;  /* Not really a parse error (out of mem) */
            } else {
                CCSIP_DEBUG_ERROR(parse_errors[parse_errno], "sippmh_parse_url");                
            }
        }
        sippmh_genurl_free(genUrl);
        return NULL;
    case URL_TYPE_TEL:
        genUrl->u.telUrl = (telUrl_t *) cpr_calloc(1, sizeof(telUrl_t));
        if (!genUrl->u.telUrl) {
            if (dup_flag) {
                cpr_free(genUrl->str_start);
            }
            cpr_free(genUrl);
            return NULL;
        }
        parse_errno = parseTelUrl(url_main, genUrl);
        if (parse_errno == 0) {
            //remove_visual_separators(genUrl);
            return genUrl;
        }
        /* For syntax error, we would already have printed the error msg */
        if (parse_errno != PARSE_ERR_SYNTAX) {
            if (parse_errno == PARSE_ERR_NO_MEMORY) {
                parse_errno = 0;  /* Not really a parse error (out of mem) */
            } else {
                CCSIP_DEBUG_ERROR(parse_errors[parse_errno], "sippmh_parse_url");                
            }
        }
        sippmh_genurl_free(genUrl);
        return NULL;
    default:
        sippmh_genurl_free(genUrl);
        return NULL;
    }
}

/* If this is a SIP URL, free the sipUrl_t.  If this is
 * a telUrl_t free the telUrl_t.  Also, free memory allocated by creation
 * of the genUrl_t then free the genUrl_t.
 */
void
sippmh_genurl_free (genUrl_t *genUrl)
{
    uint16_t i;

    if (!genUrl) {
        return;
    }
    if (genUrl->str_start) {
        cpr_free(genUrl->str_start);
    }
    if ((genUrl->schema == URL_TYPE_SIP) ||
        (genUrl->schema == URL_TYPE_CID)) {
        if (genUrl->u.sipUrl->headerp) {
            cpr_free(genUrl->u.sipUrl->headerp);
        }
        cpr_free(genUrl->u.sipUrl);
    } else if (genUrl->schema == URL_TYPE_TEL) {
        cpr_free(genUrl->u.telUrl);
    }

    /* Free any "other" parameters that were saved */
    i = 0;
    while (i < SIP_MAX_LOCATIONS) {
        if (genUrl->other_params[i] != NULL) {
            cpr_free(genUrl->other_params[i]);
        }
        i++;
    }

    cpr_free(genUrl);
}

/* Parse display name:
 * "Mr. Watson" <sip:watson@bell-telephone.com>
 * "Mr. Watson" is the display-name
 * Return pointer to the '<'
 */

static char *
parse_display_name (char *ptr)
{
    static const char fname[] = "parse_display_name";

    while (*ptr) {
        if (*ptr == DOUBLE_QUOTE && *(ptr - 1) != ESCAPE_CHAR) {
            /* We reached end of quoted-string, mark end of display name */
            *ptr++ = 0;
            SKIP_LWS(ptr); /* Skip spaces till '<' */
            /* ptr should now be pointing to '<' */
            if (*ptr != LEFT_ANGULAR_BRACKET) {
                parse_errno = PARSE_ERR_UNMATCHED_BRACKET;
                CCSIP_DEBUG_ERROR(parse_errors[parse_errno], fname);                

                return NULL;
            }
            return ptr;
        }
        ptr++;
    }

    parse_errno = PARSE_ERR_UNTERMINATED_STRING;
    CCSIP_DEBUG_ERROR(parse_errors[parse_errno], fname);    

    return NULL;  /* Unmatched " */
}


/*
 * Parse the name-addr form or the addr-spec form. These parse entities can
 * be present in the From, To and Contact headers. So it is called either
 * by sippmh_parse_contact() or sippmh_parse_from_or_to().
 *
 * input_loc_ptr - Pointer to the string to be parsed
 * dup_flag - Should the input string be duplicated before parsing
 * more_ptr - Return pointer to the next
 * Tokens are extracted in place, i.e. we do not do a strdup for every token.
 * We have on string and the output struct has pointers pointing to various
 * tokens in the string.
 * Note that this routine relies on the caller setting parse_errno to 0
 * before calling.
 *
 * input_loc_ptr - Pointer to input string to be parsed
 * start_ptr - Pointer to beginning of the string, which will be used for
 *            cleanup.
 * dup_flag - Should the input string be duplicated before parsing
 * name_addr_only_flag - If TRUE, means only name-addr form is allowed
 * more_ptr - Return pointer to start of remaining string ( string that
 *            remains after parsing name-addr or addr-spec
 *
 * Output: A pointer to a valid SIP location  OR
 *         NULL (in case of error)
 *
 */
sipLocation_t *
sippmh_parse_nameaddr_or_addrspec (char *input_loc_ptr,
                                   char *start_ptr,
                                   boolean dup_flag,
                                   boolean name_addr_only_flag,
                                   char **more_ptr)
{
    const char *fname = "sippmh_parse_nameaddr_or_addrspec";
    char           *addr_param;
    char           *loc_ptr, *addr_spec, *left_bracket;
    sipLocation_t  *sipLoc = NULL;
    char           *right_bracket = NULL;
    char            save_ch = 0;
    char           *displayNameStart;

    *more_ptr = NULL;

    /*
     * name-addr      = [ display-name ] "<" addr-spec ">"
     * addr-spec      = SIP-URL | URI
     * display-name   = *token | quoted-string
     */
    if (dup_flag) {
        /* Duplicate the string and work with it */
        start_ptr = loc_ptr = cpr_strdup(input_loc_ptr);
        if (loc_ptr == NULL) {
            return NULL;
        }
    } else {
        loc_ptr = input_loc_ptr;
    }

    if (*loc_ptr == DOUBLE_QUOTE) {
        displayNameStart = loc_ptr + 1;
        left_bracket = parse_display_name(loc_ptr + 1);
        if (left_bracket == NULL) {
            /* Could not find matching " or reached end of string or could not
             * find <
             */
            if (dup_flag) {
                cpr_free(loc_ptr);
            }
            /*  parse_display_name has already set the parse_errno */

            return NULL;
        }
    } else {

        displayNameStart = loc_ptr;
        /* Either we have token(s) preceding '<' or start of addr-spec */
        left_bracket = strpbrk(loc_ptr, ",<");
        if (left_bracket) {
            if (*left_bracket == COMMA) {
                *left_bracket = 0;
                *more_ptr = left_bracket;
                left_bracket = NULL;
                save_ch = COMMA;
            }
        } else {
            *more_ptr = NULL;
        }
    }

    sipLoc = (sipLocation_t *) cpr_calloc(1, sizeof(sipLocation_t));
    if (sipLoc == NULL) {
        if (dup_flag) {
            cpr_free(loc_ptr);
        }
        return NULL;
    }
    sipLoc->loc_start = start_ptr;  /* Save pointer to start of allocated mem */

    if (left_bracket) {
        /* This is a name-addr form */

        *left_bracket = 0; /* Terminate the display-name portion */
        sipLoc->name = displayNameStart;
        addr_spec = left_bracket + 1;
        right_bracket = strchr(addr_spec, RIGHT_ANGULAR_BRACKET);
        if (right_bracket == NULL) {
            if (dup_flag) {
                cpr_free(loc_ptr);
            }
            cpr_free(sipLoc);
            parse_errno = PARSE_ERR_UNMATCHED_BRACKET;
            CCSIP_DEBUG_ERROR(parse_errors[parse_errno],
                       "sippmh_parse_nameaddr_or_addrspec");            
            return NULL;
        }

        /* Terminate the addr_spec at the > */
        *right_bracket++ = 0;

        /* Look for the next non-whitespace character */
        SKIP_LWS(right_bracket);
        *more_ptr = right_bracket;
    } else {

        /* This is addr-spec format */

        if (name_addr_only_flag) {
            if (dup_flag) {
                cpr_free(loc_ptr);
            }
            cpr_free(sipLoc);
            CCSIP_ERR_DEBUG {
                buginf("\n%s: Bad name-addr format", fname);
            }
            return NULL;
        }

        addr_spec = loc_ptr;
        /* In addr-spec form, it is illegal to have URL special
         * chars such as SEMI_COLON, QUESTION MARK
         */
        addr_param = addr_spec;
        while (*addr_param) {
            if (*addr_param == QUESTION_MARK || *addr_param == SEMI_COLON) {

                if ((save_ch != 0) && *more_ptr) {
                    /* restore the saved char mostly COMMA */
                    **more_ptr = save_ch;
                }

                save_ch = *addr_param;
                *more_ptr = addr_param;

                /* Terminate the addr-spec form at Special char */
                *addr_param = 0;
                break;
            }
            addr_param++;
        }
    }

    sipLoc->genUrl = sippmh_parse_url(addr_spec, FALSE);
    if (sipLoc->genUrl == NULL) {
        if (dup_flag) {
            cpr_free(loc_ptr);
        }
        cpr_free(sipLoc);
        /* sippmh_parse_url would have set the parse_errno  */
        return NULL;
    }

    /*
     * There is more stuff to follow since *more_ptr is not NULL
     * replace the special chars (such as ? or ;) in original string
     */
    if ((save_ch != 0) && (*more_ptr != NULL)) {
        **more_ptr = save_ch;
    }
    return sipLoc;
}


/*
 * Validate the tag. The tag is composed of tokens. The tag_ptr
 * points to the start of the tag and a list of semicolon
 * separated addr-extensions or just the tag itself.
 *
 * Returns 0 (no error) if the tag is valid, else
 * returns PARSE_ERR_SYNTAX.
 *
 */
static int
validate_tag (sipLocation_t *sipLoc, char *tag_ptr)
{
    static const char fname[] = "validate_tag";
    int   ret_val = 0;
    char *term_ptr;

    /* Skip "tag=", points to value */
    tag_ptr += 4;
    SKIP_LWS(tag_ptr);
    if (*tag_ptr == 0) {
        ret_val = PARSE_ERR_UNEXPECTED_EOS;
        CCSIP_ERR_DEBUG {
            buginf(parse_errors[ret_val], fname);
        }

        return ret_val;
    }

    sipLoc->tag = tag_ptr;

    /* Walk the string until we encounter a non-token
     * character. The only valid character at the end of
     * string is either semicolon or end-of-line.
     */

    SKIP_SIP_TOKEN(tag_ptr);
    term_ptr = tag_ptr;
    /* skip trailing spaces */
    SKIP_LWS(tag_ptr);
    if ((*tag_ptr != SEMI_COLON) && (*tag_ptr != 0)) {
        ret_val = PARSE_ERR_SYNTAX;
        CCSIP_ERR_DEBUG {
            buginf(parse_errors[ret_val], fname, tag_ptr);
        }
    } else {
        /* terminate the tag */
        *term_ptr = 0;
    }

    return ret_val;
}


sipLocation_t *
sippmh_parse_from_or_to (char *input_loc_ptr, boolean dup_flag)
{
    static const char fname[] = "sippmh_parse_from_or_to";
    char *more_ptr;
    sipLocation_t *sipLoc;
    boolean tag_found = FALSE;
    char *lasts = NULL;

    /*
     *  From         =  ( "From" | "f" ) ":" ( name-addr | addr-spec )
     *                  *( ";" addr-params )
     *  addr-params  =  tag-param | addr-extension
     *  tag-param    =  "tag=" token
     *  addr-exten   =  token = ["=" (token | quoted-string)]
     *
     *  NOTE: Only the tag parameter in the addr-params is relevant to
     *        SIP so we skip all addr-extensions when parsing addr-params.
     */

    parse_errno = 0;
    more_ptr = NULL;
    sipLoc = sippmh_parse_nameaddr_or_addrspec(input_loc_ptr, input_loc_ptr,
                                               dup_flag, FALSE, &more_ptr);

    if (sipLoc) {  /* valid sipLocation */

        if (more_ptr == NULL) {
            return sipLoc;
        }

        /* initialize the tag */
        sipLoc->tag = NULL;
        if (*more_ptr == SEMI_COLON) {
            *more_ptr++ = 0;
            more_ptr = cpr_strtok(more_ptr, ";", &lasts);
            /* if we had a ; without any addr-params */
            if (more_ptr == NULL) {
                parse_errno = PARSE_ERR_UNEXPECTED_EOS;
                CCSIP_DEBUG_ERROR(parse_errors[parse_errno], fname);                
            } else {
                /* parse the tag but skip any addr-extensions */
                while (more_ptr && tag_found == FALSE) {
                    SKIP_LWS(more_ptr);
                    if (strncmp(more_ptr, "tag=", 4) == 0) {
                        tag_found = TRUE;
                        parse_errno = validate_tag(sipLoc, more_ptr);
                    } else {
                        more_ptr = cpr_strtok(NULL, ";", &lasts);
                    }
                }
            }

        } else if (*more_ptr) {
            parse_errno = PARSE_ERR_SYNTAX;
            CCSIP_DEBUG_ERROR(parse_errors[parse_errno], fname, more_ptr);            
        }
    }

    if (parse_errno) {
        sippmh_free_location(sipLoc);
        sipLoc = NULL;
    }

    return sipLoc;
}

void
sippmh_free_location (sipLocation_t * sipLoc)
{
    if (sipLoc) {
        cpr_free(sipLoc->loc_start);
        /* make sure to free contents in genUrl_t. */
        sippmh_genurl_free(sipLoc->genUrl);
        cpr_free(sipLoc);
    }
}

static char *
sippmh_parse_contact_params (char *params, sipContactParams_t *contact_params)
{
    char   *param_value;
    char   *contact_other_param = NULL;
    boolean good_params;
    boolean good_qval;
    char    tmp_char;


    /*
     *    contact-params = "q"       "=" qvalue
     *            | "action"  "=" "proxy" | "redirect"
     *            | "expires" "=" delta-seconds | <"> SIP-date <">
     *            | extension-attribute
     *
     * params is pointing to first character after ';'
     */
    SKIP_LWS(params);
    if (*params == 0) {
        return params;
    }
    while (1) {
        good_params = FALSE;

        /* Parse qval parameter */
        if (*params == 'q' || *params == 'Q') {
            params++;
            SKIP_LWS(params);
            if (*params == EQUAL_SIGN) {
                params++;
                SKIP_LWS(params);
                if (*params) {
                    param_value = params;
                    good_qval = TRUE;
                    tmp_char = *params;
                    /* the qval must be in one of these forms */
                    if ((!strncmp(params, "0", 1)) ||
                        (!strncmp(params, "1", 1))) {
                        params++;  /* move up to "." if present */
                        if (*params == DOT) {
                            params++;
                            if (isdigit((int) *params)) {
                                while (isdigit((int) *params)) {
                                    if ((tmp_char == '1') &&
                                        (*params != '0')) {
                                        good_qval = FALSE;
                                        break;
                                    }
                                    params++;
                                }
                            } else {
                                good_qval = FALSE;
                            }
                        } else if (((*params)) && ((*params != SPACE) &&
                                   (*params != SEMI_COLON) &&
                                   (*params != TAB))) {
                            good_qval = FALSE;
                        }
                    } else {
                        good_qval = FALSE;
                    }
                    if (good_qval) {
                        good_params = TRUE;
                        contact_params->qval = param_value;
                        if (*params == SPACE || *params == TAB) {
                            *params++ = 0;  /* Terminate the qval */
                        }
                    }
                }
            }

            /* Parse action paramater */
        } else if (cpr_strncasecmp(params, "action", 6) == 0) {
            params += 6;
            SKIP_LWS(params);
            if (*params == EQUAL_SIGN) {
                params++;
                SKIP_LWS(params);
                if (cpr_strncasecmp(params, "proxy", 5) == 0) {
                    contact_params->action = PROXY;
                    params += 5;
                    good_params = TRUE;
                } else if (cpr_strncasecmp(params, "redirect", 8) == 0) {
                    contact_params->action = REDIRECT;
                    params += 8;
                    good_params = TRUE;
                }
            }

            /* Parse expires paramater */
        } else if (cpr_strncasecmp(params, "expires", 7) == 0) {
            params += 7;
            SKIP_LWS(params);
            if (*params == EQUAL_SIGN) {
                params++;
                SKIP_LWS(params);
                if (*params) {
                    char save_ch;
                    boolean is_int = FALSE;

                    /* Expires can be given in delta seconds or date format.
                     * If date is used, the date must be enclosed in quotes.
                     * IOS does not use this parameter, but the SIP phones
                     * do.
                     */
                    contact_params->expires_gmt = '\0';
                    good_params = TRUE;
                    param_value = params;
                    while (isdigit((int) *params)) {
                        params++;
                        is_int = TRUE;
                    }
                    if (is_int == TRUE) {
                        save_ch = *params;
                        *params = 0; /* terminate the string for strtoul */
                        contact_params->expires = strtoul(param_value, NULL, 10);
                        *params = save_ch;
                    } else {
                        /* expires may be in SIP-date format */
                        char *gmt_str;

                        if (*params == DOUBLE_QUOTE) { /* skip the quote */
                            params++;
                        }
                        if ((gmt_str = strstr(params, "GMT")) != NULL) {
                            contact_params->expires_gmt = params;
                            params = gmt_str + 3;
                            contact_params->expires = 0;

                            if (*params == SPACE || *params == TAB || *params == DOUBLE_QUOTE) {
                                *params++ = '\0';
                            }
                        }
                    }
                }
            }
            /* Parse x-cisco-newreg */
        } else if (cpr_strncasecmp(params, REQ_CONT_PARAM_CISCO_NEWREG,
                                   sizeof(REQ_CONT_PARAM_CISCO_NEWREG) - 1) == 0) {
            /*
             * x-cisco-newreg in the contact is set by CCM in the 200 OK
             * response to the 1st. line registration message as information
             * only to the endpoint that its registration is a new
             * registration to the CCM.
             */

            /*
             * Move parameter pointer pass this parm (-1 is to exclude
             * null terminating character of the parameter name).
             */
            params += sizeof(REQ_CONT_PARAM_CISCO_NEWREG) - 1;
            good_params = TRUE;
            contact_params->flags |= SIP_CONTACT_PARM_X_CISCO_NEWREG;

            /* Parse contact-extension paramater */
        } else {
            if (params) {
                params = parse_other_param(params, &contact_other_param);
                if (params) {
                    good_params = TRUE;
                    /*
                     * Contact extensions are not supported. Throw away
                     * any that were just parsed.
                     */
                    if (contact_other_param != NULL) {
                        cpr_free(contact_other_param);
                    }
                }
            }
        }
        if (good_params == FALSE) {
            parse_errno = PARSE_ERR_SYNTAX;
            return params;
        }
        SKIP_LWS(params);
        if (*params == SEMI_COLON) {
            /* More parameters follow */
            *params++ = 0;
            SKIP_LWS(params);
        } else {
            /* Either we have encountered end of string or start of next
             * location in this contact header.
             */
            return params;
        }
    }
}

/* Parses limit parameter for CC-Diversion or CC-Redirect
 * header. The parsed value is in sipDiversion_t structure.
 * Returns pointer to remaining characters in the buffer, if fails NULL
 */
static char *
sippmh_parse_limit_params (char *limit_t, sipDiversion_t *sipdiversion)
{
    boolean params_good = FALSE;
    char save_ch;
    char *param_value = NULL;
    int digit_count = 0;

    if ((limit_t == NULL) || (sipdiversion == NULL)) {
        return NULL;
    }

    SKIP_LWS(limit_t);
    if (*limit_t == EQUAL_SIGN) {
        limit_t++;
        SKIP_LWS(limit_t);
        if (*limit_t) {
            params_good = TRUE;
            param_value = limit_t;
            while (isdigit((int) *limit_t)) {
                digit_count++;
                limit_t++;
            }
            if (digit_count > 2) {
                /* More than two digits */
                params_good = FALSE;
                return NULL;
            }
            digit_count = 0;
            save_ch = *limit_t;
            *limit_t = 0;
            sipdiversion->limit = strtoul(param_value, NULL, 10);
            *limit_t = save_ch;
        }
    } else {
        /* Equal to sign missing */
        params_good = FALSE;
    }

    if (params_good) {
        return limit_t;
    } else {
        return NULL;
    }
}


/* Parses counter parameter for CC-Diversion or CC-Redirect
 * header. The parsed value is in sipDiversion_t structure.
 * Returns pointer to remaining characters in the buffer, if fails NULL
 */
static char *
sippmh_parse_counter_params (char *counter_t, sipDiversion_t *sipdiversion)
{
    boolean params_good = FALSE;
    char save_ch;
    char *param_value = NULL;
    int digit_count = 0;

    if ((counter_t == NULL) || (sipdiversion == NULL)) {
        return NULL;
    }

    SKIP_LWS(counter_t);
    if (*counter_t == EQUAL_SIGN) {
        counter_t++;
        SKIP_LWS(counter_t);
        if (*counter_t) {
            params_good = TRUE;
            param_value = counter_t;
            while (isdigit((int) *counter_t)) {
                digit_count++;
                counter_t++;
            }
            if (digit_count > 2) {
                /* More than two digits */
                params_good = FALSE;
                return NULL;
            }
            digit_count = 0;
            save_ch = *counter_t;
            *counter_t = 0;
            sipdiversion->counter = strtoul(param_value, NULL, 10);
            *counter_t = save_ch;

        }
    } else {
        /* Equal to sign missing */
        params_good = FALSE;
    }

    if (params_good) {
        return counter_t;
    } else {
        return NULL;
    }
}


static boolean
sippmh_parse_diversion_params (char *diversion_t, sipDiversion_t *sipdiversion)
{

    while (1) {
        /* Parsing diversion-reason parameter */
        if (strncasecmp(diversion_t, DIVERSION_REASON, 6) == 0) {
// We don't use reason parameter, so comment out at this time.
//         diversion_t += 6;
//         if ((diversion_t = sippmh_parse_reason_params(diversion_t, sipdiversion)) == NULL)
//            return FALSE;
            /*
             * Since we do not use reason parameter, search for semi-colan to mark
             * start of next parameter. If no semi-colon, then we are done.
             */
            diversion_t = strchr(diversion_t, SEMI_COLON);
            if (diversion_t == NULL) {
                // End of parameter list
                return TRUE;
            }
            /* Parse diversion-limit parameter   */
        } else if (strncasecmp(diversion_t, DIVERSION_LIMIT, 5) == 0) {
            diversion_t += 5;
            if ((diversion_t = sippmh_parse_limit_params(diversion_t, sipdiversion)) == FALSE)
                return FALSE;

            /* Parse diversion-counter parameter   */
        } else if (strncasecmp(diversion_t, DIVERSION_COUNTER, 7) == 0) {
            diversion_t += 7;
            if ((diversion_t = sippmh_parse_counter_params(diversion_t, sipdiversion)) == FALSE)
                return FALSE;

            /* Parse diversion-privacy parameter   */
        } else if (strncasecmp(diversion_t, DIVERSION_PRIVACY, 7) == 0) {
            diversion_t += 7;
            if ((diversion_t = parse_generic_param(diversion_t, &(sipdiversion->privacy))) == NULL)
                return FALSE;

            /* Parse diversion-screen parameter   */
        } else if (strncasecmp(diversion_t, DIVERSION_SCREEN, 6) == 0) {
            diversion_t += 6;
            if ((diversion_t = parse_generic_param(diversion_t, &(sipdiversion->screen))) == NULL)
                return FALSE;
        }

        SKIP_LWS(diversion_t);
        if (*diversion_t == SEMI_COLON) {
            /* More parameters follow */
            *diversion_t++ = 0;
            SKIP_LWS(diversion_t);
        } else {
            break;
        }
    }

    return TRUE;

}

sipDiversion_t *
sippmh_parse_diversion (const char *diversion, char *diversionhead)
{
    char           *param_ptr;
    sipDiversion_t *sipdiversion;
    sipLocation_t  *sipLoc;
    char           *diversion_t, *start_ptr;

    /*  CC-Diversion = "CC-Diversion"
     *                              *(diversion-params [comment])
     *  diversion-params  = diversion-addr | diversion-reason |
     *                      diversion-counter| diversion-limit
     *  diversion-addr    = (name-addr | addr-spec)*(";"addr-params)
     *  diversion-reason  = "reason"= "unknown" | "user-busy" | "no-answer" |
     *                     "unconditional" | "deflection"| "follow-me" |
     *                     "out-of-service" | " time-of-day" | "unavailable" |
     *                     "do-not-disturb"
     *  diversion-counter = "counter" = 2*DIGIT
     *  diversion-limit   = "limit" = 2*DIGIT
     */

    /*
     * Diversion = "Diversion" ":" 1# (name-addr *( ";" diversion_params ))
     * diversion-params = diversion-reason | diversion-counter |
     *                  diversion-limit | diversion-privacy |
     *         diversion-screen | diversion-extension
     * diversion-reason = "reason" "="
     *               ( "unknown" | "user-busy" | "no-answer" |
     *                 "unavailable" | "unconditional" |
     *                 "time-of-day" | "do-not-disturb" |
     *                 "deflection" | "follow-me" |
     *                 "out-of-service" | "away" |
     *                 token | quoted-string )
     * diversion-counter = "counter" "=" 1*2DIGIT
     * diversion-limit = "limit" "=" 1*2DIGIT
     * diversion-privacy = "privacy" "=" ( "full" | "name" |
     *                   "uri" | "off" | token | quoted-string )
     * diversion-screen = "screen" "=" ( "yes" | "no" | token |
     *                                    quoted-string )
     * diversion-extension = token ["=" (token | quoted-string)]
     */

    sipdiversion = (sipDiversion_t *) cpr_calloc(1, sizeof(sipDiversion_t));
    if (sipdiversion == NULL) {
        return NULL;
    }

    diversion_t = cpr_strdup(diversion);

    if (diversion_t == NULL) {
        sippmh_free_diversion(sipdiversion);
        return NULL;
    }

    start_ptr = diversion_t;

    do {

        param_ptr = NULL;
        sipLoc = sippmh_parse_nameaddr_or_addrspec(diversion_t, start_ptr, FALSE, FALSE,
                                                   &param_ptr);
        if (sipLoc == NULL) {
            cpr_free(start_ptr);
            sippmh_free_diversion(sipdiversion);
            sipdiversion = NULL;
            break;
        }

        sipdiversion->locations = sipLoc;

        if (param_ptr == NULL || *param_ptr == 0) {
            /* No params */
            break;
        }
        diversion_t = param_ptr;
        if (*diversion_t == SEMI_COLON) {
            /* Parsing Diversion Headers */
            *diversion_t++ = 0;

            if ((strncasecmp(diversionhead, SIP_HEADER_DIVERSION,
                             sizeof(SIP_HEADER_DIVERSION)) == 0) ||
                (strncasecmp(diversionhead, SIP_HEADER_CC_DIVERSION,
                             sizeof(SIP_HEADER_CC_DIVERSION)) == 0)) {

                if (sippmh_parse_diversion_params(diversion_t, sipdiversion) == FALSE) {

                    CCSIP_ERR_DEBUG {
                        buginf("\nsippmh_parse_diversion: syntax error in Diversion header");
                    }
                    parse_errno = PARSE_ERR_SYNTAX;
                    sippmh_free_diversion(sipdiversion);
                    sipdiversion = NULL;
                    break;
                }
            }
            /* Must reach here after parsing all parameters  */
            break; /* break out of original do loop */
        } else {
            CCSIP_ERR_DEBUG {
                buginf("\nsippmh_parse_diversion: syntax error missing "
                       "semicolon in Diversion header");
            }
            parse_errno = PARSE_ERR_SYNTAX;
            sippmh_free_diversion(sipdiversion);
            sipdiversion = NULL;
            break;
        }

    } while (1);
    return sipdiversion;
}

void
sippmh_free_diversion (sipDiversion_t * ccr)
{
    if (ccr) {
        if (ccr->locations) {
            sippmh_free_location(ccr->locations);
        }
        cpr_free(ccr);
    }
}


sipContact_t *
sippmh_parse_contact (const char *input_contact)
{
    int j, k;
    char *contact = NULL, *start_ptr = NULL;
    char *more_ptr;
    sipContact_t *sipContact;
    sipContactParams_t params;

    /*
     * Contact = ( "Contact" | "m" ) ":"
     *       ("*" | (1# (( name-addr | addr-spec )
     *       [ *( ";" contact-params ) ] [ comment ] )))
     * name-addr      = [ display-name ] "<" addr-spec ">"
     * addr-spec      = SIP-URL | URI
     * display-name   = *token | quoted-string
     *
     * contact-params = "q"       "=" qvalue
     *            | "action"  "=" "proxy" | "redirect"
     *            | "expires" "=" delta-seconds | <"> SIP-date <">
     *            | extension-attribute
     *
     */
    parse_errno = 0;
    contact = cpr_strdup(input_contact);
    if (contact == NULL) {
        return NULL;
    }

    sipContact = (sipContact_t *) cpr_calloc(1, sizeof(sipContact_t));
    if (sipContact == NULL) {
        cpr_free(contact);
        return NULL;
    }

    memset(&params, 0, sizeof(sipContactParams_t));

    start_ptr = contact;

    do {
        sipLocation_t *sipLoc;

        more_ptr = NULL;
        sipLoc = sippmh_parse_nameaddr_or_addrspec(contact, start_ptr, FALSE,
                                                   FALSE, &more_ptr);
        if (sipLoc == NULL) {
            if ((more_ptr != NULL) && (*more_ptr == COMMA)) {
                /* Another location follows */
                contact = more_ptr;
                *contact++ = 0;
                SKIP_LWS(contact);
                if (sipContact->num_locations == SIP_MAX_LOCATIONS) {
                    CCSIP_ERR_DEBUG {
                        buginf("\nsippmh_parse_contact: Too many location headers"
                               " in Contact header");
                    }
                    /* go with what we have */
                    break;
                }
                continue;
            } else {
                if (sipContact->num_locations == 0) {
                    // Free start_ptr only if we fail the first location
                    // For the others, it will be freed when sipContact is
                    // freed
                    cpr_free(start_ptr);
                }
                sippmh_free_contact(sipContact);
                sipContact = NULL;
                break;
            }
        }
        if (sipContact->num_locations) {
            sipLoc->loc_start = NULL;
        }
        if (more_ptr == NULL || *more_ptr == 0) {
            /* No params, means no qval and assume lowest priority */
            sipContact->locations[sipContact->num_locations++] = sipLoc;
            break;
        }
        contact = more_ptr;
        /*
         * At this point, either contact is pointing to
         * 1) ';' - Contact params
         * 2) ',' - Beginning of next location
         * 3) End of string
         * 4) Any other character is syntax error
         */

        j = sipContact->num_locations; /* By default insert at the end */
        params.qval = "";

        if (*contact == SEMI_COLON) {
            /* Now parse the Contact params */
            *contact++ = 0;
            params.flags = 0;
            contact = sippmh_parse_contact_params(contact, &params);
            /*
             * Now, do an insertion sort on this list (qval is the key).
             * Higher values of q come first.
             */
            for (j = 0; j < sipContact->num_locations; ++j) {
                if (strcmp(params.qval, sipContact->params[j].qval) > 0) {
                    for (k = sipContact->num_locations; k > j; --k) {
                        sipContact->locations[k] = sipContact->locations[k - 1];
                        sipContact->params[k] = sipContact->params[k - 1];
                    }
                    break;
                }
            }
            /* At this point j points to the entry to use */
        }

        sipContact->params[j] = params;
        sipContact->locations[j] = sipLoc;
        sipContact->num_locations++;

        if (contact && *contact == COMMA) {
            /* Another location follows */
            *contact++ = 0;
            SKIP_LWS(contact);
            if (sipContact->num_locations == SIP_MAX_LOCATIONS) {
                CCSIP_ERR_DEBUG {
                    buginf("\nsippmh_parse_contact: Too many location headers"
                           " in Contact header");
                }
                /* go with what we have */
                break;
            }
        } else {
            if (contact && *contact) {
                parse_errno = PARSE_ERR_SYNTAX;
                CCSIP_DEBUG_ERROR(parse_errors[parse_errno], "sippmh_parse_contact",
                           contact);                
                sippmh_free_contact(sipContact);
                sipContact = NULL;
            }
            break;
        }
    } while (1);

    if (sipContact) {
        sipContact->new_flag = TRUE;
    }
    return sipContact;
}


void
sippmh_free_contact (sipContact_t *sc)
{
    int i;

    if (sc) {
        for (i = 0; i < sc->num_locations; ++i) {
            if (sc->locations[i]->loc_start) {
                /* Free the entire header value that we duplicated */
                cpr_free(sc->locations[i]->loc_start);
            }
            /* Free the genUrl_t struct in the sipLocation_t */
            sippmh_genurl_free(sc->locations[i]->genUrl);
            /* Free the sipLocation_t struct */
            cpr_free(sc->locations[i]);
        }

        /* Free the sipContact_t struct */
        cpr_free(sc);
    }
}

static boolean
parse_via_params (char *str, sipVia_t *sipVia)
{
    char **ptr;

    /* str is currently pointing to beginning of parameters */

    while (1) {
        /* To be friendly, skip leading spaces */
        SKIP_LWS(str);
        if (strncasecmp(str, VIA_HIDDEN, 6) == 0) {
            str += 6;
            sipVia->flags |= VIA_IS_HIDDEN;
            ptr = NULL;
        } else if (strncasecmp(str, VIA_TTL, 3) == 0) {
            str += 3;
            ptr = &(sipVia->ttl);
        } else if (strncasecmp(str, VIA_MADDR, 5) == 0) {
            str += 5;
            ptr = &(sipVia->maddr);
        } else if (strncasecmp(str, VIA_RECEIVED, 8) == 0) {
            str += 8;
            ptr = &(sipVia->recd_host);
        } else if (strncasecmp(str, VIA_BRANCH, 6) == 0) {
            str += 6;
            ptr = &(sipVia->branch_param);
        } else {
            ptr = NULL;
        }
        if (ptr) {
            if (*ptr) {
                /* ptr already points to something
                 * -- duplicate param found */
                return FALSE;
            }
            SKIP_LWS(str); /* Skip spaces till equal sign */
            if (*str == EQUAL_SIGN) {
                str++;
                SKIP_LWS(str);
                *ptr = str;
            }
        }
        str = strpbrk(str, ";,");
        if (str == NULL) {
            if (ptr && *ptr) {
                trim_right(*ptr);
            }
            return TRUE;
        }
        if (*str == COMMA) {
            *str++ = 0;
            SKIP_LWS(str);
            sipVia->more_via = str;
            if (ptr && *ptr) {
                trim_right(*ptr);
            }
            return TRUE;
        }
        *str++ = 0;             /* Zero ';' and advance pointer */
        if (ptr && *ptr) {
            trim_right(*ptr);
        }
    }
}

sipVia_t *
sippmh_parse_via (const char *input_via)
{
    static const char fname[] = "sippmh_parse_via";
    char     *via;
    char     *separator;
    sipVia_t *sipVia;
    char     *endptr;
    char     *port_num_str;
    uint16_t  port_num;

    /*
     * Via              = ( "Via" | "v") ":" 1#( sent-protocol sent-by
     *               *( ";" via-params ) [ comment ] )
     * via-params       = via-hidden | via-ttl | via-maddr
     *             | via-received | via-branch
     * via-hidden       = "hidden"
     * via-ttl          = "ttl" "=" ttl
     * via-maddr        = "maddr" "=" maddr
     * via-received     = "received" "=" host
     * via-branch       = "branch" "=" token
     * sent-protocol    = protocol-name "/" protocol-version "/" transport
     * protocol-name    = "SIP" | token
     * protocol-version = token
     * transport        = "UDP" | "TCP" | "TLS" | token
     * sent-by          = ( host [ ":" port ] ) | ( concealed-host )
     * concealed-host   = token
     * ttl              = 1*3DIGIT     ; 0 to 255
     */

    parse_errno = PARSE_ERR_SYNTAX;
    if (input_via == NULL) {
        return NULL;
    }
    sipVia = NULL;
    via = NULL;
    SKIP_LWS(input_via);
    do {
        if (strncasecmp(input_via, "SIP", 3)) {
            /* Unknown protocol-name */
            break;
        }
        input_via += 3;

        SKIP_LWS(input_via);
        if (*input_via != '/') {
            break;
        }
        input_via++; /* Skip '/' */

        SKIP_LWS(input_via);
        /* input_via should now be pointing to version */

        /* Duplicate the string and work with it */
        via = cpr_strdup(input_via);
        if (via == NULL) {
            parse_errno = 0;
            return NULL;
        }
        /* Allocate sipVia_t struct */
        sipVia = (sipVia_t *) cpr_calloc(1, sizeof(sipVia_t));
        if (sipVia == NULL) {
            parse_errno = 0;
            cpr_free(via);
            return NULL;
        }

        /* version also points to the start of memory allocated */
        sipVia->version = via;
        if (strncmp(via, "2.0", 3)) {
            break;
        }
        via += 3; /* Go past version */

        SKIP_LWS(via);
        if (*via != '/') {
            break;
        }
        *via++ = 0; /* Terminate version string  and go past '/' */

        SKIP_LWS(via); /* Point to transport */
        sipVia->transport = via;
        if (strncasecmp(via, "UDP", 3) && strncasecmp(via, "TCP", 3) &&
            strncasecmp(via, "TLS", 3)) {
            /* Invalid transport */
            break;
        }
        via += 3;   /* Go past the transport string */
        *via++ = 0; /* Terminate transport string and advance pointer */


        /* Skip blanks */
        SKIP_LWS(via);
        if (*via == 0) {
            parse_errno = PARSE_ERR_UNEXPECTED_EOS;
            CCSIP_DEBUG_ERROR(parse_errors[parse_errno], fname);            
            break;
        }

        parse_errno = 0;

    } while (0);

    if (parse_errno) {
        if (parse_errno == PARSE_ERR_SYNTAX) {
            CCSIP_DEBUG_ERROR(parse_errors[parse_errno], fname, via ? via : input_via);            
        }
        sippmh_free_via(sipVia);
        return NULL;
    }

    port_num_str = SIP_WELL_KNOWN_PORT_STR;
    sipVia->host = via;
    /*
     * Either we can have a ':' followed by port
     * or start of parameters signalled by ';'
     * or end of this Via indicated by ','
     */
    if (*via == '[') {
        /* IPv6 address */
        sipVia->host = via;
        sipVia->is_ipv6 = TRUE;
        separator = strpbrk(via, "]");

        if (separator && *separator == ']') {
            separator++;
        }
    } else  {

        separator = via;
    }

    if (separator) {
        separator = strpbrk(separator, ":;,");
    }
    if (separator) {
        if (*separator == COLON) {
            /* Port number follows */
            *separator++ = 0;
            port_num_str = separator;
            separator = strpbrk(separator, ";,");
        }
        if (separator) {
            if (*separator == SEMI_COLON) {
                *separator++ = 0;
                /* Parameters follow */
                if (parse_via_params(separator, sipVia) == FALSE) {
                    CCSIP_ERR_DEBUG
                        buginf("%s: Duplicate params in Via\n", fname);
                    sippmh_free_via(sipVia);
                    return NULL;
                }
            } else {
                *separator++ = 0;
                SKIP_LWS(separator);
                sipVia->more_via = separator;
            }
        }
    }

    /* Validate the host portion */
    if ((sipSPI_validate_ip_addr_name(sipVia->host) == FALSE)) {
        CCSIP_ERR_DEBUG buginf("\n%s: Invalid host in Via", fname);
        sippmh_free_via(sipVia);
        return NULL;
    }

    /* Fix host portion */
    trim_right(sipVia->host);
    /* Fix IPv6 host */
    trim_ipv6_host(sipVia->host);

    /* validate port portion */
    port_num = (uint16_t) strtol(port_num_str, &endptr, 10);

    /*
     * There may be trailing white space in the port portion. So, ignore them.
     */
    SKIP_LWS(endptr);
    if (*endptr || port_num == 0) {
        sippmh_free_via(sipVia);
        CCSIP_ERR_DEBUG buginf("\n%s: Invalid port number in Via", fname);
        return NULL;
    }
    sipVia->remote_port = port_num;
    sipVia->recd_host = sipVia->host;
    return sipVia;
}

void
sippmh_free_via (sipVia_t * sipVia)
{
    if (sipVia) {
        cpr_free(sipVia->version);
        cpr_free(sipVia);
    }
}


sipCseq_t *
sippmh_parse_cseq (const char *cseq)
{
    char *lasts = NULL;
    /*
     * CSeq  =  "CSeq" ":" 1*DIGIT Method
     */
    sipCseq_t *sipCseq = (sipCseq_t *) cpr_calloc(1, sizeof(sipCseq_t));

    if (!sipCseq) {
        return (NULL);
    }

/* if ',' is present then more than 1 cseq are present */
    if (strchr(cseq, ',')) {
        cpr_free(sipCseq);
        return (NULL);
    }

    if (cseq) {
        char *mycseq = cpr_strdup(cseq);

        sipCseq->method = sipMethodInvalid;

        if (mycseq) {
            char *this_token = cpr_strtok(mycseq, " ", &lasts);

            if (this_token) {
                sipCseq->number = strtoul(this_token, NULL, 10);

                /* make sure the CSeq value is not > 2^^31 */
                if (sipCseq->number >= TWO_POWER_31) {
                    cpr_free(sipCseq);
                    cpr_free(mycseq);
                    return (NULL);
                }

                this_token = cpr_strtok(NULL, " ", &lasts);
                if (this_token) {
                    sipCseq->method = sippmh_get_method_code(this_token);
                }

            }
            if (!this_token) {
                cpr_free(sipCseq);
                cpr_free(mycseq);
                return (NULL);
            }
            cpr_free(mycseq);
        } else {
            cpr_free(sipCseq);
            return NULL;
        }
    }

    return (sipCseq);
}

sipRet_t
sippmh_add_cseq (sipMessage_t *msg, const char *method, uint32_t seq_no)
{
    sipRet_t retval = STATUS_FAILURE;

    if (msg && method) {
        char cseq[32];

        sprintf(&cseq[0], "%lu %s", (unsigned long) seq_no, method);
        retval = sippmh_add_text_header(msg, SIP_HEADER_CSEQ,
                                        (const char *)&cseq[0]);
    }

    return (retval);
}

/*
 * The Valid SIP URL MUST contain host field.
 * This is not true for TEL URLs.  Only user is mandatory.
 * This check is sufficient for "From" & "To" headers
 * from a GW standpoint. The ReqLine MUST have a "user"
 * in the URL as the GW supports multiple users.
 */
boolean
sippmh_valid_url (genUrl_t *genUrl)
{
    boolean retval = FALSE;

    if (!genUrl) {
        return retval;
    }
    if (genUrl->schema == URL_TYPE_SIP) {
        if (genUrl->u.sipUrl->host && genUrl->u.sipUrl->host[0])
            retval = TRUE;
    } else if (genUrl->schema == URL_TYPE_TEL) {
        if (genUrl->u.telUrl->user)
            retval = TRUE;
    }

    return (retval);
}

/*
 * The Input string is the comma separated cached Record-Route
 * header string. This function duplicates the input string
 * and generates a set of pointers which are pointing to a
 * name-addr format string. These pointers are pointing within
 * the duplicated cached header string.
 *
 *                  Loc 0       Loc 1       Loc 2
 * Loc_start -> name-addr1, name-addr2, name-addr3
 *
 * Loc_start points to the duplicated input string and needs
 * to be freed.
 * Loc_start is preserved so that the string
 * can be freed, when required. Thus this pointer needs to be
 * stored only once.
 */
sipRecordRoute_t *
sippmh_parse_record_route (const char *input_record_route)
{
    char *record_route = NULL, *start_ptr = NULL;
    char *more_ptr;
    sipRecordRoute_t *sipRecordRoute;
    char *rr_other_param = NULL;

    /*
     * Record-Route = "Record-Route" ":" (name-addr *(";" rr-param))
     * name-addr      = [ display-name ] "<" addr-spec ">"
     * addr-spec      = SIP-URL | URI
     * display-name   = *token | quoted-string
     * rr-param       = generic-param
     * generic-param  = token [  =  ( token | host | quoted-string ) ]
     */

    record_route = cpr_strdup(input_record_route);
    if (record_route == NULL) {
        return NULL;
    }

    sipRecordRoute = (sipRecordRoute_t *)
        cpr_calloc(1, sizeof(sipRecordRoute_t));

    if (sipRecordRoute == NULL) {
        cpr_free(record_route);
        return NULL;
    }

    start_ptr = record_route;

    do {
        sipLocation_t *sipLoc;

        more_ptr = NULL;
        parse_errno = 0;
        sipLoc = sippmh_parse_nameaddr_or_addrspec(record_route, start_ptr,
                                                   FALSE, TRUE, &more_ptr);
        if (sipLoc == NULL) {
            if (sipRecordRoute->locations == 0) {
                // Free record_route only if we fail the first location
                // For the others, it will be freed when sipRecordRoute is
                // freed
                cpr_free(record_route);
            }
            sippmh_free_record_route(sipRecordRoute);
            sipRecordRoute = NULL;
            break;
        }

        /* Loc_start points to the duplicated input  string &
         * is stored in the zeroth location of location[].
         * This value needs to be freed only once
         */
        if (sipRecordRoute->num_locations) {
            sipLoc->loc_start = NULL;
        }

        sipRecordRoute->locations[sipRecordRoute->num_locations++] = sipLoc;

        if (more_ptr == NULL || *more_ptr == 0) {
            /* No params, means no qval and assume lowest priority */
            break;
        }

        record_route = more_ptr;

        /*
         * At this point, either record_route is pointing to
         * 1) ',' - Beginning of next location
         * 3) End of string
         * 4) Any other character is syntax error
         */

        /*
         * This following while loop eats up all the rr_params.
         * Note that these 'other_params' are not the same as the ones handled
         * by parseURLParams. That routines parses other params INSIDE the < > brackets.
         * The following code would handle unknown params outside the brackets.
         */
        while (record_route && *record_route == SEMI_COLON) {
            record_route++;
            record_route = parse_other_param(record_route, &rr_other_param);
            /*
             * Record route extensions are not supported. Throw away
             * any that were just parsed.
             */
            if (rr_other_param != NULL) {
                cpr_free(rr_other_param);
            }
        }


        if (record_route && *record_route == COMMA) {
            /* Another location follows */
            *record_route++ = 0;
            SKIP_LWS(record_route);
            if (sipRecordRoute->num_locations == SIP_MAX_LOCATIONS) {
                sippmh_free_record_route(sipRecordRoute);
                sipRecordRoute = NULL;
                CCSIP_ERR_DEBUG {
                    buginf("\nsippmh_parse_record_route: Too many location "
                           "headers in Record-Route header");
                }
                break;
            }
        } else {
            /* Flag error if not "end of header" &
             * any other character other than COMMA
             */
            if (record_route && *record_route) {
                parse_errno = PARSE_ERR_SYNTAX;
                CCSIP_DEBUG_ERROR(parse_errors[parse_errno],
                           "sippmh_parse_record_route", record_route);                
                sippmh_free_record_route(sipRecordRoute);
                sipRecordRoute = NULL;
            }
            break;
        }
    } while (1);

    if (sipRecordRoute) {
        sipRecordRoute->new_flag = TRUE;
    }
    return sipRecordRoute;
}

/*
 *  This function is a placeholder for future expansion.
 *  If deep copy of the record route is needed from one ccb to another 
 *  this function can be used.
 */
sipRecordRoute_t *
sippmh_copy_record_route (sipRecordRoute_t *rr)
{
    //TBD
    return NULL;
}

void
sippmh_free_record_route (sipRecordRoute_t *rr)
{
    int i;

    if (rr) {
        for (i = 0; i < rr->num_locations; ++i) {
            if (rr->locations[i]->loc_start) {
                /* Free the entire header value that we duplicated */
                cpr_free(rr->locations[i]->loc_start);
            }
            /* Free the genUrl_t struct in the sipLocation_t */
            sippmh_genurl_free(rr->locations[i]->genUrl);
            /* Free the sipLocation_t in Record-Route struct */
            cpr_free(rr->locations[i]);
        }
        /* Free the sipContact_t struct */
        cpr_free(rr);
    }
}

cc_content_disposition_t *
sippmh_parse_content_disposition (const char *input_content_disp)
{
    char *content_disp = NULL;
    char *content_disp_start = NULL;
    cc_content_disposition_t *sipContentDisp = NULL;
    char *separator = NULL;
    char *more_ptr = NULL;

    if (input_content_disp == NULL) {
        return NULL;
    }

    content_disp_start = content_disp = cpr_strdup(input_content_disp);
    if (content_disp == NULL) {
        return NULL;
    }

    /*
     * Content-Disposition = "Content-Disposition" ":"
     *                        disposition-type*(";" disposition-param)
     * disposition-type    = "render" "session" "Icon" "alert" "precondition"
     *                        disp-extension-token
     * disposition-param   = "handling" "="
     *                        ("optional"|"required" other-handling)
     *                        generic-param
     * other-handling      = token
     * disp-extension-token = token
     */

    sipContentDisp = (cc_content_disposition_t *)
        cpr_calloc(1, sizeof(cc_content_disposition_t));
    if (sipContentDisp == NULL) {
        cpr_free(content_disp);
        return sipContentDisp;
    }

    /* Set the field to protocol define defaults
     * - Session & required handling
     */
    sipContentDisp->disposition = cc_disposition_session;
    sipContentDisp->required_handling = TRUE;

    /* first token should be disposition-type - so look for
     * a token delimiter either a space or semicolon
     */
    SKIP_LWS(content_disp);
    separator = strpbrk(content_disp, " ;");
    if (separator) {
        if (*separator == ';') {
            *separator = 0;
            more_ptr = separator + 1;
        } else {
            *separator = 0;
            more_ptr = NULL;
        }
    } else {
        more_ptr = NULL;
    }

    /* Parse disposition-type */
    if (strncasecmp(content_disp, SIP_CONTENT_DISPOSITION_SESSION, 7) == 0) {
        sipContentDisp->disposition = cc_disposition_session;
    } else if (strncasecmp(content_disp, SIP_CONTENT_DISPOSITION_PRECONDITION, 12) == 0) {
        sipContentDisp->disposition = cc_disposition_precondition;
    } else if (strncasecmp(content_disp, SIP_CONTENT_DISPOSITION_ICON, 4) == 0) {
        sipContentDisp->disposition = cc_dispostion_icon;
    } else if (strncasecmp(content_disp, SIP_CONTENT_DISPOSITION_ALERT, 5) == 0) {
        sipContentDisp->disposition = cc_disposition_alert;
    } else if (strncasecmp(content_disp, SIP_CONTENT_DISPOSITION_RENDER, 6) == 0) {
        sipContentDisp->disposition = cc_disposition_render;
    } else {
        sipContentDisp->disposition = cc_disposition_unknown;
    }

    if (more_ptr) {
        /* parse disposition-param */
        SKIP_LWS(more_ptr);
        if (strncasecmp(more_ptr, "handling", 8) == 0) {
            more_ptr += 8;
            SKIP_LWS(more_ptr);
            if (*more_ptr == '=') {
                more_ptr++;
                SKIP_LWS(more_ptr);
                if (strncasecmp(more_ptr, "optional", 8) == 0) {
                    sipContentDisp->required_handling = FALSE;
                } else if (strncasecmp(more_ptr, "required", 8) == 0) {
                    sipContentDisp->required_handling = TRUE;
                }
            }
        } else {
            /* Keyword "handling" is not found
             * default values are already populated
             */
        }
    }

    cpr_free(content_disp_start);
    return (sipContentDisp);
}


/***************************************************************
 *
 * Parses headers in Refer-To body in a char array
 *
 **************************************************************/
static uint16_t
sippmh_parse_referto_headers (char *refto_line, char **header_arr)
{
    uint16_t headNum = 0;
    char *tmpHead = NULL;
    char *lasts = NULL;

    if (refto_line == NULL) {
        return 0;
    }

    tmpHead = cpr_strtok(refto_line, "?&", &lasts);
    while (tmpHead != NULL) {
        header_arr[headNum++] = tmpHead;
        tmpHead = cpr_strtok(NULL, "?&", &lasts);
    }
    return headNum;
}


/*******************************************************************
 *
 * Parser routine for headers other than sip url. Parses Replaces
 * header, copies Accept-Contact and Proxy-Authorization headers
 * and skips other headers.
 *
 * Input parameters : Array containing headers in Refer-To body,
 *                    count of number of headers in Refer-To body
 *                    & pointer to Refer-To structure for parsed
 *                    values to be returned to calling function
 *
 * Returns : Zero if success, non-zero otherwise
 *
 *******************************************************************/
static int
sippmh_parse_other_headers (char **headerArr, uint16_t tokCount,
                            sipReferTo_t *refer_to)
{
    uint16_t count = 0;
    char *dup_header, *mhead;

    dup_header = mhead = NULL;

    for (count = 1; count < tokCount; count++) {
        dup_header = cpr_strdup(headerArr[count]);
        if (dup_header == NULL) {
            return (PARSE_ERR_NO_MEMORY);
        }
        mhead = strchr(dup_header, '=');
        if (mhead == NULL) {
            cpr_free(dup_header);
            return (PARSE_ERR_SYNTAX);
        } else {
            *mhead = '\0';
        }
        if (cpr_strcasecmp(dup_header, SIP_HEADER_REPLACES) == 0) {

            char *unescRepl_str = NULL;

            /*
             * copy the Replaces header so that it can be
             * echoed back in the triggered INVITE. Also
             * unescape any escpaed characters.
             */
            headerArr[count] = strstr(headerArr[count], "=");
            if (headerArr[count] == NULL) {
                cpr_free(dup_header);
                return (PARSE_ERR_SYNTAX);
            }
            *(headerArr[count])++ = 0;
            SKIP_LWS(headerArr[count]);

            /* if header body is NULL, return syntax error */
            if (*headerArr[count] == '\0') {
                cpr_free(dup_header);
                return (PARSE_ERR_SYNTAX);
            }

            unescRepl_str = (char *) cpr_malloc(strlen(headerArr[count]) + 1);
            if (unescRepl_str == NULL) {
                cpr_free(dup_header);
                return (PARSE_ERR_NO_MEMORY);
            }

            sippmh_convertEscCharToChar((const char *) headerArr[count],
                                        (uint8_t) strlen(headerArr[count]), unescRepl_str);

            refer_to->sip_replaces_hdr = unescRepl_str;

        } else if (cpr_strcasecmp(dup_header, SIP_HEADER_ACCEPT_CONTACT) == 0 ||
                   cpr_strcasecmp(dup_header, SIP_C_HEADER_ACCEPT_CONTACT) == 0) {
            char *unescAccCont = NULL;

            /*
             * copy unescaped version of this header so
             * that it can be echoed back in triggered INVITE
             */
            headerArr[count] = strstr(headerArr[count], "=");
            if (headerArr[count] == NULL) {
                cpr_free(dup_header);
                return (PARSE_ERR_SYNTAX);
            }
            *(headerArr[count])++ = 0;
            SKIP_LWS(headerArr[count]);

            /* if header body is NULL, return syntax error */
            if (*headerArr[count] == '\0') {
                cpr_free(dup_header);
                return (PARSE_ERR_SYNTAX);
            }

            unescAccCont = (char *) cpr_malloc(strlen(headerArr[count]) + 1);

            if (unescAccCont == NULL) {
                cpr_free(dup_header);
                return (PARSE_ERR_NO_MEMORY);
            }

            sippmh_convertEscCharToChar((const char *) headerArr[count],
                                        (uint8_t) strlen(headerArr[count]),
                                        unescAccCont);

            refer_to->sip_acc_cont = unescAccCont;

        } else if (cpr_strcasecmp(dup_header, SIP_HEADER_PROXY_AUTH) == 0) {

            char *unescPrAuth = NULL;

            /*
             * copy this header to be echoed back in triggered INVITE
             * unescape any escaped chars before storing
             */

            headerArr[count] = strstr(headerArr[count], "=");
            if (headerArr[count] == NULL) {
                cpr_free(dup_header);
                return (PARSE_ERR_SYNTAX);
            }
            *(headerArr[count])++ = 0;
            SKIP_LWS(headerArr[count]);

            /* if header body is NULL, return syntax error */
            if (*headerArr[count] == '\0') {
                cpr_free(dup_header);
                return (PARSE_ERR_SYNTAX);
            }

            unescPrAuth = (char *) cpr_malloc(strlen(headerArr[count]) + 1);
            if (unescPrAuth == NULL) {
                cpr_free(dup_header);
                return (PARSE_ERR_NO_MEMORY);
            }

            sippmh_convertEscCharToChar((const char *) headerArr[count],
                                        (uint8_t) strlen(headerArr[count]),
                                        unescPrAuth);

            refer_to->sip_proxy_auth = unescPrAuth;
        }
        cpr_free(dup_header);
    }

    return 0;  /* SUCCESS */
}

/**************************************************************
 * Parser routine for Refer-To header. Refer-To may contain
 * other headers like:
 * a) Replaces
 * b) Accept-Contact
 * c) Proxy-Authorization
 *  will skip headers, we do not understand; parse Replaces;
 *  and copy Accept-Contact and Proxy-Authorization headers
 *  to be echoed back in the triggered INVITE.
 *
 * Input parameters : Refer-To string to be parsed.
 * Returns : Pointer to Refer-To structure with parsed values
 *           if successful, NULL otherwise.
 *
 **************************************************************/
sipReferTo_t *
sippmh_parse_refer_to (char *input_refer_to)
{
    char *refer_to_headers[MAX_REFER_TO_HEADER_CONTENTS];
    char *left_bracket, *right_bracket, *msg_body, *refer_to_val;
    sipReferTo_t *sipReferTo = NULL;
    genUrl_t *refUrl = NULL;
    uint16_t tokens = 0;
    int ref_header_count = 0;

    left_bracket = right_bracket = msg_body = refer_to_val = NULL;

    if (input_refer_to == NULL) {
        return NULL;
    }

    /*
     * Refer-To = ("Refer-To" | "r") ":" URL
     */

    /* make a copy of refer-to to work with */
    refer_to_val = cpr_strdup(input_refer_to);
    if (refer_to_val == NULL) {
        return NULL;
    }

    sipReferTo = (sipReferTo_t *) cpr_calloc(1, sizeof(sipReferTo_t));
    if (sipReferTo == NULL) {
        cpr_free(refer_to_val);
        return NULL;
    }
    sipReferTo->ref_to_start = refer_to_val;

    /* Initialize */
    for (ref_header_count = 0;
         ref_header_count < MAX_REFER_TO_HEADER_CONTENTS;
         ref_header_count++) {
        refer_to_headers[ref_header_count] = NULL;
    }

    /* first get rid of opening and closing braces, if there */
    left_bracket = strpbrk(refer_to_val, ",<");
    if (left_bracket) {

        /* This is a name-addr form */
        left_bracket++;
        right_bracket = strchr(left_bracket, '>');
        if (right_bracket) {
            *right_bracket++ = 0;
        }
        msg_body = left_bracket;
    } else {

        /* This is addr-spec format */
        msg_body = refer_to_val;
        SKIP_LWS(msg_body);
    }

    /* parse headers separated by ? or & in an array of char pointers */
    tokens = sippmh_parse_referto_headers(msg_body, refer_to_headers);

    if (tokens == 0) {
        CCSIP_ERR_DEBUG {
            buginf("\nEmpty Refer-To header\n");
        }
        sippmh_free_refer_to(sipReferTo);
        return NULL;
    }

    /* First parse url portion, this should be the first element
     * of the headers array
     */
    refUrl = sippmh_parse_url(refer_to_headers[0], FALSE);
    if (refUrl == NULL) {
        sippmh_free_refer_to(sipReferTo);
        return NULL;
    }
    sipReferTo->targetUrl = refUrl;

    /* next parse rest of the headers, skip ones we don't understand */
    parse_errno = sippmh_parse_other_headers(refer_to_headers, tokens,
                                             sipReferTo);

    if (parse_errno != 0) {
        CCSIP_ERR_DEBUG {
            buginf("\nError while parsing other Refer-To header\n");
        }
        sippmh_free_refer_to(sipReferTo);
        return NULL;
    }

    return (sipReferTo);
}





/*****************************************************************
 *
 * Parser routine for Replaces header. Replaces header has
 * call-id, to and from tags and signature scheme as parameters.
 * The parser parses and stores call-id and tags in the Replaces
 * structure and copies the signature scheme to be echoed back
 * in the triggered INVITE for attended transfer.
 *
 * Input parameters : Replaces string to be parsed,
 *                    boolean flag indicating whether the input
 *                    string shall be duplicated or not.
 *
 * Returns : pointer to Replaces structure with parsed values
 *           if successful, NULL otherwise.
 *
 *****************************************************************/
sipReplaces_t *
sippmh_parse_replaces (char *input_repl, boolean dup_flag)
{

    char *repl_str, *this_tok, *tagVal, *sign, *callid;
    sipReplaces_t *repcs = NULL;
    char *lasts = NULL;

    repl_str = this_tok = tagVal = sign = NULL;

    if (input_repl == NULL) {
        return NULL;
    }


    /*
     *  Replaces  = "Replaces" ":" 1#replaces-values
     *  replaces-values  = callid *( ";" replaces-param )
     *  callid  = token [ "@" token ]
     *  replaces-param  = to-tag | from-tag | rep-signature
     *                    | extension-param
     *  to-tag  =  "to-tag=" UUID
     *  from-tag  = "from-tag=" UUID
     *  rep-signature = signature-scheme *( ";" sig-scheme-params)
     *  signature-scheme = "scheme" "=" token
     *  sig-scheme-params = token "=" ( token | quoted string )
     */

    repcs = (sipReplaces_t *) cpr_calloc(1, sizeof(sipReplaces_t));
    if (repcs == NULL) {
        return NULL;
    }

    if (dup_flag) {
        repl_str = cpr_strdup(input_repl);
        if (!repl_str) {
            cpr_free(repcs);
            return NULL;
        }
        repcs->str_start = repl_str;
    } else {
        repl_str = input_repl;
    }


    /* if Replaces string has signature scheme parameter,
     * make a copy of the input string for parsing entire signature
     * parameter
     */

    if ((sign = strstr(repl_str, SIGNATURE_SCHEME)) != NULL) {

        /*
         * if to or from tag follows signature params, remove
         * them from this string
         */
        char *sign_str, *tag_str;

        sign_str = tag_str = NULL;

        sign_str = cpr_strdup(sign);
        if (sign_str == NULL) {
            sippmh_free_replaces(repcs);
            return NULL;
        }
        if ((tag_str = strstr(sign_str, ";to-tag")) ||
            (tag_str = strstr(sign_str, ";from-tag"))) {
            *tag_str = '\0';
        } else {
            /* terminate input string at signature param */
            *sign = '\0';
        }
        (repcs->signature_scheme) = sign_str;
    }


    this_tok = cpr_strtok(input_repl, ";", &lasts);
    while (this_tok != NULL) {
        if (strncasecmp(this_tok, TO_TAG, 6) == 0) {
            if (repcs->toTag != NULL) {
                sippmh_free_replaces(repcs);
                return NULL; /* ERROR - more than 1 TO tag */
            } else {
                tagVal = strchr(this_tok, '=');
                if (tagVal) {
                    tagVal++;
                    SKIP_LWS(tagVal);
                    repcs->toTag = tagVal;
                } else {
                    sippmh_free_replaces(repcs);
                    return NULL;    /* ERROR */
                }
            }
        } else if (strncasecmp(this_tok, FROM_TAG, 8) == 0) {
            if (repcs->fromTag != NULL) {
                sippmh_free_replaces(repcs);
                return NULL; /* ERROR - more than 1 FROM tag */
            } else {
                tagVal = strchr(this_tok, '=');
                if (tagVal) {
                    tagVal++;
                    SKIP_LWS(tagVal);
                    repcs->fromTag = tagVal;
                } else {
                    sippmh_free_replaces(repcs);
                    return NULL; /* ERROR */
                }
            }
        } else if (strncasecmp(this_tok, SIP_HEADER_REPLACES,
                               sizeof(SIP_HEADER_REPLACES) - 1) == 0) {
            /* only other allowable field should be callid */
            callid = strchr(this_tok, '=');
            if (callid) {
                char *sp_token;

                callid++;
                SKIP_LWS(callid);
                repcs->callid = callid;
                /* remove trailing spaces */
                sp_token = strchr(repcs->callid, ' ');
                if (sp_token == NULL) {
                    sp_token = strchr(repcs->callid, '\t');
                }
                if (sp_token) {
                    *sp_token = 0;
                }
            } else {
                sippmh_free_replaces(repcs);
                return NULL; /* ERROR */
            }
        } else {
            sippmh_free_replaces(repcs);
            return NULL;
        }
        this_tok = cpr_strtok(NULL, ";", &lasts);
    }
    /* check for errors */
    if ((repcs->callid == NULL) ||
        (repcs->toTag == NULL) ||
        (repcs->fromTag == NULL)) {
        sippmh_free_replaces(repcs);
        return NULL;
    }

    return repcs; /* SUCCESS */
}


/*
 * Converts a hex character (0-9, a-f, or A-F) into its integer
 * equivalent value.
 *    ex.)  '9' --> 9
 *          'a' --> 10
 *          'F' --> 15
 */
static int
sippmh_htoi (const char inputChar)
{
    int value = 0;
    char inputValue[2];

    if (isdigit((int) inputChar)) {
        inputValue[0] = inputChar;
        inputValue[1] = '\0';
        value = atoi(inputValue);
    } else if (isupper((int) inputChar)) {
        value = 9 + (inputChar - 'A' + 1);
    } else if (islower((int) inputChar)) {
        value = 9 + (inputChar - 'a' + 1);
    }

    return (value);
}

/*
 * This function(similar to memchr()) works with both strings and array of characters.
 * Returns the pointer to the matched character, if any; NULL otherwise. To include memchr.c
 * in the phone library takes 1K of extra size and hence wrote this.
 */
static void *
char_lookup (const void *src, unsigned char c, int n)
{
    const unsigned char *mem = (unsigned char *) src;

    while (n--) {
        if (*mem == c) {
            return ((void *) mem);
        }
        mem++;
    }

    return NULL;
}


/* List of characters that can exist in user part of URL/URI without escaping */
#define  userUnResCharListSize  17
static char userUnResCharList[userUnResCharListSize] =
{
    SEMI_COLON, EQUAL_SIGN, TILDA, STAR, UNDERSCORE, PLUS,
    SINGLE_QUOTE, LEFT_PARENTHESIS, RIGHT_PARENTHESIS, DOLLAR_SIGN,
    FORWARD_SLASH, DOT, DASH, EXCLAMATION, AMPERSAND, COMMA, QUESTION_MARK
};


/*
 * This utility function converts selected characters in a string to escaped characters,
 * per RFC2396. Escape format = %hexhex (hexhex - hex value of ASCII character)
 *   ex.  'abc@123' gets converted to "abc%40123" if '@' exists in escapable character list
 *
 * inptStr     = ptr to string of chars
 * inputStrLen = num of characters that you want to convert
 * excludeCharTbl  = lookup table/string of characters that need not be escaped
 * excludeCharTblSize = number of characters in the table/string that need not be escaped
 * null_terminate = TRUE if '\0' is required to be added at end
 *                  FALSE if not required.
 * outputStr   = ptr to pre-allocated memory for storing the
 *               converted string
 * outputStrSize = the size limit of the outputStr buffer
 *
 * returns the new size of the output string
 * Converted string is stored in outputStr
 *                          (contents = converted str)
 */
static size_t
sippmh_escapeChars_util (const char *inputStr, size_t inputStrLen,
                         char *excludeCharTbl, size_t excludeCharTblSize,
                         char *outputStr, size_t outputStrSize,
                         boolean null_terminate)
{
    size_t char_cnt = 0;
    char  *nextOutputChar;
    int    additional_byte = null_terminate ? 1 : 0;
    size_t copy_count = 0;

    nextOutputChar = outputStr;

    while (char_cnt++ < inputStrLen) {
        /* Check for characters that doesn't need to be escaped */
        if (((*inputStr) >= 'A' && (*inputStr) <= 'Z') ||
            ((*inputStr) >= 'a' && (*inputStr) <= 'z') ||
            ((*inputStr) >= '0' && (*inputStr) <= '9') ||
            (char_lookup(excludeCharTbl, (unsigned char) *inputStr,
            excludeCharTblSize) != NULL)) {
            if (outputStrSize < (copy_count + 1 + additional_byte)) {
                break;
            }
            *nextOutputChar++ = *inputStr++;
            copy_count++;
        } else {  // Characters that need escaping
            if (outputStrSize < (copy_count + 3 + additional_byte)) {
                break;
            }
            /* Copy %hh to output string */
            sprintf(nextOutputChar, "%c%x", '%', *inputStr++);
            nextOutputChar = nextOutputChar + 3;
            copy_count += 3;
        }

    }

    if (null_terminate) {
        copy_count++;
        *nextOutputChar = '\0';
    }
    return (copy_count);
}

/*
 * This function converts selected characters in a string to escaped characters,
 * per RFC2396. Escape format = %hexhex (hexhex - hex value of ASCII character)
 *   ex.  'abc@123' gets converted to "abc%40123" if '@' exists in escapable character list
 *
 * inptStr     = ptr to string of chars
 * inputStrLen = num of characters that you want to convert
 * outputStr   = ptr to pre-allocated memory for storing the
 *               converted string
 * outputStrSize = the size limit of the outputStr buffer
 * null_terminate = TRUE if '\0' is required to be added at end
 *                  FALSE if not required.
 * returns the new size of the output string
 *
 * Converted string is stored in outputStr
 *                          (contents = converted str)
 */
size_t
sippmh_convertURLCharToEscChar (const char *inputStr, size_t inputStrLen,
                                char *outputStr, size_t outputStrSize,
                                boolean null_terminate)
{
    return (sippmh_escapeChars_util(inputStr, inputStrLen,
                                    userUnResCharList, userUnResCharListSize,
                                    outputStr, outputStrSize, null_terminate));
}
/*
 * This function converts any '\' or '"' character in a quoted string to escaped characters,
 * per RFC3261. Escape format = %hexhex (hexhex - hex value of ASCII character)
 *   ex.  'abc\\' gets converted to "abc%5c%5c" if '\' exists in escapable character list
 *
 * inptStr     = ptr to string of chars
 * inputStrLen = num of characters that you want to convert
 * outputStr   = ptr to pre-allocated memory for storing the
 *               converted string
 * outputStrSize = the size limit of the outputStr buffer
 * null_terminate = TRUE if '\0' is required to be added at end
 *                  FALSE if not required.
 * returns the new size of the output string
 *
 * Converted string is stored in outputStr
 *                          (contents = converted str)
 */
size_t
sippmh_converQuotedStrToEscStr(const char *inputStr, size_t inputStrLen,
                         char *outputStr, size_t outputStrSize,
                         boolean null_terminate)
{
    size_t char_cnt = 0;
    char  *nextOutputChar;
    int    additional_byte = null_terminate ? 1 : 0;
    size_t copy_count = 0;

    nextOutputChar = outputStr;

    while (char_cnt++ < inputStrLen) {
        /* Check for characters that need to be escaped */
        if( ((*inputStr) == ESCAPE_CHAR)  || ((*inputStr) == DOUBLE_QUOTE))
         {
            // Characters that need escaping
             if (outputStrSize < (copy_count + 3 + additional_byte)) {
                break;
            }
            /* Copy %hh to output string */
            sprintf(nextOutputChar, "%c%02x", '%', *inputStr++);
            nextOutputChar = nextOutputChar + 3;
            copy_count += 3;
        }
        else {
            if (outputStrSize < (copy_count + 1 + additional_byte)) {
                break;
            }
            *nextOutputChar++ = *inputStr++;
            copy_count++;
        }

    }

    if (null_terminate) {
        copy_count++;
        *nextOutputChar = '\0';
    }

    return (copy_count);
}

/*
 * This function converts all Escaped characters in a string
 * to their non-escaped format according to RFC2396.
 * Escape format = %hh   (%<hex equivalent of ASCII char>)
 *   ex.  'abc%40123' gets converted to "abc@123"
 *
 * inptStr     = ptr to string of chars
 * inputStrLen = num of characters that you want to convert
 * outputStr   = ptr to pre-allocated memory for storing the
 *               converted string
 *
 * Converted string is stored in outputStr
 *                          (contents = converted str)
 *                          (contents = '\0' if no input string)
 */
void
sippmh_convertEscCharToChar (const char *inputStr, size_t inputStrLen,
                             char *outputStr)
{
    size_t  char_cnt = 0;
    char   *nextOutputChar;
    int     value;
    boolean esc_char_found = FALSE;

    *outputStr = '\0';
    nextOutputChar = outputStr;

    while (char_cnt < inputStrLen) {
        if (*inputStr == PERCENT) {
            /* Skip ESC syntax */
            inputStr++;
            char_cnt++;
            esc_char_found = TRUE;
        }

        /* Copy character to output str */
        if (esc_char_found) {

            /* Convert 1st and 2nd hex chars to int */
            value = sippmh_htoi(*inputStr) * 16;
            inputStr++;
            char_cnt++;
            value += sippmh_htoi(*inputStr);
            sprintf(nextOutputChar, "%c", toascii(value));

            inputStr++;
            char_cnt++;
            esc_char_found = FALSE;

        } else {
            *nextOutputChar = *inputStr;
            inputStr++;
            char_cnt++;
        }

        nextOutputChar++;
    }

    *nextOutputChar = '\0';
}


void
sippmh_free_replaces (sipReplaces_t *repl)
{

    if (repl == NULL) {
        return;
    }
    if (repl->str_start) {
        cpr_free(repl->str_start);
    }
    if (repl->signature_scheme) {
        cpr_free(repl->signature_scheme);
    }
    cpr_free(repl);
}


void
sippmh_free_refer_to (sipReferTo_t *ref_to)
{

    if (ref_to == NULL) {
        return;
    }
    if (ref_to->ref_to_start) {
        cpr_free(ref_to->ref_to_start);
    }
    if (ref_to->sip_replaces_hdr) {
        cpr_free(ref_to->sip_replaces_hdr);
    }
    if (ref_to->sip_acc_cont) {
        cpr_free(ref_to->sip_acc_cont);
    }
    if (ref_to->sip_proxy_auth) {
        cpr_free(ref_to->sip_proxy_auth);
    }
    if (ref_to->targetUrl) {
        sippmh_genurl_free(ref_to->targetUrl);
    }
    cpr_free(ref_to);

}


static void
sippmh_free_remote_party_id (sipRemotePartyId_t *remote_party_id)
{
    if (remote_party_id) {
        if (remote_party_id->loc) {
            sippmh_free_location(remote_party_id->loc);
        }
        cpr_free(remote_party_id);
    }
}


void
sippmh_free_remote_party_id_info (sipRemotePartyIdInfo_t *rpid_info)
{
    uint32_t i;

    if (rpid_info) {
        if (rpid_info->num_rpid > 0) {
            for (i = 0; i < rpid_info->num_rpid; i++) {
                sippmh_free_remote_party_id(rpid_info->rpid[i]);
                rpid_info->rpid[i] = NULL;
            }
        }
        cpr_free(rpid_info);
    }
}

void
sippmh_free_diversion_info (sipDiversionInfo_t *div_info)
{
    if (div_info) {
        if (div_info->orig_called_name) {
            strlib_free(div_info->orig_called_name);
        }
        if (div_info->orig_called_number) {
            strlib_free(div_info->orig_called_number);
        }
        if (div_info->last_redirect_name) {
            strlib_free(div_info->last_redirect_name);
        }
        if (div_info->last_redirect_number) {
            strlib_free(div_info->last_redirect_number);
        }
        cpr_free(div_info);
    }

}

static boolean
sippmh_parse_remote_party_id_params (char *params,
                                     sipRemotePartyId_t *remote_party_id)
{
    boolean params_good;
    int param_len;
    char *param_name;

    if ((params == NULL) || (remote_party_id == NULL)) {
        return FALSE;
    }

    while (1) {
        params_good = FALSE;

        while (*params == ';') {
            params++;
        }

        param_name = params;
        SKIP_SIP_TOKEN(params);
        param_len = params - param_name;
        if (param_len == 0) {
            return FALSE;
        }

        /* Parse screen parameter */
        /* If there are multiple screen parameters, "no" takes precedence */
        if ((param_len == RPID_SCREEN_LEN) &&
            (strncasecmp(param_name, RPID_SCREEN, RPID_SCREEN_LEN) == 0) &&
            (!remote_party_id->screen ||
             cpr_strcasecmp(remote_party_id->screen, "no"))) {
            params = parse_generic_param(params, &remote_party_id->screen);
            if (params == NULL) {
                return FALSE;
            } else {
                params_good = TRUE;
            }

        /* Parse party-type parameter */
        } else if ((param_len == RPID_PARTY_TYPE_LEN) &&
                   (strncasecmp(param_name, RPID_PARTY_TYPE,
                                RPID_PARTY_TYPE_LEN) == 0)) {
            params = parse_generic_param(params, &remote_party_id->party_type);
            if (params == NULL) {
                return FALSE;
            } else {
                params_good = TRUE;
            }

        /* Parse id-type parameter */
        } else if ((param_len == RPID_ID_TYPE_LEN) &&
                   (strncasecmp(param_name, RPID_ID_TYPE,
                                RPID_ID_TYPE_LEN) == 0)) {
            params = parse_generic_param(params, &remote_party_id->id_type);
            if (params == NULL) {
                return FALSE;
            } else {
                params_good = TRUE;
            }

        /* Parse privacy parameter */
        } else if ((param_len == RPID_PRIVACY_LEN) &&
                   (strncasecmp(param_name, RPID_PRIVACY,
                                RPID_PRIVACY_LEN) == 0)) {
            params = parse_generic_param(params, &remote_party_id->privacy);
            if (params == NULL) {
                return FALSE;
            } else {
                params_good = TRUE;
            }

        /* Parse np parameter */
        } else if ((param_len == RPID_NP_LEN) &&
                   (strncasecmp(param_name, RPID_NP, RPID_NP_LEN) == 0)) {
            params = parse_generic_param(params, &remote_party_id->np);
            if (params == NULL) {
                return FALSE;
            } else {
                params_good = TRUE;
            }
        } 

        SKIP_LWS(params);
        if (*params == SEMI_COLON) {
            /* More parameters follow */
            *params++ = '\0';
            SKIP_LWS(params);
        } else {
            break;
        }
    }

    return params_good;
}

sipRemotePartyId_t *
sippmh_parse_remote_party_id (const char *input_remote_party_id)
{
    /* static const char fname[] = "sippmh_parse_remote_party_id"; */
    char *more_ptr;
    sipRemotePartyId_t *remote_party_id;
    char *remote_party_id_str;

    /*
     * Remote-Party-ID = "Remote-Party-ID" ":" [display-name]
     *                   "<" addr-spec ">" *(";" rpi-token)
     * rpi-token       = rpi-screen | rpi-pty-type |
     *                   rpi-id-type | rpi-privacy | rpi-np |
     *                   other-rpi-token
     * rpi-screen      = "screen" "=" ("no" | "yes")
     * rpi-pty-type    = "party" "=" ("calling" | "called" | token)
     * rpi-id-type     = "id-type" "=" ("subscriber" | "user" | "alias" |
     *                                  "return" | "term" | token)
     * rpi-privacy     = "privacy" = 1#(
     *                     ("full" | "name" | "uri" | "off" | token)
     *                     ["-" ("network" | token) ] )
     * rpi-np = "np" "=" ("ordinary" | "residential" | "business" |
     *                    "priority" | "hotel" | "failure" | "hospital" |
     *                    "prison" | "police" | "test" | "payphone" |
     *                    "coin" | "payphone-public" | "payphone-private" |
     *                    "coinless" | "restrict" | "coin-restrict" |
     *                    "coinless-restrict" | "reserved" | "operator" |
     *                    "trans-freephone" | "isdn-res" | "isdn-bus" |
     *                    "unknown" | "emergency" | token )
     * other-rpi-token = ["-"] token ["=" (token | quoted-string)]
     */

    remote_party_id = (sipRemotePartyId_t *)
        cpr_calloc(1, sizeof(sipRemotePartyId_t));
    if (remote_party_id == NULL) {
        return NULL;
    }

    /* Duplicate the string and work with it -
     * This duplicate string is stored in the location structure
     * pointed to by remote_party_id->loc and freed when the that
     * structure is freed. Note that this pointer is *not* dup'ed
     * in the parse_nameaddr_or_addrspec structure
     */
    remote_party_id_str = cpr_strdup(input_remote_party_id);

    if (remote_party_id_str == NULL) {
        sippmh_free_remote_party_id(remote_party_id);
        remote_party_id = NULL;
        more_ptr = NULL;
    } else {
        remote_party_id->loc =
            sippmh_parse_nameaddr_or_addrspec(remote_party_id_str,
                                              remote_party_id_str, FALSE,
                                              FALSE, &more_ptr);

        if (remote_party_id->loc == NULL) {
            cpr_free(remote_party_id_str);
            sippmh_free_remote_party_id(remote_party_id);
            remote_party_id = NULL;
            more_ptr = NULL;
        }
    }

    if (more_ptr == NULL || *more_ptr == 0) {
        /* No params. Use defaults of:
         *     party=calling, screen=no, id-type=subscriber,
         *     privacy=off, np=0 (ordinary)
         */
    } else {
        (void) sippmh_parse_remote_party_id_params(more_ptr, remote_party_id);
    }

    return remote_party_id;
}


char *
sippmh_generate_authorization (sip_author_t *sip_author)
{
    char *buffer;

    /*
     * This routine takes a sip_author_t struct and generates an Authorization
     * header or a Proxy-Authorization header.  Since this routine generates
     * a header for Authorization or Proxy-Authorization, the header will
     * start with "Digest" or "Basic".  The user should use strncat to
     * add "Proxy Authorization: " or "Authorization: " depending which is
     * needed.
     */
    if (!sip_author) {
        return NULL;
    }
    buffer = (char *) cpr_malloc(MAX_SIP_HEADER_LENGTH);
    if (!buffer) {
        return NULL;
    }

    if (sip_author->scheme == SIP_DIGEST) {
        snprintf(buffer, MAX_SIP_HEADER_LENGTH,
                 "%s %s=\"%s\",%s=\"%s\",%s=\"%s\",%s=\"%s\",%s=\"%s\"",
                 AUTHENTICATION_DIGEST,
                 AUTHENTICATION_USERNAME, sip_author->d_username,
                 AUTHENTICATION_REALM, sip_author->realm,
                 AUTHENTICATION_URI, sip_author->unparsed_uri,
                 AUTHENTICATION_RESPONSE, sip_author->response,
                 AUTHENTICATION_NONCE, sip_author->nonce);

        /*
         * The header must have username, realm, uri, response, and nonce.
         * All other parameters are optional, so check if they exist and
         * add them on to the buffer if they do.
         */

        if (sip_author->opaque) {
            char *buffer2;

            buffer2 = (char *) cpr_malloc(MAX_URI_LENGTH);
            if (!buffer2) {
                cpr_free(buffer);
                return NULL;
            }
            snprintf(buffer2, MAX_URI_LENGTH, ",%s=\"%s\"",
                     AUTHENTICATION_OPAQUE, sip_author->opaque);
            strncat(buffer, buffer2, MAX_SIP_HEADER_LENGTH - strlen(buffer) - 1);
            cpr_free(buffer2);
        }
        if (sip_author->cnonce) {
            char *buffer3;

            buffer3 = (char *) cpr_malloc(MAX_URI_LENGTH);
            if (!buffer3) {
                cpr_free(buffer);
                return NULL;
            }
            snprintf(buffer3, MAX_URI_LENGTH,
                     ",cnonce=\"%s\"", sip_author->cnonce);
            strncat(buffer, buffer3, MAX_SIP_HEADER_LENGTH - strlen(buffer) - 1);
            cpr_free(buffer3);
        }
        if (sip_author->qop) {
            char *buffer4;

            buffer4 = (char *) cpr_malloc(MAX_URI_LENGTH);
            if (!buffer4) {
                cpr_free(buffer);
                return NULL;
            }
            snprintf(buffer4, MAX_URI_LENGTH, ",qop=%s", sip_author->qop);
            strncat(buffer, buffer4, MAX_SIP_HEADER_LENGTH - strlen(buffer) - 1);
            cpr_free(buffer4);
        }
        if (sip_author->nc_count) {
            char *buffer5;

            buffer5 = (char *) cpr_malloc(MAX_URI_LENGTH);
            if (!buffer5) {
                cpr_free(buffer);
                return NULL;
            }
            snprintf(buffer5, MAX_URI_LENGTH, ",nc=%s", sip_author->nc_count);
            strncat(buffer, buffer5, MAX_SIP_HEADER_LENGTH - strlen(buffer) - 1);
            cpr_free(buffer5);
        }
        if (sip_author->algorithm) {
            char *buffer6;

            buffer6 = (char *) cpr_malloc(MAX_URI_LENGTH);
            if (!buffer6) {
                cpr_free(buffer);
                return NULL;
            }
            snprintf(buffer6, MAX_URI_LENGTH,
                     ",%s=%s", AUTHENTICATION_ALGORITHM, sip_author->algorithm);
            strncat(buffer, buffer6, MAX_SIP_HEADER_LENGTH - strlen(buffer) - 1);
            cpr_free(buffer6);
        }
    } else {
        sprintf(buffer, "%s %s", AUTHENTICATION_BASIC, sip_author->user_pass);
    }
    return buffer;
}

void
sippmh_free_authen (sip_authen_t *sip_authen)
{
    if (sip_authen) {
        cpr_free(sip_authen->str_start);
        cpr_free(sip_authen);
    }
}

static boolean
sippmh_validate_authenticate (sip_authen_t *sip_authen)
{
    if (sip_authen) {
        if ((sip_authen->realm != NULL) && (sip_authen->nonce != NULL)) {
            return TRUE;
        } else {
            return FALSE;
        }
    }
    return FALSE;
}

/* routine used to parse WWW-Authenticate and Proxy-Authenticate headers */

sip_authen_t *
sippmh_parse_authenticate (const char *input_char)
{
    sip_authen_t *sip_authen;
    char *input;
    boolean good_params;
    boolean qop_found = FALSE;
    char *trash;

/*
 * Basic:
 * "WWW-Authenticate" ":" 1#challenge
 * challenge   = "Basic" realm
 *******************************************************
 * Digest:
 * "Digest" digest-challenge
 * digest-challenge  = 1#( realm | [ domain ] | nonce |
 *                     [ opaque ] |[ stale ] | [ algorithm ] |
 *                     [ qop-options ] | [auth-param] )
 *
 * domain            = "domain" "=" <"> URI ( 1*SP URI ) <">
 * URI               = absoluteURI | abs_path
 * opaque            = "opaque" "=" quoted-string
 * stale             = "stale" "=" ( "true" | "false" )
 * algorithm         = "algorithm" "=" ( "MD5" | "MD5-sess" |
 *                     token )
 * qop-options       = "qop" "=" <"> 1#qop-value <">
 * qop-value         = "auth" | "auth-int" | token
 */

    if (!input_char) {
        return NULL;
    }

    input = cpr_strdup(input_char);
    if (!input) {
        return NULL;
    }

    sip_authen = (sip_authen_t *) cpr_calloc(1, sizeof(sip_authen_t));
    if (!sip_authen) {
        cpr_free(input);
        return NULL;
    }

    sip_authen->str_start = input;
    SKIP_WHITE_SPACE(input);

    /* now pointing at Basic, pgp or Digest */

    if (strncasecmp(input, AUTHENTICATION_BASIC, 5) == 0) {
        sip_authen->scheme = SIP_BASIC;
        input += 5;
        *input = 0;
        input++;
        SKIP_WHITE_SPACE(input);
        if (strncasecmp(input, AUTHENTICATION_REALM, 5) == 0) {
            input += 5;
            SKIP_WHITE_SPACE(input);
            if (*input == '=') {
                input++;
            } else {
                sippmh_free_authen(sip_authen);
                CCSIP_ERR_DEBUG {
                    buginf("\n%s", get_debug_string(DEBUG_PMH_INCORRECT_SYNTAX));
                }
                return NULL;
            }
            SKIP_WHITE_SPACE(input);
            sip_authen->realm = input;
            return sip_authen;
        }
        sip_authen->user_pass = input;  /* encoded base 64 */
        return sip_authen;

    } else if (strncasecmp(input, AUTHENTICATION_DIGEST, 6) == 0) {
        sip_authen->scheme = SIP_DIGEST;
        input += 6;
        *input = 0;
        input++;
        SKIP_WHITE_SPACE(input);
    } else {
        sippmh_free_authen(sip_authen);
        CCSIP_ERR_DEBUG {
            buginf("\n%s", get_debug_string(DEBUG_PMH_INVALID_SCHEME));
        }
        return NULL;
    }

    /* pointing at Digest-challenge */

    /* if algorithm is not in string set it to md5 */

    sip_authen->algorithm = "md5";

    /* loop through string until all fields have been handled */

    while (input) {
        char **ptr = NULL;

        if (strncasecmp(input, AUTHENTICATION_DOMAIN, 6) == 0) {
            input += 6;
            ptr = &(sip_authen->unparsed_domain);
        } else if (strncasecmp(input, AUTHENTICATION_ALGORITHM, 9) == 0) {
            input += 9;
            ptr = &(sip_authen->algorithm);
        } else if (strncasecmp(input, AUTHENTICATION_OPAQUE, 6) == 0) {
            input += 6;
            ptr = &(sip_authen->opaque);
        } else if (strncasecmp(input, "stale", 5) == 0) {
            input += 5;
            ptr = &(sip_authen->stale);
        } else if (strncasecmp(input, AUTHENTICATION_REALM, 5) == 0) {
            input += 5;
            ptr = &(sip_authen->realm);
        } else if (strncasecmp(input, AUTHENTICATION_NONCE, 5) == 0) {
            input += 5;
            ptr = &(sip_authen->nonce);
        } else if (strncasecmp(input, "qop", 3) == 0) {
            input += 3;
            ptr = &(sip_authen->qop);
            qop_found = TRUE;
        } else {
            /*
             * Ignore unrecognized parameters.
             */
            ptr = &trash;
            input = strchr(input, EQUAL_SIGN);
            if (input == NULL) {
                sippmh_free_authen(sip_authen);
                CCSIP_ERR_DEBUG {
                    buginf("\n%s", get_debug_string(DEBUG_PMH_INVALID_FIELD_VALUE));
                }
                return NULL;
            }
        }
        SKIP_WHITE_SPACE(input);
        if (*input != EQUAL_SIGN) {
            sippmh_free_authen(sip_authen);
            CCSIP_ERR_DEBUG {
                buginf("\n%s", get_debug_string(DEBUG_PMH_INCORRECT_SYNTAX));
            }
            return NULL;
        }
        input++; /* skip the equal sign */
        SKIP_WHITE_SPACE(input);
        /* input is now pointing at the value of the parameter */
        *ptr = input;
        if (*input == DOUBLE_QUOTE) {
            input++;
            *ptr = input; /*sam do not want the quotes in the string */
            input = strchr(input, DOUBLE_QUOTE);
            if (input == NULL) {
                sippmh_free_authen(sip_authen);
                CCSIP_ERR_DEBUG {
                    buginf("\n%s", get_debug_string(DEBUG_PMH_INCORRECT_SYNTAX));
                }
                return NULL;
            } else {
                *input++ = '\0';    /*sam string needs to be null terminated */
            }
            /*
             * Check the qop. If qop is included it has to be either
             * auth, auth-int or both. If both are included then we will
             * just drop the second one.
             */
            if (qop_found == TRUE) {
                char *qop_input = NULL;

                if (sip_authen->qop) {
                    qop_input = strchr(sip_authen->qop, COMMA);
                }
                if (qop_input != NULL) {
                    *qop_input = '\0';
                }

                /*
                 * Make sure that the qop is either auth or auth-int
                 */
                if ((strncasecmp(sip_authen->qop, "auth", 4) != 0) &&
                    (strncasecmp(sip_authen->qop, "auth-int", 8) != 0)) {
                    sip_authen->qop = NULL;
                }
            }
        }

        input = strchr(input, COMMA);
        if (!input) {
            good_params = sippmh_validate_authenticate(sip_authen);
            if (good_params) {
                return sip_authen;
            } else {
                sippmh_free_authen(sip_authen);
                CCSIP_ERR_DEBUG {
                    buginf("\n%s", get_debug_string(DEBUG_PMH_NOT_ENOUGH_PARAMETERS));
                }
                return NULL;
            }
        }
        *input++ = 0;
        SKIP_WHITE_SPACE(input);
    }
    
    sippmh_free_authen(sip_authen);
    return NULL;
}

/*
 *  Function: sippmh_parse_displaystr
 *
 *  Parameters: Display string to be parsed
 *
 *  Description:Removes leading <sip: and every character
 *              after : in string
 *
 *  Returns:  Pointer to parsed string
 *
 */

string_t
sippmh_parse_displaystr (string_t displaystr)
{
    char temp_displaystr[CC_MAX_DIALSTRING_LEN];
    char *temp_ptr;
    char *temp;

    sstrncpy(temp_displaystr, displaystr, CC_MAX_DIALSTRING_LEN);
    temp_ptr = (char *) temp_displaystr;

    temp = strcasestr(temp_ptr, "sip:");
    if (temp) {
        temp_ptr = temp + 4;
    }
    temp = strchr(temp_ptr, ':');
    if (temp) {
        *temp = 0;
    }
    temp = strchr(temp_ptr, '?');
    if (temp) {
        *temp = 0;
    }
    temp = strchr(temp_ptr, ';');
    if (temp) {
        *temp = 0;
    }
    temp = strchr(temp_ptr, '>');
    if (temp) {
        *temp = 0;
    }
    displaystr = strlib_update(displaystr, temp_ptr);
    return (displaystr);
}

/*
 *  Function: sippmh_process_via_header
 *
 *  Parameters: Received Sip Message
 *              IP Address the Message was received from
 *
 *  Description: Appends received=<ip_address> to the first Via
 *               field if the ip address we received this message
 *               from is not the same as the ip address in the via
 *               field or if it contains a domain name
 *
 *  Returns: None
 *
 */
void
sippmh_process_via_header (sipMessage_t *sip_message,
                           cpr_ip_addr_t *source_ip_address)
{
    char dotted_ip[MAX_IPADDR_STR_LEN];
    char *hdr_start;
    char *new_buf;
    char *offset;
    long old_header_offset;
    sipVia_t *via;
    cpr_ip_addr_t tmp_ip_addr;

    CPR_IP_ADDR_INIT(tmp_ip_addr);

    if (sip_message && sippmh_is_request(sip_message)) {
        via = sippmh_parse_via(sip_message->hdr_cache[VIA].val_start);
        if (via == NULL) {
            /* error message already displayed in parse function */
            /* mark the message as being incomplete so a 400 will get sent */
            sip_message->is_complete = 0;
            return;
        }

        util_ntohl(&tmp_ip_addr, source_ip_address);
        ipaddr2dotted(dotted_ip, &tmp_ip_addr);
        if (strcmp(via->host, dotted_ip) && (via->recd_host == NULL)) {
            hdr_start = sip_message->hdr_cache[VIA].hdr_start;

            /*
             * +3 accounts for the ; and the = before and after the received
             * and the terminating NULL
             */
            new_buf = (char *) cpr_malloc(strlen(hdr_start) +
                                          sizeof(VIA_RECEIVED) +
                                          strlen(dotted_ip) + 2);
            /*
             * If we cannot allocate memory, we will just leave
             * the VIA alone and hope things work out for the best
             */
            if (new_buf != NULL) {
                offset = strchr(hdr_start, ',');
                old_header_offset = sip_message->hdr_cache[VIA].val_start -
                    hdr_start;
                sip_message->hdr_cache[VIA].hdr_start = new_buf;
                sip_message->hdr_cache[VIA].val_start = new_buf +
                    old_header_offset;

                if (offset) {
                    strncpy(new_buf, hdr_start, offset - hdr_start);
                    new_buf[offset - hdr_start] = '\0';
                } else {
                    strcpy(new_buf, hdr_start);
                }
                strcat(new_buf, ";");
                strcat(new_buf, VIA_RECEIVED);
                strcat(new_buf, "=");
                strcat(new_buf, dotted_ip);
                if (offset) {
                    strcat(new_buf, offset);
                }

                cpr_free(hdr_start);
            }
        }
        sippmh_free_via(via);
    }
}


/*
 *  Function: sippmh_parse_user
 *
 *  Parameters: user portion of URL to parse
 *
 *  Description: This function will remove any parameters
 *          which appear in the user portion of the url.
 *    (i.e. 15726;oct3=128@10.10.10.10 will return 15726)
 *    The SIP Phone does not care about the extra parameters and
 *    they need to be removed so that the user portion will
 *    match the phone provisioning and display correctly.
 *
 *  Returns: A Copy of the input string with the parameters removed
 *           if things go well.  NULL if we encounter an error.
 */
char *
sippmh_parse_user (char *url_main)
{
    char *user = NULL;
    size_t size = 0;
    char *lasts = NULL;

    if (url_main == NULL) {
        return (NULL);
    }

    /*
     * If the first char is ';', give up.  That
     * breaks our assumption.
     */
    if (*url_main == SEMI_COLON) {
        return (NULL);
    }

    /*
     * Create a new string and copy the url_main
     * string into it.  (We can't change the original
     * string because it may be needed for creating
     * SIP response messages.)
     */
    size = strlen(url_main) + 1;
    user = (char *) cpr_malloc(size);
    /*
     * If the malloc fails, return the original URL.
     */
    if (user == NULL) {
        return (NULL);
    }

    sstrncpy(user, url_main, size);

    /*
     * Search for ";" and null terminate the string there.
     * An assumption has been made that any parameters will come
     * after the number. i.e.: "15726;param=paramval"
     */
    (void) cpr_strtok(user, ";", &lasts);
    return (user);
}

/*
 *  Function: sippmh_parse_message_summary
 *
 *  Parameters: The incoming SIP message and a container to be 
 *              filled with the parsed message body.   
 *
 *  Description: This function will parse the incoming MWI NTFY SIP message
 *               and fill the passed in structure with the 
 *               contents of the parsed message.Basic Error checking is done on
 *               the passed in message.  
 *
 *  Returns: SIP_ERROR on error.SIP_OK otherwise. 
 */
int32_t
sippmh_parse_message_summary(sipMessage_t *pSipMessage, sipMessageSummary_t *mesgSummary)
{
    int     i           = 0;
    int     j           = 0;
    char   *p           = NULL;
    char   *val         = NULL;
    char    temp[MAX_SIP_URL_LENGTH];
    boolean  token_found = FALSE;
    boolean  hp_found = FALSE;
    
    p = strstr(pSipMessage->mesg_body[0].msgBody, "Messages-Waiting");

    if (!p) {
        p = strstr(pSipMessage->mesg_body[0].msgBody, "Message-Waiting");
    }

    if (p) {
        memset(temp, '\0', MAX_SIP_URL_LENGTH);

        trim_right(p);

        val = strstr(p, ":");
        if (val) {
            val++;

            i = j = 0;
            while (val[j] != '\0' && val[j] != '\n' && isspace(val[j])) {
                j++;
            }
            while (val[j] != '\0' && val[j] != '\n' && val[j] != '\r') {
                // return error if obviously wrong
                if (!isalpha(val[j])) {
                    return SIP_ERROR;
                }
                temp[i] = val[j];
                j++;
                if (++i >= MAX_SIP_URL_LENGTH) {
                    return SIP_ERROR;
                }
            }
            temp[i] = '\0';
        } else {
            return SIP_ERROR;
        }
    } else {
        return SIP_ERROR;        
    }

    if (cpr_strcasecmp(temp, "no") == 0) {
        mesgSummary->mesg_waiting_on = FALSE;
    } else if (cpr_strcasecmp(temp, "yes") == 0) {
        mesgSummary->mesg_waiting_on = TRUE;
    } else {
        return SIP_ERROR;
    }
    
/*
    p = strstr(pSipMessage->mesg_body[0].msgBody, "Message-Account");
   
    if (p) {    
        trim_right(p);

        val = strstr(p, ":");
        if (val) {
            val++;

            i = j = 0;
            while (val[j] != '\0' && val[j] != '\n' && isspace(val[j])) {
                j++;
            }

            while (val[j] != '\0' && val[j] != '\n' && val[j] != '\r') {
                mesgSummary->message_account[i] = val[j];
                j++;
                if (++i >= MAX_SIP_URL_LENGTH) {
                    return SIP_ERROR;
                }
            }
            mesgSummary->message_account[i] = '\0';
        } else { 
            return SIP_ERROR;
        }
    }
*/

    p = strstr(pSipMessage->mesg_body[0].msgBody, "Voice-Message");
    
    if (p) {
        mesgSummary->type = mwiVoiceType;
        memset(temp, '\0', MAX_SIP_URL_LENGTH);
        trim_right(p);
        i = j = 0;
        val = strstr(p, ":");
        if (val) {
            val++;

            while (val[j] != '\0' && val[j] != '\n' && isspace(val[j])) {
                j++;
            }

            while (val[j] != '\0' && val[j] != '\n' && val[j] != '\r' && !isspace(val[j]) && val[j] != '(') {
                if (!isdigit(val[j])) {
                    if (val[j] == '/') {
                        temp[i] = '\0';
                        mesgSummary->newCount = atoi(temp);
                        token_found = TRUE;
                        i = 0;
                    } else {
                        return SIP_ERROR;
                    }
                } else {
                    temp[i] = val[j];
                    if (++i >= MAX_SIP_URL_LENGTH) {
                        return SIP_ERROR;
                    }
                }
                j++;
            }
            temp[i] = '\0';

            if (token_found) {
                mesgSummary->oldCount = atoi(temp);              
            }

            temp[i = 0] = '\0';
            while (val[j] != '\0' && val[j] != '\n' && isspace(val[j])) {
                j++;
            }

            if (val[j] == '(') {
                j++;
                while (val[j] != '\0' && val[j] != '\n' && isspace(val[j])) {
                    j++;
                }

                while (val[j] != '\0' && val[j] != '\n' && val[j] != '\r' && !isspace(val[j]) && val[j] != ')') {
                    if (!isdigit(val[j])) {
                        if (val[j] == '/') {
                            temp[i] = '\0';
                            mesgSummary->hpNewCount = atoi(temp);
                            hp_found = TRUE;
                            i = 0;
                        } else {
                            return SIP_ERROR;
                        }
                    } else {
                        temp[i] = val[j];
                        if (++i >= MAX_SIP_URL_LENGTH) {
                            return SIP_ERROR;
                        }
                    }
                    j++;
                }
                temp[i] = '\0';
                if (hp_found) {
                    mesgSummary->hpOldCount = atoi(temp);
                }
            }
            if (!hp_found) {
                mesgSummary->hpNewCount = mesgSummary->hpOldCount = -1;
            }
            /*make sure the urgent message counts don't exceed the total message counts*/
            if ((mesgSummary->hpNewCount > mesgSummary->newCount)   ||
                (mesgSummary->hpOldCount > mesgSummary->oldCount)) {
                return SIP_ERROR;
            }
            if (!token_found) {
                return SIP_ERROR;
            }
       } else {
            return SIP_ERROR;
       }
    }

    return SIP_OK;
}
/*
 * This function compares two strings that may contain escaped characters
 * in either of them and determines if they are equivalent, ignoring the
 * case if ignore_case is TRUE.
 * s1 = ptr to the first string of chars
 * s2 = ptr to the second string of chars
 * return: integer value of 0 if they are equivalent, <> 0 otherwise.
 */
int
sippmh_cmpURLStrings (const char *s1, const char *s2, boolean ignore_case)
{
    const unsigned char *us1 = (const unsigned char *) s1;
    const unsigned char *us2 = (const unsigned char *) s2;
    int value1, value2;
    int esc_char_incr_in_s1 = 0; // # of chars to skip in s1 if the given char is escaped
    int esc_char_incr_in_s2 = 0; // # of chars to skip in s2 if the given char is escaped

    if ((!s1 && s2) || (s1 && !s2)) /* no match if only one ptr is NULL */
        return (int) (s1 - s2); /* if one of these is NULL it will be the
                                 * lesser of the two values and therefore
                                 * we'll get the proper sign in the int */

    if (s1 == s2) {             /* match if both ptrs the same (e.g. NULL) */
        return 0;
    }

    value1 = 0;
    value2 = 0;
    while (*us1 != '\0') {
        if (*us1 == PERCENT) {
            esc_char_incr_in_s1 = 2;

            /* Convert 1st and 2nd hex chars to int */
            value1 = sippmh_htoi(*(us1 + 1)) * 16;
            value1 += sippmh_htoi(*(us1 + 2));
        } else {
            value1 = *us1;
        }

        if (*us2 == PERCENT) {
            esc_char_incr_in_s2 = 2;

            /* Convert 1st and 2nd hex chars to int */
            value2 = sippmh_htoi(*(us2 + 1)) * 16;
            value2 += sippmh_htoi(*(us2 + 2));
        } else {
            value2 = *us2;
        }

        if ((ignore_case && (toupper(value1) == toupper(value2))) ||
            (!ignore_case && (value1 == value2))) {
            us1 += (1 + esc_char_incr_in_s1);
            us2 += (1 + esc_char_incr_in_s2);
            esc_char_incr_in_s1 = 0;
            esc_char_incr_in_s2 = 0;
        } else {
            break;
        }
    }

    if (ignore_case) {
        return (toupper(value1) - toupper(value2));
    } else {
        return (value1 - value2);
    }
}

int
sippmh_add_call_info (sipMessage_t *sip_message_p, cc_call_info_t *call_info_p)
{
    if (sip_message_p && call_info_p) {
        char *call_info_hdr_p = ccsip_encode_call_info_hdr(call_info_p, NULL);

        if (call_info_hdr_p != NULL) {
            (void) sippmh_add_text_header(sip_message_p, SIP_HEADER_CALL_INFO,
                                          (const char *) call_info_hdr_p);
            cpr_free(call_info_hdr_p);
        }
    }
    return (0);
}

/*
 *  Function: sippmh_parse_supported_require
 *
 *  Parameters: contents of the Supported or Require header
 *
 *              punsupported_tokens: if the caller wants to know the
 *                  unsupported option tokens, it will pass a non-null pointer.
 *                  If punsupporte_tokens is NULL, then the caller does not
 *                  want to know the unsupported  options.
 *
 *              It is caller's responsibility to free the contents of this
 *              pointer.
 *
 *  Description: This function will parse for the various options tags
 *               in the supported and require headers
 *
 *  Returns: A bit map containing the values in the header
 *
 *  Format of the line is as follows:
 *  Supported: replaces, join, sec-agree, whatever ...
 *  Require: replaces, join, sec-agree, whatever ...
 */
uint32_t
sippmh_parse_supported_require (const char *header, char **punsupported_tokens)
{
    const char *fname = "sippmh_parse_supported_require";
    uint32_t tags = 0;
    char *temp_header;
    char *token;
    const char *delim = ", \r\n\t";
    char *unsupported_tokens;
    char *bad_token = NULL;
    int  size;
    char *lasts = NULL;

    if (header == NULL) {
        return (tags);
    }

    if (punsupported_tokens != NULL) {
        *punsupported_tokens = NULL; //assume everything  will go right
    }
    unsupported_tokens = NULL; //no memory allocated yet

    //need to keep own buffer since cpr_strtok is destructive
    size = strlen(header) + 1;
    temp_header = (char *) cpr_malloc(size);
    if (temp_header == NULL) {
        CCSIP_DEBUG_ERROR("%s: malloc failed for strlen(header)=%d\n", fname,
                          strlen(header));
        return tags;
    }
    sstrncpy(temp_header, header, size);

    token = cpr_strtok(temp_header, delim, &lasts);
    while (token != NULL) {
        bad_token = NULL;

        if (strcmp(token, REQ_SUPP_PARAM_REPLACES) == 0) {
            tags |= replaces_tag;
        } else if (strcmp(token, REQ_SUPP_PARAM_100REL) == 0) {
            tags |= rel_tag;
            bad_token = token;
        } else if (strcmp(token, REQ_SUPP_PARAM_EARLY_SESSION) == 0) {
            tags |= early_session_tag;
            bad_token = token;
        } else if (strcmp(token, REQ_SUPP_PARAM_JOIN) == 0) {
            tags |= join_tag;
        } else if (strcmp(token, REQ_SUPP_PARAM_PATH) == 0) {
            tags |= path_tag;
            bad_token = token;
        } else if (strcmp(token, REQ_SUPP_PARAM_PRECONDITION) == 0) {
            tags |= precondition_tag;
            bad_token = token;
        } else if (strcmp(token, REQ_SUPP_PARAM_PREF) == 0) {
            tags |= pref_tag;
            bad_token = token;
        } else if (strcmp(token, REQ_SUPP_PARAM_PRIVACY) == 0) {
            tags |= privacy_tag;
            bad_token = token;
        } else if (strcmp(token, REQ_SUPP_PARAM_SEC_AGREE) == 0) {
            tags |= sec_agree_tag;
            bad_token = token;
        } else if (strcmp(token, REQ_SUPP_PARAM_TIMER) == 0) {
            tags |= timer_tag;
            bad_token = token;
        } else if (strcmp(token, REQ_SUPP_PARAM_NOREFERSUB) == 0) {
            tags |= norefersub_tag;
        } else if (strcmp(token, REQ_SUPP_PARAM_CISCO_CALLINFO) == 0) {
            tags |= cisco_callinfo_tag;
        } else if (strcmp(token, REQ_SUPP_PARAM_CISCO_SERVICE_CONTROL) == 0) {
            tags |= cisco_service_control_tag;

        } else if (strcmp(token, REQ_SUPP_PARAM_SDP_ANAT) == 0) {
            tags |= sdp_anat_tag;
        }
        else if (strcmp(token, REQ_SUPP_PARAM_EXTENED_REFER) == 0) {
            tags |= extended_refer_tag;
        } else if (strcmp(token, REQ_SUPP_PARAM_CISCO_SERVICEURI) == 0) {
            tags |= cisco_serviceuri_tag;
        } else if (strcmp(token, REQ_SUPP_PARAM_CISCO_ESCAPECODES) == 0) {
            tags |= cisco_escapecodes_tag;
        } else if (strcmp(token, REQ_SUPP_PARAM_CISCO_SRTP_FALLBACK) == 0) {
            tags |= cisco_srtp_fallback_tag;
        }
        else {
            //This is not necessarily an error. Other end may support something we
            //don't. Is only an error if it shows up in Require.
            tags |= (uint32_t) unrecognized_tag;
            bad_token = token;
        }

        if (bad_token != NULL) {
            CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Invalid tag in Require/Supported %s\n", 
                DEB_F_PREFIX_ARGS(SIP_TAG, fname), bad_token);

            //allocate memory for unsupported options if necessary
            if (punsupported_tokens != NULL) {
                if (unsupported_tokens == NULL) {
                    *punsupported_tokens = (char *)
                        cpr_malloc(strlen(header) + 1);
                    unsupported_tokens = *punsupported_tokens;
                    if (unsupported_tokens) {
                        unsupported_tokens[0] = '\0';
                    }
                }
            }

            //caller wants to know what came in Require and we dont support
            if (unsupported_tokens != NULL) {
                //include token into illegal_tokens
                if (unsupported_tokens[0] != '\0') {
                    strcat(unsupported_tokens, ","); //add a ","
                }
                strcat(unsupported_tokens, bad_token);
            }
            bad_token = NULL;
        }

        //get next token
        token = cpr_strtok(NULL, delim, &lasts);
    }
    cpr_free(temp_header);
    return (tags);
}

/*
 *  Function: sippmh_parse_allow_header
 *
 *  Parameters: contents of the Allow header
 *
 *  Description: This function will parse for the various methods
 *               supported in the allow header
 *
 *  Returns: A bit map containing the values in the header
 *
 *  Format of the line is as follows:
 *  Allow: ACK, BYE, CANCEL, ....
 */
uint16_t
sippmh_parse_allow_header (const char *header)
{
    uint16_t tags = 0;

    if (header == NULL) {
        return (tags);
    }
    if (strstr(header, SIP_METHOD_ACK)) {
        tags |= ALLOW_ACK;
    }
    if (strstr(header, SIP_METHOD_BYE)) {
        tags |= ALLOW_BYE;
    }
    if (strstr(header, SIP_METHOD_CANCEL)) {
        tags |= ALLOW_CANCEL;
    }
    if (strstr(header, SIP_METHOD_INFO)) {
        tags |= ALLOW_INFO;
    }
    if (strstr(header, SIP_METHOD_INVITE)) {
        tags |= ALLOW_INVITE;
    }
    if (strstr(header, SIP_METHOD_MESSAGE)) {
        tags |= ALLOW_MESSAGE;
    }
    if (strstr(header, SIP_METHOD_NOTIFY)) {
        tags |= ALLOW_NOTIFY;
    }
    if (strstr(header, SIP_METHOD_OPTIONS)) {
        tags |= ALLOW_OPTIONS;
    }
    if (strstr(header, SIP_METHOD_PRACK)) {
        tags |= ALLOW_PRACK;
    }
    if (strstr(header, SIP_METHOD_PUBLISH)) {
        tags |= ALLOW_PUBLISH;
    }
    if (strstr(header, SIP_METHOD_REFER)) {
        tags |= ALLOW_REFER;
    }
    if (strstr(header, SIP_METHOD_REGISTER)) {
        tags |= ALLOW_REGISTER;
    }
    if (strstr(header, SIP_METHOD_SUBSCRIBE)) {
        tags |= ALLOW_SUBSCRIBE;
    }
    if (strstr(header, SIP_METHOD_UPDATE)) {
        tags |= ALLOW_UPDATE;
    }
    return (tags);
}

/*
 *  Function: sippmh_parse_accept_header
 *
 *  Parameters: contents of the Accept header
 *
 *  Description: This function will parse for the various capabilities
 *               signalled in the Accept header
 *
 *  Returns: A bit map containing the values in the header
 *
 *  Format of the line is as follows:
 *  Accept: application/sdp, nultipart/mixed, multipart/alternative
 */
uint16_t
sippmh_parse_accept_header (const char *header)
{
    uint16_t tags = 0;

    if (header == NULL) {
        return (tags);
    }
    if (strstr(header, SIP_CONTENT_TYPE_SDP)) {
        tags |= (0x01 << SIP_CONTENT_TYPE_SDP_VALUE);
    }
    if (strstr(header, SIP_CONTENT_TYPE_MULTIPART_MIXED)) {
        tags |= (0x01 << SIP_CONTENT_TYPE_MULTIPART_MIXED_VALUE);
    }
    if (strstr(header, SIP_CONTENT_TYPE_MULTIPART_ALTERNATIVE)) {
        tags |= (0x01 << SIP_CONTENT_TYPE_MULTIPART_ALTERNATIVE_VALUE);
    }
    return tags;
}

/*
 *  Function: ccsip_process_network_message
 *
 *  Parameters: sipmsg, pointer to pointer of the buffer
 *              bytes used, display message.
 *
 *  Description: Currently only used for tcp packets received to do the
 *               framing. Ported from Propel.
 *  Returns: A bit map containing the values in the header
 */
ccsipRet_e
ccsip_process_network_message (sipMessage_t **sipmsg_p,
                               char **buf,
                               unsigned long *nbytes_used,
                               char **display_msg)
{
    static const char fname[] = "ccsip_process_network_message";
    sipMessage_t *sip_msg = NULL;
    int local_nbytes = *nbytes_used;
    uint32_t bytes_used;
    char *local_buf_ptr;

    sip_msg = sippmh_message_create();
    if (sip_msg == NULL) {
        CCSIP_ERR_DEBUG {
            buginf("%s: Error in creating SIP Msg\n", fname);
        }
        *sipmsg_p = NULL;
        return SIP_MSG_CREATE_ERR;
    }

    /* Init local variables */
    bytes_used = local_nbytes;
    local_buf_ptr = *buf;

    if (sippmh_process_network_message(sip_msg, local_buf_ptr, &bytes_used)
        == STATUS_FAILURE) {
        CCSIP_ERR_DEBUG {
            buginf("%s: process_network_message failed.\n", fname);
        }
        sippmh_message_free(sip_msg);
        *sipmsg_p = NULL;
        return SIP_MSG_PARSE_ERR;
    }

    if (sippmh_is_message_complete(sip_msg)) {
        if (display_msg) {
            *display_msg = (char *) cpr_malloc(bytes_used + 1);
            if (*display_msg == NULL) {
                CCSIP_ERR_DEBUG {
                    buginf("%s: malloc of display msg failed.\n", fname);
                }
                sippmh_message_free(sip_msg);
                *sipmsg_p = NULL;
                return SIP_MSG_PARSE_ERR;
            }
            sstrncpy(*display_msg, local_buf_ptr, bytes_used + 1);
        }
        local_nbytes -= bytes_used;
        local_buf_ptr += bytes_used;

        /* Update information */
        *sipmsg_p = sip_msg;
        *nbytes_used = local_nbytes;
        *buf = local_buf_ptr;

        return SIP_SUCCESS;
    } else {
        CCSIP_ERR_DEBUG {
            buginf("%s: process_network_msg: not complete\n", fname);
        }
        sippmh_message_free(sip_msg);
        *sipmsg_p = NULL;

        return SIP_MSG_INCOMPLETE_ERR;
    }
}

/*
 *  Function: sippmh_parse_service_control_body
 *
 *  Parameters: contents of the service control notify body and its length
 *
 *  Description: This function will parse for the reset/restart
 *               instructions and parameters in this body
 *
 *  Returns: 0 if body parsed correctly
 *           Parsed values are added to passed structure
 *
 *  Format of the body is as follows:
 *            action = [reset | restart | check-version | call-preservation | apply-config ]
 *            RegisterCallId= {<callid from register msg>}
 *            ConfigVersionStamp = {79829A69-9489-4C8E-8143-90A9C22DFAD7}
 *            DialplanVersionStamp = {79829A69-9489-4C8E-8143-90A9C22DFAD7}
 *            SoftkeyVersionStamp = {79829A69-9489-4C8E-8143-90A9C22DFAD7}
 *            CUCMResult=[no_change | config_applied | reregister_needed]
 *            FirmwareLoadId = {SIP70.8-4-0-28S}
 *            LoadServer={10.81.15.24}
 *            LogServer={<ipv4 address or ipv6 address or fqdn> <port>} // This is used for ppid
 *            UpgradeTime=[now | later]
 *            PPID=[enabled | disabled]
 */

sipServiceControl_t *
sippmh_parse_service_control_body (char *msgBody, int msgLength)
{
    pmhRstream_t *rs = NULL;
    char *line = NULL, *value = NULL;
    boolean body_read = FALSE;
    sipServiceControl_t *scp = NULL;

    if (msgLength==0) {
        return (NULL);
    }

    if ((rs = pmhutils_rstream_create(msgBody, msgLength))
            == NULL) {
        return (NULL);
    }

    scp = (sipServiceControl_t *)
        cpr_calloc(1, sizeof(sipServiceControl_t));
    if (!scp) {
        cpr_free(rs);
        return NULL;
    }

    while (!body_read) {
        line = pmhutils_rstream_read_line(rs);
        if (line) {
            value = strchr(line, '=');
            if (value) {
                value++;
                while (*value == ' ') {
                    value++;
                }
            }
            if (strlen(line) == 0) {

            } else if (!strncasecmp(line, "action", sizeof("action") - 1)) {
                if (value == NULL) {
                    scp->action = SERVICE_CONTROL_ACTION_INVALID;
                } else if (!strncasecmp(value, "reset", sizeof("reset") - 1)) {
                    scp->action = SERVICE_CONTROL_ACTION_RESET;
                } else if (!strncasecmp(value, "restart",
                                        sizeof("restart") - 1)) {
                    scp->action = SERVICE_CONTROL_ACTION_RESTART;
                } else if (!strncasecmp(value, "check-version",
                                        sizeof("check-version") - 1)) {
                    scp->action = SERVICE_CONTROL_ACTION_CHECK_VERSION;
                } else if (!strncasecmp(value, "call-preservation",
                                        sizeof("call-preservation") - 1)) {
                    scp->action = SERVICE_CONTROL_ACTION_CALL_PRESERVATION;
                } else if (!strncasecmp(value, "apply-config",
                                        sizeof("apply-config") - 1)) {
                    scp->action = SERVICE_CONTROL_ACTION_APPLY_CONFIG;
                    
                } else {
                    scp->action = SERVICE_CONTROL_ACTION_INVALID;
                }
            } else if (!strncasecmp(line, "RegisterCallId",
                                    sizeof("RegisterCallId") - 1)) {
                if (scp->registerCallID == NULL) {
                    if (value == NULL) {
                        scp->registerCallID = NULL;
                    } else if (value[0]) {
                        int len = strlen(value);

                        /* make sure curly braces are present */
                        if ((*value == '{') && (*(value + len - 1) == '}')) {
                            scp->registerCallID = (char *)
                                cpr_calloc(1, len + 1);
                            /* ignore the beginning and ending curly brace */
                            if (scp->registerCallID)
                                memcpy(scp->registerCallID, value + 1,
                                       (len - 2));
                        }
                    }
                }

            } else if (!strncasecmp(line, "ConfigVersionStamp",
                                    sizeof("ConfigVersionStamp") - 1)) {
                if (scp->configVersionStamp == NULL) {
                    if (value == NULL) {
                        scp->configVersionStamp = NULL;
                    } else if (value[0]) {
                        int len = strlen(value);

                        /* make sure curly braces are present */
                        if ((*value == '{') && (*(value + len - 1) == '}')) {
                            scp->configVersionStamp = (char *)
                                cpr_calloc(1, len + 1);
                            /* ignore the beginning and ending curly brace */
                            if (scp->configVersionStamp)
                                memcpy(scp->configVersionStamp, value + 1,
                                       (len - 2));
                        }
                    }
                }

            } else if (!strncasecmp(line, "DialplanVersionStamp",
                                    sizeof("DialplanVersionStamp") - 1)) {
                if (scp->dialplanVersionStamp == NULL) {
                    if (value == NULL) {
                        scp->dialplanVersionStamp = NULL;
                    } else if (value[0]) {
                        int len = strlen(value);

                        /* make sure curly braces are present */
                        if ((*value == '{') && (*(value + len - 1) == '}')) {
                            scp->dialplanVersionStamp = (char *)
                                cpr_calloc(1, len + 1);
                            /* ignore the beginning and ending curly brace */
                            if (scp->dialplanVersionStamp)
                                memcpy(scp->dialplanVersionStamp, value + 1,
                                       (len - 2));
                        }
                    }
                }
            } else if (!strncasecmp(line, "SoftkeyVersionStamp",
                                    sizeof("SoftkeyVersionStamp") - 1)) {
                if (scp->softkeyVersionStamp == NULL) {
                    if (value == NULL) {
                        scp->softkeyVersionStamp = NULL;
                    } else if (value[0]) {
                        int len = strlen(value);

                        /* make sure curly braces are present */
                        if ((*value == '{') && (*(value + len - 1) == '}')) {
                            scp->softkeyVersionStamp = (char *)
                                cpr_calloc(1, len + 1);
                            /* ignore the beginning and ending curly brace */
                            if (scp->softkeyVersionStamp)
                                memcpy(scp->softkeyVersionStamp, value + 1,
                                       (len - 2));
                        }
                    }
                }
            } else if (!strncasecmp(line, "FeatureControlVersionStamp",
                                    sizeof("FeatureControlVersionStamp") - 1)) {
                if (scp->fcpVersionStamp == NULL) {
                    if (value == NULL) {
                        scp->fcpVersionStamp = NULL;
                    } else if (value[0]) {
                        int len = strlen(value);

                        /* make sure curly braces are present */
                        if ((*value == '{') && (*(value + len - 1) == '}')) {
                            scp->fcpVersionStamp = (char *)
                                cpr_calloc(1, len + 1);
                            /* ignore the beginning and ending curly brace */
                            if (scp->fcpVersionStamp)
                                memcpy(scp->fcpVersionStamp, value + 1,
                                       (len - 2));
                        }
                    }
                }
            }  else if (!strncasecmp(line, "CUCMResult", 
                                                 sizeof("CUCMResult") - 1)) {
                if (value == NULL) {
                    scp->cucm_result = NULL;
                } else {
                    int len =0;
                    if ((strncasecmp(value, "no_change", 
                                         len = strlen("no_change")) == 0)  ||
                        (strncasecmp(value, "config_applied", 
                                    len = strlen("config_applied")) == 0)  ||
                        (strncasecmp(value, "reregister_needed", 
                                    len = strlen("reregister_needed")) == 0)) {
                        scp->cucm_result = (char *) cpr_calloc(1,len + 1);
                        if (scp->cucm_result) {
                            strncpy(scp->cucm_result, value, len);
                            scp->cucm_result[len] = '\0';
                        }
                    }
                }
            } else if (!strncasecmp(line, "LoadId",
                                    sizeof("LoadId") - 1)) {
                if (scp->firmwareLoadId == NULL) {
                    if (value == NULL) {
                        scp->firmwareLoadId = NULL;
                    } else if (value[0]) {
                        int len = strlen(value);

                        /* make sure curly braces are present */
                        if ((*value == '{') && (*(value + len - 1) == '}')) {
                            scp->firmwareLoadId = (char *)
                                cpr_calloc(1, len + 1);
                            /* ignore the beginning and ending curly brace */
                            if (scp->firmwareLoadId)
                                memcpy(scp->firmwareLoadId, value + 1,
                                       (len - 2));
                        }
                    }
                }
            } else if (!strncasecmp(line, "InactiveLoadId",
                                    sizeof("InactiveLoadId") - 1)) {
                if (scp->firmwareInactiveLoadId == NULL) {
                    if (value == NULL) {
                        scp->firmwareInactiveLoadId = NULL;
                    } else if (value[0]) {
                        int len = strlen(value);

                        /* make sure curly braces are present */
                        if ((*value == '{') && (*(value + len - 1) == '}')) {
                            scp->firmwareInactiveLoadId = (char *)
                                cpr_calloc(1, len + 1);
                            /* ignore the beginning and ending curly brace */
                            if (scp->firmwareInactiveLoadId)
                                memcpy(scp->firmwareInactiveLoadId, value + 1,
                                       (len - 2));
                        }
                    }
                }
           }  else if (!strncasecmp(line, "LoadServer",
                                    sizeof("LoadServer") - 1)) {
                if (scp->loadServer == NULL) {
                    if (value == NULL) {
                        scp->loadServer = NULL;
                    } else if (value[0]) {
                        int len = strlen(value);

                        /* make sure curly braces are present */
                        if ((*value == '{') && (*(value + len - 1) == '}')) {
                            scp->loadServer = (char *)
                                cpr_calloc(1, len + 1);
                            /* ignore the beginning and ending curly brace */
                            if (scp->loadServer)
                                memcpy(scp->loadServer, value + 1,
                                       (len - 2));
                        }
                    }
                }
           }  else if (!strncasecmp(line, "LogServer",
                                    sizeof("LogServer") - 1)) {
                if (scp->logServer == NULL) {
                    if (value == NULL) {
                        scp->logServer = NULL;
                    } else if (value[0]) {
                        int len = strlen(value);

                        /* make sure curly braces are present */
                        if ((*value == '{') && (*(value + len - 1) == '}')) {
                            scp->logServer = (char *)
                                cpr_calloc(1, len + 1);
                            /* ignore the beginning and ending curly brace */
                            if (scp->logServer)
                                memcpy(scp->logServer, value + 1,
                                       (len - 2));
                        }
                    }
                }
           }   else if (!strncasecmp(line, "PPID", sizeof("PPID") - 1)) {
                if (value == NULL) {
                    scp->ppid = FALSE;
                } else if (!strncasecmp(value, "enabled", sizeof("enabled") - 1)) {
                    scp->ppid = TRUE;
                } else if (!strncasecmp(value, "disabled",
                                        sizeof("disabled") - 1)) {
                    scp->ppid = FALSE;  
                } else {
                    scp->ppid = FALSE;
                }

           }
           cpr_free(line);
            
        } else {
            body_read = TRUE;
        }
    }
    // Free the created stream structure
    pmhutils_rstream_delete(rs, FALSE);
    cpr_free(rs);
    return (scp);
}

void
sippmh_free_service_control_info (sipServiceControl_t *scp)
{
    if (!scp) {
        return;
    }

    if (scp->configVersionStamp)
        cpr_free(scp->configVersionStamp);
    if (scp->dialplanVersionStamp)
        cpr_free(scp->dialplanVersionStamp);
    if (scp->registerCallID)
        cpr_free(scp->registerCallID);
    if (scp->softkeyVersionStamp)
        cpr_free(scp->softkeyVersionStamp);
    if (scp->fcpVersionStamp)
        cpr_free(scp->fcpVersionStamp);
    if (scp->cucm_result)
        cpr_free(scp->cucm_result);
    if (scp->loadServer)
        cpr_free(scp->loadServer);
    if (scp->firmwareLoadId)    
        cpr_free(scp->firmwareLoadId);                
    if (scp->logServer)
        cpr_free(scp->logServer);        

    cpr_free(scp);
}

int32_t
sippmh_parse_max_forwards (const char *max_fwd_hdr)
{
    int32_t maxFwd;

    if (max_fwd_hdr) {
        if (isdigit(*max_fwd_hdr)) {
            maxFwd = strtol(max_fwd_hdr, NULL, 10);
            if ((maxFwd >= 0) && (maxFwd <= 255)) {
                return maxFwd;
            }
        }
    }
    return -1;
}

/*
 *  Function: sippmh_parse_url_from_hdr
 *
 *  Parameters: String that needs to be parsed 
 *
 *  Description: This function will strip out the tags and 
 *                other characters in the header to retreive 
 *               a string that could be used as the request uri.
 *               Note that the input string needs to be well formed.
 *
 *  Returns: A string containing the request uri
 *
 */
string_t
sippmh_get_url_from_hdr (char *input_str)
{
    char *left_bracket, *right_bracket = NULL;

    /* 
      This function is only intended for properly 
      formed headers.   
    */
    left_bracket = strpbrk(input_str, ",<");
    if (left_bracket) {
        left_bracket++;
        right_bracket = strchr(left_bracket, '>');
        if (right_bracket) {
            *right_bracket = '\0';
        }
        input_str = left_bracket;
    }
    return (string_t)input_str;
}
/*
 *  Function: sippmh_parse_join_header
 *
 *  Parameters: pointer to string of the join header contents
 *
 *  Returns: Parsed structure if parsed correctly, NULL otherwise
 *
 *  Format of the header is as follows:
 *           Join: abcd;from-tag=efgh;to-tag=ijkl
 */
sipJoinInfo_t *
sippmh_parse_join_header (const char *header)
{
    sipJoinInfo_t *join = NULL;
    char          *semi = NULL;
    unsigned int  param_len;
    char          *params, *param_name, *param_value, *save_params;

    if (!header) {
        return NULL;
    }

    join = (sipJoinInfo_t *) cpr_calloc(1, sizeof(sipJoinInfo_t));
    if (!join) {
        return NULL;
    }

    // Read the call-id, if present
    semi = strchr(header, SEMI_COLON);
    if (semi) {
        join->call_id = (char *) cpr_calloc(1, semi-header + 1);
        if (join->call_id == NULL) {
            sippmh_free_join_info(join);
            return NULL;
        }
        strncpy(join->call_id, header, semi-header);
    } else {
        // call-id is the only parameter
        join->call_id = cpr_strdup(header);
        if (join->call_id == NULL) {
            sippmh_free_join_info(join);
            return NULL;
        }
        return (join);
    }

    params = cpr_strdup(semi);
    if (!params) {
        sippmh_free_join_info(join);
        return NULL;
    }
    save_params = params;
    while (1) {
        while (*params == ';') {
            params++;
        }
        param_name = params;
        SKIP_SIP_TOKEN(params);
        param_len = params - param_name;
        if (param_len == 0) {
            sippmh_free_join_info(join);
            cpr_free(save_params);
            return NULL;
        }

        /* Parse from-tag parameter */
        if ((param_len == sizeof(SIP_HEADER_JOIN_FROM_TAG) - 1) &&
            (strncasecmp(param_name, SIP_HEADER_JOIN_FROM_TAG,
                         sizeof(SIP_HEADER_JOIN_FROM_TAG) - 1) == 0) &&
            (join->from_tag == NULL)) {
            params = parse_generic_param(params, &param_value);
            if (params == NULL) {
                sippmh_free_join_info(join);
                cpr_free(save_params);
                return NULL;
            } else {
                join->from_tag = (char *) cpr_calloc(1, params-param_value + 1);
                if (join->from_tag) {
                    strncpy(join->from_tag, param_value, params-param_value);
                }
                SKIP_LWS(params);
                if (*params == SEMI_COLON) {
                    *params++ = '\0';
                } else {
                    break;
                }

            }

        /* Parse to-tag parameter */
        } else if ((param_len == sizeof(SIP_HEADER_JOIN_TO_TAG) - 1) &&
                   (strncasecmp(param_name, SIP_HEADER_JOIN_TO_TAG,
                                sizeof(SIP_HEADER_JOIN_TO_TAG) - 1) == 0) &&
                   (join->to_tag == NULL)) {
            params = parse_generic_param(params, &param_value);
            if (params == NULL) {
                sippmh_free_join_info(join);
                cpr_free(save_params);
                return NULL;
            } else {
                join->to_tag = (char *) cpr_calloc(1, params-param_value + 1);
                if (join->to_tag) {
                    strncpy(join->to_tag, param_value, params-param_value);
                }
                if (*params == SEMI_COLON) {
                    *params++ = '\0';
                } else {
                    break;
                }
            }

        } else {
            // Skip over unexpected parameter
            SKIP_SIP_TOKEN(params);
        }

        SKIP_LWS(params);
    }
    cpr_free(save_params);
    return (join);
}

void
sippmh_free_join_info (sipJoinInfo_t *join)
{

    if (!join) {
        return;
    }
    if (join->call_id)
        cpr_free(join->call_id);
    if (join->from_tag)
        cpr_free(join->from_tag);
    if (join->to_tag)
        cpr_free(join->to_tag);
    cpr_free(join);
}

sipRet_t
sippmh_add_join_header (sipMessage_t *message, sipJoinInfo_t *join)
{
    char joinhdr[MAX_SIP_HEADER_LENGTH+1];
    int  left;

    // Write out the header in a buffer
    if (!message) {
        return STATUS_FAILURE;
    }

    snprintf(joinhdr, MAX_SIP_HEADER_LENGTH, "%s", join->call_id);
    left = (uint16_t) MAX_SIP_HEADER_LENGTH - strlen(join->call_id);
    if (join->from_tag && left > 0) {
        strncat(joinhdr, ";from-tag=", left);
        left -= sizeof(";from-tag=") - 1;
        strncat(joinhdr, join->from_tag, left);
        left -= strlen(join->from_tag);
    }
    if (join->to_tag && left > 0) {
        strncat(joinhdr, ";to-tag=", left);
        left -= sizeof(";to-tag=") - 1;
        strncat(joinhdr, join->to_tag, left);
    }
    return (sippmh_add_text_header(message, SIP_HEADER_JOIN, joinhdr));
}

/*
 *  Function: sippmh_parse_subscription_state
 *
 *  Parameters: contents of the Subscription-State header
 *
 *  Description: This function will parse for the state, expiry
 *               time, and a reason code in the header
 *
 *  Returns: -1 if error, 0 if successfully parsed.
 *
 *  Format of the line is as follows:
 *  Subscription-State: active;expires=7200;reason=whatever;retry-after=50
 */
int
sippmh_parse_subscription_state(sipSubscriptionStateInfo_t *subsStateInfo,
                                const char *subs_state)
//TODO: how can subs_state be a const char [] when it can be modified below?
{
    char *ptr;
    char temp[TEMP_PARSE_BUFFER_SIZE];
    uint8_t i;

    if (!subs_state) {
        return (-1);
    }

    if (!strncasecmp(subs_state, SIP_SUBSCRIPTION_STATE_ACTIVE,
                     sizeof(SIP_SUBSCRIPTION_STATE_ACTIVE) - 1)) {
        subsStateInfo->state = SUBSCRIPTION_STATE_ACTIVE;
    } else if (!strncasecmp(subs_state, SIP_SUBSCRIPTION_STATE_PENDING,
                     sizeof(SIP_SUBSCRIPTION_STATE_PENDING) - 1)) {
        subsStateInfo->state = SUBSCRIPTION_STATE_PENDING;
    } else if (!strncasecmp(subs_state, SIP_SUBSCRIPTION_STATE_TERMINATED,
                     sizeof(SIP_SUBSCRIPTION_STATE_PENDING) - 1)) {
        subsStateInfo->state = SUBSCRIPTION_STATE_TERMINATED;
    }

    ptr = strchr(subs_state, ';');
    if (!ptr) {
        return (0);
    }
    SKIP_LWS(ptr);

    // Look for expires tag
    ptr = strstr(subs_state, SIP_SUBSCRIPTION_STATE_EXPIRES);
    if (ptr) {
        ptr = ptr + sizeof(SIP_SUBSCRIPTION_STATE_EXPIRES); // Go over the '='
        SKIP_LWS(ptr);
        if (*ptr) {
            boolean is_int = FALSE;

            memset(temp, 0, sizeof(temp));
            i = 0;
            while (isdigit(*ptr) && (i < sizeof(temp)-1)) {
                temp[i++] = *ptr;
                ptr++;
                is_int = TRUE;
            }
            if (is_int == TRUE) {
                subsStateInfo->expires = strtoul(temp, NULL, 10);
            }
        }
    }
    // Look for reason tag
    ptr = strstr(subs_state, SIP_SUBSCRIPTION_STATE_REASON);
    if (ptr) {
        ptr = ptr + sizeof(SIP_SUBSCRIPTION_STATE_REASON);
        SKIP_LWS(ptr);
        if (*ptr) {
            if (!strncasecmp(ptr, SIP_SUBSCRIPTION_STATE_REASON_DEACTIVATED,
                             sizeof(SIP_SUBSCRIPTION_STATE_REASON_DEACTIVATED) - 1)) {
                subsStateInfo->reason = SUBSCRIPTION_STATE_REASON_DEACTIVATED;
            } else if (!strncasecmp(ptr, SIP_SUBSCRIPTION_STATE_REASON_PROBATION,
                             sizeof(SIP_SUBSCRIPTION_STATE_REASON_PROBATION) - 1)) {
                subsStateInfo->reason = SUBSCRIPTION_STATE_REASON_PROBATION;
            } else if (!strncasecmp(ptr, SIP_SUBSCRIPTION_STATE_REASON_REJECTED,
                             sizeof(SIP_SUBSCRIPTION_STATE_REASON_REJECTED) - 1)) {
                subsStateInfo->reason = SUBSCRIPTION_STATE_REASON_REJECTED;
            } else if (!strncasecmp(ptr, SIP_SUBSCRIPTION_STATE_REASON_TIMEOUT,
                             sizeof(SIP_SUBSCRIPTION_STATE_REASON_TIMEOUT) - 1)) {
                subsStateInfo->reason = SUBSCRIPTION_STATE_REASON_TIMEOUT;
            } else if (!strncasecmp(ptr, SIP_SUBSCRIPTION_STATE_REASON_GIVEUP,
                             sizeof(SIP_SUBSCRIPTION_STATE_REASON_GIVEUP) - 1)) {
                subsStateInfo->reason = SUBSCRIPTION_STATE_REASON_GIVEUP;
            } else if (!strncasecmp(ptr, SIP_SUBSCRIPTION_STATE_REASON_NORESOURCE,
                             sizeof(SIP_SUBSCRIPTION_STATE_REASON_NORESOURCE) - 1)) {
                subsStateInfo->reason = SUBSCRIPTION_STATE_REASON_NORESOURCE;
            } else {
                subsStateInfo->reason = SUBSCRIPTION_STATE_REASON_INVALID;
            }
        }
    }

    // Look for retry-after tag
    ptr = strstr(subs_state, SIP_SUBSCRIPTION_STATE_RETRY_AFTER);
    if (ptr) {
        ptr = ptr + sizeof(SIP_SUBSCRIPTION_STATE_RETRY_AFTER); // Go over the '='
        SKIP_LWS(ptr);
        if (*ptr) {
            boolean is_int = FALSE;

            memset(temp, 0, sizeof(temp));
            i = 0;
            while (isdigit(*ptr) && (i < sizeof(temp)-1)) {
                temp[i++] = *ptr;
                ptr++;
                is_int = TRUE;
            }
            if (is_int == TRUE) {
                *ptr = '\0';
                subsStateInfo->retry_after = strtoul(temp, NULL, 10);
            }
        }
    }

    return (0);
}

/*
 *  Function: sippmh_add_subscription_state
 *
 *  Parameters: contents of the Subscription-State header
 *
 *  Description: This function will add the subscription state header
 *
 *  Returns: -1 if error, 0 if successfully added
 *
 *  Format of the line is as follows:
 *  Subscription-State: active; expires=7200; reason=whatever
 */
int
sippmh_add_subscription_state(sipMessage_t *msg,
                              sipSubscriptionStateInfo_t *subsStateInfo)
{
    char subs_state[MAX_SUB_STATE_HEADER_SIZE];

    if (!msg) {
        return -1;
    }

    if ((subsStateInfo->state == SUBSCRIPTION_STATE_ACTIVE) && (subsStateInfo->expires == 0)) {
        snprintf(&subs_state[0], MAX_SUB_STATE_HEADER_SIZE, "%s", SIP_SUBSCRIPTION_STATE_ACTIVE);
    } else if (subsStateInfo->state == SUBSCRIPTION_STATE_ACTIVE) {
        snprintf(&subs_state[0], MAX_SUB_STATE_HEADER_SIZE, "%s; expires=%d",
                SIP_SUBSCRIPTION_STATE_ACTIVE,
                subsStateInfo->expires);
    } else if (subsStateInfo->state == SUBSCRIPTION_STATE_PENDING) {
        snprintf(&subs_state[0], MAX_SUB_STATE_HEADER_SIZE, "%s; expires=%d",
                SIP_SUBSCRIPTION_STATE_PENDING,
                subsStateInfo->expires);
    } else if (subsStateInfo->state == SUBSCRIPTION_STATE_TERMINATED) {
        snprintf(&subs_state[0], MAX_SUB_STATE_HEADER_SIZE, "%s; reason=timeout",
                SIP_SUBSCRIPTION_STATE_TERMINATED);
    }

    (void) sippmh_add_text_header(msg, SIP_HEADER_SUBSCRIPTION_STATE,
                                  (const char *)&subs_state[0]);
    return 0;
}

boolean
sippmh_parse_kpml_event_id_params (char *params, char **call_id,
                                   char **from_tag, char **to_tag)
{
    boolean params_good;
    int param_len;
    char *param_name;

    if (params == NULL) {
        return FALSE;
    }

    while (1) {
        params_good = FALSE;

        while (*params == ';') {
            params++;
        }

        param_name = params;
        SKIP_SIP_TOKEN(params);
        param_len = params - param_name;
        if (param_len == 0) {
            return FALSE;
        }

        /* Parse call-id parameter */
        if ((param_len == KPML_ID_CALLID_LEN) &&
            (strncasecmp(param_name, KPML_ID_CALLID, KPML_ID_CALLID_LEN) == 0)) {
            params = parse_generic_param(params, call_id);
            if (params == NULL) {
                return FALSE;
            } else {
                params_good = TRUE;
            }

        /* Parse from-tag parameter */
        } else if ((param_len == KPML_ID_FROM_TAG_LEN) &&
                   (strncasecmp(param_name, KPML_ID_FROM_TAG,
                                KPML_ID_FROM_TAG_LEN) == 0)) {
            params = parse_generic_param(params, from_tag);
            if (params == NULL) {
                return FALSE;
            } else {
                params_good = TRUE;
            }

        /* Parse to-tag parameter */
        } else if ((param_len == KPML_ID_TO_TAG_LEN) &&
                   (strncasecmp(param_name, KPML_ID_TO_TAG,
                                KPML_ID_TO_TAG_LEN) == 0)) {
            params = parse_generic_param(params, to_tag);
            if (params == NULL) {
                return FALSE;
            } else {
                params_good = TRUE;
            }

        }

        SKIP_LWS(params);
        if (*params == SEMI_COLON) {
            /* More parameters follow */
            *params++ = '\0';
            SKIP_LWS(params);
        } else {
            break;
        }
    }

    return params_good;
}
