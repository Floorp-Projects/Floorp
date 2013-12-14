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



static char *RCSSTRING __UNUSED__="$Id: ice_candidate_pair.c,v 1.2 2008/04/28 17:59:01 ekr Exp $";

#include <assert.h>
#include <string.h>
#include <nr_api.h>
#include "async_timer.h"
#include "ice_ctx.h"
#include "ice_util.h"
#include "ice_codeword.h"
#include "stun.h"

static char *nr_ice_cand_pair_states[]={"UNKNOWN","FROZEN","WAITING","IN_PROGRESS","FAILED","SUCCEEDED","CANCELLED"};

static void nr_ice_candidate_pair_restart_stun_role_change_cb(NR_SOCKET s, int how, void *cb_arg);
static void nr_ice_candidate_pair_compute_codeword(nr_ice_cand_pair *pair,
  nr_ice_candidate *lcand, nr_ice_candidate *rcand);

int nr_ice_candidate_pair_create(nr_ice_peer_ctx *pctx, nr_ice_candidate *lcand,nr_ice_candidate *rcand,nr_ice_cand_pair **pairp)
  {
    nr_ice_cand_pair *pair=0;
    UINT8 o_priority, a_priority;
    int r,_status;
    UINT4 RTO;
    nr_ice_candidate tmpcand;
    UINT8 t_priority;

    if(!(pair=RCALLOC(sizeof(nr_ice_cand_pair))))
      ABORT(R_NO_MEMORY);

    pair->pctx=pctx;

    nr_ice_candidate_pair_compute_codeword(pair,lcand,rcand);

    if(r=nr_concat_strings(&pair->as_string,pair->codeword,"|",lcand->addr.as_string,"|",
        rcand->addr.as_string,"(",lcand->label,"|",rcand->label,")", NULL))
      ABORT(r);

    nr_ice_candidate_pair_set_state(pctx,pair,NR_ICE_PAIR_STATE_FROZEN);
    pair->local=lcand;
    pair->remote=rcand;

    /* Priority computation S 5.7.2 */
    if(pctx->ctx->flags & NR_ICE_CTX_FLAGS_OFFERER)
    {
      assert(!(pctx->ctx->flags & NR_ICE_CTX_FLAGS_ANSWERER));

      o_priority=lcand->priority;
      a_priority=rcand->priority;
    }
    else{
      o_priority=rcand->priority;
      a_priority=lcand->priority;
    }
    pair->priority=(MIN(o_priority, a_priority))<<32 |
      (MAX(o_priority, a_priority))<<1 | (o_priority > a_priority?0:1);

    /*
       TODO(bcampen@mozilla.com): Would be nice to log why this candidate was
       created (initial pair generation, triggered check, and new trickle
       candidate seem to be the possibilities here).
    */
    r_log(LOG_ICE,LOG_INFO,"ICE(%s)/CAND-PAIR(%s): Pairing candidate %s (%x):%s (%x) priority=%llu (%llx)",pctx->ctx->label,pair->codeword,lcand->addr.as_string,lcand->priority,rcand->addr.as_string,rcand->priority,pair->priority,pair->priority);

    /* Foundation */
    if(r=nr_concat_strings(&pair->foundation,lcand->foundation,"|",
      rcand->foundation,NULL))
      ABORT(r);

    /* Compute the RTO per S 16 */
    RTO = MAX(100, (pctx->ctx->Ta * pctx->waiting_pairs));

    /* Make a bogus candidate to compute a theoretical peer reflexive
     * priority per S 7.1.1.1 */
    memcpy(&tmpcand, lcand, sizeof(tmpcand));
    tmpcand.type = PEER_REFLEXIVE;
    if (r=nr_ice_candidate_compute_priority(&tmpcand))
      ABORT(r);
    t_priority = tmpcand.priority;

    /* Our sending context */
    if(r=nr_stun_client_ctx_create(pair->as_string,
      lcand->osock,
      &rcand->addr,RTO,&pair->stun_client))
      ABORT(r);
    if(!(pair->stun_client->params.ice_binding_request.username=r_strdup(rcand->stream->l2r_user)))
      ABORT(R_NO_MEMORY);
    if(r=r_data_copy(&pair->stun_client->params.ice_binding_request.password,
      &rcand->stream->l2r_pass))
      ABORT(r);
    pair->stun_client->params.ice_binding_request.priority=t_priority;
    /* TODO(ekr@rtfm.com): Do we need to frob this when we change role. Bug 890667 */
    pair->stun_client->params.ice_binding_request.control = pctx->controlling?
      NR_ICE_CONTROLLING:NR_ICE_CONTROLLED;
    pair->stun_client->params.ice_use_candidate.priority=t_priority;

    pair->stun_client->params.ice_binding_request.tiebreaker=pctx->tiebreaker;

    *pairp=pair;

    _status=0;
  abort:
    if(_status){
      nr_ice_candidate_pair_destroy(&pair);
    }
    return(_status);
  }

int nr_ice_candidate_pair_destroy(nr_ice_cand_pair **pairp)
  {
    nr_ice_cand_pair *pair;

    if(!pairp || !*pairp)
      return(0);

    pair=*pairp;
    *pairp=0;

    RFREE(pair->as_string);
    RFREE(pair->foundation);
    nr_ice_socket_deregister(pair->local->isock,pair->stun_client_handle);
    if (pair->stun_client) {
      RFREE(pair->stun_client->params.ice_binding_request.username);
      RFREE(pair->stun_client->params.ice_binding_request.password.data);
      nr_stun_client_ctx_destroy(&pair->stun_client);
    }

    NR_async_timer_cancel(pair->stun_cb_timer);
    NR_async_timer_cancel(pair->restart_role_change_cb_timer);
    NR_async_timer_cancel(pair->restart_nominated_cb_timer);

    RFREE(pair);
    return(0);
  }

int nr_ice_candidate_pair_unfreeze(nr_ice_peer_ctx *pctx, nr_ice_cand_pair *pair)
  {
    assert(pair->state==NR_ICE_PAIR_STATE_FROZEN);

    nr_ice_candidate_pair_set_state(pctx,pair,NR_ICE_PAIR_STATE_WAITING);

    return(0);
  }

static void nr_ice_candidate_pair_stun_cb(NR_SOCKET s, int how, void *cb_arg)
  {
    int r,_status;
    nr_ice_cand_pair *pair=cb_arg,*orig_pair;
    nr_ice_candidate *cand=0;
    nr_stun_message *sres;
    nr_transport_addr *request_src;
    nr_transport_addr *request_dst;
    nr_transport_addr *response_src;
    nr_transport_addr response_dst;
    nr_stun_message_attribute *attr;

    pair->stun_cb_timer=0;

    r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s)/STREAM(%s)/CAND-PAIR(%s): STUN cb on pair addr = %s",
      pair->pctx->label,pair->local->stream->label,pair->codeword,pair->as_string);

    /* This ordinarily shouldn't happen, but can if we're
       doing the second check to confirm nomination.
       Just bail out */
    if(pair->state==NR_ICE_PAIR_STATE_SUCCEEDED)
      goto done;

    switch(pair->stun_client->state){
      case NR_STUN_CLIENT_STATE_FAILED:
        sres=pair->stun_client->response;
        if(sres && nr_stun_message_has_attribute(sres,NR_STUN_ATTR_ERROR_CODE,&attr)&&attr->u.error_code.number==487){

          /*
           * Flip the controlling bit; subsequent 487s for other pairs will be
           * ignored, since we abandon their STUN transactions.
           */
          nr_ice_peer_ctx_switch_controlling_role(pair->pctx);

          return;
        }
        /* Fall through */
      case NR_STUN_CLIENT_STATE_TIMED_OUT:
        nr_ice_candidate_pair_set_state(pair->pctx,pair,NR_ICE_PAIR_STATE_FAILED);
        break;
      case NR_STUN_CLIENT_STATE_DONE:
        /* make sure the addresses match up S 7.1.2.2 */
        response_src=&pair->stun_client->peer_addr;
        request_dst=&pair->remote->addr;
        if (nr_transport_addr_cmp(response_src,request_dst,NR_TRANSPORT_ADDR_CMP_MODE_ALL)){
          r_log(LOG_ICE,LOG_WARNING,"ICE-PEER(%s)/CAND-PAIR(%s): Peer address mismatch %s != %s",pair->pctx->label,pair->codeword,response_src->as_string,request_dst->as_string);
          nr_ice_candidate_pair_set_state(pair->pctx,pair,NR_ICE_PAIR_STATE_FAILED);
          break;
        }
        request_src=&pair->stun_client->my_addr;
        nr_socket_getaddr(pair->local->osock,&response_dst);
        if (nr_transport_addr_cmp(request_src,&response_dst,NR_TRANSPORT_ADDR_CMP_MODE_ALL)){
          r_log(LOG_ICE,LOG_WARNING,"ICE-PEER(%s)/CAND-PAIR(%s): Local address mismatch %s != %s",pair->pctx->label,pair->codeword,request_src->as_string,response_dst.as_string);
          nr_ice_candidate_pair_set_state(pair->pctx,pair,NR_ICE_PAIR_STATE_FAILED);
          break;
        }

        if(strlen(pair->stun_client->results.ice_binding_response.mapped_addr.as_string)==0){
          /* we're using the mapped_addr returned by the server to lookup our
           * candidate, but if the server fails to do that we can't perform
           * the lookup -- this may be a BUG because if we've gotten here
           * then the transaction ID check succeeded, and perhaps we should
           * just assume that it's the server we're talking to and that our
           * peer is ok, but I'm not sure how that'll interact with the
           * peer reflexive logic below */
          r_log(LOG_ICE,LOG_WARNING,"ICE-PEER(%s)/CAND-PAIR(%s): server failed to return mapped address on pair %s", pair->pctx->label,pair->codeword,pair->as_string);
          nr_ice_candidate_pair_set_state(pair->pctx,pair,NR_ICE_PAIR_STATE_FAILED);
          break;
        }
        else if(!nr_transport_addr_cmp(&pair->local->addr,&pair->stun_client->results.ice_binding_response.mapped_addr,NR_TRANSPORT_ADDR_CMP_MODE_ALL)){
          nr_ice_candidate_pair_set_state(pair->pctx,pair,NR_ICE_PAIR_STATE_SUCCEEDED);
        }
        else{
          /* OK, this didn't correspond to a pair on the check list, but
             it probably matches one of our candidates */

          cand=TAILQ_FIRST(&pair->local->component->candidates);
          while(cand){
            if(!nr_transport_addr_cmp(&cand->addr,&pair->stun_client->results.ice_binding_response.mapped_addr,NR_TRANSPORT_ADDR_CMP_MODE_ALL))
              break;

            cand=TAILQ_NEXT(cand,entry_comp);
          }

          /* OK, nothing found, must be peer reflexive */
          if(!cand){
            if(r=nr_ice_candidate_create(pair->pctx->ctx,
              pair->local->component,pair->local->isock,pair->local->osock,
              PEER_REFLEXIVE,0,pair->local->component->component_id,&cand))
              ABORT(r);
            if(r=nr_transport_addr_copy(&cand->addr,&pair->stun_client->results.ice_binding_response.mapped_addr))
              ABORT(r);
            cand->state=NR_ICE_CAND_STATE_INITIALIZED;
            TAILQ_INSERT_TAIL(&pair->local->component->candidates,cand,entry_comp);
          }

          /* Note: we stomp the existing pair! */
          orig_pair=pair;
          if(r=nr_ice_candidate_pair_create(pair->pctx,cand,pair->remote,
            &pair))
            ABORT(r);

          nr_ice_candidate_pair_set_state(pair->pctx,pair,NR_ICE_PAIR_STATE_SUCCEEDED);

          if(r=nr_ice_component_insert_pair(pair->remote->component,pair))
            ABORT(r);

          /* If the original pair was nominated, make us nominated,
             since we replace him*/
          if(orig_pair->peer_nominated)
            pair->peer_nominated=1;


          /* Now mark the orig pair failed */
          nr_ice_candidate_pair_set_state(orig_pair->pctx,orig_pair,NR_ICE_PAIR_STATE_FAILED);

        }

        /* Should we set nominated? */
        if(pair->pctx->controlling){
          if(pair->pctx->ctx->flags & NR_ICE_CTX_FLAGS_AGGRESSIVE_NOMINATION)
            pair->nominated=1;
        }
        else{
          if(pair->peer_nominated)
            pair->nominated=1;
        }


        /* increment the number of valid pairs in the component */
        /* We don't bother to maintain a separate valid list */
        pair->remote->component->valid_pairs++;

        /* S 7.1.2.2: unfreeze other pairs with the same foundation*/
        if(r=nr_ice_media_stream_unfreeze_pairs_foundation(pair->remote->stream,pair->foundation))
          ABORT(r);

        /* Deal with this pair being nominated */
        if(pair->nominated){
          if(r=nr_ice_component_nominated_pair(pair->remote->component, pair))
            ABORT(r);
        }

        break;
      default:
        ABORT(R_INTERNAL);
    }

    /* If we're controlling but in regular mode, ask the handler
       if he wants to nominate something and stop... */
    if(pair->pctx->controlling && !(pair->pctx->ctx->flags & NR_ICE_CTX_FLAGS_AGGRESSIVE_NOMINATION)){

      if(r=nr_ice_component_select_pair(pair->pctx,pair->remote->component)){
        if(r!=R_NOT_FOUND)
          ABORT(r);
      }
    }

  done:
    _status=0;
  abort:
    return;
  }

static void nr_ice_candidate_pair_restart(nr_ice_peer_ctx *pctx, nr_ice_cand_pair *pair)
  {
    int r,_status;
    UINT4 mode;

    nr_ice_candidate_pair_set_state(pctx,pair,NR_ICE_PAIR_STATE_IN_PROGRESS);

    /* Start STUN */
    if(pair->pctx->controlling && (pair->pctx->ctx->flags & NR_ICE_CTX_FLAGS_AGGRESSIVE_NOMINATION))
      mode=NR_ICE_CLIENT_MODE_USE_CANDIDATE;
    else
      mode=NR_ICE_CLIENT_MODE_BINDING_REQUEST;

    nr_stun_client_reset(pair->stun_client);

    if(r=nr_stun_client_start(pair->stun_client,mode,nr_ice_candidate_pair_stun_cb,pair))
      ABORT(r);

    if ((r=nr_ice_ctx_remember_id(pair->pctx->ctx, pair->stun_client->request))) {
      /* ignore if this fails (which it shouldn't) because it's only an
       * optimization and the cleanup routines are not going to do the right
       * thing if this fails */
      assert(0);
    }

    _status=0;
  abort:
    if(_status){
      /* Don't fire the CB, but schedule it to fire ASAP */
      assert(!pair->stun_cb_timer);
      NR_ASYNC_TIMER_SET(0,nr_ice_candidate_pair_stun_cb,pair, &pair->stun_cb_timer);
      _status=0;
    }
  }

int nr_ice_candidate_pair_start(nr_ice_peer_ctx *pctx, nr_ice_cand_pair *pair)
  {
    int r,_status;
    UINT4 mode;

    /* Register the stun ctx for when responses come in*/
    if(r=nr_ice_socket_register_stun_client(pair->local->isock,pair->stun_client,&pair->stun_client_handle))
      ABORT(r);

    nr_ice_candidate_pair_restart(pctx, pair);

    _status=0;
  abort:
    return(_status);
  }


int nr_ice_candidate_pair_do_triggered_check(nr_ice_peer_ctx *pctx, nr_ice_cand_pair *pair)
  {
    int r,_status;

    r_log(LOG_ICE,LOG_INFO,"ICE-PEER(%s)/CAND-PAIR(%s): triggered check on %s",pctx->label,pair->codeword,pair->as_string);

    switch(pair->state){
      case NR_ICE_PAIR_STATE_FROZEN:
        nr_ice_candidate_pair_set_state(pctx,pair,NR_ICE_PAIR_STATE_WAITING);
        /* Fall through */
      case NR_ICE_PAIR_STATE_WAITING:
        /* Start the checks */
        if(r=nr_ice_candidate_pair_start(pctx,pair))
          ABORT(r);
        break;
      case NR_ICE_PAIR_STATE_IN_PROGRESS:
        if(r=nr_stun_client_force_retransmit(pair->stun_client))
          ABORT(r);
        break;
      default:
        break;
    }

    /* Activate the media stream if required */
    if(pair->remote->stream->ice_state==NR_ICE_MEDIA_STREAM_CHECKS_FROZEN){
      if(r=nr_ice_media_stream_start_checks(pair->pctx,pair->remote->stream))
        ABORT(r);
    }

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_candidate_pair_cancel(nr_ice_peer_ctx *pctx,nr_ice_cand_pair *pair)
  {
    if(pair->state != NR_ICE_PAIR_STATE_FAILED){
      /* If it's already running we need to terminate the stun */
      if(pair->state==NR_ICE_PAIR_STATE_IN_PROGRESS){
        nr_stun_client_cancel(pair->stun_client);
      }
      nr_ice_candidate_pair_set_state(pctx,pair,NR_ICE_PAIR_STATE_CANCELLED);
    }

    return(0);
  }

int nr_ice_candidate_pair_select(nr_ice_cand_pair *pair)
  {
    int r,_status;

    if(!pair){
      r_log(LOG_ICE,LOG_ERR,"ICE-PAIR: No pair chosen");
      ABORT(R_BAD_ARGS);
    }

    if(pair->state!=NR_ICE_PAIR_STATE_SUCCEEDED){
      r_log(LOG_ICE,LOG_ERR,"ICE-PEER(%s)/CAND-PAIR(%s): tried to install non-succeeded pair, ignoring: %s",pair->pctx->label,pair->codeword,pair->as_string);
    }
    else{
      /* Ok, they chose one */
      /* 1. Send a new request with nominated. Do it as a scheduled
            event to avoid reentrancy issues. Only do this if it hasn't
            happened already (though this shouldn't happen.)
      */
      if(!pair->restart_nominated_cb_timer)
        NR_ASYNC_TIMER_SET(0,nr_ice_candidate_pair_restart_stun_nominated_cb,pair,&pair->restart_nominated_cb_timer);

      /* 2. Tell ourselves this pair is ready */
      if(r=nr_ice_component_nominated_pair(pair->remote->component, pair))
        ABORT(r);
    }

    _status=0;
  abort:
    return(_status);
 }

int nr_ice_candidate_pair_set_state(nr_ice_peer_ctx *pctx, nr_ice_cand_pair *pair, int state)
  {
    int r,_status;

    r_log(LOG_ICE,LOG_INFO,"ICE-PEER(%s)/CAND-PAIR(%s): setting pair to state %s: %s",
      pctx->label,pair->codeword,nr_ice_cand_pair_states[state],pair->as_string);

    /* NOTE: This function used to reference pctx->state instead of
       pair->state and the assignment to pair->state was at the top
       of this function. Because pctx->state was never changed, this seems to have
       been a typo. The natural logic is "if the state changed
       decrement the counter" so this implies we should be checking
       the pair state rather than the pctx->state.

       This didn't cause big problems because waiting_pairs was only
       used for pacing, so the pacing just was kind of broken.

       This note is here as a reminder until we do more testing
       and make sure that in fact this was a typo.
    */
    if(pair->state!=NR_ICE_PAIR_STATE_WAITING){
      if(state==NR_ICE_PAIR_STATE_WAITING)
        pctx->waiting_pairs++;
    }
    else{
      if(state!=NR_ICE_PAIR_STATE_WAITING)
        pctx->waiting_pairs--;

      assert(pctx->waiting_pairs>=0);
    }
    pair->state=state;


    if(pair->state==NR_ICE_PAIR_STATE_FAILED){
      if(r=nr_ice_component_failed_pair(pair->remote->component, pair))
        ABORT(r);
    }

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_candidate_pair_dump_state(nr_ice_cand_pair *pair, FILE *out)
  {
    /*r_log(LOG_ICE,LOG_DEBUG,"CAND-PAIR(%s): pair %s: state=%s, priority=0x%llx\n",pair->codeword,pair->as_string,nr_ice_cand_pair_states[pair->state],pair->priority);*/

    return(0);
  }


int nr_ice_candidate_pair_insert(nr_ice_cand_pair_head *head,nr_ice_cand_pair *pair)
  {
    nr_ice_cand_pair *c1;

    c1=TAILQ_FIRST(head);
    while(c1){
      if(c1->priority < pair->priority){
        TAILQ_INSERT_BEFORE(c1,pair,entry);
        break;
      }

      c1=TAILQ_NEXT(c1,entry);
    }
    if(!c1) TAILQ_INSERT_TAIL(head,pair,entry);

    return(0);
  }

void nr_ice_candidate_pair_restart_stun_nominated_cb(NR_SOCKET s, int how, void *cb_arg)
  {
    nr_ice_cand_pair *pair=cb_arg;
    int r,_status;

    pair->restart_nominated_cb_timer=0;

    r_log(LOG_ICE,LOG_INFO,"ICE-PEER(%s)/STREAM(%s)/CAND-PAIR(%s)/COMP(%d): Restarting pair as nominated: %s",pair->pctx->label,pair->local->stream->label,pair->codeword,pair->remote->component->component_id,pair->as_string);

    nr_stun_client_reset(pair->stun_client);

    if(r=nr_stun_client_start(pair->stun_client,NR_ICE_CLIENT_MODE_USE_CANDIDATE,nr_ice_candidate_pair_stun_cb,pair))
      ABORT(r);

    if(r=nr_ice_ctx_remember_id(pair->pctx->ctx, pair->stun_client->request))
      ABORT(r);

    _status=0;
  abort:
    return;
  }

static void nr_ice_candidate_pair_restart_stun_role_change_cb(NR_SOCKET s, int how, void *cb_arg)
 {
    nr_ice_cand_pair *pair=cb_arg;

    pair->restart_role_change_cb_timer=0;

    r_log(LOG_ICE,LOG_INFO,"ICE-PEER(%s)/STREAM(%s)/CAND-PAIR(%s):COMP(%d): Restarting pair as %s: %s",pair->pctx->label,pair->local->stream->label,pair->codeword,pair->remote->component->component_id,pair->pctx->controlling ? "CONTROLLING" : "CONTROLLED",pair->as_string);

    nr_ice_candidate_pair_restart(pair->pctx, pair);
  }

void nr_ice_candidate_pair_role_change(nr_ice_cand_pair *pair)
  {
    pair->stun_client->params.ice_binding_request.control = pair->pctx->controlling ? NR_ICE_CONTROLLING : NR_ICE_CONTROLLED;

    if(pair->state == NR_ICE_PAIR_STATE_IN_PROGRESS) {
      /* We could try only restarting in-progress pairs when they receive their
       * 487, but this ends up being simpler, because any extra 487 are dropped.
       */
      if(!pair->restart_role_change_cb_timer)
        NR_ASYNC_TIMER_SET(0,nr_ice_candidate_pair_restart_stun_role_change_cb,pair,&pair->restart_role_change_cb_timer);
    }
  }

static void nr_ice_candidate_pair_compute_codeword(nr_ice_cand_pair *pair,
  nr_ice_candidate *lcand, nr_ice_candidate *rcand)
  {
    char as_string[2048];

    snprintf(as_string,
             sizeof(as_string),
             "%s|%s(%s|%s)",
             lcand->addr.as_string,
             rcand->addr.as_string,
             lcand->label,
             rcand->label);

    nr_ice_compute_codeword(as_string,strlen(as_string),pair->codeword);
  }

