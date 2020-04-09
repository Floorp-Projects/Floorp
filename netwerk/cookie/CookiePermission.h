/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookiePermission_h
#define mozilla_net_CookiePermission_h

#include "nsICookiePermission.h"
#include "mozilla/PermissionManager.h"

namespace mozilla {
namespace net {

class CookiePermission final : public nsICookiePermission {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOOKIEPERMISSION

  // Singleton accessor
  static already_AddRefed<nsICookiePermission> GetOrCreate();

  bool Init();

 private:
  ~CookiePermission() = default;

  bool EnsureInitialized() { return (mPermMgr != nullptr) || Init(); };

  RefPtr<mozilla::PermissionManager> mPermMgr;
};

}  // namespace net
}  // namespace mozilla

#endif
