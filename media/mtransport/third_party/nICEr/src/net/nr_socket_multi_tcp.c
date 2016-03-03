/*
Copyright (c) 2007, Adobe Systems, Incorporated
Copyright (c) 2014, Mozilla
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

#include <assert.h>
#include <sys/types.h>

#include "nr_api.h"
#include "ice_ctx.h"
#include "nr_socket.h"
#include "nr_socket_local.h"
#include "nr_socket_multi_tcp.h"
#include "nr_socket_buffered_stun.h"
#include "async_timer.h"

typedef struct nr_tcp_socket_ctx_ {
     nr_socket * inner;
     nr_transport_addr remote_addr;
     int is_framed;

     TAILQ_ENTRY(nr_tcp_socket_ctx_) entry;
} nr_tcp_socket_ctx;

typedef TAILQ_HEAD(nr_tcp_socket_head_,nr_tcp_socket_ctx_) nr_tcp_socket_head;

static void nr_tcp_socket_readable_cb(NR_SOCKET s, int how, void *arg);

static int nr_tcp_socket_ctx_destroy(nr_tcp_socket_ctx **objp)
  {
    nr_tcp_socket_ctx *sock;

    if (!objp || !*objp)
      return(0);

    sock=*objp;
    *objp=0;

    nr_socket_destroy(&sock->inner);

    RFREE(sock);

    return(0);
  }

/* This takes ownership of nrsock whether it fails or not. */
static int nr_tcp_socket_ctx_create(nr_socket *nrsock, int is_framed,
  int max_pending, nr_tcp_socket_ctx **sockp)
  {
    int r, _status;
    nr_tcp_socket_ctx *sock = 0;
    nr_socket *tcpsock;

    if (!(sock = RCALLOC(sizeof(nr_tcp_socket_ctx)))) {
      nr_socket_destroy(&nrsock);
      ABORT(R_NO_MEMORY);
    }

    if ((r=nr_socket_buffered_stun_create(nrsock, max_pending, is_framed ? ICE_TCP_FRAMING : TURN_TCP_FRAMING, &tcpsock))){
      nr_socket_destroy(&nrsock);
      ABORT(r);
    }

    sock->inner=tcpsock;
    sock->is_framed=is_framed;

    if ((r=nr_ip4_port_to_transport_addr(ntohl(INADDR_ANY), 0, IPPROTO_TCP, &sock->remote_addr)))
      ABORT(r);

    *sockp=sock;

    _status=0;
abort:
    if (_status) {
      r_log(LOG_ICE,LOG_DEBUG,"%s:%d function %s failed with error %d",__FILE__,__LINE__,__FUNCTION__,_status);
      nr_tcp_socket_ctx_destroy(&sock);
    }
    return(_status);
  }

static int nr_tcp_socket_ctx_initialize(nr_tcp_socket_ctx *tcpsock,
  nr_transport_addr *addr, void* cb_arg)
  {
    int r, _status;
    NR_SOCKET fd;

    if ((r=nr_transport_addr_copy(&tcpsock->remote_addr, addr)))
      ABORT(r);
    if ((r=nr_socket_getfd(tcpsock->inner, &fd)))
      ABORT(r);
    NR_ASYNC_WAIT(fd, NR_ASYNC_WAIT_READ, nr_tcp_socket_readable_cb, cb_arg);

    _status=0;
  abort:
    if (_status)
      r_log(LOG_ICE,LOG_DEBUG,"%s:%d function %s(addr:%s) failed with error %d",__FILE__,__LINE__,__FUNCTION__,addr->as_string,_status);
    return(_status);
  }

typedef struct nr_socket_multi_tcp_ {
  nr_ice_ctx *ctx;
  nr_socket *listen_socket;
  nr_tcp_socket_head sockets;
  nr_socket_tcp_type tcp_type;
  nr_transport_addr addr;
  NR_async_cb readable_cb;
  void *readable_cb_arg;
  int max_pending;
} nr_socket_multi_tcp;

static int nr_socket_multi_tcp_destroy(void **objp);
static int nr_socket_multi_tcp_sendto(void *obj,const void *msg, size_t len,
  int flags, nr_transport_addr *to);
static int nr_socket_multi_tcp_recvfrom(void *obj,void * restrict buf,
  size_t maxlen, size_t *len, int flags, nr_transport_addr *from);
static int nr_socket_multi_tcp_getaddr(void *obj, nr_transport_addr *addrp);
static int nr_socket_multi_tcp_close(void *obj);
static int nr_socket_multi_tcp_connect(void *sock, nr_transport_addr *addr);
static int nr_socket_multi_tcp_listen(void *obj, int backlog);

static nr_socket_vtbl nr_socket_multi_tcp_vtbl={
  2,
  nr_socket_multi_tcp_destroy,
  nr_socket_multi_tcp_sendto,
  nr_socket_multi_tcp_recvfrom,
  0,
  nr_socket_multi_tcp_getaddr,
  nr_socket_multi_tcp_connect,
  0,
  0,
  nr_socket_multi_tcp_close,
  nr_socket_multi_tcp_listen,
  0
};

static int nr_socket_multi_tcp_create_stun_server_socket(
  nr_socket_multi_tcp *sock, nr_ice_stun_server * stun_server,
  nr_transport_addr *addr, int max_pending)
  {
    int r, _status;
    nr_tcp_socket_ctx *tcp_socket_ctx=0;
    nr_socket * nrsock;

    if (stun_server->transport!=IPPROTO_TCP) {
      r_log(LOG_ICE,LOG_INFO,"%s:%d function %s skipping UDP STUN server(addr:%s)",__FILE__,__LINE__,__FUNCTION__,stun_server->u.addr.as_string);
      ABORT(R_BAD_ARGS);
    }

    if (stun_server->type == NR_ICE_STUN_SERVER_TYPE_ADDR &&
        nr_transport_addr_cmp(&stun_server->u.addr,addr,NR_TRANSPORT_ADDR_CMP_MODE_VERSION)) {
      r_log(LOG_ICE,LOG_INFO,"%s:%d function %s skipping STUN with different IP version (%u) than local socket (%u),",__FILE__,__LINE__,__FUNCTION__,stun_server->u.addr.ip_version,addr->ip_version);
      ABORT(R_BAD_ARGS);
    }

    if ((r=nr_socket_factory_create_socket(sock->ctx->socket_factory,addr, &nrsock)))
      ABORT(r);

    /* This takes ownership of nrsock whether it fails or not. */
    if ((r=nr_tcp_socket_ctx_create(nrsock, 0, max_pending, &tcp_socket_ctx)))
      ABORT(r);

    if (stun_server->type == NR_ICE_STUN_SERVER_TYPE_ADDR) {
      nr_transport_addr stun_server_addr;

      nr_transport_addr_copy(&stun_server_addr, &stun_server->u.addr);
      r=nr_socket_connect(tcp_socket_ctx->inner, &stun_server_addr);
      if (r && r!=R_WOULDBLOCK) {
        r_log(LOG_ICE,LOG_WARNING,"%s:%d function %s connect to STUN server(addr:%s) failed with error %d",__FILE__,__LINE__,__FUNCTION__,stun_server_addr.as_string,r);
        ABORT(r);
      }

      if ((r=nr_tcp_socket_ctx_initialize(tcp_socket_ctx, &stun_server_addr, sock)))
        ABORT(r);
    }

    TAILQ_INSERT_TAIL(&sock->sockets, tcp_socket_ctx, entry);

    _status=0;
  abort:
    if (_status) {
      nr_tcp_socket_ctx_destroy(&tcp_socket_ctx);
      r_log(LOG_ICE,LOG_DEBUG,"%s:%d function %s(addr:%s) failed with error %d",__FILE__,__LINE__,__FUNCTION__,addr->as_string,_status);
    }
    return(_status);
  }

int nr_socket_multi_tcp_create(struct nr_ice_ctx_ *ctx,
    nr_transport_addr *addr, nr_socket_tcp_type tcp_type,
    int precreated_so_count, int max_pending, nr_socket **sockp)
  {
    int i=0;
    int r, _status;
    nr_socket_multi_tcp *sock=0;
    nr_tcp_socket_ctx *tcp_socket_ctx;
    nr_socket * nrsock;

    if (!(sock = RCALLOC(sizeof(nr_socket_multi_tcp))))
      ABORT(R_NO_MEMORY);

    TAILQ_INIT(&sock->sockets);

    sock->ctx=ctx;
    sock->max_pending=max_pending;
    sock->tcp_type=tcp_type;
    nr_transport_addr_copy(&sock->addr, addr);

    if((tcp_type==TCP_TYPE_PASSIVE) &&
      ((r=nr_socket_factory_create_socket(sock->ctx->socket_factory, addr, &sock->listen_socket))))
      ABORT(r);

    if (tcp_type!=TCP_TYPE_ACTIVE) {
      if (sock->ctx && sock->ctx->stun_servers) {
        for (i=0; i<sock->ctx->stun_server_ct; ++i) {
          if ((r=nr_socket_multi_tcp_create_stun_server_socket(sock,
              sock->ctx->stun_servers+i, addr, max_pending))) {
            if (r!=R_BAD_ARGS) {
              r_log(LOG_ICE,LOG_WARNING,"%s:%d function %s failed to connect STUN server from addr:%s with error %d",__FILE__,__LINE__,__FUNCTION__,addr->as_string,r);
            }
          }
        }
      }
      if (sock->ctx && sock->ctx->turn_servers) {
        for (i=0; i<sock->ctx->turn_server_ct; ++i) {
          if ((r=nr_socket_multi_tcp_create_stun_server_socket(sock,
              &(sock->ctx->turn_servers[i]).turn_server, addr, max_pending))) {
            if (r!=R_BAD_ARGS) {
              r_log(LOG_ICE,LOG_WARNING,"%s:%d function %s failed to connect TURN server from addr:%s with error %d",__FILE__,__LINE__,__FUNCTION__,addr->as_string,r);
            }
          }
        }
      }
    }

    if ((tcp_type==TCP_TYPE_SO)) {
      for (i=0; i<precreated_so_count; ++i) {

        if ((r=nr_socket_factory_create_socket(sock->ctx->socket_factory, addr, &nrsock)))
          ABORT(r);

        /* This takes ownership of nrsock whether it fails or not. */
        if ((r=nr_tcp_socket_ctx_create(nrsock, 1, max_pending, &tcp_socket_ctx))){
          ABORT(r);
        }
        TAILQ_INSERT_TAIL(&sock->sockets, tcp_socket_ctx, entry);
      }
    }

    if((r=nr_socket_create_int(sock, &nr_socket_multi_tcp_vtbl, sockp)))
      ABORT(r);

    _status=0;
  abort:
    if (_status) {
      r_log(LOG_ICE,LOG_DEBUG,"%s:%d function %s(addr:%s) failed with error %d",__FILE__,__LINE__,__FUNCTION__,addr->as_string,_status);
      nr_socket_multi_tcp_destroy((void**)&sock);
    }
    return(_status);
  }

int nr_socket_multi_tcp_set_readable_cb(nr_socket *sock,
  NR_async_cb readable_cb, void *readable_cb_arg)
  {
    nr_socket_multi_tcp *mtcp_sock = (nr_socket_multi_tcp *)sock->obj;

    mtcp_sock->readable_cb=readable_cb;
    mtcp_sock->readable_cb_arg=readable_cb_arg;

    return 0;
  }

#define PREALLOC_CONNECT_FRAMED          0
#define PREALLOC_CONNECT_NON_FRAMED      1
#define PREALLOC_DONT_CONNECT_UNLESS_SO  2

static int nr_socket_multi_tcp_get_sock_connected_to(nr_socket_multi_tcp *sock,
  nr_transport_addr *to, int preallocated_connect_mode, nr_socket **ret_sock)
  {
    int r, _status;
    nr_tcp_socket_ctx *tcp_sock_ctx;
    nr_socket * nrsock;

    to->protocol=IPPROTO_TCP;

    TAILQ_FOREACH(tcp_sock_ctx, &sock->sockets, entry) {
      if (!nr_transport_addr_is_wildcard(&tcp_sock_ctx->remote_addr)) {
        if (!nr_transport_addr_cmp(to, &tcp_sock_ctx->remote_addr, NR_TRANSPORT_ADDR_CMP_MODE_ALL)) {
          *ret_sock=tcp_sock_ctx->inner;
          return(0);
        }
      }
    }

    tcp_sock_ctx=NULL;
    /* not connected yet */
    if (sock->tcp_type != TCP_TYPE_ACTIVE) {
      if (preallocated_connect_mode == PREALLOC_DONT_CONNECT_UNLESS_SO && sock->tcp_type != TCP_TYPE_SO)
        ABORT(R_FAILED);

      /* find free preallocated socket and connect */
      TAILQ_FOREACH(tcp_sock_ctx, &sock->sockets, entry) {
        if (nr_transport_addr_is_wildcard(&tcp_sock_ctx->remote_addr)) {
          if (preallocated_connect_mode == PREALLOC_CONNECT_NON_FRAMED && tcp_sock_ctx->is_framed)
            continue;
          if (preallocated_connect_mode != PREALLOC_CONNECT_NON_FRAMED && !tcp_sock_ctx->is_framed)
            continue;

          if ((r=nr_socket_connect(tcp_sock_ctx->inner, to))){
            if (r!=R_WOULDBLOCK)
               ABORT(r);
          }

          if ((r=nr_tcp_socket_ctx_initialize(tcp_sock_ctx, to, sock)))
            ABORT(r);

          *ret_sock=tcp_sock_ctx->inner;

          return(0);
        }
      }
      tcp_sock_ctx=NULL;
      ABORT(R_FAILED);
    }

    /* if active type - create new socket for each new remote addr */
    assert(sock->tcp_type == TCP_TYPE_ACTIVE);

    if ((r=nr_socket_factory_create_socket(sock->ctx->socket_factory, &sock->addr, &nrsock)))
      ABORT(r);

    /* This takes ownership of nrsock whether it fails or not. */
    if ((r=nr_tcp_socket_ctx_create(nrsock, 1, sock->max_pending, &tcp_sock_ctx))){
      ABORT(r);
    }

    TAILQ_INSERT_TAIL(&sock->sockets, tcp_sock_ctx, entry);

    if ((r=nr_socket_connect(tcp_sock_ctx->inner, to))){
      if (r!=R_WOULDBLOCK)
        ABORT(r);
    }

    if ((r=nr_tcp_socket_ctx_initialize(tcp_sock_ctx, to, sock)))
      ABORT(r);

    *ret_sock=tcp_sock_ctx->inner;
    tcp_sock_ctx=NULL;

    _status=0;
  abort:
    if (_status) {
      if (tcp_sock_ctx) {
        r_log(LOG_ICE,LOG_DEBUG,"%s:%d function %s failed with error %d, tcp_sock_ctx remote_addr: %s",__FILE__,__LINE__,__FUNCTION__,_status, tcp_sock_ctx->remote_addr.as_string);
        TAILQ_REMOVE(&sock->sockets, tcp_sock_ctx, entry);
        nr_tcp_socket_ctx_destroy(&tcp_sock_ctx);
      } else {
        r_log(LOG_ICE,LOG_DEBUG,"%s:%d function %s failed with error %d, tcp_sock_ctx=NULL",__FILE__,__LINE__,__FUNCTION__,_status);
      }
    }

    return(_status);
  }

int nr_socket_multi_tcp_stun_server_connect(nr_socket *sock,
  nr_transport_addr *addr)
  {
    int r, _status;
    nr_socket_multi_tcp *mtcp_sock = (nr_socket_multi_tcp *)sock->obj;
    nr_socket *nrsock;

    assert(mtcp_sock->tcp_type != TCP_TYPE_ACTIVE);
    if (mtcp_sock->tcp_type == TCP_TYPE_ACTIVE)
      ABORT(R_INTERNAL);

    if ((r=nr_socket_multi_tcp_get_sock_connected_to(mtcp_sock,addr,PREALLOC_CONNECT_NON_FRAMED,&nrsock)))
      ABORT(r);

    _status=0;
  abort:
    if (_status)
      r_log(LOG_ICE,LOG_DEBUG,"%s:%d function %s(addr:%s) failed with error %d",__FILE__,__LINE__,__FUNCTION__,addr->as_string,_status);
    return(_status);
  }

static int nr_socket_multi_tcp_destroy(void **objp)
  {
    nr_socket_multi_tcp *sock;
    nr_tcp_socket_ctx *tcpsock;
    NR_SOCKET fd;

    if (!objp || !*objp)
      return 0;

    sock=(nr_socket_multi_tcp *)*objp;
    *objp=0;

    /* Cancel waiting on the socket */
    if (sock->listen_socket && !nr_socket_getfd(sock->listen_socket, &fd)) {
      NR_ASYNC_CANCEL(fd, NR_ASYNC_WAIT_READ);
    }

    nr_socket_destroy(&sock->listen_socket);

    while (!TAILQ_EMPTY(&sock->sockets)) {

      tcpsock = TAILQ_FIRST(&sock->sockets);
      TAILQ_REMOVE(&sock->sockets, tcpsock, entry);

      if (!nr_socket_getfd(tcpsock->inner, &fd)) {
        NR_ASYNC_CANCEL(fd, NR_ASYNC_WAIT_READ);
      }

      nr_tcp_socket_ctx_destroy(&tcpsock);
    }

    RFREE(sock);

    return 0;
  }

static int nr_socket_multi_tcp_sendto(void *obj, const void *msg, size_t len,
  int flags, nr_transport_addr *to)
  {
    int r, _status;
    nr_socket_multi_tcp *sock=(nr_socket_multi_tcp *)obj;
    nr_socket *nrsock;

    if ((r=nr_socket_multi_tcp_get_sock_connected_to(sock, to,
      PREALLOC_DONT_CONNECT_UNLESS_SO, &nrsock)))
      ABORT(r);

    if((r=nr_socket_sendto(nrsock, msg, len, flags, to)))
      ABORT(r);

    _status=0;
  abort:
    if (_status)
      r_log(LOG_ICE,LOG_DEBUG,"%s:%d function %s(to:%s) failed with error %d",__FILE__,__LINE__,__FUNCTION__,to->as_string,_status);

    return(_status);
}

static int nr_socket_multi_tcp_recvfrom(void *obj,void * restrict buf,
  size_t maxlen, size_t *len, int flags, nr_transport_addr *from)
  {
    int r, _status = 0;
    nr_socket_multi_tcp *sock=(nr_socket_multi_tcp *)obj;
    nr_tcp_socket_ctx *tcpsock;

    if (TAILQ_EMPTY(&sock->sockets))
      ABORT(R_FAILED);

    TAILQ_FOREACH(tcpsock, &sock->sockets, entry) {
      if (nr_transport_addr_is_wildcard(&tcpsock->remote_addr))
        continue;
      r=nr_socket_recvfrom(tcpsock->inner, buf, maxlen, len, flags, from);
      if (!r)
        return 0;

      if (r!=R_WOULDBLOCK) {
        NR_SOCKET fd;
        r_log(LOG_ICE,LOG_DEBUG,
              "%s:%d function %s(to:%s) failed with error %d",__FILE__,
              __LINE__,__FUNCTION__,tcpsock->remote_addr.as_string,r);
        if (!nr_socket_getfd(tcpsock->inner, &fd)) {
          NR_ASYNC_CANCEL(fd, NR_ASYNC_WAIT_READ);
          NR_ASYNC_CANCEL(fd, NR_ASYNC_WAIT_WRITE);
        }

        TAILQ_REMOVE(&sock->sockets, tcpsock, entry);
        nr_tcp_socket_ctx_destroy(&tcpsock);
        ABORT(r);
      }
    }

    /* this also gets returned if all tcpsocks have wildcard remote_addr */
    _status=R_WOULDBLOCK;
  abort:

    return(_status);
  }

static int nr_socket_multi_tcp_getaddr(void *obj, nr_transport_addr *addrp)
  {
    nr_socket_multi_tcp *sock=(nr_socket_multi_tcp *)obj;

    return nr_transport_addr_copy(addrp,&sock->addr);
  }

static int nr_socket_multi_tcp_close(void *obj)
  {
    nr_socket_multi_tcp *sock=(nr_socket_multi_tcp *)obj;
    nr_tcp_socket_ctx *tcpsock;

    if(sock->listen_socket)
      nr_socket_close(sock->listen_socket);

    TAILQ_FOREACH(tcpsock, &sock->sockets, entry) {
      nr_socket_close(tcpsock->inner); //ignore errors
    }

    return 0;
  }

static void nr_tcp_socket_readable_cb(NR_SOCKET s, int how, void *arg)
  {
    nr_socket_multi_tcp *sock=(nr_socket_multi_tcp *)arg;

    // rearm
    NR_ASYNC_WAIT(s, NR_ASYNC_WAIT_READ, nr_tcp_socket_readable_cb, arg);

    if (sock->readable_cb)
      sock->readable_cb(s, how, sock->readable_cb_arg);
  }

static int nr_socket_multi_tcp_connect(void *obj, nr_transport_addr *addr)
  {
    int r, _status;
    nr_socket_multi_tcp *sock=(nr_socket_multi_tcp *)obj;
    nr_socket *nrsock;

    if ((r=nr_socket_multi_tcp_get_sock_connected_to(sock,addr,PREALLOC_CONNECT_FRAMED,&nrsock)))
      ABORT(r);

    _status=0;
abort:
    if (_status)
      r_log(LOG_ICE,LOG_DEBUG,"%s:%d function %s(addr:%s) failed with error %d",__FILE__,__LINE__,__FUNCTION__,addr->as_string,_status);

    return(_status);
  }

static void nr_tcp_multi_lsocket_readable_cb(NR_SOCKET s, int how, void *arg)
  {
    nr_socket_multi_tcp *sock=(nr_socket_multi_tcp *)arg;
    nr_socket *newsock;
    nr_transport_addr remote_addr;
    nr_tcp_socket_ctx *tcp_sock_ctx;
    int r, _status;

    // rearm
    NR_ASYNC_WAIT(s, NR_ASYNC_WAIT_READ, nr_tcp_multi_lsocket_readable_cb, arg);

    /* accept */
    if ((r=nr_socket_accept(sock->listen_socket, &remote_addr, &newsock)))
      ABORT(r);

    /* This takes ownership of newsock whether it fails or not. */
    if ((r=nr_tcp_socket_ctx_create(newsock, 1, sock->max_pending, &tcp_sock_ctx)))
      ABORT(r);

    nr_socket_buffered_set_connected_to(tcp_sock_ctx->inner, &remote_addr);

    if ((r=nr_tcp_socket_ctx_initialize(tcp_sock_ctx, &remote_addr, sock))) {
      nr_tcp_socket_ctx_destroy(&tcp_sock_ctx);
      ABORT(r);
    }

    TAILQ_INSERT_HEAD(&sock->sockets, tcp_sock_ctx, entry);

    _status=0;
abort:
    if (_status) {
      r_log(LOG_ICE,LOG_WARNING,"%s:%d %s failed to accept new TCP connection: %d",__FILE__,__LINE__,__FUNCTION__,_status);
    } else {
      r_log(LOG_ICE,LOG_INFO,"%s:%d %s accepted new TCP connection from %s",__FILE__,__LINE__,__FUNCTION__,remote_addr.as_string);
    }
  }

static int nr_socket_multi_tcp_listen(void *obj, int backlog)
  {
    int r, _status;
    nr_socket_multi_tcp *sock=(nr_socket_multi_tcp *)obj;
    NR_SOCKET fd;

    if(!sock->listen_socket)
      ABORT(R_FAILED);

    if ((r=nr_socket_listen(sock->listen_socket, backlog)))
      ABORT(r);

    if ((r=nr_socket_getfd(sock->listen_socket, &fd)))
      ABORT(r);

    NR_ASYNC_WAIT(fd, NR_ASYNC_WAIT_READ, nr_tcp_multi_lsocket_readable_cb, sock);

    _status=0;
  abort:
    if (_status)
      r_log(LOG_ICE,LOG_WARNING,"%s:%d function %s failed with error %d",__FILE__,__LINE__,__FUNCTION__,_status);

    return(_status);
  }
