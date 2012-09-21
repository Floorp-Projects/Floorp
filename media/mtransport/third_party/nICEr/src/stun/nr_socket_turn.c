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



static char *RCSSTRING __UNUSED__="$Id: nr_socket_turn.c,v 1.2 2008/04/28 18:21:30 ekr Exp $";

#ifdef USE_TURN

#include <csi_platform.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>

#include "stun.h"
#include "nr_socket_turn.h"
#include "turn_client_ctx.h"


static char *nr_socket_turn_magic_cookie = "nr_socket_turn";

typedef struct nr_socket_turn_ {
  char *magic_cookie;
  int turn_state;
  nr_socket *sock;
  nr_transport_addr relay_addr;
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
  nr_socket_turn_destroy,
  nr_socket_turn_sendto,
  nr_socket_turn_recvfrom,
  nr_socket_turn_getfd,
  nr_socket_turn_getaddr,
  nr_socket_turn_close
};

int nr_socket_turn_create(nr_socket *sock, int turn_state, nr_socket **sockp)
  {
    int r,_status;
    nr_socket_turn *sturn=0;

    if(!(sturn=RCALLOC(sizeof(nr_socket_turn))))
      ABORT(R_NO_MEMORY);

    sturn->magic_cookie = nr_socket_turn_magic_cookie;
    sturn->sock=sock;
    sturn->turn_state=turn_state;

    if(r=nr_socket_create_int(sturn, &nr_socket_turn_vtbl, sockp))
      ABORT(r);

    _status=0;
  abort:
    if(_status){
      nr_socket_turn_destroy((void **)&sturn);
    }
    return(_status);
  }

void nr_socket_turn_set_state(nr_socket *sock, int turn_state)
{
    nr_socket_turn *sturn=(nr_socket_turn*)sock->obj;
    assert(sturn->magic_cookie == nr_socket_turn_magic_cookie);

    sturn->turn_state=turn_state;
}

void nr_socket_turn_set_relay_addr(nr_socket *sock, nr_transport_addr *relay)
{
    nr_socket_turn *sturn=(nr_socket_turn*)sock->obj;
    assert(sturn->magic_cookie == nr_socket_turn_magic_cookie);

    nr_transport_addr_copy(&sturn->relay_addr, relay);
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
/*  abort:*/
    return(_status);
  }

static int nr_socket_turn_sendto(void *obj,const void *msg, size_t len,
  int flags, nr_transport_addr *addr)
  {
    int r,_status;
    nr_socket_turn *sturn=obj;
    assert(sturn->magic_cookie == nr_socket_turn_magic_cookie);

    switch (sturn->turn_state) {
    case NR_TURN_CLIENT_STATE_INITTED:
        if ((r=nr_socket_sendto(sturn->sock,msg,len,flags,addr)))
            ABORT(r);
        break;
#if 0
/* ACTIVE mode not implemented yet */
    case NR_TURN_CLIENT_STATE_ACTIVE:
        if (!nr_has_stun_cookie((char*)msg,len)) {
            if ((r=nr_socket_sendto(sturn->sock,msg,len,flags,addr)))
                ABORT(r);
            break;  /* BREAK */
        }
        /* FALL THROUGH */
#endif
    case NR_TURN_CLIENT_STATE_ALLOCATED:
        if ((r=nr_turn_client_relay_indication_data(sturn->sock, msg, len, flags, &sturn->relay_addr, addr)))
            ABORT(r);
        break;
    default:
        ABORT(R_INTERNAL);
        break;
    }

    _status=0;
  abort:
    return(_status);
  }

static int nr_socket_turn_recvfrom(void *obj,void * restrict buf,
  size_t maxlen, size_t *len, int flags, nr_transport_addr *addr)
  {
    int r,_status;
    nr_socket_turn *sturn=obj;

    assert(sturn->magic_cookie == nr_socket_turn_magic_cookie);

    if ((r=nr_socket_recvfrom(sturn->sock,buf,maxlen,len,flags,addr)))
        ABORT(r);

    switch (sturn->turn_state) {
    case NR_TURN_CLIENT_STATE_INITTED:
        /* just pass the data through verbatim */
        break;
#if 0
/* ACTIVE mode not implemented yet */
    case NR_TURN_CLIENT_STATE_ACTIVE:
       if (!nr_has_stun_cookie(buf,len)) {
           break;  /* BREAK */
       }
       /* FALL THROUGH */
#endif
    case NR_TURN_CLIENT_STATE_ALLOCATED:
        assert(nr_has_stun_cookie(buf,*len));
        if ((r=nr_turn_client_rewrite_indication_data(buf, *len, len, addr))) {
            if (!nr_is_stun_message(buf,*len)) {
                r_log_nr(NR_LOG_TURN,LOG_WARNING,r,"TURN_SOCKET: Discarding unexpected data");

            }
            else {
                r_log(NR_LOG_TURN,LOG_DEBUG,"TURN_SOCKET: Discarding unexpected or retransmitted message");
            }

            *len = 0;
        }
        break;
    default:
        ABORT(R_INTERNAL);
        break;
    }

    _status=0;
  abort:
    return(_status);
  }

static int nr_socket_turn_getfd(void *obj, NR_SOCKET *fd)
  {
    nr_socket_turn *sturn=obj;
    assert(sturn->magic_cookie == nr_socket_turn_magic_cookie);
    return nr_socket_getfd(sturn->sock,fd);
  }

static int nr_socket_turn_getaddr(void *obj, nr_transport_addr *addrp)
  {
    nr_socket_turn *sturn=obj;
    assert(sturn->magic_cookie == nr_socket_turn_magic_cookie);
#if 0
//    return nr_socket_getaddr(sturn->sock,addrp);
// not sure if this the following is correct, but the above doesn't
// seem to work either
#endif
    switch (sturn->turn_state) {
    case NR_TURN_CLIENT_STATE_INITTED:
        return nr_socket_getaddr(sturn->sock,addrp);
        break;
    case NR_TURN_CLIENT_STATE_ALLOCATED:
        return nr_transport_addr_copy(addrp, &sturn->relay_addr);
        break;
    default:
        assert(0);
        break;
    }

    return R_FAILED;
  }

static int nr_socket_turn_close(void *obj)
  {
#ifndef NDEBUG
    nr_socket_turn *sturn=obj;
    assert(sturn->magic_cookie == nr_socket_turn_magic_cookie);
#endif

    return 0;
  }

#endif /* USE_TURN */
