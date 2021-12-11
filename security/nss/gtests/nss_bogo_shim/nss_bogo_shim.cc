/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "config.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include "nspr.h"
#include "nss.h"
#include "prio.h"
#include "prnetdb.h"
#include "secerr.h"
#include "ssl.h"
#include "ssl3prot.h"
#include "sslerr.h"
#include "sslproto.h"
#include "nss_scoped_ptrs.h"

#include "nsskeys.h"

static const char* kVersionDisableFlags[] = {"no-ssl3", "no-tls1", "no-tls11",
                                             "no-tls12", "no-tls13"};

bool exitCodeUnimplemented = false;

std::string FormatError(PRErrorCode code) {
  return std::string(":") + PORT_ErrorToName(code) + ":" + ":" +
         PORT_ErrorToString(code);
}

class TestAgent {
 public:
  TestAgent(const Config& cfg) : cfg_(cfg) {}

  ~TestAgent() {}

  static std::unique_ptr<TestAgent> Create(const Config& cfg) {
    std::unique_ptr<TestAgent> agent(new TestAgent(cfg));

    if (!agent->Init()) return nullptr;

    return agent;
  }

  bool Init() {
    if (!ConnectTcp()) {
      return false;
    }

    if (!SetupKeys()) {
      std::cerr << "Couldn't set up keys/certs\n";
      return false;
    }

    if (!SetupOptions()) {
      std::cerr << "Couldn't configure socket\n";
      return false;
    }

    SECStatus rv = SSL_ResetHandshake(ssl_fd_.get(), cfg_.get<bool>("server"));
    if (rv != SECSuccess) return false;

    return true;
  }

  bool ConnectTcp() {
    // Try IPv6 first, then IPv4 in case of failure.
    if (!OpenConnection("::1") && !OpenConnection("127.0.0.1")) {
      return false;
    }

    ssl_fd_ = ScopedPRFileDesc(SSL_ImportFD(NULL, pr_fd_.get()));
    if (!ssl_fd_) {
      return false;
    }
    pr_fd_.release();

    return true;
  }

  bool OpenConnection(const char* ip) {
    PRStatus prv;
    PRNetAddr addr;

    prv = PR_StringToNetAddr(ip, &addr);

    if (prv != PR_SUCCESS) {
      return false;
    }

    addr.inet.port = PR_htons(cfg_.get<int>("port"));

    pr_fd_ = ScopedPRFileDesc(PR_OpenTCPSocket(addr.raw.family));
    if (!pr_fd_) return false;

    prv = PR_Connect(pr_fd_.get(), &addr, PR_INTERVAL_NO_TIMEOUT);
    if (prv != PR_SUCCESS) {
      return false;
    }
    return true;
  }

  bool SetupKeys() {
    SECStatus rv;

    if (cfg_.get<std::string>("key-file") != "") {
      key_ = ScopedSECKEYPrivateKey(
          ReadPrivateKey(cfg_.get<std::string>("key-file")));
      if (!key_) return false;
    }
    if (cfg_.get<std::string>("cert-file") != "") {
      cert_ = ScopedCERTCertificate(
          ReadCertificate(cfg_.get<std::string>("cert-file")));
      if (!cert_) return false;
    }

    // Needed because certs are not entirely valid.
    rv = SSL_AuthCertificateHook(ssl_fd_.get(), AuthCertificateHook, this);
    if (rv != SECSuccess) return false;

    if (cfg_.get<bool>("server")) {
      // Server
      rv = SSL_ConfigServerCert(ssl_fd_.get(), cert_.get(), key_.get(), nullptr,
                                0);
      if (rv != SECSuccess) {
        std::cerr << "Couldn't configure server cert\n";
        return false;
      }

    } else if (key_ && cert_) {
      // Client.
      rv =
          SSL_GetClientAuthDataHook(ssl_fd_.get(), GetClientAuthDataHook, this);
      if (rv != SECSuccess) return false;
    }

    return true;
  }

  static bool ConvertFromWireVersion(SSLProtocolVariant variant,
                                     int wire_version, uint16_t* lib_version) {
    // These default values are used when {min,max}-version isn't given.
    if (wire_version == 0 || wire_version == 0xffff) {
      *lib_version = static_cast<uint16_t>(wire_version);
      return true;
    }

#ifdef TLS_1_3_DRAFT_VERSION
    if (wire_version == (0x7f00 | TLS_1_3_DRAFT_VERSION)) {
      // N.B. SSL_LIBRARY_VERSION_DTLS_1_3_WIRE == SSL_LIBRARY_VERSION_TLS_1_3
      wire_version = SSL_LIBRARY_VERSION_TLS_1_3;
    }
#endif

    if (variant == ssl_variant_datagram) {
      switch (wire_version) {
        case SSL_LIBRARY_VERSION_DTLS_1_0_WIRE:
          *lib_version = SSL_LIBRARY_VERSION_DTLS_1_0;
          break;
        case SSL_LIBRARY_VERSION_DTLS_1_2_WIRE:
          *lib_version = SSL_LIBRARY_VERSION_DTLS_1_2;
          break;
        case SSL_LIBRARY_VERSION_DTLS_1_3_WIRE:
          *lib_version = SSL_LIBRARY_VERSION_DTLS_1_3;
          break;
        default:
          std::cerr << "Unrecognized DTLS version " << wire_version << ".\n";
          return false;
      }
    } else {
      if (wire_version < SSL_LIBRARY_VERSION_3_0 ||
          wire_version > SSL_LIBRARY_VERSION_TLS_1_3) {
        std::cerr << "Unrecognized TLS version " << wire_version << ".\n";
        return false;
      }
      *lib_version = static_cast<uint16_t>(wire_version);
    }
    return true;
  }

  bool GetVersionRange(SSLVersionRange* range_out, SSLProtocolVariant variant) {
    SSLVersionRange supported;
    if (SSL_VersionRangeGetSupported(variant, &supported) != SECSuccess) {
      return false;
    }

    uint16_t min_allowed;
    uint16_t max_allowed;
    if (!ConvertFromWireVersion(variant, cfg_.get<int>("min-version"),
                                &min_allowed)) {
      return false;
    }
    if (!ConvertFromWireVersion(variant, cfg_.get<int>("max-version"),
                                &max_allowed)) {
      return false;
    }

    min_allowed = std::max(min_allowed, supported.min);
    max_allowed = std::min(max_allowed, supported.max);

    bool found_min = false;
    bool found_max = false;
    // Ignore -no-ssl3, because SSLv3 is never supported.
    for (size_t i = 1; i < PR_ARRAY_SIZE(kVersionDisableFlags); ++i) {
      auto version =
          static_cast<uint16_t>(SSL_LIBRARY_VERSION_TLS_1_0 + (i - 1));
      if (variant == ssl_variant_datagram) {
        // In DTLS mode, the -no-tlsN flags refer to DTLS versions,
        // but NSS wants the corresponding TLS versions.
        if (version == SSL_LIBRARY_VERSION_TLS_1_1) {
          // DTLS 1.1 doesn't exist.
          continue;
        }
        if (version == SSL_LIBRARY_VERSION_TLS_1_0) {
          version = SSL_LIBRARY_VERSION_DTLS_1_0;
        }
      }

      if (version < min_allowed) {
        continue;
      }
      if (version > max_allowed) {
        break;
      }

      const bool allowed = !cfg_.get<bool>(kVersionDisableFlags[i]);

      if (!found_min && allowed) {
        found_min = true;
        range_out->min = version;
      }
      if (found_min && !found_max) {
        if (allowed) {
          range_out->max = version;
        } else {
          found_max = true;
        }
      }
      if (found_max && allowed) {
        std::cerr << "Discontiguous version range.\n";
        return false;
      }
    }

    if (!found_min) {
      std::cerr << "All versions disabled.\n";
    }
    return found_min;
  }

  bool SetupOptions() {
    SECStatus rv =
        SSL_OptionSet(ssl_fd_.get(), SSL_ENABLE_TLS13_COMPAT_MODE, PR_TRUE);
    if (rv != SECSuccess) return false;

    rv = SSL_OptionSet(ssl_fd_.get(), SSL_ENABLE_SESSION_TICKETS, PR_TRUE);
    if (rv != SECSuccess) return false;

    SSLVersionRange vrange;
    if (!GetVersionRange(&vrange, ssl_variant_stream)) return false;

    rv = SSL_VersionRangeSet(ssl_fd_.get(), &vrange);
    if (rv != SECSuccess) return false;

    SSLVersionRange verify_vrange;
    rv = SSL_VersionRangeGet(ssl_fd_.get(), &verify_vrange);
    if (rv != SECSuccess) return false;
    if (vrange.min != verify_vrange.min || vrange.max != verify_vrange.max)
      return false;

    rv = SSL_OptionSet(ssl_fd_.get(), SSL_NO_CACHE, false);
    if (rv != SECSuccess) return false;

    auto alpn = cfg_.get<std::string>("advertise-alpn");
    if (!alpn.empty()) {
      assert(!cfg_.get<bool>("server"));

      rv = SSL_OptionSet(ssl_fd_.get(), SSL_ENABLE_ALPN, PR_TRUE);
      if (rv != SECSuccess) return false;

      rv = SSL_SetNextProtoNego(
          ssl_fd_.get(), reinterpret_cast<const unsigned char*>(alpn.c_str()),
          alpn.size());
      if (rv != SECSuccess) return false;
    }

    // Set supported signature schemes.
    auto sign_prefs = cfg_.get<std::vector<int>>("signing-prefs");
    auto verify_prefs = cfg_.get<std::vector<int>>("verify-prefs");
    if (sign_prefs.empty()) {
      sign_prefs = verify_prefs;
    } else if (!verify_prefs.empty()) {
      return false;  // Both shouldn't be set.
    }
    if (!sign_prefs.empty()) {
      std::vector<SSLSignatureScheme> sig_schemes;
      std::transform(
          sign_prefs.begin(), sign_prefs.end(), std::back_inserter(sig_schemes),
          [](int scheme) { return static_cast<SSLSignatureScheme>(scheme); });

      rv = SSL_SignatureSchemePrefSet(
          ssl_fd_.get(), sig_schemes.data(),
          static_cast<unsigned int>(sig_schemes.size()));
      if (rv != SECSuccess) return false;
    }

    if (cfg_.get<bool>("fallback-scsv")) {
      rv = SSL_OptionSet(ssl_fd_.get(), SSL_ENABLE_FALLBACK_SCSV, PR_TRUE);
      if (rv != SECSuccess) return false;
    }

    if (cfg_.get<bool>("false-start")) {
      rv = SSL_OptionSet(ssl_fd_.get(), SSL_ENABLE_FALSE_START, PR_TRUE);
      if (rv != SECSuccess) return false;
    }

    if (cfg_.get<bool>("enable-ocsp-stapling")) {
      rv = SSL_OptionSet(ssl_fd_.get(), SSL_ENABLE_OCSP_STAPLING, PR_TRUE);
      if (rv != SECSuccess) return false;
    }

    bool requireClientCert = cfg_.get<bool>("require-any-client-certificate");
    if (requireClientCert || cfg_.get<bool>("verify-peer")) {
      assert(cfg_.get<bool>("server"));

      rv = SSL_OptionSet(ssl_fd_.get(), SSL_REQUEST_CERTIFICATE, PR_TRUE);
      if (rv != SECSuccess) return false;

      rv = SSL_OptionSet(
          ssl_fd_.get(), SSL_REQUIRE_CERTIFICATE,
          requireClientCert ? SSL_REQUIRE_ALWAYS : SSL_REQUIRE_NO_ERROR);
      if (rv != SECSuccess) return false;
    }

    if (!cfg_.get<bool>("server")) {
      // Needed to make resumption work.
      rv = SSL_SetURL(ssl_fd_.get(), "server");
      if (rv != SECSuccess) return false;
    }

    rv = SSL_OptionSet(ssl_fd_.get(), SSL_ENABLE_EXTENDED_MASTER_SECRET,
                       PR_TRUE);
    if (rv != SECSuccess) return false;

    if (!ConfigureCiphers()) return false;

    return true;
  }

  bool ConfigureCiphers() {
    auto cipherList = cfg_.get<std::string>("nss-cipher");

    if (cipherList.empty()) {
      return EnableNonExportCiphers();
    }

    for (size_t i = 0; i < SSL_NumImplementedCiphers; ++i) {
      SSLCipherSuiteInfo csinfo;
      std::string::size_type n;
      SECStatus rv = SSL_GetCipherSuiteInfo(SSL_ImplementedCiphers[i], &csinfo,
                                            sizeof(csinfo));
      if (rv != SECSuccess) {
        return false;
      }

      // Check if cipherList contains the name of the Cipher Suite and
      // enable/disable accordingly.
      n = cipherList.find(csinfo.cipherSuiteName, 0);
      if (std::string::npos == n) {
        rv = SSL_CipherPrefSet(ssl_fd_.get(), SSL_ImplementedCiphers[i],
                               PR_FALSE);
      } else {
        rv = SSL_CipherPrefSet(ssl_fd_.get(), SSL_ImplementedCiphers[i],
                               PR_TRUE);
      }
      if (rv != SECSuccess) {
        return false;
      }
    }
    return true;
  }

  bool EnableNonExportCiphers() {
    for (size_t i = 0; i < SSL_NumImplementedCiphers; ++i) {
      SSLCipherSuiteInfo csinfo;

      SECStatus rv = SSL_GetCipherSuiteInfo(SSL_ImplementedCiphers[i], &csinfo,
                                            sizeof(csinfo));
      if (rv != SECSuccess) {
        return false;
      }

      rv = SSL_CipherPrefSet(ssl_fd_.get(), SSL_ImplementedCiphers[i], PR_TRUE);
      if (rv != SECSuccess) {
        return false;
      }
    }
    return true;
  }

  // Dummy auth certificate hook.
  static SECStatus AuthCertificateHook(void* arg, PRFileDesc* fd,
                                       PRBool checksig, PRBool isServer) {
    return SECSuccess;
  }

  static SECStatus GetClientAuthDataHook(void* self, PRFileDesc* fd,
                                         CERTDistNames* caNames,
                                         CERTCertificate** cert,
                                         SECKEYPrivateKey** privKey) {
    TestAgent* a = static_cast<TestAgent*>(self);
    *cert = CERT_DupCertificate(a->cert_.get());
    *privKey = SECKEY_CopyPrivateKey(a->key_.get());
    return SECSuccess;
  }

  SECStatus Handshake() { return SSL_ForceHandshake(ssl_fd_.get()); }

  // Implement a trivial echo client/server. Read bytes from the other side,
  // flip all the bits, and send them back.
  SECStatus ReadWrite() {
    for (;;) {
      uint8_t block[512];
      int32_t rv = PR_Read(ssl_fd_.get(), block, sizeof(block));
      if (rv < 0) {
        std::cerr << "Failure reading\n";
        return SECFailure;
      }
      if (rv == 0) return SECSuccess;

      int32_t len = rv;
      for (int32_t i = 0; i < len; ++i) {
        block[i] ^= 0xff;
      }

      rv = PR_Write(ssl_fd_.get(), block, len);
      if (rv != len) {
        std::cerr << "Write failure\n";
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
      }
    }
    return SECSuccess;
  }

  // Write bytes to the other side then read them back and check
  // that they were correctly XORed as in ReadWrite.
  SECStatus WriteRead() {
    static const uint8_t ch = 'E';

    // We do 600-byte blocks to provide mis-alignment of the
    // reader and writer.
    uint8_t block[600];
    memset(block, ch, sizeof(block));
    int32_t rv = PR_Write(ssl_fd_.get(), block, sizeof(block));
    if (rv != sizeof(block)) {
      std::cerr << "Write failure\n";
      PORT_SetError(SEC_ERROR_OUTPUT_LEN);
      return SECFailure;
    }

    size_t left = sizeof(block);
    while (left) {
      rv = PR_Read(ssl_fd_.get(), block, left);
      if (rv < 0) {
        std::cerr << "Failure reading\n";
        return SECFailure;
      }
      if (rv == 0) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
      }

      int32_t len = rv;
      for (int32_t i = 0; i < len; ++i) {
        if (block[i] != (ch ^ 0xff)) {
          PORT_SetError(SEC_ERROR_BAD_DATA);
          return SECFailure;
        }
      }
      left -= len;
    }
    return SECSuccess;
  }

  SECStatus DoExchange() {
    SECStatus rv = Handshake();
    if (rv != SECSuccess) {
      PRErrorCode err = PR_GetError();
      std::cerr << "Handshake failed with error=" << err << FormatError(err)
                << std::endl;
      return SECFailure;
    }

    if (cfg_.get<bool>("write-then-read")) {
      rv = WriteRead();
      if (rv != SECSuccess) {
        PRErrorCode err = PR_GetError();
        std::cerr << "WriteRead failed with error=" << FormatError(err)
                  << std::endl;
        return SECFailure;
      }
    } else {
      rv = ReadWrite();
      if (rv != SECSuccess) {
        PRErrorCode err = PR_GetError();
        std::cerr << "ReadWrite failed with error=" << FormatError(err)
                  << std::endl;
        return SECFailure;
      }
    }

    auto alpn = cfg_.get<std::string>("expect-alpn");
    if (!alpn.empty()) {
      SSLNextProtoState state;
      char chosen[256];
      unsigned int chosen_len;
      rv = SSL_GetNextProto(ssl_fd_.get(), &state,
                            reinterpret_cast<unsigned char*>(chosen),
                            &chosen_len, sizeof(chosen));
      if (rv != SECSuccess) {
        PRErrorCode err = PR_GetError();
        std::cerr << "SSL_GetNextProto failed with error=" << FormatError(err)
                  << std::endl;
        return SECFailure;
      }

      assert(chosen_len <= sizeof(chosen));
      if (std::string(chosen, chosen_len) != alpn) {
        std::cerr << "Unexpected ALPN selection" << std::endl;
        return SECFailure;
      }
    }

    auto sig_alg = cfg_.get<int>("expect-peer-signature-algorithm");
    if (sig_alg) {
      SSLChannelInfo info;
      rv = SSL_GetChannelInfo(ssl_fd_.get(), &info, sizeof(info));
      if (rv != SECSuccess) {
        PRErrorCode err = PR_GetError();
        std::cerr << "SSL_GetChannelInfo failed with error=" << FormatError(err)
                  << std::endl;
        return SECFailure;
      }

      auto expected = static_cast<SSLSignatureScheme>(sig_alg);
      if (info.signatureScheme != expected) {
        std::cerr << "Unexpected signature scheme" << std::endl;
        return SECFailure;
      }
    }

    return SECSuccess;
  }

 private:
  const Config& cfg_;
  ScopedPRFileDesc pr_fd_;
  ScopedPRFileDesc ssl_fd_;
  ScopedCERTCertificate cert_;
  ScopedSECKEYPrivateKey key_;
};

std::unique_ptr<const Config> ReadConfig(int argc, char** argv) {
  std::unique_ptr<Config> cfg(new Config());

  cfg->AddEntry<int>("port", 0);
  cfg->AddEntry<bool>("server", false);
  cfg->AddEntry<int>("resume-count", 0);
  cfg->AddEntry<std::string>("key-file", "");
  cfg->AddEntry<std::string>("cert-file", "");
  cfg->AddEntry<int>("min-version", 0);
  cfg->AddEntry<int>("max-version", 0xffff);
  for (auto flag : kVersionDisableFlags) {
    cfg->AddEntry<bool>(flag, false);
  }
  cfg->AddEntry<bool>("fallback-scsv", false);
  cfg->AddEntry<bool>("false-start", false);
  cfg->AddEntry<bool>("enable-ocsp-stapling", false);
  cfg->AddEntry<bool>("write-then-read", false);
  cfg->AddEntry<bool>("require-any-client-certificate", false);
  cfg->AddEntry<bool>("verify-peer", false);
  cfg->AddEntry<bool>("is-handshaker-supported", false);
  cfg->AddEntry<std::string>("handshaker-path", "");  // Ignore this
  cfg->AddEntry<std::string>("advertise-alpn", "");
  cfg->AddEntry<std::string>("expect-alpn", "");
  cfg->AddEntry<std::vector<int>>("signing-prefs", std::vector<int>());
  cfg->AddEntry<std::vector<int>>("verify-prefs", std::vector<int>());
  cfg->AddEntry<int>("expect-peer-signature-algorithm", 0);
  cfg->AddEntry<std::string>("nss-cipher", "");

  auto rv = cfg->ParseArgs(argc, argv);
  switch (rv) {
    case Config::kOK:
      break;
    case Config::kUnknownFlag:
      exitCodeUnimplemented = true;
    default:
      return nullptr;
  }

  // Needed to change to std::unique_ptr<const Config>
  return std::move(cfg);
}

bool RunCycle(std::unique_ptr<const Config>& cfg) {
  std::unique_ptr<TestAgent> agent(TestAgent::Create(*cfg));
  return agent && agent->DoExchange() == SECSuccess;
}

int GetExitCode(bool success) {
  if (exitCodeUnimplemented) {
    return 89;
  }

  if (success) {
    return 0;
  }

  return 1;
}

int main(int argc, char** argv) {
  std::unique_ptr<const Config> cfg = ReadConfig(argc, argv);
  if (!cfg) {
    return GetExitCode(false);
  }

  if (cfg->get<bool>("is-handshaker-supported")) {
    std::cout << "No\n";
    return 0;
  }

  if (cfg->get<bool>("server")) {
    if (SSL_ConfigServerSessionIDCache(1024, 0, 0, ".") != SECSuccess) {
      std::cerr << "Couldn't configure session cache\n";
      return 1;
    }
  }

  if (NSS_NoDB_Init(nullptr) != SECSuccess) {
    return 1;
  }

  // Run a single test cycle.
  bool success = RunCycle(cfg);

  int resume_count = cfg->get<int>("resume-count");
  while (success && resume_count-- > 0) {
    std::cout << "Resuming" << std::endl;
    success = RunCycle(cfg);
  }

  SSL_ClearSessionCache();

  if (cfg->get<bool>("server")) {
    SSL_ShutdownServerSessionIDCache();
  }

  if (NSS_Shutdown() != SECSuccess) {
    success = false;
  }

  return GetExitCode(success);
}
