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



static char *RCSSTRING __UNUSED__="$Id: stun_client_ctx.c,v 1.2 2008/04/28 18:21:30 ekr Exp $";

#include <assert.h>
#include <string.h>

#include <nr_api.h>
#include "stun.h"
#include "async_timer.h"
#include "registry.h"
#include "stun_reg.h"
#include "r_time.h"

static int nr_stun_client_send_request(nr_stun_client_ctx *ctx);
static void nr_stun_client_timer_expired_cb(NR_SOCKET s, int b, void *cb_arg);
static int nr_stun_client_get_password(void *arg, nr_stun_message *msg, Data **password);

int nr_stun_client_ctx_create(char *label, nr_socket *sock, nr_transport_addr *peer, UINT4 RTO, nr_stun_client_ctx **ctxp)
  {
    nr_stun_client_ctx *ctx=0;
    int r,_status;

    if ((r=nr_stun_startup()))
      ABORT(r);

    if(!(ctx=RCALLOC(sizeof(nr_stun_client_ctx))))
      ABORT(R_NO_MEMORY);

    ctx->state=NR_STUN_CLIENT_STATE_INITTED;

    if(!(ctx->label=r_strdup(label)))
      ABORT(R_NO_MEMORY);

    ctx->sock=sock;

    nr_socket_getaddr(sock,&ctx->my_addr);
    nr_transport_addr_copy(&ctx->peer_addr,peer);

    if (NR_reg_get_uint4(NR_STUN_REG_PREF_CLNT_RETRANSMIT_TIMEOUT, &ctx->rto_ms)) {
        if (RTO != 0)
            ctx->rto_ms = RTO;
        else
            ctx->rto_ms = 100;
    }

    if (NR_reg_get_double(NR_STUN_REG_PREF_CLNT_RETRANSMIT_BACKOFF, &ctx->retransmission_backoff_factor))
        ctx->retransmission_backoff_factor = 2.0;

    if (NR_reg_get_uint4(NR_STUN_REG_PREF_CLNT_MAXIMUM_TRANSMITS, &ctx->maximum_transmits))
        ctx->maximum_transmits = 7;

    if (NR_reg_get_uint4(NR_STUN_REG_PREF_CLNT_FINAL_RETRANSMIT_BACKOFF, &ctx->final_retransmit_backoff_ms))
        ctx->final_retransmit_backoff_ms = 16 * ctx->rto_ms;

    *ctxp=ctx;

    _status=0;
  abort:
    if(_status){
      nr_stun_client_ctx_destroy(&ctx);
    }
    return(_status);
  }

int nr_stun_client_start(nr_stun_client_ctx *ctx, int mode, NR_async_cb finished_cb, void *cb_arg)
  {
    int r,_status;

    if (ctx->state != NR_STUN_CLIENT_STATE_INITTED)
        ABORT(R_NOT_PERMITTED);
    
    ctx->mode=mode;

    ctx->state=NR_STUN_CLIENT_STATE_RUNNING;
    ctx->finished_cb=finished_cb;
    ctx->cb_arg=cb_arg;

    if(mode!=NR_STUN_CLIENT_MODE_KEEPALIVE){
      if(r=nr_stun_client_send_request(ctx))
        ABORT(r);
    }

    _status=0;
  abort:
   if (ctx->state != NR_STUN_CLIENT_STATE_RUNNING) {
        ctx->finished_cb = 0;  /* prevent 2nd call */
        /* finished_cb call must be absolutely last thing in function
         * because as a side effect this ctx may be operated on in the
         * callback */
        finished_cb(0,0,cb_arg);
    }

    return(_status);
  }

int nr_stun_client_restart(nr_stun_client_ctx *ctx)
  {
    int r,_status;
    int mode;
    NR_async_cb finished_cb;
    void *cb_arg;
    nr_stun_message_attribute *ec;
    nr_stun_message_attribute *as;

    if (ctx->state != NR_STUN_CLIENT_STATE_RUNNING)
        ABORT(R_NOT_PERMITTED);
 
    assert(ctx->retry_ct <= 2);
    if (ctx->retry_ct > 2)
        ABORT(R_NOT_PERMITTED);

    ++ctx->retry_ct;

    mode = ctx->mode;
    finished_cb = ctx->finished_cb;
    cb_arg = ctx->cb_arg;

    if (nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_ERROR_CODE, &ec)
     && ec->u.error_code.number == 300) {
        if (nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_ALTERNATE_SERVER, &as)) {
            nr_transport_addr_copy(&ctx->peer_addr, &as->u.alternate_server);
        }
    }

    nr_stun_client_reset(ctx);

    if (r=nr_stun_client_start(ctx, mode, finished_cb, cb_arg))
      ABORT(r);

    _status=0;
  abort:
    return(_status);
  }

int
nr_stun_client_reset(nr_stun_client_ctx *ctx)
{
    /* Cancel the timer firing */
    if (ctx->timer_handle){
        NR_async_timer_cancel(ctx->timer_handle);
        ctx->timer_handle=0;
    }

    nr_stun_message_destroy(&ctx->request);
    ctx->request   = 0;

    nr_stun_message_destroy(&ctx->response);
    ctx->response  = 0;

    memset(&ctx->results, 0, sizeof(ctx->results));

    ctx->mode          = 0;
    ctx->finished_cb   = 0;
    ctx->cb_arg        = 0;
    ctx->request_ct    = 0;
    ctx->timeout_ms    = 0;

    ctx->state = NR_STUN_CLIENT_STATE_INITTED;

    return 0;
}

static void nr_stun_client_timer_expired_cb(NR_SOCKET s, int b, void *cb_arg)
  {
    int _status;
    nr_stun_client_ctx *ctx=cb_arg;
    struct timeval now;
    INT8 ms_waited;

    /* Prevent this timer from being cancelled later */
    ctx->timer_handle=0;

    /* Shouldn't happen */
    if(ctx->state==NR_STUN_CLIENT_STATE_CANCELLED)
      ABORT(R_REJECTED);

    gettimeofday(&now, 0);
    if (r_timeval_diff_ms(&now, &ctx->timer_set, &ms_waited)) {
        r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): Timer expired",ctx->label);
    }
    else {
        r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): Timer expired (after %llu ms)",ctx->label, ms_waited);
    }

    if (ctx->request_ct >= ctx->maximum_transmits) {
        r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): Timed out",ctx->label);
        ctx->state=NR_STUN_CLIENT_STATE_TIMED_OUT;
        ABORT(R_FAILED);
    }

    if (ctx->state != NR_STUN_CLIENT_STATE_RUNNING)
        ABORT(R_NOT_PERMITTED);

    /* as a side effect will reset the timer */
    nr_stun_client_send_request(ctx);

    _status = 0;
  abort:
    if (ctx->state != NR_STUN_CLIENT_STATE_RUNNING) {
        /* Cancel the timer firing */
        if (ctx->timer_handle){
            NR_async_timer_cancel(ctx->timer_handle);
            ctx->timer_handle=0;
        }

        if (ctx->finished_cb) {
            NR_async_cb finished_cb = ctx->finished_cb; 
            ctx->finished_cb = 0;  /* prevent 2nd call */
            /* finished_cb call must be absolutely last thing in function
             * because as a side effect this ctx may be operated on in the
             * callback */
            finished_cb(0,0,ctx->cb_arg);
        }
    }
    return;
  }

int nr_stun_client_force_retransmit(nr_stun_client_ctx *ctx)
  {
    int r,_status;

    if (ctx->state != NR_STUN_CLIENT_STATE_RUNNING)
        ABORT(R_NOT_PERMITTED);

    if (ctx->request_ct > ctx->maximum_transmits) {
        r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): Too many retransmit attempts",ctx->label);
        ABORT(R_FAILED);
    }

    /* if there is a scheduled retransimt, get rid of the scheduled retransmit
     * and retransmit immediately */
    if (ctx->timer_handle) {
        NR_async_timer_cancel(ctx->timer_handle);
        ctx->timer_handle=0;

        if (r=nr_stun_client_send_request(ctx))
            ABORT(r);
    }

    _status=0;
  abort:

    return(_status);
  }

static int nr_stun_client_send_request(nr_stun_client_ctx *ctx)
  {
    int r,_status;
    char string[256];

    if (ctx->state != NR_STUN_CLIENT_STATE_RUNNING)
        ABORT(R_NOT_PERMITTED);

    r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): Sending(my_addr=%s,peer_addr=%s)",ctx->label,ctx->my_addr.as_string,ctx->peer_addr.as_string);

    if (ctx->request == 0) {
        switch (ctx->mode) {
        case NR_STUN_CLIENT_MODE_BINDING_REQUEST_LONG_TERM_AUTH:
            ctx->params.stun_binding_request.nonce = ctx->nonce;
            ctx->params.stun_binding_request.realm = ctx->realm;
            if ((r=nr_stun_build_req_lt_auth(&ctx->params.stun_binding_request, &ctx->request)))
                ABORT(r);
            break;
        case NR_STUN_CLIENT_MODE_BINDING_REQUEST_SHORT_TERM_AUTH:
            if ((r=nr_stun_build_req_st_auth(&ctx->params.stun_binding_request, &ctx->request)))
                ABORT(r);
            break;
        case NR_STUN_CLIENT_MODE_BINDING_REQUEST_NO_AUTH:
            if ((r=nr_stun_build_req_no_auth(&ctx->params.stun_binding_request, &ctx->request)))
                ABORT(r);
            break;
        case NR_STUN_CLIENT_MODE_KEEPALIVE:
            if ((r=nr_stun_build_keepalive(&ctx->params.stun_keepalive, &ctx->request)))
                ABORT(r);
            break;
#ifdef USE_STUND_0_96
        case NR_STUN_CLIENT_MODE_BINDING_REQUEST_STUND_0_96:
            if ((r=nr_stun_build_req_stund_0_96(&ctx->params.stun_binding_request_stund_0_96, &ctx->request)))
                ABORT(r);
            break;
#endif /* USE_STUND_0_96 */

#ifdef USE_ICE
        case NR_ICE_CLIENT_MODE_USE_CANDIDATE:
            if ((r=nr_stun_build_use_candidate(&ctx->params.ice_use_candidate, &ctx->request)))
                ABORT(r);
            break;
        case NR_ICE_CLIENT_MODE_BINDING_REQUEST:
            if ((r=nr_stun_build_req_ice(&ctx->params.ice_binding_request, &ctx->request)))
                ABORT(r);
            break;
#endif /* USE_ICE */

#ifdef USE_TURN
        case NR_TURN_CLIENT_MODE_ALLOCATE_REQUEST1:
            if ((r=nr_stun_build_allocate_request1(&ctx->params.allocate_request1, &ctx->request)))
                ABORT(r);
            break;
        case NR_TURN_CLIENT_MODE_ALLOCATE_REQUEST2:
            if ((r=nr_stun_build_allocate_request2(&ctx->params.allocate_request2, &ctx->request)))
                ABORT(r);
            break;
        case NR_TURN_CLIENT_MODE_SEND_INDICATION:
            if ((r=nr_stun_build_send_indication(&ctx->params.send_indication, &ctx->request)))
                ABORT(r);
            break;
        case NR_TURN_CLIENT_MODE_SET_ACTIVE_DEST_REQUEST:
            if ((r=nr_stun_build_set_active_dest_request(&ctx->params.set_active_dest_request, &ctx->request)))
                ABORT(r);
            break;
        case NR_TURN_CLIENT_MODE_DATA_INDICATION:
            if ((r=nr_stun_build_data_indication(&ctx->params.data_indication, &ctx->request)))
                ABORT(r);
            break;
#endif /* USE_TURN */

        default:
            assert(0);
            ABORT(R_FAILED);
            break;
        }
    }

    if (ctx->request->length == 0) {
        if ((r=nr_stun_encode_message(ctx->request)))
            ABORT(r);
    }

    snprintf(string, sizeof(string)-1, "STUN-CLIENT(%s): Sending to %s ", ctx->label, ctx->peer_addr.as_string);
    r_dump(NR_LOG_STUN, LOG_DEBUG, string, (char*)ctx->request->buffer, ctx->request->length);

    if(r=nr_socket_sendto(ctx->sock, ctx->request->buffer, ctx->request->length, 0, &ctx->peer_addr))
      ABORT(r);
    
    ctx->request_ct++;

    if (NR_STUN_GET_TYPE_CLASS(ctx->request->header.type) == NR_CLASS_INDICATION) {
        /* no need to set the timer because indications don't receive a
         * response */
    }
    else {
        if (ctx->request_ct < ctx->maximum_transmits) {
            ctx->timeout_ms *= ctx->retransmission_backoff_factor;
            ctx->timeout_ms += ctx->rto_ms;
        }
        else {
            ctx->timeout_ms += ctx->final_retransmit_backoff_ms;
        }

        r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): Next timer will fire in %u ms",ctx->label, ctx->timeout_ms);

        gettimeofday(&ctx->timer_set, 0);

        NR_ASYNC_TIMER_SET(ctx->timeout_ms, nr_stun_client_timer_expired_cb, ctx, &ctx->timer_handle);
    }
   
    _status=0;
  abort:
    if (_status) {
      ctx->state=NR_STUN_CLIENT_STATE_FAILED;
    }
    return(_status);
  }

static int nr_stun_client_get_password(void *arg, nr_stun_message *msg, Data **password)
{
    *password = (Data*)arg;
    if (!arg)
        return(R_NOT_FOUND);
    return(0);
}

int nr_stun_client_process_response(nr_stun_client_ctx *ctx, UCHAR *msg, int len, nr_transport_addr *peer_addr)
  {
    int r,_status;
    char string[256];
    Data *password = 0;
    nr_stun_message_attribute *attr;
    nr_transport_addr *mapped_addr = 0;

    if(ctx->state==NR_STUN_CLIENT_STATE_CANCELLED)
      ABORT(R_REJECTED);

    if (ctx->state != NR_STUN_CLIENT_STATE_RUNNING)
      ABORT(R_REJECTED);

    r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): Received(my_addr=%s,peer_addr=%s)",ctx->label,ctx->my_addr.as_string,peer_addr->as_string);

    snprintf(string, sizeof(string)-1, "STUN-CLIENT(%s): Received ", ctx->label);
    r_dump(NR_LOG_STUN, LOG_DEBUG, string, (char*)msg, len);

    /* determine password */
    switch (ctx->mode) {
    case NR_STUN_CLIENT_MODE_BINDING_REQUEST_LONG_TERM_AUTH:
    case NR_STUN_CLIENT_MODE_BINDING_REQUEST_SHORT_TERM_AUTH:
        password = ctx->params.stun_binding_request.password;
        break;

    case NR_STUN_CLIENT_MODE_BINDING_REQUEST_NO_AUTH:
        /* do nothing */
        break;

    case NR_STUN_CLIENT_MODE_BINDING_REQUEST_STUND_0_96:
        /* do nothing */
        break;

#ifdef USE_ICE
    case NR_ICE_CLIENT_MODE_BINDING_REQUEST:
        password = &ctx->params.ice_binding_request.password;
        break;
    case NR_ICE_CLIENT_MODE_USE_CANDIDATE:
        password = &ctx->params.ice_binding_request.password;
        break;
#endif /* USE_ICE */

#ifdef USE_TURN
    case NR_TURN_CLIENT_MODE_ALLOCATE_REQUEST1:
        /* do nothing */
        break;
    case NR_TURN_CLIENT_MODE_ALLOCATE_REQUEST2:
        /* do nothing */
        break;
    case NR_TURN_CLIENT_MODE_SET_ACTIVE_DEST_REQUEST:
        /* do nothing */
        break;
    case NR_TURN_CLIENT_MODE_SEND_INDICATION:
        /* do nothing -- we just got our DATA-INDICATION */
        break;
#endif /* USE_TURN */

    default:
        assert(0);
        ABORT(R_FAILED);
        break;
    }

#ifdef USE_TURN
    if (ctx->mode == NR_TURN_CLIENT_MODE_SEND_INDICATION) {
        /* SEND-INDICATION gets a DATA-INDICATION back, which will always
         * be a different transaction, so don't perform the check in that
         * case */
    }
#endif /* USE_TURN */

    if ((r=nr_stun_message_create2(&ctx->response, msg, len)))
        ABORT(r);

    if ((r=nr_stun_decode_message(ctx->response, nr_stun_client_get_password, password))) {
        r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): error decoding message",ctx->label);
        ABORT(r);
    }

    if ((r=nr_stun_receive_message(ctx->request, ctx->response))) {
        r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): error receiving message",ctx->label);
        ABORT(r);
    }

    r_log(NR_LOG_STUN,LOG_DEBUG,
          "STUN-CLIENT(%s): successfully received message; processing",ctx->label);

/* TODO: !nn! currently using password!=0 to mean that auth is required,
 * TODO: !nn! but we should probably pass that in explicitly via the
 * TODO: !nn! usage (ctx->mode?) */
    if (password) {
        if (nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_NONCE, 0)) {
            if ((r=nr_stun_receive_response_long_term_auth(ctx->response, ctx)))
                ABORT(r);
        }
        else {
            if ((r=nr_stun_receive_response_short_term_auth(ctx->response)))
                ABORT(r);
        }
    }

    if (NR_STUN_GET_TYPE_CLASS(ctx->response->header.type) == NR_CLASS_RESPONSE) {
        if ((r=nr_stun_process_success_response(ctx->response)))
            ABORT(r);
    }
    else {
        if ((r=nr_stun_process_error_response(ctx->response))) {
            ABORT(r);
        }
        else {
            /* drop the error on the floor */
            ABORT(R_FAILED);
        }
    }

    r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): Successfully parsed mode=%d",ctx->label,ctx->mode);

/* TODO: !nn! this should be moved to individual message receive/processing sections */
    switch (ctx->mode) {
    case NR_STUN_CLIENT_MODE_BINDING_REQUEST_LONG_TERM_AUTH:
    case NR_STUN_CLIENT_MODE_BINDING_REQUEST_SHORT_TERM_AUTH:
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_XOR_MAPPED_ADDRESS, 0))
            ABORT(R_BAD_DATA);
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_MESSAGE_INTEGRITY, 0))
            ABORT(R_BAD_DATA);

        mapped_addr = &ctx->results.stun_binding_response.mapped_addr;
        break;

    case NR_STUN_CLIENT_MODE_BINDING_REQUEST_NO_AUTH:
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_XOR_MAPPED_ADDRESS, 0))
            ABORT(R_BAD_DATA);

        mapped_addr = &ctx->results.stun_binding_response.mapped_addr;
        break;

    case NR_STUN_CLIENT_MODE_BINDING_REQUEST_STUND_0_96:
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_MAPPED_ADDRESS, 0) && ! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_XOR_MAPPED_ADDRESS, 0))
            ABORT(R_BAD_DATA);

        mapped_addr = &ctx->results.stun_binding_response_stund_0_96.mapped_addr;
        break;

#ifdef USE_ICE
    case NR_ICE_CLIENT_MODE_BINDING_REQUEST:
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_XOR_MAPPED_ADDRESS, 0))
            ABORT(R_BAD_DATA);
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_MESSAGE_INTEGRITY, 0))
            ABORT(R_BAD_DATA);

        mapped_addr = &ctx->results.stun_binding_response.mapped_addr;
        break;
    case NR_ICE_CLIENT_MODE_USE_CANDIDATE:
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_XOR_MAPPED_ADDRESS, 0))
            ABORT(R_BAD_DATA);
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_MESSAGE_INTEGRITY, 0))
            ABORT(R_BAD_DATA);

        mapped_addr = &ctx->results.stun_binding_response.mapped_addr;
        break;
#endif /* USE_ICE */

#ifdef USE_TURN
    case NR_TURN_CLIENT_MODE_ALLOCATE_REQUEST1:
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_REALM, &attr))
            ABORT(R_BAD_DATA);

        ctx->results.allocate_response1.realm = attr->u.realm;

        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_NONCE, &attr))
            ABORT(R_BAD_DATA);

        ctx->results.allocate_response1.nonce = attr->u.nonce;
        break;
    case NR_TURN_CLIENT_MODE_ALLOCATE_REQUEST2:
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_USERNAME, 0))
            ABORT(R_BAD_DATA);
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_XOR_MAPPED_ADDRESS, 0))
            ABORT(R_BAD_DATA);
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_MESSAGE_INTEGRITY, 0))
            ABORT(R_BAD_DATA);

        if (nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_RELAY_ADDRESS, &attr)) {
            if ((r=nr_transport_addr_copy(
                             &ctx->results.allocate_response2.relay_addr,
                             &attr->u.relay_address)))
                ABORT(r);
        }
        else {
            if ((r=nr_transport_addr_copy(&ctx->results.allocate_response2.relay_addr, peer_addr)))
                ABORT(r);
        }

        mapped_addr = &ctx->results.allocate_response2.mapped_addr;

        break;
    case NR_TURN_CLIENT_MODE_SET_ACTIVE_DEST_REQUEST:
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_MESSAGE_INTEGRITY, 0))
            ABORT(R_BAD_DATA);
        break;
    case NR_TURN_CLIENT_MODE_SEND_INDICATION:
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_REMOTE_ADDRESS, 0))
            ABORT(R_BAD_DATA);
        break;
#endif /* USE_TURN */

    default:
        assert(0);
        ABORT(R_FAILED);
        break;
    }

    /* make sure we have the most up-to-date address from this peer */
    if (nr_transport_addr_cmp(&ctx->peer_addr, peer_addr, NR_TRANSPORT_ADDR_CMP_MODE_ALL)) {
        r_log(NR_LOG_STUN,LOG_INFO,"STUN-CLIENT(%s): Peer moved from %s to %s", ctx->label, ctx->peer_addr.as_string, peer_addr->as_string);
        nr_transport_addr_copy(&ctx->peer_addr, peer_addr);
    }

    if (mapped_addr) {
        if (nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_XOR_MAPPED_ADDRESS, &attr)) {
            if ((r=nr_transport_addr_copy(mapped_addr, &attr->u.xor_mapped_address.unmasked)))
                ABORT(r);
        }
        else if (nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_MAPPED_ADDRESS, &attr)) {
            if ((r=nr_transport_addr_copy(mapped_addr, &attr->u.mapped_address)))
                ABORT(r);
        }
        else
            ABORT(R_BAD_DATA);
 
        r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): Received mapped address: %s", ctx->label, mapped_addr->as_string);
    }

    ctx->state=NR_STUN_CLIENT_STATE_DONE;

    _status=0;
  abort:
    if (ctx->state != NR_STUN_CLIENT_STATE_RUNNING) {
        /* Cancel the timer firing */
        if (ctx->timer_handle) {
            NR_async_timer_cancel(ctx->timer_handle);
            ctx->timer_handle = 0;
        }

        /* Fire the callback */
        if (ctx->finished_cb) {
            NR_async_cb finished_cb = ctx->finished_cb; 
            ctx->finished_cb = 0;  /* prevent 2nd call */
            /* finished_cb call must be absolutely last thing in function
             * because as a side effect this ctx may be operated on in the
             * callback */
            finished_cb(0,0,ctx->cb_arg);
        }
    }

    return(_status);
  }

int nr_stun_client_ctx_destroy(nr_stun_client_ctx **ctxp)
  {
    nr_stun_client_ctx *ctx;

    if(!ctxp || !*ctxp)
      return(0);
    
    ctx=*ctxp;

    nr_stun_client_reset(ctx);

    RFREE(ctx->nonce);
    RFREE(ctx->realm);

    RFREE(ctx->label);
    RFREE(ctx);
    
    return(0);
  }


int nr_stun_client_cancel(nr_stun_client_ctx *ctx)
  {
    /* Cancel the timer firing */
    if (ctx->timer_handle){
      NR_async_timer_cancel(ctx->timer_handle);
      ctx->timer_handle=0;
    }

    /* Mark cancelled so we ignore any returned messsages */
    ctx->state=NR_STUN_CLIENT_STATE_CANCELLED;
    
    return(0);
  }
