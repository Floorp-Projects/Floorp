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
#include "tls_client_config.h"
#include "tls_client_socket.h"

static PRStatus EnableAllProtocolVersions() {
  SSLVersionRange supported;

  SECStatus rv = SSL_VersionRangeGetSupported(ssl_variant_stream, &supported);
  assert(rv == SECSuccess);

  rv = SSL_VersionRangeSetDefault(ssl_variant_stream, &supported);
  assert(rv == SECSuccess);

  return PR_SUCCESS;
}

static SECStatus AuthCertificateHook(void* arg, PRFileDesc* fd, PRBool checksig,
                                     PRBool isServer) {
  assert(!isServer);
  auto config = reinterpret_cast<ClientConfig*>(arg);
  return config->FailCertificateAuthentication() ? SECFailure : SECSuccess;
}

static void SetSocketOptions(PRFileDesc* fd,
                             std::unique_ptr<ClientConfig>& config) {
  // Disable session cache for now.
  SECStatus rv = SSL_OptionSet(fd, SSL_NO_CACHE, true);
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_ENABLE_EXTENDED_MASTER_SECRET,
                     config->EnableExtendedMasterSecret());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_REQUIRE_DH_NAMED_GROUPS,
                     config->RequireDhNamedGroups());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_ENABLE_FALSE_START, config->EnableFalseStart());
  assert(rv == SECSuccess);

  rv =
      SSL_OptionSet(fd, SSL_ENABLE_RENEGOTIATION, SSL_RENEGOTIATE_UNRESTRICTED);
  assert(rv == SECSuccess);
}

static void EnableAllCipherSuites(PRFileDesc* fd) {
  for (uint16_t i = 0; i < SSL_NumImplementedCiphers; ++i) {
    SECStatus rv = SSL_CipherPrefSet(fd, SSL_ImplementedCiphers[i], true);
    assert(rv == SECSuccess);
  }
}

// This is only called when we set SSL_ENABLE_FALSE_START=1,
// so we can always just set *canFalseStart=true.
static SECStatus CanFalseStartCallback(PRFileDesc* fd, void* arg,
                                       PRBool* canFalseStart) {
  *canFalseStart = true;
  return SECSuccess;
}

static void SetupCallbacks(PRFileDesc* fd, ClientConfig* config) {
  SECStatus rv = SSL_AuthCertificateHook(fd, AuthCertificateHook, config);
  assert(rv == SECSuccess);

  rv = SSL_SetCanFalseStartCallback(fd, CanFalseStartCallback, nullptr);
  assert(rv == SECSuccess);
}

static void DoHandshake(PRFileDesc* fd) {
  SECStatus rv = SSL_ResetHandshake(fd, false /* asServer */);
  assert(rv == SECSuccess);

  do {
    rv = SSL_ForceHandshake(fd);
  } while (rv != SECSuccess && PR_GetError() == PR_WOULD_BLOCK_ERROR);

  // If the handshake succeeds, let's read some data from the server, if any.
  if (rv == SECSuccess) {
    uint8_t block[1024];
    int32_t nb;

    // Read application data and echo it back.
    while ((nb = PR_Read(fd, block, sizeof(block))) > 0) {
      PR_Write(fd, block, nb);
    }
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t len) {
  static std::unique_ptr<NSSDatabase> db(new NSSDatabase());
  assert(db != nullptr);

  EnableAllProtocolVersions();
  std::unique_ptr<ClientConfig> config(new ClientConfig(data, len));

#ifdef UNSAFE_FUZZER_MODE
  // Reset the RNG state.
  SECStatus rv = RNG_ResetForFuzzing();
  assert(rv == SECSuccess);
#endif

  // Create and import dummy socket.
  std::unique_ptr<DummyPrSocket> socket(new DummyPrSocket(data, len));
  static PRDescIdentity id = PR_GetUniqueIdentity("fuzz-client");
  ScopedPRFileDesc fd(DummyIOLayerMethods::CreateFD(id, socket.get()));
  PRFileDesc* ssl_fd = SSL_ImportFD(nullptr, fd.get());
  assert(ssl_fd == fd.get());

  // Probably not too important for clients.
  SSL_SetURL(ssl_fd, "server");

  SetSocketOptions(ssl_fd, config);
  EnableAllCipherSuites(ssl_fd);
  SetupCallbacks(ssl_fd, config.get());
  DoHandshake(ssl_fd);

  return 0;
}
