/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIPrefService.h"
#include "nsIX509CertDB.h"
#include "nsServiceManagerUtils.h"
#include "TestHarness.h"

int
main(int argc, char* argv[])
{
  ScopedXPCOM xpcom("TestCertDB");
  {
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (!prefs) {
      return -1;
    }
    // When NSS initializes, it attempts to get some localized strings.
    // As a result, Android flips out if this isn't set.
    nsresult rv = prefs->SetBoolPref("intl.locale.matchOS", true);
    if (NS_FAILED(rv)) {
      return -1;
    }
    nsCOMPtr<nsIX509CertDB> certdb(do_GetService(NS_X509CERTDB_CONTRACTID));
    if (!certdb) {
      return -1;
    }
  } // this scopes the nsCOMPtrs
  // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
  return 0;
}
