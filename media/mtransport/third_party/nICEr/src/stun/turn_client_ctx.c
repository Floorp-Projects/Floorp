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



static char *RCSSTRING __UNUSED__="$Id: turn_client_ctx.c,v 1.2 2008/04/28 18:21:30 ekr Exp $";

#ifdef USE_TURN

#include <assert.h>
#include <string.h>

#include "nr_api.h"
#include "stun.h"
#include "turn_client_ctx.h"


int NR_LOG_TURN = 0;

#define NUMBER_OF_STUN_CTX  ((int)(sizeof(((nr_turn_client_ctx*)0)->stun_ctx)/sizeof(*(((nr_turn_client_ctx*)0)->stun_ctx))))

static char *TURN_PHASE_LABEL[NUMBER_OF_STUN_CTX] = {
    "ALLOCATE-REQUEST1",
    "ALLOCATE-REQUEST2",
    "SET-ACTIVE-DESTINATION"
};

static int TURN_PHASE_MODE[NUMBER_OF_STUN_CTX] = {
    NR_TURN_CLIENT_MODE_ALLOCATE_REQUEST1,
    NR_TURN_CLIENT_MODE_ALLOCATE_REQUEST2,
    NR_TURN_CLIENT_MODE_SET_ACTIVE_DEST_REQUEST
};


static int nr_turn_client_next_action(nr_turn_client_ctx *ctx, int stun_ctx_state);
static void nr_turn_client_cb(NR_SOCKET s, int how, void *cb_arg);
static int
nr_turn_client_prepare_start(nr_turn_client_ctx *ctx, char *username, Data *password, UINT4 bandwidth_kbps, UINT4 lifetime_secs, NR_async_cb finished_cb, void *cb_arg);


int
nr_turn_client_next_action(nr_turn_client_ctx *ctx, int stun_ctx_state)
{
    int r,_status;

    assert(ctx->phase >= -1 && ctx->phase < NUMBER_OF_STUN_CTX);

    switch (ctx->state) {
    //case NR_TURN_CLIENT_STATE_ALLOCATING:
    case NR_TURN_CLIENT_STATE_RUNNING:
    case NR_TURN_CLIENT_STATE_ALLOCATED:
        /* these are the acceptable states */
        break;
    default:
        assert(0);
        ABORT(R_INTERNAL);  /* should never happen */
        break;
    }

    if (ctx->phase != NR_TURN_CLIENT_PHASE_INITIALIZED
     && ctx->state != NR_TURN_CLIENT_STATE_ALLOCATED) {
        r_log(NR_LOG_TURN,LOG_DEBUG,"TURN-CLIENT(%s): Finishing phase %s", ctx->label, TURN_PHASE_LABEL[ctx->phase]);
    }

    switch (stun_ctx_state) {
    case NR_STUN_CLIENT_STATE_DONE:
        /* sanity check that NR_TURN_CLIENT_PHASE_SET_ACTIVE_DEST is
           the final phase */
        assert(NUMBER_OF_STUN_CTX == NR_TURN_CLIENT_PHASE_SET_ACTIVE_DEST+1);

        if (ctx->phase == NR_TURN_CLIENT_PHASE_SET_ACTIVE_DEST) {
#if 0
            ctx->state = NR_TURN_CLIENT_STATE_ACTIVE;
#else
            UNIMPLEMENTED;
#endif
            r_log(NR_LOG_TURN,LOG_DEBUG,"TURN-CLIENT(%s): Active", ctx->label);
        }
        else if (ctx->phase == NR_TURN_CLIENT_PHASE_ALLOCATE_REQUEST2
              && ctx->state != NR_TURN_CLIENT_STATE_ALLOCATED) {
            ctx->state = NR_TURN_CLIENT_STATE_ALLOCATED;
            r_log(NR_LOG_TURN,LOG_DEBUG,"TURN-CLIENT(%s): Allocated", ctx->label);
        }
        else {
            ++(ctx->phase);

            ctx->state=NR_TURN_CLIENT_STATE_RUNNING;

            r_log(NR_LOG_TURN,LOG_DEBUG,"TURN-CLIENT(%s): Starting phase %s", ctx->label, TURN_PHASE_LABEL[ctx->phase]);

            if ((r=nr_stun_client_start(ctx->stun_ctx[ctx->phase], TURN_PHASE_MODE[ctx->phase], nr_turn_client_cb, ctx)))
                ABORT(r);
        }
        break;

    case NR_STUN_CLIENT_STATE_FAILED:
        ctx->state = NR_TURN_CLIENT_STATE_FAILED;
        r_log(NR_LOG_TURN,LOG_DEBUG,"TURN-CLIENT(%s): Failed in phase %s", ctx->label, TURN_PHASE_LABEL[ctx->phase]);
        ABORT(R_FAILED);
        break;

    case NR_STUN_CLIENT_STATE_TIMED_OUT:
        ctx->state = NR_TURN_CLIENT_STATE_TIMED_OUT;
        r_log(NR_LOG_TURN,LOG_DEBUG,"TURN-CLIENT(%s): Timed out", ctx->label);
        ABORT(R_INTERRUPTED);
        break;

    default:
        assert(0);
        ABORT(R_INTERNAL);  /* should never happen */
        break;
    }

    _status=0;
  abort:
    if (ctx->state != NR_TURN_CLIENT_STATE_RUNNING) {
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

void
nr_turn_client_cb(NR_SOCKET s, int how, void *cb_arg)
{
    int r,_status;
    nr_turn_client_ctx *ctx = (nr_turn_client_ctx*)cb_arg;
    nr_stun_client_ctx *stun_ctx = ctx->stun_ctx[ctx->phase];

    assert(ctx->phase >= 0);

    if ((r=nr_turn_client_next_action(ctx, stun_ctx->state)))
        ABORT(r);

    _status=0;
  abort:
    if (_status) {
        r_log_nr(NR_LOG_TURN,LOG_WARNING,_status,"TURN-CLIENT(%s): Failed to move to next phase", ctx->label);
    }
    /*return(_status);*/
}

int
nr_turn_client_ctx_create(char *label, nr_socket *sock, nr_socket *wsock, nr_transport_addr *turn_server, UINT4 RTO, nr_turn_client_ctx **ctxp)
{
    nr_turn_client_ctx *ctx=0;
    int r,_status;
    int i;
    char string[256];

    if ((r=r_log_register("turn", &NR_LOG_TURN)))
      ABORT(r);

    if(!(ctx=RCALLOC(sizeof(nr_turn_client_ctx))))
      ABORT(R_NO_MEMORY);

    ctx->state=NR_TURN_CLIENT_STATE_INITTED;

    if(!(ctx->label=r_strdup(label)))
      ABORT(R_NO_MEMORY);

    ctx->sock=sock;
    ctx->wrapping_sock=wsock;

    ctx->phase=NR_TURN_CLIENT_PHASE_INITIALIZED;

    nr_transport_addr_copy(&ctx->turn_server_addr,turn_server);

    for (i = 0; i < NUMBER_OF_STUN_CTX; ++i) {
        snprintf(string, sizeof(string), "%s:%s", label, TURN_PHASE_LABEL[i]);

        if ((r=nr_stun_client_ctx_create(string, sock, turn_server, RTO, &ctx->stun_ctx[i]))) {
            r_log_nr(NR_LOG_TURN,LOG_ERR,r,"TURN(%s): Failed to start",string);
            ABORT(r);
        }
    }

    *ctxp=ctx;

    _status=0;
  abort:
    if(_status){
      nr_turn_client_ctx_destroy(&ctx);
    }
    return(_status);
}

int
nr_turn_client_ctx_destroy(nr_turn_client_ctx **ctxp)
{
    nr_turn_client_ctx *ctx;
    int i;

    if(!ctxp || !*ctxp)
      return(0);

    ctx=*ctxp;

    for (i = 0; i < NUMBER_OF_STUN_CTX; ++i)
        nr_stun_client_ctx_destroy(&ctx->stun_ctx[i]);

    RFREE(ctx->username);
    r_data_destroy(&ctx->password);

    RFREE(ctx->label);
    RFREE(ctx);

    return(0);
}

static int
nr_turn_client_prepare_start(nr_turn_client_ctx *ctx, char *username, Data *password, UINT4 bandwidth_kbps, UINT4 lifetime_secs, NR_async_cb finished_cb, void *cb_arg)
{
    int r,_status;
    nr_stun_client_allocate_request1_params          *allocate_request1 = 0;
    nr_stun_client_allocate_request2_params          *allocate_request2 = 0;
    nr_stun_client_allocate_response1_results        *allocate_response1 = 0;
//    nr_stun_client_allocate_response2_results        *allocate_response2;

    if (ctx->state != NR_TURN_CLIENT_STATE_INITTED)
        ABORT(R_NOT_PERMITTED);

    assert(ctx->phase == NR_TURN_CLIENT_PHASE_INITIALIZED);

    allocate_request1        = &ctx->stun_ctx[NR_TURN_CLIENT_PHASE_ALLOCATE_REQUEST1]->params.allocate_request1;
    allocate_response1       = &ctx->stun_ctx[NR_TURN_CLIENT_PHASE_ALLOCATE_REQUEST1]->results.allocate_response1;
    allocate_request2        = &ctx->stun_ctx[NR_TURN_CLIENT_PHASE_ALLOCATE_REQUEST2]->params.allocate_request2;

    ctx->state=NR_TURN_CLIENT_STATE_RUNNING;
    ctx->finished_cb=finished_cb;
    ctx->cb_arg=cb_arg;

    ctx->username = r_strdup(username);
    if (!ctx->username)
        ABORT(R_NO_MEMORY);

    if ((r=r_data_create(&ctx->password,
                         password->data, password->len)))
        ABORT(r);

    allocate_request1->username = ctx->username;

    allocate_request2->username = ctx->username;
    allocate_request2->password = ctx->password;
    allocate_request2->realm = allocate_response1->realm;
    allocate_request2->nonce = allocate_response1->nonce;
    allocate_request2->bandwidth_kbps = bandwidth_kbps;
    allocate_request2->lifetime_secs = lifetime_secs;

    _status=0;
  abort:
    if (_status)
        ctx->state = NR_TURN_CLIENT_STATE_FAILED;
    if (ctx->state != NR_TURN_CLIENT_STATE_RUNNING) {
        ctx->finished_cb = 0;  /* prevent 2nd call */
        /* finished_cb call must be absolutely last thing in function
         * because as a side effect this ctx may be operated on in the
         * callback */
        finished_cb(0,0,cb_arg);
    }

    return(_status);
}

int
nr_turn_client_allocate(nr_turn_client_ctx *ctx, char *username, Data *password, UINT4 bandwidth_kbps, UINT4 lifetime_secs, NR_async_cb finished_cb, void *cb_arg)
{
    int r,_status;

    if ((r=nr_turn_client_prepare_start(ctx, username, password, bandwidth_kbps, lifetime_secs, finished_cb, cb_arg)))
        ABORT(r);

    /* "DONE" tells nr_turn_client_next_action to start the next phase */
    if ((r=nr_turn_client_next_action(ctx, NR_STUN_CLIENT_STATE_DONE)))
        ABORT(r);

    _status=0;
  abort:
    return(_status);
}

#if 0
// this is the code for doing a SET-ACTIVE-DESTINATION, but it's
// got a bogus SendIndication->DataIndication pair in there that
// needs to be removed
int
nr_turn_client_set_active_destination(nr_turn_client_ctx *ctx, nr_transport_addr *remote_addr, NR_async_cb finished_cb, void *cb_arg)
{
    int r,_status;
    nr_stun_client_set_active_dest_request_params    *set_active_dest_request;

    if (ctx->state != NR_TURN_CLIENT_STATE_ALLOCATED)
        ABORT(R_NOT_PERMITTED);

    assert(ctx->phase == NR_TURN_CLIENT_PHASE_ALLOCATE_REQUEST2);

    set_active_dest_request = &ctx->stun_ctx[NR_TURN_CLIENT_PHASE_SET_ACTIVE_DESTINATION]->params.set_active_dest_request;

    ctx->finished_cb=finished_cb;
    ctx->cb_arg=cb_arg;

    if ((r=nr_transport_addr_copy(&set_active_dest_request->remote_addr, remote_addr)))
        ABORT(r);

    _status=0;
  abort:
    if (_status)
        ctx->state = NR_TURN_CLIENT_STATE_FAILED;
    if (ctx->state != NR_TURN_CLIENT_STATE_ALLOCATED) {
        ctx->finished_cb = 0;  /* prevent 2nd call */
        /* finished_cb call must be absolutely last thing in function
         * because as a side effect this ctx may be operated on in the
         * callback */
        finished_cb(0,0,cb_arg);
    }

    return(_status);
}
#endif

int
nr_turn_client_process_response(nr_turn_client_ctx *ctx, UCHAR *msg, int len, nr_transport_addr *turn_server_addr)
{
    int r,_status;
    nr_stun_client_ctx *stun_ctx;

    assert(ctx->phase >= 0 && ctx->phase < NUMBER_OF_STUN_CTX);
    if (ctx->phase < 0 || ctx->phase > NUMBER_OF_STUN_CTX)
        ABORT(R_INTERNAL);  /* should never happen */

    stun_ctx = ctx->stun_ctx[ctx->phase];

    if (ctx->state != NR_TURN_CLIENT_STATE_RUNNING
     && ctx->state != NR_TURN_CLIENT_STATE_ALLOCATED) {
#if 0
//TODO: !nn! it really shouldn't be the case that we're processing a response in this state,
//TODO: but perhaps we failed and we haven't taken ourself off the
//TODO: ASYNC wait list, so an outstanding packet can cause us this grief
assert(0);
        ABORT(R_INTERNAL);  /* should never happen */
#else
//TODO: !nn! fix so we can never get into this state
r_log(NR_LOG_TURN,LOG_ERR,"TURN-CLIENT(%s): dropping packet in phase %s", ctx->label, TURN_PHASE_LABEL[ctx->phase]);
return R_INTERNAL;
#endif
    }

    if (ctx->phase != NR_TURN_CLIENT_PHASE_INITIALIZED) {
        r_log(NR_LOG_TURN,LOG_DEBUG,"TURN-CLIENT(%s): Received response in phase %s", ctx->label, TURN_PHASE_LABEL[ctx->phase]);
    }
    else {
        ABORT(R_INTERNAL);
    }

    if ((r=nr_stun_client_process_response(stun_ctx, msg, len, turn_server_addr)))
        ABORT(r);

    _status=0;
  abort:
    if (ctx->state != NR_TURN_CLIENT_STATE_RUNNING) {
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

int
nr_turn_client_cancel(nr_turn_client_ctx *ctx)
{
    int i;

    if (ctx->state == NR_TURN_CLIENT_STATE_RUNNING
     || ctx->state == NR_TURN_CLIENT_STATE_ALLOCATED) {
        for (i = 0; i < NUMBER_OF_STUN_CTX; ++i) {
            if (ctx->stun_ctx[i])
                nr_stun_client_cancel(ctx->stun_ctx[i]);
        }
    }

    ctx->state=NR_TURN_CLIENT_STATE_CANCELLED;

    return(0);
}

int
nr_turn_client_relay_indication_data(nr_socket *sock, const UCHAR *msg, size_t len, int flags, nr_transport_addr *relay_addr, nr_transport_addr *remote_addr)
{
    int r,_status;
    nr_stun_client_send_indication_params params = { { 0 } };
    nr_stun_message *ind = 0;

    if ((r=nr_transport_addr_copy(&params.remote_addr, remote_addr)))
        ABORT(r);

    params.data.data = (UCHAR*)msg;
    params.data.len = len;
 
    if ((r=nr_stun_build_send_indication(&params, &ind)))
        ABORT(r);

    if ((r=nr_socket_sendto(sock, ind->buffer, ind->length, flags, relay_addr)))
        ABORT(r);

    _status=0;
  abort:
    RFREE(ind);
    return(_status);
}

int
nr_turn_client_rewrite_indication_data(UCHAR *msg, size_t len, size_t *newlen, nr_transport_addr *remote_addr)
{
    int _status;
#if 0
    int r,_status;
    nr_stun_message ind;

    if ((r=nr_stun_parse_message((char*)msg, len, &ind)))
        ABORT(r);

    if (ind.msgHdr.msgType != DataIndicationMsg)
        ABORT(R_BAD_ARGS);

    if (ind.hasRemoteAddress) {
        if ((r=nr_transport_addr_copy(remote_addr, &ind.remoteAddress)))
            ABORT(r);
    }
    else {
        ABORT(R_BAD_ARGS);
    }

    if (ind.hasData) {
        assert(ind.data.length < len);

        memmove(msg, ind.data.data, ind.data.length);
        msg[ind.data.length] = '\0';

        *newlen = ind.data.length;
    }
    else {
        *newlen = 0;
    }
#else
//TODO: what will this be turned into in the latest version of the spec???
ABORT(R_FAILED);
#endif

    _status=0;
  abort:
    return(_status);
}

#endif /* USE_TURN */
