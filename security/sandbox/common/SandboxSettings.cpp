/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozISandboxSettings.h"

#include "mozilla/Omnijar.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/Preferences.h"

#include "nsDirectoryServiceDefs.h"

#if defined(XP_MACOSX)
#include <CoreServices/CoreServices.h>
// Info.plist key associated with the developer repo path
#define MAC_DEV_REPO_KEY "MozillaDeveloperRepoPath"
// Info.plist key associated with the developer repo object directory
#define MAC_DEV_OBJ_KEY "MozillaDeveloperObjPath"
#else
#include "prenv.h"
#endif /* XP_MACOSX */

namespace mozilla {

bool IsDevelopmentBuild()
{
  nsCOMPtr<nsIFile> path = mozilla::Omnijar::GetPath(mozilla::Omnijar::GRE);
  // If the path doesn't exist, we're a dev build.
  return path == nullptr;
}

#if defined(XP_MACOSX)
/*
 * Helper function to read a string value for a given key from the .app's
 * Info.plist.
 */
static nsresult
GetStringValueFromBundlePlist(const nsAString& aKey, nsAutoCString& aValue)
{
  CFBundleRef mainBundle = CFBundleGetMainBundle();

  // Read this app's bundle Info.plist as a dictionary
  CFDictionaryRef bundleInfoDict = CFBundleGetInfoDictionary(mainBundle);
  if (bundleInfoDict == NULL) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString keyAutoCString = NS_ConvertUTF16toUTF8(aKey);
  CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault,
                                              keyAutoCString.get(),
                                              kCFStringEncodingUTF8);

  CFStringRef value = (CFStringRef)CFDictionaryGetValue(bundleInfoDict, key);
  const char* valueCString = CFStringGetCStringPtr(value,
                                                   kCFStringEncodingUTF8);
  aValue.Assign(valueCString);
  CFRelease(key);

  return NS_OK;
}

/*
 * Helper function for reading a path string from the .app's Info.plist
 * and returning a directory object for that path with symlinks resolved.
 */
static nsresult
GetDirFromBundlePlist(const nsAString& aKey, nsIFile **aDir)
{
  nsresult rv;

  nsAutoCString dirPath;
  rv = GetStringValueFromBundlePlist(aKey, dirPath);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> dir;
  rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(dirPath),
                       false,
                       getter_AddRefs(dir));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dir->Normalize();
  NS_ENSURE_SUCCESS(rv, rv);

  bool isDirectory = false;
  rv = dir->IsDirectory(&isDirectory);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isDirectory) {
    return NS_ERROR_FILE_NOT_DIRECTORY;
  }

  dir.swap(*aDir);
  return NS_OK;
}

#else /* !XP_MACOSX */

/*
 * Helper function for getting a directory object for a given env variable
 */
static nsresult
GetDirFromEnv(const char* aEnvVar, nsIFile **aDir)
{
  nsresult rv;

  nsCOMPtr<nsIFile> dir;
  const char *dir_path = PR_GetEnv(aEnvVar);
  if (!dir_path) {
    return NS_ERROR_INVALID_ARG;
  }

  rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(dir_path),
                       false,
                       getter_AddRefs(dir));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dir->Normalize();
  NS_ENSURE_SUCCESS(rv, rv);

  bool isDirectory = false;
  rv = dir->IsDirectory(&isDirectory);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isDirectory) {
    return NS_ERROR_FILE_NOT_DIRECTORY;
  }

  dir.swap(*aDir);
  return NS_OK;
}
#endif /* XP_MACOSX */

nsresult
GetRepoDir(nsIFile **aRepoDir)
{
  MOZ_ASSERT(IsDevelopmentBuild());
#if defined(XP_MACOSX)
  return GetDirFromBundlePlist(NS_LITERAL_STRING(MAC_DEV_REPO_KEY), aRepoDir);
#else
  return GetDirFromEnv("MOZ_DEVELOPER_REPO_DIR", aRepoDir);
#endif /* XP_MACOSX */
}

nsresult
GetObjDir(nsIFile **aObjDir)
{
  MOZ_ASSERT(IsDevelopmentBuild());
#if defined(XP_MACOSX)
  return GetDirFromBundlePlist(NS_LITERAL_STRING(MAC_DEV_OBJ_KEY), aObjDir);
#else
  return GetDirFromEnv("MOZ_DEVELOPER_OBJ_DIR", aObjDir);
#endif /* XP_MACOSX */
}

int GetEffectiveContentSandboxLevel() {
  int level = Preferences::GetInt("security.sandbox.content.level");
// On Windows and macOS, enforce a minimum content sandbox level of 1 (except on
// Nightly, where it can be set to 0).
#if !defined(NIGHTLY_BUILD) && (defined(XP_WIN) || defined(XP_MACOSX))
  if (level < 1) {
    level = 1;
  }
#endif
  return level;
}

class SandboxSettings final : public mozISandboxSettings
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISANDBOXSETTINGS

  SandboxSettings() { }

private:
  ~SandboxSettings() { }
};

NS_IMPL_ISUPPORTS(SandboxSettings, mozISandboxSettings)

NS_IMETHODIMP SandboxSettings::GetEffectiveContentSandboxLevel(int32_t *aRetVal)
{
  *aRetVal = mozilla::GetEffectiveContentSandboxLevel();
  return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(SandboxSettings)

NS_DEFINE_NAMED_CID(MOZ_SANDBOX_SETTINGS_CID);

static const mozilla::Module::CIDEntry kSandboxSettingsCIDs[] = {
  { &kMOZ_SANDBOX_SETTINGS_CID, false, nullptr, SandboxSettingsConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kSandboxSettingsContracts[] = {
  { MOZ_SANDBOX_SETTINGS_CONTRACTID, &kMOZ_SANDBOX_SETTINGS_CID },
  { nullptr }
};

static const mozilla::Module kSandboxSettingsModule = {
  mozilla::Module::kVersion,
  kSandboxSettingsCIDs,
  kSandboxSettingsContracts
};

NSMODULE_DEFN(SandboxSettingsModule) = &kSandboxSettingsModule;

} // namespace mozilla
