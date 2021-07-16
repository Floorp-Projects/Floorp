/*
 * Copyright 2020 WebAssembly Community Group participants
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

#include "src/shared-validator.h"

#include <algorithm>
#include <cinttypes>
#include <limits>

namespace wabt {

TypeVector SharedValidator::ToTypeVector(Index count, const Type* types) {
  return TypeVector(&types[0], &types[count]);
}

SharedValidator::SharedValidator(Errors* errors, const ValidateOptions& options)
    : options_(options), errors_(errors), typechecker_(options.features) {
  typechecker_.set_error_callback(
      [this](const char* msg) { OnTypecheckerError(msg); });
}

Result WABT_PRINTF_FORMAT(3, 4) SharedValidator::PrintError(const Location& loc,
                                                            const char* format,
                                                            ...) {
  WABT_SNPRINTF_ALLOCA(buffer, length, format);
  errors_->emplace_back(ErrorLevel::Error, loc, buffer);
  return Result::Error;
}

void SharedValidator::OnTypecheckerError(const char* msg) {
  PrintError(*expr_loc_, "%s", msg);
}

Result SharedValidator::OnFuncType(const Location& loc,
                                   Index param_count,
                                   const Type* param_types,
                                   Index result_count,
                                   const Type* result_types) {
  Result result = Result::Ok;
  if (!options_.features.multi_value_enabled() && result_count > 1) {
    result |=
        PrintError(loc, "multiple result values not currently supported.");
  }
  func_types_.emplace(num_types_++,
                      FuncType{ToTypeVector(param_count, param_types),
                               ToTypeVector(result_count, result_types)});
  return result;
}

Result SharedValidator::OnStructType(const Location&,
                                     Index field_count,
                                     TypeMut* fields) {
  struct_types_.emplace(num_types_++, StructType{TypeMutVector(
                                          &fields[0], &fields[field_count])});
  return Result::Ok;
}

Result SharedValidator::OnArrayType(const Location&, TypeMut field) {
  array_types_.emplace(num_types_++, ArrayType{field});
  return Result::Ok;
}

Result SharedValidator::OnFunction(const Location& loc, Var sig_var) {
  Result result = Result::Ok;
  FuncType type;
  result |= CheckFuncTypeIndex(sig_var, &type);
  funcs_.push_back(type);
  return result;
}

Result SharedValidator::CheckLimits(const Location& loc,
                                    const Limits& limits,
                                    uint64_t absolute_max,
                                    const char* desc) {
  Result result = Result::Ok;
  if (limits.initial > absolute_max) {
    result |=
        PrintError(loc, "initial %s (%" PRIu64 ") must be <= (%" PRIu64 ")",
                   desc, limits.initial, absolute_max);
  }

  if (limits.has_max) {
    if (limits.max > absolute_max) {
      result |= PrintError(loc, "max %s (%" PRIu64 ") must be <= (%" PRIu64 ")",
                           desc, limits.max, absolute_max);
    }

    if (limits.max < limits.initial) {
      result |= PrintError(
          loc, "max %s (%" PRIu64 ") must be >= initial %s (%" PRIu64 ")", desc,
          limits.max, desc, limits.initial);
    }
  }
  return result;
}

Result SharedValidator::OnTable(const Location& loc,
                                Type elem_type,
                                const Limits& limits) {
  Result result = Result::Ok;
  if (tables_.size() > 0 && !options_.features.reference_types_enabled()) {
    result |= PrintError(loc, "only one table allowed");
  }
  result |= CheckLimits(loc, limits, UINT32_MAX, "elems");

  if (limits.is_shared) {
    result |= PrintError(loc, "tables may not be shared");
  }
  if (elem_type != Type::FuncRef &&
      !options_.features.reference_types_enabled()) {
    result |= PrintError(loc, "tables must have funcref type");
  }
  if (!elem_type.IsRef()) {
    result |= PrintError(loc, "tables must have reference types");
  }

  tables_.push_back(TableType{elem_type, limits});
  return result;
}

Result SharedValidator::OnMemory(const Location& loc, const Limits& limits) {
  Result result = Result::Ok;
  if (memories_.size() > 0) {
    result |= PrintError(loc, "only one memory block allowed");
  }
  result |= CheckLimits(loc, limits, WABT_MAX_PAGES, "pages");

  if (limits.is_shared) {
    if (!options_.features.threads_enabled()) {
      result |= PrintError(loc, "memories may not be shared");
    } else if (!limits.has_max) {
      result |= PrintError(loc, "shared memories must have max sizes");
    }
  }

  memories_.push_back(MemoryType{limits});
  return result;
}

Result SharedValidator::OnGlobalImport(const Location& loc,
                                       Type type,
                                       bool mutable_) {
  Result result = Result::Ok;
  if (mutable_ && !options_.features.mutable_globals_enabled()) {
    result |= PrintError(loc, "mutable globals cannot be imported");
  }
  globals_.push_back(GlobalType{type, mutable_});
  ++num_imported_globals_;
  return result;
}

Result SharedValidator::OnGlobal(const Location& loc,
                                 Type type,
                                 bool mutable_) {
  globals_.push_back(GlobalType{type, mutable_});
  return Result::Ok;
}

Result SharedValidator::CheckType(const Location& loc,
                                  Type actual,
                                  Type expected,
                                  const char* desc) {
  if (Failed(TypeChecker::CheckType(actual, expected))) {
    PrintError(loc, "type mismatch at %s. got %s, expected %s", desc,
               actual.GetName(), expected.GetName());
    return Result::Error;
  }
  return Result::Ok;
}

Result SharedValidator::OnGlobalInitExpr_Const(const Location& loc,
                                               Type actual) {
  return CheckType(loc, actual, globals_.back().type,
                   "global initializer expression");
}

Result SharedValidator::OnGlobalInitExpr_GlobalGet(const Location& loc,
                                                   Var ref_global_var) {
  Result result = Result::Ok;
  GlobalType ref_global;
  CHECK_RESULT(CheckGlobalIndex(ref_global_var, &ref_global));

  if (ref_global_var.index() >= num_imported_globals_) {
    result |= PrintError(
        ref_global_var.loc,
        "initializer expression can only reference an imported global");
  }

  if (ref_global.mutable_) {
    result |= PrintError(
        loc, "initializer expression cannot reference a mutable global");
  }

  result |= CheckType(loc, ref_global.type, globals_.back().type,
                      "global initializer expression");
  return result;
}

Result SharedValidator::OnGlobalInitExpr_RefNull(const Location& loc,
                                                 Type type) {
  return CheckType(loc, type, globals_.back().type,
                   "global initializer expression");
}

Result SharedValidator::OnGlobalInitExpr_RefFunc(const Location& loc,
                                                 Var func_var) {
  Result result = Result::Ok;
  result |= CheckFuncIndex(func_var);
  init_expr_funcs_.push_back(func_var);
  result |= CheckType(loc, Type::FuncRef, globals_.back().type,
                      "global initializer expression");
  return result;
}

Result SharedValidator::OnGlobalInitExpr_Other(const Location& loc) {
  return PrintError(
      loc,
      "invalid global initializer expression, must be a constant expression");
}

Result SharedValidator::OnEvent(const Location& loc, Var sig_var) {
  Result result = Result::Ok;
  FuncType type;
  result |= CheckFuncTypeIndex(sig_var, &type);
  if (!type.results.empty()) {
    result |= PrintError(loc, "Event signature must have 0 results.");
  }
  events_.push_back(EventType{type.params});
  return result;
}

Result SharedValidator::OnExport(const Location& loc,
                                 ExternalKind kind,
                                 Var item_var,
                                 string_view name) {
  Result result = Result::Ok;
  auto name_str = name.to_string();
  if (export_names_.find(name_str) != export_names_.end()) {
    result |= PrintError(loc, "duplicate export \"" PRIstringview "\"",
                         WABT_PRINTF_STRING_VIEW_ARG(name));
  }
  export_names_.insert(name_str);

  switch (kind) {
    case ExternalKind::Func:
      result |= CheckFuncIndex(item_var);
      break;

    case ExternalKind::Table:
      result |= CheckTableIndex(item_var);
      break;

    case ExternalKind::Memory:
      result |= CheckMemoryIndex(item_var);
      break;

    case ExternalKind::Global:
      result |= CheckGlobalIndex(item_var);
      break;

    case ExternalKind::Event:
      result |= CheckEventIndex(item_var);
      break;
  }
  return result;
}

Result SharedValidator::OnStart(const Location& loc, Var func_var) {
  Result result = Result::Ok;
  if (starts_++ > 0) {
    result |= PrintError(loc, "only one start function allowed");
  }
  FuncType func_type;
  result |= CheckFuncIndex(func_var, &func_type);
  if (func_type.params.size() != 0) {
    result |= PrintError(loc, "start function must be nullary");
  }
  if (func_type.results.size() != 0) {
    result |= PrintError(loc, "start function must not return anything");
  }
  return result;
}

Result SharedValidator::OnElemSegment(const Location& loc,
                                      Var table_var,
                                      SegmentKind kind) {
  Result result = Result::Ok;
  if (kind == SegmentKind::Active) {
    result |= CheckTableIndex(table_var);
  }
  elems_.push_back(ElemType{Type::Void});  // Updated in OnElemSegmentElemType.
  return result;
}

void SharedValidator::OnElemSegmentElemType(Type elem_type) {
  elems_.back().element = elem_type;
}

Result SharedValidator::OnElemSegmentInitExpr_Const(const Location& loc,
                                                    Type type) {
  return CheckType(loc, type, Type::I32, "elem segment offset");
}

Result SharedValidator::OnElemSegmentInitExpr_GlobalGet(const Location& loc,
                                                        Var global_var) {
  Result result = Result::Ok;
  GlobalType ref_global;
  result |= CheckGlobalIndex(global_var, &ref_global);

  if (ref_global.mutable_) {
    result |= PrintError(
        loc, "initializer expression cannot reference a mutable global");
  }

  result |= CheckType(loc, ref_global.type, Type::I32, "elem segment offset");
  return result;
}

Result SharedValidator::OnElemSegmentInitExpr_Other(const Location& loc) {
  return PrintError(loc,
                    "invalid elem segment offset, must be a constant "
                    "expression; either i32.const or "
                    "global.get.");
}

Result SharedValidator::OnElemSegmentElemExpr_RefNull(const Location& loc,
                                                      Type type) {
  return CheckType(loc, type, elems_.back().element, "elem expression");
}

Result SharedValidator::OnElemSegmentElemExpr_RefFunc(const Location& loc,
                                                      Var func_var) {
  Result result = Result::Ok;
  result |= CheckFuncIndex(func_var);
  declared_funcs_.insert(func_var.index());
  return result;
}

Result SharedValidator::OnElemSegmentElemExpr_Other(const Location& loc) {
  return PrintError(loc,
                    "invalid elem expression expression; must be either "
                    "ref.null or ref.func.");
}

void SharedValidator::OnDataCount(Index count) {
  data_segments_ = count;
}

Result SharedValidator::OnDataSegment(const Location& loc,
                                      Var memory_var,
                                      SegmentKind kind) {
  Result result = Result::Ok;
  if (kind == SegmentKind::Active) {
    result |= CheckMemoryIndex(memory_var);
  }
  return result;
}

Result SharedValidator::OnDataSegmentInitExpr_Const(const Location& loc,
                                                    Type type) {
  auto required =
      memories_.empty() ? Type(Type::I32) : memories_[0].limits.IndexType();
  return CheckType(loc, type, required, "data segment offset");
}

Result SharedValidator::OnDataSegmentInitExpr_GlobalGet(const Location& loc,
                                                        Var global_var) {
  Result result = Result::Ok;
  GlobalType ref_global;
  result |= CheckGlobalIndex(global_var, &ref_global);

  if (ref_global.mutable_) {
    result |= PrintError(
        loc, "initializer expression cannot reference a mutable global");
  }

  auto required =
      memories_.empty() ? Type(Type::I32) : memories_[0].limits.IndexType();
  result |= CheckType(loc, ref_global.type, required, "data segment offset");
  return result;
}

Result SharedValidator::OnDataSegmentInitExpr_Other(const Location& loc) {
  return PrintError(loc,
                    "invalid data segment offset, must be a constant "
                    "expression; either iXX.const or "
                    "global.get.");
}

Result SharedValidator::CheckDeclaredFunc(Var func_var) {
  if (declared_funcs_.count(func_var.index()) == 0) {
    return PrintError(func_var.loc,
                      "function is not declared in any elem sections");
  }
  return Result::Ok;
}

Result SharedValidator::EndModule() {
  // Verify that any ref.func used in init expressions for globals are
  // mentioned in an elems section.  This can't be done while process the
  // globals because the global section comes before the elem section.
  Result result = Result::Ok;
  for (Var func_var : init_expr_funcs_) {
    result |= CheckDeclaredFunc(func_var);
  }
  return result;
}

Result SharedValidator::CheckIndex(Var var, Index max_index, const char* desc) {
  if (var.index() >= max_index) {
    return PrintError(
        var.loc, "%s variable out of range: %" PRIindex " (max %" PRIindex ")",
        desc, var.index(), max_index);
  }
  return Result::Ok;
}

template <typename T>
Result SharedValidator::CheckIndexWithValue(Var var,
                                            const std::vector<T>& values,
                                            T* out,
                                            const char* desc) {
  Result result = CheckIndex(var, values.size(), desc);
  if (out) {
    *out = Succeeded(result) ? values[var.index()] : T{};
  }
  return result;
}

Result SharedValidator::CheckLocalIndex(Var local_var, Type* out_type) {
  auto iter = std::upper_bound(
      locals_.begin(), locals_.end(), local_var.index(),
      [](Index index, const LocalDecl& decl) { return index < decl.end; });
  if (iter == locals_.end()) {
    // TODO: better error
    return PrintError(local_var.loc, "local variable out of range (max %u)",
                      GetLocalCount());
  }
  *out_type = iter->type;
  return Result::Ok;
}

Result SharedValidator::CheckFuncTypeIndex(Var sig_var, FuncType* out) {
  Result result = CheckIndex(sig_var, num_types_, "function type");
  if (Failed(result)) {
    *out = FuncType{};
    return Result::Error;
  }

  auto iter = func_types_.find(sig_var.index());
  if (iter == func_types_.end()) {
    return PrintError(sig_var.loc, "type %d is not a function",
                      sig_var.index());
  }

  if (out) {
    *out = iter->second;
  }
  return Result::Ok;
}

Result SharedValidator::CheckFuncIndex(Var func_var, FuncType* out) {
  return CheckIndexWithValue(func_var, funcs_, out, "function");
}

Result SharedValidator::CheckMemoryIndex(Var memory_var, MemoryType* out) {
  return CheckIndexWithValue(memory_var, memories_, out, "memory");
}

Result SharedValidator::CheckTableIndex(Var table_var, TableType* out) {
  return CheckIndexWithValue(table_var, tables_, out, "table");
}

Result SharedValidator::CheckGlobalIndex(Var global_var, GlobalType* out) {
  return CheckIndexWithValue(global_var, globals_, out, "global");
}

Result SharedValidator::CheckEventIndex(Var event_var, EventType* out) {
  return CheckIndexWithValue(event_var, events_, out, "event");
}

Result SharedValidator::CheckElemSegmentIndex(Var elem_segment_var,
                                              ElemType* out) {
  return CheckIndexWithValue(elem_segment_var, elems_, out, "elem_segment");
}

Result SharedValidator::CheckDataSegmentIndex(Var data_segment_var) {
  return CheckIndex(data_segment_var, data_segments_, "data_segment");
}

Result SharedValidator::CheckBlockSignature(const Location& loc,
                                            Opcode opcode,
                                            Type sig_type,
                                            TypeVector* out_param_types,
                                            TypeVector* out_result_types) {
  Result result = Result::Ok;

  if (sig_type.IsIndex()) {
    Index sig_index = sig_type.GetIndex();
    FuncType func_type;
    result |= CheckFuncTypeIndex(Var(sig_index, loc), &func_type);

    if (!func_type.params.empty() && !options_.features.multi_value_enabled()) {
      result |= PrintError(loc, "%s params not currently supported.",
                           opcode.GetName());
    }
    // Multiple results without --enable-multi-value is checked above in
    // OnType.

    *out_param_types = func_type.params;
    *out_result_types = func_type.results;
  } else {
    out_param_types->clear();
    *out_result_types = sig_type.GetInlineVector();
  }

  return result;
}

Result SharedValidator::BeginFunctionBody(const Location& loc,
                                          Index func_index) {
  expr_loc_ = &loc;
  locals_.clear();
  if (func_index < funcs_.size()) {
    for (Type type : funcs_[func_index].params) {
      // TODO: Coalesce parameters of the same type?
      locals_.push_back(LocalDecl{type, GetLocalCount() + 1});
    }
    return typechecker_.BeginFunction(funcs_[func_index].results);
  } else {
    // Signature isn't available, use empty.
    return typechecker_.BeginFunction(TypeVector());
  }
}

Result SharedValidator::EndFunctionBody(const Location& loc) {
  return typechecker_.EndFunction();
}

Result SharedValidator::OnLocalDecl(const Location& loc,
                                    Index count,
                                    Type type) {
  const auto max_locals = std::numeric_limits<Index>::max();
  if (count > max_locals - GetLocalCount()) {
    PrintError(loc, "local count must be < 0x10000000");
    return Result::Error;
  }
  locals_.push_back(LocalDecl{type, GetLocalCount() + count});
  return Result::Ok;
}

Index SharedValidator::GetLocalCount() const {
  return locals_.empty() ? 0 : locals_.back().end;
}

static bool is_power_of_two(uint32_t x) {
  return x && ((x & (x - 1)) == 0);
}

Result SharedValidator::CheckAlign(const Location& loc,
                                   Address alignment,
                                   Address natural_alignment) {
  if (!is_power_of_two(alignment)) {
    PrintError(loc, "alignment (%" PRIaddress ") must be a power of 2",
               alignment);
    return Result::Error;
  }
  if (alignment > natural_alignment) {
    PrintError(
        loc,
        "alignment must not be larger than natural alignment (%" PRIaddress ")",
        natural_alignment);
    return Result::Error;
  }
  return Result::Ok;
}

Result SharedValidator::CheckAtomicAlign(const Location& loc,
                                         Address alignment,
                                         Address natural_alignment) {
  if (!is_power_of_two(alignment)) {
    PrintError(loc, "alignment (%" PRIaddress ") must be a power of 2",
               alignment);
    return Result::Error;
  }
  if (alignment != natural_alignment) {
    PrintError(loc,
               "alignment must be equal to natural alignment (%" PRIaddress ")",
               natural_alignment);
    return Result::Error;
  }
  return Result::Ok;
}

Result SharedValidator::OnAtomicFence(const Location& loc,
                                      uint32_t consistency_model) {
  Result result = Result::Ok;
  if (consistency_model != 0) {
    result |= PrintError(
        loc, "unexpected atomic.fence consistency model (expected 0): %u",
        consistency_model);
  }
  result |= typechecker_.OnAtomicFence(consistency_model);
  return result;
}

Result SharedValidator::OnAtomicLoad(const Location& loc,
                                     Opcode opcode,
                                     Address alignment) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= CheckAtomicAlign(loc, alignment, opcode.GetMemorySize());
  result |= typechecker_.OnAtomicLoad(opcode, mt.limits);
  return result;
}

Result SharedValidator::OnAtomicNotify(const Location& loc,
                                       Opcode opcode,
                                       Address alignment) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= CheckAtomicAlign(loc, alignment, opcode.GetMemorySize());
  result |= typechecker_.OnAtomicNotify(opcode, mt.limits);
  return result;
}

Result SharedValidator::OnAtomicRmwCmpxchg(const Location& loc,
                                           Opcode opcode,
                                           Address alignment) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= CheckAtomicAlign(loc, alignment, opcode.GetMemorySize());
  result |= typechecker_.OnAtomicRmwCmpxchg(opcode, mt.limits);
  return result;
}

Result SharedValidator::OnAtomicRmw(const Location& loc,
                                    Opcode opcode,
                                    Address alignment) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= CheckAtomicAlign(loc, alignment, opcode.GetMemorySize());
  result |= typechecker_.OnAtomicRmw(opcode, mt.limits);
  return result;
}

Result SharedValidator::OnAtomicStore(const Location& loc,
                                      Opcode opcode,
                                      Address alignment) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= CheckAtomicAlign(loc, alignment, opcode.GetMemorySize());
  result |= typechecker_.OnAtomicStore(opcode, mt.limits);
  return result;
}

Result SharedValidator::OnAtomicWait(const Location& loc,
                                     Opcode opcode,
                                     Address alignment) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= CheckAtomicAlign(loc, alignment, opcode.GetMemorySize());
  result |= typechecker_.OnAtomicWait(opcode, mt.limits);
  return result;
}

Result SharedValidator::OnBinary(const Location& loc, Opcode opcode) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnBinary(opcode);
  return result;
}

Result SharedValidator::OnBlock(const Location& loc, Type sig_type) {
  Result result = Result::Ok;
  TypeVector param_types, result_types;
  expr_loc_ = &loc;
  result |= CheckBlockSignature(loc, Opcode::Block, sig_type, &param_types,
                                &result_types);
  result |= typechecker_.OnBlock(param_types, result_types);
  return result;
}

Result SharedValidator::OnBr(const Location& loc, Var depth) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnBr(depth.index());
  return result;
}

Result SharedValidator::OnBrIf(const Location& loc, Var depth) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnBrIf(depth.index());
  return result;
}

Result SharedValidator::BeginBrTable(const Location& loc) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.BeginBrTable();
  return result;
}

Result SharedValidator::OnBrTableTarget(const Location& loc, Var depth) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnBrTableTarget(depth.index());
  return result;
}

Result SharedValidator::EndBrTable(const Location& loc) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.EndBrTable();
  return result;
}

Result SharedValidator::OnCall(const Location& loc, Var func_var) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  FuncType func_type;
  result |= CheckFuncIndex(func_var, &func_type);
  result |= typechecker_.OnCall(func_type.params, func_type.results);
  return result;
}

Result SharedValidator::OnCallIndirect(const Location& loc,
                                       Var sig_var,
                                       Var table_var) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  FuncType func_type;
  result |= CheckFuncTypeIndex(sig_var, &func_type);
  result |= CheckTableIndex(table_var);
  result |= typechecker_.OnCallIndirect(func_type.params, func_type.results);
  return result;
}

Result SharedValidator::OnCatch(const Location& loc,
                                Var event_var,
                                bool is_catch_all) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  if (is_catch_all) {
    TypeVector empty;
    result |= typechecker_.OnCatch(empty);
  } else {
    EventType event_type;
    result |= CheckEventIndex(event_var, &event_type);
    result |= typechecker_.OnCatch(event_type.params);
  }
  return result;
}

Result SharedValidator::OnCompare(const Location& loc, Opcode opcode) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnCompare(opcode);
  return result;
}

Result SharedValidator::OnConst(const Location& loc, Type type) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnConst(type);
  return result;
}

Result SharedValidator::OnConvert(const Location& loc, Opcode opcode) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnConvert(opcode);
  return result;
}

Result SharedValidator::OnDataDrop(const Location& loc, Var segment_var) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= CheckDataSegmentIndex(segment_var);
  result |= typechecker_.OnDataDrop(segment_var.index());
  return result;
}

Result SharedValidator::OnDelegate(const Location& loc, Var depth) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnDelegate(depth.index());
  return result;
}

Result SharedValidator::OnDrop(const Location& loc) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnDrop();
  return result;
}

Result SharedValidator::OnElemDrop(const Location& loc, Var segment_var) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= CheckElemSegmentIndex(segment_var);
  result |= typechecker_.OnElemDrop(segment_var.index());
  return result;
}

Result SharedValidator::OnElse(const Location& loc) {
  Result result = Result::Ok;
  result |= typechecker_.OnElse();
  return result;
}

Result SharedValidator::OnEnd(const Location& loc) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnEnd();
  return result;
}

Result SharedValidator::OnGlobalGet(const Location& loc, Var global_var) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  GlobalType global_type;
  result |= CheckGlobalIndex(global_var, &global_type);
  result |= typechecker_.OnGlobalGet(global_type.type);
  return result;
}

Result SharedValidator::OnGlobalSet(const Location& loc, Var global_var) {
  Result result = Result::Ok;
  GlobalType global_type;
  result |= CheckGlobalIndex(global_var, &global_type);
  if (!global_type.mutable_) {
    result |= PrintError(
        loc, "can't global.set on immutable global at index %" PRIindex ".",
        global_var.index());
  }
  expr_loc_ = &loc;
  result |= typechecker_.OnGlobalSet(global_type.type);
  return result;
}

Result SharedValidator::OnIf(const Location& loc, Type sig_type) {
  Result result = Result::Ok;
  TypeVector param_types, result_types;
  expr_loc_ = &loc;
  result |= CheckBlockSignature(loc, Opcode::If, sig_type, &param_types,
                                &result_types);
  result |= typechecker_.OnIf(param_types, result_types);
  return result;
}

Result SharedValidator::OnLoad(const Location& loc,
                               Opcode opcode,
                               Address alignment) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= CheckAlign(loc, alignment, opcode.GetMemorySize());
  result |= typechecker_.OnLoad(opcode, mt.limits);
  return result;
}

Result SharedValidator::OnLoadSplat(const Location& loc,
                                    Opcode opcode,
                                    Address alignment) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= CheckAlign(loc, alignment, opcode.GetMemorySize());
  result |= typechecker_.OnLoad(opcode, mt.limits);
  return result;
}

Result SharedValidator::OnLoadZero(const Location& loc,
                                   Opcode opcode,
                                   Address alignment) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= CheckAlign(loc, alignment, opcode.GetMemorySize());
  result |= typechecker_.OnLoad(opcode, mt.limits);
  return result;
}

Result SharedValidator::OnLocalGet(const Location& loc, Var local_var) {
  Result result = Result::Ok;
  Type type = Type::Any;
  expr_loc_ = &loc;
  result |= CheckLocalIndex(local_var, &type);
  result |= typechecker_.OnLocalGet(type);
  return result;
}

Result SharedValidator::OnLocalSet(const Location& loc, Var local_var) {
  Result result = Result::Ok;
  Type type = Type::Any;
  expr_loc_ = &loc;
  result |= CheckLocalIndex(local_var, &type);
  result |= typechecker_.OnLocalSet(type);
  return result;
}

Result SharedValidator::OnLocalTee(const Location& loc, Var local_var) {
  Result result = Result::Ok;
  Type type = Type::Any;
  expr_loc_ = &loc;
  result |= CheckLocalIndex(local_var, &type);
  result |= typechecker_.OnLocalTee(type);
  return result;
}

Result SharedValidator::OnLoop(const Location& loc, Type sig_type) {
  Result result = Result::Ok;
  TypeVector param_types, result_types;
  expr_loc_ = &loc;
  result |= CheckBlockSignature(loc, Opcode::Loop, sig_type, &param_types,
                                &result_types);
  result |= typechecker_.OnLoop(param_types, result_types);
  return result;
}

Result SharedValidator::OnMemoryCopy(const Location& loc) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= typechecker_.OnMemoryCopy(mt.limits);
  return result;
}

Result SharedValidator::OnMemoryFill(const Location& loc) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= typechecker_.OnMemoryFill(mt.limits);
  return result;
}

Result SharedValidator::OnMemoryGrow(const Location& loc) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= typechecker_.OnMemoryGrow(mt.limits);
  return result;
}

Result SharedValidator::OnMemoryInit(const Location& loc, Var segment_var) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= CheckDataSegmentIndex(segment_var);
  result |= typechecker_.OnMemoryInit(segment_var.index(), mt.limits);
  return result;
}

Result SharedValidator::OnMemorySize(const Location& loc) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= typechecker_.OnMemorySize(mt.limits);
  return result;
}

Result SharedValidator::OnNop(const Location& loc) {
  expr_loc_ = &loc;
  return Result::Ok;
}

Result SharedValidator::OnRefFunc(const Location& loc, Var func_var) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= CheckDeclaredFunc(func_var);
  result |= typechecker_.OnRefFuncExpr(func_var.index());
  return result;
}

Result SharedValidator::OnRefIsNull(const Location& loc) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnRefIsNullExpr();
  return result;
}

Result SharedValidator::OnRefNull(const Location& loc, Type type) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnRefNullExpr(type);
  return result;
}

Result SharedValidator::OnRethrow(const Location& loc, Var depth) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnRethrow(depth.index());
  return result;
}

Result SharedValidator::OnReturnCall(const Location& loc, Var func_var) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  FuncType func_type;
  result |= CheckFuncIndex(func_var, &func_type);
  result |= typechecker_.OnReturnCall(func_type.params, func_type.results);
  return result;
}

Result SharedValidator::OnReturnCallIndirect(const Location& loc,
                                             Var sig_var,
                                             Var table_var) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= CheckTableIndex(table_var);
  FuncType func_type;
  result |= CheckFuncTypeIndex(sig_var, &func_type);
  result |=
      typechecker_.OnReturnCallIndirect(func_type.params, func_type.results);
  return result;
}

Result SharedValidator::OnReturn(const Location& loc) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnReturn();
  return result;
}

Result SharedValidator::OnSelect(const Location& loc,
                                 Index result_count,
                                 Type* result_types) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  if (result_count > 1) {
    result |=
        PrintError(loc, "invalid arity in select instruction: %" PRIindex ".",
                   result_count);
  } else {
    result |= typechecker_.OnSelect(ToTypeVector(result_count, result_types));
  }
  return result;
}

Result SharedValidator::OnSimdLaneOp(const Location& loc,
                                     Opcode opcode,
                                     uint64_t value) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnSimdLaneOp(opcode, value);
  return result;
}

Result SharedValidator::OnSimdLoadLane(const Location& loc,
                                     Opcode opcode,
                                     Address alignment,
                                     uint64_t value) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= CheckAlign(loc, alignment, opcode.GetMemorySize());
  result |= typechecker_.OnSimdLoadLane(opcode, mt.limits, value);
  return result;
}

Result SharedValidator::OnSimdStoreLane(const Location& loc,
                                        Opcode opcode,
                                        Address alignment,
                                        uint64_t value) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= CheckAlign(loc, alignment, opcode.GetMemorySize());
  result |= typechecker_.OnSimdStoreLane(opcode, mt.limits, value);
  return result;
}

Result SharedValidator::OnSimdShuffleOp(const Location& loc,
                                        Opcode opcode,
                                        v128 value) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnSimdShuffleOp(opcode, value);
  return result;
}

Result SharedValidator::OnStore(const Location& loc,
                                Opcode opcode,
                                Address alignment) {
  Result result = Result::Ok;
  MemoryType mt;
  expr_loc_ = &loc;
  result |= CheckMemoryIndex(Var(0, loc), &mt);
  result |= CheckAlign(loc, alignment, opcode.GetMemorySize());
  result |= typechecker_.OnStore(opcode, mt.limits);
  return result;
}

Result SharedValidator::OnTableCopy(const Location& loc,
                                    Var dst_var,
                                    Var src_var) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  TableType dst_table;
  TableType src_table;
  result |= CheckTableIndex(dst_var, &dst_table);
  result |= CheckTableIndex(src_var, &src_table);
  result |= typechecker_.OnTableCopy();
  result |= CheckType(loc, src_table.element, dst_table.element, "table.copy");
  return result;
}

Result SharedValidator::OnTableFill(const Location& loc, Var table_var) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  TableType table_type;
  result |= CheckTableIndex(table_var, &table_type);
  result |= typechecker_.OnTableFill(table_type.element);
  return result;
}

Result SharedValidator::OnTableGet(const Location& loc, Var table_var) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  TableType table_type;
  result |= CheckTableIndex(table_var, &table_type);
  result |= typechecker_.OnTableGet(table_type.element);
  return result;
}

Result SharedValidator::OnTableGrow(const Location& loc, Var table_var) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  TableType table_type;
  result |= CheckTableIndex(table_var, &table_type);
  result |= typechecker_.OnTableGrow(table_type.element);
  return result;
}

Result SharedValidator::OnTableInit(const Location& loc,
                                    Var segment_var,
                                    Var table_var) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  TableType table_type;
  ElemType elem_type;
  result |= CheckTableIndex(table_var, &table_type);
  result |= CheckElemSegmentIndex(segment_var, &elem_type);
  result |= typechecker_.OnTableInit(table_var.index(), segment_var.index());
  result |= CheckType(loc, elem_type.element, table_type.element, "table.init");
  return result;
}

Result SharedValidator::OnTableSet(const Location& loc, Var table_var) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  TableType table_type;
  result |= CheckTableIndex(table_var, &table_type);
  result |= typechecker_.OnTableSet(table_type.element);
  return result;
}

Result SharedValidator::OnTableSize(const Location& loc, Var table_var) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= CheckTableIndex(table_var);
  result |= typechecker_.OnTableSize();
  return result;
}

Result SharedValidator::OnTernary(const Location& loc, Opcode opcode) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnTernary(opcode);
  return result;
}

Result SharedValidator::OnThrow(const Location& loc, Var event_var) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  EventType event_type;
  result |= CheckEventIndex(event_var, &event_type);
  result |= typechecker_.OnThrow(event_type.params);
  return result;
}

Result SharedValidator::OnTry(const Location& loc, Type sig_type) {
  Result result = Result::Ok;
  TypeVector param_types, result_types;
  expr_loc_ = &loc;
  result |= CheckBlockSignature(loc, Opcode::Try, sig_type, &param_types,
                                &result_types);
  result |= typechecker_.OnTry(param_types, result_types);
  return result;
}

Result SharedValidator::OnUnary(const Location& loc, Opcode opcode) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnUnary(opcode);
  return result;
}

Result SharedValidator::OnUnreachable(const Location& loc) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnUnreachable();
  return result;
}

Result SharedValidator::OnUnwind(const Location& loc) {
  Result result = Result::Ok;
  expr_loc_ = &loc;
  result |= typechecker_.OnUnwind();
  return result;
}

}  // namespace wabt
