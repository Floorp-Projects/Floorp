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



#ifndef _ice_ctx_h
#define _ice_ctx_h
#ifdef __cplusplus
using namespace std;
extern "C" {
#endif /* __cplusplus */

/* Not good practice but making includes simpler */
#include "transport_addr.h"
#include "nr_socket.h"
#include "nr_resolver.h"
#include "nr_interface_prioritizer.h"
#include "nr_socket_wrapper.h"
#include "stun_client_ctx.h"
#include "stun_server_ctx.h"
#include "turn_client_ctx.h"

#define NR_ICE_STUN_SERVER_TYPE_ADDR    1
#define NR_ICE_STUN_SERVER_TYPE_DNSNAME 2

typedef struct nr_ice_stun_server_ {
  int type;
  union {
    nr_transport_addr addr;
    struct {
      char host[256];  /* Limit from RFC 1034, plus a 0 byte */
      UINT2 port;
    } dnsname;
  } u;
  int id;
  int transport;
  int tls; /* Whether to use TLS or not */
} nr_ice_stun_server;

typedef struct nr_ice_turn_server_ {
    nr_ice_stun_server    turn_server;
    char                 *username;
    Data                 *password;
} nr_ice_turn_server;

typedef struct nr_ice_foundation_ {
  int index;

  nr_transport_addr addr;
  int type;
  nr_ice_stun_server *stun_server;

  STAILQ_ENTRY(nr_ice_foundation_) entry;
} nr_ice_foundation;

typedef STAILQ_HEAD(nr_ice_foundation_head_,nr_ice_foundation_) nr_ice_foundation_head;

typedef TAILQ_HEAD(nr_ice_candidate_head_,nr_ice_candidate_) nr_ice_candidate_head;
typedef TAILQ_HEAD(nr_ice_cand_pair_head_,nr_ice_cand_pair_) nr_ice_cand_pair_head;
typedef struct nr_ice_component_ nr_ice_component;
typedef struct nr_ice_media_stream_ nr_ice_media_stream;
typedef struct nr_ice_ctx_ nr_ice_ctx;
typedef struct nr_ice_peer_ctx_ nr_ice_peer_ctx;
typedef struct nr_ice_candidate_ nr_ice_candidate;
typedef struct nr_ice_cand_pair_ nr_ice_cand_pair;
typedef void (*nr_ice_trickle_candidate_cb) (void *cb_arg,
  nr_ice_ctx *ctx, nr_ice_media_stream *stream, int component_id,
  nr_ice_candidate *candidate);

#include "ice_socket.h"
#include "ice_component.h"
#include "ice_media_stream.h"
#include "ice_candidate.h"
#include "ice_candidate_pair.h"
#include "ice_handler.h"
#include "ice_peer_ctx.h"

typedef struct nr_ice_stun_id_ {
  UCHAR id[12];

  STAILQ_ENTRY(nr_ice_stun_id_) entry;
} nr_ice_stun_id;

typedef STAILQ_HEAD(nr_ice_stun_id_head_,nr_ice_stun_id_) nr_ice_stun_id_head;

typedef struct nr_ice_stats_ {
  UINT2 stun_retransmits;
  UINT2 turn_401s;
  UINT2 turn_403s;
  UINT2 turn_438s;
} nr_ice_stats;

struct nr_ice_ctx_ {
  UINT4 flags;
  char *label;

  char *ufrag;
  char *pwd;

  UINT4 Ta;

  nr_ice_stun_server *stun_servers;           /* The list of stun servers */
  int stun_server_ct;
  nr_ice_turn_server *turn_servers;           /* The list of turn servers */
  int turn_server_ct;
  nr_local_addr *local_addrs;                 /* The list of available local addresses and corresponding interface information */
  int local_addr_ct;

  nr_resolver *resolver;                      /* The resolver to use */
  nr_interface_prioritizer *interface_prioritizer;  /* Priority decision logic */
  nr_socket_wrapper_factory *turn_tcp_socket_wrapper; /* The TURN TCP socket wrapper to use */
  nr_socket_factory *socket_factory;

  nr_ice_foundation_head foundations;

  nr_ice_media_stream_head streams;           /* Media streams */
  int stream_ct;
  nr_ice_socket_head sockets;                 /* The sockets we're using */
  int uninitialized_candidates;

  UINT4 gather_rto;
  UINT4 stun_delay;

  UINT4 test_timer_divider;

  nr_ice_peer_ctx_head peers;
  nr_ice_stun_id_head ids;

  NR_async_cb done_cb;
  void *cb_arg;

  nr_ice_trickle_candidate_cb trickle_cb;
  void *trickle_cb_arg;

  char force_net_interface[MAXIFNAME];
  nr_ice_stats stats;
};

int nr_ice_ctx_create(char *label, UINT4 flags, nr_ice_ctx **ctxp);
int nr_ice_ctx_create_with_credentials(char *label, UINT4 flags, char* ufrag, char* pwd, nr_ice_ctx **ctxp);
#define NR_ICE_CTX_FLAGS_AGGRESSIVE_NOMINATION             (1)
#define NR_ICE_CTX_FLAGS_LITE                              (1<<1)
#define NR_ICE_CTX_FLAGS_RELAY_ONLY                        (1<<2)
#define NR_ICE_CTX_FLAGS_HIDE_HOST_CANDIDATES              (1<<3)
#define NR_ICE_CTX_FLAGS_ONLY_DEFAULT_ADDRS                (1<<4)
#define NR_ICE_CTX_FLAGS_ONLY_PROXY                        (1<<5)

void nr_ice_ctx_add_flags(nr_ice_ctx *ctx, UINT4 flags);
void nr_ice_ctx_remove_flags(nr_ice_ctx *ctx, UINT4 flags);
int nr_ice_ctx_destroy(nr_ice_ctx **ctxp);
int nr_ice_set_local_addresses(nr_ice_ctx *ctx, nr_local_addr* stun_addrs, int stun_addr_ct);
int nr_ice_gather(nr_ice_ctx *ctx, NR_async_cb done_cb, void *cb_arg);
int nr_ice_add_candidate(nr_ice_ctx *ctx, nr_ice_candidate *cand);
void nr_ice_gather_finished_cb(NR_SOCKET s, int h, void *cb_arg);
int nr_ice_add_media_stream(nr_ice_ctx *ctx,char *label,int components, nr_ice_media_stream **streamp);
int nr_ice_remove_media_stream(nr_ice_ctx *ctx,nr_ice_media_stream **streamp);
int nr_ice_get_global_attributes(nr_ice_ctx *ctx,char ***attrsp, int *attrctp);
int nr_ice_ctx_deliver_packet(nr_ice_ctx *ctx, nr_ice_component *comp, nr_transport_addr *source_addr, UCHAR *data, int len);
int nr_ice_ctx_is_known_id(nr_ice_ctx *ctx, UCHAR id[12]);
int nr_ice_ctx_remember_id(nr_ice_ctx *ctx, nr_stun_message *msg);
int nr_ice_ctx_finalize(nr_ice_ctx *ctx, nr_ice_peer_ctx *pctx);
int nr_ice_ctx_set_stun_servers(nr_ice_ctx *ctx,nr_ice_stun_server *servers, int ct);
int nr_ice_ctx_set_turn_servers(nr_ice_ctx *ctx,nr_ice_turn_server *servers, int ct);
int nr_ice_ctx_copy_turn_servers(nr_ice_ctx *ctx, nr_ice_turn_server *servers, int ct);
int nr_ice_ctx_set_resolver(nr_ice_ctx *ctx, nr_resolver *resolver);
int nr_ice_ctx_set_interface_prioritizer(nr_ice_ctx *ctx, nr_interface_prioritizer *prioritizer);
int nr_ice_ctx_set_turn_tcp_socket_wrapper(nr_ice_ctx *ctx, nr_socket_wrapper_factory *wrapper);
void nr_ice_ctx_set_socket_factory(nr_ice_ctx *ctx, nr_socket_factory *factory);
int nr_ice_ctx_set_trickle_cb(nr_ice_ctx *ctx, nr_ice_trickle_candidate_cb cb, void *cb_arg);
int nr_ice_ctx_hide_candidate(nr_ice_ctx *ctx, nr_ice_candidate *cand);
int nr_ice_get_new_ice_ufrag(char** ufrag);
int nr_ice_get_new_ice_pwd(char** pwd);

#define NR_ICE_MAX_ATTRIBUTE_SIZE 256

extern int LOG_ICE;

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif

