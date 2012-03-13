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

#ifndef _UTIL_STRING_H_
#define _UTIL_STRING_H_

#include "cpr_types.h"
#include "cpr_socket.h"

#define EMPTY_STR ""
#define EMPTY_STR_LEN 1

boolean is_empty_str(char *str);
void init_empty_str(char *str); 

void ipaddr2dotted(char *addr_str, cpr_ip_addr_t *addr);
uint32_t dotted2ipaddr(const char *addr_str);
int str2ip(const char *str, cpr_ip_addr_t *addr);
void util_ntohl(cpr_ip_addr_t *ip_addr_out, cpr_ip_addr_t *ip_addr_in);
boolean util_compare_ip(cpr_ip_addr_t *ip_add1, cpr_ip_addr_t *ip_addr2);
void util_extract_ip(cpr_ip_addr_t *ip_addr, cpr_sockaddr_storage *from);
uint16_t util_get_port(cpr_sockaddr_storage *sock_storage);
void util_get_ip_using_mode(cpr_ip_addr_t *ip_addr, cpr_ip_mode_e ip_mode,
                             uint32_t  ip4, char *ip6);
boolean util_check_if_ip_valid(cpr_ip_addr_t *ip_addr);

#endif
