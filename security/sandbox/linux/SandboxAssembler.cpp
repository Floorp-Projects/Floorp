/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxAssembler.h"

#include <errno.h>
#include <utility>

#include "sandbox/linux/bpf_dsl/dump_bpf.h"
#include "sandbox/linux/bpf_dsl/trap_registry.h"
#include "sandbox/linux/seccomp-bpf/codegen.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"

namespace mozilla {

// Need this for the SECCOMP_*_IDX macros to work.
using sandbox::arch_seccomp_data;

class SandboxAssemblerImpl {
  typedef sandbox::Instruction* NodePtr;

  sandbox::CodeGen mCode;
  NodePtr mHead;

  NodePtr RetAllow();
  NodePtr RetDeny(int aErrno);
  NodePtr RetKill();
  NodePtr LoadArch(NodePtr aNext);
  NodePtr LoadSyscall(NodePtr aNext);
  NodePtr LoadArgHi(int aArg, NodePtr aNext);
  NodePtr LoadArgLo(int aArg, NodePtr aNext);
  NodePtr JmpEq(uint32_t aValue, NodePtr aThen, NodePtr aElse);
public:
  SandboxAssemblerImpl();
  void PrependCheck(int aMaybeError, int aSyscallNr);
  void PrependCheck(int aMaybeError, int aSyscallNr, int aArgChecked,
                    const std::vector<uint32_t>& aArgValues);
  void Compile(std::vector<sock_filter>* aProgram, bool aPrint);
};

void
SandboxAssemblerImpl::Compile(std::vector<sock_filter>* aProgram, bool aPrint)
{
  NodePtr prog = LoadSyscall(mHead);
  prog = LoadArch(JmpEq(SECCOMP_ARCH, prog, RetKill()));

  mCode.Compile(prog, aProgram);
  if (aPrint) {
    sandbox::bpf_dsl::DumpBPF::PrintProgram(*aProgram);
  }
}

void
SandboxAssemblerImpl::PrependCheck(int aMaybeError, int aSyscallNr)
{
  NodePtr ret = aMaybeError ? RetDeny(aMaybeError) : RetAllow();
  mHead = JmpEq(aSyscallNr, ret, mHead);
}

void
SandboxAssemblerImpl::PrependCheck(int aMaybeError, int aSyscallNr,
                                   int aArgChecked,
                                   const std::vector<uint32_t>& aArgValues)
{
  NodePtr ret = aMaybeError ? RetDeny(aMaybeError) : RetAllow();
  NodePtr noMatch = mHead;
  NodePtr checkArg = LoadSyscall(noMatch);

  // Loop backwards, prepending checks onto the no-match base case.
  for (auto i = aArgValues.rbegin(); i != aArgValues.rend(); ++i) {
    checkArg = JmpEq(*i, ret, checkArg);
  }
  checkArg = LoadArgLo(aArgChecked, checkArg);
  checkArg = LoadArgHi(aArgChecked, JmpEq(0, checkArg, RetKill()));

  mHead = JmpEq(aSyscallNr, checkArg, noMatch);
}

SandboxAssemblerImpl::SandboxAssemblerImpl() {
  mHead = RetKill();
}

void
SandboxAssembler::Compile(std::vector<sock_filter>* aProgram, bool aPrint)
{
  SandboxAssemblerImpl impl;

  for (auto i = mRuleStack.rbegin(); i != mRuleStack.rend(); ++i) {
    if (i->second.mCheckingArg) {
      impl.PrependCheck(i->first, i->second.mSyscallNr, i->second.mArgChecked,
                        i->second.mArgValues);
    } else {
      impl.PrependCheck(i->first, i->second.mSyscallNr);
    }
  }

  impl.Compile(aProgram, aPrint);
}


SandboxAssemblerImpl::NodePtr
SandboxAssemblerImpl::LoadArch(NodePtr aNext)
{
  return mCode.MakeInstruction(BPF_LD + BPF_W + BPF_ABS,
                               SECCOMP_ARCH_IDX,
                               aNext);
}

SandboxAssemblerImpl::NodePtr
SandboxAssemblerImpl::LoadSyscall(NodePtr aNext)
{
  return mCode.MakeInstruction(BPF_LD + BPF_W + BPF_ABS,
                               SECCOMP_NR_IDX,
                               aNext);
}

SandboxAssemblerImpl::NodePtr
SandboxAssemblerImpl::LoadArgHi(int aArg, NodePtr aNext)
{
  return mCode.MakeInstruction(BPF_LD + BPF_W + BPF_ABS,
                               SECCOMP_ARG_MSB_IDX(aArg),
                               aNext);
}

SandboxAssemblerImpl::NodePtr
SandboxAssemblerImpl::LoadArgLo(int aArg, NodePtr aNext)
{
  return mCode.MakeInstruction(BPF_LD + BPF_W + BPF_ABS,
                               SECCOMP_ARG_LSB_IDX(aArg),
                               aNext);
}

SandboxAssemblerImpl::NodePtr
SandboxAssemblerImpl::JmpEq(uint32_t aValue, NodePtr aThen, NodePtr aElse)
{
  return mCode.MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K,
                               aValue, aThen, aElse);
}

SandboxAssemblerImpl::NodePtr
SandboxAssemblerImpl::RetAllow()
{
  return mCode.MakeInstruction(BPF_RET + BPF_K,
                               SECCOMP_RET_ALLOW,
                               nullptr);
}

SandboxAssemblerImpl::NodePtr
SandboxAssemblerImpl::RetDeny(int aErrno)
{
  return mCode.MakeInstruction(BPF_RET + BPF_K,
                               SECCOMP_RET_ERRNO + aErrno,
                               nullptr);
}

SandboxAssemblerImpl::NodePtr
SandboxAssemblerImpl::RetKill()
{
  return mCode.MakeInstruction(BPF_RET + BPF_K,
                               SECCOMP_RET_TRAP,
                               nullptr);
}

} // namespace mozilla
