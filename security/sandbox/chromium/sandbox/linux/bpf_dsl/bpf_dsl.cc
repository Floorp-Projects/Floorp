// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"

#include <limits>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_impl.h"
#include "sandbox/linux/bpf_dsl/policy_compiler.h"
#include "sandbox/linux/seccomp-bpf/errorcode.h"
#include "mozilla/Attributes.h"

namespace sandbox {
namespace bpf_dsl {
namespace {

class AllowResultExprImpl : public internal::ResultExprImpl {
 public:
  AllowResultExprImpl() {}

  ErrorCode Compile(PolicyCompiler* pc) const override {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }

 private:
  ~AllowResultExprImpl() override {}

  DISALLOW_COPY_AND_ASSIGN(AllowResultExprImpl);
};

class ErrorResultExprImpl : public internal::ResultExprImpl {
 public:
  explicit ErrorResultExprImpl(int err) : err_(err) {
    CHECK(err_ >= ErrorCode::ERR_MIN_ERRNO && err_ <= ErrorCode::ERR_MAX_ERRNO);
  }

  ErrorCode Compile(PolicyCompiler* pc) const override {
    return pc->Error(err_);
  }

 private:
  ~ErrorResultExprImpl() override {}

  int err_;

  DISALLOW_COPY_AND_ASSIGN(ErrorResultExprImpl);
};

class KillResultExprImpl : public internal::ResultExprImpl {
 public:
  explicit KillResultExprImpl(const char* msg) : msg_(msg) { DCHECK(msg_); }

  ErrorCode Compile(PolicyCompiler* pc) const override {
    return pc->Kill(msg_);
  }

 private:
  ~KillResultExprImpl() override {}

  const char* msg_;

  DISALLOW_COPY_AND_ASSIGN(KillResultExprImpl);
};

class TraceResultExprImpl : public internal::ResultExprImpl {
 public:
  MOZ_IMPLICIT TraceResultExprImpl(uint16_t aux) : aux_(aux) {}

  ErrorCode Compile(PolicyCompiler* pc) const override {
    return ErrorCode(ErrorCode::ERR_TRACE + aux_);
  }

 private:
  ~TraceResultExprImpl() override {}

  uint16_t aux_;

  DISALLOW_COPY_AND_ASSIGN(TraceResultExprImpl);
};

class TrapResultExprImpl : public internal::ResultExprImpl {
 public:
  TrapResultExprImpl(TrapRegistry::TrapFnc func, const void* arg)
      : func_(func), arg_(arg) {
    DCHECK(func_);
  }

  ErrorCode Compile(PolicyCompiler* pc) const override {
    return pc->Trap(func_, arg_);
  }

 private:
  ~TrapResultExprImpl() override {}

  TrapRegistry::TrapFnc func_;
  const void* arg_;

  DISALLOW_COPY_AND_ASSIGN(TrapResultExprImpl);
};

class UnsafeTrapResultExprImpl : public internal::ResultExprImpl {
 public:
  UnsafeTrapResultExprImpl(TrapRegistry::TrapFnc func, const void* arg)
      : func_(func), arg_(arg) {
    DCHECK(func_);
  }

  ErrorCode Compile(PolicyCompiler* pc) const override {
    return pc->UnsafeTrap(func_, arg_);
  }

  bool HasUnsafeTraps() const override { return true; }

 private:
  ~UnsafeTrapResultExprImpl() override {}

  TrapRegistry::TrapFnc func_;
  const void* arg_;

  DISALLOW_COPY_AND_ASSIGN(UnsafeTrapResultExprImpl);
};

class IfThenResultExprImpl : public internal::ResultExprImpl {
 public:
  IfThenResultExprImpl(const BoolExpr& cond,
                       const ResultExpr& then_result,
                       const ResultExpr& else_result)
      : cond_(cond), then_result_(then_result), else_result_(else_result) {}

  ErrorCode Compile(PolicyCompiler* pc) const override {
    return cond_->Compile(
        pc, then_result_->Compile(pc), else_result_->Compile(pc));
  }

  bool HasUnsafeTraps() const override {
    return then_result_->HasUnsafeTraps() || else_result_->HasUnsafeTraps();
  }

 private:
  ~IfThenResultExprImpl() override {}

  BoolExpr cond_;
  ResultExpr then_result_;
  ResultExpr else_result_;

  DISALLOW_COPY_AND_ASSIGN(IfThenResultExprImpl);
};

class ConstBoolExprImpl : public internal::BoolExprImpl {
 public:
  MOZ_IMPLICIT ConstBoolExprImpl(bool value) : value_(value) {}

  ErrorCode Compile(PolicyCompiler* pc,
                    ErrorCode true_ec,
                    ErrorCode false_ec) const override {
    return value_ ? true_ec : false_ec;
  }

 private:
  ~ConstBoolExprImpl() override {}

  bool value_;

  DISALLOW_COPY_AND_ASSIGN(ConstBoolExprImpl);
};

class PrimitiveBoolExprImpl : public internal::BoolExprImpl {
 public:
  PrimitiveBoolExprImpl(int argno,
                        ErrorCode::ArgType is_32bit,
                        uint64_t mask,
                        uint64_t value)
      : argno_(argno), is_32bit_(is_32bit), mask_(mask), value_(value) {}

  ErrorCode Compile(PolicyCompiler* pc,
                    ErrorCode true_ec,
                    ErrorCode false_ec) const override {
    return pc->CondMaskedEqual(
        argno_, is_32bit_, mask_, value_, true_ec, false_ec);
  }

 private:
  ~PrimitiveBoolExprImpl() override {}

  int argno_;
  ErrorCode::ArgType is_32bit_;
  uint64_t mask_;
  uint64_t value_;

  DISALLOW_COPY_AND_ASSIGN(PrimitiveBoolExprImpl);
};

class NegateBoolExprImpl : public internal::BoolExprImpl {
 public:
  explicit NegateBoolExprImpl(const BoolExpr& cond) : cond_(cond) {}

  ErrorCode Compile(PolicyCompiler* pc,
                    ErrorCode true_ec,
                    ErrorCode false_ec) const override {
    return cond_->Compile(pc, false_ec, true_ec);
  }

 private:
  ~NegateBoolExprImpl() override {}

  BoolExpr cond_;

  DISALLOW_COPY_AND_ASSIGN(NegateBoolExprImpl);
};

class AndBoolExprImpl : public internal::BoolExprImpl {
 public:
  AndBoolExprImpl(const BoolExpr& lhs, const BoolExpr& rhs)
      : lhs_(lhs), rhs_(rhs) {}

  ErrorCode Compile(PolicyCompiler* pc,
                    ErrorCode true_ec,
                    ErrorCode false_ec) const override {
    return lhs_->Compile(pc, rhs_->Compile(pc, true_ec, false_ec), false_ec);
  }

 private:
  ~AndBoolExprImpl() override {}

  BoolExpr lhs_;
  BoolExpr rhs_;

  DISALLOW_COPY_AND_ASSIGN(AndBoolExprImpl);
};

class OrBoolExprImpl : public internal::BoolExprImpl {
 public:
  OrBoolExprImpl(const BoolExpr& lhs, const BoolExpr& rhs)
      : lhs_(lhs), rhs_(rhs) {}

  ErrorCode Compile(PolicyCompiler* pc,
                    ErrorCode true_ec,
                    ErrorCode false_ec) const override {
    return lhs_->Compile(pc, true_ec, rhs_->Compile(pc, true_ec, false_ec));
  }

 private:
  ~OrBoolExprImpl() override {}

  BoolExpr lhs_;
  BoolExpr rhs_;

  DISALLOW_COPY_AND_ASSIGN(OrBoolExprImpl);
};

}  // namespace

namespace internal {

bool ResultExprImpl::HasUnsafeTraps() const {
  return false;
}

uint64_t DefaultMask(size_t size) {
  switch (size) {
    case 4:
      return std::numeric_limits<uint32_t>::max();
    case 8:
      return std::numeric_limits<uint64_t>::max();
    default:
      CHECK(false) << "Unimplemented DefaultMask case";
      return 0;
  }
}

BoolExpr ArgEq(int num, size_t size, uint64_t mask, uint64_t val) {
  CHECK(size == 4 || size == 8);

  // TODO(mdempsky): Should we just always use TP_64BIT?
  const ErrorCode::ArgType arg_type =
      (size == 4) ? ErrorCode::TP_32BIT : ErrorCode::TP_64BIT;

  return BoolExpr(new const PrimitiveBoolExprImpl(num, arg_type, mask, val));
}

}  // namespace internal

ResultExpr Allow() {
  return ResultExpr(new const AllowResultExprImpl());
}

ResultExpr Error(int err) {
  return ResultExpr(new const ErrorResultExprImpl(err));
}

ResultExpr Kill(const char* msg) {
  return ResultExpr(new const KillResultExprImpl(msg));
}

ResultExpr Trace(uint16_t aux) {
  return ResultExpr(new const TraceResultExprImpl(aux));
}

ResultExpr Trap(TrapRegistry::TrapFnc trap_func, const void* aux) {
  return ResultExpr(new const TrapResultExprImpl(trap_func, aux));
}

ResultExpr UnsafeTrap(TrapRegistry::TrapFnc trap_func, const void* aux) {
  return ResultExpr(new const UnsafeTrapResultExprImpl(trap_func, aux));
}

BoolExpr BoolConst(bool value) {
  return BoolExpr(new const ConstBoolExprImpl(value));
}

BoolExpr operator!(const BoolExpr& cond) {
  return BoolExpr(new const NegateBoolExprImpl(cond));
}

BoolExpr operator&&(const BoolExpr& lhs, const BoolExpr& rhs) {
  return BoolExpr(new const AndBoolExprImpl(lhs, rhs));
}

BoolExpr operator||(const BoolExpr& lhs, const BoolExpr& rhs) {
  return BoolExpr(new const OrBoolExprImpl(lhs, rhs));
}

Elser If(const BoolExpr& cond, const ResultExpr& then_result) {
  return Elser(nullptr).ElseIf(cond, then_result);
}

Elser::Elser(cons::List<Clause> clause_list) : clause_list_(clause_list) {
}

Elser::Elser(const Elser& elser) : clause_list_(elser.clause_list_) {
}

Elser::~Elser() {
}

Elser Elser::ElseIf(const BoolExpr& cond, const ResultExpr& then_result) const {
  return Elser(Cons(std::make_pair(cond, then_result), clause_list_));
}

ResultExpr Elser::Else(const ResultExpr& else_result) const {
  // We finally have the default result expression for this
  // if/then/else sequence.  Also, we've already accumulated all
  // if/then pairs into a list of reverse order (i.e., lower priority
  // conditions are listed before higher priority ones).  E.g., an
  // expression like
  //
  //    If(b1, e1).ElseIf(b2, e2).ElseIf(b3, e3).Else(e4)
  //
  // will have built up a list like
  //
  //    [(b3, e3), (b2, e2), (b1, e1)].
  //
  // Now that we have e4, we can walk the list and create a ResultExpr
  // tree like:
  //
  //    expr = e4
  //    expr = (b3 ? e3 : expr) = (b3 ? e3 : e4)
  //    expr = (b2 ? e2 : expr) = (b2 ? e2 : (b3 ? e3 : e4))
  //    expr = (b1 ? e1 : expr) = (b1 ? e1 : (b2 ? e2 : (b3 ? e3 : e4)))
  //
  // and end up with an appropriately chained tree.

  ResultExpr expr = else_result;
  for (const Clause& clause : clause_list_) {
    expr = ResultExpr(
        new const IfThenResultExprImpl(clause.first, clause.second, expr));
  }
  return expr;
}

}  // namespace bpf_dsl
}  // namespace sandbox

template class scoped_refptr<const sandbox::bpf_dsl::internal::BoolExprImpl>;
template class scoped_refptr<const sandbox::bpf_dsl::internal::ResultExprImpl>;
