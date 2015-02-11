// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_POLICY_COMPILER_H_
#define SANDBOX_LINUX_BPF_DSL_POLICY_COMPILER_H_

#include <stdint.h>

#include <map>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "sandbox/linux/seccomp-bpf/codegen.h"
#include "sandbox/linux/seccomp-bpf/errorcode.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
struct Instruction;

namespace bpf_dsl {
class Policy;

// PolicyCompiler implements the bpf_dsl compiler, allowing users to
// transform bpf_dsl policies into BPF programs to be executed by the
// Linux kernel.
class SANDBOX_EXPORT PolicyCompiler {
 public:
  PolicyCompiler(const Policy* policy, TrapRegistry* registry);
  ~PolicyCompiler();

  // Compile registers any trap handlers needed by the policy and
  // compiles the policy to a BPF program, which it returns.
  scoped_ptr<CodeGen::Program> Compile();

  // Error returns an ErrorCode to indicate the system call should fail with
  // the specified error number.
  ErrorCode Error(int err);

  // We can use ErrorCode to request calling of a trap handler. This method
  // performs the required wrapping of the callback function into an
  // ErrorCode object.
  // The "aux" field can carry a pointer to arbitrary data. See EvaluateSyscall
  // for a description of how to pass data from SetSandboxPolicy() to a Trap()
  // handler.
  ErrorCode Trap(TrapRegistry::TrapFnc fnc, const void* aux);

  // Calls a user-space trap handler and disables all sandboxing for system
  // calls made from this trap handler.
  // This feature is available only if explicitly enabled by the user having
  // set the CHROME_SANDBOX_DEBUGGING environment variable.
  // Returns an ET_INVALID ErrorCode, if called when not enabled.
  // NOTE: This feature, by definition, disables all security features of
  //   the sandbox. It should never be used in production, but it can be
  //   very useful to diagnose code that is incompatible with the sandbox.
  //   If even a single system call returns "UnsafeTrap", the security of
  //   entire sandbox should be considered compromised.
  ErrorCode UnsafeTrap(TrapRegistry::TrapFnc fnc, const void* aux);

  // UnsafeTraps require some syscalls to always be allowed.
  // This helper function returns true for these calls.
  static bool IsRequiredForUnsafeTrap(int sysno);

  // We can also use ErrorCode to request evaluation of a conditional
  // statement based on inspection of system call parameters.
  // This method wrap an ErrorCode object around the conditional statement.
  // Argument "argno" (1..6) will be bitwise-AND'd with "mask" and compared
  // to "value"; if equal, then "passed" will be returned, otherwise "failed".
  // If "is32bit" is set, the argument must in the range of 0x0..(1u << 32 - 1)
  // If it is outside this range, the sandbox treats the system call just
  // the same as any other ABI violation (i.e. it aborts with an error
  // message).
  ErrorCode CondMaskedEqual(int argno,
                            ErrorCode::ArgType is_32bit,
                            uint64_t mask,
                            uint64_t value,
                            const ErrorCode& passed,
                            const ErrorCode& failed);

  // Kill the program and print an error message.
  ErrorCode Kill(const char* msg);

  // Returns the fatal ErrorCode that is used to indicate that somebody
  // attempted to pass a 64bit value in a 32bit system call argument.
  // This method is primarily needed for testing purposes.
  ErrorCode Unexpected64bitArgument();

 private:
  struct Range;
  typedef std::vector<Range> Ranges;
  typedef std::map<uint32_t, ErrorCode> ErrMap;
  typedef std::set<ErrorCode, struct ErrorCode::LessThan> Conds;

  // Used by CondExpressionHalf to track which half of the argument it's
  // emitting instructions for.
  enum ArgHalf {
    LowerHalf,
    UpperHalf,
  };

  // Compile the configured policy into a complete instruction sequence.
  Instruction* AssemblePolicy();

  // Return an instruction sequence that checks the
  // arch_seccomp_data's "arch" field is valid, and then passes
  // control to |passed| if so.
  Instruction* CheckArch(Instruction* passed);

  // If |has_unsafe_traps_| is true, returns an instruction sequence
  // that allows all system calls from Syscall::Call(), and otherwise
  // passes control to |rest|. Otherwise, simply returns |rest|.
  Instruction* MaybeAddEscapeHatch(Instruction* rest);

  // Return an instruction sequence that loads and checks the system
  // call number, performs a binary search, and then dispatches to an
  // appropriate instruction sequence compiled from the current
  // policy.
  Instruction* DispatchSyscall();

  // Return an instruction sequence that checks the system call number
  // (expected to be loaded in register A) and if valid, passes
  // control to |passed| (with register A still valid).
  Instruction* CheckSyscallNumber(Instruction* passed);

  // Finds all the ranges of system calls that need to be handled. Ranges are
  // sorted in ascending order of system call numbers. There are no gaps in the
  // ranges. System calls with identical ErrorCodes are coalesced into a single
  // range.
  void FindRanges(Ranges* ranges);

  // Returns a BPF program snippet that implements a jump table for the
  // given range of system call numbers. This function runs recursively.
  Instruction* AssembleJumpTable(Ranges::const_iterator start,
                                 Ranges::const_iterator stop);

  // Returns a BPF program snippet that makes the BPF filter program exit
  // with the given ErrorCode "err". N.B. the ErrorCode may very well be a
  // conditional expression; if so, this function will recursively call
  // CondExpression() and possibly RetExpression() to build a complex set of
  // instructions.
  Instruction* RetExpression(const ErrorCode& err);

  // Returns a BPF program that evaluates the conditional expression in
  // "cond" and returns the appropriate value from the BPF filter program.
  // This function recursively calls RetExpression(); it should only ever be
  // called from RetExpression().
  Instruction* CondExpression(const ErrorCode& cond);

  // Returns a BPF program that evaluates half of a conditional expression;
  // it should only ever be called from CondExpression().
  Instruction* CondExpressionHalf(const ErrorCode& cond,
                                  ArgHalf half,
                                  Instruction* passed,
                                  Instruction* failed);

  // MakeTrap is the common implementation for Trap and UnsafeTrap.
  ErrorCode MakeTrap(TrapRegistry::TrapFnc fnc, const void* aux, bool safe);

  const Policy* policy_;
  TrapRegistry* registry_;

  Conds conds_;
  CodeGen gen_;
  bool has_unsafe_traps_;

  DISALLOW_COPY_AND_ASSIGN(PolicyCompiler);
};

}  // namespace bpf_dsl
}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_POLICY_COMPILER_H_
