/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBasicAuth_h__
#define nsBasicAuth_h__

#include "nsIHttpAuthenticator.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// The nsHttpBasicAuth class produces HTTP Basic-auth responses for a username/
// (optional)password pair, BASE64("user:pass").
//-----------------------------------------------------------------------------

class nsHttpBasicAuth : public nsIHttpAuthenticator {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHTTPAUTHENTICATOR

  nsHttpBasicAuth() = default;

  static already_AddRefed<nsIHttpAuthenticator> GetOrCreate();

 private:
  virtual ~nsHttpBasicAuth() = default;

  static StaticRefPtr<nsHttpBasicAuth> gSingleton;
};

}  // namespace net
}  // namespace mozilla

#endif  // !nsHttpBasicAuth_h__
