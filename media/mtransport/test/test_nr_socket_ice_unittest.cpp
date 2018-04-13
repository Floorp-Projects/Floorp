/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Some of this code is taken from nricectx.cpp and nricemediastream.cpp
// which in turn contains code cut-and-pasted from nICEr. Copyright is:

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

#include "gtest/gtest.h"
#include "gtest_utils.h"
#include "nss.h"
#include "ssl.h"

extern "C" {
#include "stun_msg.h"
#include "ice_ctx.h"
#include "ice_peer_ctx.h"
#include "nICEr/src/net/transport_addr.h"
}

#include "mtransport_test_utils.h"
#include "nricectx.h"
#include "nricemediastream.h"
#include "runnable_utils.h"
#include "test_nr_socket.h"

namespace mozilla {

static unsigned int kDefaultTimeout = 7000;

class IcePeer {

public:
  IcePeer(const char* name, TestNat* nat, UINT4 flags,
          MtransportTestUtils* test_utils)
    : name_(name)
    , ice_checking_(false)
    , ice_connected_(false)
    , ice_disconnected_(false)
    , gather_cb_(false)
    , stream_ready_(false)
    , stream_failed_(false)
    , ice_ctx_(nullptr)
    , peer_ctx_(nullptr)
    , nat_(nat)
    , test_utils_(test_utils)
  {
    nr_ice_ctx_create(const_cast<char *>(name_.c_str()), flags, &ice_ctx_);

    if (nat_) {
      nr_socket_factory* factory;
      nat_->create_socket_factory(&factory);
      nr_ice_ctx_set_socket_factory(ice_ctx_, factory);
    }

    // Create the handler objects
    ice_handler_vtbl_ = new nr_ice_handler_vtbl();
    ice_handler_vtbl_->select_pair = &IcePeer::select_pair;
    ice_handler_vtbl_->stream_ready = &IcePeer::stream_ready;
    ice_handler_vtbl_->stream_failed = &IcePeer::stream_failed;
    ice_handler_vtbl_->ice_connected = &IcePeer::ice_connected;
    ice_handler_vtbl_->msg_recvd = &IcePeer::msg_recvd;
    ice_handler_vtbl_->ice_checking = &IcePeer::ice_checking;
    ice_handler_vtbl_->ice_disconnected = &IcePeer::ice_disconnected;

    ice_handler_ = new nr_ice_handler();
    ice_handler_->vtbl = ice_handler_vtbl_;
    ice_handler_->obj = this;

    nr_ice_peer_ctx_create(ice_ctx_, ice_handler_,
                           const_cast<char *>(name_.c_str()),
                           &peer_ctx_);

    nr_ice_add_media_stream(ice_ctx_,
                            const_cast<char *>(name_.c_str()),
                            2, &ice_media_stream_);

    nr_ice_media_stream_initialize(ice_ctx_, ice_media_stream_);
  }

  virtual ~IcePeer()
  {
    Destroy();
  }

  void Destroy()
  {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(this,
                     &IcePeer::Destroy_s),
        NS_DISPATCH_SYNC);
  }

  void Destroy_s()
  {
    nr_ice_peer_ctx_destroy(&peer_ctx_);
    delete ice_handler_;
    delete ice_handler_vtbl_;
    nr_ice_ctx_destroy(&ice_ctx_);
  }

  void Gather(bool default_route_only=false)
  {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(this,
                     &IcePeer::Gather_s, default_route_only),
        NS_DISPATCH_SYNC);
  }

  void Gather_s(bool default_route_only=false)
  {
    int r = nr_ice_gather(ice_ctx_, &IcePeer::gather_cb, this);
    ASSERT_TRUE(r == 0 || r == R_WOULDBLOCK);
  }

  std::vector<std::string> GetLocalCandidates() const {
    char attr[256];
    std::vector<std::string> candidates;
    nr_ice_component* comp = STAILQ_FIRST(&ice_media_stream_->components);
    while(comp){
      if (comp->state != NR_ICE_COMPONENT_DISABLED) {
        nr_ice_candidate *cand = TAILQ_FIRST(&comp->candidates);
        while(cand){
          int r = nr_ice_format_candidate_attribute(cand, attr, 255);
          if (r == 0) {
            candidates.push_back(attr);
          }

          cand = TAILQ_NEXT(cand, entry_comp);
        }
      }

      comp = STAILQ_NEXT(comp, entry);
    }

    return candidates;
  }

  std::vector<std::string> GetGlobalAttributes() {

    char **attrs = nullptr;
    int attrct;
    std::vector<std::string> ret;

    nr_ice_get_global_attributes(ice_ctx_, &attrs, &attrct);

    for (int i=0; i<attrct; i++) {
      ret.push_back(std::string(attrs[i]));
      RFREE(attrs[i]);
    }
    RFREE(attrs);

    return ret;
  }

  void ParseGlobalAttributes(std::vector<std::string> attrs) {
    std::vector<char *> attrs_in;

    for (auto& attr : attrs) {
      attrs_in.push_back(const_cast<char *>(attr.c_str()));
    }

    int r = nr_ice_peer_ctx_parse_global_attributes(peer_ctx_,
                                                    attrs_in.empty() ?
                                                    nullptr : &attrs_in[0],
                                                    attrs_in.size());
    ASSERT_EQ(0, r);
  }

  void SetControlling(bool controlling) {
    peer_ctx_->controlling = controlling ? 1 : 0;
  }

  void SetRemoteAttributes(std::vector<std::string> attributes) {
    int r;

    std::vector<char*> attrs;
    for (auto& attr: attributes) {
      attrs.push_back(const_cast<char*>(attr.c_str()));
    }

    if (!attrs.empty()) {
      r = nr_ice_peer_ctx_parse_stream_attributes(peer_ctx_, ice_media_stream_, &attrs[0], attrs.size());
      ASSERT_EQ(0, r);
    }
  }

  void StartChecks() {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(this,
                     &IcePeer::StartChecks_s),
        NS_DISPATCH_SYNC);
  }

  void StartChecks_s() {
    int r = nr_ice_peer_ctx_pair_candidates(peer_ctx_);
    ASSERT_EQ(0, r);

    r = nr_ice_peer_ctx_start_checks2(peer_ctx_, 1);
    ASSERT_EQ(0, r);
  }

  // Handler callbacks
  static int select_pair(void *obj, nr_ice_media_stream *stream,
                         int component_id, nr_ice_cand_pair **potentials,
                         int potential_ct) {
    return 0;
  }

  static int stream_ready(void *obj, nr_ice_media_stream *stream) {
    IcePeer* peer = static_cast<IcePeer*>(obj);
    peer->stream_ready_ = true;
    return 0;
  }

  static int stream_failed(void *obj, nr_ice_media_stream *stream) {
    IcePeer* peer = static_cast<IcePeer*>(obj);
    peer->stream_failed_ = true;
    return 0;
  }

  static int ice_checking(void *obj, nr_ice_peer_ctx *pctx) {
    IcePeer* peer = static_cast<IcePeer*>(obj);
    peer->ice_checking_ = true;
    return 0;
  }

  static int ice_connected(void *obj, nr_ice_peer_ctx *pctx) {
    IcePeer* peer = static_cast<IcePeer*>(obj);
    peer->ice_connected_ = true;
    return 0;
  }

  static int ice_disconnected(void *obj, nr_ice_peer_ctx *pctx) {
    IcePeer* peer = static_cast<IcePeer*>(obj);
    peer->ice_disconnected_ = true;
    return 0;
  }

  static int msg_recvd(void *obj, nr_ice_peer_ctx *pctx,
                       nr_ice_media_stream *stream, int component_id,
                       UCHAR *msg, int len) {
    return 0;
  }

  static void gather_cb(NR_SOCKET s, int h, void *arg) {
    IcePeer* peer = static_cast<IcePeer*>(arg);
    peer->gather_cb_ = true;
  }

  std::string name_;

  bool ice_checking_;
  bool ice_connected_;
  bool ice_disconnected_;
  bool gather_cb_;
  bool stream_ready_;
  bool stream_failed_;

  nr_ice_ctx* ice_ctx_;
  nr_ice_handler* ice_handler_;
  nr_ice_handler_vtbl* ice_handler_vtbl_;
  nr_ice_media_stream* ice_media_stream_;
  nr_ice_peer_ctx* peer_ctx_;
  TestNat* nat_;
  MtransportTestUtils* test_utils_;
};

class TestNrSocketIceUnitTest : public ::testing::Test {

public:
  void SetUp() override
  {
    NSS_NoDB_Init(nullptr);
    NSS_SetDomesticPolicy();

    test_utils_ = new MtransportTestUtils();
    test_utils2_ = new MtransportTestUtils();

    NrIceCtx::InitializeGlobals(false, false, false);
  }

  void TearDown() override
  {
    delete test_utils_;
    delete test_utils2_;
  }

  MtransportTestUtils* test_utils_;
  MtransportTestUtils* test_utils2_;

};

TEST_F(TestNrSocketIceUnitTest, TestIcePeer) {
  IcePeer peer("IcePeer", nullptr, NR_ICE_CTX_FLAGS_AGGRESSIVE_NOMINATION,
               test_utils_);
  ASSERT_NE(peer.ice_ctx_, nullptr);
  ASSERT_NE(peer.peer_ctx_, nullptr);
  ASSERT_NE(peer.ice_media_stream_, nullptr);
  peer.Gather();
  std::vector<std::string> attrs = peer.GetGlobalAttributes();
  ASSERT_NE(attrs.size(), 0UL);
  std::vector<std::string> candidates = peer.GetLocalCandidates();
  ASSERT_NE(candidates.size(), 0UL);
}

TEST_F(TestNrSocketIceUnitTest, TestIcePeersNoNAT) {
  IcePeer peer("IcePeer", nullptr, NR_ICE_CTX_FLAGS_AGGRESSIVE_NOMINATION,
               test_utils_);
  IcePeer peer2("IcePeer2", nullptr, NR_ICE_CTX_FLAGS_AGGRESSIVE_NOMINATION,
               test_utils2_);
  peer.SetControlling(true);
  peer2.SetControlling(false);

  peer.Gather();
  peer2.Gather();
  std::vector<std::string> attrs = peer.GetGlobalAttributes();
  peer2.ParseGlobalAttributes(attrs);
  std::vector<std::string> candidates = peer.GetLocalCandidates();
  peer2.SetRemoteAttributes(candidates);

  attrs = peer2.GetGlobalAttributes();
  peer.ParseGlobalAttributes(attrs);
  candidates = peer2.GetLocalCandidates();
  peer.SetRemoteAttributes(candidates);
  peer2.StartChecks();
  peer.StartChecks();

  ASSERT_TRUE_WAIT(peer.ice_connected_, kDefaultTimeout);
  ASSERT_TRUE_WAIT(peer2.ice_connected_, kDefaultTimeout);
}

TEST_F(TestNrSocketIceUnitTest, TestIcePeersPacketLoss) {
  IcePeer peer("IcePeer", nullptr, NR_ICE_CTX_FLAGS_AGGRESSIVE_NOMINATION,
               test_utils_);

  RefPtr<TestNat> nat(new TestNat);
  class NatDelegate : public TestNat::NatDelegate {
  public:
    NatDelegate()
      : messages(0) {}

    int on_read(TestNat *nat, void *buf, size_t maxlen, size_t *len) override
    {
      return 0;
    }

    int on_sendto(TestNat *nat, const void *msg, size_t len,
                          int flags, nr_transport_addr *to) override
    {
      ++messages;
      // 25% packet loss
      if (messages % 4 == 0) {
        return 1;
      }
      return 0;
    }

    int on_write(TestNat *nat, const void *msg, size_t len, size_t *written) override
    {
      return 0;
    }

    int messages;
  } delegate;
  nat->nat_delegate_ = &delegate;

  IcePeer peer2("IcePeer2", nat, NR_ICE_CTX_FLAGS_AGGRESSIVE_NOMINATION,
               test_utils2_);
  peer.SetControlling(true);
  peer2.SetControlling(false);

  peer.Gather();
  peer2.Gather();
  std::vector<std::string> attrs = peer.GetGlobalAttributes();
  peer2.ParseGlobalAttributes(attrs);
  std::vector<std::string> candidates = peer.GetLocalCandidates();
  peer2.SetRemoteAttributes(candidates);

  attrs = peer2.GetGlobalAttributes();
  peer.ParseGlobalAttributes(attrs);
  candidates = peer2.GetLocalCandidates();
  peer.SetRemoteAttributes(candidates);
  peer2.StartChecks();
  peer.StartChecks();

  ASSERT_TRUE_WAIT(peer.ice_connected_, kDefaultTimeout);
  ASSERT_TRUE_WAIT(peer2.ice_connected_, kDefaultTimeout);
}


}
