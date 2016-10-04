/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "config.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include "nspr.h"
#include "nss.h"
#include "prio.h"
#include "prnetdb.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

#include "nsskeys.h"

bool exitCodeUnimplemented = false;

std::string FormatError(PRErrorCode code) {
  return std::string(":") + PORT_ErrorToName(code) + ":" + ":" +
         PORT_ErrorToString(code);
}

class TestAgent {
 public:
  TestAgent(const Config& cfg)
      : cfg_(cfg),
        pr_fd_(nullptr),
        ssl_fd_(nullptr),
        cert_(nullptr),
        key_(nullptr) {}

  ~TestAgent() {
    if (pr_fd_) {
      PR_Close(pr_fd_);
    }

    if (ssl_fd_) {
      PR_Close(ssl_fd_);
    }

    if (key_) {
      SECKEY_DestroyPrivateKey(key_);
    }

    if (cert_) {
      CERT_DestroyCertificate(cert_);
    }
  }

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

    SECStatus rv = SSL_ResetHandshake(ssl_fd_, cfg_.get<bool>("server"));
    if (rv != SECSuccess) return false;

    return true;
  }

  bool ConnectTcp() {
    PRStatus prv;
    PRNetAddr addr;

    prv = PR_StringToNetAddr("127.0.0.1", &addr);
    if (prv != PR_SUCCESS) {
      return false;
    }
    addr.inet.port = PR_htons(cfg_.get<int>("port"));

    pr_fd_ = PR_OpenTCPSocket(addr.raw.family);
    if (!pr_fd_) return false;

    prv = PR_Connect(pr_fd_, &addr, PR_INTERVAL_NO_TIMEOUT);
    if (prv != PR_SUCCESS) {
      return false;
    }

    ssl_fd_ = SSL_ImportFD(NULL, pr_fd_);
    if (!ssl_fd_) return false;
    pr_fd_ = nullptr;

    return true;
  }

  bool SetupKeys() {
    SECStatus rv;

    if (cfg_.get<std::string>("key-file") != "") {
      key_ = ReadPrivateKey(cfg_.get<std::string>("key-file"));
      if (!key_) {
        // Temporary to handle our inability to handle ECDSA.
        exitCodeUnimplemented = true;
        return false;
      }
    }
    if (cfg_.get<std::string>("cert-file") != "") {
      cert_ = ReadCertificate(cfg_.get<std::string>("cert-file"));
      if (!cert_) return false;
    }
    if (cfg_.get<bool>("server")) {
      // Server
      rv = SSL_ConfigServerCert(ssl_fd_, cert_, key_, nullptr, 0);
      if (rv != SECSuccess) {
        std::cerr << "Couldn't configure server cert\n";
        return false;
      }
    } else {
      // Client.

      // Needed because server certs are not entirely valid.
      rv = SSL_AuthCertificateHook(ssl_fd_, AuthCertificateHook, this);
      if (rv != SECSuccess) return false;

      if (key_ && cert_) {
        rv = SSL_GetClientAuthDataHook(ssl_fd_, GetClientAuthDataHook, this);
        if (rv != SECSuccess) return false;
      }
    }

    return true;
  }

  bool SetupOptions() {
    SECStatus rv = SSL_OptionSet(ssl_fd_, SSL_ENABLE_SESSION_TICKETS, PR_TRUE);
    if (rv != SECSuccess) return false;

    SSLVersionRange vrange = {SSL_LIBRARY_VERSION_TLS_1_0,
                              SSL_LIBRARY_VERSION_TLS_1_3};
    rv = SSL_VersionRangeSet(ssl_fd_, &vrange);
    if (rv != SECSuccess) return false;

    rv = SSL_OptionSet(ssl_fd_, SSL_NO_CACHE, false);
    if (rv != SECSuccess) return false;

    if (!cfg_.get<bool>("server")) {
      // Needed to make resumption work.
      rv = SSL_SetURL(ssl_fd_, "server");
      if (rv != SECSuccess) return false;
    }

    rv = SSL_OptionSet(ssl_fd_, SSL_ENABLE_EXTENDED_MASTER_SECRET, PR_TRUE);
    if (rv != SECSuccess) return false;

    if (!EnableNonExportCiphers()) return false;

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

      rv = SSL_CipherPrefSet(ssl_fd_, SSL_ImplementedCiphers[i], PR_TRUE);
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
    *cert = CERT_DupCertificate(a->cert_);
    *privKey = SECKEY_CopyPrivateKey(a->key_);
    return SECSuccess;
  }

  SECStatus Handshake() { return SSL_ForceHandshake(ssl_fd_); }

  // Implement a trivial echo client/server. Read bytes from the other side,
  // flip all the bits, and send them back.
  SECStatus ReadWrite() {
    for (;;) {
      uint8_t block[512];
      int32_t rv = PR_Read(ssl_fd_, block, sizeof(block));
      if (rv < 0) {
        std::cerr << "Failure reading\n";
        return SECFailure;
      }
      if (rv == 0) return SECSuccess;

      int32_t len = rv;
      for (int32_t i = 0; i < len; ++i) {
        block[i] ^= 0xff;
      }

      rv = PR_Write(ssl_fd_, block, len);
      if (rv != len) {
        std::cerr << "Write failure\n";
        return SECFailure;
      }
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

    rv = ReadWrite();
    if (rv != SECSuccess) {
      PRErrorCode err = PR_GetError();
      std::cerr << "ReadWrite failed with error=" << FormatError(err)
                << std::endl;
      return SECFailure;
    }

    return SECSuccess;
  }

 private:
  const Config& cfg_;
  PRFileDesc* pr_fd_;
  PRFileDesc* ssl_fd_;
  CERTCertificate* cert_;
  SECKEYPrivateKey* key_;
};

std::unique_ptr<const Config> ReadConfig(int argc, char** argv) {
  std::unique_ptr<Config> cfg(new Config());

  cfg->AddEntry<int>("port", 0);
  cfg->AddEntry<bool>("server", false);
  cfg->AddEntry<bool>("resume", false);
  cfg->AddEntry<std::string>("key-file", "");
  cfg->AddEntry<std::string>("cert-file", "");

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

  if (success && cfg->get<bool>("resume")) {
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
