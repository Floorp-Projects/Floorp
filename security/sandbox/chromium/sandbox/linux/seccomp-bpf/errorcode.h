// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_ERRORCODE_H__
#define SANDBOX_LINUX_SECCOMP_BPF_ERRORCODE_H__

#include "sandbox/linux/seccomp-bpf/trap.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
namespace bpf_dsl {
class PolicyCompiler;
}

// This class holds all the possible values that can be returned by a sandbox
// policy.
// We can either wrap a symbolic ErrorCode (i.e. ERR_XXX enum values), an
// errno value (in the range 0..4095), a pointer to a TrapFnc callback
// handling a SECCOMP_RET_TRAP trap, or a complex constraint.
// All of the commonly used values are stored in the "err_" field. So, code
// that is using the ErrorCode class typically operates on a single 32bit
// field.
class SANDBOX_EXPORT ErrorCode {
 public:
  enum {
    // Allow this system call. The value of ERR_ALLOWED is pretty much
    // completely arbitrary. But we want to pick it so that is is unlikely
    // to be passed in accidentally, when the user intended to return an
    // "errno" (see below) value instead.
    ERR_ALLOWED = 0x04000000,

    // If the progress is being ptraced with PTRACE_O_TRACESECCOMP, then the
    // tracer will be notified of a PTRACE_EVENT_SECCOMP and allowed to change
    // or skip the system call.  The lower 16 bits of err will be available to
    // the tracer via PTRACE_GETEVENTMSG.
    ERR_TRACE   = 0x08000000,

    // Deny the system call with a particular "errno" value.
    // N.B.: It is also possible to return "0" here. That would normally
    //       indicate success, but it won't actually run the system call.
    //       This is very different from return ERR_ALLOWED.
    ERR_MIN_ERRNO = 0,
#if defined(__mips__)
    // MIPS only supports errno up to 1133
    ERR_MAX_ERRNO = 1133,
#else
    // TODO(markus): Android only supports errno up to 255
    // (crbug.com/181647).
    ERR_MAX_ERRNO = 4095,
#endif
  };

  // While BPF filter programs always operate on 32bit quantities, the kernel
  // always sees system call arguments as 64bit values. This statement is true
  // no matter whether the host system is natively operating in 32bit or 64bit.
  // The BPF compiler hides the fact that BPF instructions cannot directly
  // access 64bit quantities. But policies are still advised to specify whether
  // a system call expects a 32bit or a 64bit quantity.
  enum ArgType {
    // When passed as an argument to SandboxBPF::Cond(), TP_32BIT requests that
    // the conditional test should operate on the 32bit part of the system call
    // argument.
    // On 64bit architectures, this verifies that user space did not pass
    // a 64bit value as an argument to the system call. If it did, that will be
    // interpreted as an attempt at breaking the sandbox and results in the
    // program getting terminated.
    // In other words, only perform a 32bit test, if you are sure this
    // particular system call would never legitimately take a 64bit
    // argument.
    // Implementation detail: TP_32BIT does two things. 1) it restricts the
    // conditional test to operating on the LSB only, and 2) it adds code to
    // the BPF filter program verifying that the MSB  the kernel received from
    // user space is either 0, or 0xFFFFFFFF; the latter is acceptable, iff bit
    // 31 was set in the system call argument. It deals with 32bit arguments
    // having been sign extended.
    TP_32BIT,

    // When passed as an argument to SandboxBPF::Cond(), TP_64BIT requests that
    // the conditional test should operate on the full 64bit argument. It is
    // generally harmless to perform a 64bit test on 32bit systems, as the
    // kernel will always see the top 32 bits of all arguments as zero'd out.
    // This approach has the desirable property that for tests of pointer
    // values, we can always use TP_64BIT no matter the host architecture.
    // But of course, that also means, it is possible to write conditional
    // policies that turn into no-ops on 32bit systems; this is by design.
    TP_64BIT,
  };

  // Deprecated.
  enum Operation {
    // Test whether the system call argument is equal to the operand.
    OP_EQUAL,

    // Tests a system call argument against a bit mask.
    // The "ALL_BITS" variant performs this test: "arg & mask == mask"
    // This implies that a mask of zero always results in a passing test.
    // The "ANY_BITS" variant performs this test: "arg & mask != 0"
    // This implies that a mask of zero always results in a failing test.
    OP_HAS_ALL_BITS,
    OP_HAS_ANY_BITS,
  };

  enum ErrorType {
    ET_INVALID,
    ET_SIMPLE,
    ET_TRAP,
    ET_COND,
  };

  // We allow the default constructor, as it makes the ErrorCode class
  // much easier to use. But if we ever encounter an invalid ErrorCode
  // when compiling a BPF filter, we deliberately generate an invalid
  // program that will get flagged both by our Verifier class and by
  // the Linux kernel.
  ErrorCode();
  explicit ErrorCode(int err);

  // For all practical purposes, ErrorCodes are treated as if they were
  // structs. The copy constructor and assignment operator are trivial and
  // we do not need to explicitly specify them.
  // Most notably, it is in fact perfectly OK to directly copy the passed_ and
  // failed_ field. They only ever get set by our private constructor, and the
  // callers handle life-cycle management for these objects.

  // Destructor
  ~ErrorCode() {}

  bool Equals(const ErrorCode& err) const;
  bool LessThan(const ErrorCode& err) const;

  uint32_t err() const { return err_; }
  ErrorType error_type() const { return error_type_; }

  bool safe() const { return safe_; }

  uint64_t mask() const { return mask_; }
  uint64_t value() const { return value_; }
  int argno() const { return argno_; }
  ArgType width() const { return width_; }
  const ErrorCode* passed() const { return passed_; }
  const ErrorCode* failed() const { return failed_; }

  struct LessThan {
    bool operator()(const ErrorCode& a, const ErrorCode& b) const {
      return a.LessThan(b);
    }
  };

 private:
  friend bpf_dsl::PolicyCompiler;
  friend class CodeGen;
  friend class SandboxBPF;
  friend class Trap;

  // If we are wrapping a callback, we must assign a unique id. This id is
  // how the kernel tells us which one of our different SECCOMP_RET_TRAP
  // cases has been triggered.
  ErrorCode(uint16_t trap_id, Trap::TrapFnc fnc, const void* aux, bool safe);

  // Some system calls require inspection of arguments. This constructor
  // allows us to specify additional constraints.
  ErrorCode(int argno,
            ArgType width,
            uint64_t mask,
            uint64_t value,
            const ErrorCode* passed,
            const ErrorCode* failed);

  ErrorType error_type_;

  union {
    // Fields needed for SECCOMP_RET_TRAP callbacks
    struct {
      Trap::TrapFnc fnc_;  // Callback function and arg, if trap was
      void* aux_;          //   triggered by the kernel's BPF filter.
      bool safe_;          // Keep sandbox active while calling fnc_()
    };

    // Fields needed when inspecting additional arguments.
    struct {
      uint64_t mask_;            // Mask that we are comparing under.
      uint64_t value_;           // Value that we are comparing with.
      int argno_;                // Syscall arg number that we are inspecting.
      ArgType width_;            // Whether we are looking at a 32/64bit value.
      const ErrorCode* passed_;  // Value to be returned if comparison passed,
      const ErrorCode* failed_;  //   or if it failed.
    };
  };

  // 32bit field used for all possible types of ErrorCode values. This is
  // the value that uniquely identifies any ErrorCode and it (typically) can
  // be emitted directly into a BPF filter program.
  uint32_t err_;
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_ERRORCODE_H__
