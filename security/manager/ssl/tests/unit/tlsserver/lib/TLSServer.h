/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TLSServer_h
#define TLSServer_h

// This is a standalone server for testing SSL features of Gecko.
// The client is expected to connect and initiate an SSL handshake (with SNI
// to indicate which "server" to connect to). If all is good, the client then
// sends one encrypted byte and receives that same byte back.
// This server also has the ability to "call back" another process waiting on
// it. That is, when the server is all set up and ready to receive connections,
// it will connect to a specified port and issue a simple HTTP request.

#include <stdint.h>

#include "ScopedNSSTypes.h"
#include "mozilla/Casting.h"
#include "prio.h"
#include "secerr.h"
#include "ssl.h"

namespace mozilla {

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePRDir, PRDir, PR_CloseDir);

}  // namespace mozilla

namespace mozilla {
namespace test {

typedef SECStatus (*ServerConfigFunc)(PRFileDesc* fd);

enum DebugLevel { DEBUG_ERRORS = 1, DEBUG_WARNINGS = 2, DEBUG_VERBOSE = 3 };

extern DebugLevel gDebugLevel;

void PrintPRError(const char* aPrefix);

// The default certificate is trusted for localhost and *.example.com
extern const char DEFAULT_CERT_NICKNAME[];

// ConfigSecureServerWithNamedCert sets up the hostname name provided. If the
// extraData parameter is presented, extraData->certChain will be automatically
// filled in using database information.
// Pass DEFAULT_CERT_NICKNAME as certName unless you need a specific
// certificate.
SECStatus ConfigSecureServerWithNamedCert(
    PRFileDesc* fd, const char* certName,
    /*optional*/ UniqueCERTCertificate* cert,
    /*optional*/ SSLKEAType* kea,
    /*optional*/ SSLExtraServerCertData* extraData);

SECStatus InitializeNSS(const char* nssCertDBDir);

// StartServer initializes NSS, sockets, the SNI callback, and a default
// certificate. configFunc (optional) is a pointer to an implementation-
// defined configuration function, which is called on the model socket
// prior to handling any connections.
int StartServer(int argc, char* argv[], SSLSNISocketConfig sniSocketConfig,
                void* sniSocketConfigArg,
                ServerConfigFunc configFunc = nullptr);

template <typename Host>
inline const Host* GetHostForSNI(const SECItem* aSrvNameArr,
                                 uint32_t aSrvNameArrSize, const Host* hosts) {
  for (uint32_t i = 0; i < aSrvNameArrSize; i++) {
    for (const Host* host = hosts; host->mHostName; ++host) {
      SECItem hostName;
      hostName.data = BitwiseCast<unsigned char*, const char*>(host->mHostName);
      hostName.len = strlen(host->mHostName);
      if (SECITEM_ItemsAreEqual(&hostName, &aSrvNameArr[i])) {
        if (gDebugLevel >= DEBUG_VERBOSE) {
          fprintf(stderr, "found pre-defined host '%s'\n", host->mHostName);
        }
        return host;
      }
    }
  }

  if (gDebugLevel >= DEBUG_VERBOSE) {
    fprintf(stderr, "could not find host info from SNI\n");
  }

  PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
  return nullptr;
}

}  // namespace test
}  // namespace mozilla

#endif  // TLSServer_h
