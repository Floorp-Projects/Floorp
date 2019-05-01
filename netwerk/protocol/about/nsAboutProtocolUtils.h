/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAboutProtocolUtils_h
#define nsAboutProtocolUtils_h

#include "nsIURI.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIAboutModule.h"
#include "nsServiceManagerUtils.h"
#include "prtime.h"

inline MOZ_MUST_USE nsresult NS_GetAboutModuleName(nsIURI* aAboutURI,
                                                   nsCString& aModule) {
#ifdef DEBUG
  {
    bool isAbout;
    NS_ASSERTION(
        NS_SUCCEEDED(aAboutURI->SchemeIs("about", &isAbout)) && isAbout,
        "should be used only on about: URIs");
  }
#endif

  nsresult rv = aAboutURI->GetPathQueryRef(aModule);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t f = aModule.FindCharInSet(NS_LITERAL_CSTRING("#?"));
  if (f != kNotFound) {
    aModule.Truncate(f);
  }

  // convert to lowercase, as all about: modules are lowercase
  ToLowerCase(aModule);
  return NS_OK;
}

inline nsresult NS_GetAboutModule(nsIURI* aAboutURI, nsIAboutModule** aModule) {
  MOZ_ASSERT(aAboutURI, "Must have URI");

  nsAutoCString contractID;
  nsresult rv = NS_GetAboutModuleName(aAboutURI, contractID);
  if (NS_FAILED(rv)) return rv;

  // look up a handler to deal with "what"
  contractID.InsertLiteral(NS_ABOUT_MODULE_CONTRACTID_PREFIX, 0);

  return CallGetService(contractID.get(), aModule);
}

inline PRTime SecondsToPRTime(uint32_t t_sec) {
  PRTime t_usec, usec_per_sec;
  t_usec = t_sec;
  usec_per_sec = PR_USEC_PER_SEC;
  return t_usec *= usec_per_sec;
}
inline void PrintTimeString(char* buf, uint32_t bufsize, uint32_t t_sec) {
  PRExplodedTime et;
  PRTime t_usec = SecondsToPRTime(t_sec);
  PR_ExplodeTime(t_usec, PR_LocalTimeParameters, &et);
  PR_FormatTime(buf, bufsize, "%Y-%m-%d %H:%M:%S", &et);
}

#endif
