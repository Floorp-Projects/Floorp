/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieSettings_h
#define mozilla_net_CookieSettings_h

#include "nsICookieSettings.h"
#include "nsDataHashtable.h"

class nsIPermission;

namespace mozilla {
namespace net {

class CookieSettingsArgs;

/**
 * Class that provides an nsICookieSettings implementation.
 */
class CookieSettings final : public nsICookieSettings {
 public:
  typedef nsTArray<RefPtr<nsIPermission>> CookiePermissionList;

  NS_DECL_ISUPPORTS
  NS_DECL_NSICOOKIESETTINGS

  static already_AddRefed<nsICookieSettings> CreateBlockingAll();

  static already_AddRefed<nsICookieSettings> Create();

  void Serialize(CookieSettingsArgs& aData);

  static void Deserialize(const CookieSettingsArgs& aData,
                          nsICookieSettings** aCookieSettings);

  void Merge(const CookieSettingsArgs& aData);

 private:
  enum State {
    // No cookie permissions are allowed to be stored in this object.
    eFixed,

    // Cookie permissions can be stored in case they are unknown when they are
    // asked or when they are sent from the parent process.
    eProgressive,
  };

  CookieSettings(uint32_t aCookieBehavior, State aState);
  ~CookieSettings();

  uint32_t mCookieBehavior;
  CookiePermissionList mCookiePermissions;

  State mState;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieSettings_h
