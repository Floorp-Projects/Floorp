/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SubstitutingProtocolHandler_h___
#define SubstitutingProtocolHandler_h___

#include "nsISubstitutingProtocolHandler.h"

#include "nsTHashMap.h"
#include "nsStandardURL.h"
#include "nsJARURI.h"
#include "mozilla/chrome/RegistryMessageUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/RWLock.h"

class nsIIOService;

namespace mozilla {
namespace net {

//
// Base class for resource://-like substitution protocols.
//
// If you add a new protocol, make sure to change nsChromeRegistryChrome
// to properly invoke CollectSubstitutions at the right time.
class SubstitutingProtocolHandler {
 public:
  SubstitutingProtocolHandler(const char* aScheme, uint32_t aFlags,
                              bool aEnforceFileOrJar = true);
  explicit SubstitutingProtocolHandler(const char* aScheme);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SubstitutingProtocolHandler);
  NS_DECL_NON_VIRTUAL_NSIPROTOCOLHANDLER;
  NS_DECL_NON_VIRTUAL_NSISUBSTITUTINGPROTOCOLHANDLER;

  bool HasSubstitution(const nsACString& aRoot) const {
    AutoReadLock lock(const_cast<RWLock&>(mSubstitutionsLock));
    return mSubstitutions.Get(aRoot, nullptr);
  }

  nsresult NewURI(const nsACString& aSpec, const char* aCharset,
                  nsIURI* aBaseURI, nsIURI** aResult);

  [[nodiscard]] nsresult CollectSubstitutions(
      nsTArray<SubstitutionMapping>& aMappings);

 protected:
  virtual ~SubstitutingProtocolHandler() = default;
  void ConstructInternal();

  [[nodiscard]] nsresult SendSubstitution(const nsACString& aRoot,
                                          nsIURI* aBaseURI, uint32_t aFlags);

  nsresult GetSubstitutionFlags(const nsACString& root, uint32_t* flags);

  // Override this in the subclass to try additional lookups after checking
  // mSubstitutions.
  [[nodiscard]] virtual nsresult GetSubstitutionInternal(
      const nsACString& aRoot, nsIURI** aResult, uint32_t* aFlags) {
    *aResult = nullptr;
    *aFlags = 0;
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Override this in the subclass to check for special case when resolving URIs
  // _before_ checking substitutions.
  [[nodiscard]] virtual bool ResolveSpecialCases(const nsACString& aHost,
                                                 const nsACString& aPath,
                                                 const nsACString& aPathname,
                                                 nsACString& aResult) {
    return false;
  }

  // This method should only return true if GetSubstitutionInternal would
  // return the RESOLVE_JAR_URI flag.
  [[nodiscard]] virtual bool MustResolveJAR(const nsACString& aRoot) {
    return false;
  }

  // Override this in the subclass to check for special case when opening
  // channels.
  [[nodiscard]] virtual nsresult SubstituteChannel(nsIURI* uri,
                                                   nsILoadInfo* aLoadInfo,
                                                   nsIChannel** result) {
    return NS_OK;
  }

  nsIIOService* IOService() { return mIOService; }

 private:
  struct SubstitutionEntry {
    nsCOMPtr<nsIURI> baseURI;
    uint32_t flags = 0;
  };

  // Notifies all observers that a new substitution from |aRoot| to
  // |aBaseURI| has been set/installed for this protocol handler.
  void NotifyObservers(const nsACString& aRoot, nsIURI* aBaseURI);

  nsCString mScheme;
  Maybe<uint32_t> mFlags;

  RWLock mSubstitutionsLock;
  nsTHashMap<nsCStringHashKey, SubstitutionEntry> mSubstitutions
      MOZ_GUARDED_BY(mSubstitutionsLock);
  nsCOMPtr<nsIIOService> mIOService;

  // Returns a SubstitutingJARURI if |aUrl| maps to a |jar:| URI,
  // otherwise will return |aURL|
  nsresult ResolveJARURI(nsIURL* aURL, nsIURI** aResult);

  // In general, we expect the principal of a document loaded from a
  // substituting URI to be a content principal for that URI (rather than
  // a principal for whatever is underneath). However, this only works if
  // the protocol handler for the underlying URI doesn't set an explicit
  // owner (which chrome:// does, for example). So we want to require that
  // substituting URIs only map to other URIs of the same type, or to
  // file:// and jar:// URIs.
  //
  // Enforcing this for ye olde resource:// URIs could carry compat risks, so
  // we just try to enforce it on new protocols going forward.
  bool mEnforceFileOrJar;
};

}  // namespace net
}  // namespace mozilla

#endif /* SubstitutingProtocolHandler_h___ */
