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

/**
 *  @section DNS APIS
 *  This module contains method definitions to be implemented by the platform.
 *  SIP Stack uses these methods to perform DNS queries
 */


#ifndef _DNS_UTIL_H_
#define _DNS_UTIL_H_

#include "cc_constants.h"
#include "cpr_types.h"

/**
 * Defines the return values for DNS query.
 */
#define CC_DNS_OK                 0
#define CC_DNS_ERR_NOBUF          1
#define CC_DNS_ERR_INUSE          2
#define CC_DNS_ERR_TIMEOUT        3
#define CC_DNS_ERR_NOHOST         4
#define CC_DNS_ERR_HOST_UNAVAIL   5

typedef void *srv_handle_t;

/**
 * Perform an DNS lookup of the name specified
 *
 * @param[in] hname host name
 * @param[out] ipaddr_ptr the ip address returned by DNS for host with name hname.
 * @param[in] timeout the timeout for the query
 * @param[in] retries the retry times for the query
 *
 * @return CC_DNS_OK or CC_DNS_ERR_NOHOST
 */
cc_int32_t dnsGetHostByName(const char *hname,
                          cpr_ip_addr_t *ipaddr_ptr,
                          cc_int32_t timeout,
                          cc_int32_t retries);

/**
 * This function calls the dns api to perform a dns srv query.
 *
 * @param[in]  service the service name, e.g. "sip"
 * @param[in]  protocol the protocol name, e.g. "udp"
 * @param[in]  domain the domain name
 * @param[out] ipaddr_ptr the returned ip address
 * @param[out] port the port
 * @param[in]  timeout the timeout for the query
 * @param[in]  retries the number of retries for the query
 * @param[in,out] psrv_handle the handle for this query
 *
 * @note if handle is NULL an new query needs to be made and the
 * first response needs to be returned along with a valid handle.
 * Subsequent calls should return the next result if a valid handle has been provided.
 *
 * @returns CC_DNS_OK for success or
 *           CC_DNS_ERR_NOHOST, CC_DNS_ERR_HOST_UNAVAIL
 */
cc_int32_t dnsGetHostBySRV(cc_int8_t *service,
                         cc_int8_t *protocol,
                         cc_int8_t *domain,
                         cpr_ip_addr_t *ipaddr_ptr,
                         cc_uint16_t *port,
                         cc_int32_t timeout,
                         cc_int32_t retries,
                         srv_handle_t *psrv_handle);

/**
 * Free the srvhandle allocated in dnsGetHostBySRV.
 *
 * @param[in] srv_handle handle to be freed.
 *
 */

void dnsFreeSrvHandle(srv_handle_t srv_handle);

#endif /* _DNS_UTIL_H_ */
