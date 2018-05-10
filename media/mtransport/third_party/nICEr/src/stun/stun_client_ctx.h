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



#ifndef _stun_client_ctx_h
#define _stun_client_ctx_h

/* forward declaration */
typedef struct nr_stun_client_ctx_ nr_stun_client_ctx;

#include "stun.h"

/* Checklist for adding new STUN transaction types

   1. Add new method type in stun.h (NR_METHOD_*)
   2. Add new MSGs in stun.h (NR_STUN_MSG_*)
   3. Add new messages to stun_util.c:nr_stun_msg_type
   4. Add new request type to stun_build.h
   4. Add new message builder to stun_build.c
   5. Add new response type to stun_client_ctx.h
   6. Add new arm to stun_client_ctx.c:nr_stun_client_send_request
   7. Add new arms to nr_stun_client_process_response
   8. Add new arms to stun_hint.c:nr_is_stun_message
*/




typedef union nr_stun_client_params_ {

    nr_stun_client_stun_binding_request_params               stun_binding_request;
    nr_stun_client_stun_keepalive_params                     stun_keepalive;
#ifdef USE_STUND_0_96
    nr_stun_client_stun_binding_request_stund_0_96_params    stun_binding_request_stund_0_96;
#endif /* USE_STUND_0_96 */

#ifdef USE_ICE
    nr_stun_client_ice_binding_request_params                ice_binding_request;
#endif /* USE_ICE */

#ifdef USE_TURN
    nr_stun_client_allocate_request_params                   allocate_request;
    nr_stun_client_refresh_request_params                    refresh_request;
    nr_stun_client_permission_request_params                 permission_request;
    nr_stun_client_send_indication_params                    send_indication;
#endif /* USE_TURN */

} nr_stun_client_params;

typedef struct nr_stun_client_stun_binding_response_results_ {
    nr_transport_addr mapped_addr;
} nr_stun_client_stun_binding_response_results;

typedef struct nr_stun_client_stun_binding_response_stund_0_96_results_ {
    nr_transport_addr mapped_addr;
} nr_stun_client_stun_binding_response_stund_0_96_results;

#ifdef USE_ICE
typedef struct nr_stun_client_ice_use_candidate_results_ {
#ifdef WIN32  // silly VC++ gives error if no members
    int dummy;
#endif
} nr_stun_client_ice_use_candidate_results;

typedef struct nr_stun_client_ice_binding_response_results_ {
    nr_transport_addr mapped_addr;
} nr_stun_client_ice_binding_response_results;
#endif /* USE_ICE */

#ifdef USE_TURN
typedef struct nr_stun_client_allocate_response_results_ {
    nr_transport_addr relay_addr;
    nr_transport_addr mapped_addr;
    UINT4 lifetime_secs;
} nr_stun_client_allocate_response_results;

typedef struct nr_stun_client_refresh_response_results_ {
    UINT4 lifetime_secs;
} nr_stun_client_refresh_response_results;

typedef struct nr_stun_client_permission_response_results_ {
    UINT4 lifetime_secs;
} nr_stun_client_permission_response_results;

#endif /* USE_TURN */

typedef union nr_stun_client_results_ {
    nr_stun_client_stun_binding_response_results              stun_binding_response;
    nr_stun_client_stun_binding_response_stund_0_96_results   stun_binding_response_stund_0_96;

#ifdef USE_ICE
    nr_stun_client_ice_use_candidate_results                  ice_use_candidate;
    nr_stun_client_ice_binding_response_results               ice_binding_response;
#endif /* USE_ICE */

#ifdef USE_TURN
    nr_stun_client_allocate_response_results                   allocate_response;
    nr_stun_client_refresh_response_results                    refresh_response;
#endif /* USE_TURN */
} nr_stun_client_results;

struct nr_stun_client_ctx_ {
  int state;
#define NR_STUN_CLIENT_STATE_INITTED         0
#define NR_STUN_CLIENT_STATE_RUNNING         1
#define NR_STUN_CLIENT_STATE_DONE            2
#define NR_STUN_CLIENT_STATE_FAILED          3
#define NR_STUN_CLIENT_STATE_TIMED_OUT       4
#define NR_STUN_CLIENT_STATE_CANCELLED       5
#define NR_STUN_CLIENT_STATE_WAITING         6

  int mode;
#define NR_STUN_CLIENT_MODE_BINDING_REQUEST_SHORT_TERM_AUTH   1
#define NR_STUN_CLIENT_MODE_BINDING_REQUEST_LONG_TERM_AUTH    2
#define NR_STUN_CLIENT_MODE_BINDING_REQUEST_NO_AUTH           3
#define NR_STUN_CLIENT_MODE_KEEPALIVE                         4
#define NR_STUN_CLIENT_MODE_BINDING_REQUEST_STUND_0_96        5
#ifdef USE_ICE
#define NR_ICE_CLIENT_MODE_USE_CANDIDATE                  10
#define NR_ICE_CLIENT_MODE_BINDING_REQUEST                11
#endif /* USE_ICE */
#ifdef USE_TURN
#define NR_TURN_CLIENT_MODE_ALLOCATE_REQUEST              20
#define NR_TURN_CLIENT_MODE_REFRESH_REQUEST               21
#define NR_TURN_CLIENT_MODE_SEND_INDICATION               22
#define NR_TURN_CLIENT_MODE_DATA_INDICATION               24
#define NR_TURN_CLIENT_MODE_PERMISSION_REQUEST            25
#endif /* USE_TURN */

  char *label;
  nr_transport_addr my_addr;
  nr_transport_addr peer_addr;
  nr_socket *sock;
  nr_stun_client_auth_params auth_params;
  nr_stun_client_params params;
  nr_stun_client_results results;
  char *nonce;
  char *realm;
  void *timer_handle;
  UINT2 request_ct;
  UINT2 retransmit_ct;
  UINT4 rto_ms;    /* retransmission time out */
  double retransmission_backoff_factor;
  UINT4 maximum_transmits;
  UINT4 maximum_transmits_timeout_ms;
  UINT4 mapped_addr_check_mask;  /* What checks to run on mapped addresses */
  int timeout_ms;
  struct timeval timer_set;
  int retry_ct;
  NR_async_cb finished_cb;
  void *cb_arg;
  nr_stun_message *request;
  nr_stun_message *response;
  UINT2 error_code;
};

int nr_stun_client_ctx_create(char *label, nr_socket *sock, nr_transport_addr *peer, UINT4 RTO, nr_stun_client_ctx **ctxp);
int nr_stun_client_start(nr_stun_client_ctx *ctx, int mode, NR_async_cb finished_cb, void *cb_arg);
int nr_stun_client_restart(nr_stun_client_ctx *ctx);
int nr_stun_client_force_retransmit(nr_stun_client_ctx *ctx);
int nr_stun_client_reset(nr_stun_client_ctx *ctx);
int nr_stun_client_ctx_destroy(nr_stun_client_ctx **ctxp);
int nr_stun_transport_addr_check(nr_transport_addr* addr, UINT4 mask);
int nr_stun_client_process_response(nr_stun_client_ctx *ctx, UCHAR *msg, int len, nr_transport_addr *peer_addr);
int nr_stun_client_cancel(nr_stun_client_ctx *ctx);
int nr_stun_client_wait(nr_stun_client_ctx *ctx);
int nr_stun_client_failed(nr_stun_client_ctx *ctx);

#endif

