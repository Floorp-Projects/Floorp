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

#include "cpr_in.h"
#include "cpr_types.h"
#include "phone_types.h"
#include "cpr_string.h"
#include "cpr_socket.h"
#include "ccsip_platform.h"
#include "sip_common_transport.h"
#include "sip_csps_transport.h"
#include "util_string.h"

int
sip_dns_gethostbysrv (char *domain,
                      cpr_ip_addr_t *ipaddr_ptr,
                      uint16_t *port,
                      srv_handle_t *psrv_order,
                      boolean retried_addr)
{
    int      bLoopDeath = 0;
    uint16_t tmp_port = 0;
    int      rc=DNS_ERR_NOHOST;

    while (!bLoopDeath) {
        /* Try and fetch a proxy using DNS SRV records */
        rc = dnsGetHostBySRV("sip", "udp", (char *) domain, ipaddr_ptr,
                              &tmp_port, 100, 1, psrv_order);
        switch (rc) {
        case DNS_ERR_TTL_EXPIRED:
        case DNS_ERR_NOHOST:
            /* Re-try if all entries for this proxy have expired
             * or the end of the call record list was reached.
             */
            break;
        default:
            /* Run don't walk to the nearest exit */
            bLoopDeath = 1;
            break;
        }
    }
    if (tmp_port) {
        *port = tmp_port;
    }
    return rc;
}

int
sip_dns_gethostbysrvorname (char *hname,
                            cpr_ip_addr_t *ipaddr_ptr,
                            uint16_t *port)
{
    /*
     * OK according to rfc2543bis-03
     * 1. If the destination is an IP address it is used. If no port is
     *    specified then use 5060
     * 2. If the destination specifies the default port (5060) or no port
     *    then try SRV
     * 3. If the destination specifies a port number other than 5060 or
     *    there are no SRV records A record lookup
     */
    srv_handle_t srv_order = NULL;
    int rc=DNS_ERR_NOHOST;

    if ((*port == SIP_WELL_KNOWN_PORT) || (*port == 0)) {
        rc = sip_dns_gethostbysrv(hname, ipaddr_ptr, port, &srv_order, FALSE);
    }
    if (rc != DNS_OK) {
        rc = dnsGetHostByName(hname, ipaddr_ptr, 100, 1);
    }
    if (srv_order) {
        dnsFreeSrvHandle(srv_order);
    }
    return rc;
}

cpr_socket_t
sipTransportCSPSGetProxyHandleByDN (line_t dn)
{
    static const char *fname = "sipTransportCSPSGetProxyHandleByDN";
    ti_common_t *ti_common;

    if ((((int)dn) < 1) || (((int)dn) > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN %d out of "
                          "bounds.\n", fname, dn);
        return INVALID_SOCKET;
    }
    ti_common = &CSPS_Config_Table[dn - 1].ti_common;
    return ((cpr_socket_t) ti_common->handle);
}

short
sipTransportCSPSGetProxyPortByDN (line_t dn)
{
    static const char *fname = "sipTransportCSPSGetProxyPortByDN";
    ti_common_t *ti_common;

    if ((((int)dn) < 1) || (((int)dn) > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN %d out of "
                          "bounds.\n", fname, dn);
        return (-1);
    }
    ti_common = &CSPS_Config_Table[dn - 1].ti_common;
    return ((short) ti_common->port);
}

// This function is broken
uint16_t
sipTransportCSPSGetProxyAddressByDN(cpr_ip_addr_t *ip_addr, line_t dn)
{
//    static const char *fname = "sipTransportCSPSGetProxyAddressByDN";
//    ti_common_t *ti_common;
//
//    if ((((int)dn) < 1) || (((int)dn) > MAX_REG_LINES)) {
//        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN out of "
//                          "bounds.\n", fname, dn);
//        return (0);
//    }
//
//    ti_common = &CSPS_Config_Table[dn - 1].ti_common;
//
//    ip_addr = &(ti_common->addr);
//
    return(1);
}

/*
 * sip_config_get_proxy_port()
 *
 * Get table entry from the table string and option number
 */
uint16_t
sip_config_get_proxy_port (line_t line)
{
    uint32_t port;

    config_get_line_value(CFGID_PROXY_PORT, &port, sizeof(port), line);

    if (port == 0) {
        config_get_line_value(CFGID_PROXY_PORT, &port, sizeof(port), DEFAULT_LINE);
    }

    return ((uint16_t) port);
}

/*
 * sip_config_get_proxy_addr()
 *
 * Get table entry from the table string and option number
 */
void
sip_config_get_proxy_addr (line_t line, char *buffer, int buffer_len)
{
    config_get_line_string(CFGID_PROXY_ADDRESS, buffer, line, buffer_len);

    if ((strcmp(buffer, UNPROVISIONED) == 0) || (buffer[0] == '\0')) {
        config_get_line_string(CFGID_PROXY_ADDRESS, buffer, DEFAULT_LINE,
                               buffer_len);
    }
}

/*
 * sip_config_get_backup_proxy_addr()
 *
 * Get table entry from the table string and option number
 */
uint16_t
sip_config_get_backup_proxy_addr (cpr_ip_addr_t *IPAddress, char *buffer, int buffer_len)
{

    *IPAddress = ip_addr_invalid;

    config_get_string(CFGID_PROXY_BACKUP, buffer, buffer_len);

    if ((cpr_strcasecmp(buffer, UNPROVISIONED) == 0) || (buffer[0] == 0)) {
        buffer[0] = 0;
    } else {
        (void) str2ip(buffer, IPAddress);
    }
    return (1);
}

/*
 * sipTransportCSPSClearProxyHandle
 *
 * Clear Proxy handle from the table
 */
void
sipTransportCSPSClearProxyHandle (cpr_ip_addr_t *ipaddr,
                                  uint16_t port,
                                  cpr_socket_t this_fd)
{
    ti_common_t *ti_common;
    int i;

    for (i = 0; i < MAX_REG_LINES; i++) {
        ti_common = &CSPS_Config_Table[i].ti_common;
        if ((ti_common->port == port) &&
            util_compare_ip(&(ti_common->addr),ipaddr) &&
            (ti_common->handle == this_fd)) {
            ti_common->handle = INVALID_SOCKET;
            return;
        }
    }
}
