/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glue_ModuleLoadFrame_h
#define mozilla_glue_ModuleLoadFrame_h

#include "mozilla/Attributes.h"
#include "mozilla/LoaderAPIInterfaces.h"

namespace mozilla {
namespace glue {

class MOZ_RAII ModuleLoadFrame final {
 public:
  explicit ModuleLoadFrame(PCUNICODE_STRING aRequestedDllName);
  ~ModuleLoadFrame();

  void SetLoadStatus(NTSTATUS aNtStatus, HANDLE aHandle);

  ModuleLoadFrame(const ModuleLoadFrame&) = delete;
  ModuleLoadFrame(ModuleLoadFrame&&) = delete;
  ModuleLoadFrame& operator=(const ModuleLoadFrame&) = delete;
  ModuleLoadFrame& operator=(ModuleLoadFrame&&) = delete;

  static void StaticInit(nt::LoaderObserver* aNewObserver,
                         nt::WinLauncherFunctions* aOutWinLauncherFunctions);

 private:
  bool mAlreadyLoaded;
  void* mContext;
  NTSTATUS mDllLoadStatus;
  ModuleLoadInfo mLoadInfo;

 private:
  static nt::LoaderAPI* sLoaderAPI;
};

}  // namespace glue
}  // namespace mozilla

#endif  // mozilla_glue_ModuleLoadFrame_h
