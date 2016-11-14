/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsCOMPtr.h"
#include "nsIPrefService.h"
#include "nsISimpleEnumerator.h"
#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsIX509CertList.h"
#include "nsServiceManagerUtils.h"

TEST(psm_CertDB, Test)
{
  {
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
    ASSERT_TRUE(prefs) << "couldn't get nsIPrefBranch";

    // When PSM initializes, it attempts to get some localized strings.
    // As a result, Android flips out if this isn't set.
    nsresult rv = prefs->SetBoolPref("intl.locale.matchOS", true);
    ASSERT_TRUE(NS_SUCCEEDED(rv)) << "couldn't set pref 'intl.locale.matchOS'";

    nsCOMPtr<nsIX509CertDB> certdb(do_GetService(NS_X509CERTDB_CONTRACTID));
    ASSERT_TRUE(certdb) << "couldn't get certdb";

    nsCOMPtr<nsIX509CertList> certList;
    rv = certdb->GetCerts(getter_AddRefs(certList));
    ASSERT_TRUE(NS_SUCCEEDED(rv)) << "couldn't get list of certificates";

    nsCOMPtr<nsISimpleEnumerator> enumerator;
    rv = certList->GetEnumerator(getter_AddRefs(enumerator));
    ASSERT_TRUE(NS_SUCCEEDED(rv)) << "couldn't enumerate certificate list";

    bool foundBuiltIn = false;
    bool hasMore = false;
    while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> supports;
      ASSERT_TRUE(NS_SUCCEEDED(enumerator->GetNext(getter_AddRefs(supports))))
        << "couldn't get next certificate";

      nsCOMPtr<nsIX509Cert> cert(do_QueryInterface(supports));
      ASSERT_TRUE(cert) << "couldn't QI to nsIX509Cert";

      ASSERT_TRUE(NS_SUCCEEDED(cert->GetIsBuiltInRoot(&foundBuiltIn))) <<
        "GetIsBuiltInRoot failed";

      if (foundBuiltIn) {
        break;
      }
    }

    ASSERT_TRUE(foundBuiltIn) << "didn't load any built-in certificates";

    printf("successfully loaded at least one built-in certificate\n");

  } // this scopes the nsCOMPtrs
}
