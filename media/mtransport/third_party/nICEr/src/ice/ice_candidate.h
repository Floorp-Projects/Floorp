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



#ifndef _ice_candidate_h
#define _ice_candidate_h
#ifdef __cplusplus
using namespace std;
extern "C" {
#endif /* __cplusplus */

typedef enum {HOST=1, SERVER_REFLEXIVE, PEER_REFLEXIVE, RELAYED, CTYPE_MAX} nr_ice_candidate_type;

struct nr_ice_candidate_ {
  char *label;
  char codeword[5];
  int state;
#define NR_ICE_CAND_STATE_CREATED          1
#define NR_ICE_CAND_STATE_INITIALIZING     2
#define NR_ICE_CAND_STATE_INITIALIZED      3
#define NR_ICE_CAND_STATE_FAILED           4
#define NR_ICE_CAND_PEER_CANDIDATE_UNPAIRED 9
#define NR_ICE_CAND_PEER_CANDIDATE_PAIRED   10
  struct nr_ice_ctx_ *ctx;
  nr_ice_socket *isock;               /* The socket to read from
                                         (it contains all other candidates
                                         on this socket) */
  nr_socket *osock;                   /* The socket to write to */
  nr_ice_media_stream *stream;        /* The media stream this is associated with */
  nr_ice_component *component;        /* The component this is associated with */
  nr_ice_candidate_type type;         /* The type of the candidate (S 4.1.1) */
  UCHAR component_id;                 /* The component id (S 4.1.2.1) */
  nr_transport_addr addr;             /* The advertised address;
                                         JDR calls this the candidate */
  nr_transport_addr base;             /* The base address (S 2.1)*/
  char *foundation;                   /* Foundation for the candidate (S 4) */
  UINT4 priority;                     /* The priority value (S 5.4 */
  nr_ice_stun_server *stun_server;
  nr_transport_addr stun_server_addr; /* Resolved STUN server address */
  void *delay_timer;
  void *resolver_handle;

  /* Holding data for STUN and TURN */
  union {
    struct {
      nr_stun_client_ctx *stun;
      void *stun_handle;
    } srvrflx;
    struct {
      nr_turn_client_ctx *turn;
      nr_ice_turn_server *server;
      nr_ice_candidate *srvflx_candidate;
      nr_socket *turn_sock;
      void *turn_handle;
    } relayed;
  } u;

  NR_async_cb done_cb;
  void *cb_arg;

  NR_async_cb ready_cb;
  void *ready_cb_arg;
  void *ready_cb_timer;

  TAILQ_ENTRY(nr_ice_candidate_) entry_sock;
  TAILQ_ENTRY(nr_ice_candidate_) entry_comp;
};

extern char *nr_ice_candidate_type_names[];


int nr_ice_candidate_create(struct nr_ice_ctx_ *ctx,nr_ice_component *component, nr_ice_socket *isock, nr_socket *osock, nr_ice_candidate_type ctype, nr_ice_stun_server *stun_server, UCHAR component_id, nr_ice_candidate **candp);
int nr_ice_candidate_initialize(nr_ice_candidate *cand, NR_async_cb ready_cb, void *cb_arg);
void nr_ice_candidate_compute_codeword(nr_ice_candidate *cand);
int nr_ice_candidate_process_stun(nr_ice_candidate *cand, UCHAR *msg, int len, nr_transport_addr *faddr);
int nr_ice_candidate_destroy(nr_ice_candidate **candp);
void nr_ice_candidate_destroy_cb(NR_SOCKET s, int h, void *cb_arg);
int nr_ice_format_candidate_attribute(nr_ice_candidate *cand, char *attr, int maxlen);
int nr_ice_peer_candidate_from_attribute(nr_ice_ctx *ctx,char *attr,nr_ice_media_stream *stream,nr_ice_candidate **candp);
int nr_ice_peer_peer_rflx_candidate_create(nr_ice_ctx *ctx,char *label, nr_ice_component *comp,nr_transport_addr *addr, nr_ice_candidate **candp);
int nr_ice_candidate_compute_priority(nr_ice_candidate *cand);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif

