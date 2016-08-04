/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Sandbox_h
#define mozilla_Sandbox_h

#include "mozilla/Types.h"
#include "nsXULAppAPI.h"

// This defines the entry points for a content process to start
// sandboxing itself.  See also common/SandboxInfo.h for what parts of
// sandboxing are enabled/supported.

#ifdef ANDROID
// Defined in libmozsandbox and referenced by linking against it.
#define MOZ_SANDBOX_EXPORT MOZ_EXPORT
#else
// Defined in plugin-container and referenced by libraries it loads.
#define MOZ_SANDBOX_EXPORT MOZ_EXPORT __attribute__((weak))
#endif

namespace mozilla {

// This must be called early, while the process is still single-threaded.
MOZ_SANDBOX_EXPORT void SandboxEarlyInit(GeckoProcessType aType);

#ifdef MOZ_CONTENT_SANDBOX
// Call only if SandboxInfo::CanSandboxContent() returns true.
// (No-op if MOZ_DISABLE_CONTENT_SANDBOX is set.)
// aBrokerFd is the filesystem broker client file descriptor,
// or -1 to allow direct filesystem access.
MOZ_SANDBOX_EXPORT bool SetContentProcessSandbox(int aBrokerFd);
#endif

#ifdef MOZ_GMP_SANDBOX
// Call only if SandboxInfo::CanSandboxMedia() returns true.
// (No-op if MOZ_DISABLE_GMP_SANDBOX is set.)
// aFilePath is the path to the plugin file.
MOZ_SANDBOX_EXPORT void SetMediaPluginSandbox(const char *aFilePath);
#endif

} // namespace mozilla

#endif // mozilla_Sandbox_h
