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
#include "ice_reg.h"

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

#define MAXADDRS 100 // Ridiculously high
/* Make all the candidates we can make at the beginning */
int nr_ice_component_initialize(struct nr_ice_ctx_ *ctx,nr_ice_component *component)
  {
    int r,_status;
    nr_transport_addr addrs[MAXADDRS];
    nr_socket *sock;
    nr_ice_socket *isock=0;
    nr_ice_candidate *cand=0;
    char *lufrag;
    char *lpwd;
    Data pwd;
    int addr_ct;
    int i;
    int j;
    char label[256];

    r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): initializing component with id %d",ctx->label,component->component_id);

    /* First, gather all the local addresses we have */
    if(r=nr_stun_find_local_addresses(addrs,MAXADDRS,&addr_ct)) {
      r_log(LOG_ICE,LOG_ERR,"ICE(%s): unable to find local addresses",ctx->label);
      ABORT(r);
    }

    if(addr_ct==0){
      r_log(LOG_ICE,LOG_ERR,"ICE(%s): no local addresses available",ctx->label);
      ABORT(R_NOT_FOUND);
    }

    /* Now one ice_socket for each address */
    for(i=0;i<addr_ct;i++){
      char suppress;

      if(r=NR_reg_get2_char(NR_ICE_REG_SUPPRESS_INTERFACE_PRFX,addrs[i].ifname,&suppress)){
        if(r!=R_NOT_FOUND)
          ABORT(r);
      }
      else{
        if(suppress)
          continue;
      }


      r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): host address %s",ctx->label,addrs[i].as_string);
      if(r=nr_socket_local_create(&addrs[i],&sock)){
        r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): couldn't create socket for address %s",ctx->label,addrs[i].as_string);
        continue;
      }

      if(r=nr_ice_socket_create(ctx,component,sock,&isock))
        ABORT(r);

      /* Create one host candidate */
      if(r=nr_ice_candidate_create(ctx,component,isock,sock,HOST,0,
        component->component_id,&cand))
        ABORT(r);

      TAILQ_INSERT_TAIL(&component->candidates,cand,entry_comp);
      component->candidate_ct++;
      cand=0;

      /* And a srvrflx candidate for each STUN server */
      for(j=0;j<ctx->stun_server_ct;j++){
        if(r=nr_ice_candidate_create(ctx,component,
          isock,sock,SERVER_REFLEXIVE,
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

        /* srvrflx */
        if(r=nr_ice_candidate_create(ctx,component,
          isock,sock,SERVER_REFLEXIVE,
          &ctx->turn_servers[j].turn_server,component->component_id,&cand))
          ABORT(r);
        cand->state=NR_ICE_CAND_STATE_INITIALIZING; /* Don't start */
        cand->done_cb=nr_ice_initialize_finished_cb;
        cand->cb_arg=ctx;

        TAILQ_INSERT_TAIL(&component->candidates,cand,entry_comp);
        component->candidate_ct++;
        srvflx_cand=cand;

        /* relayed*/
        if(r=nr_socket_turn_create(sock, &turn_sock))
          ABORT(r);
        if(r=nr_ice_candidate_create(ctx,component,
          isock,turn_sock,RELAYED,
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
      snprintf(label, sizeof(label), "server(%s)", addrs[i].as_string);
      if(r=nr_stun_server_ctx_create(label,sock,&isock->stun_server))
        ABORT(r);
      if(r=nr_ice_socket_register_stun_server(isock,isock->stun_server,&isock->stun_server_handle))
        ABORT(r);
      /* Add the default STUN credentials so that we can respond before
         we hear about the peer. Note: we need to recompute these because
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

      if(r=nr_stun_server_add_default_client(isock->stun_server, lufrag, &pwd, nr_ice_component_stun_server_default_cb, component))
        ABORT(r);

      STAILQ_INSERT_TAIL(&component->sockets,isock,entry);
    }
    isock=0;


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
        if(r=nr_ice_candidate_initialize(cand,nr_ice_initialize_finished_cb,ctx)){
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

/* Prune redundant candidates. We use an n^2 algorithm for now.

   This algorithm combined with the other algorithms, favors
   host > srflx > relay

   This actually won't prune relayed in the very rare
   case that relayed is the same. Not relevant in practice.
*/

int nr_ice_component_prune_candidates(nr_ice_ctx *ctx, nr_ice_component *comp)
  {
    nr_ice_candidate *c1,*c1n,*c2;

    c1=TAILQ_FIRST(&comp->candidates);
    while(c1){
      c1n=TAILQ_NEXT(c1,entry_comp);
      if(c1->state!=NR_ICE_CAND_STATE_INITIALIZED){
        r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): Removing non-initialized candidate %s",
          ctx->label,c1->label);
        if (c1->state == NR_ICE_CAND_STATE_INITIALIZING) {
          r_log(LOG_ICE,LOG_NOTICE, "ICE(%s): Removing candidate %s which is in INITIALIZING state",
            ctx->label, c1->label);
        }
        TAILQ_REMOVE(&comp->candidates,c1,entry_comp);
        comp->candidate_ct--;
        TAILQ_REMOVE(&c1->isock->candidates,c1,entry_sock);
        /* schedule this delete for later as we don't want to delete the underlying
         * objects while in the middle of a callback on one of those objects */
        NR_ASYNC_SCHEDULE(nr_ice_candidate_destroy_cb,c1);
        goto next_c1;
      }

      c2=TAILQ_NEXT(c1,entry_comp);

      while(c2){
        nr_ice_candidate *tmp;

        if(!nr_transport_addr_cmp(&c1->base,&c2->base,NR_TRANSPORT_ADDR_CMP_MODE_ALL) && !nr_transport_addr_cmp(&c1->addr,&c2->addr,NR_TRANSPORT_ADDR_CMP_MODE_ALL)){

          if((c1->type == c2->type) ||
            (c1->type==HOST && c2->type == SERVER_REFLEXIVE) ||
            (c2->type==HOST && c1->type == SERVER_REFLEXIVE)){

            /* OK these are redundant. Remove the lower pri one */
            tmp=c2;
            c2=TAILQ_NEXT(c2,entry_comp);
            if(c1n==tmp)
              c1n=c2;

            r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): Removing redundant candidate %s",
              ctx->label,tmp->label);

            TAILQ_REMOVE(&comp->candidates,tmp,entry_comp);
            comp->candidate_ct--;
            TAILQ_REMOVE(&tmp->isock->candidates,tmp,entry_sock);

            nr_ice_candidate_destroy(&tmp);
          }
        }
        else{
          c2=TAILQ_NEXT(c2,entry_comp);
        }
      }
    next_c1:
      c1=c1n;
    }

    return(0);
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

    r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/STREAM(%s)(%d): received request from %s",comp->stream->pctx->label,comp->stream->label,comp->component_id,req->src_addr.as_string);

    /* Check for role conficts (7.2.1.1) */
    if(comp->stream->pctx->controlling){
      if(nr_stun_message_has_attribute(sreq,NR_STUN_ATTR_ICE_CONTROLLING,&attr)){
        /* OK, there is a conflict. Who's right? */
        r_log(LOG_ICE,LOG_ERR,"ICE-PEER(%s): role conflict, both controlling",comp->stream->pctx->label);

        if(attr->u.ice_controlling > comp->stream->pctx->tiebreaker){
          /* They are: switch */
          r_log(LOG_ICE,LOG_ERR,"ICE-PEER(%s): switching to controlled",comp->stream->pctx->label);

          comp->stream->pctx->controlling=0;
        }
        else {
          /* We are: throw an error */
          r_log(LOG_ICE,LOG_ERR,"ICE-PEER(%s): returning 487 role conflict",comp->stream->pctx->label);

          *error=487;
          ABORT(R_REJECTED);
        }
      }
    }
    else{
      if(nr_stun_message_has_attribute(sreq,NR_STUN_ATTR_ICE_CONTROLLED,&attr)){
        /* OK, there is a conflict. Who's right? */
        r_log(LOG_ICE,LOG_ERR,"ICE-PEER(%s): role conflict, both controlled",comp->stream->pctx->label);

        if(attr->u.ice_controlling < comp->stream->pctx->tiebreaker){
          r_log(LOG_ICE,LOG_ERR,"ICE-PEER(%s): switching to controlling",comp->stream->pctx->label);

          /* They are: switch */
          comp->stream->pctx->controlling=1;
        }
        else {
          /* We are: throw an error */
          r_log(LOG_ICE,LOG_ERR,"ICE-PEER(%s): returning 487 role conflict",comp->stream->pctx->label);

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
        r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s): Found a matching pair: %s",comp->stream->pctx->label,pair->as_string);
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

        /* We now need to make a peer reflexive */
        if(r=nr_ice_peer_peer_rflx_candidate_create(comp->stream->pctx->ctx,"prflx",comp,&req->src_addr,&pcand)) {
          *error=(r==R_NO_MEMORY)?500:400;
          ABORT(r);
        }
        if(!nr_stun_message_has_attribute(sreq,NR_STUN_ATTR_PRIORITY,&attr)){
          r_log(LOG_ICE,LOG_ERR,"ICE-PEER(%s): Rejecting stun request without priority",comp->stream->pctx->label);
          *error=487;
          ABORT(R_BAD_DATA);
        }
        pcand->priority=attr->u.priority;
        pcand->state=NR_ICE_CAND_PEER_CANDIDATE_PAIRED;;
        TAILQ_INSERT_TAIL(&comp->candidates,pcand,entry_comp);

        if(r=nr_ice_candidate_pair_create(comp->stream->pctx,cand,pcand,
             &pair)) {
          *error=(r==R_NO_MEMORY)?500:400;
          ABORT(r);
        }
        nr_ice_candidate_pair_set_state(pair->pctx,pair,NR_ICE_PAIR_STATE_FROZEN);

        if(r=nr_ice_candidate_pair_insert(&comp->stream->check_list,pair)) {
          *error=(r==R_NO_MEMORY)?500:400;
          ABORT(r);
        }

        pcand=0;
      }
      else{
        /* OK, there was a pair, it's just invalid: According to Section
           7.2.1.4, we need to resurrect it
        */
        if(found_invalid->state == NR_ICE_PAIR_STATE_FAILED){
          pair=found_invalid;

          r_log(LOG_ICE,LOG_WARNING,"ICE-PEER(%s): received STUN check on invalid pair %s: resurrecting",comp->stream->pctx->label,pair->as_string);
          nr_ice_candidate_pair_set_state(pair->pctx,pair,NR_ICE_PAIR_STATE_WAITING);
        }
        else{
          /* This shouldn't happen */
          r_log(LOG_ICE,LOG_ERR,"ICE-PEER(%s): received STUN check on invalid pair %s but not in FAILED state",comp->stream->pctx->label,pair->as_string);
          *error=500;
          ABORT(R_BAD_DATA);
        }
      }
    }

    /* OK, we've got a pair to work with. Turn it on */
    if(nr_stun_message_has_attribute(sreq,NR_STUN_ATTR_USE_CANDIDATE,0)){
      if(comp->stream->pctx->controlling){
        r_log(LOG_ICE,LOG_ERR,"ICE-PEER(%s): Peer sent USE-CANDIDATE but is controlled",comp->stream->pctx->label);
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
    assert(pair != 0);

    if(r=nr_ice_candidate_pair_do_triggered_check(comp->stream->pctx,pair)) {
      *error=(r==R_NO_MEMORY)?500:400;
      ABORT(r);
    }

    _status=0;
  abort:
    if(_status){
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

    r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/STREAM(%s)/comp(%d): looking for pre-answer requests",pctx->label,comp->stream->label,comp->component_id);

    STAILQ_FOREACH_SAFE(r1, &comp->pre_answer_reqs, entry, r2) {
      if (!strcmp(r1->username, username)) {
        int error = 0;

        r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/STREAM(%s)/comp(%d): found pre-answer request",pctx->label,comp->stream->label,comp->component_id);
        r = nr_ice_component_process_incoming_check(pcomp, &r1->local_addr, &r1->req, &error);
        if (r) {
          r_log(LOG_ICE,LOG_INFO,"ICE-PEER(%s)/STREAM(%s)/comp(%d): error processing pre-answer request. Would have returned %d",pctx->label,comp->stream->label,comp->component_id, error);
        }
        (*serviced)++;
        STAILQ_REMOVE(&comp->pre_answer_reqs,r1,nr_ice_pre_answer_request_, entry);
        nr_ice_pre_answer_request_destroy(&r1);
      }
    }

    _status=0;
 abort:
    return(_status);
  }

int nr_ice_component_pair_candidates(nr_ice_peer_ctx *pctx, nr_ice_component *lcomp,nr_ice_component *pcomp)
  {
    nr_ice_candidate *lcand,*pcand;
    nr_ice_cand_pair *pair=0;
    nr_ice_socket *isock;
    int r,_status;
    char codeword[5];

    r_log(LOG_ICE,LOG_DEBUG,"Pairing candidates======");
    /* Create the candidate pairs */
    lcand=TAILQ_FIRST(&lcomp->candidates);
    while(lcand){
      int was_paired = 0;

      nr_ice_compute_codeword(lcand->label,strlen(lcand->label),codeword);
      r_log(LOG_ICE,LOG_DEBUG,"Examining local candidate %s:%s",codeword,lcand->label);

      switch(lcand->type){
        case HOST:
          break;
        case SERVER_REFLEXIVE:
        case PEER_REFLEXIVE:
          /* Don't actually pair these candidates */
          goto next_cand;
          break;
        case RELAYED:
          break;
        default:
          assert(0);
          ABORT(R_INTERNAL);
          break;
      }

      /* PAIR with each peer*/
      if(TAILQ_EMPTY(&pcomp->candidates)) {
          /* can happen if our peer proposes no (or all bogus) candidates */
          goto next_cand;
      }
      pcand=TAILQ_FIRST(&pcomp->candidates);
      while(pcand){
        /* Only pair peer candidates which have not yet been paired.
           This allows "trickle ICE". (Not yet standardized, but
           part of WebRTC).

           TODO(ekr@rtfm.com): Add refernece to the spec when there
           is one.
         */
        if (pcand->state == NR_ICE_CAND_PEER_CANDIDATE_UNPAIRED) {
          nr_ice_compute_codeword(pcand->label,strlen(pcand->label),codeword);
          r_log(LOG_ICE,LOG_DEBUG,"Examining peer candidate %s:%s",codeword,pcand->label);

          if(r=nr_ice_candidate_pair_create(pctx,lcand,pcand,&pair))
            ABORT(r);

          if(r=nr_ice_candidate_pair_insert(&pcomp->stream->check_list,
              pair))
            ABORT(r);
        }
        else {
          was_paired = 1;
        }
        pcand=TAILQ_NEXT(pcand,entry_comp);
      }

      if(!pair)
        goto next_cand;

    next_cand:
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
    r_log(LOG_ICE,LOG_DEBUG,"ICE(%s)/STREAM(%s)/comp(%d): Received STUN request pre-answer from %s",
          comp->ctx->label, comp->stream->label, comp->component_id, req->src_addr.as_string);

    if (r=nr_ice_pre_answer_request_create(sock, req, &par))
      ABORT(r);

    *dont_free = 1;
    STAILQ_INSERT_TAIL(&comp->pre_answer_reqs, par, entry);

    _status=0;
 abort:
    return 0;
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
      r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/STREAM(%s)/comp(%d): replacing pair %s with pair %s",comp->stream->pctx->label,comp->stream->label,comp->component_id,comp->nominated->as_string,pair->as_string);
    }

    /* Set the new nominated pair */
    r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/STREAM(%s)/comp(%d): nominated pair is %s (0x%p)",comp->stream->pctx->label,comp->stream->label,comp->component_id,pair->as_string,pair);
    comp->state=NR_ICE_COMPONENT_NOMINATED;
    comp->nominated=pair;
    comp->active=pair;

    r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/STREAM(%s)/comp(%d): cancelling all pairs but %s (0x%p)",comp->stream->pctx->label,comp->stream->label,comp->component_id,pair->as_string,pair);

    /* Cancel checks in WAITING and FROZEN per ICE S 8.1.2 */
    p2=TAILQ_FIRST(&comp->stream->check_list);
    while(p2){
      if((p2 != pair) &&
         (p2->remote->component->component_id == comp->component_id) &&
         ((p2->state == NR_ICE_PAIR_STATE_FROZEN) ||
	  (p2->state == NR_ICE_PAIR_STATE_WAITING))) {
        r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/STREAM(%s)/comp(%d): cancelling pair %s (0x%p)",comp->stream->pctx->label,comp->stream->label,comp->component_id,p2->as_string,p2);

        if(r=nr_ice_candidate_pair_cancel(pair->pctx,p2))
          ABORT(r);
      }

      p2=TAILQ_NEXT(p2,entry);
    }
    r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/STREAM(%s)/comp(%d): cancelling done",comp->stream->pctx->label,comp->stream->label,comp->component_id);

    if(r=nr_ice_media_stream_component_nominated(comp->stream,comp))
      ABORT(r);

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_component_failed_pair(nr_ice_component *comp, nr_ice_cand_pair *pair)
  {
    int r,_status;
    nr_ice_cand_pair *p2;

    assert(pair->state == NR_ICE_PAIR_STATE_FAILED);

    p2=TAILQ_FIRST(&comp->stream->check_list);
    while(p2){
      if(comp->component_id==p2->local->component_id){
        switch(p2->state){
        case NR_ICE_PAIR_STATE_FROZEN:
        case NR_ICE_PAIR_STATE_WAITING:
        case NR_ICE_PAIR_STATE_IN_PROGRESS:
            /* answer component status cannot be determined yet */
            goto done;
            break;
        case NR_ICE_PAIR_STATE_SUCCEEDED:
            /* the component will succeed */
            goto done;
            break;
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

    /* all the pairs in the component are in their final states with
     * none of them being SUCCEEDED, so the component fails entirely,
     * tell the media stream that this component has failed */

    if(r=nr_ice_media_stream_component_failed(comp->stream,comp))
      ABORT(r);

  done:
    _status=0;
  abort:
    return(_status);
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
