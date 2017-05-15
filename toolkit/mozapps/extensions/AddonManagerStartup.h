/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AddonManagerStartup_h
#define AddonManagerStartup_h

#include "amIAddonManagerStartup.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsISupports.h"

#include "jsapi.h"

namespace mozilla {

class Addon;

class AddonManagerStartup final : public amIAddonManagerStartup
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_AMIADDONMANAGERSTARTUP

  AddonManagerStartup();

  static AddonManagerStartup& GetSingleton();

  static already_AddRefed<AddonManagerStartup> GetInstance()
  {
    RefPtr<AddonManagerStartup> inst = &GetSingleton();
    return inst.forget();
  }

  const nsCOMArray<nsIFile>& ExtensionPaths()
  {
    return mExtensionPaths;
  }

  const nsCOMArray<nsIFile>& ThemePaths()
  {
    return mExtensionPaths;
  }

private:
  void AddInstallLocation(Addon& addon);

  nsIFile* ProfileDir();

  nsCOMPtr<nsIFile> mProfileDir;

  nsCOMArray<nsIFile> mExtensionPaths;
  nsCOMArray<nsIFile> mThemePaths;

  bool mInitialized;

protected:
  virtual ~AddonManagerStartup() = default;
};

} // namespace mozilla

#define NS_ADDONMANAGERSTARTUP_CONTRACTID \
  "@mozilla.org/addons/addon-manager-startup;1"

// {17a59a6b-92b8-42e5-bce0-ab434c7a7135
#define NS_ADDON_MANAGER_STARTUP_CID \
{ 0x17a59a6b, 0x92b8, 0x42e5, \
  { 0xbc, 0xe0, 0xab, 0x43, 0x4c, 0x7a, 0x71, 0x35 } }

#endif // AddonManagerStartup_h
