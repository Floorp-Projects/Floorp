/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMIMEInfoMac_h_
#define nsMIMEInfoMac_h_

#include "nsMIMEInfoImpl.h"

class nsMIMEInfoMac : public nsMIMEInfoImpl {
 public:
  explicit nsMIMEInfoMac(const char* aMIMEType = "")
      : nsMIMEInfoImpl(aMIMEType) {}
  explicit nsMIMEInfoMac(const nsACString& aMIMEType)
      : nsMIMEInfoImpl(aMIMEType) {}
  nsMIMEInfoMac(const nsACString& aType, HandlerClass aClass)
      : nsMIMEInfoImpl(aType, aClass) {}

  NS_IMETHOD LaunchWithFile(nsIFile* aFile) override;

 protected:
  [[nodiscard]] virtual nsresult LoadUriInternal(nsIURI* aURI) override;
#ifdef DEBUG
  [[nodiscard]] virtual nsresult LaunchDefaultWithFile(
      nsIFile* aFile) override {
    MOZ_ASSERT_UNREACHABLE("do not call this method, use LaunchWithFile");
    return NS_ERROR_UNEXPECTED;
  }
#endif
  NS_IMETHOD GetDefaultDescription(nsAString& aDefaultDescription) override;
};

#endif
