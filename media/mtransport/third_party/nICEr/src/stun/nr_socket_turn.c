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

#ifdef USE_TURN

#include <csi_platform.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>

#include "stun.h"
#include "turn_client_ctx.h"
#include "nr_socket_turn.h"


static char *nr_socket_turn_magic_cookie = "nr_socket_turn";

typedef struct nr_socket_turn_ {
  char *magic_cookie;
  nr_turn_client_ctx *turn;
} nr_socket_turn;


static int nr_socket_turn_destroy(void **objp);
static int nr_socket_turn_sendto(void *obj,const void *msg, size_t len,
  int flags, nr_transport_addr *to);
static int nr_socket_turn_recvfrom(void *obj,void * restrict buf,
  size_t maxlen, size_t *len, int flags, nr_transport_addr *from);
static int nr_socket_turn_getfd(void *obj, NR_SOCKET *fd);
static int nr_socket_turn_getaddr(void *obj, nr_transport_addr *addrp);
static int nr_socket_turn_close(void *obj);

static nr_socket_vtbl nr_socket_turn_vtbl={
  2,
  nr_socket_turn_destroy,
  nr_socket_turn_sendto,
  nr_socket_turn_recvfrom,
  nr_socket_turn_getfd,
  nr_socket_turn_getaddr,
  0,
  0,
  0,
  nr_socket_turn_close,
  0,
  0
};

int nr_socket_turn_create(nr_socket *sock, nr_socket **sockp)
  {
    int r,_status;
    nr_socket_turn *sturn=0;

    if(!(sturn=RCALLOC(sizeof(nr_socket_turn))))
      ABORT(R_NO_MEMORY);

    sturn->magic_cookie = nr_socket_turn_magic_cookie;

    if(r=nr_socket_create_int(sturn, &nr_socket_turn_vtbl, sockp))
      ABORT(r);

    _status=0;
  abort:
    if(_status){
      nr_socket_turn_destroy((void **)&sturn);
    }
    return(_status);
  }

static int nr_socket_turn_destroy(void **objp)
  {
    int _status;
    nr_socket_turn *sturn;

    if(!objp || !*objp)
      return(0);

    sturn=*objp;
    *objp=0;

    assert(sturn->magic_cookie == nr_socket_turn_magic_cookie);

    /* we don't own the socket, so don't destroy it */

    RFREE(sturn);

    _status=0;
    return(_status);
  }

static int nr_socket_turn_sendto(void *obj,const void *msg, size_t len,
  int flags, nr_transport_addr *addr)
  {
    int r,_status;
    nr_socket_turn *sturn=obj;

    assert(sturn->magic_cookie == nr_socket_turn_magic_cookie);
    assert(sturn->turn);

    if ((r = nr_turn_client_send_indication(sturn->turn, msg, len, flags,
                                            addr)))
      ABORT(r);

    _status=0;
  abort:
    return(_status);
  }

static int nr_socket_turn_recvfrom(void *obj,void * restrict buf,
  size_t maxlen, size_t *len, int flags, nr_transport_addr *addr)
  {
    /* Reading from TURN sockets is done by the indication
       processing code in turn_client_ctx. */
    assert(0);

    return(R_INTERNAL);
  }

static int nr_socket_turn_getfd(void *obj, NR_SOCKET *fd)
  {
    /* You should never directly be touching this fd. */
    assert(0);

    return(R_INTERNAL);
  }

static int nr_socket_turn_getaddr(void *obj, nr_transport_addr *addrp)
  {
    nr_socket_turn *sturn=obj;
    int r, _status;

    assert(sturn->magic_cookie == nr_socket_turn_magic_cookie);
    assert(sturn->turn);

    /* This returns the relayed address */
    if ((r=nr_turn_client_get_relayed_address(sturn->turn, addrp)))
      ABORT(r);

    _status=0;
 abort:
    return(_status);
  }

static int nr_socket_turn_close(void *obj)
  {
    /* No-op */
#ifndef NDEBUG
    nr_socket_turn *sturn=obj;
    assert(sturn->magic_cookie == nr_socket_turn_magic_cookie);
#endif

    return 0;
  }

int nr_socket_turn_set_ctx(nr_socket *sock, nr_turn_client_ctx *ctx)
{
  nr_socket_turn *sturn=(nr_socket_turn*)sock->obj;
  assert(sturn->magic_cookie == nr_socket_turn_magic_cookie);
  assert(!sturn->turn);

  sturn->turn = ctx;

  return 0;
}

#endif /* USE_TURN */
