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

#include "scoped_ptrs.h"
#include "tls_parser.h"
#include "tls_filter.h"
#include "tls_connect.h"
#include "gtest_utils.h"

namespace nss_test {

TEST_P(TlsConnectGeneric, ClientAuth) {
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
}

// In TLS 1.3, the client sends its cert rejection on the
// second flight, and since it has already received the
// server's Finished, it transitions to complete and
// then gets an alert from the server. The test harness
// doesn't handle this right yet.
TEST_P(TlsConnectStream, DISABLED_ClientAuthRequiredRejected) {
  server_->RequestClientAuth(true);
  ConnectExpectFail();
}

TEST_P(TlsConnectGeneric, ClientAuthRequestedRejected) {
  server_->RequestClientAuth(false);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
}


TEST_P(TlsConnectGeneric, ClientAuthEcdsa) {
  Reset(TlsAgent::kServerEcdsa);
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_ecdsa);
}

static const SSLSignatureAndHashAlg SignatureEcdsaSha384[] = {
  {ssl_hash_sha384, ssl_sign_ecdsa}
};
static const SSLSignatureAndHashAlg SignatureEcdsaSha256[] = {
  {ssl_hash_sha256, ssl_sign_ecdsa}
};
static const SSLSignatureAndHashAlg SignatureRsaSha384[] = {
  {ssl_hash_sha384, ssl_sign_rsa}
};
static const SSLSignatureAndHashAlg SignatureRsaSha256[] = {
  {ssl_hash_sha256, ssl_sign_rsa}
};

// When signature algorithms match up, this should connect successfully; even
// for TLS 1.1 and 1.0, where they should be ignored.
TEST_P(TlsConnectGeneric, SignatureAlgorithmServerAuth) {
  client_->SetSignatureAlgorithms(SignatureEcdsaSha384,
                                  PR_ARRAY_SIZE(SignatureEcdsaSha384));
  server_->SetSignatureAlgorithms(SignatureEcdsaSha384,
                                  PR_ARRAY_SIZE(SignatureEcdsaSha384));
  Reset(TlsAgent::kServerEcdsa);
  Connect();
}

// Here the client picks a single option, which should work in all versions.
// Defaults on the server include the first option.
TEST_P(TlsConnectGeneric, SignatureAlgorithmClientOnly) {
  const SSLSignatureAndHashAlg clientAlgorithms[] = {
    {ssl_hash_sha384, ssl_sign_ecdsa},
    {ssl_hash_sha384, ssl_sign_rsa}, // supported but unusable
    {ssl_hash_md5, ssl_sign_ecdsa} // unsupported and ignored
  };
  client_->SetSignatureAlgorithms(clientAlgorithms,
                                  PR_ARRAY_SIZE(clientAlgorithms));
  Reset(TlsAgent::kServerEcdsa);
  Connect();
}

// Here the server picks a single option, which should work in all versions.
// Defaults on the client include the provided option.
TEST_P(TlsConnectGeneric, SignatureAlgorithmServerOnly) {
  server_->SetSignatureAlgorithms(SignatureEcdsaSha384,
                                  PR_ARRAY_SIZE(SignatureEcdsaSha384));
  Reset(TlsAgent::kServerEcdsa);
  Connect();
}

// There is no need for overlap on signatures; since we don't actually use the
// signatures for static RSA, this should still connect successfully.
// This should also work in TLS 1.0 and 1.1 where the algorithms aren't used.
TEST_P(TlsConnectGenericPre13, SignatureAlgorithmNoOverlapStaticRsa) {
  client_->SetSignatureAlgorithms(SignatureRsaSha384,
                                  PR_ARRAY_SIZE(SignatureRsaSha384));
  server_->SetSignatureAlgorithms(SignatureRsaSha256,
                                  PR_ARRAY_SIZE(SignatureRsaSha256));
  EnableOnlyStaticRsaCiphers();
  Connect();
  CheckKeys(ssl_kea_rsa, ssl_auth_rsa_decrypt);
}

// TODO(ekr@rtfm.com): We need to enable this for 1.3 when we fix
// bug 1287267.
TEST_P(TlsConnectTls12, SignatureAlgorithmNoOverlapEcdsa) {
  Reset(TlsAgent::kServerEcdsa);
  client_->SetSignatureAlgorithms(SignatureEcdsaSha384,
                                  PR_ARRAY_SIZE(SignatureEcdsaSha384));
  server_->SetSignatureAlgorithms(SignatureEcdsaSha256,
                                  PR_ARRAY_SIZE(SignatureEcdsaSha256));
  ConnectExpectFail();
}

// Pre 1.2, a mismatch on signature algorithms shouldn't affect anything.
TEST_P(TlsConnectPre12, SignatureAlgorithmNoOverlapEcdsa) {
  Reset(TlsAgent::kServerEcdsa);
  client_->SetSignatureAlgorithms(SignatureEcdsaSha384,
                                  PR_ARRAY_SIZE(SignatureEcdsaSha384));
  server_->SetSignatureAlgorithms(SignatureEcdsaSha256,
                                  PR_ARRAY_SIZE(SignatureEcdsaSha256));
  Connect();
}

TEST_P(TlsConnectTls12Plus, RequestClientAuthWithSha384) {
  server_->SetSignatureAlgorithms(SignatureRsaSha384,
                                  PR_ARRAY_SIZE(SignatureRsaSha384));
  server_->RequestClientAuth(false);
  Connect();
}

class BeforeFinished : public TlsRecordFilter {
 private:
  enum HandshakeState {
    BEFORE_CCS,
    AFTER_CCS,
    DONE
  };

 public:
  BeforeFinished(TlsAgent* client, TlsAgent* server,
                 VoidFunction before_ccs, VoidFunction before_finished)
      : client_(client),
        server_(server),
        before_ccs_(before_ccs),
        before_finished_(before_finished),
        state_(BEFORE_CCS) {}

 protected:
  virtual PacketFilter::Action FilterRecord(
      const RecordHeader& header, const DataBuffer& body, DataBuffer* out) {
    switch (state_) {
      case BEFORE_CCS:
        // Awaken when we see the CCS.
        if (header.content_type() == kTlsChangeCipherSpecType) {
          before_ccs_();

          // Write the CCS out as a separate write, so that we can make
          // progress. Ordinarily, libssl sends the CCS and Finished together,
          // but that means that they both get processed together.
          DataBuffer ccs;
          header.Write(&ccs, 0, body);
          server_->SendDirect(ccs);
          client_->Handshake();
          state_ = AFTER_CCS;
          // Request that the original record be dropped by the filter.
          return DROP;
        }
        break;

      case AFTER_CCS:
        EXPECT_EQ(kTlsHandshakeType, header.content_type());
        // This could check that data contains a Finished message, but it's
        // encrypted, so that's too much extra work.

        before_finished_();
        state_ = DONE;
        break;

      case DONE:
        break;
    }
    return KEEP;
  }

 private:
  TlsAgent* client_;
  TlsAgent* server_;
  VoidFunction before_ccs_;
  VoidFunction before_finished_;
  HandshakeState state_;
};

// Running code after the client has started processing the encrypted part of
// the server's first flight, but before the Finished is processed is very hard
// in TLS 1.3.  These encrypted messages are sent in a single encrypted blob.
// The following test uses DTLS to make it possible to force the client to
// process the handshake in pieces.
//
// The first encrypted message from the server is dropped, and the MTU is
// reduced to just below the original message size so that the server sends two
// messages.  The Finished message is then processed separately.
class BeforeFinished13 : public PacketFilter {
 private:
  enum HandshakeState {
    INIT,
    BEFORE_FIRST_FRAGMENT,
    BEFORE_SECOND_FRAGMENT,
    DONE
  };

 public:
  BeforeFinished13(TlsAgent* client, TlsAgent *server,
                   VoidFunction before_finished)
      : client_(client),
        server_(server),
        before_finished_(before_finished),
        records_(0) {}

 protected:
  virtual PacketFilter::Action Filter(const DataBuffer& input,
                                      DataBuffer* output) {
    switch (++records_) {
      case 1:
        // Packet 1 is the server's entire first flight.  Drop it.
        EXPECT_EQ(SECSuccess,
                  SSLInt_SetMTU(server_->ssl_fd(), input.len() - 1));
        return DROP;

        // Packet 2 is the first part of the server's retransmitted first
        // flight.  Keep that.

      case 3:
        // Packet 3 is the second part of the server's retransmitted first
        // flight.  Before passing that on, make sure that the client processes
        // packet 2, then call the before_finished_() callback.
        client_->Handshake();
        before_finished_();
        break;

      default:
        break;
    }
    return KEEP;
  }

 private:
  TlsAgent *client_;
  TlsAgent *server_;
  VoidFunction before_finished_;
  size_t records_;
};

#ifdef NSS_ENABLE_TLS_1_3
// This test uses an AuthCertificateCallback that blocks.  A filter is used to
// split the server's first flight into two pieces.  Before the second piece is
// processed by the client, SSL_AuthCertificateComplete() is called.
TEST_F(TlsConnectDatagram13, AuthCompleteBeforeFinished) {
  client_->SetAuthCertificateCallback(
      [](TlsAgent&, PRBool, PRBool) -> SECStatus {
        return SECWouldBlock;
      });
  server_->SetPacketFilter(new BeforeFinished13(client_, server_, [this]() {
        EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client_->ssl_fd(), 0));
      }));
  Connect();
}

static void TriggerAuthComplete(PollTarget *target, Event event) {
  std::cerr << "client: call SSL_AuthCertificateComplete" << std::endl;
  EXPECT_EQ(TIMER_EVENT, event);
  TlsAgent* client = static_cast<TlsAgent*>(target);
  EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client->ssl_fd(), 0));
}

// This test uses a simple AuthCertificateCallback.  Due to the way that the
// entire server flight is processed, the call to SSL_AuthCertificateComplete
// will trigger after the Finished message is processed.
TEST_F(TlsConnectDatagram13, AuthCompleteAfterFinished) {
  client_->SetAuthCertificateCallback(
      [this](TlsAgent&, PRBool, PRBool) -> SECStatus {
        Poller::Timer *timer_handle;
        // This is really just to unroll the stack.
        Poller::Instance()->SetTimer(1U, client_, TriggerAuthComplete,
                                     &timer_handle);
        return SECWouldBlock;
      });
  Connect();
}
#endif

TEST_P(TlsConnectGenericPre13, ClientWriteBetweenCCSAndFinishedWithFalseStart) {
  client_->EnableFalseStart();
  server_->SetPacketFilter(new BeforeFinished(client_, server_, [this]() {
        EXPECT_TRUE(client_->can_falsestart_hook_called());
      }, [this]() {
        // Write something, which used to fail: bug 1235366.
        client_->SendData(10);
      }));

  Connect();
  server_->SendData(10);
  Receive(10);
}

TEST_P(TlsConnectGenericPre13, AuthCompleteBeforeFinishedWithFalseStart) {
  client_->EnableFalseStart();
  client_->SetAuthCertificateCallback(
      [](TlsAgent&, PRBool, PRBool) -> SECStatus {
        return SECWouldBlock;
      });
  server_->SetPacketFilter(new BeforeFinished(client_, server_, []() {
        // Do nothing before CCS
      }, [this]() {
        EXPECT_FALSE(client_->can_falsestart_hook_called());
        // AuthComplete before Finished still enables false start.
        EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client_->ssl_fd(), 0));
        EXPECT_TRUE(client_->can_falsestart_hook_called());
        client_->SendData(10);
      }));

  Connect();
  server_->SendData(10);
  Receive(10);
}

}
