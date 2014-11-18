// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"
#include "sandbox/linux/seccomp-bpf/syscall_iterator.h"
#include "sandbox/linux/seccomp-bpf/verifier.h"


namespace sandbox {

namespace {

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

uint32_t EvaluateErrorCode(SandboxBPF* sandbox,
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
      return sandbox->Unexpected64bitArgument().err();
    }
    switch (code.op()) {
      case ErrorCode::OP_EQUAL:
        return EvaluateErrorCode(sandbox,
                                 (code.width() == ErrorCode::TP_32BIT
                                      ? uint32_t(data.args[code.argno()])
                                      : data.args[code.argno()]) == code.value()
                                     ? *code.passed()
                                     : *code.failed(),
                                 data);
      case ErrorCode::OP_HAS_ALL_BITS:
        return EvaluateErrorCode(sandbox,
                                 ((code.width() == ErrorCode::TP_32BIT
                                       ? uint32_t(data.args[code.argno()])
                                       : data.args[code.argno()]) &
                                  code.value()) == code.value()
                                     ? *code.passed()
                                     : *code.failed(),
                                 data);
      case ErrorCode::OP_HAS_ANY_BITS:
        return EvaluateErrorCode(sandbox,
                                 (code.width() == ErrorCode::TP_32BIT
                                      ? uint32_t(data.args[code.argno()])
                                      : data.args[code.argno()]) &
                                         code.value()
                                     ? *code.passed()
                                     : *code.failed(),
                                 data);
      default:
        return SECCOMP_RET_INVALID;
    }
  } else {
    return SECCOMP_RET_INVALID;
  }
}

bool VerifyErrorCode(SandboxBPF* sandbox,
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
    } else if (computed_ret != EvaluateErrorCode(sandbox, root_code, *data)) {
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
    switch (code.op()) {
      case ErrorCode::OP_EQUAL:
        // Verify that we can check a 32bit value (or the LSB of a 64bit value)
        // for equality.
        data->args[code.argno()] = code.value();
        if (!VerifyErrorCode(
                 sandbox, program, data, root_code, *code.passed(), err)) {
          return false;
        }

        // Change the value to no longer match and verify that this is detected
        // as an inequality.
        data->args[code.argno()] = code.value() ^ 0x55AA55AA;
        if (!VerifyErrorCode(
                 sandbox, program, data, root_code, *code.failed(), err)) {
          return false;
        }

        // BPF programs can only ever operate on 32bit values. So, we have
        // generated additional BPF instructions that inspect the MSB. Verify
        // that they behave as intended.
        if (code.width() == ErrorCode::TP_32BIT) {
          if (code.value() >> 32) {
            SANDBOX_DIE(
                "Invalid comparison of a 32bit system call argument "
                "against a 64bit constant; this test is always false.");
          }

          // If the system call argument was intended to be a 32bit parameter,
          // verify that it is a fatal error if a 64bit value is ever passed
          // here.
          data->args[code.argno()] = 0x100000000ull;
          if (!VerifyErrorCode(sandbox,
                               program,
                               data,
                               root_code,
                               sandbox->Unexpected64bitArgument(),
                               err)) {
            return false;
          }
        } else {
          // If the system call argument was intended to be a 64bit parameter,
          // verify that we can handle (in-)equality for the MSB. This is
          // essentially the same test that we did earlier for the LSB.
          // We only need to verify the behavior of the inequality test. We
          // know that the equality test already passed, as unlike the kernel
          // the Verifier does operate on 64bit quantities.
          data->args[code.argno()] = code.value() ^ 0x55AA55AA00000000ull;
          if (!VerifyErrorCode(
                   sandbox, program, data, root_code, *code.failed(), err)) {
            return false;
          }
        }
        break;
      case ErrorCode::OP_HAS_ALL_BITS:
      case ErrorCode::OP_HAS_ANY_BITS:
        // A comprehensive test of bit values is difficult and potentially
        // rather
        // time-expensive. We avoid doing so at run-time and instead rely on the
        // unittest for full testing. The test that we have here covers just the
        // common cases. We test against the bitmask itself, all zeros and all
        // ones.
        {
          // Testing "any" bits against a zero mask is always false. So, there
          // are some cases, where we expect tests to take the "failed()" branch
          // even though this is a test that normally should take "passed()".
          const ErrorCode& passed =
              (!code.value() && code.op() == ErrorCode::OP_HAS_ANY_BITS) ||

                      // On a 32bit system, it is impossible to pass a 64bit
                      // value as a
                      // system call argument. So, some additional tests always
                      // evaluate
                      // as false.
                      ((code.value() & ~uint64_t(uintptr_t(-1))) &&
                       code.op() == ErrorCode::OP_HAS_ALL_BITS) ||
                      (code.value() && !(code.value() & uintptr_t(-1)) &&
                       code.op() == ErrorCode::OP_HAS_ANY_BITS)
                  ? *code.failed()
                  : *code.passed();

          // Similary, testing for "all" bits in a zero mask is always true. So,
          // some cases pass despite them normally failing.
          const ErrorCode& failed =
              !code.value() && code.op() == ErrorCode::OP_HAS_ALL_BITS
                  ? *code.passed()
                  : *code.failed();

          data->args[code.argno()] = code.value() & uintptr_t(-1);
          if (!VerifyErrorCode(
                   sandbox, program, data, root_code, passed, err)) {
            return false;
          }
          data->args[code.argno()] = uintptr_t(-1);
          if (!VerifyErrorCode(
                   sandbox, program, data, root_code, passed, err)) {
            return false;
          }
          data->args[code.argno()] = 0;
          if (!VerifyErrorCode(
                   sandbox, program, data, root_code, failed, err)) {
            return false;
          }
        }
        break;
      default:  // TODO(markus): Need to add support for OP_GREATER
        *err = "Unsupported operation in conditional error code";
        return false;
    }
  } else {
    *err = "Attempting to return invalid error code from BPF program";
    return false;
  }
  return true;
}

void Ld(State* state, const struct sock_filter& insn, const char** err) {
  if (BPF_SIZE(insn.code) != BPF_W || BPF_MODE(insn.code) != BPF_ABS) {
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

bool Verifier::VerifyBPF(SandboxBPF* sandbox,
                         const std::vector<struct sock_filter>& program,
                         const SandboxBPFPolicy& policy,
                         const char** err) {
  *err = NULL;
  for (SyscallIterator iter(false); !iter.Done();) {
    uint32_t sysnum = iter.Next();
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
    ErrorCode code = iter.IsValid(sysnum)
                         ? policy.EvaluateSyscall(sandbox, sysnum)
                         : policy.InvalidSyscall(sandbox);
    if (!VerifyErrorCode(sandbox, program, &data, code, code, err)) {
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
