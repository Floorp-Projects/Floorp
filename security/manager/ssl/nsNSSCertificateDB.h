/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSCertificateDB_h
#define nsNSSCertificateDB_h

#include "ScopedNSSTypes.h"
#include "certt.h"
#include "mozilla/Mutex.h"
#include "mozilla/NotNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsIX509CertDB.h"
#include "nsString.h"

class nsIArray;

class nsNSSCertificateDB final : public nsIX509CertDB

{
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIX509CERTDB

  // This is a separate static method so nsNSSComponent can use it during NSS
  // initialization. Other code should probably not use it.
  static nsresult FindCertByDBKey(const nsACString& aDBKey,
                                  mozilla::UniqueCERTCertificate& cert);

  static nsresult ConstructCertArrayFromUniqueCertList(
      const mozilla::UniqueCERTCertList& aCertListIn,
      nsTArray<RefPtr<nsIX509Cert>>& aCertListOut);

 protected:
  virtual ~nsNSSCertificateDB() = default;

 private:
  // Use this function to generate a default nickname for a user
  // certificate that is to be imported onto a token.
  static void get_default_nickname(CERTCertificate* cert,
                                   nsIInterfaceRequestor* ctx,
                                   nsCString& nickname);

  static nsresult ImportCACerts(nsTArray<nsTArray<uint8_t>>& CACerts,
                                nsIInterfaceRequestor* ctx);

  static void DisplayCertificateAlert(nsIInterfaceRequestor* ctx,
                                      const char* stringID,
                                      nsIX509Cert* certToShow);

  nsresult getCertsFromPackage(nsTArray<nsTArray<uint8_t>>& collectArgs,
                               uint8_t* data, uint32_t length);
  nsresult handleCACertDownload(mozilla::NotNull<nsIArray*> x509Certs,
                                nsIInterfaceRequestor* ctx);
  nsresult ConstructX509FromSpan(const mozilla::Span<const uint8_t> aInputSpan,
                                 nsIX509Cert** _retval);
};

#define NS_X509CERTDB_CID                            \
  { /* fb0bbc5c-452e-4783-b32c-80124693d871 */       \
    0xfb0bbc5c, 0x452e, 0x4783, {                    \
      0xb3, 0x2c, 0x80, 0x12, 0x46, 0x93, 0xd8, 0x71 \
    }                                                \
  }

SECStatus ChangeCertTrustWithPossibleAuthentication(
    const mozilla::UniqueCERTCertificate& cert, CERTCertTrust& trust,
    void* ctx);

#endif  // nsNSSCertificateDB_h
