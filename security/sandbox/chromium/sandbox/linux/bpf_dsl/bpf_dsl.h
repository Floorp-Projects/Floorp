// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_BPF_DSL_H_
#define SANDBOX_LINUX_BPF_DSL_BPF_DSL_H_

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_forward.h"
#include "sandbox/linux/bpf_dsl/cons.h"
#include "sandbox/linux/bpf_dsl/trap_registry.h"
#include "sandbox/sandbox_export.h"

// The sandbox::bpf_dsl namespace provides a domain-specific language
// to make writing BPF policies more expressive.  In general, the
// object types all have value semantics (i.e., they can be copied
// around, returned from or passed to function calls, etc. without any
// surprising side effects), though not all support assignment.
//
// An idiomatic and demonstrative (albeit silly) example of this API
// would be:
//
//      #include "sandbox/linux/bpf_dsl/bpf_dsl.h"
//
//      using namespace sandbox::bpf_dsl;
//
//      class SillyPolicy : public Policy {
//       public:
//        SillyPolicy() {}
//        virtual ~SillyPolicy() {}
//        virtual ResultExpr EvaluateSyscall(int sysno) const override {
//          if (sysno == __NR_fcntl) {
//            Arg<int> fd(0), cmd(1);
//            Arg<unsigned long> flags(2);
//            const uint64_t kGoodFlags = O_ACCMODE | O_NONBLOCK;
//            return If(fd == 0 && cmd == F_SETFL && (flags & ~kGoodFlags) == 0,
//                      Allow())
//                .ElseIf(cmd == F_DUPFD || cmd == F_DUPFD_CLOEXEC,
//                        Error(EMFILE))
//                .Else(Trap(SetFlagHandler, NULL));
//          } else {
//            return Allow();
//          }
//        }
//
//       private:
//        DISALLOW_COPY_AND_ASSIGN(SillyPolicy);
//      };
//
// More generally, the DSL currently supports the following grammar:
//
//   result = Allow() | Error(errno) | Kill(msg) | Trace(aux)
//          | Trap(trap_func, aux) | UnsafeTrap(trap_func, aux)
//          | If(bool, result)[.ElseIf(bool, result)].Else(result)
//          | Switch(arg)[.Case(val, result)].Default(result)
//   bool   = BoolConst(boolean) | !bool | bool && bool | bool || bool
//          | arg == val | arg != val
//   arg    = Arg<T>(num) | arg & mask
//
// The semantics of each function and operator are intended to be
// intuitive, but are described in more detail below.
//
// (Credit to Sean Parent's "Inheritance is the Base Class of Evil"
// talk at Going Native 2013 for promoting value semantics via shared
// pointers to immutable state.)

namespace sandbox {
namespace bpf_dsl {

// ResultExpr is an opaque reference to an immutable result expression tree.
typedef scoped_refptr<const internal::ResultExprImpl> ResultExpr;

// BoolExpr is an opaque reference to an immutable boolean expression tree.
typedef scoped_refptr<const internal::BoolExprImpl> BoolExpr;

// Allow specifies a result that the system call should be allowed to
// execute normally.
SANDBOX_EXPORT ResultExpr Allow();

// Error specifies a result that the system call should fail with
// error number |err|.  As a special case, Error(0) will result in the
// system call appearing to have succeeded, but without having any
// side effects.
SANDBOX_EXPORT ResultExpr Error(int err);

// Kill specifies a result to kill the program and print an error message.
SANDBOX_EXPORT ResultExpr Kill(const char* msg);

// Trace specifies a result to notify a tracing process via the
// PTRACE_EVENT_SECCOMP event and allow it to change or skip the system call.
// The value of |aux| will be available to the tracer via PTRACE_GETEVENTMSG.
SANDBOX_EXPORT ResultExpr Trace(uint16_t aux);

// Trap specifies a result that the system call should be handled by
// trapping back into userspace and invoking |trap_func|, passing
// |aux| as the second parameter.
SANDBOX_EXPORT ResultExpr
    Trap(TrapRegistry::TrapFnc trap_func, const void* aux);

// UnsafeTrap is like Trap, except the policy is marked as "unsafe"
// and allowed to use SandboxSyscall to invoke any system call.
//
// NOTE: This feature, by definition, disables all security features of
//   the sandbox. It should never be used in production, but it can be
//   very useful to diagnose code that is incompatible with the sandbox.
//   If even a single system call returns "UnsafeTrap", the security of
//   entire sandbox should be considered compromised.
SANDBOX_EXPORT ResultExpr
    UnsafeTrap(TrapRegistry::TrapFnc trap_func, const void* aux);

// BoolConst converts a bool value into a BoolExpr.
SANDBOX_EXPORT BoolExpr BoolConst(bool value);

// Various ways to combine boolean expressions into more complex expressions.
// They follow standard boolean algebra laws.
SANDBOX_EXPORT BoolExpr operator!(const BoolExpr& cond);
SANDBOX_EXPORT BoolExpr operator&&(const BoolExpr& lhs, const BoolExpr& rhs);
SANDBOX_EXPORT BoolExpr operator||(const BoolExpr& lhs, const BoolExpr& rhs);

template <typename T>
class SANDBOX_EXPORT Arg {
 public:
  // Initializes the Arg to represent the |num|th system call
  // argument (indexed from 0), which is of type |T|.
  explicit Arg(int num);

  Arg(const Arg& arg) : num_(arg.num_), mask_(arg.mask_) {}

  // Returns an Arg representing the current argument, but after
  // bitwise-and'ing it with |rhs|.
  friend Arg operator&(const Arg& lhs, uint64_t rhs) {
    return Arg(lhs.num_, lhs.mask_ & rhs);
  }

  // Returns a boolean expression comparing whether the system call argument
  // (after applying any bitmasks, if appropriate) equals |rhs|.
  friend BoolExpr operator==(const Arg& lhs, T rhs) { return lhs.EqualTo(rhs); }

  // Returns a boolean expression comparing whether the system call argument
  // (after applying any bitmasks, if appropriate) does not equal |rhs|.
  friend BoolExpr operator!=(const Arg& lhs, T rhs) { return !(lhs == rhs); }

 private:
  Arg(int num, uint64_t mask) : num_(num), mask_(mask) {}

  BoolExpr EqualTo(T val) const;

  int num_;
  uint64_t mask_;

  DISALLOW_ASSIGN(Arg);
};

// If begins a conditional result expression predicated on the
// specified boolean expression.
SANDBOX_EXPORT Elser If(const BoolExpr& cond, const ResultExpr& then_result);

class SANDBOX_EXPORT Elser {
 public:
  Elser(const Elser& elser);
  ~Elser();

  // ElseIf extends the conditional result expression with another
  // "if then" clause, predicated on the specified boolean expression.
  Elser ElseIf(const BoolExpr& cond, const ResultExpr& then_result) const;

  // Else terminates a conditional result expression using |else_result| as
  // the default fallback result expression.
  ResultExpr Else(const ResultExpr& else_result) const;

 private:
  typedef std::pair<BoolExpr, ResultExpr> Clause;

  explicit Elser(cons::List<Clause> clause_list);

  cons::List<Clause> clause_list_;

  friend Elser If(const BoolExpr&, const ResultExpr&);
  template <typename T>
  friend Caser<T> Switch(const Arg<T>&);
  DISALLOW_ASSIGN(Elser);
};

// Switch begins a switch expression dispatched according to the
// specified argument value.
template <typename T>
SANDBOX_EXPORT Caser<T> Switch(const Arg<T>& arg);

template <typename T>
class SANDBOX_EXPORT Caser {
 public:
  Caser(const Caser<T>& caser) : arg_(caser.arg_), elser_(caser.elser_) {}
  ~Caser() {}

  // Case adds a single-value "case" clause to the switch.
  Caser<T> Case(T value, ResultExpr result) const;

  // Cases adds a multiple-value "case" clause to the switch.
  // See also the SANDBOX_BPF_DSL_CASES macro below for a more idiomatic way
  // of using this function.
  Caser<T> Cases(const std::vector<T>& values, ResultExpr result) const;

  // Terminate the switch with a "default" clause.
  ResultExpr Default(ResultExpr result) const;

 private:
  Caser(const Arg<T>& arg, Elser elser) : arg_(arg), elser_(elser) {}

  Arg<T> arg_;
  Elser elser_;

  template <typename U>
  friend Caser<U> Switch(const Arg<U>&);
  DISALLOW_ASSIGN(Caser);
};

// Recommended usage is to put
//    #define CASES SANDBOX_BPF_DSL_CASES
// near the top of the .cc file (e.g., nearby any "using" statements), then
// use like:
//    Switch(arg).CASES((3, 5, 7), result)...;
#define SANDBOX_BPF_DSL_CASES(values, result) \
  Cases(SANDBOX_BPF_DSL_CASES_HELPER values, result)

// Helper macro to construct a std::vector from an initializer list.
// TODO(mdempsky): Convert to use C++11 initializer lists instead.
#define SANDBOX_BPF_DSL_CASES_HELPER(value, ...)                           \
  ({                                                                       \
    const __typeof__(value) bpf_dsl_cases_values[] = {value, __VA_ARGS__}; \
    std::vector<__typeof__(value)>(                                        \
        bpf_dsl_cases_values,                                              \
        bpf_dsl_cases_values + arraysize(bpf_dsl_cases_values));           \
  })

// =====================================================================
// Official API ends here.
// =====================================================================

namespace internal {

// Make argument-dependent lookup work.  This is necessary because although
// BoolExpr is defined in bpf_dsl, since it's merely a typedef for
// scoped_refptr<const internal::BoolExplImpl>, argument-dependent lookup only
// searches the "internal" nested namespace.
using bpf_dsl::operator!;
using bpf_dsl::operator||;
using bpf_dsl::operator&&;

// Returns a boolean expression that represents whether system call
// argument |num| of size |size| is equal to |val|, when masked
// according to |mask|.  Users should use the Arg template class below
// instead of using this API directly.
SANDBOX_EXPORT BoolExpr
    ArgEq(int num, size_t size, uint64_t mask, uint64_t val);

// Returns the default mask for a system call argument of the specified size.
SANDBOX_EXPORT uint64_t DefaultMask(size_t size);

}  // namespace internal

template <typename T>
Arg<T>::Arg(int num)
    : num_(num), mask_(internal::DefaultMask(sizeof(T))) {
}

// Definition requires ArgEq to have been declared.  Moved out-of-line
// to minimize how much internal clutter users have to ignore while
// reading the header documentation.
//
// Additionally, we use this helper member function to avoid linker errors
// caused by defining operator== out-of-line.  For a more detailed explanation,
// see http://www.parashift.com/c++-faq-lite/template-friends.html.
template <typename T>
BoolExpr Arg<T>::EqualTo(T val) const {
  return internal::ArgEq(num_, sizeof(T), mask_, static_cast<uint64_t>(val));
}

template <typename T>
SANDBOX_EXPORT Caser<T> Switch(const Arg<T>& arg) {
  return Caser<T>(arg, Elser(nullptr));
}

template <typename T>
Caser<T> Caser<T>::Case(T value, ResultExpr result) const {
  return SANDBOX_BPF_DSL_CASES((value), result);
}

template <typename T>
Caser<T> Caser<T>::Cases(const std::vector<T>& values,
                         ResultExpr result) const {
  // Theoretically we could evaluate arg_ just once and emit a more efficient
  // dispatch table, but for now we simply translate into an equivalent
  // If/ElseIf/Else chain.

  typedef typename std::vector<T>::const_iterator Iter;
  BoolExpr test = BoolConst(false);
  for (Iter i = values.begin(), end = values.end(); i != end; ++i) {
    test = test || (arg_ == *i);
  }
  return Caser<T>(arg_, elser_.ElseIf(test, result));
}

template <typename T>
ResultExpr Caser<T>::Default(ResultExpr result) const {
  return elser_.Else(result);
}

}  // namespace bpf_dsl
}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_BPF_DSL_H_
