/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


// Original author: ekr@rtfm.com

// Some of this code is cut-and-pasted from nICEr. Copyright is:

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

#include <string>
#include <vector>

#include "logging.h"
#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsError.h"
#include "nsIEventTarget.h"
#include "nsNetCID.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "ScopedNSSTypes.h"

// nICEr includes
extern "C" {
#include "nr_api.h"
#include "registry.h"
#include "async_timer.h"
#include "r_crc32.h"
#include "r_memory.h"
#include "ice_util.h"
#include "transport_addr.h"
#include "nr_crypto.h"
#include "nr_socket.h"
#include "nr_socket_local.h"
#include "stun_client_ctx.h"
#include "stun_server_ctx.h"
#include "ice_codeword.h"
#include "ice_ctx.h"
#include "ice_candidate.h"
#include "ice_handler.h"
}

// Local includes
#include "nricectx.h"
#include "nricemediastream.h"
#include "nr_socket_prsock.h"
#include "nrinterfaceprioritizer.h"

namespace mozilla {

MOZ_MTLOG_MODULE("mtransport")

static bool initialized = false;

// Implement NSPR-based crypto algorithms
static int nr_crypto_nss_random_bytes(UCHAR *buf, int len) {
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot)
    return R_INTERNAL;

  SECStatus rv = PK11_GenerateRandomOnSlot(slot, buf, len);
  if (rv != SECSuccess)
    return R_INTERNAL;

  return 0;
}

static int nr_crypto_nss_hmac(UCHAR *key, int keyl, UCHAR *buf, int bufl,
                              UCHAR *result) {
  CK_MECHANISM_TYPE mech = CKM_SHA_1_HMAC;
  PK11SlotInfo *slot = 0;
  MOZ_ASSERT(keyl > 0);
  SECItem keyi = { siBuffer, key, static_cast<unsigned int>(keyl)};
  PK11SymKey *skey = 0;
  PK11Context *hmac_ctx = 0;
  SECStatus status;
  unsigned int hmac_len;
  SECItem param = { siBuffer, nullptr, 0 };
  int err = R_INTERNAL;

  slot = PK11_GetInternalKeySlot();
  if (!slot)
    goto abort;

  skey = PK11_ImportSymKey(slot, mech, PK11_OriginUnwrap,
                          CKA_SIGN, &keyi, nullptr);
  if (!skey)
    goto abort;


  hmac_ctx = PK11_CreateContextBySymKey(mech, CKA_SIGN,
                                        skey, &param);

  status = PK11_DigestBegin(hmac_ctx);
  if (status != SECSuccess)
    goto abort;

  status = PK11_DigestOp(hmac_ctx, buf, bufl);
  if (status != SECSuccess)
    goto abort;

  status = PK11_DigestFinal(hmac_ctx, result, &hmac_len, 20);
  if (status != SECSuccess)
    goto abort;

  MOZ_ASSERT(hmac_len == 20);

  err = 0;

 abort:
  if(hmac_ctx) PK11_DestroyContext(hmac_ctx, PR_TRUE);
  if (skey) PK11_FreeSymKey(skey);
  if (slot) PK11_FreeSlot(slot);

  return err;
}

static int nr_crypto_nss_md5(UCHAR *buf, int bufl, UCHAR *result) {
  int err = R_INTERNAL;
  SECStatus rv;

  const SECHashObject *ho = HASH_GetHashObject(HASH_AlgMD5);
  MOZ_ASSERT(ho);
  if (!ho)
    goto abort;

  MOZ_ASSERT(ho->length == 16);

  rv = HASH_HashBuf(ho->type, result, buf, bufl);
  if (rv != SECSuccess)
    goto abort;

  err = 0;
abort:
  return err;
}

static nr_ice_crypto_vtbl nr_ice_crypto_nss_vtbl = {
  nr_crypto_nss_random_bytes,
  nr_crypto_nss_hmac,
  nr_crypto_nss_md5
};




nsresult NrIceStunServer::ToNicerStunStruct(nr_ice_stun_server *server) const {
  int r;

  memset(server, 0, sizeof(nr_ice_stun_server));

  if (has_addr_) {
    r = nr_praddr_to_transport_addr(&addr_, &server->u.addr, 0);
    if (r) {
      return NS_ERROR_FAILURE;
    }
    server->type=NR_ICE_STUN_SERVER_TYPE_ADDR;
  }
  else {
    MOZ_ASSERT(sizeof(server->u.dnsname.host) > host_.size());
    PL_strncpyz(server->u.dnsname.host, host_.c_str(),
                sizeof(server->u.dnsname.host));
    server->u.dnsname.port = port_;
    server->type=NR_ICE_STUN_SERVER_TYPE_DNSNAME;
  }

  return NS_OK;
}


nsresult NrIceTurnServer::ToNicerTurnStruct(nr_ice_turn_server *server) const {
  memset(server, 0, sizeof(nr_ice_turn_server));

  nsresult rv = ToNicerStunStruct(&server->turn_server);
  if (NS_FAILED(rv))
    return rv;

  if (username_.empty())
    return NS_ERROR_INVALID_ARG;
  if (password_.empty())
    return NS_ERROR_INVALID_ARG;

  if (!(server->username=r_strdup(username_.c_str())))
    return NS_ERROR_OUT_OF_MEMORY;

  // TODO(ekr@rtfm.com): handle non-ASCII passwords somehow?
  // STUN requires they be SASLpreped, but we don't know if
  // they are at this point.

  // C++03 23.2.4, Paragraph 1 stipulates that the elements
  // in std::vector must be contiguous, and can therefore be
  // used as input to functions expecting C arrays.
  int r = r_data_create(&server->password,
                        const_cast<UCHAR *>(&password_[0]),
                        password_.size());
  if (r) {
    RFREE(server->username);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

// Handler callbacks
int NrIceCtx::select_pair(void *obj,nr_ice_media_stream *stream,
                   int component_id, nr_ice_cand_pair **potentials,
                   int potential_ct) {
  MOZ_MTLOG(ML_DEBUG, "select pair called: potential_ct = "
            << potential_ct);

  return 0;
}

int NrIceCtx::stream_ready(void *obj, nr_ice_media_stream *stream) {
  MOZ_MTLOG(ML_DEBUG, "stream_ready called");

  // Get the ICE ctx
  NrIceCtx *ctx = static_cast<NrIceCtx *>(obj);

  RefPtr<NrIceMediaStream> s = ctx->FindStream(stream);

  // Streams which do not exist should never be ready.
  MOZ_ASSERT(s);

  s->Ready();

  return 0;
}

int NrIceCtx::stream_failed(void *obj, nr_ice_media_stream *stream) {
  MOZ_MTLOG(ML_DEBUG, "stream_failed called");

  // Get the ICE ctx
  NrIceCtx *ctx = static_cast<NrIceCtx *>(obj);
  RefPtr<NrIceMediaStream> s = ctx->FindStream(stream);

  // Streams which do not exist should never fail.
  MOZ_ASSERT(s);

  ctx->SetState(ICE_CTX_FAILED);
  s -> SignalFailed(s);
  return 0;
}

int NrIceCtx::ice_completed(void *obj, nr_ice_peer_ctx *pctx) {
  MOZ_MTLOG(ML_DEBUG, "ice_completed called");

  // Get the ICE ctx
  NrIceCtx *ctx = static_cast<NrIceCtx *>(obj);

  ctx->SetState(ICE_CTX_OPEN);

  return 0;
}

int NrIceCtx::msg_recvd(void *obj, nr_ice_peer_ctx *pctx,
                        nr_ice_media_stream *stream, int component_id,
                        UCHAR *msg, int len) {
  // Get the ICE ctx
  NrIceCtx *ctx = static_cast<NrIceCtx *>(obj);
  RefPtr<NrIceMediaStream> s = ctx->FindStream(stream);

  // Streams which do not exist should never have packets.
  MOZ_ASSERT(s);

  s->SignalPacketReceived(s, component_id, msg, len);

  return 0;
}


RefPtr<NrIceCtx> NrIceCtx::Create(const std::string& name,
                                  bool offerer,
                                  bool set_interface_priorities) {
  RefPtr<NrIceCtx> ctx = new NrIceCtx(name, offerer);

  // Initialize the crypto callbacks
  if (!initialized) {
    NR_reg_init(NR_REG_MODE_LOCAL);
    nr_crypto_vtbl = &nr_ice_crypto_nss_vtbl;
    initialized = true;

    // Set the priorites for candidate type preferences
    NR_reg_set_uchar((char *)"ice.pref.type.srv_rflx",100);
    NR_reg_set_uchar((char *)"ice.pref.type.peer_rflx",105);
    NR_reg_set_uchar((char *)"ice.pref.type.prflx",99);
    NR_reg_set_uchar((char *)"ice.pref.type.host",125);
    NR_reg_set_uchar((char *)"ice.pref.type.relayed",126);

    if (set_interface_priorities) {
      NR_reg_set_uchar((char *)"ice.pref.interface.rl0", 255);
      NR_reg_set_uchar((char *)"ice.pref.interface.wi0", 254);
      NR_reg_set_uchar((char *)"ice.pref.interface.lo0", 253);
      NR_reg_set_uchar((char *)"ice.pref.interface.en1", 252);
      NR_reg_set_uchar((char *)"ice.pref.interface.en0", 251);
      NR_reg_set_uchar((char *)"ice.pref.interface.eth0", 252);
      NR_reg_set_uchar((char *)"ice.pref.interface.eth1", 251);
      NR_reg_set_uchar((char *)"ice.pref.interface.eth2", 249);
      NR_reg_set_uchar((char *)"ice.pref.interface.ppp", 250);
      NR_reg_set_uchar((char *)"ice.pref.interface.ppp0", 249);
      NR_reg_set_uchar((char *)"ice.pref.interface.en2", 248);
      NR_reg_set_uchar((char *)"ice.pref.interface.en3", 247);
      NR_reg_set_uchar((char *)"ice.pref.interface.em0", 251);
      NR_reg_set_uchar((char *)"ice.pref.interface.em1", 252);
      NR_reg_set_uchar((char *)"ice.pref.interface.vmnet0", 240);
      NR_reg_set_uchar((char *)"ice.pref.interface.vmnet1", 241);
      NR_reg_set_uchar((char *)"ice.pref.interface.vmnet3", 239);
      NR_reg_set_uchar((char *)"ice.pref.interface.vmnet4", 238);
      NR_reg_set_uchar((char *)"ice.pref.interface.vmnet5", 237);
      NR_reg_set_uchar((char *)"ice.pref.interface.vmnet6", 236);
      NR_reg_set_uchar((char *)"ice.pref.interface.vmnet7", 235);
      NR_reg_set_uchar((char *)"ice.pref.interface.vmnet8", 234);
      NR_reg_set_uchar((char *)"ice.pref.interface.virbr0", 233);
      NR_reg_set_uchar((char *)"ice.pref.interface.wlan0", 232);
    }

    NR_reg_set_uint4((char *)"stun.client.maximum_transmits",4);
  }

  // Create the ICE context
  int r;

  UINT4 flags = offerer ? NR_ICE_CTX_FLAGS_OFFERER:
      NR_ICE_CTX_FLAGS_ANSWERER;
  flags |= NR_ICE_CTX_FLAGS_AGGRESSIVE_NOMINATION;

  r = nr_ice_ctx_create(const_cast<char *>(name.c_str()), flags,
                        &ctx->ctx_);
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't create ICE ctx for '" << name << "'");
    return nullptr;
  }

#ifdef USE_INTERFACE_PRIORITIZER
  nr_interface_prioritizer *prioritizer = CreateInterfacePrioritizer();
  if (!prioritizer) {
    MOZ_MTLOG(PR_LOG_ERROR, "Couldn't create interface prioritizer.");
    return nullptr;
  }

  r = nr_ice_ctx_set_interface_prioritizer(ctx->ctx_, prioritizer);
  if (r) {
    MOZ_MTLOG(PR_LOG_ERROR, "Couldn't set interface prioritizer.");
    return nullptr;
  }
#endif  // USE_INTERFACE_PRIORITIZER

  // Create the handler objects
  ctx->ice_handler_vtbl_ = new nr_ice_handler_vtbl();
  ctx->ice_handler_vtbl_->select_pair = &NrIceCtx::select_pair;
  ctx->ice_handler_vtbl_->stream_ready = &NrIceCtx::stream_ready;
  ctx->ice_handler_vtbl_->stream_failed = &NrIceCtx::stream_failed;
  ctx->ice_handler_vtbl_->ice_completed = &NrIceCtx::ice_completed;
  ctx->ice_handler_vtbl_->msg_recvd = &NrIceCtx::msg_recvd;

  ctx->ice_handler_ = new nr_ice_handler();
  ctx->ice_handler_->vtbl = ctx->ice_handler_vtbl_;
  ctx->ice_handler_->obj = ctx;

  // Create the peer ctx. Because we do not support parallel forking, we
  // only have one peer ctx.
  std::string peer_name = name + ":default";
  r = nr_ice_peer_ctx_create(ctx->ctx_, ctx->ice_handler_,
                             const_cast<char *>(peer_name.c_str()),
                             &ctx->peer_);
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't create ICE peer ctx for '" << name << "'");
    return nullptr;
  }

  nsresult rv;
  ctx->sts_target_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);

  if (!NS_SUCCEEDED(rv))
    return nullptr;

  return ctx;
}


NrIceCtx::~NrIceCtx() {
  MOZ_MTLOG(ML_DEBUG, "Destroying ICE ctx '" << name_ <<"'");
  nr_ice_peer_ctx_destroy(&peer_);
  nr_ice_ctx_destroy(&ctx_);
  delete ice_handler_vtbl_;
  delete ice_handler_;
}

RefPtr<NrIceMediaStream>
NrIceCtx::CreateStream(const std::string& name, int components) {
  RefPtr<NrIceMediaStream> stream =
    NrIceMediaStream::Create(this, name, components);

  streams_.push_back(stream);

  return stream;
}

void NrIceCtx::destroy_peer_ctx() {
  nr_ice_peer_ctx_destroy(&peer_);
}

nsresult NrIceCtx::SetControlling(Controlling controlling) {
  peer_->controlling = (controlling == ICE_CONTROLLING)? 1 : 0;

  MOZ_MTLOG(ML_DEBUG, "ICE ctx " << name_ << " setting controlling to" <<
            controlling);
  return NS_OK;
}

nsresult NrIceCtx::SetStunServers(const std::vector<NrIceStunServer>&
                                  stun_servers) {
  if (stun_servers.empty())
    return NS_OK;

  ScopedDeleteArray<nr_ice_stun_server> servers(
      new nr_ice_stun_server[stun_servers.size()]);

  for (size_t i=0; i < stun_servers.size(); ++i) {
    nsresult rv = stun_servers[i].ToNicerStunStruct(&servers[i]);
    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR, "Couldn't set STUN server for '" << name_ << "'");
      return NS_ERROR_FAILURE;
    }
  }

  int r = nr_ice_ctx_set_stun_servers(ctx_, servers, stun_servers.size());
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't set STUN server for '" << name_ << "'");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

// TODO(ekr@rtfm.com): This is just SetStunServers with s/Stun/Turn
// Could we do a template or something?
nsresult NrIceCtx::SetTurnServers(const std::vector<NrIceTurnServer>&
                                  turn_servers) {
  if (turn_servers.empty())
    return NS_OK;

  ScopedDeleteArray<nr_ice_turn_server> servers(
      new nr_ice_turn_server[turn_servers.size()]);

  for (size_t i=0; i < turn_servers.size(); ++i) {
    nsresult rv = turn_servers[i].ToNicerTurnStruct(&servers[i]);
    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR, "Couldn't set TURN server for '" << name_ << "'");
      return NS_ERROR_FAILURE;
    }
  }

  int r = nr_ice_ctx_set_turn_servers(ctx_, servers, turn_servers.size());
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't set TURN server for '" << name_ << "'");
    return NS_ERROR_FAILURE;
  }

  // TODO(ekr@rtfm.com): This leaks the username/password. Need to free that.

  return NS_OK;
}

nsresult NrIceCtx::SetResolver(nr_resolver *resolver) {
  int r = nr_ice_ctx_set_resolver(ctx_, resolver);

  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't set resolver for '" << name_ << "'");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult NrIceCtx::StartGathering() {
  MOZ_ASSERT(ctx_->state == ICE_CTX_INIT);
  if (ctx_->state != ICE_CTX_INIT) {
    MOZ_MTLOG(ML_ERROR, "ICE ctx in the wrong state for gathering: '"
              << name_ << "'");
    SetState(ICE_CTX_FAILED);
    return NS_ERROR_FAILURE;
  }

  int r = nr_ice_initialize(ctx_, &NrIceCtx::initialized_cb,
                            this);

  if (r && r != R_WOULDBLOCK) {
      MOZ_MTLOG(ML_ERROR, "Couldn't gather ICE candidates for '"
                << name_ << "'");
      SetState(ICE_CTX_FAILED);
      return NS_ERROR_FAILURE;
  }

  SetState(ICE_CTX_GATHERING);

  return NS_OK;
}

void NrIceCtx::EmitAllCandidates() {
  MOZ_MTLOG(ML_NOTICE, "Gathered all ICE candidates for '"
            << name_ << "'");

  for(size_t i=0; i<streams_.size(); ++i) {
    streams_[i]->EmitAllCandidates();
  }
}

RefPtr<NrIceMediaStream> NrIceCtx::FindStream(
    nr_ice_media_stream *stream) {
  for (size_t i=0; i<streams_.size(); ++i) {
    if (streams_[i]->stream() == stream) {
      return streams_[i];
    }
  }

  return nullptr;
}

std::vector<std::string> NrIceCtx::GetGlobalAttributes() {
  char **attrs = 0;
  int attrct;
  int r;
  std::vector<std::string> ret;

  r = nr_ice_get_global_attributes(ctx_, &attrs, &attrct);
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't get ufrag and password for '"
              << name_ << "'");
    return ret;
  }

  for (int i=0; i<attrct; i++) {
    ret.push_back(std::string(attrs[i]));
    RFREE(attrs[i]);
  }
  RFREE(attrs);

  return ret;
}

nsresult NrIceCtx::ParseGlobalAttributes(std::vector<std::string> attrs) {
  std::vector<char *> attrs_in;

  for (size_t i=0; i<attrs.size(); ++i) {
    attrs_in.push_back(const_cast<char *>(attrs[i].c_str()));
  }

  int r = nr_ice_peer_ctx_parse_global_attributes(peer_,
                                                  attrs_in.size() ?
                                                  &attrs_in[0] : nullptr,
                                                  attrs_in.size());
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't parse global attributes for "
              << name_ << "'");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult NrIceCtx::StartChecks() {
  int r;

  r=nr_ice_peer_ctx_pair_candidates(peer_);
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't pair candidates on "
              << name_ << "'");
    SetState(ICE_CTX_FAILED);
    return NS_ERROR_FAILURE;
  }

  r = nr_ice_peer_ctx_start_checks2(peer_,1);
  if (r) {
    if (r == R_NOT_FOUND) {
      MOZ_MTLOG(ML_ERROR, "Couldn't start peer checks on "
                << name_ << "' assuming trickle ICE");
    } else {
      MOZ_MTLOG(ML_ERROR, "Couldn't start peer checks on "
                << name_ << "'");
      SetState(ICE_CTX_FAILED);
      return NS_ERROR_FAILURE;
    }
  } else {
    SetState(ICE_CTX_CHECKING);
  }

  return NS_OK;
}


void NrIceCtx::initialized_cb(NR_SOCKET s, int h, void *arg) {
  NrIceCtx *ctx = static_cast<NrIceCtx *>(arg);

  ctx->EmitAllCandidates();

  ctx->SetState(ICE_CTX_GATHERED);
}

nsresult NrIceCtx::Finalize() {
  int r = nr_ice_ctx_finalize(ctx_, peer_);

  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't finalize "
         << name_ << "'");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void NrIceCtx::SetState(State state) {
  if (state == state_)
    return;

  MOZ_MTLOG(ML_DEBUG, "NrIceCtx(" << name_ << "): state " <<
            state_ << "->" << state);
  state_ = state;

  switch(state_) {
    case ICE_CTX_GATHERED:
      SignalGatheringCompleted(this);
      break;
    case ICE_CTX_OPEN:
      SignalCompleted(this);
      break;
    case ICE_CTX_FAILED:
      SignalFailed(this);
      break;
    default:
      break;
  }
}

}  // close namespace


extern "C" {
int nr_bin2hex(UCHAR *in,int len,UCHAR *out);
}

// Reimplement nr_ice_compute_codeword to avoid copyright issues
void nr_ice_compute_codeword(char *buf, int len,char *codeword) {
    UINT4 c;
    UCHAR cc[2];

    r_crc32(buf,len,&c);
    c %= 2048;

    cc[0] = (c >> 8) & 0xff;
    cc[1] = c & 0xff;

    nr_bin2hex(cc, 2, reinterpret_cast<UCHAR *>(codeword));

    return;
}

