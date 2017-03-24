// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_impl.h"
#include "sandbox/linux/bpf_dsl/errorcode.h"
#include "sandbox/linux/bpf_dsl/policy_compiler.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"

namespace sandbox {
namespace bpf_dsl {
namespace {

class ReturnResultExprImpl : public internal::ResultExprImpl {
 public:
  explicit ReturnResultExprImpl(uint32_t ret) : ret_(ret) {}

  CodeGen::Node Compile(PolicyCompiler* pc) const override {
    return pc->Return(ret_);
  }

  bool IsAllow() const override { return IsAction(SECCOMP_RET_ALLOW); }

  bool IsDeny() const override {
    return IsAction(SECCOMP_RET_ERRNO) || IsAction(SECCOMP_RET_KILL);
  }

 private:
  ~ReturnResultExprImpl() override {}

  bool IsAction(uint32_t action) const {
    return (ret_ & SECCOMP_RET_ACTION) == action;
  }

  uint32_t ret_;

  DISALLOW_COPY_AND_ASSIGN(ReturnResultExprImpl);
};

class TrapResultExprImpl : public internal::ResultExprImpl {
 public:
  TrapResultExprImpl(TrapRegistry::TrapFnc func, const void* arg, bool safe)
      : func_(func), arg_(arg), safe_(safe) {
    DCHECK(func_);
  }

  CodeGen::Node Compile(PolicyCompiler* pc) const override {
    return pc->Trap(func_, arg_, safe_);
  }

  bool HasUnsafeTraps() const override { return safe_ == false; }

  bool IsDeny() const override { return true; }

 private:
  ~TrapResultExprImpl() override {}

  TrapRegistry::TrapFnc func_;
  const void* arg_;
  bool safe_;

  DISALLOW_COPY_AND_ASSIGN(TrapResultExprImpl);
};

class IfThenResultExprImpl : public internal::ResultExprImpl {
 public:
  IfThenResultExprImpl(const BoolExpr& cond,
                       const ResultExpr& then_result,
                       const ResultExpr& else_result)
      : cond_(cond), then_result_(then_result), else_result_(else_result) {}

  CodeGen::Node Compile(PolicyCompiler* pc) const override {
    // We compile the "then" and "else" expressions in separate statements so
    // they have a defined sequencing.  See https://crbug.com/529480.
    CodeGen::Node then_node = then_result_->Compile(pc);
    CodeGen::Node else_node = else_result_->Compile(pc);
    return cond_->Compile(pc, then_node, else_node);
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
  ConstBoolExprImpl(bool value) : value_(value) {}

  CodeGen::Node Compile(PolicyCompiler* pc,
                        CodeGen::Node then_node,
                        CodeGen::Node else_node) const override {
    return value_ ? then_node : else_node;
  }

 private:
  ~ConstBoolExprImpl() override {}

  bool value_;

  DISALLOW_COPY_AND_ASSIGN(ConstBoolExprImpl);
};

class MaskedEqualBoolExprImpl : public internal::BoolExprImpl {
 public:
  MaskedEqualBoolExprImpl(int argno,
                          size_t width,
                          uint64_t mask,
                          uint64_t value)
      : argno_(argno), width_(width), mask_(mask), value_(value) {}

  CodeGen::Node Compile(PolicyCompiler* pc,
                        CodeGen::Node then_node,
                        CodeGen::Node else_node) const override {
    return pc->MaskedEqual(argno_, width_, mask_, value_, then_node, else_node);
  }

 private:
  ~MaskedEqualBoolExprImpl() override {}

  int argno_;
  size_t width_;
  uint64_t mask_;
  uint64_t value_;

  DISALLOW_COPY_AND_ASSIGN(MaskedEqualBoolExprImpl);
};

class NegateBoolExprImpl : public internal::BoolExprImpl {
 public:
  explicit NegateBoolExprImpl(const BoolExpr& cond) : cond_(cond) {}

  CodeGen::Node Compile(PolicyCompiler* pc,
                        CodeGen::Node then_node,
                        CodeGen::Node else_node) const override {
    return cond_->Compile(pc, else_node, then_node);
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

  CodeGen::Node Compile(PolicyCompiler* pc,
                        CodeGen::Node then_node,
                        CodeGen::Node else_node) const override {
    return lhs_->Compile(pc, rhs_->Compile(pc, then_node, else_node),
                         else_node);
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

  CodeGen::Node Compile(PolicyCompiler* pc,
                        CodeGen::Node then_node,
                        CodeGen::Node else_node) const override {
    return lhs_->Compile(pc, then_node,
                         rhs_->Compile(pc, then_node, else_node));
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

bool ResultExprImpl::IsAllow() const {
  return false;
}

bool ResultExprImpl::IsDeny() const {
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
  // If this is changed, update Arg<T>::EqualTo's static_cast rules
  // accordingly.
  CHECK(size == 4 || size == 8);

  return BoolExpr(new const MaskedEqualBoolExprImpl(num, size, mask, val));
}

}  // namespace internal

ResultExpr Allow() {
  return ResultExpr(new const ReturnResultExprImpl(SECCOMP_RET_ALLOW));
}

ResultExpr Error(int err) {
  CHECK(err >= ErrorCode::ERR_MIN_ERRNO && err <= ErrorCode::ERR_MAX_ERRNO);
  return ResultExpr(new const ReturnResultExprImpl(SECCOMP_RET_ERRNO + err));
}

ResultExpr Kill() {
  return ResultExpr(new const ReturnResultExprImpl(SECCOMP_RET_KILL));
}

ResultExpr Trace(uint16_t aux) {
  return ResultExpr(new const ReturnResultExprImpl(SECCOMP_RET_TRACE + aux));
}

ResultExpr Trap(TrapRegistry::TrapFnc trap_func, const void* aux) {
  return ResultExpr(
      new const TrapResultExprImpl(trap_func, aux, true /* safe */));
}

ResultExpr UnsafeTrap(TrapRegistry::TrapFnc trap_func, const void* aux) {
  return ResultExpr(
      new const TrapResultExprImpl(trap_func, aux, false /* unsafe */));
}

BoolExpr BoolConst(bool value) {
  return BoolExpr(new const ConstBoolExprImpl(value));
}

BoolExpr Not(const BoolExpr& cond) {
  return BoolExpr(new const NegateBoolExprImpl(cond));
}

BoolExpr AllOf() {
  return BoolConst(true);
}

BoolExpr AllOf(const BoolExpr& lhs, const BoolExpr& rhs) {
  return BoolExpr(new const AndBoolExprImpl(lhs, rhs));
}

BoolExpr AnyOf() {
  return BoolConst(false);
}

BoolExpr AnyOf(const BoolExpr& lhs, const BoolExpr& rhs) {
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
