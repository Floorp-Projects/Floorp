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

#include "wabt/binary-reader-ir.h"

#include <cassert>
#include <cinttypes>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "wabt/binary-reader-nop.h"
#include "wabt/cast.h"
#include "wabt/common.h"
#include "wabt/ir.h"

namespace wabt {

namespace {

struct LabelNode {
  LabelNode(LabelType, ExprList* exprs, Expr* context = nullptr);

  LabelType label_type;
  ExprList* exprs;
  Expr* context;
};

LabelNode::LabelNode(LabelType label_type, ExprList* exprs, Expr* context)
    : label_type(label_type), exprs(exprs), context(context) {}

class CodeMetadataExprQueue {
 private:
  struct Entry {
    Func* func;
    std::deque<std::unique_ptr<CodeMetadataExpr>> func_queue;
    Entry(Func* f) : func(f) {}
  };
  std::deque<Entry> entries;

 public:
  CodeMetadataExprQueue() {}
  void push_func(Func* f) { entries.emplace_back(f); }
  void push_metadata(std::unique_ptr<CodeMetadataExpr> meta) {
    assert(!entries.empty());
    entries.back().func_queue.push_back(std::move(meta));
  }

  std::unique_ptr<CodeMetadataExpr> pop_match(Func* f, Offset offset) {
    std::unique_ptr<CodeMetadataExpr> ret;
    if (entries.empty()) {
      return ret;
    }

    auto& current_entry = entries.front();

    if (current_entry.func != f)
      return ret;
    if (current_entry.func_queue.empty()) {
      entries.pop_front();
      return ret;
    }

    auto& current_metadata = current_entry.func_queue.front();
    if (current_metadata->loc.offset + current_entry.func->loc.offset !=
        offset) {
      return ret;
    }

    current_metadata->loc = Location(offset);
    ret = std::move(current_metadata);
    current_entry.func_queue.pop_front();

    return ret;
  }
};

class BinaryReaderIR : public BinaryReaderNop {
  static constexpr size_t kMaxNestingDepth = 16384;  // max depth of label stack
  static constexpr size_t kMaxFunctionLocals = 50000;  // matches V8
  static constexpr size_t kMaxFunctionParams = 1000;   // matches V8
  static constexpr size_t kMaxFunctionResults = 1000;  // matches V8

 public:
  BinaryReaderIR(Module* out_module, const char* filename, Errors* errors);

  bool OnError(const Error&) override;

  Result OnTypeCount(Index count) override;
  Result OnFuncType(Index index,
                    Index param_count,
                    Type* param_types,
                    Index result_count,
                    Type* result_types) override;
  Result OnStructType(Index index, Index field_count, TypeMut* fields) override;
  Result OnArrayType(Index index, TypeMut field) override;

  Result OnImportCount(Index count) override;
  Result OnImportFunc(Index import_index,
                      std::string_view module_name,
                      std::string_view field_name,
                      Index func_index,
                      Index sig_index) override;
  Result OnImportTable(Index import_index,
                       std::string_view module_name,
                       std::string_view field_name,
                       Index table_index,
                       Type elem_type,
                       const Limits* elem_limits) override;
  Result OnImportMemory(Index import_index,
                        std::string_view module_name,
                        std::string_view field_name,
                        Index memory_index,
                        const Limits* page_limits) override;
  Result OnImportGlobal(Index import_index,
                        std::string_view module_name,
                        std::string_view field_name,
                        Index global_index,
                        Type type,
                        bool mutable_) override;
  Result OnImportTag(Index import_index,
                     std::string_view module_name,
                     std::string_view field_name,
                     Index tag_index,
                     Index sig_index) override;

  Result OnFunctionCount(Index count) override;
  Result OnFunction(Index index, Index sig_index) override;

  Result OnTableCount(Index count) override;
  Result OnTable(Index index,
                 Type elem_type,
                 const Limits* elem_limits) override;

  Result OnMemoryCount(Index count) override;
  Result OnMemory(Index index, const Limits* limits) override;

  Result OnGlobalCount(Index count) override;
  Result BeginGlobal(Index index, Type type, bool mutable_) override;
  Result BeginGlobalInitExpr(Index index) override;
  Result EndGlobalInitExpr(Index index) override;

  Result OnExportCount(Index count) override;
  Result OnExport(Index index,
                  ExternalKind kind,
                  Index item_index,
                  std::string_view name) override;

  Result OnStartFunction(Index func_index) override;

  Result OnFunctionBodyCount(Index count) override;
  Result BeginFunctionBody(Index index, Offset size) override;
  Result OnLocalDecl(Index decl_index, Index count, Type type) override;

  Result OnOpcode(Opcode opcode) override;
  Result OnAtomicLoadExpr(Opcode opcode,
                          Index memidx,
                          Address alignment_log2,
                          Address offset) override;
  Result OnAtomicStoreExpr(Opcode opcode,
                           Index memidx,
                           Address alignment_log2,
                           Address offset) override;
  Result OnAtomicRmwExpr(Opcode opcode,
                         Index memidx,
                         Address alignment_log2,
                         Address offset) override;
  Result OnAtomicRmwCmpxchgExpr(Opcode opcode,
                                Index memidx,
                                Address alignment_log2,
                                Address offset) override;
  Result OnAtomicWaitExpr(Opcode opcode,
                          Index memidx,
                          Address alignment_log2,
                          Address offset) override;
  Result OnAtomicFenceExpr(uint32_t consistency_model) override;
  Result OnAtomicNotifyExpr(Opcode opcode,
                            Index memidx,
                            Address alignment_log2,
                            Address offset) override;
  Result OnBinaryExpr(Opcode opcode) override;
  Result OnBlockExpr(Type sig_type) override;
  Result OnBrExpr(Index depth) override;
  Result OnBrIfExpr(Index depth) override;
  Result OnBrTableExpr(Index num_targets,
                       Index* target_depths,
                       Index default_target_depth) override;
  Result OnCallExpr(Index func_index) override;
  Result OnCatchExpr(Index tag_index) override;
  Result OnCatchAllExpr() override;
  Result OnCallIndirectExpr(Index sig_index, Index table_index) override;
  Result OnCallRefExpr() override;
  Result OnReturnCallExpr(Index func_index) override;
  Result OnReturnCallIndirectExpr(Index sig_index, Index table_index) override;
  Result OnCompareExpr(Opcode opcode) override;
  Result OnConvertExpr(Opcode opcode) override;
  Result OnDelegateExpr(Index depth) override;
  Result OnDropExpr() override;
  Result OnElseExpr() override;
  Result OnEndExpr() override;
  Result OnF32ConstExpr(uint32_t value_bits) override;
  Result OnF64ConstExpr(uint64_t value_bits) override;
  Result OnV128ConstExpr(v128 value_bits) override;
  Result OnGlobalGetExpr(Index global_index) override;
  Result OnGlobalSetExpr(Index global_index) override;
  Result OnI32ConstExpr(uint32_t value) override;
  Result OnI64ConstExpr(uint64_t value) override;
  Result OnIfExpr(Type sig_type) override;
  Result OnLoadExpr(Opcode opcode,
                    Index memidx,
                    Address alignment_log2,
                    Address offset) override;
  Result OnLocalGetExpr(Index local_index) override;
  Result OnLocalSetExpr(Index local_index) override;
  Result OnLocalTeeExpr(Index local_index) override;
  Result OnLoopExpr(Type sig_type) override;
  Result OnMemoryCopyExpr(Index srcmemidx, Index destmemidx) override;
  Result OnDataDropExpr(Index segment_index) override;
  Result OnMemoryFillExpr(Index memidx) override;
  Result OnMemoryGrowExpr(Index memidx) override;
  Result OnMemoryInitExpr(Index segment_index, Index memidx) override;
  Result OnMemorySizeExpr(Index memidx) override;
  Result OnTableCopyExpr(Index dst_index, Index src_index) override;
  Result OnElemDropExpr(Index segment_index) override;
  Result OnTableInitExpr(Index segment_index, Index table_index) override;
  Result OnTableGetExpr(Index table_index) override;
  Result OnTableSetExpr(Index table_index) override;
  Result OnTableGrowExpr(Index table_index) override;
  Result OnTableSizeExpr(Index table_index) override;
  Result OnTableFillExpr(Index table_index) override;
  Result OnRefFuncExpr(Index func_index) override;
  Result OnRefNullExpr(Type type) override;
  Result OnRefIsNullExpr() override;
  Result OnNopExpr() override;
  Result OnRethrowExpr(Index depth) override;
  Result OnReturnExpr() override;
  Result OnSelectExpr(Index result_count, Type* result_types) override;
  Result OnStoreExpr(Opcode opcode,
                     Index memidx,
                     Address alignment_log2,
                     Address offset) override;
  Result OnThrowExpr(Index tag_index) override;
  Result OnTryExpr(Type sig_type) override;
  Result OnUnaryExpr(Opcode opcode) override;
  Result OnTernaryExpr(Opcode opcode) override;
  Result OnUnreachableExpr() override;
  Result EndFunctionBody(Index index) override;
  Result OnSimdLaneOpExpr(Opcode opcode, uint64_t value) override;
  Result OnSimdLoadLaneExpr(Opcode opcode,
                            Index memidx,
                            Address alignment_log2,
                            Address offset,
                            uint64_t value) override;
  Result OnSimdStoreLaneExpr(Opcode opcode,
                             Index memidx,
                             Address alignment_log2,
                             Address offset,
                             uint64_t value) override;
  Result OnSimdShuffleOpExpr(Opcode opcode, v128 value) override;
  Result OnLoadSplatExpr(Opcode opcode,
                         Index memidx,
                         Address alignment_log2,
                         Address offset) override;
  Result OnLoadZeroExpr(Opcode opcode,
                        Index memidx,
                        Address alignment_log2,
                        Address offset) override;

  Result OnElemSegmentCount(Index count) override;
  Result BeginElemSegment(Index index,
                          Index table_index,
                          uint8_t flags) override;
  Result BeginElemSegmentInitExpr(Index index) override;
  Result EndElemSegmentInitExpr(Index index) override;
  Result OnElemSegmentElemType(Index index, Type elem_type) override;
  Result OnElemSegmentElemExprCount(Index index, Index count) override;
  Result OnElemSegmentElemExpr_RefNull(Index segment_index, Type type) override;
  Result OnElemSegmentElemExpr_RefFunc(Index segment_index,
                                       Index func_index) override;

  Result OnDataSegmentCount(Index count) override;
  Result BeginDataSegment(Index index,
                          Index memory_index,
                          uint8_t flags) override;
  Result BeginDataSegmentInitExpr(Index index) override;
  Result EndDataSegmentInitExpr(Index index) override;
  Result OnDataSegmentData(Index index,
                           const void* data,
                           Address size) override;

  Result OnModuleName(std::string_view module_name) override;
  Result OnFunctionNamesCount(Index num_functions) override;
  Result OnFunctionName(Index function_index,
                        std::string_view function_name) override;
  Result OnLocalNameLocalCount(Index function_index, Index num_locals) override;
  Result OnLocalName(Index function_index,
                     Index local_index,
                     std::string_view local_name) override;
  Result OnNameEntry(NameSectionSubsection type,
                     Index index,
                     std::string_view name) override;

  Result BeginTagSection(Offset size) override { return Result::Ok; }
  Result OnTagCount(Index count) override { return Result::Ok; }
  Result OnTagType(Index index, Index sig_index) override;
  Result EndTagSection() override { return Result::Ok; }

  Result OnDataSymbol(Index index,
                      uint32_t flags,
                      std::string_view name,
                      Index segment,
                      uint32_t offset,
                      uint32_t size) override;
  Result OnFunctionSymbol(Index index,
                          uint32_t flags,
                          std::string_view name,
                          Index func_index) override;
  Result OnGlobalSymbol(Index index,
                        uint32_t flags,
                        std::string_view name,
                        Index global_index) override;
  Result OnSectionSymbol(Index index,
                         uint32_t flags,
                         Index section_index) override;
  /* Code Metadata sections */
  Result BeginCodeMetadataSection(std::string_view name, Offset size) override;
  Result OnCodeMetadataFuncCount(Index count) override;
  Result OnCodeMetadataCount(Index function_index, Index count) override;
  Result OnCodeMetadata(Offset offset, const void* data, Address size) override;

  Result OnTagSymbol(Index index,
                     uint32_t flags,
                     std::string_view name,
                     Index tag_index) override;
  Result OnTableSymbol(Index index,
                       uint32_t flags,
                       std::string_view name,
                       Index table_index) override;

 private:
  Location GetLocation() const;
  void PrintError(const char* format, ...);
  Result PushLabel(LabelType label_type,
                   ExprList* first,
                   Expr* context = nullptr);
  Result BeginInitExpr(ExprList* init_expr);
  Result EndInitExpr();
  Result PopLabel();
  Result GetLabelAt(LabelNode** label, Index depth);
  Result TopLabel(LabelNode** label);
  Result TopLabelExpr(LabelNode** label, Expr** expr);
  Result AppendExpr(std::unique_ptr<Expr> expr);
  Result AppendCatch(Catch&& catch_);
  void SetFuncDeclaration(FuncDeclaration* decl, Var var);
  void SetBlockDeclaration(BlockDeclaration* decl, Type sig_type);
  Result SetMemoryName(Index index, std::string_view name);
  Result SetTableName(Index index, std::string_view name);
  Result SetFunctionName(Index index, std::string_view name);
  Result SetTypeName(Index index, std::string_view name);
  Result SetGlobalName(Index index, std::string_view name);
  Result SetDataSegmentName(Index index, std::string_view name);
  Result SetElemSegmentName(Index index, std::string_view name);
  Result SetTagName(Index index, std::string_view name);

  std::string GetUniqueName(BindingHash* bindings,
                            const std::string& original_name);

  Errors* errors_ = nullptr;
  Module* module_ = nullptr;

  Func* current_func_ = nullptr;
  std::vector<LabelNode> label_stack_;
  const char* filename_;

  CodeMetadataExprQueue code_metadata_queue_;
  std::string_view current_metadata_name_;
};

BinaryReaderIR::BinaryReaderIR(Module* out_module,
                               const char* filename,
                               Errors* errors)
    : errors_(errors), module_(out_module), filename_(filename) {}

Location BinaryReaderIR::GetLocation() const {
  Location loc;
  loc.filename = filename_;
  loc.offset = state->offset;
  return loc;
}

void WABT_PRINTF_FORMAT(2, 3) BinaryReaderIR::PrintError(const char* format,
                                                         ...) {
  WABT_SNPRINTF_ALLOCA(buffer, length, format);
  errors_->emplace_back(ErrorLevel::Error, Location(kInvalidOffset), buffer);
}

Result BinaryReaderIR::PushLabel(LabelType label_type,
                                 ExprList* first,
                                 Expr* context) {
  if (label_stack_.size() >= kMaxNestingDepth) {
    PrintError("label stack exceeds max nesting depth");
    return Result::Error;
  }
  label_stack_.emplace_back(label_type, first, context);
  return Result::Ok;
}

Result BinaryReaderIR::PopLabel() {
  if (label_stack_.size() == 0) {
    PrintError("popping empty label stack");
    return Result::Error;
  }

  label_stack_.pop_back();
  return Result::Ok;
}

Result BinaryReaderIR::GetLabelAt(LabelNode** label, Index depth) {
  if (depth >= label_stack_.size()) {
    PrintError("accessing stack depth: %" PRIindex " >= max: %" PRIzd, depth,
               label_stack_.size());
    return Result::Error;
  }

  *label = &label_stack_[label_stack_.size() - depth - 1];
  return Result::Ok;
}

Result BinaryReaderIR::TopLabel(LabelNode** label) {
  return GetLabelAt(label, 0);
}

Result BinaryReaderIR::TopLabelExpr(LabelNode** label, Expr** expr) {
  CHECK_RESULT(TopLabel(label));
  LabelNode* parent_label;
  CHECK_RESULT(GetLabelAt(&parent_label, 1));
  if (parent_label->exprs->empty()) {
    PrintError("TopLabelExpr: parent label has empty expr list");
    return Result::Error;
  }
  *expr = &parent_label->exprs->back();
  return Result::Ok;
}

Result BinaryReaderIR::AppendExpr(std::unique_ptr<Expr> expr) {
  expr->loc = GetLocation();
  LabelNode* label;
  CHECK_RESULT(TopLabel(&label));
  label->exprs->push_back(std::move(expr));
  return Result::Ok;
}

void BinaryReaderIR::SetFuncDeclaration(FuncDeclaration* decl, Var var) {
  decl->has_func_type = true;
  decl->type_var = var;
  if (auto* func_type = module_->GetFuncType(var)) {
    decl->sig = func_type->sig;
  }
}

void BinaryReaderIR::SetBlockDeclaration(BlockDeclaration* decl,
                                         Type sig_type) {
  if (sig_type.IsIndex()) {
    Index type_index = sig_type.GetIndex();
    SetFuncDeclaration(decl, Var(type_index, GetLocation()));
  } else {
    decl->has_func_type = false;
    decl->sig.param_types.clear();
    decl->sig.result_types = sig_type.GetInlineVector();
  }
}

std::string BinaryReaderIR::GetUniqueName(BindingHash* bindings,
                                          const std::string& orig_name) {
  int counter = 1;
  std::string unique_name = orig_name;
  while (bindings->count(unique_name) != 0) {
    unique_name = orig_name + "." + std::to_string(counter++);
  }
  return unique_name;
}

bool BinaryReaderIR::OnError(const Error& error) {
  errors_->push_back(error);
  return true;
}

Result BinaryReaderIR::OnTypeCount(Index count) {
  WABT_TRY
  module_->types.reserve(count);
  WABT_CATCH_BAD_ALLOC
  return Result::Ok;
}

Result BinaryReaderIR::OnFuncType(Index index,
                                  Index param_count,
                                  Type* param_types,
                                  Index result_count,
                                  Type* result_types) {
  if (param_count > kMaxFunctionParams) {
    PrintError("FuncType param count exceeds maximum value");
    return Result::Error;
  }

  if (result_count > kMaxFunctionResults) {
    PrintError("FuncType result count exceeds maximum value");
    return Result::Error;
  }

  auto field = std::make_unique<TypeModuleField>(GetLocation());
  auto func_type = std::make_unique<FuncType>();
  func_type->sig.param_types.assign(param_types, param_types + param_count);
  func_type->sig.result_types.assign(result_types, result_types + result_count);

  module_->features_used.simd |=
      std::any_of(func_type->sig.param_types.begin(),
                  func_type->sig.param_types.end(),
                  [](auto x) { return x == Type::V128; }) ||
      std::any_of(func_type->sig.result_types.begin(),
                  func_type->sig.result_types.end(),
                  [](auto x) { return x == Type::V128; });

  field->type = std::move(func_type);
  module_->AppendField(std::move(field));
  return Result::Ok;
}

Result BinaryReaderIR::OnStructType(Index index,
                                    Index field_count,
                                    TypeMut* fields) {
  auto field = std::make_unique<TypeModuleField>(GetLocation());
  auto struct_type = std::make_unique<StructType>();
  struct_type->fields.resize(field_count);
  for (Index i = 0; i < field_count; ++i) {
    struct_type->fields[i].type = fields[i].type;
    struct_type->fields[i].mutable_ = fields[i].mutable_;
    module_->features_used.simd |= (fields[i].type == Type::V128);
  }
  field->type = std::move(struct_type);
  module_->AppendField(std::move(field));
  return Result::Ok;
}

Result BinaryReaderIR::OnArrayType(Index index, TypeMut type_mut) {
  auto field = std::make_unique<TypeModuleField>(GetLocation());
  auto array_type = std::make_unique<ArrayType>();
  array_type->field.type = type_mut.type;
  array_type->field.mutable_ = type_mut.mutable_;
  module_->features_used.simd |= (type_mut.type == Type::V128);
  field->type = std::move(array_type);
  module_->AppendField(std::move(field));
  return Result::Ok;
}

Result BinaryReaderIR::OnImportCount(Index count) {
  WABT_TRY
  module_->imports.reserve(count);
  WABT_CATCH_BAD_ALLOC
  return Result::Ok;
}

Result BinaryReaderIR::OnImportFunc(Index import_index,
                                    std::string_view module_name,
                                    std::string_view field_name,
                                    Index func_index,
                                    Index sig_index) {
  auto import = std::make_unique<FuncImport>();
  import->module_name = module_name;
  import->field_name = field_name;
  SetFuncDeclaration(&import->func.decl, Var(sig_index, GetLocation()));
  module_->AppendField(
      std::make_unique<ImportModuleField>(std::move(import), GetLocation()));
  return Result::Ok;
}

Result BinaryReaderIR::OnImportTable(Index import_index,
                                     std::string_view module_name,
                                     std::string_view field_name,
                                     Index table_index,
                                     Type elem_type,
                                     const Limits* elem_limits) {
  auto import = std::make_unique<TableImport>();
  import->module_name = module_name;
  import->field_name = field_name;
  import->table.elem_limits = *elem_limits;
  import->table.elem_type = elem_type;
  module_->AppendField(
      std::make_unique<ImportModuleField>(std::move(import), GetLocation()));
  return Result::Ok;
}

Result BinaryReaderIR::OnImportMemory(Index import_index,
                                      std::string_view module_name,
                                      std::string_view field_name,
                                      Index memory_index,
                                      const Limits* page_limits) {
  auto import = std::make_unique<MemoryImport>();
  import->module_name = module_name;
  import->field_name = field_name;
  import->memory.page_limits = *page_limits;
  module_->AppendField(
      std::make_unique<ImportModuleField>(std::move(import), GetLocation()));
  return Result::Ok;
}

Result BinaryReaderIR::OnImportGlobal(Index import_index,
                                      std::string_view module_name,
                                      std::string_view field_name,
                                      Index global_index,
                                      Type type,
                                      bool mutable_) {
  auto import = std::make_unique<GlobalImport>();
  import->module_name = module_name;
  import->field_name = field_name;
  import->global.type = type;
  import->global.mutable_ = mutable_;
  module_->AppendField(
      std::make_unique<ImportModuleField>(std::move(import), GetLocation()));
  module_->features_used.simd |= (type == Type::V128);
  return Result::Ok;
}

Result BinaryReaderIR::OnImportTag(Index import_index,
                                   std::string_view module_name,
                                   std::string_view field_name,
                                   Index tag_index,
                                   Index sig_index) {
  auto import = std::make_unique<TagImport>();
  import->module_name = module_name;
  import->field_name = field_name;
  SetFuncDeclaration(&import->tag.decl, Var(sig_index, GetLocation()));
  module_->AppendField(
      std::make_unique<ImportModuleField>(std::move(import), GetLocation()));
  module_->features_used.exceptions = true;
  return Result::Ok;
}

Result BinaryReaderIR::OnFunctionCount(Index count) {
  WABT_TRY
  module_->funcs.reserve(module_->num_func_imports + count);
  WABT_CATCH_BAD_ALLOC
  return Result::Ok;
}

Result BinaryReaderIR::OnFunction(Index index, Index sig_index) {
  auto field = std::make_unique<FuncModuleField>(GetLocation());
  Func& func = field->func;
  SetFuncDeclaration(&func.decl, Var(sig_index, GetLocation()));
  module_->AppendField(std::move(field));
  return Result::Ok;
}

Result BinaryReaderIR::OnTableCount(Index count) {
  WABT_TRY
  module_->tables.reserve(module_->num_table_imports + count);
  WABT_CATCH_BAD_ALLOC
  return Result::Ok;
}

Result BinaryReaderIR::OnTable(Index index,
                               Type elem_type,
                               const Limits* elem_limits) {
  auto field = std::make_unique<TableModuleField>(GetLocation());
  Table& table = field->table;
  table.elem_limits = *elem_limits;
  table.elem_type = elem_type;
  module_->AppendField(std::move(field));
  return Result::Ok;
}

Result BinaryReaderIR::OnMemoryCount(Index count) {
  WABT_TRY
  module_->memories.reserve(module_->num_memory_imports + count);
  WABT_CATCH_BAD_ALLOC
  return Result::Ok;
}

Result BinaryReaderIR::OnMemory(Index index, const Limits* page_limits) {
  auto field = std::make_unique<MemoryModuleField>(GetLocation());
  Memory& memory = field->memory;
  memory.page_limits = *page_limits;
  module_->AppendField(std::move(field));
  return Result::Ok;
}

Result BinaryReaderIR::OnGlobalCount(Index count) {
  WABT_TRY
  module_->globals.reserve(module_->num_global_imports + count);
  WABT_CATCH_BAD_ALLOC
  return Result::Ok;
}

Result BinaryReaderIR::BeginGlobal(Index index, Type type, bool mutable_) {
  auto field = std::make_unique<GlobalModuleField>(GetLocation());
  Global& global = field->global;
  global.type = type;
  global.mutable_ = mutable_;
  module_->AppendField(std::move(field));
  module_->features_used.simd |= (type == Type::V128);
  return Result::Ok;
}

Result BinaryReaderIR::BeginGlobalInitExpr(Index index) {
  assert(index == module_->globals.size() - 1);
  Global* global = module_->globals[index];
  return BeginInitExpr(&global->init_expr);
}

Result BinaryReaderIR::EndGlobalInitExpr(Index index) {
  return EndInitExpr();
}

Result BinaryReaderIR::OnExportCount(Index count) {
  WABT_TRY
  module_->exports.reserve(count);
  WABT_CATCH_BAD_ALLOC
  return Result::Ok;
}

Result BinaryReaderIR::OnExport(Index index,
                                ExternalKind kind,
                                Index item_index,
                                std::string_view name) {
  auto field = std::make_unique<ExportModuleField>(GetLocation());
  Export& export_ = field->export_;
  export_.name = name;
  export_.var = Var(item_index, GetLocation());
  export_.kind = kind;
  module_->AppendField(std::move(field));
  return Result::Ok;
}

Result BinaryReaderIR::OnStartFunction(Index func_index) {
  Var start(func_index, GetLocation());
  module_->AppendField(
      std::make_unique<StartModuleField>(start, GetLocation()));
  return Result::Ok;
}

Result BinaryReaderIR::OnFunctionBodyCount(Index count) {
  // Can hit this case on a malformed module if we don't stop on first error.
  if (module_->num_func_imports + count != module_->funcs.size()) {
    PrintError(
        "number of imported func + func count in code section does not match "
        "actual number of funcs in module");
    return Result::Error;
  }
  return Result::Ok;
}

Result BinaryReaderIR::BeginFunctionBody(Index index, Offset size) {
  current_func_ = module_->funcs[index];
  current_func_->loc = GetLocation();
  return PushLabel(LabelType::Func, &current_func_->exprs);
}

Result BinaryReaderIR::OnLocalDecl(Index decl_index, Index count, Type type) {
  current_func_->local_types.AppendDecl(type, count);

  if (current_func_->GetNumLocals() > kMaxFunctionLocals) {
    PrintError("function local count exceeds maximum value");
    return Result::Error;
  }

  module_->features_used.simd |= (type == Type::V128);
  return Result::Ok;
}

Result BinaryReaderIR::OnOpcode(Opcode opcode) {
  std::unique_ptr<CodeMetadataExpr> metadata =
      code_metadata_queue_.pop_match(current_func_, GetLocation().offset - 1);
  if (metadata) {
    return AppendExpr(std::move(metadata));
  }
  module_->features_used.simd |= (opcode.GetResultType() == Type::V128);
  return Result::Ok;
}

Result BinaryReaderIR::OnAtomicLoadExpr(Opcode opcode,
                                        Index memidx,
                                        Address alignment_log2,
                                        Address offset) {
  return AppendExpr(std::make_unique<AtomicLoadExpr>(
      opcode, Var(memidx, GetLocation()), 1 << alignment_log2, offset));
}

Result BinaryReaderIR::OnAtomicStoreExpr(Opcode opcode,
                                         Index memidx,
                                         Address alignment_log2,
                                         Address offset) {
  return AppendExpr(std::make_unique<AtomicStoreExpr>(
      opcode, Var(memidx, GetLocation()), 1 << alignment_log2, offset));
}

Result BinaryReaderIR::OnAtomicRmwExpr(Opcode opcode,
                                       Index memidx,
                                       Address alignment_log2,
                                       Address offset) {
  return AppendExpr(std::make_unique<AtomicRmwExpr>(
      opcode, Var(memidx, GetLocation()), 1 << alignment_log2, offset));
}

Result BinaryReaderIR::OnAtomicRmwCmpxchgExpr(Opcode opcode,
                                              Index memidx,
                                              Address alignment_log2,
                                              Address offset) {
  return AppendExpr(std::make_unique<AtomicRmwCmpxchgExpr>(
      opcode, Var(memidx, GetLocation()), 1 << alignment_log2, offset));
}

Result BinaryReaderIR::OnAtomicWaitExpr(Opcode opcode,
                                        Index memidx,
                                        Address alignment_log2,
                                        Address offset) {
  return AppendExpr(std::make_unique<AtomicWaitExpr>(
      opcode, Var(memidx, GetLocation()), 1 << alignment_log2, offset));
}

Result BinaryReaderIR::OnAtomicFenceExpr(uint32_t consistency_model) {
  return AppendExpr(std::make_unique<AtomicFenceExpr>(consistency_model));
}

Result BinaryReaderIR::OnAtomicNotifyExpr(Opcode opcode,
                                          Index memidx,
                                          Address alignment_log2,
                                          Address offset) {
  return AppendExpr(std::make_unique<AtomicNotifyExpr>(
      opcode, Var(memidx, GetLocation()), 1 << alignment_log2, offset));
}

Result BinaryReaderIR::OnBinaryExpr(Opcode opcode) {
  return AppendExpr(std::make_unique<BinaryExpr>(opcode));
}

Result BinaryReaderIR::OnBlockExpr(Type sig_type) {
  auto expr = std::make_unique<BlockExpr>();
  SetBlockDeclaration(&expr->block.decl, sig_type);
  ExprList* expr_list = &expr->block.exprs;
  CHECK_RESULT(AppendExpr(std::move(expr)));
  return PushLabel(LabelType::Block, expr_list);
}

Result BinaryReaderIR::OnBrExpr(Index depth) {
  return AppendExpr(std::make_unique<BrExpr>(Var(depth, GetLocation())));
}

Result BinaryReaderIR::OnBrIfExpr(Index depth) {
  return AppendExpr(std::make_unique<BrIfExpr>(Var(depth, GetLocation())));
}

Result BinaryReaderIR::OnBrTableExpr(Index num_targets,
                                     Index* target_depths,
                                     Index default_target_depth) {
  auto expr = std::make_unique<BrTableExpr>();
  expr->default_target = Var(default_target_depth, GetLocation());
  expr->targets.resize(num_targets);
  for (Index i = 0; i < num_targets; ++i) {
    expr->targets[i] = Var(target_depths[i], GetLocation());
  }
  return AppendExpr(std::move(expr));
}

Result BinaryReaderIR::OnCallExpr(Index func_index) {
  return AppendExpr(std::make_unique<CallExpr>(Var(func_index, GetLocation())));
}

Result BinaryReaderIR::OnCallIndirectExpr(Index sig_index, Index table_index) {
  auto expr = std::make_unique<CallIndirectExpr>();
  SetFuncDeclaration(&expr->decl, Var(sig_index, GetLocation()));
  expr->table = Var(table_index, GetLocation());
  return AppendExpr(std::move(expr));
}

Result BinaryReaderIR::OnCallRefExpr() {
  return AppendExpr(std::make_unique<CallRefExpr>());
}

Result BinaryReaderIR::OnReturnCallExpr(Index func_index) {
  return AppendExpr(
      std::make_unique<ReturnCallExpr>(Var(func_index, GetLocation())));
}

Result BinaryReaderIR::OnReturnCallIndirectExpr(Index sig_index,
                                                Index table_index) {
  auto expr = std::make_unique<ReturnCallIndirectExpr>();
  SetFuncDeclaration(&expr->decl, Var(sig_index, GetLocation()));
  expr->table = Var(table_index, GetLocation());
  return AppendExpr(std::move(expr));
}

Result BinaryReaderIR::OnCompareExpr(Opcode opcode) {
  return AppendExpr(std::make_unique<CompareExpr>(opcode));
}

Result BinaryReaderIR::OnConvertExpr(Opcode opcode) {
  return AppendExpr(std::make_unique<ConvertExpr>(opcode));
}

Result BinaryReaderIR::OnDropExpr() {
  return AppendExpr(std::make_unique<DropExpr>());
}

Result BinaryReaderIR::OnElseExpr() {
  LabelNode* label;
  Expr* expr;
  CHECK_RESULT(TopLabelExpr(&label, &expr));

  if (label->label_type == LabelType::If) {
    auto* if_expr = cast<IfExpr>(expr);
    if_expr->true_.end_loc = GetLocation();
    label->exprs = &if_expr->false_;
    label->label_type = LabelType::Else;
  } else {
    PrintError("else expression without matching if");
    return Result::Error;
  }

  return Result::Ok;
}

Result BinaryReaderIR::OnEndExpr() {
  if (label_stack_.size() > 1) {
    LabelNode* label;
    Expr* expr;
    CHECK_RESULT(TopLabelExpr(&label, &expr));
    switch (label->label_type) {
      case LabelType::Block:
        cast<BlockExpr>(expr)->block.end_loc = GetLocation();
        break;
      case LabelType::Loop:
        cast<LoopExpr>(expr)->block.end_loc = GetLocation();
        break;
      case LabelType::If:
        cast<IfExpr>(expr)->true_.end_loc = GetLocation();
        break;
      case LabelType::Else:
        cast<IfExpr>(expr)->false_end_loc = GetLocation();
        break;
      case LabelType::Try:
        cast<TryExpr>(expr)->block.end_loc = GetLocation();
        break;

      case LabelType::InitExpr:
      case LabelType::Func:
      case LabelType::Catch:
        break;
    }
  }

  return PopLabel();
}

Result BinaryReaderIR::OnF32ConstExpr(uint32_t value_bits) {
  return AppendExpr(
      std::make_unique<ConstExpr>(Const::F32(value_bits, GetLocation())));
}

Result BinaryReaderIR::OnF64ConstExpr(uint64_t value_bits) {
  return AppendExpr(
      std::make_unique<ConstExpr>(Const::F64(value_bits, GetLocation())));
}

Result BinaryReaderIR::OnV128ConstExpr(v128 value_bits) {
  return AppendExpr(
      std::make_unique<ConstExpr>(Const::V128(value_bits, GetLocation())));
}

Result BinaryReaderIR::OnGlobalGetExpr(Index global_index) {
  return AppendExpr(
      std::make_unique<GlobalGetExpr>(Var(global_index, GetLocation())));
}

Result BinaryReaderIR::OnLocalGetExpr(Index local_index) {
  return AppendExpr(
      std::make_unique<LocalGetExpr>(Var(local_index, GetLocation())));
}

Result BinaryReaderIR::OnI32ConstExpr(uint32_t value) {
  return AppendExpr(
      std::make_unique<ConstExpr>(Const::I32(value, GetLocation())));
}

Result BinaryReaderIR::OnI64ConstExpr(uint64_t value) {
  return AppendExpr(
      std::make_unique<ConstExpr>(Const::I64(value, GetLocation())));
}

Result BinaryReaderIR::OnIfExpr(Type sig_type) {
  auto expr = std::make_unique<IfExpr>();
  SetBlockDeclaration(&expr->true_.decl, sig_type);
  ExprList* expr_list = &expr->true_.exprs;
  CHECK_RESULT(AppendExpr(std::move(expr)));
  return PushLabel(LabelType::If, expr_list);
}

Result BinaryReaderIR::OnLoadExpr(Opcode opcode,
                                  Index memidx,
                                  Address alignment_log2,
                                  Address offset) {
  return AppendExpr(std::make_unique<LoadExpr>(
      opcode, Var(memidx, GetLocation()), 1 << alignment_log2, offset));
}

Result BinaryReaderIR::OnLoopExpr(Type sig_type) {
  auto expr = std::make_unique<LoopExpr>();
  SetBlockDeclaration(&expr->block.decl, sig_type);
  ExprList* expr_list = &expr->block.exprs;
  CHECK_RESULT(AppendExpr(std::move(expr)));
  return PushLabel(LabelType::Loop, expr_list);
}

Result BinaryReaderIR::OnMemoryCopyExpr(Index srcmemidx, Index destmemidx) {
  return AppendExpr(std::make_unique<MemoryCopyExpr>(
      Var(srcmemidx, GetLocation()), Var(destmemidx, GetLocation())));
}

Result BinaryReaderIR::OnDataDropExpr(Index segment) {
  return AppendExpr(
      std::make_unique<DataDropExpr>(Var(segment, GetLocation())));
}

Result BinaryReaderIR::OnMemoryFillExpr(Index memidx) {
  return AppendExpr(
      std::make_unique<MemoryFillExpr>(Var(memidx, GetLocation())));
}

Result BinaryReaderIR::OnMemoryGrowExpr(Index memidx) {
  return AppendExpr(
      std::make_unique<MemoryGrowExpr>(Var(memidx, GetLocation())));
}

Result BinaryReaderIR::OnMemoryInitExpr(Index segment, Index memidx) {
  return AppendExpr(std::make_unique<MemoryInitExpr>(
      Var(segment, GetLocation()), Var(memidx, GetLocation())));
}

Result BinaryReaderIR::OnMemorySizeExpr(Index memidx) {
  return AppendExpr(
      std::make_unique<MemorySizeExpr>(Var(memidx, GetLocation())));
}

Result BinaryReaderIR::OnTableCopyExpr(Index dst_index, Index src_index) {
  return AppendExpr(std::make_unique<TableCopyExpr>(
      Var(dst_index, GetLocation()), Var(src_index, GetLocation())));
}

Result BinaryReaderIR::OnElemDropExpr(Index segment) {
  return AppendExpr(
      std::make_unique<ElemDropExpr>(Var(segment, GetLocation())));
}

Result BinaryReaderIR::OnTableInitExpr(Index segment, Index table_index) {
  return AppendExpr(std::make_unique<TableInitExpr>(
      Var(segment, GetLocation()), Var(table_index, GetLocation())));
}

Result BinaryReaderIR::OnTableGetExpr(Index table_index) {
  return AppendExpr(
      std::make_unique<TableGetExpr>(Var(table_index, GetLocation())));
}

Result BinaryReaderIR::OnTableSetExpr(Index table_index) {
  return AppendExpr(
      std::make_unique<TableSetExpr>(Var(table_index, GetLocation())));
}

Result BinaryReaderIR::OnTableGrowExpr(Index table_index) {
  return AppendExpr(
      std::make_unique<TableGrowExpr>(Var(table_index, GetLocation())));
}

Result BinaryReaderIR::OnTableSizeExpr(Index table_index) {
  return AppendExpr(
      std::make_unique<TableSizeExpr>(Var(table_index, GetLocation())));
}

Result BinaryReaderIR::OnTableFillExpr(Index table_index) {
  return AppendExpr(
      std::make_unique<TableFillExpr>(Var(table_index, GetLocation())));
}

Result BinaryReaderIR::OnRefFuncExpr(Index func_index) {
  return AppendExpr(
      std::make_unique<RefFuncExpr>(Var(func_index, GetLocation())));
}

Result BinaryReaderIR::OnRefNullExpr(Type type) {
  return AppendExpr(std::make_unique<RefNullExpr>(type));
}

Result BinaryReaderIR::OnRefIsNullExpr() {
  return AppendExpr(std::make_unique<RefIsNullExpr>());
}

Result BinaryReaderIR::OnNopExpr() {
  return AppendExpr(std::make_unique<NopExpr>());
}

Result BinaryReaderIR::OnRethrowExpr(Index depth) {
  return AppendExpr(std::make_unique<RethrowExpr>(Var(depth, GetLocation())));
}

Result BinaryReaderIR::OnReturnExpr() {
  return AppendExpr(std::make_unique<ReturnExpr>());
}

Result BinaryReaderIR::OnSelectExpr(Index result_count, Type* result_types) {
  TypeVector results;
  results.assign(result_types, result_types + result_count);
  return AppendExpr(std::make_unique<SelectExpr>(results));
}

Result BinaryReaderIR::OnGlobalSetExpr(Index global_index) {
  return AppendExpr(
      std::make_unique<GlobalSetExpr>(Var(global_index, GetLocation())));
}

Result BinaryReaderIR::OnLocalSetExpr(Index local_index) {
  return AppendExpr(
      std::make_unique<LocalSetExpr>(Var(local_index, GetLocation())));
}

Result BinaryReaderIR::OnStoreExpr(Opcode opcode,
                                   Index memidx,
                                   Address alignment_log2,
                                   Address offset) {
  return AppendExpr(std::make_unique<StoreExpr>(
      opcode, Var(memidx, GetLocation()), 1 << alignment_log2, offset));
}

Result BinaryReaderIR::OnThrowExpr(Index tag_index) {
  return AppendExpr(std::make_unique<ThrowExpr>(Var(tag_index, GetLocation())));
}

Result BinaryReaderIR::OnLocalTeeExpr(Index local_index) {
  return AppendExpr(
      std::make_unique<LocalTeeExpr>(Var(local_index, GetLocation())));
}

Result BinaryReaderIR::OnTryExpr(Type sig_type) {
  auto expr_ptr = std::make_unique<TryExpr>();
  // Save expr so it can be used below, after expr_ptr has been moved.
  TryExpr* expr = expr_ptr.get();
  ExprList* expr_list = &expr->block.exprs;
  SetBlockDeclaration(&expr->block.decl, sig_type);
  CHECK_RESULT(AppendExpr(std::move(expr_ptr)));
  module_->features_used.exceptions = true;
  return PushLabel(LabelType::Try, expr_list, expr);
}

Result BinaryReaderIR::AppendCatch(Catch&& catch_) {
  LabelNode* label = nullptr;
  CHECK_RESULT(TopLabel(&label));

  if (label->label_type != LabelType::Try) {
    PrintError("catch not inside try block");
    return Result::Error;
  }

  auto* try_ = cast<TryExpr>(label->context);

  if (catch_.IsCatchAll() && !try_->catches.empty() &&
      try_->catches.back().IsCatchAll()) {
    PrintError("only one catch_all allowed in try block");
    return Result::Error;
  }

  if (try_->kind == TryKind::Plain) {
    try_->kind = TryKind::Catch;
  } else if (try_->kind != TryKind::Catch) {
    PrintError("catch not allowed in try-delegate");
    return Result::Error;
  }

  try_->catches.push_back(std::move(catch_));
  label->exprs = &try_->catches.back().exprs;
  return Result::Ok;
}

Result BinaryReaderIR::OnCatchExpr(Index except_index) {
  return AppendCatch(Catch(Var(except_index, GetLocation())));
}

Result BinaryReaderIR::OnCatchAllExpr() {
  return AppendCatch(Catch(GetLocation()));
}

Result BinaryReaderIR::OnDelegateExpr(Index depth) {
  LabelNode* label = nullptr;
  CHECK_RESULT(TopLabel(&label));

  if (label->label_type != LabelType::Try) {
    PrintError("delegate not inside try block");
    return Result::Error;
  }

  auto* try_ = cast<TryExpr>(label->context);

  if (try_->kind == TryKind::Plain) {
    try_->kind = TryKind::Delegate;
  } else if (try_->kind != TryKind::Delegate) {
    PrintError("delegate not allowed in try-catch");
    return Result::Error;
  }

  try_->delegate_target = Var(depth, GetLocation());

  PopLabel();
  return Result::Ok;
}

Result BinaryReaderIR::OnUnaryExpr(Opcode opcode) {
  return AppendExpr(std::make_unique<UnaryExpr>(opcode));
}

Result BinaryReaderIR::OnTernaryExpr(Opcode opcode) {
  return AppendExpr(std::make_unique<TernaryExpr>(opcode));
}

Result BinaryReaderIR::OnUnreachableExpr() {
  return AppendExpr(std::make_unique<UnreachableExpr>());
}

Result BinaryReaderIR::EndFunctionBody(Index index) {
  current_func_ = nullptr;
  if (!label_stack_.empty()) {
    PrintError("function %" PRIindex " missing end marker", index);
    return Result::Error;
  }
  return Result::Ok;
}

Result BinaryReaderIR::OnSimdLaneOpExpr(Opcode opcode, uint64_t value) {
  return AppendExpr(std::make_unique<SimdLaneOpExpr>(opcode, value));
}

Result BinaryReaderIR::OnSimdLoadLaneExpr(Opcode opcode,
                                          Index memidx,
                                          Address alignment_log2,
                                          Address offset,
                                          uint64_t value) {
  return AppendExpr(std::make_unique<SimdLoadLaneExpr>(
      opcode, Var(memidx, GetLocation()), 1 << alignment_log2, offset, value));
}

Result BinaryReaderIR::OnSimdStoreLaneExpr(Opcode opcode,
                                           Index memidx,
                                           Address alignment_log2,
                                           Address offset,
                                           uint64_t value) {
  return AppendExpr(std::make_unique<SimdStoreLaneExpr>(
      opcode, Var(memidx, GetLocation()), 1 << alignment_log2, offset, value));
}

Result BinaryReaderIR::OnSimdShuffleOpExpr(Opcode opcode, v128 value) {
  return AppendExpr(std::make_unique<SimdShuffleOpExpr>(opcode, value));
}

Result BinaryReaderIR::OnLoadSplatExpr(Opcode opcode,
                                       Index memidx,
                                       Address alignment_log2,
                                       Address offset) {
  return AppendExpr(std::make_unique<LoadSplatExpr>(
      opcode, Var(memidx, GetLocation()), 1 << alignment_log2, offset));
}

Result BinaryReaderIR::OnLoadZeroExpr(Opcode opcode,
                                      Index memidx,
                                      Address alignment_log2,
                                      Address offset) {
  return AppendExpr(std::make_unique<LoadZeroExpr>(
      opcode, Var(memidx, GetLocation()), 1 << alignment_log2, offset));
}

Result BinaryReaderIR::OnElemSegmentCount(Index count) {
  WABT_TRY
  module_->elem_segments.reserve(count);
  WABT_CATCH_BAD_ALLOC
  return Result::Ok;
}

Result BinaryReaderIR::BeginElemSegment(Index index,
                                        Index table_index,
                                        uint8_t flags) {
  auto field = std::make_unique<ElemSegmentModuleField>(GetLocation());
  ElemSegment& elem_segment = field->elem_segment;
  elem_segment.table_var = Var(table_index, GetLocation());
  if ((flags & SegDeclared) == SegDeclared) {
    elem_segment.kind = SegmentKind::Declared;
  } else if ((flags & SegPassive) == SegPassive) {
    elem_segment.kind = SegmentKind::Passive;
  } else {
    elem_segment.kind = SegmentKind::Active;
  }
  module_->AppendField(std::move(field));
  return Result::Ok;
}

Result BinaryReaderIR::BeginInitExpr(ExprList* expr) {
  return PushLabel(LabelType::InitExpr, expr);
}

Result BinaryReaderIR::BeginElemSegmentInitExpr(Index index) {
  assert(index == module_->elem_segments.size() - 1);
  ElemSegment* segment = module_->elem_segments[index];
  return BeginInitExpr(&segment->offset);
}

Result BinaryReaderIR::EndInitExpr() {
  if (!label_stack_.empty()) {
    PrintError("init expression missing end marker");
    return Result::Error;
  }
  return Result::Ok;
}

Result BinaryReaderIR::EndElemSegmentInitExpr(Index index) {
  return EndInitExpr();
}

Result BinaryReaderIR::OnElemSegmentElemType(Index index, Type elem_type) {
  assert(index == module_->elem_segments.size() - 1);
  ElemSegment* segment = module_->elem_segments[index];
  segment->elem_type = elem_type;
  return Result::Ok;
}

Result BinaryReaderIR::OnElemSegmentElemExprCount(Index index, Index count) {
  assert(index == module_->elem_segments.size() - 1);
  ElemSegment* segment = module_->elem_segments[index];
  WABT_TRY
  segment->elem_exprs.reserve(count);
  WABT_CATCH_BAD_ALLOC
  return Result::Ok;
}

Result BinaryReaderIR::OnElemSegmentElemExpr_RefNull(Index segment_index,
                                                     Type type) {
  assert(segment_index == module_->elem_segments.size() - 1);
  ElemSegment* segment = module_->elem_segments[segment_index];
  Location loc = GetLocation();
  ExprList init_expr;
  init_expr.push_back(std::make_unique<RefNullExpr>(type, loc));
  segment->elem_exprs.push_back(std::move(init_expr));
  return Result::Ok;
}

Result BinaryReaderIR::OnElemSegmentElemExpr_RefFunc(Index segment_index,
                                                     Index func_index) {
  assert(segment_index == module_->elem_segments.size() - 1);
  ElemSegment* segment = module_->elem_segments[segment_index];
  Location loc = GetLocation();
  ExprList init_expr;
  init_expr.push_back(std::make_unique<RefFuncExpr>(Var(func_index, loc), loc));
  segment->elem_exprs.push_back(std::move(init_expr));
  return Result::Ok;
}

Result BinaryReaderIR::OnDataSegmentCount(Index count) {
  WABT_TRY
  module_->data_segments.reserve(count);
  WABT_CATCH_BAD_ALLOC
  return Result::Ok;
}

Result BinaryReaderIR::BeginDataSegment(Index index,
                                        Index memory_index,
                                        uint8_t flags) {
  auto field = std::make_unique<DataSegmentModuleField>(GetLocation());
  DataSegment& data_segment = field->data_segment;
  data_segment.memory_var = Var(memory_index, GetLocation());
  if ((flags & SegPassive) == SegPassive) {
    data_segment.kind = SegmentKind::Passive;
  } else {
    data_segment.kind = SegmentKind::Active;
  }
  module_->AppendField(std::move(field));
  return Result::Ok;
}

Result BinaryReaderIR::BeginDataSegmentInitExpr(Index index) {
  assert(index == module_->data_segments.size() - 1);
  DataSegment* segment = module_->data_segments[index];
  return BeginInitExpr(&segment->offset);
}

Result BinaryReaderIR::EndDataSegmentInitExpr(Index index) {
  return EndInitExpr();
}

Result BinaryReaderIR::OnDataSegmentData(Index index,
                                         const void* data,
                                         Address size) {
  assert(index == module_->data_segments.size() - 1);
  DataSegment* segment = module_->data_segments[index];
  segment->data.resize(size);
  if (size > 0) {
    memcpy(segment->data.data(), data, size);
  }
  return Result::Ok;
}

Result BinaryReaderIR::OnFunctionNamesCount(Index count) {
  if (count > module_->funcs.size()) {
    PrintError("expected function name count (%" PRIindex
               ") <= function count (%" PRIzd ")",
               count, module_->funcs.size());
    return Result::Error;
  }
  return Result::Ok;
}

static std::string MakeDollarName(std::string_view name) {
  return std::string("$") + std::string(name);
}

Result BinaryReaderIR::OnModuleName(std::string_view name) {
  if (name.empty()) {
    return Result::Ok;
  }

  module_->name = MakeDollarName(name);
  return Result::Ok;
}

Result BinaryReaderIR::SetGlobalName(Index index, std::string_view name) {
  if (name.empty()) {
    return Result::Ok;
  }
  if (index >= module_->globals.size()) {
    PrintError("invalid global index: %" PRIindex, index);
    return Result::Error;
  }
  Global* glob = module_->globals[index];
  std::string dollar_name =
      GetUniqueName(&module_->global_bindings, MakeDollarName(name));
  glob->name = dollar_name;
  module_->global_bindings.emplace(dollar_name, Binding(index));
  return Result::Ok;
}

Result BinaryReaderIR::SetFunctionName(Index index, std::string_view name) {
  if (name.empty()) {
    return Result::Ok;
  }
  if (index >= module_->funcs.size()) {
    PrintError("invalid function index: %" PRIindex, index);
    return Result::Error;
  }
  Func* func = module_->funcs[index];
  std::string dollar_name =
      GetUniqueName(&module_->func_bindings, MakeDollarName(name));
  func->name = dollar_name;
  module_->func_bindings.emplace(dollar_name, Binding(index));
  return Result::Ok;
}

Result BinaryReaderIR::SetTypeName(Index index, std::string_view name) {
  if (name.empty()) {
    return Result::Ok;
  }
  if (index >= module_->types.size()) {
    PrintError("invalid type index: %" PRIindex, index);
    return Result::Error;
  }
  TypeEntry* type = module_->types[index];
  std::string dollar_name =
      GetUniqueName(&module_->type_bindings, MakeDollarName(name));
  type->name = dollar_name;
  module_->type_bindings.emplace(dollar_name, Binding(index));
  return Result::Ok;
}

Result BinaryReaderIR::SetTableName(Index index, std::string_view name) {
  if (name.empty()) {
    return Result::Ok;
  }
  if (index >= module_->tables.size()) {
    PrintError("invalid table index: %" PRIindex, index);
    return Result::Error;
  }
  Table* table = module_->tables[index];
  std::string dollar_name =
      GetUniqueName(&module_->table_bindings, MakeDollarName(name));
  table->name = dollar_name;
  module_->table_bindings.emplace(dollar_name, Binding(index));
  return Result::Ok;
}

Result BinaryReaderIR::SetDataSegmentName(Index index, std::string_view name) {
  if (name.empty()) {
    return Result::Ok;
  }
  if (index >= module_->data_segments.size()) {
    PrintError("invalid data segment index: %" PRIindex, index);
    return Result::Error;
  }
  DataSegment* segment = module_->data_segments[index];
  std::string dollar_name =
      GetUniqueName(&module_->data_segment_bindings, MakeDollarName(name));
  segment->name = dollar_name;
  module_->data_segment_bindings.emplace(dollar_name, Binding(index));
  return Result::Ok;
}

Result BinaryReaderIR::SetElemSegmentName(Index index, std::string_view name) {
  if (name.empty()) {
    return Result::Ok;
  }
  if (index >= module_->elem_segments.size()) {
    PrintError("invalid elem segment index: %" PRIindex, index);
    return Result::Error;
  }
  ElemSegment* segment = module_->elem_segments[index];
  std::string dollar_name =
      GetUniqueName(&module_->elem_segment_bindings, MakeDollarName(name));
  segment->name = dollar_name;
  module_->elem_segment_bindings.emplace(dollar_name, Binding(index));
  return Result::Ok;
}

Result BinaryReaderIR::SetMemoryName(Index index, std::string_view name) {
  if (name.empty()) {
    return Result::Ok;
  }
  if (index >= module_->memories.size()) {
    PrintError("invalid memory index: %" PRIindex, index);
    return Result::Error;
  }
  Memory* memory = module_->memories[index];
  std::string dollar_name =
      GetUniqueName(&module_->memory_bindings, MakeDollarName(name));
  memory->name = dollar_name;
  module_->memory_bindings.emplace(dollar_name, Binding(index));
  return Result::Ok;
}

Result BinaryReaderIR::SetTagName(Index index, std::string_view name) {
  if (name.empty()) {
    return Result::Ok;
  }
  if (index >= module_->tags.size()) {
    PrintError("invalid tag index: %" PRIindex, index);
    return Result::Error;
  }
  Tag* tag = module_->tags[index];
  std::string dollar_name =
      GetUniqueName(&module_->tag_bindings, MakeDollarName(name));
  tag->name = dollar_name;
  module_->tag_bindings.emplace(dollar_name, Binding(index));
  return Result::Ok;
}

Result BinaryReaderIR::OnFunctionName(Index index, std::string_view name) {
  return SetFunctionName(index, name);
}

Result BinaryReaderIR::OnNameEntry(NameSectionSubsection type,
                                   Index index,
                                   std::string_view name) {
  switch (type) {
    // TODO(sbc): remove OnFunctionName in favor of just using
    // OnNameEntry so that this works
    case NameSectionSubsection::Function:
    case NameSectionSubsection::Local:
    case NameSectionSubsection::Module:
    case NameSectionSubsection::Label:
      break;
    case NameSectionSubsection::Type:
      SetTypeName(index, name);
      break;
    case NameSectionSubsection::Tag:
      SetTagName(index, name);
      break;
    case NameSectionSubsection::Global:
      SetGlobalName(index, name);
      break;
    case NameSectionSubsection::Table:
      SetTableName(index, name);
      break;
    case NameSectionSubsection::DataSegment:
      SetDataSegmentName(index, name);
      break;
    case NameSectionSubsection::Memory:
      SetMemoryName(index, name);
      break;
    case NameSectionSubsection::ElemSegment:
      SetElemSegmentName(index, name);
      break;
  }
  return Result::Ok;
}

Result BinaryReaderIR::OnLocalNameLocalCount(Index index, Index count) {
  assert(index < module_->funcs.size());
  Func* func = module_->funcs[index];
  Index num_params_and_locals = func->GetNumParamsAndLocals();
  if (count > num_params_and_locals) {
    PrintError("expected local name count (%" PRIindex
               ") <= local count (%" PRIindex ")",
               count, num_params_and_locals);
    return Result::Error;
  }
  return Result::Ok;
}

Result BinaryReaderIR::BeginCodeMetadataSection(std::string_view name,
                                                Offset size) {
  current_metadata_name_ = name;
  return Result::Ok;
}

Result BinaryReaderIR::OnCodeMetadataFuncCount(Index count) {
  return Result::Ok;
}

Result BinaryReaderIR::OnCodeMetadataCount(Index function_index, Index count) {
  code_metadata_queue_.push_func(module_->funcs[function_index]);
  return Result::Ok;
}

Result BinaryReaderIR::OnCodeMetadata(Offset offset,
                                      const void* data,
                                      Address size) {
  std::vector<uint8_t> data_(static_cast<const uint8_t*>(data),
                             static_cast<const uint8_t*>(data) + size);
  auto meta = std::make_unique<CodeMetadataExpr>(current_metadata_name_,
                                                 std::move(data_));
  meta->loc.offset = offset;
  code_metadata_queue_.push_metadata(std::move(meta));
  return Result::Ok;
}

Result BinaryReaderIR::OnLocalName(Index func_index,
                                   Index local_index,
                                   std::string_view name) {
  if (name.empty()) {
    return Result::Ok;
  }

  Func* func = module_->funcs[func_index];
  func->bindings.emplace(GetUniqueName(&func->bindings, MakeDollarName(name)),
                         Binding(local_index));
  return Result::Ok;
}

Result BinaryReaderIR::OnTagType(Index index, Index sig_index) {
  auto field = std::make_unique<TagModuleField>(GetLocation());
  Tag& tag = field->tag;
  SetFuncDeclaration(&tag.decl, Var(sig_index, GetLocation()));
  module_->AppendField(std::move(field));
  module_->features_used.exceptions = true;
  return Result::Ok;
}

Result BinaryReaderIR::OnDataSymbol(Index index,
                                    uint32_t flags,
                                    std::string_view name,
                                    Index segment,
                                    uint32_t offset,
                                    uint32_t size) {
  if (name.empty()) {
    return Result::Ok;
  }
  if (flags & WABT_SYMBOL_FLAG_UNDEFINED) {
    // Refers to data in another file, `segment` not valid.
    return Result::Ok;
  }
  if (offset) {
    // If it is pointing into the data segment, then it's not really naming
    // the whole segment.
    return Result::Ok;
  }
  if (segment >= module_->data_segments.size()) {
    PrintError("invalid data segment index: %" PRIindex, segment);
    return Result::Error;
  }
  DataSegment* seg = module_->data_segments[segment];
  std::string dollar_name =
      GetUniqueName(&module_->data_segment_bindings, MakeDollarName(name));
  seg->name = dollar_name;
  module_->data_segment_bindings.emplace(dollar_name, Binding(segment));
  return Result::Ok;
}

Result BinaryReaderIR::OnFunctionSymbol(Index index,
                                        uint32_t flags,
                                        std::string_view name,
                                        Index func_index) {
  if (name.empty()) {
    return Result::Ok;
  }
  if (func_index >= module_->funcs.size()) {
    PrintError("invalid function index: %" PRIindex, func_index);
    return Result::Error;
  }
  Func* func = module_->funcs[func_index];
  if (!func->name.empty()) {
    // The name section has already named this function.
    return Result::Ok;
  }
  std::string dollar_name =
      GetUniqueName(&module_->func_bindings, MakeDollarName(name));
  func->name = dollar_name;
  module_->func_bindings.emplace(dollar_name, Binding(func_index));
  return Result::Ok;
}

Result BinaryReaderIR::OnGlobalSymbol(Index index,
                                      uint32_t flags,
                                      std::string_view name,
                                      Index global_index) {
  return SetGlobalName(global_index, name);
}

Result BinaryReaderIR::OnSectionSymbol(Index index,
                                       uint32_t flags,
                                       Index section_index) {
  return Result::Ok;
}

Result BinaryReaderIR::OnTagSymbol(Index index,
                                   uint32_t flags,
                                   std::string_view name,
                                   Index tag_index) {
  if (name.empty()) {
    return Result::Ok;
  }
  if (tag_index >= module_->tags.size()) {
    PrintError("invalid tag index: %" PRIindex, tag_index);
    return Result::Error;
  }
  Tag* tag = module_->tags[tag_index];
  std::string dollar_name =
      GetUniqueName(&module_->tag_bindings, MakeDollarName(name));
  tag->name = dollar_name;
  module_->tag_bindings.emplace(dollar_name, Binding(tag_index));
  return Result::Ok;
}

Result BinaryReaderIR::OnTableSymbol(Index index,
                                     uint32_t flags,
                                     std::string_view name,
                                     Index table_index) {
  return SetTableName(table_index, name);
}

}  // end anonymous namespace

Result ReadBinaryIr(const char* filename,
                    const void* data,
                    size_t size,
                    const ReadBinaryOptions& options,
                    Errors* errors,
                    Module* out_module) {
  BinaryReaderIR reader(out_module, filename, errors);
  return ReadBinary(data, size, &reader, options);
}

}  // namespace wabt
