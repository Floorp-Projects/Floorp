/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CERTIFICATECHECK_H_
#define _CERTIFICATECHECK_H_

#include <wincrypt.h>

struct CertificateCheckInfo {
  LPCWSTR name;
  LPCWSTR issuer;
};

BOOL DoCertificateAttributesMatch(PCCERT_CONTEXT pCertContext,
                                  CertificateCheckInfo& infoToMatch);
DWORD VerifyCertificateTrustForFile(LPCWSTR filePath);
DWORD CheckCertificateForPEFile(LPCWSTR filePath,
                                CertificateCheckInfo& infoToMatch);

#endif
