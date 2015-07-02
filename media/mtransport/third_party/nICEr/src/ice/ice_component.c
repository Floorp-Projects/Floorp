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



static char *RCSSTRING __UNUSED__="$Id: ice_component.c,v 1.2 2008/04/28 17:59:01 ekr Exp $";

#include <string.h>
#include <assert.h>
#include <nr_api.h>
#include <registry.h>
#include <async_timer.h>
#include "ice_ctx.h"
#include "ice_codeword.h"
#include "stun.h"
#include "nr_socket_local.h"
#include "nr_socket_turn.h"
#include "nr_socket_wrapper.h"
#include "nr_socket_buffered_stun.h"
#include "nr_socket_multi_tcp.h"
#include "ice_reg.h"
#include "nr_crypto.h"

static int nr_ice_component_stun_server_default_cb(void *cb_arg,nr_stun_server_ctx *stun_ctx,nr_socket *sock, nr_stun_server_request *req, int *dont_free, int *error);
static int nr_ice_pre_answer_request_destroy(nr_ice_pre_answer_request **parp);

/* This function takes ownership of the contents of req (but not req itself) */
static int nr_ice_pre_answer_request_create(nr_socket *sock, nr_stun_server_request *req, nr_ice_pre_answer_request **parp)
  {
    int r, _status;
    nr_ice_pre_answer_request *par = 0;
    nr_stun_message_attribute *attr;

    if (!(par = RCALLOC(sizeof(nr_ice_pre_answer_request))))
      ABORT(R_NO_MEMORY);

    par->req = *req; /* Struct assignment */
    memset(req, 0, sizeof(*req)); /* Zero contents to avoid confusion */

    if (r=nr_socket_getaddr(sock, &par->local_addr))
      ABORT(r);
    if (!nr_stun_message_has_attribute(par->req.request, NR_STUN_ATTR_USERNAME, &attr))
      ABORT(R_INTERNAL);
    if (!(par->username = r_strdup(attr->u.username)))
      ABORT(R_NO_MEMORY);

    *parp=par;
    _status=0;
 abort:
    if (_status) {
      /* Erase the request so we don't free it */
      memset(&par->req, 0, sizeof(nr_stun_server_request));
      nr_ice_pre_answer_request_destroy(&par);
    }

    return(_status);
  }

static int nr_ice_pre_answer_request_destroy(nr_ice_pre_answer_request **parp)
  {
    nr_ice_pre_answer_request *par;

    if (!parp || !*parp)
      return(0);

    par = *parp;
    *parp = 0;

    nr_stun_message_destroy(&par->req.request);
    nr_stun_message_destroy(&par->req.response);

    RFREE(par->username);
    RFREE(par);

    return(0);
  }

int nr_ice_component_create(nr_ice_media_stream *stream, int component_id, nr_ice_component **componentp)
  {
    int _status;
    nr_ice_component *comp=0;

    if(!(comp=RCALLOC(sizeof(nr_ice_component))))
      ABORT(R_NO_MEMORY);

    comp->state=NR_ICE_COMPONENT_UNPAIRED;
    comp->component_id=component_id;
    comp->stream=stream;
    comp->ctx=stream->ctx;

    STAILQ_INIT(&comp->sockets);
    TAILQ_INIT(&comp->candidates);
    STAILQ_INIT(&comp->pre_answer_reqs);

    STAILQ_INSERT_TAIL(&stream->components,comp,entry);

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_component_destroy(nr_ice_component **componentp)
  {
    nr_ice_component *component;
    nr_ice_socket *s1,*s2;
    nr_ice_candidate *c1,*c2;
    nr_ice_pre_answer_request *r1,*r2;

    if(!componentp || !*componentp)
      return(0);

    component=*componentp;
    *componentp=0;

    /* Detach ourselves from the sockets */
    if (component->local_component){
      nr_ice_socket *isock=STAILQ_FIRST(&component->local_component->sockets);
      while(isock){
        nr_stun_server_remove_client(isock->stun_server, component);
        isock=STAILQ_NEXT(isock, entry);
      }
    }

    /* candidates MUST be destroyed before the sockets so that
       they can deregister */
    TAILQ_FOREACH_SAFE(c1, &component->candidates, entry_comp, c2){
      TAILQ_REMOVE(&component->candidates,c1,entry_comp);
      nr_ice_candidate_destroy(&c1);
    }

    STAILQ_FOREACH_SAFE(s1, &component->sockets, entry, s2){
      STAILQ_REMOVE(&component->sockets,s1,nr_ice_socket_,entry);
      nr_ice_socket_destroy(&s1);
    }

    STAILQ_FOREACH_SAFE(r1, &component->pre_answer_reqs, entry, r2){
      STAILQ_REMOVE(&component->pre_answer_reqs,r1,nr_ice_pre_answer_request_, entry);
      nr_ice_pre_answer_request_destroy(&r1);
    }

    if(component->keepalive_timer)
      NR_async_timer_cancel(component->keepalive_timer);
    nr_stun_client_ctx_destroy(&component->keepalive_ctx);

    RFREE(component);
    return(0);
  }

static int nr_ice_component_create_stun_server_ctx(nr_ice_component *component, nr_ice_socket *isock, nr_socket *sock, nr_transport_addr *addr, char *lufrag, Data *pwd)
  {
    char label[256];
    int r,_status;

    /* Create a STUN server context for this socket */
    snprintf(label, sizeof(label), "server(%s)", addr->as_string);
    if(r=nr_stun_server_ctx_create(label,sock,&isock->stun_server))
      ABORT(r);
    if(r=nr_ice_socket_register_stun_server(isock,isock->stun_server,&isock->stun_server_handle))
      ABORT(r);

   /* Add the default STUN credentials so that we can respond before
      we hear about the peer.*/
    if(r=nr_stun_server_add_default_client(isock->stun_server, lufrag, pwd, nr_ice_component_stun_server_default_cb, component))
      ABORT(r);

    _status = 0;
 abort:
    return(_status);
  }

static int nr_ice_component_initialize_udp(struct nr_ice_ctx_ *ctx,nr_ice_component *component, nr_local_addr *addrs, int addr_ct, char *lufrag, Data *pwd)
  {
    nr_socket *sock;
    nr_ice_socket *isock=0;
    nr_ice_candidate *cand=0;
    int i;
    int j;
    int r,_status;

    /* Now one ice_socket for each address */
    for(i=0;i<addr_ct;i++){
      char suppress;

      if(r=NR_reg_get2_char(NR_ICE_REG_SUPPRESS_INTERFACE_PRFX,addrs[i].addr.ifname,&suppress)){
        if(r!=R_NOT_FOUND)
          ABORT(r);
      }
      else{
        if(suppress)
          continue;
      }
      r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): host address %s",ctx->label,addrs[i].addr.as_string);
      if((r=nr_socket_factory_create_socket(ctx->socket_factory,&addrs[i].addr,&sock))){
        r_log(LOG_ICE,LOG_WARNING,"ICE(%s): couldn't create socket for address %s",ctx->label,addrs[i].addr.as_string);
        continue;
      }

      if(r=nr_ice_socket_create(ctx,component,sock,NR_ICE_SOCKET_TYPE_DGRAM,&isock))
        ABORT(r);
      /* Create one host candidate */
      if(r=nr_ice_candidate_create(ctx,component,isock,sock,HOST,0,0,
        component->component_id,&cand))
        ABORT(r);

      TAILQ_INSERT_TAIL(&component->candidates,cand,entry_comp);
      component->candidate_ct++;
      cand=0;

      /* And a srvrflx candidate for each STUN server */
      for(j=0;j<ctx->stun_server_ct;j++){
        if(r=nr_ice_candidate_create(ctx,component,
          isock,sock,SERVER_REFLEXIVE,0,
          &ctx->stun_servers[j],component->component_id,&cand))
          ABORT(r);
        TAILQ_INSERT_TAIL(&component->candidates,cand,entry_comp);
        component->candidate_ct++;
        cand=0;
      }

#ifdef USE_TURN
      /* And both a srvrflx and relayed candidate for each TURN server */
      for(j=0;j<ctx->turn_server_ct;j++){
        nr_socket *turn_sock;
        nr_ice_candidate *srvflx_cand;

        /* Skip non-UDP */
        if (ctx->turn_servers[j].turn_server.transport != IPPROTO_UDP)
          continue;

        /* srvrflx */
        if(r=nr_ice_candidate_create(ctx,component,
          isock,sock,SERVER_REFLEXIVE,0,
          &ctx->turn_servers[j].turn_server,component->component_id,&cand))
          ABORT(r);
        cand->state=NR_ICE_CAND_STATE_INITIALIZING; /* Don't start */
        cand->done_cb=nr_ice_gather_finished_cb;
        cand->cb_arg=cand;

        TAILQ_INSERT_TAIL(&component->candidates,cand,entry_comp);
        component->candidate_ct++;
        srvflx_cand=cand;

        /* relayed*/
        if(r=nr_socket_turn_create(sock, &turn_sock))
          ABORT(r);
        if(r=nr_ice_candidate_create(ctx,component,
          isock,turn_sock,RELAYED,0,
          &ctx->turn_servers[j].turn_server,component->component_id,&cand))
           ABORT(r);
        cand->u.relayed.srvflx_candidate=srvflx_cand;
        cand->u.relayed.server=&ctx->turn_servers[j];
        TAILQ_INSERT_TAIL(&component->candidates,cand,entry_comp);
        component->candidate_ct++;

        cand=0;
      }
#endif /* USE_TURN */

      /* Create a STUN server context for this socket */
      if ((r=nr_ice_component_create_stun_server_ctx(component,isock,sock,&addrs[i].addr,lufrag,pwd)))
        ABORT(r);

      STAILQ_INSERT_TAIL(&component->sockets,isock,entry);
    }

    _status = 0;
 abort:
    return(_status);
  }

static int nr_ice_component_get_port_from_ephemeral_range(uint16_t *port)
  {
    int _status, r;
    void *buf = port;
    if(r=nr_crypto_random_bytes(buf, 2))
      ABORT(r);
    *port|=49152; /* make it fit into IANA ephemeral port range >= 49152 */
    _status=0;
abort:
    return(_status);
  }

static int nr_ice_component_create_tcp_host_candidate(struct nr_ice_ctx_ *ctx,
  nr_ice_component *component, nr_transport_addr *interface_addr, nr_socket_tcp_type tcp_type,
  int backlog, int so_sock_ct, char *lufrag, Data *pwd, nr_ice_socket **isock)
  {
    int r,_status;
    nr_ice_candidate *cand=0;
    int tries=3;
    nr_ice_socket *isock_tmp=0;
    nr_socket *nrsock=0;
    nr_transport_addr addr;
    uint16_t local_port;

    if ((r=nr_transport_addr_copy(&addr,interface_addr)))
      ABORT(r);
    addr.protocol=IPPROTO_TCP;

    do{
      if (!tries--)
        ABORT(r);

      if((r=nr_ice_component_get_port_from_ephemeral_range(&local_port)))
        ABORT(r);

      if ((r=nr_transport_addr_set_port(&addr, local_port)))
        ABORT(r);

      if((r=nr_transport_addr_fmt_addr_string(&addr)))
        ABORT(r);

      /* It would be better to stop trying if there is error other than
         port already used, but it'd require significant work to support this. */
      r=nr_socket_multi_tcp_create(ctx,&addr,tcp_type,so_sock_ct,1,NR_STUN_MAX_MESSAGE_SIZE,&nrsock);

    } while(r);

    if((tcp_type == TCP_TYPE_PASSIVE) && (r=nr_socket_listen(nrsock,backlog)))
      ABORT(r);

    if((r=nr_ice_socket_create(ctx,component,nrsock,NR_ICE_SOCKET_TYPE_STREAM_TCP,&isock_tmp)))
      ABORT(r);

    /* nr_ice_socket took ownership of nrsock */
    nrsock=NULL;

    /* Create a STUN server context for this socket */
    if ((r=nr_ice_component_create_stun_server_ctx(component,isock_tmp,isock_tmp->sock,&addr,lufrag,pwd)))
      ABORT(r);

    if((r=nr_ice_candidate_create(ctx,component,isock_tmp,isock_tmp->sock,HOST,tcp_type,0,
      component->component_id,&cand)))
      ABORT(r);

    if (isock)
      *isock=isock_tmp;

    TAILQ_INSERT_TAIL(&component->candidates,cand,entry_comp);
    component->candidate_ct++;

    STAILQ_INSERT_TAIL(&component->sockets,isock_tmp,entry);

    _status=0;
abort:
    if (_status) {
      nr_ice_socket_destroy(&isock_tmp);
      nr_socket_destroy(&nrsock);
    }
    return(_status);
  }

static int nr_ice_component_initialize_tcp(struct nr_ice_ctx_ *ctx,nr_ice_component *component, nr_local_addr *addrs, int addr_ct, char *lufrag, Data *pwd)
  {
    nr_ice_candidate *cand=0;
    int i;
    int j;
    int r,_status;
    int so_sock_ct=0;
    int backlog=10;
    char ice_tcp_disabled=1;

    r_log(LOG_ICE,LOG_DEBUG,"nr_ice_component_initialize_tcp");

    if(r=NR_reg_get_int4(NR_ICE_REG_ICE_TCP_SO_SOCK_COUNT,&so_sock_ct)){
      if(r!=R_NOT_FOUND)
        ABORT(r);
    }

    if(r=NR_reg_get_int4(NR_ICE_REG_ICE_TCP_LISTEN_BACKLOG,&backlog)){
      if(r!=R_NOT_FOUND)
        ABORT(r);
    }

    if ((r=NR_reg_get_char(NR_ICE_REG_ICE_TCP_DISABLE, &ice_tcp_disabled))) {
      if (r != R_NOT_FOUND)
        ABORT(r);
    }

    for(i=0;i<addr_ct;i++){
      char suppress;
      nr_ice_socket *isock_psv=0;
      nr_ice_socket *isock_so=0;

      if(r=NR_reg_get2_char(NR_ICE_REG_SUPPRESS_INTERFACE_PRFX,addrs[i].addr.ifname,&suppress)){
        if(r!=R_NOT_FOUND)
          ABORT(r);
      }
      else if(suppress) {
          continue;
      }

      if (!ice_tcp_disabled) {
        /* passive host candidate */
        if ((r=nr_ice_component_create_tcp_host_candidate(ctx, component, &addrs[i].addr,
          TCP_TYPE_PASSIVE, backlog, 0, lufrag, pwd, &isock_psv)))
          ABORT(r);

        /* active host candidate */
        if ((r=nr_ice_component_create_tcp_host_candidate(ctx, component, &addrs[i].addr,
          TCP_TYPE_ACTIVE, 0, 0, lufrag, pwd, NULL)))
          ABORT(r);

        /* simultaneous-open host candidate */
        if (so_sock_ct) {
          if ((r=nr_ice_component_create_tcp_host_candidate(ctx, component, &addrs[i].addr,
            TCP_TYPE_SO, 0, so_sock_ct, lufrag, pwd, &isock_so)))
            ABORT(r);
        }

        /* And srvrflx candidates for each STUN server */
        for(j=0;j<ctx->stun_server_ct;j++){
          if (ctx->stun_servers[j].transport!=IPPROTO_TCP)
            continue;

          if(r=nr_ice_candidate_create(ctx,component,
            isock_psv,isock_psv->sock,SERVER_REFLEXIVE,TCP_TYPE_PASSIVE,
            &ctx->stun_servers[j],component->component_id,&cand))
            ABORT(r);
          TAILQ_INSERT_TAIL(&component->candidates,cand,entry_comp);
          component->candidate_ct++;
          cand=0;

          if (so_sock_ct) {
            if(r=nr_ice_candidate_create(ctx,component,
              isock_so,isock_so->sock,SERVER_REFLEXIVE,TCP_TYPE_SO,
              &ctx->stun_servers[j],component->component_id,&cand))
              ABORT(r);
            TAILQ_INSERT_TAIL(&component->candidates,cand,entry_comp);
            component->candidate_ct++;
            cand=0;
          }
        }
      }

#ifdef USE_TURN
      /* Create a new relayed candidate for each addr/TURN server pair */
      for(j=0;j<ctx->turn_server_ct;j++){
        nr_transport_addr addr;
        nr_socket *local_sock;
        nr_socket *buffered_sock;
        nr_socket *turn_sock;
        nr_ice_socket *turn_isock;

        /* Skip non-TCP */
        if (ctx->turn_servers[j].turn_server.transport != IPPROTO_TCP)
          continue;

        if (!ice_tcp_disabled) {
          /* Use TURN server to get srflx candidates */
          if(r=nr_ice_candidate_create(ctx,component,
            isock_psv,isock_psv->sock,SERVER_REFLEXIVE,TCP_TYPE_PASSIVE,
            &ctx->turn_servers[j].turn_server,component->component_id,&cand))
            ABORT(r);
          TAILQ_INSERT_TAIL(&component->candidates,cand,entry_comp);
          component->candidate_ct++;
          cand=0;

          if (so_sock_ct) {
            if(r=nr_ice_candidate_create(ctx,component,
              isock_so,isock_so->sock,SERVER_REFLEXIVE,TCP_TYPE_SO,
              &ctx->turn_servers[j].turn_server,component->component_id,&cand))
              ABORT(r);
            TAILQ_INSERT_TAIL(&component->candidates,cand,entry_comp);
            component->candidate_ct++;
            cand=0;
          }
        }

        /* Create relay candidate */
        if ((r=nr_transport_addr_copy(&addr, &addrs[i].addr)))
          ABORT(r);
        addr.protocol = IPPROTO_TCP;
        if ((r=nr_transport_addr_fmt_addr_string(&addr)))
          ABORT(r);
        /* Create a local socket */
        if((r=nr_socket_factory_create_socket(ctx->socket_factory,&addr,&local_sock))){
          r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): couldn't create socket for address %s",ctx->label,addr.as_string);
          continue;
        }

        r_log(LOG_ICE,LOG_DEBUG,"nr_ice_component_initialize_tcp create");

        if (ctx->turn_tcp_socket_wrapper) {
          /* Wrap it */
          if((r=nr_socket_wrapper_factory_wrap(ctx->turn_tcp_socket_wrapper, local_sock, &local_sock)))
            ABORT(r);
        }

        /* Wrap it */
        if((r=nr_socket_buffered_stun_create(local_sock, NR_STUN_MAX_MESSAGE_SIZE, TURN_TCP_FRAMING, &buffered_sock)))
          ABORT(r);

        /* The TURN socket */
        if(r=nr_socket_turn_create(buffered_sock, &turn_sock))
          ABORT(r);

        /* Create an ICE socket */
        if((r=nr_ice_socket_create(ctx, component, buffered_sock, NR_ICE_SOCKET_TYPE_STREAM_TURN, &turn_isock)))
          ABORT(r);

        /* Attach ourselves to it */
        if(r=nr_ice_candidate_create(ctx,component,
          turn_isock,turn_sock,RELAYED,TCP_TYPE_NONE,
          &ctx->turn_servers[j].turn_server,component->component_id,&cand))
          ABORT(r);
        cand->u.relayed.srvflx_candidate=NULL;
        cand->u.relayed.server=&ctx->turn_servers[j];
        TAILQ_INSERT_TAIL(&component->candidates,cand,entry_comp);
        component->candidate_ct++;
        cand=0;

        /* Create a STUN server context for this socket */
        if ((r=nr_ice_component_create_stun_server_ctx(component,turn_isock,local_sock,&addr,lufrag,pwd)))
          ABORT(r);

        STAILQ_INSERT_TAIL(&component->sockets,turn_isock,entry);
      }
#endif /* USE_TURN */
    }

    _status = 0;
 abort:
    return(_status);
  }


/* Make all the candidates we can make at the beginning */
int nr_ice_component_initialize(struct nr_ice_ctx_ *ctx,nr_ice_component *component)
  {
    int r,_status;
    nr_local_addr *addrs=ctx->local_addrs;
    int addr_ct=ctx->local_addr_ct;
    char *lufrag;
    char *lpwd;
    Data pwd;
    nr_ice_candidate *cand;

    if (component->candidate_ct) {
      r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): component with id %d already has candidates, probably restarting gathering because of a new stream",ctx->label,component->component_id);
      return(0);
    }

    r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): initializing component with id %d",ctx->label,component->component_id);

    if(addr_ct==0){
      r_log(LOG_ICE,LOG_ERR,"ICE(%s): no local addresses available",ctx->label);
      ABORT(R_NOT_FOUND);
    }

    /* Note: we need to recompute these because
       we have not yet computed the values in the peer media stream.*/
    lufrag=component->stream->ufrag ? component->stream->ufrag : ctx->ufrag;
    assert(lufrag);
    if (!lufrag)
      ABORT(R_INTERNAL);
    lpwd=component->stream->pwd ? component->stream->pwd :ctx->pwd;
    assert(lpwd);
    if (!lpwd)
      ABORT(R_INTERNAL);
    INIT_DATA(pwd, (UCHAR *)lpwd, strlen(lpwd));

    /* Initialize the UDP candidates */
    if (r=nr_ice_component_initialize_udp(ctx, component, addrs, addr_ct, lufrag, &pwd))
      ABORT(r);
    /* And the TCP candidates */
    if (r=nr_ice_component_initialize_tcp(ctx, component, addrs, addr_ct, lufrag, &pwd))
      ABORT(r);

    /* count the candidates that will be initialized */
    cand=TAILQ_FIRST(&component->candidates);
    if(!cand){
      r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): couldn't create any valid candidates",ctx->label);
      ABORT(R_NOT_FOUND);
    }

    while(cand){
      ctx->uninitialized_candidates++;
      cand=TAILQ_NEXT(cand,entry_comp);
    }

    /* Now initialize all the candidates */
    cand=TAILQ_FIRST(&component->candidates);
    while(cand){
      if(cand->state!=NR_ICE_CAND_STATE_INITIALIZING){
        if(r=nr_ice_candidate_initialize(cand,nr_ice_gather_finished_cb,cand)){
          if(r!=R_WOULDBLOCK){
            ctx->uninitialized_candidates--;
            cand->state=NR_ICE_CAND_STATE_FAILED;
          }
        }
      }
      cand=TAILQ_NEXT(cand,entry_comp);
    }
    _status=0;
 abort:
    return(_status);
  }

static int nr_ice_any_peer_paired(nr_ice_candidate* cand) {
  nr_ice_peer_ctx* pctx=STAILQ_FIRST(&cand->ctx->peers);
  while(pctx && pctx->state == NR_ICE_PEER_STATE_UNPAIRED){
    /* Is it worth actually looking through the check lists? Probably not. */
    pctx=STAILQ_NEXT(pctx,entry);
  }
  return pctx != NULL;
}

/*
  Compare this newly initialized candidate against the other initialized
  candidates and discard the lower-priority one if they are redundant.

   This algorithm combined with the other algorithms, favors
   host > srflx > relay
 */
int nr_ice_component_maybe_prune_candidate(nr_ice_ctx *ctx, nr_ice_component *comp, nr_ice_candidate *c1, int *was_pruned)
  {
    nr_ice_candidate *c2, *tmp = NULL;

    *was_pruned = 0;
    c2 = TAILQ_FIRST(&comp->candidates);
    while(c2){
      if((c1 != c2) &&
         (c2->state == NR_ICE_CAND_STATE_INITIALIZED) &&
         !nr_transport_addr_cmp(&c1->base,&c2->base,NR_TRANSPORT_ADDR_CMP_MODE_ALL) &&
         !nr_transport_addr_cmp(&c1->addr,&c2->addr,NR_TRANSPORT_ADDR_CMP_MODE_ALL)){

        if((c1->type == c2->type) ||
           (c1->type==HOST && c2->type == SERVER_REFLEXIVE) ||
           (c2->type==HOST && c1->type == SERVER_REFLEXIVE)){

          /*
             These are redundant. Remove the lower pri one, or if pairing has
             already occurred, remove the newest one.

             Since this algorithmis run whenever a new candidate
             is initialized, there should at most one duplicate.
           */
          if ((c1->priority <= c2->priority) || nr_ice_any_peer_paired(c2)) {
            tmp = c1;
            *was_pruned = 1;
          }
          else {
            tmp = c2;
          }
          break;
        }
      }

      c2=TAILQ_NEXT(c2,entry_comp);
    }

    if (tmp) {
      r_log(LOG_ICE,LOG_DEBUG,"ICE(%s)/CAND(%s): Removing redundant candidate",
            ctx->label,tmp->label);

      TAILQ_REMOVE(&comp->candidates,tmp,entry_comp);
      comp->candidate_ct--;
      TAILQ_REMOVE(&tmp->isock->candidates,tmp,entry_sock);

      nr_ice_candidate_destroy(&tmp);
    }

    return 0;
  }

/* Section 7.2.1 */
static int nr_ice_component_process_incoming_check(nr_ice_component *comp, nr_transport_addr *local_addr, nr_stun_server_request *req, int *error)
  {
    nr_ice_cand_pair *pair;
    nr_ice_candidate *pcand=0;
    nr_stun_message *sreq=req->request;
    nr_stun_message_attribute *attr;
    int component_id_matched;
    int local_addr_matched;
    int remote_addr_matched;
    nr_ice_cand_pair *found_invalid=0;
    int r=0,_status;
    int new_pcand_created=0;

    r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/STREAM(%s)/COMP(%d): received request from %s",comp->stream->pctx->label,comp->stream->label,comp->component_id,req->src_addr.as_string);

    if (comp->state == NR_ICE_COMPONENT_DISABLED)
      ABORT(R_REJECTED);

    /* Check for role conficts (7.2.1.1) */
    if(comp->stream->pctx->controlling){
      if(nr_stun_message_has_attribute(sreq,NR_STUN_ATTR_ICE_CONTROLLING,&attr)){
        /* OK, there is a conflict. Who's right? */
        r_log(LOG_ICE,LOG_INFO,"ICE-PEER(%s): role conflict, both controlling",comp->stream->pctx->label);

        if(attr->u.ice_controlling > comp->stream->pctx->tiebreaker){
          /* Update the peer ctx. This will propagate to all candidate pairs
             in the context. */
          nr_ice_peer_ctx_switch_controlling_role(comp->stream->pctx);
        }
        else {
          /* We are: throw an error */
          r_log(LOG_ICE,LOG_WARNING,"ICE-PEER(%s): returning 487 role conflict",comp->stream->pctx->label);

          *error=487;
          ABORT(R_REJECTED);
        }
      }
    }
    else{
      if(nr_stun_message_has_attribute(sreq,NR_STUN_ATTR_ICE_CONTROLLED,&attr)){
        /* OK, there is a conflict. Who's right? */
        r_log(LOG_ICE,LOG_INFO,"ICE-PEER(%s): role conflict, both controlled",comp->stream->pctx->label);

        if(attr->u.ice_controlling < comp->stream->pctx->tiebreaker){
          /* Update the peer ctx. This will propagate to all candidate pairs
             in the context. */
          nr_ice_peer_ctx_switch_controlling_role(comp->stream->pctx);
        }
        else {
          /* We are: throw an error */
          r_log(LOG_ICE,LOG_WARNING,"ICE-PEER(%s): returning 487 role conflict",comp->stream->pctx->label);

          *error=487;
          ABORT(R_REJECTED);
        }
      }
    }

    r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s): This STUN request appears to map to local addr %s",comp->stream->pctx->label,local_addr->as_string);

    pair=TAILQ_FIRST(&comp->stream->check_list);
    while(pair){
      component_id_matched = 0;
      local_addr_matched = 0;
      remote_addr_matched = 0;

      if(pair->remote->component->component_id!=comp->component_id)
        goto next_pair;
      component_id_matched = 1;

      if(nr_transport_addr_cmp(&pair->local->base,local_addr,NR_TRANSPORT_ADDR_CMP_MODE_ALL))
        goto next_pair;
      local_addr_matched=1;


      if(nr_transport_addr_cmp(&pair->remote->addr,&req->src_addr,NR_TRANSPORT_ADDR_CMP_MODE_ALL))
        goto next_pair;
      remote_addr_matched = 1;

      if(pair->state==NR_ICE_PAIR_STATE_FAILED){
        found_invalid=pair;
        goto next_pair;
      }

      if (local_addr_matched && remote_addr_matched){
        r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/CAND_PAIR(%s): Found a matching pair for received check: %s",comp->stream->pctx->label,pair->codeword,pair->as_string);
        break; /* OK, this is a known pair */
      }

    next_pair:
      pair=TAILQ_NEXT(pair,entry);
    }

    if(!pair){
      if(!found_invalid){
        /* First find our local component candidate */
        nr_ice_candidate *cand;

        r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s): no matching pair",comp->stream->pctx->label);
        cand=TAILQ_FIRST(&comp->local_component->candidates);
        while(cand){
          if(!nr_transport_addr_cmp(&cand->addr,local_addr,NR_TRANSPORT_ADDR_CMP_MODE_ALL))
            break;

          cand=TAILQ_NEXT(cand,entry_comp);
        }

        /* Well, this really shouldn't happen, but it's an error from the
           other side, so we just throw an error and keep going */
        if(!cand){
          r_log(LOG_ICE,LOG_WARNING,"ICE-PEER(%s): stun request to unknown local address %s, discarding",comp->stream->pctx->label,local_addr->as_string);

          *error=400;
          ABORT(R_NOT_FOUND);
        }

        /* Try to find matching peer active tcp candidate */
        pcand=TAILQ_FIRST(&comp->candidates);
        while(pcand){
          if(pcand->tcp_type == TCP_TYPE_ACTIVE) {
            if(!nr_transport_addr_cmp(&pcand->addr,&req->src_addr,NR_TRANSPORT_ADDR_CMP_MODE_ALL))
              break;
          }

          pcand=TAILQ_NEXT(pcand,entry_comp);
        }

        if (!pcand){
          /* We now need to make a peer reflexive */
          if(r=nr_ice_peer_peer_rflx_candidate_create(comp->stream->pctx->ctx,"prflx",comp,&req->src_addr,&pcand)) {
            *error=(r==R_NO_MEMORY)?500:400;
            ABORT(r);
          }
          new_pcand_created=1;
          if(!nr_stun_message_has_attribute(sreq,NR_STUN_ATTR_PRIORITY,&attr)){
            r_log(LOG_ICE,LOG_WARNING,"ICE-PEER(%s): Rejecting stun request without priority",comp->stream->pctx->label);
            *error=487;
            ABORT(R_BAD_DATA);
          }
          pcand->priority=attr->u.priority;
        }

        pcand->state=NR_ICE_CAND_PEER_CANDIDATE_PAIRED;

        if(r=nr_ice_candidate_pair_create(comp->stream->pctx,cand,pcand,
             &pair)) {
          *error=(r==R_NO_MEMORY)?500:400;
          ABORT(r);
        }
        nr_ice_candidate_pair_set_state(pair->pctx,pair,NR_ICE_PAIR_STATE_FROZEN);

        if(r=nr_ice_component_insert_pair(comp,pair)) {
          *error=(r==R_NO_MEMORY)?500:400;
          nr_ice_candidate_pair_destroy(&pair);
          ABORT(r);
        }

        /* Do this last, since any call to ABORT will destroy pcand */
        if (new_pcand_created){
          TAILQ_INSERT_TAIL(&comp->candidates,pcand,entry_comp);
          pcand=0;
        }
      }
      else{
        /* OK, there was a pair, it's just invalid: According to Section
           7.2.1.4, we need to resurrect it
        */
        if(found_invalid->state == NR_ICE_PAIR_STATE_FAILED){
          pair=found_invalid;

          r_log(LOG_ICE,LOG_INFO,"ICE-PEER(%s)/CAND-PAIR(%s): received STUN check on invalid pair, resurrecting: %s",comp->stream->pctx->label,pair->codeword,pair->as_string);
          nr_ice_candidate_pair_set_state(pair->pctx,pair,NR_ICE_PAIR_STATE_WAITING);
        }
        else{
          /* This shouldn't happen */
          r_log(LOG_ICE,LOG_ERR,"ICE-PEER(%s)/CAND-PAIR(%s): received STUN check on invalid pair that was not in state FAILED; this should not happen: %s",comp->stream->pctx->label,pair->codeword,pair->as_string);
          *error=500;
          ABORT(R_BAD_DATA);
        }
      }
    }

    /* OK, we've got a pair to work with. Turn it on */
    assert(pair);
    if(nr_stun_message_has_attribute(sreq,NR_STUN_ATTR_USE_CANDIDATE,0)){
      if(comp->stream->pctx->controlling){
        r_log(LOG_ICE,LOG_WARNING,"ICE-PEER(%s)/CAND_PAIR(%s): Peer sent USE-CANDIDATE but is controlled",comp->stream->pctx->label, pair->codeword);
      }
      else{
        /* If this is the first time we've noticed this is nominated...*/
        pair->peer_nominated=1;

        if(pair->state==NR_ICE_PAIR_STATE_SUCCEEDED && !pair->nominated){
          pair->nominated=1;

          if(r=nr_ice_component_nominated_pair(pair->remote->component, pair)) {
            *error=(r==R_NO_MEMORY)?500:400;
            ABORT(r);
          }
        }
      }
    }

    if(r=nr_ice_candidate_pair_do_triggered_check(comp->stream->pctx,pair)) {
      *error=(r==R_NO_MEMORY)?500:400;
      ABORT(r);
    }

    _status=0;
  abort:
    if(_status){
      if (new_pcand_created)
        nr_ice_candidate_destroy(&pcand);
      assert(*error != 0);
      if(r!=R_NO_MEMORY) assert(*error != 500);
    }
    return(_status);
  }

static int nr_ice_component_stun_server_cb(void *cb_arg,nr_stun_server_ctx *stun_ctx,nr_socket *sock, nr_stun_server_request *req, int *dont_free, int *error)
  {
    nr_ice_component *comp=cb_arg;
    nr_transport_addr local_addr;
    int r,_status;

    /* Find the candidate pair that this maps to */
    if(r=nr_socket_getaddr(sock,&local_addr)) {
      *error=500;
      ABORT(r);
    }

    if (r=nr_ice_component_process_incoming_check(comp, &local_addr, req, error))
      ABORT(r);

    _status=0;
 abort:
    return(_status);
  }

int nr_ice_component_service_pre_answer_requests(nr_ice_peer_ctx *pctx, nr_ice_component *pcomp, char *username, int *serviced)
  {
    nr_ice_pre_answer_request *r1,*r2;
    nr_ice_component *comp = pcomp->local_component;
    int r,_status;

    if (serviced)
      *serviced = 0;

    r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/STREAM(%s)/COMP(%d): looking for pre-answer requests",pctx->label,comp->stream->label,comp->component_id);

    STAILQ_FOREACH_SAFE(r1, &comp->pre_answer_reqs, entry, r2) {
      if (!strcmp(r1->username, username)) {
        int error = 0;

        r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/STREAM(%s)/COMP(%d): found pre-answer request",pctx->label,comp->stream->label,comp->component_id);
        r = nr_ice_component_process_incoming_check(pcomp, &r1->local_addr, &r1->req, &error);
        if (r) {
          r_log(LOG_ICE,LOG_INFO,"ICE-PEER(%s)/STREAM(%s)/COMP(%d): error processing pre-answer request. Would have returned %d",pctx->label,comp->stream->label,comp->component_id, error);
        }
        (*serviced)++;
        STAILQ_REMOVE(&comp->pre_answer_reqs,r1,nr_ice_pre_answer_request_, entry);
        nr_ice_pre_answer_request_destroy(&r1);
      }
    }

    _status=0;
     return(_status);
  }

int nr_ice_component_pair_candidate(nr_ice_peer_ctx *pctx, nr_ice_component *pcomp, nr_ice_candidate *lcand, int pair_all_remote)
  {
    int r, _status;
    nr_ice_candidate *pcand;
    nr_ice_cand_pair *pair=0;
    char codeword[5];

    nr_ice_compute_codeword(lcand->label,strlen(lcand->label),codeword);
    r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/CAND(%s): Pairing local candidate %s",pctx->label,codeword,lcand->label);

    switch(lcand->type){
      case HOST:
        break;
      case SERVER_REFLEXIVE:
      case PEER_REFLEXIVE:
        /* Don't actually pair these candidates */
        goto done;
        break;
      case RELAYED:
        break;
      default:
        assert(0);
        ABORT(R_INTERNAL);
        break;
    }

    TAILQ_FOREACH(pcand, &pcomp->candidates, entry_comp){
      if (lcand->tcp_type && !pcand->tcp_type)
        continue;
      if (!lcand->tcp_type && pcand->tcp_type)
        continue;
      if (lcand->tcp_type == TCP_TYPE_ACTIVE && pcand->tcp_type != TCP_TYPE_PASSIVE)
        continue;
      if (lcand->tcp_type == TCP_TYPE_SO && pcand->tcp_type != TCP_TYPE_SO)
        continue;
      if (lcand->tcp_type == TCP_TYPE_PASSIVE)
        continue;
      if(pcand->addr.ip_version != lcand->addr.ip_version)
        continue;

      /*
        Two modes, depending on |pair_all_remote|

        1. Pair remote candidates which have not been paired
           (used in initial pairing or in processing the other side's
           trickle candidates).
        2. Pair any remote candidate (used when processing our own
           trickle candidates).
      */
      if (pair_all_remote || (pcand->state == NR_ICE_CAND_PEER_CANDIDATE_UNPAIRED)) {
        if (pair_all_remote) {
          /* When a remote candidate arrives after the start of checking, but
           * before the gathering of local candidates, it can be in UNPAIRED */
          pcand->state = NR_ICE_CAND_PEER_CANDIDATE_PAIRED;
        }

        nr_ice_compute_codeword(pcand->label,strlen(pcand->label),codeword);
        r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/CAND(%s): Pairing with peer candidate %s", pctx->label, codeword, pcand->label);

        if(r=nr_ice_candidate_pair_create(pctx,lcand,pcand,&pair))
          ABORT(r);

        if(r=nr_ice_component_insert_pair(pcomp, pair))
          ABORT(r);
      }
    }

   done:
    _status = 0;
   abort:
    return(_status);
  }

int nr_ice_component_pair_candidates(nr_ice_peer_ctx *pctx, nr_ice_component *lcomp,nr_ice_component *pcomp)
  {
    nr_ice_candidate *lcand, *pcand;
    nr_ice_socket *isock;
    int r,_status;

    r_log(LOG_ICE,LOG_DEBUG,"Pairing candidates======");

    /* Create the candidate pairs */
    lcand=TAILQ_FIRST(&lcomp->candidates);
    while(lcand){
      if (lcand->state == NR_ICE_CAND_STATE_INITIALIZED) {
        if ((r = nr_ice_component_pair_candidate(pctx, pcomp, lcand, 0)))
          ABORT(r);
      }

      lcand=TAILQ_NEXT(lcand,entry_comp);
    }

    /* Mark all peer candidates as paired */
    pcand=TAILQ_FIRST(&pcomp->candidates);
    while(pcand){
      pcand->state = NR_ICE_CAND_PEER_CANDIDATE_PAIRED;

      pcand=TAILQ_NEXT(pcand,entry_comp);

    }

    /* Now register the STUN server callback for this component.
       Note that this is a per-component CB so we only need to
       do this once.
    */
    if (pcomp->state != NR_ICE_COMPONENT_RUNNING) {
      isock=STAILQ_FIRST(&lcomp->sockets);
      while(isock){
        if(r=nr_stun_server_add_client(isock->stun_server,pctx->label,
          pcomp->stream->r2l_user,&pcomp->stream->r2l_pass,nr_ice_component_stun_server_cb,pcomp)) {
            ABORT(r);
        }
        isock=STAILQ_NEXT(isock,entry);
      }
    }

    pcomp->state = NR_ICE_COMPONENT_RUNNING;

    _status=0;
  abort:
    return(_status);
  }

/* Fires when we have an incoming candidate that doesn't correspond to an existing
   remote peer. This is either pre-answer or just spurious. Store it in the
   component for use when we see the actual answer, at which point we need
   to do the procedures from S 7.2.1 in nr_ice_component_stun_server_cb.
 */
static int nr_ice_component_stun_server_default_cb(void *cb_arg,nr_stun_server_ctx *stun_ctx,nr_socket *sock, nr_stun_server_request *req, int *dont_free, int *error)
  {
    int r, _status;
    nr_ice_component *comp = (nr_ice_component *)cb_arg;
    nr_ice_pre_answer_request *par = 0;
    r_log(LOG_ICE,LOG_DEBUG,"ICE(%s)/STREAM(%s)/COMP(%d): Received STUN request pre-answer from %s",
          comp->ctx->label, comp->stream->label, comp->component_id, req->src_addr.as_string);

    if (r=nr_ice_pre_answer_request_create(sock, req, &par))
      ABORT(r);

    *dont_free = 1;
    STAILQ_INSERT_TAIL(&comp->pre_answer_reqs, par, entry);

    _status=0;
 abort:
    return(_status);
  }

int nr_ice_component_nominated_pair(nr_ice_component *comp, nr_ice_cand_pair *pair)
  {
    int r,_status;
    int fire_cb=0;
    nr_ice_cand_pair *p2;

    if(!comp->nominated)
      fire_cb=1;

    /* Are we changing what the nominated pair is? */
    if(comp->nominated){
      if(comp->nominated->priority > pair->priority)
        return(0);
      r_log(LOG_ICE,LOG_INFO,"ICE-PEER(%s)/STREAM(%s)/COMP(%d)/CAND-PAIR(%s): replacing pair %s with CAND-PAIR(%s)",comp->stream->pctx->label,comp->stream->label,comp->component_id,comp->nominated->codeword,comp->nominated->as_string,pair->codeword);
    }

    /* Set the new nominated pair */
    r_log(LOG_ICE,LOG_INFO,"ICE-PEER(%s)/STREAM(%s)/COMP(%d)/CAND-PAIR(%s): nominated pair is %s",comp->stream->pctx->label,comp->stream->label,comp->component_id,pair->codeword,pair->as_string);
    comp->state=NR_ICE_COMPONENT_NOMINATED;
    comp->nominated=pair;
    comp->active=pair;

    r_log(LOG_ICE,LOG_INFO,"ICE-PEER(%s)/STREAM(%s)/COMP(%d)/CAND-PAIR(%s): cancelling all pairs but %s",comp->stream->pctx->label,comp->stream->label,comp->component_id,pair->codeword,pair->as_string);

    /* Cancel checks in WAITING and FROZEN per ICE S 8.1.2 */
    p2=TAILQ_FIRST(&comp->stream->check_list);
    while(p2){
      if((p2 != pair) &&
         (p2->remote->component->component_id == comp->component_id) &&
         ((p2->state == NR_ICE_PAIR_STATE_FROZEN) ||
	  (p2->state == NR_ICE_PAIR_STATE_WAITING))) {
        r_log(LOG_ICE,LOG_INFO,"ICE-PEER(%s)/STREAM(%s)/COMP(%d)/CAND-PAIR(%s): cancelling FROZEN/WAITING pair %s because CAND-PAIR(%s) was nominated.",comp->stream->pctx->label,comp->stream->label,comp->component_id,p2->codeword,p2->as_string,pair->codeword);

        if(r=nr_ice_candidate_pair_cancel(pair->pctx,p2))
          ABORT(r);
      }

      p2=TAILQ_NEXT(p2,entry);
    }
    r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/STREAM(%s)/COMP(%d): cancelling done",comp->stream->pctx->label,comp->stream->label,comp->component_id);

    if(r=nr_ice_media_stream_component_nominated(comp->stream,comp))
      ABORT(r);

    _status=0;
  abort:
    return(_status);
  }

static int nr_ice_component_have_all_pairs_failed(nr_ice_component *comp)
  {
    nr_ice_cand_pair *p2;

    p2=TAILQ_FIRST(&comp->stream->check_list);
    while(p2){
      if(comp->component_id==p2->local->component_id){
        switch(p2->state){
        case NR_ICE_PAIR_STATE_FROZEN:
        case NR_ICE_PAIR_STATE_WAITING:
        case NR_ICE_PAIR_STATE_IN_PROGRESS:
        case NR_ICE_PAIR_STATE_SUCCEEDED:
            return(0);
        case NR_ICE_PAIR_STATE_FAILED:
        case NR_ICE_PAIR_STATE_CANCELLED:
            /* states that will never be recovered from */
            break;
        default:
            assert(0);
            break;
        }
      }

      p2=TAILQ_NEXT(p2,entry);
    }

    return(1);
  }

int nr_ice_component_failed_pair(nr_ice_component *comp, nr_ice_cand_pair *pair)
  {
    return nr_ice_component_check_if_failed(comp);
  }

int nr_ice_component_check_if_failed(nr_ice_component *comp)
  {
    if (comp->state == NR_ICE_COMPONENT_RUNNING) {
      /* Don't do anything to streams that aren't currently running */
      r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/STREAM(%s)/COMP(%d): Checking whether component needs to be marked failed.",comp->stream->pctx->label,comp->stream->label,comp->component_id);

      if (!comp->stream->pctx->trickle_grace_period_timer &&
          nr_ice_component_have_all_pairs_failed(comp)) {
        r_log(LOG_ICE,LOG_INFO,"ICE-PEER(%s)/STREAM(%s)/COMP(%d): All pairs are failed, and grace period has elapsed. Marking component as failed.",comp->stream->pctx->label,comp->stream->label,comp->component_id);
        return nr_ice_media_stream_component_failed(comp->stream,comp);
      }
    }

    return(0);
  }

int nr_ice_component_select_pair(nr_ice_peer_ctx *pctx, nr_ice_component *comp)
  {
    nr_ice_cand_pair **pairs=0;
    int ct=0;
    nr_ice_cand_pair *pair;
    int r,_status;

    /* Size the array */
    pair=TAILQ_FIRST(&comp->stream->check_list);
    while(pair){
      if (comp->component_id == pair->local->component_id)
          ct++;

      pair=TAILQ_NEXT(pair,entry);
    }

    /* Make and fill the array */
    if(!(pairs=RCALLOC(sizeof(nr_ice_cand_pair *)*ct)))
      ABORT(R_NO_MEMORY);

    ct=0;
    pair=TAILQ_FIRST(&comp->stream->check_list);
    while(pair){
      if (comp->component_id == pair->local->component_id)
          pairs[ct++]=pair;

      pair=TAILQ_NEXT(pair,entry);
    }

    if (pctx->handler) {
      if(r=pctx->handler->vtbl->select_pair(pctx->handler->obj,
        comp->stream,comp->component_id,pairs,ct))
        ABORT(r);
    }

    _status=0;
  abort:
    RFREE(pairs);
    return(_status);
  }


static void nr_ice_component_keepalive_cb(NR_SOCKET s, int how, void *cb_arg)
  {
    nr_ice_component *comp=cb_arg;
    UINT4 keepalive_timeout;

    assert(comp->keepalive_ctx);

    if(NR_reg_get_uint4(NR_ICE_REG_KEEPALIVE_TIMER,&keepalive_timeout)){
      keepalive_timeout=15000; /* Default */
    }

    if(comp->keepalive_needed)
      nr_stun_client_force_retransmit(comp->keepalive_ctx);

    comp->keepalive_needed=1;
    NR_ASYNC_TIMER_SET(keepalive_timeout,nr_ice_component_keepalive_cb,cb_arg,&comp->keepalive_timer);
  }


/* Close the underlying sockets for everything but the nominated candidate */
int nr_ice_component_finalize(nr_ice_component *lcomp, nr_ice_component *rcomp)
  {
    nr_ice_socket *isock=0;
    int r,_status;
    nr_ice_socket *s1,*s2;

    if(rcomp->state==NR_ICE_COMPONENT_NOMINATED){
      assert(rcomp->active == rcomp->nominated);
      isock=rcomp->nominated->local->isock;
    }

    STAILQ_FOREACH_SAFE(s1, &lcomp->sockets, entry, s2){
      if(s1!=isock){
        STAILQ_REMOVE(&lcomp->sockets,s1,nr_ice_socket_,entry);
        nr_ice_socket_destroy(&s1);
      }
    }

    /* Set up the keepalives for the chosen socket */
    if(r=nr_stun_client_ctx_create("keepalive",rcomp->nominated->local->osock,
      &rcomp->nominated->remote->addr,0,&rcomp->keepalive_ctx))
      ABORT(r);
    if(r=nr_stun_client_start(rcomp->keepalive_ctx,NR_STUN_CLIENT_MODE_KEEPALIVE,0,0))
      ABORT(r);
    nr_ice_component_keepalive_cb(0,0,rcomp);


    _status=0;
  abort:

    return(_status);
  }


int nr_ice_component_insert_pair(nr_ice_component *pcomp, nr_ice_cand_pair *pair)
  {
    int r,_status;

    /* Pairs for peer reflexive are marked SUCCEEDED immediately */
    if (pair->state != NR_ICE_PAIR_STATE_FROZEN &&
        pair->state != NR_ICE_PAIR_STATE_SUCCEEDED){
      assert(0);
      ABORT(R_BAD_ARGS);
    }

    if(r=nr_ice_candidate_pair_insert(&pair->remote->stream->check_list,pair))
      ABORT(r);

    /* Make sure the check timer is running, if the stream was previously
     * started. We will not start streams just because a pair was created,
     * unless it is the first pair to be created across all streams. */
    r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/CAND-PAIR(%s): Ensure that check timer is running for new pair %s.",pair->remote->stream->pctx->label, pair->codeword, pair->as_string);

    if(pair->remote->stream->ice_state == NR_ICE_MEDIA_STREAM_CHECKS_ACTIVE ||
       (pair->remote->stream->ice_state == NR_ICE_MEDIA_STREAM_CHECKS_FROZEN &&
        !pair->remote->stream->pctx->checks_started)){
      if(nr_ice_media_stream_start_checks(pair->remote->stream->pctx, pair->remote->stream)) {
        r_log(LOG_ICE,LOG_WARNING,"ICE-PEER(%s)/CAND-PAIR(%s): Could not restart checks for new pair %s.",pair->remote->stream->pctx->label, pair->codeword, pair->as_string);
        ABORT(R_INTERNAL);
      }
    }

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_component_get_default_candidate(nr_ice_component *comp, nr_ice_candidate **candp, int ip_version)
  {
    int _status;
    nr_ice_candidate *cand;
    nr_ice_candidate *best_cand = NULL;

    /* We have the component. Now find the "best" candidate, making
       use of the fact that more "reliable" candidate types have
       higher numbers. So, we sort by type and then priority within
       type
    */
    cand=TAILQ_FIRST(&comp->candidates);
    while(cand){
      if (cand->state == NR_ICE_CAND_STATE_INITIALIZED &&
          cand->addr.ip_version == ip_version) {
        if (!best_cand) {
          best_cand = cand;
        }
        else if (best_cand->type < cand->type) {
          best_cand = cand;
        } else if (best_cand->type == cand->type &&
                   best_cand->priority < cand->priority) {
          best_cand = cand;
        }
      }

      cand=TAILQ_NEXT(cand,entry_comp);
    }

    /* No candidates */
    if (!best_cand)
      ABORT(R_NOT_FOUND);

    *candp = best_cand;

    _status=0;
  abort:
    return(_status);

  }

