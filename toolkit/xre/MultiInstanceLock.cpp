/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MultiInstanceLock.h"

#include "commonupdatedir.h"  // for GetInstallHash
#include "mozilla/UniquePtr.h"
#include "nsPrintfCString.h"
#include "nsPromiseFlatString.h"
#include "updatedefines.h"  // for NS_t* definitions

#ifdef XP_WIN
#  include <shlwapi.h>
#else
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#endif

#ifdef XP_WIN
#  include "WinUtils.h"
#endif

#ifdef MOZ_WIDGET_COCOA
#  include "nsILocalFileMac.h"
#endif

namespace mozilla {

static bool GetLockFileName(const char* nameToken, const char16_t* installPath,
                            nsCString& filePath) {
#ifdef XP_WIN
  // On Windows, the lock file is placed at the path
  // [updateDirectory]\[nameToken]-[pathHash], so first we need to get the
  // update directory path and then append the file name.

  // Note: This will return something like
  //   C:\ProgramData\Mozilla-1de4eec8-1241-4177-a864-e594e8d1fb38\updates\<hash>
  // But we actually are going to want to return the root update directory,
  // the grandparent of this directory, which will look something like this:
  //   C:\ProgramData\Mozilla-1de4eec8-1241-4177-a864-e594e8d1fb38
  mozilla::UniquePtr<wchar_t[]> updateDir;
  HRESULT hr = GetCommonUpdateDirectory(
      reinterpret_cast<const wchar_t*>(installPath), updateDir);
  if (FAILED(hr)) {
    return false;
  }

  // For the path manipulation that we are about to do, it is important that
  // the update directory have no trailing slash.
  size_t len = wcslen(updateDir.get());
  if (len == 0) {
    return false;
  }
  if (updateDir.get()[len - 1] == '/' || updateDir.get()[len - 1] == '\\') {
    updateDir.get()[len - 1] = '\0';
  }

  wchar_t* hashPtr = PathFindFileNameW(updateDir.get());
  // PathFindFileNameW returns a pointer to the beginning of the string on
  // failure.
  if (hashPtr == updateDir.get()) {
    return false;
  }

  // We need to make a copy of the hash before we modify updateDir to get the
  // root update dir.
  size_t hashSize = wcslen(hashPtr) + 1;
  mozilla::UniquePtr<wchar_t[]> hash = mozilla::MakeUnique<wchar_t[]>(hashSize);
  errno_t error = wcscpy_s(hash.get(), hashSize, hashPtr);
  if (error != 0) {
    return false;
  }

  // Get the root update dir from the update dir.
  BOOL success = PathRemoveFileSpecW(updateDir.get());
  if (!success) {
    return false;
  }
  success = PathRemoveFileSpecW(updateDir.get());
  if (!success) {
    return false;
  }

  filePath =
      nsPrintfCString("%s\\%s-%s", NS_ConvertUTF16toUTF8(updateDir.get()).get(),
                      nameToken, NS_ConvertUTF16toUTF8(hash.get()).get());

#else
  mozilla::UniquePtr<NS_tchar[]> pathHash;
  if (!GetInstallHash(installPath, pathHash)) {
    return false;
  }

  // On POSIX platforms the base path is /tmp/[vendor][nameToken]-[pathHash].
  filePath = nsPrintfCString("/tmp/%s%s-%s", MOZ_APP_VENDOR, nameToken,
                             pathHash.get());

#endif

  return true;
}

MultiInstLockHandle OpenMultiInstanceLock(const char* nameToken,
                                          const char16_t* installPath) {
  nsCString filePath;
  if (!GetLockFileName(nameToken, installPath, filePath)) {
    return MULTI_INSTANCE_LOCK_HANDLE_ERROR;
  }

  // Open a file handle with full privileges and sharing, and then attempt to
  // take a shared (nonexclusive, read-only) lock on it.
#ifdef XP_WIN
  HANDLE h =
      ::CreateFileW(PromiseFlatString(NS_ConvertUTF8toUTF16(filePath)).get(),
                    GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_ALWAYS, 0, nullptr);
  if (h != INVALID_HANDLE_VALUE) {
    // The LockFileEx functions always require an OVERLAPPED structure even
    // though we did not open the lock file for overlapped I/O.
    OVERLAPPED o = {0};
    if (!::LockFileEx(h, LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &o)) {
      CloseHandle(h);
      h = INVALID_HANDLE_VALUE;
    }
  }
  return h;

#else
  int fd = ::open(PromiseFlatCString(filePath).get(),
                  O_CLOEXEC | O_CREAT | O_NOFOLLOW,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  if (fd != -1) {
    // We would like to ensure that the lock file is deleted when we are done
    // with it. The normal way to do that would be to call unlink on it right
    // now, but that would immediately delete the name from the file system, and
    // we need other instances to be able to open that name and get the same
    // inode, so we can't unlink the file before we're done with it. This means
    // we accept some unreliability in getting the file deleted, but it's a zero
    // byte file in the tmp directory, so having it stay around isn't the worst.
    struct flock l = {0};
    l.l_start = 0;
    l.l_len = 0;
    l.l_type = F_RDLCK;
    if (::fcntl(fd, F_SETLK, &l)) {
      ::close(fd);
      fd = -1;
    }
  }
  return fd;

#endif
}

void ReleaseMultiInstanceLock(MultiInstLockHandle lock) {
  if (lock != MULTI_INSTANCE_LOCK_HANDLE_ERROR) {
#ifdef XP_WIN
    OVERLAPPED o = {0};
    ::UnlockFileEx(lock, 0, 1, 0, &o);
    ::CloseHandle(lock);

#else
    // If we're the last instance, then unlink the lock file. There is a race
    // condition here that may cause an instance to fail to open the same inode
    // as another even though they use the same path, but there's no reasonable
    // way to avoid that without skipping deleting the file at all, so we accept
    // that risk.
    bool otherInstance = true;
    if (IsOtherInstanceRunning(lock, &otherInstance) && !otherInstance) {
      // Recover the file's path so we can unlink it.
      // There's no error checking in here because we're content to let the file
      // hang around if any of this fails (which can happen if for example we're
      // on a system where /proc/self/fd does not exist); this is a zero-byte
      // file in the tmp directory after all.
      UniquePtr<NS_tchar[]> linkPath = MakeUnique<NS_tchar[]>(MAXPATHLEN + 1);
      NS_tsnprintf(linkPath.get(), MAXPATHLEN + 1, "/proc/self/fd/%d", lock);
      UniquePtr<NS_tchar[]> lockFilePath =
          MakeUnique<NS_tchar[]>(MAXPATHLEN + 1);
      if (::readlink(linkPath.get(), lockFilePath.get(), MAXPATHLEN + 1) !=
          -1) {
        ::unlink(lockFilePath.get());
      }
    }
    // Now close the lock file, which will release the lock.
    ::close(lock);
#endif
  }
}

bool IsOtherInstanceRunning(MultiInstLockHandle lock, bool* aResult) {
  // Every running instance has opened a readonly lock, and read locks prevent
  // write locks from being opened, so to see if we are the only instance, we
  // attempt to take a write lock, and if it succeeds then that must mean there
  // are no other read locks open and therefore no other instances.
  if (lock == MULTI_INSTANCE_LOCK_HANDLE_ERROR) {
    return false;
  }

#ifdef XP_WIN
  // We need to release the lock we're holding before we would be allowed to
  // take an exclusive lock, and if that succeeds we need to release it too
  // in order to get our shared lock back. This procedure is not atomic, so we
  // accept the risk of the scheduler deciding to ruin our day between these
  // operations; we'd get a false negative in a different instance's check.
  OVERLAPPED o = {0};
  // Release our current shared lock.
  if (!::UnlockFileEx(lock, 0, 1, 0, &o)) {
    return false;
  }
  // Attempt to take an exclusive lock.
  bool rv = false;
  if (::LockFileEx(lock, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0,
                   1, 0, &o)) {
    // We got the exclusive lock, so now release it.
    ::UnlockFileEx(lock, 0, 1, 0, &o);
    *aResult = false;
    rv = true;
  } else if (::GetLastError() == ERROR_LOCK_VIOLATION) {
    // We didn't get the exclusive lock because of outstanding shared locks.
    *aResult = true;
    rv = true;
  }
  // Attempt to reclaim the shared lock we released at the beginning.
  if (!::LockFileEx(lock, LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &o)) {
    rv = false;
  }
  return rv;

#else
  // See if we would be allowed to set a write lock (no need to actually do so).
  struct flock l = {0};
  l.l_start = 0;
  l.l_len = 0;
  l.l_type = F_WRLCK;
  if (::fcntl(lock, F_GETLK, &l)) {
    return false;
  }
  *aResult = l.l_type != F_UNLCK;
  return true;

#endif
}

already_AddRefed<nsIFile> GetNormalizedAppFile(nsIFile* aAppFile) {
  // If we're given an app file, use it; otherwise, get it from the ambient
  // directory service.
  nsresult rv;
  nsCOMPtr<nsIFile> appFile;
  if (aAppFile) {
    rv = aAppFile->Clone(getter_AddRefs(appFile));
    NS_ENSURE_SUCCESS(rv, nullptr);
  } else {
    nsCOMPtr<nsIProperties> dirSvc =
        do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
    NS_ENSURE_TRUE(dirSvc, nullptr);

    rv = dirSvc->Get(XRE_EXECUTABLE_FILE, NS_GET_IID(nsIFile),
                     getter_AddRefs(appFile));
    NS_ENSURE_SUCCESS(rv, nullptr);
  }

  // It is possible that the path we have is on a case insensitive
  // filesystem in which case the path may vary depending on how the
  // application is called. We want to normalize the case somehow.
  // On Linux XRE_EXECUTABLE_FILE already seems to be set to the correct path.
  //
  // See similar nsXREDirProvider::GetInstallHash. The main difference here is
  // to allow lookup to fail on OSX, because some tests use a nonexistent
  // appFile.
#ifdef XP_WIN
  // Windows provides a way to get the correct case.
  if (!mozilla::widget::WinUtils::ResolveJunctionPointsAndSymLinks(appFile)) {
    NS_WARNING("Failed to resolve install directory.");
  }
#elif defined(MOZ_WIDGET_COCOA)
  // On OSX roundtripping through an FSRef fixes the case.
  FSRef ref;
  nsCOMPtr<nsILocalFileMac> macFile = do_QueryInterface(appFile);
  if (macFile && NS_SUCCEEDED(macFile->GetFSRef(&ref)) &&
      NS_SUCCEEDED(
          NS_NewLocalFileWithFSRef(&ref, true, getter_AddRefs(macFile)))) {
    appFile = static_cast<nsIFile*>(macFile);
  } else {
    NS_WARNING("Failed to resolve install directory.");
  }
#endif

  return appFile.forget();
}

};  // namespace mozilla
