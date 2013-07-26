/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a standalone server that delivers various stapled OCSP responses.
// The client is expected to connect, initiate an SSL handshake (with SNI
// to indicate which "server" to connect to), and verify the OCSP response.
// If all is good, the client then sends one encrypted byte and receives that
// same byte back.
// This server also has the ability to "call back" another process waiting on
// it. That is, when the server is all set up and ready to receive connections,
// it will connect to a specified port and issue a simple HTTP request.

#include <stdio.h>
#include "ScopedNSSTypes.h"
#include "nspr.h"
#include "nss.h"
#include "ocsp.h"
#include "ocspt.h"
#include "plarenas.h"
#include "prenv.h"
#include "prerror.h"
#include "prnetdb.h"
#include "prtime.h"
#include "ssl.h"
#include "secerr.h"

using namespace mozilla;

#define LISTEN_PORT 8443
#define DEBUG_ERRORS 1
#define DEBUG_WARNINGS 2
#define DEBUG_VERBOSE 3

uint32_t gDebugLevel = 0;
uint32_t gCallbackPort = 0;

enum OCSPStapleResponseType
{
  OSRTNull = 0,
  OSRTGood,             // the certificate is good
  OSRTRevoked,          // the certificate has been revoked
  OSRTUnknown,          // the responder doesn't know if the cert is good
  OSRTGoodOtherCert,    // the response references a different certificate
  OSRTGoodOtherCA,      // the wrong CA has signed the response
  OSRTExpired,          // the signature on the response has expired
  OSRTExpiredFreshCA,   // fresh signature, but old validity period
  OSRTNone,             // no stapled response
  OSRTMalformed,        // the response from the responder was malformed
  OSRTSrverr,           // the response indicates there was a server error
  OSRTTryLater,         // the responder replied with "try again later"
  OSRTNeedsSig,         // the response needs a signature
  OSRTUnauthorized      // the responder is not authorized for this certificate
};

struct OCSPHost
{
  const char *mHostName;
  const char *mCertName;
  OCSPStapleResponseType mOSRT;
};

const OCSPHost sOCSPHosts[] =
{
  { "ocsp-stapling-good.example.com", "good", OSRTGood },
  { "ocsp-stapling-revoked.example.com", "revoked", OSRTRevoked },
  { "ocsp-stapling-unknown.example.com", "unknown", OSRTUnknown },
  { "ocsp-stapling-good-other.example.com", "good-other", OSRTGoodOtherCert },
  { "ocsp-stapling-good-other-ca.example.com", "good-otherCA", OSRTGoodOtherCA },
  { "ocsp-stapling-expired.example.com", "expired", OSRTExpired },
  { "ocsp-stapling-expired-fresh-ca.example.com", "expired-freshCA", OSRTExpiredFreshCA },
  { "ocsp-stapling-none.example.com", "none", OSRTNone },
  { "ocsp-stapling-malformed.example.com", "malformed", OSRTMalformed },
  { "ocsp-stapling-srverr.example.com", "srverr", OSRTSrverr },
  { "ocsp-stapling-trylater.example.com", "trylater", OSRTTryLater },
  { "ocsp-stapling-needssig.example.com", "needssig", OSRTNeedsSig },
  { "ocsp-stapling-unauthorized.example.com", "unauthorized", OSRTUnauthorized },
  { nullptr, nullptr, OSRTNull }
};

struct Connection
{
  const OCSPHost *mHost;
  PRFileDesc *mSocket;
  char mByte;

  Connection(PRFileDesc *aSocket);
  ~Connection();
};

Connection::Connection(PRFileDesc *aSocket)
: mHost(nullptr)
, mSocket(aSocket)
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

const OCSPHost *
GetOcspHost(const char *aServerName)
{
  const OCSPHost *host = sOCSPHosts;
  while (host->mHostName != nullptr &&
         strcmp(host->mHostName, aServerName) != 0) {
    host++;
  }

  if (!host->mHostName) {
    fprintf(stderr, "host '%s' not in pre-defined list\n", aServerName);
    MOZ_CRASH();
    return nullptr;
  }

  return host;
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
  if (bytesRead < 1) {
    PrintPRError("PR_Recv failed");
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
  if (NS_SUCCEEDED(rv)) {
    rv = ReadRequest(&conn);
  }
  if (NS_SUCCEEDED(rv)) {
    rv = ReplyToRequest(&conn);
  }
}

void
DoCallback()
{
  ScopedPRFileDesc socket(PR_NewTCPSocket());
  if (!socket) {
    PrintPRError("PR_NewTCPSocket failed");
    return;
  }

  PRNetAddr addr;
  PR_InitializeNetAddr(PR_IpAddrLoopback, gCallbackPort, &addr);
  if (PR_Connect(socket, &addr, PR_INTERVAL_NO_TIMEOUT) != PR_SUCCESS) {
    PrintPRError("PR_Connect failed");
    return;
  }

  const char *request = "GET / HTTP/1.0\r\n\r\n";
  SendAll(socket, request, strlen(request));
  char buf[4096];
  memset(buf, 0, sizeof(buf));
  int32_t bytesRead = PR_Recv(socket, buf, sizeof(buf) - 1, 0,
                              PR_INTERVAL_NO_TIMEOUT);
  if (bytesRead < 1) {
    PrintPRError("PR_Recv failed");
    return;
  }
  fprintf(stderr, "%s\n", buf);
  memset(buf, 0, sizeof(buf));
  bytesRead = PR_Recv(socket, buf, sizeof(buf) - 1, 0, PR_INTERVAL_NO_TIMEOUT);
  if (bytesRead < 1) {
    PrintPRError("PR_Recv failed");
    return;
  }
  fprintf(stderr, "%s\n", buf);
}

SECItemArray *
GetOCSPResponseForType(OCSPStapleResponseType aOSRT, CERTCertificate *aCert,
                       PLArenaPool *aArena)
{
  PRTime now = PR_Now();
  ScopedCERTOCSPCertID id(CERT_CreateOCSPCertID(aCert, now));
  if (!id) {
    PrintPRError("CERT_CreateOCSPCertID failed");
    return nullptr;
  }
  PRTime nextUpdate = now + 10 * PR_USEC_PER_SEC;
  PRTime oneDay = 60*60*24 * (PRTime)PR_USEC_PER_SEC;
  PRTime expiredTime = now - oneDay;
  PRTime oldNow = now - (8 * oneDay);
  PRTime oldNextUpdate = oldNow + 10 * PR_USEC_PER_SEC;
  ScopedCERTCertificate othercert(PK11_FindCertFromNickname("good", nullptr));
  ScopedCERTOCSPCertID otherid(CERT_CreateOCSPCertID(othercert, now));
  if (!otherid) {
    PrintPRError("CERT_CreateOCSPCertID failed");
    return nullptr;
  }
  CERTOCSPSingleResponse *sr = nullptr;
  switch (aOSRT) {
    case OSRTGood:
    case OSRTGoodOtherCA:
      sr = CERT_CreateOCSPSingleResponseGood(aArena, id, now, &nextUpdate);
      if (!sr) {
        PrintPRError("CERT_CreateOCSPSingleResponseGood failed");
        return nullptr;
      }
      break;
    case OSRTRevoked:
      sr = CERT_CreateOCSPSingleResponseRevoked(aArena, id, now, &nextUpdate,
                                                expiredTime, nullptr);
      if (!sr) {
        PrintPRError("CERT_CreateOCSPSingleResponseRevoked failed");
        return nullptr;
      }
      break;
    case OSRTUnknown:
      sr = CERT_CreateOCSPSingleResponseUnknown(aArena, id, now, &nextUpdate);
      if (!sr) {
        PrintPRError("CERT_CreateOCSPSingleResponseUnknown failed");
        return nullptr;
      }
      break;
    case OSRTExpired:
    case OSRTExpiredFreshCA:
      sr = CERT_CreateOCSPSingleResponseGood(aArena, id, oldNow, &oldNextUpdate);
      if (!sr) {
        PrintPRError("CERT_CreateOCSPSingleResponseGood failed");
        return nullptr;
      }
      break;
    case OSRTGoodOtherCert:
      sr = CERT_CreateOCSPSingleResponseGood(aArena, otherid, now, &nextUpdate);
      if (!sr) {
        PrintPRError("CERT_CreateOCSPSingleResponseGood failed");
        return nullptr;
      }
      break;
    case OSRTNone:
    case OSRTMalformed:
    case OSRTSrverr:
    case OSRTTryLater:
    case OSRTNeedsSig:
    case OSRTUnauthorized:
      break;
    default:
      if (gDebugLevel >= DEBUG_ERRORS) {
        fprintf(stderr, "bad ocsp response type: %d\n", aOSRT);
      }
      break;
  }
  ScopedCERTCertificate ca;
  if (aOSRT == OSRTGoodOtherCA) {
    ca = PK11_FindCertFromNickname("otherCA", nullptr);
    if (!ca) {
      PrintPRError("PK11_FindCertFromNickname failed");
      return nullptr;
    }
  } else {
    // XXX CERT_FindCertIssuer uses the old, deprecated path-building logic
    ca = CERT_FindCertIssuer(aCert, now, certUsageSSLCA);
    if (!ca) {
      PrintPRError("CERT_FindCertIssuer failed");
      return nullptr;
    }
  }

  PRTime signTime = now;
  if (aOSRT == OSRTExpired) {
    signTime = oldNow;
  }

  CERTOCSPSingleResponse **responses;
  SECItem *response = nullptr;
  switch (aOSRT) {
    case OSRTMalformed:
      response = CERT_CreateEncodedOCSPErrorResponse(
        aArena, SEC_ERROR_OCSP_MALFORMED_REQUEST);
      if (!response) {
        PrintPRError("CERT_CreateEncodedOCSPErrorResponse failed");
        return nullptr;
      }
      break;
    case OSRTSrverr:
      response = CERT_CreateEncodedOCSPErrorResponse(
        aArena, SEC_ERROR_OCSP_SERVER_ERROR);
      if (!response) {
        PrintPRError("CERT_CreateEncodedOCSPErrorResponse failed");
        return nullptr;
      }
      break;
    case OSRTTryLater:
      response = CERT_CreateEncodedOCSPErrorResponse(
        aArena, SEC_ERROR_OCSP_TRY_SERVER_LATER);
      if (!response) {
        PrintPRError("CERT_CreateEncodedOCSPErrorResponse failed");
        return nullptr;
      }
      break;
    case OSRTNeedsSig:
      response = CERT_CreateEncodedOCSPErrorResponse(
        aArena, SEC_ERROR_OCSP_REQUEST_NEEDS_SIG);
      if (!response) {
        PrintPRError("CERT_CreateEncodedOCSPErrorResponse failed");
        return nullptr;
      }
      break;
    case OSRTUnauthorized:
      response = CERT_CreateEncodedOCSPErrorResponse(
        aArena, SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST);
      if (!response) {
        PrintPRError("CERT_CreateEncodedOCSPErrorResponse failed");
        return nullptr;
      }
      break;
    case OSRTNone:
      break;
    default:
      // responses is contained in aArena and will be freed when aArena is
      responses = PORT_ArenaNewArray(aArena, CERTOCSPSingleResponse *, 2);
      if (!responses) {
        PrintPRError("PORT_ArenaNewArray failed");
        return nullptr;
      }
      responses[0] = sr;
      responses[1] = nullptr;
      response = CERT_CreateEncodedOCSPSuccessResponse(
        aArena, ca, ocspResponderID_byName, signTime, responses, nullptr);
      if (!response) {
        PrintPRError("CERT_CreateEncodedOCSPSuccessResponse failed");
        return nullptr;
      }
      break;
  }

  SECItemArray *arr = SECITEM_AllocArray(aArena, nullptr, 1);
  arr->items[0].data = response ? response->data : nullptr;
  arr->items[0].len = response ? response->len : 0;

  return arr;
}

int32_t
DoSNISocketConfig(PRFileDesc *aFd, const SECItem *aSrvNameArr,
                  uint32_t aSrvNameArrSize, void *aArg)
{
  const OCSPHost *host = nullptr;
  for (uint32_t i = 0; i < aSrvNameArrSize; i++) {
    host = GetOcspHost((const char *)aSrvNameArr[i].data);
    if (host) {
      break;
    }
  }

  if (!host) {
    return SSL_SNI_SEND_ALERT;
  }

  if (gDebugLevel >= DEBUG_VERBOSE) {
    fprintf(stderr, "found pre-defined host '%s'\n", host->mHostName);
  }

  ScopedCERTCertificate cert(PK11_FindCertFromNickname(host->mCertName, nullptr));
  if (!cert) {
    PrintPRError("PK11_FindCertFromNickname failed");
    return SSL_SNI_SEND_ALERT;
  }

  ScopedSECKEYPrivateKey key(PK11_FindKeyByAnyCert(cert, nullptr));
  if (!key) {
    PrintPRError("PK11_FindKeyByAnyCert failed");
    return SSL_SNI_SEND_ALERT;
  }

  SSLKEAType certKEA = NSS_FindCertKEAType(cert);

  if (SSL_ConfigSecureServer(aFd, cert, key, certKEA) != SECSuccess) {
    PrintPRError("SSL_ConfigSecureServer failed");
    return SSL_SNI_SEND_ALERT;
  }

  PLArenaPool *arena = PORT_NewArena(1024);
  if (!arena) {
    PrintPRError("PORT_NewArena failed");
    return SSL_SNI_SEND_ALERT;
  }
  // response is contained by the arena - freeing the arena will free it
  SECItemArray *response = GetOCSPResponseForType(host->mOSRT, cert, arena);
  if (!response) {
    PORT_FreeArena(arena, PR_FALSE);
    return SSL_SNI_SEND_ALERT;
  }
  // SSL_SetStapledOCSPResponses makes a deep copy of response
  SECStatus st = SSL_SetStapledOCSPResponses(aFd, response, certKEA);
  PORT_FreeArena(arena, PR_FALSE);
  if (st != SECSuccess) {
    PrintPRError("SSL_SetStapledOCSPResponses failed");
    return SSL_SNI_SEND_ALERT;
  }

  return 0;
}

void
StartServer()
{
  ScopedPRFileDesc serverSocket(PR_NewTCPSocket());
  if (!serverSocket) {
    PrintPRError("PR_NewTCPSocket failed");
    return;
  }

  PRSocketOptionData socketOption;
  socketOption.option = PR_SockOpt_Reuseaddr;
  socketOption.value.reuse_addr = true;
  PR_SetSocketOption(serverSocket, &socketOption);

  PRNetAddr serverAddr;
  PR_InitializeNetAddr(PR_IpAddrLoopback, LISTEN_PORT, &serverAddr);
  if (PR_Bind(serverSocket, &serverAddr) != PR_SUCCESS) {
    PrintPRError("PR_Bind failed");
    return;
  }

  if (PR_Listen(serverSocket, 1) != PR_SUCCESS) {
    PrintPRError("PR_Listen failed");
    return;
  }

  ScopedPRFileDesc rawModelSocket(PR_NewTCPSocket());
  if (!rawModelSocket) {
    PrintPRError("PR_NewTCPSocket failed for rawModelSocket");
    return;
  }

  ScopedPRFileDesc modelSocket(SSL_ImportFD(nullptr, rawModelSocket.forget()));
  if (!modelSocket) {
    PrintPRError("SSL_ImportFD of rawModelSocket failed");
    return;
  }

  if (SECSuccess != SSL_SNISocketConfigHook(modelSocket, DoSNISocketConfig,
                                            nullptr)) {
    PrintPRError("SSL_SNISocketConfigHook failed");
    return;
  }

  // We have to configure the server with a certificate, but it's not one
  // we're actually going to end up using. In the SNI callback, we pick
  // the right certificate for the connection.
  ScopedCERTCertificate cert(PK11_FindCertFromNickname("localhost", nullptr));
  if (!cert) {
    PrintPRError("PK11_FindCertFromNickname failed");
    return;
  }

  ScopedSECKEYPrivateKey key(PK11_FindKeyByAnyCert(cert, nullptr));
  if (!key) {
    PrintPRError("PK11_FindKeyByAnyCert failed");
    return;
  }

  SSLKEAType certKEA = NSS_FindCertKEAType(cert);

  if (SSL_ConfigSecureServer(modelSocket, cert, key, certKEA) != SECSuccess) {
    PrintPRError("SSL_ConfigSecureServer failed");
    return;
  }

  if (gCallbackPort != 0) {
    DoCallback();
  }

  while (true) {
    PRNetAddr clientAddr;
    PRFileDesc *clientSocket = PR_Accept(serverSocket, &clientAddr,
                                         PR_INTERVAL_NO_TIMEOUT);
    HandleConnection(clientSocket, modelSocket);
  }
}

int
main(int argc, char *argv[])
{
  const char *debugLevel = PR_GetEnv("OCSP_SERVER_DEBUG_LEVEL");
  if (debugLevel) {
    gDebugLevel = atoi(debugLevel);
  }

  const char *callbackPort = PR_GetEnv("OCSP_SERVER_CALLBACK_PORT");
  if (callbackPort) {
    gCallbackPort = atoi(callbackPort);
  }

  if (argc != 2) {
    fprintf(stderr, "usage: %s <NSS DB directory>\n", argv[0]);
    return 1;
  }

  if (NSS_Init(argv[1]) != SECSuccess) {
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

  StartServer();

  return 0;
}
