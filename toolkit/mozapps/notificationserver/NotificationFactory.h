/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NotificationFactory_h__
#define NotificationFactory_h__

#include <filesystem>

#include "NotificationCallback.h"

using namespace std::filesystem;
using namespace Microsoft::WRL;

class NotificationFactory final : public ClassFactory<> {
 public:
  HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* pUnkOuter, REFIID riid,
                                           void** ppvObject) final;

  explicit NotificationFactory(const GUID& runtimeGuid,
                               const path& dllInstallDir)
      : notificationGuid(runtimeGuid), installDir(dllInstallDir) {}

 private:
  const GUID notificationGuid = {};
  const path installDir = {};
};

#endif
