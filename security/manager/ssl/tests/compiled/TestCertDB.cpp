/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"
#include "nsCOMPtr.h"
#include "nsIPrefService.h"
#include "nsISimpleEnumerator.h"
#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsIX509CertList.h"
#include "nsServiceManagerUtils.h"

int
main(int argc, char* argv[])
{
  ScopedXPCOM xpcom("TestCertDB");
  if (xpcom.failed()) {
    fail("couldn't initialize XPCOM");
    return 1;
  }
  {
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (!prefs) {
      fail("couldn't get nsIPrefBranch");
      return 1;
    }
    // When PSM initializes, it attempts to get some localized strings.
    // As a result, Android flips out if this isn't set.
    nsresult rv = prefs->SetBoolPref("intl.locale.matchOS", true);
    if (NS_FAILED(rv)) {
      fail("couldn't set pref 'intl.locale.matchOS'");
      return 1;
    }
    nsCOMPtr<nsIX509CertDB> certdb(do_GetService(NS_X509CERTDB_CONTRACTID));
    if (!certdb) {
      fail("couldn't get certdb");
      return 1;
    }
    nsCOMPtr<nsIX509CertList> certList;
    rv = certdb->GetCerts(getter_AddRefs(certList));
    if (NS_FAILED(rv)) {
      fail("couldn't get list of certificates");
      return 1;
    }
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    rv = certList->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_FAILED(rv)) {
      fail("couldn't enumerate certificate list");
      return 1;
    }
    bool foundBuiltIn = false;
    bool hasMore = false;
    while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> supports;
      if (NS_FAILED(enumerator->GetNext(getter_AddRefs(supports)))) {
        fail("couldn't get next certificate");
        return 1;
      }
      nsCOMPtr<nsIX509Cert> cert(do_QueryInterface(supports));
      if (!cert) {
        fail("couldn't QI to nsIX509Cert");
        return 1;
      }
      if (NS_FAILED(cert->GetIsBuiltInRoot(&foundBuiltIn))) {
        fail("GetIsBuiltInRoot failed");
        return 1;
      }
      if (foundBuiltIn) {
        break;
      }
    }
    if (foundBuiltIn) {
      passed("successfully loaded at least one built-in certificate");
    } else {
      fail("didn't load any built-in certificates");
      return 1;
    }
  } // this scopes the nsCOMPtrs
  // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
  return 0;
}
