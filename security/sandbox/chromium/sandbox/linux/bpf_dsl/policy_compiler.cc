// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/bpf_dsl/policy_compiler.h"

#include <errno.h>
#include <linux/filter.h>
#include <sys/syscall.h>

#include <limits>

#include "base/logging.h"
#include "base/macros.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_impl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf/codegen.h"
#include "sandbox/linux/seccomp-bpf/die.h"
#include "sandbox/linux/seccomp-bpf/errorcode.h"
#include "sandbox/linux/seccomp-bpf/instruction.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/seccomp-bpf/syscall_iterator.h"

namespace sandbox {
namespace bpf_dsl {

namespace {

#if defined(__i386__) || defined(__x86_64__)
const bool kIsIntel = true;
#else
const bool kIsIntel = false;
#endif
#if defined(__x86_64__) && defined(__ILP32__)
const bool kIsX32 = true;
#else
const bool kIsX32 = false;
#endif

const int kSyscallsRequiredForUnsafeTraps[] = {
    __NR_rt_sigprocmask,
    __NR_rt_sigreturn,
#if defined(__NR_sigprocmask)
    __NR_sigprocmask,
#endif
#if defined(__NR_sigreturn)
    __NR_sigreturn,
#endif
};

bool HasExactlyOneBit(uint64_t x) {
  // Common trick; e.g., see http://stackoverflow.com/a/108329.
  return x != 0 && (x & (x - 1)) == 0;
}

bool IsDenied(const ErrorCode& code) {
  return (code.err() & SECCOMP_RET_ACTION) == SECCOMP_RET_TRAP ||
         (code.err() >= (SECCOMP_RET_ERRNO + ErrorCode::ERR_MIN_ERRNO) &&
          code.err() <= (SECCOMP_RET_ERRNO + ErrorCode::ERR_MAX_ERRNO));
}

// A Trap() handler that returns an "errno" value. The value is encoded
// in the "aux" parameter.
intptr_t ReturnErrno(const struct arch_seccomp_data&, void* aux) {
  // TrapFnc functions report error by following the native kernel convention
  // of returning an exit code in the range of -1..-4096. They do not try to
  // set errno themselves. The glibc wrapper that triggered the SIGSYS will
  // ultimately do so for us.
  int err = reinterpret_cast<intptr_t>(aux) & SECCOMP_RET_DATA;
  return -err;
}

intptr_t BPFFailure(const struct arch_seccomp_data&, void* aux) {
  SANDBOX_DIE(static_cast<char*>(aux));
}

bool HasUnsafeTraps(const Policy* policy) {
  for (uint32_t sysnum : SyscallSet::ValidOnly()) {
    if (policy->EvaluateSyscall(sysnum)->HasUnsafeTraps()) {
      return true;
    }
  }
  return policy->InvalidSyscall()->HasUnsafeTraps();
}

}  // namespace

struct PolicyCompiler::Range {
  Range(uint32_t f, const ErrorCode& e) : from(f), err(e) {}
  uint32_t from;
  ErrorCode err;
};

PolicyCompiler::PolicyCompiler(const Policy* policy, TrapRegistry* registry)
    : policy_(policy),
      registry_(registry),
      conds_(),
      gen_(),
      has_unsafe_traps_(HasUnsafeTraps(policy_)) {
}

PolicyCompiler::~PolicyCompiler() {
}

scoped_ptr<CodeGen::Program> PolicyCompiler::Compile() {
  if (!IsDenied(policy_->InvalidSyscall()->Compile(this))) {
    SANDBOX_DIE("Policies should deny invalid system calls.");
  }

  // If our BPF program has unsafe traps, enable support for them.
  if (has_unsafe_traps_) {
    // As support for unsafe jumps essentially defeats all the security
    // measures that the sandbox provides, we print a big warning message --
    // and of course, we make sure to only ever enable this feature if it
    // is actually requested by the sandbox policy.
    if (Syscall::Call(-1) == -1 && errno == ENOSYS) {
      SANDBOX_DIE(
          "Support for UnsafeTrap() has not yet been ported to this "
          "architecture");
    }

    for (int sysnum : kSyscallsRequiredForUnsafeTraps) {
      if (!policy_->EvaluateSyscall(sysnum)->Compile(this)
               .Equals(ErrorCode(ErrorCode::ERR_ALLOWED))) {
        SANDBOX_DIE(
            "Policies that use UnsafeTrap() must unconditionally allow all "
            "required system calls");
      }
    }

    if (!registry_->EnableUnsafeTraps()) {
      // We should never be able to get here, as UnsafeTrap() should never
      // actually return a valid ErrorCode object unless the user set the
      // CHROME_SANDBOX_DEBUGGING environment variable; and therefore,
      // "has_unsafe_traps" would always be false. But better double-check
      // than enabling dangerous code.
      SANDBOX_DIE("We'd rather die than enable unsafe traps");
    }
  }

  // Assemble the BPF filter program.
  scoped_ptr<CodeGen::Program> program(new CodeGen::Program());
  gen_.Compile(AssemblePolicy(), program.get());
  return program.Pass();
}

Instruction* PolicyCompiler::AssemblePolicy() {
  // A compiled policy consists of three logical parts:
  //   1. Check that the "arch" field matches the expected architecture.
  //   2. If the policy involves unsafe traps, check if the syscall was
  //      invoked by Syscall::Call, and then allow it unconditionally.
  //   3. Check the system call number and jump to the appropriate compiled
  //      system call policy number.
  return CheckArch(MaybeAddEscapeHatch(DispatchSyscall()));
}

Instruction* PolicyCompiler::CheckArch(Instruction* passed) {
  // If the architecture doesn't match SECCOMP_ARCH, disallow the
  // system call.
  return gen_.MakeInstruction(
      BPF_LD + BPF_W + BPF_ABS,
      SECCOMP_ARCH_IDX,
      gen_.MakeInstruction(
          BPF_JMP + BPF_JEQ + BPF_K,
          SECCOMP_ARCH,
          passed,
          RetExpression(Kill("Invalid audit architecture in BPF filter"))));
}

Instruction* PolicyCompiler::MaybeAddEscapeHatch(Instruction* rest) {
  // If no unsafe traps, then simply return |rest|.
  if (!has_unsafe_traps_) {
    return rest;
  }

  // Allow system calls, if they originate from our magic return address
  // (which we can query by calling Syscall::Call(-1)).
  uint64_t syscall_entry_point =
      static_cast<uint64_t>(static_cast<uintptr_t>(Syscall::Call(-1)));
  uint32_t low = static_cast<uint32_t>(syscall_entry_point);
  uint32_t hi = static_cast<uint32_t>(syscall_entry_point >> 32);

  // BPF cannot do native 64-bit comparisons, so we have to compare
  // both 32-bit halves of the instruction pointer. If they match what
  // we expect, we return ERR_ALLOWED. If either or both don't match,
  // we continue evalutating the rest of the sandbox policy.
  //
  // For simplicity, we check the full 64-bit instruction pointer even
  // on 32-bit architectures.
  return gen_.MakeInstruction(
      BPF_LD + BPF_W + BPF_ABS,
      SECCOMP_IP_LSB_IDX,
      gen_.MakeInstruction(
          BPF_JMP + BPF_JEQ + BPF_K,
          low,
          gen_.MakeInstruction(
              BPF_LD + BPF_W + BPF_ABS,
              SECCOMP_IP_MSB_IDX,
              gen_.MakeInstruction(
                  BPF_JMP + BPF_JEQ + BPF_K,
                  hi,
                  RetExpression(ErrorCode(ErrorCode::ERR_ALLOWED)),
                  rest)),
          rest));
}

Instruction* PolicyCompiler::DispatchSyscall() {
  // Evaluate all possible system calls and group their ErrorCodes into
  // ranges of identical codes.
  Ranges ranges;
  FindRanges(&ranges);

  // Compile the system call ranges to an optimized BPF jumptable
  Instruction* jumptable = AssembleJumpTable(ranges.begin(), ranges.end());

  // Grab the system call number, so that we can check it and then
  // execute the jump table.
  return gen_.MakeInstruction(
      BPF_LD + BPF_W + BPF_ABS, SECCOMP_NR_IDX, CheckSyscallNumber(jumptable));
}

Instruction* PolicyCompiler::CheckSyscallNumber(Instruction* passed) {
  if (kIsIntel) {
    // On Intel architectures, verify that system call numbers are in the
    // expected number range.
    Instruction* invalidX32 =
        RetExpression(Kill("Illegal mixing of system call ABIs"));
    if (kIsX32) {
      // The newer x32 API always sets bit 30.
      return gen_.MakeInstruction(
          BPF_JMP + BPF_JSET + BPF_K, 0x40000000, passed, invalidX32);
    } else {
      // The older i386 and x86-64 APIs clear bit 30 on all system calls.
      return gen_.MakeInstruction(
          BPF_JMP + BPF_JSET + BPF_K, 0x40000000, invalidX32, passed);
    }
  }

  // TODO(mdempsky): Similar validation for other architectures?
  return passed;
}

void PolicyCompiler::FindRanges(Ranges* ranges) {
  // Please note that "struct seccomp_data" defines system calls as a signed
  // int32_t, but BPF instructions always operate on unsigned quantities. We
  // deal with this disparity by enumerating from MIN_SYSCALL to MAX_SYSCALL,
  // and then verifying that the rest of the number range (both positive and
  // negative) all return the same ErrorCode.
  const ErrorCode invalid_err = policy_->InvalidSyscall()->Compile(this);
  uint32_t old_sysnum = 0;
  ErrorCode old_err = SyscallSet::IsValid(old_sysnum)
                          ? policy_->EvaluateSyscall(old_sysnum)->Compile(this)
                          : invalid_err;

  for (uint32_t sysnum : SyscallSet::All()) {
    ErrorCode err =
        SyscallSet::IsValid(sysnum)
            ? policy_->EvaluateSyscall(static_cast<int>(sysnum))->Compile(this)
            : invalid_err;
    if (!err.Equals(old_err)) {
      ranges->push_back(Range(old_sysnum, old_err));
      old_sysnum = sysnum;
      old_err = err;
    }
  }
  ranges->push_back(Range(old_sysnum, old_err));
}

Instruction* PolicyCompiler::AssembleJumpTable(Ranges::const_iterator start,
                                               Ranges::const_iterator stop) {
  // We convert the list of system call ranges into jump table that performs
  // a binary search over the ranges.
  // As a sanity check, we need to have at least one distinct ranges for us
  // to be able to build a jump table.
  if (stop - start <= 0) {
    SANDBOX_DIE("Invalid set of system call ranges");
  } else if (stop - start == 1) {
    // If we have narrowed things down to a single range object, we can
    // return from the BPF filter program.
    return RetExpression(start->err);
  }

  // Pick the range object that is located at the mid point of our list.
  // We compare our system call number against the lowest valid system call
  // number in this range object. If our number is lower, it is outside of
  // this range object. If it is greater or equal, it might be inside.
  Ranges::const_iterator mid = start + (stop - start) / 2;

  // Sub-divide the list of ranges and continue recursively.
  Instruction* jf = AssembleJumpTable(start, mid);
  Instruction* jt = AssembleJumpTable(mid, stop);
  return gen_.MakeInstruction(BPF_JMP + BPF_JGE + BPF_K, mid->from, jt, jf);
}

Instruction* PolicyCompiler::RetExpression(const ErrorCode& err) {
  switch (err.error_type()) {
    case ErrorCode::ET_COND:
      return CondExpression(err);
    case ErrorCode::ET_SIMPLE:
    case ErrorCode::ET_TRAP:
      return gen_.MakeInstruction(BPF_RET + BPF_K, err.err());
    default:
      SANDBOX_DIE("ErrorCode is not suitable for returning from a BPF program");
  }
}

Instruction* PolicyCompiler::CondExpression(const ErrorCode& cond) {
  // Sanity check that |cond| makes sense.
  if (cond.argno_ < 0 || cond.argno_ >= 6) {
    SANDBOX_DIE("sandbox_bpf: invalid argument number");
  }
  if (cond.width_ != ErrorCode::TP_32BIT &&
      cond.width_ != ErrorCode::TP_64BIT) {
    SANDBOX_DIE("sandbox_bpf: invalid argument width");
  }
  if (cond.mask_ == 0) {
    SANDBOX_DIE("sandbox_bpf: zero mask is invalid");
  }
  if ((cond.value_ & cond.mask_) != cond.value_) {
    SANDBOX_DIE("sandbox_bpf: value contains masked out bits");
  }
  if (cond.width_ == ErrorCode::TP_32BIT &&
      ((cond.mask_ >> 32) != 0 || (cond.value_ >> 32) != 0)) {
    SANDBOX_DIE("sandbox_bpf: test exceeds argument size");
  }
  // TODO(mdempsky): Reject TP_64BIT on 32-bit platforms. For now we allow it
  // because some SandboxBPF unit tests exercise it.

  Instruction* passed = RetExpression(*cond.passed_);
  Instruction* failed = RetExpression(*cond.failed_);

  // We want to emit code to check "(arg & mask) == value" where arg, mask, and
  // value are 64-bit values, but the BPF machine is only 32-bit. We implement
  // this by independently testing the upper and lower 32-bits and continuing to
  // |passed| if both evaluate true, or to |failed| if either evaluate false.
  return CondExpressionHalf(cond,
                            UpperHalf,
                            CondExpressionHalf(cond, LowerHalf, passed, failed),
                            failed);
}

Instruction* PolicyCompiler::CondExpressionHalf(const ErrorCode& cond,
                                                ArgHalf half,
                                                Instruction* passed,
                                                Instruction* failed) {
  if (cond.width_ == ErrorCode::TP_32BIT && half == UpperHalf) {
    // Special logic for sanity checking the upper 32-bits of 32-bit system
    // call arguments.

    // TODO(mdempsky): Compile Unexpected64bitArgument() just per program.
    Instruction* invalid_64bit = RetExpression(Unexpected64bitArgument());

    const uint32_t upper = SECCOMP_ARG_MSB_IDX(cond.argno_);
    const uint32_t lower = SECCOMP_ARG_LSB_IDX(cond.argno_);

    if (sizeof(void*) == 4) {
      // On 32-bit platforms, the upper 32-bits should always be 0:
      //   LDW  [upper]
      //   JEQ  0, passed, invalid
      return gen_.MakeInstruction(
          BPF_LD + BPF_W + BPF_ABS,
          upper,
          gen_.MakeInstruction(
              BPF_JMP + BPF_JEQ + BPF_K, 0, passed, invalid_64bit));
    }

    // On 64-bit platforms, the upper 32-bits may be 0 or ~0; but we only allow
    // ~0 if the sign bit of the lower 32-bits is set too:
    //   LDW  [upper]
    //   JEQ  0, passed, (next)
    //   JEQ  ~0, (next), invalid
    //   LDW  [lower]
    //   JSET (1<<31), passed, invalid
    //
    // TODO(mdempsky): The JSET instruction could perhaps jump to passed->next
    // instead, as the first instruction of passed should be "LDW [lower]".
    return gen_.MakeInstruction(
        BPF_LD + BPF_W + BPF_ABS,
        upper,
        gen_.MakeInstruction(
            BPF_JMP + BPF_JEQ + BPF_K,
            0,
            passed,
            gen_.MakeInstruction(
                BPF_JMP + BPF_JEQ + BPF_K,
                std::numeric_limits<uint32_t>::max(),
                gen_.MakeInstruction(
                    BPF_LD + BPF_W + BPF_ABS,
                    lower,
                    gen_.MakeInstruction(BPF_JMP + BPF_JSET + BPF_K,
                                         1U << 31,
                                         passed,
                                         invalid_64bit)),
                invalid_64bit)));
  }

  const uint32_t idx = (half == UpperHalf) ? SECCOMP_ARG_MSB_IDX(cond.argno_)
                                           : SECCOMP_ARG_LSB_IDX(cond.argno_);
  const uint32_t mask = (half == UpperHalf) ? cond.mask_ >> 32 : cond.mask_;
  const uint32_t value = (half == UpperHalf) ? cond.value_ >> 32 : cond.value_;

  // Emit a suitable instruction sequence for (arg & mask) == value.

  // For (arg & 0) == 0, just return passed.
  if (mask == 0) {
    CHECK_EQ(0U, value);
    return passed;
  }

  // For (arg & ~0) == value, emit:
  //   LDW  [idx]
  //   JEQ  value, passed, failed
  if (mask == std::numeric_limits<uint32_t>::max()) {
    return gen_.MakeInstruction(
        BPF_LD + BPF_W + BPF_ABS,
        idx,
        gen_.MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, value, passed, failed));
  }

  // For (arg & mask) == 0, emit:
  //   LDW  [idx]
  //   JSET mask, failed, passed
  // (Note: failed and passed are intentionally swapped.)
  if (value == 0) {
    return gen_.MakeInstruction(
        BPF_LD + BPF_W + BPF_ABS,
        idx,
        gen_.MakeInstruction(BPF_JMP + BPF_JSET + BPF_K, mask, failed, passed));
  }

  // For (arg & x) == x where x is a single-bit value, emit:
  //   LDW  [idx]
  //   JSET mask, passed, failed
  if (mask == value && HasExactlyOneBit(mask)) {
    return gen_.MakeInstruction(
        BPF_LD + BPF_W + BPF_ABS,
        idx,
        gen_.MakeInstruction(BPF_JMP + BPF_JSET + BPF_K, mask, passed, failed));
  }

  // Generic fallback:
  //   LDW  [idx]
  //   AND  mask
  //   JEQ  value, passed, failed
  return gen_.MakeInstruction(
      BPF_LD + BPF_W + BPF_ABS,
      idx,
      gen_.MakeInstruction(
          BPF_ALU + BPF_AND + BPF_K,
          mask,
          gen_.MakeInstruction(
              BPF_JMP + BPF_JEQ + BPF_K, value, passed, failed)));
}

ErrorCode PolicyCompiler::Unexpected64bitArgument() {
  return Kill("Unexpected 64bit argument detected");
}

ErrorCode PolicyCompiler::Error(int err) {
  if (has_unsafe_traps_) {
    // When inside an UnsafeTrap() callback, we want to allow all system calls.
    // This means, we must conditionally disable the sandbox -- and that's not
    // something that kernel-side BPF filters can do, as they cannot inspect
    // any state other than the syscall arguments.
    // But if we redirect all error handlers to user-space, then we can easily
    // make this decision.
    // The performance penalty for this extra round-trip to user-space is not
    // actually that bad, as we only ever pay it for denied system calls; and a
    // typical program has very few of these.
    return Trap(ReturnErrno, reinterpret_cast<void*>(err));
  }

  return ErrorCode(err);
}

ErrorCode PolicyCompiler::MakeTrap(TrapRegistry::TrapFnc fnc,
                                   const void* aux,
                                   bool safe) {
  uint16_t trap_id = registry_->Add(fnc, aux, safe);
  return ErrorCode(trap_id, fnc, aux, safe);
}

ErrorCode PolicyCompiler::Trap(TrapRegistry::TrapFnc fnc, const void* aux) {
  return MakeTrap(fnc, aux, true /* Safe Trap */);
}

ErrorCode PolicyCompiler::UnsafeTrap(TrapRegistry::TrapFnc fnc,
                                     const void* aux) {
  return MakeTrap(fnc, aux, false /* Unsafe Trap */);
}

bool PolicyCompiler::IsRequiredForUnsafeTrap(int sysno) {
  for (size_t i = 0; i < arraysize(kSyscallsRequiredForUnsafeTraps); ++i) {
    if (sysno == kSyscallsRequiredForUnsafeTraps[i]) {
      return true;
    }
  }
  return false;
}

ErrorCode PolicyCompiler::CondMaskedEqual(int argno,
                                          ErrorCode::ArgType width,
                                          uint64_t mask,
                                          uint64_t value,
                                          const ErrorCode& passed,
                                          const ErrorCode& failed) {
  return ErrorCode(argno,
                   width,
                   mask,
                   value,
                   &*conds_.insert(passed).first,
                   &*conds_.insert(failed).first);
}

ErrorCode PolicyCompiler::Kill(const char* msg) {
  return Trap(BPFFailure, const_cast<char*>(msg));
}

}  // namespace bpf_dsl
}  // namespace sandbox
