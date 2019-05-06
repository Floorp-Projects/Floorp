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

#if defined(MOZ_SANDBOX)
  static bool GetAppPath(nsCString& aAppPath);

#  ifdef DEBUG
  static nsresult GetBloatLogDir(nsCString& aDirectoryPath);
  static nsresult GetDirectoryPath(const char* aPath,
                                   nsCString& aDirectoryPath);
#  endif /* DEBUG */
#endif   /* MOZ_SANDBOX */

  static void EnableTCSMIfAvailable();
  static bool IsTCSMAvailable();
  static uint32_t GetPhysicalCPUCount();

 private:
  ~nsMacUtilsImpl() {}

  nsresult GetArchString(nsAString& aArchString);

  // A string containing a "-" delimited list of architectures
  // in our binary.
  nsString mBinaryArchs;

#if defined(MOZ_SANDBOX)
  // Cache the appDir returned from GetAppPath to avoid doing I/O
  static StaticAutoPtr<nsCString> sCachedAppPath;
  // For thread safe setting/checking of sCachedAppPath
  static StaticMutex sCachedAppPathMutex;
#endif

  enum TCSMStatus {
    TCSM_Unknown = 0,
    TCSM_Available,
    TCSM_Unavailable
  };
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
