/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TLSServer.h"

#include <stdio.h>
#include "ScopedNSSTypes.h"
#include "nspr.h"
#include "nss.h"
#include "plarenas.h"
#include "prenv.h"
#include "prerror.h"
#include "prnetdb.h"
#include "prtime.h"
#include "ssl.h"

namespace mozilla { namespace test {

static const uint16_t LISTEN_PORT = 8443;

DebugLevel gDebugLevel = DEBUG_ERRORS;
uint16_t gCallbackPort = 0;

const char DEFAULT_CERT_NICKNAME[] = "localhostAndExampleCom";

struct Connection
{
  PRFileDesc *mSocket;
  char mByte;

  explicit Connection(PRFileDesc *aSocket);
  ~Connection();
};

Connection::Connection(PRFileDesc *aSocket)
: mSocket(aSocket)
, mByte(0)
{}

Connection::~Connection()
{
  if (mSocket) {
    PR_Close(mSocket);
  }
}

void
PrintPRError(const char *aPrefix)
{
  const char *err = PR_ErrorToName(PR_GetError());
  if (err) {
    if (gDebugLevel >= DEBUG_ERRORS) {
      fprintf(stderr, "%s: %s\n", aPrefix, err);
    }
  } else {
    if (gDebugLevel >= DEBUG_ERRORS) {
      fprintf(stderr, "%s\n", aPrefix);
    }
  }
}

nsresult
SendAll(PRFileDesc *aSocket, const char *aData, size_t aDataLen)
{
  if (gDebugLevel >= DEBUG_VERBOSE) {
    fprintf(stderr, "sending '%s'\n", aData);
  }

  while (aDataLen > 0) {
    int32_t bytesSent = PR_Send(aSocket, aData, aDataLen, 0,
                                PR_INTERVAL_NO_TIMEOUT);
    if (bytesSent == -1) {
      PrintPRError("PR_Send failed");
      return NS_ERROR_FAILURE;
    }

    aDataLen -= bytesSent;
    aData += bytesSent;
  }

  return NS_OK;
}

nsresult
ReplyToRequest(Connection *aConn)
{
  // For debugging purposes, SendAll can print out what it's sending.
  // So, any strings we give to it to send need to be null-terminated.
  char buf[2] = { aConn->mByte, 0 };
  return SendAll(aConn->mSocket, buf, 1);
}

nsresult
SetupTLS(Connection *aConn, PRFileDesc *aModelSocket)
{
  PRFileDesc *sslSocket = SSL_ImportFD(aModelSocket, aConn->mSocket);
  if (!sslSocket) {
    PrintPRError("SSL_ImportFD failed");
    return NS_ERROR_FAILURE;
  }
  aConn->mSocket = sslSocket;

  SSL_OptionSet(sslSocket, SSL_SECURITY, true);
  SSL_OptionSet(sslSocket, SSL_HANDSHAKE_AS_CLIENT, false);
  SSL_OptionSet(sslSocket, SSL_HANDSHAKE_AS_SERVER, true);

  SSL_ResetHandshake(sslSocket, /* asServer */ 1);

  return NS_OK;
}

nsresult
ReadRequest(Connection *aConn)
{
  int32_t bytesRead = PR_Recv(aConn->mSocket, &aConn->mByte, 1, 0,
                              PR_INTERVAL_NO_TIMEOUT);
  if (bytesRead < 0) {
    PrintPRError("PR_Recv failed");
    return NS_ERROR_FAILURE;
  } else if (bytesRead == 0) {
    PR_SetError(PR_IO_ERROR, 0);
    PrintPRError("PR_Recv EOF in ReadRequest");
    return NS_ERROR_FAILURE;
  } else {
    if (gDebugLevel >= DEBUG_VERBOSE) {
      fprintf(stderr, "read '0x%hhx'\n", aConn->mByte);
    }
  }
  return NS_OK;
}

void
HandleConnection(PRFileDesc *aSocket, PRFileDesc *aModelSocket)
{
  Connection conn(aSocket);
  nsresult rv = SetupTLS(&conn, aModelSocket);
  if (NS_FAILED(rv)) {
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    PrintPRError("PR_Recv failed");
    exit(1);
  }

  // TODO: On tests that are expected to fail (e.g. due to a revoked
  // certificate), the client will close the connection wtihout sending us the
  // request byte. In those cases, we should keep going. But, in the cases
  // where the connection is supposed to suceed, we should verify that we
  // successfully receive the request and send the response.
  rv = ReadRequest(&conn);
  if (NS_SUCCEEDED(rv)) {
    rv = ReplyToRequest(&conn);
  }
}

// returns 0 on success, non-zero on error
int
DoCallback()
{
  ScopedPRFileDesc socket(PR_NewTCPSocket());
  if (!socket) {
    PrintPRError("PR_NewTCPSocket failed");
    return 1;
  }

  PRNetAddr addr;
  PR_InitializeNetAddr(PR_IpAddrLoopback, gCallbackPort, &addr);
  if (PR_Connect(socket, &addr, PR_INTERVAL_NO_TIMEOUT) != PR_SUCCESS) {
    PrintPRError("PR_Connect failed");
    return 1;
  }

  const char *request = "GET / HTTP/1.0\r\n\r\n";
  SendAll(socket, request, strlen(request));
  char buf[4096];
  memset(buf, 0, sizeof(buf));
  int32_t bytesRead = PR_Recv(socket, buf, sizeof(buf) - 1, 0,
                              PR_INTERVAL_NO_TIMEOUT);
  if (bytesRead < 0) {
    PrintPRError("PR_Recv failed 1");
    return 1;
  }
  if (bytesRead == 0) {
    fprintf(stderr, "PR_Recv eof 1\n");
    return 1;
  }
  fprintf(stderr, "%s\n", buf);
  return 0;
}

SECStatus
ConfigSecureServerWithNamedCert(PRFileDesc *fd, const char *certName,
                                /*optional*/ ScopedCERTCertificate *certOut,
                                /*optional*/ SSLKEAType *keaOut)
{
  ScopedCERTCertificate cert(PK11_FindCertFromNickname(certName, nullptr));
  if (!cert) {
    PrintPRError("PK11_FindCertFromNickname failed");
    return SECFailure;
  }
  // If an intermediate certificate issued the server certificate (rather than
  // directly by a trust anchor), we want to send it along in the handshake so
  // we don't encounter unknown issuer errors when that's not what we're
  // testing.
  ScopedCERTCertificateList certList;
  ScopedCERTCertificate issuerCert(
    CERT_FindCertByName(CERT_GetDefaultCertDB(), &cert->derIssuer));
  // If we can't find the issuer cert, continue without it.
  if (issuerCert) {
    // Sadly, CERTCertificateList does not have a CERT_NewCertificateList
    // utility function, so we must create it ourselves. This consists
    // of creating an arena, allocating space for the CERTCertificateList,
    // and then transferring ownership of the arena to that list.
    ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
    if (!arena) {
      PrintPRError("PORT_NewArena failed");
      return SECFailure;
    }
    certList = reinterpret_cast<CERTCertificateList*>(
      PORT_ArenaAlloc(arena, sizeof(CERTCertificateList)));
    if (!certList) {
      PrintPRError("PORT_ArenaAlloc failed");
      return SECFailure;
    }
    certList->arena = arena.forget();
    // We also have to manually copy the certificates we care about to the
    // list, because there aren't any utility functions for that either.
    certList->certs = reinterpret_cast<SECItem*>(
      PORT_ArenaAlloc(certList->arena, 2 * sizeof(SECItem)));
    if (SECITEM_CopyItem(certList->arena, certList->certs, &cert->derCert)
          != SECSuccess) {
      PrintPRError("SECITEM_CopyItem failed");
      return SECFailure;
    }
    if (SECITEM_CopyItem(certList->arena, certList->certs + 1,
                         &issuerCert->derCert) != SECSuccess) {
      PrintPRError("SECITEM_CopyItem failed");
      return SECFailure;
    }
    certList->len = 2;
  }

  ScopedSECKEYPrivateKey key(PK11_FindKeyByAnyCert(cert, nullptr));
  if (!key) {
    PrintPRError("PK11_FindKeyByAnyCert failed");
    return SECFailure;
  }

  SSLKEAType certKEA = NSS_FindCertKEAType(cert);

  if (SSL_ConfigSecureServerWithCertChain(fd, cert, certList, key, certKEA)
        != SECSuccess) {
    PrintPRError("SSL_ConfigSecureServer failed");
    return SECFailure;
  }

  if (certOut) {
    *certOut = cert.forget();
  }

  if (keaOut) {
    *keaOut = certKEA;
  }

  return SECSuccess;
}

int
StartServer(const char *nssCertDBDir, SSLSNISocketConfig sniSocketConfig,
            void *sniSocketConfigArg)
{
  const char *debugLevel = PR_GetEnv("MOZ_TLS_SERVER_DEBUG_LEVEL");
  if (debugLevel) {
    int level = atoi(debugLevel);
    switch (level) {
      case DEBUG_ERRORS: gDebugLevel = DEBUG_ERRORS; break;
      case DEBUG_WARNINGS: gDebugLevel = DEBUG_WARNINGS; break;
      case DEBUG_VERBOSE: gDebugLevel = DEBUG_VERBOSE; break;
      default:
        PrintPRError("invalid MOZ_TLS_SERVER_DEBUG_LEVEL");
        return 1;
    }
  }

  const char *callbackPort = PR_GetEnv("MOZ_TLS_SERVER_CALLBACK_PORT");
  if (callbackPort) {
    gCallbackPort = atoi(callbackPort);
  }

  if (NSS_Init(nssCertDBDir) != SECSuccess) {
    PrintPRError("NSS_Init failed");
    return 1;
  }

  if (NSS_SetDomesticPolicy() != SECSuccess) {
    PrintPRError("NSS_SetDomesticPolicy failed");
    return 1;
  }

  if (SSL_ConfigServerSessionIDCache(0, 0, 0, nullptr) != SECSuccess) {
    PrintPRError("SSL_ConfigServerSessionIDCache failed");
    return 1;
  }

  ScopedPRFileDesc serverSocket(PR_NewTCPSocket());
  if (!serverSocket) {
    PrintPRError("PR_NewTCPSocket failed");
    return 1;
  }

  PRSocketOptionData socketOption;
  socketOption.option = PR_SockOpt_Reuseaddr;
  socketOption.value.reuse_addr = true;
  PR_SetSocketOption(serverSocket, &socketOption);

  PRNetAddr serverAddr;
  PR_InitializeNetAddr(PR_IpAddrLoopback, LISTEN_PORT, &serverAddr);
  if (PR_Bind(serverSocket, &serverAddr) != PR_SUCCESS) {
    PrintPRError("PR_Bind failed");
    return 1;
  }

  if (PR_Listen(serverSocket, 1) != PR_SUCCESS) {
    PrintPRError("PR_Listen failed");
    return 1;
  }

  ScopedPRFileDesc rawModelSocket(PR_NewTCPSocket());
  if (!rawModelSocket) {
    PrintPRError("PR_NewTCPSocket failed for rawModelSocket");
    return 1;
  }

  ScopedPRFileDesc modelSocket(SSL_ImportFD(nullptr, rawModelSocket.forget()));
  if (!modelSocket) {
    PrintPRError("SSL_ImportFD of rawModelSocket failed");
    return 1;
  }

  if (SECSuccess != SSL_SNISocketConfigHook(modelSocket, sniSocketConfig,
                                            sniSocketConfigArg)) {
    PrintPRError("SSL_SNISocketConfigHook failed");
    return 1;
  }

  // We have to configure the server with a certificate, but it's not one
  // we're actually going to end up using. In the SNI callback, we pick
  // the right certificate for the connection.
  if (SECSuccess != ConfigSecureServerWithNamedCert(modelSocket,
                                                    DEFAULT_CERT_NICKNAME,
                                                    nullptr, nullptr)) {
    return 1;
  }

  if (gCallbackPort != 0) {
    if (DoCallback()) {
      return 1;
    }
  }

  while (true) {
    PRNetAddr clientAddr;
    PRFileDesc *clientSocket = PR_Accept(serverSocket, &clientAddr,
                                         PR_INTERVAL_NO_TIMEOUT);
    HandleConnection(clientSocket, modelSocket);
  }

  return 0;
}

} } // namespace mozilla::test
