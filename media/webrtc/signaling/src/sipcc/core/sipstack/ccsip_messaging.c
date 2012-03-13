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

#include "cpr_types.h"
#include "cpr_time.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "cpr_string.h"
#include "cpr_in.h"
#include "cpr_rand.h"
#include "phntask.h"
#include "text_strings.h"
#include "util_string.h"
#include "ccsip_core.h"
#include "ccsip_macros.h"
#include "ccsip_messaging.h"
#include "ccsip_platform.h"
#include "ccsip_task.h"
#include "prot_configmgr.h"
#include "phone_debug.h"
#include "ccsip_reldev.h"
#include "digcalc.h"
#include "ccsip_register.h"
#include "ccsip_credentials.h"
#include "dns_utils.h"
#include "config.h"
#include "string_lib.h"
#include "dialplan.h"
#include "rtp_defs.h"
#include "ccapi.h"
#include "ccsip_platform_udp.h"
#include "ccsip_task.h"
#include "sdp.h"
#include "sip_common_transport.h"
#include "sip_common_regmgr.h"
#include "uiapi.h"
#include "ccsip_callinfo.h"
#include "sip_interface_regmgr.h"
#include "ccsip_spi_utils.h"
#include "ccsip_subsmanager.h"
#include "subapi.h"
#include "xml_util.h"
#include "platform_api.h"

#define SIPS_URL_LEN 8
#define NONCE_LEN    9
#define SUBS_STATE_HDR_LEN 80
#define MAX_EXPIRES_LEN      12
#define MAX_ESCAPED_USER_LEN 94 // Worst case all 31 chars require escaping (3 chars) + a NULL
#define MAX_PHONE_NAME_LEN 20
#define MAX_UNREG_REASON_STR_LEN    256

#define MAX_ESCAPED_USER_LEN 94 // Worst case all 31 chars require escaping (3 chars) + a NULL
#define INITIAL_BUFFER_SIZE 2048


/* External declarations */
extern int dns_error_code; // DNS error code global
extern sipPlatformUITimer_t sipPlatformUISMTimers[];
extern sipCallHistory_t gCallHistory[];
extern ccsipGlobInfo_t gGlobInfo;
extern int16_t clockIsSetup;
extern struct tm *gmtime_r(const time_t *, struct tm *);
extern char *Basic_is_phone_forwarded(line_t line);
extern uint16_t server_caps;
extern char sipPhoneModelNumber[];
extern char phone_load_name[];
extern sipGlobal_t sip;
extern ccm_act_stdby_table_t CCM_Active_Standby_Table;

/* Forward declarations */
boolean sipSPIAddRequestRecordRoute(sipMessage_t *, sipMessage_t *);
static boolean sendResponse(ccsipCCB_t *ccb, sipMessage_t *response,
                            sipMessage_t *refrequest, boolean retx,
                            sipMethod_t method);
static sipRet_t CopyLocalSDPintoResponse(sipMessage_t *request,
                                         cc_msgbody_info_t *local_msg_body);

#if defined SIP_OS_WINDOWS
#define debugif_printf printf
#endif
/*
 * Functions to manipulate transaction blocks
 */

/*
 * This function gets the index of the last request sent or received
 * It determines this by choosing the last one that has a
 * non CCSIP_START_CSEQ cseq value
 */
int16_t
get_last_request_trx_index (ccsipCCB_t *ccb, boolean sent)
{
    const char *fname = "get_last_request_trx_index";
    int16_t i;

    if (ccb == NULL) {
        return -1;
    }

    CCSIP_DEBUG_TRX(DEB_F_PREFIX"Getting last TRX index, sent = %d\n", DEB_F_PREFIX_ARGS(SIP_TRX, fname), sent);

    if (sent) {
        for (i = MAX_REQ_OUTSTANDING - 1; i >= 0; i--) {
            if (ccb->sent_request[i].cseq_number != CCSIP_START_CSEQ) {
                CCSIP_DEBUG_TRX(DEB_F_PREFIX"Got TRX(%d) for sent req\n", DEB_F_PREFIX_ARGS(SIP_TRX, fname), i);
                return i;
            }
        }
    } else {
        for (i = MAX_REQ_OUTSTANDING - 1; i >= 0; i--) {
            if (ccb->recv_request[i].cseq_number != CCSIP_START_CSEQ) {
                CCSIP_DEBUG_TRX(DEB_F_PREFIX"Got TRX(%d) for recv req\n", DEB_F_PREFIX_ARGS(SIP_TRX, fname), i);
                return i;
            }
        }
    }
    return -1;
}

/*
 * This function gets the next cseq index that can be used to send
 * a request. It determines this by choosing the next available
 * cseq index
 */
int16_t
get_next_request_trx_index (ccsipCCB_t *ccb, boolean sent)
{
    const char *fname = "get_next_request_trx_index";
    int16_t i;

    if (ccb == NULL) {
        return -1;
    }

    CCSIP_DEBUG_TRX(DEB_F_PREFIX"Getting next TRX index, sent = %d\n", DEB_F_PREFIX_ARGS(SIP_TRX, fname), sent);
    if (sent) {
        for (i = 0; i < MAX_REQ_OUTSTANDING; i++) {
            if (ccb->sent_request[i].cseq_number == CCSIP_START_CSEQ) {
                CCSIP_DEBUG_TRX(DEB_F_PREFIX"Got TRX(%d) for sent req\n", DEB_F_PREFIX_ARGS(SIP_TRX, fname), i);
                return i;
            }
        }
    } else {
        for (i = 0; i < MAX_REQ_OUTSTANDING; i++) {
            if (ccb->recv_request[i].cseq_number == CCSIP_START_CSEQ) {
                CCSIP_DEBUG_TRX(DEB_F_PREFIX"Got TRX(%d) for recv req\n", DEB_F_PREFIX_ARGS(SIP_TRX, fname), i);
                return i;
            }
        }
    }
    CCSIP_DEBUG_TRX(DEB_F_PREFIX"Unable to get any open TRX!!\n", DEB_F_PREFIX_ARGS(SIP_TRX, fname));
    return -1;
}

/*
 * This function gets the index corresponding to a specific method
 */
int16_t
get_method_request_trx_index (ccsipCCB_t *ccb, sipMethod_t method, boolean sent)
{
    const char *fname = "get_method_request_trx_index";
    int16_t i;

    if (ccb == NULL) {
        return -1;
    }

    CCSIP_DEBUG_TRX(DEB_F_PREFIX"Getting TRX for method(%s), sent = %d\n",
                    DEB_F_PREFIX_ARGS(SIP_TRX, fname), sipGetMethodString(method), sent);

    if (sent) {
        for (i = 0; i < MAX_REQ_OUTSTANDING; i++) {
            if (ccb->sent_request[i].cseq_method == method) {
                CCSIP_DEBUG_TRX(DEB_F_PREFIX"Got TRX(%d) for sent method(%s)\n",
                                DEB_F_PREFIX_ARGS(SIP_TRX, fname), i, sipGetMethodString(method));
                return i;
            }
        }
    } else {
        for (i = 0; i < MAX_REQ_OUTSTANDING; i++) {
            if (ccb->recv_request[i].cseq_method == method) {
                CCSIP_DEBUG_TRX(DEB_F_PREFIX"Got TRX(%d) for recv method(%s)\n",
                                DEB_F_PREFIX_ARGS(SIP_TRX, fname), i, sipGetMethodString(method));
                return i;
            }
        }
    }
    CCSIP_DEBUG_TRX(DEB_F_PREFIX"Unable to find any TRX for method!!\n", DEB_F_PREFIX_ARGS(SIP_TRX, fname));
    return -1;
}

/*
 * This function cleans the CSeq array and removes the entry
 * corresponding to the method and compacts the array
 */
void
clean_method_request_trx (ccsipCCB_t *ccb, sipMethod_t method, boolean sent)
{
    const char *fname = "clean_method_request_trx";
    uint8_t     i, j, k;
    boolean     found = FALSE;
    sipTransaction_t *transactionp = NULL;

    if (ccb == NULL) {
        return;
    }

    CCSIP_DEBUG_TRX(DEB_F_PREFIX"Removing TRX for method(%s), sent = %d\n",
                    DEB_F_PREFIX_ARGS(SIP_TRX, fname), sipGetMethodString(method), sent);

    if (sent) {
        transactionp = &(ccb->sent_request[0]);
    } else {
        transactionp = &(ccb->recv_request[0]);
    }

    for (i = 0; i < MAX_REQ_OUTSTANDING && !found; i++) {
        if (transactionp[i].cseq_method == method) {
            transactionp[i].cseq_method = sipMethodInvalid;
            transactionp[i].cseq_number = CCSIP_START_CSEQ;
            strlib_free(transactionp[i].u.sip_via_header);
            strlib_free(transactionp[i].sip_via_sentby);
            CCSIP_DEBUG_TRX(DEB_F_PREFIX"Removed TRX(%d) for method(%s)\n",
                            DEB_F_PREFIX_ARGS(SIP_TRX, fname), i, sipGetMethodString(method));
            found = TRUE;
        }
        if (found) {
            k = i;
            for (j = k + 1; j < MAX_REQ_OUTSTANDING; j++, k++) {
                memcpy(&(transactionp[k]), &(transactionp[j]),
                       sizeof(sipTransaction_t));
            }
            // re-init the last transaction
            transactionp[MAX_REQ_OUTSTANDING - 1].cseq_method =
                sipMethodInvalid;
            transactionp[MAX_REQ_OUTSTANDING - 1].cseq_number =
                CCSIP_START_CSEQ;
            transactionp[MAX_REQ_OUTSTANDING - 1].u.sip_via_header =
                strlib_empty();
            transactionp[MAX_REQ_OUTSTANDING - 1].sip_via_sentby =
                strlib_empty();
        }
    }
}

line_t
get_dn_line_from_dn (const char *watcher)
{
    line_t dn_line;
    char   line_name[CC_MAX_DIALSTRING_LEN];

    for (dn_line = 1; dn_line <= MAX_REG_LINES; dn_line++) {
        config_get_line_string(CFGID_LINE_NAME, line_name, (int) dn_line,
                               sizeof(line_name));
        if (!cpr_strcasecmp(watcher, line_name)) {
            break;
        }
    }
    return dn_line;
}

boolean
validateHostName (char *str, char *dn)
{
    line_t dn_line = (line_t) -1;
    char   buffer[MAX_SIP_URL_LENGTH];
    char   ccm1_addr[MAX_SIP_URL_LENGTH];
    char   ccm2_addr[MAX_SIP_URL_LENGTH];
    char   ccm3_addr[MAX_SIP_URL_LENGTH];

    dn_line = get_dn_line_from_dn(dn);
    if (dn_line >= 1 && dn_line <= MAX_REG_LINES) {
        if (sip_regmgr_get_cc_mode(dn_line) == REG_MODE_NON_CCM) {
            config_get_line_string(CFGID_PROXY_ADDRESS, buffer, dn_line,
                                   MAX_SIP_URL_LENGTH);
            if (!strncmp(buffer, str, MAX_SIP_URL_LENGTH)) {
                return (TRUE);
            } else {
                return (FALSE);
            }
        } else {
            config_get_string(CFGID_CCM1_ADDRESS, ccm1_addr,
                              MAX_SIP_URL_LENGTH);
            config_get_string(CFGID_CCM2_ADDRESS, ccm2_addr,
                              MAX_SIP_URL_LENGTH);
            config_get_string(CFGID_CCM3_ADDRESS, ccm3_addr,
                              MAX_SIP_URL_LENGTH);

            if (!strncmp(ccm1_addr, str, MAX_SIP_URL_LENGTH) ||
                !strncmp(ccm2_addr, str, MAX_SIP_URL_LENGTH) ||
                !strncmp(ccm3_addr, str, MAX_SIP_URL_LENGTH)) {
                return (TRUE);
            } else {
                return (FALSE);
            }
        }
    }

    return (FALSE);
}

/*************************************************************
 * Function: sipGetSupportedOptionList
 * This function returns the list of the supported option tags.
 * The function may returns NULL pointer for the case that
 * there is no tag to add.
 **************************************************************/
static const char *
sipGetSupportedOptionList (ccsipCCB_t *ccb, sipMethod_t sipmethod)
{
	return (SIP_CISCO_SUPPORTED_REG_TAGS);
}

/*
 * Send REGISTER
 *
 * Send a SIP REGISTER request.
 * Assumes that
 * - connection has been setup beforehand.
 */
boolean
sipSPISendRegister (ccsipCCB_t *ccb,
                    boolean no_dns_lookup,
                    const char *user,
                    int expires_int)
{
    const char       fname[] = "SIPSPISendRegister";
    sipMessage_t    *request = NULL;
    char             obp_address[MAX_IPADDR_STR_LEN];
    cpr_ip_addr_t    ipaddr;
    boolean          obp_present = FALSE;
    boolean          send_result = FALSE;

    CPR_IP_ADDR_INIT(ipaddr);

    if (!(request = sipSPIBuildRegisterHeaders(ccb, user, expires_int))) {
        CCSIP_DEBUG_ERROR("%s: Error: Building Register Headers.\n",
                          fname);
        return (send_result);
    }

    /*
     * If we are being called as a result of a 4xx message we need to
     * respond to the same proxy which sent us the 4xx message so use
     * the previous ccb->reg.addr to send to.
     */
    config_get_string(CFGID_OUTBOUND_PROXY, obp_address, sizeof(obp_address));
    if ((cpr_strcasecmp(obp_address, UNPROVISIONED) != 0) &&
        (obp_address[0] != 0) &&
        (obp_address[0] != '0')) {
        obp_present = TRUE;
    }
    if ((!no_dns_lookup) &&
        ((obp_present == FALSE) || ((ccb->index == REG_BACKUP_CCB)))) {
        /* See if DNS SRV record is available     */
        dns_error_code = sipTransportGetServerAddrPort(ccb->reg.proxy, &ipaddr,
                                                       (uint16_t *)&ccb->reg.port,
                                                       &ccb->SRVhandle, FALSE);
        if (dns_error_code == 0) {
            /*
             * Found an SRV record. Use that address. If there is more
             * than one record in SRV response, setup to try all the
             * servers in the list
             */
            util_ntohl(&(ccb->reg.addr), &ipaddr);
        } else {
            /* Do a DNS A  record lookup on the proxy */
            dns_error_code = dnsGetHostByName(ccb->reg.proxy, &ipaddr, 100, 1);
            if (dns_error_code == 0) {
                util_ntohl(&ipaddr, &ipaddr);
                ccb->reg.addr = ipaddr;
            } else {
                ccb->reg.addr = ip_addr_invalid;
            }

        }
    }

    /* If we failed to get a valid IP address for the proxy
     * do not broadcast the REGISTER message, but bail instead.
     */
    if ((util_check_if_ip_valid(&(ccb->reg.addr))) || obp_present) {
        send_result = SendRequest(ccb, request, sipMethodRegister,
                                  FALSE, TRUE, FALSE);
    } else {
        err_msg("%s: Unable to retrieve address of proxy.\n", fname);
        free_sip_message(request);
    }

    if (!send_result) {
        clean_method_request_trx(ccb, sipMethodRegister, TRUE);
    }
    return (send_result);
}

char *
cc2siptype (cc_content_type_t type)
{
    switch (type) {
    default:
    case cc_content_type_unknown:
        return SIP_CONTENT_TYPE_UNKNOWN;
    case cc_content_type_SDP:
        return SIP_CONTENT_TYPE_SDP;
    case cc_content_type_CMXML:
        return SIP_CONTENT_TYPE_CMXML;
    case cc_content_type_sipfrag:
        return SIP_CONTENT_TYPE_SIPFRAG;
    }
}

cc_content_type_t
sip2cctype (uint8_t type)
{
    switch (type) {
    default:
    case SIP_CONTENT_TYPE_UNKNOWN_VALUE:
        return cc_content_type_unknown;
    case SIP_CONTENT_TYPE_SDP_VALUE:
        return cc_content_type_SDP;
    case SIP_CONTENT_TYPE_CMXML_VALUE:
        return cc_content_type_CMXML;
    case SIP_CONTENT_TYPE_SIPFRAG_VALUE:
        return cc_content_type_sipfrag;
    }
}

uint8_t
cc2sipdisp (cc_disposition_type_t type)
{
    switch (type) {
    default:
    case cc_disposition_unknown:
        return SIP_CONTENT_DISPOSITION_UNKNOWN_VALUE;
    case cc_disposition_render:
        return SIP_CONTENT_DISPOSITION_RENDER_VALUE;
    case cc_disposition_session:
        return SIP_CONTENT_DISPOSITION_SESSION_VALUE;
    case cc_dispostion_icon:
        return SIP_CONTENT_DISPOSITION_ICON_VALUE;
    case cc_disposition_alert:
        return SIP_CONTENT_DISPOSITION_ALERT_VALUE;
    case cc_disposition_precondition:
        return SIP_CONTENT_DISPOSITION_PRECONDITION_VALUE;
    }
}

cc_disposition_type_t
sip2ccdisp (uint8_t type)
{
    switch (type) {
    default:
    case SIP_CONTENT_DISPOSITION_UNKNOWN_VALUE:
        return cc_disposition_unknown;
    case SIP_CONTENT_DISPOSITION_RENDER_VALUE:
        return cc_disposition_render;
    case SIP_CONTENT_DISPOSITION_SESSION_VALUE:
        return cc_disposition_session;
    case SIP_CONTENT_DISPOSITION_ICON_VALUE:
        return cc_dispostion_icon;
    case SIP_CONTENT_DISPOSITION_ALERT_VALUE:
        return cc_disposition_alert;
    case SIP_CONTENT_DISPOSITION_PRECONDITION_VALUE:
        return cc_disposition_precondition;
    }
}


static boolean
sipSPIIsPrivate (ccsipCCB_t *ccb)
{
    int     blocking;
    boolean private_flag = FALSE;

    /*
     * If Caller ID Blocking is OFF or emergency route is ON,
     * display the actual name.  Otherwise, display "Anonymous".
     */
    config_get_value(CFGID_CALLERID_BLOCKING, &blocking, sizeof(blocking));
    if ((blocking & 1) && (ccb->routeMode != RouteEmergency)) {
        private_flag = TRUE;
    }
    return private_flag;
}

/*
 * sipSPISetRPID
 *
 * Set the RPID header string sent in either a request or response.
 */

static int
sipSPISetRPID (ccsipCCB_t *ccb, boolean request)
{
    const char *fname = "sipSPISetRPID";
    int         rpid_flag = RPID_DISABLED;
    boolean     private_flag;
    size_t      escaped_url_len;
    char        remote_party_id_buf[MAX_SIP_URL_LENGTH];
    char        line_name[MAX_LINE_NAME_SIZE];
    char        display_name[MAX_LINE_NAME_SIZE];
    char        src_addr_str[MAX_IPADDR_STR_LEN];
    cpr_ip_type ip_type;

    src_addr_str[0] = '\0';
    config_get_value(CFGID_REMOTE_PARTY_ID, &rpid_flag, sizeof(rpid_flag));

    if (rpid_flag != RPID_ENABLED) {
        return RPID_DISABLED;
    }

    if (!ccb) {
        CCSIP_DEBUG_ERROR("%s: Error: NULL ccb.\n", fname);
        return rpid_flag;
    }

    /* If RPID string is already set, just return */
    if (ccb->sip_remote_party_id[0]) {
        return RPID_ENABLED;
    }

    private_flag = sipSPIIsPrivate(ccb);

    config_get_string((CFGID_LINE_NAME + ccb->dn_line - 1), line_name,
                      sizeof(line_name));
    sip_config_get_display_name(ccb->dn_line, display_name,
                                sizeof(display_name));

    ip_type = sipTransportGetPrimServerAddress(ccb->dn_line, src_addr_str);
    
    sstrncpy(remote_party_id_buf, "\"", MAX_SIP_URL_LENGTH);
    escaped_url_len = 1;
    escaped_url_len +=
        sippmh_converQuotedStrToEscStr(display_name, strlen(display_name),
        			           remote_party_id_buf + escaped_url_len,
                                           MAX_SIP_URL_LENGTH - escaped_url_len,
                                           TRUE) - 1;
    strncat(remote_party_id_buf,"\" <sip:",MAX_SIP_URL_LENGTH - escaped_url_len );
    escaped_url_len = strlen(remote_party_id_buf);

    escaped_url_len +=
        sippmh_convertURLCharToEscChar(line_name, strlen(line_name),
                                       remote_party_id_buf + escaped_url_len,
                                       MAX_SIP_URL_LENGTH - escaped_url_len,
                                       FALSE);
    if (ip_type == CPR_IP_ADDR_IPV6) {
        snprintf(remote_party_id_buf + escaped_url_len,
                 MAX_SIP_URL_LENGTH - escaped_url_len,
                 "@[%s]>;party=%s;id-type=subscriber;privacy=%s;screen=yes",
                src_addr_str, (request ? "calling" : "called"),
                (private_flag ? "full" : "off"));
    } else {
        snprintf(remote_party_id_buf + escaped_url_len,
                 MAX_SIP_URL_LENGTH - escaped_url_len,
                 "@%s>;party=%s;id-type=subscriber;privacy=%s;screen=yes",
                src_addr_str, (request ? "calling" : "called"),
                (private_flag ? "full" : "off"));
    }

    ccb->sip_remote_party_id = strlib_update(ccb->sip_remote_party_id,
                                             remote_party_id_buf);

    return RPID_ENABLED;
}

static void
sipSPISetFrom (ccsipCCB_t *ccb)
{
    const char *fname = "sipSPISetFrom";
    boolean     private_flag;
    size_t      escaped_url_len;
    char       *sip_from_tag;
    char       *temp_from_tag;
    char       *sip_from_temp;
    char        line_name[MAX_LINE_NAME_SIZE];
    char        display_name[MAX_LINE_NAME_SIZE];
    char        dest_sip_addr_str[MAX_IPADDR_STR_LEN];
    char       *addr_str = 0;
    char        addr[MAX_IPADDR_STR_LEN];
    cpr_ip_type ip_type = CPR_IP_ADDR_INVALID;

    if (!ccb) {
        CCSIP_DEBUG_ERROR("%s: Error: NULL ccb.\n", fname);
        return;
    }

    ipaddr2dotted(dest_sip_addr_str, &ccb->dest_sip_addr);

    if ((ccb->routeMode == RouteEmergency) ||
        (ccb->proxySelection == SIP_PROXY_BACKUP)) {
        addr_str = dest_sip_addr_str;
    } else {
        ip_type = sipTransportGetPrimServerAddress(ccb->dn_line, addr);
        addr_str = addr;
    }

    sip_from_temp = strlib_open(ccb->sip_from, MAX_SIP_URL_LENGTH);

    if (sip_from_temp == NULL) {
        CCSIP_DEBUG_ERROR("%s: Error: sip_from_temp is NULL.\n", fname);
        return;
    }

    private_flag = sipSPIIsPrivate(ccb);

    if (private_flag == TRUE) {
        if (ip_type == CPR_IP_ADDR_IPV6) {
            snprintf(sip_from_temp, MAX_SIP_URL_LENGTH, "\"%s\" <sip:%s@[%s]>",
                 SIP_HEADER_ANONYMOUS_STR, SIP_HEADER_ANONYMOUS_STR,
                 addr_str);
        } else {
            snprintf(sip_from_temp, MAX_SIP_URL_LENGTH, "\"%s\" <sip:%s@%s>",
                 SIP_HEADER_ANONYMOUS_STR, SIP_HEADER_ANONYMOUS_STR,
                 addr_str);
        }
    } else {
        /* From header needs to have global address in it */
        config_get_string((CFGID_LINE_NAME + ccb->dn_line - 1), line_name,
                          sizeof(line_name));
        sip_config_get_display_name(ccb->dn_line, display_name,
                                    sizeof(display_name));
        sstrncpy(sip_from_temp, "\"", MAX_SIP_URL_LENGTH);
        escaped_url_len = 1;
        escaped_url_len +=
        	sippmh_converQuotedStrToEscStr(display_name, strlen(display_name),
                                           sip_from_temp + escaped_url_len,
                                           MAX_SIP_URL_LENGTH - escaped_url_len,
                                           TRUE) - 1;
        strncat(sip_from_temp,"\" <sip:",MAX_SIP_URL_LENGTH - escaped_url_len );
        escaped_url_len = strlen(sip_from_temp);
        escaped_url_len +=
            sippmh_convertURLCharToEscChar(line_name, strlen(line_name),
                                           sip_from_temp + escaped_url_len,
                                           MAX_SIP_URL_LENGTH - escaped_url_len,
                                           TRUE) - 1;

        /* construct From header's URI */
        if (ip_type == CPR_IP_ADDR_IPV6) {

            snprintf(sip_from_temp + escaped_url_len,
                     MAX_SIP_URL_LENGTH - escaped_url_len, "@[%s]>",
                     addr_str);
        } else {
            snprintf(sip_from_temp + escaped_url_len,
                     MAX_SIP_URL_LENGTH - escaped_url_len, "@%s>",
                     addr_str);
        }
    }

    /* Now add tag to the From header */
    strncat(sip_from_temp, ";tag=",
            MAX_SIP_URL_LENGTH - strlen(sip_from_temp) - 1);
    temp_from_tag = ccsip_find_preallocated_sip_local_tag(ccb->dn_line);
    sip_from_tag = strlib_open(ccb->sip_from_tag, MAX_SIP_URL_LENGTH);
    if (temp_from_tag == NULL) {
        if (sip_from_tag) {
            sip_util_make_tag(sip_from_tag);
            strncat(sip_from_temp, sip_from_tag,
                    MAX_SIP_URL_LENGTH - strlen(sip_from_temp) - 1);
        }
    } else {
        if (sip_from_tag) {
            sstrncpy(sip_from_tag, temp_from_tag, MAX_SIP_URL_LENGTH);
            strncat(sip_from_temp, temp_from_tag,
                    MAX_SIP_URL_LENGTH - strlen(sip_from_temp) - 1);
        }
        ccsip_free_preallocated_sip_local_tag(ccb->dn_line);
    }
    ccb->sip_from_tag = strlib_close(sip_from_tag);
    ccb->sip_from = strlib_close(sip_from_temp);
}

/*
 * Send INVITE
 *
 * As the name suggests, called to send a SIP INVITE request.
 * Assumes that
 * - connection has been setup beforehand.
 * - SDP description has been setup.
 * Does not affect call state.
 */
boolean
sipSPISendInvite (ccsipCCB_t *ccb, sipInviteType_t inviteType,
                  boolean initInvite)
{
    const char      *fname = "SIPSPISendInvite";
    sipMessage_t    *request = NULL;
    sipRet_t         flag = STATUS_SUCCESS;
    sipRet_t         tflag = STATUS_SUCCESS;
    char             called_number[MAX_SIP_URL_LENGTH];
    ccsipCCB_t      *referccb = NULL;
    boolean          inviterefer = FALSE;
    sipMessageFlag_t messageflag;
    int              i;
    int              rpid_flag;

    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_REQUEST),
                      fname, "INVITE");

    if (ccb->wastransferred) {
        inviterefer = TRUE;
    }
    // This routine can be called in three different contexts. These are the
    // important differences between them:
    // CONTEXT         inviteType                       referccb
    // -----------------------------------------------------------
    // Normal          SIP_INVITE_TYPE_NORMAL           NULL
    // Blind Xfer      SIP_INVITE_TYPE_NORMAL           valid
    // Att Xfer        SIP_INVITE_TYPE_TRANSFER         valid

    referccb = sip_sm_get_target_call_by_gsm_id(ccb->gsm_id);

    // If there is a referccb, try to get authentication stuff from it
    if (referccb != NULL) {
        if (referccb->refer_proxy_auth != NULL) {
            ccb->refer_proxy_auth = cpr_strdup(referccb->refer_proxy_auth);
        }
    }

    if (inviteType == SIP_INVITE_TYPE_TRANSFER) {
        // For attended transfers we need to make a local copy of the
        // referccb if it exists
        if (NULL != referccb) {
            ccb->sip_referredBy = strlib_update(ccb->sip_referredBy,
                                                referccb->sip_referredBy);
            if (referccb->featuretype == CC_FEATURE_XFER) {
                ccb->sipxfercallid = strlib_update(ccb->sipxfercallid,
                                                   referccb->sipxfercallid);
                if (ccb->sipxfercallid) {
                    if (ccb->sipxfercallid[0] != '\0') {
                        ccb->wastransferred = TRUE;
                        inviterefer = TRUE;
                    }
                }
            }
        }

        if (inviterefer == FALSE) {
            //Not enough information for starting attended transfer
            CCSIP_DEBUG_ERROR("%s: Error:Replaces INVITE build unsuccessful.\n",
                              fname);
            return (FALSE);
        }
    }

    if (inviteType != SIP_INVITE_TYPE_REDIRECTED) {
        ccb->sip_to = strlib_update(ccb->sip_to, ccb->calledNumber);
    }


    /*
     * Set from header
     */
    sipSPISetFrom(ccb);

    /*
     * Set RPID header.
     */
    rpid_flag = sipSPISetRPID(ccb, TRUE);

    /*
     * The calledNumber needs to be rewritten with the backup proxy
     * ip address if the backup proxy is active
     */
    if (ccb->proxySelection == SIP_PROXY_BACKUP) {
        sstrncpy(called_number, ccb->calledDisplayedName, MAX_SIP_URL_LENGTH);

        /*
         * NOTE: Need to replace with new routine ???
         */
        if (called_number[0] != '\0') {
            sip_sm_util_normalize_name(ccb, called_number);
        }
    }

    // Add Create Request Here
    messageflag.flags = 0;
    messageflag.flags |= SIP_HEADER_ACCEPT_BIT |
                         SIP_HEADER_EXPIRES_BIT |
                         SIP_HEADER_CONTACT_BIT |
                         SIP_HEADER_DIVERSION_BIT |
                         SIP_HEADER_SUPPORTED_BIT |
                         SIP_HEADER_ALLOW_EVENTS_BIT |
                         SIP_HEADER_ALLOW_BIT |
                         SIP_HEADER_RECV_INFO_BIT |
                         SIP_HEADER_REQUIRE_BIT;

    if (ccb->authen.authorization != NULL) {
        messageflag.flags |= SIP_HEADER_AUTHENTICATION_BIT;
    }
    if (ccb->refer_proxy_auth != NULL) {
        messageflag.flags |= SIP_HEADER_PROXY_AUTH_BIT;
    }
    if (ccb->sip_referredBy[0] != '\0') {
        messageflag.flags |= SIP_HEADER_REFERRED_BY_BIT;
    }
    if (((TRUE == inviterefer) || (ccb->flags & SENT_INVITE_REPLACE)) &&
        ('\0' != ccb->sipxfercallid[0])) {
        messageflag.flags |= SIP_HEADER_REPLACES_BIT;
    }
    if (ccb->sip_reqby[0]) {
        messageflag.flags |= SIP_HEADER_REQUESTED_BY_BIT;
    }
    if (rpid_flag == RPID_ENABLED) {
        messageflag.flags |= SIP_HEADER_REMOTE_PARTY_ID_BIT;
    }
    if (ccb->out_call_info != NULL) {
        messageflag.flags |= SIP_HEADER_CALL_INFO_BIT;
    }
    if (ccb->join_info != NULL) {
        messageflag.flags |= SIP_HEADER_JOIN_INFO_BIT;
    }

    /* Write SDP */
    messageflag.flags |= SIP_HEADER_CONTENT_TYPE_BIT;
    request = GET_SIP_MESSAGE();
    if (request == NULL) {
        CCSIP_DEBUG_ERROR("%s: Error: Unable to allocate INVITE request\n",
                          fname);
        return (FALSE);
    }
    messageflag.extflags = 0;
    if (CreateRequest(ccb, messageflag, sipMethodInvite, request,
                      initInvite, 0)) {
        tflag = HSTATUS_SUCCESS;
    } else {
        tflag = HSTATUS_FAILURE;
    }
    UPDATE_FLAGS(flag, tflag);

    ccb->ReqURIOriginal = strlib_update(ccb->ReqURIOriginal, ccb->ReqURI);

    /*
     * If overloaded headers are present from the 302, add them to the
     * Invite message
     */
    if ((ccb->redirect_info) &&
        (ccb->redirect_info->sipContact->locations[0]->genUrl) &&
        (ccb->redirect_info->sipContact->locations[0]->genUrl->u.sipUrl) &&
        (ccb->redirect_info->sipContact->locations[0]->genUrl->u.sipUrl->headerp) &&
        (inviteType == SIP_INVITE_TYPE_REDIRECTED)) {
        for (i = 0;
             i < ccb->redirect_info->sipContact->locations[0]->genUrl->u.sipUrl->num_headers;
             i++) {
            tflag = sippmh_add_text_header(request,
                        ccb->redirect_info->sipContact->locations[0]->genUrl->u.
                        sipUrl->headerp[i].attr,
                        ccb->redirect_info->sipContact->locations[0]->genUrl->u.
                        sipUrl->headerp[i].value);
            UPDATE_FLAGS(flag, tflag);
        }
    }

    if (flag != STATUS_SUCCESS) {
        free_sip_message(request);
        CCSIP_DEBUG_ERROR("%s: Error: INVITE message build unsuccessful.\n",
                          fname);
        clean_method_request_trx(ccb, sipMethodInvite, TRUE);
        return (FALSE);
    }

    ccb->retx_counter = 0;

    if (SendRequest(ccb, request, sipMethodInvite, FALSE, TRUE, TRUE)
            == FALSE) {
        clean_method_request_trx(ccb, sipMethodInvite, TRUE);
        return (FALSE);
    } else {
        return (TRUE);
    }
}

sipRet_t
sipSPIAddCallStats (ccsipCCB_t *ccb, sipMessage_t *msg)
{
    int      call_stats_flag;
    sipRet_t tflag = STATUS_SUCCESS;

    config_get_value(CFGID_CALL_STATS, &call_stats_flag, sizeof(call_stats_flag));
    if ((call_stats_flag) && (ccb->kfactor_ptr)) {
        if (ccb->kfactor_ptr->rxstats[0] != NUL) {
            tflag = sippmh_add_text_header(msg, SIP_RX_CALL_STATS, ccb->kfactor_ptr->rxstats);
        }
        if (ccb->kfactor_ptr->txstats[0] != NUL) {
            tflag = sippmh_add_text_header(msg, SIP_TX_CALL_STATS, ccb->kfactor_ptr->txstats);
        }
    }
    return tflag;
}

boolean
sipSPISendInviteMidCall (ccsipCCB_t *ccb, boolean expires)
{
    const char      *fname = "sipSPISendInviteMidCall";
    sipMessage_t    *request = NULL;
    sipRet_t         flag = STATUS_SUCCESS;
    sipRet_t         tflag = STATUS_SUCCESS;
    sipMessageFlag_t messageflag;
    int              rpid_flag;

    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_REQUEST),
                      fname, "INVITE");

    /*
     * Build the request Here
     */
    messageflag.flags = 0;
    messageflag.flags |= SIP_HEADER_CONTACT_BIT |
                         SIP_HEADER_ROUTE_BIT |
                         SIP_HEADER_CONTENT_TYPE_BIT |
                         SIP_HEADER_ACCEPT_BIT |
                         SIP_HEADER_SUPPORTED_BIT |
                         SIP_HEADER_ALLOW_EVENTS_BIT |
                         SIP_HEADER_ALLOW_BIT |
                         SIP_HEADER_RECV_INFO_BIT |
                         SIP_HEADER_REQUIRE_BIT;

    if ((ccb->authen.authorization != NULL) &&
        ((ccb->state == SIP_STATE_SENT_INVITE) ||
         (ccb->state == SIP_STATE_SENT_MIDCALL_INVITE))) {
        messageflag.flags |= SIP_HEADER_AUTHENTICATION_BIT;
    }

    if ((ccb->authen.authorization != NULL) &&
        (ccb->state == SIP_STATE_SENT_INVITE)) {
        if (ccb->join_info != NULL) {
            messageflag.flags |= SIP_HEADER_JOIN_INFO_BIT;
        }
    }

    if ('\0' != ccb->sipxfercallid[0]) {
        messageflag.flags |= SIP_HEADER_REPLACES_BIT;
    }

    if (expires > 0) {
        messageflag.flags |= SIP_HEADER_EXPIRES_BIT;
    }

    if (ccb->sip_referredBy[0]) {
        messageflag.flags |= SIP_HEADER_REFERRED_BY_BIT;
    }

    /*
     * Set RPID header.
     */
    rpid_flag = sipSPISetRPID(ccb, TRUE);
    if (rpid_flag == RPID_ENABLED) {
        messageflag.flags |= SIP_HEADER_REMOTE_PARTY_ID_BIT;
    }

    if (ccb->out_call_info) {
        messageflag.flags |= SIP_HEADER_CALL_INFO_BIT;
    }

    request = GET_SIP_MESSAGE();
    messageflag.extflags = 0;
    if (CreateRequest(ccb, messageflag, sipMethodInvite, request, FALSE, 0)) {
        tflag = HSTATUS_SUCCESS;
    } else {
        tflag = HSTATUS_FAILURE;
    }

    UPDATE_FLAGS(flag, tflag);


    tflag = sipSPIAddCallStats(ccb, request);
    UPDATE_FLAGS(flag, tflag);

    /* Recalc and add Authorization header if needed */
    if ((ccb->state != SIP_STATE_SENT_INVITE) &&
        (ccb->state != SIP_STATE_SENT_MIDCALL_INVITE)) {
        sipSPIGenerateGenAuthorizationResponse(ccb, request, &flag,
                                               SIP_METHOD_INVITE);
    }

    /* Write SDP */
    if (flag != STATUS_SUCCESS) {
        free_sip_message(request);
        CCSIP_DEBUG_ERROR("%s: Error: INVITE message build unsuccessful.\n",
                          fname);
        clean_method_request_trx(ccb, sipMethodInvite, TRUE);
        return (FALSE);
    }
    /*
     * Currently the following field is being set to zero when the
     * midcall invite is not being done as a response to an authorization
     * challenge. So utilize this fact to prevent proxy backup selection
     * on a mid-call invite. The end result is to stick with the same
     * proxy as the original invite.
     */
    if (ccb->authen.cred_type == 0) {
        ccb->proxySelection = SIP_PROXY_DO_NOT_CHANGE_MIDCALL;
    }

    /* Successfully constructed msg with new URI for this INVITE to send out.
     * Update URIOriginal before sending out.
     */
    ccb->ReqURIOriginal = strlib_update(ccb->ReqURIOriginal, ccb->ReqURI); 
    
    /* Enable reTx and send */
    ccb->retx_counter = 0;
    if (SendRequest(ccb, request, sipMethodInvite, TRUE, TRUE, TRUE) == FALSE) {
        clean_method_request_trx(ccb, sipMethodInvite, TRUE);
        return (FALSE);
    } else {
        return (TRUE);
    }
}


/*
 * Sends the ACK request.
 * Assumes that
 * - the connection is setup.
 * Appends session description if sd = TRUE. (This would
 * be the case, if we want to change the codec that
 * was sent on the INVITE.
 * Does not affect call state.
 */
boolean
sipSPISendAck (ccsipCCB_t *ccb, sipMessage_t *response)
{
    const char      *fname = "sipSPISendAck";
    sipMessage_t    *request = NULL;
    sipRet_t        flag = STATUS_SUCCESS;
    sipRet_t        tflag = STATUS_SUCCESS;
    sipMessageFlag_t messageflag;
    uint32_t         response_cseq_number = 0;
    sipCseq_t       *response_cseq_structure;
    const char      *response_cseq;
    int16_t          trx_index = -1;
    boolean          retval;
    int              rpid_flag;

    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_REQUEST),
                      fname, "ACK");

    /*
     * Build the request
     */
    messageflag.flags = 0;
    messageflag.flags = SIP_HEADER_ROUTE_BIT |
                        SIP_HEADER_RECV_INFO_BIT;

    /*
     * Cseq number in the response could be different from that in ccb.
     * If there was no response as in
     * ccsip_handle_sentinviteconnected_ev_cc_connected_ack
     * then use ccb for getting Cseq number
     */
    if (response) {
        response_cseq = sippmh_get_cached_header_val(response, CSEQ);
        if (!response_cseq) {
            CCSIP_DEBUG_ERROR("%s: Error: Unable to obtain response CSeq "
                              "header.\n", fname);
            return (FALSE);
        }
        response_cseq_structure = sippmh_parse_cseq(response_cseq);
        if (!response_cseq_structure) {
            CCSIP_DEBUG_ERROR("%s: Error: Unable to parse response CSeq "
                              "header.\n", fname);
            return (FALSE);
        }
        response_cseq_number = response_cseq_structure->number;
        cpr_free(response_cseq_structure);
        CCSIP_DEBUG_STATE(DEB_F_PREFIX"Cseq from response = %d \n", 
            DEB_F_PREFIX_ARGS(SIP_ACK, "sipSPISendAck"), response_cseq_number);
    } else {
        trx_index = get_method_request_trx_index(ccb, sipMethodInvite, TRUE);
        if (trx_index < 0) {
            return (FALSE);
        }
        response_cseq_number = ccb->sent_request[trx_index].cseq_number;
        CCSIP_DEBUG_STATE(DEB_F_PREFIX"Cseq from ccb = %d \n", 
            DEB_F_PREFIX_ARGS(SIP_ACK, "sipSPISendAck"), response_cseq_number);
    }

    messageflag.flags |= SIP_HEADER_CONTENT_LENGTH_BIT;
    if (ccb->authen.authorization != NULL) {
        messageflag.flags |= SIP_HEADER_AUTHENTICATION_BIT;
    }

    /*
     * Set RPID header.
     */
    rpid_flag = sipSPISetRPID(ccb, TRUE);
    if (rpid_flag == RPID_ENABLED) {
        messageflag.flags |= SIP_HEADER_REMOTE_PARTY_ID_BIT;
    }

    request = GET_SIP_MESSAGE();
    messageflag.extflags = 0;
    if (CreateRequest(ccb, messageflag, sipMethodAck, request, FALSE,
                      response_cseq_number)) {
        tflag = HSTATUS_SUCCESS;
    } else {
        tflag = HSTATUS_FAILURE;
    }

    UPDATE_FLAGS(flag, tflag);
    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (request)
            free_sip_message(request);

        clean_method_request_trx(ccb, sipMethodInvite, TRUE);
        return (FALSE);
    }

    /* Send message */
    retval = SendRequest(ccb, request, sipMethodAck, FALSE, FALSE, FALSE);

    // We are done with this INVITE request so lets clear and reorder our
    // cseq list of outstanding requests. Assumes that ACK will only be
    // sent for INVITE. Also note that no trx block is allocated for Ack
    // and so there is no need to free it.
    clean_method_request_trx(ccb, sipMethodInvite, TRUE);
    return (retval);
}


/*
 * Sends the BYE request for the call.
 * Assumes that
 * - the connection is setup.
 * Does not change the state, but changes the disconnection flags.
 */
void
sipSPISendBye (ccsipCCB_t *ccb, char *alsoString, sipMessage_t *pForked200)
{
    const char       *fname = "sipSPISendBye";
    sipMessage_t     *request = NULL;
    sipRet_t          flag = STATUS_SUCCESS;
    sipRet_t          tflag = STATUS_SUCCESS;
    sipContact_t     *stored_contact_info = NULL;
    sipRecordRoute_t *stored_record_route_info = NULL;
    static char       stored_sip_to[MAX_SIP_URL_LENGTH];
    static char       stored_sip_from[MAX_SIP_URL_LENGTH];
    static char       last_route[MAX_SIP_URL_LENGTH];
    const char       *forked200_contact = NULL;
    const char       *forked200_record_route = NULL;
    const char       *forked200_to = NULL;
    const char       *forked200_from = NULL;
    sipMessageFlag_t  messageflag;


    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_REQUEST),
                      fname, "BYE");

    /*
     * If this BYE is in response to the secondary forked 200 OK message
     * (from the callee being rejected), then we need to use the
     * Contact and Record-Route of this 200 OK message, rather than the
     * stored ccb->contact_info and ccb->record_route_info.
     * So, if pForked200 exists, then we will:
     * - save existing ccb->contact_info and ccb->record_route_info fields
     * - parse Contact and Record-Route of the forked 200
     * - use these values to form this BYE's Req-URI
     * - restore the original ccb->contact_info and ccb->record_route_info
     */
    if (pForked200) {
        stored_contact_info = ccb->contact_info;
        stored_record_route_info = ccb->record_route_info;
        sstrncpy(stored_sip_to, ccb->sip_to, MAX_SIP_URL_LENGTH);
        sstrncpy(stored_sip_from, ccb->sip_from, MAX_SIP_URL_LENGTH);

        forked200_contact = sippmh_get_cached_header_val(pForked200, CONTACT);
        forked200_record_route =
            sippmh_get_cached_header_val(pForked200, RECORD_ROUTE);
        forked200_to = sippmh_get_cached_header_val(pForked200, TO);
        forked200_from = sippmh_get_cached_header_val(pForked200, FROM);

        if (forked200_contact) {
            ccb->contact_info = sippmh_parse_contact(forked200_contact);
        }
        if (forked200_record_route) {
            ccb->record_route_info =
                sippmh_parse_record_route(forked200_record_route);
        }
        ccb->sip_to = strlib_update(ccb->sip_to, forked200_to);
        ccb->sip_from = strlib_update(ccb->sip_from, forked200_from);
    }

    /*
     * Build the request
     */
    messageflag.flags = 0;
    messageflag.flags = SIP_HEADER_CONTENT_LENGTH_BIT;

    request = GET_SIP_MESSAGE();
    messageflag.extflags = 0;
    if (CreateRequest(ccb, messageflag, sipMethodBye, request, FALSE, 0)) {
        tflag = HSTATUS_SUCCESS;
    } else {
        tflag = HSTATUS_FAILURE;
    }

    UPDATE_FLAGS(flag, tflag);


    /* add in call stats header if needed */
    tflag = sipSPIAddCallStats(ccb, request);
    UPDATE_FLAGS(flag, tflag);

    if (alsoString) {
        tflag = sippmh_add_text_header(request, SIP_HEADER_ALSO, alsoString);
        UPDATE_FLAGS(flag, tflag);
    }

    memset(last_route, 0, MAX_SIP_URL_LENGTH);
    tflag = (sipSPIAddRouteHeaders(request, ccb, last_route, MAX_SIP_URL_LENGTH)) ?
        STATUS_SUCCESS : STATUS_FAILURE;
    UPDATE_FLAGS(flag, tflag);
    sipSPIGenerateGenAuthorizationResponse(ccb, request, &flag, SIP_METHOD_BYE);

    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (request)
            free_sip_message(request);
        if (alsoString)
            cpr_free(alsoString);
        clean_method_request_trx(ccb, sipMethodBye, TRUE);
        return;
    }

    ccb->retx_counter = 0;
    /* Send message */
    (void) SendRequest(ccb, request, sipMethodBye, FALSE, TRUE, FALSE);

    /*
     * Update history
     */
    /* Record Also header if any */
    if (alsoString) {
        if (alsoString[0]) {
            sstrncpy(gCallHistory[ccb->index].last_bye_also_string, alsoString,
                     MAX_SIP_URL_LENGTH);
        }
        cpr_free(alsoString);
    } else {
        memset(gCallHistory[ccb->index].last_bye_also_string, 0,
               MAX_SIP_URL_LENGTH);
    }

    /* Record current Route */
    if (last_route[0]) {
        sstrncpy(gCallHistory[ccb->index].last_route, last_route,
                 MAX_SIP_URL_LENGTH);
    } else {
        memset(gCallHistory[ccb->index].last_route, 0, MAX_SIP_URL_LENGTH);
    }
    /* Record current Request-URI */
    if (ccb->ReqURI[0]) {
        sstrncpy(gCallHistory[ccb->index].last_route_request_uri, ccb->ReqURI,
                 MAX_SIP_URL_LENGTH);
    } else {
        memset(gCallHistory[ccb->index].last_route_request_uri, 0,
               MAX_SIP_URL_LENGTH);
    }

// bugid: CSCsz34666
//    /*
//     * Store call history info
//     */
//    if ((int) (ccb->index) <= TEL_CCB_END) {
//        memcpy(gCallHistory[ccb->index].last_call_id, ccb->sipCallID,
//               MAX_SIP_CALL_ID);
//    }

    /*
     * Restore the original ccb->contact and ccb->record_route fields
     */
    if (pForked200) {
        if (ccb->contact_info) {
            sippmh_free_contact(ccb->contact_info);
        }
        ccb->contact_info = stored_contact_info;
        if (ccb->record_route_info) {
            sippmh_free_record_route(ccb->record_route_info);
        }
        ccb->record_route_info = stored_record_route_info;

        ccb->sip_to = strlib_update(ccb->sip_to, stored_sip_to);
        ccb->sip_from = strlib_update(ccb->sip_from, stored_sip_from);
    }

    return;
}



/*
 * Sends the SIP CANCEL request, to disconnect a call that
 * is not in the active state.
 */
void
sipSPISendCancel (ccsipCCB_t *ccb)
{
    const char      *fname   = "sipSPISendCancel";
    sipMessage_t    *request = NULL;
    sipRet_t         flag    = STATUS_SUCCESS;
    sipRet_t         tflag   = STATUS_SUCCESS;
    sipMessageFlag_t messageflag;
    char            *temp = NULL;
    char             local_cpy[MAX_SIP_URL_LENGTH];
    string_t         hold_to_tag = strlib_copy(ccb->sip_to);

    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_REQUEST),
                      fname, "CANCEL");

    messageflag.flags = 0;
    ccb->authen.cred_type = 0;

    messageflag.flags = SIP_HEADER_CONTENT_LENGTH_BIT;

    /* Remove the to_tag from the CANCEL message if present */
    sstrncpy(local_cpy, ccb->sip_to, MAX_SIP_URL_LENGTH);
    temp = strstr(local_cpy, ">");
    if (temp != NULL) {
        *(temp + 1) = '\0';
    }
    ccb->sip_to = strlib_update(ccb->sip_to, local_cpy);

    request = GET_SIP_MESSAGE();
    messageflag.extflags = 0;
    if (CreateRequest(ccb, messageflag, sipMethodCancel, request, FALSE, 0)) {
        tflag = HSTATUS_SUCCESS;
    } else {
        tflag = HSTATUS_FAILURE;
    }

    /* restore the to_tag if it was there */
    if (hold_to_tag) {
        ccb->sip_to = strlib_update(ccb->sip_to, hold_to_tag);
        strlib_free(hold_to_tag);
    }
    hold_to_tag = strlib_empty();

    UPDATE_FLAGS(flag, tflag);
    /* Recalc and add Authorization header if needed */
    sipSPIGenerateGenAuthorizationResponse(ccb, request, &flag,
                                           SIP_METHOD_CANCEL);

    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (request)
            free_sip_message(request);
        clean_method_request_trx(ccb, sipMethodCancel, TRUE);
        return;
    }
    /* Record current Request-URI */
    if (ccb->ReqURI[0]) {
        sstrncpy(gCallHistory[ccb->index].last_route_request_uri, ccb->ReqURI,
                 MAX_SIP_URL_LENGTH);
    } else {
        memset(gCallHistory[ccb->index].last_route_request_uri, 0,
               MAX_SIP_URL_LENGTH);
    }
    if (SendRequest(ccb, request, sipMethodCancel, FALSE, TRUE, FALSE)
            == FALSE) {
        clean_method_request_trx(ccb, sipMethodCancel, TRUE);
        return;
    } else {
        return;
    }
}

void
sip_platform_icmp_unreachable_callback (void *ccb, uint32_t ipaddr)
{
    static const char fname[] = "sip_platform_icmp_unreachable_callback";
    uint32_t *icmp_msg;

    icmp_msg = (uint32_t *) SIPTaskGetBuffer(sizeof(uint32_t));
    if (!icmp_msg) {
        CCSIP_DEBUG_ERROR("%s: Error: get buffer failed.\n", fname);
        return;
    }
    *icmp_msg = ((ccsipCCB_t *)ccb)->index;

    if (SIPTaskSendMsg(SIP_ICMP_UNREACHABLE, (cprBuffer_t)icmp_msg,
                       sizeof(uint32_t), (void *)(long)ipaddr) == CPR_FAILURE) {
        CCSIP_DEBUG_ERROR("%s: Error: send msg failed.\n", fname);
        cprReleaseBuffer((cprBuffer_t)icmp_msg);
    }
    return;
}

/*
 * Sends the SIP REFER request, to Transfer the call
 *
 * Parameters:
 *     ccb - reference tio the existing call control block
 *     referto - Dial string to make a call (If null it will pick up from ccb)
 *     referto_typ  to indicate if the referto is trasnfer or token refer
 *
 */
boolean
sipSPISendRefer (ccsipCCB_t *ccb, char *referto, sipRefEnum_e referto_type)
{
    const char     *fname    = "sipSPISendRefer";
    sipMessage_t   *request  = NULL;
    sipRet_t        flag     = STATUS_SUCCESS;
    sipRet_t        tflag    = STATUS_SUCCESS;
    ccsipCCB_t     *xfer_ccb = NULL;
    char            tempreferto[MAX_SIP_URL_LENGTH + 2];
    char            callid[MAX_SIP_HEADER_LENGTH + 2];
    sipMessageFlag_t messageflag;
    char           *semi            = NULL;
    char           *left_bracket    = NULL;
    char           *right_bracket   = NULL;
    char           *msg_referto     = NULL;
    string_t        copy_of_referto = NULL;
    int             rpid_flag;
    char            *ref_to_callid = NULL;
    const char      *to_tag = NULL;
    const char      *from_tag = NULL;
    boolean         dm_info = FALSE;
    sipJoinInfo_t   join_info;

    memset(&join_info, 0, sizeof(join_info));

    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_REQUEST),
                      fname, "REFER");
    /*
     * Build the request
     */

    if (sipSPIGenerateReferredByHeader(ccb) == FALSE) {
        tflag = HSTATUS_FAILURE;
    } else {
        tflag = HSTATUS_SUCCESS;
    }
    UPDATE_FLAGS(flag, tflag);

    messageflag.flags = 0;
    /*
     * Don't add the content length bit here. Content length
     * needs to be the last in the header. TCP uses the content
     * length to do framing.
     */
    messageflag.flags = SIP_HEADER_CONTACT_BIT | SIP_HEADER_ROUTE_BIT;

    /*
     * Set RPID header.
     */
    rpid_flag = sipSPISetRPID(ccb, TRUE);
    if (rpid_flag == RPID_ENABLED) {
        messageflag.flags |= SIP_HEADER_REMOTE_PARTY_ID_BIT;
    }

    if (referto) {
        // Get a new call-id if this REFER is for fallback token registration
        if (strncmp(referto, TOKEN_REFER_TO, sizeof(TOKEN_REFER_TO)) == 0) {
            ccb->sipCallID[0] = '\0';
            sip_util_get_new_call_id(ccb);
        }
    }

    request = GET_SIP_MESSAGE();
    messageflag.extflags = 0;
    if (CreateRequest(ccb, messageflag, sipMethodRefer, request, FALSE, 0)) {
        tflag = HSTATUS_SUCCESS;
    } else {
        tflag = HSTATUS_FAILURE;
    }

    UPDATE_FLAGS(flag, tflag);

    /* Recalc and add Authorization header if needed */
    sipSPIGenerateGenAuthorizationResponse(ccb, request, &flag,
                                           SIP_METHOD_REFER);

    memset(tempreferto, 0, MAX_SIP_URL_LENGTH + 2);
    memset(callid, 0, MAX_SIP_HEADER_LENGTH + 2);

    /* see if we have ;user= */
    if (referto) {
        semi = strchr(referto, ';');
    }

    if (CC_FEATURE_XFER == ccb->featuretype) {
        // The con_call_id is filled up by GSM when it opens up the line This
        // is used to cross reference the call_ids
        xfer_ccb = sip_sm_get_ccb_by_target_call_id(ccb->con_call_id);

        if (xfer_ccb != NULL) {
            ref_to_callid = xfer_ccb->sipCallID;
            to_tag = xfer_ccb->sip_to_tag;
            from_tag = xfer_ccb->sip_from_tag;
        }

        if (xfer_ccb != NULL || dm_info == TRUE) {
            int i = 0;

            // Create Refer_to header with replace id (call_id of other call)
            // and To-Tag of other call - escape the replaced callid
            while (*ref_to_callid != '\0') {
                if (*ref_to_callid != '@') {
                    callid[i++] = *ref_to_callid;
                } else {
                    callid[i++] = '%';
                    callid[i++] = '4';
                    callid[i++] = '0';
                }
                ref_to_callid++;
            }
            callid[i] = '\0';

            /* first get rid of opening and closing braces, if there */
            copy_of_referto = strlib_copy(referto);
            if (copy_of_referto) {
                left_bracket = strpbrk(copy_of_referto, "<");
            }
            if (left_bracket) {
                left_bracket++;
                right_bracket = strchr(left_bracket, '>');
                if (right_bracket) {
                    *right_bracket++ = 0;
                }
                msg_referto = left_bracket;
            } else {
                msg_referto = referto;
            }
            if (msg_referto) {
                if (strncmp(msg_referto, "sip:", 4) == 0) {
                    snprintf(tempreferto, sizeof(tempreferto),
                             "<%s%c%s%c%s%%3B%s%%3D%s%%3B%s%%3D%s>",
                             msg_referto, QUESTION_MARK,
                             SIP_HEADER_REPLACES, EQUAL_SIGN, callid,
                             TO_TAG, to_tag,
                             FROM_TAG, from_tag);
                } else {
                    snprintf(tempreferto, sizeof(tempreferto),
                             "<sip:%s%c%s%c%s%%3B%s%%3D%s%%3B%s%%3D%s>",
                             msg_referto, QUESTION_MARK,
                             SIP_HEADER_REPLACES, EQUAL_SIGN, callid,
                             TO_TAG, to_tag,
                             FROM_TAG, from_tag);
                }
            }
            strlib_free(copy_of_referto);
        }

        if (dm_info) {
            cpr_free(join_info.call_id);
            cpr_free(join_info.to_tag);
            cpr_free(join_info.from_tag);
        }

        tflag = sippmh_add_text_header(request, SIP_HEADER_REFER_TO,
                                   ((NULL != xfer_ccb)|| (dm_info == TRUE)) ? tempreferto : referto);
        UPDATE_FLAGS(flag, tflag);
    } else {
        if (referto) {
            if ((strncmp(referto, "<sip:", 5) == 0) ||
                (strncmp(referto, "sip:", 4) == 0) ||
                (strncmp(referto, "<urn:", 5) == 0)) {
                strncpy(tempreferto, referto, strlen(referto));

                /* REFER to get the token should be norefersub */
                if (strncmp(referto, TOKEN_REFER_TO, sizeof(TOKEN_REFER_TO))
                        == 0) {
                    (void) sippmh_add_text_header(request, SIP_HEADER_REQUIRE,
                                                  "norefersub");
                }
            } else {
                if (semi) {
                    snprintf(tempreferto, sizeof(tempreferto), "<sip:%s>",
                             referto);
                } else {
                    snprintf(tempreferto, sizeof(tempreferto), "sip:%s",
                             referto);
                }
            }
        }
        tflag = sippmh_add_text_header(request, SIP_HEADER_REFER_TO,
                                       tempreferto);
        UPDATE_FLAGS(flag, tflag);
    }

    ccb->sip_referTo = strlib_update(ccb->sip_referTo, referto);
    tflag = sippmh_add_text_header(request, SIP_HEADER_REFERRED_BY,
                                   ccb->sip_referredBy);
    if (tflag != HSTATUS_SUCCESS) {
        return FALSE;
    }
    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (request)
            free_sip_message(request);
        clean_method_request_trx(ccb, sipMethodRefer, TRUE);
        return (FALSE);
    }
//    cpr_free(referto);

    /*
     * Add the content length now.
     */
    tflag = sippmh_add_int_header(request, SIP_HEADER_CONTENT_LENGTH, 0);
    UPDATE_FLAGS(flag, tflag);

    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (request) {
            free_sip_message(request);
        }
        return FALSE;
    }

    ccb->retx_counter = 0;

    if (SendRequest(ccb, request, sipMethodRefer, FALSE, TRUE, FALSE) == FALSE) {
        clean_method_request_trx(ccb, sipMethodRefer, TRUE);
        return (FALSE);
    } else {
        return (TRUE);
    }
}

/*
 * Sends the Notify
 * Will be sent when we get OK from target (in case of Refer)
 *
 * Note: Assumes that the connection is setup.
 *
 * Parameter:
 *    ccb - call control block
 *    response
*/
boolean
sipSPISendNotify (ccsipCCB_t *ccb, int response)
{
    const char     *fname = "sipSPISendNotify";
    sipMessage_t   *request = NULL;
    sipRet_t        flag = STATUS_SUCCESS;
    sipRet_t        tflag = STATUS_SUCCESS;
    sipMessageFlag_t messageflag;
    char           *body;
    char            errortext[MAX_SIP_URL_LENGTH];
    char            subs_state_hdr[SUBS_STATE_HDR_LEN];
    int             respClass;

    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_REQUEST),
                      fname, "Notify");

    /*
     * Clean up any remaining transactions before sending the next request.
     * Normally the transactions are cleaned after final responses (200OK)
     * and if not they will be cleaned after the timeouts. Currently for
     * NOTIFYs there are not timeout cleanups, so we will explicitly clean
     * any outstanding transactions now before the next request.
     *
     * This fix works today because we don't really care about any responses
     * to NOTIFY requests and do not act on them. The transaction layer should
     * be enhanced to handles these cases. This should be addressed under the
     * Ringpops feature.
     */
    clean_method_request_trx(ccb, sipMethodNotify, TRUE);

    /*
     * Before we do the real notify work, we may need to clean up the
     * refer proxy authorization stuff that may have been left laying around
     * during the transfer.
     *
     * This is the Proxy-Authorization that came in the REFER's Refer-To
     * as an escaped header and therefore should only be alive till the
     * transfer is done.
     *
     * But we have to do this only if we are sending the last notify for
     * a REFER, i.e. if we are sending the last sipfrag. For ex:
     * "200 OK", or "404 Not Found". We have to check for this because
     * after sending a NOTIFY("100 Trying") to the Transferor we send an
     * INVITE to the target and the Proxy-Authorization is required. When
     * we send a NOTIFY indicating success or failure, we can remove the
     * refer_proxy_auth in both ccbs.
     */
    respClass = response / 100;
    if (respClass >= 2) {
        if (ccb->refer_proxy_auth) {
            ccsipCCB_t *other_ccb;

            cpr_free(ccb->refer_proxy_auth);
            ccb->refer_proxy_auth = NULL;
            // now, find the ccb of the call we transferred to....
            other_ccb = sip_sm_get_ccb_by_callid(ccb->sipxfercallid);
            if (other_ccb != NULL) {
                if (other_ccb->refer_proxy_auth) {
                    cpr_free(other_ccb->refer_proxy_auth);
                    other_ccb->refer_proxy_auth = NULL;
                }
            }
        }
    }

    /*
     * Build the request
     */
    // The addition of the CSeq method will be done when CSeq number is added
    // ccb->last_sent_request_cseq_method = sipMethodNotify;
    messageflag.flags = 0;
    messageflag.flags = SIP_HEADER_ROUTE_BIT | SIP_HEADER_CONTACT_BIT;

    request = GET_SIP_MESSAGE();
    messageflag.extflags = 0;
    if (CreateRequest(ccb, messageflag, sipMethodNotify, request, FALSE, 0)) {
        tflag = HSTATUS_SUCCESS;
    } else {
        tflag = HSTATUS_FAILURE;
    }

    UPDATE_FLAGS(flag, tflag);

    tflag = sippmh_add_text_header(request, SIP_HEADER_EVENT, SIP_EVENT_REFER);
    UPDATE_FLAGS(flag, tflag);

    // Add Subscription-State header
    if (ccb->flags & FINAL_NOTIFY) {
        snprintf(subs_state_hdr, SUBS_STATE_HDR_LEN,
                 "terminated; reason=noresource");
    } else {
        uint32_t expires_timeout = 0;

        config_get_value(CFGID_TIMER_INVITE_EXPIRES, &expires_timeout,
                         sizeof(expires_timeout));
        snprintf(subs_state_hdr, SUBS_STATE_HDR_LEN, "active; expires=%d",
                 expires_timeout);
    }
    tflag = sippmh_add_text_header(request, SIP_HEADER_SUBSCRIPTION_STATE,
                                   subs_state_hdr);
    UPDATE_FLAGS(flag, tflag);

    /* Recalc and add Authorization header if needed */
    sipSPIGenerateGenAuthorizationResponse(ccb, request, &flag,
                                           SIP_METHOD_NOTIFY);

    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (request) {
            free_sip_message(request);
        }
        clean_method_request_trx(ccb, sipMethodNotify, TRUE);
        return (FALSE);
    }

    /* Write message */
    /*
     * Currently the only notify message the phone sends is with respect
     * to refer. This routine looks like a general send notify routine.
     * One would assume then there needs to be some sort of if (refer)
     * check to cause it to send "message/sipfrag" otherwise send
     * "application/sip"
     */
//    tflag = sippmh_add_text_header(request, SIP_HEADER_CONTENT_TYPE,
//                                   SIP_CONTENT_TYPE_SIP );
    // Don't add content-type explicitly
    // tflag = sippmh_add_text_header(request, SIP_HEADER_CONTENT_TYPE,
    //                               SIP_CONTENT_TYPE_SIPFRAG );
//  UPDATE_FLAGS(flag, tflag);

    body = (char *) cpr_malloc(MAX_SIP_URL_LENGTH * sizeof(char));
    if (!body) {
        if (request) {
            free_sip_message(request);
        }
        clean_method_request_trx(ccb, sipMethodNotify, TRUE);
        return FALSE;
    }
    memset(errortext, 0, MAX_SIP_URL_LENGTH);
    get_sip_error_string(errortext, response);
    snprintf(body, MAX_SIP_URL_LENGTH, "%s %d %s\r\n", SIP_VERSION,
             response, errortext);

    tflag = sippmh_add_message_body(request, body, strlen(body),
                                    SIP_CONTENT_TYPE_SIPFRAG,
                                    SIP_CONTENT_DISPOSITION_SESSION_VALUE,
                                    TRUE, NULL);
    UPDATE_FLAGS(flag, tflag);

    // No need to add header length separately
    // tflag = sippmh_add_int_header(request, SIP_HEADER_CONTENT_LENGTH,
    //                              strlen(message_body));
    // UPDATE_FLAGS(flag, tflag);


    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (request) {
            free_sip_message(request);
        }
        clean_method_request_trx(ccb, sipMethodNotify, TRUE);
        return (FALSE);
    }

    ccb->retx_counter = 0;

    if (SendRequest(ccb, request, sipMethodNotify, FALSE, TRUE, FALSE) == FALSE) {
        clean_method_request_trx(ccb, sipMethodNotify, TRUE);
        return (FALSE);
    } else {
        return (TRUE);
    }
}

/*
 * Sends the Info
 *
 * Note: Assumes that the connection is setup.
 *
 * Parameter:
 *    ccb - call control block
 *    info_package - the Info-Package header of the Info Package
 *    content_type - the Content-Type header of the Info Package
 *    message_body - the message body of the Info Package
*/
boolean
sipSPISendInfo (ccsipCCB_t *ccb, const char *info_package,
                const char *content_type, const char *message_body)
{
    const char     *fname = "sipSPISendInfo";
    sipMessage_t   *request = NULL;
    sipRet_t        flag = STATUS_SUCCESS;
    sipRet_t        tflag = STATUS_SUCCESS;
    sipMessageFlag_t messageflag;
    char           *body;
    boolean         retval;

    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_REQUEST),
                      fname, "Info");

    /*
     * Build the request
     */
    messageflag.flags = 0;
    messageflag.flags = SIP_HEADER_ROUTE_BIT | SIP_HEADER_CONTACT_BIT;

    request = GET_SIP_MESSAGE();
    messageflag.extflags = 0;
    if (CreateRequest(ccb, messageflag, sipMethodInfo, request, FALSE, 0)) {
        tflag = HSTATUS_SUCCESS;
    } else {
        tflag = HSTATUS_FAILURE;
    }

    UPDATE_FLAGS(flag, tflag);

    /* FIXME Media Control currently does not follow the offer/offer
             negotiation process outlined in IETF draft
             draft-ietf-sip-info-events-01, so do not add Info-Package
             header field if it's Media Control Info Package. */
    if (cpr_strncasecmp(content_type, SIP_CONTENT_TYPE_MEDIA_CONTROL,
                        strlen(SIP_CONTENT_TYPE_MEDIA_CONTROL)) != 0) {
        tflag = sippmh_add_text_header(request, SIP_HEADER_INFO_PACKAGE, info_package);
        UPDATE_FLAGS(flag, tflag);
    }

    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (request) {
            free_sip_message(request);
        }
        return FALSE;
    }

    body = (char *) cpr_malloc((strlen(message_body) + 1) * sizeof(char));
    if (!body) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_MEMORY_OUT_OF_MEM), fname);
        if (request) {
            free_sip_message(request);
        }
        return FALSE;
    }
    memcpy(body, message_body, strlen(message_body) + 1);

    tflag = sippmh_add_message_body(request, body, strlen(body),
                                    content_type,
                                    SIP_CONTENT_DISPOSITION_SESSION_VALUE,
                                    TRUE, NULL);
    flag = tflag;

    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        cpr_free(body);
        if (request) {
            free_sip_message(request);
        }
        return FALSE;
    }

    retval = SendRequest(ccb, request, sipMethodInfo, TRUE, FALSE, FALSE);

    // Don't keep the trx so as not to mess up the retran timer of other requests
    /*
     * FIXME fix it when the framework is modified to support concurrent requests
     */
    clean_method_request_trx(ccb, sipMethodInfo, TRUE);

    return retval;
}

/*
 * Sends the BYE or CANCEL response for the call.
 * Assumes that
 * - the connection is setup.
 */
boolean
sipSPISendByeOrCancelResponse (ccsipCCB_t *ccb, sipMessage_t *request,
                               sipMethod_t sipMethodByeorCancel)
{
    const char      *fname = "sipSPISendByeResponse";
    sipMessage_t    *response = NULL;
    sipRet_t         flag = STATUS_SUCCESS;
    sipMessageFlag_t messageflag;
    boolean          result;

    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_RESPONSE),
                      fname, 200);

    messageflag.flags = 0;
    messageflag.flags = SIP_HEADER_CONTENT_LENGTH_BIT;

    response = GET_SIP_MESSAGE();
    messageflag.extflags = 0;
    if (CreateResponse(ccb, messageflag, SIP_STATUS_SUCCESS, response,
                       SIP_SUCCESS_SETUP_PHRASE, 0, NULL, sipMethodByeorCancel)) {
        flag = HSTATUS_SUCCESS;
    } else {
        flag = HSTATUS_FAILURE;
    }

    /* send call stats on BYE response */
    if ((flag == STATUS_SUCCESS) && (sipMethodByeorCancel == sipMethodBye)) {
        flag = sipSPIAddCallStats(ccb, response);
    }

    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (response) {
            free_sip_message(response);
        }
        clean_method_request_trx(ccb, sipMethodByeorCancel, FALSE);
        return (FALSE);
    }
    result = sendResponse(ccb, response, request, FALSE, sipMethodByeorCancel);
    clean_method_request_trx(ccb, sipMethodByeorCancel, FALSE);
    return (result);
}



/*
 * Send 100 TRYING
 */
void
sipSPISendInviteResponse100 (ccsipCCB_t *ccb, boolean remove_to_tag)
{
    char    *temp = NULL;
    char     local_cpy[MAX_SIP_URL_LENGTH];
    string_t hold_to_tag = NULL;

    if (remove_to_tag) {
        hold_to_tag = strlib_copy(ccb->sip_to);

    /* remove the to_tag from the 100 Trying message */
    sstrncpy(local_cpy, ccb->sip_to, MAX_SIP_URL_LENGTH);
    temp = strstr(local_cpy, ">");
    if (temp != NULL) {
        *(temp + 1) = '\0';
    }
    ccb->sip_to = strlib_update(ccb->sip_to, local_cpy);
    }

    sipSPISendInviteResponse(ccb, SIP_1XX_TRYING, SIP_1XX_TRYING_PHRASE,
                             0, NULL, FALSE, /* no SDP */
                             FALSE /* no reTx */);

    if (hold_to_tag) {
        ccb->sip_to = strlib_update(ccb->sip_to, hold_to_tag);
        strlib_free(hold_to_tag);
    }
}


/*
 * Send 180 RINGING
 */
void
sipSPISendInviteResponse180 (ccsipCCB_t *ccb)
{
    sipSPISendInviteResponse(ccb, SIP_1XX_RINGING,
                             SIP_1XX_RINGING_PHRASE, 0, NULL,
                             (boolean)(ccb->flags & INBAND_ALERTING),
                             FALSE /* no reTx */);
}


/*
 * Send 200 OK
 */
void
sipSPISendInviteResponse200 (ccsipCCB_t *ccb)
{
    sipSPISendInviteResponse(ccb, SIP_STATUS_SUCCESS,
                             SIP_SUCCESS_SETUP_PHRASE, 0, NULL,
                             TRUE, TRUE /* reTx */);
}

void
sipSPISendInviteResponse302 (ccsipCCB_t *ccb)
{

    sipSPISendInviteResponse(ccb, SIP_RED_MOVED_TEMP,
                             SIP_RED_MOVED_TEMP_PHRASE,
                             0, NULL, FALSE, /* no SDP */
                             TRUE /* reTx */);
}

/*Send Option Response
 * - connection is set up.
 * Currently just gives the methods supported
 * Does not affect call state.
*/
boolean
sipSPISendOptionResponse (ccsipCCB_t *ccb, sipMessage_t *request)
{
    const char      *fname = "SIPSPISendOptionResponse";
    sipMessage_t    *response = NULL;
    sipRet_t         flag = STATUS_SUCCESS;
    sipMessageFlag_t messageflag;
    boolean          result;

    messageflag.flags = 0;
    messageflag.flags = SIP_HEADER_CONTACT_BIT |
                        SIP_HEADER_RECORD_ROUTE_BIT |
                        SIP_HEADER_ALLOW_BIT |
                        SIP_HEADER_ACCEPT_BIT |
                        SIP_HEADER_ACCEPT_ENCODING_BIT |
                        SIP_HEADER_ACCEPT_LANGUAGE_BIT |
                        SIP_HEADER_SUPPORTED_BIT;

    /* Write SDP */
    messageflag.flags |= SIP_HEADER_OPTIONS_CONTENT_TYPE_BIT;

    response = GET_SIP_MESSAGE();
    messageflag.extflags = 0;
    if (CreateResponse(ccb, messageflag, SIP_STATUS_SUCCESS, response,
                       SIP_SUCCESS_SETUP_PHRASE, 0, NULL, sipMethodOptions)) {
        flag = HSTATUS_SUCCESS;
    } else {
        flag = HSTATUS_FAILURE;
    }

    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (response) {
            free_sip_message(response);
        }
        clean_method_request_trx(ccb, sipMethodOptions, FALSE);
        return (FALSE);
    }
    result = sendResponse(ccb, response, request, FALSE, sipMethodOptions);
    clean_method_request_trx(ccb, sipMethodOptions, FALSE);
    return (result);
}

/*
 * The function below handles an OPTIONS message not associated with
 * any ongoing dialog and serves for the phone to gather capabilities
 * of its server
 */
boolean
sipSPIsendNonActiveOptionResponse (sipMessage_t *msg,
                                   cc_msgbody_info_t *local_msg_body)
{
    const char    *fname = "sipSPIsendNonActiveOptionResponse";
    sipMessage_t  *response       = NULL;
    sipRet_t       flag           = STATUS_SUCCESS;
    sipRet_t       tflag          = STATUS_SUCCESS;
    const char    *sip_from       = NULL;
    const char    *sip_to         = NULL;
    const char    *request_callid = NULL;
    const char    *request_cseq   = NULL;
    sipCseq_t     *request_cseq_structure = NULL;
    char           temp[MAX_SIP_HEADER_LENGTH];
    sipLocation_t *to_loc         = NULL;
    char           sip_to_tag[MAX_SIP_TAG_LENGTH];
    char           sip_to_temp[MAX_SIP_URL_LENGTH];
    sipLocation_t *from_loc       = NULL;
    boolean        request_uri_error = FALSE;
    sipReqLine_t  *requestURI     = NULL;
    sipLocation_t *uri_loc        = NULL;
    const char    *accept_hdr     = NULL;
    const char    *supported      = NULL;
    int            kpml_config;

    if (!msg) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT),
                          fname, "msg");
        return (FALSE);
    }

    // Parse through the Accept header to get the supported capabilities
    accept_hdr = sippmh_get_header_val(msg, SIP_HEADER_ACCEPT, NULL);
    if (accept_hdr) {
        server_caps = sippmh_parse_accept_header(accept_hdr);
    }

    // Parse through the Supported header to get the supported capabilities
    supported = sippmh_get_cached_header_val(msg, SUPPORTED);
    if (supported) {
        sippmh_parse_supported_require(supported, NULL);
    }

    response = GET_SIP_MESSAGE();
    if (!response) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "GET_SIP_MESSAGE()");
        return (FALSE);
    }

    sip_from = sippmh_get_cached_header_val(msg, FROM);
    sip_to = sippmh_get_cached_header_val(msg, TO);
    sstrncpy(sip_to_temp, sip_to, MAX_SIP_URL_LENGTH);
    request_callid = sippmh_get_cached_header_val(msg, CALLID);

    requestURI = sippmh_get_request_line(msg);
    if (requestURI) {
        if (requestURI->url) {
            uri_loc = sippmh_parse_from_or_to(requestURI->url, TRUE);
            if (uri_loc) {
                if (uri_loc->genUrl->schema != URL_TYPE_SIP) {
                    request_uri_error = TRUE;
                }
                sippmh_free_location(uri_loc);
            } else {
                request_uri_error = TRUE;
            }
        } else {
            request_uri_error = TRUE;
        }
        SIPPMH_FREE_REQUEST_LINE(requestURI);
    } else {
        request_uri_error = TRUE;
    }
    if (request_uri_error) {
        CCSIP_DEBUG_ERROR("%s: Error: Invalid Request URI failed.\n", fname);
        free_sip_message(response);
        /* Send 400 error */
        if (sipSPISendErrorResponse(msg, SIP_CLI_ERR_BAD_REQ,
                                    SIP_CLI_ERR_BAD_REQ_PHRASE,
                                    SIP_WARN_MISC,
                                    SIP_CLI_ERR_BAD_REQ_REQLINE_ERROR,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        return (FALSE);
    }


    /*
     * Parse From
     */
    from_loc = sippmh_parse_from_or_to((char *)sip_from, TRUE);
    if (!from_loc) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname,
                          get_debug_string(DEBUG_FUNCTIONNAME_SIPPMH_PARSE_FROM));
        free_sip_message(response);
        /* Send 400 error */
        if (sipSPISendErrorResponse(msg, SIP_CLI_ERR_BAD_REQ,
                                    SIP_CLI_ERR_BAD_REQ_PHRASE,
                                    SIP_WARN_MISC,
                                    SIP_CLI_ERR_BAD_REQ_FROMURL_ERROR,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        return (FALSE);
    }

    /*
     * From is parsed just to make sure we got valid From
     * Header, so we free it now
     */
    sippmh_free_location(from_loc);

    /*
     * Parse To
     */
    to_loc = sippmh_parse_from_or_to((char *)sip_to, TRUE);
    if (!to_loc) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname,
                          get_debug_string(DEBUG_FUNCTIONNAME_SIPPMH_PARSE_TO));
        if (response) {
            free_sip_message(response);
        }
        /* Send 400 error */
        if (sipSPISendErrorResponse(msg, SIP_CLI_ERR_BAD_REQ,
                                    SIP_CLI_ERR_BAD_REQ_PHRASE,
                                    SIP_WARN_MISC,
                                    SIP_CLI_ERR_BAD_REQ_ToURL_ERROR,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        return (FALSE);
    }

    /* Check/Generate tags */
    if (to_loc->tag) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED), NULL,
                          NULL, fname, "Initial Option with to_tag");
        if (response) {
            free_sip_message(response);
        }
        if (sipSPISendErrorResponse(msg, SIP_CLI_ERR_BAD_REQ,
                                    SIP_CLI_ERR_BAD_REQ_PHRASE,
                                    SIP_WARN_MISC,
                                    SIP_CLI_ERR_BAD_REQ_ToURL_ERROR,
                                    NULL) != TRUE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                              fname, SIP_CLI_ERR_BAD_REQ);
        }
        sippmh_free_location(to_loc);
        return (FALSE);
    } else {
        sip_util_make_tag(sip_to_tag);
        strncat(sip_to_temp, ";tag=",
                MAX_SIP_URL_LENGTH - strlen(sip_to_temp) - 1);
        strncat(sip_to_temp, sip_to_tag,
                MAX_SIP_URL_LENGTH - strlen(sip_to_temp) - 1);
    }
    sippmh_free_location(to_loc);

    tflag = sippmh_add_response_line(response, SIP_VERSION, SIP_STATUS_SUCCESS,
                                     SIP_SUCCESS_SETUP_PHRASE);
    UPDATE_FLAGS(flag, tflag);
    tflag = (sipSPIAddRequestVia(NULL, response, msg, sipMethodOptions)) ?
        STATUS_SUCCESS : STATUS_FAILURE;
    UPDATE_FLAGS(flag, tflag);
    tflag = sippmh_add_text_header(response, SIP_HEADER_FROM, sip_from);
    UPDATE_FLAGS(flag, tflag);
    tflag = sippmh_add_text_header(response, SIP_HEADER_TO, sip_to_temp);
    UPDATE_FLAGS(flag, tflag);
    tflag = sippmh_add_text_header(response, SIP_HEADER_CALLID, request_callid);
    UPDATE_FLAGS(flag, tflag);

    // Add date header
    tflag = sipAddDateHeader(response);
    UPDATE_FLAGS(flag, tflag);
    /* Write CSeq */
    request_cseq = sippmh_get_cached_header_val(msg, CSEQ);
    if (request_cseq) {
        request_cseq_structure = sippmh_parse_cseq(request_cseq);
        if (!request_cseq_structure) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sippmh_parse_cseq()");
            free_sip_message(response);
            /*
             * We should send 400 here but as we don't have Cseq this
             * is not possible.
             */
            return FALSE;
        }
        if (request_cseq_structure->method != sipMethodOptions) {
            CCSIP_DEBUG_ERROR("%s: Error: Invalid method in Cseq failed.\n",
                              fname);
            free_sip_message(response);
            /* Send 400 error */
            if (sipSPISendErrorResponse(msg, SIP_CLI_ERR_BAD_REQ,
                                        SIP_CLI_ERR_BAD_REQ_PHRASE,
                                        SIP_WARN_MISC,
                                        SIP_CLI_ERR_BAD_REQ_VIA_OR_CSEQ,
                                        NULL) != TRUE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_SPI_SEND_ERROR),
                                  fname, SIP_CLI_ERR_BAD_REQ);
            }
            cpr_free(request_cseq_structure);
            return FALSE;
        }
        tflag = sippmh_add_text_header(response, SIP_HEADER_CSEQ, request_cseq);
        cpr_free(request_cseq_structure);
        UPDATE_FLAGS(flag, tflag);
    }

    tflag = sippmh_add_text_header(response, SIP_HEADER_SERVER,
                                   sipHeaderServer);

    UPDATE_FLAGS(flag, tflag);

    /*
     * Add the local sdp we got from gsm into the response
     */
    tflag = CopyLocalSDPintoResponse(response, local_msg_body);
    UPDATE_FLAGS(flag, tflag);

    // Add Allow
    snprintf(temp, MAX_SIP_HEADER_LENGTH, "%s,%s,%s,%s,%s,%s,%s,%s,%s",
             SIP_METHOD_ACK, SIP_METHOD_BYE, SIP_METHOD_CANCEL,
             SIP_METHOD_INVITE, SIP_METHOD_NOTIFY, SIP_METHOD_OPTIONS,
             SIP_METHOD_REFER, SIP_METHOD_REGISTER, SIP_METHOD_UPDATE);
    tflag = sippmh_add_text_header(response, SIP_HEADER_ALLOW, temp);
    UPDATE_FLAGS(flag, tflag);

    // Add Allow-Events
    config_get_value(CFGID_KPML_ENABLED, &kpml_config, sizeof(kpml_config));
    if (kpml_config) {
        snprintf(temp, MAX_SIP_HEADER_LENGTH, "%s,%s,%s", SIP_EVENT_KPML,
                 SIP_EVENT_DIALOG, SIP_EVENT_REFER);
    } else {
        snprintf(temp, MAX_SIP_HEADER_LENGTH, "%s,%s", SIP_EVENT_DIALOG,
                 SIP_EVENT_REFER);
    }
    tflag = sippmh_add_text_header(response, SIP_HEADER_ALLOW_EVENTS, temp);
    UPDATE_FLAGS(flag, tflag);

    // Add Accept
    snprintf(temp, MAX_SIP_HEADER_LENGTH, "%s,%s,%s",
             SIP_CONTENT_TYPE_SDP,
             SIP_CONTENT_TYPE_MULTIPART_MIXED,
             SIP_CONTENT_TYPE_MULTIPART_ALTERNATIVE);
    tflag = sippmh_add_text_header(response, SIP_HEADER_ACCEPT, temp);
    UPDATE_FLAGS(flag, tflag);

    // Add Accept-Encoding
    tflag = sippmh_add_text_header(response, SIP_HEADER_ACCEPT_ENCODING,
                                   "identity");
    UPDATE_FLAGS(flag, tflag);

    // Add Accept-Language
    tflag = sippmh_add_text_header(response, SIP_HEADER_ACCEPT_LANGUAGE, "en");
    UPDATE_FLAGS(flag, tflag);

    tflag = sippmh_add_text_header(response, SIP_HEADER_SUPPORTED,
                                   SIP_RFC_SUPPORTED_TAGS);

    UPDATE_FLAGS(flag, tflag);

    /* Determine from the Via field where the message is supposed
     * to be sent
     */

    if (tflag != HSTATUS_SUCCESS) {
        free_sip_message(response);
        return FALSE;
    }
    return sendResponse(NULL, response, msg, FALSE, sipMethodOptions);
}


/*
 * Send the invite response. Adds the session description
 * if send_sd is TRUE. Assumes
 * - connection is set up.
 * Does not affect call state.
 */
void
sipSPISendInviteResponse (ccsipCCB_t *ccb,
                          uint16_t statusCode,
                          const char *reason_phrase,
                          uint16_t status_code_warning,
                          const char *reason_phrase_warning,
                          boolean send_sd,
                          boolean retx)
{
    const char      *fname = "SIPSPISendInviteResponse";
    sipMessage_t    *response = NULL;
    sipRet_t         flag = STATUS_SUCCESS;
    sipRet_t         tflag = STATUS_SUCCESS;
    sipMessageFlag_t messageflag;
    int              rpid_flag;


    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_RESPONSE),
                      fname, statusCode);

    messageflag.flags = 0;
    messageflag.flags = SIP_HEADER_CONTACT_BIT |
                        SIP_HEADER_RECORD_ROUTE_BIT |
                        SIP_HEADER_ALLOW_BIT |
                        SIP_HEADER_DIVERSION_BIT |
                        SIP_HEADER_ALLOW_EVENTS_BIT;

    /* Write Content-Type */
    /* Add SDP body if required */
    if (send_sd) {
        messageflag.flags |= SIP_HEADER_CONTENT_TYPE_BIT;
    } else {
        messageflag.flags |= SIP_HEADER_CONTENT_LENGTH_BIT;
    }
    if (statusCode == SIP_CLI_ERR_EXTENSION) {
        messageflag.flags |= SIP_HEADER_UNSUPPORTED_BIT;
    }
    if ((statusCode >=SIP_1XX_TRYING) && (statusCode <= SIP_STATUS_SUCCESS)) {
        messageflag.flags |= SIP_HEADER_SUPPORTED_BIT;
    }
    if (statusCode == SIP_SERV_ERR_INTERNAL) {
        messageflag.flags |= SIP_HEADER_RETRY_AFTER_BIT;
    }
    if ((statusCode == SIP_1XX_TRYING) || (statusCode == SIP_STATUS_SUCCESS)) {
        messageflag.flags |= SIP_HEADER_RECV_INFO_BIT;
    }

    /*
     * Set RPID header.
     */
    if (statusCode != SIP_1XX_TRYING) {
        /*
         * The RPID header is not sent in the 100 Trying because the response is directly
         * sent from the SIP stack.The information needed for building the RPID is updated 
         * by the gsm later. 
         */
        rpid_flag = sipSPISetRPID(ccb, FALSE);
        if (rpid_flag == RPID_ENABLED) {
            messageflag.flags |= SIP_HEADER_REMOTE_PARTY_ID_BIT;
        }
    }

    response = GET_SIP_MESSAGE();
    messageflag.extflags = 0;
    if (CreateResponse(ccb, messageflag, statusCode, response, reason_phrase,
                       status_code_warning, reason_phrase_warning,
                       sipMethodInvite)) {
        tflag = HSTATUS_SUCCESS;
    } else {
        tflag = HSTATUS_FAILURE;
    }

    UPDATE_FLAGS(flag, tflag);

    tflag = sipSPIAddCallStats(ccb, response);
  
    UPDATE_FLAGS(flag, tflag);
    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (response) {
            free_sip_message(response);
        }
        return;
    }

    (void) sendResponse(ccb, response, ccb->last_request, retx, sipMethodInvite);
    return;
}


void
sipSPIGenerateGenAuthorizationResponse (ccsipCCB_t *ccb,
                                        sipMessage_t *request,
                                        sipRet_t *flag,
                                        char *method)
{
    const char   *fname = "sipSPIGenerateGenAuthorizationResponse";
    sipRet_t      tflag = STATUS_SUCCESS;
    char         *author_str = NULL;
    credentials_t credentials;

    /*
     * CSCds70538
     * Re-generate the Authorization header for CANCEL, BYE, mid-INVITE
     * The header is cached after an INVITE is challenged.
     * I know that I have been challenged preciously because sip_authen
     * is not NULL
     */
    if (ccb->authen.sip_authen) {
        cred_get_line_credentials(ccb->dn_line, &credentials,
                                  sizeof(credentials.id),
                                  sizeof(credentials.pw));
        if (sipSPIGenerateAuthorizationResponse(ccb->authen.sip_authen,
                    ccb->ReqURI, method, credentials.id, credentials.pw,
                    &author_str, &(ccb->authen.nc_count), ccb)) {

            if (ccb->authen.authorization != NULL) {
                cpr_free(ccb->authen.authorization);
                ccb->authen.authorization = NULL;
            }

            /*
             * Don't free the sip_authen since we were not challenged
             */
            ccb->authen.authorization = (char *)
                cpr_malloc(strlen(author_str) + 1);

            /*
             * Cache the Authorization header so that it can be used for later
             * requests
             */
            if (ccb->authen.authorization != NULL) {
                sstrncpy(ccb->authen.authorization, author_str,
                         strlen(author_str) * sizeof(char) + 1);
            }

            cpr_free(author_str);
        } else {
            CCSIP_DEBUG_ERROR("%s: Error: Authorization header build "
                              "unsuccessful\n", fname);
        }
    }

    if (ccb->authen.authorization != NULL) {
        tflag = sippmh_add_text_header(request,
                                       AUTHOR_HDR(ccb->authen.status_code),
                                       ccb->authen.authorization);
        UPDATE_FLAGS(*flag, tflag);
    }
}


/*
 * Sends the 202 Refer Accepted .
 * Assumes that
 * - the connection is setup.
 * Will be sent when we get Refer
*/
boolean
sipSPISendReferResponse202 (ccsipCCB_t *ccb)
{
    const char      *fname = "SIPSPISendReferResponse";
    sipMessage_t    *response = NULL;
    sipRet_t         flag = STATUS_SUCCESS;
    sipMessageFlag_t messageflag;
    boolean          result;

    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_RESPONSE),
                      fname, SIP_ACCEPTED);

    messageflag.flags = 0;
    messageflag.flags = SIP_HEADER_CONTACT_BIT |
                        SIP_HEADER_RECORD_ROUTE_BIT |
                        SIP_HEADER_CONTENT_LENGTH_BIT;

    /* Add Content Length */
    response = GET_SIP_MESSAGE();
    messageflag.extflags = 0;
    if (CreateResponse(ccb, messageflag, SIP_ACCEPTED, response,
                       SIP_ACCEPTED_PHRASE, 0, NULL, sipMethodRefer)) {
        flag = HSTATUS_SUCCESS;
    } else {
        flag = HSTATUS_FAILURE;
    }

    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (response)
            free_sip_message(response);
        clean_method_request_trx(ccb, sipMethodRefer, FALSE);
        return (FALSE);
    }

    result = sendResponse(ccb, response, ccb->last_request, FALSE,
                          sipMethodRefer);
    clean_method_request_trx(ccb, sipMethodRefer, FALSE);
    return (result);
}

/*
 * General function to send an error response for a given request.
 * Does not affect call state or disconnection flags.
 *
 * Valid values of warn_code is 3xx. Set warn_code to ZERO, when
 * warn_code is absent. Use of a valid warn_code, will lead to
 * generation of Warning Header in the response.
 */
boolean
sipSPISendErrorResponse (sipMessage_t *msg,
                         uint16_t status_code,
                         const char *reason_phrase,
                         uint16_t status_code_warning,
                         const char *reason_phrase_warning,
                         ccsipCCB_t *ccb)
{
    const char   *fname = "sipSPISendErrorResponse";
    sipMessage_t *response = NULL;
    sipRet_t      flag     = STATUS_SUCCESS;
    sipRet_t      tflag    = STATUS_SUCCESS;
    const char   *sip_from = NULL;
    const char   *sip_to   = NULL;
    const char   *request_callid = NULL;
    const char   *request_cseq   = NULL;
    sipCseq_t    *request_cseq_structure = NULL;
    sipMethod_t   method   = sipMethodInvalid;
    boolean       result   = FALSE;
    char          temp[MAX_SIP_HEADER_LENGTH];

    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_RESPONSE),
                      fname, status_code);

    if (!msg) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT),
                          fname, "msg");
        return (FALSE);
    }

    response = GET_SIP_MESSAGE();
    if (!response) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "GET_SIP_MESSAGE()");
        return (FALSE);
    }

    tflag = sippmh_add_response_line(response, SIP_VERSION, status_code,
                                      reason_phrase) ? STATUS_FAILURE : STATUS_SUCCESS;

    UPDATE_FLAGS(flag, tflag);

    tflag = (sipSPIAddRequestVia(NULL, response, msg, sipMethodInvalid)) ?
        STATUS_SUCCESS : STATUS_FAILURE;
    UPDATE_FLAGS(flag, tflag);

    sip_from = sippmh_get_cached_header_val(msg, FROM);
    sip_to = sippmh_get_cached_header_val(msg, TO);
    request_callid = sippmh_get_cached_header_val(msg, CALLID);
    tflag = sippmh_add_text_header(response, SIP_HEADER_FROM, sip_from);
    UPDATE_FLAGS(flag, tflag);
    tflag = sippmh_add_text_header(response, SIP_HEADER_TO, sip_to);
    UPDATE_FLAGS(flag, tflag);
    tflag = sippmh_add_text_header(response, SIP_HEADER_CALLID, request_callid);
    UPDATE_FLAGS(flag, tflag);

    // Add date header
    tflag = sipAddDateHeader(response);
    UPDATE_FLAGS(flag, tflag);

    if (reason_phrase_warning) {
        char *warning = NULL;

        warning = (char *) cpr_malloc(strlen(reason_phrase_warning) + 5);
        if (warning) {
            snprintf(warning, strlen(reason_phrase_warning) + 5,
                     "%d %s", status_code_warning, reason_phrase_warning);
            tflag = sippmh_add_text_header(response, SIP_HEADER_WARN, warning);
            UPDATE_FLAGS(flag, tflag);
            cpr_free(warning);
        }
    }

    // Add Retry-After, if needed
    if (status_code == SIP_SERV_ERR_INTERNAL &&
        status_code_warning == SIP_WARN_PROCESSING_PREVIOUS_REQUEST) {
        tflag = sippmh_add_int_header(response, SIP_HEADER_RETRY_AFTER,
                                      abs((cpr_rand() % 11)));
    }

    //Add Accept-Encoding, Accept and Accept-Language headers
    //if outgoing response is 415-Unsupported Media Type
    if (status_code == SIP_CLI_ERR_MEDIA) {
        snprintf(temp, MAX_SIP_HEADER_LENGTH, "%s,%s,%s,%s,%s",
                 SIP_CONTENT_TYPE_SDP,
                 SIP_CONTENT_TYPE_MULTIPART_MIXED,
                 SIP_CONTENT_TYPE_MULTIPART_ALTERNATIVE,
                 SIP_CONTENT_TYPE_SIPFRAG,
                 SIP_CONTENT_TYPE_MWI);
        // should we add all registered Info Package Content-Type here??
        tflag = sippmh_add_text_header(response, SIP_HEADER_ACCEPT, temp);
        UPDATE_FLAGS(flag, tflag);

        tflag = sippmh_add_text_header(response, SIP_HEADER_ACCEPT_ENCODING,
                                       SIP_CONTENT_ENCODING_IDENTITY);
        UPDATE_FLAGS(flag, tflag);

        tflag = sippmh_add_text_header(response, SIP_HEADER_ACCEPT_LANGUAGE,
                                       "en");
        UPDATE_FLAGS(flag, tflag);
    }
    
    if (status_code == SIP_CLI_ERR_NOT_ALLOWED) {
        // Add Allow
        snprintf(temp, MAX_SIP_HEADER_LENGTH, "%s,%s,%s,%s,%s,%s,%s,%s,%s",
                 SIP_METHOD_ACK, SIP_METHOD_BYE, SIP_METHOD_CANCEL,
                 SIP_METHOD_INVITE, SIP_METHOD_NOTIFY, SIP_METHOD_OPTIONS,
                 SIP_METHOD_REFER, SIP_METHOD_UPDATE, SIP_METHOD_SUBSCRIBE);
        tflag = sippmh_add_text_header(response, SIP_HEADER_ALLOW, temp);
        UPDATE_FLAGS(flag, tflag);        
    }

    /* Write CSeq */
    request_cseq = sippmh_get_cached_header_val(msg, CSEQ);
    if (request_cseq) {
        request_cseq_structure = sippmh_parse_cseq(request_cseq);
        if (!request_cseq_structure) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sippmh_parse_cseq()");
            free_sip_message(response);
            return (FALSE);
        }
        tflag = sippmh_add_text_header(response, SIP_HEADER_CSEQ, request_cseq);
        method = request_cseq_structure->method;
        cpr_free(request_cseq_structure);
        UPDATE_FLAGS(flag, tflag);
    } else {
        CCSIP_DEBUG_ERROR("%s: Error: Did not find valid CSeq header. "
                          "Cannot send response.\n", fname);
        if (response) {
            free_sip_message(response);
            response = NULL;
        }
    }

    /* Write Content-Length: 0 */
    tflag = sippmh_add_int_header(response, SIP_HEADER_CONTENT_LENGTH, 0);
    UPDATE_FLAGS(flag, tflag);

    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (response) {
            free_sip_message(response);
            response = NULL;
        }
    }

    /* Determine from the Via field where the message is supposed
     * to be sent
     */
    if (response) {
        result = sendResponse(NULL, response, msg, FALSE, method);
    }
    if (ccb) {
        clean_method_request_trx(ccb, method, FALSE);
    }
    return (result);
}

void
sipSPISendFailureResponseAck (ccsipCCB_t *ccb, sipMessage_t *response,
                              boolean prevcall, line_t previous_call_line)
{
    const char *fname = "sipSPISendFailureResponseAck";

    sipMessage_t   *request = NULL;
    sipRet_t        flag = STATUS_SUCCESS;
    sipRet_t        tflag = STATUS_SUCCESS;

    char            src_addr_str[MAX_IPADDR_STR_LEN];
    static char     via[SIP_MAX_VIA_LENGTH];
    char            via_branch[SIP_MAX_VIA_LENGTH];
    const char     *response_to = NULL;
    const char     *response_from = NULL;
    const char     *response_callid = NULL;
    const char     *response_cseq = NULL;

    cpr_ip_addr_t   dest_ipaddr;
    cpr_ip_addr_t   src_ipaddr;
    uint32_t        dest_port = 0;
    uint32_t        cseq_number = 0;
    sipCseq_t      *response_cseq_structure;
    sipMethod_t     response_cseq_method = sipMethodInvalid;
    sipRespLine_t  *respLine = NULL;
    int             status_code = 0;
    char            local_ReqURI[MAX_SIP_URL_LENGTH];
    line_t          dn_line;
    const char     *authenticate = NULL;
    credentials_t   credentials;
    sip_authen_t   *sip_authen = NULL;
    sipAuthenticate_t authen;
    char           *author_str = NULL;
    int             nc_count = 0;
    boolean         bad_authentication = FALSE;
    int16_t         trx_index = -1;
    line_t          line;
    int             nat_enable = 0;
    int             reldel_stored_msg = RELDEV_NO_STORED_MSG;

    CPR_IP_ADDR_INIT(dest_ipaddr);
    CPR_IP_ADDR_INIT(src_ipaddr);

    if (prevcall) {
        dest_ipaddr = gCallHistory[previous_call_line].last_bye_dest_ipaddr;
        dest_port   = gCallHistory[previous_call_line].last_bye_dest_port;
        dn_line     = gCallHistory[previous_call_line].dn_line;
        sstrncpy(local_ReqURI,
                 gCallHistory[previous_call_line].last_route_request_uri,
                 MAX_SIP_URL_LENGTH);
        strncpy(via_branch, gCallHistory[previous_call_line].via_branch,
                SIP_MAX_VIA_LENGTH - 1);
    } else {
        /*
         * The ACK to a 300-699 response MUST
         * be sent to same address, port, and
         * transport to which the original request was sent.
         */
        dn_line = ccb->dn_line;
        // Get the via_branch from the last request that we sent
        trx_index = get_method_request_trx_index(ccb, sipMethodInvite, TRUE);
        if (trx_index != -1) {
            strncpy(via_branch,
                    (const char *)(ccb->sent_request[trx_index].u.sip_via_branch),
                    SIP_MAX_VIA_LENGTH - 1);
        }
        // via_branch = (char *) ccb->sip_via_branch;
        /*
         * Use outbound proxy if we used to send messages for this call,
         * we are using the default proxy, we are not using the Emergency
         * route, and it is not the backup proxy registration.
         */
        if (util_check_if_ip_valid(&(ccb->outBoundProxyAddr)) &&
            (ccb->proxySelection == SIP_PROXY_DEFAULT) &&
            (ccb->routeMode != RouteEmergency) &&
            (ccb->index != REG_BACKUP_CCB)) {
            dest_ipaddr = ccb->outBoundProxyAddr;
            if (ccb->outBoundProxyPort != 0) {
                dest_port = ccb->outBoundProxyPort;
            } else {
                config_get_value(CFGID_OUTBOUND_PROXY_PORT, &dest_port,
                                 sizeof(dest_port));
            }
        } else {
            dest_ipaddr = ccb->dest_sip_addr;
            dest_port = ccb->dest_sip_port;
        }
    }

    response_cseq = sippmh_get_cached_header_val(response, CSEQ);
    if (!response_cseq) {
        CCSIP_DEBUG_ERROR("%s: Error: Unable to obtain request's CSeq "
                          "header.\n", fname);
        return;
    }
    response_cseq_structure = sippmh_parse_cseq(response_cseq);
    if (!response_cseq_structure) {
        CCSIP_DEBUG_ERROR("%s: Error: Unable to parse request's CSeq "
                          "header.\n", fname);
        return;
    }
    cseq_number = response_cseq_structure->number;
    response_cseq_method = response_cseq_structure->method;
    cpr_free(response_cseq_structure);

    // Process the Failure response
    response_to = sippmh_get_cached_header_val(response, TO);
    response_from = sippmh_get_cached_header_val(response, FROM);
    response_callid = sippmh_get_cached_header_val(response, CALLID);

    request = GET_SIP_MESSAGE();
    if (!request) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "GET_SIP_MESSAGE()");
        return;
    }

    /*
     * Determine the Request-URI
     */
    respLine = sippmh_get_response_line(response);
    if (respLine) {
        status_code = respLine->status_code;
        SIPPMH_FREE_RESPONSE_LINE(respLine);
    }

    if (prevcall == FALSE) {
        char tmp_ReqURI[MAX_SIP_URL_LENGTH];

        // Save original ReqURI
        sstrncpy(tmp_ReqURI, ccb->ReqURI, MAX_SIP_URL_LENGTH);
        // Update ReqURI using RR, etc
        (void) sipSPIGenRequestURI(ccb, sipMethodAck, FALSE);
        // Use the ReqURI
        sstrncpy(local_ReqURI, ccb->ReqURI, MAX_SIP_URL_LENGTH);
        // Restore original ReqURI
        sstrncpy(ccb->ReqURI, tmp_ReqURI, MAX_SIP_URL_LENGTH);

        // Clean up the INVITE transaction block as we no longer need it
        clean_method_request_trx(ccb, sipMethodInvite, TRUE);
    }

    config_get_value(CFGID_NAT_ENABLE, &nat_enable, sizeof(nat_enable));
    if (nat_enable == 0) {
        sip_config_get_net_device_ipaddr(&src_ipaddr);
        ipaddr2dotted(src_addr_str, &src_ipaddr);
    } else {
        sip_config_get_nat_ipaddr(&src_ipaddr);
        ipaddr2dotted(src_addr_str, &src_ipaddr);
    }

    /*
     * Build the request
     */
    tflag = sippmh_add_request_line(request, SIP_METHOD_ACK, local_ReqURI,
                                    SIP_VERSION);
    UPDATE_FLAGS(flag, tflag);

    /* Write Via */
    if (prevcall) {
        /*
         * Use line 1 as default to get the transport type.
         * Transport type is going to be device specific with
         * the first release of skittles. Will need to change
         * for mixed cc mode support.
         */
        line = 1;
    } else {
        line = ccb->dn_line;
    }
    snprintf(via, sizeof(via), "SIP/2.0/%s %s:%d;%s=%s",
             sipTransportGetTransportType(line, TRUE, ccb),
             src_addr_str, sipTransportGetListenPort(line, ccb),
             VIA_BRANCH, via_branch);
    tflag = sippmh_add_text_header(request, SIP_HEADER_VIA, via);
    UPDATE_FLAGS(flag, tflag);

    /* Write To, From, and Call-ID standard headers */
    tflag = sippmh_add_text_header(request, SIP_HEADER_FROM, response_from);
    UPDATE_FLAGS(flag, tflag);
    tflag = sippmh_add_text_header(request, SIP_HEADER_TO, response_to);
    UPDATE_FLAGS(flag, tflag);
    tflag = sippmh_add_text_header(request, SIP_HEADER_CALLID, response_callid);
    UPDATE_FLAGS(flag, tflag);

    switch (status_code) {
    case SIP_CLI_ERR_UNAUTH:
    case SIP_CLI_ERR_PROXY_REQD:
        if (response_cseq_method == sipMethodAck) {
            authen.authorization = NULL;
            cred_get_line_credentials(dn_line, &credentials,
                                      sizeof(credentials.id),
                                      sizeof(credentials.pw));
            authenticate = sippmh_get_header_val(response,
                                                 AUTH_HDR(status_code), NULL);
            if (authenticate != NULL) {
                sip_authen = sippmh_parse_authenticate(authenticate);
                if (sip_authen) {
                    if (sipSPIGenerateAuthorizationResponse(sip_authen,
                                                            local_ReqURI,
                                                            SIP_METHOD_ACK,
                                                            credentials.id,
                                                            credentials.pw,
                                                            &author_str,
                                                            &nc_count, NULL)) {
                        authen.authorization = (char *)
                            cpr_malloc(strlen(author_str) * sizeof(char) + 1);
                        if (authen.authorization != NULL) {
                            sstrncpy(authen.authorization, author_str,
                                     strlen(author_str) * sizeof(char) + 1);
                            authen.status_code = status_code;
                        } else {
                            CCSIP_DEBUG_ERROR("%s: Error: malloc() failed "
                                              "for authen.authorization\n",
                                              fname);
                            bad_authentication = TRUE;
                        }
                        cpr_free(author_str);
                    } else {
                        CCSIP_DEBUG_ERROR("%s: Error: "
                                          "sipSPIGenerateAuthorizationResponse()"
                                          " returned null.\n", fname);
                        bad_authentication = TRUE;
                    }
                    sippmh_free_authen(sip_authen);
                } else {
                    CCSIP_DEBUG_ERROR("%s: Error: sippmh_parse_authenticate() "
                                      "returned null.\n", fname);
                    bad_authentication = TRUE;
                }
            } else {
                CCSIP_DEBUG_ERROR("%s: Error: sippmh_get_header_val(AUTH_HDR) "
                                  "returned null.\n", fname);
                bad_authentication = TRUE;
            }

            if (!bad_authentication) {
                tflag = sippmh_add_text_header(request,
                                               AUTHOR_HDR(status_code),
                                               authen.authorization);
                cpr_free(authen.authorization);
                UPDATE_FLAGS(flag, tflag);
            } else {
                CCSIP_DEBUG_ERROR("%s: Error: Bad authentication header.\n",
                                  fname);
                if (request) {
                    free_sip_message(request);
                }
                if (authen.authorization) {
                    cpr_free(authen.authorization);
                }
                return;
            }

        }
        break;

    default:
        break;
    }

    /* Write Date header */
    tflag = sipAddDateHeader(request);
    UPDATE_FLAGS(flag, tflag);

    /* Write CSeq */
    tflag = sippmh_add_cseq(request, SIP_METHOD_ACK, cseq_number);
    UPDATE_FLAGS(flag, tflag);

    /* Add Route Header */
    tflag = (sipSPIAddRouteHeaders(request, ccb, NULL, 0)) ?
        STATUS_SUCCESS : STATUS_FAILURE;

    /* Write Content-Length */
    tflag = sippmh_add_int_header(request, SIP_HEADER_CONTENT_LENGTH, 0);
    UPDATE_FLAGS(flag, tflag);

    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (request) {
            free_sip_message(request);
        }
        return;
    }
    /* Store the ACK so that it can be retransmitted in response
     * to any duplicate 200 OK or error responses
     */
    if ((previous_call_line == 0xFF) && (prevcall == FALSE)) {
        CCSIP_DEBUG_ERROR("%s: INFO: Skipping store for this Ack\n", fname);
    } else {
        reldel_stored_msg = sipRelDevCoupledMessageStore(request,
                                                         response_callid,
                                                         cseq_number,
                                                         response_cseq_method,
                                                         TRUE,
                                                         status_code,
                                                         &dest_ipaddr,
                                                         (int16_t)dest_port,
                                                         FALSE /*Do check tag*/);
    }
    /* Send message */
    if (sipTransportChannelCreateSend(NULL, request, sipMethodAck,
                                      &dest_ipaddr, (int16_t)dest_port, 0,
                                      reldel_stored_msg) < 0) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sipTransportChannelCreateSend()");
        if (request)
            free_sip_message(request);
        return;
    }

    return;
}


/*
 * Resend the last message sent
 */
boolean
sipSPISendLastMessage (ccsipCCB_t *ccb)
{
    const char *fname = "sipSPISendLastMessage";

    /* Args Check */
    if (!ccb) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT),
                          fname, "ccb");
        return (FALSE);
    }


    /* Send message */
    if (ccb->index == 0) {
        if (sipTransportSendMessage(ccb,
                            sipPlatformUISMTimers[ccb->index].message_buffer,
                            sipPlatformUISMTimers[ccb->index].message_buffer_len,
                            sipPlatformUISMTimers[ccb->index].message_type,
                            &(sipPlatformUISMTimers[ccb->index].ipaddr),
                            sipPlatformUISMTimers[ccb->index].port,
                            TRUE, TRUE, 0, NULL) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sipTransportSendMessage()");
            return (FALSE);
        }
    } else {
        if (sipTransportChannelSend(ccb,
                            sipPlatformUISMTimers[ccb->index].message_buffer,
                            sipPlatformUISMTimers[ccb->index].message_buffer_len,
                            sipPlatformUISMTimers[ccb->index].message_type,
                            &(sipPlatformUISMTimers[ccb->index].ipaddr),
                            sipPlatformUISMTimers[ccb->index].port, 0) < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sipTransportChannelSend()");
            return (FALSE);
        }
    }
    return (TRUE);
}

void
sipGetRequestMethod (sipMessage_t *pRequest, sipMethod_t *pMethod)
{
    const char   *fname = "SIPGetRequestMethod";
    sipReqLine_t *pReqLine = NULL;

    *pMethod = sipMethodInvalid;
    pReqLine = sippmh_get_request_line(pRequest);

    if (!pReqLine) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_get_request_line()");
        return;
    }

    if (pReqLine->method) {
        *pMethod = sippmh_get_method_code(pReqLine->method);
    } else {
        CCSIP_DEBUG_ERROR("%s: Error: No recognizable method in Req-URI!\n",
                          fname);
    }

    SIPPMH_FREE_REQUEST_LINE(pReqLine);
    return;
}


int
sipGetResponseMethod (sipMessage_t *pResponse, sipMethod_t *pMethod)
{
    const char    *fname = "SIPGetResponseMethod";
    sipRespLine_t *pRespLine = NULL;
    const char    *cseq = NULL;
    sipCseq_t     *sipCseq = NULL;

    pRespLine = sippmh_get_response_line(pResponse);
    if (pRespLine) {
        cseq = sippmh_get_cached_header_val(pResponse, CSEQ);
        if (!cseq) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sippmh_get_cached_header_val(CSEQ)");
            SIPPMH_FREE_RESPONSE_LINE(pRespLine);
            return (-1);
        }
        /* Extract method code */
        sipCseq = sippmh_parse_cseq(cseq);
        if (!sipCseq) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sippmh_parse_cseq()");
            SIPPMH_FREE_RESPONSE_LINE(pRespLine);
            return (-1);
        }
        *pMethod = sipCseq->method;
        cpr_free(sipCseq);
    } else {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_get_response_line()");
        return (-1);
    }

    SIPPMH_FREE_RESPONSE_LINE(pRespLine);
    return (0);
}


int
sipGetResponseCode (sipMessage_t *pResponse, int *pResponseCode)
{
    const char    *fname = "SIPGetResponseCode";
    sipRespLine_t *pRespLine = NULL;

    pRespLine = sippmh_get_response_line(pResponse);
    if (pRespLine) {
        *pResponseCode = pRespLine->status_code;
    } else {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_get_response_line()");
        return (-1);
    }

    SIPPMH_FREE_RESPONSE_LINE(pRespLine);
    return (0);
}


/*
 * Util workhorse to add the standard headers to all SIP messages.
 * Does not affect call state.
 */
boolean
sipSPIAddStdHeaders (sipMessage_t *msg, ccsipCCB_t *ccb, boolean isResponse)
{
    boolean retval = FALSE;
    boolean flip = TRUE;
    int     max_fwd = 0;

    if (!(ccb && msg)) {
        return (retval);
    }

    if (isResponse) {
        if (ccb->flags & INCOMING) {
            flip = FALSE;
        } else {
            flip = TRUE;
        }
    } else {
        if (ccb->flags & INCOMING) {
            flip = TRUE;
        } else {
            flip = FALSE;
        }
    }

    if (!flip) {
        retval = (boolean)
            ((STATUS_SUCCESS == sippmh_add_text_header(msg,
                                                       SIP_HEADER_FROM,
                                                       ccb->sip_from)) &&
             (STATUS_SUCCESS == sippmh_add_text_header(msg,
                                                       SIP_HEADER_TO,
                                                       ccb->sip_to)) &&
             (STATUS_SUCCESS == sippmh_add_text_header(msg,
                                                       SIP_HEADER_CALLID,
                                                       ccb->sipCallID)));
    } else {
        retval = (boolean)
            ((STATUS_SUCCESS == sippmh_add_text_header(msg,
                                                       SIP_HEADER_FROM,
                                                       ccb->sip_to)) &&
             (STATUS_SUCCESS == sippmh_add_text_header(msg,
                                                       SIP_HEADER_TO,
                                                       ccb->sip_from)) &&
             (STATUS_SUCCESS == sippmh_add_text_header(msg,
                                                       SIP_HEADER_CALLID,
                                                       ccb->sipCallID)));
    }

    // Add max-forwards header if this is a request
    if (isResponse == FALSE && retval == TRUE) {
        config_get_value(CFGID_SIP_MAX_FORWARDS, &max_fwd, sizeof(max_fwd));
        if (max_fwd == 0) {
            max_fwd = SIP_MAX_FORWARDS_DEFAULT_VALUE;
        }
        if (sippmh_add_int_header(msg, SIP_HEADER_MAX_FORWARDS, max_fwd)
                != STATUS_SUCCESS) {
            retval = FALSE;
        }
    }
    return (retval);
}


/*
 * Add a SIP Via header to the sipMessage_t.
 * Should return (TRUE) if the right members are
 * present in the CCB struct.
 * Does not affect call state.
 */
boolean
sipSPIAddLocalVia (sipMessage_t *msg, ccsipCCB_t *ccb, sipMethod_t method)
{
    const char *fname = "sipSPIAddLocalVia";

    if (msg && ccb) {
        if (util_check_if_ip_valid(&(ccb->src_addr))) {
            static char via[SIP_MAX_VIA_LENGTH];
            char src_addr_str[MAX_IPADDR_STR_LEN];
            char  *sip_via_branch;
            int16_t trx_index = -1;

            /*
             * We don't allocate a block for ACK(200 OK) although
             * it is technically another transaction.  Instead we
             * locate the INVITE transaction in ccb.
             */
            if (method == sipMethodAck) {
                trx_index = get_method_request_trx_index(ccb, sipMethodInvite,
                                                         TRUE);
            } else {
                trx_index = get_last_request_trx_index(ccb, TRUE);
            }

            if (trx_index < 0) {
                return FALSE;
            }
            ipaddr2dotted(src_addr_str, &ccb->src_addr);
            if (method == sipMethodCancel) {
                // Get the via header from last sent request transaction
                if (trx_index < 1) {
                    return FALSE;
                }
                sip_via_branch = strlib_open(ccb->sent_request[trx_index].u.sip_via_branch,
                                             VIA_BRANCH_LENGTH);
                strncpy(sip_via_branch, (char *)(ccb->sent_request[trx_index - 1].u.sip_via_branch), VIA_BRANCH_LENGTH);
                ccb->sent_request[trx_index].u.sip_via_branch = strlib_close(sip_via_branch);

                snprintf(via, sizeof(via),
                         "SIP/2.0/%s %s:%d;%s=%s",
                         sipTransportGetTransportType(ccb->dn_line, TRUE, ccb),
                         src_addr_str, ccb->local_port, VIA_BRANCH,
                         (char *)(ccb->sent_request[trx_index].u.sip_via_branch));

            } else {
                snprintf(via, sizeof(via),
                         "SIP/2.0/%s %s:%d;%s=",
                         sipTransportGetTransportType(ccb->dn_line, TRUE, ccb),
                         src_addr_str, ccb->local_port, VIA_BRANCH);

                sip_via_branch = strlib_open(ccb->sent_request[trx_index].u.sip_via_branch,
                                             VIA_BRANCH_LENGTH);
                if (sip_via_branch) {
                    snprintf(sip_via_branch, VIA_BRANCH_LENGTH,
                             "%s%.8x", VIA_BRANCH_START,
                             (unsigned int)cpr_rand());
                }
                ccb->sent_request[trx_index].u.sip_via_branch =
                    strlib_close(sip_via_branch);

                if (sip_via_branch) {
                    sstrncat(via, sip_via_branch,
                            SIP_MAX_VIA_LENGTH - strlen(via));
                }
            }
            if (sippmh_add_text_header(msg, SIP_HEADER_VIA, via)
                    != STATUS_SUCCESS) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                                  fname, "sippmh_add_text_header(VIA)");
                return (FALSE);
            }
        } else {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT),
                              fname, "ccb->src_addr");
            return (FALSE);
        }
    }

    return (TRUE);
}


/*
 * For a response, add the Via headers that came in with the
 * request to the response.
 */
boolean
sipSPIAddRequestVia (ccsipCCB_t *ccb, sipMessage_t *response,
                    sipMessage_t *request, sipMethod_t method)
{
    const char *fname = "sipSPIAddRequestVia";
    int16_t     trx_index = -1;

    /* Check args */
    if (!response) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT),
                          fname, "response");
        return (FALSE);
    }
    if (!request) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT),
                          fname, "request");
        return (FALSE);
    }

    // Get the stored via from the transaction that we are responding to
    if (ccb) {
        trx_index = get_method_request_trx_index(ccb, method, FALSE);
        if (trx_index >= 0) {
            (void) sippmh_add_text_header(response, SIP_HEADER_VIA,
                                          (char *)(ccb->recv_request[trx_index].u.sip_via_header));
            return (TRUE);
        }
    }

    // If no transaction, get the via directly from the request
    (void) sippmh_add_text_header(response, SIP_HEADER_VIA,
                                  sippmh_get_cached_header_val(request, VIA));

    return (TRUE);
}


/*
 * Procedure to add a Route/Record-Route header in SIP responses
 */
boolean
sipSPIAddRouteHeaders (sipMessage_t *msg, ccsipCCB_t *ccb,
                       char *result_route, int result_route_length)
{
    const char *fname = "SIPSPIAddRouteHeaders";
    /* NOTE: route will be limited to 4 hops */
    static char route[MAX_SIP_HEADER_LENGTH * NUM_INITIAL_RECORD_ROUTE_BUFS];
    static char Contact[MAX_SIP_HEADER_LENGTH];
    boolean     lr = FALSE;

    /* Check args */
    if (!msg) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT),
                          fname, "msg");
        return (FALSE);
    }
    if (!ccb) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT),
                          fname, "ccb");
        return (FALSE);
    }

    if (!ccb->record_route_info) {
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Route info not available; will not"
                            " add Route header.\n", DEB_F_PREFIX_ARGS(SIP_ROUTE, fname));
        return (TRUE);
    }

    memset(route, 0, MAX_SIP_HEADER_LENGTH * NUM_INITIAL_RECORD_ROUTE_BUFS);
    memset(Contact, 0, MAX_SIP_HEADER_LENGTH);

    if (ccb->flags & INCOMING) {
        /*
         * For Incoming call (UAS), Copy the RR headers as it is
         * If Contact is present, append it at the end
         */
        if (sipSPIGenerateRouteHeaderUAS(ccb->record_route_info, route,
                    sizeof(route), &lr) == FALSE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sipSPIGenerateRouteHeaderUAS()");
            return (FALSE);
        }
    } else {
        /*
         * For Outgoing call (UAC), Copy the RR headers in the reverse
         * order. If Contact is present, append it at the end
         */
        if (sipSPIGenerateRouteHeaderUAC(ccb->record_route_info, route,
                    sizeof(route), &lr) == FALSE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sipSPIGenerateRouteHeaderUAC()");
            return (FALSE);
        }
    }
    /*
     * If loose_routing is TRUE, then the contact header is NOT appended
     * to the Routeset but is instead used in the Req-URI
     */
    if (!lr) {
        Contact[0] = '\0';
        if (sipSPIGenerateContactHeader(ccb->contact_info, Contact,
                    sizeof(Contact)) == FALSE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sipSPIGenerateContactHeader()");
            return (FALSE);
        }

        /* Append Contact to the Route Header, if Contact is available */
        if (Contact[0] != '\0') {
            if (route[0] != '\0') {
                strncat(route, ", ", sizeof(route) - strlen(route) - 1);
            }
            strncat(route, Contact, sizeof(route) - strlen(route) - 1);
        }
    }

    if (route[0] != '\0') {
        if (sippmh_add_text_header(msg, SIP_HEADER_ROUTE, route)
                == STATUS_SUCCESS) {
            CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Adding route = %s\n", 
                DEB_F_PREFIX_ARGS(SIP_ROUTE, fname), route);
            if (result_route) {
                sstrncpy(result_route, route, result_route_length);
            }
        } else {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sippmh_add_text_header(ROUTE)");
            return (FALSE);
        }
    } else {
        /* Having nothing in Route header is a legal case.
         * This would happen when the Record-Route header has
         * a single entry and Contact was NULL
         */
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Not adding route \n", DEB_F_PREFIX_ARGS(SIP_ROUTE, fname));
    }

    return (TRUE);
}


boolean
sipSPIAddRequestRecordRoute (sipMessage_t *response, sipMessage_t *request)
{
    const char *fname = "SIPSPIAddRequestRecordRoute";

    /* Check args */
    if (!response) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT),
                          fname, "response");
        return (FALSE);
    }
    if (!request) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT),
                          fname, "request");
        return (FALSE);
    }

    (void) sippmh_add_text_header(response, SIP_HEADER_RECORD_ROUTE,
                                  sippmh_get_cached_header_val(request, RECORD_ROUTE));

    return (TRUE);
}


/*
 * Procedure to add a proprietary SIP header to carry GUID in the SIP messsage
 */
boolean
sipSPIAddCiscoGuid (sipMessage_t *msg, ccsipCCB_t *ccb)
{
    boolean retval = STATUS_FAILURE;
/*
    const char *fname = "sipSPIAddCiscoGuid";
    uint32_t guid[4];
    char guid_char[4*CC_GUID_SIZE];

    if (ccb && msg) {

        memcpy(&guid[0], ccb->guid, sizeof(guid[0]));
        memcpy(&guid[1], &ccb->guid[sizeof(guid[0])], sizeof(guid[0]));
        memcpy(&guid[2], &ccb->guid[2*sizeof(guid[0])], sizeof(guid[0]));
        memcpy(&guid[3], &ccb->guid[3*sizeof(guid[0])], sizeof(guid[0]));

        sprintf(guid_char, "%u-%u-%u-%u", guid[0], guid[1], guid[2], guid[3]);

        retval = (STATUS_SUCCESS == sippmh_add_text_header(msg,
                                                      SIP_HEADER_CISCO_GUID,
                                                      guid_char));
    } else {
        CCSIP_DEBUG_ERROR("%s: Error: Fatal error in parsing ccb.\n",
                          fname);
    }
*/
    return (retval);
}

int
sipSPICheckContentHeaders (sipMessage_t *msg)
{
    uint8_t     i;
    const char *accept_hdr = NULL;
    const char *content_enc = NULL;
    const char *content_disp_str = NULL;
    const char *accepted_enc_str = NULL;
    cc_content_disposition_t *content_disp = NULL;
    const char *fname = "sipSPICheckContentHeaders";
    sipMethod_t method = sipMethodInvalid;
    char *lasts = NULL;

    if (!msg) {
        return (SIP_MESSAGING_ERROR);
    }

    if (sippmh_msg_header_present(msg, SIP_HEADER_ACCEPT)) {
        accept_hdr = sippmh_get_header_val(msg, SIP_HEADER_ACCEPT, NULL);
        if (!accept_hdr) {
            if (sippmh_is_request(msg)) {
                sipGetRequestMethod(msg, &method);
                if (method == sipMethodInvite) {
                    /*
                     * Currently we are rejecting empty Accept headers for
                     * INVITE only.  This check is done here instead of
                     * previously because Accept is Content related header
                     * and also in the future if we want to extend this
                     * check to other requests and responses this would be
                     * a good place
                     */
                    return (SIP_MESSAGING_NOT_ACCEPTABLE);
                }
            }
        }
    }
    content_enc = sippmh_get_header_val(msg, SIP_HEADER_CONTENT_ENCODING,
                                        SIP_C_HEADER_CONTENT_ENCODING);
    content_disp_str = sippmh_get_header_val(msg, SIP_HEADER_CONTENT_DISP,
                                             SIP_HEADER_CONTENT_DISP);
    accepted_enc_str = sippmh_get_header_val(msg, SIP_HEADER_ACCEPT_ENCODING,
                                             SIP_HEADER_ACCEPT_ENCODING);
    if (content_disp_str) {
        content_disp = sippmh_parse_content_disposition(content_disp_str);
    }

    // Check Content-Encoding
    // Only identity encoding is supported at this time
    if (content_enc) {
        if (cpr_strcasecmp(content_enc, SIP_CONTENT_ENCODING_IDENTITY)) {
            // If Content-Encoding is not understood, check to see if
            // the Content-Disposition is required. If so, flag an error
            if (content_disp) {
                if (content_disp->required_handling) {
                    cpr_free(content_disp);
                    return (SIP_MESSAGING_ERROR_UNSUPPORTED_MEDIA);
                }
            } else {
                return (SIP_MESSAGING_ERROR_UNSUPPORTED_MEDIA);
            }
        }
    }

    // Check Content-Disposition
    // Only disposition of "session", if not reject if handling it
    // content is required
    if (content_disp) {
        if (content_disp->disposition != cc_disposition_session) {
            if (content_disp->required_handling) {
                cpr_free(content_disp);
                return (SIP_MESSAGING_ERROR_UNSUPPORTED_MEDIA);
            }
        }
    }
    if (content_disp) {
        cpr_free(content_disp);
    }

    if (accepted_enc_str) {
        //parse the accepted_enc_str
        //check to see if it contains "identity"
        //if it does not contain "identity" return error

        boolean found = FALSE;
        char *ptr = NULL;
        char *accepted_enc_str_dup;

        accepted_enc_str_dup = cpr_strdup(accepted_enc_str);
        if (accepted_enc_str_dup == NULL) {
            CCSIP_DEBUG_ERROR("%s: Error: cpr_strdup() failed "
                              "for accepted_enc_str_dup\n", fname);
            return (SIP_SERV_ERR_INTERNAL);
        }

        ptr = cpr_strtok(accepted_enc_str_dup, ", ", &lasts);

        while (ptr) {
            if (strcmp(ptr, SIP_CONTENT_ENCODING_IDENTITY) == 0) {
                found = TRUE;
                break;
            }
            ptr = cpr_strtok(NULL, ", ", &lasts);
        }

        cpr_free(accepted_enc_str_dup);

        if (found == FALSE) {
            return (SIP_MESSAGING_ERROR_UNSUPPORTED_MEDIA);
        }
    }


    // Check all the body parts
    for (i = 0; i < HTTPISH_MAX_BODY_PARTS; i++) {
        if (msg->mesg_body[i].msgBody) {
            if (msg->mesg_body[i].msgContentTypeValue
                    == SIP_CONTENT_TYPE_UNKNOWN_VALUE) {
                CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Pass-through \"%s\"\n",
                                    DEB_F_PREFIX_ARGS(SIP_CONTENT_TYPE, fname),
                                    msg->mesg_body[i].msgContentType);
                // return (SIP_MESSAGING_ERROR_UNSUPPORTED_MEDIA);
            }
            if ((msg->mesg_body[i].msgContentEnc
                        != SIP_CONTENT_ENCODING_IDENTITY_VALUE) &&
                (msg->mesg_body[i].msgRequiredHandling == TRUE)) {
                return (SIP_MESSAGING_ERROR_UNSUPPORTED_MEDIA);
            }
            if ((msg->mesg_body[i].msgContentDisp
                        != SIP_CONTENT_DISPOSITION_SESSION_VALUE) &&
                (msg->mesg_body[i].msgRequiredHandling == TRUE)) {
                return (SIP_MESSAGING_ERROR_UNSUPPORTED_MEDIA);
            }
        }
    }

    return (SIP_MESSAGING_OK);
}

// This function returns TRUE if the header length is acceptable
boolean
is_good_header_length (const char *header, uint8_t header_type)
{

    if (!header) {
        return FALSE;
    }
    switch (header_type) {
    case FROM:
    case TO:
        if ((strlen(header) >= MAX_SIP_URL_LENGTH) || (strlen(header) == 0)) {
            return FALSE;
        }
        break;
    case CALLID:
        if ((strlen(header) >= MAX_SIP_CALL_ID) || (strlen(header) == 0)) {
            return FALSE;
        }
        break;
    default:
        break;
    }
    return TRUE;
}

/*
 * sipCheckRequestURI
 *
 * Check the validity of mandatory fields in Req URI
 *
 * Adding restrictive checks to the following will break
 * ccm features: "user=phone"
 *
 */
int
sipCheckRequestURI (ccsipCCB_t *ccb, sipMessage_t *request)
{
    sipReqLine_t   *requestURI = NULL;
    genUrl_t       *genUrl = NULL;
    sipUrl_t       *sipUriUrl = NULL;
    char           src_addr_str[MAX_IPADDR_STR_LEN];
    char           *pUser = NULL;
    cpr_ip_addr_t  src_addr;
    int            nat_enable = 0;
    boolean        request_uri_error = FALSE;
    int            errorCode = SIP_MESSAGING_ERROR;
    cpr_ip_addr_t  ipaddr;

    CPR_IP_ADDR_INIT(src_addr);
    CPR_IP_ADDR_INIT(ipaddr);

    requestURI = sippmh_get_request_line(request);
    if (requestURI) {
        if (requestURI->url) {
            genUrl = sippmh_parse_url(requestURI->url, TRUE);
            if (genUrl) {
                if (genUrl->schema == URL_TYPE_SIP) {
                    sipUriUrl = genUrl->u.sipUrl;
                }
                if (sipUriUrl) {
                    pUser = sippmh_parse_user(sipUriUrl->user);
                    if (pUser) {
                        if (sipUriUrl->host) {
                            if (!str2ip(sipUriUrl->host, &ipaddr)) {
                                config_get_value(CFGID_NAT_ENABLE, &nat_enable,
                                                 sizeof(nat_enable));
                                if (nat_enable == 0) {
                                    sip_config_get_net_device_ipaddr(&src_addr);
                                } else {
                                    sip_config_get_nat_ipaddr(&src_addr);
                                }
                                ipaddr2dotted(src_addr_str, &src_addr);
                                if (strcmp(sipUriUrl->host, src_addr_str)) {
                                    if (!validateHostName(sipUriUrl->host, pUser)) {
                                        CCSIP_DEBUG_ERROR("Unknown address in Request URI\n");
                                        request_uri_error = TRUE;
                                        errorCode = SIP_MESSAGING_ENDPOINT_NOT_FOUND;
                                    }
                                }
                            } else {
                                if (!validateHostName(sipUriUrl->host, pUser)) {
                                    CCSIP_DEBUG_ERROR("Unknown address in Request URI\n");
                                    request_uri_error = TRUE;
                                    errorCode = SIP_MESSAGING_ENDPOINT_NOT_FOUND;
                                }
                            }
                            if (sipUriUrl->port_present) {
                                if (ccb &&
                                    cpr_strcasecmp(sipTransportGetTransportType(ccb->dn_line, FALSE, ccb), "udp") == 0) {
                                    if (sipUriUrl->port != ccb->local_port) {
                                        CCSIP_DEBUG_ERROR("Port Mismatch(UDP), URL Port: %d, Port Used: %d\n",
                                             sipUriUrl->port, ccb->local_port);
                                        request_uri_error = TRUE;
                                        errorCode = SIP_MESSAGING_ENDPOINT_NOT_FOUND;
                                    }
                                }
                            }
                        }
                        if (pUser[0] == '\0') {
                            request_uri_error = TRUE;
                            errorCode = SIP_MESSAGING_ENDPOINT_NOT_FOUND;
                        } else {
                            if (ccb && !request_uri_error) {
                                sstrncpy(ccb->ReqURI, pUser,
                                         sizeof(ccb->ReqURI));
                            }
                        }
                        cpr_free(pUser);
                    }
                }
                sippmh_genurl_free(genUrl);
            } else {
                request_uri_error = TRUE;
                errorCode = SIP_MESSAGING_ENDPOINT_NOT_FOUND;
            }
        } else {
            request_uri_error = TRUE;
            errorCode = SIP_MESSAGING_ERROR;
        }
        SIPPMH_FREE_REQUEST_LINE(requestURI);
    } else {
        request_uri_error = TRUE;
        errorCode = SIP_MESSAGING_ERROR;
    }

    if (request_uri_error) {
        return errorCode;
    } else {
        return (SIP_MESSAGING_OK);
    }
}

/*
 * This is called for every request received.
 * It checks the basic fields(url, From, To).
 * Does not affect call state.
 * It sends error responses for generic error types.
 */
int
sipSPICheckRequest (ccsipCCB_t *ccb, sipMessage_t *request)
{
    const char   *fname = "sipSPICheckRequest";
    const char   *callID = NULL;
    const char   *via = NULL;
    const char   *from = NULL;
    const char   *to = NULL;
    const char   *contact = NULL;
    int           retval = SIP_MESSAGING_OK;
    char         *replaceshdr = NULL;

    const char   *request_cseq = NULL;
    sipCseq_t    *request_cseq_structure = NULL;
    uint32_t      request_cseq_number = 0;
    sipMethod_t   request_cseq_method = sipMethodInvalid;
    sipReqLine_t *requestURI;

    /*
     * Check args - CCB may be NULL
     */
    if (!request) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT),
                          fname, "request");
        return (SIP_MESSAGING_ERROR);
    }

    /*
     * Check for the incomplete message body
     */
    if (!sippmh_is_message_complete(request)) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_is_message_complete()");
        return (SIP_MESSAGING_ERROR);
    }


    if ((retval = sipCheckRequestURI(ccb, request)) != SIP_MESSAGING_OK) {
        CCSIP_DEBUG_ERROR("%s: Request URI Not Found\n", fname);
        return (retval);
    }


    /*
     * Check whether all mandatory SIP request headers are there.
     * Also check if their lengths are within an acceptable range.
     */
    from = sippmh_get_cached_header_val(request, FROM);
    if (!from || (!is_good_header_length(from, FROM))) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_get_cached_header_val(FROM)");
        return (SIP_MESSAGING_ERROR);
    }

    to = sippmh_get_cached_header_val(request, TO);
    if (!to || (!is_good_header_length(to, TO))) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_get_cached_header_val(TO)");
        return (SIP_MESSAGING_ERROR);
    }

    via = sippmh_get_cached_header_val(request, VIA);
    if (!via) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_get_cached_header_val(VIA)");
        return (SIP_MESSAGING_ERROR);
    }

    callID = sippmh_get_cached_header_val(request, CALLID);
    if (!callID || (!is_good_header_length(callID, CALLID))) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_get_cached_header_val(CALLID)");
        return (SIP_MESSAGING_ERROR);
    }

    contact = sippmh_get_cached_header_val(request, CONTACT);
    if (contact) {
        int contact_check_result = 0;

        contact_check_result = sipSPICheckContact(contact);
        if (contact_check_result < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sipSPICheckContact()");
            return contact_check_result;
        }
    }

    if ((retval = sipSPICheckContentHeaders(request)) != SIP_MESSAGING_OK) {
        CCSIP_DEBUG_ERROR("%s: Content header value not supported\n", fname);
        return (retval);
    }

    /*
     * Check whether the current call id and this request's CallId match
     */
    if (ccb && ccb->sipCallID[0]) {
        if (strcmp(ccb->sipCallID, callID) != 0) {
            CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Call ID match: new call id.\n", DEB_F_PREFIX_ARGS(SIP_CALL_ID, fname));
            retval = SIP_MESSAGING_NEW_CALLID;
        } else {
            CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Call ID match: same call id.\n", DEB_F_PREFIX_ARGS(SIP_CALL_ID, fname));
        }
    }

    /*
     * Parse CSeq
     */
    request_cseq = sippmh_get_cached_header_val(request, CSEQ);
    if (!request_cseq) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_get_cached_header_val(CSEQ)");
        return (SIP_MESSAGING_ERROR);
    }

    request_cseq_structure = sippmh_parse_cseq(request_cseq);
    if (!request_cseq_structure) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_parse_cseq()");
        return (SIP_MESSAGING_ERROR);
    }
    request_cseq_number = request_cseq_structure->number;
    request_cseq_method = request_cseq_structure->method;
    cpr_free(request_cseq_structure);

    // Check continuity of this request wrt CSeq number and method
    if (request_cseq_method != sipMethodAck &&
        request_cseq_method != sipMethodCancel) {
        // If ccb is present, check if the CSeq number is acceptable
        if (ccb) {
            if (request_cseq_number < ccb->last_recv_request_cseq) {
                CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Inconsistent CSEQ number. "
                                    "current= %d, last= %d.\n",
                                    DEB_F_PREFIX_ARGS(SIP_CSEQ, fname), request_cseq_number,
                                    ccb->last_recv_request_cseq);
                                    
                return (SIP_CLI_ERR_BAD_REQ);
            }
            if (request_cseq_number == ccb->last_recv_request_cseq) {
                if (request_cseq_method != ccb->last_recv_request_cseq_method) {
                    CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Inconsistent CSEQ method. "
                                        "current= %s, last= %s\n.",
                                        DEB_F_PREFIX_ARGS(SIP_CSEQ, fname),
                                        sipGetMethodString(request_cseq_method),
                                        sipGetMethodString(ccb->last_recv_request_cseq_method));
                                        
                    return (SIP_CLI_ERR_BAD_REQ);
                }
            }
        }
    }

    if (request->mesg_line) {
        if (strlen(request->mesg_line) >= MAX_SIP_URL_LENGTH) {
            CCSIP_DEBUG_ERROR("%s: Request URI length exceeds acceptable value\n",
                              fname);
            return (SIP_MESSAGING_ERROR);
        }
    }

    requestURI = sippmh_get_request_line(request);
    if (requestURI) {
        if (sippmh_get_method_code(requestURI->method) != request_cseq_method) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "Method in RURI != method in CSeq");
            SIPPMH_FREE_REQUEST_LINE(requestURI);
            return (SIP_MESSAGING_ERROR);
        }
        SIPPMH_FREE_REQUEST_LINE(requestURI);
    }

    if (request_cseq_method != sipMethodInvite) {
        if (sippmh_msg_header_present(request, SIP_HEADER_REPLACES)) {
            CCSIP_DEBUG_ERROR("%s: Error: Replace header is not valid for "
                              "this request\n", fname);
            return SIP_MESSAGING_ERROR;
        }
    } else {
        if (sippmh_get_num_particular_headers(request, SIP_HEADER_REPLACES,
                    NULL, &replaceshdr, MAX_REPLACES_HEADERS + 1)
                    > MAX_REPLACES_HEADERS) {
            CCSIP_DEBUG_ERROR("%s: Error: More than one replaces header "
                              "in this request\n", fname);
            return SIP_MESSAGING_ERROR;
        }
    }

    /*
     * Reliable Delivery
     */
    if (SipRelDevEnabled) {
        /*
         * responseRecord is a static because the structure it is made out of is
         * so large.
         */
        static sipRelDevMessageRecord_t requestRecord;
        int            handle = -1;
        const char    *reldev_to = NULL;
        sipLocation_t *reldev_to_loc = NULL;
        char           reldev_to_tag[MAX_SIP_TAG_LENGTH];
        const char    *reldev_from = NULL;
        sipLocation_t *reldev_from_loc = NULL;
        char           reldev_from_tag[MAX_SIP_TAG_LENGTH];

        memset(&requestRecord, 0, sizeof(requestRecord));
        memset(reldev_to_tag, 0, MAX_SIP_TAG_LENGTH);
        memset(reldev_from_tag, 0, MAX_SIP_TAG_LENGTH);

        /* Get to_tag */
        reldev_to = sippmh_get_cached_header_val(request, TO);

        if (reldev_to) {
            reldev_to_loc = sippmh_parse_from_or_to((char *)reldev_to, TRUE);
            if (reldev_to_loc) {
                if (reldev_to_loc->genUrl->schema != URL_TYPE_SIP) {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_URL_ERROR),
                                      fname);
                    sippmh_free_location(reldev_to_loc);
                    return (SIP_CLI_ERR_FORBIDDEN);
                }

                if (reldev_to_loc->tag) {
                    sstrncpy(reldev_to_tag,
                             sip_sm_purify_tag(reldev_to_loc->tag),
                             MAX_SIP_TAG_LENGTH);
                }
                sstrncpy(requestRecord.to_user,
                         reldev_to_loc->genUrl->u.sipUrl->user,
                         RELDEV_MAX_USER_NAME_LEN);
                sippmh_free_location(reldev_to_loc);

            } else {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                                  fname,
                                  get_debug_string(DEBUG_FUNCTIONNAME_SIPPMH_PARSE_TO));
                return (SIP_MESSAGING_ERROR);
            }
        } else {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sippmh_get_cached_header_val(TO)");
            return (SIP_MESSAGING_ERROR);
        }

        /* Store from_user and from_host */
        reldev_from = sippmh_get_cached_header_val(request, FROM);
        if (reldev_from) {
            reldev_from_loc = sippmh_parse_from_or_to((char *)reldev_from, TRUE);
            if (reldev_from_loc) {
                sstrncpy(requestRecord.from_user,
                         reldev_from_loc->genUrl->u.sipUrl->user,
                         RELDEV_MAX_USER_NAME_LEN);
                sstrncpy(requestRecord.from_host,
                         reldev_from_loc->genUrl->u.sipUrl->host,
                         RELDEV_MAX_HOST_NAME_LEN);
                if (reldev_from_loc->tag) {
                    sstrncpy(reldev_from_tag,
                             sip_sm_purify_tag(reldev_from_loc->tag),
                             MAX_SIP_TAG_LENGTH);
                }
                sippmh_free_location(reldev_from_loc);
            }
        }

        /* Check whether the tag matches the stored tag */
        if (ccb) {
            if ((request_cseq_method == sipMethodInvite) ||
                (request_cseq_method == sipMethodAck) ||
                (request_cseq_method == sipMethodBye)) {

                boolean         to_tag_match = TRUE;
                boolean         from_tag_match = TRUE;

                CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"in_to_tag:<%s>, in_from_tag:<%s>, "
                                    "stored to tag=<%s>, stored from tag=<%s>\n",
                                    DEB_F_PREFIX_ARGS(SIP_TAG, fname), reldev_to_tag, reldev_from_tag,
                                    ccb->sip_to_tag, ccb->sip_from_tag);

                /* Check to_tag first */
                if (ccb->sip_to_tag[0]) {
                    if ((request_cseq_method == sipMethodBye) &&
                        (reldev_to_tag[0] == '\0') &&
                        (SIP_SM_CALL_SETUP_NOT_COMPLETED(ccb))) {
                        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Allow early call termination "
                                            "BYE.\n", DEB_F_PREFIX_ARGS(SIP_CALL_STATUS, fname));
                    } else if ((request_cseq_method == sipMethodInvite) &&
                               (reldev_to_tag[0] == '\0') &&
                               (SIP_SM_CALL_SETUP_NOT_COMPLETED(ccb))) {
                        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Allow crossed "
                                            "response and INVITE.\n", DEB_F_PREFIX_ARGS(SIP_CALL_STATUS, fname));
                    } else {
                        if (ccb->flags & INCOMING) {
                            // Incoming request for an incoming call
                            // Match the stored to-tag with the to-tag in request
                            if (strcasecmp_ignorewhitespace(reldev_to_tag,
                                        ccb->sip_to_tag) != 0) {
                                to_tag_match = FALSE;
                            }
                        } else {
                            // We made the call, so match the stored to-tag
                            // with the from-tag in the incoming request
                            if (strcasecmp_ignorewhitespace(reldev_from_tag,
                                        ccb->sip_to_tag) != 0) {
                                to_tag_match = FALSE;
                            }
                        }
                    }
                }
                if (!to_tag_match) {
                    CCSIP_DEBUG_ERROR("%s: To-Tag mismatch detected!\n", fname);
                    return (SIP_CLI_ERR_CALLEG);
                }
                // Now check from-tag
                if (ccb->sip_from_tag[0]) {
                    if (ccb->flags & INCOMING) {
                        // Incoming request for an incoming call
                        // Match the stored from-tag with the from-tag in request
                        if (strcasecmp_ignorewhitespace(reldev_from_tag,
                                    ccb->sip_from_tag) != 0) {
                            from_tag_match = FALSE;
                        }
                    } else {
                        // We made the call, so match the stored from-tag
                        // with the to-tag in the incoming request
                        if (strcasecmp_ignorewhitespace(reldev_to_tag,
                                    ccb->sip_from_tag) != 0) {
                            from_tag_match = FALSE;
                        }
                    }
                    if (!from_tag_match) {
                        CCSIP_DEBUG_ERROR("%s: From-Tag mismatch detected!\n",
                                          fname);
                        return (SIP_CLI_ERR_CALLEG);
                    }
                }
            }
        }

        requestRecord.is_request = TRUE;
        sstrncpy(requestRecord.call_id, (callID) ? callID : "",
                 MAX_SIP_CALL_ID);
        requestRecord.cseq_number = request_cseq_number;
        requestRecord.cseq_method = request_cseq_method;
        sstrncpy(requestRecord.tag, reldev_to_tag, MAX_SIP_TAG_LENGTH);
        if (ccb) {
            //requestRecord.line = ccb->index;
        } else {
            //requestRecord.line = 1;
        }
        if (ccb && (requestRecord.cseq_method == sipMethodInvite) &&
            (ccb->state == SIP_STATE_IDLE)) {
            sipRelDevMessagesClear(requestRecord.call_id,
                                   requestRecord.from_user,
                                   requestRecord.from_host,
                                   requestRecord.to_user);
        }
        /* Check if duplicate */
        if (sipRelDevMessageIsDuplicate(&requestRecord, &handle)) {
            CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Duplicate request detected...\n", DEB_F_PREFIX_ARGS(SIP_RESP, fname));
            // If the request is an ACK we do not have any thing to send
            // This was retransmitted ACK so drop it.
            if (requestRecord.cseq_method != sipMethodAck) {
                if (sipRelDevCoupledMessageSend(handle) < 0) {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                                      fname, "sipRelDevCoupledMessageSend()");
                }
            }
            return (SIP_MESSAGING_DUPLICATE);
        }

        /* Record the request in Recent Requests List */
        sipRelDevMessageStore(&requestRecord);
    }
    return (retval);
}


/*
 * This is called for every response received.
 * It checks the basic fields(response line, From, To).
 * If a session description is present, it reads and parses it.
 * (and overwrites an old one if present in the CCB.)
 * Does not affect call state.
 * Returns TRUE if the request is valid.
 * Usually, but not necessarily an action function will drop
 * an invalid response. Exceptions are usually disconnection states,
 * where  a bad response may still be accepted.
 *
 * NOTE:
 *  It is assumed that this routine would never be called recursively or
 *  reentered via a separate task because responseRecord is declared as a
 *  large static variable
 */
int
sipSPICheckResponse (ccsipCCB_t *ccb, sipMessage_t *response)
{
    const char    *fname = "sipSPICheckResponse";
    const char    *from = NULL;
    const char    *to = NULL;
    const char    *callID = NULL;
    const char    *cseq = NULL;
    sipCseq_t     *sipCseq = NULL;
    sipRespLine_t *pRespLine = NULL;
    uint32_t       response_cseq_number = 0;
    sipMethod_t    response_method = sipMethodInvalid;
    uint16_t       response_code = 0;
    sipStatusCodeClass_t code_class = codeClassInvalid;
    int16_t        trx_index = -1;
    const char    *via = NULL;

    /*
     * Check for the incomplete message body
     */
    if (!sippmh_is_message_complete(response)) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_is_message_complete()");
        return (SIP_MESSAGING_ERROR);
    }

    /*
     * Check whether all mandatory SIP request headers are there.
     */
    from = sippmh_get_cached_header_val(response, FROM);
    if (!from || (!is_good_header_length(from, FROM))) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_get_cached_header_val(FROM)");
        return (SIP_MESSAGING_ERROR);
    }
    to = sippmh_get_cached_header_val(response, TO);
    if (!to || (!is_good_header_length(to, TO))) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_get_cached_header_val(TO)");
        return (SIP_MESSAGING_ERROR);
    }
    callID = sippmh_get_cached_header_val(response, CALLID);
    if (!callID || (!is_good_header_length(callID, CALLID))) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_get_cached_header_val(CALLID)");
        return (SIP_MESSAGING_ERROR);
    }
    cseq = sippmh_get_cached_header_val(response, CSEQ);
    if (!cseq) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_get_cached_header_val(CSEQ)");
        return (SIP_MESSAGING_ERROR);
    }
    via = sippmh_get_cached_header_val(response, VIA);
    if (via) {
        if (strchr(via, COMMA)) {
            CCSIP_DEBUG_ERROR("%s: Multiple Via headers found in response\n",
                              fname);
            return (SIP_MESSAGING_ERROR);
        }
    }

    if (sipSPICheckContentHeaders(response) != SIP_MESSAGING_OK) {
        CCSIP_DEBUG_ERROR("%s: Content header value not supported\n", fname);
        return (SIP_MESSAGING_ERROR);
    }

    /*
     * Get SIP response code
     */
    pRespLine = sippmh_get_response_line(response);
    if (!pRespLine) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_get_response_line()");
        return (SIP_MESSAGING_ERROR);
    }
    response_code = pRespLine->status_code;
    SIPPMH_FREE_RESPONSE_LINE(pRespLine);
    code_class = sippmh_get_code_class(response_code);

    /*
     * Extract response method and Cseq number from CSeq
     */
    sipCseq = sippmh_parse_cseq(cseq);
    if (!sipCseq) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sippmh_parse_cseq()");
        return (SIP_MESSAGING_ERROR);
    }
    response_method      = sipCseq->method;
    response_cseq_number = sipCseq->number;
    cpr_free(sipCseq);

    /*
     * If its a authentication response should not do any response
     * mismatch checking since some ACK's are not stored and so this
     * check will could return response mismatch.
     */
    switch (response_code) {
    case SIP_CLI_ERR_UNAUTH:
    case SIP_CLI_ERR_PROXY_REQD:
        if (response_method == sipMethodAck) {
            CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Authentication Request for ACK: callid=%s,"
                                "cseq=%u, cseq_method=%s\n",
                                DEB_F_PREFIX_ARGS(SIP_ACK, fname), callID, response_cseq_number,
                                sipGetMethodString(response_method));
            return (SIP_MESSAGING_OK);
        }
        break;

    default:
        break;
    }

    if (sippmh_msg_header_present(response, SIP_HEADER_REPLACES)) {
        CCSIP_DEBUG_ERROR("%s: Error: Replace header is not valid "
                          "in a response\n", fname);
        return SIP_MESSAGING_ERROR;
    }

    /*
     * Reliable Delivery
     */
    if (SipRelDevEnabled) {
        /*
         * responseRecord is a static because the structure it is
         * made out of is so large.
         */
        static sipRelDevMessageRecord_t responseRecord;
        int handle = -1;
        sipLocation_t *reldev_to_loc = NULL;
        char reldev_to_tag[MAX_SIP_TAG_LENGTH];
        sipLocation_t *reldev_from_loc = NULL;

        memset(&responseRecord, 0, sizeof(responseRecord));
        memset(reldev_to_tag, 0, MAX_SIP_TAG_LENGTH);

        /* Get to_tag */
        reldev_to_loc = sippmh_parse_from_or_to((char *)to, TRUE);
        if (reldev_to_loc) {
            if (reldev_to_loc->tag) {
                sstrncpy(reldev_to_tag,
                         sip_sm_purify_tag(reldev_to_loc->tag),
                         MAX_SIP_TAG_LENGTH);
            } else {
                reldev_to_tag[0] = '\0';
            }
            sstrncpy(responseRecord.to_user,
                     reldev_to_loc->genUrl->u.sipUrl->user,
                     RELDEV_MAX_USER_NAME_LEN);
            sippmh_free_location(reldev_to_loc);
        } else {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname,
                              get_debug_string(DEBUG_FUNCTIONNAME_SIPPMH_PARSE_TO));
            return (SIP_MESSAGING_ERROR);
        }

        reldev_from_loc = sippmh_parse_from_or_to((char *)from, TRUE);
        if (reldev_from_loc) {
            sstrncpy(responseRecord.from_user,
                     reldev_from_loc->genUrl->u.sipUrl->user,
                     RELDEV_MAX_USER_NAME_LEN);
            sstrncpy(responseRecord.from_host,
                     reldev_from_loc->genUrl->u.sipUrl->host,
                     RELDEV_MAX_HOST_NAME_LEN);

            /* Check from-tag */
            if (reldev_from_loc->tag) {
                if (!(ccb->flags & INCOMING)) {
                    if (strcmp(reldev_from_loc->tag, ccb->sip_from_tag) != 0) {
                        sippmh_free_location(reldev_from_loc);
                        CCSIP_DEBUG_ERROR("%s: Outgoing: From tag in response "
                                          "does not match stored value\n",
                                          fname);
                        return (SIP_MESSAGING_ERROR);
                    }
                } else {
                    if (strcmp(reldev_from_loc->tag, ccb->sip_to_tag) != 0) {
                        sippmh_free_location(reldev_from_loc);
                        CCSIP_DEBUG_ERROR("%s: Incoming: From tag in response "
                                          "does not match stored value\n",
                                          fname);
                        return (SIP_MESSAGING_ERROR);
                    }
                }
            }

            sippmh_free_location(reldev_from_loc);
        } else {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname,
                              get_debug_string(DEBUG_FUNCTIONNAME_SIPPMH_PARSE_FROM));
            return (SIP_MESSAGING_ERROR);
        }

        responseRecord.is_request = FALSE;
        sstrncpy(responseRecord.call_id, (callID) ? callID : "",
                 MAX_SIP_CALL_ID);
        responseRecord.cseq_method   = response_method;
        responseRecord.cseq_number   = response_cseq_number;
        responseRecord.response_code = response_code;
        sstrncpy(responseRecord.tag, reldev_to_tag, MAX_SIP_TAG_LENGTH);
        //responseRecord.line = ccb->index;

        /* Check if duplicate */
        if (sipRelDevMessageIsDuplicate(&responseRecord, &handle)) {
            CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Duplicate response detected...\n", DEB_F_PREFIX_ARGS(SIP_RESP, fname));
            if (sipRelDevCoupledMessageSend(handle) < 0) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                                  fname, "sipRelDevCoupledMessageSend()");
            }
            if ((response_method == sipMethodInvite) &&
                (code_class == codeClass1xx)) {
                /* Allow multiple provisional responses */
                CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Allowed duplicate provisional response\n",
                                    DEB_F_PREFIX_ARGS(SIP_RESP, fname));
            } else {
                return (SIP_MESSAGING_DUPLICATE);
            }
        }
        /* Record the response in Recent Requests List */
        sipRelDevMessageStore(&responseRecord);
    }

    /*
     * Check whether the outstanding request's and this response's
     * call id, cseq, and cseq method match
     */
    trx_index = get_method_request_trx_index(ccb, response_method, TRUE);
    if (trx_index < 0) {
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"No Matching Request Found!:\n"
                            "(Response: cseq=%u, trx_method=%s)\n",
                            DEB_F_PREFIX_ARGS(SIP_CALL_STATUS, fname), response_cseq_number,
                            sipGetMethodString(response_method));
        return (SIP_MESSAGING_ERROR_NO_TRX);
    }
    if ((ccb->sent_request[trx_index].cseq_number != response_cseq_number) ||
        (ccb->sent_request[trx_index].cseq_method != response_method) ||
        (strcmp(ccb->sipCallID, callID) != 0)) {
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Response mismatch:\n(Response:"
                            "callid=%s, cseq=%u, cseq_method=%s),\n"
                            "(Request: callid=%s, cseq=%u, "
                            "cseq_method=%s)\n",
                            DEB_F_PREFIX_ARGS(SIP_RESP, fname), callID, response_cseq_number,
                            sipGetMethodString(response_method),
                            ccb->sipCallID,
                            ccb->sent_request[trx_index].cseq_number,
                            sipGetMethodString(ccb->sent_request[trx_index].cseq_method));
        /*
         * What happened here is that a timing
         * issue occured between the proxy and phone where we
         * timed out at almost the same time and we have already
         * sent another request(probably a cancel) when the proxy
         * sends this response back to us. So that we just Ack this
         * response, we play games with previous call fields. We don't
         * want to store the information because this response is basically
         * stale at this point. We set the prevcall boolean to FALSE
         * but we set the previous_line to 0xFF. This will indicate to
         * the send routine to skip the storing of this Ack for retransmission.
         */
        if (response_method == sipMethodInvite) {
            const char *resp_via = NULL;
            sipVia_t *resp_via_parm = NULL;
            int16_t trx_index_temp = -1;
            const char *sip_via_branch = NULL;

            switch (code_class) {
            case codeClass2xx:
                /*
                 * If we get 200 response to a cancelled invite, send bye
                 * to clear up the call.
                 */
                trx_index_temp = get_method_request_trx_index(ccb,
                                                              sipMethodCancel,
                                                              TRUE);
                // if there is a CANCEL request outstanding -
                // if (ccb->last_sent_request_cseq_method == sipMethodCancel) {
                if (trx_index_temp > 0) {
                    const char *contact = NULL;

                    /*
                     * We need to update the contact and to, from tags recvd.
                     * in ccb because the CANCEL effectively becomes a nop due
                     * to the race condition where the UAS sent 200 OK to
                     * INVITE at the same instance we sent CANCEL. Hence we
                     * need to ACK the 200 OK for INVITE with route header and
                     * then send a BYE with to tag sent in order to clean up
                     * the call at the called party
                     */
                    ccb->sip_to = strlib_update(ccb->sip_to, to);
                    ccb->sip_from = strlib_update(ccb->sip_from, from);
                    contact = sippmh_get_cached_header_val(response, CONTACT);
                    if (contact) {
                        if (ccb->contact_info) {
                            sippmh_free_contact(ccb->contact_info);
                        }
                        ccb->contact_info = sippmh_parse_contact(contact);
                    }
                    sipSPISendFailureResponseAck(ccb, response, FALSE, 0xFF);
                    sipSPISendBye(ccb, NULL, NULL);
                } else {
                    /*
                     * No CANCEL was sent, so this is a plain mismatch
                     * of the CSeq #
                     */
                    CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Ignoring response message\n",
                                        DEB_F_PREFIX_ARGS(SIP_RESP, fname));
                    return (SIP_MESSAGING_ERROR);
                }
                break;
            case codeClass4xx:
            case codeClass5xx:
            case codeClass6xx:
                /*
                 * Locate the branch Parameter. If the branch parameter is
                 * the same, we are dealing with an outstanding transaction,
                 * but the response was out of sync with expected. If branch
                 * parameter is different, the transaction is gone. Ignore
                 * stray responses
                 */
                resp_via = sippmh_get_cached_header_val(response, VIA);
                if (resp_via) {
                    resp_via_parm = sippmh_parse_via(resp_via);
                    if (resp_via_parm) {
                        /* check for branch param match for transaction */
                        if ((resp_via_parm->branch_param) &&
                            (strncmp(resp_via_parm->branch_param,
                                     VIA_BRANCH_START, 7) == 0)) {
                            trx_index_temp = get_last_request_trx_index(ccb,
                                                                        TRUE);
                            if (trx_index_temp != -1) {
                                sip_via_branch = (char *)
                                    ccb->sent_request[trx_index_temp].u.sip_via_branch;
                                if (strncmp(resp_via_parm->branch_param,
                                            // (char *) ccb->sip_via_branch,
                                            sip_via_branch, VIA_BRANCH_LENGTH) != 0) {
                                    CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Stray Response: "
                                            "Response branch: %s Request "
                                            "branch: %s\n", DEB_F_PREFIX_ARGS(SIP_RESP, fname),
                                            resp_via_parm->branch_param,
                                            // (char *) ccb->sip_via_branch);
                                            sip_via_branch);
                                    sippmh_free_via(resp_via_parm);
                                    return (SIP_MESSAGING_ERROR_STALE_RESP);
                                }
                            }
                        }
                        sippmh_free_via(resp_via_parm);
                    }
                }
                sipSPISendFailureResponseAck(ccb, response, FALSE, 0xFF);
                break;
            default:
                sipSPISendFailureResponseAck(ccb, response, FALSE, 0xFF);
            }
        }
        return (SIP_MESSAGING_ERROR_STALE_RESP);
    }
    CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Response match: callid=%s, cseq=%u, "
                        "cseq_method=%s\n", DEB_F_PREFIX_ARGS(SIP_RESP, fname), 
                        callID, response_cseq_number, sipGetMethodString(response_method));


    return (SIP_MESSAGING_OK);
}

/*
 * Generate Authorization response
 * Concats all the right strings and MD5s the result
 */
boolean
sipSPIGenerateAuthorizationResponse (sip_authen_t *sip_authen,
                                    const char *uri,
                                    const char *method,
                                    const char *user_name,
                                    const char *user_password,
                                    char **author_str,
                                    int *nc_count,
                                    ccsipCCB_t *ccb)
{
    static const char fname[] = "sipSPIGenerateAuthorizationResponse";
    sip_author_t    sip_author;
    HASHHEX         HA1;
    HASHHEX         HA2 = "";
    char            cnonce_str[NONCE_LEN];
    char            nc_count_str[NONCE_LEN];
    boolean         md5_sess_used;
    uint32_t        i;

    md5_sess_used = (cpr_strcasecmp(sip_authen->algorithm, "md5-sess") == 0) ?
        TRUE : FALSE;

    if ((sip_authen->scheme != SIP_DIGEST) ||
        (!md5_sess_used &&
         (cpr_strcasecmp(sip_authen->algorithm, "md5") != 0))) {
        CCSIP_DEBUG_ERROR("%s: Error: invalid XXX-Authenticate.\n", fname);
        CCSIP_DEBUG_ERROR("  scheme: %d, algorithm: %s.\n", sip_authen->scheme,
                          sip_authen->algorithm);
        return (FALSE);
    }

    sip_author.response = (char *) cpr_malloc(33 * sizeof(char));

    /* sip_author has pointers that point to the same data that sip_authen
     * points to. Ensure that sip_authen is not freed before sip_author
     * is finished.
     */

    sip_author.str_start = NULL;
    sip_author.user_pass = (char *) user_password;
    sip_author.d_username   = (char *) user_name;
    sip_author.unparsed_uri = (char *) uri;
    sip_author.scheme    = sip_authen->scheme;
    sip_author.realm     = sip_authen->realm;
    sip_author.nonce     = sip_authen->nonce;
    sip_author.algorithm = sip_authen->algorithm;

    sip_author.opaque    = sip_authen->opaque;
    sip_author.qop       = sip_authen->qop;

    /*
     * Setup the cnonce and nc_count if qop options are used or if
     * md5-sess is the algorithm requested.
     */
    if (!md5_sess_used && (sip_authen->qop == NULL)) {
        sip_author.cnonce = NULL;
        sip_author.nc_count = NULL;
    } else {
        if (md5_sess_used && (ccb != NULL)) {
            if (ccb->authen.cnonce[0] == '\0') {
                snprintf(ccb->authen.cnonce, NONCE_LEN, "%8.8x",
                         (unsigned int)cpr_rand());
            }
            sip_author.cnonce = ccb->authen.cnonce;
        } else {
            snprintf(cnonce_str, NONCE_LEN, "%8.8x",
                     (unsigned int)cpr_rand());
            sip_author.cnonce = cnonce_str;
        }
        snprintf(nc_count_str, NONCE_LEN, "%08x", ++(*nc_count));
        sip_author.nc_count = nc_count_str;
    }
    sip_author.auth_param = NULL;

    DigestCalcHA1(sip_author.algorithm, sip_author.d_username,
                  sip_author.realm, (char *) user_password, sip_author.nonce,
                  sip_author.cnonce, HA1);

    /*
     * Need to calculate HEntity.
     * HEntity is basically just the hash of the SDP. Only the INVITE has SDP
     * so we hash the SDP for the INVITE and for other methods we just
     * hash the "".
     */
    if ((strcmp(method, SIP_METHOD_INVITE) == 0) && (ccb != NULL)) {
        /* Use the content from the body saved */
        for (i = 0; i < ccb->local_msg_body.num_parts; i++) {
            if (ccb->local_msg_body.parts[i].body != NULL) {
                DigestString(ccb->local_msg_body.parts[i].body, HA2);
                AUTH_DEBUG(DEB_F_PREFIX"entity body= \n", DEB_F_PREFIX_ARGS(SIP_MSG, fname));
            }
        }
    } else {
        DigestString("", HA2);
    }

    DigestCalcResponse(HA1, sip_author.nonce, sip_author.nc_count,
                       sip_author.cnonce, sip_author.qop,
                       (char *)method, //SIP_METHOD_REGISTER,
                       sip_author.unparsed_uri, HA2, sip_author.response);

    *author_str = sippmh_generate_authorization(&sip_author);

    cpr_free(sip_author.response);

    return (TRUE);
}


/*
 * Generates Route Headers for UAC
 * Pops the first RR entry and puts in Req Line
 * Copies the rest of RR entries in reverse order into Route Header
 * Appends Contact Header at the end of Route
 * header
 * If the first route entry has loose routing,"lr", set. does not add the
 * Contact Header to the end of Route, and does not pop the first entry.
 * In this case, indicates loose
 * routing to caller so that they may add the Contact to the Req-URI
 * instead
 */
boolean
sipSPIGenerateRouteHeaderUAC (sipRecordRoute_t *rr_info,
                              char *route,
                              int route_str_len,
                              boolean *loose_routing)
{
    boolean     retval = FALSE;
    int         i, j, start, limit;
    static char temp_route[MAX_SIP_HEADER_LENGTH];
    sipUrl_t   *url_info = NULL;
    genUrl_t   *gen;
    boolean     lr = FALSE;
    char        url[SIPS_URL_LEN];

    if (route == NULL) {
        return retval;
    }

    start = rr_info->num_locations - 1;
    limit = 0;
    route[0] = '\0';

    for (i = start; i >= limit; i--) {
        url_info = rr_info->locations[i]->genUrl->u.sipUrl;
        if (i == start) {
            if (url_info->lr_flag == FALSE) {
                // Skip the first entry if lr flag is not set
                continue;
            } else {
                lr = TRUE;
            }
        }
        if (rr_info->locations[i]->genUrl->sips) {
            snprintf(url, sizeof(url), "sips");
        } else {
            snprintf(url, sizeof(url), "sip");
        }
        temp_route[0] = '\0';
        if (url_info->user == NULL) {
            snprintf(temp_route, sizeof(temp_route),
                     "<%s:%s:%d", url, url_info->host, url_info->port);
        } else {
            if (url_info->password) {
                snprintf(temp_route, sizeof(temp_route), "<%s:%s:%s@%s:%d",
                         url, url_info->user, url_info->password,
                         url_info->host, url_info->port);
            } else {
                snprintf(temp_route, sizeof(temp_route), "<%s:%s@%s:%d",
                         url, url_info->user, url_info->host, url_info->port);
            }
        }
        if (url_info->maddr) {
            /*static */ char maddr[MAX_SIP_HEADER_LENGTH];

            snprintf(maddr, sizeof(maddr), ";maddr=%s", url_info->maddr);
            strncat(temp_route, maddr,
                    sizeof(temp_route) - strlen(temp_route) - 1);
        }

        if (url_info->ttl_val) {
            /*static */ char ttl[MAX_SIP_HEADER_LENGTH];

            snprintf(ttl, sizeof(ttl), ";ttl=%d", url_info->ttl_val);
            strncat(temp_route, ttl,
                    sizeof(temp_route) - strlen(temp_route) - 1);
        }

        switch (url_info->transport) {
        case TRANSPORT_UDP:
            strncat(temp_route, ";transport=udp",
                    sizeof(temp_route) - strlen(temp_route) - 1);
            break;
        case TRANSPORT_TCP:
            strncat(temp_route, ";transport=tcp",
                    sizeof(temp_route) - strlen(temp_route) - 1);
            break;
        case TRANSPORT_TLS:
            strncat(temp_route, ";transport=tls",
                    sizeof(temp_route) - strlen(temp_route) - 1);
            break;
        case TRANSPORT_SCTP:
            strncat(temp_route, ";transport=sctp",
                    sizeof(temp_route) - strlen(temp_route) - 1);
            break;
        }

        if (url_info->is_phone) {
            strncat(temp_route, ";user=phone",
                    sizeof(temp_route) - strlen(temp_route) - 1);
        }

        if (url_info->lr_flag) {
            strncat(temp_route, ";lr",
                    sizeof(temp_route) - strlen(temp_route) - 1);
        }

        j = 0;
        gen = rr_info->locations[i]->genUrl;

        while (j < SIP_MAX_LOCATIONS) {
            if (gen->other_params[j] != NULL) {
                strncat(temp_route, ";",
                        sizeof(temp_route) - strlen(temp_route) - 1);
                strncat(temp_route, gen->other_params[j],
                        sizeof(temp_route) - strlen(temp_route) - 1);
                break;
            }
            j++;
        }

        if (i > limit) {
            strncat(temp_route, ">,",
                    sizeof(temp_route) - strlen(temp_route) - 1);
        } else {
            strncat(temp_route, ">",
                    sizeof(temp_route) - strlen(temp_route) - 1);
        }

        strncat(route, temp_route, route_str_len - strlen(route) - 1);

    }

    *loose_routing = lr;
    retval = TRUE;
    return retval;
}

/*
 * Generates Route Headers for UAS
 * Pops the first RR entry and puts in Req Line
 * Copies the rest of RR entries into Route Header
 * Appends Contact Header at the end of Route
 * header
 */
boolean
sipSPIGenerateRouteHeaderUAS (sipRecordRoute_t *rr_info,
                              char *route,
                              int route_str_len,
                              boolean *loose_routing)
{
    const char *fname = "sipSPIGenerateRouteHeaderUAS";
    boolean     retval = FALSE;
    int         i, j, start, limit;
    static char temp_route[MAX_SIP_HEADER_LENGTH];
    sipUrl_t   *url_info;
    genUrl_t   *gen;
    boolean     lr = FALSE;
    char        url[SIPS_URL_LEN];

    if (route == NULL) {
        return (retval);
    }

    start = 0;
    limit = rr_info->num_locations - 1;
    route[0] = '\0';

    for (i = start; i <= limit; i++) {
        if (rr_info->locations[i]->genUrl->schema == URL_TYPE_SIP) {
            url_info = rr_info->locations[i]->genUrl->u.sipUrl;
        } else {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_URL_ERROR), fname);
            return (FALSE);
        }
        if (i == start) {
            if (url_info->lr_flag == FALSE) {
                // don't add the first route, if the lr flag is not set
                continue;
            } else {
                lr = TRUE;
            }
        }
        if (rr_info->locations[i]->genUrl->sips) {
            snprintf(url, sizeof(url), "sips");
        } else {
            snprintf(url, sizeof(url), "sip");
        }
        temp_route[0] = '\0';
        if (url_info->user == NULL) {
            snprintf(temp_route, sizeof(temp_route),
                     "<%s:%s:%d", url, url_info->host, url_info->port);
        } else {
            if (url_info->password) {
                snprintf(temp_route, sizeof(temp_route), "<%s:%s:%s@%s:%d",
                         url, url_info->user,
                         url_info->password, url_info->host, url_info->port);
            } else {
                snprintf(temp_route, sizeof(temp_route), "<%s:%s@%s:%d",
                         url, url_info->user, url_info->host, url_info->port);
            }
        }

        if (url_info->maddr) {
            /*static */ char maddr[MAX_SIP_HEADER_LENGTH];

            snprintf(maddr, sizeof(maddr), ";maddr=%s", url_info->maddr);
            strncat(temp_route, maddr,
                    sizeof(temp_route) - strlen(temp_route) - 1);
        }

        if (url_info->ttl_val) {
            /*static */ char ttl[MAX_SIP_HEADER_LENGTH];

            snprintf(ttl, sizeof(ttl), ";ttl=%d", url_info->ttl_val);
            strncat(temp_route, ttl,
                    sizeof(temp_route) - strlen(temp_route) - 1);
        }

        switch (url_info->transport) {
        case TRANSPORT_UDP:
            strncat(temp_route, ";transport=udp",
                    sizeof(temp_route) - strlen(temp_route) - 1);
            break;
        case TRANSPORT_TCP:
            strncat(temp_route, ";transport=tcp",
                    sizeof(temp_route) - strlen(temp_route) - 1);
            break;
        case TRANSPORT_TLS:
            strncat(temp_route, ";transport=tls",
                    sizeof(temp_route) - strlen(temp_route) - 1);
            break;
        case TRANSPORT_SCTP:
            strncat(temp_route, ";transport=sctp",
                    sizeof(temp_route) - strlen(temp_route) - 1);
            break;
        }

        if (url_info->is_phone) {
            strncat(temp_route, ";user=phone",
                    sizeof(temp_route) - strlen(temp_route) - 1);
        }

        if (url_info->lr_flag) {
            strncat(temp_route, ";lr",
                    sizeof(temp_route) - strlen(temp_route) - 1);
        }

        j = 0;
        gen = rr_info->locations[i]->genUrl;

        while (j < SIP_MAX_LOCATIONS) {
            if (gen->other_params[j] != NULL) {
                strncat(temp_route, ";",
                        sizeof(temp_route) - strlen(temp_route) - 1);
                strncat(temp_route, gen->other_params[j],
                        sizeof(temp_route) - strlen(temp_route) - 1);
                break;
            }
            j++;
        }

        if (i < limit) {
            strncat(temp_route, ">,",
                    sizeof(temp_route) - strlen(temp_route) - 1);
        } else {
            strncat(temp_route, ">",
                    sizeof(temp_route) - strlen(temp_route) - 1);
        }
        strncat(route, temp_route, route_str_len - strlen(route) - 1);
    }

    *loose_routing = lr;
    retval = TRUE;
    return (retval);
}

/*
 * This function regenerates the Contact Header
 * from the parsed information stored in CCB
 */
boolean
sipSPIGenerateContactHeader (sipContact_t *contact_info,
                             char *contact,
                             int len)
{
    const char *fname = "sipSPIGenerateContactHeader";
    boolean     retval = FALSE;
    sipUrl_t   *sipUrl;

    if (contact == NULL) {
        return (retval);
    }

    if (contact_info == NULL) {
        contact[0] = '\0';
        retval = TRUE;
        return (retval);
    }

    if (contact_info->locations[0]->genUrl->schema == URL_TYPE_SIP) {
        sipUrl = contact_info->locations[0]->genUrl->u.sipUrl;
    } else {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_URL_ERROR), fname);
        return (FALSE);
    }

    if (sipUrl->user == NULL) {
        snprintf(contact, len, "<sip:%s:%d", sipUrl->host, sipUrl->port);
    } else {
        if ((sipUrl->password) && (*(sipUrl->password))) {
            snprintf(contact, len, "<sip:%s:%s@%s:%d", sipUrl->user,
                     sipUrl->password, sipUrl->host, sipUrl->port);
        } else {
            snprintf(contact, len, "<sip:%s@%s:%d", sipUrl->user,
                     sipUrl->host, sipUrl->port);
        }
    }

    /* Assume contact length is the same for all clients */

    if (sipUrl->maddr) {
        /*static */ char maddr[MAX_SIP_HEADER_LENGTH];

        snprintf(maddr, sizeof(maddr), ";maddr=%s", sipUrl->maddr);
        strncat(contact, maddr, MAX_SIP_HEADER_LENGTH - 1);
    }

    if (sipUrl->ttl_val) {
        /*static */ char ttl[MAX_SIP_HEADER_LENGTH];

        snprintf(ttl, sizeof(ttl), ";ttl=%d", sipUrl->ttl_val);
        strncat(contact, ttl, MAX_SIP_HEADER_LENGTH - 1);
    }

    switch (sipUrl->transport) {
    case TRANSPORT_UDP:
        strncat(contact, ";transport=udp", MAX_SIP_HEADER_LENGTH - 1);
        break;
    case TRANSPORT_TCP:
        strncat(contact, ";transport=tcp", MAX_SIP_HEADER_LENGTH - 1);
        break;
    case TRANSPORT_TLS:
        strncat(contact, ";transport=tls", MAX_SIP_HEADER_LENGTH - 1);
        break;
    case TRANSPORT_SCTP:
        strncat(contact, ";transport=sctp", MAX_SIP_HEADER_LENGTH - 1);
        break;
    }

    if (sipUrl->is_phone) {
        strncat(contact, ";user=phone", MAX_SIP_HEADER_LENGTH - 1);
    }

    strncat(contact, ">", MAX_SIP_HEADER_LENGTH - 1);
    retval = TRUE;
    return (retval);
}


char *
sipSPIUrlDestination (sipUrl_t *sipUrl)
{
    return ((sipUrl->maddr) ? sipUrl->maddr : sipUrl->host);
}


int
sipSPICheckContact (const char *contact)
{
    const char   *fname = "sipSPICheckContact";
    sipContact_t *contact_info = NULL;
    int           result = 0;

    contact_info = sippmh_parse_contact(contact);
    if (contact_info) {
        if (contact_info->locations[0]->genUrl->schema != URL_TYPE_SIP) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_URL_ERROR), fname);
            sippmh_free_contact(contact_info);
            return (-1);
        }

        sippmh_free_contact(contact_info);
    }

    return (result);
}


sipRet_t
sipAddDateHeader (sipMessage_t *sip_message)
{
    char       sip_date[MAX_SIP_DATE_LENGTH];
    cpr_time_t timestamp;
    struct tm  ts;

    (void) time((time_t *)&timestamp);

    (void) gmtime_r((time_t *)&timestamp, &ts);

    if (strftime(sip_date, MAX_SIP_DATE_LENGTH, "%a, %d %b %Y %H:%M:%S GMT", &ts)
        == 0) {
        /* If defined, allow for a failure if unable to create date header */
        return STATUS_FAILURE;
    }

    return sippmh_add_text_header(sip_message, SIP_HEADER_DATE, sip_date);
}

int
sipGetMessageCSeq (sipMessage_t *pMessage,
                   uint32_t *pResultCSeqNumber,
                   sipMethod_t *pResultCSeqMethod)
{
    const char *cseq = NULL;
    sipCseq_t  *sipCseq = NULL;

    cseq = sippmh_get_cached_header_val(pMessage, CSEQ);
    if (!cseq) {
        return (-1);
    }

    sipCseq = sippmh_parse_cseq(cseq);
    if (!sipCseq) {
        return (-1);
    }

    *pResultCSeqNumber = sipCseq->number;
    *pResultCSeqMethod = sipCseq->method;

    cpr_free(sipCseq);
    return (0);
}


void
sipGetMessageToTag (sipMessage_t *pMessage, char *to_tag,
                    int to_tag_max_length)
{
    const char    *to = NULL;
    sipLocation_t *to_loc = NULL;

    memset(to_tag, 0, to_tag_max_length);
    //For self generated methods (i.e. outgoing), the TO HEADER
    //may not be in the cached area of the message structue.
    to = sippmh_get_cached_header_val(pMessage, TO);
    if (!to) {
        to = sippmh_get_header_val(pMessage, SIP_HEADER_TO, SIP_C_HEADER_TO);
    }

    if (to) {
        to_loc = sippmh_parse_from_or_to((char *)to, TRUE);
        if (to_loc) {
            if (to_loc->tag) {
                sstrncpy(to_tag, sip_sm_purify_tag(to_loc->tag),
                         to_tag_max_length);
            }
            sippmh_free_location(to_loc);
        }
    }

    return;
}


boolean
sipSPISendByeAuth (sipMessage_t *pResponse,
                  sipAuthenticate_t authen,
                  cpr_ip_addr_t *dest_ipaddr,
                  uint16_t dest_port,
                  uint32_t cseq_number,
                  char *alsoString,
                  char *last_call_route,
                  char *last_call_route_request_uri,
                  line_t previous_call_line)
{
    const char     *fname = "sipSPISendByeAuth";
    sipMessage_t   *request = NULL;
    sipRet_t        flag = STATUS_SUCCESS;
    sipRet_t        tflag = STATUS_SUCCESS;
    const char     *response_contact = NULL;
    const char     *response_record_route = NULL;
    const char     *response_to = NULL;
    const char     *response_from = NULL;
    const char     *response_callid = NULL;
    sipContact_t   *response_contact_info = NULL;
    sipRecordRoute_t *response_record_route_info = NULL;

    cpr_ip_addr_t   request_uri_addr;
    uint16_t        request_uri_port = 0;
    cpr_ip_addr_t  src_ipaddr;
    char            src_addr_str[MAX_IPADDR_STR_LEN];
    static char     ReqURI[MAX_SIP_URL_LENGTH];
    static char     via[SIP_MAX_VIA_LENGTH];
    ccsipCCB_t     *ccb = NULL;
    int             timeout = 0;
    sipLocation_t  *request_uri_loc = NULL;
    sipUrl_t       *request_uri_url = NULL;
    sipUrl_t       *sipUrl = NULL;
    int             i = 0;
    int             nat_enable = 0;

    CPR_IP_ADDR_INIT(request_uri_addr);
    CPR_IP_ADDR_INIT(src_ipaddr);

    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_REQUEST),
                      fname, "BYE(Auth)");

    request = GET_SIP_MESSAGE();
    if (!request) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "GET_SIP_MESSAGE()");
        return (FALSE);
    }

    /* Init temp ccb to ccbs[previous_call_line] */
    ccb = &gGlobInfo.ccbs[previous_call_line];
    if (ccb->state != SIP_STATE_IDLE_MSG_TIMER_OUTSTANDING) {
        CCSIP_DEBUG_ERROR("%s: Error: ccb %d is not in "
                          "SIP_STATE_IDLE_MSG_TIMER_OUTSTANDING state.\n",
                          fname, previous_call_line);
        free_sip_message(request);
        return (FALSE);
    }
    ccb->contact_info = NULL;
    ccb->record_route_info = NULL;
    ccb->flags = 0;

    response_contact = sippmh_get_cached_header_val(pResponse, CONTACT);
    response_record_route =
        sippmh_get_cached_header_val(pResponse, RECORD_ROUTE);
    response_to = sippmh_get_cached_header_val(pResponse, TO);
    response_from = sippmh_get_cached_header_val(pResponse, FROM);
    response_callid = sippmh_get_cached_header_val(pResponse, CALLID);

    if (response_contact) {
        response_contact_info = sippmh_parse_contact(response_contact);
        ccb->contact_info = response_contact_info;
    }
    if (response_record_route) {
        response_record_route_info =
            sippmh_parse_record_route(response_record_route);
        ccb->record_route_info = response_record_route_info;
    }

    /*
     * Determine the Request-URI
     */
    memset(ReqURI, 0, sizeof(ReqURI));
    if (response_record_route_info) {
        CCSIP_DEBUG_STATE(DEB_F_PREFIX"Forming Req-URI: using 401/407's Record-Route\n",
                          DEB_F_PREFIX_ARGS(SIP_REQ_URI, fname));
        i = response_record_route_info->num_locations - 1;
        if (response_record_route_info->locations[i]->genUrl->schema
                == URL_TYPE_SIP) {
            sipUrl = response_record_route_info->locations[i]->genUrl->u.sipUrl;
        } else {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_URL_ERROR), fname);
            if (ccb->record_route_info) {
                sippmh_free_record_route(ccb->record_route_info);
                ccb->record_route_info = NULL;
            }
            free_sip_message(request);
            return (FALSE);
        }

        request_uri_port = sipUrl->port;
        if (!sipUrl->port_present) {
            dns_error_code =
                sipTransportGetServerAddrPort(sipSPIUrlDestination(sipUrl),
                                              &request_uri_addr,
                                              &request_uri_port,
                                              NULL, FALSE);
        } else {
            dns_error_code = dnsGetHostByName(sipSPIUrlDestination(sipUrl),
                                               &request_uri_addr, 100, 1);
        }
        if (dns_error_code == 0) {
            util_ntohl(&request_uri_addr, &request_uri_addr);

        } else {
            request_uri_addr = ip_addr_invalid;
        }

        if (sipUrl->user != NULL) {
            if (sipUrl->password) {
                snprintf(ReqURI, sizeof(ReqURI),
                         sipUrl->is_phone ? "sip:%s:%s@%s:%d;user=phone" :
                         "sip:%s:%s@%s:%d",
                         sipUrl->user, sipUrl->password,
                         sipUrl->host, sipUrl->port);
            } else {
                snprintf(ReqURI, sizeof(ReqURI),
                         sipUrl->is_phone ? "sip:%s@%s:%d;user=phone" :
                         "sip:%s@%s:%d", sipUrl->user, sipUrl->host,
                         sipUrl->port);
            }
        } else {
            snprintf(ReqURI, sizeof(ReqURI),
                     sipUrl->is_phone ? "sip:%s:%d;user=phone" : "sip:%s:%d",
                     sipUrl->host, sipUrl->port);
        }
    } else if (last_call_route[0] && last_call_route_request_uri[0]) {
        CCSIP_DEBUG_STATE(DEB_F_PREFIX"Forming Req-URI: using current Route "
                          "information.\n", DEB_F_PREFIX_ARGS(SIP_REQ_URI, fname));
        sstrncpy(ReqURI, last_call_route_request_uri, sizeof(ReqURI));
        request_uri_addr = *dest_ipaddr;
        request_uri_port = dest_port;
    } else if (response_contact_info) {
        CCSIP_DEBUG_STATE(DEB_F_PREFIX"Forming Req-URI: using Contact\n", DEB_F_PREFIX_ARGS(SIP_REQ_URI, fname));
        if (response_contact_info->locations[0]->genUrl->schema == URL_TYPE_SIP) {
            sipUrl = response_contact_info->locations[0]->genUrl->u.sipUrl;
        } else {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_URL_ERROR), fname);
            if (ccb->contact_info) {
                sippmh_free_contact(ccb->contact_info);
                ccb->contact_info = NULL;
            }
            free_sip_message(request);
            return (FALSE);
        }

        request_uri_port = sipUrl->port;
        if (!sipUrl->port_present) {
            dns_error_code =
                sipTransportGetServerAddrPort(sipSPIUrlDestination(sipUrl),
                                              &request_uri_addr,
                                              &request_uri_port,
                                              NULL, FALSE);
        } else {
            dns_error_code = dnsGetHostByName(sipSPIUrlDestination(sipUrl),
                                               &request_uri_addr, 100, 1);
        }
        if (dns_error_code == 0) {

            util_ntohl(&request_uri_addr, &request_uri_addr);
        } else {
            request_uri_addr = ip_addr_invalid;
        }

        if (sipUrl->user != NULL) {
            if (sipUrl->password) {
                snprintf(ReqURI, sizeof(ReqURI),
                         sipUrl->is_phone ? "sip:%s:%s@%s:%d;user=phone" :
                         "sip:%s:%s@%s:%d",
                         sipUrl->user, sipUrl->password,
                         sipUrl->host, sipUrl->port);
            } else {
                snprintf(ReqURI, sizeof(ReqURI),
                         sipUrl->is_phone ? "sip:%s@%s:%d;user=phone" :
                         "sip:%s@%s:%d", sipUrl->user,
                         sipUrl->host, sipUrl->port);
            }
        } else {
            snprintf(ReqURI, sizeof(ReqURI),
                     sipUrl->is_phone ? "sip:%s:%d;user=phone" : "sip:%s:%d",
                     sipUrl->host, sipUrl->port);
        }
    } else {
        request_uri_addr = *dest_ipaddr;
        request_uri_port = dest_port;
        request_uri_loc = sippmh_parse_from_or_to((char *)response_to, TRUE);
        if (!request_uri_loc) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname,
                              get_debug_string(DEBUG_FUNCTIONNAME_SIPPMH_PARSE_TO));
            free_sip_message(request);
            return (FALSE);
        }

        if (!sippmh_valid_url(request_uri_loc->genUrl)) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sippmh_valid_url()");
            sippmh_free_location(request_uri_loc);
            free_sip_message(request);
            return (FALSE);
        }

        CCSIP_DEBUG_STATE(DEB_F_PREFIX"Forming Req-URI (Caller): using original "
                          "Req-URI\n", DEB_F_PREFIX_ARGS(SIP_REQ_URI, fname));
        if (request_uri_loc->name) {
            if (request_uri_loc->name[0]) {
                strncat(ReqURI, "\"", sizeof(ReqURI) - strlen(ReqURI) - 1);
                strncat(ReqURI, request_uri_loc->name,
                        sizeof(ReqURI) - strlen(ReqURI) - 1);
                strncat(ReqURI, "\" ", sizeof(ReqURI) - strlen(ReqURI) - 1);
            }
        }
        strncat(ReqURI, "sip:", sizeof(ReqURI) - strlen(ReqURI) - 1);
        if (request_uri_loc->genUrl->schema == URL_TYPE_SIP) {
            request_uri_url = request_uri_loc->genUrl->u.sipUrl;
        } else {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_URL_ERROR), fname);
            sippmh_free_location(request_uri_loc);
            free_sip_message(request);
            return (FALSE);
        }

        if (request_uri_url->user) {
            strncat(ReqURI, request_uri_url->user,
                    sizeof(ReqURI) - strlen(ReqURI) - 1);
            strncat(ReqURI, "@", sizeof(ReqURI) - strlen(ReqURI) - 1);
        }
        if (request_uri_url->is_phone) {
            strncat(ReqURI, ";user=phone",
                    sizeof(ReqURI) - strlen(ReqURI) - 1);
        }
        strncat(ReqURI, request_uri_url->host,
                sizeof(ReqURI) - strlen(ReqURI) - 1);
        sippmh_free_location(request_uri_loc);
    }

    config_get_value(CFGID_NAT_ENABLE, &nat_enable, sizeof(nat_enable));
    if (nat_enable == 0) {
        sip_config_get_net_device_ipaddr(&src_ipaddr);
        ipaddr2dotted(src_addr_str, &src_ipaddr);
    } else {
        sip_config_get_nat_ipaddr(&src_ipaddr);
        ipaddr2dotted(src_addr_str, &src_ipaddr);
    }

    /*
     * Build the request
     */

    /* Write Req-URI */
    tflag = sippmh_add_request_line(request, SIP_METHOD_BYE, ReqURI,
                                    SIP_VERSION);
    UPDATE_FLAGS(flag, tflag);

    /* Write Via */
    snprintf(via, sizeof(via), "SIP/2.0/%s %s:%d;%s=%s%.8x",
             sipTransportGetTransportType(ccb->dn_line, TRUE, ccb),
             src_addr_str,
             ccb->local_port,
             VIA_BRANCH, VIA_BRANCH_START,
             (unsigned int)cpr_rand());
    tflag = sippmh_add_text_header(request, SIP_HEADER_VIA, via);
    UPDATE_FLAGS(flag, tflag);

    /* Write To, From, and Call-ID standard headers */
    tflag = sippmh_add_text_header(request, SIP_HEADER_FROM, response_from);
    UPDATE_FLAGS(flag, tflag);
    tflag = sippmh_add_text_header(request, SIP_HEADER_TO, response_to);
    UPDATE_FLAGS(flag, tflag);
    tflag = sippmh_add_text_header(request, SIP_HEADER_CALLID, response_callid);
    UPDATE_FLAGS(flag, tflag);

    /* Write Date header */
    tflag = sipAddDateHeader(request);
    UPDATE_FLAGS(flag, tflag);

    /* Write User Agent header */
    tflag = sippmh_add_text_header(request, SIP_HEADER_USER_AGENT,
                                   sipHeaderUserAgent);
    UPDATE_FLAGS(flag, tflag);

    /* add in call stats header if needed */
    tflag = sipSPIAddCallStats(ccb, request);
    UPDATE_FLAGS(flag, tflag);

    /* Write Route */
    if (response_record_route_info) {
        tflag = (sipSPIAddRouteHeaders(request, ccb, NULL, 0)) ?
            STATUS_SUCCESS : STATUS_FAILURE;
    } else if (last_call_route[0]) {
        tflag = sippmh_add_text_header(request, SIP_HEADER_ROUTE,
                                       last_call_route);
    }
    UPDATE_FLAGS(flag, tflag);

    /* Free contact and record-route */
    if (response_contact) {
        if (ccb->contact_info) {
            sippmh_free_contact(ccb->contact_info);
            ccb->contact_info = NULL;
        }
    }
    if (response_record_route) {
        if (ccb->record_route_info) {
            sippmh_free_record_route(ccb->record_route_info);
            ccb->record_route_info = NULL;
        }
    }

    /* Write CSeq */
    tflag = sippmh_add_cseq(request, SIP_METHOD_BYE, cseq_number);
    UPDATE_FLAGS(flag, tflag);

    if (alsoString) {
        if (alsoString[0]) {
            tflag = sippmh_add_text_header(request, SIP_HEADER_ALSO,
                                           alsoString);
            UPDATE_FLAGS(flag, tflag);
        }
    }

    if (authen.authorization != NULL) {
        tflag = sippmh_add_text_header(request, AUTHOR_HDR(authen.status_code),
                                       authen.authorization);
        UPDATE_FLAGS(flag, tflag);
        cpr_free(authen.authorization);
    }

    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (request)
            free_sip_message(request);
        return (FALSE);
    }

    /* Send message */
    config_get_value(CFGID_TIMER_T1, &timeout, sizeof(timeout));
    ccb->retx_counter = 0;
    if (sipTransportChannelCreateSend(ccb, request, sipMethodBye,
                                      &request_uri_addr,
                                      request_uri_port, timeout,
                                      RELDEV_NO_STORED_MSG) < 0) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "sipTransportChannelCreateSend()");
        if (request)
            free_sip_message(request);

        return (FALSE);
    }
    return (TRUE);
}

void
free_sip_message (sipMessage_t *message)
{
    if (message) {
        sippmh_message_free(message);
    }
}

/*
 * Generate the full URL (currently limited to the sipUrl only)
 */
void
sipSPIGenerateTargetUrl (genUrl_t *genUrl, char *sipurlstr)
{
    uint8_t i;
    char    temp[MAX_SIP_HEADER_LENGTH];
    size_t  url_length = 0;
    char   *right_bracket = NULL;
    boolean right_bracket_removed = FALSE;

    if (genUrl->schema == URL_TYPE_SIP) {
        sipSPIGenerateSipUrl(genUrl->u.sipUrl, sipurlstr);
    } else {
        return;
    }

    url_length = strlen(sipurlstr);
    if (url_length == 0) {
        return;
    }

    for (i = 0; i < SIP_MAX_LOCATIONS; i++) {
        if (genUrl->other_params[i] != NULL) {
            if (i == 0) {
                // Remove the ">" from the end of sipurlstr
                right_bracket = strchr(sipurlstr, '>');
                if (right_bracket) {
                    *right_bracket = '\0';
                    right_bracket_removed = TRUE;
                }
            }
            snprintf(temp, sizeof(temp), ";%s", genUrl->other_params[i]);
            strncat(sipurlstr, temp, MAX_SIP_URL_LENGTH - url_length - 1);
            url_length = strlen(sipurlstr);
        }
    }

    if (right_bracket_removed) {
        strncat(sipurlstr, ">", MAX_SIP_URL_LENGTH - url_length - 1);
    }
}

/*
 * Generates a plain string from sipUrl structure
 *
*/
void
sipSPIGenerateSipUrl (sipUrl_t *sipUrl, char *sipurlstr)
{
    char temp[MAX_SIP_HEADER_LENGTH];

    if (sipUrl->user == NULL) {
        snprintf(sipurlstr, MAX_SIP_HEADER_LENGTH, "<sip:%s:%d",
                 sipUrl->host, sipUrl->port);
    } else {
        snprintf(sipurlstr, MAX_SIP_HEADER_LENGTH, "<sip:%s@%s:%d",
                 sipUrl->user, sipUrl->host, sipUrl->port);
    }

    /* Assume sipurlstr _length is the same for all clients */

    if (sipUrl->maddr) {
        snprintf(temp, sizeof(temp), ";maddr=%s", sipUrl->maddr);
        strncat(sipurlstr, temp, MAX_SIP_HEADER_LENGTH - 1);
    }

    if (sipUrl->ttl_val) {
        snprintf(temp, sizeof(temp), ";ttl=%d", sipUrl->ttl_val);
        strncat(sipurlstr, temp, MAX_SIP_HEADER_LENGTH - 1);
    }

    switch (sipUrl->transport) {
    case TRANSPORT_UDP:
        strncat(sipurlstr, ";transport=udp", MAX_SIP_HEADER_LENGTH - 1);
        break;
    case TRANSPORT_TCP:
        strncat(sipurlstr, ";transport=tcp", MAX_SIP_HEADER_LENGTH - 1);
        break;
    case TRANSPORT_TLS:
        strncat(sipurlstr, ";transport=tls", MAX_SIP_HEADER_LENGTH - 1);
        break;
    case TRANSPORT_SCTP:
        strncat(sipurlstr, ";transport=sctp", MAX_SIP_HEADER_LENGTH - 1);
        break;
    }

    if (sipUrl->is_phone) {
        strncat(sipurlstr, ";user=phone", MAX_SIP_HEADER_LENGTH - 1);
    }

    strncat(sipurlstr, ">", MAX_SIP_HEADER_LENGTH - 1);

}

#define MAX_SIP_METHOD_STRINGS 17
#define MAX_SIP_METHOD_STRING_LEN 16
const char *
sipGetMethodString (sipMethod_t methodname /*, char *methodstring */)
{
    int ino = (int) methodname; //Register is defined as 100 in pmh.h file
    //Please make sure the following array is consistent with the enum
    int idx = 0;
    static const char methods[MAX_SIP_METHOD_STRINGS][MAX_SIP_METHOD_STRING_LEN] =
        { "REGISTER", "OPTIONS", "INVITE", "BYE",
        "CANCEL", "PRACK", "COMET", "NOTIFY",
        "REFER", "ACK", "MESSAGE", "SUBSCRIBE",
        "PUBLISH", "UPDATE", "RESPONSE", "INFO", "UNKNOWN"
    };


    idx = ino - (int) sipMethodRegister;
    if (idx >= 0 && idx <= (int) (sizeof(methods) / sizeof(methods[0]) - 1)) {
        return methods[idx]; // Its OK to send this as it is a static array
    } else {
        return NULL;
    }
}

sipRet_t
sipSPIAddRequestLine (ccsipCCB_t *ccb, sipMessage_t *request,
                      sipMethod_t methodname, boolean initInvite)
{
    sipRet_t tflag = STATUS_FAILURE;

    if (TRUE == sipSPIGenRequestURI(ccb, methodname, initInvite)) {
        tflag = sippmh_add_request_line(request,
                                        sipGetMethodString(methodname),
                                        ccb->ReqURI,
                                        SIP_VERSION);
    } else {
        tflag = STATUS_FAILURE;
    }
    return tflag;
}

boolean
getCSeqInfo (sipMessage_t *request, sipCseq_t ** request_cseq_structure)
{
    const char *request_cseq = NULL;

    request_cseq = sippmh_get_cached_header_val(request, CSEQ);
    if (!request_cseq) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          "AddCSeq in Factory",
                          "sippmh_get_cached_header_val()");
        return (FALSE);
    }
    *request_cseq_structure = sippmh_parse_cseq(request_cseq);
    if (!*request_cseq_structure) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          "AddCSeq in Factory",
                          "sippmh_parse_cseq()");
        return (FALSE);
    }
    return TRUE;
}

// Allocate an outgoing transaction and fill in the cseq number and method
boolean
allocateTrx (ccsipCCB_t *ccb, sipMethod_t method)
{
    uint32_t trx_cseq_number = 0;
    int16_t  new_trx_index = -1, last_trx_index = -1;

    if (method == sipMethodCancel) {
        // For CANCEL, the cseq is the same as the cseq
        // of the earlier sent request that it cancels
        last_trx_index = get_last_request_trx_index(ccb, TRUE);
        if (last_trx_index < 0) {
            return FALSE;
        }
        trx_cseq_number = ccb->sent_request[last_trx_index].cseq_number;
        // Now allocate a new CSeq for the CANCEL request itself
        new_trx_index = get_next_request_trx_index(ccb, TRUE);
        if (new_trx_index < 0) {
            return FALSE;
        }
        ccb->sent_request[new_trx_index].cseq_number = trx_cseq_number;
        ccb->sent_request[new_trx_index].cseq_method = sipMethodCancel;

    } else if (method != sipMethodAck) {
        // No new transaction is needed for Ack
        new_trx_index = get_next_request_trx_index(ccb, TRUE);
        if (new_trx_index < 0) {
            return FALSE;
        }
        ccb->sent_request[new_trx_index].cseq_number = ++(ccb->last_used_cseq);
        ccb->sent_request[new_trx_index].cseq_method = method;
    }
    return TRUE;
}

boolean
AddCSeq (ccsipCCB_t *ccb,
         sipMessage_t *request,
         boolean isResponse,
         sipMethod_t method,
         uint32_t response_cseq_number)
{
    uint32_t request_cseq_number = 0;
    sipRet_t tflag = STATUS_FAILURE;
    int16_t  trx_index = -1;

    if (TRUE == isResponse) {
        if (response_cseq_number == 0) {
            trx_index = get_method_request_trx_index(ccb, method, FALSE);
            if (trx_index != -1) {
                request_cseq_number = ccb->recv_request[trx_index].cseq_number;
            } else {
                return FALSE;
            }
        } else {
            request_cseq_number = response_cseq_number;
        }
    } else {
        // For Request
        if (method == sipMethodAck) {
            request_cseq_number = response_cseq_number;
        } else {
            // Pull up the previously allocated transaction
            trx_index = get_last_request_trx_index(ccb, TRUE);
            if (trx_index < 0) {
                return FALSE;
            }
            request_cseq_number = ccb->sent_request[trx_index].cseq_number;
        }
    }
    tflag = sippmh_add_cseq(request, sipGetMethodString(method),
                            request_cseq_number);
    if (tflag != HSTATUS_SUCCESS) {
        return FALSE;
    }
    return TRUE;
}

/*
 *   This will generate the most common headers for every message
 */

sipRet_t
sipSPIAddCommonHeaders (ccsipCCB_t *ccb,
                        sipMessage_t *request,
                        boolean isResponse,
                        sipMethod_t method,
                        uint32_t response_cseq_number)
{
    sipRet_t tflag = STATUS_FAILURE;

    tflag = (sipSPIAddStdHeaders(request, ccb, isResponse)) ?
        STATUS_SUCCESS : STATUS_FAILURE;
    if (tflag != HSTATUS_SUCCESS) {
        return tflag;
    }

    // Add date header
    tflag = sipAddDateHeader(request);
    if (tflag != HSTATUS_SUCCESS) {
        return tflag;
    }

    // Add CSEQ
    if (AddCSeq(ccb, request, isResponse, method, response_cseq_number)
            == FALSE) {
        return STATUS_FAILURE;
    }

    return HSTATUS_SUCCESS;
}

boolean
is_extended_feature (ccsipCCB_t *ccb)
{
    if (ccb) {
        switch (ccb->featuretype) {
        case CC_FEATURE_B2BCONF:
        case CC_FEATURE_CANCEL: 
            return TRUE;
        default:
            return FALSE;
        }
    }
    return FALSE;
}

boolean
sipSPIGenRequestURI (ccsipCCB_t *ccb, sipMethod_t sipmethod, boolean initInvite)
{
    sipUrl_t   *sipUrl = NULL;
    int         i = 0;
    const char *fname = "sipSPIGenRequestURI";
    char        dest_sip_addr_str[MAX_IPADDR_STR_LEN];
    char       *domainloc;
    genUrl_t   *gen;
    int         j = 0;
    boolean     lr = FALSE, uriAdded = FALSE;
    char        hdr_str[MAX_SIP_URL_LENGTH];

    /*
     * Construct the request URI
     */
    // Special case for Initial Invite and Cancel messgae for creating
    // the Request URI
    if ((sipMethodInvite == sipmethod) && (initInvite == TRUE)) {
        // This is the first Invite initiated so Request URI is
        // built differently.
        if (ccb->calledNumber[0] == '<') {
            // Remove leading '<'
            sstrncpy(ccb->ReqURI, ccb->calledNumber + 1, MAX_SIP_URL_LENGTH);
        }
        // If there is no hostname, add proxy address as hostname
        domainloc = strchr(ccb->ReqURI, '@');
        if (domainloc == NULL) {
            domainloc = ccb->ReqURI + strlen(ccb->ReqURI);
            if ((domainloc - ccb->ReqURI) < (MAX_SIP_URL_LENGTH - 1)) {
                /*
                 * We need to check and see if we are already truncating a
                 * string string that goes into ReqURI.  If we are, then we
                 * CANNOT add any more characters without overwriting memory
                 */
                *domainloc++ = '@';
                sstrncpy(dest_sip_addr_str, ccb->reg.proxy,
                        MAX_IPADDR_STR_LEN);

                if (ccb->reg.addr.type == CPR_IP_ADDR_IPV6) {                
                    *domainloc++ = '[';
                }

                sstrncpy(domainloc, dest_sip_addr_str,
                         MAX_SIP_URL_LENGTH - (domainloc - ccb->ReqURI));

                if (ccb->reg.addr.type == CPR_IP_ADDR_IPV6) {                
                    *domainloc++ = ']';
                }
            }
        }
        // Remove trailing '>'
        domainloc = strchr(ccb->ReqURI, '>');
        if (domainloc) {
            *domainloc = '\0';
        }
        return TRUE;
    } else if ((sipMethodCancel == sipmethod) ||
               ((sipMethodAck == sipmethod) &&
                (gCallHistory[ccb->index].last_rspcode_rcvd > codeClass2xx))) {
        /* Use the same REQ URI that was created on the INVITE
         * Replace the ReqURI with the contents of ReqURIOriginal
         */
        if (ccb->ReqURIOriginal[0] != '\0') {
            sstrncpy(ccb->ReqURI, ccb->ReqURIOriginal, MAX_SIP_URL_LENGTH);
        }
        return TRUE;
    }

    if (sipMethodRegister == sipmethod ||
        ((sipMethodRefer == sipmethod) &&
         (ccb->type == SIP_REG_CCB || is_extended_feature(ccb)))) {
        // If it is REGISTER or a token-registration REFER, the URI is formed
        // simply by the destination IP address. This is also the case when
        // sending a REFER for one of the softkey functions for TNP
        if (ccb->reg.proxy[0] == '\0') {
            ipaddr2dotted(dest_sip_addr_str, &ccb->dest_sip_addr);
        } else {
            sstrncpy(dest_sip_addr_str, ccb->reg.proxy, MAX_IPADDR_STR_LEN);
        }
        if (ccb->dest_sip_addr.type == CPR_IP_ADDR_IPV6) {

            snprintf(ccb->ReqURI, MAX_SIP_URL_LENGTH, "sip:[%s]",
                     dest_sip_addr_str); /* proxy */
        } else {
            snprintf(ccb->ReqURI, MAX_SIP_URL_LENGTH, "sip:%s",
                     dest_sip_addr_str); /* proxy */
        }

        return TRUE;
    } else {
        if (ccb->record_route_info) {
            /*
             * If loose-routing is being used, use the remote contact in
             * REQ-URI
             */
            if (ccb->flags & INCOMING) {
                i = 0;
            } else {
                i = ccb->record_route_info->num_locations - 1;
            }

            if (ccb->record_route_info->locations[i]->genUrl->schema
                    == URL_TYPE_SIP) {
                if (ccb->record_route_info->locations[i]->genUrl->u.sipUrl->lr_flag) {
                    lr = TRUE;
                }
            }
            if (!lr) {
                CCSIP_DEBUG_STATE(DEB_F_PREFIX"Strict Routing: Forming Req-URI using "
                                  "Record Route\n", DEB_F_PREFIX_ARGS(SIP_REQ_URI, fname));
                if (ccb->record_route_info->locations[i]->genUrl->schema
                        == URL_TYPE_SIP) {
                    sipUrl = ccb->record_route_info->locations[i]->genUrl->u.sipUrl;
                } else {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_URL_ERROR),
                                      fname);
                    return (FALSE);
                }

                if (sipUrl->user != NULL) {
                    if (sipUrl->password) {
                        if (sipUrl->port_present) {
                             snprintf(ccb->ReqURI, MAX_SIP_URL_LENGTH,
                                 sipUrl->is_phone ?
                                     "sip:%s:%s@%s:%d;user=phone" :
                                     "sip:%s:%s@%s:%d",
                                 sipUrl->user, sipUrl->password,
                                 sipUrl->host, sipUrl->port);
                        } else {
                             snprintf(ccb->ReqURI, MAX_SIP_URL_LENGTH,
                                 sipUrl->is_phone ?
                                     "sip:%s:%s@%s;user=phone" :
                                     "sip:%s:%s@%s",
                                 sipUrl->user, sipUrl->password,
                                 sipUrl->host);
                        }
                    } else {
                        if (sipUrl->port_present) {
                             snprintf(ccb->ReqURI, MAX_SIP_URL_LENGTH,
                                 sipUrl->is_phone ?
                                     "sip:%s@%s:%d;user=phone" : "sip:%s@%s:%d",
                                 sipUrl->user, sipUrl->host, sipUrl->port);
                        } else {
                             snprintf(ccb->ReqURI, MAX_SIP_URL_LENGTH,
                                 sipUrl->is_phone ?
                                     "sip:%s@%s;user=phone" : "sip:%s@%s",
                                 sipUrl->user, sipUrl->host);
                        }
                    }
                } else {
                    if (sipUrl->port_present) {
                       snprintf(ccb->ReqURI, MAX_SIP_URL_LENGTH,
                             sipUrl->is_phone ?
                                 "sip:%s:%d;user=phone" : "sip:%s:%d",
                             sipUrl->host, sipUrl->port);
                     } else {
                       snprintf(ccb->ReqURI, MAX_SIP_URL_LENGTH,
                             sipUrl->is_phone ?
                                 "sip:%s;user=phone" : "sip:%s",
                             sipUrl->host);
                     }
                }
                uriAdded = TRUE;
            }
        }

        if (!uriAdded) {
            if ((ccb->contact_info) &&
                (lr || (ccb->state >= SIP_STATE_SENT_INVITE_CONNECTED))) {
                // Use Contact info ONLY if loose routing or
                // if we're fully connected - otherwise use proxy
                CCSIP_DEBUG_STATE(DEB_F_PREFIX"Forming Req-URI: using Contact\n", 
                                  DEB_F_PREFIX_ARGS(SIP_REQ_URI, fname));

                if ((sipMethodInvite == sipmethod) &&
                    (ccb->redirect_info) &&
                    (ccb->state == SIP_STATE_SENT_INVITE)) {
                    sipUrl = ccb->redirect_info->sipContact->locations
                        [ccb->redirect_info->next_choice - 1]->genUrl->u.sipUrl;
                } else if (ccb->contact_info->locations[0]->genUrl->schema
                        == URL_TYPE_SIP) {
                    sipUrl = ccb->contact_info->locations[0]->genUrl->u.sipUrl;
                } else {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_URL_ERROR),
                                      fname);
                    return (FALSE);
                }
                if (sipUrl->user != NULL) {
                    if (sipUrl->password) {
                        snprintf(ccb->ReqURI, MAX_SIP_URL_LENGTH,
                                 sipUrl->is_phone ?
                                     "sip:%s:%s@%s:%d;user=phone" :
                                     "sip:%s:%s@%s:%d",
                                 sipUrl->user, sipUrl->password,
                                 sipUrl->host, sipUrl->port);
                    } else {
                        snprintf(ccb->ReqURI, MAX_SIP_URL_LENGTH,
                                 sipUrl->is_phone ?
                                     "sip:%s@%s:%d;user=phone" :
                                     "sip:%s@%s:%d",
                                 sipUrl->user, sipUrl->host, sipUrl->port);
                    }
                } else {
                    snprintf(ccb->ReqURI, MAX_SIP_URL_LENGTH,
                             sipUrl->is_phone ?
                                 "sip:%s:%d;user=phone" :
                                 "sip:%s:%d",
                             sipUrl->host, sipUrl->port);
                }
            } else {
                if (ccb->flags & INCOMING) {
                    CCSIP_DEBUG_STATE(DEB_F_PREFIX"Forming Req-URI (Callee): using "
                                      "From\n", DEB_F_PREFIX_ARGS(SIP_REQ_URI, fname));
                    if (ccb->sip_from) {
                        sstrncpy(hdr_str, ccb->sip_from, MAX_SIP_URL_LENGTH);
                        sstrncpy(ccb->ReqURI, sippmh_get_url_from_hdr(hdr_str), MAX_SIP_URL_LENGTH);
                    }
                } else {
                    CCSIP_DEBUG_STATE(DEB_F_PREFIX"Forming Req-URI (Caller): using "
                                      "original Req-URI\n", DEB_F_PREFIX_ARGS(SIP_REQ_URI, fname));
                    sstrncpy(ccb->ReqURI, ccb->ReqURIOriginal,
                            MAX_SIP_URL_LENGTH);
                }
            }
        }

        // Check to see the transport parameter specified in sipUrl.
        // If this is different than UDP, add it to the URI
        if (sipUrl) {
            switch (sipUrl->transport) {
            case TRANSPORT_UDP:
                strncat(ccb->ReqURI, ";transport=udp",
                        sizeof(ccb->ReqURI) - strlen(ccb->ReqURI) - 1);
                break;
            case TRANSPORT_TCP:
                strncat(ccb->ReqURI, ";transport=tcp",
                        sizeof(ccb->ReqURI) - strlen(ccb->ReqURI) - 1);
                break;
            case TRANSPORT_TLS:
                strncat(ccb->ReqURI, ";transport=tls",
                        sizeof(ccb->ReqURI) - strlen(ccb->ReqURI) - 1);
                break;
            case TRANSPORT_SCTP:
                strncat(ccb->ReqURI, ";transport=sctp",
                        sizeof(ccb->ReqURI) - strlen(ccb->ReqURI) - 1);
                break;
            default:
                break;
            }
        }
        if (ccb->record_route_info) {
            /*
             * Look for any unknown params that might have been
             * glued onto the URL
             */
            gen = ccb->record_route_info->locations[i]->genUrl;
            while (j < SIP_MAX_LOCATIONS) {
                if (gen->other_params[j] != NULL) {
                    strncat(ccb->ReqURI, ";",
                            sizeof(ccb->ReqURI) - strlen(ccb->ReqURI) - 1);
                    strncat(ccb->ReqURI, gen->other_params[j],
                            sizeof(ccb->ReqURI) - strlen(ccb->ReqURI) - 1);
                    break;
                }
                j++;
            }
        }
    }
    return TRUE;
}

sipRet_t
sipSPIAddContactHeader (ccsipCCB_t *ccb, sipMessage_t *request)
{
    char        pContactStr[MAX_SIP_URL_LENGTH];
    char        src_addr_str[MAX_IPADDR_STR_LEN];
    char        line_name[MAX_LINE_NAME_SIZE];
    char        reg_user_info[MAX_REG_USER_INFO_LEN];
    int         rpid_flag = RPID_DISABLED;
    int         blocking;
    uint8_t     mac_address[MAC_ADDRESS_LENGTH];
    char        device_instance[MAX_SIP_TAG_LENGTH];
    char        contact[MAX_LINE_CONTACT_SIZE];
    size_t      escaped_char_str_len;
    const char *transport_type_str;
    int size;
    char        sipDeviceName[MAX_REG_USER_INFO_LEN];

    config_get_value(CFGID_REMOTE_PARTY_ID, &rpid_flag, sizeof(rpid_flag));

    config_get_value(CFGID_DEVICE_NAME, sipDeviceName, sizeof(sipDeviceName));

    /* get reg_user_info and pk-id */
    contact[0] = '\0';
    config_get_string(CFGID_REG_USER_INFO, reg_user_info,
                      sizeof(reg_user_info));
    config_get_line_string(CFGID_LINE_CONTACT, contact, ccb->dn_line,
                           sizeof(contact));


    ipaddr2dotted(src_addr_str, &ccb->src_addr);

    config_get_value(CFGID_CALLERID_BLOCKING, &blocking, sizeof(blocking));

    transport_type_str = sipTransportGetTransportType(ccb->dn_line, FALSE, ccb);

    /*
     * If caller id blocking is enabled and phone
     * is configured to use RPID then userinfo should
     * be random identifier same as that used in from
     * header
     */
    if ((blocking & 1) && (rpid_flag == RPID_ENABLED) &&
        (ccb->type != SIP_REG_CCB)) {
        sstrncpy(line_name, SIP_HEADER_ANONYMOUS_STR, MAX_LINE_NAME_SIZE);
    } else {
        config_get_line_string(CFGID_LINE_NAME, line_name, ccb->dn_line,
                               sizeof(line_name));
    }

    if (ccb->type == SIP_REG_CCB) {
        snprintf(pContactStr, 6, "<sip:");
        if ((cpr_strcasecmp(contact, UNPROVISIONED) != 0) &&
            (contact[0] != '\0')) {
            escaped_char_str_len =
                sippmh_convertURLCharToEscChar(contact, strlen(contact),
                                               pContactStr + 5,
                                               (MAX_SIP_URL_LENGTH - 5),
                                               FALSE);
        } else {
            escaped_char_str_len =
                sippmh_convertURLCharToEscChar(line_name,
                                               strlen(line_name),
                                               pContactStr + 5,
                                               (MAX_SIP_URL_LENGTH - 5),
                                               FALSE);
        }
        if (cpr_strcasecmp(reg_user_info, "none") == 0) {
            snprintf(pContactStr + 5 + escaped_char_str_len,
                     sizeof(pContactStr) - 5 - escaped_char_str_len,
                     "@%s:%d;transport=%s>", src_addr_str,
                     ccb->local_port, transport_type_str);
        } else {
            snprintf(pContactStr + 5 + escaped_char_str_len,
                     sizeof(pContactStr) - 5 - escaped_char_str_len,
                     "@%s:%d;user=%s;transport=%s>",
                     src_addr_str, ccb->local_port, reg_user_info,
                     transport_type_str);
        }

        // Add the instance ID for unique device identification
        // The format is as follows:
        // Contact: <sip:line1@192.168.0.2>
        // ;+sip.instance="<urn:uuid:00000000-0000-0000-0000-000A95A0E128>"
        // ;+u.sip!model.ccm.cisco.com="336"
        // where the 000A95A0E128 is the MAC address of the phone and
        // the 336 is the model number of the phone
        // All the 000's are supposed to encode callee capabilities (RFC 3840)
        // but we are leaving them 000 for now as no-one reads them
        platform_get_active_mac_address(mac_address);
        memset(device_instance, '\0', sizeof(device_instance));
        snprintf(device_instance, MAX_SIP_TAG_LENGTH,
                 ";+sip.instance=\"<urn:uuid:00000000-0000-0000-0000-%.4x%.4x%.4x>\"",
                 mac_address[0] * 256 + mac_address[1],
                 mac_address[2] * 256 + mac_address[3],
                 mac_address[4] * 256 + mac_address[5]);
        size = MAX_SIP_URL_LENGTH - strlen(pContactStr);
        if (size > (int)strlen(device_instance)) {
            sstrncat(pContactStr, device_instance, size);
        }
        // Add the instance ID for unique device identification
        // The format is as follows:
        // Contact: <sip:5b1d1cd7-4e1b-99ae-665a-efdf0d96a7a2@172.18.199.196:52046;
        //transport=tcp>;+sip.instance="<urn:uuid:00000000-0000-0000-0000-0019e89a7f3e>";
        //+u.sip!devicename.ccm.cisco.com="SEP0019E89A7F3D";
        //++u.sip!model.ccm.cisco.com="30006"
        // where the 000A95A0E128 is the MAC address of the phone and
        // the 336 is the model number of the phone
        // All the 000's are supposed to encode callee capabilities (RFC 3840)
        // but we are leaving them 000 for now as no-one reads them
        platform_get_wired_mac_address(mac_address);
        memset(device_instance, '\0', sizeof(device_instance));
        snprintf(device_instance, MAX_SIP_TAG_LENGTH,
                 ";+sip.instance=\"<urn:uuid:00000000-0000-0000-0000-%.4x%.4x%.4x>\""
		 ";+u.sip!devicename.ccm.cisco.com=\"%s\""
		 ";+u.sip!model.ccm.cisco.com=\"%s\"",
                 mac_address[0] * 256 + mac_address[1],
                 mac_address[2] * 256 + mac_address[3],
                 mac_address[4] * 256 + mac_address[5],
                 sipDeviceName,
                 sipPhoneModelNumber);
        size = MAX_SIP_URL_LENGTH - strlen(pContactStr);
        if (size > (int)strlen(device_instance)) {
            sstrncat(pContactStr, device_instance, size);
        }
        // add tag cisco-keep-alive for keep alive messages in ccm mode
        if ((ccb->cc_type == CC_CCM) && (ccb->index >= REG_BACKUP_CCB)) {
            sipMethod_t method = sipMethodInvalid;

            sipGetRequestMethod(request, &method);
            if ((method == sipMethodRegister)) {
                strncat(pContactStr, ";expires=0;cisco-keep-alive",
                        MAX_SIP_HEADER_LENGTH - strlen(pContactStr) - 1);
            }
        }
    } else {
        char *forward_url = NULL;

        forward_url = Basic_is_phone_forwarded(ccb->dn_line);
        /* only use the forward URL if we are sending a 302 */
        if ((forward_url) &&
            (strstr(request->mesg_line, SIP_RED_MOVED_TEMP_PHRASE))) {
            char *user_info = strchr(forward_url, '@');

            /*
             * forward_url will always have domain/host address preceded
             * by @ following the user, returned by
             * Basic_is_phone_forwarded. so no need to check for
             * user_info == NULL
             */
            snprintf(pContactStr, 6, "<sip:");
            escaped_char_str_len =
                sippmh_convertURLCharToEscChar(forward_url,
                                               user_info - forward_url, // len of user
                                               pContactStr + 5,
                                               (MAX_SIP_URL_LENGTH - 5),
                                               FALSE);
            snprintf(pContactStr + 5 + escaped_char_str_len,
                     sizeof(pContactStr) - 5 - escaped_char_str_len, "%s>",
                     user_info);
        } else {
            /*
             * Use the contact value supplied to us, if available.
             * If not, use name
             */
            snprintf(pContactStr, 6, "<sip:");
            if ((cpr_strcasecmp(contact, UNPROVISIONED) != 0) &&
                (contact[0] != '\0')) {
                escaped_char_str_len =
                    sippmh_convertURLCharToEscChar(contact, strlen(contact),
                                                   pContactStr + 5,
                                                   (MAX_SIP_URL_LENGTH - 5),
                                                   FALSE);
            } else {
                escaped_char_str_len =
                    sippmh_convertURLCharToEscChar(line_name, strlen(line_name),
                                                   pContactStr + 5,
                                                   (MAX_SIP_URL_LENGTH - 5),
                                                   FALSE);
            }

            if (cpr_strcasecmp(reg_user_info, "none") == 0) {
                snprintf(pContactStr + 5 + escaped_char_str_len,
                         sizeof(pContactStr) - 5 - escaped_char_str_len,
                         "@%s:%d;transport=%s>", src_addr_str,
                         ccb->local_port, transport_type_str);
            } else {
                snprintf(pContactStr + 5 + escaped_char_str_len,
                         sizeof(pContactStr) - 5 - escaped_char_str_len,
                         "@%s:%d;user=%s;transport=%s>",
                         src_addr_str, ccb->local_port,
                         reg_user_info, transport_type_str);
            }
        }
    }
    return (sippmh_add_text_header(request, SIP_HEADER_CONTACT, pContactStr));
}

/**
 * Convert phone name to upper case 
 *
 * @param phone_name - phone name
 *
 * @return status none
 *
 * @pre none
 */
void convert_phone_name_to_upper_case(char *phone_name)
{
    while (phone_name && (*phone_name) != '\0') {
        *phone_name = (char)toupper(*phone_name);
        phone_name++;
    }
}
/**
 * Add reason header to SIP message
 *
 * eg: Reason: SIP;cause=200;text="cisco:22 Name=SEP000000000000
 * Load=SIP70.8-2-25 Last=reset-reset
 * 
 * @param ccb       call control block
 * @param request   sip message
 *
 * @return status 
 *         success: STATUS_SUCCESS
 *         failure: STATUS_FAILURE
 *  
 * @pre  (ccb      not_eq NULL)
 * @pre  (request  not_eq NULL)
 *
 */
sipRet_t
sipSPIAddReasonHeader (ccsipCCB_t *ccb, sipMessage_t *request)
{
    const char *fname = "sipSPIAddReasonHeader";
    char        pReasonStr[MAX_SIP_HEADER_LENGTH];
    uint8_t     mac_address[MAC_ADDRESS_LENGTH];
    char        phone_name[MAX_PHONE_NAME_LEN];
    char        image_a[MAX_LOAD_ID_STRING];
    char        image_b[MAX_LOAD_ID_STRING];
    int         active_partition;
    int         unreg_reason_code = 0;   
    char        unreg_reason_str[MAX_UNREG_REASON_STR_LEN];

    if (ccb->send_reason_header) { 
        // should only be set when the phone is registering after a restart/reset
        platform_get_wired_mac_address(mac_address);
  
        snprintf(phone_name, MAX_PHONE_NAME_LEN, "SEP%04x%04x%04x", mac_address[0] * 256 + mac_address[1], 
                                                                    mac_address[2] * 256 + mac_address[3],
                                                                    mac_address[4] * 256 + mac_address[5]);

        convert_phone_name_to_upper_case(phone_name);


        unreg_reason_code = platGetUnregReason();

        unreg_reason_str[0] = '\0';
        get_reason_string(unreg_reason_code, unreg_reason_str, MAX_UNREG_REASON_STR_LEN);
        active_partition = platGetActiveInactivePhoneLoadName(image_a, image_b, MAX_LOAD_ID_STRING);
        snprintf(pReasonStr, MAX_SIP_HEADER_LENGTH, 
            "SIP;cause=200;text=\"cisco-alarm:%d Name=%s ActiveLoad=%s InactiveLoad=%s Last=%s",
            unreg_reason_code, phone_name, (active_partition == 1) ?  image_a:image_b,
            (active_partition == 1) ? image_b:image_a, unreg_reason_str);
        sstrncat(pReasonStr, "\"", 
                MAX_SIP_HEADER_LENGTH - strlen(pReasonStr) - 1);
        return (sippmh_add_text_header(request, SIP_HEADER_REASON, pReasonStr));
    } else {
        CCSIP_DEBUG_ERROR("%s called with send_reason_header set to false\n", fname);
        return (STATUS_SUCCESS);
    }
}

/**
 * Return the reason string that is to be returned 
 * corresponding to the unreg reason that is to be sent
 * in the register message.
 * 
 * @param unreg_reason    - unreg reason code 
 * @param char *          - reason string corresponding to the code 
 * @return none
 *
 */
void
get_reason_string (int unreg_reason, char *unreg_reason_str, int len)
{

    switch(unreg_reason) {
        case UNREG_REASON_RESET_RESTART:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "reset-restart");
            break;
        case UNREG_REASON_RESET_RESET:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "reset-reset");
            break;
        case UNREG_REASON_PHONE_INITIALIZED:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "initialized");
            break;
        case UNREG_REASON_REG_TIMEOUT:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "reg-timeout");
            break;
        case UNREG_REASON_PHONE_KEYPAD:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "phone-keypad");
            break;
        case UNREG_REASON_PHONE_REG_REJ:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "phone-reg-rej");
            break;
        case UNREG_REASON_FALLBACK:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "fallback");
            break;
        case UNREG_REASON_VERSION_STAMP_MISMATCH:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "version-stamp-mismatch(%s)", sipUnregisterReason);
            break;
        case UNREG_REASON_VERSION_STAMP_MISMATCH_CONFIG:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "version-stamp-mismatch-config");
            break;
        case UNREG_REASON_VERSION_STAMP_MISMATCH_SOFTKEY:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "version-stamp-mismatch-softkey");
            break;
        case UNREG_REASON_VERSION_STAMP_MISMATCH_DIALPLAN:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "version-stamp-mismatch-dialplan");
            break;
        case UNREG_REASON_CONFIG_RETRY_RESTART:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "config-retry-restart");
            break;
        case UNREG_REASON_TLS_ERROR:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "tls-error");
            break;
        case UNREG_REASON_TCP_TIMEOUT:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "tcp_timeout");
            break;
        case UNREG_REASON_CM_CLOSED_TCP:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "cm-closed-tcp");
            break;
        case UNREG_REASON_CM_RESET_TCP:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "cm-reset-tcp");
            break;
        case UNREG_REASON_CM_ABORTED_TCP:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "cm-aborted-tcp");
            break;
        case UNREG_REASON_APPLY_CONFIG_RESTART:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "apply_config");
            break;
        case UNREG_REASON_VOICE_VLAN_CHANGED:
            snprintf(unreg_reason_str, MAX_UNREG_REASON_STR_LEN, "VLAN-Changed");
            break;
        default:
            unreg_reason_str[0] = '\0';
            CCSIP_DEBUG_ERROR("Unkown unreg reason code passed\n");
            break;
    }
}
boolean
CreateRequest (ccsipCCB_t *ccb, sipMessageFlag_t messageflag,
               sipMethod_t sipmethod, sipMessage_t *request,
               boolean initInvite, uint32_t response_cseq_number)
{
    sipRet_t tflag = STATUS_FAILURE;

    if (!request) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          "CreateRequest", "GET_SIP_MESSAGE()");
        return FALSE;
    }


    if (sipMethodResponse != sipmethod) {
        if (sipSPIAddRequestLine(ccb, request, sipmethod, initInvite)
                == STATUS_FAILURE) {
            return FALSE;
        }
    }

    ccb->outBoundProxyPort = 0;
    ccb->outBoundProxyAddr = ip_addr_invalid;
    if (ccb->ObpSRVhandle != NULL) {
        dnsFreeSrvHandle(ccb->ObpSRVhandle);
        ccb->ObpSRVhandle = NULL;
    }

    tflag = (allocateTrx(ccb, sipmethod)) ? STATUS_SUCCESS : STATUS_FAILURE;

    if (tflag == STATUS_SUCCESS) {
        tflag = (sipSPIAddLocalVia(request, ccb, sipmethod)) ?
            STATUS_SUCCESS : STATUS_FAILURE;
        /* Don't stop adding headers to a Register just because
         * the VIA line wasn't added. A Register doesn't really
         * need the VIA line anyways.
         */
        if ((HSTATUS_SUCCESS != tflag) && (ccb->type != SIP_REG_CCB)) {
            return FALSE;
        }
    }

    if (tflag == STATUS_SUCCESS) {
        tflag = sipSPIAddCommonHeaders(ccb, request, FALSE, sipmethod,
                                       response_cseq_number);
    }

    if (tflag != HSTATUS_SUCCESS) {
        return FALSE;
    }

    tflag = sippmh_add_text_header(request, SIP_HEADER_USER_AGENT,
                                   sipHeaderUserAgent);

    if (tflag != HSTATUS_SUCCESS) {
        return FALSE;
    }
    return AddGeneralHeaders(ccb, messageflag, request, sipmethod);
}

boolean
CreateResponse (ccsipCCB_t *ccb,
               sipMessageFlag_t messageflag,
               uint16_t status_code,
               sipMessage_t *response,
               const char *reason_phrase,
               uint16_t status_code_warning,
               const char *reason_phrase_warning,
               sipMethod_t method)
{
    sipRet_t tflag = HSTATUS_SUCCESS;
    char    *warning = NULL;
    uint32_t response_cseq_number = 0;

    if (!ccb) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT),
                          "CreateResponse", "ccb");
        return FALSE;
    }
    if (!ccb->last_request) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_BADARGUMENT),
                          "Create Response", "ccb->last_request");
        return FALSE;
    }
    if (!response) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          "CreateResponse", "GET_SIP_MESSAGE()");
        return FALSE;
    }

    tflag = sippmh_add_response_line(response, SIP_VERSION, status_code,
                                     reason_phrase);

    if (tflag != HSTATUS_SUCCESS)
        return FALSE;

    tflag = (sipSPIAddRequestVia(ccb, response, ccb->last_request, method)) ?
        STATUS_SUCCESS : STATUS_FAILURE;

    if (tflag != HSTATUS_SUCCESS)
        return FALSE;

    response_cseq_number = 0;

    tflag = sipSPIAddCommonHeaders(ccb, response, TRUE, method,
                                   response_cseq_number);
    if (tflag != HSTATUS_SUCCESS)
        return FALSE;

    if (reason_phrase_warning) {
        warning = (char *) cpr_malloc(strlen(reason_phrase_warning) + 5);
        if (warning) {
            snprintf(warning, strlen(reason_phrase_warning) + 5,
                     "%d %s", status_code_warning, reason_phrase_warning);
            tflag = sippmh_add_text_header(response, SIP_HEADER_WARN, warning);
            cpr_free(warning);
            if (tflag != HSTATUS_SUCCESS)
                return FALSE;
        }
    }

    tflag = sippmh_add_text_header(response, SIP_HEADER_SERVER,
                                   sipHeaderServer);
    if (tflag != HSTATUS_SUCCESS) {
        return FALSE;
    }

    return AddGeneralHeaders(ccb, messageflag, response, method);
}

/*************************************************************
 * Function: SendRequest
 * this function sends out a request pointed to by the request parameter
 * in ccb's context.
 *
 * 'request' will be freed. The caller does not have the responsibility
 * to free it.
 **************************************************************/
boolean
SendRequest (ccsipCCB_t *ccb, sipMessage_t *request, sipMethod_t method,
            boolean midcall, boolean reTx, boolean retranTimer)
{
    const char     *fname = "SendRequest";
    cpr_ip_addr_t  cc_remote_ipaddr;
    uint16_t       cc_remote_port = 0;
    int            timeout = 0;
    int            expires_timeout;
    sipUrl_t       *sipUrl = NULL;
    boolean        isRegister = FALSE;
    int16_t        trx_index;
    int            reldev_stored_msg = RELDEV_NO_STORED_MSG;

    CPR_IP_ADDR_INIT(cc_remote_ipaddr);

    if (sipMethodRegister == method) {
        if (ccb->reg.proxy[0] == '\0') {
            cc_remote_ipaddr = ccb->dest_sip_addr;
            cc_remote_port = (uint16_t) ccb->dest_sip_port;
        } else {
            cc_remote_ipaddr = ccb->reg.addr;
            cc_remote_port = ccb->reg.port;
        }
        config_get_value(CFGID_TIMER_T1, &timeout, sizeof(timeout));
        isRegister = TRUE;

    } else if ((sipMethodInvite == method) && (midcall == FALSE)) {
        char *host;

        if (!ccb->ReqURI) {
            free_sip_message(request);
            return FALSE;
        }
        host = strchr(ccb->ReqURI, '@');
        if (!host) {
            free_sip_message(request);
            return FALSE;
        }

        /* Enable reTx and send */
        if (TRUE == reTx) {
            config_get_value(CFGID_TIMER_T1, &timeout, sizeof(timeout));
        }
        cc_remote_ipaddr = ccb->dest_sip_addr;
        cc_remote_port = (uint16_t) ccb->dest_sip_port;
    } else {
        if ((ccb->record_route_info) && (sipMethodCancel != method)) {
            int16_t i;

            if (ccb->flags & INCOMING) {
                i = 0;
            } else {
                i = ccb->record_route_info->num_locations - 1;
            }

            if (ccb->record_route_info->locations[i]->genUrl->schema
                    == URL_TYPE_SIP) {
                sipUrl = ccb->record_route_info->locations[i]->genUrl->u.sipUrl;
            } else {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_URL_ERROR), fname);
                free_sip_message(request);
                return (FALSE);
            }

            cc_remote_port = sipUrl->port;

            if (!sipUrl->port_present) {
                dns_error_code =
                    sipTransportGetServerAddrPort(sipSPIUrlDestination(sipUrl),
                                                  &cc_remote_ipaddr,
                                                  &cc_remote_port,
                                                  NULL, FALSE);
            } else {
                dns_error_code = dnsGetHostByName(sipSPIUrlDestination(sipUrl),
                                                   &cc_remote_ipaddr, 100, 1);
            }
            if (dns_error_code == 0) {
                util_ntohl(&cc_remote_ipaddr, &cc_remote_ipaddr);
            } else {
                cc_remote_ipaddr = ip_addr_invalid;
            }

        } else if ((ccb->contact_info) &&
                   (ccb->state >= SIP_STATE_SENT_INVITE_CONNECTED)) {
            /*
             * If the call has been set up, we are free to use the
             * contact header.  If the call has NOT been set up,
             * drop through and use the proxy.
             */
            if (ccb->contact_info->locations[0]->genUrl->schema
                    == URL_TYPE_SIP) {
                sipUrl = ccb->contact_info->locations[0]->genUrl->u.sipUrl;
            } else {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_URL_ERROR), fname);
                free_sip_message(request);
                return (FALSE);
            }

            cc_remote_port = sipUrl->port;

            if (!sipUrl->port_present) {
                dns_error_code =
                    sipTransportGetServerAddrPort(sipSPIUrlDestination(sipUrl),
                                                  &cc_remote_ipaddr,
                                                  &cc_remote_port,
                                                  NULL, FALSE);
            } else {
                dns_error_code = dnsGetHostByName(sipSPIUrlDestination(sipUrl),
                                                   &cc_remote_ipaddr, 100, 1);
            }
            if (dns_error_code == 0) {

                util_ntohl(&cc_remote_ipaddr, &cc_remote_ipaddr);
            } else {
                cc_remote_ipaddr = ip_addr_invalid;
            }

        } else {
            cc_remote_ipaddr = ccb->dest_sip_addr;
            cc_remote_port = (uint16_t) ccb->dest_sip_port;
        }
        if (TRUE == reTx)
            config_get_value(CFGID_TIMER_T1, &timeout, sizeof(timeout));
    }

    if (util_check_if_ip_valid(&cc_remote_ipaddr) == FALSE) {
        free_sip_message(request);
        return (FALSE);
    }

    // Update default destination address and port
    ccb->dest_sip_addr = cc_remote_ipaddr;
    ccb->dest_sip_port = cc_remote_port;

    /*
     * Store the ACK so that it can be retransmitted in response
     * to any duplicate 200 OK or error responses
     */
    trx_index = get_last_request_trx_index(ccb, TRUE);
    if (trx_index < 0) {
        CCSIP_DEBUG_ERROR("%s: No Valid Trx found!\n", "SendRequest");
        return (FALSE);
    }
    if (sipMethodAck == method) {
        reldev_stored_msg =
            sipRelDevCoupledMessageStore(request, ccb->sipCallID,
                                         ccb->sent_request[trx_index].cseq_number,
                                         ccb->sent_request[trx_index].cseq_method,
                                         TRUE, ccb->last_recvd_response_code,
                                         &cc_remote_ipaddr, cc_remote_port,
                                         FALSE /* Do check tag */);
    }
    if (sipTransportCreateSendMessage(ccb, request, method,
                                      &cc_remote_ipaddr, cc_remote_port,
                                      isRegister, reTx, timeout, NULL,
                                      reldev_stored_msg) < 0) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          "SendRequest", "sipTransportCreateSendMessage()");
        //dont need message free here..sipTransportCreateSendMessage() will free it
        return (FALSE);
    }

    /* Start INVITE expires timer */
    if (retranTimer) {
        config_get_value(CFGID_TIMER_INVITE_EXPIRES, &expires_timeout,
                         sizeof(expires_timeout));
        if (expires_timeout > 0) {
            if (sip_platform_expires_timer_start(expires_timeout * 1000,
                                                 ccb->index,
                                                 &cc_remote_ipaddr,  //ccb->dest_sip_addr,
                                                 cc_remote_port)    //ccb->dest_sip_port,
                != SIP_OK) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                                  "SendRequest",
                                  "sip_platform_expires_timer_start()");
                return (FALSE);
            }
        }
    }
    // Save Call History
    if (sipMethodCancel == method || sipMethodBye == method) {
        gCallHistory[ccb->index].last_bye_cseq_number =
            ccb->sent_request[trx_index].cseq_number;
        gCallHistory[ccb->index].proxy_dest_ipaddr = ccb->dest_sip_addr;
        gCallHistory[ccb->index].dn_line = ccb->dn_line;
        sstrncpy(gCallHistory[ccb->index].via_branch,
                 ccb->sent_request[trx_index].u.sip_via_branch,
                 VIA_BRANCH_LENGTH);
    }
    return (TRUE);
}

boolean
sendResponse (ccsipCCB_t *ccb,
              sipMessage_t *response,
              sipMessage_t *refrequest,
              boolean retx,
              sipMethod_t method)
{
    sipVia_t       *via = NULL;
    const char     *request_callid = NULL;
    sipCseq_t      *request_cseq_structure;
    cpr_ip_addr_t  cc_remote_ipaddr;
    uint16_t       cc_remote_port = 0;
    int            timeout = 0;
    const char     *pViaHeaderStr = NULL;
    char           *dest_ip_addr_str = 0;
    int16_t        trx_index = -1;
    boolean        port_present = FALSE;
    int            reldev_stored_msg;
    int            status_code = 0;

    CPR_IP_ADDR_INIT(cc_remote_ipaddr);

    if (ccb) {
        request_callid = ccb->sipCallID;
        trx_index = get_method_request_trx_index(ccb, method, FALSE);
        if (trx_index >= 0) {
            pViaHeaderStr = (const char *)
                (ccb->recv_request[trx_index].u.sip_via_header);
            request_cseq_structure = (sipCseq_t *)
                cpr_malloc(sizeof(sipCseq_t));
            if (!request_cseq_structure) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                                  "Sendresponse", "malloc failed");
                free_sip_message(response);
                return (FALSE);
            }
            request_cseq_structure->method =
                ccb->recv_request[trx_index].cseq_method;
            request_cseq_structure->number =
                ccb->recv_request[trx_index].cseq_number;
        } else {
            pViaHeaderStr =
                sippmh_get_cached_header_val(ccb->last_request, VIA);
            if (getCSeqInfo(ccb->last_request, &request_cseq_structure)
                == FALSE) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                                  "Sendresponse", "getCSeqInfo returned false");
                free_sip_message(response);
                return (FALSE);
            }
        }
    } else {
        pViaHeaderStr = sippmh_get_cached_header_val(refrequest, VIA);
        request_callid = sippmh_get_cached_header_val(refrequest, CALLID);
        if (FALSE == getCSeqInfo(refrequest, &request_cseq_structure)) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              "Sendresponse", "getCSeqInfo returned false");
            free_sip_message(response);
            return (FALSE);
        }
    }

    via = sippmh_parse_via(pViaHeaderStr);
    if (!via) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          "Sendresponse", "Bad Via Header in Message!");
        cpr_free(request_cseq_structure);
        free_sip_message(response);
        return (FALSE);
    }

    if (via->remote_port) {
        cc_remote_port = via->remote_port;
        port_present = TRUE;
    } else {
        /* Use default 5060 if via does not have port */
        cc_remote_port = SIP_WELL_KNOWN_PORT;
    }

    /*
     * if maddr is present use it
     */
    if (via->maddr) {
        if (!port_present) {
            dns_error_code = sipTransportGetServerAddrPort(via->maddr,
                                                           &cc_remote_ipaddr,
                                                           &cc_remote_port,
                                                           NULL, FALSE);
        } else {
            dns_error_code = dnsGetHostByName(via->maddr,
                                               &cc_remote_ipaddr, 100, 1);
        }
        if (dns_error_code != 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              "Sendresponse",
                              "sipTransportGetServerAddrPort or dnsGetHostByName");
        } else {
            util_ntohl(&cc_remote_ipaddr, &cc_remote_ipaddr);
        }
    }

    /*
     * if maddr isn't present, or the DNS lookup failed, send the
     * response to the IP address we received the message from.
     */
    if (util_check_if_ip_valid(&cc_remote_ipaddr) == FALSE) {
        if (via->recd_host) {
            dest_ip_addr_str = via->recd_host;
        } else {
            dest_ip_addr_str = via->host;
        }

        if (!port_present) {
            dns_error_code = sipTransportGetServerAddrPort(dest_ip_addr_str,
                                                           &cc_remote_ipaddr,
                                                           &cc_remote_port,
                                                           NULL, FALSE);
        } else {
            dns_error_code = dnsGetHostByName(dest_ip_addr_str,
                                               &cc_remote_ipaddr, 100, 1);
        }
        if (dns_error_code != 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              "Sendresponse",
                              "sipTransportGetServerAddrPort or dnsGetHostByName");
            cpr_free(request_cseq_structure);
            sippmh_free_via(via);
            free_sip_message(response);
            return (FALSE);
        } else {
            util_ntohl(&cc_remote_ipaddr , &cc_remote_ipaddr);
        }
    }

    sippmh_free_via(via);

    reldev_stored_msg =
        sipRelDevCoupledMessageStore(response, request_callid,
                                     request_cseq_structure->number,
                                     request_cseq_structure->method,
                                     FALSE, status_code,
                                     &cc_remote_ipaddr, cc_remote_port,
                                     /* If responding to call setup don't check tag */
                                     (boolean)(ccb != NULL ?
                                          SIP_SM_CALL_SETUP_RESPONDING(ccb) :
                                          FALSE));
    cpr_free(request_cseq_structure);
    /* Enable reTx and send */
    if (retx) {
        config_get_value(CFGID_TIMER_T1, &timeout, sizeof(timeout));
        if (ccb) {
            ccb->retx_counter = 0;
        }
    } else {
        timeout = 0; /* No reTx timer */
    }

    if (sipTransportChannelCreateSend(ccb, response, sipMethodResponse,
                                      &cc_remote_ipaddr, cc_remote_port,
                                      timeout, reldev_stored_msg) < 0) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          "SendResponse", "sipTransportChannelCreateSend()");
        return FALSE;
    }
    return TRUE;
}

static sipRet_t
CopyLocalSDPintoResponse (sipMessage_t *request,
                          cc_msgbody_info_t *local_msg_body)
{
    sipRet_t          tflag = HSTATUS_SUCCESS;
    uint32_t          body_index;
    cc_msgbody_info_t tmp_body, *tmp_body_p;
    cc_msgbody_t     *part;

    if (local_msg_body->num_parts == 0) {
        /* content type specified but no msg. body to add */
        return HSTATUS_FAILURE;
    }

    /*
     * Duplicate a msg. body to send out which will be freed
     * after the full message is created during the send of the
     * msg.
     */
    tmp_body.num_parts = 0; /*initialize to no parts */
    tmp_body_p = &tmp_body;
    if (cc_cp_msg_body_parts(tmp_body_p, local_msg_body) != CC_RC_SUCCESS) {
        /* Unable to duplicate the msg. body */
        return HSTATUS_FAILURE;
    }
    part = &tmp_body_p->parts[0];
    for (body_index = 0; body_index < tmp_body_p->num_parts; body_index++) {
        if ((part->body != NULL) && (part->body_length)) {
            tflag = sippmh_add_message_body(request, part->body,
                        part->body_length,
                        cc2siptype(part->content_type),
                        cc2sipdisp(part->content_disposition.disposition),
                        part->content_disposition.required_handling,
                        part->content_id);
        } else {
            /* Invalid entry */
            tflag = HSTATUS_FAILURE;
            break;
        }
    }
    return tflag;
}

boolean
AddGeneralHeaders (ccsipCCB_t *ccb,
                   sipMessageFlag_t messageflag,
                   sipMessage_t *request,
                   sipMethod_t sipmethod)
{
    const char  *fname = "AddGeneralHeaders";
    sipRet_t     tflag = HSTATUS_SUCCESS;
    unsigned int isOver = messageflag.flags;
    unsigned int whichflag = 0x0001;
    unsigned int bit_to_reset = 0; // Test and then reset
    int16_t      i;
    int          time_exp;
    unsigned int info_index;
    cpr_ip_mode_e ip_mode;

    while (0 != isOver) {
        bit_to_reset = (whichflag & messageflag.flags);
        switch (bit_to_reset) {
        case SIP_HEADER_CONTACT_BIT:
            tflag = sipSPIAddContactHeader(ccb, request);
            break;

        case SIP_HEADER_RECORD_ROUTE_BIT:
            if (ccb->record_route_info) {
                tflag = (sipSPIAddRequestRecordRoute(request, ccb->last_request)) ?
                    STATUS_SUCCESS : STATUS_FAILURE;
            }
            break;

        case SIP_HEADER_ROUTE_BIT:
            tflag = (sipSPIAddRouteHeaders(request, ccb, NULL, 0)) ?
                STATUS_SUCCESS : STATUS_FAILURE;
            break;

        case SIP_HEADER_UNSUPPORTED_BIT:
            if (ccb->sip_unsupported[0] != '\0') {
                tflag = sippmh_add_text_header(request,
                                               SIP_HEADER_UNSUPPORTED,
                                               &ccb->sip_unsupported[0]);
            }
            break;

        case SIP_HEADER_REQUESTED_BY_BIT:
            tflag = sippmh_add_text_header(request, SIP_HEADER_REQUESTED_BY,
                                           ccb->sip_reqby);
            break;

        case SIP_HEADER_REMOTE_PARTY_ID_BIT:
            tflag = sippmh_add_text_header(request, SIP_HEADER_REMOTE_PARTY_ID,
                                           ccb->sip_remote_party_id);
            break;

        case SIP_HEADER_DIVERSION_BIT:
            for (i = 0; i < MAX_DIVERSION_HEADERS; i++) {
                if (ccb->diversion[i]) {
                    tflag = sippmh_add_text_header(request,
                                                   SIP_HEADER_DIVERSION,
                                                   ccb->diversion[i]);
                    if (tflag != HSTATUS_SUCCESS)
                        break;
                }
            }
            break;

        case SIP_HEADER_AUTHENTICATION_BIT:
            tflag = sippmh_add_text_header(request,
                                           AUTHOR_HDR(ccb->authen.status_code),
                                           ccb->authen.authorization);
            break;

        case SIP_HEADER_PROXY_AUTH_BIT:
            // This happens when we get a refer with a proxy-auth. We just
            // parrot whatever the proxy told us to do in this invite.
            tflag = sippmh_add_text_header(request,
                                           SIP_HEADER_PROXY_AUTHORIZATION,
                                           ccb->refer_proxy_auth);
            break;

        case SIP_HEADER_REFER_TO_BIT:
            break;

        case SIP_HEADER_REFERRED_BY_BIT:
            tflag = sippmh_add_text_header(request, SIP_HEADER_REFERRED_BY,
                                           ccb->sip_referredBy);
            break;

        case SIP_HEADER_REPLACES_BIT:
            tflag = sippmh_add_text_header(request, SIP_HEADER_REPLACES,
                                           ccb->sipxfercallid);
            break;

        case SIP_HEADER_EVENT_BIT:
            break;

        case SIP_HEADER_EXPIRES_BIT:
            config_get_value(CFGID_TIMER_INVITE_EXPIRES, &time_exp,
                             sizeof(time_exp));
            tflag = sippmh_add_int_header(request, SIP_HEADER_EXPIRES,
                                          time_exp);
            break;

        case SIP_HEADER_REASON_BIT:	    
            tflag = sipSPIAddReasonHeader(ccb, request);
            break;

        case SIP_HEADER_CONTENT_LENGTH_BIT:
            if ((messageflag.flags & SIP_HEADER_CONTENT_TYPE_BIT) ||
                (messageflag.flags & SIP_HEADER_OPTIONS_CONTENT_TYPE_BIT)) {
                /*
                 * Header content length will be set when SDP body is added
                 * so we do not set it here.
                 */
                break;
            }
            tflag = sippmh_add_int_header(request, SIP_HEADER_CONTENT_LENGTH, 0);
            break;

        case SIP_HEADER_CONTENT_TYPE_BIT:
            tflag = CopyLocalSDPintoResponse(request, &ccb->local_msg_body);
            if (tflag != HSTATUS_SUCCESS) {
                /* there is some thing wrong with message body */
                CCSIP_DEBUG_ERROR("%s: Error adding message body.\n", fname);
                break;
            }

            /*
             * SDP successfully added to message. Update offer/answer state.
             */
            if (ccb->oa_state == OA_OFFER_RECEIVED) {
                ccb->oa_state = OA_IDLE;
            } else {
                ccb->oa_state = OA_OFFER_SENT;
            }
            break;

        case SIP_HEADER_OPTIONS_CONTENT_TYPE_BIT:
            tflag = CopyLocalSDPintoResponse(request, &ccb->local_msg_body);
            if (tflag != HSTATUS_SUCCESS) {
                /* there is some thing wrong with message body */
                CCSIP_DEBUG_ERROR("%s: Error adding options message body.\n",
                                  fname);
            }

            /*
             * SDP successfully added to message. OPTIONS response does
             * not alter offer/answer state as the
             * SIP_HEADER_CONTENT_TYPE_BIT does.
             */
            break;

        case SIP_HEADER_ALLOW_BIT:
            {
                char temp[MAX_SIP_HEADER_LENGTH];

                snprintf(temp, MAX_SIP_HEADER_LENGTH,
                         "%s,%s,%s,%s,%s,%s,%s,%s,%s",
                         SIP_METHOD_ACK, SIP_METHOD_BYE, SIP_METHOD_CANCEL,
                         SIP_METHOD_INVITE, SIP_METHOD_NOTIFY,
                         SIP_METHOD_OPTIONS, SIP_METHOD_REFER,
                         SIP_METHOD_REGISTER, SIP_METHOD_UPDATE);
                strncat(temp, ",", (MAX_SIP_HEADER_LENGTH - strlen(temp) - 1));
                strncat(temp, SIP_METHOD_SUBSCRIBE,
                        (MAX_SIP_HEADER_LENGTH - strlen(temp) - 1));
                strncat(temp, ",", (MAX_SIP_HEADER_LENGTH - strlen(temp) - 1));
                strncat(temp, SIP_METHOD_INFO,
                        (MAX_SIP_HEADER_LENGTH - strlen(temp) - 1));
                tflag = sippmh_add_text_header(request, SIP_HEADER_ALLOW, temp);
            }
            break;

        case SIP_HEADER_ACCEPT_BIT:
            tflag = sippmh_add_text_header(request, SIP_HEADER_ACCEPT,
                                           "application/sdp");
            break;

        case SIP_HEADER_ACCEPT_ENCODING_BIT:
            tflag = sippmh_add_text_header(request, SIP_HEADER_ACCEPT_ENCODING,
                                           "identity");
            break;

        case SIP_HEADER_ACCEPT_LANGUAGE_BIT:
            tflag = sippmh_add_text_header(request, SIP_HEADER_ACCEPT_LANGUAGE,
                                           "en");
            break;

        case SIP_HEADER_CISCO_GUID_BIT:
            tflag = (sipSPIAddCiscoGuid(request, ccb)) ?
                STATUS_SUCCESS : STATUS_FAILURE;
            break;

        case SIP_HEADER_CALL_INFO_BIT:
            /*
             * Include call info header if in CCM mode or
             * other end indicates that it can support it.
             */
            if ((sip_regmgr_get_cc_mode(ccb->dn_line) == REG_MODE_CCM) ||
                (ccb->supported_tags & cisco_callinfo_tag)) {
                tflag = (sipRet_t) sippmh_add_call_info(request,
                                                        ccb->out_call_info);
            }
            break;

        case SIP_HEADER_JOIN_INFO_BIT:
            tflag = (sipRet_t) sippmh_add_join_header(request, ccb->join_info);
            break;

        case SIP_HEADER_ALLOW_EVENTS_BIT:
            {
                char temp[MAX_SIP_HEADER_LENGTH];
                int  kpml_config;

                // Get kpml configuration
                config_get_value(CFGID_KPML_ENABLED, &kpml_config,
                                 sizeof(kpml_config));
                if (kpml_config) {
                    snprintf(temp, MAX_SIP_HEADER_LENGTH, "%s,%s",
                             SIP_EVENT_KPML, SIP_EVENT_DIALOG);
                } else {
                    snprintf(temp, MAX_SIP_HEADER_LENGTH, "%s",
                             SIP_EVENT_DIALOG);
                }
                tflag = sippmh_add_text_header(request, SIP_HEADER_ALLOW_EVENTS,
                                               temp);
            }
            break;

        case SIP_HEADER_SUPPORTED_BIT:
            {
                const char *opt_tags;

                opt_tags = sipGetSupportedOptionList(ccb, sipmethod);

                tflag = sippmh_add_text_header(request,
                                               SIP_HEADER_SUPPORTED,
                                               opt_tags);
				}
            break;

        case SIP_HEADER_REQUIRE_BIT:
            ip_mode = platform_get_ip_address_mode();
            if (ip_mode == CPR_IP_MODE_DUAL) {
                tflag = sippmh_add_text_header(request, SIP_HEADER_REQUIRE, "sdp-anat");
            }
            break;     
 
        case SIP_HEADER_RETRY_AFTER_BIT:
            tflag = sippmh_add_int_header(request,
                                          SIP_HEADER_RETRY_AFTER,
                                          abs((cpr_rand() % 11)));
            break;

        case SIP_HEADER_RECV_INFO_BIT:
            for (info_index = 0; info_index < MAX_INFO_HANDLER; info_index++) {
                if (g_registered_info[info_index] != NULL) {
                    tflag = sippmh_add_text_header(request, SIP_HEADER_RECV_INFO,
                                                   g_registered_info[info_index]);
                    if (tflag != HSTATUS_SUCCESS) {
                        break;
                    }
                }
            }
            break;

        default:
            //tflag = HSTATUS_FAILURE;
            break;
        }

        if (tflag != HSTATUS_SUCCESS) {
            return FALSE;
        }
        whichflag = whichflag << 1;
        /*
         * Reset this bit so if nothing else is there we do not need to test
         */
        isOver &= ~bit_to_reset;
    }
    return TRUE;
}


/*************************************************************
 * Function: sipSPISendUpdate
 * This function creates, formats, and sends an UPDATE message
 * on an existing (early) dialog
 **************************************************************/
boolean
sipSPISendUpdate (ccsipCCB_t *ccb)
{
    const char      *fname = "sipSPISendUpdate";
    sipMessageFlag_t messageflag;
    sipMessage_t    *request = NULL;
    sipRet_t         flag = STATUS_SUCCESS;


    // UPDATE requests mandates the use of the following headers:
    // Allow, Call-ID, Contact, CSeq, From, Max-Forwards, To, and Via

    messageflag.flags = 0;
    messageflag.flags |= SIP_HEADER_ALLOW_BIT |
                         SIP_HEADER_CONTACT_BIT |
                         SIP_HEADER_ROUTE_BIT;

    if (ccb->local_msg_body.num_parts) {
        messageflag.flags |= SIP_HEADER_CONTENT_TYPE_BIT;
    } else {
        messageflag.flags |= SIP_HEADER_CONTENT_LENGTH_BIT;
    }

    request = GET_SIP_MESSAGE();
    messageflag.extflags = 0;
    // CreateRequest adds the request line, Via, Date, CSeq, User-Agent
    // and all the headers in the messageflags above. It will also write
    // the new SDP which it picks from the ccb (ccb->sip_sdp)
    if (CreateRequest(ccb, messageflag, sipMethodUpdate, request, FALSE, 0)) {
        flag = STATUS_SUCCESS;
    } else {
        flag = STATUS_FAILURE;
    }

    if (flag != STATUS_SUCCESS) {
        free_sip_message(request);
        CCSIP_DEBUG_ERROR("%s: Error: UPDATE message build unsuccessful.\n",
                          fname);
        clean_method_request_trx(ccb, sipMethodUpdate, TRUE);
        return (FALSE);
    }
    // Send the request
    ccb->retx_counter = 0;
    if (SendRequest(ccb, request, sipMethodUpdate, TRUE, TRUE, FALSE) == FALSE) {
        clean_method_request_trx(ccb, sipMethodUpdate, TRUE);
        return (FALSE);
    } else {
        return (TRUE);
    }
}

/*************************************************************
 * Function: sipSPISendUpdateResponse
 * This function creates, formats, and sends a response to an
 * UPDATE message received on an early dialog
 **************************************************************/
boolean
sipSPISendUpdateResponse (ccsipCCB_t *ccb,
                          boolean send_sdp,
                          cc_causes_t cause,
                          boolean retx)
{
    const char      *fname = "SIPSPISendUpdateResponse";
    sipMessage_t    *response = NULL;
    sipRet_t         flag = STATUS_SUCCESS;
    sipMessageFlag_t messageflag;
    int              statusCode;
    char            *reason_phrase;
    boolean          result;

    // Determine the statusCode and reason_phrase from the cause value
    statusCode = ccsip_cc_to_sip_cause(cause, &reason_phrase);

    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_RESPONSE),
                      fname, statusCode);

    messageflag.flags = 0;
    messageflag.flags = SIP_HEADER_CONTACT_BIT |
                        SIP_HEADER_RECORD_ROUTE_BIT |
                        SIP_HEADER_ALLOW_BIT;

    if (send_sdp) {
        messageflag.flags |= SIP_HEADER_CONTENT_TYPE_BIT;
    } else {
        messageflag.flags |= SIP_HEADER_CONTENT_LENGTH_BIT;
    }

    if (statusCode == SIP_CLI_ERR_EXTENSION) {
        messageflag.flags |= SIP_HEADER_UNSUPPORTED_BIT;
    }
    if (statusCode == SIP_SERV_ERR_INTERNAL) {
        messageflag.flags |= SIP_HEADER_RETRY_AFTER_BIT;
    }
    response = GET_SIP_MESSAGE();
    messageflag.extflags = 0;
    if (CreateResponse(ccb, messageflag, (uint16_t)statusCode,
                       response, reason_phrase, 0, NULL, sipMethodUpdate)) {
        flag = HSTATUS_SUCCESS;
    } else {
        flag = HSTATUS_FAILURE;
    }

    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (response)
            free_sip_message(response);
        clean_method_request_trx(ccb, sipMethodUpdate, FALSE);
        return (FALSE);
    }

    result = sendResponse(ccb, response, ccb->last_request, retx,
                          sipMethodUpdate);
    clean_method_request_trx(ccb, sipMethodUpdate, FALSE);
    return result;
}

boolean
sipSPISendNotifyResponse (ccsipCCB_t *ccb, cc_causes_t cause)
{
    const char      *fname = "SIPSPISendNotifyResponse";
    sipMessage_t    *response = NULL;
    sipRet_t         flag = STATUS_SUCCESS;
    sipMessageFlag_t messageflag;
    int              sip_response_code;
    char            *sip_response_phrase;
    boolean          result;

    sip_response_code = ccsip_cc_to_sip_cause(cause, &sip_response_phrase);

    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_RESPONSE),
                      fname, sip_response_code);

    messageflag.flags = 0;
    messageflag.flags = SIP_HEADER_CONTACT_BIT |
                        SIP_HEADER_RECORD_ROUTE_BIT |
                        SIP_HEADER_CONTENT_LENGTH_BIT;

    /* Add Content Length */

    response = GET_SIP_MESSAGE();
    messageflag.extflags = 0;
    if (CreateResponse(ccb, messageflag, (unsigned short)sip_response_code,
                       response, sip_response_phrase, 0, NULL, sipMethodNotify)) {
        flag = HSTATUS_SUCCESS;
    } else {
        flag = HSTATUS_FAILURE;
    }

    /* If build error detected, cleanup and do not send message */
    if (flag != STATUS_SUCCESS) {
        /* !!! Clean up */
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_BUILDFLAG_ERROR), fname);
        if (response)
            free_sip_message(response);
        clean_method_request_trx(ccb, sipMethodNotify, FALSE);
        return (FALSE);
    }

    result = sendResponse(ccb, response, ccb->last_request, FALSE,
                          sipMethodNotify);
    clean_method_request_trx(ccb, sipMethodNotify, FALSE);
    return result;
}

/*
 * sipSPIGenerateReferredByHeader
 *
 * This function is called to generate SIP Referred-By Header
 * when SIP REFER request is sent, to Transfer the call
 *
 * @param[in,out] ccb   CCB call info structure
 *
 * @return              TRUE if the header is successfully
 *                      created; FALSE otherwise.
 *
 * @pre                 (ccb not_eqs NULL)
 *
 */
boolean 
sipSPIGenerateReferredByHeader (ccsipCCB_t *ccb)
{
    char        line_name[MAX_LINE_NAME_SIZE];
    char        escaped_line_name[MAX_ESCAPED_USER_LEN];
    char        dest_sip_addr_str[MAX_IPADDR_STR_LEN];
    char        pReferByStr[MAX_SIP_URL_LENGTH];
    boolean     retval = FALSE;
    cpr_ip_type ip_type;
    
    /* Initialize */
    line_name[0] = '\0';
    escaped_line_name[0] = '\0';
    dest_sip_addr_str[0] = '\0';
    pReferByStr[0] = '\0';

    /*
     * get Server Ip Addr, line_name and form AOR to populate referredBy
     */
    config_get_line_string(CFGID_LINE_NAME, line_name, ccb->dn_line,
                           sizeof(line_name));

    if (line_name[0] != '\0') {
        (void) sippmh_convertURLCharToEscChar(line_name, strlen(line_name),
                                              escaped_line_name,
                                              sizeof(escaped_line_name),
                                              TRUE);
    }

    ip_type = sipTransportGetPrimServerAddress(ccb->dn_line, dest_sip_addr_str);
    
    if (escaped_line_name[0] != '\0') {
        if (ip_type == CPR_IP_ADDR_IPV6) {
            snprintf(pReferByStr, MAX_SIP_URL_LENGTH, "<sip:%s@[%s]>",
                     escaped_line_name, dest_sip_addr_str);
        } else {
            snprintf(pReferByStr, MAX_SIP_URL_LENGTH, "<sip:%s@%s>",
                     escaped_line_name, dest_sip_addr_str);
        }
    }

    if (pReferByStr[0] != '\0') {
        ccb->sip_referredBy = strlib_update(ccb->sip_referredBy, pReferByStr);
        retval = TRUE;
    }

    return (retval);
}

/*
 *  Function: sipSPIBuildRegisterHeaders
 *
 *  Parameters: 
 *      ccsipCCB_t * - pointer to the ccb used for registration
 *      const char * user - used to build the headers
 *      int expires_int - registration expiry time
 *
 *  Description:  The function builds the register message
 *
 *  Returns:
 *      sipMessage_t * - pointer to the sip request
 *
 */
sipMessage_t *
sipSPIBuildRegisterHeaders(ccsipCCB_t *ccb, 
                    const char *user,
                    int expires_int)
{
    const char       fname[] = "sipSPIBuildRegisterHeaders";
    char            *sip_from_temp;
    char            *sip_to_temp;
    sipRet_t         flag  = STATUS_SUCCESS;
    sipRet_t         tflag = STATUS_SUCCESS;
    char             src_addr_str[MAX_IPADDR_STR_LEN];
    char             dest_sip_addr_str[MAX_IPADDR_STR_LEN];
    char             expires[MAX_EXPIRES_LEN];
    sipMessageFlag_t messageflag;
    char             reg_user_info[MAX_REG_USER_INFO_LEN];
    char             escaped_user[MAX_ESCAPED_USER_LEN];
    char            *sip_from_tag;
    sipMessage_t    *request = NULL;

    (void) sippmh_convertURLCharToEscChar(user, strlen(user),
                                          escaped_user, sizeof(escaped_user),
                                          TRUE);
    /* get reg_user_info */
    config_get_string(CFGID_REG_USER_INFO, reg_user_info,
                      sizeof(reg_user_info));
    ipaddr2dotted(src_addr_str, &ccb->src_addr);

    sstrncpy(dest_sip_addr_str, ccb->reg.proxy, MAX_IPADDR_STR_LEN);

    CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_MSG_SENDING_REQUEST),
                      fname, "REGISTER");

    // Create a new From header only if the previous one in the CCB is blank.
    // It will not be blank if we received a 401/407 response to a prior
    // REGISTER request where we reuse the CCB that we first used without
    // cleaning it.

    if (ccb->sip_from[0] == '\0') {
        sip_from_temp = strlib_open(ccb->sip_from, MAX_SIP_URL_LENGTH);
        if (sip_from_temp) {
            if (ccb->reg.addr.type == CPR_IP_ADDR_IPV6) {
                snprintf(sip_from_temp, MAX_SIP_URL_LENGTH, "<sip:%s@[%s]>",
                         escaped_user, dest_sip_addr_str); /* proxy */
            } else {
                snprintf(sip_from_temp, MAX_SIP_URL_LENGTH, "<sip:%s@%s>",
                         escaped_user, dest_sip_addr_str); /* proxy */
            }
            /* Now add tag to the From header */
            sip_from_tag = strlib_open(ccb->sip_from_tag, MAX_SIP_URL_LENGTH);
            if (sip_from_tag) {
                sip_util_make_tag(sip_from_tag);
                strncat(sip_from_temp, ";tag=",
                        MAX_SIP_URL_LENGTH - strlen(sip_from_temp) - 1);
                strncat(sip_from_temp, sip_from_tag,
                        MAX_SIP_URL_LENGTH - strlen(sip_from_temp) - 1);
            }
            ccb->sip_from_tag = strlib_close(sip_from_tag);
        }
        ccb->sip_from = strlib_close(sip_from_temp);
    }
    sip_to_temp = strlib_open(ccb->sip_to, MAX_SIP_URL_LENGTH);
    if (ccb->reg.addr.type == CPR_IP_ADDR_IPV6) {
        snprintf(sip_to_temp, MAX_SIP_URL_LENGTH, "<sip:%s@[%s]>",
                 escaped_user, dest_sip_addr_str); /* proxy */
    } else {
        snprintf(sip_to_temp, MAX_SIP_URL_LENGTH, "<sip:%s@%s>",
                 escaped_user, dest_sip_addr_str); /* proxy */
    }
    ccb->sip_to = strlib_close(sip_to_temp);
    /*
     * Build A request from Message Factory using following Flags...
     * You do not need to specify common headers they will automatically
     * get added
     */

    messageflag.flags = 0;

    messageflag.flags |= SIP_HEADER_CONTACT_BIT |
                         SIP_HEADER_SUPPORTED_BIT |
                         SIP_HEADER_CISCO_GUID_BIT;

    messageflag.flags |=  SIP_HEADER_CONTENT_LENGTH_BIT;


    if (ccb->authen.authorization != NULL) {
        messageflag.flags |= SIP_HEADER_AUTHENTICATION_BIT;
    }

    if (ccb->send_reason_header) {   
        messageflag.flags |= SIP_HEADER_REASON_BIT;
    }


    request = GET_SIP_MESSAGE();
    messageflag.extflags = 0;
    if (CreateRequest(ccb, messageflag, sipMethodRegister, request, FALSE, 0)) {
        tflag = HSTATUS_SUCCESS;
    } else {
        tflag = HSTATUS_FAILURE;
    }
    UPDATE_FLAGS(flag, tflag);

    snprintf(expires, sizeof(expires), "%d", expires_int);
    tflag = sippmh_add_text_header(request, SIP_HEADER_EXPIRES, expires);

    UPDATE_FLAGS(flag, tflag);

    if (flag != STATUS_SUCCESS) {
        free_sip_message(request);
        CCSIP_DEBUG_ERROR("%s: Error: REGISTER message build unsuccessful.\n",
                          fname);
        clean_method_request_trx(ccb, sipMethodRegister, TRUE);
        return (NULL);
    }

    return (request);
}

