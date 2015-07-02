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



static char *RCSSTRING __UNUSED__="$Id: transport_addr_reg.c,v 1.2 2008/04/28 17:59:03 ekr Exp $";

#include <csi_platform.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <sys/types.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <assert.h>
#include "nr_api.h"
#include "transport_addr.h"
#include "transport_addr_reg.h"

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46 /* Value used by linux/BSD */
#endif

int
nr_reg_get_transport_addr(NR_registry prefix, int keep, nr_transport_addr *addr)
{
    int r,_status;
    unsigned int count;
    char *address = 0;
    UINT2 port = 0;
    char *ifname = 0;
    char *protocol = 0;
    int p;

    if ((r=NR_reg_get_child_count(prefix, &count)))
        ABORT(r);

    if (count == 0)
        ABORT(R_NOT_FOUND);

    if ((r=NR_reg_alloc2_string(prefix, "address", &address))) {
        if (r != R_NOT_FOUND)
            ABORT(r);
        address = 0;
    }

    if ((r=NR_reg_alloc2_string(prefix, "ifname", &ifname))) {
        if (r != R_NOT_FOUND)
            ABORT(r);
        ifname = 0;
    }

    if ((r=NR_reg_get2_uint2(prefix, "port", &port))) {
        if (r != R_NOT_FOUND)
            ABORT(r);
        port = 0;
    }

    if ((r=NR_reg_alloc2_string(prefix, "protocol", &protocol))) {
        if (r != R_NOT_FOUND)
            ABORT(r);
        p = IPPROTO_UDP;

        protocol = 0;
    }
    else {
        if (!strcasecmp("tcp", protocol))
            p = IPPROTO_TCP;
        else if (!strcasecmp("udp", protocol))
            p = IPPROTO_UDP;
        else
            ABORT(R_BAD_DATA);
    }

    if (!keep) memset(addr, 0, sizeof(*addr));

    if ((r=nr_str_port_to_transport_addr(address?address:"0.0.0.0", port, p, addr)))
        ABORT(r);

    if (ifname)
        strlcpy(addr->ifname, ifname, sizeof(addr->ifname));

    _status=0;
  abort:
    RFREE(protocol);
    RFREE(ifname);
    RFREE(address);
    return(_status);
}

int
nr_reg_set_transport_addr(NR_registry prefix, int keep, nr_transport_addr *addr)
{
    int r,_status;

    if (! keep) {
        if ((r=NR_reg_del(prefix)))
            ABORT(r);
    }

    switch (addr->ip_version) {
    case NR_IPV4:
        if (!nr_transport_addr_is_wildcard(addr)) {
            if ((r=NR_reg_set2_string(prefix, "address", inet_ntoa(addr->u.addr4.sin_addr))))
                ABORT(r);
        }

        if (addr->u.addr4.sin_port != 0) {
            if ((r=NR_reg_set2_uint2(prefix, "port", ntohs(addr->u.addr4.sin_port))))
                ABORT(r);
        }
        break;

    case NR_IPV6:
        if (!nr_transport_addr_is_wildcard(addr)) {
          char address[INET6_ADDRSTRLEN];
          if(!inet_ntop(AF_INET6, &addr->u.addr6.sin6_addr,address,sizeof(address))) {
            ABORT(R_BAD_DATA);
          }

          if ((r=NR_reg_set2_string(prefix, "address", address))) {
            ABORT(r);
          }
        }

        if (addr->u.addr6.sin6_port != 0) {
            if ((r=NR_reg_set2_uint2(prefix, "port", ntohs(addr->u.addr6.sin6_port))))
                ABORT(r);
        }
        break;
    default:
        ABORT(R_INTERNAL);
        break;
    }

    /* We abort if neither NR_IPV4 or NR_IPV6 above */
    switch (addr->protocol) {
      case IPPROTO_TCP:
        if ((r=NR_reg_set2_string(prefix, "protocol", "tcp")))
          ABORT(r);
        break;
      case IPPROTO_UDP:
        if ((r=NR_reg_set2_string(prefix, "protocol", "udp")))
          ABORT(r);
        break;
      default:
        UNIMPLEMENTED;
        break;
    }

    if (strlen(addr->ifname) > 0) {
      if ((r=NR_reg_set2_string(prefix, "ifname", addr->ifname)))
        ABORT(r);
    }

    _status=0;
  abort:
    if (_status)
        NR_reg_del(prefix);
    return _status;
}

int
nr_reg_get_transport_addr2(NR_registry prefix, char *name, int keep, nr_transport_addr *addr)
{
    int r, _status;
    NR_registry registry;

    if ((r=NR_reg_make_registry(prefix, name, registry)))
        ABORT(r);

    if ((r=nr_reg_get_transport_addr(registry, keep, addr)))
        ABORT(r);

    _status = 0;
abort:
    return _status;
}

int
nr_reg_set_transport_addr2(NR_registry prefix, char *name, int keep, nr_transport_addr *addr)
{
    int r, _status;
    NR_registry registry;

    if ((r=NR_reg_make_registry(prefix, name, registry)))
        ABORT(r);

    if ((r=nr_reg_set_transport_addr(registry, keep, addr)))
        ABORT(r);

    _status = 0;
abort:
    return _status;
}

