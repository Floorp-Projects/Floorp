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

using mozilla::ipc::IOThreadChild;

namespace mozilla {
namespace net {

LazyLogModule gSocketProcessLog("socketprocess");

SocketProcessImpl::SocketProcessImpl(ProcessId aParentPid)
    : ProcessChild(aParentPid) {}

SocketProcessImpl::~SocketProcessImpl() {}

bool SocketProcessImpl::Init(int aArgc, char* aArgv[]) {
#ifdef OS_POSIX
  if (PR_GetEnv("MOZ_DEBUG_SOCKET_PROCESS")) {
    printf_stderr("\n\nSOCKETPROCESSnSOCKETPROCESS\n  debug me @ %d\n\n",
                  base::GetCurrentProcId());
    sleep(30);
  }
#endif
  // TODO: reduce duplicate code about preference below. See bug 1484774.
  Maybe<base::SharedMemoryHandle> prefsHandle;
  Maybe<FileDescriptor> prefMapHandle;
  Maybe<size_t> prefsLen;
  Maybe<size_t> prefMapSize;
  char* parentBuildID = nullptr;

  // Parses an arg containing a pointer-sized-integer.
  auto parseUIntPtrArg = [](char*& aArg) {
    // ContentParent uses %zu to print a word-sized unsigned integer. So
    // even though strtoull() returns a long long int, it will fit in a
    // uintptr_t.
    return uintptr_t(strtoull(aArg, &aArg, 10));
  };

#ifdef XP_WIN
  auto parseHandleArg = [&](char*& aArg) {
    return HANDLE(parseUIntPtrArg(aArg));
  };
#endif

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
      char* str = aArgv[i];
      prefsHandle = Some(parseHandleArg(str));
      if (str[0] != '\0') {
        return false;
      }

    } else if (strcmp(aArgv[i], "-prefMapHandle") == 0) {
      if (++i == aArgc) {
        return false;
      }
      char* str = aArgv[i];
      // The FileDescriptor constructor will clone this handle when constructed,
      // so store it in a UniquePlatformHandle to make sure the original gets
      // closed.
      FileDescriptor::UniquePlatformHandle handle(parseHandleArg(str));
      prefMapHandle.emplace(handle.get());
      if (str[0] != '\0') {
        return false;
      }
#endif
    } else if (strcmp(aArgv[i], "-prefsLen") == 0) {
      if (++i == aArgc) {
        return false;
      }
      char* str = aArgv[i];
      prefsLen = Some(parseUIntPtrArg(str));
      if (str[0] != '\0') {
        return false;
      }

    } else if (strcmp(aArgv[i], "-prefMapSize") == 0) {
      if (++i == aArgc) {
        return false;
      }
      char* str = aArgv[i];
      prefMapSize = Some(parseUIntPtrArg(str));
      if (str[0] != '\0') {
        return false;
      }
    }
  }

#ifdef XP_UNIX
  prefsHandle = Some(base::FileDescriptor(kPrefsFileDescriptor,
                                          /* auto_close */ true));

  // The FileDescriptor constructor will clone this handle when constructed,
  // so store it in a UniquePlatformHandle to make sure the original gets
  // closed.
  FileDescriptor::UniquePlatformHandle handle(kPrefMapFileDescriptor);
  prefMapHandle.emplace(handle.get());
#endif

  // Init the shared-memory base preference mapping first, so that only changed
  // preferences wind up in heap memory.
  Preferences::InitSnapshot(prefMapHandle.ref(), *prefMapSize);

  // Set up early prefs from the shared memory.
  base::SharedMemory shm;
  if (!shm.SetHandle(*prefsHandle, /* read_only */ true)) {
    NS_ERROR("failed to open shared memory in the child");
    return false;
  }
  if (!shm.Map(*prefsLen)) {
    NS_ERROR("failed to map shared memory in the child");
    return false;
  }
  Preferences::DeserializePreferences(static_cast<char*>(shm.memory()),
                                      *prefsLen);

  if (NS_FAILED(NS_InitXPCOM2(nullptr, nullptr, nullptr))) {
    return false;
  }

  return mSocketProcessChild.Init(ParentPid(), parentBuildID,
                                  IOThreadChild::message_loop(),
                                  IOThreadChild::channel());
}

void SocketProcessImpl::CleanUp() { mSocketProcessChild.CleanUp(); }

}  // namespace net
}  // namespace mozilla
