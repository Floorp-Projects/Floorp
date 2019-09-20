/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WinDllServices_h
#define mozilla_WinDllServices_h

#include "mozilla/glue/WindowsDllServices.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UntrustedModulesProcessor.h"

namespace mozilla {

class DllServices : public glue::DllServices {
 public:
  static DllServices* Get();

  static const char* kTopicDllLoadedMainThread;
  static const char* kTopicDllLoadedNonMainThread;

  RefPtr<UntrustedModulesPromise> GetUntrustedModulesData();

 private:
  DllServices();
  ~DllServices() = default;

  void NotifyDllLoad(glue::EnhancedModuleLoadInfo&& aModLoadInfo) override;
  void NotifyModuleLoadBacklog(ModuleLoadInfoVec&& aEvents) override;

  RefPtr<UntrustedModulesProcessor> mUntrustedModulesProcessor;
};

}  // namespace mozilla

#endif  // mozilla_WinDllServices_h
