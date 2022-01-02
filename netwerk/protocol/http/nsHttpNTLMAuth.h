/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpNTLMAuth_h__
#define nsHttpNTLMAuth_h__

#include "nsIHttpAuthenticator.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace net {

class nsHttpNTLMAuth : public nsIHttpAuthenticator {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHTTPAUTHENTICATOR

  nsHttpNTLMAuth() = default;

  static already_AddRefed<nsIHttpAuthenticator> GetOrCreate();

 private:
  virtual ~nsHttpNTLMAuth() = default;

  // This flag indicates whether we are using the native NTLM implementation
  // or the internal one.
  bool mUseNative{false};

  static StaticRefPtr<nsHttpNTLMAuth> gSingleton;
};

}  // namespace net
}  // namespace mozilla

#endif  // !nsHttpNTLMAuth_h__
