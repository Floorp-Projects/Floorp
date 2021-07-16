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

#include "src/apply-names.h"

#include <cassert>
#include <cstdio>
#include <vector>

#include "src/expr-visitor.h"
#include "src/ir.h"
#include "src/string-view.h"

namespace wabt {

namespace {

class NameApplier : public ExprVisitor::DelegateNop {
 public:
  NameApplier();

  Result VisitModule(Module* module);

  // Implementation of ExprVisitor::DelegateNop.
  Result BeginBlockExpr(BlockExpr*) override;
  Result EndBlockExpr(BlockExpr*) override;
  Result OnBrExpr(BrExpr*) override;
  Result OnBrIfExpr(BrIfExpr*) override;
  Result OnBrTableExpr(BrTableExpr*) override;
  Result OnCallExpr(CallExpr*) override;
  Result OnRefFuncExpr(RefFuncExpr*) override;
  Result OnCallIndirectExpr(CallIndirectExpr*) override;
  Result OnReturnCallExpr(ReturnCallExpr*) override;
  Result OnReturnCallIndirectExpr(ReturnCallIndirectExpr*) override;
  Result OnGlobalGetExpr(GlobalGetExpr*) override;
  Result OnGlobalSetExpr(GlobalSetExpr*) override;
  Result BeginIfExpr(IfExpr*) override;
  Result EndIfExpr(IfExpr*) override;
  Result OnLocalGetExpr(LocalGetExpr*) override;
  Result OnLocalSetExpr(LocalSetExpr*) override;
  Result OnLocalTeeExpr(LocalTeeExpr*) override;
  Result BeginLoopExpr(LoopExpr*) override;
  Result EndLoopExpr(LoopExpr*) override;
  Result OnDataDropExpr(DataDropExpr*) override;
  Result OnMemoryInitExpr(MemoryInitExpr*) override;
  Result OnElemDropExpr(ElemDropExpr*) override;
  Result OnTableCopyExpr(TableCopyExpr*) override;
  Result OnTableInitExpr(TableInitExpr*) override;
  Result OnTableGetExpr(TableGetExpr*) override;
  Result OnTableSetExpr(TableSetExpr*) override;
  Result OnTableGrowExpr(TableGrowExpr*) override;
  Result OnTableSizeExpr(TableSizeExpr*) override;
  Result OnTableFillExpr(TableFillExpr*) override;
  Result BeginTryExpr(TryExpr*) override;
  Result EndTryExpr(TryExpr*) override;
  Result OnCatchExpr(TryExpr*, Catch*) override;
  Result OnDelegateExpr(TryExpr*) override;
  Result OnThrowExpr(ThrowExpr*) override;
  Result OnRethrowExpr(RethrowExpr*) override;

 private:
  void PushLabel(const std::string& label);
  void PopLabel();
  string_view FindLabelByVar(Var* var);
  void UseNameForVar(string_view name, Var* var);
  Result UseNameForFuncTypeVar(Var* var);
  Result UseNameForFuncVar(Var* var);
  Result UseNameForGlobalVar(Var* var);
  Result UseNameForTableVar(Var* var);
  Result UseNameForMemoryVar(Var* var);
  Result UseNameForEventVar(Var* var);
  Result UseNameForDataSegmentVar(Var* var);
  Result UseNameForElemSegmentVar(Var* var);
  Result UseNameForParamAndLocalVar(Func* func, Var* var);
  Result VisitFunc(Index func_index, Func* func);
  Result VisitGlobal(Global* global);
  Result VisitEvent(Event* event);
  Result VisitExport(Index export_index, Export* export_);
  Result VisitElemSegment(Index elem_segment_index, ElemSegment* segment);
  Result VisitDataSegment(Index data_segment_index, DataSegment* segment);
  Result VisitStart(Var* start_var);

  Module* module_ = nullptr;
  Func* current_func_ = nullptr;
  ExprVisitor visitor_;
  std::vector<std::string> param_and_local_index_to_name_;
  std::vector<std::string> labels_;
};

NameApplier::NameApplier() : visitor_(this) {}

void NameApplier::PushLabel(const std::string& label) {
  labels_.push_back(label);
}

void NameApplier::PopLabel() {
  labels_.pop_back();
}

string_view NameApplier::FindLabelByVar(Var* var) {
  if (var->is_name()) {
    for (int i = labels_.size() - 1; i >= 0; --i) {
      const std::string& label = labels_[i];
      if (label == var->name()) {
        return label;
      }
    }
    return string_view();
  } else {
    if (var->index() >= labels_.size()) {
      return string_view();
    }
    return labels_[labels_.size() - 1 - var->index()];
  }
}

void NameApplier::UseNameForVar(string_view name, Var* var) {
  if (var->is_name()) {
    assert(name == var->name());
    return;
  }

  if (!name.empty()) {
    var->set_name(name);
  }
}

Result NameApplier::UseNameForFuncTypeVar(Var* var) {
  FuncType* func_type = module_->GetFuncType(*var);
  if (!func_type) {
    return Result::Error;
  }
  UseNameForVar(func_type->name, var);
  return Result::Ok;
}

Result NameApplier::UseNameForFuncVar(Var* var) {
  Func* func = module_->GetFunc(*var);
  if (!func) {
    return Result::Error;
  }
  UseNameForVar(func->name, var);
  return Result::Ok;
}

Result NameApplier::UseNameForGlobalVar(Var* var) {
  Global* global = module_->GetGlobal(*var);
  if (!global) {
    return Result::Error;
  }
  UseNameForVar(global->name, var);
  return Result::Ok;
}

Result NameApplier::UseNameForTableVar(Var* var) {
  Table* table = module_->GetTable(*var);
  if (!table) {
    return Result::Error;
  }
  UseNameForVar(table->name, var);
  return Result::Ok;
}

Result NameApplier::UseNameForMemoryVar(Var* var) {
  Memory* memory = module_->GetMemory(*var);
  if (!memory) {
    return Result::Error;
  }
  UseNameForVar(memory->name, var);
  return Result::Ok;
}

Result NameApplier::UseNameForEventVar(Var* var) {
  Event* event = module_->GetEvent(*var);
  if (!event) {
    return Result::Error;
  }
  UseNameForVar(event->name, var);
  return Result::Ok;
}

Result NameApplier::UseNameForDataSegmentVar(Var* var) {
  DataSegment* data_segment = module_->GetDataSegment(*var);
  if (!data_segment) {
    return Result::Error;
  }
  UseNameForVar(data_segment->name, var);
  return Result::Ok;
}

Result NameApplier::UseNameForElemSegmentVar(Var* var) {
  ElemSegment* elem_segment = module_->GetElemSegment(*var);
  if (!elem_segment) {
    return Result::Error;
  }
  UseNameForVar(elem_segment->name, var);
  return Result::Ok;
}

Result NameApplier::UseNameForParamAndLocalVar(Func* func, Var* var) {
  Index local_index = func->GetLocalIndex(*var);
  if (local_index >= func->GetNumParamsAndLocals()) {
    return Result::Error;
  }

  std::string name = param_and_local_index_to_name_[local_index];
  if (var->is_name()) {
    assert(name == var->name());
    return Result::Ok;
  }

  if (!name.empty()) {
    var->set_name(name);
  }
  return Result::Ok;
}

Result NameApplier::BeginBlockExpr(BlockExpr* expr) {
  PushLabel(expr->block.label);
  return Result::Ok;
}

Result NameApplier::EndBlockExpr(BlockExpr* expr) {
  PopLabel();
  return Result::Ok;
}

Result NameApplier::BeginLoopExpr(LoopExpr* expr) {
  PushLabel(expr->block.label);
  return Result::Ok;
}

Result NameApplier::EndLoopExpr(LoopExpr* expr) {
  PopLabel();
  return Result::Ok;
}

Result NameApplier::OnDataDropExpr(DataDropExpr* expr) {
  CHECK_RESULT(UseNameForDataSegmentVar(&expr->var));
  return Result::Ok;
}

Result NameApplier::OnMemoryInitExpr(MemoryInitExpr* expr)  {
  CHECK_RESULT(UseNameForDataSegmentVar(&expr->var));
  return Result::Ok;
}

Result NameApplier::OnElemDropExpr(ElemDropExpr* expr)  {
  CHECK_RESULT(UseNameForElemSegmentVar(&expr->var));
  return Result::Ok;
}

Result NameApplier::OnTableCopyExpr(TableCopyExpr* expr) {
  CHECK_RESULT(UseNameForTableVar(&expr->dst_table));
  CHECK_RESULT(UseNameForTableVar(&expr->src_table));
  return Result::Ok;
}

Result NameApplier::OnTableInitExpr(TableInitExpr* expr)  {
  CHECK_RESULT(UseNameForElemSegmentVar(&expr->segment_index));
  CHECK_RESULT(UseNameForTableVar(&expr->table_index));
  return Result::Ok;
}

Result NameApplier::OnTableGetExpr(TableGetExpr* expr)  {
  CHECK_RESULT(UseNameForTableVar(&expr->var));
  return Result::Ok;
}

Result NameApplier::OnTableSetExpr(TableSetExpr* expr)  {
  CHECK_RESULT(UseNameForTableVar(&expr->var));
  return Result::Ok;
}

Result NameApplier::OnTableGrowExpr(TableGrowExpr* expr)  {
  CHECK_RESULT(UseNameForTableVar(&expr->var));
  return Result::Ok;
}

Result NameApplier::OnTableSizeExpr(TableSizeExpr* expr)  {
  CHECK_RESULT(UseNameForTableVar(&expr->var));
  return Result::Ok;
}

Result NameApplier::OnTableFillExpr(TableFillExpr* expr)  {
  CHECK_RESULT(UseNameForTableVar(&expr->var));
  return Result::Ok;
}

Result NameApplier::OnBrExpr(BrExpr* expr) {
  string_view label = FindLabelByVar(&expr->var);
  UseNameForVar(label, &expr->var);
  return Result::Ok;
}

Result NameApplier::OnBrIfExpr(BrIfExpr* expr) {
  string_view label = FindLabelByVar(&expr->var);
  UseNameForVar(label, &expr->var);
  return Result::Ok;
}

Result NameApplier::OnBrTableExpr(BrTableExpr* expr) {
  for (Var& target : expr->targets) {
    string_view label = FindLabelByVar(&target);
    UseNameForVar(label, &target);
  }

  string_view label = FindLabelByVar(&expr->default_target);
  UseNameForVar(label, &expr->default_target);
  return Result::Ok;
}

Result NameApplier::BeginTryExpr(TryExpr* expr) {
  PushLabel(expr->block.label);
  return Result::Ok;
}

Result NameApplier::EndTryExpr(TryExpr*) {
  PopLabel();
  return Result::Ok;
}

Result NameApplier::OnCatchExpr(TryExpr*, Catch* expr) {
  if (!expr->IsCatchAll()) {
    CHECK_RESULT(UseNameForEventVar(&expr->var));
  }
  return Result::Ok;
}

Result NameApplier::OnDelegateExpr(TryExpr* expr) {
  string_view label = FindLabelByVar(&expr->delegate_target);
  UseNameForVar(label, &expr->delegate_target);
  return Result::Ok;
}

Result NameApplier::OnThrowExpr(ThrowExpr* expr) {
  CHECK_RESULT(UseNameForEventVar(&expr->var));
  return Result::Ok;
}

Result NameApplier::OnRethrowExpr(RethrowExpr* expr) {
  string_view label = FindLabelByVar(&expr->var);
  UseNameForVar(label, &expr->var);
  return Result::Ok;
}

Result NameApplier::OnCallExpr(CallExpr* expr) {
  CHECK_RESULT(UseNameForFuncVar(&expr->var));
  return Result::Ok;
}

Result NameApplier::OnRefFuncExpr(RefFuncExpr* expr) {
  CHECK_RESULT(UseNameForFuncVar(&expr->var));
  return Result::Ok;
}

Result NameApplier::OnCallIndirectExpr(CallIndirectExpr* expr) {
  if (expr->decl.has_func_type) {
    CHECK_RESULT(UseNameForFuncTypeVar(&expr->decl.type_var));
  }
  CHECK_RESULT(UseNameForTableVar(&expr->table));
  return Result::Ok;
}

Result NameApplier::OnReturnCallExpr(ReturnCallExpr* expr) {
  CHECK_RESULT(UseNameForFuncVar(&expr->var));
  return Result::Ok;
}

Result NameApplier::OnReturnCallIndirectExpr(ReturnCallIndirectExpr* expr) {
  if (expr->decl.has_func_type) {
    CHECK_RESULT(UseNameForFuncTypeVar(&expr->decl.type_var));
  }
  CHECK_RESULT(UseNameForTableVar(&expr->table));
  return Result::Ok;
}

Result NameApplier::OnGlobalGetExpr(GlobalGetExpr* expr) {
  CHECK_RESULT(UseNameForGlobalVar(&expr->var));
  return Result::Ok;
}

Result NameApplier::OnLocalGetExpr(LocalGetExpr* expr) {
  CHECK_RESULT(UseNameForParamAndLocalVar(current_func_, &expr->var));
  return Result::Ok;
}

Result NameApplier::BeginIfExpr(IfExpr* expr) {
  PushLabel(expr->true_.label);
  return Result::Ok;
}

Result NameApplier::EndIfExpr(IfExpr* expr) {
  PopLabel();
  return Result::Ok;
}

Result NameApplier::OnGlobalSetExpr(GlobalSetExpr* expr) {
  CHECK_RESULT(UseNameForGlobalVar(&expr->var));
  return Result::Ok;
}

Result NameApplier::OnLocalSetExpr(LocalSetExpr* expr) {
  CHECK_RESULT(UseNameForParamAndLocalVar(current_func_, &expr->var));
  return Result::Ok;
}

Result NameApplier::OnLocalTeeExpr(LocalTeeExpr* expr) {
  CHECK_RESULT(UseNameForParamAndLocalVar(current_func_, &expr->var));
  return Result::Ok;
}

Result NameApplier::VisitFunc(Index func_index, Func* func) {
  current_func_ = func;
  if (func->decl.has_func_type) {
    CHECK_RESULT(UseNameForFuncTypeVar(&func->decl.type_var));
  }

  MakeTypeBindingReverseMapping(func->GetNumParamsAndLocals(), func->bindings,
                                &param_and_local_index_to_name_);

  CHECK_RESULT(visitor_.VisitFunc(func));
  current_func_ = nullptr;
  return Result::Ok;
}

Result NameApplier::VisitGlobal(Global* global) {
  CHECK_RESULT(visitor_.VisitExprList(global->init_expr));
  return Result::Ok;
}

Result NameApplier::VisitEvent(Event* event) {
  if (event->decl.has_func_type) {
    CHECK_RESULT(UseNameForFuncTypeVar(&event->decl.type_var));
  }
  return Result::Ok;
}

Result NameApplier::VisitExport(Index export_index, Export* export_) {
  if (export_->kind == ExternalKind::Func) {
    UseNameForFuncVar(&export_->var);
  }
  return Result::Ok;
}

Result NameApplier::VisitElemSegment(Index elem_segment_index,
                                     ElemSegment* segment) {
  CHECK_RESULT(UseNameForTableVar(&segment->table_var));
  CHECK_RESULT(visitor_.VisitExprList(segment->offset));
  for (ElemExpr& elem_expr : segment->elem_exprs) {
    if (elem_expr.kind == ElemExprKind::RefFunc) {
      CHECK_RESULT(UseNameForFuncVar(&elem_expr.var));
    }
  }
  return Result::Ok;
}

Result NameApplier::VisitDataSegment(Index data_segment_index,
                                     DataSegment* segment) {
  CHECK_RESULT(UseNameForMemoryVar(&segment->memory_var));
  CHECK_RESULT(visitor_.VisitExprList(segment->offset));
  return Result::Ok;
}

Result NameApplier::VisitStart(Var* start_var) {
  CHECK_RESULT(UseNameForFuncVar(start_var));
  return Result::Ok;
}

Result NameApplier::VisitModule(Module* module) {
  module_ = module;
  for (size_t i = 0; i < module->funcs.size(); ++i)
    CHECK_RESULT(VisitFunc(i, module->funcs[i]));
  for (size_t i = 0; i < module->globals.size(); ++i)
    CHECK_RESULT(VisitGlobal(module->globals[i]));
  for (size_t i = 0; i < module->events.size(); ++i)
    CHECK_RESULT(VisitEvent(module->events[i]));
  for (size_t i = 0; i < module->exports.size(); ++i)
    CHECK_RESULT(VisitExport(i, module->exports[i]));
  for (size_t i = 0; i < module->elem_segments.size(); ++i)
    CHECK_RESULT(VisitElemSegment(i, module->elem_segments[i]));
  for (size_t i = 0; i < module->data_segments.size(); ++i)
    CHECK_RESULT(VisitDataSegment(i, module->data_segments[i]));
  for (size_t i = 0; i < module->starts.size(); ++i)
    CHECK_RESULT(VisitStart(module->starts[i]));
  module_ = nullptr;
  return Result::Ok;
}

}  // end anonymous namespace

Result ApplyNames(Module* module) {
  NameApplier applier;
  return applier.VisitModule(module);
}

}  // namespace wabt
