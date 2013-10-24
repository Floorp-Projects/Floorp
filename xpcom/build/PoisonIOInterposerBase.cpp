/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Mutex.h"
#include "mozilla/Scoped.h"

#include <algorithm>
#include <vector>

#include "PoisonIOInterposer.h"

// Auxiliary method to convert file descriptors to ids
#if defined(XP_WIN32)
#include <io.h>
inline intptr_t FileDescriptorToID(int aFd) {
  return _get_osfhandle(aFd);
}
#else
inline intptr_t FileDescriptorToID(int aFd) {
  return aFd;
}
#endif /* if not XP_WIN32 */

using namespace mozilla;

namespace {
struct DebugFilesAutoLockTraits {
  typedef PRLock *type;
  const static type empty() {
    return nullptr;
  }
  const static void release(type aL) {
    PR_Unlock(aL);
  }
};

class DebugFilesAutoLock : public Scoped<DebugFilesAutoLockTraits> {
  static PRLock *Lock;
public:
  static void Clear();
  static PRLock *getDebugFileIDsLock() {
    // On windows this static is not thread safe, but we know that the first
    // call is from
    // * An early registration of a debug FD or
    // * The call to InitWritePoisoning.
    // Since the early debug FDs are logs created early in the main thread
    // and no writes are trapped before InitWritePoisoning, we are safe.
    if (!Lock) {
      Lock = PR_NewLock();
    }

    // We have to use something lower level than a mutex. If we don't, we
    // can get recursive in here when called from logging a call to free.
    return Lock;
  }

  DebugFilesAutoLock() :
    Scoped<DebugFilesAutoLockTraits>(getDebugFileIDsLock()) {
    PR_Lock(get());
  }
};

PRLock *DebugFilesAutoLock::Lock;
void DebugFilesAutoLock::Clear() {
  MOZ_ASSERT(Lock != nullptr);
  Lock = nullptr;
}

// Return a vector used to hold the IDs of the current debug files. On unix
// an ID is a file descriptor. On Windows it is a file HANDLE.
std::vector<intptr_t>* getDebugFileIDs() {
  PR_ASSERT_CURRENT_THREAD_OWNS_LOCK(DebugFilesAutoLock::getDebugFileIDsLock());
  // We have to use new as some write happen during static destructors
  // so an static std::vector might be destroyed while we still need it.
  static std::vector<intptr_t> *DebugFileIDs = new std::vector<intptr_t>();
  return DebugFileIDs;
}

} // anonymous namespace

namespace mozilla{

// Auxiliary Method to test if a file descriptor is registered to be ignored
// by the poisoning IO interposer
bool IsDebugFile(intptr_t aFileID) {
  DebugFilesAutoLock lockedScope;

  std::vector<intptr_t> &Vec = *getDebugFileIDs();
  return std::find(Vec.begin(), Vec.end(), aFileID) != Vec.end();
}

// Clean-up for the registered debug files.
// We should probably make sure all debug files are unregistered instead.
// But as the poison IO interposer is used for late-write checks we're not
// disabling it at any point yet. So Really no need for this.
//
// void ClearDebugFileRegister() {
//   PRLock *Lock;
//   {
//     DebugFilesAutoLock lockedScope;
//     delete getDebugFileIDs();
//     Lock = DebugFilesAutoLock::getDebugFileIDsLock();
//     DebugFilesAutoLock::Clear();
//   }
//   PR_DestroyLock(Lock);
// }

} // namespace mozilla

extern "C" {

  void MozillaRegisterDebugFD(int fd) {
    intptr_t fileId = FileDescriptorToID(fd);
    DebugFilesAutoLock lockedScope;
    std::vector<intptr_t> &Vec = *getDebugFileIDs();
    MOZ_ASSERT(std::find(Vec.begin(), Vec.end(), fileId) == Vec.end());
    Vec.push_back(fileId);
  }

  void MozillaRegisterDebugFILE(FILE *f) {
    int fd = fileno(f);
    if (fd == 1 || fd == 2) {
      return;
    }
    MozillaRegisterDebugFD(fd);
  }

  void MozillaUnRegisterDebugFD(int fd) {
    DebugFilesAutoLock lockedScope;
    intptr_t fileId = FileDescriptorToID(fd);
    std::vector<intptr_t> &Vec = *getDebugFileIDs();
    std::vector<intptr_t>::iterator i =
      std::find(Vec.begin(), Vec.end(), fileId);
    MOZ_ASSERT(i != Vec.end());
    Vec.erase(i);
  }

  void MozillaUnRegisterDebugFILE(FILE *f) {
    int fd = fileno(f);
    if (fd == 1 || fd == 2) {
      return;
    }
    fflush(f);
    MozillaUnRegisterDebugFD(fd);
  }

}