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
// The policy is currently just a map from strings to sets of
// permissions; the broker doesn't attempt to interpret or
// canonicalize pathnames.  This makes the broker simpler, and thus
// less likely to contain vulnerabilities a compromised client could
// exploit.  (This might need to change in the future if we need to
// whitelist a set of files that could change after policy
// construction, like hotpluggable devices.)
//
// The broker currently runs on a thread in the parent process (with
// effective uid changed on B2G), which is for memory efficiency
// (compared to forking a process) and simplicity (compared to having
// a separate executable and serializing/deserializing the policy).
//
// See also ../SandboxBrokerClient.h for the corresponding client.

class SandboxBroker final
  : private SandboxBrokerCommon
  , public PlatformThread::Delegate
{
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
  };
  // Bitwise operations on enum values return ints, so just use int in
  // the hash table type (and below) to avoid cluttering code with casts.
  typedef nsDataHashtable<nsCStringHashKey, int> PathMap;

  class Policy {
    PathMap mMap;
  public:
    Policy();
    Policy(const Policy& aOther);
    ~Policy();

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
    // All files in a directory with a given prefix; useful for devices.
    void AddPrefix(int aPerms, const char* aDir, const char* aPrefix);
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
  };

  // Constructing a broker involves creating a socketpair and a
  // background thread to handle requests, so it can fail.  If this
  // returns nullptr, do not use the value of aClientFdOut.
  static UniquePtr<SandboxBroker>
    Create(UniquePtr<const Policy> aPolicy, int aChildPid,
           ipc::FileDescriptor& aClientFdOut);
  virtual ~SandboxBroker();

 private:
  PlatformThreadHandle mThread;
  int mFileDesc;
  const int mChildPid;
  const UniquePtr<const Policy> mPolicy;

  SandboxBroker(UniquePtr<const Policy> aPolicy, int aChildPid,
                int& aClientFd);
  void ThreadMain(void) override;

  // Holding a UniquePtr should disallow copying, but to make that explicit:
  SandboxBroker(const SandboxBroker&) = delete;
  void operator=(const SandboxBroker&) = delete;
};

} // namespace mozilla

#endif // mozilla_SandboxBroker_h
