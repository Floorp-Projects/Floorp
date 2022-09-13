/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MockHttpAuth_h__
#define MockHttpAuth_h__

#include "nsIHttpAuthenticator.h"

namespace mozilla::net {

// This authenticator is only used for testing purposes.
class MockHttpAuth : public nsIHttpAuthenticator {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHTTPAUTHENTICATOR

  MockHttpAuth() = default;

  static already_AddRefed<nsIHttpAuthenticator> Create() {
    nsCOMPtr<nsIHttpAuthenticator> authenticator = new MockHttpAuth();
    return authenticator.forget();
  }

 private:
  virtual ~MockHttpAuth() = default;
};

}  // namespace mozilla::net

#endif  // MockHttpAuth_h__
