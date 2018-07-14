/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EnterpriseRoots_h
#define EnterpriseRoots_h

#include "ErrorList.h"
#include "ScopedNSSTypes.h"

#ifdef XP_WIN
#include "windows.h" // this needs to be before the following includes
#include "wincrypt.h"
#endif // XP_WIN

nsresult GatherEnterpriseRoots(mozilla::UniqueCERTCertList& result);

#ifdef XP_WIN

mozilla::UniqueCERTCertificate PCCERT_CONTEXTToCERTCertificate(PCCERT_CONTEXT pccert);

extern const wchar_t* kWindowsDefaultRootStoreName;
extern const nsLiteralCString kMicrosoftFamilySafetyCN;

// Because HCERTSTORE is just a typedef void*, we can't use any of the nice
// scoped or unique pointer templates. To elaborate, any attempt would
// instantiate those templates with T = void. When T gets used in the context
// of T&, this results in void&, which isn't legal.
class ScopedCertStore final
{
public:
  explicit ScopedCertStore(HCERTSTORE certstore) : certstore(certstore) {}

  ~ScopedCertStore()
  {
    CertCloseStore(certstore, 0);
  }

  HCERTSTORE get()
  {
    return certstore;
  }

private:
  ScopedCertStore(const ScopedCertStore&) = delete;
  ScopedCertStore& operator=(const ScopedCertStore&) = delete;
  HCERTSTORE certstore;
};

#endif // XP_WIN

#endif // EnterpriseRoots_h
