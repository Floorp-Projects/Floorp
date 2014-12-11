/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxAssembler_h
#define mozilla_SandboxAssembler_h

#include <vector>

#include "mozilla/Assertions.h"

struct sock_filter;

namespace mozilla {

class SandboxAssembler {
public:
  class Condition {
    friend class SandboxAssembler;
    uint32_t mSyscallNr;
    bool mCheckingArg;
    uint8_t mArgChecked;
    // This class retains a copy of the array, because C++11
    // initializer_list isn't supported on all relevant platforms.
    std::vector<uint32_t> mArgValues;
  public:
    // Match any instance of the given syscall, with any arguments.
    explicit Condition(uint32_t aSyscallNr)
      : mSyscallNr(aSyscallNr)
      , mCheckingArg(false)
    { }
    // Match the syscall only if the given argument is one of the
    // values in the array specified.  (If the argument isn't
    // representable as uint32, the process is killed or signaled, as
    // appropriate.)
    template<size_t n>
    Condition(uint32_t aSyscallNr, uint8_t aArgChecked,
              const uint32_t (&aArgValues)[n])
      : mSyscallNr(aSyscallNr)
      , mCheckingArg(true)
      , mArgChecked(aArgChecked)
      , mArgValues(aArgValues, aArgValues + n)
    {
      MOZ_ASSERT(aArgChecked < sNumArgs);
    }
    // This isn't perf-critical, so a naive copy ctor is fine.
    Condition(const Condition& aOther)
    : mSyscallNr(aOther.mSyscallNr)
    , mCheckingArg(aOther.mCheckingArg)
    , mArgChecked(aOther.mArgChecked)
    , mArgValues(aOther.mArgValues)
    { }
  };

  // Allow syscalls matching this condition, if no earlier condition matched.
  void Allow(const Condition& aCond) {
    mRuleStack.push_back(std::make_pair(0, aCond));
  }
  // Cause syscalls matching this condition to fail with the given error, if
  // no earlier condition matched.
  void Deny(int aErrno, const Condition& aCond) {
    MOZ_ASSERT(aErrno != 0);
    mRuleStack.push_back(std::make_pair(aErrno, aCond));
  }

  void Compile(std::vector<sock_filter>* aProgram, bool aPrint = false);
private:
  std::vector<std::pair<int, Condition>> mRuleStack;

  static const uint8_t sNumArgs = 6;
};

}

#endif
