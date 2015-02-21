/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Sandbox_h
#define mozilla_Sandbox_h

#include "nsString.h"

enum MacSandboxType {
  MacSandboxType_Default = 0,
  MacSandboxType_Plugin,
  MacSandboxType_Content,
  MacSandboxType_Invalid
};

enum MacSandboxPluginType {
  MacSandboxPluginType_Default = 0,
  MacSandboxPluginType_GMPlugin_Default,  // Any Gecko Media Plugin
  MacSandboxPluginType_GMPlugin_OpenH264, // Gecko Media Plugin, OpenH264
  MacSandboxPluginType_GMPlugin_EME,      // Gecko Media Plugin, EME
  MacSandboxPluginType_Invalid
};

typedef struct _MacSandboxPluginInfo {
  _MacSandboxPluginInfo()
    : type(MacSandboxPluginType_Default) {}
  MacSandboxPluginType type;
  nsCString pluginPath;
  nsCString pluginBinaryPath;
} MacSandboxPluginInfo;

typedef struct _MacSandboxInfo {
  _MacSandboxInfo()
    : type(MacSandboxType_Default) {}
  MacSandboxType type;
  MacSandboxPluginInfo pluginInfo;
  nsCString appPath;
  nsCString appBinaryPath;
  nsCString appDir;
} MacSandboxInfo;

namespace mozilla {

bool StartMacSandbox(MacSandboxInfo aInfo, nsCString &aErrorMessage);

} // namespace mozilla

#endif // mozilla_Sandbox_h
