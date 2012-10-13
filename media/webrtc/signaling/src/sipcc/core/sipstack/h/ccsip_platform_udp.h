/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
