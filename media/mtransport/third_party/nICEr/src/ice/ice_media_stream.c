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



static char *RCSSTRING __UNUSED__="$Id: ice_media_stream.c,v 1.2 2008/04/28 17:59:01 ekr Exp $";

#include <string.h>
#include <assert.h>
#include <nr_api.h>
#include <r_assoc.h>
#include <async_timer.h>
#include "ice_util.h"
#include "ice_ctx.h"

static char *nr_ice_media_stream_states[]={"INVALID",
  "UNPAIRED","FROZEN","ACTIVE","COMPLETED","FAILED"
};

int nr_ice_media_stream_set_state(nr_ice_media_stream *str, int state);

int nr_ice_media_stream_create(nr_ice_ctx *ctx,char *label,int components, nr_ice_media_stream **streamp)
  {
    int r,_status;
    nr_ice_media_stream *stream=0;
    nr_ice_component *comp=0;
    int i;

    if(!(stream=RCALLOC(sizeof(nr_ice_media_stream))))
      ABORT(R_NO_MEMORY);

    if(!(stream->label=r_strdup(label)))
      ABORT(R_NO_MEMORY);

    stream->ctx=ctx;

    STAILQ_INIT(&stream->components);
    for(i=0;i<components;i++){
      /* component-id must be > 0, so increment by 1 */
      if(r=nr_ice_component_create(stream, i+1, &comp))
        ABORT(r);

    }

    TAILQ_INIT(&stream->check_list);

    stream->component_ct=components;
    stream->ice_state = NR_ICE_MEDIA_STREAM_UNPAIRED;
    *streamp=stream;

    _status=0;
  abort:
    if(_status){
      nr_ice_media_stream_destroy(&stream);
    }
    return(_status);
  }

int nr_ice_media_stream_destroy(nr_ice_media_stream **streamp)
  {
    nr_ice_media_stream *stream;
    nr_ice_component *c1,*c2;
    nr_ice_cand_pair *p1,*p2;
    if(!streamp || !*streamp)
      return(0);

    stream=*streamp;
    *streamp=0;

    STAILQ_FOREACH_SAFE(c1, &stream->components, entry, c2){
      STAILQ_REMOVE(&stream->components,c1,nr_ice_component_,entry);
      nr_ice_component_destroy(&c1);
    }

    TAILQ_FOREACH_SAFE(p1, &stream->check_list, entry, p2){
      TAILQ_REMOVE(&stream->check_list,p1,entry);
      nr_ice_candidate_pair_destroy(&p1);
    }

    RFREE(stream->label);

    RFREE(stream->ufrag);
    RFREE(stream->pwd);
    RFREE(stream->r2l_user);
    RFREE(stream->l2r_user);
    r_data_zfree(&stream->r2l_pass);
    r_data_zfree(&stream->l2r_pass);

    if(stream->timer)
      NR_async_timer_cancel(stream->timer);

    RFREE(stream);

    return(0);
  }

int nr_ice_media_stream_initialize(nr_ice_ctx *ctx, nr_ice_media_stream *stream)
  {
    int r,_status;
    nr_ice_component *comp;

    comp=STAILQ_FIRST(&stream->components);
    while(comp){
      if(r=nr_ice_component_initialize(ctx,comp))
        ABORT(r);
      comp=STAILQ_NEXT(comp,entry);
    }

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_media_stream_get_attributes(nr_ice_media_stream *stream, char ***attrsp, int *attrctp)
  {
    int attrct=0;
    nr_ice_component *comp;
    char **attrs=0;
    int index=0;
    nr_ice_candidate *cand;
    int r,_status;

    *attrctp=0;

    /* First find out how many attributes we need */
    comp=STAILQ_FIRST(&stream->components);
    while(comp){
      if (comp->state != NR_ICE_COMPONENT_DISABLED) {
        cand = TAILQ_FIRST(&comp->candidates);
        while(cand){
          if (cand->state == NR_ICE_CAND_STATE_INITIALIZED) {
            ++attrct;
          }

          cand = TAILQ_NEXT(cand, entry_comp);
        }
      }
      comp=STAILQ_NEXT(comp,entry);
    }

    if(attrct < 1){
      r_log(LOG_ICE,LOG_ERR,"ICE-STREAM(%s): Failed to find any components for stream",stream->label);
      ABORT(R_FAILED);
    }

    /* Make the array we'll need */
    if(!(attrs=RCALLOC(sizeof(char *)*attrct)))
      ABORT(R_NO_MEMORY);
    for(index=0;index<attrct;index++){
      if(!(attrs[index]=RMALLOC(NR_ICE_MAX_ATTRIBUTE_SIZE)))
        ABORT(R_NO_MEMORY);
    }

    index=0;
    /* Now format the attributes */
    comp=STAILQ_FIRST(&stream->components);
    while(comp){
      if (comp->state != NR_ICE_COMPONENT_DISABLED) {
        nr_ice_candidate *cand;

        cand=TAILQ_FIRST(&comp->candidates);
        while(cand){
          if (cand->state == NR_ICE_CAND_STATE_INITIALIZED) {
            assert(index < attrct);

            if (index >= attrct)
              ABORT(R_INTERNAL);

            if(r=nr_ice_format_candidate_attribute(cand, attrs[index],NR_ICE_MAX_ATTRIBUTE_SIZE))
              ABORT(r);

            index++;
          }

          cand=TAILQ_NEXT(cand,entry_comp);
        }
      }
      comp=STAILQ_NEXT(comp,entry);
    }

    *attrsp=attrs;
    *attrctp=attrct;

    _status=0;
  abort:
    if(_status){
      if(attrs){
        for(index=0;index<attrct;index++){
          RFREE(attrs[index]);
        }
        RFREE(attrs);
      }
    }
    return(_status);
  }


/* Get a default candidate per 4.1.4 */
int nr_ice_media_stream_get_default_candidate(nr_ice_media_stream *stream, int component, nr_ice_candidate **candp)
  {
    int _status;
    nr_ice_component *comp;
    nr_ice_candidate *cand;
    nr_ice_candidate *best_cand = NULL;

    comp=STAILQ_FIRST(&stream->components);
    while(comp){
      if (comp->component_id == component)
        break;

      comp=STAILQ_NEXT(comp,entry);
    }

    if (!comp)
      ABORT(R_NOT_FOUND);

    /* We have the component. Now find the "best" candidate, making
       use of the fact that more "reliable" candidate types have
       higher numbers. So, we sort by type and then priority within
       type
    */
    cand=TAILQ_FIRST(&comp->candidates);
    while(cand){
      if (cand->state == NR_ICE_CAND_STATE_INITIALIZED) {
        if (!best_cand) {
          best_cand = cand;
        }
        else {
          if (best_cand->type < cand->type) {
            best_cand = cand;
          } else if (best_cand->type == cand->type) {
            if (best_cand->priority < cand->priority)
              best_cand = cand;
          }
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


int nr_ice_media_stream_pair_candidates(nr_ice_peer_ctx *pctx,nr_ice_media_stream *lstream,nr_ice_media_stream *pstream)
  {
    int r,_status;
    nr_ice_component *pcomp,*lcomp;

    pcomp=STAILQ_FIRST(&pstream->components);
    lcomp=STAILQ_FIRST(&lstream->components);
    while(pcomp){
      if ((lcomp->state != NR_ICE_COMPONENT_DISABLED) &&
          (pcomp->state != NR_ICE_COMPONENT_DISABLED)) {
        if(r=nr_ice_component_pair_candidates(pctx,lcomp,pcomp))
          ABORT(r);
      }

      lcomp=STAILQ_NEXT(lcomp,entry);
      pcomp=STAILQ_NEXT(pcomp,entry);
    };

    if (pstream->ice_state == NR_ICE_MEDIA_STREAM_UNPAIRED) {
      nr_ice_media_stream_set_state(pstream, NR_ICE_MEDIA_STREAM_CHECKS_FROZEN);
    }

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_media_stream_service_pre_answer_requests(nr_ice_peer_ctx *pctx, nr_ice_media_stream *lstream, nr_ice_media_stream *pstream, int *serviced)
  {
    nr_ice_component *pcomp;
    int r,_status;
    char *user = 0;

    if (serviced)
      *serviced = 0;

    pcomp=STAILQ_FIRST(&pstream->components);
    while(pcomp){
      int serviced_inner=0;

      /* Flush all the pre-answer requests */
      if(r=nr_ice_component_service_pre_answer_requests(pctx, pcomp, pstream->r2l_user, &serviced_inner))
        ABORT(r);
      if (serviced)
        *serviced += serviced_inner;

      pcomp=STAILQ_NEXT(pcomp,entry);
    }

    _status=0;
   abort:
    RFREE(user);
    return(_status);
  }

/* S 5.8 -- run the highest priority WAITING pair or if not available
   FROZEN pair */
static void nr_ice_media_stream_check_timer_cb(NR_SOCKET s, int h, void *cb_arg)
  {
    int r,_status;
    nr_ice_media_stream *stream=cb_arg;
    nr_ice_cand_pair *pair;
    int timer_val;

    assert(stream->pctx->active_streams!=0);

    timer_val=stream->pctx->ctx->Ta*stream->pctx->active_streams;

    if (stream->ice_state == NR_ICE_MEDIA_STREAM_CHECKS_COMPLETED) {
      r_log(LOG_ICE,LOG_ERR,"ICE-PEER(%s): (bug) bogus state for stream %s",stream->pctx->label,stream->label);
    }
    assert(stream->ice_state != NR_ICE_MEDIA_STREAM_CHECKS_COMPLETED);

    r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s): check timer expired for media stream %s",stream->pctx->label,stream->label);
    stream->timer=0;

    /* Find the highest priority WAITING check and move it to RUNNING */
    pair=TAILQ_FIRST(&stream->check_list);
    while(pair){
      if(pair->state==NR_ICE_PAIR_STATE_WAITING)
        break;
      pair=TAILQ_NEXT(pair,entry);
    }

    /* Hmmm... No WAITING. Let's look for FROZEN */
    if(!pair){
      pair=TAILQ_FIRST(&stream->check_list);

      while(pair){
        if(pair->state==NR_ICE_PAIR_STATE_FROZEN){
          if(r=nr_ice_candidate_pair_unfreeze(stream->pctx,pair))
            ABORT(r);
          break;
        }
        pair=TAILQ_NEXT(pair,entry);
      }
    }

    if(pair){
      nr_ice_candidate_pair_start(pair->pctx,pair); /* Ignore failures */
      NR_ASYNC_TIMER_SET(timer_val,nr_ice_media_stream_check_timer_cb,cb_arg,&stream->timer);
    }
    else {
      r_log(LOG_ICE,LOG_WARNING,"ICE-PEER(%s): no pairs for %s",stream->pctx->label,stream->label);
    }

    _status=0;
  abort:
    return;
  }


/* Start checks for this media stream (aka check list) */
int nr_ice_media_stream_start_checks(nr_ice_peer_ctx *pctx, nr_ice_media_stream *stream)
  {
    int r,_status;

    /* Don't start the check timer if the stream is done (failed/completed) */
    if (stream->ice_state > NR_ICE_MEDIA_STREAM_CHECKS_ACTIVE) {
      assert(0);
      ABORT(R_INTERNAL);
    }

    if(r=nr_ice_media_stream_set_state(stream,NR_ICE_MEDIA_STREAM_CHECKS_ACTIVE))
      ABORT(r);

    if (!stream->timer) {
      r_log(LOG_ICE,LOG_INFO,"ICE-PEER(%s)/ICE-STREAM(%s): Starting check timer for stream.",pctx->label,stream->label);
      nr_ice_media_stream_check_timer_cb(0,0,stream);
    }

    _status=0;
  abort:
    return(_status);
  }

/* Start checks for this media stream (aka check list) S 5.7 */
int nr_ice_media_stream_unfreeze_pairs(nr_ice_peer_ctx *pctx, nr_ice_media_stream *stream)
  {
    int r,_status;
    r_assoc *assoc=0;
    nr_ice_cand_pair *pair=0;

    /* Already seen assoc */
    if(r=r_assoc_create(&assoc,r_assoc_crc32_hash_compute,5))
      ABORT(r);

    /* S 5.7.4. Set the highest priority pairs in each foundation to WAITING */
    pair=TAILQ_FIRST(&stream->check_list);
    while(pair){
      void *v;

      if(r=r_assoc_fetch(assoc,pair->foundation,strlen(pair->foundation),&v)){
        if(r!=R_NOT_FOUND)
          ABORT(r);
        if(r=nr_ice_candidate_pair_unfreeze(pctx,pair))
          ABORT(r);

        if(r=r_assoc_insert(assoc,pair->foundation,strlen(pair->foundation),
          0,0,0,R_ASSOC_NEW))
          ABORT(r);
      }

      /* Already exists... fall through */
      pair=TAILQ_NEXT(pair,entry);
    }

    _status=0;
  abort:
    r_assoc_destroy(&assoc);
    return(_status);
  }

static int nr_ice_media_stream_unfreeze_pairs_match(nr_ice_media_stream *stream, char *foundation)
  {
    nr_ice_cand_pair *pair;
    int r,_status;
    int unfroze=0;

    pair=TAILQ_FIRST(&stream->check_list);
    while(pair){
      if(pair->state==NR_ICE_PAIR_STATE_FROZEN &&
        !strcmp(foundation,pair->foundation)){
        if(r=nr_ice_candidate_pair_unfreeze(stream->pctx,pair))
          ABORT(r);
        unfroze++;
      }
      pair=TAILQ_NEXT(pair,entry);
    }

    if(!unfroze)
      return(R_NOT_FOUND);

    _status=0;
  abort:
    return(_status);
  }

/* S 7.1.2.2 */
int nr_ice_media_stream_unfreeze_pairs_foundation(nr_ice_media_stream *stream, char *foundation)
  {
    int r,_status;
    nr_ice_media_stream *str;
       nr_ice_component *comp;
    int invalid_comps=0;

    /* 1. Unfreeze all frozen pairs with the same foundation
       in this stream */
    if(r=nr_ice_media_stream_unfreeze_pairs_match(stream,foundation)){
      if(r!=R_NOT_FOUND)
        ABORT(r);
    }

    /* 2. See if there is a pair in the valid list for every component */
    comp=STAILQ_FIRST(&stream->components);
    while(comp){
      if(!comp->valid_pairs)
        invalid_comps++;

      comp=STAILQ_NEXT(comp,entry);
    }

    /* If there is a pair in the valid list for every component... */
    /* Now go through the check lists for the other streams */
    str=STAILQ_FIRST(&stream->pctx->peer_streams);
    while(str){
      if(str!=stream){
        switch(str->ice_state){
          case NR_ICE_MEDIA_STREAM_CHECKS_ACTIVE:
            /* Unfreeze matching pairs */
            if(r=nr_ice_media_stream_unfreeze_pairs_match(str,foundation)){
              if(r!=R_NOT_FOUND)
                ABORT(r);
            }
            break;
          case NR_ICE_MEDIA_STREAM_CHECKS_FROZEN:
            /* Unfreeze matching pairs if any */
            r=nr_ice_media_stream_unfreeze_pairs_match(str,foundation);
            if(r){
              if(r!=R_NOT_FOUND)
                ABORT(r);

              /* OK, no matching pairs: execute the algorithm from 5.7
                 for this stream */
              if(r=nr_ice_media_stream_unfreeze_pairs(str->pctx,str))
                ABORT(r);
            }
            if(r=nr_ice_media_stream_start_checks(str->pctx,str))
              ABORT(r);

            break;
          default:
            break;
        }
      }

      str=STAILQ_NEXT(str,entry);
    }

/*    nr_ice_media_stream_dump_state(stream->pctx,stream,stderr); */


    _status=0;
  abort:
    return(_status);
  }


int nr_ice_media_stream_dump_state(nr_ice_peer_ctx *pctx, nr_ice_media_stream *stream,FILE *out)
  {
    nr_ice_cand_pair *pair;

    /* r_log(LOG_ICE,LOG_DEBUG,"MEDIA-STREAM(%s): state dump", stream->label); */
    pair=TAILQ_FIRST(&stream->check_list);
    while(pair){
      nr_ice_candidate_pair_dump_state(pair,out);

      pair=TAILQ_NEXT(pair,entry);
    }

    return(0);
  }

int nr_ice_media_stream_set_state(nr_ice_media_stream *str, int state)
  {
    /* Make no-change a no-op */
    if (state == str->ice_state)
      return 0;

    assert(state < sizeof(nr_ice_media_stream_states)/sizeof(char *));
    assert(str->ice_state < sizeof(nr_ice_media_stream_states)/sizeof(char *));

    r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s): stream %s state %s->%s",
      str->pctx->label,str->label,
      nr_ice_media_stream_states[str->ice_state],
      nr_ice_media_stream_states[state]);

    if(state == NR_ICE_MEDIA_STREAM_CHECKS_ACTIVE)
      str->pctx->active_streams++;
    if(str->ice_state == NR_ICE_MEDIA_STREAM_CHECKS_ACTIVE)
      str->pctx->active_streams--;

    r_log(LOG_ICE,LOG_DEBUG,"ICE-PEER(%s): %d active streams",
      str->pctx->label, str->pctx->active_streams);

    str->ice_state=state;

    return(0);
  }

/* S OK, this component has a nominated. If every component has a nominated,
   the stream is ready */
int nr_ice_media_stream_component_nominated(nr_ice_media_stream *stream,nr_ice_component *component)
  {
    int r,_status;
    nr_ice_component *comp;

    comp=STAILQ_FIRST(&stream->components);
    while(comp){
      if((comp->state != NR_ICE_COMPONENT_DISABLED) &&
         (comp->local_component->state != NR_ICE_COMPONENT_DISABLED) &&
         !comp->nominated)
        break;

      comp=STAILQ_NEXT(comp,entry);
    }

    /* At least one un-nominated component */
    if(comp)
      goto done;

    /* All done... */
    r_log(LOG_ICE,LOG_INFO,"ICE-PEER(%s)/ICE-STREAM(%s): all active components have nominated candidate pairs",stream->pctx->label,stream->label);
    nr_ice_media_stream_set_state(stream,NR_ICE_MEDIA_STREAM_CHECKS_COMPLETED);

    /* Cancel our timer */
    if(stream->timer){
      NR_async_timer_cancel(stream->timer);
      stream->timer=0;
    }

    if (stream->pctx->handler) {
      stream->pctx->handler->vtbl->stream_ready(stream->pctx->handler->obj,stream->local_stream);
    }

    /* Now tell the peer_ctx that we're done */
    if(r=nr_ice_peer_ctx_stream_done(stream->pctx,stream))
      ABORT(r);

  done:
    _status=0;
  abort:
    return(_status);
  }

int nr_ice_media_stream_component_failed(nr_ice_media_stream *stream,nr_ice_component *component)
  {
    int r,_status;
    nr_ice_cand_pair *p2;

    component->state=NR_ICE_COMPONENT_FAILED;

    /* at least one component failed in this media stream, so the entire
     * media stream is marked failed */

    nr_ice_media_stream_set_state(stream,NR_ICE_MEDIA_STREAM_CHECKS_FAILED);

    /* OK, we need to cancel off everything on this component */
    p2=TAILQ_FIRST(&stream->check_list);
    while(p2){
      if(r=nr_ice_candidate_pair_cancel(p2->pctx,p2))
        ABORT(r);

      p2=TAILQ_NEXT(p2,entry);
    }

    /* Cancel our timer */
    if(stream->timer){
      NR_async_timer_cancel(stream->timer);
      stream->timer=0;
    }

    if (stream->pctx->handler) {
      stream->pctx->handler->vtbl->stream_failed(stream->pctx->handler->obj,stream->local_stream);
    }

    /* Now tell the peer_ctx that we're done */
    if(r=nr_ice_peer_ctx_stream_done(stream->pctx,stream))
      ABORT(r);

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_media_stream_get_best_candidate(nr_ice_media_stream *str, int component, nr_ice_candidate **candp)
  {
    nr_ice_candidate *cand;
    nr_ice_candidate *best_cand=0;
    nr_ice_component *comp;
    int r,_status;

    if(r=nr_ice_media_stream_find_component(str,component,&comp))
      ABORT(r);

    cand=TAILQ_FIRST(&comp->candidates);
    while(cand){
      if(cand->state==NR_ICE_CAND_STATE_INITIALIZED){
        if(!best_cand || (cand->priority>best_cand->priority))
          best_cand=cand;

      }
      cand=TAILQ_NEXT(cand,entry_comp);
    }

    if(!best_cand)
      ABORT(R_NOT_FOUND);

    *candp=best_cand;

    _status=0;
  abort:
    return(_status);
  }


/* OK, we have the stream the user created, but that reflects the base
   ICE ctx, not the peer_ctx. So, find the related stream in the pctx,
   and then find the component */
int nr_ice_media_stream_find_component(nr_ice_media_stream *str, int comp_id, nr_ice_component **compp)
  {
    int _status;
    nr_ice_component *comp;

    comp=STAILQ_FIRST(&str->components);
    while(comp){
      if(comp->component_id==comp_id)
        break;

      comp=STAILQ_NEXT(comp,entry);
    }
    if(!comp)
      ABORT(R_NOT_FOUND);

    *compp=comp;

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_media_stream_send(nr_ice_peer_ctx *pctx, nr_ice_media_stream *str, int component, UCHAR *data, int len)
  {
    int r,_status;
    nr_ice_component *comp;

    /* First find the peer component */
    if(r=nr_ice_peer_ctx_find_component(pctx, str, component, &comp))
      ABORT(r);

    /* Do we have an active pair yet? We should... */
    if(!comp->active)
      ABORT(R_NOT_FOUND);

    /* OK, write to that pair, which means:
       1. Use the socket on our local side.
       2. Use the address on the remote side
    */
    comp->keepalive_needed=0; /* Suppress keepalives */
    if(r=nr_socket_sendto(comp->active->local->osock,data,len,0,
                          &comp->active->remote->addr))
      ABORT(r);

    _status=0;
  abort:
    return(_status);
  }

/* Returns R_REJECTED if the component is unpaired or has been disabled. */
int nr_ice_media_stream_get_active(nr_ice_peer_ctx *pctx, nr_ice_media_stream *str, int component, nr_ice_candidate **local, nr_ice_candidate **remote)
  {
    int r,_status;
    nr_ice_component *comp;

    /* First find the peer component */
    if(r=nr_ice_peer_ctx_find_component(pctx, str, component, &comp))
      ABORT(r);

    if (comp->state == NR_ICE_COMPONENT_UNPAIRED ||
        comp->state == NR_ICE_COMPONENT_DISABLED)
      ABORT(R_REJECTED);

    if(!comp->active)
      ABORT(R_NOT_FOUND);

    if (local) *local = comp->active->local;
    if (remote) *remote = comp->active->remote;

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_media_stream_addrs(nr_ice_peer_ctx *pctx, nr_ice_media_stream *str, int component, nr_transport_addr *local, nr_transport_addr *remote)
  {
    int r,_status;
    nr_ice_component *comp;

    /* First find the peer component */
    if(r=nr_ice_peer_ctx_find_component(pctx, str, component, &comp))
      ABORT(r);

    /* Do we have an active pair yet? We should... */
    if(!comp->active)
      ABORT(R_BAD_ARGS);

    /* Use the socket on our local side */
    if(r=nr_socket_getaddr(comp->active->local->osock,local))
      ABORT(r);

    /* Use the address on the remote side */
    if(r=nr_transport_addr_copy(remote,&comp->active->remote->addr))
      ABORT(r);

    _status=0;
  abort:
    return(_status);
  }



int nr_ice_media_stream_finalize(nr_ice_media_stream *lstr,nr_ice_media_stream *rstr)
  {
    nr_ice_component *lcomp,*rcomp;

    r_log(LOG_ICE,LOG_DEBUG,"Finalizing media stream %s, peer=%s",lstr->label,
      rstr?rstr->label:"NONE");

    lcomp=STAILQ_FIRST(&lstr->components);
    if(rstr)
      rcomp=STAILQ_FIRST(&rstr->components);
    else
      rcomp=0;

    while(lcomp){
      nr_ice_component_finalize(lcomp,rcomp);

      lcomp=STAILQ_NEXT(lcomp,entry);
      if(rcomp){
        rcomp=STAILQ_NEXT(rcomp,entry);
      }
    }

    return(0);
  }

int nr_ice_media_stream_pair_new_trickle_candidate(nr_ice_peer_ctx *pctx, nr_ice_media_stream *pstream, nr_ice_candidate *cand)
  {
    int r,_status;
    nr_ice_component *comp;

    if ((r=nr_ice_media_stream_find_component(pstream, cand->component_id, &comp)))
      ABORT(R_NOT_FOUND);

    if (r=nr_ice_component_pair_candidate(pctx, comp, cand, 1))
      ABORT(r);

    _status=0;
 abort:
    return(_status);
  }

int nr_ice_media_stream_disable_component(nr_ice_media_stream *stream, int component_id)
  {
    int r,_status;
    nr_ice_component *comp;

    if (stream->ice_state != NR_ICE_MEDIA_STREAM_UNPAIRED)
      ABORT(R_FAILED);

    if ((r=nr_ice_media_stream_find_component(stream, component_id, &comp)))
      ABORT(r);

    /* Can only disable before pairing */
    if (comp->state != NR_ICE_COMPONENT_UNPAIRED &&
        comp->state != NR_ICE_COMPONENT_DISABLED)
      ABORT(R_FAILED);

    comp->state = NR_ICE_COMPONENT_DISABLED;

    _status=0;
 abort:
    return(_status);
  }

void nr_ice_media_stream_role_change(nr_ice_media_stream *stream)
  {
    nr_ice_cand_pair *pair;
    assert(stream->ice_state != NR_ICE_MEDIA_STREAM_UNPAIRED);

    pair=TAILQ_FIRST(&stream->check_list);
    while(pair){
      nr_ice_candidate_pair_role_change(pair);
      pair=TAILQ_NEXT(pair,entry);
    }
  }

