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
};


bool
stringToOCSPResponseType(const char* respText,
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
  const char* dbdir = argv[1];

  SECStatus rv;
  rv = NSS_Init(dbdir);
  if (rv != SECSuccess) {
    PrintPRError("Failed to initialize NSS");
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
    if (!stringToOCSPResponseType(ocspTypeText, &ORT)) {
      PR_fprintf(PR_STDERR, "Cannot generate OCSP response of type %s\n",
                 ocspTypeText);
      exit(EXIT_FAILURE);
    }

    ScopedCERTCertificate cert;
    cert = PK11_FindCertFromNickname(certNick, nullptr);
    if (!cert) {
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
