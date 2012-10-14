/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
