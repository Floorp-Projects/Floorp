/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 tw=80 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This simple program takes a database directory, and one or more tuples like
 * <typeOfResponse> <CertNick> <ExtraCertNick> <outPutFilename>
 * to generate (one or more) ocsp responses.
 */

#include <stdio.h>

#include "mozilla/ArrayUtils.h"

#include "base64.h"
#include "cert.h"
#include "nspr.h"
#include "nss.h"
#include "plarenas.h"
#include "prerror.h"
#include "ssl.h"
#include "secerr.h"

#include "OCSPCommon.h"
#include "ScopedNSSTypes.h"
#include "TLSServer.h"

using namespace mozilla;
using namespace mozilla::test;

struct OCSPResponseName
{
  const char *mTypeString;
  const OCSPResponseType mORT;
};

const static OCSPResponseName kOCSPResponseNameList[] = {
  { "good",            ORTGood },          // the certificate is good
  { "revoked",         ORTRevoked},        // the certificate has been revoked
  { "unknown",         ORTUnknown},        // the responder doesn't know if the
                                           //   cert is good
  { "goodotherca",     ORTGoodOtherCA},    // the wrong CA has signed the
                                           //   response
  { "expiredresponse", ORTExpired},        // the signature on the response has
                                           //   expired
  { "oldvalidperiod",  ORTExpiredFreshCA}, // fresh signature, but old validity
                                           //   period
  { "empty",           ORTEmpty},          // an empty stapled response

  { "malformed",       ORTMalformed},      // the response from the responder
                                           //   was malformed
  { "serverr",         ORTSrverr},         // the response indicates there was a
                                           //   server error
  { "trylater",        ORTTryLater},       // the responder replied with
                                           //   "try again later"
  { "resp-unsigned",   ORTNeedsSig},       // the response needs a signature
  { "unauthorized",    ORTUnauthorized},   // the responder does not know about
                                           //   the cert
  { "bad-signature",   ORTBadSignature},   // the response has a bad signature
  { "longvalidityalmostold", ORTLongValidityAlmostExpired}, // the response is
                                           // still valid, but the generation
                                           // is almost a year old
  { "ancientstillvalid", ORTAncientAlmostExpired}, // The response is still
                                           // valid but the generation is almost
                                           // two years old
};

bool
StringToOCSPResponseType(const char* respText,
                         /*out*/ OCSPResponseType* OCSPType)
{
  if (!OCSPType) {
    return false;
  }
  for (uint32_t i = 0; i < mozilla::ArrayLength(kOCSPResponseNameList); i++) {
    if (strcmp(respText, kOCSPResponseNameList[i].mTypeString) == 0) {
      *OCSPType = kOCSPResponseNameList[i].mORT;
      return true;
    }
  }
  return false;
}

bool
WriteResponse(const char* filename, const SECItem* item)
{
  if (!filename || !item || !item->data) {
    PR_fprintf(PR_STDERR, "invalid parameters to WriteResponse");
    return false;
  }

  ScopedPRFileDesc outFile(PR_Open(filename,
                                   PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                                   0644));
  if (!outFile) {
    PrintPRError("cannot open file for writing");
    return false;
  }
  int32_t rv = PR_Write(outFile, item->data, item->len);
  if (rv < 0 || (uint32_t) rv != item->len) {
    PrintPRError("File write failure");
    return false;
  }

  return true;
}

template <size_t N>
SECStatus
ReadFileToBuffer(const char* basePath, const char* filename, char (&buf)[N])
{
  static_assert(N > 0, "input buffer too small for ReadFileToBuffer");
  if (PR_snprintf(buf, N - 1, "%s/%s", basePath, filename) == 0) {
    PrintPRError("PR_snprintf failed");
    return SECFailure;
  }
  ScopedPRFileDesc fd(PR_OpenFile(buf, PR_RDONLY, 0));
  if (!fd) {
    PrintPRError("PR_Open failed");
    return SECFailure;
  }
  int32_t fileSize = PR_Available(fd);
  if (fileSize < 0) {
    PrintPRError("PR_Available failed");
    return SECFailure;
  }
  if (static_cast<size_t>(fileSize) > N - 1) {
    PR_fprintf(PR_STDERR, "file too large - not reading\n");
    return SECFailure;
  }
  int32_t bytesRead = PR_Read(fd, buf, fileSize);
  if (bytesRead != fileSize) {
    PrintPRError("PR_Read failed");
    return SECFailure;
  }
  buf[bytesRead] = 0;
  return SECSuccess;
}

namespace mozilla {

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPRDir, PRDir, PR_CloseDir);
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPORTString, unsigned char, PORT_Free);

};

void
AddKeyFromFile(const char* basePath, const char* filename)
{
  const char* PRIVATE_KEY_HEADER = "-----BEGIN PRIVATE KEY-----";
  const char* PRIVATE_KEY_FOOTER = "-----END PRIVATE KEY-----";

  char buf[16384] = { 0 };
  SECStatus rv = ReadFileToBuffer(basePath, filename, buf);
  if (rv != SECSuccess) {
    return;
  }
  if (strncmp(buf, PRIVATE_KEY_HEADER, strlen(PRIVATE_KEY_HEADER)) != 0) {
    PR_fprintf(PR_STDERR, "invalid key - not importing\n");
    return;
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
  ScopedPORTString bin(ATOB_AsciiToData(base64, &binLength));
  if (!bin || binLength == 0) {
    PrintPRError("ATOB_AsciiToData failed");
    return;
  }
  ScopedSECItem secitem(SECITEM_AllocItem(nullptr, nullptr, binLength));
  if (!secitem) {
    PrintPRError("SECITEM_AllocItem failed");
    return;
  }
  memcpy(secitem->data, bin, binLength);
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    PrintPRError("PK11_GetInternalKeySlot failed");
    return;
  }
  if (PK11_NeedUserInit(slot)) {
    if (PK11_InitPin(slot, nullptr, nullptr) != SECSuccess) {
      PrintPRError("PK11_InitPin failed");
      return;
    }
  }
  SECKEYPrivateKey* privateKey;
  if (PK11_ImportDERPrivateKeyInfoAndReturnKey(slot, secitem, nullptr, nullptr,
                                               true, false, KU_ALL,
                                               &privateKey, nullptr)
        != SECSuccess) {
    PrintPRError("PK11_ImportDERPrivateKeyInfoAndReturnKey failed");
    return;
  }
  SECKEY_DestroyPrivateKey(privateKey);
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

void
AddCertificateFromFile(const char* basePath, const char* filename)
{
  char buf[16384] = { 0 };
  SECStatus rv = ReadFileToBuffer(basePath, filename, buf);
  if (rv != SECSuccess) {
    return;
  }
  SECItem certDER;
  rv = CERT_DecodeCertPackage(buf, strlen(buf), DecodeCertCallback, &certDER);
  if (rv != SECSuccess) {
    PrintPRError("CERT_DecodeCertPackage failed");
    return;
  }
  ScopedCERTCertificate cert(CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                                     &certDER, nullptr, false,
                                                     true));
  PORT_Free(certDER.data);
  if (!cert) {
    PrintPRError("CERT_NewTempCertificate failed");
    return;
  }
  const char* extension = strstr(filename, ".pem");
  if (!extension) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return;
  }
  size_t nicknameLength = extension - filename;
  memset(buf, 0, sizeof(buf));
  memcpy(buf, filename, nicknameLength);
  buf[nicknameLength] = 0;
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    PrintPRError("PK11_GetInternalKeySlot failed");
    return;
  }
  rv = PK11_ImportCert(slot, cert, CK_INVALID_HANDLE, buf, false);
  if (rv != SECSuccess) {
    PrintPRError("PK11_ImportCert failed");
  }
}

SECStatus
InitializeNSS(const char* nssCertDBDir)
{
  // First attempt to initialize NSS in read-only mode, in case the specified
  // directory contains NSS DBs that are tracked by revision control.
  // If this succeeds, we're done.
  if (NSS_Initialize(nssCertDBDir, "", "", SECMOD_DB, NSS_INIT_READONLY)
        == SECSuccess) {
    return SECSuccess;
  }
  // Otherwise, create a new read-write DB and load all .pem and .key files.
  if (NSS_Initialize(nssCertDBDir, "", "", SECMOD_DB, 0) != SECSuccess) {
    PrintPRError("NSS_Initialize failed");
    return SECFailure;
  }
  const char* basePath = nssCertDBDir;
  // The NSS cert DB path could have been specified as "sql:path". Trim off
  // the leading "sql:" if so.
  if (strncmp(basePath, "sql:", 4) == 0) {
    basePath = basePath + 4;
  }
  ScopedPRDir fdDir(PR_OpenDir(basePath));
  if (!fdDir) {
    PrintPRError("PR_OpenDir failed");
    return SECFailure;
  }
  for (PRDirEntry* dirEntry = PR_ReadDir(fdDir, PR_SKIP_BOTH); dirEntry;
       dirEntry = PR_ReadDir(fdDir, PR_SKIP_BOTH)) {
    size_t nameLength = strlen(dirEntry->name);
    if (nameLength > 4) {
      if (strncmp(dirEntry->name + nameLength - 4, ".pem", 4) == 0) {
        AddCertificateFromFile(basePath, dirEntry->name);
      } else if (strncmp(dirEntry->name + nameLength - 4, ".key", 4) == 0) {
        AddKeyFromFile(basePath, dirEntry->name);
      }
    }
  }
  return SECSuccess;
}

int
main(int argc, char* argv[])
{

  if (argc < 6 || (argc - 6) % 4 != 0) {
    PR_fprintf(PR_STDERR, "usage: %s <NSS DB directory> <responsetype> "
                          "<cert_nick> <extranick> <outfilename> [<resptype> "
                          "<cert_nick> <extranick> <outfilename>]* \n",
                          argv[0]);
    exit(EXIT_FAILURE);
  }
  SECStatus rv = InitializeNSS(argv[1]);
  if (rv != SECSuccess) {
    PR_fprintf(PR_STDERR, "Failed to initialize NSS\n");
    exit(EXIT_FAILURE);
  }
  PLArenaPool* arena = PORT_NewArena(256 * argc);
  if (!arena) {
    PrintPRError("PORT_NewArena failed");
    exit(EXIT_FAILURE);
  }

  for (int i = 2; i + 3 < argc; i += 4) {
    const char* ocspTypeText  = argv[i];
    const char* certNick      = argv[i + 1];
    const char* extraCertname = argv[i + 2];
    const char* filename      = argv[i + 3];

    OCSPResponseType ORT;
    if (!StringToOCSPResponseType(ocspTypeText, &ORT)) {
      PR_fprintf(PR_STDERR, "Cannot generate OCSP response of type %s\n",
                 ocspTypeText);
      exit(EXIT_FAILURE);
    }

    ScopedCERTCertificate cert(PK11_FindCertFromNickname(certNick, nullptr));
    if (!cert) {
      PrintPRError("PK11_FindCertFromNickname failed");
      PR_fprintf(PR_STDERR, "Failed to find certificate with nick '%s'\n",
                 certNick);
      exit(EXIT_FAILURE);
    }

    SECItemArray* response = GetOCSPResponseForType(ORT, cert, arena,
                                                    extraCertname);
    if (!response) {
      PR_fprintf(PR_STDERR, "Failed to generate OCSP response of type %s "
                            "for %s\n", ocspTypeText, certNick);
      exit(EXIT_FAILURE);
    }

    if (!WriteResponse(filename, &response->items[0])) {
      PR_fprintf(PR_STDERR, "Failed to write file %s\n", filename);
      exit(EXIT_FAILURE);
    }
  }
  return 0;
}
