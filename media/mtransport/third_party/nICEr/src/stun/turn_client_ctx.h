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



#ifndef _turn_client_ctx_h
#define _turn_client_ctx_h

typedef struct nr_turn_client_ctx_ {
  int state;
#define NR_TURN_CLIENT_STATE_INITTED         1
#define NR_TURN_CLIENT_STATE_RUNNING         2
#define NR_TURN_CLIENT_STATE_ALLOCATED       3
//#define NR_TURN_CLIENT_STATE_ACTIVE          5
#define NR_TURN_CLIENT_STATE_FAILED          6
#define NR_TURN_CLIENT_STATE_TIMED_OUT       7
#define NR_TURN_CLIENT_STATE_CANCELLED       8

  int phase;
#define NR_TURN_CLIENT_PHASE_INITIALIZED          -1 
#define NR_TURN_CLIENT_PHASE_ALLOCATE_REQUEST1    0
#define NR_TURN_CLIENT_PHASE_ALLOCATE_REQUEST2    1
#define NR_TURN_CLIENT_PHASE_SET_ACTIVE_DEST      2

  nr_stun_client_ctx  *stun_ctx[3];    /* one for each phase */

  char *label;
  nr_transport_addr turn_server_addr;
  nr_socket *sock;
  nr_socket *wrapping_sock;
  char *username;
  Data *password;

  nr_transport_addr relay_addr;

  NR_async_cb finished_cb;
  void *cb_arg;
} nr_turn_client_ctx;

extern int NR_LOG_TURN;

int nr_turn_client_ctx_create(char *label, nr_socket *sock, nr_socket *wsock, nr_transport_addr *turn_server, UINT4 RTO, nr_turn_client_ctx **ctxp);
int nr_turn_client_ctx_destroy(nr_turn_client_ctx **ctxp);
int nr_turn_client_allocate(nr_turn_client_ctx *ctx, char *username, Data *password, UINT4 bandwidth_kbps, UINT4 lifetime_secs, NR_async_cb finished_cb, void *cb_arg);
//int nr_turn_client_set_active_destination(nr_turn_client_ctx *ctx, nr_transport_addr *remote_addr, NR_async_cb finished_cb, void *cb_arg);
int nr_turn_client_process_response(nr_turn_client_ctx *ctx, UCHAR *msg, int len, nr_transport_addr *turn_server_addr);
int nr_turn_client_cancel(nr_turn_client_ctx *ctx);
int nr_turn_client_relay_indication_data(nr_socket *sock, const UCHAR *msg, size_t len, int flags, nr_transport_addr *relay_addr, nr_transport_addr *remote_addr);
int nr_turn_client_rewrite_indication_data(UCHAR *msg, size_t len, size_t *newlen, nr_transport_addr *remote_addr);


#endif

