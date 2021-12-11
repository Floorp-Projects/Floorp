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

#include "src/binary-writer.h"

#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

#include "config.h"

#include "src/binary.h"
#include "src/cast.h"
#include "src/ir.h"
#include "src/leb128.h"
#include "src/stream.h"
#include "src/string-view.h"

#define PRINT_HEADER_NO_INDEX -1
#define MAX_U32_LEB128_BYTES 5

namespace wabt {

void WriteStr(Stream* stream,
              string_view s,
              const char* desc,
              PrintChars print_chars) {
  WriteU32Leb128(stream, s.length(), "string length");
  stream->WriteData(s.data(), s.length(), desc, print_chars);
}

void WriteOpcode(Stream* stream, Opcode opcode) {
  if (opcode.HasPrefix()) {
    stream->WriteU8(opcode.GetPrefix(), "prefix");
    WriteU32Leb128(stream, opcode.GetCode(), opcode.GetName());
  } else {
    stream->WriteU8(opcode.GetCode(), opcode.GetName());
  }
}

void WriteType(Stream* stream, Type type, const char* desc) {
  WriteS32Leb128(stream, type, desc ? desc : type.GetName());
}

void WriteLimits(Stream* stream, const Limits* limits) {
  uint32_t flags = limits->has_max ? WABT_BINARY_LIMITS_HAS_MAX_FLAG : 0;
  flags |= limits->is_shared ? WABT_BINARY_LIMITS_IS_SHARED_FLAG : 0;
  flags |= limits->is_64 ? WABT_BINARY_LIMITS_IS_64_FLAG : 0;
  WriteU32Leb128(stream, flags, "limits: flags");
  if (limits->is_64) {
    WriteU64Leb128(stream, limits->initial, "limits: initial");
    if (limits->has_max) {
      WriteU64Leb128(stream, limits->max, "limits: max");
    }
  } else {
    WriteU32Leb128(stream, limits->initial, "limits: initial");
    if (limits->has_max) {
      WriteU32Leb128(stream, limits->max, "limits: max");
    }  
  }
}

void WriteDebugName(Stream* stream, string_view name, const char* desc) {
  string_view stripped_name = name;
  if (!stripped_name.empty()) {
    // Strip leading $ from name
    assert(stripped_name.front() == '$');
    stripped_name.remove_prefix(1);
  }
  WriteStr(stream, stripped_name, desc, PrintChars::Yes);
}

namespace {

/* TODO(binji): better leb size guess. Some sections we know will only be 1
 byte, but others we can be fairly certain will be larger. */
static const size_t LEB_SECTION_SIZE_GUESS = 1;

#define ALLOC_FAILURE \
  fprintf(stderr, "%s:%d: allocation failed\n", __FILE__, __LINE__)

struct RelocSection {
  RelocSection(const char* name, Index index)
      : name(name), section_index(index) {}

  const char* name;
  Index section_index;
  std::vector<Reloc> relocations;
};

class Symbol {
 public:
  struct Function {
    static const SymbolType type = SymbolType::Function;
    Index index;
  };
  struct Data {
    static const SymbolType type = SymbolType::Data;
    Index index;
    Offset offset;
    Address size;
  };
  struct Global {
    static const SymbolType type = SymbolType::Global;
    Index index;
  };
  struct Section {
    static const SymbolType type = SymbolType::Section;
    Index section;
  };
  struct Tag {
    static const SymbolType type = SymbolType::Tag;
    Index index;
  };
  struct Table {
    static const SymbolType type = SymbolType::Table;
    Index index;
  };

 private:
  SymbolType type_;
  string_view name_;
  uint8_t flags_;
  union {
    Function function_;
    Data data_;
    Global global_;
    Section section_;
    Tag tag_;
    Table table_;
  };

 public:
  Symbol(const string_view& name, uint8_t flags, const Function& f)
      : type_(Function::type), name_(name), flags_(flags), function_(f) {}
  Symbol(const string_view& name, uint8_t flags, const Data& d)
      : type_(Data::type), name_(name), flags_(flags), data_(d) {}
  Symbol(const string_view& name, uint8_t flags, const Global& g)
      : type_(Global::type), name_(name), flags_(flags), global_(g) {}
  Symbol(const string_view& name, uint8_t flags, const Section& s)
      : type_(Section::type), name_(name), flags_(flags), section_(s) {}
  Symbol(const string_view& name, uint8_t flags, const Tag& e)
      : type_(Tag::type), name_(name), flags_(flags), tag_(e) {}
  Symbol(const string_view& name, uint8_t flags, const Table& t)
      : type_(Table::type), name_(name), flags_(flags), table_(t) {}

  SymbolType type() const { return type_; }
  const string_view& name() const { return name_; }
  uint8_t flags() const { return flags_; }

  SymbolVisibility visibility() const {
    return static_cast<SymbolVisibility>(flags() & WABT_SYMBOL_MASK_VISIBILITY);
  }
  SymbolBinding binding() const {
    return static_cast<SymbolBinding>(flags() & WABT_SYMBOL_MASK_BINDING);
  }
  bool undefined() const { return flags() & WABT_SYMBOL_FLAG_UNDEFINED; }
  bool defined() const { return !undefined(); }
  bool exported() const { return flags() & WABT_SYMBOL_FLAG_EXPORTED; }
  bool explicit_name() const { return flags() & WABT_SYMBOL_FLAG_EXPLICIT_NAME; }
  bool no_strip() const { return flags() & WABT_SYMBOL_FLAG_NO_STRIP; }

  bool IsFunction() const { return type() == Function::type; }
  bool IsData() const { return type() == Data::type; }
  bool IsGlobal() const { return type() == Global::type; }
  bool IsSection() const { return type() == Section::type; }
  bool IsTag() const { return type() == Tag::type; }
  bool IsTable() const { return type() == Table::type; }

  const Function& AsFunction() const {
    assert(IsFunction());
    return function_;
  }
  const Data& AsData() const {
    assert(IsData());
    return data_;
  }
  const Global& AsGlobal() const {
    assert(IsGlobal());
    return global_;
  }
  const Section& AsSection() const {
    assert(IsSection());
    return section_;
  }
  const Tag& AsTag() const {
    assert(IsTag());
    return tag_;
  }
  const Table& AsTable() const {
    assert(IsTable());
    return table_;
  }
};

class SymbolTable {
  WABT_DISALLOW_COPY_AND_ASSIGN(SymbolTable);

  std::vector<Symbol> symbols_;

  std::vector<Index> functions_;
  std::vector<Index> tables_;
  std::vector<Index> globals_;

  std::set<string_view> seen_names_;

  Result EnsureUnique(const string_view& name) {
    if (seen_names_.count(name)) {
      fprintf(stderr, "error: duplicate symbol when writing relocatable "
              "binary: %s\n", &name[0]);
      return Result::Error;
    }
    seen_names_.insert(name);
    return Result::Ok;
  };

  template <typename T>
  Result AddSymbol(std::vector<Index>* map, string_view name,
                   bool imported, bool exported, T&& sym) {
    uint8_t flags = 0;
    if (imported) {
      flags |= WABT_SYMBOL_FLAG_UNDEFINED;
      // Wabt currently has no way for a user to explicitly specify the name of
      // an import, so never set the EXPLICIT_NAME flag, and ignore any display
      // name fabricated by wabt.
      name = string_view();
    } else {
      if (name.empty()) {
        // Definitions without a name are local.
        flags |= uint8_t(SymbolBinding::Local);
        flags |= uint8_t(SymbolVisibility::Hidden);
      } else {
        // Otherwise, strip the dollar off the name; a definition $foo is
        // available for linking as "foo".
        assert(name[0] == '$');
        name.remove_prefix(1);
      }

      if (exported) {
        CHECK_RESULT(EnsureUnique(name));
        flags |= uint8_t(SymbolVisibility::Hidden);
        flags |= WABT_SYMBOL_FLAG_NO_STRIP;
      }
    }
    if (exported) {
      flags |= WABT_SYMBOL_FLAG_EXPORTED;
    }

    map->push_back(symbols_.size());
    symbols_.emplace_back(name, flags, sym);
    return Result::Ok;
  };

  Index SymbolIndex(const std::vector<Index>& table, Index index) const {
    // For well-formed modules, an index into (e.g.) functions_ will always be
    // within bounds; the out-of-bounds case here is just to allow --relocatable
    // to write known-invalid modules.
    return index < table.size() ? table[index] : kInvalidIndex;
  }

 public:
  SymbolTable() {}

  Result Populate(const Module* module) {
    std::set<Index> exported_funcs;
    std::set<Index> exported_globals;
    std::set<Index> exported_tags;
    std::set<Index> exported_tables;

    for (const Export* export_ : module->exports) {
      switch (export_->kind) {
      case ExternalKind::Func:
        exported_funcs.insert(module->GetFuncIndex(export_->var));
        break;
      case ExternalKind::Table:
        exported_tables.insert(module->GetTableIndex(export_->var));
        break;
      case ExternalKind::Memory:
        break;
      case ExternalKind::Global:
        exported_globals.insert(module->GetGlobalIndex(export_->var));
        break;
      case ExternalKind::Tag:
        exported_tags.insert(module->GetTagIndex(export_->var));
        break;
      }
    }

    // We currently only create symbol table entries for function, table, and
    // global symbols.
    for (size_t i = 0; i < module->funcs.size(); ++i) {
      const Func* func = module->funcs[i];
      bool imported = i < module->num_func_imports;
      bool exported = exported_funcs.count(i);
      CHECK_RESULT(AddSymbol(&functions_, func->name, imported, exported,
                             Symbol::Function{Index(i)}));
    }

    for (size_t i = 0; i < module->tables.size(); ++i) {
      const Table* table = module->tables[i];
      bool imported = i < module->num_table_imports;
      bool exported = exported_tables.count(i);
      CHECK_RESULT(AddSymbol(&tables_, table->name, imported, exported,
                             Symbol::Table{Index(i)}));
    }

    for (size_t i = 0; i < module->globals.size(); ++i) {
      const Global* global = module->globals[i];
      bool imported = i < module->num_global_imports;
      bool exported = exported_globals.count(i);
      CHECK_RESULT(AddSymbol(&globals_, global->name, imported, exported,
                             Symbol::Global{Index(i)}));
    }

    return Result::Ok;
  }

  const std::vector<Symbol>& symbols() const { return symbols_; }
  Index FunctionSymbolIndex(Index index) const {
    return SymbolIndex(functions_, index);
  }
  Index TableSymbolIndex(Index index) const {
    return SymbolIndex(tables_, index);
  }
  Index GlobalSymbolIndex(Index index) const {
    return SymbolIndex(globals_, index);
  }
};

class BinaryWriter {
  WABT_DISALLOW_COPY_AND_ASSIGN(BinaryWriter);

 public:
  BinaryWriter(Stream*,
               const WriteBinaryOptions& options,
               const Module* module);

  Result WriteModule();

 private:
  void WriteHeader(const char* name, int index);
  Offset WriteU32Leb128Space(Offset leb_size_guess, const char* desc);
  Offset WriteFixupU32Leb128Size(Offset offset,
                                 Offset leb_size_guess,
                                 const char* desc);
  void BeginKnownSection(BinarySection section_code);
  void BeginCustomSection(const char* name);
  void WriteSectionHeader(const char* desc, BinarySection section_code);
  void EndSection();
  void BeginSubsection(const char* name);
  void EndSubsection();
  Index GetLabelVarDepth(const Var* var);
  Index GetTagVarDepth(const Var* var);
  Index GetLocalIndex(const Func* func, const Var& var);
  Index GetSymbolIndex(RelocType reloc_type, Index index);
  void AddReloc(RelocType reloc_type, Index index);
  void WriteBlockDecl(const BlockDeclaration& decl);
  void WriteU32Leb128WithReloc(Index index,
                               const char* desc,
                               RelocType reloc_type);
  void WriteS32Leb128WithReloc(int32_t value,
                               const char* desc,
                               RelocType reloc_type);
  void WriteTableNumberWithReloc(Index table_number, const char* desc);
  template <typename T>
  void WriteLoadStoreExpr(const Func* func, const Expr* expr, const char* desc);
  template <typename T>
  void WriteSimdLoadStoreLaneExpr(const Func* func, const Expr* expr, const char* desc);
  void WriteExpr(const Func* func, const Expr* expr);
  void WriteExprList(const Func* func, const ExprList& exprs);
  void WriteInitExpr(const ExprList& expr);
  void WriteFuncLocals(const Func* func, const LocalTypes& local_types);
  void WriteFunc(const Func* func);
  void WriteTable(const Table* table);
  void WriteMemory(const Memory* memory);
  void WriteGlobalHeader(const Global* global);
  void WriteTagType(const Tag* tag);
  void WriteRelocSection(const RelocSection* reloc_section);
  void WriteLinkingSection();
  template <typename T>
  void WriteNames(const std::vector<T*>& elems, NameSectionSubsection type);

  Stream* stream_;
  const WriteBinaryOptions& options_;
  const Module* module_;

  SymbolTable symtab_;
  std::vector<RelocSection> reloc_sections_;
  RelocSection* current_reloc_section_ = nullptr;

  Index section_count_ = 0;
  size_t last_section_offset_ = 0;
  size_t last_section_leb_size_guess_ = 0;
  BinarySection last_section_type_ = BinarySection::Invalid;
  size_t last_section_payload_offset_ = 0;

  size_t last_subsection_offset_ = 0;
  size_t last_subsection_leb_size_guess_ = 0;
  size_t last_subsection_payload_offset_ = 0;

  // Information about the data count section, so it can be removed if it is
  // not needed, and relocs relative to the code section patched up.
  size_t code_start_ = 0;
  size_t data_count_start_ = 0;
  size_t data_count_end_ = 0;
  bool has_data_segment_instruction_ = false;
};

static uint8_t log2_u32(uint32_t x) {
  uint8_t result = 0;
  while (x > 1) {
    x >>= 1;
    result++;
  }
  return result;
}

BinaryWriter::BinaryWriter(Stream* stream,
                           const WriteBinaryOptions& options,
                           const Module* module)
    : stream_(stream), options_(options), module_(module) {}

void BinaryWriter::WriteHeader(const char* name, int index) {
  if (stream_->has_log_stream()) {
    if (index == PRINT_HEADER_NO_INDEX) {
      stream_->log_stream().Writef("; %s\n", name);
    } else {
      stream_->log_stream().Writef("; %s %d\n", name, index);
    }
  }
}

/* returns offset of leb128 */
Offset BinaryWriter::WriteU32Leb128Space(Offset leb_size_guess,
                                         const char* desc) {
  assert(leb_size_guess <= MAX_U32_LEB128_BYTES);
  uint8_t data[MAX_U32_LEB128_BYTES] = {0};
  Offset result = stream_->offset();
  Offset bytes_to_write =
      options_.canonicalize_lebs ? leb_size_guess : MAX_U32_LEB128_BYTES;
  stream_->WriteData(data, bytes_to_write, desc);
  return result;
}

Offset BinaryWriter::WriteFixupU32Leb128Size(Offset offset,
                                             Offset leb_size_guess,
                                             const char* desc) {
  if (options_.canonicalize_lebs) {
    Offset size = stream_->offset() - offset - leb_size_guess;
    Offset leb_size = U32Leb128Length(size);
    Offset delta = leb_size - leb_size_guess;
    if (delta != 0) {
      Offset src_offset = offset + leb_size_guess;
      Offset dst_offset = offset + leb_size;
      stream_->MoveData(dst_offset, src_offset, size);
    }
    WriteU32Leb128At(stream_, offset, size, desc);
    stream_->AddOffset(delta);
    return delta;
  } else {
    Offset size = stream_->offset() - offset - MAX_U32_LEB128_BYTES;
    WriteFixedU32Leb128At(stream_, offset, size, desc);
    return 0;
  }
}

void BinaryWriter::WriteBlockDecl(const BlockDeclaration& decl) {
  if (decl.sig.GetNumParams() == 0 && decl.sig.GetNumResults() <= 1) {
    if (decl.sig.GetNumResults() == 0) {
      WriteType(stream_, Type::Void);
    } else if (decl.sig.GetNumResults() == 1) {
      WriteType(stream_, decl.sig.GetResultType(0));
    }
    return;
  }

  Index index = decl.has_func_type ? module_->GetFuncTypeIndex(decl.type_var)
                                   : module_->GetFuncTypeIndex(decl.sig);
  assert(index != kInvalidIndex);
  WriteS32Leb128WithReloc(index, "block type function index", RelocType::TypeIndexLEB);
}

void BinaryWriter::WriteSectionHeader(const char* desc,
                                      BinarySection section_code) {
  assert(last_section_leb_size_guess_ == 0);
  WriteHeader(desc, PRINT_HEADER_NO_INDEX);
  stream_->WriteU8Enum(section_code, "section code");
  last_section_type_ = section_code;
  last_section_leb_size_guess_ = LEB_SECTION_SIZE_GUESS;
  last_section_offset_ =
      WriteU32Leb128Space(LEB_SECTION_SIZE_GUESS, "section size (guess)");
  last_section_payload_offset_ = stream_->offset();
}

void BinaryWriter::BeginKnownSection(BinarySection section_code) {
  char desc[100];
  wabt_snprintf(desc, sizeof(desc), "section \"%s\" (%u)",
                GetSectionName(section_code),
                static_cast<unsigned>(section_code));
  WriteSectionHeader(desc, section_code);
}

void BinaryWriter::BeginCustomSection(const char* name) {
  char desc[100];
  wabt_snprintf(desc, sizeof(desc), "section \"%s\"", name);
  WriteSectionHeader(desc, BinarySection::Custom);
  WriteStr(stream_, name, "custom section name", PrintChars::Yes);
}

void BinaryWriter::EndSection() {
  assert(last_section_leb_size_guess_ != 0);
  WriteFixupU32Leb128Size(last_section_offset_, last_section_leb_size_guess_,
                          "FIXUP section size");
  last_section_leb_size_guess_ = 0;
  section_count_++;
}

void BinaryWriter::BeginSubsection(const char* name) {
  assert(last_subsection_leb_size_guess_ == 0);
  last_subsection_leb_size_guess_ = LEB_SECTION_SIZE_GUESS;
  last_subsection_offset_ =
      WriteU32Leb128Space(LEB_SECTION_SIZE_GUESS, "subsection size (guess)");
  last_subsection_payload_offset_ = stream_->offset();
}

void BinaryWriter::EndSubsection() {
  assert(last_subsection_leb_size_guess_ != 0);
  WriteFixupU32Leb128Size(last_subsection_offset_,
                          last_subsection_leb_size_guess_,
                          "FIXUP subsection size");
  last_subsection_leb_size_guess_ = 0;
}

Index BinaryWriter::GetLabelVarDepth(const Var* var) {
  return var->index();
}

Index BinaryWriter::GetTagVarDepth(const Var* var) {
  return var->index();
}

Index BinaryWriter::GetSymbolIndex(RelocType reloc_type, Index index) {
  switch (reloc_type) {
    case RelocType::FuncIndexLEB:
      return symtab_.FunctionSymbolIndex(index);
    case RelocType::TableNumberLEB:
      return symtab_.TableSymbolIndex(index);
    case RelocType::GlobalIndexLEB:
      return symtab_.GlobalSymbolIndex(index);
    case RelocType::TypeIndexLEB:
      // Type indexes don't create entries in the symbol table; instead their
      // index is used directly.
      return index;
    default:
      fprintf(stderr, "warning: unsupported relocation type: %s\n",
              GetRelocTypeName(reloc_type));
      return kInvalidIndex;
  }
}

void BinaryWriter::AddReloc(RelocType reloc_type, Index index) {
  // Add a new reloc section if needed
  if (!current_reloc_section_ ||
      current_reloc_section_->section_index != section_count_) {
    reloc_sections_.emplace_back(GetSectionName(last_section_type_), section_count_);
    current_reloc_section_ = &reloc_sections_.back();
  }

  // Add a new relocation to the curent reloc section
  size_t offset = stream_->offset() - last_section_payload_offset_;
  Index symbol_index = GetSymbolIndex(reloc_type, index);
  if (symbol_index == kInvalidIndex) {
    // The file is invalid, for example a reference to function 42 where only 10
    // functions are defined.  The user must have already passed --no-check, so
    // no extra warning here is needed.
    return;
  }
  current_reloc_section_->relocations.emplace_back(reloc_type, offset,
                                                   symbol_index);
}

void BinaryWriter::WriteU32Leb128WithReloc(Index index,
                                           const char* desc,
                                           RelocType reloc_type) {
  if (options_.relocatable) {
    AddReloc(reloc_type, index);
    WriteFixedU32Leb128(stream_, index, desc);
  } else {
    WriteU32Leb128(stream_, index, desc);
  }
}

void BinaryWriter::WriteS32Leb128WithReloc(int32_t value,
                                           const char* desc,
                                           RelocType reloc_type) {
  if (options_.relocatable) {
    AddReloc(reloc_type, value);
    WriteFixedS32Leb128(stream_, value, desc);
  } else {
    WriteS32Leb128(stream_, value, desc);
  }
}

void BinaryWriter::WriteTableNumberWithReloc(Index value,
                                             const char* desc) {
  // Unless reference types are enabled, all references to tables refer to table
  // 0, so no relocs need be emitted when making relocatable binaries.
  if (options_.relocatable && options_.features.reference_types_enabled()) {
    AddReloc(RelocType::TableNumberLEB, value);
    WriteFixedS32Leb128(stream_, value, desc);
  } else {
    WriteS32Leb128(stream_, value, desc);
  }
}

Index BinaryWriter::GetLocalIndex(const Func* func, const Var& var) {
  // func can be nullptr when using local.get/local.set/local.tee in an
  // init_expr.
  if (func) {
    return func->GetLocalIndex(var);
  } else if (var.is_index()) {
    return var.index();
  } else {
    return kInvalidIndex;
  }
}

// TODO(binji): Rename this, it is used for more than loads/stores now.
template <typename T>
void BinaryWriter::WriteLoadStoreExpr(const Func* func,
                                      const Expr* expr,
                                      const char* desc) {
  auto* typed_expr = cast<T>(expr);
  WriteOpcode(stream_, typed_expr->opcode);
  Address align = typed_expr->opcode.GetAlignment(typed_expr->align);
  stream_->WriteU8(log2_u32(align), "alignment");
  WriteU32Leb128(stream_, typed_expr->offset, desc);
}

template <typename T>
void BinaryWriter::WriteSimdLoadStoreLaneExpr(const Func* func,
                                              const Expr* expr,
                                              const char* desc) {
  auto* typed_expr = cast<T>(expr);
  WriteOpcode(stream_, typed_expr->opcode);
  Address align = typed_expr->opcode.GetAlignment(typed_expr->align);
  stream_->WriteU8(log2_u32(align), "alignment");
  WriteU32Leb128(stream_, typed_expr->offset, desc);
  stream_->WriteU8(static_cast<uint8_t>(typed_expr->val), "Simd Lane literal");
}

void BinaryWriter::WriteExpr(const Func* func, const Expr* expr) {
  switch (expr->type()) {
    case ExprType::AtomicLoad:
      WriteLoadStoreExpr<AtomicLoadExpr>(func, expr, "memory offset");
      break;
    case ExprType::AtomicRmw:
      WriteLoadStoreExpr<AtomicRmwExpr>(func, expr, "memory offset");
      break;
    case ExprType::AtomicRmwCmpxchg:
      WriteLoadStoreExpr<AtomicRmwCmpxchgExpr>(func, expr, "memory offset");
      break;
    case ExprType::AtomicStore:
      WriteLoadStoreExpr<AtomicStoreExpr>(func, expr, "memory offset");
      break;
    case ExprType::AtomicWait:
      WriteLoadStoreExpr<AtomicWaitExpr>(func, expr, "memory offset");
      break;
    case ExprType::AtomicFence: {
      auto* fence_expr = cast<AtomicFenceExpr>(expr);
      WriteOpcode(stream_, Opcode::AtomicFence);
      WriteU32Leb128(stream_, fence_expr->consistency_model,
                     "consistency model");
      break;
    }
    case ExprType::AtomicNotify:
      WriteLoadStoreExpr<AtomicNotifyExpr>(func, expr, "memory offset");
      break;
    case ExprType::Binary:
      WriteOpcode(stream_, cast<BinaryExpr>(expr)->opcode);
      break;
    case ExprType::Block:
      WriteOpcode(stream_, Opcode::Block);
      WriteBlockDecl(cast<BlockExpr>(expr)->block.decl);
      WriteExprList(func, cast<BlockExpr>(expr)->block.exprs);
      WriteOpcode(stream_, Opcode::End);
      break;
    case ExprType::Br:
      WriteOpcode(stream_, Opcode::Br);
      WriteU32Leb128(stream_, GetLabelVarDepth(&cast<BrExpr>(expr)->var),
                     "break depth");
      break;
    case ExprType::BrIf:
      WriteOpcode(stream_, Opcode::BrIf);
      WriteU32Leb128(stream_, GetLabelVarDepth(&cast<BrIfExpr>(expr)->var),
                     "break depth");
      break;
    case ExprType::BrTable: {
      auto* br_table_expr = cast<BrTableExpr>(expr);
      WriteOpcode(stream_, Opcode::BrTable);
      WriteU32Leb128(stream_, br_table_expr->targets.size(), "num targets");
      Index depth;
      for (const Var& var : br_table_expr->targets) {
        depth = GetLabelVarDepth(&var);
        WriteU32Leb128(stream_, depth, "break depth");
      }
      depth = GetLabelVarDepth(&br_table_expr->default_target);
      WriteU32Leb128(stream_, depth, "break depth for default");
      break;
    }
    case ExprType::Call:{
      Index index = module_->GetFuncIndex(cast<CallExpr>(expr)->var);
      WriteOpcode(stream_, Opcode::Call);
      WriteU32Leb128WithReloc(index, "function index", RelocType::FuncIndexLEB);
      break;
    }
    case ExprType::ReturnCall: {
      Index index = module_->GetFuncIndex(cast<ReturnCallExpr>(expr)->var);
      WriteOpcode(stream_, Opcode::ReturnCall);
      WriteU32Leb128WithReloc(index, "function index", RelocType::FuncIndexLEB);
      break;
    }
    case ExprType::CallIndirect:{
      Index sig_index =
        module_->GetFuncTypeIndex(cast<CallIndirectExpr>(expr)->decl);
      Index table_index =
        module_->GetTableIndex(cast<CallIndirectExpr>(expr)->table);
      WriteOpcode(stream_, Opcode::CallIndirect);
      WriteU32Leb128WithReloc(sig_index, "signature index", RelocType::TypeIndexLEB);
      WriteTableNumberWithReloc(table_index, "table index");
      break;
    }
    case ExprType::CallRef:{
      WriteOpcode(stream_, Opcode::CallRef);
      break;
    }
    case ExprType::ReturnCallIndirect: {
      Index sig_index =
          module_->GetFuncTypeIndex(cast<ReturnCallIndirectExpr>(expr)->decl);
      Index table_index =
          module_->GetTableIndex(cast<ReturnCallIndirectExpr>(expr)->table);
      WriteOpcode(stream_, Opcode::ReturnCallIndirect);
      WriteU32Leb128WithReloc(sig_index, "signature index", RelocType::TypeIndexLEB);
      WriteTableNumberWithReloc(table_index, "table index");
      break;
    }
    case ExprType::Compare:
      WriteOpcode(stream_, cast<CompareExpr>(expr)->opcode);
      break;
    case ExprType::Const: {
      const Const& const_ = cast<ConstExpr>(expr)->const_;
      switch (const_.type()) {
        case Type::I32: {
          WriteOpcode(stream_, Opcode::I32Const);
          WriteS32Leb128(stream_, const_.u32(), "i32 literal");
          break;
        }
        case Type::I64:
          WriteOpcode(stream_, Opcode::I64Const);
          WriteS64Leb128(stream_, const_.u64(), "i64 literal");
          break;
        case Type::F32:
          WriteOpcode(stream_, Opcode::F32Const);
          stream_->WriteU32(const_.f32_bits(), "f32 literal");
          break;
        case Type::F64:
          WriteOpcode(stream_, Opcode::F64Const);
          stream_->WriteU64(const_.f64_bits(), "f64 literal");
          break;
        case Type::V128:
          WriteOpcode(stream_, Opcode::V128Const);
          stream_->WriteU128(const_.vec128(), "v128 literal");
          break;
        default:
          assert(0);
      }
      break;
    }
    case ExprType::Convert:
      WriteOpcode(stream_, cast<ConvertExpr>(expr)->opcode);
      break;
    case ExprType::Drop:
      WriteOpcode(stream_, Opcode::Drop);
      break;
    case ExprType::GlobalGet: {
      Index index = module_->GetGlobalIndex(cast<GlobalGetExpr>(expr)->var);
      WriteOpcode(stream_, Opcode::GlobalGet);
      WriteU32Leb128WithReloc(index, "global index", RelocType::GlobalIndexLEB);
      break;
    }
    case ExprType::GlobalSet: {
      Index index = module_->GetGlobalIndex(cast<GlobalSetExpr>(expr)->var);
      WriteOpcode(stream_, Opcode::GlobalSet);
      WriteU32Leb128WithReloc(index, "global index", RelocType::GlobalIndexLEB);
      break;
    }
    case ExprType::If: {
      auto* if_expr = cast<IfExpr>(expr);
      WriteOpcode(stream_, Opcode::If);
      WriteBlockDecl(if_expr->true_.decl);
      WriteExprList(func, if_expr->true_.exprs);
      if (!if_expr->false_.empty()) {
        WriteOpcode(stream_, Opcode::Else);
        WriteExprList(func, if_expr->false_);
      }
      WriteOpcode(stream_, Opcode::End);
      break;
    }
    case ExprType::Load:
      WriteLoadStoreExpr<LoadExpr>(func, expr, "load offset");
      break;
    case ExprType::LocalGet: {
      Index index = GetLocalIndex(func, cast<LocalGetExpr>(expr)->var);
      WriteOpcode(stream_, Opcode::LocalGet);
      WriteU32Leb128(stream_, index, "local index");
      break;
    }
    case ExprType::LocalSet: {
      Index index = GetLocalIndex(func, cast<LocalSetExpr>(expr)->var);
      WriteOpcode(stream_, Opcode::LocalSet);
      WriteU32Leb128(stream_, index, "local index");
      break;
    }
    case ExprType::LocalTee: {
      Index index = GetLocalIndex(func, cast<LocalTeeExpr>(expr)->var);
      WriteOpcode(stream_, Opcode::LocalTee);
      WriteU32Leb128(stream_, index, "local index");
      break;
    }
    case ExprType::Loop:
      WriteOpcode(stream_, Opcode::Loop);
      WriteBlockDecl(cast<LoopExpr>(expr)->block.decl);
      WriteExprList(func, cast<LoopExpr>(expr)->block.exprs);
      WriteOpcode(stream_, Opcode::End);
      break;
    case ExprType::MemoryCopy:
      WriteOpcode(stream_, Opcode::MemoryCopy);
      WriteU32Leb128(stream_, 0, "memory.copy reserved");
      WriteU32Leb128(stream_, 0, "memory.copy reserved");
      break;
    case ExprType::DataDrop: {
      Index index =
          module_->GetDataSegmentIndex(cast<DataDropExpr>(expr)->var);
      WriteOpcode(stream_, Opcode::DataDrop);
      WriteU32Leb128(stream_, index, "data.drop segment");
      has_data_segment_instruction_ = true;
      break;
    }
    case ExprType::MemoryFill:
      WriteOpcode(stream_, Opcode::MemoryFill);
      WriteU32Leb128(stream_, 0, "memory.fill reserved");
      break;
    case ExprType::MemoryGrow:
      WriteOpcode(stream_, Opcode::MemoryGrow);
      WriteU32Leb128(stream_, 0, "memory.grow reserved");
      break;
    case ExprType::MemoryInit: {
      Index index =
          module_->GetDataSegmentIndex(cast<MemoryInitExpr>(expr)->var);
      WriteOpcode(stream_, Opcode::MemoryInit);
      WriteU32Leb128(stream_, index, "memory.init segment");
      WriteU32Leb128(stream_, 0, "memory.init reserved");
      has_data_segment_instruction_ = true;
      break;
    }
    case ExprType::MemorySize:
      WriteOpcode(stream_, Opcode::MemorySize);
      WriteU32Leb128(stream_, 0, "memory.size reserved");
      break;
    case ExprType::TableCopy: {
      auto* copy_expr = cast<TableCopyExpr>(expr);
      Index dst = module_->GetTableIndex(copy_expr->dst_table);
      Index src = module_->GetTableIndex(copy_expr->src_table);
      WriteOpcode(stream_, Opcode::TableCopy);
      WriteTableNumberWithReloc(dst, "table.copy dst_table");
      WriteTableNumberWithReloc(src, "table.copy src_table");
      break;
    }
    case ExprType::ElemDrop: {
      Index index =
          module_->GetElemSegmentIndex(cast<ElemDropExpr>(expr)->var);
      WriteOpcode(stream_, Opcode::ElemDrop);
      WriteU32Leb128(stream_, index, "elem.drop segment");
      break;
    }
    case ExprType::TableInit: {
      auto* init_expr = cast<TableInitExpr>(expr);
      Index table_index = module_->GetTableIndex(init_expr->table_index);
      Index segment_index =
          module_->GetElemSegmentIndex(init_expr->segment_index);
      WriteOpcode(stream_, Opcode::TableInit);
      WriteU32Leb128(stream_, segment_index, "table.init segment");
      WriteTableNumberWithReloc(table_index, "table.init table");
      break;
    }
    case ExprType::TableGet: {
      Index index =
          module_->GetTableIndex(cast<TableGetExpr>(expr)->var);
      WriteOpcode(stream_, Opcode::TableGet);
      WriteTableNumberWithReloc(index, "table.get table index");
      break;
    }
    case ExprType::TableSet: {
      Index index =
          module_->GetTableIndex(cast<TableSetExpr>(expr)->var);
      WriteOpcode(stream_, Opcode::TableSet);
      WriteTableNumberWithReloc(index, "table.set table index");
      break;
    }
    case ExprType::TableGrow: {
      Index index =
          module_->GetTableIndex(cast<TableGrowExpr>(expr)->var);
      WriteOpcode(stream_, Opcode::TableGrow);
      WriteTableNumberWithReloc(index, "table.grow table index");
      break;
    }
    case ExprType::TableSize: {
      Index index =
          module_->GetTableIndex(cast<TableSizeExpr>(expr)->var);
      WriteOpcode(stream_, Opcode::TableSize);
      WriteTableNumberWithReloc(index, "table.size table index");
      break;
    }
    case ExprType::TableFill: {
      Index index =
          module_->GetTableIndex(cast<TableFillExpr>(expr)->var);
      WriteOpcode(stream_, Opcode::TableFill);
      WriteTableNumberWithReloc(index, "table.fill table index");
      break;
    }
    case ExprType::RefFunc: {
      WriteOpcode(stream_, Opcode::RefFunc);
      Index index = module_->GetFuncIndex(cast<RefFuncExpr>(expr)->var);
      WriteU32Leb128WithReloc(index, "function index", RelocType::FuncIndexLEB);
      break;
    }
    case ExprType::RefNull: {
      WriteOpcode(stream_, Opcode::RefNull);
      WriteType(stream_, cast<RefNullExpr>(expr)->type, "ref.null type");
      break;
    }
    case ExprType::RefIsNull:
      WriteOpcode(stream_, Opcode::RefIsNull);
      break;
    case ExprType::Nop:
      WriteOpcode(stream_, Opcode::Nop);
      break;
    case ExprType::Rethrow:
      WriteOpcode(stream_, Opcode::Rethrow);
      WriteU32Leb128(stream_, GetLabelVarDepth(&cast<RethrowExpr>(expr)->var),
                     "rethrow depth");
      break;
    case ExprType::Return:
      WriteOpcode(stream_, Opcode::Return);
      break;
    case ExprType::Select: {
      auto* select_expr = cast<SelectExpr>(expr);
      if (select_expr->result_type.empty()) {
        WriteOpcode(stream_, Opcode::Select);
      } else {
        WriteOpcode(stream_, Opcode::SelectT);
        WriteU32Leb128(stream_, select_expr->result_type.size(),
                       "num result types");
        for (Type t : select_expr->result_type) {
          WriteType(stream_, t, "result type");
        }
      }
      break;
    }
    case ExprType::Store:
      WriteLoadStoreExpr<StoreExpr>(func, expr, "store offset");
      break;
    case ExprType::Throw:
      WriteOpcode(stream_, Opcode::Throw);
      WriteU32Leb128(stream_, GetTagVarDepth(&cast<ThrowExpr>(expr)->var),
                     "throw tag");
      break;
    case ExprType::Try: {
      auto* try_expr = cast<TryExpr>(expr);
      WriteOpcode(stream_, Opcode::Try);
      WriteBlockDecl(try_expr->block.decl);
      WriteExprList(func, try_expr->block.exprs);
      switch (try_expr->kind) {
        case TryKind::Catch:
          for (const Catch& catch_ : try_expr->catches) {
            if (catch_.IsCatchAll()) {
              WriteOpcode(stream_, Opcode::CatchAll);
            } else {
              WriteOpcode(stream_, Opcode::Catch);
              WriteU32Leb128(stream_, GetTagVarDepth(&catch_.var), "catch tag");
            }
            WriteExprList(func, catch_.exprs);
          }
          WriteOpcode(stream_, Opcode::End);
          break;
        case TryKind::Delegate:
          WriteOpcode(stream_, Opcode::Delegate);
          WriteU32Leb128(stream_,
                         GetLabelVarDepth(&try_expr->delegate_target),
                         "delegate depth");
          break;
        case TryKind::Plain:
          WriteOpcode(stream_, Opcode::End);
          break;
      }
      break;
    }
    case ExprType::Unary:
      WriteOpcode(stream_, cast<UnaryExpr>(expr)->opcode);
      break;
    case ExprType::Ternary:
      WriteOpcode(stream_, cast<TernaryExpr>(expr)->opcode);
      break;
    case ExprType::SimdLaneOp: {
      const Opcode opcode = cast<SimdLaneOpExpr>(expr)->opcode;
      WriteOpcode(stream_, opcode);
      stream_->WriteU8(static_cast<uint8_t>(cast<SimdLaneOpExpr>(expr)->val),
                       "Simd Lane literal");
      break;
    }
    case ExprType::SimdLoadLane: {
      WriteSimdLoadStoreLaneExpr<SimdLoadLaneExpr>(func, expr, "load offset");
      break;
    }
    case ExprType::SimdStoreLane: {
      WriteSimdLoadStoreLaneExpr<SimdStoreLaneExpr>(func, expr, "store offset");
      break;
    }
    case ExprType::SimdShuffleOp: {
      const Opcode opcode = cast<SimdShuffleOpExpr>(expr)->opcode;
      WriteOpcode(stream_, opcode);
      stream_->WriteU128(cast<SimdShuffleOpExpr>(expr)->val,
                         "Simd Lane[16] literal");
      break;
    }
    case ExprType::LoadSplat:
      WriteLoadStoreExpr<LoadSplatExpr>(func, expr, "load offset");
      break;
    case ExprType::LoadZero:
      WriteLoadStoreExpr<LoadZeroExpr>(func, expr, "load offset");
      break;
    case ExprType::Unreachable:
      WriteOpcode(stream_, Opcode::Unreachable);
      break;
  }
}

void BinaryWriter::WriteExprList(const Func* func, const ExprList& exprs) {
  for (const Expr& expr : exprs) {
    WriteExpr(func, &expr);
  }
}

void BinaryWriter::WriteInitExpr(const ExprList& expr) {
  WriteExprList(nullptr, expr);
  WriteOpcode(stream_, Opcode::End);
}

void BinaryWriter::WriteFuncLocals(const Func* func,
                                   const LocalTypes& local_types) {
  if (local_types.size() == 0) {
    WriteU32Leb128(stream_, 0, "local decl count");
    return;
  }

  Index local_decl_count = local_types.decls().size();
  WriteU32Leb128(stream_, local_decl_count, "local decl count");
  for (auto decl : local_types.decls()) {
    WriteU32Leb128(stream_, decl.second, "local type count");
    WriteType(stream_, decl.first);
  }
}

void BinaryWriter::WriteFunc(const Func* func) {
  WriteFuncLocals(func, func->local_types);
  WriteExprList(func, func->exprs);
  WriteOpcode(stream_, Opcode::End);
}

void BinaryWriter::WriteTable(const Table* table) {
  WriteType(stream_, table->elem_type);
  WriteLimits(stream_, &table->elem_limits);
}

void BinaryWriter::WriteMemory(const Memory* memory) {
  WriteLimits(stream_, &memory->page_limits);
}

void BinaryWriter::WriteGlobalHeader(const Global* global) {
  WriteType(stream_, global->type);
  stream_->WriteU8(global->mutable_, "global mutability");
}

void BinaryWriter::WriteTagType(const Tag* tag) {
  stream_->WriteU8(0, "tag attribute");
  WriteU32Leb128(stream_, module_->GetFuncTypeIndex(tag->decl),
                 "tag signature index");
}

void BinaryWriter::WriteRelocSection(const RelocSection* reloc_section) {
  char section_name[128];
  wabt_snprintf(section_name, sizeof(section_name), "%s.%s",
                WABT_BINARY_SECTION_RELOC, reloc_section->name);
  BeginCustomSection(section_name);
  WriteU32Leb128(stream_, reloc_section->section_index, "reloc section index");
  const std::vector<Reloc>& relocs = reloc_section->relocations;
  WriteU32Leb128(stream_, relocs.size(), "num relocs");

  for (const Reloc& reloc : relocs) {
    WriteU32Leb128(stream_, reloc.type, "reloc type");
    WriteU32Leb128(stream_, reloc.offset, "reloc offset");
    WriteU32Leb128(stream_, reloc.index, "reloc index");
    switch (reloc.type) {
      case RelocType::MemoryAddressLEB:
      case RelocType::MemoryAddressLEB64:
      case RelocType::MemoryAddressSLEB:
      case RelocType::MemoryAddressSLEB64:
      case RelocType::MemoryAddressRelSLEB:
      case RelocType::MemoryAddressRelSLEB64:
      case RelocType::MemoryAddressI32:
      case RelocType::MemoryAddressI64:
      case RelocType::FunctionOffsetI32:
      case RelocType::SectionOffsetI32:
      case RelocType::MemoryAddressTLSSLEB:
      case RelocType::MemoryAddressTLSI32:
        WriteU32Leb128(stream_, reloc.addend, "reloc addend");
        break;
      case RelocType::FuncIndexLEB:
      case RelocType::TableIndexSLEB:
      case RelocType::TableIndexSLEB64:
      case RelocType::TableIndexI32:
      case RelocType::TableIndexI64:
      case RelocType::TypeIndexLEB:
      case RelocType::GlobalIndexLEB:
      case RelocType::TagIndexLEB:
      case RelocType::TableIndexRelSLEB:
      case RelocType::TableNumberLEB:
        break;
      default:
        fprintf(stderr, "warning: unsupported relocation type: %s\n",
                GetRelocTypeName(reloc.type));
    }
  }

  EndSection();
}

void BinaryWriter::WriteLinkingSection() {
  BeginCustomSection(WABT_BINARY_SECTION_LINKING);
  WriteU32Leb128(stream_, 2, "metadata version");
  const std::vector<Symbol>& symbols = symtab_.symbols();
  if (symbols.size()) {
    stream_->WriteU8Enum(LinkingEntryType::SymbolTable, "symbol table");
    BeginSubsection("symbol table");
    WriteU32Leb128(stream_, symbols.size(), "num symbols");

    for (const Symbol& sym : symbols) {
      stream_->WriteU8Enum(sym.type(), "symbol type");
      WriteU32Leb128(stream_, sym.flags(), "symbol flags");
      switch (sym.type()) {
        case SymbolType::Function:
          WriteU32Leb128(stream_, sym.AsFunction().index, "function index");
          if (sym.defined() || sym.explicit_name()) {
            WriteStr(stream_, sym.name(), "function name", PrintChars::Yes);
          }
          break;
        case SymbolType::Data:
          WriteStr(stream_, sym.name(), "data name", PrintChars::Yes);
          if (sym.defined()) {
            WriteU32Leb128(stream_, sym.AsData().index, "data index");
            WriteU32Leb128(stream_, sym.AsData().offset, "data offset");
            WriteU32Leb128(stream_, sym.AsData().size, "data size");
          }
          break;
        case SymbolType::Global:
          WriteU32Leb128(stream_, sym.AsGlobal().index, "global index");
          if (sym.defined() || sym.explicit_name()) {
            WriteStr(stream_, sym.name(), "global name", PrintChars::Yes);
          }
          break;
        case SymbolType::Section:
          WriteU32Leb128(stream_, sym.AsSection().section, "section index");
          break;
        case SymbolType::Tag:
          WriteU32Leb128(stream_, sym.AsTag().index, "tag index");
          if (sym.defined() || sym.explicit_name()) {
            WriteStr(stream_, sym.name(), "tag name", PrintChars::Yes);
          }
          break;
        case SymbolType::Table:
          WriteU32Leb128(stream_, sym.AsTable().index, "table index");
          if (sym.defined() || sym.explicit_name()) {
            WriteStr(stream_, sym.name(), "table name", PrintChars::Yes);
          }
          break;
      }
    }
    EndSubsection();
  }
  EndSection();
}

template <typename T>
void BinaryWriter::WriteNames(const std::vector<T*>& elems,
                              NameSectionSubsection type) {
  size_t num_named_elems = 0;
  for (const T* elem : elems) {
    if (!elem->name.empty()) {
      num_named_elems++;
    }
  }

  if (!num_named_elems) {
    return;
  }

  WriteU32Leb128(stream_, type, "name subsection type");
  BeginSubsection("name subsection");

  char desc[100];
  WriteU32Leb128(stream_, num_named_elems, "num names");
  for (size_t i = 0; i < elems.size(); ++i) {
    const T* elem = elems[i];
    if (elem->name.empty()) {
      continue;
    }
    WriteU32Leb128(stream_, i, "elem index");
    wabt_snprintf(desc, sizeof(desc), "elem name %" PRIzd, i);
    WriteDebugName(stream_, elem->name, desc);
  }
  EndSubsection();
}

Result BinaryWriter::WriteModule() {
  stream_->WriteU32(WABT_BINARY_MAGIC, "WASM_BINARY_MAGIC");
  stream_->WriteU32(WABT_BINARY_VERSION, "WASM_BINARY_VERSION");

  if (options_.relocatable) {
    CHECK_RESULT(symtab_.Populate(module_));
  }

  if (module_->types.size()) {
    BeginKnownSection(BinarySection::Type);
    WriteU32Leb128(stream_, module_->types.size(), "num types");
    for (size_t i = 0; i < module_->types.size(); ++i) {
      const TypeEntry* type = module_->types[i];
      switch (type->kind()) {
        case TypeEntryKind::Func: {
          const FuncType* func_type = cast<FuncType>(type);
          const FuncSignature* sig = &func_type->sig;
          WriteHeader("func type", i);
          WriteType(stream_, Type::Func);

          Index num_params = sig->param_types.size();
          Index num_results = sig->result_types.size();
          WriteU32Leb128(stream_, num_params, "num params");
          for (size_t j = 0; j < num_params; ++j) {
            WriteType(stream_, sig->param_types[j]);
          }

          WriteU32Leb128(stream_, num_results, "num results");
          for (size_t j = 0; j < num_results; ++j) {
            WriteType(stream_, sig->result_types[j]);
          }
          break;
        }

        case TypeEntryKind::Struct: {
          const StructType* struct_type = cast<StructType>(type);
          WriteHeader("struct type", i);
          WriteType(stream_, Type::Struct);
          Index num_fields = struct_type->fields.size();
          WriteU32Leb128(stream_, num_fields, "num fields");
          for (size_t j = 0; j < num_fields; ++j) {
            const Field& field = struct_type->fields[j];
            WriteType(stream_, field.type);
            stream_->WriteU8(field.mutable_, "field mutability");
          }
          break;
        }

        case TypeEntryKind::Array: {
          const ArrayType* array_type = cast<ArrayType>(type);
          WriteHeader("array type", i);
          WriteType(stream_, Type::Array);
          WriteType(stream_, array_type->field.type);
          stream_->WriteU8(array_type->field.mutable_, "field mutability");
          break;
        }
      }
    }
    EndSection();
  }

  if (module_->imports.size()) {
    BeginKnownSection(BinarySection::Import);
    WriteU32Leb128(stream_, module_->imports.size(), "num imports");

    for (size_t i = 0; i < module_->imports.size(); ++i) {
      const Import* import = module_->imports[i];
      WriteHeader("import header", i);
      WriteStr(stream_, import->module_name, "import module name",
               PrintChars::Yes);
      WriteStr(stream_, import->field_name, "import field name",
               PrintChars::Yes);
      stream_->WriteU8Enum(import->kind(), "import kind");
      switch (import->kind()) {
        case ExternalKind::Func:
          WriteU32Leb128(
              stream_,
              module_->GetFuncTypeIndex(cast<FuncImport>(import)->func.decl),
              "import signature index");
          break;

        case ExternalKind::Table:
          WriteTable(&cast<TableImport>(import)->table);
          break;

        case ExternalKind::Memory:
          WriteMemory(&cast<MemoryImport>(import)->memory);
          break;

        case ExternalKind::Global:
          WriteGlobalHeader(&cast<GlobalImport>(import)->global);
          break;

        case ExternalKind::Tag:
          WriteTagType(&cast<TagImport>(import)->tag);
          break;
      }
    }
    EndSection();
  }

  assert(module_->funcs.size() >= module_->num_func_imports);
  Index num_funcs = module_->funcs.size() - module_->num_func_imports;
  if (num_funcs) {
    BeginKnownSection(BinarySection::Function);
    WriteU32Leb128(stream_, num_funcs, "num functions");

    for (size_t i = 0; i < num_funcs; ++i) {
      const Func* func = module_->funcs[i + module_->num_func_imports];
      char desc[100];
      wabt_snprintf(desc, sizeof(desc), "function %" PRIzd " signature index",
                    i);
      WriteU32Leb128(stream_, module_->GetFuncTypeIndex(func->decl), desc);
    }
    EndSection();
  }

  assert(module_->tables.size() >= module_->num_table_imports);
  Index num_tables = module_->tables.size() - module_->num_table_imports;
  if (num_tables) {
    BeginKnownSection(BinarySection::Table);
    WriteU32Leb128(stream_, num_tables, "num tables");
    for (size_t i = 0; i < num_tables; ++i) {
      const Table* table = module_->tables[i + module_->num_table_imports];
      WriteHeader("table", i);
      WriteTable(table);
    }
    EndSection();
  }

  assert(module_->memories.size() >= module_->num_memory_imports);
  Index num_memories = module_->memories.size() - module_->num_memory_imports;
  if (num_memories) {
    BeginKnownSection(BinarySection::Memory);
    WriteU32Leb128(stream_, num_memories, "num memories");
    for (size_t i = 0; i < num_memories; ++i) {
      const Memory* memory = module_->memories[i + module_->num_memory_imports];
      WriteHeader("memory", i);
      WriteMemory(memory);
    }
    EndSection();
  }

  assert(module_->tags.size() >= module_->num_tag_imports);
  Index num_tags = module_->tags.size() - module_->num_tag_imports;
  if (num_tags) {
    BeginKnownSection(BinarySection::Tag);
    WriteU32Leb128(stream_, num_tags, "tag count");
    for (size_t i = 0; i < num_tags; ++i) {
      WriteHeader("tag", i);
      const Tag* tag = module_->tags[i + module_->num_tag_imports];
      WriteTagType(tag);
    }
    EndSection();
  }

  assert(module_->globals.size() >= module_->num_global_imports);
  Index num_globals = module_->globals.size() - module_->num_global_imports;
  if (num_globals) {
    BeginKnownSection(BinarySection::Global);
    WriteU32Leb128(stream_, num_globals, "num globals");

    for (size_t i = 0; i < num_globals; ++i) {
      const Global* global = module_->globals[i + module_->num_global_imports];
      WriteGlobalHeader(global);
      WriteInitExpr(global->init_expr);
    }
    EndSection();
  }

  if (module_->exports.size()) {
    BeginKnownSection(BinarySection::Export);
    WriteU32Leb128(stream_, module_->exports.size(), "num exports");

    for (const Export* export_ : module_->exports) {
      WriteStr(stream_, export_->name, "export name", PrintChars::Yes);
      stream_->WriteU8Enum(export_->kind, "export kind");
      switch (export_->kind) {
        case ExternalKind::Func: {
          Index index = module_->GetFuncIndex(export_->var);
          WriteU32Leb128(stream_, index, "export func index");
          break;
        }
        case ExternalKind::Table: {
          Index index = module_->GetTableIndex(export_->var);
          WriteU32Leb128(stream_, index, "export table index");
          break;
        }
        case ExternalKind::Memory: {
          Index index = module_->GetMemoryIndex(export_->var);
          WriteU32Leb128(stream_, index, "export memory index");
          break;
        }
        case ExternalKind::Global: {
          Index index = module_->GetGlobalIndex(export_->var);
          WriteU32Leb128(stream_, index, "export global index");
          break;
        }
        case ExternalKind::Tag: {
          Index index = module_->GetTagIndex(export_->var);
          WriteU32Leb128(stream_, index, "export tag index");
          break;
        }
      }
    }
    EndSection();
  }

  if (module_->starts.size()) {
    Index start_func_index = module_->GetFuncIndex(*module_->starts[0]);
    if (start_func_index != kInvalidIndex) {
      BeginKnownSection(BinarySection::Start);
      WriteU32Leb128(stream_, start_func_index, "start func index");
      EndSection();
    }
  }

  if (module_->elem_segments.size()) {
    BeginKnownSection(BinarySection::Elem);
    WriteU32Leb128(stream_, module_->elem_segments.size(), "num elem segments");
    for (size_t i = 0; i < module_->elem_segments.size(); ++i) {
      ElemSegment* segment = module_->elem_segments[i];
      WriteHeader("elem segment header", i);
      // 1. flags
      uint8_t flags = segment->GetFlags(module_);
      stream_->WriteU8(flags, "segment flags");
      // 2. optional target table
      if (flags & SegExplicitIndex && segment->kind != SegmentKind::Declared) {
        WriteU32Leb128(stream_, module_->GetTableIndex(segment->table_var),
                       "table index");
      }
      // 3. optional target location within the table (active segments only)
      if (!(flags & SegPassive)) {
        WriteInitExpr(segment->offset);
      }
      // 4. type of item in the following list (omitted for "legacy" segments)
      if (flags & (SegPassive | SegExplicitIndex)) {
        if (flags & SegUseElemExprs) {
          WriteType(stream_, segment->elem_type, "elem expr list type");
        } else {
          stream_->WriteU8Enum(ExternalKind::Func, "elem list type");
        }
      }
      // 5. actual list of elements (with extern indexes or elem expr's)
      // preceeded by length
      WriteU32Leb128(stream_, segment->elem_exprs.size(), "num elems");
      if (flags & SegUseElemExprs) {
        for (const ElemExpr& elem_expr : segment->elem_exprs) {
          switch (elem_expr.kind) {
            case ElemExprKind::RefNull:
              WriteOpcode(stream_, Opcode::RefNull);
              WriteType(stream_, elem_expr.type, "elem expr ref.null type");
              break;

            case ElemExprKind::RefFunc:
              WriteOpcode(stream_, Opcode::RefFunc);
              WriteU32Leb128(stream_, module_->GetFuncIndex(elem_expr.var), "elem expr function index");
              break;
          }
          WriteOpcode(stream_, Opcode::End);
        }
      } else {
        for (const ElemExpr& elem_expr : segment->elem_exprs) {
          assert(elem_expr.kind == ElemExprKind::RefFunc);
          WriteU32Leb128(stream_, module_->GetFuncIndex(elem_expr.var), "elem function index");
        }
      }
    }
    EndSection();
  }

  if (options_.features.bulk_memory_enabled()) {
    // Keep track of the data count section offset so it can be removed if
    // it isn't needed.
    data_count_start_ = stream_->offset();
    BeginKnownSection(BinarySection::DataCount);
    WriteU32Leb128(stream_, module_->data_segments.size(), "data count");
    EndSection();
    data_count_end_ = stream_->offset();
  }

  if (num_funcs) {
    code_start_ = stream_->offset();
    BeginKnownSection(BinarySection::Code);
    WriteU32Leb128(stream_, num_funcs, "num functions");

    for (size_t i = 0; i < num_funcs; ++i) {
      WriteHeader("function body", i);
      const Func* func = module_->funcs[i + module_->num_func_imports];

      /* TODO(binji): better guess of the size of the function body section */
      const Offset leb_size_guess = 1;
      Offset body_size_offset =
          WriteU32Leb128Space(leb_size_guess, "func body size (guess)");
      WriteFunc(func);
      auto func_start_offset = body_size_offset - last_section_payload_offset_;
      auto func_end_offset = stream_->offset() - last_section_payload_offset_;
      auto delta = WriteFixupU32Leb128Size(body_size_offset, leb_size_guess,
                              "FIXUP func body size");
      if (current_reloc_section_ && delta != 0) {
        for (Reloc& reloc : current_reloc_section_->relocations) {
          if (reloc.offset >= func_start_offset && reloc.offset <= func_end_offset) {
            reloc.offset += delta;
          }
        }
      }
    }
    EndSection();
  }

  // Remove the DataCount section if there are no instructions that require it.
  if (options_.features.bulk_memory_enabled() &&
      !has_data_segment_instruction_) {
    Offset size = stream_->offset() - data_count_end_;
    if (size) {
      // If the DataCount section was followed by anything, assert that it's
      // only the Code section.  This limits the amount of fixing-up that we
      // need to do.
      assert(data_count_end_ == code_start_);
      assert(last_section_type_ == BinarySection::Code);
      stream_->MoveData(data_count_start_, data_count_end_, size);
    }
    stream_->Truncate(data_count_start_ + size);

    --section_count_;

    // We just effectively decremented the code section's index; adjust anything
    // that might have captured it.
    for (RelocSection& section : reloc_sections_) {
      if (section.section_index == section_count_) {
        assert(last_section_type_ == BinarySection::Code);
        --section.section_index;
      }
    }
  }

  if (module_->data_segments.size()) {
    BeginKnownSection(BinarySection::Data);
    WriteU32Leb128(stream_, module_->data_segments.size(), "num data segments");
    for (size_t i = 0; i < module_->data_segments.size(); ++i) {
      const DataSegment* segment = module_->data_segments[i];
      WriteHeader("data segment header", i);
      uint8_t flags = segment->GetFlags(module_);
      stream_->WriteU8(flags, "segment flags");
      if (!(flags & SegPassive)) {
        assert(module_->GetMemoryIndex(segment->memory_var) == 0);
        WriteInitExpr(segment->offset);
      }
      WriteU32Leb128(stream_, segment->data.size(), "data segment size");
      WriteHeader("data segment data", i);
      stream_->WriteData(segment->data, "data segment data");
    }
    EndSection();
  }

  if (options_.write_debug_names) {
    std::vector<std::string> index_to_name;

    char desc[100];
    BeginCustomSection(WABT_BINARY_SECTION_NAME);

    if (!module_->name.empty()) {
      WriteU32Leb128(stream_, NameSectionSubsection::Module,
                     "module name type");
      BeginSubsection("module name subsection");
      WriteDebugName(stream_, module_->name, "module name");
      EndSubsection();
    }

    WriteNames<Func>(module_->funcs, NameSectionSubsection::Function);

    WriteU32Leb128(stream_, 2, "local name type");

    BeginSubsection("local name subsection");
    WriteU32Leb128(stream_, module_->funcs.size(), "num functions");
    for (size_t i = 0; i < module_->funcs.size(); ++i) {
      const Func* func = module_->funcs[i];
      Index num_params_and_locals = func->GetNumParamsAndLocals();

      WriteU32Leb128(stream_, i, "function index");
      WriteU32Leb128(stream_, num_params_and_locals, "num locals");

      MakeTypeBindingReverseMapping(num_params_and_locals, func->bindings,
                                    &index_to_name);
      for (size_t j = 0; j < num_params_and_locals; ++j) {
        const std::string& name = index_to_name[j];
        wabt_snprintf(desc, sizeof(desc), "local name %" PRIzd, j);
        WriteU32Leb128(stream_, j, "local index");
        WriteDebugName(stream_, name, desc);
      }
    }
    EndSubsection();

    WriteNames<TypeEntry>(module_->types, NameSectionSubsection::Type);
    WriteNames<Table>(module_->tables, NameSectionSubsection::Table);
    WriteNames<Memory>(module_->memories, NameSectionSubsection::Memory);
    WriteNames<Global>(module_->globals, NameSectionSubsection::Global);
    WriteNames<ElemSegment>(module_->elem_segments,
                            NameSectionSubsection::ElemSegment);
    WriteNames<DataSegment>(module_->data_segments,
                            NameSectionSubsection::DataSegment);

    EndSection();
  }

  if (options_.relocatable) {
    WriteLinkingSection();
    for (RelocSection& section : reloc_sections_) {
      WriteRelocSection(&section);
    }
  }

  return stream_->result();
}

}  // end anonymous namespace

Result WriteBinaryModule(Stream* stream,
                         const Module* module,
                         const WriteBinaryOptions& options) {
  BinaryWriter binary_writer(stream, options, module);
  return binary_writer.WriteModule();
}

}  // namespace wabt
