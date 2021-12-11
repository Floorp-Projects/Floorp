/*
 * Copyright 2019 WebAssembly Community Group participants
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

#include "src/decompiler.h"

#include "src/decompiler-ast.h"
#include "src/decompiler-ls.h"
#include "src/decompiler-naming.h"

#include "src/stream.h"

#define WABT_TRACING 0
#include "src/tracing.h"

#include <inttypes.h>

namespace wabt {

struct Decompiler {
  Decompiler(const Module& module, const DecompileOptions& options)
      : mc(module), options(options) {}

  // Sorted such that "greater precedence" is also the bigger enum val.
  enum Precedence {
    // Low precedence.
    None,      // precedence doesn't matter, since never nested.
    Assign,    // =
    OtherBin,  // min
    Bit,       // & |
    Equal,     // == != < > >= <=
    Shift,     // << >>
    Add,       // + -
    Multiply,  // * / %
    If,        // if{}
    Indexing,  // []
    Atomic,    // (), a, 1, a()
    // High precedence.
  };

  // Anything besides these will get parentheses if used with equal precedence,
  // for clarity.
  bool Associative(Precedence p) {
    return p == Precedence::Add || p == Precedence::Multiply;
  }

  struct Value {
    std::vector<std::string> v;
    // Lazily add bracketing only if the parent requires it.
    // This is the precedence level of this value, for example, if this
    // precedence is Add, and the parent is Multiply, bracketing is needed,
    // but not if it is the reverse.
    Precedence precedence;

    Value(std::vector<std::string>&& v, Precedence p)
      : v(v), precedence(p) {}

    size_t width() {
      size_t w = 0;
      for (auto &s : v) {
        w = std::max(w, s.size());
      }
      return w;
    }

    // This value should really never be copied, only moved.
    Value(Value&& rhs) = default;
    Value(const Value& rhs) = delete;
    Value& operator=(Value&& rhs) = default;
    Value& operator=(const Value& rhs) = delete;
  };

  std::string to_string(double d) {
    auto s = std::to_string(d);
    // Remove redundant trailing '0's that to_string produces.
    while (s.size() > 2 && s.back() == '0' && s[s.size() - 2] != '.')
      s.pop_back();
    return s;
  }

  std::string Indent(size_t amount) {
    return std::string(amount, ' ');
  }

  std::string OpcodeToToken(Opcode opcode) {
    std::string s = opcode.GetDecomp();
    std::replace(s.begin(), s.end(), '.', '_');
    return s;
  }

  void IndentValue(Value &val, size_t amount, string_view first_indent) {
    auto indent = Indent(amount);
    for (auto& stat : val.v) {
      auto is = (&stat != &val.v[0] || first_indent.empty())
                    ? string_view(indent)
                    : first_indent;
      stat.insert(0, is.data(), is.size());
    }
  }

  Value WrapChild(Value &child, string_view prefix, string_view postfix,
                  Precedence precedence) {
    auto width = prefix.size() + postfix.size() + child.width();
    auto &v = child.v;
    if (width < target_exp_width ||
        (prefix.size() <= indent_amount && postfix.size() <= indent_amount)) {
      if (v.size() == 1) {
        // Fits in a single line.
        v[0].insert(0, prefix.data(), prefix.size());
        v[0].append(postfix.data(), postfix.size());
      } else {
        // Multiline, but with prefix on same line.
        IndentValue(child, prefix.size(), prefix);
        v.back().append(postfix.data(), postfix.size());
      }
    } else {
      // Multiline with prefix on its own line.
      IndentValue(child, indent_amount, {});
      v.insert(v.begin(), std::string(prefix));
      v.back().append(postfix.data(), postfix.size());
    }
    child.precedence = precedence;
    return std::move(child);
  }

  void BracketIfNeeded(Value &val, Precedence parent_precedence) {
    if (parent_precedence < val.precedence ||
        (parent_precedence == val.precedence &&
         Associative(parent_precedence))) return;
    val = WrapChild(val, "(", ")", Precedence::Atomic);
  }

  Value WrapBinary(std::vector<Value>& args, string_view infix,
                   bool indent_right, Precedence precedence) {
    assert(args.size() == 2);
    auto& left = args[0];
    auto& right = args[1];
    BracketIfNeeded(left, precedence);
    BracketIfNeeded(right, precedence);
    auto width = infix.size() + left.width() + right.width() + 2;
    if (width < target_exp_width && left.v.size() == 1 && right.v.size() == 1) {
      return Value{{cat(left.v[0], " ", infix, " ", right.v[0])}, precedence};
    } else {
      Value bin { {}, precedence };
      std::move(left.v.begin(), left.v.end(), std::back_inserter(bin.v));
      bin.v.back().append(" ", 1);
      bin.v.back().append(infix.data(), infix.size());
      if (indent_right) IndentValue(right, indent_amount, {});
      std::move(right.v.begin(), right.v.end(), std::back_inserter(bin.v));
      return bin;
    }
  }

  Value WrapNAry(std::vector<Value>& args, string_view prefix,
                 string_view postfix, Precedence precedence) {
    size_t total_width = 0;
    size_t max_width = 0;
    bool multiline = false;
    for (auto& child : args) {
      auto w = child.width();
      max_width = std::max(max_width, w);
      total_width += w;
      multiline = multiline || child.v.size() > 1;
    }
    if (!multiline &&
        (total_width + prefix.size() + postfix.size() < target_exp_width ||
         args.empty())) {
      // Single line.
      auto s = std::string(prefix);
      for (auto& child : args) {
        if (&child != &args[0])
          s += ", ";
        s += child.v[0];
      }
      s += postfix;
      return Value{{std::move(s)}, precedence};
    } else {
      // Multi-line.
      Value ml { {}, precedence };
      auto ident_with_name = max_width + prefix.size() < target_exp_width;
      size_t i = 0;
      for (auto& child : args) {
        IndentValue(child, ident_with_name ? prefix.size() : indent_amount,
                    !i && ident_with_name ? prefix : string_view {});
        if (i < args.size() - 1) child.v.back() += ",";
        std::move(child.v.begin(), child.v.end(),
                  std::back_inserter(ml.v));
        i++;
      }
      if (!ident_with_name) ml.v.insert(ml.v.begin(), std::string(prefix));
      ml.v.back() += postfix;
      return ml;
    }
  }

  string_view VarName(string_view name) {
    assert(!name.empty());
    return name[0] == '$' ? name.substr(1) : name;
  }

  template<ExprType T> Value Get(const VarExpr<T>& ve) {
    return Value{{std::string(VarName(ve.var.name()))}, Precedence::Atomic};
  }

  template<ExprType T> Value Set(Value& child, const VarExpr<T>& ve) {
    return WrapChild(child, VarName(ve.var.name()) + " = ", "", Precedence::Assign);
  }

  std::string TempVarName(Index n) {
    // FIXME: this needs much better variable naming. Problem is, the code
    // in generate-names.cc has allready run, its dictionaries deleted, so it
    // is not easy to integrate with it.
    return "t" + std::to_string(n);
  }

  std::string LocalDecl(const std::string& name, Type t) {
    auto struc = lst.GenTypeDecl(name);
    return cat(VarName(name), ":",
               struc.empty() ? GetDecompTypeName(t) : struc);
  }

  bool ConstIntVal(const Expr* e, uint64_t &dest) {
    dest = 0;
    if (!e || e->type() != ExprType::Const) return false;
    auto& c = cast<ConstExpr>(e)->const_;
    if (c.type() != Type::I32 && c.type() != Type::I64) return false;
    dest = c.type() == Type::I32 ? c.u32() : c.u64();
    return true;
  }

  void LoadStore(Value &val, const Node& addr_exp, uint64_t offset,
                  Opcode opc, Address align, Type op_type) {
    bool append_type = true;
    auto access = lst.GenAccess(offset, addr_exp);
    if (!access.empty()) {
      if (access == "*") {
        // The variable was declared as a typed pointer, so this access
        // doesn't need a type.
        append_type = false;
      } else {
        // We can do this load/store as a struct access.
        BracketIfNeeded(val, Precedence::Indexing);
        val.v.back() += "." + access;
        return;
      }
    }
    // Detect absolute addressing, which we try to turn into references to the
    // data section when possible.
    uint64_t abs_base;
    if (ConstIntVal(addr_exp.e, abs_base)) {
      // We don't care what part of the absolute address was stored where,
      // 1[0] and 0[1] are the same.
      abs_base += offset;
      // FIXME: make this less expensive with a binary search or whatever.
      for (auto dat : mc.module.data_segments) {
        uint64_t dat_base;
        if (dat->offset.size() == 1 &&
            ConstIntVal(&dat->offset.front(), dat_base) &&
            abs_base >= dat_base &&
            abs_base < dat_base + dat->data.size()) {
          // We are inside the range of this data segment!
          // Turn expression into data_name[index]
          val = Value { { dat->name }, Precedence::Atomic };
          // The new offset is from the start of the data segment, instead of
          // whatever it was.. this may be a different value from both the
          // original const and offset!
          offset = abs_base - dat_base;
        }
      }
    }
    // Do the load/store as a generalized indexing operation.
    // The offset is divisible by the alignment in 99.99% of
    // cases, but the spec doesn't guarantee it, so we must
    // have a backup syntax.
    auto index = offset % align == 0
                ? std::to_string(offset / align)
                : cat(std::to_string(offset), "@", std::to_string(align));
    // Detect the very common case of (base + (index << 2))[0]:int etc.
    // so we can instead do base[index]:int
    // TODO: (index << 2) on the left of + occurs also.
    // TODO: sadly this does not address cases where the shift amount > align.
    // (which happens for arrays of structs or arrays of long (with align=4)).
    // TODO: also very common is (v = base + (index << 2))[0]:int
    if (addr_exp.etype == ExprType::Binary) {
      auto& pe = *cast<BinaryExpr>(addr_exp.e);
      auto& shift_exp = addr_exp.children[1];
      if (pe.opcode == Opcode::I32Add &&
          shift_exp.etype == ExprType::Binary) {
        auto& se = *cast<BinaryExpr>(shift_exp.e);
        auto& const_exp = shift_exp.children[1];
        if (se.opcode == Opcode::I32Shl &&
            const_exp.etype == ExprType::Const) {
          auto& ce = *cast<ConstExpr>(const_exp.e);
          if (ce.const_.type() == Type::I32 &&
              (1ULL << ce.const_.u32()) == align) {
            // Pfew, case detected :( Lets re-write this in Haskell.
            // TODO: we're decompiling these twice.
            // The thing to the left of << is going to be part of the index.
            auto ival = DecompileExpr(shift_exp.children[0], &shift_exp);
            if (ival.v.size() == 1) {  // Don't bother if huge.
              if (offset == 0) {
                index = ival.v[0];
              } else {
                BracketIfNeeded(ival, Precedence::Add);
                index = cat(ival.v[0], " + ", index);
              }
              // We're going to use the thing to the left of + as the new
              // base address:
              val = DecompileExpr(addr_exp.children[0], &addr_exp);
            }
          }
        }
      }
    }
    BracketIfNeeded(val, Precedence::Indexing);
    val.v.back() += cat("[", index, "]");
    if (append_type) {
      val.v.back() +=
        cat(":", GetDecompTypeName(GetMemoryType(op_type, opc)),
            lst.GenAlign(align, opc));
    }
    val.precedence = Precedence::Indexing;
  }

  Value DecompileExpr(const Node& n, const Node* parent) {
    std::vector<Value> args;
    for (auto& c : n.children) {
      args.push_back(DecompileExpr(c, &n));
    }
    // First deal with the specialized node types.
    switch (n.ntype) {
      case NodeType::FlushToVars: {
        std::string decls = "let ";
        for (Index i = 0; i < n.u.var_count; i++) {
          if (i) decls += ", ";
          decls += TempVarName(n.u.var_start + i);
        }
        decls += " = ";
        return WrapNAry(args, decls, "", Precedence::Assign);
      }
      case NodeType::FlushedVar: {
        return Value { { TempVarName(n.u.var_start) }, Precedence::Atomic };
      }
      case NodeType::Statements: {
        Value stats { {}, Precedence::None };
        for (size_t i = 0; i < n.children.size(); i++) {
          auto& s = args[i].v.back();
          if (s.back() != '}' && s.back() != ':') s += ';';
          std::move(args[i].v.begin(), args[i].v.end(),
                    std::back_inserter(stats.v));
        }
        return stats;
      }
      case NodeType::EndReturn: {
        return WrapNAry(args, "return ", "", Precedence::None);
      }
      case NodeType::Decl: {
        cur_ast->vars_defined[n.u.var->name()].defined = true;
        return Value{
            {"var " + LocalDecl(std::string(n.u.var->name()),
                                cur_func->GetLocalType(*n.u.var))},
            Precedence::None};
      }
      case NodeType::DeclInit: {
        if (cur_ast->vars_defined[n.u.var->name()].defined) {
          // This has already been pre-declared, output as assign.
          return WrapChild(args[0], cat(VarName(n.u.var->name()), " = "), "",
                           Precedence::None);
        } else {
          return WrapChild(
              args[0],
              cat("var ",
                  LocalDecl(std::string(n.u.var->name()),
                            cur_func->GetLocalType(*n.u.var)),
                  " = "),
              "", Precedence::None);
        }
      }
      case NodeType::Expr:
        // We're going to fall thru to the second switch to deal with ExprType.
        break;
      case NodeType::Uninitialized:
        assert(false);
        break;
    }
    // Existing ExprTypes.
    switch (n.etype) {
      case ExprType::Const: {
        auto& c = cast<ConstExpr>(n.e)->const_;
        switch (c.type()) {
          case Type::I32:
            return Value{{std::to_string(static_cast<int32_t>(c.u32()))},
                         Precedence::Atomic};
          case Type::I64:
            return Value{{std::to_string(static_cast<int64_t>(c.u64())) + "L"},
                         Precedence::Atomic};
          case Type::F32: {
            float f = Bitcast<float>(c.f32_bits());
            return Value{{to_string(f) + "f"}, Precedence::Atomic};
          }
          case Type::F64: {
            double d = Bitcast<double>(c.f64_bits());
            return Value{{to_string(d)}, Precedence::Atomic};
          }
          case Type::V128:
            return Value{{"V128"}, Precedence::Atomic};  // FIXME
          default:
            WABT_UNREACHABLE;
        }
      }
      case ExprType::LocalGet: {
        return Get(*cast<LocalGetExpr>(n.e));
      }
      case ExprType::GlobalGet: {
        return Get(*cast<GlobalGetExpr>(n.e));
      }
      case ExprType::LocalSet: {
        return Set(args[0], *cast<LocalSetExpr>(n.e));
      }
      case ExprType::GlobalSet: {
        return Set(args[0], *cast<GlobalSetExpr>(n.e));
      }
      case ExprType::LocalTee: {
        auto& te = *cast<LocalTeeExpr>(n.e);
        return args.empty() ? Get(te) : Set(args[0], te);
      }
      case ExprType::Binary: {
        auto& be = *cast<BinaryExpr>(n.e);
        auto opcs = OpcodeToToken(be.opcode);
        // TODO: Is this selection better done on Opcode values directly?
        // What if new values get added and OtherBin doesn't make sense?
        auto prec = Precedence::OtherBin;
        if (opcs == "*" || opcs == "/" || opcs == "%") {
          prec = Precedence::Multiply;
        } else if (opcs == "+" || opcs == "-") {
          prec = Precedence::Add;
        } else if (opcs == "&" || opcs == "|" || opcs == "^") {
          prec = Precedence::Bit;
        } else if (opcs == "<<" || opcs == ">>") {
          prec = Precedence::Shift;
        }
        return WrapBinary(args, opcs, false, prec);
      }
      case ExprType::Compare: {
        auto& ce = *cast<CompareExpr>(n.e);
        return WrapBinary(args, OpcodeToToken(ce.opcode), false,
                          Precedence::Equal);
      }
      case ExprType::Unary: {
        auto& ue = *cast<UnaryExpr>(n.e);
        //BracketIfNeeded(stack.back());
        // TODO: also version without () depending on precedence.
        return WrapChild(args[0], OpcodeToToken(ue.opcode) + "(", ")",
                         Precedence::Atomic);
      }
      case ExprType::Load: {
        auto& le = *cast<LoadExpr>(n.e);
        LoadStore(args[0], n.children[0], le.offset, le.opcode, le.align,
                  le.opcode.GetResultType());
        return std::move(args[0]);
      }
      case ExprType::Store: {
        auto& se = *cast<StoreExpr>(n.e);
        LoadStore(args[0], n.children[0], se.offset, se.opcode, se.align,
                  se.opcode.GetParamType2());
        return WrapBinary(args, "=", true, Precedence::Assign);
      }
      case ExprType::If: {
        auto ife = cast<IfExpr>(n.e);
        Value *elsep = nullptr;
        if (!ife->false_.empty()) {
          elsep = &args[2];
        }
        auto& thenp = args[1];
        auto& ifs = args[0];
        bool multiline = ifs.v.size() > 1 || thenp.v.size() > 1;
        size_t width = ifs.width() + thenp.width();
        if (elsep) {
          width += elsep->width();
          multiline = multiline || elsep->v.size() > 1;
        }
        multiline = multiline || width > target_exp_width;
        if (multiline) {
          auto if_start = string_view("if (");
          IndentValue(ifs, if_start.size(), if_start);
          ifs.v.back() += ") {";
          IndentValue(thenp, indent_amount, {});
          std::move(thenp.v.begin(), thenp.v.end(), std::back_inserter(ifs.v));
          if (elsep) {
            ifs.v.push_back("} else {");
            IndentValue(*elsep, indent_amount, {});
            std::move(elsep->v.begin(), elsep->v.end(), std::back_inserter(ifs.v));
          }
          ifs.v.push_back("}");
          ifs.precedence = Precedence::If;
          return std::move(ifs);
        } else {
          auto s = cat("if (", ifs.v[0], ") { ", thenp.v[0], " }");
          if (elsep)
            s += cat(" else { ", elsep->v[0], " }");
          return Value{{std::move(s)}, Precedence::If};
        }
      }
      case ExprType::Block: {
        auto& val = args[0];
        val.v.push_back(
              cat("label ", VarName(cast<BlockExpr>(n.e)->block.label), ":"));
        // If this block is part of a larger statement scope, it doesn't
        // need its own indenting, but if its part of an exp we wrap it in {}.
        if (parent && parent->ntype != NodeType::Statements
                   && parent->etype != ExprType::Block
                   && parent->etype != ExprType::Loop
                   && (parent->etype != ExprType::If ||
                       &parent->children[0] == &n)) {
          IndentValue(val, indent_amount, {});
          val.v.insert(val.v.begin(), "{");
          val.v.push_back("}");
        }
        val.precedence = Precedence::Atomic;
        return std::move(val);
      }
      case ExprType::Loop: {
        auto& val = args[0];
        auto& block = cast<LoopExpr>(n.e)->block;
        IndentValue(val, indent_amount, {});
        val.v.insert(val.v.begin(), cat("loop ", VarName(block.label), " {"));
        val.v.push_back("}");
        val.precedence = Precedence::Atomic;
        return std::move(val);
      }
      case ExprType::Br: {
        auto be = cast<BrExpr>(n.e);
        return Value{{(n.u.lt == LabelType::Loop ? "continue " : "goto ") +
                      VarName(be->var.name())},
                     Precedence::None};
      }
      case ExprType::BrIf: {
        auto bie = cast<BrIfExpr>(n.e);
        auto jmp = n.u.lt == LabelType::Loop ? "continue" : "goto";
        return WrapChild(args[0], "if (", cat(") ", jmp, " ",
                                              VarName(bie->var.name())),
                         Precedence::None);
      }
      case ExprType::Return: {
        return WrapNAry(args, "return ", "", Precedence::None);
      }
      case ExprType::Rethrow: {
        return WrapNAry(args, "rethrow ", "", Precedence::None);
      }
      case ExprType::Drop: {
        // Silent dropping of return values is very common, so currently
        // don't output this.
        return std::move(args[0]);
      }
      case ExprType::Nop: {
        return Value{{"nop"}, Precedence::None};
      }
      case ExprType::Unreachable: {
        return Value{{"unreachable"}, Precedence::None};
      }
      case ExprType::RefNull: {
        return Value{{"null"}, Precedence::Atomic};
      }
      case ExprType::BrTable: {
        auto bte = cast<BrTableExpr>(n.e);
        std::string ts = "br_table[";
        for (auto &v : bte->targets) {
          ts += VarName(v.name());
          ts += ", ";
        }
        ts += "..";
        ts += VarName(bte->default_target.name());
        ts += "](";
        return WrapChild(args[0], ts, ")", Precedence::Atomic);
      }
      default: {
        // Everything that looks like a function call.
        std::string name;
        auto precedence = Precedence::Atomic;
        switch (n.etype) {
          case ExprType::Call:
            name = cast<CallExpr>(n.e)->var.name();
            break;
          case ExprType::ReturnCall:
            name = "return_call " + cast<ReturnCallExpr>(n.e)->var.name();
            precedence = Precedence::None;
            break;
          case ExprType::Convert:
            name = std::string(OpcodeToToken(cast<ConvertExpr>(n.e)->opcode));
            break;
          case ExprType::Ternary:
            name = std::string(OpcodeToToken(cast<TernaryExpr>(n.e)->opcode));
            break;
          case ExprType::Select:
            // This one looks like it could be translated to "?:" style ternary,
            // but the arguments are NOT lazy, and side effects definitely do
            // occur in the branches. So it has no clear equivalent in C-syntax.
            // To emphasize that all args are being evaluated in order, we
            // leave it as a function call.
            name = "select_if";
            break;
          case ExprType::MemoryGrow:
            name = "memory_grow";
            break;
          case ExprType::MemorySize:
            name = "memory_size";
            break;
          case ExprType::MemoryCopy:
            name = "memory_copy";
            break;
          case ExprType::MemoryFill:
            name = "memory_fill";
            break;
          case ExprType::RefIsNull:
            name = "is_null";
            break;
          case ExprType::CallIndirect:
            name = "call_indirect";
            break;
          case ExprType::ReturnCallIndirect:
            name = "return_call call_indirect";
            break;
          default:
            name = GetExprTypeName(n.etype);
            break;
        }
        return WrapNAry(args, name + "(", ")", precedence);
      }
    }
  }

  bool CheckImportExport(std::string& s,
                         ExternalKind kind,
                         Index index,
                         string_view name) {
    // Figure out if this thing is imported, exported, or neither.
    auto is_import = mc.module.IsImport(kind, Var(index));
    // TODO: is this the best way to check for export?
    // FIXME: this doesn't work for functions that get renamed in some way,
    // as the export has the original name..
    auto xport = mc.module.GetExport(name);
    auto is_export = xport && xport->kind == kind;
    if (is_export)
      s += "export ";
    if (is_import)
      s += "import ";
    return is_import;
  }

  std::string InitExp(const ExprList &el) {
    assert(!el.empty());
    AST ast(mc, nullptr);
    ast.Construct(el, 1, 0, false);
    auto val = DecompileExpr(ast.exp_stack[0], nullptr);
    assert(ast.exp_stack.size() == 1 && val.v.size() == 1);
    return std::move(val.v[0]);
  }

  // FIXME: Merge with WatWriter::WriteQuotedData somehow.
  std::string BinaryToString(const std::vector<uint8_t> &in) {
    std::string s = "\"";
    size_t line_start = 0;
    static const char s_hexdigits[] = "0123456789abcdef";
    for (auto c : in) {
      if (c >= ' ' && c <= '~') {
        s += c;
      } else {
        s += '\\';
        s += s_hexdigits[c >> 4];
        s += s_hexdigits[c & 0xf];
      }
      if (s.size() - line_start > target_exp_width) {
        if (line_start == 0) {
          s = "  " + s;
        }
        s += "\"\n  ";
        line_start = s.size();
        s += "\"";
      }
    }
    s += '\"';
    return s;
  }

  std::string Decompile() {
    std::string s;
    // Memories.
    Index memory_index = 0;
    for (auto m : mc.module.memories) {
      auto is_import =
          CheckImportExport(s, ExternalKind::Memory, memory_index, m->name);
      s += cat("memory ", m->name);
      if (!is_import) {
        s += cat("(initial: ", std::to_string(m->page_limits.initial),
                 ", max: ", std::to_string(m->page_limits.max), ")");
      }
      s += ";\n";
      memory_index++;
    }
    if (!mc.module.memories.empty())
      s += "\n";

    // Globals.
    Index global_index = 0;
    for (auto g : mc.module.globals) {
      auto is_import =
          CheckImportExport(s, ExternalKind::Global, global_index, g->name);
      s += cat("global ", g->name, ":", GetDecompTypeName(g->type));
      if (!is_import) {
        s += cat(" = ", InitExp(g->init_expr));
      }
      s += ";\n";
      global_index++;
    }
    if (!mc.module.globals.empty())
      s += "\n";

    // Tables.
    Index table_index = 0;
    for (auto tab : mc.module.tables) {
      auto is_import =
          CheckImportExport(s, ExternalKind::Table, table_index, tab->name);
      s += cat("table ", tab->name, ":", GetDecompTypeName(tab->elem_type));
      if (!is_import) {
        s += cat("(min: ", std::to_string(tab->elem_limits.initial),
                 ", max: ", std::to_string(tab->elem_limits.max), ")");
      }
      s += ";\n";
      table_index++;
    }
    if (!mc.module.tables.empty())
      s += "\n";

    // Data.
    for (auto dat : mc.module.data_segments) {
      s += cat("data ", dat->name, "(offset: ", InitExp(dat->offset), ") =");
      auto ds = BinaryToString(dat->data);
      if (ds.size() > target_exp_width / 2) {
        s += "\n";
      } else {
        s += " ";
      }
      s += ds;
      s += ";\n";
    }
    if (!mc.module.data_segments.empty())
      s += "\n";

    // Code.
    Index func_index = 0;
    for (auto f : mc.module.funcs) {
      cur_func = f;
      auto is_import =
          CheckImportExport(s, ExternalKind::Func, func_index, f->name);
      AST ast(mc, f);
      cur_ast = &ast;
      if (!is_import) {
        ast.Construct(f->exprs, f->GetNumResults(), 0, true);
        lst.Track(ast.exp_stack[0]);
        lst.CheckLayouts();
      }
      s += cat("function ", f->name, "(");
      for (Index i = 0; i < f->GetNumParams(); i++) {
        if (i)
          s += ", ";
        auto t = f->GetParamType(i);
        auto name = "$" + IndexToAlphaName(i);
        s += LocalDecl(name, t);
      }
      s += ")";
      if (f->GetNumResults()) {
        if (f->GetNumResults() == 1) {
          s += cat(":", GetDecompTypeName(f->GetResultType(0)));
        } else {
          s += ":(";
          for (Index i = 0; i < f->GetNumResults(); i++) {
            if (i)
              s += ", ";
            s += GetDecompTypeName(f->GetResultType(i));
          }
          s += ")";
        }
      }
      if (is_import) {
        s += ";";
      } else {
        s += " {\n";
        auto val = DecompileExpr(ast.exp_stack[0], nullptr);
        IndentValue(val, indent_amount, {});
        for (auto& stat : val.v) {
          s += stat;
          s += "\n";
        }
        s += "}";
      }
      s += "\n\n";
      mc.EndFunc();
      lst.Clear();
      func_index++;
      cur_ast = nullptr;
      cur_func = nullptr;
    }
    return s;
  }

  ModuleContext mc;
  const DecompileOptions& options;
  size_t indent_amount = 2;
  size_t target_exp_width = 70;
  const Func* cur_func = nullptr;
  AST* cur_ast = nullptr;
  LoadStoreTracking lst;
};

std::string Decompile(const Module& module, const DecompileOptions& options) {
  Decompiler decompiler(module, options);
  return decompiler.Decompile();
}

}  // namespace wabt
