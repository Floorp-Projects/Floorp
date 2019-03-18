/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Sandbox_h
#define mozilla_Sandbox_h

#include "mozilla/Types.h"
#include "nsXULAppAPI.h"
#include <vector>

// This defines the entry points for a content process to start
// sandboxing itself.  See also SandboxInfo.h for what parts of
// sandboxing are enabled/supported.

namespace mozilla {

namespace ipc {
class FileDescriptor;
}  // namespace ipc

// This must be called early, before glib creates any worker threads.
// (See bug 1176099.)
MOZ_EXPORT void SandboxEarlyInit();

// A collection of sandbox parameters that have to be extracted from
// prefs or other libxul facilities and passed down, because
// libmozsandbox can't link against the APIs to read them.
struct ContentProcessSandboxParams {
  // Content sandbox level; see also GetEffectiveSandboxLevel in
  // SandboxSettings.h and the comments for the Linux version of
  // "security.sandbox.content.level" in browser/app/profile/firefox.js
  int mLevel = 0;
  // The filesystem broker client file descriptor, or -1 to allow
  // direct filesystem access.  (Warning: this is not a RAII class and
  // will not close the fd on destruction.)
  int mBrokerFd = -1;
  // Determines whether we allow reading all files, for processes that
  // render file:/// URLs.
  bool mFileProcess = false;
  // Syscall numbers to allow even if the seccomp-bpf policy otherwise
  // wouldn't.
  std::vector<int> mSyscallWhitelist;

  static ContentProcessSandboxParams ForThisProcess(
      const Maybe<ipc::FileDescriptor>& aBroker);
};

// Call only if SandboxInfo::CanSandboxContent() returns true.
// (No-op if the sandbox is disabled.)
// isFileProcess determines whether we allow system wide file reads.
MOZ_EXPORT bool SetContentProcessSandbox(ContentProcessSandboxParams&& aParams);

// Call only if SandboxInfo::CanSandboxMedia() returns true.
// (No-op if MOZ_DISABLE_GMP_SANDBOX is set.)
// aFilePath is the path to the plugin file.
MOZ_EXPORT void SetMediaPluginSandbox(const char* aFilePath);

MOZ_EXPORT void SetRemoteDataDecoderSandbox(int aBroker);

}  // namespace mozilla

#endif  // mozilla_Sandbox_h
