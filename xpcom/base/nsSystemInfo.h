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
#include "mozilla/LazyIdleThread.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/dom/PContent.h"
#endif  // MOZ_WIDGET_ANDROID

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
  bool isWow64;
  bool isWowARM64;
  int32_t cpuCount;
  int32_t cpuCores;
  nsCString cpuVendor;
  int32_t cpuFamily;
  int32_t cpuModel;
  int32_t cpuStepping;
  int32_t l2cacheKB;
  int32_t l3cacheKB;
  int32_t cpuSpeed;
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
