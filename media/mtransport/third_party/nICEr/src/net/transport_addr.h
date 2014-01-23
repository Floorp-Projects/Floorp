/*
Copyright (c) 2007, Adobe Systems, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/



#ifndef _transport_addr_h
#define _transport_addr_h

#include <sys/types.h>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#ifdef WIN32
#define MAXIFNAME IFNAMSIZ
#else
#define MAXIFNAME 16
#endif

/* Generic transport address

   This spans both sockaddr_in and sockaddr_in6
 */
typedef struct nr_transport_addr_ {
  UCHAR ip_version;  /* 4 or 6 */
#define NR_IPV4 4
#define NR_IPV6 6
  UCHAR protocol;    /* IPPROTO_TCP, IPPROTO_UDP */
  struct sockaddr *addr;
  int addr_len;
  union {
    struct sockaddr_in addr4;
    struct sockaddr_in6 addr6;
  } u;
  char ifname[MAXIFNAME];
  /* A string version.
     56 = 5 ("IP6:[") + 39 (ipv6 address) + 2 ("]:") + 5 (port) + 4 (/UDP) + 1 (null) */
  char as_string[56];
} nr_transport_addr;

int nr_sockaddr_to_transport_addr(struct sockaddr *saddr, int saddr_len, int protocol, int keep, nr_transport_addr *addr);

// addresses, ports in local byte order
int nr_ip4_port_to_transport_addr(UINT4 ip4, UINT2 port, int protocol, nr_transport_addr *addr);
int nr_ip4_str_port_to_transport_addr(const char *ip4, UINT2 port, int protocol, nr_transport_addr *addr);

int nr_transport_addr_get_addrstring(nr_transport_addr *addr, char *str, int maxlen);
int nr_transport_addr_get_port(nr_transport_addr *addr, int *port);
int nr_transport_addr_get_ip4(nr_transport_addr *addr, UINT4 *ip4p);
int nr_transport_addr_cmp(nr_transport_addr *addr1,nr_transport_addr *addr2,int mode);
#define NR_TRANSPORT_ADDR_CMP_MODE_VERSION   1
#define NR_TRANSPORT_ADDR_CMP_MODE_PROTOCOL  2
#define NR_TRANSPORT_ADDR_CMP_MODE_ADDR      3
#define NR_TRANSPORT_ADDR_CMP_MODE_ALL       4

int nr_transport_addr_is_wildcard(nr_transport_addr *addr);
int nr_transport_addr_is_loopback(nr_transport_addr *addr);
int nr_transport_addr_copy(nr_transport_addr *to, nr_transport_addr *from);
int nr_transport_addr_copy_keep_ifname(nr_transport_addr *to, nr_transport_addr *from);
int nr_transport_addr_fmt_addr_string(nr_transport_addr *addr);
int nr_transport_addr_fmt_ifname_addr_string(const nr_transport_addr *addr, char *buf, int len);
int nr_transport_addr_set_port(nr_transport_addr *addr, int port);

#endif

