/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WinDllServices_h
#define mozilla_WinDllServices_h

#include "mozilla/glue/WindowsDllServices.h"
#include "mozilla/mozalloc.h"
#include "mozilla/Vector.h"

namespace mozilla {

class DllServices : public mozilla::glue::DllServices
{
public:
  static DllServices* Get();

  static const char* kTopicDllLoadedMainThread;
  static const char* kTopicDllLoadedNonMainThread;

private:
  DllServices();
  ~DllServices() = default;

  void NotifyDllLoad(const bool aIsMainThread, const nsString& aDllName) override;

  void NotifyUntrustedModuleLoads(
    const Vector<glue::ModuleLoadEvent, 0, InfallibleAllocPolicy>& aEvents) override;
};

} // namespace mozilla

#endif // mozilla_WinDllServices_h
