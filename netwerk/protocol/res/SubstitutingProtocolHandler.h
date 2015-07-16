/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SubstitutingProtocolHandler_h___
#define SubstitutingProtocolHandler_h___

#include "nsISubstitutingProtocolHandler.h"

#include "nsInterfaceHashtable.h"
#include "nsIOService.h"
#include "nsStandardURL.h"
#include "mozilla/chrome/RegistryMessageUtils.h"

class nsIIOService;

namespace mozilla {

//
// Base class for resource://-like substitution protocols.
//
// If you add a new protocol, make sure to change nsChromeRegistryChrome
// to properly invoke CollectSubstitutions at the right time.
class SubstitutingProtocolHandler
{
public:
  SubstitutingProtocolHandler(const char* aScheme, uint32_t aFlags);

  NS_INLINE_DECL_REFCOUNTING(SubstitutingProtocolHandler);
  NS_DECL_NON_VIRTUAL_NSIPROTOCOLHANDLER;
  NS_DECL_NON_VIRTUAL_NSISUBSTITUTINGPROTOCOLHANDLER;

  void CollectSubstitutions(InfallibleTArray<SubstitutionMapping>& aResources);

protected:
  virtual ~SubstitutingProtocolHandler() {}

  void SendSubstitution(const nsACString& aRoot, nsIURI* aBaseURI);

  // Override this in the subclass to try additional lookups after checking
  // mSubstitutions.
  virtual nsresult GetSubstitutionInternal(const nsACString& aRoot, nsIURI** aResult)
  {
    *aResult = nullptr;
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsIIOService* IOService() { return mIOService; }

private:
  nsCString mScheme;
  uint32_t mFlags;
  nsInterfaceHashtable<nsCStringHashKey,nsIURI> mSubstitutions;
  nsCOMPtr<nsIIOService> mIOService;
};

// SubstitutingURL : overrides nsStandardURL::GetFile to provide nsIFile resolution
class SubstitutingURL : public nsStandardURL
{
public:
  SubstitutingURL() : nsStandardURL(true) {}
  virtual nsStandardURL* StartClone();
  virtual nsresult EnsureFile();
  NS_IMETHOD GetClassIDNoAlloc(nsCID *aCID);
};

} // namespace mozilla

#endif /* SubstitutingProtocolHandler_h___ */
