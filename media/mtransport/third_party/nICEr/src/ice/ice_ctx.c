
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



static char *RCSSTRING __UNUSED__="$Id: ice_ctx.c,v 1.2 2008/04/28 17:59:01 ekr Exp $";

#include <csi_platform.h>
#include <assert.h>
#include <sys/types.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <sys/queue.h>
#include <string.h>
#include <nr_api.h>
#include <registry.h>
#include "stun.h"
#include "ice_ctx.h"
#include "ice_reg.h"
#include "nr_crypto.h"
#include "async_timer.h"
#include "util.h"
#include "nr_socket_local.h"


int LOG_ICE = 0;

static int nr_ice_random_string(char *str, int len);
static int nr_ice_fetch_stun_servers(int ct, nr_ice_stun_server **out);
#ifdef USE_TURN
static int nr_ice_fetch_turn_servers(int ct, nr_ice_turn_server **out);
#endif /* USE_TURN */
static void nr_ice_ctx_destroy_cb(NR_SOCKET s, int how, void *cb_arg);
static int nr_ice_ctx_pair_new_trickle_candidates(nr_ice_ctx *ctx, nr_ice_candidate *cand);
static int no_op(void **obj) {
  return 0;
}

static nr_socket_factory_vtbl default_socket_factory_vtbl = {
  nr_socket_local_create,
  no_op
};

int nr_ice_fetch_stun_servers(int ct, nr_ice_stun_server **out)
  {
    int r,_status;
    nr_ice_stun_server *servers = 0;
    int i;
    NR_registry child;
    char *addr=0;
    UINT2 port;
    in_addr_t addr_int;

    if(!(servers=RCALLOC(sizeof(nr_ice_stun_server)*ct)))
      ABORT(R_NO_MEMORY);

    for(i=0;i<ct;i++){
      if(r=NR_reg_get_child_registry(NR_ICE_REG_STUN_SRV_PRFX,i,child))
        ABORT(r);
      /* Assume we have a v4 addr for now */
      if(r=NR_reg_alloc2_string(child,"addr",&addr))
        ABORT(r);
      addr_int=inet_addr(addr);
      if(addr_int==INADDR_NONE){
        r_log(LOG_ICE,LOG_ERR,"Invalid address %s;",addr);
        ABORT(R_BAD_ARGS);
      }
      if(r=NR_reg_get2_uint2(child,"port",&port)) {
        if (r != R_NOT_FOUND)
          ABORT(r);
        port = 3478;
      }
      if(r=nr_ip4_port_to_transport_addr(ntohl(addr_int), port, IPPROTO_UDP,
        &servers[i].u.addr))
        ABORT(r);
      servers[i].index=i;
      servers[i].type = NR_ICE_STUN_SERVER_TYPE_ADDR;
      RFREE(addr);
      addr=0;
    }

    *out = servers;

    _status=0;
  abort:
    RFREE(addr);
    if (_status) RFREE(servers);
    return(_status);
  }

int nr_ice_ctx_set_stun_servers(nr_ice_ctx *ctx,nr_ice_stun_server *servers,int ct)
  {
    int _status;

    if(ctx->stun_servers){
      RFREE(ctx->stun_servers);
      ctx->stun_server_ct=0;
    }

    if (ct) {
      if(!(ctx->stun_servers=RCALLOC(sizeof(nr_ice_stun_server)*ct)))
        ABORT(R_NO_MEMORY);

      memcpy(ctx->stun_servers,servers,sizeof(nr_ice_stun_server)*ct);
      ctx->stun_server_ct = ct;
    }

    _status=0;
 abort:
    return(_status);
  }

int nr_ice_ctx_set_turn_servers(nr_ice_ctx *ctx,nr_ice_turn_server *servers,int ct)
  {
    int _status;

    if(ctx->turn_servers){
      RFREE(ctx->turn_servers);
      ctx->turn_server_ct=0;
    }

    if(ct) {
      if(!(ctx->turn_servers=RCALLOC(sizeof(nr_ice_turn_server)*ct)))
        ABORT(R_NO_MEMORY);

      memcpy(ctx->turn_servers,servers,sizeof(nr_ice_turn_server)*ct);
      ctx->turn_server_ct = ct;
    }

    _status=0;
 abort:
    return(_status);
  }

static int nr_ice_ctx_set_local_addrs(nr_ice_ctx *ctx,nr_local_addr *addrs,int ct)
  {
    int _status,i,r;

    if(ctx->local_addrs) {
      RFREE(ctx->local_addrs);
      ctx->local_addr_ct=0;
      ctx->local_addrs=0;
    }

    if (ct) {
      if(!(ctx->local_addrs=RCALLOC(sizeof(nr_local_addr)*ct)))
        ABORT(R_NO_MEMORY);

      for (i=0;i<ct;++i) {
        if (r=nr_local_addr_copy(ctx->local_addrs+i,addrs+i)) {
          ABORT(r);
        }
      }
      ctx->local_addr_ct = ct;
    }

    _status=0;
   abort:
    return(_status);
  }

int nr_ice_ctx_set_resolver(nr_ice_ctx *ctx, nr_resolver *resolver)
  {
    int _status;

    if (ctx->resolver) {
      ABORT(R_ALREADY);
    }

    ctx->resolver = resolver;

    _status=0;
   abort:
    return(_status);
  }

int nr_ice_ctx_set_interface_prioritizer(nr_ice_ctx *ctx, nr_interface_prioritizer *ip)
  {
    int _status;

    if (ctx->interface_prioritizer) {
      ABORT(R_ALREADY);
    }

    ctx->interface_prioritizer = ip;

    _status=0;
   abort:
    return(_status);
  }

int nr_ice_ctx_set_turn_tcp_socket_wrapper(nr_ice_ctx *ctx, nr_socket_wrapper_factory *wrapper)
  {
    int _status;

    if (ctx->turn_tcp_socket_wrapper) {
      ABORT(R_ALREADY);
    }

    ctx->turn_tcp_socket_wrapper = wrapper;

    _status=0;
   abort:
    return(_status);
  }

void nr_ice_ctx_set_socket_factory(nr_ice_ctx *ctx, nr_socket_factory *factory)
  {
    nr_socket_factory_destroy(&ctx->socket_factory);
    ctx->socket_factory = factory;
  }

#ifdef USE_TURN
int nr_ice_fetch_turn_servers(int ct, nr_ice_turn_server **out)
  {
    int r,_status;
    nr_ice_turn_server *servers = 0;
    int i;
    NR_registry child;
    char *addr=0;
    UINT2 port;
    in_addr_t addr_int;
    Data data={0};

    if(!(servers=RCALLOC(sizeof(nr_ice_turn_server)*ct)))
      ABORT(R_NO_MEMORY);

    for(i=0;i<ct;i++){
      if(r=NR_reg_get_child_registry(NR_ICE_REG_TURN_SRV_PRFX,i,child))
        ABORT(r);
      /* Assume we have a v4 addr for now */
      if(r=NR_reg_alloc2_string(child,"addr",&addr))
        ABORT(r);
      addr_int=inet_addr(addr);
      if(addr_int==INADDR_NONE){
        r_log(LOG_ICE,LOG_ERR,"Invalid address %s",addr);
        ABORT(R_BAD_ARGS);
      }
      if(r=NR_reg_get2_uint2(child,"port",&port)) {
        if (r != R_NOT_FOUND)
          ABORT(r);
        port = 3478;
      }
      if(r=nr_ip4_port_to_transport_addr(ntohl(addr_int), port, IPPROTO_UDP,
        &servers[i].turn_server.u.addr))
        ABORT(r);


      if(r=NR_reg_alloc2_string(child,NR_ICE_REG_TURN_SRV_USERNAME,&servers[i].username)){
        if(r!=R_NOT_FOUND)
          ABORT(r);
      }

      if(r=NR_reg_alloc2_data(child,NR_ICE_REG_TURN_SRV_PASSWORD,&data)){
        if(r!=R_NOT_FOUND)
          ABORT(r);
      }
      else {
        servers[i].password=RCALLOC(sizeof(*servers[i].password));
        if(!servers[i].password)
          ABORT(R_NO_MEMORY);
        servers[i].password->data = data.data;
        servers[i].password->len = data.len;
        data.data=0;
      }

      servers[i].turn_server.index=i;

      RFREE(addr);
      addr=0;
    }

    *out = servers;

    _status=0;
  abort:
    RFREE(data.data);
    RFREE(addr);
    if (_status) RFREE(servers);
    return(_status);
  }
#endif /* USE_TURN */

#define MAXADDRS 100 /* Ridiculously high */
int nr_ice_ctx_create(char *label, UINT4 flags, nr_ice_ctx **ctxp)
  {
    nr_ice_ctx *ctx=0;
    int r,_status;
    char buf[100];

    if(r=r_log_register("ice", &LOG_ICE))
      ABORT(r);

    if(!(ctx=RCALLOC(sizeof(nr_ice_ctx))))
      ABORT(R_NO_MEMORY);

    ctx->flags=flags;

    if(!(ctx->label=r_strdup(label)))
      ABORT(R_NO_MEMORY);

    if(r=nr_ice_random_string(buf,8))
      ABORT(r);
    if(!(ctx->ufrag=r_strdup(buf)))
      ABORT(r);
    if(r=nr_ice_random_string(buf,32))
      ABORT(r);
    if(!(ctx->pwd=r_strdup(buf)))
      ABORT(r);

    /* Get the STUN servers */
    if(r=NR_reg_get_child_count(NR_ICE_REG_STUN_SRV_PRFX,
      (unsigned int *)&ctx->stun_server_ct)||ctx->stun_server_ct==0) {
      r_log(LOG_ICE,LOG_WARNING,"ICE(%s): No STUN servers specified", ctx->label);
      ctx->stun_server_ct=0;
    }

    /* 31 is the max for our priority algorithm */
    if(ctx->stun_server_ct>31){
      r_log(LOG_ICE,LOG_WARNING,"ICE(%s): Too many STUN servers specified: max=31", ctx->label);
      ctx->stun_server_ct=31;
    }

    if(ctx->stun_server_ct>0){
      if(r=nr_ice_fetch_stun_servers(ctx->stun_server_ct,&ctx->stun_servers)){
        r_log(LOG_ICE,LOG_ERR,"ICE(%s): Couldn't load STUN servers from registry", ctx->label);
        ctx->stun_server_ct=0;
        ABORT(r);
      }
    }

#ifdef USE_TURN
    /* Get the TURN servers */
    if(r=NR_reg_get_child_count(NR_ICE_REG_TURN_SRV_PRFX,
      (unsigned int *)&ctx->turn_server_ct)||ctx->turn_server_ct==0) {
      r_log(LOG_ICE,LOG_NOTICE,"ICE(%s): No TURN servers specified", ctx->label);
      ctx->turn_server_ct=0;
    }
#else
    ctx->turn_server_ct=0;
#endif /* USE_TURN */

    ctx->local_addrs=0;
    ctx->local_addr_ct=0;

    /* 31 is the max for our priority algorithm */
    if((ctx->stun_server_ct+ctx->turn_server_ct)>31){
      r_log(LOG_ICE,LOG_WARNING,"ICE(%s): Too many STUN/TURN servers specified: max=31", ctx->label);
      ctx->turn_server_ct=31-ctx->stun_server_ct;
    }

#ifdef USE_TURN
    if(ctx->turn_server_ct>0){
      if(r=nr_ice_fetch_turn_servers(ctx->turn_server_ct,&ctx->turn_servers)){
        ctx->turn_server_ct=0;
        r_log(LOG_ICE,LOG_ERR,"ICE(%s): Couldn't load TURN servers from registry", ctx->label);
        ABORT(r);
      }
    }
#endif /* USE_TURN */


    ctx->Ta = 20;

    if (r=nr_socket_factory_create_int(NULL, &default_socket_factory_vtbl, &ctx->socket_factory))
      ABORT(r);

    if ((r=NR_reg_get_string((char *)NR_ICE_REG_PREF_FORCE_INTERFACE_NAME, ctx->force_net_interface, sizeof(ctx->force_net_interface)))) {
      if (r == R_NOT_FOUND) {
        ctx->force_net_interface[0] = 0;
      } else {
        ABORT(r);
      }
    }

    STAILQ_INIT(&ctx->streams);
    STAILQ_INIT(&ctx->sockets);
    STAILQ_INIT(&ctx->foundations);
    STAILQ_INIT(&ctx->peers);
    STAILQ_INIT(&ctx->ids);

    *ctxp=ctx;

    _status=0;
  abort:
    if(_status)
      nr_ice_ctx_destroy_cb(0,0,ctx);

    return(_status);
  }

static void nr_ice_ctx_destroy_cb(NR_SOCKET s, int how, void *cb_arg)
  {
    nr_ice_ctx *ctx=cb_arg;
    nr_ice_foundation *f1,*f2;
    nr_ice_media_stream *s1,*s2;
    int i;
    nr_ice_stun_id *id1,*id2;

    STAILQ_FOREACH_SAFE(s1, &ctx->streams, entry, s2){
      STAILQ_REMOVE(&ctx->streams,s1,nr_ice_media_stream_,entry);
      nr_ice_media_stream_destroy(&s1);
    }

    RFREE(ctx->label);

    RFREE(ctx->stun_servers);

    RFREE(ctx->local_addrs);

    for (i = 0; i < ctx->turn_server_ct; i++) {
        RFREE(ctx->turn_servers[i].username);
        r_data_destroy(&ctx->turn_servers[i].password);
    }
    RFREE(ctx->turn_servers);

    f1=STAILQ_FIRST(&ctx->foundations);
    while(f1){
      f2=STAILQ_NEXT(f1,entry);
      RFREE(f1);
      f1=f2;
    }
    RFREE(ctx->pwd);
    RFREE(ctx->ufrag);

    STAILQ_FOREACH_SAFE(id1, &ctx->ids, entry, id2){
      STAILQ_REMOVE(&ctx->ids,id1,nr_ice_stun_id_,entry);
      RFREE(id1);
    }

    nr_resolver_destroy(&ctx->resolver);
    nr_interface_prioritizer_destroy(&ctx->interface_prioritizer);
    nr_socket_wrapper_factory_destroy(&ctx->turn_tcp_socket_wrapper);
    nr_socket_factory_destroy(&ctx->socket_factory);

    RFREE(ctx);
  }

int nr_ice_ctx_destroy(nr_ice_ctx **ctxp)
  {
    if(!ctxp || !*ctxp)
      return(0);

    (*ctxp)->done_cb=0;
    (*ctxp)->trickle_cb=0;

    NR_ASYNC_SCHEDULE(nr_ice_ctx_destroy_cb,*ctxp);

    *ctxp=0;

    return(0);
  }

void nr_ice_gather_finished_cb(NR_SOCKET s, int h, void *cb_arg)
  {
    int r;
    nr_ice_candidate *cand=cb_arg;
    nr_ice_ctx *ctx;


    assert(cb_arg);
    if (!cb_arg)
      return;
    ctx = cand->ctx;

    ctx->uninitialized_candidates--;

    /* Avoid the need for yet another initialization function */
    if (cand->state == NR_ICE_CAND_STATE_INITIALIZING && cand->type == HOST)
      cand->state = NR_ICE_CAND_STATE_INITIALIZED;

    if (cand->state == NR_ICE_CAND_STATE_INITIALIZED) {
      int was_pruned = 0;

      if (r=nr_ice_component_maybe_prune_candidate(ctx, cand->component,
                                                   cand, &was_pruned)) {
          r_log(LOG_ICE, LOG_NOTICE, "ICE(%s): Problem pruning candidates",ctx->label);
      }

      /* If we are initialized, the candidate wasn't pruned,
         and we have a trickle ICE callback fire the callback */
      if (ctx->trickle_cb && !was_pruned &&
          !nr_ice_ctx_hide_candidate(ctx, cand)) {
        ctx->trickle_cb(ctx->trickle_cb_arg, ctx, cand->stream, cand->component_id, cand);

        if (nr_ice_ctx_pair_new_trickle_candidates(ctx, cand)) {
          r_log(LOG_ICE,LOG_ERR, "ICE(%s): All could not pair new trickle candidate",ctx->label);
          /* But continue */
        }
      }
    }

    if(ctx->uninitialized_candidates==0){
      r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): All candidates initialized",ctx->label);
      if (ctx->done_cb) {
        ctx->done_cb(0,0,ctx->cb_arg);
      }
      else {
        r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): No done_cb. We were probably destroyed.",ctx->label);
      }
    }
    else {
      r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): Waiting for %d candidates to be initialized",ctx->label, ctx->uninitialized_candidates);
    }
  }

static int nr_ice_ctx_pair_new_trickle_candidates(nr_ice_ctx *ctx, nr_ice_candidate *cand)
  {
    int r,_status;
    nr_ice_peer_ctx *pctx;

    pctx=STAILQ_FIRST(&ctx->peers);
    while(pctx){
      if (pctx->state == NR_ICE_PEER_STATE_PAIRED) {
        r = nr_ice_peer_ctx_pair_new_trickle_candidate(ctx, pctx, cand);
        if (r)
          ABORT(r);
      }

      pctx=STAILQ_NEXT(pctx,entry);
    }

    _status=0;
 abort:
    return(_status);
  }

/* Get the default address by doing a connect to a known public IP address,
   in this case Google public DNS:

   IPv4: 8.8.8.8
   IPv6: 2001:4860:4860::8888

   Then we can do getsockname to get the address. No packets get sent
   since this is UDP. It's just a way to get the address.
*/
static int nr_ice_get_default_address(nr_ice_ctx *ctx, int ip_version, nr_transport_addr* addrp)
  {
    int r,_status;
    nr_transport_addr addr;
    nr_transport_addr remote_addr;
    nr_socket *sock=0;

    switch(ip_version) {
      case NR_IPV4:
        if ((r=nr_str_port_to_transport_addr("0.0.0.0", 0, IPPROTO_UDP, &addr)))
          ABORT(r);
        if ((r=nr_str_port_to_transport_addr("8.8.8.8", 53, IPPROTO_UDP, &remote_addr)))
          ABORT(r);
        break;
      case NR_IPV6:
        if ((r=nr_str_port_to_transport_addr("::0", 0, IPPROTO_UDP, &addr)))
          ABORT(r);
        if ((r=nr_str_port_to_transport_addr("2001:4860:4860::8888", 53, IPPROTO_UDP, &remote_addr)))
          ABORT(r);
        break;
      default:
        assert(0);
        ABORT(R_INTERNAL);
    }

    if ((r=nr_socket_factory_create_socket(ctx->socket_factory, &addr, &sock)))
      ABORT(r);
    if ((r=nr_socket_connect(sock, &remote_addr)))
      ABORT(r);
    if ((r=nr_socket_getaddr(sock, addrp)))
      ABORT(r);

    r_log(LOG_GENERIC, LOG_DEBUG, "Default address: %s", addrp->as_string);

    _status=0;
  abort:
    nr_socket_destroy(&sock);
    return(_status);
  }

static int nr_ice_get_default_local_address(nr_ice_ctx *ctx, int ip_version, nr_local_addr* addrs, int addr_ct, nr_local_addr *addrp)
  {
    int r,_status;
    nr_transport_addr default_addr;
    int i;

    if ((r=nr_ice_get_default_address(ctx, ip_version, &default_addr)))
        ABORT(r);

    for(i=0; i<addr_ct; ++i) {
      if (!nr_transport_addr_cmp(&default_addr, &addrs[i].addr,
                                 NR_TRANSPORT_ADDR_CMP_MODE_ADDR)) {
        if ((r=nr_local_addr_copy(addrp, &addrs[i])))
          ABORT(r);
        break;
      }
    }
    if (i==addr_ct)
      ABORT(R_NOT_FOUND);

    _status=0;
  abort:
    return(_status);
  }

static int nr_ice_get_local_addresses(nr_ice_ctx *ctx)
  {
    int r,_status;
    nr_local_addr local_addrs[MAXADDRS];
    nr_local_addr *addrs = 0;
    int i,addr_ct;
    nr_local_addr default_addrs[2];
    int default_addr_ct = 0;

    if (!ctx->local_addrs) {
      /* First, gather all the local addresses we have */
      if((r=nr_stun_find_local_addresses(local_addrs,MAXADDRS,&addr_ct))) {
        r_log(LOG_ICE,LOG_ERR,"ICE(%s): unable to find local addresses",ctx->label);
        ABORT(r);
      }

      if (ctx->force_net_interface[0]) {
        /* Limit us to only addresses on a single interface */
        int force_addr_ct = 0;
        for(i=0;i<addr_ct;i++){
          if (!strcmp(local_addrs[i].addr.ifname, ctx->force_net_interface)) {
            // copy it down in the array, if needed
            if (i != force_addr_ct) {
              if (r=nr_local_addr_copy(&local_addrs[force_addr_ct], &local_addrs[i])) {
                ABORT(r);
              }
            }
            force_addr_ct++;
          }
        }
        addr_ct = force_addr_ct;
      }

      if (ctx->flags & NR_ICE_CTX_FLAGS_ONLY_DEFAULT_ADDRS) {
        /* Get just the default IPv4 and IPv6 addrs */
        if(!nr_ice_get_default_local_address(ctx, NR_IPV4, local_addrs, addr_ct,
                                             &default_addrs[default_addr_ct])) {
          ++default_addr_ct;
        }
        if(!nr_ice_get_default_local_address(ctx, NR_IPV6, local_addrs, addr_ct,
                                             &default_addrs[default_addr_ct])) {
          ++default_addr_ct;
        }
        addrs = default_addrs;
        addr_ct = default_addr_ct;
      }
      else {
        addrs = local_addrs;
      }

      /* Sort interfaces by preference */
      if(ctx->interface_prioritizer) {
        for(i=0;i<addr_ct;i++){
          if(r=nr_interface_prioritizer_add_interface(ctx->interface_prioritizer,addrs+i)) {
            r_log(LOG_ICE,LOG_ERR,"ICE(%s): unable to add interface ",ctx->label);
            ABORT(r);
          }
        }
        if(r=nr_interface_prioritizer_sort_preference(ctx->interface_prioritizer)) {
          r_log(LOG_ICE,LOG_ERR,"ICE(%s): unable to sort interface by preference",ctx->label);
          ABORT(r);
        }
      }

      if (r=nr_ice_ctx_set_local_addrs(ctx,addrs,addr_ct)) {
        ABORT(r);
      }
    }

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_gather(nr_ice_ctx *ctx, NR_async_cb done_cb, void *cb_arg)
  {
    int r,_status;
    nr_ice_media_stream *stream;

    if ((r=nr_ice_get_local_addresses(ctx)))
      ABORT(r);

    if(STAILQ_EMPTY(&ctx->streams)) {
      r_log(LOG_ICE,LOG_ERR,"ICE(%s): Missing streams to initialize",ctx->label);
      ABORT(R_BAD_ARGS);
    }

    r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): Initializing candidates",ctx->label);
    ctx->done_cb=done_cb;
    ctx->cb_arg=cb_arg;

    /* Initialize all the media stream/component pairs */
    stream=STAILQ_FIRST(&ctx->streams);
    while(stream){
      if(r=nr_ice_media_stream_initialize(ctx,stream))
        ABORT(r);

      stream=STAILQ_NEXT(stream,entry);
    }

    if(ctx->uninitialized_candidates)
      ABORT(R_WOULDBLOCK);

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_add_media_stream(nr_ice_ctx *ctx,char *label,int components, nr_ice_media_stream **streamp)
  {
    int r,_status;

    if(r=nr_ice_media_stream_create(ctx,label,components,streamp))
      ABORT(r);

    STAILQ_INSERT_TAIL(&ctx->streams,*streamp,entry);

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_remove_media_stream(nr_ice_ctx *ctx,nr_ice_media_stream **streamp)
  {
    int r,_status;
    nr_ice_peer_ctx *pctx;
    nr_ice_media_stream *peer_stream;

    pctx=STAILQ_FIRST(&ctx->peers);
    while(pctx){
      if(!nr_ice_peer_ctx_find_pstream(pctx, *streamp, &peer_stream)) {
        if(r=nr_ice_peer_ctx_remove_pstream(pctx, &peer_stream)) {
          ABORT(r);
        }
      }

      pctx=STAILQ_NEXT(pctx,entry);
    }

    STAILQ_REMOVE(&ctx->streams,*streamp,nr_ice_media_stream_,entry);
    if(r=nr_ice_media_stream_destroy(streamp)) {
      ABORT(r);
    }

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_get_global_attributes(nr_ice_ctx *ctx,char ***attrsp, int *attrctp)
  {
    char **attrs=0;
    int _status;
    char *tmp=0;

    if(!(attrs=RCALLOC(sizeof(char *)*2)))
      ABORT(R_NO_MEMORY);

    if(!(tmp=RMALLOC(100)))
      ABORT(R_NO_MEMORY);
    snprintf(tmp,100,"ice-ufrag:%s",ctx->ufrag);
    attrs[0]=tmp;

    if(!(tmp=RMALLOC(100)))
      ABORT(R_NO_MEMORY);
    snprintf(tmp,100,"ice-pwd:%s",ctx->pwd);
    attrs[1]=tmp;

    *attrctp=2;
    *attrsp=attrs;

    _status=0;
  abort:
    if (_status){
      if (attrs){
        RFREE(attrs[0]);
        RFREE(attrs[1]);
      }
      RFREE(attrs);
    }
    return(_status);
  }

static int nr_ice_random_string(char *str, int len)
  {
    unsigned char bytes[100];
    int needed;
    int r,_status;

    if(len%2) ABORT(R_BAD_ARGS);
    needed=len/2;

    if(needed>sizeof(bytes)) ABORT(R_BAD_ARGS);

    if(r=nr_crypto_random_bytes(bytes,needed))
      ABORT(r);

    if(r=nr_bin2hex(bytes,needed,(unsigned char *)str))
      ABORT(r);

    _status=0;
  abort:
    return(_status);
  }

/* This is incredibly annoying: we now have a datagram but we don't
   know which peer it's from, and we need to be able to tell the
   API user. So, offer it to each peer and if one bites, assume
   the others don't want it
*/
int nr_ice_ctx_deliver_packet(nr_ice_ctx *ctx, nr_ice_component *comp, nr_transport_addr *source_addr, UCHAR *data, int len)
  {
    nr_ice_peer_ctx *pctx;
    int r;

    pctx=STAILQ_FIRST(&ctx->peers);
    while(pctx){
      r=nr_ice_peer_ctx_deliver_packet_maybe(pctx, comp, source_addr, data, len);
      if(!r)
        break;

      pctx=STAILQ_NEXT(pctx,entry);
    }

    if(!pctx)
      r_log(LOG_ICE,LOG_WARNING,"ICE(%s): Packet received from %s which doesn't match any known peer",ctx->label,source_addr->as_string);

    return(0);
  }

int nr_ice_ctx_is_known_id(nr_ice_ctx *ctx, UCHAR id[12])
  {
    nr_ice_stun_id *xid;

    xid=STAILQ_FIRST(&ctx->ids);
    while(xid){
      if (!memcmp(xid->id, id, 12))
          return 1;

      xid=STAILQ_NEXT(xid,entry);
    }

    return 0;
  }

int nr_ice_ctx_remember_id(nr_ice_ctx *ctx, nr_stun_message *msg)
{
    int _status;
    nr_ice_stun_id *xid;

    xid = RCALLOC(sizeof(*xid));
    if (!xid)
        ABORT(R_NO_MEMORY);

    assert(sizeof(xid->id) == sizeof(msg->header.id));
#if __STDC_VERSION__ >= 201112L
    _Static_assert(sizeof(xid->id) == sizeof(msg->header.id),"Message ID Size Mismatch");
#endif
    memcpy(xid->id, &msg->header.id, sizeof(xid->id));

    STAILQ_INSERT_TAIL(&ctx->ids,xid,entry);

    _status=0;
  abort:
    return(_status);
}


/* Clean up some of the resources (mostly file descriptors) used
   by candidates we didn't choose. Note that this still leaves
   a fair amount of non-system stuff floating around. This gets
   cleaned up when you destroy the ICE ctx */
int nr_ice_ctx_finalize(nr_ice_ctx *ctx, nr_ice_peer_ctx *pctx)
  {
    nr_ice_media_stream *lstr,*rstr;

    r_log(LOG_ICE,LOG_DEBUG,"Finalizing ICE ctx %s, peer=%s",ctx->label,pctx->label);
    /*
       First find the peer stream, if any
    */
    lstr=STAILQ_FIRST(&ctx->streams);
    while(lstr){
      rstr=STAILQ_FIRST(&pctx->peer_streams);

      while(rstr){
        if(rstr->local_stream==lstr)
          break;

        rstr=STAILQ_NEXT(rstr,entry);
      }

      nr_ice_media_stream_finalize(lstr,rstr);

      lstr=STAILQ_NEXT(lstr,entry);
    }

    return(0);
  }


int nr_ice_ctx_set_trickle_cb(nr_ice_ctx *ctx, nr_ice_trickle_candidate_cb cb, void *cb_arg)
{
  ctx->trickle_cb = cb;
  ctx->trickle_cb_arg = cb_arg;

  return 0;
}

int nr_ice_ctx_hide_candidate(nr_ice_ctx *ctx, nr_ice_candidate *cand)
  {
    if (cand->state != NR_ICE_CAND_STATE_INITIALIZED) {
      return 1;
    }

    if (ctx->flags & NR_ICE_CTX_FLAGS_ONLY_DEFAULT_ADDRS) {
      if (cand->type == HOST)
        return 1;
    }

    return 0;
  }
