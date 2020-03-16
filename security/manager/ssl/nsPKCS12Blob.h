/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPKCS12Blob_h
#define nsPKCS12Blob_h

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestor.h"
#include "nsString.h"
#include "nsTArray.h"
#include "p12.h"
#include "prerror.h"
#include "ScopedNSSTypes.h"
#include "seccomon.h"

class nsIFile;
class nsIX509Cert;

// Class for importing/exporting PKCS#12 blobs
class nsPKCS12Blob {
 public:
  nsPKCS12Blob();
  ~nsPKCS12Blob() = default;

  // PKCS#12 Import
  nsresult ImportFromFile(nsIFile* file, const nsAString& password,
                          uint32_t& error);

  // PKCS#12 Export
  nsresult ExportToFile(nsIFile* file,
                        const nsTArray<RefPtr<nsIX509Cert>>& certs,
                        const nsAString& password, uint32_t& error);

 private:
  nsCOMPtr<nsIInterfaceRequestor> mUIContext;

  // local helper functions
  nsresult inputToDecoder(mozilla::UniqueSEC_PKCS12DecoderContext& dcx,
                          nsIFile* file, PRErrorCode& nssError);
  mozilla::UniquePtr<uint8_t[]> stringToBigEndianBytes(const nsAString& uni,
                                                       uint32_t& bytesLength);
  uint32_t handlePRErrorCode(PRErrorCode prerr);

  static SECItem* nicknameCollision(SECItem* oldNick, PRBool* cancel,
                                    void* wincx);
  static void writeExportFile(void* arg, const char* buf, unsigned long len);
};

#endif  // nsPKCS12Blob_h
