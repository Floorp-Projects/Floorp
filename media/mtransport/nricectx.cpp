/* -*- mode: c++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nr_socket_proxy_config.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"

#include "logging.h"
#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"
#include "plbase64.h"

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsError.h"
#include "nsIEventTarget.h"
#include "nsIUUIDGenerator.h"
#include "nsNetCID.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "ScopedNSSTypes.h"
#include "runnable_utils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

// nICEr includes
extern "C" {
#include "nr_api.h"
#include "registry.h"
#include "async_timer.h"
#include "r_crc32.h"
#include "r_memory.h"
#include "ice_reg.h"
#include "ice_util.h"
#include "transport_addr.h"
#include "nr_crypto.h"
#include "nr_socket.h"
#include "nr_socket_local.h"
#include "stun_client_ctx.h"
#include "stun_reg.h"
#include "stun_server_ctx.h"
#include "stun_util.h"
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
#include "rlogconnector.h"
#include "test_nr_socket.h"

namespace mozilla {

using std::shared_ptr;

TimeStamp nr_socket_short_term_violation_time() {
  return NrSocketBase::short_term_violation_time();
}

TimeStamp nr_socket_long_term_violation_time() {
  return NrSocketBase::long_term_violation_time();
}

MOZ_MTLOG_MODULE("mtransport")

const char kNrIceTransportUdp[] = "udp";
const char kNrIceTransportTcp[] = "tcp";
const char kNrIceTransportTls[] = "tls";

static bool initialized = false;

static int noop(void** obj) { return 0; }

static nr_socket_factory_vtbl ctx_socket_factory_vtbl = {nr_socket_local_create,
                                                         noop};

// Implement NSPR-based crypto algorithms
static int nr_crypto_nss_random_bytes(UCHAR* buf, size_t len) {
  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) return R_INTERNAL;

  SECStatus rv = PK11_GenerateRandomOnSlot(slot.get(), buf, len);
  if (rv != SECSuccess) return R_INTERNAL;

  return 0;
}

static int nr_crypto_nss_hmac(UCHAR* key, size_t keyl, UCHAR* buf, size_t bufl,
                              UCHAR* result) {
  CK_MECHANISM_TYPE mech = CKM_SHA_1_HMAC;
  PK11SlotInfo* slot = nullptr;
  MOZ_ASSERT(keyl > 0);
  SECItem keyi = {siBuffer, key, static_cast<unsigned int>(keyl)};
  PK11SymKey* skey = nullptr;
  PK11Context* hmac_ctx = nullptr;
  SECStatus status;
  unsigned int hmac_len;
  SECItem param = {siBuffer, nullptr, 0};
  int err = R_INTERNAL;

  slot = PK11_GetInternalKeySlot();
  if (!slot) goto abort;

  skey = PK11_ImportSymKey(slot, mech, PK11_OriginUnwrap, CKA_SIGN, &keyi,
                           nullptr);
  if (!skey) goto abort;

  hmac_ctx = PK11_CreateContextBySymKey(mech, CKA_SIGN, skey, &param);
  if (!hmac_ctx) goto abort;

  status = PK11_DigestBegin(hmac_ctx);
  if (status != SECSuccess) goto abort;

  status = PK11_DigestOp(hmac_ctx, buf, bufl);
  if (status != SECSuccess) goto abort;

  status = PK11_DigestFinal(hmac_ctx, result, &hmac_len, 20);
  if (status != SECSuccess) goto abort;

  MOZ_ASSERT(hmac_len == 20);

  err = 0;

abort:
  if (hmac_ctx) PK11_DestroyContext(hmac_ctx, PR_TRUE);
  if (skey) PK11_FreeSymKey(skey);
  if (slot) PK11_FreeSlot(slot);

  return err;
}

static int nr_crypto_nss_md5(UCHAR* buf, size_t bufl, UCHAR* result) {
  int err = R_INTERNAL;
  SECStatus rv;

  const SECHashObject* ho = HASH_GetHashObject(HASH_AlgMD5);
  MOZ_ASSERT(ho);
  if (!ho) goto abort;

  MOZ_ASSERT(ho->length == 16);

  rv = HASH_HashBuf(ho->type, result, buf, bufl);
  if (rv != SECSuccess) goto abort;

  err = 0;
abort:
  return err;
}

static nr_ice_crypto_vtbl nr_ice_crypto_nss_vtbl = {
    nr_crypto_nss_random_bytes, nr_crypto_nss_hmac, nr_crypto_nss_md5};

nsresult NrIceStunServer::ToNicerStunStruct(nr_ice_stun_server* server) const {
  int r;

  memset(server, 0, sizeof(nr_ice_stun_server));
  if (transport_ == kNrIceTransportUdp) {
    server->transport = IPPROTO_UDP;
  } else if (transport_ == kNrIceTransportTcp) {
    server->transport = IPPROTO_TCP;
  } else if (transport_ == kNrIceTransportTls) {
    server->transport = IPPROTO_TCP;
    if (has_addr_) {
      // Refuse to try TLS without an FQDN
      return NS_ERROR_INVALID_ARG;
    }
    server->tls = 1;
  } else {
    MOZ_MTLOG(ML_ERROR, "Unsupported STUN server transport: " << transport_);
    return NS_ERROR_FAILURE;
  }

  if (has_addr_) {
    r = nr_praddr_to_transport_addr(&addr_, &server->u.addr, server->transport,
                                    0);
    if (r) {
      return NS_ERROR_FAILURE;
    }
    server->type = NR_ICE_STUN_SERVER_TYPE_ADDR;
  } else {
    MOZ_ASSERT(sizeof(server->u.dnsname.host) > host_.size());
    PL_strncpyz(server->u.dnsname.host, host_.c_str(),
                sizeof(server->u.dnsname.host));
    server->u.dnsname.port = port_;
    server->type = NR_ICE_STUN_SERVER_TYPE_DNSNAME;
  }

  return NS_OK;
}

nsresult NrIceTurnServer::ToNicerTurnStruct(nr_ice_turn_server* server) const {
  memset(server, 0, sizeof(nr_ice_turn_server));

  nsresult rv = ToNicerStunStruct(&server->turn_server);
  if (NS_FAILED(rv)) return rv;

  if (!(server->username = r_strdup(username_.c_str())))
    return NS_ERROR_OUT_OF_MEMORY;

  // TODO(ekr@rtfm.com): handle non-ASCII passwords somehow?
  // STUN requires they be SASLpreped, but we don't know if
  // they are at this point.

  // C++03 23.2.4, Paragraph 1 stipulates that the elements
  // in std::vector must be contiguous, and can therefore be
  // used as input to functions expecting C arrays.
  int r = r_data_create(&server->password, const_cast<UCHAR*>(&password_[0]),
                        password_.size());
  if (r) {
    RFREE(server->username);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NrIceCtx::NrIceCtx(const std::string& name, Policy policy)
    : connection_state_(ICE_CTX_INIT),
      gathering_state_(ICE_CTX_GATHER_INIT),
      name_(name),
      ice_controlling_set_(false),
      streams_(),
      ctx_(nullptr),
      peer_(nullptr),
      ice_handler_vtbl_(nullptr),
      ice_handler_(nullptr),
      trickle_(true),
      policy_(policy),
      nat_(nullptr),
      proxy_config_(nullptr),
      proxy_only_(false) {}

/* static */
RefPtr<NrIceCtx> NrIceCtx::Create(const std::string& name, bool allow_loopback,
                                  bool tcp_enabled, bool allow_link_local,
                                  Policy policy) {
  // InitializeGlobals only executes once
  NrIceCtx::InitializeGlobals(allow_loopback, tcp_enabled, allow_link_local);

  RefPtr<NrIceCtx> ctx = new NrIceCtx(name, policy);

  if (!ctx->Initialize()) {
    return nullptr;
  }

  return ctx;
}

RefPtr<NrIceMediaStream> NrIceCtx::CreateStream(const std::string& id,
                                                const std::string& name,
                                                int components) {
  if (streams_.count(id)) {
    MOZ_ASSERT(false);
    return nullptr;
  }

  RefPtr<NrIceMediaStream> stream =
      new NrIceMediaStream(this, id, name, components);
  streams_[id] = stream;
  return stream;
}

void NrIceCtx::DestroyStream(const std::string& id) {
  auto it = streams_.find(id);
  if (it != streams_.end()) {
    auto preexisting_stream = it->second;
    streams_.erase(it);
    preexisting_stream->Close();
  }
}

// Handler callbacks
int NrIceCtx::select_pair(void* obj, nr_ice_media_stream* stream,
                          int component_id, nr_ice_cand_pair** potentials,
                          int potential_ct) {
  MOZ_MTLOG(ML_DEBUG, "select pair called: potential_ct = " << potential_ct);
  MOZ_ASSERT(stream->local_stream);
  MOZ_ASSERT(!stream->local_stream->obsolete);

  return 0;
}

int NrIceCtx::stream_ready(void* obj, nr_ice_media_stream* stream) {
  MOZ_MTLOG(ML_DEBUG, "stream_ready called");
  MOZ_ASSERT(!stream->local_stream);
  MOZ_ASSERT(!stream->obsolete);

  // Get the ICE ctx.
  NrIceCtx* ctx = static_cast<NrIceCtx*>(obj);

  RefPtr<NrIceMediaStream> s = ctx->FindStream(stream);

  // Streams which do not exist should never be ready.
  MOZ_ASSERT(s);

  s->Ready();

  return 0;
}

int NrIceCtx::stream_failed(void* obj, nr_ice_media_stream* stream) {
  MOZ_MTLOG(ML_DEBUG, "stream_failed called");
  MOZ_ASSERT(!stream->local_stream);
  MOZ_ASSERT(!stream->obsolete);

  // Get the ICE ctx
  NrIceCtx* ctx = static_cast<NrIceCtx*>(obj);
  RefPtr<NrIceMediaStream> s = ctx->FindStream(stream);

  // Streams which do not exist should never fail.
  MOZ_ASSERT(s);

  ctx->SetConnectionState(ICE_CTX_FAILED);
  s->Failed();
  return 0;
}

int NrIceCtx::ice_checking(void* obj, nr_ice_peer_ctx* pctx) {
  MOZ_MTLOG(ML_DEBUG, "ice_checking called");

  // Get the ICE ctx
  NrIceCtx* ctx = static_cast<NrIceCtx*>(obj);

  ctx->SetConnectionState(ICE_CTX_CHECKING);

  return 0;
}

int NrIceCtx::ice_connected(void* obj, nr_ice_peer_ctx* pctx) {
  MOZ_MTLOG(ML_DEBUG, "ice_connected called");

  // Get the ICE ctx
  NrIceCtx* ctx = static_cast<NrIceCtx*>(obj);

  // This is called even on failed contexts.
  if (ctx->connection_state() != ICE_CTX_FAILED) {
    ctx->SetConnectionState(ICE_CTX_CONNECTED);
  }

  return 0;
}

int NrIceCtx::ice_disconnected(void* obj, nr_ice_peer_ctx* pctx) {
  MOZ_MTLOG(ML_DEBUG, "ice_disconnected called");

  // Get the ICE ctx
  NrIceCtx* ctx = static_cast<NrIceCtx*>(obj);

  ctx->SetConnectionState(ICE_CTX_DISCONNECTED);

  return 0;
}

int NrIceCtx::msg_recvd(void* obj, nr_ice_peer_ctx* pctx,
                        nr_ice_media_stream* stream, int component_id,
                        UCHAR* msg, int len) {
  // Get the ICE ctx
  NrIceCtx* ctx = static_cast<NrIceCtx*>(obj);
  RefPtr<NrIceMediaStream> s = ctx->FindStream(stream);

  // Streams which do not exist should never have packets.
  MOZ_ASSERT(s);

  s->SignalPacketReceived(s, component_id, msg, len);

  return 0;
}

void NrIceCtx::trickle_cb(void* arg, nr_ice_ctx* ice_ctx,
                          nr_ice_media_stream* stream, int component_id,
                          nr_ice_candidate* candidate) {
  MOZ_ASSERT(!stream->obsolete);
  // Get the ICE ctx
  NrIceCtx* ctx = static_cast<NrIceCtx*>(arg);
  RefPtr<NrIceMediaStream> s = ctx->FindStream(stream);

  if (!s) {
    // This stream has been removed because it is inactive
    return;
  }

  if (!candidate) {
    s->SignalCandidate(s, "", stream->ufrag, "", "");
    return;
  }

  std::string actual_addr;
  std::string mdns_addr;
  ctx->GenerateObfuscatedAddress(candidate, &mdns_addr, &actual_addr);

  // Format the candidate.
  char candidate_str[NR_ICE_MAX_ATTRIBUTE_SIZE];
  int r = nr_ice_format_candidate_attribute(candidate, candidate_str,
                                            sizeof(candidate_str));
  MOZ_ASSERT(!r);
  if (r) return;

  MOZ_MTLOG(ML_INFO, "NrIceCtx(" << ctx->name_ << "): trickling candidate "
                                 << candidate_str);

  s->SignalCandidate(s, candidate_str, stream->ufrag, mdns_addr, actual_addr);
}

void NrIceCtx::InitializeGlobals(bool allow_loopback, bool tcp_enabled,
                                 bool allow_link_local) {
  RLogConnector::CreateInstance();
  // Initialize the crypto callbacks and logging stuff
  if (!initialized) {
    NR_reg_init(NR_REG_MODE_LOCAL);
    nr_crypto_vtbl = &nr_ice_crypto_nss_vtbl;
    initialized = true;

    // Set the priorites for candidate type preferences.
    // These numbers come from RFC 5245 S. 4.1.2.2
    NR_reg_set_uchar((char*)NR_ICE_REG_PREF_TYPE_SRV_RFLX, 100);
    NR_reg_set_uchar((char*)NR_ICE_REG_PREF_TYPE_PEER_RFLX, 110);
    NR_reg_set_uchar((char*)NR_ICE_REG_PREF_TYPE_HOST, 126);
    NR_reg_set_uchar((char*)NR_ICE_REG_PREF_TYPE_RELAYED, 5);
    NR_reg_set_uchar((char*)NR_ICE_REG_PREF_TYPE_SRV_RFLX_TCP, 99);
    NR_reg_set_uchar((char*)NR_ICE_REG_PREF_TYPE_PEER_RFLX_TCP, 109);
    NR_reg_set_uchar((char*)NR_ICE_REG_PREF_TYPE_HOST_TCP, 125);
    NR_reg_set_uchar((char*)NR_ICE_REG_PREF_TYPE_RELAYED_TCP, 0);

    int32_t stun_client_maximum_transmits = 7;
    int32_t ice_trickle_grace_period = 5000;
    int32_t ice_tcp_so_sock_count = 3;
    int32_t ice_tcp_listen_backlog = 10;
    nsAutoCString force_net_interface;
    nsresult res;
    nsCOMPtr<nsIPrefService> prefs =
        do_GetService("@mozilla.org/preferences-service;1", &res);

    if (NS_SUCCEEDED(res)) {
      nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);
      if (branch) {
        branch->GetIntPref(
            "media.peerconnection.ice.stun_client_maximum_transmits",
            &stun_client_maximum_transmits);
        branch->GetIntPref("media.peerconnection.ice.trickle_grace_period",
                           &ice_trickle_grace_period);
        branch->GetIntPref("media.peerconnection.ice.tcp_so_sock_count",
                           &ice_tcp_so_sock_count);
        branch->GetIntPref("media.peerconnection.ice.tcp_listen_backlog",
                           &ice_tcp_listen_backlog);
        branch->GetCharPref("media.peerconnection.ice.force_interface",
                            force_net_interface);
      }
    }

    NR_reg_set_uint4((char*)"stun.client.maximum_transmits",
                     stun_client_maximum_transmits);
    NR_reg_set_uint4((char*)NR_ICE_REG_TRICKLE_GRACE_PERIOD,
                     ice_trickle_grace_period);
    NR_reg_set_int4((char*)NR_ICE_REG_ICE_TCP_SO_SOCK_COUNT,
                    ice_tcp_so_sock_count);
    NR_reg_set_int4((char*)NR_ICE_REG_ICE_TCP_LISTEN_BACKLOG,
                    ice_tcp_listen_backlog);

    NR_reg_set_char((char*)NR_ICE_REG_ICE_TCP_DISABLE, !tcp_enabled);

    if (allow_loopback) {
      NR_reg_set_char((char*)NR_STUN_REG_PREF_ALLOW_LOOPBACK_ADDRS, 1);
    }

    if (allow_link_local) {
      NR_reg_set_char((char*)NR_STUN_REG_PREF_ALLOW_LINK_LOCAL_ADDRS, 1);
    }
    if (force_net_interface.Length() > 0) {
      // Stupid cast.... but needed
      const nsCString& flat =
          PromiseFlatCString(static_cast<nsACString&>(force_net_interface));
      NR_reg_set_string((char*)NR_ICE_REG_PREF_FORCE_INTERFACE_NAME,
                        const_cast<char*>(flat.get()));
    }
  }
}

void NrIceCtx::SetTargetForDefaultLocalAddressLookup(
    const std::string& target_ip, uint16_t target_port) {
  nr_ice_set_target_for_default_local_address_lookup(ctx_, target_ip.c_str(),
                                                     target_port);
}

#define MAXADDRS 100  // mirrors setting in ice_ctx.c

/* static */
nsTArray<NrIceStunAddr> NrIceCtx::GetStunAddrs() {
  nsTArray<NrIceStunAddr> addrs;

  nr_local_addr local_addrs[MAXADDRS];
  int addr_ct = 0;

  // most likely running on parent process and need crypto vtbl
  // initialized on Windows (Linux and OSX don't seem to care)
  if (!initialized) {
    nr_crypto_vtbl = &nr_ice_crypto_nss_vtbl;
  }

  MOZ_MTLOG(ML_INFO, "NrIceCtx static call to find local stun addresses");
  if (nr_stun_find_local_addresses(local_addrs, MAXADDRS, &addr_ct)) {
    MOZ_MTLOG(ML_INFO, "Error finding local stun addresses");
  } else {
    for (int i = 0; i < addr_ct; ++i) {
      NrIceStunAddr addr(&local_addrs[i]);
      addrs.AppendElement(addr);
    }
  }

  return addrs;
}

void NrIceCtx::SetStunAddrs(const nsTArray<NrIceStunAddr>& addrs) {
  nr_local_addr* local_addrs;
  local_addrs = new nr_local_addr[addrs.Length()];

  for (size_t i = 0; i < addrs.Length(); ++i) {
    nr_local_addr_copy(&local_addrs[i],
                       const_cast<nr_local_addr*>(&addrs[i].localAddr()));
  }
  nr_ice_set_local_addresses(ctx_, local_addrs, addrs.Length());

  delete[] local_addrs;
}

bool NrIceCtx::Initialize() {
  // Create the ICE context
  int r;

  UINT4 flags = NR_ICE_CTX_FLAGS_AGGRESSIVE_NOMINATION;
  switch (policy_) {
    case ICE_POLICY_RELAY:
      flags |= NR_ICE_CTX_FLAGS_RELAY_ONLY;
      break;
    case ICE_POLICY_NO_HOST:
      flags |= NR_ICE_CTX_FLAGS_HIDE_HOST_CANDIDATES;
      break;
    case ICE_POLICY_ALL:
      break;
  }

  r = nr_ice_ctx_create(const_cast<char*>(name_.c_str()), flags, &ctx_);

  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't create ICE ctx for '" << name_ << "'");
    return false;
  }

  // override default factory to capture optional proxy config when creating
  // sockets.
  nr_socket_factory* factory;
  r = nr_socket_factory_create_int(this, &ctx_socket_factory_vtbl, &factory);

  if (r) {
    MOZ_MTLOG(LogLevel::Error, "Couldn't create ctx socket factory.");
    return false;
  }
  nr_ice_ctx_set_socket_factory(ctx_, factory);

  nr_interface_prioritizer* prioritizer = CreateInterfacePrioritizer();
  if (!prioritizer) {
    MOZ_MTLOG(LogLevel::Error, "Couldn't create interface prioritizer.");
    return false;
  }

  r = nr_ice_ctx_set_interface_prioritizer(ctx_, prioritizer);
  if (r) {
    MOZ_MTLOG(LogLevel::Error, "Couldn't set interface prioritizer.");
    return false;
  }

  if (generating_trickle()) {
    r = nr_ice_ctx_set_trickle_cb(ctx_, &NrIceCtx::trickle_cb, this);
    if (r) {
      MOZ_MTLOG(ML_ERROR, "Couldn't set trickle cb for '" << name_ << "'");
      return false;
    }
  }

  nsAutoCString mapping_type;
  nsAutoCString filtering_type;
  bool block_udp = false;
  bool block_tcp = false;

  nsresult rv;
  nsCOMPtr<nsIPrefService> pref_service =
      do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIPrefBranch> pref_branch;
    rv = pref_service->GetBranch(nullptr, getter_AddRefs(pref_branch));
    if (NS_SUCCEEDED(rv)) {
      Unused << pref_branch->GetCharPref(
          "media.peerconnection.nat_simulator.mapping_type", mapping_type);
      Unused << pref_branch->GetCharPref(
          "media.peerconnection.nat_simulator.filtering_type", filtering_type);
      Unused << pref_branch->GetBoolPref(
          "media.peerconnection.nat_simulator.block_udp", &block_udp);
      Unused << pref_branch->GetBoolPref(
          "media.peerconnection.nat_simulator.block_tcp", &block_tcp);
    }
  }

  if (!mapping_type.IsEmpty() && !filtering_type.IsEmpty()) {
    MOZ_MTLOG(ML_DEBUG, "NAT filtering type: " << filtering_type.get());
    MOZ_MTLOG(ML_DEBUG, "NAT mapping type: " << mapping_type.get());
    TestNat* test_nat = new TestNat;
    test_nat->filtering_type_ = TestNat::ToNatBehavior(filtering_type.get());
    test_nat->mapping_type_ = TestNat::ToNatBehavior(mapping_type.get());
    test_nat->block_udp_ = block_udp;
    test_nat->block_tcp_ = block_tcp;
    test_nat->enabled_ = true;
    SetNat(test_nat);
  }

  // Create the handler objects
  ice_handler_vtbl_ = new nr_ice_handler_vtbl();
  ice_handler_vtbl_->select_pair = &NrIceCtx::select_pair;
  ice_handler_vtbl_->stream_ready = &NrIceCtx::stream_ready;
  ice_handler_vtbl_->stream_failed = &NrIceCtx::stream_failed;
  ice_handler_vtbl_->ice_connected = &NrIceCtx::ice_connected;
  ice_handler_vtbl_->msg_recvd = &NrIceCtx::msg_recvd;
  ice_handler_vtbl_->ice_checking = &NrIceCtx::ice_checking;
  ice_handler_vtbl_->ice_disconnected = &NrIceCtx::ice_disconnected;

  ice_handler_ = new nr_ice_handler();
  ice_handler_->vtbl = ice_handler_vtbl_;
  ice_handler_->obj = this;

  // Create the peer ctx. Because we do not support parallel forking, we
  // only have one peer ctx.
  std::string peer_name = name_ + ":default";
  r = nr_ice_peer_ctx_create(ctx_, ice_handler_,
                             const_cast<char*>(peer_name.c_str()), &peer_);
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't create ICE peer ctx for '" << name_ << "'");
    return false;
  }

  sts_target_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);

  if (!NS_SUCCEEDED(rv)) return false;

  return true;
}

int NrIceCtx::SetNat(const RefPtr<TestNat>& aNat) {
  nat_ = aNat;
  nr_socket_factory* fac;
  int r = nat_->create_socket_factory(&fac);
  if (r) {
    return r;
  }
  nr_ice_ctx_set_socket_factory(ctx_, fac);
  return 0;
}

// ONLY USE THIS FOR TESTING. Will cause totally unpredictable and possibly very
// bad effects if ICE is still live.
void NrIceCtx::internal_DeinitializeGlobal() {
  NR_reg_del((char*)"stun");
  NR_reg_del((char*)"ice");
  RLogConnector::DestroyInstance();
  nr_crypto_vtbl = nullptr;
  initialized = false;
}

void NrIceCtx::internal_SetTimerAccelarator(int divider) {
  ctx_->test_timer_divider = divider;
}

void NrIceCtx::AccumulateStats(const NrIceStats& stats) {
  nr_accumulate_count(&(ctx_->stats.stun_retransmits), stats.stun_retransmits);
  nr_accumulate_count(&(ctx_->stats.turn_401s), stats.turn_401s);
  nr_accumulate_count(&(ctx_->stats.turn_403s), stats.turn_403s);
  nr_accumulate_count(&(ctx_->stats.turn_438s), stats.turn_438s);
}

NrIceStats NrIceCtx::Destroy() {
  // designed to be called more than once so if stats are desired, this can be
  // called just prior to the destructor
  MOZ_MTLOG(ML_DEBUG, "Destroying ICE ctx '" << name_ << "'");
  for (auto& idAndStream : streams_) {
    idAndStream.second->Close();
  }

  NrIceStats stats;
  if (ctx_) {
    stats.stun_retransmits = ctx_->stats.stun_retransmits;
    stats.turn_401s = ctx_->stats.turn_401s;
    stats.turn_403s = ctx_->stats.turn_403s;
    stats.turn_438s = ctx_->stats.turn_438s;
  }

  if (peer_) {
    nr_ice_peer_ctx_destroy(&peer_);
  }
  if (ctx_) {
    nr_ice_ctx_destroy(&ctx_);
  }

  delete ice_handler_vtbl_;
  delete ice_handler_;

  ice_handler_vtbl_ = nullptr;
  ice_handler_ = nullptr;
  proxy_config_ = nullptr;
  streams_.clear();

  return stats;
}

NrIceCtx::~NrIceCtx() {}

void NrIceCtx::destroy_peer_ctx() { nr_ice_peer_ctx_destroy(&peer_); }

nsresult NrIceCtx::SetControlling(Controlling controlling) {
  if (!ice_controlling_set_) {
    peer_->controlling = (controlling == ICE_CONTROLLING) ? 1 : 0;
    ice_controlling_set_ = true;

    MOZ_MTLOG(ML_DEBUG,
              "ICE ctx " << name_ << " setting controlling to" << controlling);
  }
  return NS_OK;
}

NrIceCtx::Controlling NrIceCtx::GetControlling() {
  return (peer_->controlling) ? ICE_CONTROLLING : ICE_CONTROLLED;
}

nsresult NrIceCtx::SetPolicy(Policy policy) {
  policy_ = policy;
  return NS_OK;
}

nsresult NrIceCtx::SetStunServers(
    const std::vector<NrIceStunServer>& stun_servers) {
  if (stun_servers.empty()) return NS_OK;

  auto servers = MakeUnique<nr_ice_stun_server[]>(stun_servers.size());

  for (size_t i = 0; i < stun_servers.size(); ++i) {
    nsresult rv = stun_servers[i].ToNicerStunStruct(&servers[i]);
    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR, "Couldn't set STUN server for '" << name_ << "'");
      return NS_ERROR_FAILURE;
    }
  }

  int r = nr_ice_ctx_set_stun_servers(ctx_, servers.get(), stun_servers.size());
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't set STUN server for '" << name_ << "'");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

// TODO(ekr@rtfm.com): This is just SetStunServers with s/Stun/Turn
// Could we do a template or something?
nsresult NrIceCtx::SetTurnServers(
    const std::vector<NrIceTurnServer>& turn_servers) {
  if (turn_servers.empty()) return NS_OK;

  auto servers = MakeUnique<nr_ice_turn_server[]>(turn_servers.size());

  for (size_t i = 0; i < turn_servers.size(); ++i) {
    nsresult rv = turn_servers[i].ToNicerTurnStruct(&servers[i]);
    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR, "Couldn't set TURN server for '" << name_ << "'");
      return NS_ERROR_FAILURE;
    }
  }

  int r = nr_ice_ctx_set_turn_servers(ctx_, servers.get(), turn_servers.size());
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't set TURN server for '" << name_ << "'");
    return NS_ERROR_FAILURE;
  }

  // TODO(ekr@rtfm.com): This leaks the username/password. Need to free that.

  return NS_OK;
}

nsresult NrIceCtx::SetResolver(nr_resolver* resolver) {
  int r = nr_ice_ctx_set_resolver(ctx_, resolver);

  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't set resolver for '" << name_ << "'");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult NrIceCtx::SetProxyServer(NrSocketProxyConfig&& config) {
  proxy_config_.reset(new NrSocketProxyConfig(std::move(config)));
  return NS_OK;
}

void NrIceCtx::SetCtxFlags(bool default_route_only, bool proxy_only) {
  ASSERT_ON_THREAD(sts_target_);

  if (default_route_only) {
    nr_ice_ctx_add_flags(ctx_, NR_ICE_CTX_FLAGS_ONLY_DEFAULT_ADDRS);
  } else {
    nr_ice_ctx_remove_flags(ctx_, NR_ICE_CTX_FLAGS_ONLY_DEFAULT_ADDRS);
  }

  if (proxy_only) {
    nr_ice_ctx_add_flags(ctx_, NR_ICE_CTX_FLAGS_ONLY_PROXY);
  } else {
    nr_ice_ctx_remove_flags(ctx_, NR_ICE_CTX_FLAGS_ONLY_PROXY);
  }
}

nsresult NrIceCtx::StartGathering(bool default_route_only, bool proxy_only) {
  ASSERT_ON_THREAD(sts_target_);

  SetGatheringState(ICE_CTX_GATHER_STARTED);

  SetCtxFlags(default_route_only, proxy_only);

  proxy_only_ = proxy_only;

  // This might start gathering for the first time, or again after
  // renegotiation, or might do nothing at all if gathering has already
  // finished.
  int r = nr_ice_gather(ctx_, &NrIceCtx::gather_cb, this);

  if (!r) {
    SetGatheringState(ICE_CTX_GATHER_COMPLETE);
  } else if (r != R_WOULDBLOCK) {
    MOZ_MTLOG(ML_ERROR, "ICE FAILED: Couldn't gather ICE candidates for '"
                            << name_ << "', error=" << r);
    SetConnectionState(ICE_CTX_FAILED);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

RefPtr<NrIceMediaStream> NrIceCtx::FindStream(nr_ice_media_stream* stream) {
  for (auto& idAndStream : streams_) {
    if (idAndStream.second->HasStream(stream)) {
      return idAndStream.second;
    }
  }

  return nullptr;
}

std::vector<std::string> NrIceCtx::GetGlobalAttributes() {
  char** attrs = nullptr;
  int attrct;
  int r;
  std::vector<std::string> ret;

  r = nr_ice_get_global_attributes(ctx_, &attrs, &attrct);
  if (r) {
    MOZ_MTLOG(ML_ERROR,
              "Couldn't get ufrag and password for '" << name_ << "'");
    return ret;
  }

  for (int i = 0; i < attrct; i++) {
    ret.push_back(std::string(attrs[i]));
    RFREE(attrs[i]);
  }
  RFREE(attrs);

  return ret;
}

nsresult NrIceCtx::ParseGlobalAttributes(std::vector<std::string> attrs) {
  std::vector<char*> attrs_in;
  attrs_in.reserve(attrs.size());
  for (auto& attr : attrs) {
    attrs_in.push_back(const_cast<char*>(attr.c_str()));
  }

  int r = nr_ice_peer_ctx_parse_global_attributes(
      peer_, attrs_in.empty() ? nullptr : &attrs_in[0], attrs_in.size());
  if (r) {
    MOZ_MTLOG(ML_ERROR,
              "Couldn't parse global attributes for " << name_ << "'");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

bool NrIceCtx::HasStreamsToConnect() const {
  for (auto& idAndStream : streams_) {
    if (idAndStream.second->state() != NrIceMediaStream::ICE_CLOSED) {
      return true;
    }
  }
  return false;
}

nsresult NrIceCtx::StartChecks() {
  int r;
  if (!HasStreamsToConnect()) {
    // Nothing to do
    return NS_OK;
  }

  r = nr_ice_peer_ctx_pair_candidates(peer_);
  if (r) {
    MOZ_MTLOG(ML_ERROR, "ICE FAILED: Couldn't pair candidates on " << name_);
    SetConnectionState(ICE_CTX_FAILED);
    return NS_ERROR_FAILURE;
  }

  r = nr_ice_peer_ctx_start_checks2(peer_, 1);
  if (r) {
    if (r == R_NOT_FOUND) {
      MOZ_MTLOG(ML_INFO, "Couldn't start peer checks on "
                             << name_ << ", assuming trickle ICE");
    } else {
      MOZ_MTLOG(ML_ERROR,
                "ICE FAILED: Couldn't start peer checks on " << name_);
      SetConnectionState(ICE_CTX_FAILED);
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

void NrIceCtx::gather_cb(NR_SOCKET s, int h, void* arg) {
  NrIceCtx* ctx = static_cast<NrIceCtx*>(arg);

  ctx->SetGatheringState(ICE_CTX_GATHER_COMPLETE);
}

nsresult NrIceCtx::Finalize() {
  int r = nr_ice_ctx_finalize(ctx_, peer_);

  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't finalize " << name_ << "'");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void NrIceCtx::UpdateNetworkState(bool online) {
  MOZ_MTLOG(ML_INFO, "NrIceCtx(" << name_ << "): updating network state to "
                                 << (online ? "online" : "offline"));
  if (connection_state_ == ICE_CTX_CLOSED) {
    return;
  }

  if (online) {
    nr_ice_peer_ctx_refresh_consent_all_streams(peer_);
  } else {
    nr_ice_peer_ctx_disconnect_all_streams(peer_);
  }
}

void NrIceCtx::SetConnectionState(ConnectionState state) {
  if (state == connection_state_) return;

  MOZ_MTLOG(ML_INFO, "NrIceCtx(" << name_ << "): state " << connection_state_
                                 << "->" << state);
  connection_state_ = state;

  if (connection_state_ == ICE_CTX_FAILED) {
    MOZ_MTLOG(ML_INFO,
              "NrIceCtx(" << name_ << "): dumping r_log ringbuffer... ");
    std::deque<std::string> logs;
    RLogConnector::GetInstance()->GetAny(0, &logs);
    for (auto& log : logs) {
      MOZ_MTLOG(ML_INFO, log);
    }
  }

  SignalConnectionStateChange(this, state);
}

void NrIceCtx::SetGatheringState(GatheringState state) {
  if (state == gathering_state_) return;

  MOZ_MTLOG(ML_DEBUG, "NrIceCtx(" << name_ << "): gathering state "
                                  << gathering_state_ << "->" << state);
  gathering_state_ = state;

  SignalGatheringStateChange(this, state);
}

void NrIceCtx::GenerateObfuscatedAddress(nr_ice_candidate* candidate,
                                         std::string* mdns_address,
                                         std::string* actual_address) {
  if (candidate->type == HOST) {
    int r;
    char addr[64];
    if ((r = nr_transport_addr_get_addrstring(&candidate->addr, addr,
                                              sizeof(addr)))) {
      return;
    }

    *actual_address = addr;

    const auto& iter = obfuscated_host_addresses_.find(*actual_address);
    if (iter != obfuscated_host_addresses_.end()) {
      *mdns_address = iter->second;
    } else {
      nsresult rv;
      nsCOMPtr<nsIUUIDGenerator> uuidgen =
          do_GetService("@mozilla.org/uuid-generator;1", &rv);

      if (NS_FAILED(rv)) {
        return;
      }

      nsID id;
      rv = uuidgen->GenerateUUIDInPlace(&id);

      if (NS_FAILED(rv)) {
        return;
      }

      char chars[NSID_LENGTH];
      id.ToProvidedString(chars);

      std::ostringstream o;
      chars[NSID_LENGTH - 2] = 0;  // trim trailing } from uuid
      o << &chars[1] << ".local";  // trim leading { from uuid
      *mdns_address = o.str();

      obfuscated_host_addresses_[*actual_address] = *mdns_address;
    }
    candidate->mdns_addr = r_strdup(mdns_address->c_str());
  }
}

}  // namespace mozilla

// Reimplement nr_ice_compute_codeword to avoid copyright issues
void nr_ice_compute_codeword(char* buf, int len, char* codeword) {
  UINT4 c;

  r_crc32(buf, len, &c);

  PL_Base64Encode(reinterpret_cast<char*>(&c), 3, codeword);
  codeword[4] = 0;
}

int nr_socket_local_create(void* obj, nr_transport_addr* addr,
                           nr_socket** sockp) {
  using namespace mozilla;

  RefPtr<NrSocketBase> sock;
  int r, _status;
  shared_ptr<NrSocketProxyConfig> config = nullptr;

  if (obj) {
    config = static_cast<NrIceCtx*>(obj)->GetProxyConfig();
    bool ctx_proxy_only = static_cast<NrIceCtx*>(obj)->proxy_only();

    if (ctx_proxy_only && !config) {
      ABORT(R_FAILED);
    }
  }

  r = NrSocketBase::CreateSocket(addr, &sock, config);
  if (r) {
    ABORT(r);
  }
  // TODO(bug 1569183): This will start out false, and may become true once the
  // socket class figures out whether a proxy needs to be used (this may be as
  // late as when it establishes a connection).
  addr->is_proxied = sock->IsProxied();

  r = nr_socket_create_int(static_cast<void*>(sock), sock->vtbl(), sockp);
  if (r) ABORT(r);

  _status = 0;

  {
    // We will release this reference in destroy(), not exactly the normal
    // ownership model, but it is what it is.
    NrSocketBase* dummy = sock.forget().take();
    (void)dummy;
  }

abort:
  return _status;
}
