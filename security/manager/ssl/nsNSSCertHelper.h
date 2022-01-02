/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSCertHelper_h
#define nsNSSCertHelper_h

#ifndef INET6_ADDRSTRLEN
#  define INET6_ADDRSTRLEN 46
#endif

#include "certt.h"
#include "nsString.h"

extern const char* kRootModuleName;
extern const size_t kRootModuleNameLen;

class nsIX509Cert;

// If input is valid UTF-8, converts from UTF-8 to UTF-16. Otherwise,
// converts from Latin1 to UTF-16.
void LossyUTF8ToUTF16(const char* str, uint32_t len, /*out*/ nsAString& result);

// Must be used on the main thread only.
nsresult GetPIPNSSBundleString(const char* stringName, nsAString& result);
nsresult GetPIPNSSBundleString(const char* stringName, nsACString& result);
nsresult PIPBundleFormatStringFromName(const char* stringName,
                                       const nsTArray<nsString>& params,
                                       nsAString& result);

nsresult GetCertSha256Fingerprint(nsIX509Cert* aCert, nsCString& aResult);

#endif  // nsNSSCertHelper_h
