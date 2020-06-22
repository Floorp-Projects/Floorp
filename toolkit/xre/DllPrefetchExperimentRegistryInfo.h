/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DllPrefetchExperimentRegistryInfo_h
#define mozilla_DllPrefetchExperimentRegistryInfo_h

#include "mozilla/Maybe.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "nsWindowsHelpers.h"

/**
 * This is a temporary file in order to conduct an experiment on the impact of
 * startup time on retention (see Bug 1640087).
 * This was generally adapted from LauncherRegistryInfo.h
 */

namespace mozilla {

enum class AlteredDllPrefetchMode {
  CurrentPrefetch,
  NoPrefetch,
  OptimizedPrefetch
};

class DllPrefetchExperimentRegistryInfo final {
 public:
  DllPrefetchExperimentRegistryInfo() : mBinPath(GetFullBinaryPath().get()) {}
  ~DllPrefetchExperimentRegistryInfo() {}

  Result<Ok, nsresult> ReflectPrefToRegistry(int32_t aVal);
  Result<Ok, nsresult> ReadRegistryValueData(DWORD expectedType);

  AlteredDllPrefetchMode GetAlteredDllPrefetchMode();

 private:
  Result<Ok, nsresult> Open();

  nsAutoRegKey mRegKey;
  nsString mBinPath;
  int32_t mPrefetchMode;

  static const wchar_t kExperimentSubKeyPath[];
};

}  // namespace mozilla

#endif  // mozilla_DllPrefetchExperimentRegistryInfo_h
