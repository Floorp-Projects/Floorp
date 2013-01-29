/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef transportlayerdtls_h__
#define transportlayerdtls_h__

#include <queue>

#include "sigslot.h"

#include "mozilla/RefPtr.h"
#include "mozilla/Scoped.h"
#include "nsCOMPtr.h"
#include "nsIEventTarget.h"
#include "nsITimer.h"
#include "ScopedNSSTypes.h"
#include "m_cpp_utils.h"
#include "dtlsidentity.h"
#include "transportflow.h"
#include "transportlayer.h"

namespace mozilla {

struct Packet;

class TransportLayerNSPRAdapter {
 public:
  TransportLayerNSPRAdapter(TransportLayer *output) :
  output_(output),
  input_() {}

  void PacketReceived(const void *data, int32_t len);
  int32_t Read(void *data, int32_t len);
  int32_t Write(const void *buf, int32_t length);

 private:
  DISALLOW_COPY_ASSIGN(TransportLayerNSPRAdapter);

  TransportLayer *output_;
  std::queue<Packet *> input_;
};

class TransportLayerDtls : public TransportLayer {
 public:
  TransportLayerDtls() :
      TransportLayer(DGRAM),
      role_(CLIENT),
      verification_mode_(VERIFY_UNSET),
      ssl_fd_(nullptr),
      auth_hook_called_(false),
      cert_ok_(false) {}

  virtual ~TransportLayerDtls();

  enum Role { CLIENT, SERVER};
  enum Verification { VERIFY_UNSET, VERIFY_ALLOW_ALL, VERIFY_DIGEST};
  const static size_t kMaxDigestLength = HASH_LENGTH_MAX;

  // DTLS-specific operations
  void SetRole(Role role) { role_ = role;}
  Role role() { return role_; }

  void SetIdentity(const RefPtr<DtlsIdentity>& identity) {
    identity_ = identity;
  }
  nsresult SetVerificationAllowAll();
  nsresult SetVerificationDigest(const std::string digest_algorithm,
                                 const unsigned char *digest_value,
                                 size_t digest_len);

  nsresult SetSrtpCiphers(std::vector<uint16_t> ciphers);
  nsresult GetSrtpCipher(uint16_t *cipher);

  nsresult ExportKeyingMaterial(const std::string& label,
                                bool use_context,
                                const std::string& context,
                                unsigned char *out,
                                unsigned int outlen);

  const CERTCertificate *GetPeerCert() const {
    return peer_cert_;
  }

  // Transport layer overrides.
  virtual nsresult InitInternal();
  virtual void WasInserted();
  virtual TransportResult SendPacket(const unsigned char *data, size_t len);

  // Signals
  void StateChange(TransportLayer *layer, State state);
  void PacketReceived(TransportLayer* layer, const unsigned char *data,
                      size_t len);

  TRANSPORT_LAYER_ID("dtls")

  private:
  DISALLOW_COPY_ASSIGN(TransportLayerDtls);

  // A single digest to check
  class VerificationDigest {
   public:
    VerificationDigest(std::string algorithm,
                       const unsigned char *value, size_t len) {
      MOZ_ASSERT(len <= sizeof(value_));

      algorithm_ = algorithm;
      memcpy(value_, value, len);
      len_ = len;
    }

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VerificationDigest)

    std::string algorithm_;
    size_t len_;
    unsigned char value_[kMaxDigestLength];

   private:
    DISALLOW_COPY_ASSIGN(VerificationDigest);
  };


  bool Setup();
  void Handshake();

  static SECStatus GetClientAuthDataHook(void *arg, PRFileDesc *fd,
                                         CERTDistNames *caNames,
                                         CERTCertificate **pRetCert,
                                         SECKEYPrivateKey **pRetKey);
  static SECStatus AuthCertificateHook(void *arg,
                                       PRFileDesc *fd,
                                       PRBool checksig,
                                       PRBool isServer);
  SECStatus AuthCertificateHook(PRFileDesc *fd,
                                PRBool checksig,
                                PRBool isServer);

  static void TimerCallback(nsITimer *timer, void *arg);

  SECStatus CheckDigest(const RefPtr<VerificationDigest>& digest,
                        CERTCertificate *cert);

  RefPtr<DtlsIdentity> identity_;
  std::vector<uint16_t> srtp_ciphers_;

  Role role_;
  Verification verification_mode_;
  std::vector<RefPtr<VerificationDigest> > digests_;

  // Must delete nspr_io_adapter after ssl_fd_ b/c ssl_fd_ causes an alert
  // (ssl_fd_ contains an un-owning pointer to nspr_io_adapter_)
  ScopedDeletePtr<TransportLayerNSPRAdapter> nspr_io_adapter_;
  ScopedPRFileDesc ssl_fd_;

  ScopedCERTCertificate peer_cert_;
  nsCOMPtr<nsITimer> timer_;
  bool auth_hook_called_;
  bool cert_ok_;
};


}  // close namespace
#endif
