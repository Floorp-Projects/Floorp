/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSSYSTEMINFO_H_
#define _NSSYSTEMINFO_H_

#include "nsHashPropertyBag.h"
#include "nsISystemInfo.h"
#include "mozilla/MozPromise.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/dom/PContent.h"
#endif  // MOZ_WIDGET_ANDROID

#if defined(XP_WIN)
#  include <inspectable.h>

// The UUID comes from winrt/windows.system.profile.idl
// in the Windows SDK
MIDL_INTERFACE("7D1D81DB-8D63-4789-9EA5-DDCF65A94F3C")
IWindowsIntegrityPolicyStatics : public IInspectable {
 public:
  virtual HRESULT STDMETHODCALLTYPE get_IsEnabled(bool* value) = 0;
};
#endif

class nsISerialEventTarget;

struct FolderDiskInfo {
  nsCString model;
  nsCString revision;
  bool isSSD;
};

struct DiskInfo {
  FolderDiskInfo binary;
  FolderDiskInfo profile;
  FolderDiskInfo system;
};

struct OSInfo {
  uint32_t installYear;
  bool hasSuperfetch;
  bool hasPrefetch;
};

struct ProcessInfo {
  bool isWow64 = false;
  bool isWowARM64 = false;
  // Whether or not the system is Windows 10 or 11 in S Mode.
  // S Mode existed prior to us being able to query it, so this
  // is unreliable on Windows versions prior to 1810.
  bool isWindowsSMode = false;
  int32_t cpuCount = 0;
  int32_t cpuCores = 0;
  nsCString cpuVendor;
  nsCString cpuName;
  int32_t cpuFamily = 0;
  int32_t cpuModel = 0;
  int32_t cpuStepping = 0;
  int32_t l2cacheKB = 0;
  int32_t l3cacheKB = 0;
  int32_t cpuSpeed = 0;
};

typedef mozilla::MozPromise<DiskInfo, nsresult, /* IsExclusive */ false>
    DiskInfoPromise;

typedef mozilla::MozPromise<nsAutoString, nsresult, /* IsExclusive */ false>
    CountryCodePromise;

typedef mozilla::MozPromise<OSInfo, nsresult, /* IsExclusive */ false>
    OSInfoPromise;

typedef mozilla::MozPromise<ProcessInfo, nsresult, /* IsExclusive */ false>
    ProcessInfoPromise;

// Synchronous info collection, avoid calling it from the main thread, consider
// using the promise-based `nsISystemInfo::GetProcessInfo()` instead.
// Note that only known fields will be written.
nsresult CollectProcessInfo(ProcessInfo& info);

class nsSystemInfo final : public nsISystemInfo, public nsHashPropertyBag {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSISYSTEMINFO

  nsSystemInfo();

  nsresult Init();

  // Slot for NS_InitXPCOM to pass information to nsSystemInfo::Init.
  // See comments above the variable definition and in NS_InitXPCOM.
  static uint32_t gUserUmask;

#ifdef MOZ_WIDGET_ANDROID
  static void GetAndroidSystemInfo(mozilla::dom::AndroidSystemInfo* aInfo);

 protected:
  void SetupAndroidInfo(const mozilla::dom::AndroidSystemInfo&);
#endif

 protected:
  void SetInt32Property(const nsAString& aPropertyName, const int32_t aValue);
  void SetUint32Property(const nsAString& aPropertyName, const uint32_t aValue);
  void SetUint64Property(const nsAString& aPropertyName, const uint64_t aValue);

 private:
  ~nsSystemInfo();

  RefPtr<DiskInfoPromise> mDiskInfoPromise;
  RefPtr<CountryCodePromise> mCountryCodePromise;
  RefPtr<OSInfoPromise> mOSInfoPromise;
  RefPtr<ProcessInfoPromise> mProcessInfoPromise;
  RefPtr<nsISerialEventTarget> mBackgroundET;
  RefPtr<nsISerialEventTarget> GetBackgroundTarget();
};

#define NS_SYSTEMINFO_CONTRACTID "@mozilla.org/system-info;1"
#define NS_SYSTEMINFO_CID                            \
  {                                                  \
    0xd962398a, 0x99e5, 0x49b2, {                    \
      0x85, 0x7a, 0xc1, 0x59, 0x04, 0x9c, 0x7f, 0x6c \
    }                                                \
  }

#endif /* _NSSYSTEMINFO_H_ */
