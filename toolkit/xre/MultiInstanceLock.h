/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MULTIINSTANCELOCK_H
#define MULTIINSTANCELOCK_H

#ifdef XP_WIN
#  include <windows.h>
#endif

// These functions manage "multi-instance locks", which are a type of lock
// specifically designed to allow instances of an application, process, or other
// task to detect when other instances relevant to them are running. Each
// instance opens a lock and holds it for the duration of the task of interest
// (which may be the lifetime of the process, or a shorter period). Then while
// the lock is open, it can be used to check whether any other instances of the
// same task are currently running out of the same copy of the binary, in the
// context of any OS user. A process can open any number of locks, so long as
// they use different names. It is necessary for the process to have permission
// to create files in /tmp/ on POSIX systems or ProgramData\[vendor]\ on
// Windows, so this mechanism may not work for sandboxed processes.

// The implementation is based on file locking. An empty file is created in a
// systemwide (not per-user) location, and a shared (read) lock is taken on that
// file; the value that OpenMultiInstanceLock() returns is the file
// handle/descriptor. When you call IsOtherInstanceRunning(), it will attempt to
// convert that shared lock into an exclusive (write) lock. If that operation
// would succeed, it means that there must not be any other shared locks
// currently taken on that file, so we know there are no other instances
// running. This is a more complex design than most file locks or most other
// concurrency mechanisms, but it is necessary for this use case because of the
// requirement that an instance must be able to detect other instances that were
// started later than it was. If, say, a mutex were used, or another kind of
// exclusive lock, then the first instance that tried to take it would succeed,
// and be unable to tell that another instance had tried to take it later and
// failed. This mechanism allows any number of instances started at any time in
// relation to one another to always be able to detect that the others exist
// (although it does not allow you to know how many others exist). The lock is
// guaranteed to be released if the process holding it crashes or is exec'd into
// something else, because the file is closed when that happens. The file itself
// is not necessarily always deleted on POSIX, because it isn't possible (within
// reason) to guarantee that unlink() is called, but the file is empty and
// created in the /tmp directory, so should not be a serious problem.

namespace mozilla {

#ifdef XP_WIN
using MultiInstLockHandle = HANDLE;
#  define MULTI_INSTANCE_LOCK_HANDLE_ERROR INVALID_HANDLE_VALUE
#else
using MultiInstLockHandle = int;
#  define MULTI_INSTANCE_LOCK_HANDLE_ERROR -1
#endif

/*
 * nameToken should be a string very briefly naming the lock you are creating
 * creating, and it should be unique except for across multiple instances of the
 * same application. The vendor name is included in the generated path, so it
 * doesn't need to be present in your supplied name. Try to keep this name sort
 * of short, ideally under about 64 characters, because creating the lock will
 * fail if the final path string (the token + the path hash + the vendor name)
 * is longer than the platform's maximum path and/or path component length.
 *
 * installPath should be the path to the directory containing the application,
 * which will be used to form a path specific to that installation.
 *
 * Returns MULTI_INSTANCE_LOCK_HANDLE_ERROR upon failure, or a handle which can
 * later be passed to the other functions declared here upon success.
 */
MultiInstLockHandle OpenMultiInstanceLock(const char* nameToken,
                                          const char16_t* installPath);

void ReleaseMultiInstanceLock(MultiInstLockHandle lock);

// aResult will be set to true if another instance *was* found, false if not.
// Return value is true on success, false on error (and aResult won't be set).
bool IsOtherInstanceRunning(MultiInstLockHandle lock, bool* aResult);

// It is possible that the path we have is on a case insensitive
// filesystem in which case the path may vary depending on how the
// application is called. We want to normalize the case somehow.
// When aAppFile is NULL, this function returns a nsIFile with a normalized
// path for the currently running binary. When aAppFile is not null,
// this function ensures the file path is properly normalized.
already_AddRefed<nsIFile> GetNormalizedAppFile(nsIFile* aAppFile);

};  // namespace mozilla

#endif  // MULTIINSTANCELOCK_H
