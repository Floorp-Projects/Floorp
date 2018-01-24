/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Sandbox.h"

#include "mozilla/Preferences.h"
#include "mozilla/SandboxSettings.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h" // for FILE_REMOTE_TYPE

namespace mozilla {

/* static */ ContentProcessSandboxParams
ContentProcessSandboxParams::ForThisProcess(const dom::MaybeFileDesc& aBroker)
{
  ContentProcessSandboxParams params;
  params.mLevel = GetEffectiveContentSandboxLevel();

  if (aBroker.type() == dom::MaybeFileDesc::TFileDescriptor) {
    auto fd = aBroker.get_FileDescriptor().ClonePlatformHandle();
    params.mBrokerFd = fd.release();
    // brokerFd < 0 means to allow direct filesystem access, so
    // make absolutely sure that doesn't happen if the parent
    // didn't intend it.
    MOZ_RELEASE_ASSERT(params.mBrokerFd >= 0);
  }
  // (Otherwise, mBrokerFd will remain -1 from the default ctor.)

  auto* cc = dom::ContentChild::GetSingleton();
  params.mFileProcess = cc->GetRemoteType().EqualsLiteral(FILE_REMOTE_TYPE);

  nsAutoCString extraSyscalls;
  nsresult rv =
    Preferences::GetCString("security.sandbox.content.syscall_whitelist",
                            extraSyscalls);
  if (NS_SUCCEEDED(rv)) {
    for (const nsACString& callNrString : extraSyscalls.Split(',')) {
      int callNr = PromiseFlatCString(callNrString).ToInteger(&rv);
      if (NS_SUCCEEDED(rv)) {
        params.mSyscallWhitelist.push_back(callNr);
      }
    }
  }

  return params;
}

} // namespace mozilla
