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

#ifndef WABT_TYPE_CHECKER_H_
#define WABT_TYPE_CHECKER_H_

#include <functional>
#include <vector>

#include "wabt/common.h"
#include "wabt/feature.h"
#include "wabt/opcode.h"

namespace wabt {

class TypeChecker {
 public:
  using ErrorCallback = std::function<void(const char* msg)>;

  struct Label {
    Label(LabelType,
          const TypeVector& param_types,
          const TypeVector& result_types,
          size_t limit);

    TypeVector& br_types() {
      return label_type == LabelType::Loop ? param_types : result_types;
    }

    LabelType label_type;
    TypeVector param_types;
    TypeVector result_types;
    size_t type_stack_limit;
    bool unreachable;
  };

  explicit TypeChecker(const Features& features) : features_(features) {}

  void set_error_callback(const ErrorCallback& error_callback) {
    error_callback_ = error_callback;
  }

  size_t type_stack_size() const { return type_stack_.size(); }

  bool IsUnreachable();
  Result GetLabel(Index depth, Label** out_label);
  Result GetRethrowLabel(Index depth, Label** out_label);
  Result GetCatchCount(Index depth, Index* out_depth);

  Result BeginFunction(const TypeVector& sig);
  Result OnAtomicFence(uint32_t consistency_model);
  Result OnAtomicLoad(Opcode, const Limits& limits);
  Result OnAtomicNotify(Opcode, const Limits& limits);
  Result OnAtomicStore(Opcode, const Limits& limits);
  Result OnAtomicRmw(Opcode, const Limits& limits);
  Result OnAtomicRmwCmpxchg(Opcode, const Limits& limits);
  Result OnAtomicWait(Opcode, const Limits& limits);
  Result OnBinary(Opcode);
  Result OnBlock(const TypeVector& param_types, const TypeVector& result_types);
  Result OnBr(Index depth);
  Result OnBrIf(Index depth);
  Result BeginBrTable();
  Result OnBrTableTarget(Index depth);
  Result EndBrTable();
  Result OnCall(const TypeVector& param_types, const TypeVector& result_types);
  Result OnCallIndirect(const TypeVector& param_types,
                        const TypeVector& result_types);
  Result OnIndexedFuncRef(Index* out_index);
  Result OnReturnCall(const TypeVector& param_types,
                      const TypeVector& result_types);
  Result OnReturnCallIndirect(const TypeVector& param_types,
                              const TypeVector& result_types);
  Result OnCatch(const TypeVector& sig);
  Result OnCompare(Opcode);
  Result OnConst(Type);
  Result OnConvert(Opcode);
  Result OnDelegate(Index depth);
  Result OnDrop();
  Result OnElse();
  Result OnEnd();
  Result OnGlobalGet(Type);
  Result OnGlobalSet(Type);
  Result OnIf(const TypeVector& param_types, const TypeVector& result_types);
  Result OnLoad(Opcode, const Limits& limits);
  Result OnLocalGet(Type);
  Result OnLocalSet(Type);
  Result OnLocalTee(Type);
  Result OnLoop(const TypeVector& param_types, const TypeVector& result_types);
  Result OnMemoryCopy(const Limits& srclimits, const Limits& dstlimits);
  Result OnDataDrop(Index);
  Result OnMemoryFill(const Limits& limits);
  Result OnMemoryGrow(const Limits& limits);
  Result OnMemoryInit(Index, const Limits& limits);
  Result OnMemorySize(const Limits& limits);
  Result OnTableCopy();
  Result OnElemDrop(Index);
  Result OnTableInit(Index, Index);
  Result OnTableGet(Type elem_type);
  Result OnTableSet(Type elem_type);
  Result OnTableGrow(Type elem_type);
  Result OnTableSize();
  Result OnTableFill(Type elem_type);
  Result OnRefFuncExpr(Index func_type);
  Result OnRefNullExpr(Type type);
  Result OnRefIsNullExpr();
  Result OnRethrow(Index depth);
  Result OnReturn();
  Result OnSelect(const TypeVector& result_types);
  Result OnSimdLaneOp(Opcode, uint64_t);
  Result OnSimdLoadLane(Opcode, const Limits& limits, uint64_t);
  Result OnSimdStoreLane(Opcode, const Limits& limits, uint64_t);
  Result OnSimdShuffleOp(Opcode, v128);
  Result OnStore(Opcode, const Limits& limits);
  Result OnTernary(Opcode);
  Result OnThrow(const TypeVector& sig);
  Result OnTry(const TypeVector& param_types, const TypeVector& result_types);
  Result OnUnary(Opcode);
  Result OnUnreachable();
  Result EndFunction();

  Result BeginInitExpr(Type type);
  Result EndInitExpr();

  static Result CheckType(Type actual, Type expected);

 private:
  void WABT_PRINTF_FORMAT(2, 3) PrintError(const char* fmt, ...);
  Result TopLabel(Label** out_label);
  void ResetTypeStackToLabel(Label* label);
  Result SetUnreachable();
  void PushLabel(LabelType label_type,
                 const TypeVector& param_types,
                 const TypeVector& result_types);
  Result PopLabel();
  Result CheckLabelType(Label* label, LabelType label_type);
  Result Check2LabelTypes(Label* label,
                          LabelType label_type1,
                          LabelType label_type2);
  Result GetThisFunctionLabel(Label** label);
  Result PeekType(Index depth, Type* out_type);
  Result PeekAndCheckType(Index depth, Type expected);
  Result DropTypes(size_t drop_count);
  void PushType(Type type);
  void PushTypes(const TypeVector& types);
  Result CheckTypeStackEnd(const char* desc);
  Result CheckTypes(const TypeVector& actual, const TypeVector& expected);
  Result CheckSignature(const TypeVector& sig, const char* desc);
  Result CheckReturnSignature(const TypeVector& sig,
                              const TypeVector& expected,
                              const char* desc);
  Result PopAndCheckSignature(const TypeVector& sig, const char* desc);
  Result PopAndCheckCall(const TypeVector& param_types,
                         const TypeVector& result_types,
                         const char* desc);
  Result PopAndCheck1Type(Type expected, const char* desc);
  Result PopAndCheck2Types(Type expected1, Type expected2, const char* desc);
  Result PopAndCheck3Types(Type expected1,
                           Type expected2,
                           Type expected3,
                           const char* desc);
  Result CheckOpcode1(Opcode opcode, const Limits* limits = nullptr);
  Result CheckOpcode2(Opcode opcode, const Limits* limits = nullptr);
  Result CheckOpcode3(Opcode opcode,
                      const Limits* limits1 = nullptr,
                      const Limits* limits2 = nullptr,
                      const Limits* limits3 = nullptr);
  Result OnEnd(Label* label, const char* sig_desc, const char* end_desc);

  template <typename... Args>
  void PrintStackIfFailed(Result result, const char* desc, Args... args) {
    // Minor optimization, check result before constructing the vector to pass
    // to the other overload of PrintStackIfFailed.
    if (Failed(result)) {
      PrintStackIfFailedV(result, desc, {args...}, /*is_end=*/false);
    }
  }

  void PrintStackIfFailedV(Result,
                           const char* desc,
                           const TypeVector&,
                           bool is_end);

  ErrorCallback error_callback_;
  TypeVector type_stack_;
  std::vector<Label> label_stack_;
  // Cache the expected br_table signature. It will be initialized to `nullptr`
  // to represent "any".
  TypeVector* br_table_sig_ = nullptr;
  Features features_;
};

}  // namespace wabt

#endif /* WABT_TYPE_CHECKER_H_ */
