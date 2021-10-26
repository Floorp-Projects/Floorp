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

#ifndef WABT_DECOMPILER_AST_H_
#define WABT_DECOMPILER_AST_H_

#include "src/cast.h"
#include "src/generate-names.h"
#include "src/ir.h"
#include "src/ir-util.h"

#include <map>

namespace wabt {

enum class NodeType {
  Uninitialized,
  FlushToVars,
  FlushedVar,
  Statements,
  EndReturn,
  Decl,
  DeclInit,
  Expr
};

// The AST we're going to convert the standard IR into.
struct Node {
  NodeType ntype;
  ExprType etype;  // Only if ntype == Expr.
  const Expr* e;
  std::vector<Node> children;
  // Node specific annotations.
  union {
    struct { Index var_start, var_count; };  // FlushedVar/FlushToVars
    const Var* var;  // Decl/DeclInit.
    LabelType lt;  // br/br_if target.
  } u;

  Node() : ntype(NodeType::Uninitialized), etype(ExprType::Nop), e(nullptr) {
  }
  Node(NodeType ntype, ExprType etype, const Expr* e, const Var* v)
    : ntype(ntype), etype(etype), e(e) {
    u.var = v;
  }

  // This value should really never be copied, only moved.
  Node(const Node& rhs) = delete;
  Node& operator=(const Node& rhs) = delete;

  Node(Node&& rhs) { *this = std::move(rhs); }
  Node& operator=(Node&& rhs) {
    ntype = rhs.ntype;
    // Reset ntype to avoid moved from values still being used.
    rhs.ntype = NodeType::Uninitialized;
    etype = rhs.etype;
    rhs.etype = ExprType::Nop;
    e = rhs.e;
    std::swap(children, rhs.children);
    u = rhs.u;
    return *this;
  }
};

struct AST {
  AST(ModuleContext& mc, const Func *f) : mc(mc), f(f) {
    if (f) {
      mc.BeginFunc(*f);
      for (Index i = 0; i < f->GetNumParams(); i++) {
        auto name = "$" + IndexToAlphaName(i);
        vars_defined.insert({ name, { 0, false }});
      }
    }
  }

  ~AST() {
    if (f) mc.EndFunc();
  }

  // Create a new node, take nargs existing nodes on the exp stack as children.
  Node& InsertNode(NodeType ntype, ExprType etype, const Expr* e, Index nargs) {
    assert(exp_stack.size() >= nargs);
    Node n { ntype, etype, e, nullptr };
    n.children.reserve(nargs);
    std::move(exp_stack.end() - nargs, exp_stack.end(),
              std::back_inserter(n.children));
    exp_stack.erase(exp_stack.end() - nargs, exp_stack.end());
    exp_stack.push_back(std::move(n));
    return exp_stack.back();
  }

  template<ExprType T> void PreDecl(const VarExpr<T>& ve) {
    // FIXME: this is slow, and would be better to avoid in callers.
    // See https://github.com/WebAssembly/wabt/issues/1565
    // And https://github.com/WebAssembly/wabt/issues/1665
    for (auto& n : predecls) {
      if (n.u.var->name() == ve.var.name()) {
        return;
      }
    }
    predecls.emplace_back(NodeType::Decl, ExprType::Nop, nullptr, &ve.var);
  }

  template<ExprType T> void Get(const VarExpr<T>& ve, bool local) {
    if (local) {
      auto ret = vars_defined.insert({ ve.var.name(), { cur_block_id, false }});
      if (ret.second) {
        // Use before def, may happen since locals are guaranteed 0.
        PreDecl(ve);
      } else if (blocks_closed[ret.first->second.block_id]) {
        // This is a use of a variable that was defined in a block that has
        // already ended. This happens rarely, but we should cater for this
        // case by lifting it to the top scope.
        PreDecl(ve);
      }
    }
    InsertNode(NodeType::Expr, T, &ve, 0);
  }

  template<ExprType T> void Set(const VarExpr<T>& ve, bool local) {
    // Seen this var before?
    if (local &&
        vars_defined.insert({ ve.var.name(), { cur_block_id, false }}).second) {
      if (value_stack_depth == 1) {
        // Top level, declare it here.
        InsertNode(NodeType::DeclInit, ExprType::Nop, nullptr, 1).u.var =
            &ve.var;
        return;
      } else {
        // Inside exp, better leave it as assignment exp and lift the decl out.
        PreDecl(ve);
      }
    }
    InsertNode(NodeType::Expr, T, &ve, 1);
  }

  template<ExprType T> void Block(const BlockExprBase<T>& be, LabelType label) {
    mc.BeginBlock(label, be.block);
    Construct(be.block.exprs, be.block.decl.GetNumResults(), be.block.decl.GetNumParams(), false);
    mc.EndBlock();
    InsertNode(NodeType::Expr, T, &be, 1);
  }

  void Construct(const Expr& e) {
    auto arity = mc.GetExprArity(e);
    switch (e.type()) {
      case ExprType::LocalGet: {
        Get(*cast<LocalGetExpr>(&e), true);
        return;
      }
      case ExprType::GlobalGet: {
        Get(*cast<GlobalGetExpr>(&e), false);
        return;
      }
      case ExprType::LocalSet: {
        Set(*cast<LocalSetExpr>(&e), true);
        return;
      }
      case ExprType::GlobalSet: {
        Set(*cast<GlobalSetExpr>(&e), false);
        return;
      }
      case ExprType::LocalTee: {
        auto& lt = *cast<LocalTeeExpr>(&e);
        Set(lt, true);
        if (value_stack_depth == 1) {  // Tee is the only thing on there.
          Get(lt, true);  // Now Set + Get instead.
        } else {
          // Things are above us on the stack so the Tee can't be eliminated.
          // The Set makes this work as a Tee when consumed by a parent.
        }
        return;
      }
      case ExprType::If: {
        auto ife = cast<IfExpr>(&e);
        value_stack_depth--;  // Condition.
        mc.BeginBlock(LabelType::Block, ife->true_);
        Construct(ife->true_.exprs, ife->true_.decl.GetNumResults(), ife->true_.decl.GetNumParams(), false);
        if (!ife->false_.empty()) {
          Construct(ife->false_, ife->true_.decl.GetNumResults(), ife->true_.decl.GetNumParams(), false);
        }
        mc.EndBlock();
        value_stack_depth++;  // Put Condition back.
        InsertNode(NodeType::Expr, ExprType::If, &e, ife->false_.empty() ? 2 : 3);
        return;
      }
      case ExprType::Block: {
        Block(*cast<BlockExpr>(&e), LabelType::Block);
        return;
      }
      case ExprType::Loop: {
        Block(*cast<LoopExpr>(&e), LabelType::Loop);
        return;
      }
      case ExprType::Br: {
        InsertNode(NodeType::Expr, ExprType::Br, &e, 0).u.lt =
            mc.GetLabel(cast<BrExpr>(&e)->var)->label_type;
        return;
      }
      case ExprType::BrIf: {
        InsertNode(NodeType::Expr, ExprType::BrIf, &e, 1).u.lt =
            mc.GetLabel(cast<BrIfExpr>(&e)->var)->label_type;
        return;
      }
      default: {
        InsertNode(NodeType::Expr, e.type(), &e, arity.nargs);
        return;
      }
    }
  }

  void Construct(const ExprList& es, Index nresults, Index nparams, bool is_function_body) {
    block_stack.push_back(cur_block_id);
    cur_block_id = blocks_closed.size();
    blocks_closed.push_back(false);
    auto start = exp_stack.size();
    int value_stack_depth_start = value_stack_depth - nparams;
    auto value_stack_in_variables = value_stack_depth;
    bool unreachable = false;
    for (auto& e : es) {
      Construct(e);
      auto arity = mc.GetExprArity(e);
      value_stack_depth -= arity.nargs;
      value_stack_in_variables = std::min(value_stack_in_variables,
                                          value_stack_depth);
      unreachable = unreachable || arity.unreachable;
      assert(unreachable || value_stack_depth >= value_stack_depth_start);
      value_stack_depth += arity.nreturns;
      // We maintain the invariant that a value_stack_depth of N is represented
      // by the last N exp_stack items (each of which returning exactly 1
      // value), all exp_stack items before it return void ("statements").
      // In particular for the wasm stack:
      // - The values between 0 and value_stack_depth_start are part of the
      //   parent block, and not touched here.
      // - The values from there up to value_stack_in_variables are variables
      //   to be used, representing previous statements that flushed the
      //   stack into variables.
      // - Values on top of that up to value_stack_depth are exps returning
      //   a single value.
      // The code below maintains the above invariants. With this in place
      // code "falls into place" the way you expect it.
      if (arity.nreturns != 1) {
        auto num_vars = value_stack_in_variables - value_stack_depth_start;
        auto num_vals = value_stack_depth - value_stack_in_variables;
        auto GenFlushVars = [&](int nargs) {
          auto& ftv = InsertNode(NodeType::FlushToVars, ExprType::Nop, nullptr,
                                 nargs);
          ftv.u.var_start = flushed_vars;
          ftv.u.var_count = num_vals;
        };
        auto MoveStatementsBelowVars = [&](size_t amount) {
          std::rotate(exp_stack.end() - num_vars - amount,
                      exp_stack.end() - amount,
                      exp_stack.end());
        };
        auto GenFlushedVars = [&]() {
          // Re-generate these values as vars.
          for (int i = 0; i < num_vals; i++) {
            auto& fv = InsertNode(NodeType::FlushedVar, ExprType::Nop, nullptr,
                                  0);
            fv.u.var_start = flushed_vars++;
            fv.u.var_count = 1;
          }
        };
        if (arity.nreturns == 0 &&
            value_stack_depth > value_stack_depth_start) {
          // We have a void item on top of the exp_stack, so we must "lift" the
          // previous values around it.
          // We track what part of the stack is in variables to avoid doing
          // this unnecessarily.
          if (num_vals > 0) {
            // We have actual new values that need lifting.
            // This puts the part of the stack that wasn't already a variable
            // (before the current void exp) into a FlushToVars.
            auto void_exp = std::move(exp_stack.back());
            exp_stack.pop_back();
            GenFlushVars(num_vals);
            exp_stack.push_back(std::move(void_exp));
            // Put this flush statement + void statement before any
            // existing variables.
            MoveStatementsBelowVars(2);
            // Now re-generate these values after the void exp as vars.
            GenFlushedVars();
          } else {
            // We have existing variables that need lifting, but no need to
            // create them anew.
            std::rotate(exp_stack.end() - num_vars - 1, exp_stack.end() - 1,
                        exp_stack.end());
          }
          value_stack_in_variables = value_stack_depth;
        } else if (arity.nreturns > 1) {
          // Multivalue: we flush the stack also.
          // Number of other non-variable values that may be present:
          assert(num_vals >= static_cast<int>(arity.nreturns));
          // Flush multi-value exp + others.
          GenFlushVars(num_vals - arity.nreturns + 1);
          // Put this flush statement before any existing variables.
          MoveStatementsBelowVars(1);
          GenFlushedVars();
          value_stack_in_variables = value_stack_depth;
        }
      } else {
        // Special optimisation: for constant instructions, we can mark these
        // as if they were variables, so they can be re-ordered for free with
        // the above code, without needing new variables!
        // TODO: this would be nice to also do for get_local and maybe others,
        // though that needs a way to ensure there's no set_local in between
        // when they get lifted, so complicates matters a bit.
        if (e.type() == ExprType::Const &&
            value_stack_in_variables == value_stack_depth - 1) {
          value_stack_in_variables++;
        }
      }
    }
    assert(unreachable ||
           value_stack_depth - value_stack_depth_start ==
           static_cast<int>(nresults));
    // Undo any changes to value_stack_depth, since parent takes care of arity
    // changes.
    value_stack_depth = value_stack_depth_start;
    auto end = exp_stack.size();
    assert(end >= start);
    if (is_function_body) {
      if (!exp_stack.empty()) {
        if (exp_stack.back().etype == ExprType::Return) {
          if (exp_stack.back().children.empty()) {
            // Return statement at the end of a void function.
            exp_stack.pop_back();
          }
        } else if (nresults) {
          // Combine nresults into a return statement, for when this is used as
          // a function body.
          // TODO: if this is some other kind of block and >1 value is being
          // returned, probably need some kind of syntax to make that clearer.
          InsertNode(NodeType::EndReturn, ExprType::Nop, nullptr, nresults);
        }
      }
      // TODO: these predecls are always at top level, but in the case of
      // use inside an exp, it be nice to do it in the current block. Can't
      // do that for predecls that are "out if scope" however.
      std::move(predecls.begin(), predecls.end(),
                std::back_inserter(exp_stack));
      std::rotate(exp_stack.begin(), exp_stack.end() - predecls.size(),
                  exp_stack.end());
      predecls.clear();
    }
    end = exp_stack.size();
    assert(end >= start);
    auto size = end - start;
    if (size != 1) {
      InsertNode(NodeType::Statements, ExprType::Nop, nullptr, size);
    }
    blocks_closed[cur_block_id] = true;
    cur_block_id = block_stack.back();
    block_stack.pop_back();
  }

  ModuleContext& mc;
  std::vector<Node> exp_stack;
  std::vector<Node> predecls;
  const Func *f;
  int value_stack_depth = 0;
  struct Variable { size_t block_id; bool defined; };
  std::map<std::string, Variable> vars_defined;
  Index flushed_vars = 0;
  size_t cur_block_id = 0;
  std::vector<size_t> block_stack;
  std::vector<bool> blocks_closed;
};

}  // namespace wabt

#endif  // WABT_DECOMPILER_AST_H_
