/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxBroker_h
#define mozilla_SandboxBroker_h

#include "mozilla/SandboxBrokerCommon.h"

#include "base/platform_thread.h"
#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsString.h"

namespace mozilla {

namespace ipc {
class FileDescriptor;
}

// This class implements a broker for filesystem operations requested
// by a sandboxed child process -- opening files and accessing their
// metadata.  (This is necessary in order to restrict access by path;
// seccomp-bpf can filter only on argument register values, not
// parameters passed in memory like pathnames.)
//
// The broker currently runs on a thread in the parent process (with
// effective uid changed on B2G), which is for memory efficiency
// (compared to forking a process) and simplicity (compared to having
// a separate executable and serializing/deserializing the policy).
//
// See also ../SandboxBrokerClient.h for the corresponding client.

class SandboxBroker final : private SandboxBrokerCommon,
                            public PlatformThread::Delegate {
 public:
  enum Perms {
    MAY_ACCESS = 1 << 0,
    MAY_READ = 1 << 1,
    MAY_WRITE = 1 << 2,
    MAY_CREATE = 1 << 3,
    // This flag is for testing policy changes -- when the client is
    // used with the seccomp-bpf integration, an access to this file
    // will invoke a crash dump with the context of the syscall.
    // (This overrides all other flags.)
    CRASH_INSTEAD = 1 << 4,
    // Applies to everything below this path, including subdirs created
    // at runtime
    RECURSIVE = 1 << 5,
    // Allow Unix-domain socket connections to a path
    MAY_CONNECT = 1 << 6,
  };
  // Bitwise operations on enum values return ints, so just use int in
  // the hash table type (and below) to avoid cluttering code with casts.
  typedef nsDataHashtable<nsCStringHashKey, int> PathPermissionMap;

  class Policy {
    PathPermissionMap mMap;

   public:
    Policy();
    Policy(const Policy& aOther);
    ~Policy();

    // Add permissions from AddDir/AddDynamic rules to any rules that
    // exist for their descendents, and remove any descendent rules
    // made redundant by this process.
    //
    // Call this after adding rules and before using the policy to
    // prevent the descendent rules from shadowing the ancestor rules
    // and removing permissions that we expect the file to have.
    void FixRecursivePermissions();

    enum AddCondition {
      AddIfExistsNow,
      AddAlways,
    };
    // Typically, files that don't exist at policy creation time don't
    // need to be whitelisted, but this allows adding entries for
    // them if they'll exist later.  See also the overload below.
    void AddPath(int aPerms, const char* aPath, AddCondition aCond);
    // This adds all regular files (not directories) in the tree
    // rooted at the given path.
    void AddTree(int aPerms, const char* aPath);
    // A directory, and all files and directories under it, even those
    // added after creation (the dir itself must exist).
    void AddDir(int aPerms, const char* aPath);
    // All files in a directory with a given prefix; useful for devices.
    void AddFilePrefix(int aPerms, const char* aDir, const char* aPrefix);
    // Everything starting with the given path, even those files/dirs
    // added after creation. The file or directory may or may not exist.
    void AddPrefix(int aPerms, const char* aPath);
    // Adds a file or dir (end with /) if it exists, and a prefix otherwhise.
    void AddDynamic(int aPerms, const char* aPath);
    // Adds permissions on all ancestors of a path.  (This doesn't
    // include the root directory, but if the path is given with a
    // trailing slash it includes the path without the slash.)
    void AddAncestors(const char* aPath, int aPerms = MAY_ACCESS);
    // Default: add file if it exists when creating policy or if we're
    // conferring permission to create it (log files, etc.).
    void AddPath(int aPerms, const char* aPath) {
      AddPath(aPerms, aPath,
              (aPerms & MAY_CREATE) ? AddAlways : AddIfExistsNow);
    }
    int Lookup(const nsACString& aPath) const;
    int Lookup(const char* aPath) const {
      return Lookup(nsDependentCString(aPath));
    }

    bool IsEmpty() const { return mMap.Count() == 0; }

   private:
    // ValidatePath checks |path| and returns true if these conditions are met
    // * Greater than 0 length
    // * Is an absolute path
    // * No trailing slash
    // * No /../ path traversal
    bool ValidatePath(const char* path) const;
    void AddPrefixInternal(int aPerms, const nsACString& aPath);
  };

  // Constructing a broker involves creating a socketpair and a
  // background thread to handle requests, so it can fail.  If this
  // returns nullptr, do not use the value of aClientFdOut.
  static UniquePtr<SandboxBroker> Create(UniquePtr<const Policy> aPolicy,
                                         int aChildPid,
                                         ipc::FileDescriptor& aClientFdOut);
  virtual ~SandboxBroker();

 private:
  PlatformThreadHandle mThread;
  int mFileDesc;
  const int mChildPid;
  const UniquePtr<const Policy> mPolicy;
  nsCString mTempPath;
  nsCString mContentTempPath;

  typedef nsDataHashtable<nsCStringHashKey, nsCString> PathMap;
  PathMap mSymlinkMap;

  SandboxBroker(UniquePtr<const Policy> aPolicy, int aChildPid, int& aClientFd);
  void ThreadMain(void) override;
  void AuditPermissive(int aOp, int aFlags, int aPerms, const char* aPath);
  void AuditDenial(int aOp, int aFlags, int aPerms, const char* aPath);
  // Remap relative paths to absolute paths.
  size_t ConvertRelativePath(char* aPath, size_t aBufSize, size_t aPathLen);
  size_t RealPath(char* aPath, size_t aBufSize, size_t aPathLen);
  // Remap references to /tmp and friends to the content process tempdir
  size_t RemapTempDirs(char* aPath, size_t aBufSize, size_t aPathLen);
  nsCString ReverseSymlinks(const nsACString& aPath);
  // Retrieves permissions for the path the original symlink sits in.
  int SymlinkPermissions(const char* aPath, const size_t aPathLen);
  // In SandboxBrokerRealPath.cpp
  char* SymlinkPath(const Policy* aPolicy, const char* __restrict aPath,
                    char* __restrict aResolved, int* aPermission);

  // Holding a UniquePtr should disallow copying, but to make that explicit:
  SandboxBroker(const SandboxBroker&) = delete;
  void operator=(const SandboxBroker&) = delete;
};

}  // namespace mozilla

#endif  // mozilla_SandboxBroker_h
