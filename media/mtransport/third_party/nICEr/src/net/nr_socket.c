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



static char *RCSSTRING __UNUSED__="$Id: nr_socket.c,v 1.2 2008/04/28 17:59:02 ekr Exp $";

#include <assert.h>
#include <nr_api.h>
#include "nr_socket.h"
#include "local_addr.h"

#define CHECK_DEFINED(f) assert(sock->vtbl->f); if (!sock->vtbl->f) ERETURN(R_INTERNAL);
int nr_socket_create_int(void *obj, nr_socket_vtbl *vtbl, nr_socket **sockp)
  {
    int _status;
    nr_socket *sock=0;

    if(!(sock=RCALLOC(sizeof(nr_socket))))
      ABORT(R_NO_MEMORY);

    assert(vtbl->version == 1);
    if (vtbl->version != 1)
       ABORT(R_INTERNAL);

    sock->obj=obj;
    sock->vtbl=vtbl;

    *sockp=sock;

    _status=0;
  abort:
    return(_status);
  }

int nr_socket_destroy(nr_socket **sockp)
  {
    nr_socket *sock;

    if(!sockp || !*sockp)
      return(0);


    sock=*sockp;
    *sockp=0;

    CHECK_DEFINED(destroy);

    assert(sock->vtbl);
    if (sock->vtbl)
      sock->vtbl->destroy(&sock->obj);

    RFREE(sock);

    return(0);
  }

int nr_socket_sendto(nr_socket *sock,const void *msg, size_t len, int flags,
  nr_transport_addr *addr)
  {
    CHECK_DEFINED(ssendto);
    return sock->vtbl->ssendto(sock->obj,msg,len,flags,addr);
  }

int nr_socket_recvfrom(nr_socket *sock,void * restrict buf, size_t maxlen,
  size_t *len, int flags, nr_transport_addr *addr)
  {
    CHECK_DEFINED(srecvfrom);
    return sock->vtbl->srecvfrom(sock->obj, buf, maxlen, len, flags, addr);
  }

int nr_socket_getfd(nr_socket *sock, NR_SOCKET *fd)
  {
    CHECK_DEFINED(getfd);
    return sock->vtbl->getfd(sock->obj, fd);
  }

int nr_socket_getaddr(nr_socket *sock, nr_transport_addr *addrp)
  {
    CHECK_DEFINED(getaddr);
    return sock->vtbl->getaddr(sock->obj, addrp);
  }

int nr_socket_close(nr_socket *sock)
  {
    CHECK_DEFINED(close);
    return sock->vtbl->close(sock->obj);
  }

int nr_socket_connect(nr_socket *sock, nr_transport_addr *addr)
  {
    CHECK_DEFINED(connect);
    return sock->vtbl->connect(sock->obj, addr);
  }

int nr_socket_write(nr_socket *sock,const void *msg, size_t len, size_t *written, int flags)
  {
    CHECK_DEFINED(swrite);
    return sock->vtbl->swrite(sock->obj, msg, len, written);
  }


int nr_socket_read(nr_socket *sock,void * restrict buf, size_t maxlen,
  size_t *len, int flags)
  {
    CHECK_DEFINED(sread);
    return sock->vtbl->sread(sock->obj, buf, maxlen, len);
  }
