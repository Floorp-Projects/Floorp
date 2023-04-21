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

#ifndef WABT_SHARED_VALIDATOR_H_
#define WABT_SHARED_VALIDATOR_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "wabt/common.h"
#include "wabt/error.h"
#include "wabt/feature.h"
#include "wabt/ir.h"
#include "wabt/opcode.h"
#include "wabt/type-checker.h"

#include "wabt/binary-reader.h"  // For TypeMut.

namespace wabt {

struct ValidateOptions {
  ValidateOptions() = default;
  ValidateOptions(const Features& features) : features(features) {}

  Features features;
};

class SharedValidator {
 public:
  WABT_DISALLOW_COPY_AND_ASSIGN(SharedValidator);
  SharedValidator(Errors*, const ValidateOptions& options);

  // TODO: Move into SharedValidator?
  using Label = TypeChecker::Label;
  size_t type_stack_size() const { return typechecker_.type_stack_size(); }
  Result GetLabel(Index depth, Label** out_label) {
    return typechecker_.GetLabel(depth, out_label);
  }
  Result GetCatchCount(Index depth, Index* out_count) {
    return typechecker_.GetCatchCount(depth, out_count);
  }

  Result WABT_PRINTF_FORMAT(3, 4)
      PrintError(const Location& loc, const char* fmt, ...);

  void OnTypecheckerError(const char* msg);

  Index GetLocalCount() const;

  Result EndModule();

  Result OnFuncType(const Location&,
                    Index param_count,
                    const Type* param_types,
                    Index result_count,
                    const Type* result_types,
                    Index type_index);
  Result OnStructType(const Location&, Index field_count, TypeMut* fields);
  Result OnArrayType(const Location&, TypeMut field);

  Result OnFunction(const Location&, Var sig_var);
  Result OnTable(const Location&, Type elem_type, const Limits&);
  Result OnMemory(const Location&, const Limits&);
  Result OnGlobalImport(const Location&, Type type, bool mutable_);
  Result OnGlobal(const Location&, Type type, bool mutable_);
  Result OnTag(const Location&, Var sig_var);

  Result OnExport(const Location&,
                  ExternalKind,
                  Var item_var,
                  std::string_view name);

  Result OnStart(const Location&, Var func_var);

  Result OnElemSegment(const Location&, Var table_var, SegmentKind);
  Result OnElemSegmentElemType(const Location&, Type elem_type);
  Result OnElemSegmentElemExpr_RefNull(const Location&, Type type);
  Result OnElemSegmentElemExpr_RefFunc(const Location&, Var func_var);
  Result OnElemSegmentElemExpr_Other(const Location&);

  void OnDataCount(Index count);
  Result OnDataSegment(const Location&, Var memory_var, SegmentKind);

  Result BeginInitExpr(const Location&, Type type);
  Result EndInitExpr();

  Result BeginFunctionBody(const Location&, Index func_index);
  Result EndFunctionBody(const Location&);
  Result OnLocalDecl(const Location&, Index count, Type type);

  Result OnAtomicFence(const Location&, uint32_t consistency_model);
  Result OnAtomicLoad(const Location&, Opcode, Var memidx, Address align);
  Result OnAtomicNotify(const Location&, Opcode, Var memidx, Address align);
  Result OnAtomicRmwCmpxchg(const Location&, Opcode, Var memidx, Address align);
  Result OnAtomicRmw(const Location&, Opcode, Var memidx, Address align);
  Result OnAtomicStore(const Location&, Opcode, Var memidx, Address align);
  Result OnAtomicWait(const Location&, Opcode, Var memidx, Address align);
  Result OnBinary(const Location&, Opcode);
  Result OnBlock(const Location&, Type sig_type);
  Result OnBr(const Location&, Var depth);
  Result OnBrIf(const Location&, Var depth);
  Result BeginBrTable(const Location&);
  Result OnBrTableTarget(const Location&, Var depth);
  Result EndBrTable(const Location&);
  Result OnCall(const Location&, Var func_var);
  Result OnCallIndirect(const Location&, Var sig_var, Var table_var);
  Result OnCallRef(const Location&, Index* function_type_index);
  Result OnCatch(const Location&, Var tag_var, bool is_catch_all);
  Result OnCompare(const Location&, Opcode);
  Result OnConst(const Location&, Type);
  Result OnConvert(const Location&, Opcode);
  Result OnDataDrop(const Location&, Var segment_var);
  Result OnDelegate(const Location&, Var depth);
  Result OnDrop(const Location&);
  Result OnElemDrop(const Location&, Var segment_var);
  Result OnElse(const Location&);
  Result OnEnd(const Location&);
  Result OnGlobalGet(const Location&, Var);
  Result OnGlobalSet(const Location&, Var);
  Result OnIf(const Location&, Type sig_type);
  Result OnLoad(const Location&, Opcode, Var memidx, Address align);
  Result OnLoadSplat(const Location&, Opcode, Var memidx, Address align);
  Result OnLoadZero(const Location&, Opcode, Var memidx, Address align);
  Result OnLocalGet(const Location&, Var);
  Result OnLocalSet(const Location&, Var);
  Result OnLocalTee(const Location&, Var);
  Result OnLoop(const Location&, Type sig_type);
  Result OnMemoryCopy(const Location&, Var srcmemidx, Var destmemidx);
  Result OnMemoryFill(const Location&, Var memidx);
  Result OnMemoryGrow(const Location&, Var memidx);
  Result OnMemoryInit(const Location&, Var segment_var, Var memidx);
  Result OnMemorySize(const Location&, Var memidx);
  Result OnNop(const Location&);
  Result OnRefFunc(const Location&, Var func_var);
  Result OnRefIsNull(const Location&);
  Result OnRefNull(const Location&, Type type);
  Result OnRethrow(const Location&, Var depth);
  Result OnReturnCall(const Location&, Var func_var);
  Result OnReturnCallIndirect(const Location&, Var sig_var, Var table_var);
  Result OnReturn(const Location&);
  Result OnSelect(const Location&, Index result_count, Type* result_types);
  Result OnSimdLaneOp(const Location&, Opcode, uint64_t lane_idx);
  Result OnSimdLoadLane(const Location&,
                        Opcode,
                        Var memidx,
                        Address align,
                        uint64_t lane_idx);
  Result OnSimdStoreLane(const Location&,
                         Opcode,
                         Var memidx,
                         Address align,
                         uint64_t lane_idx);
  Result OnSimdShuffleOp(const Location&, Opcode, v128 lane_idx);
  Result OnStore(const Location&, Opcode, Var memidx, Address align);
  Result OnTableCopy(const Location&, Var dst_var, Var src_var);
  Result OnTableFill(const Location&, Var table_var);
  Result OnTableGet(const Location&, Var table_var);
  Result OnTableGrow(const Location&, Var table_var);
  Result OnTableInit(const Location&, Var segment_var, Var table_var);
  Result OnTableSet(const Location&, Var table_var);
  Result OnTableSize(const Location&, Var table_var);
  Result OnTernary(const Location&, Opcode);
  Result OnThrow(const Location&, Var tag_var);
  Result OnTry(const Location&, Type sig_type);
  Result OnUnary(const Location&, Opcode);
  Result OnUnreachable(const Location&);

 private:
  struct FuncType {
    FuncType() = default;
    FuncType(const TypeVector& params,
             const TypeVector& results,
             Index type_index)
        : params(params), results(results), type_index(type_index) {}

    TypeVector params;
    TypeVector results;
    Index type_index;
  };

  struct StructType {
    StructType() = default;
    StructType(const TypeMutVector& fields) : fields(fields) {}

    TypeMutVector fields;
  };

  struct ArrayType {
    ArrayType() = default;
    ArrayType(TypeMut field) : field(field) {}

    TypeMut field;
  };

  struct TableType {
    TableType() = default;
    TableType(Type element, Limits limits) : element(element), limits(limits) {}

    Type element = Type::Any;
    Limits limits;
  };

  struct MemoryType {
    MemoryType() = default;
    MemoryType(Limits limits) : limits(limits) {}

    Limits limits;
  };

  struct GlobalType {
    GlobalType() = default;
    GlobalType(Type type, bool mutable_) : type(type), mutable_(mutable_) {}

    Type type = Type::Any;
    bool mutable_ = true;
  };

  struct TagType {
    TypeVector params;
  };

  struct ElemType {
    ElemType() = default;
    ElemType(Type element, bool is_active, Type table_type)
        : element(element), is_active(is_active), table_type(table_type) {}

    Type element;
    bool is_active;
    Type table_type;
  };

  struct LocalDecl {
    Type type;
    Index end;
  };

  bool ValidInitOpcode(Opcode opcode) const;
  Result CheckInstr(Opcode opcode, const Location& loc);
  Result CheckType(const Location&,
                   Type actual,
                   Type expected,
                   const char* desc);
  Result CheckLimits(const Location&,
                     const Limits&,
                     uint64_t absolute_max,
                     const char* desc);

  Result CheckLocalIndex(Var local_var, Type* out_type);

  Result CheckDeclaredFunc(Var func_var);

  Result CheckIndex(Var var, Index max_index, const char* desc);
  template <typename T>
  Result CheckIndexWithValue(Var var,
                             const std::vector<T>& values,
                             T* out,
                             const char* desc);
  Result CheckFuncTypeIndex(Var sig_var, FuncType* out = nullptr);
  Result CheckFuncIndex(Var func_var, FuncType* out = nullptr);
  Result CheckTableIndex(Var table_var, TableType* out = nullptr);
  Result CheckMemoryIndex(Var memory_var, MemoryType* out = nullptr);
  Result CheckGlobalIndex(Var global_var, GlobalType* out = nullptr);
  Result CheckTagIndex(Var tag_var, TagType* out = nullptr);
  Result CheckElemSegmentIndex(Var elem_segment_var, ElemType* out = nullptr);
  Result CheckDataSegmentIndex(Var data_segment_var);

  Result CheckAlign(const Location&, Address align, Address natural_align);
  Result CheckAtomicAlign(const Location&,
                          Address align,
                          Address natural_align);

  Result CheckBlockSignature(const Location&,
                             Opcode,
                             Type sig_type,
                             TypeVector* out_param_types,
                             TypeVector* out_result_types);

  Index GetFunctionTypeIndex(Index func_index) const;

  TypeVector ToTypeVector(Index count, const Type* types);

  ValidateOptions options_;
  Errors* errors_;
  TypeChecker typechecker_;  // TODO: Move into SharedValidator.
  // Cached for access by OnTypecheckerError.
  Location expr_loc_ = Location(kInvalidOffset);
  bool in_init_expr_ = false;

  Index num_types_ = 0;
  std::map<Index, FuncType> func_types_;
  std::map<Index, StructType> struct_types_;
  std::map<Index, ArrayType> array_types_;

  std::vector<FuncType> funcs_;       // Includes imported and defined.
  std::vector<TableType> tables_;     // Includes imported and defined.
  std::vector<MemoryType> memories_;  // Includes imported and defined.
  std::vector<GlobalType> globals_;   // Includes imported and defined.
  std::vector<TagType> tags_;         // Includes imported and defined.
  std::vector<ElemType> elems_;
  Index starts_ = 0;
  Index num_imported_globals_ = 0;
  Index data_segments_ = 0;

  // Includes parameters, since this is only used for validating
  // local.{get,set,tee} instructions.
  std::vector<LocalDecl> locals_;

  std::set<std::string> export_names_;  // Used to check for duplicates.
  std::set<Index> declared_funcs_;      // TODO: optimize?
  std::vector<Var> check_declared_funcs_;
};

}  // namespace wabt

#endif  // WABT_SHARED_VALIDATOR_H_
