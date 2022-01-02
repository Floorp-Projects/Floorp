/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSOCKSSocketProvider_h__
#define nsSOCKSSocketProvider_h__

#include "nsISocketProvider.h"

// values for ctor's |version| argument
enum { NS_SOCKS_VERSION_4 = 4, NS_SOCKS_VERSION_5 = 5 };

class nsSOCKSSocketProvider : public nsISocketProvider {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISOCKETPROVIDER

  explicit nsSOCKSSocketProvider(uint32_t version) : mVersion(version) {}

  static nsresult CreateV4(nsISupports*, REFNSIID aIID, void** aResult);
  static nsresult CreateV5(nsISupports*, REFNSIID aIID, void** aResult);

 private:
  virtual ~nsSOCKSSocketProvider() = default;

  uint32_t mVersion;  // NS_SOCKS_VERSION_4 or 5
};

#endif /* nsSOCKSSocketProvider_h__ */
