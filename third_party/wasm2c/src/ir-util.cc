/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/ir-util.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "src/cast.h"
#include "src/common.h"
#include "src/expr-visitor.h"
#include "src/ir.h"
#include "src/ir-util.h"
#include "src/literal.h"
#include "src/stream.h"

#define WABT_TRACING 0
#include "src/tracing.h"

using namespace wabt;

const Label* ModuleContext::GetLabel(const Var& var) const {
  if (var.is_name()) {
    for (Index i = GetLabelStackSize(); i > 0; --i) {
      auto label = &label_stack_[i - 1];
      if (label->name == var.name()) {
        return label;
      }
    }
  } else if (var.index() < GetLabelStackSize()) {
    auto label = &label_stack_[GetLabelStackSize() - var.index() - 1];
    return label;
  }
  return nullptr;
}

Index ModuleContext::GetLabelArity(const Var& var) const {
  auto label = GetLabel(var);
  if (!label) {
    return 0;
  }

  return label->label_type == LabelType::Loop ? label->param_types.size()
                                              : label->result_types.size();
}

Index ModuleContext::GetFuncParamCount(const Var& var) const {
  const Func* func = module.GetFunc(var);
  return func ? func->GetNumParams() : 0;
}

Index ModuleContext::GetFuncResultCount(const Var& var) const {
  const Func* func = module.GetFunc(var);
  return func ? func->GetNumResults() : 0;
}

void ModuleContext::BeginBlock(LabelType label_type, const Block& block) {
  label_stack_.emplace_back(label_type, block.label, block.decl.sig.param_types,
                            block.decl.sig.result_types);
}

void ModuleContext::EndBlock() {
  label_stack_.pop_back();
}

void ModuleContext::BeginFunc(const Func& func) {
  label_stack_.clear();
  label_stack_.emplace_back(LabelType::Func, std::string(), TypeVector(),
                            func.decl.sig.result_types);
  current_func_ = &func;
}

void ModuleContext::EndFunc() {
  current_func_ = nullptr;
}

ModuleContext::Arities ModuleContext::GetExprArity(const Expr& expr) const {
  switch (expr.type()) {
    case ExprType::AtomicNotify:
    case ExprType::AtomicRmw:
    case ExprType::Binary:
    case ExprType::Compare:
    case ExprType::TableGrow:
      return { 2, 1 };

    case ExprType::AtomicStore:
    case ExprType::Store:
    case ExprType::TableSet:
      return { 2, 0 };

    case ExprType::Block:
      return { 0, cast<BlockExpr>(&expr)->block.decl.sig.GetNumResults() };

    case ExprType::Br:
      return { GetLabelArity(cast<BrExpr>(&expr)->var), 1, true };

    case ExprType::BrIf: {
      Index arity = GetLabelArity(cast<BrIfExpr>(&expr)->var);
      return { arity + 1, arity };
    }

    case ExprType::BrTable:
      return { GetLabelArity(cast<BrTableExpr>(&expr)->default_target) + 1, 1,
               true };

    case ExprType::Call: {
      const Var& var = cast<CallExpr>(&expr)->var;
      return { GetFuncParamCount(var), GetFuncResultCount(var) };
    }

    case ExprType::ReturnCall: {
      const Var& var = cast<ReturnCallExpr>(&expr)->var;
      return { GetFuncParamCount(var), GetFuncResultCount(var), true };
    }

    case ExprType::CallIndirect: {
      const auto* ci_expr = cast<CallIndirectExpr>(&expr);
      return { ci_expr->decl.GetNumParams() + 1,
               ci_expr->decl.GetNumResults() };
    }

    case ExprType::ReturnCallIndirect: {
      const auto* rci_expr = cast<ReturnCallIndirectExpr>(&expr);
      return { rci_expr->decl.GetNumParams() + 1,
               rci_expr->decl.GetNumResults(), true };
    }

    case ExprType::Const:
    case ExprType::GlobalGet:
    case ExprType::LocalGet:
    case ExprType::MemorySize:
    case ExprType::TableSize:
    case ExprType::RefNull:
    case ExprType::RefFunc:
      return { 0, 1 };

    case ExprType::Unreachable:
      return { 0, 1, true };

    case ExprType::DataDrop:
    case ExprType::ElemDrop:
    case ExprType::AtomicFence:
      return { 0, 0 };

    case ExprType::MemoryInit:
    case ExprType::TableInit:
    case ExprType::MemoryFill:
    case ExprType::MemoryCopy:
    case ExprType::TableCopy:
    case ExprType::TableFill:
      return { 3, 0 };

    case ExprType::AtomicLoad:
    case ExprType::Convert:
    case ExprType::Load:
    case ExprType::LocalTee:
    case ExprType::MemoryGrow:
    case ExprType::Unary:
    case ExprType::TableGet:
    case ExprType::RefIsNull:
    case ExprType::LoadSplat:
    case ExprType::LoadZero:
      return { 1, 1 };

    case ExprType::Drop:
    case ExprType::GlobalSet:
    case ExprType::LocalSet:
      return { 1, 0 };

    case ExprType::If:
      return { 1, cast<IfExpr>(&expr)->true_.decl.sig.GetNumResults() };

    case ExprType::Loop:
      return { 0, cast<LoopExpr>(&expr)->block.decl.sig.GetNumResults() };

    case ExprType::Nop:
      return { 0, 0 };

    case ExprType::Return:
      return
        { static_cast<Index>(current_func_->decl.sig.result_types.size()), 1,
          true };

    case ExprType::Rethrow:
      return { 0, 0, true };

    case ExprType::AtomicRmwCmpxchg:
    case ExprType::AtomicWait:
    case ExprType::Select:
      return { 3, 1 };

    case ExprType::Throw: {
      auto throw_ = cast<ThrowExpr>(&expr);
      Index operand_count = 0;
      if (Event* event = module.GetEvent(throw_->var)) {
        operand_count = event->decl.sig.param_types.size();
      }
      return { operand_count, 0, true };
    }

    case ExprType::Try:
      return { 0, cast<TryExpr>(&expr)->block.decl.sig.GetNumResults() };

    case ExprType::Ternary:
      return { 3, 1 };

    case ExprType::SimdLaneOp: {
      const Opcode opcode = cast<SimdLaneOpExpr>(&expr)->opcode;
      switch (opcode) {
        case Opcode::I8X16ExtractLaneS:
        case Opcode::I8X16ExtractLaneU:
        case Opcode::I16X8ExtractLaneS:
        case Opcode::I16X8ExtractLaneU:
        case Opcode::I32X4ExtractLane:
        case Opcode::I64X2ExtractLane:
        case Opcode::F32X4ExtractLane:
        case Opcode::F64X2ExtractLane:
          return { 1, 1 };

        case Opcode::I8X16ReplaceLane:
        case Opcode::I16X8ReplaceLane:
        case Opcode::I32X4ReplaceLane:
        case Opcode::I64X2ReplaceLane:
        case Opcode::F32X4ReplaceLane:
        case Opcode::F64X2ReplaceLane:
          return { 2, 1 };

        default:
          fprintf(stderr, "Invalid Opcode for expr type: %s\n",
                  GetExprTypeName(expr));
          assert(0);
          return { 0, 0 };
      }
    }

    case ExprType::SimdLoadLane:
    case ExprType::SimdStoreLane: {
      return { 2, 1 };
    }

    case ExprType::SimdShuffleOp:
      return { 2, 1 };
  }

  WABT_UNREACHABLE;
}
