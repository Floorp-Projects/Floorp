/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPKCS12Blob_h
#define nsPKCS12Blob_h

#include "nsCOMPtr.h"
#include "nsIMutableArray.h"
#include "nsString.h"
#include "p12.h"
#include "seccomon.h"

class nsIFile;
class nsIX509Cert;

// Class for importing/exporting PKCS#12 blobs
class nsPKCS12Blob
{
public:
  nsPKCS12Blob();
  ~nsPKCS12Blob() {}

  // PKCS#12 Import
  nsresult ImportFromFile(nsIFile* file);

  // PKCS#12 Export
  nsresult ExportToFile(nsIFile* file, nsIX509Cert** certs, int numCerts);

private:
  nsCOMPtr<nsIInterfaceRequestor> mUIContext;

  // local helper functions
  nsresult getPKCS12FilePassword(uint32_t& passwordBufferLength,
                                 UniquePtr<uint8_t[]>& passwordBuffer);
  nsresult newPKCS12FilePassword(uint32_t& passwordBufferLength,
                                 UniquePtr<uint8_t[]>& passwordBuffer);
  nsresult inputToDecoder(UniqueSEC_PKCS12DecoderContext& dcx, nsIFile* file,
                          PRErrorCode& nssError);
  UniquePtr<uint8_t[]> stringToBigEndianBytes(const nsString& uni,
                                              uint32_t& bytesLength);
  void handleError(int myerr, PRErrorCode prerr);

  // RetryReason and ImportMode are used when importing a PKCS12 file.
  // There are two reasons that cause us to retry:
  // - When the password entered by the user is incorrect.
  //   The user will be prompted to try again.
  // - When the user entered a zero length password.
  //   An empty password should be represented as an empty string (a SECItem
  //   that contains a single terminating null UTF16 character), but some
  //   applications use a zero length SECItem. We try both variations, zero
  //   length item and empty string, without giving a user prompt when trying
  //   the different empty password flavors.
  enum class RetryReason
  {
    DoNotRetry,
    BadPassword,
    AutoRetryEmptyPassword,
  };
  enum class ImportMode
  {
    StandardPrompt,
    TryZeroLengthSecitem
  };

  void handleImportError(PRErrorCode nssError, RetryReason& retryReason,
                         uint32_t passwordLengthInBytes);

  nsresult ImportFromFileHelper(nsIFile* file,
                                ImportMode aImportMode,
                                RetryReason& aWantRetry);

  static SECItem* nicknameCollision(SECItem* oldNick, PRBool* cancel,
                                    void* wincx);
  static void writeExportFile(void* arg, const char* buf, unsigned long len);
};

#endif // nsPKCS12Blob_h
