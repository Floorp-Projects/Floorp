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

#include "wabt/resolve-names.h"

#include <cassert>
#include <cstdio>

#include "wabt/cast.h"
#include "wabt/expr-visitor.h"
#include "wabt/ir.h"
#include "wabt/wast-lexer.h"

namespace wabt {

namespace {

class NameResolver : public ExprVisitor::DelegateNop {
 public:
  NameResolver(Script* script, Errors* errors);

  Result VisitModule(Module* module);
  Result VisitScript(Script* script);

  // Implementation of ExprVisitor::DelegateNop.
  Result BeginBlockExpr(BlockExpr*) override;
  Result EndBlockExpr(BlockExpr*) override;
  Result OnBrExpr(BrExpr*) override;
  Result OnBrIfExpr(BrIfExpr*) override;
  Result OnBrTableExpr(BrTableExpr*) override;
  Result OnCallExpr(CallExpr*) override;
  Result OnCallIndirectExpr(CallIndirectExpr*) override;
  Result OnCatchExpr(TryExpr*, Catch*) override;
  Result OnDelegateExpr(TryExpr*) override;
  Result OnReturnCallExpr(ReturnCallExpr*) override;
  Result OnReturnCallIndirectExpr(ReturnCallIndirectExpr*) override;
  Result OnGlobalGetExpr(GlobalGetExpr*) override;
  Result OnGlobalSetExpr(GlobalSetExpr*) override;
  Result BeginIfExpr(IfExpr*) override;
  Result EndIfExpr(IfExpr*) override;
  Result OnLoadExpr(LoadExpr*) override;
  Result OnLocalGetExpr(LocalGetExpr*) override;
  Result OnLocalSetExpr(LocalSetExpr*) override;
  Result OnLocalTeeExpr(LocalTeeExpr*) override;
  Result BeginLoopExpr(LoopExpr*) override;
  Result EndLoopExpr(LoopExpr*) override;
  Result OnMemoryCopyExpr(MemoryCopyExpr*) override;
  Result OnDataDropExpr(DataDropExpr*) override;
  Result OnMemoryFillExpr(MemoryFillExpr*) override;
  Result OnMemoryGrowExpr(MemoryGrowExpr*) override;
  Result OnMemoryInitExpr(MemoryInitExpr*) override;
  Result OnMemorySizeExpr(MemorySizeExpr*) override;
  Result OnElemDropExpr(ElemDropExpr*) override;
  Result OnTableCopyExpr(TableCopyExpr*) override;
  Result OnTableInitExpr(TableInitExpr*) override;
  Result OnTableGetExpr(TableGetExpr*) override;
  Result OnTableSetExpr(TableSetExpr*) override;
  Result OnTableGrowExpr(TableGrowExpr*) override;
  Result OnTableSizeExpr(TableSizeExpr*) override;
  Result OnTableFillExpr(TableFillExpr*) override;
  Result OnRefFuncExpr(RefFuncExpr*) override;
  Result OnStoreExpr(StoreExpr*) override;
  Result BeginTryExpr(TryExpr*) override;
  Result EndTryExpr(TryExpr*) override;
  Result OnThrowExpr(ThrowExpr*) override;
  Result OnRethrowExpr(RethrowExpr*) override;
  Result OnSimdLoadLaneExpr(SimdLoadLaneExpr*) override;
  Result OnSimdStoreLaneExpr(SimdStoreLaneExpr*) override;

 private:
  void PrintError(const Location* loc, const char* fmt, ...);
  void PushLabel(const std::string& label);
  void PopLabel();
  void CheckDuplicateBindings(const BindingHash* bindings, const char* desc);
  void PrintDuplicateBindingsError(const BindingHash::value_type&,
                                   const BindingHash::value_type&,
                                   const char* desc);
  void ResolveLabelVar(Var* var);
  void ResolveVar(const BindingHash* bindings, Var* var, const char* desc);
  void ResolveFuncVar(Var* var);
  void ResolveGlobalVar(Var* var);
  void ResolveFuncTypeVar(Var* var);
  void ResolveTableVar(Var* var);
  void ResolveMemoryVar(Var* var);
  void ResolveTagVar(Var* var);
  void ResolveDataSegmentVar(Var* var);
  void ResolveElemSegmentVar(Var* var);
  void ResolveLocalVar(Var* var);
  void ResolveBlockDeclarationVar(BlockDeclaration* decl);
  void VisitFunc(Func* func);
  void VisitExport(Export* export_);
  void VisitGlobal(Global* global);
  void VisitTag(Tag* tag);
  void VisitElemSegment(ElemSegment* segment);
  void VisitDataSegment(DataSegment* segment);
  void VisitScriptModule(ScriptModule* script_module);
  void VisitCommand(Command* command);

  Errors* errors_ = nullptr;
  Script* script_ = nullptr;
  Module* current_module_ = nullptr;
  Func* current_func_ = nullptr;
  ExprVisitor visitor_;
  std::vector<std::string> labels_;
  Result result_ = Result::Ok;
};

NameResolver::NameResolver(Script* script, Errors* errors)
    : errors_(errors), script_(script), visitor_(this) {}

}  // end anonymous namespace

void WABT_PRINTF_FORMAT(3, 4) NameResolver::PrintError(const Location* loc,
                                                       const char* format,
                                                       ...) {
  result_ = Result::Error;
  WABT_SNPRINTF_ALLOCA(buffer, length, format);
  errors_->emplace_back(ErrorLevel::Error, *loc, buffer);
}

void NameResolver::PushLabel(const std::string& label) {
  labels_.push_back(label);
}

void NameResolver::PopLabel() {
  labels_.pop_back();
}

void NameResolver::CheckDuplicateBindings(const BindingHash* bindings,
                                          const char* desc) {
  bindings->FindDuplicates([this, desc](const BindingHash::value_type& a,
                                        const BindingHash::value_type& b) {
    PrintDuplicateBindingsError(a, b, desc);
  });
}

void NameResolver::PrintDuplicateBindingsError(const BindingHash::value_type& a,
                                               const BindingHash::value_type& b,
                                               const char* desc) {
  // Choose the location that is later in the file.
  const Location& a_loc = a.second.loc;
  const Location& b_loc = b.second.loc;
  const Location& loc = a_loc.line > b_loc.line ? a_loc : b_loc;
  PrintError(&loc, "redefinition of %s \"%s\"", desc, a.first.c_str());
}

void NameResolver::ResolveLabelVar(Var* var) {
  if (var->is_name()) {
    for (int i = labels_.size() - 1; i >= 0; --i) {
      const std::string& label = labels_[i];
      if (label == var->name()) {
        var->set_index(labels_.size() - i - 1);
        return;
      }
    }
    PrintError(&var->loc, "undefined label variable \"%s\"",
               var->name().c_str());
  }
}

void NameResolver::ResolveVar(const BindingHash* bindings,
                              Var* var,
                              const char* desc) {
  if (var->is_name()) {
    Index index = bindings->FindIndex(*var);
    if (index == kInvalidIndex) {
      PrintError(&var->loc, "undefined %s variable \"%s\"", desc,
                 var->name().c_str());
      return;
    }

    var->set_index(index);
  }
}

void NameResolver::ResolveFuncVar(Var* var) {
  ResolveVar(&current_module_->func_bindings, var, "function");
}

void NameResolver::ResolveGlobalVar(Var* var) {
  ResolveVar(&current_module_->global_bindings, var, "global");
}

void NameResolver::ResolveFuncTypeVar(Var* var) {
  ResolveVar(&current_module_->type_bindings, var, "type");
}

void NameResolver::ResolveTableVar(Var* var) {
  ResolveVar(&current_module_->table_bindings, var, "table");
}

void NameResolver::ResolveMemoryVar(Var* var) {
  ResolveVar(&current_module_->memory_bindings, var, "memory");
}

void NameResolver::ResolveTagVar(Var* var) {
  ResolveVar(&current_module_->tag_bindings, var, "tag");
}

void NameResolver::ResolveDataSegmentVar(Var* var) {
  ResolveVar(&current_module_->data_segment_bindings, var, "data segment");
}

void NameResolver::ResolveElemSegmentVar(Var* var) {
  ResolveVar(&current_module_->elem_segment_bindings, var, "elem segment");
}

void NameResolver::ResolveLocalVar(Var* var) {
  if (var->is_name()) {
    if (!current_func_) {
      return;
    }

    Index index = current_func_->GetLocalIndex(*var);
    if (index == kInvalidIndex) {
      PrintError(&var->loc, "undefined local variable \"%s\"",
                 var->name().c_str());
      return;
    }

    var->set_index(index);
  }
}

void NameResolver::ResolveBlockDeclarationVar(BlockDeclaration* decl) {
  if (decl->has_func_type) {
    ResolveFuncTypeVar(&decl->type_var);
  }
}

Result NameResolver::BeginBlockExpr(BlockExpr* expr) {
  PushLabel(expr->block.label);
  ResolveBlockDeclarationVar(&expr->block.decl);
  return Result::Ok;
}

Result NameResolver::EndBlockExpr(BlockExpr* expr) {
  PopLabel();
  return Result::Ok;
}

Result NameResolver::BeginLoopExpr(LoopExpr* expr) {
  PushLabel(expr->block.label);
  ResolveBlockDeclarationVar(&expr->block.decl);
  return Result::Ok;
}

Result NameResolver::EndLoopExpr(LoopExpr* expr) {
  PopLabel();
  return Result::Ok;
}

Result NameResolver::OnBrExpr(BrExpr* expr) {
  ResolveLabelVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnBrIfExpr(BrIfExpr* expr) {
  ResolveLabelVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnBrTableExpr(BrTableExpr* expr) {
  for (Var& target : expr->targets)
    ResolveLabelVar(&target);
  ResolveLabelVar(&expr->default_target);
  return Result::Ok;
}

Result NameResolver::OnCallExpr(CallExpr* expr) {
  ResolveFuncVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnCallIndirectExpr(CallIndirectExpr* expr) {
  if (expr->decl.has_func_type) {
    ResolveFuncTypeVar(&expr->decl.type_var);
  }
  ResolveTableVar(&expr->table);
  return Result::Ok;
}

Result NameResolver::OnReturnCallExpr(ReturnCallExpr* expr) {
  ResolveFuncVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnReturnCallIndirectExpr(ReturnCallIndirectExpr* expr) {
  if (expr->decl.has_func_type) {
    ResolveFuncTypeVar(&expr->decl.type_var);
  }
  ResolveTableVar(&expr->table);
  return Result::Ok;
}

Result NameResolver::OnGlobalGetExpr(GlobalGetExpr* expr) {
  ResolveGlobalVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnGlobalSetExpr(GlobalSetExpr* expr) {
  ResolveGlobalVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::BeginIfExpr(IfExpr* expr) {
  PushLabel(expr->true_.label);
  ResolveBlockDeclarationVar(&expr->true_.decl);
  return Result::Ok;
}

Result NameResolver::EndIfExpr(IfExpr* expr) {
  PopLabel();
  return Result::Ok;
}

Result NameResolver::OnLoadExpr(LoadExpr* expr) {
  ResolveMemoryVar(&expr->memidx);
  return Result::Ok;
}

Result NameResolver::OnLocalGetExpr(LocalGetExpr* expr) {
  ResolveLocalVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnLocalSetExpr(LocalSetExpr* expr) {
  ResolveLocalVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnLocalTeeExpr(LocalTeeExpr* expr) {
  ResolveLocalVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnMemoryCopyExpr(MemoryCopyExpr* expr) {
  ResolveMemoryVar(&expr->srcmemidx);
  ResolveMemoryVar(&expr->destmemidx);
  return Result::Ok;
}

Result NameResolver::OnDataDropExpr(DataDropExpr* expr) {
  ResolveDataSegmentVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnMemoryFillExpr(MemoryFillExpr* expr) {
  ResolveMemoryVar(&expr->memidx);
  return Result::Ok;
}

Result NameResolver::OnMemoryGrowExpr(MemoryGrowExpr* expr) {
  ResolveMemoryVar(&expr->memidx);
  return Result::Ok;
}

Result NameResolver::OnMemoryInitExpr(MemoryInitExpr* expr) {
  ResolveDataSegmentVar(&expr->var);
  ResolveMemoryVar(&expr->memidx);
  return Result::Ok;
}

Result NameResolver::OnMemorySizeExpr(MemorySizeExpr* expr) {
  ResolveMemoryVar(&expr->memidx);
  return Result::Ok;
}

Result NameResolver::OnElemDropExpr(ElemDropExpr* expr) {
  ResolveElemSegmentVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnTableCopyExpr(TableCopyExpr* expr) {
  ResolveTableVar(&expr->dst_table);
  ResolveTableVar(&expr->src_table);
  return Result::Ok;
}

Result NameResolver::OnTableInitExpr(TableInitExpr* expr) {
  ResolveElemSegmentVar(&expr->segment_index);
  ResolveTableVar(&expr->table_index);
  return Result::Ok;
}

Result NameResolver::OnTableGetExpr(TableGetExpr* expr) {
  ResolveTableVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnTableSetExpr(TableSetExpr* expr) {
  ResolveTableVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnTableGrowExpr(TableGrowExpr* expr) {
  ResolveTableVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnTableSizeExpr(TableSizeExpr* expr) {
  ResolveTableVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnTableFillExpr(TableFillExpr* expr) {
  ResolveTableVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnRefFuncExpr(RefFuncExpr* expr) {
  ResolveFuncVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnStoreExpr(StoreExpr* expr) {
  ResolveMemoryVar(&expr->memidx);
  return Result::Ok;
}

Result NameResolver::BeginTryExpr(TryExpr* expr) {
  PushLabel(expr->block.label);
  ResolveBlockDeclarationVar(&expr->block.decl);
  return Result::Ok;
}

Result NameResolver::EndTryExpr(TryExpr*) {
  PopLabel();
  return Result::Ok;
}

Result NameResolver::OnCatchExpr(TryExpr*, Catch* catch_) {
  if (!catch_->IsCatchAll()) {
    ResolveTagVar(&catch_->var);
  }
  return Result::Ok;
}

Result NameResolver::OnDelegateExpr(TryExpr* expr) {
  // Pop the label here as a try-delegate has no `end` instruction.
  PopLabel();

  // We resolve *after* popping the label in order to ensure that the
  // delegate label starts counting after the current try-delegate.
  ResolveLabelVar(&expr->delegate_target);

  return Result::Ok;
}

Result NameResolver::OnThrowExpr(ThrowExpr* expr) {
  ResolveTagVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnRethrowExpr(RethrowExpr* expr) {
  // Note: the variable refers to corresponding (enclosing) catch, using the try
  // block label for context.
  ResolveLabelVar(&expr->var);
  return Result::Ok;
}

Result NameResolver::OnSimdLoadLaneExpr(SimdLoadLaneExpr* expr) {
  ResolveMemoryVar(&expr->memidx);
  return Result::Ok;
}

Result NameResolver::OnSimdStoreLaneExpr(SimdStoreLaneExpr* expr) {
  ResolveMemoryVar(&expr->memidx);
  return Result::Ok;
}

void NameResolver::VisitFunc(Func* func) {
  current_func_ = func;
  if (func->decl.has_func_type) {
    ResolveFuncTypeVar(&func->decl.type_var);
  }

  func->bindings.FindDuplicates(
      [=](const BindingHash::value_type& a, const BindingHash::value_type& b) {
        const char* desc =
            (a.second.index < func->GetNumParams()) ? "parameter" : "local";
        PrintDuplicateBindingsError(a, b, desc);
      });

  visitor_.VisitFunc(func);
  current_func_ = nullptr;
}

void NameResolver::VisitExport(Export* export_) {
  switch (export_->kind) {
    case ExternalKind::Func:
      ResolveFuncVar(&export_->var);
      break;

    case ExternalKind::Table:
      ResolveTableVar(&export_->var);
      break;

    case ExternalKind::Memory:
      ResolveMemoryVar(&export_->var);
      break;

    case ExternalKind::Global:
      ResolveGlobalVar(&export_->var);
      break;

    case ExternalKind::Tag:
      ResolveTagVar(&export_->var);
      break;
  }
}

void NameResolver::VisitGlobal(Global* global) {
  visitor_.VisitExprList(global->init_expr);
}

void NameResolver::VisitTag(Tag* tag) {
  if (tag->decl.has_func_type) {
    ResolveFuncTypeVar(&tag->decl.type_var);
  }
}

void NameResolver::VisitElemSegment(ElemSegment* segment) {
  ResolveTableVar(&segment->table_var);
  visitor_.VisitExprList(segment->offset);
  for (ExprList& elem_expr : segment->elem_exprs) {
    if (elem_expr.size() == 1 &&
        elem_expr.front().type() == ExprType::RefFunc) {
      ResolveFuncVar(&cast<RefFuncExpr>(&elem_expr.front())->var);
    }
  }
}

void NameResolver::VisitDataSegment(DataSegment* segment) {
  ResolveMemoryVar(&segment->memory_var);
  visitor_.VisitExprList(segment->offset);
}

Result NameResolver::VisitModule(Module* module) {
  current_module_ = module;
  CheckDuplicateBindings(&module->elem_segment_bindings, "elem");
  CheckDuplicateBindings(&module->func_bindings, "function");
  CheckDuplicateBindings(&module->global_bindings, "global");
  CheckDuplicateBindings(&module->type_bindings, "type");
  CheckDuplicateBindings(&module->table_bindings, "table");
  CheckDuplicateBindings(&module->memory_bindings, "memory");
  CheckDuplicateBindings(&module->tag_bindings, "tag");

  for (Func* func : module->funcs)
    VisitFunc(func);
  for (Export* export_ : module->exports)
    VisitExport(export_);
  for (Global* global : module->globals)
    VisitGlobal(global);
  for (Tag* tag : module->tags)
    VisitTag(tag);
  for (ElemSegment* elem_segment : module->elem_segments)
    VisitElemSegment(elem_segment);
  for (DataSegment* data_segment : module->data_segments)
    VisitDataSegment(data_segment);
  for (Var* start : module->starts)
    ResolveFuncVar(start);
  current_module_ = nullptr;
  return result_;
}

void NameResolver::VisitScriptModule(ScriptModule* script_module) {
  if (auto* tsm = dyn_cast<TextScriptModule>(script_module)) {
    VisitModule(&tsm->module);
  }
}

void NameResolver::VisitCommand(Command* command) {
  switch (command->type) {
    case CommandType::Module:
      VisitModule(&cast<ModuleCommand>(command)->module);
      break;

    case CommandType::ScriptModule:
      VisitModule(&cast<ScriptModuleCommand>(command)->module);
      break;

    case CommandType::Action:
    case CommandType::AssertReturn:
    case CommandType::AssertTrap:
    case CommandType::AssertExhaustion:
    case CommandType::AssertException:
    case CommandType::Register:
      /* Don't resolve a module_var, since it doesn't really behave like other
       * vars. You can't reference a module by index. */
      break;

    case CommandType::AssertMalformed:
      /* Malformed modules should not be text; the whole point of this
       * assertion is to test for malformed binary modules. */
      break;

    case CommandType::AssertInvalid: {
      auto* assert_invalid_command = cast<AssertInvalidCommand>(command);
      /* The module may be invalid because the names cannot be resolved; we
       * don't want to print errors or fail if that's the case, but we still
       * should try to resolve names when possible. */
      Errors errors;
      NameResolver new_resolver(script_, &errors);
      new_resolver.VisitScriptModule(assert_invalid_command->module.get());
      break;
    }

    case CommandType::AssertUnlinkable:
      VisitScriptModule(cast<AssertUnlinkableCommand>(command)->module.get());
      break;

    case CommandType::AssertUninstantiable:
      VisitScriptModule(
          cast<AssertUninstantiableCommand>(command)->module.get());
      break;
  }
}

Result NameResolver::VisitScript(Script* script) {
  for (const std::unique_ptr<Command>& command : script->commands)
    VisitCommand(command.get());
  return result_;
}

Result ResolveNamesModule(Module* module, Errors* errors) {
  NameResolver resolver(nullptr, errors);
  return resolver.VisitModule(module);
}

Result ResolveNamesScript(Script* script, Errors* errors) {
  NameResolver resolver(script, errors);
  return resolver.VisitScript(script);
}

}  // namespace wabt
