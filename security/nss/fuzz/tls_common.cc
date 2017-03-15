/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>

#include "ssl.h"

#include "tls_common.h"

PRStatus EnableAllProtocolVersions() {
  SSLVersionRange supported;

  SECStatus rv = SSL_VersionRangeGetSupported(ssl_variant_stream, &supported);
  assert(rv == SECSuccess);

  rv = SSL_VersionRangeSetDefault(ssl_variant_stream, &supported);
  assert(rv == SECSuccess);

  return PR_SUCCESS;
}

void EnableAllCipherSuites(PRFileDesc* fd) {
  for (uint16_t i = 0; i < SSL_NumImplementedCiphers; ++i) {
    SECStatus rv = SSL_CipherPrefSet(fd, SSL_ImplementedCiphers[i], true);
    assert(rv == SECSuccess);
  }
}

void DoHandshake(PRFileDesc* fd, bool isServer) {
  SECStatus rv = SSL_ResetHandshake(fd, isServer);
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
