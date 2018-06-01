/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxBroker.h"
#include "SandboxInfo.h"
#include "SandboxLogging.h"
#include "SandboxBrokerUtils.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#ifdef XP_LINUX
#include <sys/prctl.h>
#endif

#include "base/string_util.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Move.h"
#include "mozilla/NullPtr.h"
#include "mozilla/Sprintf.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "SpecialSystemDirectory.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

namespace mozilla {

// Default/fallback temporary directory
static const nsLiteralCString tempDirPrefix("/tmp");

// This constructor signals failure by setting mFileDesc and aClientFd to -1.
SandboxBroker::SandboxBroker(UniquePtr<const Policy> aPolicy, int aChildPid,
                             int& aClientFd)
  : mChildPid(aChildPid), mPolicy(std::move(aPolicy))
{
  int fds[2];
  if (0 != socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fds)) {
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
  UniquePtr<SandboxBroker> rv(new SandboxBroker(std::move(aPolicy), aChildPid,
                                                clientFd));
  if (clientFd < 0) {
    rv = nullptr;
  } else {
    aClientFdOut = ipc::FileDescriptor(clientFd);
  }
  return std::move(rv);
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

SandboxBroker::Policy::Policy() = default;
SandboxBroker::Policy::~Policy() = default;

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

  // Add a Prefix permission on things inside the dir.
  nsDependentCString path(aPath);
  MOZ_ASSERT(path.Length() <= kMaxPathLen - 1);
  // Enforce trailing / on aPath
  if (path.Last() != '/') {
    path.Append('/');
  }
  Policy::AddPrefixInternal(aPerms, path);

  // Add a path permission on the dir itself so it can
  // be opened. We're guaranteed to have a trailing / now,
  // so just cut that.
  path.Truncate(path.Length() - 1);
  if (!path.IsEmpty()) {
    Policy::AddPath(aPerms, path.get(), AddAlways);
  }
}

void
SandboxBroker::Policy::AddPrefix(int aPerms, const char* aPath)
{
  Policy::AddPrefixInternal(aPerms, nsDependentCString(aPath));
}

void
SandboxBroker::Policy::AddPrefixInternal(int aPerms, const nsACString& aPath)
{
  int origPerms;
  if (!mMap.Get(aPath, &origPerms)) {
    origPerms = MAY_ACCESS;
  } else {
    MOZ_ASSERT(origPerms & MAY_ACCESS);
  }
  int newPerms = origPerms | aPerms | RECURSIVE;
  if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
    SANDBOX_LOG_ERROR("policy for %s: %d -> %d", PromiseFlatCString(aPath).get(),
                      origPerms, newPerms);
  }
  mMap.Put(aPath, newPerms);
}

void
SandboxBroker::Policy::AddFilePrefix(int aPerms, const char* aDir,
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

void
SandboxBroker::Policy::AddDynamic(int aPerms, const char* aPath)
{
  struct stat statBuf;
  bool exists = (stat(aPath, &statBuf) == 0);

  if (!exists) {
    AddPrefix(aPerms, aPath);
  } else {
    size_t len = strlen(aPath);
    if (!len) return;
    if (aPath[len - 1] == '/') {
      AddDir(aPerms, aPath);
    } else {
      AddPath(aPerms, aPath);
    }
  }
}

void
SandboxBroker::Policy::AddAncestors(const char* aPath, int aPerms)
{
  nsAutoCString path(aPath);

  while (true) {
    const auto lastSlash = path.RFindCharInSet("/");
    if (lastSlash <= 0) {
      MOZ_ASSERT(lastSlash == 0);
      return;
    }
    path.Truncate(lastSlash);
    AddPath(aPerms, path.get());
  }
}

void
SandboxBroker::Policy::FixRecursivePermissions()
{
  // This builds an entirely new hashtable in order to avoid iterator
  // invalidation problems.
  PathPermissionMap oldMap;
  mMap.SwapElements(oldMap);

  if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
    SANDBOX_LOG_ERROR("fixing recursive policy entries");
  }

  for (auto iter = oldMap.ConstIter(); !iter.Done(); iter.Next()) {
    const nsACString& path = iter.Key();
    const int& localPerms = iter.Data();
    int inheritedPerms = 0;

    nsAutoCString ancestor(path);
    // This is slightly different from the loop in AddAncestors: it
    // leaves the trailing slashes attached so they'll match AddDir
    // entries.
    while (true) {
      // Last() release-asserts that the string is not empty.  We
      // should never have empty keys in the map, and the Truncate()
      // below will always give us a non-empty string.
      if (ancestor.Last() == '/') {
        ancestor.Truncate(ancestor.Length() - 1);
      }
      const auto lastSlash = ancestor.RFindCharInSet("/");
      if (lastSlash < 0) {
        MOZ_ASSERT(ancestor.IsEmpty());
        break;
      }
      ancestor.Truncate(lastSlash + 1);
      const int ancestorPerms = oldMap.Get(ancestor);
      if (ancestorPerms & RECURSIVE) {
        inheritedPerms |= ancestorPerms & ~RECURSIVE;
      }
    }

    const int newPerms = localPerms | inheritedPerms;
    if ((newPerms & ~RECURSIVE) == inheritedPerms) {
      if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
        SANDBOX_LOG_ERROR("removing redundant %s: %d -> %d", PromiseFlatCString(path).get(),
                          localPerms, newPerms);
      }
      // Skip adding this entry to the new map.
      continue;
    }
    if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
      SANDBOX_LOG_ERROR("new policy for %s: %d -> %d", PromiseFlatCString(path).get(),
                        localPerms, newPerms);
    }
    mMap.Put(path, newPerms);
  }
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
AllowOperation(int aReqFlags, int aPerms)
{
  int needed = 0;
  if (aReqFlags & R_OK) {
    needed |= SandboxBroker::MAY_READ;
  }
  if (aReqFlags & W_OK) {
    needed |= SandboxBroker::MAY_WRITE;
  }
  // We don't really allow executing anything,
  // so in true unix tradition we hijack this
  // for directories.
  if (aReqFlags & X_OK) {
    needed |= SandboxBroker::MAY_CREATE;
  }
  return (aPerms & needed) == needed;
}

static bool
AllowAccess(int aReqFlags, int aPerms)
{
  if (aReqFlags & ~(R_OK|W_OK|X_OK|F_OK)) {
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
  // Linux allows O_TRUNC even with O_RDONLY
  if (aReqFlags & O_TRUNC) {
    needed |= SandboxBroker::MAY_WRITE;
  }
  return (aPerms & needed) == needed;
}

static int
DoStat(const char* aPath, void* aBuff, int aFlags)
{
 if (aFlags & O_NOFOLLOW) {
    return lstatsyscall(aPath, (statstruct*)aBuff);
  }
  return statsyscall(aPath, (statstruct*)aBuff);
}

static int
DoLink(const char* aPath, const char* aPath2,
       SandboxBrokerCommon::Operation aOper)
{
  if (aOper == SandboxBrokerCommon::Operation::SANDBOX_FILE_LINK) {
    return link(aPath, aPath2);
  }
  if (aOper == SandboxBrokerCommon::Operation::SANDBOX_FILE_SYMLINK) {
    return symlink(aPath, aPath2);
  }
  MOZ_CRASH("SandboxBroker: Unknown link operation");
}

static int
DoConnect(const char* aPath, size_t aLen, int aType)
{
  // Deny SOCK_DGRAM for the same reason it's denied for socketpair.
  if (aType != SOCK_STREAM && aType != SOCK_SEQPACKET) {
    errno = EACCES;
    return -1;
  }
  // Ensure that the address is a pathname.  (An empty string
  // resulting from an abstract address probably shouldn't have made
  // it past the policy check, but check explicitly just in case.)
  if (aPath[0] == '\0') {
    errno = ECONNREFUSED;
    return -1;
  }

  // Try to copy the name into a normal-sized sockaddr_un, with
  // null-termination:
  struct sockaddr_un sun;
  memset(&sun, 0, sizeof(sun));
  sun.sun_family = AF_UNIX;
  if (aLen + 1 > sizeof(sun.sun_path)) {
    errno = ENAMETOOLONG;
    return -1;
  }
  memcpy(&sun.sun_path, aPath, aLen);

  // Finally, the actual socket connection.
  const int fd = socket(AF_UNIX, aType | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return -1;
  }
  if (connect(fd, reinterpret_cast<struct sockaddr*>(&sun), sizeof(sun)) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

size_t
SandboxBroker::RealPath(char* aPath, size_t aBufSize, size_t aPathLen)
{
  char* result = realpath(aPath, nullptr);
  if (result != nullptr) {
    base::strlcpy(aPath, result, aBufSize);
    free(result);
    // Size changed, but guaranteed to be 0 terminated
    aPathLen = strlen(aPath);
  }
  return aPathLen;
}

size_t
SandboxBroker::ConvertRelativePath(char* aPath, size_t aBufSize, size_t aPathLen)
{
  if (strstr(aPath, "..") != nullptr) {
    return RealPath(aPath, aBufSize, aPathLen);
  }
  return aPathLen;
}

size_t
SandboxBroker::RemapTempDirs(char* aPath, size_t aBufSize, size_t aPathLen)
{
  nsAutoCString path(aPath);

  size_t prefixLen = 0;
  if (!mTempPath.IsEmpty() && StringBeginsWith(path, mTempPath)) {
    prefixLen = mTempPath.Length();
  } else if (StringBeginsWith(path, tempDirPrefix)) {
    prefixLen = tempDirPrefix.Length();
  }

  if (prefixLen) {
    const nsDependentCSubstring cutPath =
      Substring(path, prefixLen, path.Length() - prefixLen);

    // Only now try to get the content process temp dir
    nsCOMPtr<nsIFile> tmpDir;
    nsresult rv = NS_GetSpecialDirectory(NS_APP_CONTENT_PROCESS_TEMP_DIR,
                                          getter_AddRefs(tmpDir));
    if (NS_SUCCEEDED(rv)) {
      nsAutoCString tmpPath;
      rv = tmpDir->GetNativePath(tmpPath);
      if (NS_SUCCEEDED(rv)) {
        tmpPath.Append(cutPath);
        base::strlcpy(aPath, tmpPath.get(), aBufSize);
        return strlen(aPath);
      }
    }
  }

  return aPathLen;
}

nsCString
SandboxBroker::ReverseSymlinks(const nsACString& aPath)
{
  // Revert any symlinks we previously resolved.
  int32_t cutLength = aPath.Length();
  nsCString cutPath(Substring(aPath, 0, cutLength));

  for (;;) {
    nsCString orig;
    bool found = mSymlinkMap.Get(cutPath, &orig);
    if (found) {
      orig.Append(Substring(aPath, cutLength, aPath.Length() - cutLength));
      return orig;
    }
    // Not found? Remove a path component and try again.
    int32_t pos = cutPath.RFindChar('/');
    if (pos == kNotFound || pos <= 0) {
      // will be empty
      return orig;
    } else {
      // Cut until just before the /
      cutLength = pos;
      cutPath.Assign(Substring(cutPath, 0, cutLength));
    }
  }
}

int
SandboxBroker::SymlinkPermissions(const char* aPath, const size_t aPathLen)
{
  // Work on a temporary copy, so we can reverse it.
  // Because we bail on a writable dir, SymlinkPath
  // might not restore the callers' path exactly.
  char pathBufSymlink[kMaxPathLen + 1];
  strcpy(pathBufSymlink, aPath);

  nsCString orig = ReverseSymlinks(nsDependentCString(pathBufSymlink, aPathLen));
  if (!orig.IsEmpty()) {
    if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
      SANDBOX_LOG_ERROR("Reversing %s -> %s", aPath, orig.get());
    }
    base::strlcpy(pathBufSymlink, orig.get(), sizeof(pathBufSymlink));
  }

  int perms = 0;
  // Resolve relative paths, propagate permissions and
  // fail if a symlink is in a writable path. The output is in perms.
  char* result = SandboxBroker::SymlinkPath(mPolicy.get(), pathBufSymlink, NULL, &perms);
  if (result != NULL) {
    free(result);
    // We finished the translation, so we have a usable return in "perms".
    return perms;
  } else {
    // Empty path means we got a writable dir in the chain or tried
    // to back out of a link target.
    return 0;
  }
}

void
SandboxBroker::ThreadMain(void)
{
  char threadName[16];
  SprintfLiteral(threadName, "FS Broker %d", mChildPid);
  PlatformThread::SetName(threadName);

  // Permissive mode can only be enabled through an environment variable,
  // therefore it is sufficient to fetch the value once
  // before the main thread loop starts
  bool permissive = SandboxInfo::Get().Test(SandboxInfo::kPermissive);

  // Find the current temporary directory
  nsCOMPtr<nsIFile> tmpDir;
  nsresult rv = GetSpecialSystemDirectory(OS_TemporaryDirectory,
                                          getter_AddRefs(tmpDir));
  if (NS_SUCCEEDED(rv)) {
    rv = tmpDir->GetNativePath(mTempPath);
    if (NS_SUCCEEDED(rv)) {
      // Make sure there's no terminating /
      if (mTempPath.Last() == '/') {
        mTempPath.Truncate(mTempPath.Length() - 1);
      }
    }
  }
  // If we can't find it, we aren't bothered much: we will
  // always try /tmp anyway in the substitution code
  if (NS_FAILED(rv) || mTempPath.IsEmpty()) {
    if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
      SANDBOX_LOG_ERROR("Tempdir: /tmp");
    }
  } else {
    if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
      SANDBOX_LOG_ERROR("Tempdir: %s", mTempPath.get());
    }
    // If it's /tmp, clear it here so we don't compare against
    // it twice. Just let the fallback code do the work.
    if (mTempPath.Equals(tempDirPrefix)) {
      mTempPath.Truncate();
    }
  }

  while (true) {
    struct iovec ios[2];
    // We will receive the path strings in 1 buffer and split them back up.
    char recvBuf[2 * (kMaxPathLen + 1)];
    char pathBuf[kMaxPathLen + 1];
    char pathBuf2[kMaxPathLen + 1];
    size_t pathLen = 0;
    size_t pathLen2 = 0;
    char respBuf[kMaxPathLen + 1]; // Also serves as struct stat
    Request req;
    Response resp;
    int respfd;

    // Make sure stat responses fit in the response buffer
    MOZ_ASSERT((kMaxPathLen + 1) > sizeof(struct stat));

    // This makes our string handling below a bit less error prone.
    memset(recvBuf, 0, sizeof(recvBuf));

    ios[0].iov_base = &req;
    ios[0].iov_len = sizeof(req);
    ios[1].iov_base = recvBuf;
    ios[1].iov_len = sizeof(recvBuf);

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
    memset(&respBuf, 0, sizeof(respBuf));
    resp.mError = -EACCES;
    ios[0].iov_base = &resp;
    ios[0].iov_len = sizeof(resp);
    ios[1].iov_base = nullptr;
    ios[1].iov_len = 0;
    int openedFd = -1;

    // Clear permissions
    int perms;

    // Find end of first string, make sure the buffer is still
    // 0 terminated.
    size_t recvBufLen = static_cast<size_t>(recvd) - sizeof(req);
    if (recvBufLen > 0 && recvBuf[recvBufLen - 1] != 0) {
      SANDBOX_LOG_ERROR("corrupted path buffer from pid %d", mChildPid);
      shutdown(mFileDesc, SHUT_RD);
      break;
    }

    // First path should fit in maximum path length buffer.
    size_t first_len = strlen(recvBuf);
    if (first_len <= kMaxPathLen) {
      strcpy(pathBuf, recvBuf);
      // Skip right over the terminating 0, and try to copy in the
      // second path, if any. If there's no path, this will hit a
      // 0 immediately (we nulled the buffer before receiving).
      // We do not assume the second path is 0-terminated, this is
      // enforced below.
      strncpy(pathBuf2, recvBuf + first_len + 1, kMaxPathLen + 1);

      // First string is guaranteed to be 0-terminated.
      pathLen = first_len;

      // Look up the first pathname but first translate relative paths.
      pathLen = ConvertRelativePath(pathBuf, sizeof(pathBuf), pathLen);
      perms = mPolicy->Lookup(nsDependentCString(pathBuf, pathLen));

      // We don't have permissions on the requested dir.
      if (!perms) {
        // Was it a tempdir that we can remap?
        pathLen = RemapTempDirs(pathBuf, sizeof(pathBuf), pathLen);
        perms = mPolicy->Lookup(nsDependentCString(pathBuf, pathLen));
      }
      if (!perms) {
        // Did we arrive from a symlink in a path that is not writable?
        // Then try to figure out the original path and see if that is
        // readable. Work on the original path, this reverses
        // ConvertRelative above.
        int symlinkPerms = SymlinkPermissions(recvBuf, first_len);
        if (symlinkPerms > 0) {
          perms = symlinkPerms;
        }
      }
      if (!perms) {
        // Now try the opposite case: translate symlinks to their
        // actual destination file. Firefox always resolves symlinks,
        // and in most cases we have whitelisted fixed paths that
        // libraries will rely on and try to open. So this codepath
        // is mostly useful for Mesa which had its kernel interface
        // moved around.
        pathLen = RealPath(pathBuf, sizeof(pathBuf), pathLen);
        perms = mPolicy->Lookup(nsDependentCString(pathBuf, pathLen));
      }

      // Same for the second path.
      pathLen2 = strnlen(pathBuf2, kMaxPathLen);
      if (pathLen2 > 0) {
        // Force 0 termination.
        pathBuf2[pathLen2] = '\0';
        pathLen2 = ConvertRelativePath(pathBuf2, sizeof(pathBuf2), pathLen2);
        int perms2 = mPolicy->Lookup(nsDependentCString(pathBuf2, pathLen2));

        // Take the intersection of the permissions for both paths.
        perms &= perms2;
      }
    } else {
      // Failed to receive intelligible paths.
      perms = 0;
    }

    // And now perform the operation if allowed.
    if (perms & CRASH_INSTEAD) {
      // This is somewhat nonmodular, but it works.
      resp.mError = -ENOSYS;
    } else if (permissive || perms & MAY_ACCESS) {
      // If the operation was only allowed because of permissive mode, log it.
      if (permissive && !(perms & MAY_ACCESS)) {
        AuditPermissive(req.mOp, req.mFlags, perms, pathBuf);
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
            resp.mError = -errno;
          }
        } else {
          AuditDenial(req.mOp, req.mFlags, perms, pathBuf);
        }
        break;

      case SANDBOX_FILE_ACCESS:
        if (permissive || AllowAccess(req.mFlags, perms)) {
          if (access(pathBuf, req.mFlags) == 0) {
            resp.mError = 0;
          } else {
            resp.mError = -errno;
          }
        } else {
          AuditDenial(req.mOp, req.mFlags, perms, pathBuf);
        }
        break;

      case SANDBOX_FILE_STAT:
        if (DoStat(pathBuf, (struct stat*)&respBuf, req.mFlags) == 0) {
          resp.mError = 0;
          ios[1].iov_base = &respBuf;
          ios[1].iov_len = req.mBufSize;
        } else {
          resp.mError = -errno;
        }
        break;

      case SANDBOX_FILE_CHMOD:
        if (permissive || AllowOperation(W_OK, perms)) {
          if (chmod(pathBuf, req.mFlags) == 0) {
            resp.mError = 0;
          } else {
            resp.mError = -errno;
          }
        } else {
          AuditDenial(req.mOp, req.mFlags, perms, pathBuf);
        }
        break;

      case SANDBOX_FILE_LINK:
      case SANDBOX_FILE_SYMLINK:
        if (permissive || AllowOperation(W_OK, perms)) {
          if (DoLink(pathBuf, pathBuf2, req.mOp) == 0) {
            resp.mError = 0;
          } else {
            resp.mError = -errno;
          }
        } else {
          AuditDenial(req.mOp, req.mFlags, perms, pathBuf);
        }
        break;

      case SANDBOX_FILE_RENAME:
        if (permissive || AllowOperation(W_OK, perms)) {
          if (rename(pathBuf, pathBuf2) == 0) {
            resp.mError = 0;
          } else {
            resp.mError = -errno;
          }
        } else {
          AuditDenial(req.mOp, req.mFlags, perms, pathBuf);
        }
        break;

      case SANDBOX_FILE_MKDIR:
        if (permissive || AllowOperation(W_OK | X_OK, perms)) {
          if (mkdir(pathBuf, req.mFlags) == 0) {
            resp.mError = 0;
          } else {
            resp.mError = -errno;
          }
        } else {
          struct stat sb;
          // This doesn't need an additional policy check because
          // MAY_ACCESS is required to even enter this switch statement.
          if (lstat(pathBuf, &sb) == 0) {
            resp.mError = -EEXIST;
          } else {
            AuditDenial(req.mOp, req.mFlags, perms, pathBuf);
          }
        }
        break;

      case SANDBOX_FILE_UNLINK:
        if (permissive || AllowOperation(W_OK, perms)) {
          if (unlink(pathBuf) == 0) {
            resp.mError = 0;
          } else {
            resp.mError = -errno;
          }
        } else {
          AuditDenial(req.mOp, req.mFlags, perms, pathBuf);
        }
        break;

      case SANDBOX_FILE_RMDIR:
        if (permissive || AllowOperation(W_OK | X_OK, perms)) {
          if (rmdir(pathBuf) == 0) {
            resp.mError = 0;
          } else {
            resp.mError = -errno;
          }
        } else {
          AuditDenial(req.mOp, req.mFlags, perms, pathBuf);
        }
        break;

      case SANDBOX_FILE_READLINK:
        if (permissive || AllowOperation(R_OK, perms)) {
          ssize_t respSize = readlink(pathBuf, (char*)&respBuf, sizeof(respBuf));
          if (respSize >= 0) {
              if (respSize > 0) {
              // Record the mapping so we can invert the file to the original
              // symlink.
              nsDependentCString orig(pathBuf, pathLen);
              nsDependentCString xlat(respBuf, respSize);
              if (!orig.Equals(xlat) && xlat[0] == '/') {
                if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
                  SANDBOX_LOG_ERROR("Recording mapping %s -> %s",
                                    xlat.get(), orig.get());
                }
                mSymlinkMap.Put(xlat, orig);
              }
              // Make sure we can invert a fully resolved mapping too. If our
              // caller is realpath, and there's a relative path involved, the
              // client side will try to open this one.
              char *resolvedBuf = realpath(pathBuf, nullptr);
              if (resolvedBuf) {
                nsDependentCString resolvedXlat(resolvedBuf);
                if (!orig.Equals(resolvedXlat) && !xlat.Equals(resolvedXlat)) {
                  if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
                    SANDBOX_LOG_ERROR("Recording mapping %s -> %s",
                                      resolvedXlat.get(), orig.get());
                  }
                  mSymlinkMap.Put(resolvedXlat, orig);
                }
                free(resolvedBuf);
              }
            }
            resp.mError = respSize;
            ios[1].iov_base = &respBuf;
            ios[1].iov_len = respSize;
          } else {
            resp.mError = -errno;
          }
        } else {
          AuditDenial(req.mOp, req.mFlags, perms, pathBuf);
        }
        break;

      case SANDBOX_SOCKET_CONNECT:
        if (permissive || (perms & MAY_CONNECT) != 0) {
          openedFd = DoConnect(pathBuf, pathLen, req.mFlags);
          if (openedFd >= 0) {
            resp.mError = 0;
          } else {
            resp.mError = -errno;
          }
        } else {
          AuditDenial(req.mOp, req.mFlags, perms, pathBuf);
        }
        break;
      }
    } else {
      MOZ_ASSERT(perms == 0);
      AuditDenial(req.mOp, req.mFlags, perms, pathBuf);
    }

    const size_t numIO = ios[1].iov_len > 0 ? 2 : 1;
    const ssize_t sent = SendWithFd(respfd, ios, numIO, openedFd);
    if (sent < 0) {
      SANDBOX_LOG_ERROR("failed to send broker response to pid %d: %s", mChildPid,
                        strerror(errno));
    }
    close(respfd);
    MOZ_ASSERT(sent < 0 ||
               static_cast<size_t>(sent) == ios[0].iov_len + ios[1].iov_len);

    if (openedFd >= 0) {
      close(openedFd);
    }
  }
}

void
SandboxBroker::AuditPermissive(int aOp, int aFlags, int aPerms, const char* aPath)
{
  MOZ_RELEASE_ASSERT(SandboxInfo::Get().Test(SandboxInfo::kPermissive));

  struct stat statBuf;

  if (lstat(aPath, &statBuf) == 0) {
    // Path exists, set errno to 0 to indicate "success".
    errno = 0;
  }

  SANDBOX_LOG_ERROR("SandboxBroker: would have denied op=%s rflags=%o perms=%d path=%s for pid=%d" \
                    " permissive=1 error=\"%s\"", OperationDescription[aOp],
                    aFlags, aPerms,
                    aPath, mChildPid, strerror(errno));
}

void
SandboxBroker::AuditDenial(int aOp, int aFlags, int aPerms, const char* aPath)
{
  if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
    SANDBOX_LOG_ERROR("SandboxBroker: denied op=%s rflags=%o perms=%d path=%s for pid=%d",
                      OperationDescription[aOp], aFlags,
                      aPerms, aPath, mChildPid);
  }
}


} // namespace mozilla
