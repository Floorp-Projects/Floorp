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

#include "src/generate-names.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "src/cast.h"
#include "src/expr-visitor.h"
#include "src/ir.h"

namespace wabt {

namespace {

class NameGenerator : public ExprVisitor::DelegateNop {
 public:
  NameGenerator(NameOpts opts);

  Result VisitModule(Module* module);

  // Implementation of ExprVisitor::DelegateNop.
  Result BeginBlockExpr(BlockExpr* expr) override;
  Result BeginLoopExpr(LoopExpr* expr) override;
  Result BeginIfExpr(IfExpr* expr) override;

 private:
  static bool HasName(const std::string& str);

  // Generate a name with the given prefix, followed by the index and
  // optionally a disambiguating number. If index == kInvalidIndex, the index
  // is not appended.
  void GenerateName(const char* prefix,
                    Index index,
                    unsigned disambiguator,
                    std::string* out_str);

  // Like GenerateName, but only generates a name if |out_str| is empty.
  void MaybeGenerateName(const char* prefix,
                         Index index,
                         std::string* out_str);

  // Generate a name via GenerateName and bind it to the given binding hash. If
  // the name already exists, the name will be disambiguated until it can be
  // added.
  void GenerateAndBindName(BindingHash* bindings,
                           const char* prefix,
                           Index index,
                           std::string* out_str);

  // Like GenerateAndBindName, but only  generates a name if |out_str| is empty.
  void MaybeGenerateAndBindName(BindingHash* bindings,
                                const char* prefix,
                                Index index,
                                std::string* out_str);

  // Like MaybeGenerateAndBindName but uses the name directly, without
  // appending the index. If the name already exists, a disambiguating suffix
  // is added.
  void MaybeUseAndBindName(BindingHash* bindings,
                           const char* name,
                           Index index,
                           std::string* out_str);

  void GenerateAndBindLocalNames(Func* func);

  template <typename T>
  Result VisitAll(const std::vector<T*>& items,
                  Result (NameGenerator::*func)(Index, T*));

  Result VisitFunc(Index func_index, Func* func);
  Result VisitGlobal(Index global_index, Global* global);
  Result VisitType(Index func_type_index, TypeEntry* type);
  Result VisitTable(Index table_index, Table* table);
  Result VisitMemory(Index memory_index, Memory* memory);
  Result VisitTag(Index tag_index, Tag* tag);
  Result VisitDataSegment(Index data_segment_index, DataSegment* data_segment);
  Result VisitElemSegment(Index elem_segment_index, ElemSegment* elem_segment);
  Result VisitImport(Import* import);
  Result VisitExport(Export* export_);

  Module* module_ = nullptr;
  ExprVisitor visitor_;
  Index label_count_ = 0;

  Index num_func_imports_ = 0;
  Index num_table_imports_ = 0;
  Index num_memory_imports_ = 0;
  Index num_global_imports_ = 0;
  Index num_tag_imports_ = 0;

  NameOpts opts_;
};

NameGenerator::NameGenerator(NameOpts opts)
  : visitor_(this), opts_(opts) {}

// static
bool NameGenerator::HasName(const std::string& str) {
  return !str.empty();
}

void NameGenerator::GenerateName(const char* prefix,
                                 Index index,
                                 unsigned disambiguator,
                                 std::string* str) {
  *str = "$";
  *str += prefix;
  if (index != kInvalidIndex) {
    if (opts_ & NameOpts::AlphaNames) {
      // For params and locals, do not use a prefix char.
      if (!strcmp(prefix, "p") || !strcmp(prefix, "l")) {
        str->pop_back();
      } else {
        *str += '_';
      }
      *str += IndexToAlphaName(index);
    } else {
      *str += std::to_string(index);
    }
  }
  if (disambiguator != 0) {
    *str += '_' + std::to_string(disambiguator);
  }
}

void NameGenerator::MaybeGenerateName(const char* prefix,
                                      Index index,
                                      std::string* str) {
  if (!HasName(*str)) {
    // There's no bindings hash, so the name can't be a duplicate. Therefore it
    // doesn't need a disambiguating number.
    GenerateName(prefix, index, 0, str);
  }
}

void NameGenerator::GenerateAndBindName(BindingHash* bindings,
                                        const char* prefix,
                                        Index index,
                                        std::string* str) {
  unsigned disambiguator = 0;
  while (true) {
    GenerateName(prefix, index, disambiguator, str);
    if (bindings->find(*str) == bindings->end()) {
      bindings->emplace(*str, Binding(index));
      break;
    }

    disambiguator++;
  }
}

void NameGenerator::MaybeGenerateAndBindName(BindingHash* bindings,
                                             const char* prefix,
                                             Index index,
                                             std::string* str) {
  if (!HasName(*str)) {
    GenerateAndBindName(bindings, prefix, index, str);
  }
}

void NameGenerator::MaybeUseAndBindName(BindingHash* bindings,
                                        const char* name,
                                        Index index,
                                        std::string* str) {
  if (!HasName(*str)) {
    unsigned disambiguator = 0;
    while (true) {
      GenerateName(name, kInvalidIndex, disambiguator, str);
      if (bindings->find(*str) == bindings->end()) {
        bindings->emplace(*str, Binding(index));
        break;
      }

      disambiguator++;
    }
  }
}

void NameGenerator::GenerateAndBindLocalNames(Func* func) {
  std::vector<std::string> index_to_name;
  MakeTypeBindingReverseMapping(func->GetNumParamsAndLocals(), func->bindings,
                                &index_to_name);
  for (size_t i = 0; i < index_to_name.size(); ++i) {
    const std::string& old_name = index_to_name[i];
    if (!old_name.empty()) {
      continue;
    }

    const char* prefix = i < func->GetNumParams() ? "p" : "l";
    std::string new_name;
    GenerateAndBindName(&func->bindings, prefix, i, &new_name);
    index_to_name[i] = new_name;
  }
}

Result NameGenerator::BeginBlockExpr(BlockExpr* expr) {
  MaybeGenerateName("B", label_count_++, &expr->block.label);
  return Result::Ok;
}

Result NameGenerator::BeginLoopExpr(LoopExpr* expr) {
  MaybeGenerateName("L", label_count_++, &expr->block.label);
  return Result::Ok;
}

Result NameGenerator::BeginIfExpr(IfExpr* expr) {
  MaybeGenerateName("I", label_count_++, &expr->true_.label);
  return Result::Ok;
}

Result NameGenerator::VisitFunc(Index func_index, Func* func) {
  MaybeGenerateAndBindName(&module_->func_bindings, "f", func_index,
                           &func->name);
  GenerateAndBindLocalNames(func);

  label_count_ = 0;
  CHECK_RESULT(visitor_.VisitFunc(func));
  return Result::Ok;
}

Result NameGenerator::VisitGlobal(Index global_index, Global* global) {
  MaybeGenerateAndBindName(&module_->global_bindings, "g", global_index,
                           &global->name);
  return Result::Ok;
}

Result NameGenerator::VisitType(Index type_index, TypeEntry* type) {
  MaybeGenerateAndBindName(&module_->type_bindings, "t", type_index,
                           &type->name);
  return Result::Ok;
}

Result NameGenerator::VisitTable(Index table_index, Table* table) {
  MaybeGenerateAndBindName(&module_->table_bindings, "T", table_index,
                           &table->name);
  return Result::Ok;
}

Result NameGenerator::VisitMemory(Index memory_index, Memory* memory) {
  MaybeGenerateAndBindName(&module_->memory_bindings, "M", memory_index,
                           &memory->name);
  return Result::Ok;
}

Result NameGenerator::VisitTag(Index tag_index, Tag* tag) {
  MaybeGenerateAndBindName(&module_->tag_bindings, "e", tag_index, &tag->name);
  return Result::Ok;
}

Result NameGenerator::VisitDataSegment(Index data_segment_index,
                                       DataSegment* data_segment) {
  MaybeGenerateAndBindName(&module_->data_segment_bindings, "d",
                           data_segment_index, &data_segment->name);
  return Result::Ok;
}

Result NameGenerator::VisitElemSegment(Index elem_segment_index,
                                       ElemSegment* elem_segment) {
  MaybeGenerateAndBindName(&module_->elem_segment_bindings, "e",
                           elem_segment_index, &elem_segment->name);
  return Result::Ok;
}

Result NameGenerator::VisitImport(Import* import) {
  BindingHash* bindings = nullptr;
  std::string* name = nullptr;
  Index index = kInvalidIndex;

  switch (import->kind()) {
    case ExternalKind::Func:
      if (auto* func_import = cast<FuncImport>(import)) {
        bindings = &module_->func_bindings;
        name = &func_import->func.name;
        index = num_func_imports_++;
      }
      break;

    case ExternalKind::Table:
      if (auto* table_import = cast<TableImport>(import)) {
        bindings = &module_->table_bindings;
        name = &table_import->table.name;
        index = num_table_imports_++;
      }
      break;

    case ExternalKind::Memory:
      if (auto* memory_import = cast<MemoryImport>(import)) {
        bindings = &module_->memory_bindings;
        name = &memory_import->memory.name;
        index = num_memory_imports_++;
      }
      break;

    case ExternalKind::Global:
      if (auto* global_import = cast<GlobalImport>(import)) {
        bindings = &module_->global_bindings;
        name = &global_import->global.name;
        index = num_global_imports_++;
      }
      break;

    case ExternalKind::Tag:
      if (auto* tag_import = cast<TagImport>(import)) {
        bindings = &module_->tag_bindings;
        name = &tag_import->tag.name;
        index = num_tag_imports_++;
      }
      break;
  }

  if (bindings && name) {
    assert(index != kInvalidIndex);
    std::string new_name = import->module_name + '.' + import->field_name;
    MaybeUseAndBindName(bindings, new_name.c_str(), index, name);
  }

  return Result::Ok;
}

Result NameGenerator::VisitExport(Export* export_) {
  BindingHash* bindings = nullptr;
  std::string* name = nullptr;
  Index index = kInvalidIndex;

  switch (export_->kind) {
    case ExternalKind::Func:
      if (Func* func = module_->GetFunc(export_->var)) {
        index = module_->GetFuncIndex(export_->var);
        bindings = &module_->func_bindings;
        name = &func->name;
      }
      break;

    case ExternalKind::Table:
      if (Table* table = module_->GetTable(export_->var)) {
        index = module_->GetTableIndex(export_->var);
        bindings = &module_->table_bindings;
        name = &table->name;
      }
      break;

    case ExternalKind::Memory:
      if (Memory* memory = module_->GetMemory(export_->var)) {
        index = module_->GetMemoryIndex(export_->var);
        bindings = &module_->memory_bindings;
        name = &memory->name;
      }
      break;

    case ExternalKind::Global:
      if (Global* global = module_->GetGlobal(export_->var)) {
        index = module_->GetGlobalIndex(export_->var);
        bindings = &module_->global_bindings;
        name = &global->name;
      }
      break;

    case ExternalKind::Tag:
      if (Tag* tag = module_->GetTag(export_->var)) {
        index = module_->GetTagIndex(export_->var);
        bindings = &module_->tag_bindings;
        name = &tag->name;
      }
      break;
  }

  if (bindings && name) {
    MaybeUseAndBindName(bindings, export_->name.c_str(), index, name);
  }

  return Result::Ok;
}

template <typename T>
Result NameGenerator::VisitAll(const std::vector<T*>& items,
                               Result (NameGenerator::*func)(Index, T*)) {
  for (Index i = 0; i < items.size(); ++i) {
    CHECK_RESULT((this->*func)(i, items[i]));
  }
  return Result::Ok;
}

Result NameGenerator::VisitModule(Module* module) {
  module_ = module;
  // Visit imports and exports first to give better names, derived from the
  // import/export name.
  for (auto* import : module->imports) {
    CHECK_RESULT(VisitImport(import));
  }
  for (auto* export_ : module->exports) {
    CHECK_RESULT(VisitExport(export_));
  }

  VisitAll(module->globals, &NameGenerator::VisitGlobal);
  VisitAll(module->types, &NameGenerator::VisitType);
  VisitAll(module->funcs, &NameGenerator::VisitFunc);
  VisitAll(module->tables, &NameGenerator::VisitTable);
  VisitAll(module->memories, &NameGenerator::VisitMemory);
  VisitAll(module->tags, &NameGenerator::VisitTag);
  VisitAll(module->data_segments, &NameGenerator::VisitDataSegment);
  VisitAll(module->elem_segments, &NameGenerator::VisitElemSegment);
  module_ = nullptr;
  return Result::Ok;
}

}  // end anonymous namespace

Result GenerateNames(Module* module, NameOpts opts) {
  NameGenerator generator(opts);
  return generator.VisitModule(module);
}

}  // namespace wabt
