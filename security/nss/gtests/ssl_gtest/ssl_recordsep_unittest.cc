/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

extern "C" {
// This is not something that should make you happy.
#include "libssl_internals.h"
}

#include <queue>
#include "gtest_utils.h"
#include "nss_scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

class HandshakeSecretTracker {
 public:
  HandshakeSecretTracker(const std::shared_ptr<TlsAgent>& agent,
                         uint16_t first_read_epoch, uint16_t first_write_epoch)
      : agent_(agent),
        next_read_epoch_(first_read_epoch),
        next_write_epoch_(first_write_epoch) {
    EXPECT_EQ(SECSuccess,
              SSL_SecretCallback(agent_->ssl_fd(),
                                 HandshakeSecretTracker::SecretCb, this));
  }

  void CheckComplete() const {
    EXPECT_EQ(0, next_read_epoch_);
    EXPECT_EQ(0, next_write_epoch_);
  }

 private:
  static void SecretCb(PRFileDesc* fd, PRUint16 epoch, SSLSecretDirection dir,
                       PK11SymKey* secret, void* arg) {
    HandshakeSecretTracker* t = reinterpret_cast<HandshakeSecretTracker*>(arg);
    t->SecretUpdated(epoch, dir, secret);
  }

  void SecretUpdated(PRUint16 epoch, SSLSecretDirection dir,
                     PK11SymKey* secret) {
    if (g_ssl_gtest_verbose) {
      std::cerr << agent_->role_str() << ": secret callback for " << dir
                << " epoch " << epoch << std::endl;
    }

    EXPECT_TRUE(secret);
    uint16_t* p;
    if (dir == ssl_secret_read) {
      p = &next_read_epoch_;
    } else {
      ASSERT_EQ(ssl_secret_write, dir);
      p = &next_write_epoch_;
    }
    EXPECT_EQ(*p, epoch);
    switch (*p) {
      case 1:  // 1 == 0-RTT, next should be handshake.
      case 2:  // 2 == handshake, next should be application data.
        (*p)++;
        break;

      case 3:  // 3 == application data, there should be no more.
        // Use 0 as a sentinel value.
        *p = 0;
        break;

      default:
        ADD_FAILURE() << "Unexpected next epoch: " << *p;
    }
  }

  std::shared_ptr<TlsAgent> agent_;
  uint16_t next_read_epoch_;
  uint16_t next_write_epoch_;
};

TEST_F(TlsConnectTest, HandshakeSecrets) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  EnsureTlsSetup();

  HandshakeSecretTracker c(client_, 2, 2);
  HandshakeSecretTracker s(server_, 2, 2);

  Connect();
  SendReceive();

  c.CheckComplete();
  s.CheckComplete();
}

TEST_F(TlsConnectTest, ZeroRttSecrets) {
  SetupForZeroRtt();

  HandshakeSecretTracker c(client_, 2, 1);
  HandshakeSecretTracker s(server_, 1, 2);

  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, true);
  Handshake();
  ExpectEarlyDataAccepted(true);
  CheckConnected();
  SendReceive();

  c.CheckComplete();
  s.CheckComplete();
}

class KeyUpdateTracker {
 public:
  KeyUpdateTracker(const std::shared_ptr<TlsAgent>& agent,
                   bool expect_read_secret)
      : agent_(agent), expect_read_secret_(expect_read_secret), called_(false) {
    EXPECT_EQ(SECSuccess, SSL_SecretCallback(agent_->ssl_fd(),
                                             KeyUpdateTracker::SecretCb, this));
  }

  void CheckCalled() const { EXPECT_TRUE(called_); }

 private:
  static void SecretCb(PRFileDesc* fd, PRUint16 epoch, SSLSecretDirection dir,
                       PK11SymKey* secret, void* arg) {
    KeyUpdateTracker* t = reinterpret_cast<KeyUpdateTracker*>(arg);
    t->SecretUpdated(epoch, dir, secret);
  }

  void SecretUpdated(PRUint16 epoch, SSLSecretDirection dir,
                     PK11SymKey* secret) {
    EXPECT_EQ(4U, epoch);
    EXPECT_EQ(expect_read_secret_, dir == ssl_secret_read);
    EXPECT_TRUE(secret);
    called_ = true;
  }

  std::shared_ptr<TlsAgent> agent_;
  bool expect_read_secret_;
  bool called_;
};

TEST_F(TlsConnectTest, KeyUpdateSecrets) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  // The update is to the client write secret; the server read secret.
  KeyUpdateTracker c(client_, false);
  KeyUpdateTracker s(server_, true);
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
  SendReceive(50);
  SendReceive(60);
  CheckEpochs(4, 3);
  c.CheckCalled();
  s.CheckCalled();
}

// BadPrSocket is an instance of a PR IO layer that crashes the test if it is
// ever used for reading or writing.  It does that by failing to overwrite any
// of the DummyIOLayerMethods, which all crash when invoked.
class BadPrSocket : public DummyIOLayerMethods {
 public:
  BadPrSocket(std::shared_ptr<TlsAgent>& agent) : DummyIOLayerMethods() {
    static PRDescIdentity bad_identity = PR_GetUniqueIdentity("bad NSPR id");
    fd_ = DummyIOLayerMethods::CreateFD(bad_identity, this);

    // This is terrible, but NSPR doesn't provide an easy way to replace the
    // bottom layer of an IO stack.  Take the DummyPrSocket and replace its
    // NSPR method vtable with the ones from this object.
    dummy_layer_ =
        PR_GetIdentitiesLayer(agent->ssl_fd(), DummyPrSocket::LayerId());
    original_methods_ = dummy_layer_->methods;
    original_secret_ = dummy_layer_->secret;
    dummy_layer_->methods = fd_->methods;
    dummy_layer_->secret = reinterpret_cast<PRFilePrivate*>(this);
  }

  // This will be destroyed before the agent, so we need to restore the state
  // before we tampered with it.
  virtual ~BadPrSocket() {
    dummy_layer_->methods = original_methods_;
    dummy_layer_->secret = original_secret_;
  }

 private:
  ScopedPRFileDesc fd_;
  PRFileDesc* dummy_layer_;
  const PRIOMethods* original_methods_;
  PRFilePrivate* original_secret_;
};

class StagedRecords {
 public:
  StagedRecords(std::shared_ptr<TlsAgent>& agent) : agent_(agent), records_() {
    EXPECT_EQ(SECSuccess,
              SSL_RecordLayerWriteCallback(
                  agent_->ssl_fd(), StagedRecords::StageRecordData, this));
  }

  virtual ~StagedRecords() {
    // Uninstall so that the callback doesn't fire during cleanup.
    EXPECT_EQ(SECSuccess,
              SSL_RecordLayerWriteCallback(agent_->ssl_fd(), nullptr, nullptr));
  }

  bool empty() const { return records_.empty(); }

  void ForwardAll(std::shared_ptr<TlsAgent>& peer) {
    EXPECT_NE(agent_, peer) << "can't forward to self";
    for (auto r : records_) {
      r.Forward(peer);
    }
    records_.clear();
  }

  // This forwards all saved data and checks the resulting state.
  void ForwardAll(std::shared_ptr<TlsAgent>& peer,
                  TlsAgent::State expected_state) {
    ForwardAll(peer);
    switch (expected_state) {
      case TlsAgent::STATE_CONNECTED:
        // The handshake callback should have been called, so check that before
        // checking that SSL_ForceHandshake succeeds.
        EXPECT_EQ(expected_state, peer->state());
        EXPECT_EQ(SECSuccess, SSL_ForceHandshake(peer->ssl_fd()));
        break;

      case TlsAgent::STATE_CONNECTING:
        // Check that SSL_ForceHandshake() blocks.
        EXPECT_EQ(SECFailure, SSL_ForceHandshake(peer->ssl_fd()));
        EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());
        // Update and check the state.
        peer->Handshake();
        EXPECT_EQ(TlsAgent::STATE_CONNECTING, peer->state());
        break;

      default:
        ADD_FAILURE() << "No idea how to handle this state";
    }
  }

  void ForwardPartial(std::shared_ptr<TlsAgent>& peer) {
    if (records_.empty()) {
      ADD_FAILURE() << "No records to slice";
      return;
    }
    auto& last = records_.back();
    auto tail = last.SliceTail();
    ForwardAll(peer, TlsAgent::STATE_CONNECTING);
    records_.push_back(tail);
    EXPECT_EQ(TlsAgent::STATE_CONNECTING, peer->state());
  }

 private:
  // A single record.
  class StagedRecord {
   public:
    StagedRecord(const std::string role, uint16_t epoch, SSLContentType ct,
                 const uint8_t* data, size_t len)
        : role_(role), epoch_(epoch), content_type_(ct), data_(data, len) {
      if (g_ssl_gtest_verbose) {
        std::cerr << role_ << ": staged epoch " << epoch_ << " "
                  << content_type_ << ": " << data_ << std::endl;
      }
    }

    // This forwards staged data to the identified agent.
    void Forward(std::shared_ptr<TlsAgent>& peer) {
      // Now there should be staged data.
      EXPECT_FALSE(data_.empty());
      if (g_ssl_gtest_verbose) {
        std::cerr << role_ << ": forward " << data_ << std::endl;
      }
      EXPECT_EQ(SECSuccess,
                SSL_RecordLayerData(peer->ssl_fd(), epoch_, content_type_,
                                    data_.data(),
                                    static_cast<unsigned int>(data_.len())));
    }

    // Slices the tail off this record and returns it.
    StagedRecord SliceTail() {
      size_t slice = 1;
      if (data_.len() <= slice) {
        ADD_FAILURE() << "record too small to slice in two";
        slice = 0;
      }
      size_t keep = data_.len() - slice;
      StagedRecord tail(role_, epoch_, content_type_, data_.data() + keep,
                        slice);
      data_.Truncate(keep);
      return tail;
    }

   private:
    std::string role_;
    uint16_t epoch_;
    SSLContentType content_type_;
    DataBuffer data_;
  };

  // This is an SSLRecordWriteCallback that stages data.
  static SECStatus StageRecordData(PRFileDesc* fd, PRUint16 epoch,
                                   SSLContentType content_type,
                                   const PRUint8* data, unsigned int len,
                                   void* arg) {
    auto stage = reinterpret_cast<StagedRecords*>(arg);
    stage->records_.push_back(StagedRecord(stage->agent_->role_str(), epoch,
                                           content_type, data,
                                           static_cast<size_t>(len)));
    return SECSuccess;
  }

  std::shared_ptr<TlsAgent>& agent_;
  std::deque<StagedRecord> records_;
};

// Attempting to feed application data in before the handshake is complete
// should be caught.
static void RefuseApplicationData(std::shared_ptr<TlsAgent>& peer,
                                  uint16_t epoch) {
  static const uint8_t d[] = {1, 2, 3};
  EXPECT_EQ(SECFailure,
            SSL_RecordLayerData(peer->ssl_fd(), epoch, ssl_ct_application_data,
                                d, static_cast<unsigned int>(sizeof(d))));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}

static void SendForwardReceive(std::shared_ptr<TlsAgent>& sender,
                               StagedRecords& sender_stage,
                               std::shared_ptr<TlsAgent>& receiver) {
  const size_t count = 10;
  sender->SendData(count, count);
  sender_stage.ForwardAll(receiver);
  receiver->ReadBytes(count);
}

TEST_P(TlsConnectStream, ReplaceRecordLayer) {
  StartConnect();
  client_->SetServerKeyBits(server_->server_key_bits());

  // BadPrSocket installs an IO layer that crashes when the SSL layer attempts
  // to read or write.
  BadPrSocket bad_layer_client(client_);
  BadPrSocket bad_layer_server(server_);

  // StagedRecords installs a handler for unprotected data from the socket, and
  // captures that data.
  StagedRecords client_stage(client_);
  StagedRecords server_stage(server_);

  // Both peers should refuse application data from epoch 0.
  RefuseApplicationData(client_, 0);
  RefuseApplicationData(server_, 0);

  // This first call forwards nothing, but it causes the client to handshake,
  // which starts things off.  This stages the ClientHello as a result.
  server_stage.ForwardAll(client_, TlsAgent::STATE_CONNECTING);
  // This processes the ClientHello and stages the first server flight.
  client_stage.ForwardAll(server_, TlsAgent::STATE_CONNECTING);
  RefuseApplicationData(server_, 1);
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    // Process the server flight and the client is done.
    server_stage.ForwardAll(client_, TlsAgent::STATE_CONNECTED);
    client_stage.ForwardAll(server_, TlsAgent::STATE_CONNECTED);
  } else {
    server_stage.ForwardAll(client_, TlsAgent::STATE_CONNECTING);
    RefuseApplicationData(client_, 1);
    client_stage.ForwardAll(server_, TlsAgent::STATE_CONNECTED);
    server_stage.ForwardAll(client_, TlsAgent::STATE_CONNECTED);
  }
  CheckKeys();

  // Reading and writing application data should work.
  SendForwardReceive(client_, client_stage, server_);
  SendForwardReceive(server_, server_stage, client_);
}

static SECStatus AuthCompleteBlock(TlsAgent*, PRBool, PRBool) {
  return SECWouldBlock;
}

TEST_P(TlsConnectStream, ReplaceRecordLayerAsyncLateAuth) {
  StartConnect();
  client_->SetServerKeyBits(server_->server_key_bits());

  BadPrSocket bad_layer_client(client_);
  BadPrSocket bad_layer_server(server_);
  StagedRecords client_stage(client_);
  StagedRecords server_stage(server_);

  client_->SetAuthCertificateCallback(AuthCompleteBlock);

  server_stage.ForwardAll(client_, TlsAgent::STATE_CONNECTING);
  client_stage.ForwardAll(server_, TlsAgent::STATE_CONNECTING);
  server_stage.ForwardAll(client_, TlsAgent::STATE_CONNECTING);

  // Prior to TLS 1.3, the client sends its second flight immediately.  But in
  // TLS 1.3, a client won't send a Finished until it is happy with the server
  // certificate.  So blocking certificate validation causes the client to send
  // nothing.
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    ASSERT_TRUE(client_stage.empty());

    // Client should have stopped reading when it saw the Certificate message,
    // so it will be reading handshake epoch, and writing cleartext.
    client_->CheckEpochs(2, 0);
    // Server should be reading handshake, and writing application data.
    server_->CheckEpochs(2, 3);

    // Handshake again and the client will read the remainder of the server's
    // flight, but it will remain blocked.
    client_->Handshake();
    ASSERT_TRUE(client_stage.empty());
    EXPECT_EQ(TlsAgent::STATE_CONNECTING, client_->state());
  } else {
    // In prior versions, the client's second flight is always sent.
    ASSERT_FALSE(client_stage.empty());
  }

  // Now declare the certificate good.
  EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client_->ssl_fd(), 0));
  client_->Handshake();
  ASSERT_FALSE(client_stage.empty());

  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());
    client_stage.ForwardAll(server_, TlsAgent::STATE_CONNECTED);
  } else {
    client_stage.ForwardAll(server_, TlsAgent::STATE_CONNECTED);
    server_stage.ForwardAll(client_, TlsAgent::STATE_CONNECTED);
  }
  CheckKeys();

  // Reading and writing application data should work.
  SendForwardReceive(client_, client_stage, server_);
}

// This test ensures that data is correctly forwarded when the handshake is
// resumed after asynchronous server certificate authentication, when
// SSL_AuthCertificateComplete() is called.  The logic for resuming the
// handshake involves a different code path than the usual one, so this test
// exercises that code fully.
TEST_F(TlsConnectStreamTls13, ReplaceRecordLayerAsyncEarlyAuth) {
  StartConnect();
  client_->SetServerKeyBits(server_->server_key_bits());

  BadPrSocket bad_layer_client(client_);
  BadPrSocket bad_layer_server(server_);
  StagedRecords client_stage(client_);
  StagedRecords server_stage(server_);

  client_->SetAuthCertificateCallback(AuthCompleteBlock);

  server_stage.ForwardAll(client_, TlsAgent::STATE_CONNECTING);
  client_stage.ForwardAll(server_, TlsAgent::STATE_CONNECTING);

  // Send a partial flight on to the client.
  // This includes enough to trigger the certificate callback.
  server_stage.ForwardPartial(client_);
  EXPECT_TRUE(client_stage.empty());

  // Declare the certificate good.
  EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client_->ssl_fd(), 0));
  client_->Handshake();
  EXPECT_TRUE(client_stage.empty());

  // Send the remainder of the server flight.
  PRBool pending = PR_FALSE;
  EXPECT_EQ(SECSuccess,
            SSLInt_HasPendingHandshakeData(client_->ssl_fd(), &pending));
  EXPECT_EQ(PR_TRUE, pending);
  EXPECT_EQ(TlsAgent::STATE_CONNECTING, client_->state());
  server_stage.ForwardAll(client_, TlsAgent::STATE_CONNECTED);
  client_stage.ForwardAll(server_, TlsAgent::STATE_CONNECTED);
  CheckKeys();

  SendForwardReceive(server_, server_stage, client_);
}

TEST_P(TlsConnectStream, ForwardDataFromWrongEpoch) {
  const uint8_t data[] = {1};
  Connect();
  uint16_t next_epoch;
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    EXPECT_EQ(SECFailure,
              SSL_RecordLayerData(client_->ssl_fd(), 2, ssl_ct_application_data,
                                  data, sizeof(data)));
    EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError())
        << "Passing data from an old epoch is rejected";
    next_epoch = 4;
  } else {
    // Prior to TLS 1.3, the epoch is only updated once during the handshake.
    next_epoch = 2;
  }
  EXPECT_EQ(SECFailure,
            SSL_RecordLayerData(client_->ssl_fd(), next_epoch,
                                ssl_ct_application_data, data, sizeof(data)));
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError())
      << "Passing data from a future epoch blocks";
}

TEST_F(TlsConnectStreamTls13, ForwardInvalidData) {
  const uint8_t data[1] = {0};

  EnsureTlsSetup();
  // Zero-length data.
  EXPECT_EQ(SECFailure, SSL_RecordLayerData(client_->ssl_fd(), 0,
                                            ssl_ct_application_data, data, 0));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  // NULL data.
  EXPECT_EQ(SECFailure,
            SSL_RecordLayerData(client_->ssl_fd(), 0, ssl_ct_application_data,
                                nullptr, 1));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}

TEST_F(TlsConnectDatagram13, ForwardDataDtls) {
  EnsureTlsSetup();
  const uint8_t data[1] = {0};
  EXPECT_EQ(SECFailure,
            SSL_RecordLayerData(client_->ssl_fd(), 0, ssl_ct_application_data,
                                data, sizeof(data)));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}

}  // namespace nss_test
