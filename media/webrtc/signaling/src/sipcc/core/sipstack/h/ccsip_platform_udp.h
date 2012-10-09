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

#ifndef _CCSIP_PLATFORM_UDP_H_
#define _CCSIP_PLATFORM_UDP_H_

#include "cpr_types.h"
#include "cpr_socket.h"
#include "cpr_memory.h"

int
sip_platform_udp_channel_listen(cpr_ip_mode_e ip_mode, cpr_socket_t *s,
                                cpr_ip_addr_t *local_ipaddr,
                                uint16_t local_port);
int
sip_platform_udp_channel_accept(cpr_socket_t listenSoc,
                                cpr_socket_t *s);
int
sip_platform_udp_channel_create(cpr_ip_mode_e ip_mode, cpr_socket_t *s,
                                cpr_ip_addr_t *remote_ipaddr,
                                uint16_t remote_port,
                                uint32_t local_udp_port);
int
sip_platform_udp_channel_destroy(cpr_socket_t s);
int
sip_platform_udp_channel_send(cpr_socket_t s,
                              char *buf,
                              uint16_t buf_len);
void
sip_platform_udp_read_socket(cpr_socket_t s);

int
sip_platform_udp_channel_sendto(cpr_socket_t s,
                                char *buf,
                                uint32_t buf_len,
                                cpr_ip_addr_t *dst_addr,
                                uint16_t dst_port);
int
sip_platform_udp_channel_read(cpr_socket_t s,
                              cprBuffer_t buf,
                              uint16_t *len,
                              cpr_sockaddr_t *soc_addr,
                              cpr_socklen_t *soc_addr_len);

#endif
