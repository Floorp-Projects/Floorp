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

#ifndef WABT_IR_UTIL_H_
#define WABT_IR_UTIL_H_

#include "src/common.h"
#include "src/ir.h"

namespace wabt {

struct Label {
  Label(LabelType label_type,
        const std::string& name,
        const TypeVector& param_types,
        const TypeVector& result_types)
      : name(name),
        label_type(label_type),
        param_types(param_types),
        result_types(result_types) {}

  std::string name;
  LabelType label_type;
  TypeVector param_types;
  TypeVector result_types;
};

struct ModuleContext {
  ModuleContext(const Module &module) : module(module) {}

  Index GetLabelStackSize() const { return label_stack_.size(); }
  const Label* GetLabel(const Var& var) const;
  Index GetLabelArity(const Var& var) const;
  void SetTopLabelType(LabelType label_type) {
    label_stack_.back().label_type = label_type;
  }

  Index GetFuncParamCount(const Var& var) const;
  Index GetFuncResultCount(const Var& var) const;

  void BeginBlock(LabelType label_type, const Block& block);
  void EndBlock();
  void BeginFunc(const Func& func);
  void EndFunc();

  struct Arities {
    Index nargs;
    Index nreturns;
    bool unreachable;
    Arities(Index na, Index nr, bool ur = false)
      : nargs(na), nreturns(nr), unreachable(ur) {}
  };
  Arities GetExprArity(const Expr& expr) const;

  const Module &module;
 private:
  const Func* current_func_ = nullptr;
  std::vector<Label> label_stack_;
};

}  // namespace wabt

#endif /* WABT_IR_UTIL_H_ */
