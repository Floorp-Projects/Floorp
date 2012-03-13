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

#include "cpr_ipc.h"
#include "cpr_types.h"

#include "windows.h"

#include "dns_utils.h"
#include "phone_debug.h"
#include "util_string.h"
#include "ccsip_core.h"

/*
 *  dnsGetHostByName
 *
 *  DESCRIPTION: Perform an DNS lookup of the name specified
 *
 *  RETURNS: returns 0 for success or DNS_ERR_NOHOST
 *
 */
cc_int32_t
dnsGetHostByName (const char *hname,
                   cpr_ip_addr_t *ipaddr_ptr,
                   cc_int32_t timeout,
                   cc_int32_t retries)
{
    uint32_t ip_address;
    struct hostent *dns_response;
#ifdef IPV6_STACK_ENABLED
    uint32_t err;
    uint16_t   ip_valid;
    char     ip_addr[MAX_IPADDR_STR_LEN];
    struct addrinfo hints, *result;
    struct sockaddr sa;
#endif

#ifdef IPV6_STACK_ENABLED
    /* Check if the IPv6 address */
    ip_valid = cpr_inet_pton(AF_INET6, hname, ip_addr);

    if (ip_valid) {
        ipaddr_ptr->type = CPR_IP_ADDR_IPV6;
        cpr_memcopy(ipaddr_ptr->u.ip6.addr.base8, ip_addr, 16);
        return(0);
    }
#endif

    ip_address = inet_addr(hname);
    if (ip_address != INADDR_NONE) {
        ipaddr_ptr->u.ip4 = ip_address;
        ipaddr_ptr->type = CPR_IP_ADDR_IPV4;
        return 0;
    } else {

#ifdef IPV6_STACK_ENABLED
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_INET6;
        err = getaddrinfo(hname, NULL, &hints, &result);

        if (err) {
            return DNS_ERR_NOHOST;

        }

        while (result) {
            sa = result->ai_addr;
            if (sa->family == AF_INET) {
                ipaddr_ptr->type = CPR_IP_ADDR_IPV4;
                ip_addr->u.ip4 = ((cpr_sockaddr_in_t *)from)->sin_addr.s_addr;

            } else if (sa->family == AF_INET6) {

                //Todo IPv6: Add getaddrinfo() functioncall to get the IPv6 address.
                ipaddr_ptr->type = CPR_IP_ADDR_IPV6;
                ip_addr->u.ip6 = ((cpr_sockaddr_in6_t *)sa)->sin6_addr;
            }
        }

        freeaddrinfo(result);
#endif

        dns_response = gethostbyname(hname);
        if (dns_response) {
            ipaddr_ptr->u.ip4 = *(uint32_t *) dns_response->h_addr_list[0];
            ipaddr_ptr->type = CPR_IP_ADDR_IPV4;
            return 0;
        }


    }
    return DNS_ERR_NOHOST;
}


/*
 *  dnsGetHostBySRV
 *
 *  DESCRIPTION: This function calls the dns api to
 *  perform a dns srv query
 *
 *  RETURNS: returns 0 for success or
 *           DNS_ERR_NOHOST, DNS_ERR_HOST_UNAVAIL
 *
 */
cc_int32_t
dnsGetHostBySRV (cc_int8_t *service,
                  cc_int8_t *protocol,
                  cc_int8_t *domain,
                  cpr_ip_addr_t *ipaddr_ptr,
                  cc_uint16_t *port,
                  cc_int32_t timeout,
                  cc_int32_t retries,
                  srv_handle_t *psrv_handle)
{
    uint32_t ip_address;


    ip_address = inet_addr(domain);
    if (ip_address != INADDR_NONE) {
        ipaddr_ptr->u.ip4 = ip_address;
        ipaddr_ptr->type = CPR_IP_ADDR_IPV4;
        return 0;
    }

    return DNS_ERR_NOBUF; /* not yet implemented */
}


void
dnsFreeSrvHandle (srv_handle_t srv_handle)
{
    srv_handle = NULL;
}

