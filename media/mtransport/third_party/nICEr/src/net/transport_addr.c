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



static char *RCSSTRING __UNUSED__="$Id: transport_addr.c,v 1.2 2008/04/28 17:59:03 ekr Exp $";


#include <csi_platform.h>
#include <stdio.h>
#include <memory.h>
#include <sys/types.h>
#include <errno.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <assert.h>
#include "nr_api.h"
#include "transport_addr.h"

int nr_transport_addr_fmt_addr_string(nr_transport_addr *addr)
  {
    int _status;
    /* Max length for normalized IPv6 address string represntation is 39 */
    char buffer[40];
    const char *protocol;

    switch(addr->protocol){
      case IPPROTO_TCP:
        protocol = "TCP";
        break;
      case IPPROTO_UDP:
        protocol = "UDP";
        break;
      default:
        ABORT(R_INTERNAL);
    }

    switch(addr->ip_version){
      case NR_IPV4:
        if (!inet_ntop(AF_INET, &addr->u.addr4.sin_addr,buffer,sizeof(buffer)))
          strcpy(buffer, "[error]");
        snprintf(addr->as_string,sizeof(addr->as_string),"IP4:%s:%d/%s",buffer,(int)ntohs(addr->u.addr4.sin_port),protocol);
        break;
      case NR_IPV6:
        if (!inet_ntop(AF_INET6, &addr->u.addr6.sin6_addr,buffer,sizeof(buffer)))
          strcpy(buffer, "[error]");
        snprintf(addr->as_string,sizeof(addr->as_string),"IP6:[%s]:%d/%s",buffer,(int)ntohs(addr->u.addr6.sin6_port),protocol);
        break;
      default:
        ABORT(R_INTERNAL);
    }

    _status=0;
  abort:
    return(_status);
  }

int nr_transport_addr_fmt_ifname_addr_string(const nr_transport_addr *addr, char *buf, int len)
  {
    int _status;
    char buffer[40];

    switch(addr->ip_version){
      case NR_IPV4:
        if (!inet_ntop(AF_INET, &addr->u.addr4.sin_addr,buffer,sizeof(buffer))) {
           strncpy(buffer, "[error]", len);
        }
        break;
      case NR_IPV6:
        if (!inet_ntop(AF_INET6, &addr->u.addr6.sin6_addr,buffer,sizeof(buffer))) {
           strncpy(buffer, "[error]", len);
        }
        break;
      default:
        ABORT(R_INTERNAL);
    }
    snprintf(buf,len,"%s:%s",addr->ifname,buffer);

    _status=0;
  abort:
    return(_status);
  }

int nr_sockaddr_to_transport_addr(struct sockaddr *saddr, int protocol, int keep, nr_transport_addr *addr)
  {
    int r,_status;

    if(!keep) memset(addr,0,sizeof(nr_transport_addr));

    switch(protocol){
      case IPPROTO_TCP:
      case IPPROTO_UDP:
        break;
      default:
        ABORT(R_BAD_ARGS);
    }

    addr->protocol=protocol;

    if(saddr->sa_family==AF_INET){
      addr->ip_version=NR_IPV4;

      memcpy(&addr->u.addr4,saddr,sizeof(struct sockaddr_in));
      addr->addr=(struct sockaddr *)&addr->u.addr4;
      addr->addr_len=sizeof(struct sockaddr_in);
    }
    else if(saddr->sa_family==AF_INET6){
      addr->ip_version=NR_IPV6;

      memcpy(&addr->u.addr6, saddr, sizeof(struct sockaddr_in6));
      addr->addr=(struct sockaddr *)&addr->u.addr6;
      addr->addr_len=sizeof(struct sockaddr_in6);
    }
    else
      ABORT(R_BAD_ARGS);

    if(r=nr_transport_addr_fmt_addr_string(addr))
      ABORT(r);

    _status=0;
  abort:
    return(_status);
  }


int nr_transport_addr_copy(nr_transport_addr *to, nr_transport_addr *from)
  {
    memcpy(to,from,sizeof(nr_transport_addr));
    to->addr=(struct sockaddr *)((char *)to + ((char *)from->addr - (char *)from));

    return(0);
  }

int nr_transport_addr_copy_keep_ifname(nr_transport_addr *to, nr_transport_addr *from)
  {
    int r,_status;
    char save_ifname[MAXIFNAME];

    strncpy(save_ifname, to->ifname, MAXIFNAME);
    save_ifname[MAXIFNAME-1]=0;  /* Ensure null termination */

    if (r=nr_transport_addr_copy(to, from))
      ABORT(r);

    strncpy(to->ifname, save_ifname, MAXIFNAME);

    if (r=nr_transport_addr_fmt_addr_string(to))
      ABORT(r);

    _status=0;
 abort:
    return _status;
  }

/* Convenience fxn. Is this the right API?*/
int nr_ip4_port_to_transport_addr(UINT4 ip4, UINT2 port, int protocol, nr_transport_addr *addr)
  {
    int r,_status;

    memset(addr, 0, sizeof(nr_transport_addr));

    addr->ip_version=NR_IPV4;
    addr->protocol=protocol;
#ifdef HAVE_SIN_LEN
    addr->u.addr4.sin_len=sizeof(struct sockaddr_in);
#endif
    addr->u.addr4.sin_family=PF_INET;
    addr->u.addr4.sin_port=htons(port);
    addr->u.addr4.sin_addr.s_addr=htonl(ip4);
    addr->addr=(struct sockaddr *)&addr->u.addr4;
    addr->addr_len=sizeof(struct sockaddr_in);

    if(r=nr_transport_addr_fmt_addr_string(addr))
      ABORT(r);

    _status=0;
  abort:
    return(_status);
  }

int nr_str_port_to_transport_addr(const char *ip, UINT2 port, int protocol, nr_transport_addr *addr_out)
  {
    int r,_status;
    struct in_addr addr;
    struct in6_addr addr6;

    if (inet_pton(AF_INET, ip, &addr) == 1) {
      if(r=nr_ip4_port_to_transport_addr(ntohl(addr.s_addr),port,protocol,addr_out))
        ABORT(r);
    } else if (inet_pton(AF_INET6, ip, &addr6) == 1) {
      if(r=nr_ip6_port_to_transport_addr(&addr6,port,protocol,addr_out))
        ABORT(r);
    } else {
      ABORT(R_BAD_DATA);
    }

    _status=0;
  abort:
    return(_status);
  }

int nr_ip6_port_to_transport_addr(struct in6_addr* addr6, UINT2 port, int protocol, nr_transport_addr *addr)
  {
    int r,_status;

    memset(addr, 0, sizeof(nr_transport_addr));

    addr->ip_version=NR_IPV6;
    addr->protocol=protocol;
    addr->u.addr6.sin6_family=PF_INET6;
    addr->u.addr6.sin6_port=htons(port);
    memcpy(addr->u.addr6.sin6_addr.s6_addr, addr6->s6_addr, sizeof(addr6->s6_addr));
    addr->addr=(struct sockaddr *)&addr->u.addr6;
    addr->addr_len=sizeof(struct sockaddr_in6);

    if(r=nr_transport_addr_fmt_addr_string(addr))
      ABORT(r);

    _status=0;
  abort:
    return(_status);
  }

int nr_transport_addr_get_addrstring(const nr_transport_addr *addr, char *str, int maxlen)
  {
    int _status;
    const char *res;

    switch(addr->ip_version){
      case NR_IPV4:
        res = inet_ntop(AF_INET, &addr->u.addr4.sin_addr,str,maxlen);
        break;
      case NR_IPV6:
        res = inet_ntop(AF_INET6, &addr->u.addr6.sin6_addr,str,maxlen);
        break;
      default:
        ABORT(R_INTERNAL);
    }

    if(!res){
      if (errno == ENOSPC){
        ABORT(R_BAD_ARGS);
      }
      ABORT(R_INTERNAL);
    }

    _status=0;
  abort:
    return(_status);
  }

int nr_transport_addr_get_port(nr_transport_addr *addr, int *port)
  {
    int _status;

    switch(addr->ip_version){
      case NR_IPV4:
        *port=ntohs(addr->u.addr4.sin_port);
        break;
      case NR_IPV6:
        *port=ntohs(addr->u.addr6.sin6_port);
        break;
      default:
        ABORT(R_INTERNAL);
    }

    _status=0;
  abort:
    return(_status);
  }

int nr_transport_addr_set_port(nr_transport_addr *addr, int port)
  {
    int _status;

    switch(addr->ip_version){
      case NR_IPV4:
        addr->u.addr4.sin_port=htons(port);
        break;
      case NR_IPV6:
        addr->u.addr6.sin6_port=htons(port);
        break;
      default:
        ABORT(R_INTERNAL);
    }

    _status=0;
  abort:
    return(_status);
  }

/* memcmp() may not work if, for instance, the string or interface
   haven't been made. Hmmm.. */
int nr_transport_addr_cmp(nr_transport_addr *addr1,nr_transport_addr *addr2,int mode)
  {
    assert(mode);

    if(addr1->ip_version != addr2->ip_version)
      return(1);

    if(mode < NR_TRANSPORT_ADDR_CMP_MODE_PROTOCOL)
      return(0);

    if(addr1->protocol != addr2->protocol)
      return(1);

    if(mode < NR_TRANSPORT_ADDR_CMP_MODE_ADDR)
      return(0);

    assert(addr1->addr_len == addr2->addr_len);
    switch(addr1->ip_version){
      case NR_IPV4:
        if(addr1->u.addr4.sin_addr.s_addr != addr2->u.addr4.sin_addr.s_addr)
          return(1);
        if(mode < NR_TRANSPORT_ADDR_CMP_MODE_ALL)
          return(0);
        if(addr1->u.addr4.sin_port != addr2->u.addr4.sin_port)
          return(1);
        break;
      case NR_IPV6:
        if(memcmp(addr1->u.addr6.sin6_addr.s6_addr,addr2->u.addr6.sin6_addr.s6_addr,sizeof(struct in6_addr)))
          return(1);
        if(mode < NR_TRANSPORT_ADDR_CMP_MODE_ALL)
          return(0);
        if(addr1->u.addr6.sin6_port != addr2->u.addr6.sin6_port)
          return(1);
        break;
      default:
        abort();
    }

    return(0);
  }

int nr_transport_addr_is_loopback(nr_transport_addr *addr)
  {
    switch(addr->ip_version){
      case NR_IPV4:
        switch(addr->u.addr4.sin_family){
          case AF_INET:
            if (((ntohl(addr->u.addr4.sin_addr.s_addr)>>24)&0xff)==0x7f)
              return 1;
            break;
          default:
            UNIMPLEMENTED;
            break;
        }
        break;

      case NR_IPV6:
        if(!memcmp(addr->u.addr6.sin6_addr.s6_addr,in6addr_loopback.s6_addr,sizeof(struct in6_addr)))
          return(1);
        break;
      default:
        UNIMPLEMENTED;
    }

    return(0);
  }

int nr_transport_addr_is_link_local(nr_transport_addr *addr)
  {
    if(addr->ip_version == NR_IPV6){
      UINT4* addrTop = (UINT4*)(addr->u.addr6.sin6_addr.s6_addr);
      return ((*addrTop & htonl(0xFFC00000)) == htonl(0xFE800000));
    } else {
      assert(0);
    }

    return(0);
  }

int nr_transport_addr_is_wildcard(nr_transport_addr *addr)
  {
    switch(addr->ip_version){
      case NR_IPV4:
        if(addr->u.addr4.sin_addr.s_addr==INADDR_ANY)
          return(1);
        if(addr->u.addr4.sin_port==0)
          return(1);
        break;
      case NR_IPV6:
        if(!memcmp(addr->u.addr6.sin6_addr.s6_addr,in6addr_any.s6_addr,sizeof(struct in6_addr)))
          return(1);
        if(addr->u.addr6.sin6_port==0)
          return(1);
        break;
      default:
        UNIMPLEMENTED;
    }

    return(0);
  }
