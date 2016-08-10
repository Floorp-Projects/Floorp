/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxBroker.h"
#include "SandboxInfo.h"
#include "SandboxLogging.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef XP_LINUX
#include <sys/prctl.h>
#endif

#ifdef MOZ_WIDGET_GONK
#include <private/android_filesystem_config.h>
#include <sys/syscall.h>
#endif

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Move.h"
#include "mozilla/NullPtr.h"
#include "mozilla/ipc/FileDescriptor.h"

namespace mozilla {

// This constructor signals failure by setting mFileDesc and aClientFd to -1.
SandboxBroker::SandboxBroker(UniquePtr<const Policy> aPolicy, int aChildPid,
                             int& aClientFd)
  : mChildPid(aChildPid), mPolicy(Move(aPolicy))
{
  int fds[2];
  if (0 != socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds)) {
    SANDBOX_LOG_ERROR("SandboxBroker: socketpair failed: %s", strerror(errno));
    mFileDesc = -1;
    aClientFd = -1;
    return;
  }
  mFileDesc = fds[0];
  aClientFd = fds[1];

  if (!PlatformThread::Create(0, this, &mThread)) {
    SANDBOX_LOG_ERROR("SandboxBroker: thread creation failed: %s",
                      strerror(errno));
    close(mFileDesc);
    close(aClientFd);
    mFileDesc = -1;
    aClientFd = -1;
  }
}

UniquePtr<SandboxBroker>
SandboxBroker::Create(UniquePtr<const Policy> aPolicy, int aChildPid,
                      ipc::FileDescriptor& aClientFdOut)
{
  int clientFd;
  // Can't use MakeUnique here because the constructor is private.
  UniquePtr<SandboxBroker> rv(new SandboxBroker(Move(aPolicy), aChildPid,
                                                clientFd));
  if (clientFd < 0) {
    rv = nullptr;
  } else {
    aClientFdOut = ipc::FileDescriptor(clientFd);
  }
  return Move(rv);
}

SandboxBroker::~SandboxBroker() {
  // If the constructor failed, there's nothing to be done here.
  if (mFileDesc < 0) {
    return;
  }

  shutdown(mFileDesc, SHUT_RD);
  // The thread will now get EOF even if the client hasn't exited.
  PlatformThread::Join(mThread);
  // Now that the thread has exited, the fd will no longer be accessed.
  close(mFileDesc);
  // Having ensured that this object outlives the thread, this
  // destructor can now return.
}

SandboxBroker::Policy::Policy() { }
SandboxBroker::Policy::~Policy() { }

SandboxBroker::Policy::Policy(const Policy& aOther) {
  for (auto iter = aOther.mMap.ConstIter(); !iter.Done(); iter.Next()) {
    mMap.Put(iter.Key(), iter.Data());
  }
}

// Chromium
// sandbox/linux/syscall_broker/broker_file_permission.cc
// Async signal safe
bool
SandboxBroker::Policy::ValidatePath(const char* path) const {
  if (!path)
    return false;

  const size_t len = strlen(path);
  // No empty paths
  if (len == 0)
    return false;
  // Paths must be absolute and not relative
  if (path[0] != '/')
    return false;
  // No trailing / (but "/" is valid)
  if (len > 1 && path[len - 1] == '/')
    return false;
  // No trailing /.
  if (len >= 2 && path[len - 2] == '/' && path[len - 1] == '.')
    return false;
  // No trailing /..
  if (len >= 3 && path[len - 3] == '/' && path[len - 2] == '.' &&
      path[len - 1] == '.')
    return false;
  // No /../ anywhere
  for (size_t i = 0; i < len; i++) {
    if (path[i] == '/' && (len - i) > 3) {
      if (path[i + 1] == '.' && path[i + 2] == '.' && path[i + 3] == '/') {
        return false;
      }
    }
  }
  return true;
}

void
SandboxBroker::Policy::AddPath(int aPerms, const char* aPath,
                               AddCondition aCond)
{
  nsDependentCString path(aPath);
  MOZ_ASSERT(path.Length() <= kMaxPathLen);
  int perms;
  if (aCond == AddIfExistsNow) {
    struct stat statBuf;
    if (lstat(aPath, &statBuf) != 0) {
      return;
    }
  }
  if (!mMap.Get(path, &perms)) {
    perms = MAY_ACCESS;
  } else {
    MOZ_ASSERT(perms & MAY_ACCESS);
  }
  if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
    SANDBOX_LOG_ERROR("policy for %s: %d -> %d", aPath, perms, perms | aPerms);
  }
  perms |= aPerms;
  mMap.Put(path, perms);
}

void
SandboxBroker::Policy::AddTree(int aPerms, const char* aPath)
{
  struct stat statBuf;

  if (stat(aPath, &statBuf) != 0) {
    return;
  }
  if (!S_ISDIR(statBuf.st_mode)) {
    AddPath(aPerms, aPath, AddAlways);
  } else {
    DIR* dirp = opendir(aPath);
    if (!dirp) {
      return;
    }
    while (struct dirent* de = readdir(dirp)) {
      if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
        continue;
      }
      // Note: could optimize the string handling.
      nsAutoCString subPath;
      subPath.Assign(aPath);
      subPath.Append('/');
      subPath.Append(de->d_name);
      AddTree(aPerms, subPath.get());
    }
    closedir(dirp);
  }
}

void
SandboxBroker::Policy::AddDir(int aPerms, const char* aPath)
{
  struct stat statBuf;

  if (stat(aPath, &statBuf) != 0) {
    return;
  }

  if (!S_ISDIR(statBuf.st_mode)) {
    return;
  }

  nsDependentCString path(aPath);
  MOZ_ASSERT(path.Length() <= kMaxPathLen - 1);
  // Enforce trailing / on aPath
  if (path[path.Length() - 1] != '/') {
    path.Append('/');
  }
  int origPerms;
  if (!mMap.Get(path, &origPerms)) {
    origPerms = MAY_ACCESS;
  } else {
    MOZ_ASSERT(origPerms & MAY_ACCESS);
  }
  int newPerms = origPerms | aPerms | RECURSIVE;
  if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
    SANDBOX_LOG_ERROR("policy for %s: %d -> %d", aPath, origPerms, newPerms);
  }
  mMap.Put(path, newPerms);
}

void
SandboxBroker::Policy::AddPrefix(int aPerms, const char* aDir,
                                 const char* aPrefix)
{
  size_t prefixLen = strlen(aPrefix);
  DIR* dirp = opendir(aDir);
  struct dirent* de;
  if (!dirp) {
    return;
  }
  while ((de = readdir(dirp))) {
    if (strncmp(de->d_name, aPrefix, prefixLen) == 0) {
      nsAutoCString subPath;
      subPath.Assign(aDir);
      subPath.Append('/');
      subPath.Append(de->d_name);
      AddPath(aPerms, subPath.get(), AddAlways);
    }
  }
  closedir(dirp);
}

int
SandboxBroker::Policy::Lookup(const nsACString& aPath) const
{
  // Early exit for paths explicitly found in the
  // whitelist.
  // This means they will not gain extra permissions
  // from recursive paths.
  int perms = mMap.Get(aPath);
  if (perms) {
    return perms;
  }

  // Not a legally constructed path
  if (!ValidatePath(PromiseFlatCString(aPath).get()))
    return 0;

  // Now it's either an illegal access, or a recursive
  // directory permission. We'll have to check the entire
  // whitelist for the best match (slower).
  int allPerms = 0;
  for (auto iter = mMap.ConstIter(); !iter.Done(); iter.Next()) {
    const nsACString& whiteListPath = iter.Key();
    const int& perms = iter.Data();

    if (!(perms & RECURSIVE))
      continue;

    // passed part starts with something on the whitelist
    if (StringBeginsWith(aPath, whiteListPath)) {
      allPerms |= perms;
    }
  }

  // Strip away the RECURSIVE flag as it doesn't
  // necessarily apply to aPath.
  return allPerms & ~RECURSIVE;
}

static bool
AllowAccess(int aReqFlags, int aPerms)
{
  if (aReqFlags & ~(R_OK|W_OK|F_OK)) {
    return false;
  }
  int needed = 0;
  if (aReqFlags & R_OK) {
    needed |= SandboxBroker::MAY_READ;
  }
  if (aReqFlags & W_OK) {
    needed |= SandboxBroker::MAY_WRITE;
  }
  return (aPerms & needed) == needed;
}

// These flags are added to all opens to prevent possible side-effects
// on this process.  These shouldn't be relevant to the child process
// in any case due to the sandboxing restrictions on it.  (See also
// the use of MSG_CMSG_CLOEXEC in SandboxBrokerCommon.cpp).
static const int kRequiredOpenFlags = O_CLOEXEC | O_NOCTTY;

// Linux originally assigned a flag bit to O_SYNC but implemented the
// semantics standardized as O_DSYNC; later, that bit was renamed and
// a new bit was assigned to the full O_SYNC, and O_SYNC was redefined
// to be both bits.  As a result, this #define is needed to compensate
// for outdated kernel headers like Android's.
#define O_SYNC_NEW 04010000
static const int kAllowedOpenFlags =
  O_APPEND | O_ASYNC | O_DIRECT | O_DIRECTORY | O_EXCL | O_LARGEFILE
  | O_NOATIME | O_NOCTTY | O_NOFOLLOW | O_NONBLOCK | O_NDELAY | O_SYNC_NEW
  | O_TRUNC | O_CLOEXEC | O_CREAT;
#undef O_SYNC_NEW

static bool
AllowOpen(int aReqFlags, int aPerms)
{
  if (aReqFlags & ~O_ACCMODE & ~kAllowedOpenFlags) {
    return false;
  }
  int needed;
  switch(aReqFlags & O_ACCMODE) {
  case O_RDONLY:
    needed = SandboxBroker::MAY_READ;
    break;
  case O_WRONLY:
    needed = SandboxBroker::MAY_WRITE;
    break;
  case O_RDWR:
    needed = SandboxBroker::MAY_READ | SandboxBroker::MAY_WRITE;
    break;
  default:
    return false;
  }
  if (aReqFlags & O_CREAT) {
    needed |= SandboxBroker::MAY_CREATE;
  }
  return (aPerms & needed) == needed;
}

static int
DoStat(const char* aPath, struct stat* aStat, int aFlags)
{
  if (aFlags & O_NOFOLLOW) {
    return lstat(aPath, aStat);
  }
  return stat(aPath, aStat);
}

void
SandboxBroker::ThreadMain(void)
{
  char threadName[16];
  snprintf(threadName, sizeof(threadName), "FS Broker %d", mChildPid);
  PlatformThread::SetName(threadName);

  // Permissive mode can only be enabled through an environment variable,
  // therefore it is sufficient to fetch the value once
  // before the main thread loop starts
  bool permissive = SandboxInfo::Get().Test(SandboxInfo::kPermissive);

#ifdef MOZ_WIDGET_GONK
#ifdef __NR_setreuid32
  static const long nr_setreuid = __NR_setreuid32;
  static const long nr_setregid = __NR_setregid32;
#else
  static const long nr_setreuid = __NR_setreuid;
  static const long nr_setregid = __NR_setregid;
#endif
  if (syscall(nr_setregid, getgid(), AID_APP + mChildPid) != 0 ||
      syscall(nr_setreuid, getuid(), AID_APP + mChildPid) != 0) {
    MOZ_CRASH("SandboxBroker: failed to drop privileges");
  }
#endif

  while (true) {
    struct iovec ios[2];
    char pathBuf[kMaxPathLen + 1];
    size_t pathLen;
    struct stat statBuf;
    Request req;
    Response resp;
    int respfd;

    ios[0].iov_base = &req;
    ios[0].iov_len = sizeof(req);
    ios[1].iov_base = pathBuf;
    ios[1].iov_len = kMaxPathLen;

    const ssize_t recvd = RecvWithFd(mFileDesc, ios, 2, &respfd);
    if (recvd == 0) {
      if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
        SANDBOX_LOG_ERROR("EOF from pid %d", mChildPid);
      }
      break;
    }
    // It could be possible to continue after errors and short reads,
    // at least in some cases, but protocol violation indicates a
    // hostile client, so terminate the broker instead.
    if (recvd < 0) {
      SANDBOX_LOG_ERROR("bad read from pid %d: %s",
                        mChildPid, strerror(errno));
      shutdown(mFileDesc, SHUT_RD);
      break;
    }
    if (recvd < static_cast<ssize_t>(sizeof(req))) {
      SANDBOX_LOG_ERROR("bad read from pid %d (%d < %d)",
                        mChildPid, recvd, sizeof(req));
      shutdown(mFileDesc, SHUT_RD);
      break;
    }
    if (respfd == -1) {
      SANDBOX_LOG_ERROR("no response fd from pid %d", mChildPid);
      shutdown(mFileDesc, SHUT_RD);
      break;
    }

    // Initialize the response with the default failure.
    memset(&resp, 0, sizeof(resp));
    memset(&statBuf, 0, sizeof(statBuf));
    resp.mError = EACCES;
    ios[0].iov_base = &resp;
    ios[0].iov_len = sizeof(resp);
    ios[1].iov_base = nullptr;
    ios[1].iov_len = 0;
    int openedFd = -1;

    // Look up the pathname.
    pathLen = recvd - sizeof(req);
    // It shouldn't be possible for recvmsg to violate this assertion,
    // but one more predictable branch shouldn't have much perf impact:
    MOZ_RELEASE_ASSERT(pathLen <= kMaxPathLen);
    pathBuf[pathLen] = '\0';
    int perms = 0;
    if (!memchr(pathBuf, '\0', pathLen)) {
      perms = mPolicy->Lookup(nsDependentCString(pathBuf, pathLen));
    }

    // And now perform the operation if allowed.
    if (perms & CRASH_INSTEAD) {
      // This is somewhat nonmodular, but it works.
      resp.mError = ENOSYS;
    } else if (permissive || perms & MAY_ACCESS) {
      // If the operation was only allowed because of permissive mode, log it.
      if (permissive && !(perms & MAY_ACCESS)) {
        AuditDenial(req.mOp, req.mFlags, pathBuf);
      }

      switch(req.mOp) {
      case SANDBOX_FILE_OPEN:
        if (permissive || AllowOpen(req.mFlags, perms)) {
          // Permissions for O_CREAT hardwired to 0600; if that's
          // ever a problem we can change the protocol (but really we
          // should be trying to remove uses of MAY_CREATE, not add
          // new ones).
          openedFd = open(pathBuf, req.mFlags | kRequiredOpenFlags, 0600);
          if (openedFd >= 0) {
            resp.mError = 0;
          } else {
            resp.mError = errno;
          }
        }
        break;

      case SANDBOX_FILE_ACCESS:
        if (permissive || AllowAccess(req.mFlags, perms)) {
          // This can't use access() itself because that uses the ruid
          // and not the euid.  In theory faccessat() with AT_EACCESS
          // would work, but Linux doesn't actually implement the
          // flags != 0 case; glibc has a hack which doesn't even work
          // in this case so it'll ignore the flag, and Bionic just
          // passes through the syscall and always ignores the flags.
          //
          // Instead, because we've already checked the requested
          // r/w/x bits against the policy, just return success if the
          // file exists and hope that's close enough.
          if (stat(pathBuf, &statBuf) == 0) {
            resp.mError = 0;
          } else {
            resp.mError = errno;
          }
        }
        break;

      case SANDBOX_FILE_STAT:
        if (DoStat(pathBuf, &statBuf, req.mFlags) == 0) {
          resp.mError = 0;
          ios[1].iov_base = &statBuf;
          ios[1].iov_len = sizeof(statBuf);
        } else {
          resp.mError = errno;
        }
        break;
      }
    } else {
      MOZ_ASSERT(perms == 0);
    }

    const size_t numIO = ios[1].iov_len > 0 ? 2 : 1;
    DebugOnly<const ssize_t> sent = SendWithFd(respfd, ios, numIO, openedFd);
    close(respfd);
    MOZ_ASSERT(sent < 0 ||
               static_cast<size_t>(sent) == ios[0].iov_len + ios[1].iov_len);

    if (openedFd >= 0) {
      close(openedFd);
    }
  }
}

void
SandboxBroker::AuditDenial(int aOp, int aFlags, const char* aPath)
{
  MOZ_RELEASE_ASSERT(SandboxInfo::Get().Test(SandboxInfo::kPermissive));

  struct stat statBuf;

  if (lstat(aPath, &statBuf) == 0) {
    // Path exists, set errno to 0 to indicate "success".
    errno = 0;
  }

  SANDBOX_LOG_ERROR("SandboxBroker: denied op=%d rflags=%o path=%s for pid=%d" \
                    " permissive=1 error=\"%s\"", aOp, aFlags, aPath, mChildPid,
                    strerror(errno));
}

} // namespace mozilla
