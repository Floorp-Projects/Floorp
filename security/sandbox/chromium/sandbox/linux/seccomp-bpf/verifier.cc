// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/verifier.h"

#include <string.h>

#include <limits>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_impl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/bpf_dsl/policy_compiler.h"
#include "sandbox/linux/seccomp-bpf/errorcode.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/syscall_iterator.h"

namespace sandbox {

namespace {

const uint64_t kLower32Bits = std::numeric_limits<uint32_t>::max();
const uint64_t kUpper32Bits = static_cast<uint64_t>(kLower32Bits) << 32;
const uint64_t kFull64Bits = std::numeric_limits<uint64_t>::max();

struct State {
  State(const std::vector<struct sock_filter>& p,
        const struct arch_seccomp_data& d)
      : program(p), data(d), ip(0), accumulator(0), acc_is_valid(false) {}
  const std::vector<struct sock_filter>& program;
  const struct arch_seccomp_data& data;
  unsigned int ip;
  uint32_t accumulator;
  bool acc_is_valid;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(State);
};

uint32_t EvaluateErrorCode(bpf_dsl::PolicyCompiler* compiler,
                           const ErrorCode& code,
                           const struct arch_seccomp_data& data) {
  if (code.error_type() == ErrorCode::ET_SIMPLE ||
      code.error_type() == ErrorCode::ET_TRAP) {
    return code.err();
  } else if (code.error_type() == ErrorCode::ET_COND) {
    if (code.width() == ErrorCode::TP_32BIT &&
        (data.args[code.argno()] >> 32) &&
        (data.args[code.argno()] & 0xFFFFFFFF80000000ull) !=
            0xFFFFFFFF80000000ull) {
      return compiler->Unexpected64bitArgument().err();
    }
    bool equal = (data.args[code.argno()] & code.mask()) == code.value();
    return EvaluateErrorCode(
        compiler, equal ? *code.passed() : *code.failed(), data);
  } else {
    return SECCOMP_RET_INVALID;
  }
}

bool VerifyErrorCode(bpf_dsl::PolicyCompiler* compiler,
                     const std::vector<struct sock_filter>& program,
                     struct arch_seccomp_data* data,
                     const ErrorCode& root_code,
                     const ErrorCode& code,
                     const char** err) {
  if (code.error_type() == ErrorCode::ET_SIMPLE ||
      code.error_type() == ErrorCode::ET_TRAP) {
    uint32_t computed_ret = Verifier::EvaluateBPF(program, *data, err);
    if (*err) {
      return false;
    } else if (computed_ret != EvaluateErrorCode(compiler, root_code, *data)) {
      // For efficiency's sake, we'd much rather compare "computed_ret"
      // against "code.err()". This works most of the time, but it doesn't
      // always work for nested conditional expressions. The test values
      // that we generate on the fly to probe expressions can trigger
      // code flow decisions in multiple nodes of the decision tree, and the
      // only way to compute the correct error code in that situation is by
      // calling EvaluateErrorCode().
      *err = "Exit code from BPF program doesn't match";
      return false;
    }
  } else if (code.error_type() == ErrorCode::ET_COND) {
    if (code.argno() < 0 || code.argno() >= 6) {
      *err = "Invalid argument number in error code";
      return false;
    }

    // TODO(mdempsky): The test values generated here try to provide good
    // coverage for generated BPF instructions while avoiding combinatorial
    // explosion on large policies. Ideally we would instead take a fuzzing-like
    // approach and generate a bounded number of test cases regardless of policy
    // size.

    // Verify that we can check a value for simple equality.
    data->args[code.argno()] = code.value();
    if (!VerifyErrorCode(
            compiler, program, data, root_code, *code.passed(), err)) {
      return false;
    }

    // If mask ignores any bits, verify that setting those bits is still
    // detected as equality.
    uint64_t ignored_bits = ~code.mask();
    if (code.width() == ErrorCode::TP_32BIT) {
      ignored_bits = static_cast<uint32_t>(ignored_bits);
    }
    if ((ignored_bits & kLower32Bits) != 0) {
      data->args[code.argno()] = code.value() | (ignored_bits & kLower32Bits);
      if (!VerifyErrorCode(
              compiler, program, data, root_code, *code.passed(), err)) {
        return false;
      }
    }
    if ((ignored_bits & kUpper32Bits) != 0) {
      data->args[code.argno()] = code.value() | (ignored_bits & kUpper32Bits);
      if (!VerifyErrorCode(
              compiler, program, data, root_code, *code.passed(), err)) {
        return false;
      }
    }

    // Verify that changing bits included in the mask is detected as inequality.
    if ((code.mask() & kLower32Bits) != 0) {
      data->args[code.argno()] = code.value() ^ (code.mask() & kLower32Bits);
      if (!VerifyErrorCode(
              compiler, program, data, root_code, *code.failed(), err)) {
        return false;
      }
    }
    if ((code.mask() & kUpper32Bits) != 0) {
      data->args[code.argno()] = code.value() ^ (code.mask() & kUpper32Bits);
      if (!VerifyErrorCode(
              compiler, program, data, root_code, *code.failed(), err)) {
        return false;
      }
    }

    if (code.width() == ErrorCode::TP_32BIT) {
      // For 32-bit system call arguments, we emit additional instructions to
      // validate the upper 32-bits. Here we test that validation.

      // Arbitrary 64-bit values should be rejected.
      data->args[code.argno()] = 1ULL << 32;
      if (!VerifyErrorCode(compiler,
                           program,
                           data,
                           root_code,
                           compiler->Unexpected64bitArgument(),
                           err)) {
        return false;
      }

      // Upper 32-bits set without the MSB of the lower 32-bits set should be
      // rejected too.
      data->args[code.argno()] = kUpper32Bits;
      if (!VerifyErrorCode(compiler,
                           program,
                           data,
                           root_code,
                           compiler->Unexpected64bitArgument(),
                           err)) {
        return false;
      }
    }
  } else {
    *err = "Attempting to return invalid error code from BPF program";
    return false;
  }
  return true;
}

void Ld(State* state, const struct sock_filter& insn, const char** err) {
  if (BPF_SIZE(insn.code) != BPF_W || BPF_MODE(insn.code) != BPF_ABS ||
      insn.jt != 0 || insn.jf != 0) {
    *err = "Invalid BPF_LD instruction";
    return;
  }
  if (insn.k < sizeof(struct arch_seccomp_data) && (insn.k & 3) == 0) {
    // We only allow loading of properly aligned 32bit quantities.
    memcpy(&state->accumulator,
           reinterpret_cast<const char*>(&state->data) + insn.k,
           4);
  } else {
    *err = "Invalid operand in BPF_LD instruction";
    return;
  }
  state->acc_is_valid = true;
  return;
}

void Jmp(State* state, const struct sock_filter& insn, const char** err) {
  if (BPF_OP(insn.code) == BPF_JA) {
    if (state->ip + insn.k + 1 >= state->program.size() ||
        state->ip + insn.k + 1 <= state->ip) {
    compilation_failure:
      *err = "Invalid BPF_JMP instruction";
      return;
    }
    state->ip += insn.k;
  } else {
    if (BPF_SRC(insn.code) != BPF_K || !state->acc_is_valid ||
        state->ip + insn.jt + 1 >= state->program.size() ||
        state->ip + insn.jf + 1 >= state->program.size()) {
      goto compilation_failure;
    }
    switch (BPF_OP(insn.code)) {
      case BPF_JEQ:
        if (state->accumulator == insn.k) {
          state->ip += insn.jt;
        } else {
          state->ip += insn.jf;
        }
        break;
      case BPF_JGT:
        if (state->accumulator > insn.k) {
          state->ip += insn.jt;
        } else {
          state->ip += insn.jf;
        }
        break;
      case BPF_JGE:
        if (state->accumulator >= insn.k) {
          state->ip += insn.jt;
        } else {
          state->ip += insn.jf;
        }
        break;
      case BPF_JSET:
        if (state->accumulator & insn.k) {
          state->ip += insn.jt;
        } else {
          state->ip += insn.jf;
        }
        break;
      default:
        goto compilation_failure;
    }
  }
}

uint32_t Ret(State*, const struct sock_filter& insn, const char** err) {
  if (BPF_SRC(insn.code) != BPF_K) {
    *err = "Invalid BPF_RET instruction";
    return 0;
  }
  return insn.k;
}

void Alu(State* state, const struct sock_filter& insn, const char** err) {
  if (BPF_OP(insn.code) == BPF_NEG) {
    state->accumulator = -state->accumulator;
    return;
  } else {
    if (BPF_SRC(insn.code) != BPF_K) {
      *err = "Unexpected source operand in arithmetic operation";
      return;
    }
    switch (BPF_OP(insn.code)) {
      case BPF_ADD:
        state->accumulator += insn.k;
        break;
      case BPF_SUB:
        state->accumulator -= insn.k;
        break;
      case BPF_MUL:
        state->accumulator *= insn.k;
        break;
      case BPF_DIV:
        if (!insn.k) {
          *err = "Illegal division by zero";
          break;
        }
        state->accumulator /= insn.k;
        break;
      case BPF_MOD:
        if (!insn.k) {
          *err = "Illegal division by zero";
          break;
        }
        state->accumulator %= insn.k;
        break;
      case BPF_OR:
        state->accumulator |= insn.k;
        break;
      case BPF_XOR:
        state->accumulator ^= insn.k;
        break;
      case BPF_AND:
        state->accumulator &= insn.k;
        break;
      case BPF_LSH:
        if (insn.k > 32) {
          *err = "Illegal shift operation";
          break;
        }
        state->accumulator <<= insn.k;
        break;
      case BPF_RSH:
        if (insn.k > 32) {
          *err = "Illegal shift operation";
          break;
        }
        state->accumulator >>= insn.k;
        break;
      default:
        *err = "Invalid operator in arithmetic operation";
        break;
    }
  }
}

}  // namespace

bool Verifier::VerifyBPF(bpf_dsl::PolicyCompiler* compiler,
                         const std::vector<struct sock_filter>& program,
                         const bpf_dsl::Policy& policy,
                         const char** err) {
  *err = NULL;
  for (uint32_t sysnum : SyscallSet::All()) {
    // We ideally want to iterate over the full system call range and values
    // just above and just below this range. This gives us the full result set
    // of the "evaluators".
    // On Intel systems, this can fail in a surprising way, as a cleared bit 30
    // indicates either i386 or x86-64; and a set bit 30 indicates x32. And
    // unless we pay attention to setting this bit correctly, an early check in
    // our BPF program will make us fail with a misleading error code.
    struct arch_seccomp_data data = {static_cast<int>(sysnum),
                                     static_cast<uint32_t>(SECCOMP_ARCH)};
#if defined(__i386__) || defined(__x86_64__)
#if defined(__x86_64__) && defined(__ILP32__)
    if (!(sysnum & 0x40000000u)) {
      continue;
    }
#else
    if (sysnum & 0x40000000u) {
      continue;
    }
#endif
#endif
    ErrorCode code = SyscallSet::IsValid(sysnum)
                         ? policy.EvaluateSyscall(sysnum)->Compile(compiler)
                         : policy.InvalidSyscall()->Compile(compiler);
    if (!VerifyErrorCode(compiler, program, &data, code, code, err)) {
      return false;
    }
  }
  return true;
}

uint32_t Verifier::EvaluateBPF(const std::vector<struct sock_filter>& program,
                               const struct arch_seccomp_data& data,
                               const char** err) {
  *err = NULL;
  if (program.size() < 1 || program.size() >= SECCOMP_MAX_PROGRAM_SIZE) {
    *err = "Invalid program length";
    return 0;
  }
  for (State state(program, data); !*err; ++state.ip) {
    if (state.ip >= program.size()) {
      *err = "Invalid instruction pointer in BPF program";
      break;
    }
    const struct sock_filter& insn = program[state.ip];
    switch (BPF_CLASS(insn.code)) {
      case BPF_LD:
        Ld(&state, insn, err);
        break;
      case BPF_JMP:
        Jmp(&state, insn, err);
        break;
      case BPF_RET: {
        uint32_t r = Ret(&state, insn, err);
        switch (r & SECCOMP_RET_ACTION) {
          case SECCOMP_RET_TRAP:
          case SECCOMP_RET_ERRNO:
          case SECCOMP_RET_TRACE:
          case SECCOMP_RET_ALLOW:
            break;
          case SECCOMP_RET_KILL:     // We don't ever generate this
          case SECCOMP_RET_INVALID:  // Should never show up in BPF program
          default:
            *err = "Unexpected return code found in BPF program";
            return 0;
        }
        return r;
      }
      case BPF_ALU:
        Alu(&state, insn, err);
        break;
      default:
        *err = "Unexpected instruction in BPF program";
        break;
    }
  }
  return 0;
}

}  // namespace sandbox
