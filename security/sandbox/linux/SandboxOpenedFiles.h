/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxOpenedFiles_h
#define mozilla_SandboxOpenedFiles_h

#include "mozilla/Atomics.h"
#include "mozilla/Range.h"
#include "mozilla/UniquePtr.h"

#include <vector>
#include <string>

// The use of C++ standard library containers here should be safe; the
// standard (section container.requirements.dataraces) requires that
// using const methods/pointers not introduce data races (e.g., from
// interior mutability or global state).
//
// Reentrancy isn't guaranteed, and the library could use async signal
// unsafe mutexes for "read-only" operations, but I'm assuming that that's
// not the case at least for simple containers like string and vector.

namespace mozilla {

// This class represents a file that's been pre-opened for a media
// plugin.  It can be move-constructed but not copied.
class SandboxOpenedFile final {
 public:
  // This constructor opens the named file and saves the descriptor.
  // If the open fails, IsOpen() will return false and GetDesc() will
  // quietly return -1.  If aDup is true, GetDesc() will return a
  // dup() of the descriptor every time it's called; otherwise, the
  // first call will return the descriptor and any further calls will
  // log an error message and return -1.
  explicit SandboxOpenedFile(const char* aPath, bool aDup = false);

  // Simulates opening the pre-opened file; see the constructor's
  // comment for details.  Does not set errno on error, but may modify
  // it as a side-effect.  Thread-safe and intended to be async signal safe.
  int GetDesc() const;

  const char* Path() const { return mPath.c_str(); }

  bool IsOpen() const { return mMaybeFd >= 0; }

  ~SandboxOpenedFile();

  MOZ_IMPLICIT SandboxOpenedFile(SandboxOpenedFile&& aMoved);

 private:
  std::string mPath;
  mutable Atomic<int> mMaybeFd;
  bool mDup;
  bool mExpectError;

  int TakeDesc() const { return mMaybeFd.exchange(-1); }
};

// This class represents a collection of files to be used to handle
// open() calls from the media plugin (and the dynamic loader).
// Because the seccomp-bpf policy exists until the process exits, this
// object must not be destroyed after the syscall filter is installed.
class SandboxOpenedFiles {
public:
  SandboxOpenedFiles() = default;

  template<typename... Args>
  void Add(Args&&... aArgs) {
    mFiles.emplace_back(std::forward<Args>(aArgs)...);
  }

  int GetDesc(const char* aPath) const;

private:
  std::vector<SandboxOpenedFile> mFiles;

  // We could allow destroying instances of this class that aren't
  // used with seccomp-bpf (e.g., for unit testing) by having the
  // destructor check a flag set by the syscall policy and crash,
  // but let's not write that code until we actually need it.
  ~SandboxOpenedFiles() = delete;
};

} // namespace mozilla

#endif // mozilla_SandboxOpenedFiles_h
