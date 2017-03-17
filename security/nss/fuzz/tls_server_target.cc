/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <stdint.h>
#include <memory>

#include "blapi.h"
#include "prinit.h"
#include "ssl.h"

#include "shared.h"
#include "tls_common.h"
#include "tls_server_certs.h"
#include "tls_server_config.h"
#include "tls_socket.h"

class SSLServerSessionCache {
 public:
  SSLServerSessionCache() {
    assert(SSL_ConfigServerSessionIDCache(1024, 0, 0, ".") == SECSuccess);
  }

  ~SSLServerSessionCache() {
    assert(SSL_ShutdownServerSessionIDCache() == SECSuccess);
  }
};

static void SetSocketOptions(PRFileDesc* fd,
                             std::unique_ptr<ServerConfig>& config) {
  SECStatus rv = SSL_OptionSet(fd, SSL_NO_CACHE, config->EnableCache());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_REUSE_SERVER_ECDHE_KEY, false);
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_ENABLE_EXTENDED_MASTER_SECRET,
                     config->EnableExtendedMasterSecret());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_REQUEST_CERTIFICATE, config->RequestCertificate());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_REQUIRE_CERTIFICATE, config->RequireCertificate());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_ENABLE_DEFLATE, config->EnableDeflate());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_CBC_RANDOM_IV, config->EnableCbcRandomIv());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_REQUIRE_SAFE_NEGOTIATION,
                     config->RequireSafeNegotiation());
  assert(rv == SECSuccess);

  rv =
      SSL_OptionSet(fd, SSL_ENABLE_RENEGOTIATION, SSL_RENEGOTIATE_UNRESTRICTED);
  assert(rv == SECSuccess);
}

static PRStatus InitModelSocket(void* arg) {
  PRFileDesc* fd = reinterpret_cast<PRFileDesc*>(arg);

  EnableAllProtocolVersions();
  EnableAllCipherSuites(fd);
  InstallServerCertificates(fd);

  return PR_SUCCESS;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t len) {
  static std::unique_ptr<NSSDatabase> db(new NSSDatabase());
  assert(db != nullptr);

  static std::unique_ptr<SSLServerSessionCache> cache(
      new SSLServerSessionCache());
  assert(cache != nullptr);

  std::unique_ptr<ServerConfig> config(new ServerConfig(data, len));

  // Clear the cache. We never want to resume as we couldn't reproduce that.
  SSL_ClearSessionCache();

#ifdef UNSAFE_FUZZER_MODE
  // Reset the RNG state.
  assert(RNG_ResetForFuzzing() == SECSuccess);
#endif

  // Create model socket.
  static ScopedPRFileDesc model(SSL_ImportFD(nullptr, PR_NewTCPSocket()));
  assert(model);

  // Initialize the model socket once.
  static PRCallOnceType initModelOnce;
  PR_CallOnceWithArg(&initModelOnce, InitModelSocket, model.get());

  // Create and import dummy socket.
  std::unique_ptr<DummyPrSocket> socket(new DummyPrSocket(data, len));
  static PRDescIdentity id = PR_GetUniqueIdentity("fuzz-server");
  ScopedPRFileDesc fd(DummyIOLayerMethods::CreateFD(id, socket.get()));
  PRFileDesc* ssl_fd = SSL_ImportFD(model.get(), fd.get());
  assert(ssl_fd == fd.get());

  SetSocketOptions(ssl_fd, config);
  DoHandshake(ssl_fd, true);

  return 0;
}
