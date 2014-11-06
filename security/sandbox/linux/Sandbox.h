/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Sandbox_h
#define mozilla_Sandbox_h

#include "mozilla/Types.h"

namespace mozilla {

// Whether a given type of sandboxing is available, and why not:
enum SandboxStatus {
  // Sandboxing is enabled in Gecko but not available on this system;
  // trying to start the sandbox will crash:
  kSandboxingWouldFail,
  // Sandboxing is enabled in Gecko and is available:
  kSandboxingSupported,
  // Sandboxing is disabled, either at compile-time or at run-time;
  // trying to start the sandbox is a no-op:
  kSandboxingDisabled,
};


#ifdef MOZ_CONTENT_SANDBOX
// Disabled by setting env var MOZ_DISABLE_CONTENT_SANDBOX.
MOZ_EXPORT SandboxStatus ContentProcessSandboxStatus();
MOZ_EXPORT void SetContentProcessSandbox();
#else
static inline SandboxStatus ContentProcessSandboxStatus()
{
  return kSandboxingDisabled;
}
static inline void SetContentProcessSandbox()
{
}
#endif

#ifdef MOZ_GMP_SANDBOX
// Disabled by setting env var MOZ_DISABLE_GMP_SANDBOX.
MOZ_EXPORT SandboxStatus MediaPluginSandboxStatus();
MOZ_EXPORT void SetMediaPluginSandbox(const char *aFilePath);
#else
static inline SandboxStatus MediaPluginSandboxStatus()
{
  return kSandboxingDisabled;
}
static inline void SetMediaPluginSandbox()
{
}
#endif

// System-level security features which are relevant to our sandboxing
// and which aren't available on all Linux systems supported by Gecko.
enum SandboxFeatureFlags {
  kSandboxFeatureSeccompBPF = 1 << 0,
};

MOZ_EXPORT SandboxFeatureFlags GetSandboxFeatureFlags();

} // namespace mozilla

#endif // mozilla_Sandbox_h
