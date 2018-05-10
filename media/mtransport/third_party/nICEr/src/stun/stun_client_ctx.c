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
#include <math.h>

#include <nr_api.h>
#include "stun.h"
#include "async_timer.h"
#include "registry.h"
#include "stun_reg.h"
#include "nr_crypto.h"
#include "r_time.h"

static int nr_stun_client_send_request(nr_stun_client_ctx *ctx);
static void nr_stun_client_timer_expired_cb(NR_SOCKET s, int b, void *cb_arg);
static int nr_stun_client_get_password(void *arg, nr_stun_message *msg, Data **password);

#define NR_STUN_TRANSPORT_ADDR_CHECK_WILDCARD 1
#define NR_STUN_TRANSPORT_ADDR_CHECK_LOOPBACK 2

int nr_stun_client_ctx_create(char *label, nr_socket *sock, nr_transport_addr *peer, UINT4 RTO, nr_stun_client_ctx **ctxp)
  {
    nr_stun_client_ctx *ctx=0;
    char allow_loopback;
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

    if (RTO != 0) {
      ctx->rto_ms = RTO;
    } else if (NR_reg_get_uint4(NR_STUN_REG_PREF_CLNT_RETRANSMIT_TIMEOUT, &ctx->rto_ms)) {
      ctx->rto_ms = 100;
    }

    if (NR_reg_get_double(NR_STUN_REG_PREF_CLNT_RETRANSMIT_BACKOFF, &ctx->retransmission_backoff_factor))
      ctx->retransmission_backoff_factor = 2.0;

    if (NR_reg_get_uint4(NR_STUN_REG_PREF_CLNT_MAXIMUM_TRANSMITS, &ctx->maximum_transmits))
      ctx->maximum_transmits = 7;

    if (NR_reg_get_uint4(NR_STUN_REG_PREF_CLNT_FINAL_RETRANSMIT_BACKOFF, &ctx->maximum_transmits_timeout_ms))
      ctx->maximum_transmits_timeout_ms = 16 * ctx->rto_ms;

    ctx->mapped_addr_check_mask = NR_STUN_TRANSPORT_ADDR_CHECK_WILDCARD;
    if (NR_reg_get_char(NR_STUN_REG_PREF_ALLOW_LOOPBACK_ADDRS, &allow_loopback) ||
        !allow_loopback) {
      ctx->mapped_addr_check_mask |= NR_STUN_TRANSPORT_ADDR_CHECK_LOOPBACK;
    }

    if (ctx->my_addr.protocol == IPPROTO_TCP) {
      /* Because TCP is reliable there is only one final timeout value.
       * We store the timeout value for TCP in here, because timeout_ms gets
       * reset to 0 in client_reset() which gets called from client_start() */
      ctx->maximum_transmits_timeout_ms = ctx->rto_ms *
                        pow(ctx->retransmission_backoff_factor,
                            ctx->maximum_transmits);
      ctx->maximum_transmits = 1;
    }

    *ctxp=ctx;

    _status=0;
  abort:
    if(_status){
      nr_stun_client_ctx_destroy(&ctx);
    }
    return(_status);
  }

static void nr_stun_client_fire_finished_cb(nr_stun_client_ctx *ctx)
  {
    if (ctx->finished_cb) {
      NR_async_cb finished_cb = ctx->finished_cb;
      ctx->finished_cb = 0;  /* prevent 2nd call */
      /* finished_cb call must be absolutely last thing in function
       * because as a side effect this ctx may be operated on in the
       * callback */
      finished_cb(0,0,ctx->cb_arg);
    }
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
     nr_stun_client_fire_finished_cb(ctx);
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
        r_log(NR_LOG_STUN,LOG_INFO,"STUN-CLIENT(%s): Timed out",ctx->label);
        ctx->state=NR_STUN_CLIENT_STATE_TIMED_OUT;
        ABORT(R_FAILED);
    }

    if (ctx->state != NR_STUN_CLIENT_STATE_RUNNING)
        ABORT(R_NOT_PERMITTED);

    // track retransmits for ice telemetry
    nr_accumulate_count(&(ctx->retransmit_ct), 1);

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

        nr_stun_client_fire_finished_cb(ctx);
    }
    if (_status) {
      // cb doesn't return anything, but we should probably log that we aborted
      // This also quiets the unused variable warnings.
      r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): Timer expired cb abort with status: %d",
        ctx->label, _status);
    }
    return;
  }

int nr_stun_client_force_retransmit(nr_stun_client_ctx *ctx)
  {
    int r,_status;

    if (ctx->state != NR_STUN_CLIENT_STATE_RUNNING)
        ABORT(R_NOT_PERMITTED);

    if (ctx->request_ct > ctx->maximum_transmits) {
        r_log(NR_LOG_STUN,LOG_INFO,"STUN-CLIENT(%s): Too many retransmit attempts",ctx->label);
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

    r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): Sending check request (my_addr=%s,peer_addr=%s)",ctx->label,ctx->my_addr.as_string,ctx->peer_addr.as_string);

    if (ctx->request == 0) {
        switch (ctx->mode) {
        case NR_STUN_CLIENT_MODE_BINDING_REQUEST_LONG_TERM_AUTH:
            ctx->params.stun_binding_request.nonce = ctx->nonce;
            ctx->params.stun_binding_request.realm = ctx->realm;
            assert(0);
            ABORT(R_INTERNAL);
            /* TODO(ekr@rtfm.com): Need to implement long-term auth for binding
               requests */
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
            if ((r=nr_stun_build_use_candidate(&ctx->params.ice_binding_request, &ctx->request)))
                ABORT(r);
            break;
        case NR_ICE_CLIENT_MODE_BINDING_REQUEST:
            if ((r=nr_stun_build_req_ice(&ctx->params.ice_binding_request, &ctx->request)))
                ABORT(r);
            break;
#endif /* USE_ICE */

#ifdef USE_TURN
        case NR_TURN_CLIENT_MODE_ALLOCATE_REQUEST:
            if ((r=nr_stun_build_allocate_request(&ctx->auth_params, &ctx->params.allocate_request, &ctx->request)))
                ABORT(r);
            break;
        case NR_TURN_CLIENT_MODE_REFRESH_REQUEST:
            if ((r=nr_stun_build_refresh_request(&ctx->auth_params, &ctx->params.refresh_request, &ctx->request)))
                ABORT(r);
            break;
        case NR_TURN_CLIENT_MODE_PERMISSION_REQUEST:
            if ((r=nr_stun_build_permission_request(&ctx->auth_params, &ctx->params.permission_request, &ctx->request)))
                ABORT(r);
            break;
        case NR_TURN_CLIENT_MODE_SEND_INDICATION:
            if ((r=nr_stun_build_send_indication(&ctx->params.send_indication, &ctx->request)))
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

    assert(ctx->my_addr.protocol==ctx->peer_addr.protocol);

    if(r=nr_socket_sendto(ctx->sock, ctx->request->buffer, ctx->request->length, 0, &ctx->peer_addr)) {
      if (r != R_WOULDBLOCK) {
        ABORT(r);
      }
      r_log(NR_LOG_STUN,LOG_WARNING,"STUN-CLIENT(%s): nr_socket_sendto blocked, treating as dropped packet",ctx->label);
    }

    ctx->request_ct++;

    if (NR_STUN_GET_TYPE_CLASS(ctx->request->header.type) == NR_CLASS_INDICATION) {
        /* no need to set the timer because indications don't receive a
         * response */
    }
    else {
        if (ctx->request_ct >= ctx->maximum_transmits) {
          /* Reliable transport only get here once. Unreliable get here for
           * their final timeout. */
          ctx->timeout_ms += ctx->maximum_transmits_timeout_ms;
        }
        else if (ctx->timeout_ms) {
          /* exponential backoff */
          ctx->timeout_ms *= ctx->retransmission_backoff_factor;
        }
        else {
          /* initial timeout unreliable transports */
          ctx->timeout_ms = ctx->rto_ms;
        }

        r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): Next timer will fire in %u ms",ctx->label, ctx->timeout_ms);

        gettimeofday(&ctx->timer_set, 0);

        assert(ctx->timeout_ms);
        NR_ASYNC_TIMER_SET(ctx->timeout_ms, nr_stun_client_timer_expired_cb, ctx, &ctx->timer_handle);
    }

    _status=0;
  abort:
    if (_status) {
      nr_stun_client_failed(ctx);
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

int nr_stun_transport_addr_check(nr_transport_addr* addr, UINT4 check)
  {
    if((check & NR_STUN_TRANSPORT_ADDR_CHECK_WILDCARD) && nr_transport_addr_is_wildcard(addr))
      return(R_BAD_DATA);

    if ((check & NR_STUN_TRANSPORT_ADDR_CHECK_LOOPBACK) && nr_transport_addr_is_loopback(addr))
      return(R_BAD_DATA);

    return(0);
  }

int nr_stun_client_process_response(nr_stun_client_ctx *ctx, UCHAR *msg, int len, nr_transport_addr *peer_addr)
  {
    int r,_status;
    char string[256];
    char *username = 0;
    Data *password = 0;
    nr_stun_message_attribute *attr;
    nr_transport_addr *mapped_addr = 0;
    int fail_on_error = 0;
    UCHAR hmac_key_d[16];
    Data hmac_key;
    int compute_lt_key=0;
    /* TODO(bcampen@mozilla.com): Bug 1023619, refactor this. */
    int response_matched=0;

    ATTACH_DATA(hmac_key, hmac_key_d);

    if ((ctx->state != NR_STUN_CLIENT_STATE_RUNNING) &&
        (ctx->state != NR_STUN_CLIENT_STATE_WAITING))
      ABORT(R_REJECTED);

    r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): Inspecting STUN response (my_addr=%s, peer_addr=%s)",ctx->label,ctx->my_addr.as_string,peer_addr->as_string);

    snprintf(string, sizeof(string)-1, "STUN-CLIENT(%s): Received ", ctx->label);
    r_dump(NR_LOG_STUN, LOG_DEBUG, string, (char*)msg, len);

    /* determine password */
    switch (ctx->mode) {
    case NR_STUN_CLIENT_MODE_BINDING_REQUEST_LONG_TERM_AUTH:
      compute_lt_key = 1;
      /* Fall through */
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
    case NR_TURN_CLIENT_MODE_ALLOCATE_REQUEST:
      fail_on_error = 1;
      compute_lt_key = 1;
        username = ctx->auth_params.username;
        password = &ctx->auth_params.password;
        /* do nothing */
        break;
    case NR_TURN_CLIENT_MODE_REFRESH_REQUEST:
      fail_on_error = 1;
      compute_lt_key = 1;
        username = ctx->auth_params.username;
        password = &ctx->auth_params.password;
        /* do nothing */
        break;
    case NR_TURN_CLIENT_MODE_PERMISSION_REQUEST:
      fail_on_error = 1;
      compute_lt_key = 1;
        username = ctx->auth_params.username;
        password = &ctx->auth_params.password;
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

    if (compute_lt_key) {
      if (!ctx->realm || !username) {
        r_log(NR_LOG_STUN, LOG_DEBUG, "Long-term auth required but no realm/username specified. Randomizing key");
        /* Fill the key with random bytes to guarantee non-match */
        if (r=nr_crypto_random_bytes(hmac_key_d, sizeof(hmac_key_d)))
          ABORT(r);
      }
      else {
        if (r=nr_stun_compute_lt_message_integrity_password(username, ctx->realm,
                                                            password, &hmac_key))
          ABORT(r);
      }
      password = &hmac_key;
    }

    if (ctx->response) {
      nr_stun_message_destroy(&ctx->response);
    }

    /* TODO(bcampen@mozilla.com): Bug 1023619, refactor this. */
    if ((r=nr_stun_message_create2(&ctx->response, msg, len)))
        ABORT(r);

    if ((r=nr_stun_decode_message(ctx->response, nr_stun_client_get_password, password))) {
        r_log(NR_LOG_STUN,LOG_WARNING,"STUN-CLIENT(%s): error decoding response",ctx->label);
        ABORT(r);
    }

    /* This will return an error if request and response don't match,
       which is how we reject responses that match other contexts. */
    if ((r=nr_stun_receive_message(ctx->request, ctx->response))) {
        r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): Response is not for us",ctx->label);
        ABORT(r);
    }

    r_log(NR_LOG_STUN,LOG_INFO,
          "STUN-CLIENT(%s): Received response; processing",ctx->label);
    response_matched=1;

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
      if (fail_on_error) {
        ctx->state = NR_STUN_CLIENT_STATE_FAILED;
      }
        /* Note: most times we call process_error_response, we get r != 0.

           However, if the error is to be discarded, we get r == 0, smash
           the error code, and just keep going.
        */
        if ((r=nr_stun_process_error_response(ctx->response, &ctx->error_code))) {
            ABORT(r);
        }
        else {
          ctx->error_code = 0xffff;
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
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_XOR_MAPPED_ADDRESS, 0)) {
            if (nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_MAPPED_ADDRESS, 0)) {
              /* Compensate for a bug in Google's STUN servers where they always respond with MAPPED-ADDRESS */
              r_log(NR_LOG_STUN,LOG_INFO,"STUN-CLIENT(%s): No XOR-MAPPED-ADDRESS but MAPPED-ADDRESS. Falling back (though server is wrong).", ctx->label);
            }
            else {
              ABORT(R_BAD_DATA);
            }
        }

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
    case NR_TURN_CLIENT_MODE_ALLOCATE_REQUEST:
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_XOR_MAPPED_ADDRESS, 0))
            ABORT(R_BAD_DATA);
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_MESSAGE_INTEGRITY, 0))
            ABORT(R_BAD_DATA);

        if (!nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_XOR_RELAY_ADDRESS, &attr))
          ABORT(R_BAD_DATA);

        if ((r=nr_stun_transport_addr_check(&attr->u.relay_address.unmasked,
                                            ctx->mapped_addr_check_mask)))
          ABORT(r);

        if ((r=nr_transport_addr_copy(
                &ctx->results.allocate_response.relay_addr,
                &attr->u.relay_address.unmasked)))
          ABORT(r);

        if (!nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_LIFETIME, &attr))
          ABORT(R_BAD_DATA);
        ctx->results.allocate_response.lifetime_secs=attr->u.lifetime_secs;

        r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): Received relay address: %s", ctx->label, ctx->results.allocate_response.relay_addr.as_string);

        mapped_addr = &ctx->results.allocate_response.mapped_addr;

        break;
    case NR_TURN_CLIENT_MODE_REFRESH_REQUEST:
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_MESSAGE_INTEGRITY, 0))
            ABORT(R_BAD_DATA);
        if (!nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_LIFETIME, &attr))
          ABORT(R_BAD_DATA);
        ctx->results.refresh_response.lifetime_secs=attr->u.lifetime_secs;
        break;
    case NR_TURN_CLIENT_MODE_PERMISSION_REQUEST:
        if (! nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_MESSAGE_INTEGRITY, 0))
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
            if ((r=nr_stun_transport_addr_check(&attr->u.xor_mapped_address.unmasked,
                                                ctx->mapped_addr_check_mask)))
                ABORT(r);

            if ((r=nr_transport_addr_copy(mapped_addr, &attr->u.xor_mapped_address.unmasked)))
                ABORT(r);
        }
        else if (nr_stun_message_has_attribute(ctx->response, NR_STUN_ATTR_MAPPED_ADDRESS, &attr)) {
            if ((r=nr_stun_transport_addr_check(&attr->u.mapped_address,
                                                ctx->mapped_addr_check_mask)))
                ABORT(r);

            if ((r=nr_transport_addr_copy(mapped_addr, &attr->u.mapped_address)))
                ABORT(r);
        }
        else
            ABORT(R_BAD_DATA);

        // STUN doesn't distinguish protocol in mapped address, therefore
        // assign used protocol from peer_addr
        if (mapped_addr->protocol!=peer_addr->protocol){
          mapped_addr->protocol=peer_addr->protocol;
          nr_transport_addr_fmt_addr_string(mapped_addr);
        }

        r_log(NR_LOG_STUN,LOG_DEBUG,"STUN-CLIENT(%s): Received mapped address: %s", ctx->label, mapped_addr->as_string);
    }

    ctx->state=NR_STUN_CLIENT_STATE_DONE;

    _status=0;
  abort:
    if(_status && response_matched){
      r_log(NR_LOG_STUN,LOG_WARNING,"STUN-CLIENT(%s): Error processing response: %s, stun error code %d.", ctx->label, nr_strerror(_status), (int)ctx->error_code);
    }

    if ((ctx->state != NR_STUN_CLIENT_STATE_RUNNING) &&
        (ctx->state != NR_STUN_CLIENT_STATE_WAITING)) {
        /* Cancel the timer firing */
        if (ctx->timer_handle) {
            NR_async_timer_cancel(ctx->timer_handle);
            ctx->timer_handle = 0;
        }

        nr_stun_client_fire_finished_cb(ctx);
    }

    return(_status);
  }

int nr_stun_client_ctx_destroy(nr_stun_client_ctx **ctxp)
  {
    nr_stun_client_ctx *ctx;

    if(!ctxp || !*ctxp)
      return(0);

    ctx=*ctxp;
    *ctxp=0;

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

int nr_stun_client_wait(nr_stun_client_ctx *ctx)
  {
    nr_stun_client_cancel(ctx);
    ctx->state=NR_STUN_CLIENT_STATE_WAITING;

    ctx->request_ct = ctx->maximum_transmits;
    ctx->timeout_ms = ctx->maximum_transmits_timeout_ms;
    NR_ASYNC_TIMER_SET(ctx->timeout_ms, nr_stun_client_timer_expired_cb, ctx, &ctx->timer_handle);

    return(0);
  }

int nr_stun_client_failed(nr_stun_client_ctx *ctx)
  {
    nr_stun_client_cancel(ctx);
    ctx->state=NR_STUN_CLIENT_STATE_FAILED;
    nr_stun_client_fire_finished_cb(ctx);
    return(0);
  }
