/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIPrefService.h"
#include "nsIX509CertDB.h"
#include "nsServiceManagerUtils.h"

int
main(int argc, char* argv[])
{
  {
    NS_InitXPCOM2(nullptr, nullptr, nullptr);
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (!prefs) {
      return -1;
    }
    // When NSS initializes, it attempts to get some localized strings.
    // As a result, OS X and Windows flip out if this isn't set.
    // (This isn't done automatically since this test doesn't have a
    // lot of the other boilerplate components that would otherwise
    // keep the certificate db alive longer than we want it to.)
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
  NS_ShutdownXPCOM(nullptr);
  return 0;
}
