/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SocketProcessImpl.h"

#include "base/command_line.h"
#include "base/shared_memory.h"
#include "base/string_util.h"
#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/Preferences.h"

#if defined(OS_WIN) && defined(MOZ_SANDBOX)
#  include "mozilla/sandboxTarget.h"
#endif

using mozilla::ipc::IOThreadChild;

namespace mozilla {
namespace net {

LazyLogModule gSocketProcessLog("socketprocess");

SocketProcessImpl::SocketProcessImpl(ProcessId aParentPid)
    : ProcessChild(aParentPid) {}

SocketProcessImpl::~SocketProcessImpl() = default;

bool SocketProcessImpl::Init(int aArgc, char* aArgv[]) {
#ifdef OS_POSIX
  if (PR_GetEnv("MOZ_DEBUG_SOCKET_PROCESS")) {
    printf_stderr("\n\nSOCKETPROCESSnSOCKETPROCESS\n  debug me @ %d\n\n",
                  base::GetCurrentProcId());
    sleep(30);
  }
#endif
#if defined(MOZ_SANDBOX) && defined(OS_WIN)
  LoadLibraryW(L"nss3.dll");
  LoadLibraryW(L"softokn3.dll");
  LoadLibraryW(L"freebl3.dll");
  mozilla::SandboxTarget::Instance()->StartSandbox();
#endif
  char* parentBuildID = nullptr;
  char* prefsHandle = nullptr;
  char* prefMapHandle = nullptr;
  char* prefsLen = nullptr;
  char* prefMapSize = nullptr;

  for (int i = 1; i < aArgc; i++) {
    if (!aArgv[i]) {
      continue;
    }

    if (strcmp(aArgv[i], "-parentBuildID") == 0) {
      if (++i == aArgc) {
        return false;
      }

      parentBuildID = aArgv[i];

#ifdef XP_WIN
    } else if (strcmp(aArgv[i], "-prefsHandle") == 0) {
      if (++i == aArgc) {
        return false;
      }
      prefsHandle = aArgv[i];
    } else if (strcmp(aArgv[i], "-prefMapHandle") == 0) {
      if (++i == aArgc) {
        return false;
      }
      prefMapHandle = aArgv[i];
#endif
    } else if (strcmp(aArgv[i], "-prefsLen") == 0) {
      if (++i == aArgc) {
        return false;
      }
      prefsLen = aArgv[i];
    } else if (strcmp(aArgv[i], "-prefMapSize") == 0) {
      if (++i == aArgc) {
        return false;
      }
      prefMapSize = aArgv[i];
    }
  }

  SharedPreferenceDeserializer deserializer;
  if (!deserializer.DeserializeFromSharedMemory(prefsHandle, prefMapHandle,
                                                prefsLen, prefMapSize)) {
    return false;
  }

  return mSocketProcessChild.Init(ParentPid(), parentBuildID,
                                  IOThreadChild::message_loop(),
                                  IOThreadChild::TakeChannel());
}

void SocketProcessImpl::CleanUp() { mSocketProcessChild.CleanUp(); }

}  // namespace net
}  // namespace mozilla
