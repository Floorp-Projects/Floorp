/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMIMEInfoChild_h
#define nsMIMEInfoChild_h

#include "nsMIMEInfoImpl.h"

/*
 * A platform-generic nsMIMEInfo implementation to be used in child process
 * generic code that needs a MIMEInfo with limited functionality.
 */
class nsChildProcessMIMEInfo : public nsMIMEInfoImpl {
 public:
  explicit nsChildProcessMIMEInfo(const char* aMIMEType = "")
      : nsMIMEInfoImpl(aMIMEType) {}

  explicit nsChildProcessMIMEInfo(const nsACString& aMIMEType)
      : nsMIMEInfoImpl(aMIMEType) {}

  nsChildProcessMIMEInfo(const nsACString& aType, HandlerClass aClass)
      : nsMIMEInfoImpl(aType, aClass) {}

  NS_IMETHOD LaunchWithFile(nsIFile* aFile) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  };

  NS_IMETHOD IsCurrentAppOSDefault(bool* _retval) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  };

 protected:
  [[nodiscard]] virtual nsresult LoadUriInternal(nsIURI* aURI) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  };

#ifdef DEBUG
  [[nodiscard]] virtual nsresult LaunchDefaultWithFile(
      nsIFile* aFile) override {
    return NS_ERROR_UNEXPECTED;
  }
#endif
  [[nodiscard]] static nsresult OpenApplicationWithURI(nsIFile* aApplication,
                                                       const nsCString& aURI) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD GetDefaultDescription(nsAString& aDefaultDescription) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  };
};

#endif
