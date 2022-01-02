/*
 * Copyright 2017 WebAssembly Community Group participants
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

#include "src/expr-visitor.h"

#include "src/cast.h"
#include "src/ir.h"

namespace wabt {

ExprVisitor::ExprVisitor(Delegate* delegate) : delegate_(delegate) {}

Result ExprVisitor::VisitExpr(Expr* root_expr) {
  state_stack_.clear();
  expr_stack_.clear();
  expr_iter_stack_.clear();
  catch_index_stack_.clear();

  PushDefault(root_expr);

  while (!state_stack_.empty()) {
    State state = state_stack_.back();
    auto* expr = expr_stack_.back();

    switch (state) {
      case State::Default:
        PopDefault();
        CHECK_RESULT(HandleDefaultState(expr));
        break;

      case State::Block: {
        auto block_expr = cast<BlockExpr>(expr);
        auto& iter = expr_iter_stack_.back();
        if (iter != block_expr->block.exprs.end()) {
          PushDefault(&*iter++);
        } else {
          CHECK_RESULT(delegate_->EndBlockExpr(block_expr));
          PopExprlist();
        }
        break;
      }

      case State::IfTrue: {
        auto if_expr = cast<IfExpr>(expr);
        auto& iter = expr_iter_stack_.back();
        if (iter != if_expr->true_.exprs.end()) {
          PushDefault(&*iter++);
        } else {
          CHECK_RESULT(delegate_->AfterIfTrueExpr(if_expr));
          PopExprlist();
          PushExprlist(State::IfFalse, expr, if_expr->false_);
        }
        break;
      }

      case State::IfFalse: {
        auto if_expr = cast<IfExpr>(expr);
        auto& iter = expr_iter_stack_.back();
        if (iter != if_expr->false_.end()) {
          PushDefault(&*iter++);
        } else {
          CHECK_RESULT(delegate_->EndIfExpr(if_expr));
          PopExprlist();
        }
        break;
      }

      case State::Loop: {
        auto loop_expr = cast<LoopExpr>(expr);
        auto& iter = expr_iter_stack_.back();
        if (iter != loop_expr->block.exprs.end()) {
          PushDefault(&*iter++);
        } else {
          CHECK_RESULT(delegate_->EndLoopExpr(loop_expr));
          PopExprlist();
        }
        break;
      }

      case State::Try: {
        auto try_expr = cast<TryExpr>(expr);
        auto& iter = expr_iter_stack_.back();
        if (iter != try_expr->block.exprs.end()) {
          PushDefault(&*iter++);
        } else {
          PopExprlist();
          switch (try_expr->kind) {
            case TryKind::Catch:
              if (!try_expr->catches.empty()) {
                Catch& catch_ = try_expr->catches[0];
                CHECK_RESULT(delegate_->OnCatchExpr(try_expr, &catch_));
                PushCatch(expr, 0, catch_.exprs);
              } else {
                CHECK_RESULT(delegate_->EndTryExpr(try_expr));
              }
              break;
            case TryKind::Delegate:
              CHECK_RESULT(delegate_->OnDelegateExpr(try_expr));
              break;
            case TryKind::Plain:
              CHECK_RESULT(delegate_->EndTryExpr(try_expr));
              break;
          }
        }
        break;
      }

      case State::Catch: {
        auto try_expr = cast<TryExpr>(expr);
        Index catch_index = catch_index_stack_.back();
        auto& iter = expr_iter_stack_.back();
        if (iter != try_expr->catches[catch_index].exprs.end()) {
          PushDefault(&*iter++);
        } else {
          PopCatch();
          catch_index++;
          if (catch_index < try_expr->catches.size()) {
            Catch& catch_ = try_expr->catches[catch_index];
            CHECK_RESULT(delegate_->OnCatchExpr(try_expr, &catch_));
            PushCatch(expr, catch_index, catch_.exprs);
          } else {
            CHECK_RESULT(delegate_->EndTryExpr(try_expr));
          }
        }
        break;
      }
    }
  }

  return Result::Ok;
}

Result ExprVisitor::VisitExprList(ExprList& exprs) {
  for (Expr& expr : exprs)
    CHECK_RESULT(VisitExpr(&expr));
  return Result::Ok;
}

Result ExprVisitor::VisitFunc(Func* func) {
  return VisitExprList(func->exprs);
}

Result ExprVisitor::HandleDefaultState(Expr* expr) {
  switch (expr->type()) {
    case ExprType::AtomicLoad:
      CHECK_RESULT(delegate_->OnAtomicLoadExpr(cast<AtomicLoadExpr>(expr)));
      break;

    case ExprType::AtomicStore:
      CHECK_RESULT(delegate_->OnAtomicStoreExpr(cast<AtomicStoreExpr>(expr)));
      break;

    case ExprType::AtomicRmw:
      CHECK_RESULT(delegate_->OnAtomicRmwExpr(cast<AtomicRmwExpr>(expr)));
      break;

    case ExprType::AtomicRmwCmpxchg:
      CHECK_RESULT(
          delegate_->OnAtomicRmwCmpxchgExpr(cast<AtomicRmwCmpxchgExpr>(expr)));
      break;

    case ExprType::AtomicWait:
      CHECK_RESULT(delegate_->OnAtomicWaitExpr(cast<AtomicWaitExpr>(expr)));
      break;

    case ExprType::AtomicFence:
      CHECK_RESULT(delegate_->OnAtomicFenceExpr(cast<AtomicFenceExpr>(expr)));
      break;

    case ExprType::AtomicNotify:
      CHECK_RESULT(delegate_->OnAtomicNotifyExpr(cast<AtomicNotifyExpr>(expr)));
      break;

    case ExprType::Binary:
      CHECK_RESULT(delegate_->OnBinaryExpr(cast<BinaryExpr>(expr)));
      break;

    case ExprType::Block: {
      auto block_expr = cast<BlockExpr>(expr);
      CHECK_RESULT(delegate_->BeginBlockExpr(block_expr));
      PushExprlist(State::Block, expr, block_expr->block.exprs);
      break;
    }

    case ExprType::Br:
      CHECK_RESULT(delegate_->OnBrExpr(cast<BrExpr>(expr)));
      break;

    case ExprType::BrIf:
      CHECK_RESULT(delegate_->OnBrIfExpr(cast<BrIfExpr>(expr)));
      break;

    case ExprType::BrTable:
      CHECK_RESULT(delegate_->OnBrTableExpr(cast<BrTableExpr>(expr)));
      break;

    case ExprType::Call:
      CHECK_RESULT(delegate_->OnCallExpr(cast<CallExpr>(expr)));
      break;

    case ExprType::CallIndirect:
      CHECK_RESULT(delegate_->OnCallIndirectExpr(cast<CallIndirectExpr>(expr)));
      break;

    case ExprType::CallRef:
      CHECK_RESULT(delegate_->OnCallRefExpr(cast<CallRefExpr>(expr)));
      break;

    case ExprType::Compare:
      CHECK_RESULT(delegate_->OnCompareExpr(cast<CompareExpr>(expr)));
      break;

    case ExprType::Const:
      CHECK_RESULT(delegate_->OnConstExpr(cast<ConstExpr>(expr)));
      break;

    case ExprType::Convert:
      CHECK_RESULT(delegate_->OnConvertExpr(cast<ConvertExpr>(expr)));
      break;

    case ExprType::Drop:
      CHECK_RESULT(delegate_->OnDropExpr(cast<DropExpr>(expr)));
      break;

    case ExprType::GlobalGet:
      CHECK_RESULT(delegate_->OnGlobalGetExpr(cast<GlobalGetExpr>(expr)));
      break;

    case ExprType::GlobalSet:
      CHECK_RESULT(delegate_->OnGlobalSetExpr(cast<GlobalSetExpr>(expr)));
      break;

    case ExprType::If: {
      auto if_expr = cast<IfExpr>(expr);
      CHECK_RESULT(delegate_->BeginIfExpr(if_expr));
      PushExprlist(State::IfTrue, expr, if_expr->true_.exprs);
      break;
    }

    case ExprType::Load:
      CHECK_RESULT(delegate_->OnLoadExpr(cast<LoadExpr>(expr)));
      break;

    case ExprType::LoadSplat:
      CHECK_RESULT(delegate_->OnLoadSplatExpr(cast<LoadSplatExpr>(expr)));
      break;

    case ExprType::LoadZero:
      CHECK_RESULT(delegate_->OnLoadZeroExpr(cast<LoadZeroExpr>(expr)));
      break;

    case ExprType::LocalGet:
      CHECK_RESULT(delegate_->OnLocalGetExpr(cast<LocalGetExpr>(expr)));
      break;

    case ExprType::LocalSet:
      CHECK_RESULT(delegate_->OnLocalSetExpr(cast<LocalSetExpr>(expr)));
      break;

    case ExprType::LocalTee:
      CHECK_RESULT(delegate_->OnLocalTeeExpr(cast<LocalTeeExpr>(expr)));
      break;

    case ExprType::Loop: {
      auto loop_expr = cast<LoopExpr>(expr);
      CHECK_RESULT(delegate_->BeginLoopExpr(loop_expr));
      PushExprlist(State::Loop, expr, loop_expr->block.exprs);
      break;
    }

    case ExprType::MemoryCopy:
      CHECK_RESULT(delegate_->OnMemoryCopyExpr(cast<MemoryCopyExpr>(expr)));
      break;

    case ExprType::DataDrop:
      CHECK_RESULT(delegate_->OnDataDropExpr(cast<DataDropExpr>(expr)));
      break;

    case ExprType::MemoryFill:
      CHECK_RESULT(delegate_->OnMemoryFillExpr(cast<MemoryFillExpr>(expr)));
      break;

    case ExprType::MemoryGrow:
      CHECK_RESULT(delegate_->OnMemoryGrowExpr(cast<MemoryGrowExpr>(expr)));
      break;

    case ExprType::MemoryInit:
      CHECK_RESULT(delegate_->OnMemoryInitExpr(cast<MemoryInitExpr>(expr)));
      break;

    case ExprType::MemorySize:
      CHECK_RESULT(delegate_->OnMemorySizeExpr(cast<MemorySizeExpr>(expr)));
      break;

    case ExprType::TableCopy:
      CHECK_RESULT(delegate_->OnTableCopyExpr(cast<TableCopyExpr>(expr)));
      break;

    case ExprType::ElemDrop:
      CHECK_RESULT(delegate_->OnElemDropExpr(cast<ElemDropExpr>(expr)));
      break;

    case ExprType::TableInit:
      CHECK_RESULT(delegate_->OnTableInitExpr(cast<TableInitExpr>(expr)));
      break;

    case ExprType::TableGet:
      CHECK_RESULT(delegate_->OnTableGetExpr(cast<TableGetExpr>(expr)));
      break;

    case ExprType::TableSet:
      CHECK_RESULT(delegate_->OnTableSetExpr(cast<TableSetExpr>(expr)));
      break;

    case ExprType::TableGrow:
      CHECK_RESULT(delegate_->OnTableGrowExpr(cast<TableGrowExpr>(expr)));
      break;

    case ExprType::TableSize:
      CHECK_RESULT(delegate_->OnTableSizeExpr(cast<TableSizeExpr>(expr)));
      break;

    case ExprType::TableFill:
      CHECK_RESULT(delegate_->OnTableFillExpr(cast<TableFillExpr>(expr)));
      break;

    case ExprType::RefFunc:
      CHECK_RESULT(delegate_->OnRefFuncExpr(cast<RefFuncExpr>(expr)));
      break;

    case ExprType::RefNull:
      CHECK_RESULT(delegate_->OnRefNullExpr(cast<RefNullExpr>(expr)));
      break;

    case ExprType::RefIsNull:
      CHECK_RESULT(delegate_->OnRefIsNullExpr(cast<RefIsNullExpr>(expr)));
      break;

    case ExprType::Nop:
      CHECK_RESULT(delegate_->OnNopExpr(cast<NopExpr>(expr)));
      break;

    case ExprType::Rethrow:
      CHECK_RESULT(delegate_->OnRethrowExpr(cast<RethrowExpr>(expr)));
      break;

    case ExprType::Return:
      CHECK_RESULT(delegate_->OnReturnExpr(cast<ReturnExpr>(expr)));
      break;

    case ExprType::ReturnCall:
      CHECK_RESULT(delegate_->OnReturnCallExpr(cast<ReturnCallExpr>(expr)));
      break;

    case ExprType::ReturnCallIndirect:
      CHECK_RESULT(delegate_->OnReturnCallIndirectExpr(
          cast<ReturnCallIndirectExpr>(expr)));
      break;

    case ExprType::Select:
      CHECK_RESULT(delegate_->OnSelectExpr(cast<SelectExpr>(expr)));
      break;

    case ExprType::Store:
      CHECK_RESULT(delegate_->OnStoreExpr(cast<StoreExpr>(expr)));
      break;


    case ExprType::Throw:
      CHECK_RESULT(delegate_->OnThrowExpr(cast<ThrowExpr>(expr)));
      break;

    case ExprType::Try: {
      auto try_expr = cast<TryExpr>(expr);
      CHECK_RESULT(delegate_->BeginTryExpr(try_expr));
      PushExprlist(State::Try, expr, try_expr->block.exprs);
      break;
    }

    case ExprType::Unary:
      CHECK_RESULT(delegate_->OnUnaryExpr(cast<UnaryExpr>(expr)));
      break;

    case ExprType::Ternary:
      CHECK_RESULT(delegate_->OnTernaryExpr(cast<TernaryExpr>(expr)));
      break;

    case ExprType::SimdLaneOp: {
      CHECK_RESULT(delegate_->OnSimdLaneOpExpr(cast<SimdLaneOpExpr>(expr)));
      break;
    }

    case ExprType::SimdLoadLane: {
      CHECK_RESULT(delegate_->OnSimdLoadLaneExpr(cast<SimdLoadLaneExpr>(expr)));
      break;
    }

    case ExprType::SimdStoreLane: {
      CHECK_RESULT(
          delegate_->OnSimdStoreLaneExpr(cast<SimdStoreLaneExpr>(expr)));
      break;
    }

    case ExprType::SimdShuffleOp: {
      CHECK_RESULT(
          delegate_->OnSimdShuffleOpExpr(cast<SimdShuffleOpExpr>(expr)));
      break;
    }

    case ExprType::Unreachable:
      CHECK_RESULT(delegate_->OnUnreachableExpr(cast<UnreachableExpr>(expr)));
      break;
  }

  return Result::Ok;
}

void ExprVisitor::PushDefault(Expr* expr) {
  state_stack_.emplace_back(State::Default);
  expr_stack_.emplace_back(expr);
}

void ExprVisitor::PopDefault() {
  state_stack_.pop_back();
  expr_stack_.pop_back();
}

void ExprVisitor::PushExprlist(State state, Expr* expr, ExprList& expr_list) {
  state_stack_.emplace_back(state);
  expr_stack_.emplace_back(expr);
  expr_iter_stack_.emplace_back(expr_list.begin());
}

void ExprVisitor::PopExprlist() {
  state_stack_.pop_back();
  expr_stack_.pop_back();
  expr_iter_stack_.pop_back();
}

void ExprVisitor::PushCatch(Expr* expr,
                            Index catch_index,
                            ExprList& expr_list) {
  state_stack_.emplace_back(State::Catch);
  expr_stack_.emplace_back(expr);
  expr_iter_stack_.emplace_back(expr_list.begin());
  catch_index_stack_.emplace_back(catch_index);
}

void ExprVisitor::PopCatch() {
  state_stack_.pop_back();
  expr_stack_.pop_back();
  expr_iter_stack_.pop_back();
  catch_index_stack_.pop_back();
}

}  // namespace wabt
