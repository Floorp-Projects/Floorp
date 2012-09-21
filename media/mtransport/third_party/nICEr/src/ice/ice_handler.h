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



#ifndef _ice_h
#define _ice_h
#ifdef __cplusplus
using namespace std;
extern "C" {
#endif /* __cplusplus */

typedef struct nr_ice_handler_vtbl_ {
  /* The checks on this media stream are done. The handler needs to 
     select a single pair to proceed with (regular nomination).
     Once this returns the check starts and the pair can be
     written on. Use nr_ice_candidate_pair_select() to perform the
     selection.
     TODO: !ekr! is this right?
  */
  int (*select_pair)(void *obj,nr_ice_media_stream *stream, 
int component_id, nr_ice_cand_pair **potentials,int potential_ct);

  /* This media stream is ready to read/write (aggressive nomination).
     May be called again if the nominated pair changes due to
     ICE instability. TODO: !ekr! think about this
  */
  int (*stream_ready)(void *obj, nr_ice_media_stream *stream);

  /* This media stream has failed */
  int (*stream_failed)(void *obj, nr_ice_media_stream *stream);

  /* ICE is completed for this peer ctx
     if need_update is nonzero, a SIP update is required
   */
  int (*ice_completed)(void *obj, nr_ice_peer_ctx *pctx);

  /* A message was delivered to us */
  int (*msg_recvd)(void *obj, nr_ice_peer_ctx *pctx, nr_ice_media_stream *stream, int component_id, UCHAR *msg, int len);
} nr_ice_handler_vtbl;

typedef struct nr_ice_handler_ {
  void *obj;
  nr_ice_handler_vtbl *vtbl;
} nr_ice_handler;

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif

