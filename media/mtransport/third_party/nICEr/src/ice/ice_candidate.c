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



static char *RCSSTRING __UNUSED__="$Id: ice_candidate.c,v 1.2 2008/04/28 17:59:01 ekr Exp $";

#include <csi_platform.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/types.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include "nr_api.h"
#include "registry.h"
#include "nr_socket.h"
#include "async_timer.h"

#include "stun_client_ctx.h"
#include "stun_server_ctx.h"
#include "turn_client_ctx.h"
#include "ice_ctx.h"
#include "ice_candidate.h"
#include "ice_reg.h"
#include "ice_util.h"
#include "nr_socket_turn.h"

static int next_automatic_preference = 224;

static int nr_ice_candidate_initialize2(nr_ice_candidate *cand);
static int nr_ice_get_foundation(nr_ice_ctx *ctx,nr_ice_candidate *cand);
static int nr_ice_srvrflx_start_stun(nr_ice_candidate *cand);
static void nr_ice_srvrflx_stun_finished_cb(NR_SOCKET sock, int how, void *cb_arg);
#ifdef USE_TURN
static int nr_ice_start_relay_turn(nr_ice_candidate *cand);
static void nr_ice_turn_allocated_cb(NR_SOCKET sock, int how, void *cb_arg);
static int nr_ice_candidate_resolved_cb(void *cb_arg, nr_transport_addr *addr);
#endif /* USE_TURN */

char *nr_ice_candidate_type_names[]={0,"host","srflx","prflx","relay",0};

static const char *nr_ctype_name(nr_ice_candidate_type ctype) {
  assert(ctype<CTYPE_MAX && ctype>0);
  if (ctype <= 0 || ctype >= CTYPE_MAX) {
    return "ERROR";
  }
  return nr_ice_candidate_type_names[ctype];
}

static int nr_ice_candidate_format_stun_label(char *label, size_t size, nr_ice_candidate *cand)
  {
    int _status;

    *label = 0;
    switch(cand->stun_server->type) {
      case NR_ICE_STUN_SERVER_TYPE_ADDR:
        snprintf(label, size, "%s(%s|%s)", nr_ctype_name(cand->type), cand->base.as_string,
                 cand->stun_server->u.addr.as_string);
        break;
      case NR_ICE_STUN_SERVER_TYPE_DNSNAME:
        snprintf(label, size, "%s(%s|%s:%u)", nr_ctype_name(cand->type), cand->base.as_string,
                 cand->stun_server->u.dnsname.host, cand->stun_server->u.dnsname.port);
        break;
      default:
        assert(0);
        ABORT(R_BAD_ARGS);
    }

    _status=0;
   abort:
    return(_status);
  }

int nr_ice_candidate_create(nr_ice_ctx *ctx,nr_ice_component *comp,nr_ice_socket *isock, nr_socket *osock, nr_ice_candidate_type ctype, nr_ice_stun_server *stun_server, UCHAR component_id, nr_ice_candidate **candp)
  {
    nr_ice_candidate *cand=0;
    nr_ice_candidate *tmp=0;
    int r,_status;
    char label[512];

    if(!(cand=RCALLOC(sizeof(nr_ice_candidate))))
      ABORT(R_NO_MEMORY);
    cand->state=NR_ICE_CAND_STATE_CREATED;
    cand->ctx=ctx;
    cand->isock=isock;
    cand->osock=osock;
    cand->type=ctype;
    cand->stun_server=stun_server;
    cand->component_id=component_id;
    cand->component=comp;
    cand->stream=comp->stream;

    /* Extract the addr as the base */
    if(r=nr_socket_getaddr(cand->isock->sock,&cand->base))
      ABORT(r);

    switch(ctype) {
      case HOST:
        snprintf(label, sizeof(label), "host(%s)", cand->base.as_string);
        break;

      case SERVER_REFLEXIVE:
        if(r=nr_ice_candidate_format_stun_label(label, sizeof(label),cand))
          ABORT(r);
        break;

      case RELAYED:
        if(r=nr_ice_candidate_format_stun_label(label, sizeof(label),cand))
          ABORT(r);
        break;

      case PEER_REFLEXIVE:
        snprintf(label, sizeof(label), "prflx");
        break;

      default:
        assert(0); /* Can't happen */
        ABORT(R_BAD_ARGS);
    }
    if(!(cand->label=r_strdup(label)))
      ABORT(R_NO_MEMORY);

    if(r=nr_ice_get_foundation(ctx,cand))
      ABORT(r);
    if(r=nr_ice_candidate_compute_priority(cand))
      ABORT(r);

    TAILQ_FOREACH(tmp,&isock->candidates,entry_sock){
      if(cand->priority==tmp->priority){
        r_log(LOG_ICE,LOG_WARNING,"ICE(%s): duplicate priority %u candidate %s and candidate %s",
          ctx->label,cand->priority,cand->label,tmp->label);
      }
    }

    if(ctype==RELAYED)
      cand->u.relayed.turn_sock=osock;


    /* Add the candidate to the isock list*/
    TAILQ_INSERT_TAIL(&isock->candidates,cand,entry_sock);

    r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): created candidate %s with type %s",
      ctx->label,cand->label,nr_ctype_name(ctype));

    *candp=cand;

    _status=0;
  abort:
    if (_status){
      r_log(LOG_ICE,LOG_ERR,"ICE(%s): Failed to create candidate of type %s", ctx->label,nr_ctype_name(ctype));
      nr_ice_candidate_destroy(&cand);
    }
    return(_status);
  }


/* Create a peer reflexive candidate */
int nr_ice_peer_peer_rflx_candidate_create(nr_ice_ctx *ctx,char *label, nr_ice_component *comp,nr_transport_addr *addr, nr_ice_candidate **candp)
  {
    nr_ice_candidate *cand=0;
    nr_ice_candidate_type ctype=PEER_REFLEXIVE;
    int r,_status;

    if(!(cand=RCALLOC(sizeof(nr_ice_candidate))))
      ABORT(R_NO_MEMORY);
    if(!(cand->label=r_strdup(label)))
      ABORT(R_NO_MEMORY);

    cand->state=NR_ICE_CAND_STATE_INITIALIZED;
    cand->ctx=ctx;
    cand->type=ctype;
    cand->component_id=comp->component_id;
    cand->component=comp;
    cand->stream=comp->stream;


    r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): creating candidate %s with type %d",
      ctx->label,label,ctype);

    if(r=nr_transport_addr_copy(&cand->base,addr))
      ABORT(r);
    if(r=nr_transport_addr_copy(&cand->addr,addr))
      ABORT(r);
    /* Bogus foundation */
    if(!(cand->foundation=r_strdup(cand->addr.as_string)))
      ABORT(r);

    *candp=cand;

    _status=0;
  abort:
    if (_status){
      nr_ice_candidate_destroy(&cand);
    }
    return(_status);
  }

int nr_ice_candidate_destroy(nr_ice_candidate **candp)
  {
    nr_ice_candidate *cand=0;

    if(!candp || !*candp)
      return(0);

    cand=*candp;

    switch(cand->type){
      case HOST:
        break;
#ifdef USE_TURN
      case RELAYED:
        if (cand->u.relayed.turn_handle)
          nr_ice_socket_deregister(cand->isock, cand->u.relayed.turn_handle);
        nr_turn_client_ctx_destroy(&cand->u.relayed.turn);
        nr_socket_destroy(&cand->u.relayed.turn_sock);
        break;
#endif /* USE_TURN */
      case SERVER_REFLEXIVE:
        if (cand->u.srvrflx.stun_handle)
          nr_ice_socket_deregister(cand->isock, cand->u.srvrflx.stun_handle);
        nr_stun_client_ctx_destroy(&cand->u.srvrflx.stun);
        break;
      default:
        break;
    }

    NR_async_timer_cancel(cand->delay_timer);
    NR_async_timer_cancel(cand->ready_cb_timer);
    if(cand->resolver_handle){
      nr_resolver_cancel(cand->ctx->resolver,cand->resolver_handle);
    }

    RFREE(cand->foundation);
    RFREE(cand->label);
    RFREE(cand);

    return(0);
  }

void nr_ice_candidate_destroy_cb(NR_SOCKET s, int h, void *cb_arg)
  {
    nr_ice_candidate *cand=cb_arg;
    nr_ice_candidate_destroy(&cand);
  }

/* This algorithm is not super-fast, but I don't think we need a hash
   table just yet and it produces a small foundation string */
static int nr_ice_get_foundation(nr_ice_ctx *ctx,nr_ice_candidate *cand)
  {
    nr_ice_foundation *foundation;
    int i=0;
    char fnd[20];
    int _status;

    foundation=STAILQ_FIRST(&ctx->foundations);
    while(foundation){
      if(nr_transport_addr_cmp(&cand->base,&foundation->addr,NR_TRANSPORT_ADDR_CMP_MODE_ADDR))
        goto next;
      if(cand->type != foundation->type)
        goto next;
      if(cand->stun_server != foundation->stun_server)
        goto next;

      snprintf(fnd,sizeof(fnd),"%d",i);
      if(!(cand->foundation=r_strdup(fnd)))
        ABORT(R_NO_MEMORY);
      return(0);

    next:
      foundation=STAILQ_NEXT(foundation,entry);
      i++;
    }

    if(!(foundation=RCALLOC(sizeof(nr_ice_foundation))))
      ABORT(R_NO_MEMORY);
    nr_transport_addr_copy(&foundation->addr,&cand->base);
    foundation->type=cand->type;
    foundation->stun_server=cand->stun_server;
    STAILQ_INSERT_TAIL(&ctx->foundations,foundation,entry);

    snprintf(fnd,sizeof(fnd),"%d",i);
    if(!(cand->foundation=r_strdup(fnd)))
      ABORT(R_NO_MEMORY);

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_candidate_compute_priority(nr_ice_candidate *cand)
  {
    UCHAR type_preference;
    UCHAR interface_preference;
    UCHAR stun_priority;
    int r,_status;

    switch(cand->type){
      case HOST:
        if(r=NR_reg_get_uchar(NR_ICE_REG_PREF_TYPE_HOST,&type_preference))
          ABORT(r);
        stun_priority=0;
        break;
      case RELAYED:
        if(r=NR_reg_get_uchar(NR_ICE_REG_PREF_TYPE_RELAYED,&type_preference))
          ABORT(r);
        stun_priority=255-cand->stun_server->index;
        break;
      case SERVER_REFLEXIVE:
        if(r=NR_reg_get_uchar(NR_ICE_REG_PREF_TYPE_SRV_RFLX,&type_preference))
          ABORT(r);
        stun_priority=255-cand->stun_server->index;
        break;
      case PEER_REFLEXIVE:
        if(r=NR_reg_get_uchar(NR_ICE_REG_PREF_TYPE_PEER_RFLX,&type_preference))
          ABORT(r);
        stun_priority=0;
        break;
      default:
        ABORT(R_INTERNAL);
    }

    if(type_preference > 126)
      r_log(LOG_ICE,LOG_ERR,"Illegal type preference %d",type_preference);


    if(r=NR_reg_get2_uchar(NR_ICE_REG_PREF_INTERFACE_PRFX,cand->base.ifname,
      &interface_preference)) {
      if (r==R_NOT_FOUND) {
        if (next_automatic_preference == 1) {
          r_log(LOG_ICE,LOG_DEBUG,"Out of preference values. Can't assign one for interface %s",cand->base.ifname);
          ABORT(R_NOT_FOUND);
        }
        r_log(LOG_ICE,LOG_DEBUG,"Automatically assigning preference for interface %s->%d",cand->base.ifname,
          next_automatic_preference);
        if (r=NR_reg_set2_uchar(NR_ICE_REG_PREF_INTERFACE_PRFX,cand->base.ifname,next_automatic_preference)){
          ABORT(r);
        }
        interface_preference=next_automatic_preference;
        next_automatic_preference--;
      }
      else {
        ABORT(r);
      }
    }

    cand->priority=
      (type_preference << 24) |
      (interface_preference << 16) |
      (stun_priority << 8) |
      (256 - cand->component_id);

    /* S 4.1.2 */
    assert(cand->priority>=1&&cand->priority<=2147483647);

    _status=0;
  abort:
    return(_status);
  }

static void nr_ice_candidate_fire_ready_cb(NR_SOCKET s, int how, void *cb_arg)
  {
    nr_ice_candidate *cand = cb_arg;

    cand->ready_cb(0, 0, cand->ready_cb_arg);
    cand->ready_cb_timer = 0;
  }

int nr_ice_candidate_initialize(nr_ice_candidate *cand, NR_async_cb ready_cb, void *cb_arg)
  {
    int r,_status;
    int protocol=NR_RESOLVE_PROTOCOL_STUN;
    cand->done_cb=ready_cb;
    cand->cb_arg=cb_arg;

    switch(cand->type){
      case HOST:
        if(r=nr_socket_getaddr(cand->isock->sock,&cand->addr))
          ABORT(r);
        cand->osock=cand->isock->sock;
        cand->state=NR_ICE_CAND_STATE_INITIALIZED;
        // Post this so that it doesn't happen in-line
        cand->ready_cb = ready_cb;
        cand->ready_cb_arg = cb_arg;
        NR_ASYNC_TIMER_SET(0, nr_ice_candidate_fire_ready_cb, (void *)cand, &cand->ready_cb_timer);
        break;
#ifdef USE_TURN
      case RELAYED:
        protocol=NR_RESOLVE_PROTOCOL_TURN;
        /* Fall through */
#endif
      case SERVER_REFLEXIVE:
        cand->state=NR_ICE_CAND_STATE_INITIALIZING;

        if(cand->stun_server->type == NR_ICE_STUN_SERVER_TYPE_ADDR) {
          /* Just copy the address */
          if (r=nr_transport_addr_copy(&cand->stun_server_addr,
                                       &cand->stun_server->u.addr)) {
            r_log(LOG_ICE,LOG_ERR,"ICE-CANDIDATE(%s): Could not copy STUN server addr", cand->label);
            ABORT(r);
          }

          if(r=nr_ice_candidate_initialize2(cand))
            ABORT(r);
        }
        else {
          nr_resolver_resource resource;
          resource.domain_name=cand->stun_server->u.dnsname.host;
          resource.port=cand->stun_server->u.dnsname.port;
          resource.stun_turn=protocol;
          resource.transport_protocol=IPPROTO_UDP;  /* We don't support TCP yet */

          /* Try to resolve */
          if(!cand->ctx->resolver) {
            r_log(LOG_ICE, LOG_ERR, "Can't use DNS names without a resolver");
            ABORT(R_BAD_ARGS);
          }

          if(r=nr_resolver_resolve(cand->ctx->resolver,
                                   &resource,
                                   nr_ice_candidate_resolved_cb,
                                   (void *)cand,
                                   &cand->resolver_handle)){
            r_log(LOG_ICE,LOG_ERR,"ICE-CANDIDATE(%s): Could not resolve domain name",cand->label);
            ABORT(r);
          }
        }
        break;
      default:
        ABORT(R_INTERNAL);
    }

    _status=0;
  abort:
    if(_status && _status!=R_WOULDBLOCK)
      cand->state=NR_ICE_CAND_STATE_FAILED;
    return(_status);
  }


static int nr_ice_candidate_resolved_cb(void *cb_arg, nr_transport_addr *addr)
  {
    nr_ice_candidate *cand=cb_arg;
    int r,_status;

    cand->resolver_handle=0;

    if(addr){
      r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): resolved candidate %s. addr=%s",
            cand->ctx->label,cand->label,addr->as_string);
    }
    else {
      r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): failed to resolve candidate %s.",
            cand->ctx->label,cand->label);
      ABORT(R_NOT_FOUND);
    }

    /* Copy the address */
    if(r=nr_transport_addr_copy(&cand->stun_server_addr,addr))
      ABORT(r);

    /* Now start initializing */
    if(r=nr_ice_candidate_initialize2(cand))
      ABORT(r);

    _status=0;
  abort:
    if(_status && _status!=R_WOULDBLOCK) {
      cand->state=NR_ICE_CAND_STATE_FAILED;
      cand->done_cb(0,0,cand->cb_arg);
    }
    return(_status);
  }

static int nr_ice_candidate_initialize2(nr_ice_candidate *cand)
  {
    int r,_status;

    switch(cand->type){
      case HOST:
        assert(0); /* Can't happen */
        ABORT(R_INTERNAL);
        break;
#ifdef USE_TURN
      case RELAYED:
        if(r=nr_ice_start_relay_turn(cand))
          ABORT(r);
        ABORT(R_WOULDBLOCK);
        break;
#endif /* USE_TURN */
      case SERVER_REFLEXIVE:
        /* Need to start stun */
        if(r=nr_ice_srvrflx_start_stun(cand))
          ABORT(r);
        cand->osock=cand->isock->sock;
        ABORT(R_WOULDBLOCK);
        break;
      default:
        ABORT(R_INTERNAL);
    }

    _status=0;
  abort:
    if(_status && _status!=R_WOULDBLOCK)
      cand->state=NR_ICE_CAND_STATE_FAILED;
    return(_status);
  }

static void nr_ice_srvrflx_start_stun_timer_cb(NR_SOCKET s, int how, void *cb_arg)
  {
    nr_ice_candidate *cand=cb_arg;
    int r,_status;

    cand->delay_timer=0;

/* TODO: if the response is a BINDING-ERROR-RESPONSE, then restart
 * TODO: using NR_STUN_CLIENT_MODE_BINDING_REQUEST because the
 * TODO: server may not have understood the 0.96-style request */
    if(r=nr_stun_client_start(cand->u.srvrflx.stun, NR_STUN_CLIENT_MODE_BINDING_REQUEST_STUND_0_96, nr_ice_srvrflx_stun_finished_cb, cand))
      ABORT(r);

    if(r=nr_ice_ctx_remember_id(cand->ctx, cand->u.srvrflx.stun->request))
      ABORT(r);

    if(r=nr_ice_socket_register_stun_client(cand->isock,cand->u.srvrflx.stun,&cand->u.srvrflx.stun_handle))
      ABORT(r);

    _status=0;
  abort:
    if(_status){
      cand->state=NR_ICE_CAND_STATE_FAILED;
    }

    return;
  }

static int nr_ice_srvrflx_start_stun(nr_ice_candidate *cand)
  {
    int r,_status;

    assert(!cand->delay_timer);
    if(r=nr_stun_client_ctx_create(cand->label, cand->isock->sock,
      &cand->stun_server_addr, cand->stream->ctx->gather_rto,
      &cand->u.srvrflx.stun))
      ABORT(r);

    NR_ASYNC_TIMER_SET(cand->stream->ctx->stun_delay,nr_ice_srvrflx_start_stun_timer_cb,cand,&cand->delay_timer);
    cand->stream->ctx->stun_delay += cand->stream->ctx->Ta;

    _status=0;
  abort:
    if(_status){
      cand->state=NR_ICE_CAND_STATE_FAILED;
    }
    return(_status);
  }

#ifdef USE_TURN
static void nr_ice_start_relay_turn_timer_cb(NR_SOCKET s, int how, void *cb_arg)
  {
    nr_ice_candidate *cand=cb_arg;
    int r,_status;

    cand->delay_timer=0;

    if(r=nr_turn_client_allocate(cand->u.relayed.turn, nr_ice_turn_allocated_cb, cb_arg))
      ABORT(r);

    if(r=nr_ice_socket_register_turn_client(cand->isock, cand->u.relayed.turn,
                                            cand->osock, &cand->u.relayed.turn_handle))
      ABORT(r);

    _status=0;
  abort:
    if(_status){
      cand->state=NR_ICE_CAND_STATE_FAILED;
    }
    return;
  }

static int nr_ice_start_relay_turn(nr_ice_candidate *cand)
  {
    int r,_status;
    assert(!cand->delay_timer);
    if(r=nr_turn_client_ctx_create(cand->label, cand->isock->sock,
                                   cand->u.relayed.server->username,
                                   cand->u.relayed.server->password,
                                   &cand->stun_server_addr,
                                   &cand->u.relayed.turn))
      ABORT(r);

    if(r=nr_socket_turn_set_ctx(cand->osock, cand->u.relayed.turn))
      ABORT(r);

    NR_ASYNC_TIMER_SET(cand->stream->ctx->stun_delay,nr_ice_start_relay_turn_timer_cb,cand,&cand->delay_timer);
    cand->stream->ctx->stun_delay += cand->stream->ctx->Ta;

    _status=0;
  abort:
    if(_status){
      cand->state=NR_ICE_CAND_STATE_FAILED;
    }
    return(_status);
  }
#endif /* USE_TURN */

static void nr_ice_srvrflx_stun_finished_cb(NR_SOCKET sock, int how, void *cb_arg)
  {
    int _status;
    nr_ice_candidate *cand=cb_arg;

    r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): %s for %s",cand->ctx->label,__FUNCTION__,cand->label);

    /* Deregister to suppress duplicates */
    if(cand->u.srvrflx.stun_handle){ /* This test because we might have failed before CB registered */
      nr_ice_socket_deregister(cand->isock,cand->u.srvrflx.stun_handle);
      cand->u.srvrflx.stun_handle=0;
    }

    switch(cand->u.srvrflx.stun->state){
      /* OK, we should have a mapped address */
      case NR_STUN_CLIENT_STATE_DONE:
        /* Copy the address */
        nr_transport_addr_copy(&cand->addr, &cand->u.srvrflx.stun->results.stun_binding_response_stund_0_96.mapped_addr);
        nr_stun_client_ctx_destroy(&cand->u.srvrflx.stun);
        cand->state=NR_ICE_CAND_STATE_INITIALIZED;
        /* Execute the ready callback */
        cand->done_cb(0,0,cand->cb_arg);
        break;

      /* This failed, so go to the next STUN server if there is one */
      case NR_STUN_CLIENT_STATE_FAILED:
        ABORT(R_NOT_FOUND);
        break;
      default:
        ABORT(R_INTERNAL);
    }
    _status = 0;
  abort:
    if(_status){
      cand->state=NR_ICE_CAND_STATE_FAILED;
      cand->done_cb(0,0,cand->cb_arg);
    }
  }

#ifdef USE_TURN
static void nr_ice_turn_allocated_cb(NR_SOCKET s, int how, void *cb_arg)
  {
    int r,_status;
    nr_ice_candidate *cand=cb_arg;
    nr_turn_client_ctx *turn=cand->u.relayed.turn;
    char *label;
    nr_transport_addr relay_addr;

    switch(turn->state){
      /* OK, we should have a mapped address */
      case NR_TURN_CLIENT_STATE_ALLOCATED:
        if (r=nr_turn_client_get_relayed_address(turn, &relay_addr))
          ABORT(r);

        if(r=nr_concat_strings(&label,"turn-relay(",cand->base.as_string,"|",
                               relay_addr.as_string,")",NULL))
          ABORT(r);

        r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): Switching from TURN (%s) to RELAY (%s)",cand->u.relayed.turn->label,cand->label,label);

        /* Copy the relayed address into the candidate addr and
           into the candidate base. Note that we need to keep the
           ifname in the base. */
        if (r=nr_transport_addr_copy(&cand->addr, &relay_addr))
          ABORT(r);
        if (r=nr_transport_addr_copy_keep_ifname(&cand->base, &relay_addr))  /* Need to keep interface for priority calculation */
          ABORT(r);

        r_log(LOG_ICE,LOG_DEBUG,"ICE-CANDIDATE(%s): base=%s, candidate=%s", cand->label, cand->base.as_string, cand->addr.as_string);

        RFREE(cand->label);
        cand->label=label;
        cand->state=NR_ICE_CAND_STATE_INITIALIZED;

        /* Execute the ready callback */
        cand->done_cb(0,0,cand->cb_arg);

        /* We also need to activate the associated STUN candidate */
        if(cand->u.relayed.srvflx_candidate){
          nr_ice_candidate *cand2=cand->u.relayed.srvflx_candidate;

          if (r=nr_turn_client_get_mapped_address(cand->u.relayed.turn, &cand2->addr))
            ABORT(r);

          cand2->state=NR_ICE_CAND_STATE_INITIALIZED;
          cand2->done_cb(0,0,cand2->cb_arg);
        }

        break;

    case NR_TURN_CLIENT_STATE_FAILED:
    case NR_TURN_CLIENT_STATE_CANCELLED:
      r_log(NR_LOG_TURN, LOG_ERR,
            "ICE-CANDIDATE(%s): nr_turn_allocated_cb called with state %d",
            cand->label, turn->state);
      /* This failed, so go to the next TURN server if there is one */
      ABORT(R_NOT_FOUND);
      break;
    default:
      assert(0); /* should never happen */
      ABORT(R_INTERNAL);
    }

    _status=0;
  abort:
    if(_status){
      r_log(NR_LOG_TURN, LOG_ERR,
            "ICE-CANDIDATE(%s): nr_turn_allocated_cb failed", cand->label);
      cand->state=NR_ICE_CAND_STATE_FAILED;
      cand->done_cb(0,0,cand->cb_arg);

      if(cand->u.relayed.srvflx_candidate){
        nr_ice_candidate *cand2=cand->u.relayed.srvflx_candidate;

        cand2->state=NR_ICE_CAND_STATE_FAILED;
        cand2->done_cb(0,0,cand2->cb_arg);
      }
    }
  }
#endif /* USE_TURN */

/* Format the candidate attribute as per ICE S 15.1 */
int nr_ice_format_candidate_attribute(nr_ice_candidate *cand, char *attr, int maxlen)
  {
    int r,_status;
    char addr[64];
    int port;
    int len;

    assert(!strcmp(nr_ice_candidate_type_names[HOST], "host"));
    assert(!strcmp(nr_ice_candidate_type_names[RELAYED], "relay"));

    if(r=nr_transport_addr_get_addrstring(&cand->addr,addr,sizeof(addr)))
      ABORT(r);
    if(r=nr_transport_addr_get_port(&cand->addr,&port))
      ABORT(r);
    snprintf(attr,maxlen,"candidate:%s %d UDP %u %s %d typ %s",
      cand->foundation, cand->component_id, cand->priority, addr, port,
      nr_ctype_name(cand->type));

    len=strlen(attr); attr+=len; maxlen-=len;

    /* raddr, rport */
    switch(cand->type){
      case HOST:
        break;
      case SERVER_REFLEXIVE:
      case PEER_REFLEXIVE:
        if(r=nr_transport_addr_get_addrstring(&cand->base,addr,sizeof(addr)))
          ABORT(r);
        if(r=nr_transport_addr_get_port(&cand->base,&port))
          ABORT(r);

        snprintf(attr,maxlen," raddr %s rport %d",addr,port);
        break;
      case RELAYED:
        // comes from XorMappedAddress via AllocateResponse
        if(r=nr_transport_addr_get_addrstring(&cand->base,addr,sizeof(addr)))
          ABORT(r);
        if(r=nr_transport_addr_get_port(&cand->base,&port))
          ABORT(r);

        snprintf(attr,maxlen," raddr %s rport %d",addr,port);
        break;
      default:
        assert(0);
        ABORT(R_INTERNAL);
        break;
    }
    _status=0;
  abort:
    return(_status);
  }

