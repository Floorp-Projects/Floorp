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

#ifndef WABT_DECOMPILER_LS_H_
#define WABT_DECOMPILER_LS_H_

#include "wabt/decompiler-ast.h"
#include "wabt/string-util.h"

#include <map>

namespace wabt {

// Names starting with "u" are unsigned, the rest are "signed or doesn't matter"
inline const char* GetDecompTypeName(Type t) {
  switch (t) {
    case Type::I8: return "byte";
    case Type::I8U: return "ubyte";
    case Type::I16: return "short";
    case Type::I16U: return "ushort";
    case Type::I32: return "int";
    case Type::I32U: return "uint";
    case Type::I64: return "long";
    case Type::F32: return "float";
    case Type::F64: return "double";
    case Type::V128: return "simd";
    case Type::Func: return "func";
    case Type::FuncRef: return "funcref";
    case Type::ExternRef: return "externref";
    case Type::Void: return "void";
    default: return "ILLEGAL";
  }
}

inline Type GetMemoryType(Type operand_type, Opcode opc) {
  // TODO: something something SIMD.
  // TODO: this loses information of the type it is read into.
  // That may well not be the biggest deal since that is usually obvious
  // from context, if not, we should probably represent that as a cast around
  // the access, since it should not be part of the field type.
  if (operand_type == Type::I32 || operand_type == Type::I64) {
    auto name = std::string_view(opc.GetName());
    // FIXME: change into a new column in opcode.def instead?
    auto is_unsigned = name.substr(name.size() - 2) == "_u";
    switch (opc.GetMemorySize()) {
      case 1: return is_unsigned ? Type::I8U : Type::I8;
      case 2: return is_unsigned ? Type::I16U : Type::I16;
      case 4: return is_unsigned ? Type::I32U : Type::I32;
    }
  }
  return operand_type;
}

// Track all loads and stores inside a single function, to be able to detect
// struct layouts we can use to annotate variables with, to make code more
// readable.
struct LoadStoreTracking {
  struct LSAccess {
    Address byte_size = 0;
    Type type = Type::Any;
    Address align = 0;
    uint32_t idx = 0;
    bool is_uniform = true;
  };

  struct LSVar {
    std::map<uint64_t, LSAccess> accesses;
    bool struct_layout = true;
    Type same_type = Type::Any;
    Address same_align = kInvalidAddress;
    Opcode last_opc;
  };

  void Track(const Node& n) {
    for (auto& c : n.children) {
      Track(c);
    }
    switch (n.etype) {
      case ExprType::Load: {
        auto& le = *cast<LoadExpr>(n.e);
        LoadStore(le.offset, le.opcode, le.opcode.GetResultType(), le.align,
                  n.children[0]);
        break;
      }
      case ExprType::Store: {
        auto& se = *cast<StoreExpr>(n.e);
        LoadStore(se.offset, se.opcode, se.opcode.GetParamType2(), se.align,
                  n.children[0]);
        break;
      }
      default:
        break;
    }
  }

  const std::string AddrExpName(const Node& addr_exp) const {
    // TODO: expand this to more kinds of address expressions.
    switch (addr_exp.etype) {
      case ExprType::LocalGet:
        return cast<LocalGetExpr>(addr_exp.e)->var.name();
        break;
      case ExprType::LocalTee:
        return cast<LocalTeeExpr>(addr_exp.e)->var.name();
        break;
      default:
        return "";
    }
  }

  void LoadStore(uint64_t offset,
                 Opcode opc,
                 Type type,
                 Address align,
                 const Node& addr_exp) {
    auto byte_size = opc.GetMemorySize();
    type = GetMemoryType(type, opc);
    // We want to associate memory ops of a certain offset & size as being
    // relative to a uniquely identifiable pointer, such as a local.
    auto name = AddrExpName(addr_exp);
    if (name.empty()) {
      return;
    }
    auto& var = vars[name];
    auto& access = var.accesses[offset];
    // Check if previous access at this offset (if any) is of same size
    // and type (see Checklayouts below).
    if (access.byte_size && ((access.byte_size != byte_size) ||
                             (access.type != type) || (access.align != align)))
      access.is_uniform = false;
    // Also exclude weird alignment accesses from structs.
    if (!opc.IsNaturallyAligned(align))
      access.is_uniform = false;
    access.byte_size = byte_size;
    access.type = type;
    access.align = align;
    // Additionally, check if all accesses are to the same type, so
    // if layout check fails, we can at least declare it as pointer to
    // a type.
    if ((var.same_type == type || var.same_type == Type::Any) &&
        (var.same_align == align || var.same_align == kInvalidAddress)) {
      var.same_type = type;
      var.same_align = align;
      var.last_opc = opc;
    } else {
      var.same_type = Type::Void;
      var.same_align = kInvalidAddress;
    }
  }

  void CheckLayouts() {
    // Here we check if the set of accesses we have collected form a sequence
    // we could declare as a struct, meaning they are properly aligned,
    // contiguous, and have no overlaps between different types and sizes.
    // We do this because an int access of size 2 at offset 0 followed by
    // a float access of size 4 at offset 4 can compactly represented as a
    // struct { short, float }, whereas something that reads from overlapping
    // or discontinuous offsets would need a more complicated syntax that
    // involves explicit offsets.
    // We assume that the bulk of memory accesses are of this very regular kind,
    // so we choose not to even emit struct layouts for irregular ones,
    // given that they are rare and confusing, and thus do not benefit from
    // being represented as if they were structs.
    for (auto& var : vars) {
      if (var.second.accesses.size() == 1) {
        // If we have just one access, this is better represented as a pointer
        // than a struct.
        var.second.struct_layout = false;
        continue;
      }
      uint64_t cur_offset = 0;
      uint32_t idx = 0;
      for (auto& access : var.second.accesses) {
        access.second.idx = idx++;
        if (!access.second.is_uniform) {
          var.second.struct_layout = false;
          break;
        }
        // Align to next access: all elements are expected to be aligned to
        // a memory address thats a multiple of their own size.
        auto mask = static_cast<uint64_t>(access.second.byte_size - 1);
        cur_offset = (cur_offset + mask) & ~mask;
        if (cur_offset != access.first) {
          var.second.struct_layout = false;
          break;
        }
        cur_offset += access.second.byte_size;
      }
    }
  }

  std::string IdxToName(uint32_t idx) const {
    return IndexToAlphaName(idx);  // TODO: more descriptive names?
  }

  std::string GenAlign(Address align, Opcode opc) const {
    return opc.IsNaturallyAligned(align) ? "" : cat("@", std::to_string(align));
  }

  std::string GenTypeDecl(const std::string& name) const {
    auto it = vars.find(name);
    if (it == vars.end()) {
      return "";
    }
    if (it->second.struct_layout) {
      std::string s = "{ ";
      for (auto& access : it->second.accesses) {
        if (access.second.idx) {
          s += ", ";
        }
        s += IdxToName(access.second.idx);
        s += ':';
        s += GetDecompTypeName(access.second.type);
      }
      s += " }";
      return s;
    }
    // We don't have a struct layout, or the struct has just one field,
    // so maybe we can just declare it as a pointer to one type?
    if (it->second.same_type != Type::Void) {
      return cat(GetDecompTypeName(it->second.same_type), "_ptr",
                 GenAlign(it->second.same_align, it->second.last_opc));
    }
    return "";
  }

  std::string GenAccess(uint64_t offset, const Node& addr_exp) const {
    auto name = AddrExpName(addr_exp);
    if (name.empty()) {
      return "";
    }
    auto it = vars.find(name);
    if (it == vars.end()) {
      return "";
    }
    if (it->second.struct_layout) {
      auto ait = it->second.accesses.find(offset);
      assert(ait != it->second.accesses.end());
      return IdxToName(ait->second.idx);
    }
    // Not a struct, see if it is a typed pointer.
    if (it->second.same_type != Type::Void) {
      return "*";
    }
    return "";
  }

  void Clear() { vars.clear(); }

  std::map<std::string, LSVar> vars;
};

}  // namespace wabt

#endif  // WABT_DECOMPILER_LS_H_
