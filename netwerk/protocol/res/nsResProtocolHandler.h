/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsResProtocolHandler_h___
#define nsResProtocolHandler_h___

#include "mozilla/net/SubstitutingProtocolHandler.h"

#include "nsIResProtocolHandler.h"
#include "nsInterfaceHashtable.h"
#include "nsWeakReference.h"

struct SubstitutionMapping;
class nsResProtocolHandler final
    : public nsIResProtocolHandler,
      public mozilla::net::SubstitutingProtocolHandler,
      public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRESPROTOCOLHANDLER

  static already_AddRefed<nsResProtocolHandler> GetSingleton();

  NS_FORWARD_NSIPROTOCOLHANDLER(mozilla::net::SubstitutingProtocolHandler::)

  nsResProtocolHandler()
      : mozilla::net::SubstitutingProtocolHandler(
            "resource",
            URI_STD | URI_IS_UI_RESOURCE | URI_IS_LOCAL_RESOURCE |
                URI_IS_POTENTIALLY_TRUSTWORTHY,
            /* aEnforceFileOrJar = */ false) {}

  NS_IMETHOD SetSubstitution(const nsACString& aRoot,
                             nsIURI* aBaseURI) override;
  NS_IMETHOD SetSubstitutionWithFlags(const nsACString& aRoot, nsIURI* aBaseURI,
                                      uint32_t aFlags) override;
  NS_IMETHOD HasSubstitution(const nsACString& aRoot, bool* aResult) override;

  NS_IMETHOD GetSubstitution(const nsACString& aRoot,
                             nsIURI** aResult) override {
    return mozilla::net::SubstitutingProtocolHandler::GetSubstitution(aRoot,
                                                                      aResult);
  }

  NS_IMETHOD ResolveURI(nsIURI* aResURI, nsACString& aResult) override {
    return mozilla::net::SubstitutingProtocolHandler::ResolveURI(aResURI,
                                                                 aResult);
  }

 protected:
  [[nodiscard]] nsresult GetSubstitutionInternal(const nsACString& aRoot,
                                                 nsIURI** aResult,
                                                 uint32_t* aFlags) override;
  virtual ~nsResProtocolHandler() = default;

  [[nodiscard]] bool ResolveSpecialCases(const nsACString& aHost,
                                         const nsACString& aPath,
                                         const nsACString& aPathname,
                                         nsACString& aResult) override;

  [[nodiscard]] virtual bool MustResolveJAR(const nsACString& aRoot) override {
    return aRoot.EqualsLiteral("android");
  }

 private:
  [[nodiscard]] nsresult Init();
  static mozilla::StaticRefPtr<nsResProtocolHandler> sSingleton;

  nsCString mAppURI;
  nsCString mGREURI;
#ifdef ANDROID
  // Used for resource://android URIs
  nsCString mApkURI;
  nsresult GetApkURI(nsACString& aResult);
#endif
};

#endif /* nsResProtocolHandler_h___ */
