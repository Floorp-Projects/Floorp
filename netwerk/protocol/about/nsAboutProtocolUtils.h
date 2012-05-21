/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIURI.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIAboutModule.h"

inline nsresult
NS_GetAboutModuleName(nsIURI *aAboutURI, nsCString& aModule)
{
#ifdef DEBUG
    {
        bool isAbout;
        NS_ASSERTION(NS_SUCCEEDED(aAboutURI->SchemeIs("about", &isAbout)) &&
                     isAbout,
                     "should be used only on about: URIs");
    }
#endif

    nsresult rv = aAboutURI->GetPath(aModule);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 f = aModule.FindCharInSet(NS_LITERAL_CSTRING("#?"));
    if (f != kNotFound) {
        aModule.Truncate(f);
    }

    // convert to lowercase, as all about: modules are lowercase
    ToLowerCase(aModule);
    return NS_OK;
}

inline nsresult
NS_GetAboutModule(nsIURI *aAboutURI, nsIAboutModule** aModule)
{
  NS_PRECONDITION(aAboutURI, "Must have URI");

  nsCAutoString contractID;
  nsresult rv = NS_GetAboutModuleName(aAboutURI, contractID);
  if (NS_FAILED(rv)) return rv;

  // look up a handler to deal with "what"
  contractID.Insert(NS_LITERAL_CSTRING(NS_ABOUT_MODULE_CONTRACTID_PREFIX), 0);

  return CallGetService(contractID.get(), aModule);
}
