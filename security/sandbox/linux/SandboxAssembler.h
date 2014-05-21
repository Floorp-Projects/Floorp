/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxAssembler_h
#define mozilla_SandboxAssembler_h

#include "sandbox/linux/seccomp-bpf/codegen.h"

#include <vector>
#include "mozilla/Assertions.h"

using namespace sandbox;

namespace mozilla {

class SandboxAssembler {
public:
  SandboxAssembler();
  ~SandboxAssembler();

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
  };

  // Allow syscalls matching this condition, if no earlier condition matched.
  void Allow(const Condition &aCond) {
    Handle(aCond, RetAllow());
  }
  // Cause syscalls matching this condition to fail with the given error, if
  // no earlier condition matched.
  void Deny(int aErrno, const Condition &aCond) {
    Handle(aCond, RetDeny(aErrno));
  }

  void Finish();
  void Compile(std::vector<struct sock_filter> *aProgram,
               bool aPrint = false);
private:
  CodeGen mCode;
  // The entry point of the filter program.
  Instruction *mHead;
  // Pointer to an instruction with a null successor which needs to be filled
  // in with the rest of the program; see CodeGen::JoinInstructions.
  Instruction *mTail;
  // In some cases we will have two such instructions; this, if not null, is
  // that.  (If we have more complicated conditions in the future, this may
  // need to be generalized into a vector<Instruction*>.)
  Instruction *mTailAlt;

  Instruction *RetAllow();
  Instruction *RetDeny(int aErrno);
  Instruction *RetKill();
  Instruction *LoadArch(Instruction *aNext);
  Instruction *LoadSyscall(Instruction *aNext);
  Instruction *LoadArgHi(int aArg, Instruction *aNext);
  Instruction *LoadArgLo(int aArg, Instruction *aNext);
  Instruction *JmpEq(uint32_t aValue, Instruction *aThen, Instruction *aElse);
  void AppendCheck(Instruction *aCheck,
                   Instruction *aNewTail,
                   Instruction *aNewTailAlt);
  void Handle(const Condition &aCond, Instruction* aResult);

  static const uint8_t sNumArgs = 6;
};

}

#endif
