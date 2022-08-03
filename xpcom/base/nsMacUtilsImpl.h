/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMacUtilsImpl_h___
#define nsMacUtilsImpl_h___

#include "nsIMacUtils.h"
#include "nsString.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"

using mozilla::Atomic;
using mozilla::StaticAutoPtr;
using mozilla::StaticMutex;

class nsMacUtilsImpl final : public nsIMacUtils {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMACUTILS

  nsMacUtilsImpl() {}

  // Return the repo directory and the repo object directory respectively.
  // These should only be used on Mac developer builds to determine the path
  // to the repo or object directory.
  static nsresult GetRepoDir(nsIFile** aRepoDir);
  static nsresult GetObjDir(nsIFile** aObjDir);

#if defined(MOZ_SANDBOX) || defined(__aarch64__)
  static bool GetAppPath(nsCString& aAppPath);
#endif /* MOZ_SANDBOX || __aarch64__ */

#if defined(MOZ_SANDBOX) && defined(DEBUG)
  static nsresult GetBloatLogDir(nsCString& aDirectoryPath);
  static nsresult GetDirectoryPath(const char* aPath,
                                   nsCString& aDirectoryPath);
#endif /* MOZ_SANDBOX && DEBUG */

  static void EnableTCSMIfAvailable();
  static bool IsTCSMAvailable();
  static uint32_t GetPhysicalCPUCount();
  static nsresult GetArchitecturesForBundle(uint32_t* aArchMask);
  static nsresult GetArchitecturesForBinary(const char* aPath,
                                            uint32_t* aArchMask);

#if defined(__aarch64__)
  // Pre-translate binaries to avoid translation delays when launching
  // x64 child process instances for the first time. i.e. on first launch
  // after installation or after an update. Translations are cached so
  // repeated launches of the binaries do not encounter delays.
  static int PreTranslateXUL();
  static int PreTranslateBinary(nsCString aBinaryPath);
#endif

 private:
  ~nsMacUtilsImpl() {}

  nsresult GetArchString(nsAString& aArchString);

  // A string containing a "-" delimited list of architectures
  // in our binary.
  nsString mBinaryArchs;

#if defined(MOZ_SANDBOX) || defined(__aarch64__)
  // Cache the appDir returned from GetAppPath to avoid doing I/O
  static StaticAutoPtr<nsCString> sCachedAppPath
      MOZ_GUARDED_BY(sCachedAppPathMutex);
  // For thread safe setting/checking of sCachedAppPath
  static StaticMutex sCachedAppPathMutex;
  // Utility method to call ClearOnShutdown() on the main thread
  static nsresult ClearCachedAppPathOnShutdown();
#endif

  // The cached machine architectures of the .app bundle which can
  // be multiple architectures for universal binaries.
  static std::atomic<uint32_t> sBundleArchMaskAtomic;

#if defined(__aarch64__)
  // Limit XUL translation to one attempt
  static std::atomic<bool> sIsXULTranslated;
#endif

  enum TCSMStatus { TCSM_Unknown = 0, TCSM_Available, TCSM_Unavailable };
  static mozilla::Atomic<nsMacUtilsImpl::TCSMStatus> sTCSMStatus;

  static nsresult EnableTCSM();
#if defined(DEBUG)
  static bool IsTCSMEnabled();
#endif
};

// Global singleton service
// 697BD3FD-43E5-41CE-AD5E-C339175C0818
#define NS_MACUTILSIMPL_CID                          \
  {                                                  \
    0x697BD3FD, 0x43E5, 0x41CE, {                    \
      0xAD, 0x5E, 0xC3, 0x39, 0x17, 0x5C, 0x08, 0x18 \
    }                                                \
  }
#define NS_MACUTILSIMPL_CONTRACTID "@mozilla.org/xpcom/mac-utils;1"

#endif /* nsMacUtilsImpl_h___ */
