/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TLSServer.h"

#include <stdio.h>
#include <string>
#include <vector>

#include "base64.h"
#include "mozilla/Move.h"
#include "mozilla/Sprintf.h"
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

const char DEFAULT_CERT_NICKNAME[] = "default-ee";

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

template <size_t N>
SECStatus
ReadFileToBuffer(const char* basePath, const char* filename, char (&buf)[N])
{
  static_assert(N > 0, "input buffer too small for ReadFileToBuffer");
  if (snprintf(buf, N - 1, "%s/%s", basePath, filename) == 0) {
    PrintPRError("snprintf failed");
    return SECFailure;
  }
  UniquePRFileDesc fd(PR_OpenFile(buf, PR_RDONLY, 0));
  if (!fd) {
    PrintPRError("PR_Open failed");
    return SECFailure;
  }
  int32_t fileSize = PR_Available(fd.get());
  if (fileSize < 0) {
    PrintPRError("PR_Available failed");
    return SECFailure;
  }
  if (static_cast<size_t>(fileSize) > N - 1) {
    PR_fprintf(PR_STDERR, "file too large - not reading\n");
    return SECFailure;
  }
  int32_t bytesRead = PR_Read(fd.get(), buf, fileSize);
  if (bytesRead != fileSize) {
    PrintPRError("PR_Read failed");
    return SECFailure;
  }
  buf[bytesRead] = 0;
  return SECSuccess;
}

SECStatus
AddKeyFromFile(const char* basePath, const char* filename)
{
  const char* PRIVATE_KEY_HEADER = "-----BEGIN PRIVATE KEY-----";
  const char* PRIVATE_KEY_FOOTER = "-----END PRIVATE KEY-----";

  char buf[16384] = { 0 };
  SECStatus rv = ReadFileToBuffer(basePath, filename, buf);
  if (rv != SECSuccess) {
    return rv;
  }
  if (strncmp(buf, PRIVATE_KEY_HEADER, strlen(PRIVATE_KEY_HEADER)) != 0) {
    PR_fprintf(PR_STDERR, "invalid key - not importing\n");
    return SECFailure;
  }
  const char* bufPtr = buf + strlen(PRIVATE_KEY_HEADER);
  size_t bufLen = strlen(buf);
  char base64[16384] = { 0 };
  char* base64Ptr = base64;
  while (bufPtr < buf + bufLen) {
    if (strncmp(bufPtr, PRIVATE_KEY_FOOTER, strlen(PRIVATE_KEY_FOOTER)) == 0) {
      break;
    }
    if (*bufPtr != '\r' && *bufPtr != '\n') {
      *base64Ptr = *bufPtr;
      base64Ptr++;
    }
    bufPtr++;
  }

  unsigned int binLength;
  UniquePORTString bin(
    BitwiseCast<char*, unsigned char*>(ATOB_AsciiToData(base64, &binLength)));
  if (!bin || binLength == 0) {
    PrintPRError("ATOB_AsciiToData failed");
    return SECFailure;
  }
  UniqueSECItem secitem(::SECITEM_AllocItem(nullptr, nullptr, binLength));
  if (!secitem) {
    PrintPRError("SECITEM_AllocItem failed");
    return SECFailure;
  }
  PORT_Memcpy(secitem->data, bin.get(), binLength);
  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    PrintPRError("PK11_GetInternalKeySlot failed");
    return SECFailure;
  }
  if (PK11_NeedUserInit(slot.get())) {
    if (PK11_InitPin(slot.get(), nullptr, nullptr) != SECSuccess) {
      PrintPRError("PK11_InitPin failed");
      return SECFailure;
    }
  }
  SECKEYPrivateKey* privateKey;
  if (PK11_ImportDERPrivateKeyInfoAndReturnKey(slot.get(), secitem.get(),
                                               nullptr, nullptr, true, false,
                                               KU_ALL, &privateKey, nullptr)
        != SECSuccess) {
    PrintPRError("PK11_ImportDERPrivateKeyInfoAndReturnKey failed");
    return SECFailure;
  }
  SECKEY_DestroyPrivateKey(privateKey);
  return SECSuccess;
}

SECStatus
DecodeCertCallback(void* arg, SECItem** certs, int numcerts)
{
  if (numcerts != 1) {
    PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
    return SECFailure;
  }

  SECItem* certDEROut = static_cast<SECItem*>(arg);
  return SECITEM_CopyItem(nullptr, certDEROut, *certs);
}

SECStatus
AddCertificateFromFile(const char* basePath, const char* filename)
{
  char buf[16384] = { 0 };
  SECStatus rv = ReadFileToBuffer(basePath, filename, buf);
  if (rv != SECSuccess) {
    return rv;
  }
  ScopedAutoSECItem certDER;
  rv = CERT_DecodeCertPackage(buf, strlen(buf), DecodeCertCallback, &certDER);
  if (rv != SECSuccess) {
    PrintPRError("CERT_DecodeCertPackage failed");
    return rv;
  }
  UniqueCERTCertificate cert(CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                                     &certDER, nullptr, false,
                                                     true));
  if (!cert) {
    PrintPRError("CERT_NewTempCertificate failed");
    return SECFailure;
  }
  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    PrintPRError("PK11_GetInternalKeySlot failed");
    return SECFailure;
  }
  // The nickname is the filename without '.pem'.
  std::string nickname(filename, strlen(filename) - 4);
  rv = PK11_ImportCert(slot.get(), cert.get(), CK_INVALID_HANDLE,
                       nickname.c_str(), false);
  if (rv != SECSuccess) {
    PrintPRError("PK11_ImportCert failed");
    return rv;
  }
  return SECSuccess;
}

SECStatus
LoadCertificatesAndKeys(const char* basePath)
{
  // The NSS cert DB path could have been specified as "sql:path". Trim off
  // the leading "sql:" if so.
  if (strncmp(basePath, "sql:", 4) == 0) {
    basePath = basePath + 4;
  }

  UniquePRDir fdDir(PR_OpenDir(basePath));
  if (!fdDir) {
    PrintPRError("PR_OpenDir failed");
    return SECFailure;
  }
  // On the B2G ICS emulator, operations taken in AddCertificateFromFile
  // appear to interact poorly with readdir (more specifically, something is
  // causing readdir to never return null - it indefinitely loops through every
  // file in the directory, which causes timeouts). Rather than waste more time
  // chasing this down, loading certificates and keys happens in two phases:
  // filename collection and then loading. (This is probably a good
  // idea anyway because readdir isn't reentrant. Something could change later
  // such that it gets called as a result of calling AddCertificateFromFile or
  // AddKeyFromFile.)
  std::vector<std::string> certificates;
  std::vector<std::string> keys;
  for (PRDirEntry* dirEntry = PR_ReadDir(fdDir.get(), PR_SKIP_BOTH); dirEntry;
       dirEntry = PR_ReadDir(fdDir.get(), PR_SKIP_BOTH)) {
    size_t nameLength = strlen(dirEntry->name);
    if (nameLength > 4) {
      if (strncmp(dirEntry->name + nameLength - 4, ".pem", 4) == 0) {
        certificates.push_back(dirEntry->name);
      } else if (strncmp(dirEntry->name + nameLength - 4, ".key", 4) == 0) {
        keys.push_back(dirEntry->name);
      }
    }
  }
  SECStatus rv;
  for (std::string& certificate : certificates) {
    rv = AddCertificateFromFile(basePath, certificate.c_str());
    if (rv != SECSuccess) {
      return rv;
    }
  }
  for (std::string& key : keys) {
    rv = AddKeyFromFile(basePath, key.c_str());
    if (rv != SECSuccess) {
      return rv;
    }
  }
  return SECSuccess;
}

SECStatus
InitializeNSS(const char* nssCertDBDir)
{
  // Try initializing an existing DB.
  if (NSS_Init(nssCertDBDir) == SECSuccess) {
    return SECSuccess;
  }

  // Create a new DB if there is none...
  SECStatus rv = NSS_Initialize(nssCertDBDir, nullptr, nullptr, nullptr, 0);
  if (rv != SECSuccess) {
    return rv;
  }

  // ...and load all certificates into it.
  return LoadCertificatesAndKeys(nssCertDBDir);
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
HandleConnection(PRFileDesc* aSocket, const UniquePRFileDesc& aModelSocket)
{
  Connection conn(aSocket);
  nsresult rv = SetupTLS(&conn, aModelSocket.get());
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
  UniquePRFileDesc socket(PR_NewTCPSocket());
  if (!socket) {
    PrintPRError("PR_NewTCPSocket failed");
    return 1;
  }

  PRNetAddr addr;
  PR_InitializeNetAddr(PR_IpAddrLoopback, gCallbackPort, &addr);
  if (PR_Connect(socket.get(), &addr, PR_INTERVAL_NO_TIMEOUT) != PR_SUCCESS) {
    PrintPRError("PR_Connect failed");
    return 1;
  }

  const char *request = "GET / HTTP/1.0\r\n\r\n";
  SendAll(socket.get(), request, strlen(request));
  char buf[4096];
  memset(buf, 0, sizeof(buf));
  int32_t bytesRead = PR_Recv(socket.get(), buf, sizeof(buf) - 1, 0,
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
ConfigSecureServerWithNamedCert(PRFileDesc* fd, const char* certName,
                                /*optional*/ UniqueCERTCertificate* certOut,
                                /*optional*/ SSLKEAType* keaOut)
{
  UniqueCERTCertificate cert(PK11_FindCertFromNickname(certName, nullptr));
  if (!cert) {
    PrintPRError("PK11_FindCertFromNickname failed");
    return SECFailure;
  }
  // If an intermediate certificate issued the server certificate (rather than
  // directly by a trust anchor), we want to send it along in the handshake so
  // we don't encounter unknown issuer errors when that's not what we're
  // testing.
  UniqueCERTCertificateList certList;
  UniqueCERTCertificate issuerCert(
    CERT_FindCertByName(CERT_GetDefaultCertDB(), &cert->derIssuer));
  // If we can't find the issuer cert, continue without it.
  if (issuerCert) {
    // Sadly, CERTCertificateList does not have a CERT_NewCertificateList
    // utility function, so we must create it ourselves. This consists
    // of creating an arena, allocating space for the CERTCertificateList,
    // and then transferring ownership of the arena to that list.
    UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
    if (!arena) {
      PrintPRError("PORT_NewArena failed");
      return SECFailure;
    }
    certList.reset(static_cast<CERTCertificateList*>(
      PORT_ArenaAlloc(arena.get(), sizeof(CERTCertificateList))));
    if (!certList) {
      PrintPRError("PORT_ArenaAlloc failed");
      return SECFailure;
    }
    certList->arena = arena.release();
    // We also have to manually copy the certificates we care about to the
    // list, because there aren't any utility functions for that either.
    certList->certs = static_cast<SECItem*>(
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

  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    PrintPRError("PK11_GetInternalKeySlot failed");
    return SECFailure;
  }
  UniqueSECKEYPrivateKey key(
    PK11_FindKeyByDERCert(slot.get(), cert.get(), nullptr));
  if (!key) {
    PrintPRError("PK11_FindKeyByDERCert failed");
    return SECFailure;
  }

  SSLKEAType certKEA = NSS_FindCertKEAType(cert.get());

  if (SSL_ConfigSecureServerWithCertChain(fd, cert.get(), certList.get(),
                                          key.get(), certKEA) != SECSuccess) {
    PrintPRError("SSL_ConfigSecureServer failed");
    return SECFailure;
  }

  if (certOut) {
    *certOut = Move(cert);
  }

  if (keaOut) {
    *keaOut = certKEA;
  }

  SSL_OptionSet(fd, SSL_NO_CACHE, false);
  SSL_OptionSet(fd, SSL_ENABLE_SESSION_TICKETS, true);

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

  if (InitializeNSS(nssCertDBDir) != SECSuccess) {
    PR_fprintf(PR_STDERR, "InitializeNSS failed");
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

  UniquePRFileDesc serverSocket(PR_NewTCPSocket());
  if (!serverSocket) {
    PrintPRError("PR_NewTCPSocket failed");
    return 1;
  }

  PRSocketOptionData socketOption;
  socketOption.option = PR_SockOpt_Reuseaddr;
  socketOption.value.reuse_addr = true;
  PR_SetSocketOption(serverSocket.get(), &socketOption);

  PRNetAddr serverAddr;
  PR_InitializeNetAddr(PR_IpAddrLoopback, LISTEN_PORT, &serverAddr);
  if (PR_Bind(serverSocket.get(), &serverAddr) != PR_SUCCESS) {
    PrintPRError("PR_Bind failed");
    return 1;
  }

  if (PR_Listen(serverSocket.get(), 1) != PR_SUCCESS) {
    PrintPRError("PR_Listen failed");
    return 1;
  }

  UniquePRFileDesc rawModelSocket(PR_NewTCPSocket());
  if (!rawModelSocket) {
    PrintPRError("PR_NewTCPSocket failed for rawModelSocket");
    return 1;
  }

  UniquePRFileDesc modelSocket(SSL_ImportFD(nullptr, rawModelSocket.release()));
  if (!modelSocket) {
    PrintPRError("SSL_ImportFD of rawModelSocket failed");
    return 1;
  }

  if (SSL_SNISocketConfigHook(modelSocket.get(), sniSocketConfig,
                              sniSocketConfigArg) != SECSuccess) {
    PrintPRError("SSL_SNISocketConfigHook failed");
    return 1;
  }

  // We have to configure the server with a certificate, but it's not one
  // we're actually going to end up using. In the SNI callback, we pick
  // the right certificate for the connection.
  if (ConfigSecureServerWithNamedCert(modelSocket.get(), DEFAULT_CERT_NICKNAME,
                                      nullptr, nullptr) != SECSuccess) {
    return 1;
  }

  if (gCallbackPort != 0) {
    if (DoCallback()) {
      return 1;
    }
  }

  while (true) {
    PRNetAddr clientAddr;
    PRFileDesc* clientSocket = PR_Accept(serverSocket.get(), &clientAddr,
                                         PR_INTERVAL_NO_TIMEOUT);
    HandleConnection(clientSocket, modelSocket);
  }

  return 0;
}

} } // namespace mozilla::test
