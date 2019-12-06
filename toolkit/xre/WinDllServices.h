/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WinDllServices_h
#define mozilla_WinDllServices_h

#include "mozilla/glue/WindowsDllServices.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

class UntrustedModulesData;
class UntrustedModulesProcessor;

using UntrustedModulesPromise =
    MozPromise<Maybe<UntrustedModulesData>, nsresult, true>;

struct ModulePaths;
class ModulesMapResult;

using ModulesTrustPromise = MozPromise<ModulesMapResult, nsresult, true>;

class DllServices : public glue::DllServices {
 public:
  static DllServices* Get();

  virtual void DisableFull() override;

  static const char* kTopicDllLoadedMainThread;
  static const char* kTopicDllLoadedNonMainThread;

  void StartUntrustedModulesProcessor();

  RefPtr<UntrustedModulesPromise> GetUntrustedModulesData();

  RefPtr<ModulesTrustPromise> GetModulesTrust(ModulePaths&& aModPaths,
                                              bool aRunAtNormalPriority);

 private:
  DllServices() = default;
  ~DllServices() = default;

  void NotifyDllLoad(glue::EnhancedModuleLoadInfo&& aModLoadInfo) override;
  void NotifyModuleLoadBacklog(ModuleLoadInfoVec&& aEvents) override;

  RefPtr<UntrustedModulesProcessor> mUntrustedModulesProcessor;
};

}  // namespace mozilla

#endif  // mozilla_WinDllServices_h
