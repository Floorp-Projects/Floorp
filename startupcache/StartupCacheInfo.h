/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StartupCacheInfo_h_
#define StartupCacheInfo_h_

#include "nsIStartupCacheInfo.h"

namespace mozilla {
namespace scache {

class StartupCacheInfo final : public nsIStartupCacheInfo {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTARTUPCACHEINFO

  StartupCacheInfo() = default;

 private:
  ~StartupCacheInfo() = default;
};

}  // namespace scache
}  // namespace mozilla

#endif  // StartupCache_h_
