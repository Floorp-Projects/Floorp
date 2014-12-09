/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_in.h"
#include "ccsip_common_cb.h"
#include "sip_common_transport.h"
#include "prot_configmgr.h"
#include "ccsip_register.h"
#include "util_string.h"

/**
 * This function will set dest ip and port in common control block of SCB and PCB.
 *
 * @param[in] cb_p - pointer to the header control block.
 *
 * @return none
 *
 * @pre     (cb_p != NULL)
 */
void ccsip_common_util_set_dest_ipaddr_port (ccsip_common_cb_t *cb_p)
{
    char            addr[MAX_IPADDR_STR_LEN];

    if (cb_p->dest_sip_addr.type == CPR_IP_ADDR_INVALID) {
        sipTransportGetPrimServerAddress(cb_p->dn_line, addr);
        dns_error_code = sipTransportGetServerAddrPort(addr,
                                                       &cb_p->dest_sip_addr,
                                                       (uint16_t *)&cb_p->dest_sip_port,
                                                       &cb_p->SRVhandle,
                                                       FALSE);
        if (dns_error_code == 0) {
            util_ntohl(&(cb_p->dest_sip_addr), &(cb_p->dest_sip_addr));
        } else {
            sipTransportGetServerIPAddr(&(cb_p->dest_sip_addr), cb_p->dn_line);
        }

        cb_p->dest_sip_port = ((dns_error_code == 0) && (cb_p->dest_sip_port)) ?
                              ntohs((uint16_t)cb_p->dest_sip_port) :
                              (sipTransportGetPrimServerPort(cb_p->dn_line));
    }
}

/**
 * This function will set source ip  in common control block of SCB and PCB.
 *
 * @param[in] cb_p - pointer to the header control block.
 *
 * @return none
 *
 * @pre     (cb_p != NULL)
 */
void ccsip_common_util_set_src_ipaddr (ccsip_common_cb_t *cb_p)
{
    int             nat_enable = 0;

    config_get_value(CFGID_NAT_ENABLE, &nat_enable, sizeof(nat_enable));
    if (nat_enable == 0) {
        sip_config_get_net_device_ipaddr(&(cb_p->src_addr));
    } else {
        sip_config_get_nat_ipaddr(&(cb_p->src_addr));
    }
}

/*
 * This function will set retry settings in common control block of SCB and PCB.
 *
 * Description: Based on transport used, determines the value to be
 *     used to set either TimerE or TimerF
 *     1. For reliable tranport: we SHOULD start timer F (= 64*T1),
 *        no retransmits are required.
 *     2. To prevent retransmits for TCP/TLS, we set scbp->retx_counter
 *        to Max value.
 *     3. For unreliable tranport: we SHOULD start timer E (= T1),
 *        retransmits are required.
 *     This routine must only be invoked before sending a request for
 *     the first time with valid args.
 *
 * @param[in] cb_p - pointer to header control block.
 * @param[out] timeout_p - returns the value to be used to set a timer.
 *
 * @return none
 *
 * @pre     (cb_p != NULL) and (timeout_p != NULL)
 */
void ccsip_common_util_set_retry_settings (ccsip_common_cb_t *cb_p, int *timeout_p)
{
    uint32_t max_retx = 0;
    const char *transport = NULL;

    *timeout_p = 0;
    cb_p->retx_flag = TRUE;
    config_get_value(CFGID_TIMER_T1, timeout_p, sizeof(*timeout_p));

    transport = sipTransportGetTransportType(cb_p->dn_line, TRUE, NULL);
    if (transport) {
        if (strcmp(transport, "UDP") == 0) {
            cb_p->retx_counter = 0;
        } else {
            config_get_value(CFGID_SIP_RETX, &max_retx, sizeof(max_retx));
            if (max_retx > MAX_NON_INVITE_RETRY_ATTEMPTS) {
                max_retx = MAX_NON_INVITE_RETRY_ATTEMPTS;
            }
            cb_p->retx_counter = max_retx;
            (*timeout_p) = (64 * (*timeout_p));
        }
    }
}


/**
 * This function will generate authorization header value.
 *
 * @param[in] pSipMessage - pointer to sipMessage_t
 * @param[in] cb_p - pointer to header control block.
 * @param[in] rsp_method - response method
 * @param[in] response_code - response code
 * @param[in] uri - uri
 *
 * @return TRUE if it is successful.
 *
 * @pre     (cb_p != NULL) and (pSipMessage != NULL) and (rsp_method != NULL) and (uri != NULL)
 */
boolean ccsip_common_util_generate_auth (sipMessage_t *pSipMessage, ccsip_common_cb_t *cb_p,
                                         const char *rsp_method, int response_code, char *uri)
{
    static const char fname[] = "ccsip_common_util_generate_auth";
    const char     *authenticate = NULL;
    credentials_t   credentials;
    sip_authen_t   *sip_authen = NULL;
    char           *author_str = NULL;

    if (!(cb_p->authen.cred_type & CRED_LINE)) {
        cb_p->authen.cred_type |= CRED_LINE;
    } else {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX
                          "configured credentials for line %d not accepeted. Verify the config\n",
                          fname, cb_p->dn_line);
        return FALSE;
    }

    /*
     * get authname & password from configuration.
     */
    cred_get_line_credentials(cb_p->dn_line, &credentials,
                              sizeof(credentials.id),
                              sizeof(credentials.pw));
    /*
     * Extract Authenticate/Proxy-Authenticate header from the message.
     */
    authenticate = sippmh_get_header_val(pSipMessage, AUTH_HDR(response_code), NULL);
    if (authenticate == NULL) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"%s header missing in the %d response",
                          fname, AUTH_HDR_STR(response_code), response_code);
        return FALSE;
    }
    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Authenticate header %s = %s", DEB_F_PREFIX_ARGS(SIP_AUTH, fname), AUTH_HDR_STR(response_code), authenticate);
    /*
     * Parse Authenticate header.
     */
    sip_authen = sippmh_parse_authenticate(authenticate);
    if (sip_authen == NULL) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"%s:%s header formatted incorrectly in the %d response",
                          fname, AUTH_HDR_STR(response_code), authenticate, response_code);
        return FALSE;
    }
    cb_p->authen.new_flag = FALSE;
    cb_p->authen.cnonce[0] = '\0';
    /*
     * Generate Authorization string.
     */
    if (sipSPIGenerateAuthorizationResponse(sip_authen,
                                            uri,
                                            rsp_method,
                                            credentials.id,
                                            credentials.pw,
                                            &author_str,
                                            &(cb_p->authen.nc_count),
                                            NULL) == TRUE) {

        if (cb_p->authen.authorization != NULL) {
            cpr_free(cb_p->authen.authorization);
            cb_p->authen.authorization = NULL;
        }

        if (cb_p->authen.sip_authen != NULL) {
            sippmh_free_authen(cb_p->authen.sip_authen);
            cb_p->authen.sip_authen = NULL;
        }

        cb_p->authen.authorization = (char *)
        cpr_malloc(strlen(author_str) * sizeof(char) + 1);

        /*
         * Cache the Authorization header so that it can be
         * used for later requests
         */
        if (cb_p->authen.authorization != NULL) {
            memcpy(cb_p->authen.authorization, author_str,
                   strlen(author_str) * sizeof(char) + 1);
            cb_p->authen.status_code = response_code;
            cb_p->authen.sip_authen = sip_authen;
        }

        cpr_free(author_str);
    } else {
         CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Authorization header build unsuccessful", fname);
         sippmh_free_authen(sip_authen);
         return FALSE;
    }
    return TRUE;
}

/**
 * This function will extract user part from sip From header.
 *
 * @param[in] pSipMessage - pointer to sipMessage_t
 * @param[out] entity - pointer to user buffer.
 *
 * @return void
 *
 * @pre     (pSipMessage != NULL) and (entity != NULL)
 */
void ccsip_util_get_from_entity (sipMessage_t *pSipMessage, char *entity)
{
    const char     *sip_from = NULL;
    sipLocation_t  *from_loc = NULL;

    sip_from = sippmh_get_cached_header_val(pSipMessage, FROM);
    if (sip_from != NULL) {
        from_loc = sippmh_parse_from_or_to((char *) sip_from, TRUE);
        if ((from_loc) && (from_loc->genUrl->schema == URL_TYPE_SIP) && (from_loc->genUrl->u.sipUrl->user)) {
            sstrncpy(entity, from_loc->genUrl->u.sipUrl->user, CC_MAX_DIALSTRING_LEN);
        }
    }
    if (from_loc) {
        sippmh_free_location(from_loc);
    }
}

/**
 * This function will extract user part from sip URL.
 *
 * @param[in] url - pointer to URL
 * @param[out] user - pointer to user buffer.
 *
 * @return void
 *
 * @pre     (url != NULL) and (user != NULL)
 */
void ccsip_util_extract_user (char *url, char *user)
{
    genUrl_t *genUrl = NULL;

    genUrl = sippmh_parse_url(url, TRUE);
    if (genUrl != NULL) {
        sstrncpy(user, genUrl->u.sipUrl->user, CC_MAX_DIALSTRING_LEN);
        sippmh_genurl_free(genUrl);
    }
}
