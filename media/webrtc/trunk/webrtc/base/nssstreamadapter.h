/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_NSSSTREAMADAPTER_H_
#define WEBRTC_BASE_NSSSTREAMADAPTER_H_

#include <string>
#include <vector>

#include "nspr.h"
#include "nss.h"
#include "secmodt.h"

#include "webrtc/base/buffer.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/nssidentity.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/base/sslstreamadapterhelper.h"

namespace rtc {

// Singleton
class NSSContext {
 public:
  explicit NSSContext(PK11SlotInfo* slot) : slot_(slot) {}
  ~NSSContext() {
  }

  static PK11SlotInfo *GetSlot() {
    return Instance() ? Instance()->slot_: NULL;
  }

  static NSSContext *Instance();
  static bool InitializeSSL(VerificationCallback callback);
  static bool InitializeSSLThread();
  static bool CleanupSSL();

 private:
  PK11SlotInfo *slot_;                    // The PKCS-11 slot
  static GlobalLockPod lock;              // To protect the global context
  static NSSContext *global_nss_context;  // The global context
};


class NSSStreamAdapter : public SSLStreamAdapterHelper {
 public:
  explicit NSSStreamAdapter(StreamInterface* stream);
  ~NSSStreamAdapter() override;
  bool Init();

  StreamResult Read(void* data,
                    size_t data_len,
                    size_t* read,
                    int* error) override;
  StreamResult Write(const void* data,
                     size_t data_len,
                     size_t* written,
                     int* error) override;
  void OnMessage(Message* msg) override;

  bool GetSslCipher(std::string* cipher) override;

  // Key Extractor interface
  bool ExportKeyingMaterial(const std::string& label,
                            const uint8* context,
                            size_t context_len,
                            bool use_context,
                            uint8* result,
                            size_t result_len) override;

  // DTLS-SRTP interface
  bool SetDtlsSrtpCiphers(const std::vector<std::string>& ciphers) override;
  bool GetDtlsSrtpCipher(std::string* cipher) override;

  // Capabilities interfaces
  static bool HaveDtls();
  static bool HaveDtlsSrtp();
  static bool HaveExporter();
  static std::string GetDefaultSslCipher();

 protected:
  // Override SSLStreamAdapter
  void OnEvent(StreamInterface* stream, int events, int err) override;

  // Override SSLStreamAdapterHelper
  int BeginSSL() override;
  void Cleanup() override;
  bool GetDigestLength(const std::string& algorithm, size_t* length) override;

 private:
  int ContinueSSL();
  static SECStatus AuthCertificateHook(void *arg, PRFileDesc *fd,
                                       PRBool checksig, PRBool isServer);
  static SECStatus GetClientAuthDataHook(void *arg, PRFileDesc *fd,
                                         CERTDistNames *caNames,
                                         CERTCertificate **pRetCert,
                                         SECKEYPrivateKey **pRetKey);

  PRFileDesc *ssl_fd_;              // NSS's SSL file descriptor
  static bool initialized;          // Was InitializeSSL() called?
  bool cert_ok_;                    // Did we get and check a cert
  std::vector<PRUint16> srtp_ciphers_;  // SRTP cipher list

  static PRDescIdentity nspr_layer_identity;  // The NSPR layer identity
};

}  // namespace rtc

#endif  // WEBRTC_BASE_NSSSTREAMADAPTER_H_
