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

#include "wabt/binary-reader-objdump.h"

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <vector>

#if HAVE_STRCASECMP
#include <strings.h>
#endif

#include "wabt/binary-reader-nop.h"
#include "wabt/filenames.h"
#include "wabt/literal.h"
#include "wabt/string-util.h"

namespace wabt {

namespace {

class BinaryReaderObjdumpBase : public BinaryReaderNop {
 public:
  BinaryReaderObjdumpBase(const uint8_t* data,
                          size_t size,
                          ObjdumpOptions* options,
                          ObjdumpState* state);

  bool OnError(const Error&) override;

  Result BeginModule(uint32_t version) override;
  Result BeginSection(Index section_index,
                      BinarySection section_type,
                      Offset size) override;

  Result OnOpcode(Opcode Opcode) override;
  Result OnRelocCount(Index count, Index section_index) override;

 protected:
  std::string_view GetTypeName(Index index) const;
  std::string_view GetFunctionName(Index index) const;
  std::string_view GetGlobalName(Index index) const;
  std::string_view GetLocalName(Index function_index, Index local_index) const;
  std::string_view GetSectionName(Index index) const;
  std::string_view GetTagName(Index index) const;
  std::string_view GetSymbolName(Index index) const;
  std::string_view GetSegmentName(Index index) const;
  std::string_view GetTableName(Index index) const;
  void PrintRelocation(const Reloc& reloc, Offset offset) const;
  Offset GetPrintOffset(Offset offset) const;
  Offset GetSectionStart(BinarySection section_code) const {
    return section_starts_[static_cast<size_t>(section_code)];
  }

  ObjdumpOptions* options_;
  ObjdumpState* objdump_state_;
  const uint8_t* data_;
  size_t size_;
  bool print_details_ = false;
  BinarySection reloc_section_ = BinarySection::Invalid;
  Offset section_starts_[kBinarySectionCount];
  // Map of section index to section type
  std::vector<BinarySection> section_types_;
  bool section_found_ = false;
  std::string module_name_;
  Opcode current_opcode = Opcode::Unreachable;

  std::unique_ptr<FileStream> err_stream_;
};

BinaryReaderObjdumpBase::BinaryReaderObjdumpBase(const uint8_t* data,
                                                 size_t size,
                                                 ObjdumpOptions* options,
                                                 ObjdumpState* objdump_state)
    : options_(options),
      objdump_state_(objdump_state),
      data_(data),
      size_(size),
      err_stream_(FileStream::CreateStderr()) {
  ZeroMemory(section_starts_);
}

Result BinaryReaderObjdumpBase::BeginSection(Index section_index,
                                             BinarySection section_code,
                                             Offset size) {
  section_starts_[static_cast<size_t>(section_code)] = state->offset;
  section_types_.push_back(section_code);
  return Result::Ok;
}

bool BinaryReaderObjdumpBase::OnError(const Error&) {
  // Tell the BinaryReader that this error is "handled" for all passes other
  // than the prepass. When the error is handled the default message will be
  // suppressed.
  return options_->mode != ObjdumpMode::Prepass;
}

Result BinaryReaderObjdumpBase::BeginModule(uint32_t version) {
  switch (options_->mode) {
    case ObjdumpMode::Headers:
      printf("\n");
      printf("Sections:\n\n");
      break;
    case ObjdumpMode::Details:
      printf("\n");
      printf("Section Details:\n\n");
      break;
    case ObjdumpMode::Disassemble:
      printf("\n");
      printf("Code Disassembly:\n\n");
      break;
    case ObjdumpMode::Prepass: {
      std::string_view basename = GetBasename(options_->filename);
      if (basename == "-") {
        basename = "<stdin>";
      }
      printf("%s:\tfile format wasm %#x\n", std::string(basename).c_str(),
             version);
      break;
    }
    case ObjdumpMode::RawData:
      break;
  }

  return Result::Ok;
}

std::string_view BinaryReaderObjdumpBase::GetTypeName(Index index) const {
  return objdump_state_->type_names.Get(index);
}

std::string_view BinaryReaderObjdumpBase::GetFunctionName(Index index) const {
  return objdump_state_->function_names.Get(index);
}

std::string_view BinaryReaderObjdumpBase::GetGlobalName(Index index) const {
  return objdump_state_->global_names.Get(index);
}

std::string_view BinaryReaderObjdumpBase::GetLocalName(
    Index function_index,
    Index local_index) const {
  return objdump_state_->local_names.Get(function_index, local_index);
}

std::string_view BinaryReaderObjdumpBase::GetSectionName(Index index) const {
  return objdump_state_->section_names.Get(index);
}

std::string_view BinaryReaderObjdumpBase::GetTagName(Index index) const {
  return objdump_state_->tag_names.Get(index);
}

std::string_view BinaryReaderObjdumpBase::GetSegmentName(Index index) const {
  return objdump_state_->segment_names.Get(index);
}

std::string_view BinaryReaderObjdumpBase::GetTableName(Index index) const {
  return objdump_state_->table_names.Get(index);
}

std::string_view BinaryReaderObjdumpBase::GetSymbolName(
    Index symbol_index) const {
  if (symbol_index >= objdump_state_->symtab.size())
    return "<illegal_symbol_index>";
  ObjdumpSymbol& sym = objdump_state_->symtab[symbol_index];
  switch (sym.kind) {
    case SymbolType::Function:
      return GetFunctionName(sym.index);
    case SymbolType::Data:
      return sym.name;
    case SymbolType::Global:
      return GetGlobalName(sym.index);
    case SymbolType::Section:
      return GetSectionName(sym.index);
    case SymbolType::Tag:
      return GetTagName(sym.index);
    case SymbolType::Table:
      return GetTableName(sym.index);
  }
  WABT_UNREACHABLE;
}

void BinaryReaderObjdumpBase::PrintRelocation(const Reloc& reloc,
                                              Offset offset) const {
  printf("           %06" PRIzx ": %-18s %" PRIindex, offset,
         GetRelocTypeName(reloc.type), reloc.index);
  if (reloc.addend) {
    printf(" + %d", reloc.addend);
  }
  if (reloc.type != RelocType::TypeIndexLEB) {
    printf(" <" PRIstringview ">",
           WABT_PRINTF_STRING_VIEW_ARG(GetSymbolName(reloc.index)));
  }
  printf("\n");
}

Offset BinaryReaderObjdumpBase::GetPrintOffset(Offset offset) const {
  return options_->section_offsets
             ? offset - GetSectionStart(BinarySection::Code)
             : offset;
}

Result BinaryReaderObjdumpBase::OnOpcode(Opcode opcode) {
  current_opcode = opcode;
  return Result::Ok;
}

Result BinaryReaderObjdumpBase::OnRelocCount(Index count, Index section_index) {
  if (section_index >= section_types_.size()) {
    err_stream_->Writef("invalid relocation section index: %" PRIindex "\n",
                        section_index);
    reloc_section_ = BinarySection::Invalid;
    return Result::Error;
  }
  reloc_section_ = section_types_[section_index];
  return Result::Ok;
}

class BinaryReaderObjdumpPrepass : public BinaryReaderObjdumpBase {
 public:
  using BinaryReaderObjdumpBase::BinaryReaderObjdumpBase;

  Result BeginSection(Index section_index,
                      BinarySection section_code,
                      Offset size) override {
    BinaryReaderObjdumpBase::BeginSection(section_index, section_code, size);
    if (section_code != BinarySection::Custom) {
      objdump_state_->section_names.Set(section_index,
                                        wabt::GetSectionName(section_code));
    }
    return Result::Ok;
  }

  Result BeginCustomSection(Index section_index,
                            Offset size,
                            std::string_view section_name) override {
    objdump_state_->section_names.Set(section_index, section_name);
    return Result::Ok;
  }

  Result OnFunctionName(Index index, std::string_view name) override {
    SetFunctionName(index, name);
    return Result::Ok;
  }

  Result OnFuncType(Index index,
                    Index param_count,
                    Type* param_types,
                    Index result_count,
                    Type* result_types) override {
    objdump_state_->function_param_counts[index] = param_count;
    return Result::Ok;
  }

  Result OnNameEntry(NameSectionSubsection type,
                     Index index,
                     std::string_view name) override {
    switch (type) {
      // TODO(sbc): remove OnFunctionName in favor of just using
      // OnNameEntry so that this works
      /*
      case NameSectionSubsection::Function:
        SetFunctionName(index, name);
        break;
      */
      case NameSectionSubsection::Type:
        SetTypeName(index, name);
        break;
      case NameSectionSubsection::Global:
        SetGlobalName(index, name);
        break;
      case NameSectionSubsection::Table:
        SetTableName(index, name);
        break;
      case NameSectionSubsection::DataSegment:
        SetSegmentName(index, name);
        break;
      case NameSectionSubsection::Tag:
        SetTagName(index, name);
        break;
      default:
        break;
    }
    return Result::Ok;
  }

  Result OnLocalName(Index function_index,
                     Index local_index,
                     std::string_view local_name) override {
    SetLocalName(function_index, local_index, local_name);
    return Result::Ok;
  }

  Result OnSymbolCount(Index count) override {
    objdump_state_->symtab.resize(count);
    return Result::Ok;
  }

  Result OnDataSymbol(Index index,
                      uint32_t flags,
                      std::string_view name,
                      Index segment,
                      uint32_t offset,
                      uint32_t size) override {
    objdump_state_->symtab[index] = {SymbolType::Data, std::string(name), 0};
    return Result::Ok;
  }

  Result OnFunctionSymbol(Index index,
                          uint32_t flags,
                          std::string_view name,
                          Index func_index) override {
    if (!name.empty()) {
      SetFunctionName(func_index, name);
    }
    objdump_state_->symtab[index] = {SymbolType::Function, std::string(name),
                                     func_index};
    return Result::Ok;
  }

  Result OnGlobalSymbol(Index index,
                        uint32_t flags,
                        std::string_view name,
                        Index global_index) override {
    if (!name.empty()) {
      SetGlobalName(global_index, name);
    }
    objdump_state_->symtab[index] = {SymbolType::Global, std::string(name),
                                     global_index};
    return Result::Ok;
  }

  Result OnSectionSymbol(Index index,
                         uint32_t flags,
                         Index section_index) override {
    objdump_state_->symtab[index] = {SymbolType::Section,
                                     std::string(GetSectionName(section_index)),
                                     section_index};
    return Result::Ok;
  }

  Result OnTagSymbol(Index index,
                     uint32_t flags,
                     std::string_view name,
                     Index tag_index) override {
    if (!name.empty()) {
      SetTagName(tag_index, name);
    }
    objdump_state_->symtab[index] = {SymbolType::Tag, std::string(name),
                                     tag_index};
    return Result::Ok;
  }

  Result OnTableSymbol(Index index,
                       uint32_t flags,
                       std::string_view name,
                       Index table_index) override {
    if (!name.empty()) {
      SetTableName(table_index, name);
    }
    objdump_state_->symtab[index] = {SymbolType::Table, std::string(name),
                                     table_index};
    return Result::Ok;
  }

  Result OnImportFunc(Index import_index,
                      std::string_view module_name,
                      std::string_view field_name,
                      Index func_index,
                      Index sig_index) override {
    SetFunctionName(func_index, module_name + "." + field_name);
    return Result::Ok;
  }

  Result OnImportTag(Index import_index,
                     std::string_view module_name,
                     std::string_view field_name,
                     Index tag_index,
                     Index sig_index) override {
    SetTagName(tag_index, module_name + "." + field_name);
    return Result::Ok;
  }

  Result OnImportGlobal(Index import_index,
                        std::string_view module_name,
                        std::string_view field_name,
                        Index global_index,
                        Type type,
                        bool mutable_) override {
    SetGlobalName(global_index, module_name + "." + field_name);
    return Result::Ok;
  }

  Result OnImportTable(Index import_index,
                       std::string_view module_name,
                       std::string_view field_name,
                       Index table_index,
                       Type elem_type,
                       const Limits* elem_limits) override {
    SetTableName(table_index, module_name + "." + field_name);
    return Result::Ok;
  }

  Result OnExport(Index index,
                  ExternalKind kind,
                  Index item_index,
                  std::string_view name) override {
    if (kind == ExternalKind::Func) {
      SetFunctionName(item_index, name);
    } else if (kind == ExternalKind::Global) {
      SetGlobalName(item_index, name);
    }
    return Result::Ok;
  }

  Result OnReloc(RelocType type,
                 Offset offset,
                 Index index,
                 uint32_t addend) override;

  Result OnModuleName(std::string_view name) override {
    if (options_->mode == ObjdumpMode::Prepass) {
      printf("module name: <" PRIstringview ">\n",
             WABT_PRINTF_STRING_VIEW_ARG(name));
    }
    return Result::Ok;
  }

  Result OnSegmentInfo(Index index,
                       std::string_view name,
                       Address alignment_log2,
                       uint32_t flags) override {
    SetSegmentName(index, name);
    return Result::Ok;
  }

 protected:
  void SetTypeName(Index index, std::string_view name);
  void SetFunctionName(Index index, std::string_view name);
  void SetGlobalName(Index index, std::string_view name);
  void SetLocalName(Index function_index,
                    Index local_index,
                    std::string_view name);
  void SetTagName(Index index, std::string_view name);
  void SetTableName(Index index, std::string_view name);
  void SetSegmentName(Index index, std::string_view name);
};

void BinaryReaderObjdumpPrepass::SetTypeName(Index index,
                                             std::string_view name) {
  objdump_state_->type_names.Set(index, name);
}

void BinaryReaderObjdumpPrepass::SetFunctionName(Index index,
                                                 std::string_view name) {
  objdump_state_->function_names.Set(index, name);
}

void BinaryReaderObjdumpPrepass::SetGlobalName(Index index,
                                               std::string_view name) {
  objdump_state_->global_names.Set(index, name);
}

void BinaryReaderObjdumpPrepass::SetLocalName(Index function_index,
                                              Index local_index,
                                              std::string_view name) {
  objdump_state_->local_names.Set(function_index, local_index, name);
}

void BinaryReaderObjdumpPrepass::SetTagName(Index index,
                                            std::string_view name) {
  objdump_state_->tag_names.Set(index, name);
}

void BinaryReaderObjdumpPrepass::SetTableName(Index index,
                                              std::string_view name) {
  objdump_state_->table_names.Set(index, name);
}

void BinaryReaderObjdumpPrepass::SetSegmentName(Index index,
                                                std::string_view name) {
  objdump_state_->segment_names.Set(index, name);
}

Result BinaryReaderObjdumpPrepass::OnReloc(RelocType type,
                                           Offset offset,
                                           Index index,
                                           uint32_t addend) {
  BinaryReaderObjdumpBase::OnReloc(type, offset, index, addend);
  if (reloc_section_ == BinarySection::Code) {
    objdump_state_->code_relocations.emplace_back(type, offset, index, addend);
  } else if (reloc_section_ == BinarySection::Data) {
    objdump_state_->data_relocations.emplace_back(type, offset, index, addend);
  }
  return Result::Ok;
}

class BinaryReaderObjdumpDisassemble : public BinaryReaderObjdumpBase {
 public:
  using BinaryReaderObjdumpBase::BinaryReaderObjdumpBase;

  std::string BlockSigToString(Type type) const;

  Result BeginFunctionBody(Index index, Offset size) override;
  Result EndFunctionBody(Index index) override;

  Result OnLocalDeclCount(Index count) override;
  Result OnLocalDecl(Index decl_index, Index count, Type type) override;

  Result OnOpcode(Opcode Opcode) override;
  Result OnOpcodeBare() override;
  Result OnOpcodeIndex(Index value) override;
  Result OnOpcodeIndexIndex(Index value, Index value2) override;
  Result OnOpcodeUint32(uint32_t value) override;
  Result OnOpcodeUint32Uint32(uint32_t value, uint32_t value2) override;
  Result OnCallIndirectExpr(uint32_t sig_indix, uint32_t table_index) override;
  Result OnOpcodeUint32Uint32Uint32(uint32_t value,
                                    uint32_t value2,
                                    uint32_t value3) override;
  Result OnOpcodeUint32Uint32Uint32Uint32(uint32_t value,
                                          uint32_t value2,
                                          uint32_t value3,
                                          uint32_t value4) override;
  Result OnOpcodeUint64(uint64_t value) override;
  Result OnOpcodeF32(uint32_t value) override;
  Result OnOpcodeF64(uint64_t value) override;
  Result OnOpcodeV128(v128 value) override;
  Result OnOpcodeBlockSig(Type sig_type) override;
  Result OnOpcodeType(Type type) override;

  Result OnBrTableExpr(Index num_targets,
                       Index* target_depths,
                       Index default_target_depth) override;
  Result OnDelegateExpr(Index) override;
  Result OnEndExpr() override;

 private:
  void LogOpcode(const char* fmt, ...);

  Offset current_opcode_offset = 0;
  Offset last_opcode_end = 0;
  int indent_level = 0;
  Index next_reloc = 0;
  Index current_function_index = 0;
  Index local_index_ = 0;
  bool in_function_body = false;
  bool skip_next_opcode_ = false;
};

std::string BinaryReaderObjdumpDisassemble::BlockSigToString(Type type) const {
  if (type.IsIndex()) {
    return StringPrintf("type[%d]", type.GetIndex());
  } else if (type == Type::Void) {
    return "";
  } else {
    return type.GetName();
  }
}

Result BinaryReaderObjdumpDisassemble::OnOpcode(Opcode opcode) {
  BinaryReaderObjdumpBase::OnOpcode(opcode);
  if (!in_function_body) {
    return Result::Ok;
  }
  if (options_->debug) {
    const char* opcode_name = opcode.GetName();
    err_stream_->Writef("on_opcode: %#" PRIzx ": %s\n", state->offset,
                        opcode_name);
  }

  if (last_opcode_end) {
    // Takes care of cases where opcode's bytes was a non-canonical leb128
    // encoding. In this case, opcode.GetLength() under-reports the length,
    // since it canonicalizes the opcode.
    if (state->offset < last_opcode_end + opcode.GetLength()) {
      Opcode missing_opcode = Opcode::FromCode(data_[last_opcode_end]);
      const char* opcode_name = missing_opcode.GetName();
      fprintf(stderr,
              "error: %#" PRIzx " missing opcode callback at %#" PRIzx
              " (%#02x=%s)\n",
              state->offset, last_opcode_end + 1, data_[last_opcode_end],
              opcode_name);
      return Result::Error;
    }
  }

  current_opcode_offset = state->offset;
  return Result::Ok;
}

#define IMMEDIATE_OCTET_COUNT 9

Result BinaryReaderObjdumpDisassemble::OnLocalDeclCount(Index count) {
  if (!in_function_body) {
    return Result::Ok;
  }
  current_opcode_offset = state->offset;
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnLocalDecl(Index decl_index,
                                                   Index count,
                                                   Type type) {
  if (!in_function_body) {
    return Result::Ok;
  }
  Offset offset = current_opcode_offset;
  size_t data_size = state->offset - offset;

  printf(" %06" PRIzx ":", GetPrintOffset(offset));
  for (size_t i = 0; i < data_size && i < IMMEDIATE_OCTET_COUNT;
       i++, offset++) {
    printf(" %02x", data_[offset]);
  }
  for (size_t i = data_size; i < IMMEDIATE_OCTET_COUNT; i++) {
    printf("   ");
  }
  printf(" | local[%" PRIindex, local_index_);

  if (count != 1) {
    printf("..%" PRIindex "", local_index_ + count - 1);
  }
  local_index_ += count;

  printf("] type=%s\n", type.GetName().c_str());

  last_opcode_end = current_opcode_offset + data_size;
  current_opcode_offset = last_opcode_end;

  return Result::Ok;
}

void BinaryReaderObjdumpDisassemble::LogOpcode(const char* fmt, ...) {
  // BinaryReaderObjdumpDisassemble is only used to disassembly function bodies
  // so this should never be called for instructions outside of function bodies
  // (i.e. init expresions).
  assert(in_function_body);
  if (skip_next_opcode_) {
    skip_next_opcode_ = false;
    return;
  }
  const Offset immediate_len = state->offset - current_opcode_offset;
  const Offset opcode_size = current_opcode.GetLength();
  const Offset total_size = opcode_size + immediate_len;
  // current_opcode_offset has already read past this opcode; rewind it by the
  // size of this opcode, which may be more than one byte.
  Offset offset = current_opcode_offset - opcode_size;
  const Offset offset_end = offset + total_size;

  bool first_line = true;
  while (offset < offset_end) {
    // Print bytes, but only display a maximum of IMMEDIATE_OCTET_COUNT on each
    // line.
    printf(" %06" PRIzx ":", GetPrintOffset(offset));
    size_t i;
    for (i = 0; offset < offset_end && i < IMMEDIATE_OCTET_COUNT;
         ++i, ++offset) {
      printf(" %02x", data_[offset]);
    }
    // Fill the rest of the remaining space with spaces.
    for (; i < IMMEDIATE_OCTET_COUNT; ++i) {
      printf("   ");
    }
    printf(" | ");

    if (first_line) {
      first_line = false;

      // Print disassembly.
      int indent_level = this->indent_level;
      switch (current_opcode) {
        case Opcode::Else:
        case Opcode::Catch:
        case Opcode::CatchAll:
          indent_level--;
          break;
        default:
          break;
      }
      for (int j = 0; j < indent_level; j++) {
        printf("  ");
      }

      const char* opcode_name = current_opcode.GetName();
      printf("%s", opcode_name);
      if (fmt) {
        printf(" ");
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
      }
    }

    printf("\n");
  }

  last_opcode_end = state->offset;

  // Print relocation after then full (potentially multi-line) instruction.
  if (options_->relocs &&
      next_reloc < objdump_state_->code_relocations.size()) {
    const Reloc& reloc = objdump_state_->code_relocations[next_reloc];
    Offset code_start = GetSectionStart(BinarySection::Code);
    Offset abs_offset = code_start + reloc.offset;
    if (last_opcode_end > abs_offset) {
      PrintRelocation(reloc, abs_offset);
      next_reloc++;
    }
  }
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeBare() {
  if (!in_function_body) {
    return Result::Ok;
  }
  LogOpcode(0, nullptr);
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeIndex(Index value) {
  if (!in_function_body) {
    return Result::Ok;
  }
  std::string_view name;
  if (current_opcode == Opcode::Call &&
      !(name = GetFunctionName(value)).empty()) {
    LogOpcode("%d <" PRIstringview ">", value,
              WABT_PRINTF_STRING_VIEW_ARG(name));
  } else if (current_opcode == Opcode::Throw &&
             !(name = GetTagName(value)).empty()) {
    LogOpcode("%d <" PRIstringview ">", value,
              WABT_PRINTF_STRING_VIEW_ARG(name));
  } else if ((current_opcode == Opcode::GlobalGet ||
              current_opcode == Opcode::GlobalSet) &&
             !(name = GetGlobalName(value)).empty()) {
    LogOpcode("%d <" PRIstringview ">", value,
              WABT_PRINTF_STRING_VIEW_ARG(name));
  } else if ((current_opcode == Opcode::LocalGet ||
              current_opcode == Opcode::LocalSet) &&
             !(name = GetLocalName(current_function_index, value)).empty()) {
    LogOpcode("%d <" PRIstringview ">", value,
              WABT_PRINTF_STRING_VIEW_ARG(name));
  } else {
    LogOpcode("%d", value);
  }
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeIndexIndex(Index value,
                                                          Index value2) {
  if (!in_function_body) {
    return Result::Ok;
  }
  LogOpcode("%" PRIindex " %" PRIindex, value, value2);
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeUint32(uint32_t value) {
  if (!in_function_body) {
    return Result::Ok;
  }
  std::string_view name;
  if (current_opcode == Opcode::DataDrop &&
      !(name = GetSegmentName(value)).empty()) {
    LogOpcode("%d <" PRIstringview ">", value,
              WABT_PRINTF_STRING_VIEW_ARG(name));
  } else {
    LogOpcode("%u", value);
  }
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeUint32Uint32(uint32_t value,
                                                            uint32_t value2) {
  if (!in_function_body)
    return Result::Ok;
  std::string_view name;
  if (current_opcode == Opcode::MemoryInit &&
      !(name = GetSegmentName(value)).empty()) {
    LogOpcode("%u %u <" PRIstringview ">", value, value2,
              WABT_PRINTF_STRING_VIEW_ARG(name));
  } else {
    LogOpcode("%u %u", value, value2);
  }
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnCallIndirectExpr(
    uint32_t sig_index,
    uint32_t table_index) {
  std::string_view table_name = GetTableName(table_index);
  std::string_view type_name = GetTypeName(sig_index);
  if (!type_name.empty() && !table_name.empty()) {
    LogOpcode("%u <" PRIstringview "> (type %u <" PRIstringview ">)",
              table_index, WABT_PRINTF_STRING_VIEW_ARG(table_name), sig_index,
              WABT_PRINTF_STRING_VIEW_ARG(type_name));
  } else if (!table_name.empty()) {
    LogOpcode("%u <" PRIstringview "> (type %u)", table_index,
              WABT_PRINTF_STRING_VIEW_ARG(table_name), sig_index);
  } else if (!type_name.empty()) {
    LogOpcode("%u (type %u <" PRIstringview ">)", table_index, sig_index,
              WABT_PRINTF_STRING_VIEW_ARG(type_name));
  } else {
    LogOpcode("%u (type %u)", table_index, sig_index);
  }
  skip_next_opcode_ = true;
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeUint32Uint32Uint32(
    uint32_t value,
    uint32_t value2,
    uint32_t value3) {
  if (!in_function_body) {
    return Result::Ok;
  }
  LogOpcode("%u %u %u", value, value2, value3);
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeUint32Uint32Uint32Uint32(
    uint32_t value,
    uint32_t value2,
    uint32_t value3,
    uint32_t value4) {
  if (!in_function_body) {
    return Result::Ok;
  }
  LogOpcode("%u %u %u %u", value, value2, value3, value4);
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeUint64(uint64_t value) {
  if (!in_function_body) {
    return Result::Ok;
  }
  LogOpcode("%" PRId64, value);
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeF32(uint32_t value) {
  if (!in_function_body) {
    return Result::Ok;
  }
  char buffer[WABT_MAX_FLOAT_HEX];
  WriteFloatHex(buffer, sizeof(buffer), value);
  LogOpcode(buffer);
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeF64(uint64_t value) {
  if (!in_function_body) {
    return Result::Ok;
  }
  char buffer[WABT_MAX_DOUBLE_HEX];
  WriteDoubleHex(buffer, sizeof(buffer), value);
  LogOpcode(buffer);
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeV128(v128 value) {
  if (!in_function_body) {
    return Result::Ok;
  }
  // v128 is always dumped as i32x4:
  LogOpcode("0x%08x 0x%08x 0x%08x 0x%08x", value.u32(0), value.u32(1),
            value.u32(2), value.u32(3));
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeType(Type type) {
  if (!in_function_body) {
    return Result::Ok;
  }
  if (current_opcode == Opcode::SelectT) {
    LogOpcode(type.GetName().c_str());
  } else {
    LogOpcode(type.GetRefKindName());
  }
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnBrTableExpr(
    Index num_targets,
    Index* target_depths,
    Index default_target_depth) {
  if (!in_function_body) {
    return Result::Ok;
  }

  std::string buffer = std::string();
  for (Index i = 0; i < num_targets; i++) {
    buffer.append(std::to_string(target_depths[i])).append(" ");
  }
  buffer.append(std::to_string(default_target_depth));

  LogOpcode("%s", buffer.c_str());
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnDelegateExpr(Index depth) {
  if (!in_function_body) {
    return Result::Ok;
  }
  // Because `delegate` ends the block we need to dedent here, and
  // we don't need to dedent it in LogOpcode.
  if (indent_level > 0) {
    indent_level--;
  }
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnEndExpr() {
  if (!in_function_body) {
    return Result::Ok;
  }
  if (indent_level > 0) {
    indent_level--;
  }
  LogOpcode(0, nullptr);
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::BeginFunctionBody(Index index,
                                                         Offset size) {
  printf("%06" PRIzx " func[%" PRIindex "]", GetPrintOffset(state->offset),
         index);
  auto name = GetFunctionName(index);
  if (!name.empty()) {
    printf(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
  }
  printf(":\n");

  last_opcode_end = 0;
  in_function_body = true;
  current_function_index = index;
  local_index_ = objdump_state_->function_param_counts[index];
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::EndFunctionBody(Index index) {
  assert(in_function_body);
  in_function_body = false;
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeBlockSig(Type sig_type) {
  if (!in_function_body) {
    return Result::Ok;
  }
  if (sig_type != Type::Void) {
    LogOpcode("%s", BlockSigToString(sig_type).c_str());
  } else {
    LogOpcode(nullptr);
  }
  indent_level++;
  return Result::Ok;
}

enum class InitExprType {
  I32,
  F32,
  I64,
  F64,
  V128,
  Global,
  FuncRef,
  // TODO: There isn't a nullref anymore, this just represents ref.null of some
  // type T.
  NullRef,
};

struct InitInst {
  Opcode opcode;
  union {
    Index index;
    uint32_t i32;
    uint32_t f32;
    uint64_t i64;
    uint64_t f64;
    v128 v128_v;
    Type type;
  } imm;
};

struct InitExpr {
  InitExprType type;
  std::vector<InitInst> insts;
};

class BinaryReaderObjdump : public BinaryReaderObjdumpBase {
 public:
  BinaryReaderObjdump(const uint8_t* data,
                      size_t size,
                      ObjdumpOptions* options,
                      ObjdumpState* state);

  Result EndModule() override;
  Result BeginSection(Index section_index,
                      BinarySection section_type,
                      Offset size) override;
  Result BeginCustomSection(Index section_index,
                            Offset size,
                            std::string_view section_name) override;

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

  Result OnExportCount(Index count) override;
  Result OnExport(Index index,
                  ExternalKind kind,
                  Index item_index,
                  std::string_view name) override;

  Result OnStartFunction(Index func_index) override;
  Result OnDataCount(Index count) override;

  Result OnFunctionBodyCount(Index count) override;
  Result BeginFunctionBody(Index index, Offset size) override;

  Result OnElemSegmentCount(Index count) override;
  Result BeginElemSegment(Index index,
                          Index table_index,
                          uint8_t flags) override;
  Result OnElemSegmentElemType(Index index, Type elem_type) override;
  Result OnElemSegmentElemExprCount(Index index, Index count) override;
  Result OnElemSegmentElemExpr_RefNull(Index segment_index, Type type) override;
  Result OnElemSegmentElemExpr_RefFunc(Index segment_index,
                                       Index func_index) override;

  void BeginInitExpr() { current_init_expr_.insts.clear(); }

  Result BeginElemSegmentInitExpr(Index index) override {
    reading_elem_init_expr_ = true;
    BeginInitExpr();
    return Result::Ok;
  }

  Result EndElemSegmentInitExpr(Index index) override { return EndInitExpr(); }

  Result BeginDataSegmentInitExpr(Index index) override {
    reading_data_init_expr_ = true;
    BeginInitExpr();
    return Result::Ok;
  }

  Result EndDataSegmentInitExpr(Index index) override { return EndInitExpr(); }

  Result BeginGlobalInitExpr(Index index) override {
    reading_global_init_expr_ = true;
    BeginInitExpr();
    return Result::Ok;
  }

  Result EndGlobalInitExpr(Index index) override { return EndInitExpr(); }

  Result OnDataSegmentCount(Index count) override;
  Result BeginDataSegment(Index index,
                          Index memory_index,
                          uint8_t flags) override;
  Result OnDataSegmentData(Index index,
                           const void* data,
                           Address size) override;

  Result OnModuleName(std::string_view name) override;
  Result OnFunctionName(Index function_index,
                        std::string_view function_name) override;
  Result OnLocalName(Index function_index,
                     Index local_index,
                     std::string_view local_name) override;
  Result OnNameEntry(NameSectionSubsection type,
                     Index index,
                     std::string_view name) override;

  Result OnDylinkInfo(uint32_t mem_size,
                      uint32_t mem_align_log2,
                      uint32_t table_size,
                      uint32_t table_align_log2) override;
  Result OnDylinkNeededCount(Index count) override;
  Result OnDylinkNeeded(std::string_view so_name) override;
  Result OnDylinkImportCount(Index count) override;
  Result OnDylinkExportCount(Index count) override;
  Result OnDylinkImport(std::string_view module,
                        std::string_view name,
                        uint32_t flags) override;
  Result OnDylinkExport(std::string_view name, uint32_t flags) override;

  Result OnRelocCount(Index count, Index section_index) override;
  Result OnReloc(RelocType type,
                 Offset offset,
                 Index index,
                 uint32_t addend) override;

  Result OnFeature(uint8_t prefix, std::string_view name) override;

  Result OnSymbolCount(Index count) override;
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
  Result OnTagSymbol(Index index,
                     uint32_t flags,
                     std::string_view name,
                     Index tag_index) override;
  Result OnTableSymbol(Index index,
                       uint32_t flags,
                       std::string_view name,
                       Index table_index) override;
  Result OnSegmentInfoCount(Index count) override;
  Result OnSegmentInfo(Index index,
                       std::string_view name,
                       Address alignment_log2,
                       uint32_t flags) override;
  Result OnInitFunctionCount(Index count) override;
  Result OnInitFunction(uint32_t priority, Index symbol_index) override;
  Result OnComdatCount(Index count) override;
  Result OnComdatBegin(std::string_view name,
                       uint32_t flags,
                       Index count) override;
  Result OnComdatEntry(ComdatType kind, Index index) override;

  Result OnTagCount(Index count) override;
  Result OnTagType(Index index, Index sig_index) override;

  Result OnOpcode(Opcode Opcode) override;
  Result OnI32ConstExpr(uint32_t value) override;
  Result OnI64ConstExpr(uint64_t value) override;
  Result OnF32ConstExpr(uint32_t value) override;
  Result OnF64ConstExpr(uint64_t value) override;
  Result OnGlobalGetExpr(Index global_index) override;
  Result OnCodeMetadataCount(Index function_index, Index count) override;
  Result OnCodeMetadata(Offset code_offset,
                        const void* data,
                        Address size) override;

 private:
  Result EndInitExpr();
  bool ShouldPrintDetails();
  void PrintDetails(const char* fmt, ...);
  Result PrintSymbolFlags(uint32_t flags);
  Result PrintSegmentFlags(uint32_t flags);
  void PrintInitExpr(const InitExpr& expr, bool as_unsigned = false);
  Result OnCount(Index count);

  std::unique_ptr<FileStream> out_stream_;
  Index elem_index_ = 0;
  Index table_index_ = 0;
  Index next_data_reloc_ = 0;
  bool reading_elem_init_expr_ = false;
  bool reading_data_init_expr_ = false;
  bool reading_global_init_expr_ = false;
  InitExpr current_init_expr_;
  uint8_t data_flags_ = 0;
  uint8_t elem_flags_ = 0;
  Index data_mem_index_ = 0;
  uint64_t data_offset_ = 0;
  uint64_t elem_offset_ = 0;

  bool ReadingInitExpr() {
    return reading_elem_init_expr_ || reading_data_init_expr_ ||
           reading_global_init_expr_;
  }
};

BinaryReaderObjdump::BinaryReaderObjdump(const uint8_t* data,
                                         size_t size,
                                         ObjdumpOptions* options,
                                         ObjdumpState* objdump_state)
    : BinaryReaderObjdumpBase(data, size, options, objdump_state),
      out_stream_(FileStream::CreateStdout()) {}

Result BinaryReaderObjdump::BeginCustomSection(Index section_index,
                                               Offset size,
                                               std::string_view section_name) {
  PrintDetails(" - name: \"" PRIstringview "\"\n",
               WABT_PRINTF_STRING_VIEW_ARG(section_name));
  if (options_->mode == ObjdumpMode::Headers) {
    printf("\"" PRIstringview "\"\n",
           WABT_PRINTF_STRING_VIEW_ARG(section_name));
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::BeginSection(Index section_index,
                                         BinarySection section_code,
                                         Offset size) {
  BinaryReaderObjdumpBase::BeginSection(section_index, section_code, size);

  // |section_name| and |match_name| are identical for known sections. For
  // custom sections, |section_name| is "Custom", but |match_name| is the name
  // of the custom section.
  const char* section_name = wabt::GetSectionName(section_code);
  std::string match_name(GetSectionName(section_index));

  bool section_match = !options_->section_name ||
                       !strcasecmp(options_->section_name, match_name.c_str());
  if (section_match) {
    section_found_ = true;
  }

  switch (options_->mode) {
    case ObjdumpMode::Headers:
      printf("%9s start=%#010" PRIzx " end=%#010" PRIzx " (size=%#010" PRIoffset
             ") ",
             section_name, state->offset, state->offset + size, size);
      break;
    case ObjdumpMode::Details:
      if (section_match) {
        printf("%s", section_name);
        // All known section types except the Start and DataCount sections have
        // a count in which case this line gets completed in OnCount().
        if (section_code == BinarySection::Start ||
            section_code == BinarySection::DataCount ||
            section_code == BinarySection::Custom) {
          printf(":\n");
        }
        print_details_ = true;
      } else {
        print_details_ = false;
      }
      break;
    case ObjdumpMode::RawData:
      if (section_match) {
        printf("\nContents of section %s:\n", section_name);
        out_stream_->WriteMemoryDump(data_ + state->offset, size, state->offset,
                                     PrintChars::Yes);
      }
      break;
    case ObjdumpMode::Prepass:
    case ObjdumpMode::Disassemble:
      break;
  }
  return Result::Ok;
}

bool BinaryReaderObjdump::ShouldPrintDetails() {
  if (options_->mode != ObjdumpMode::Details) {
    return false;
  }
  return print_details_;
}

void WABT_PRINTF_FORMAT(2, 3) BinaryReaderObjdump::PrintDetails(const char* fmt,
                                                                ...) {
  if (!ShouldPrintDetails()) {
    return;
  }
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

Result BinaryReaderObjdump::OnCount(Index count) {
  if (options_->mode == ObjdumpMode::Headers) {
    printf("count: %" PRIindex "\n", count);
  } else if (options_->mode == ObjdumpMode::Details && print_details_) {
    printf("[%" PRIindex "]:\n", count);
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::EndModule() {
  if (options_->section_name && !section_found_) {
    err_stream_->Writef("Section not found: %s\n", options_->section_name);
    return Result::Error;
  }

  if (options_->relocs && ShouldPrintDetails()) {
    if (next_data_reloc_ != objdump_state_->data_relocations.size()) {
      err_stream_->Writef("Data reloctions outside of segments!:\n");
      for (size_t i = next_data_reloc_;
           i < objdump_state_->data_relocations.size(); i++) {
        const Reloc& reloc = objdump_state_->data_relocations[i];
        PrintRelocation(reloc, reloc.offset);
      }

      return Result::Error;
    }
  }

  return Result::Ok;
}

Result BinaryReaderObjdump::OnTypeCount(Index count) {
  return OnCount(count);
}

Result BinaryReaderObjdump::OnFuncType(Index index,
                                       Index param_count,
                                       Type* param_types,
                                       Index result_count,
                                       Type* result_types) {
  if (!ShouldPrintDetails()) {
    return Result::Ok;
  }
  printf(" - type[%" PRIindex "] (", index);
  for (Index i = 0; i < param_count; i++) {
    if (i != 0) {
      printf(", ");
    }
    printf("%s", param_types[i].GetName().c_str());
  }
  printf(") -> ");
  switch (result_count) {
    case 0:
      printf("nil");
      break;
    case 1:
      printf("%s", result_types[0].GetName().c_str());
      break;
    default:
      printf("(");
      for (Index i = 0; i < result_count; i++) {
        if (i != 0) {
          printf(", ");
        }
        printf("%s", result_types[i].GetName().c_str());
      }
      printf(")");
      break;
  }
  printf("\n");
  return Result::Ok;
}

Result BinaryReaderObjdump::OnStructType(Index index,
                                         Index field_count,
                                         TypeMut* fields) {
  if (!ShouldPrintDetails()) {
    return Result::Ok;
  }
  printf(" - type[%" PRIindex "] (struct", index);
  for (Index i = 0; i < field_count; i++) {
    if (fields[i].mutable_) {
      printf(" (mut");
    }
    printf(" %s", fields[i].type.GetName().c_str());
    if (fields[i].mutable_) {
      printf(")");
    }
  }
  printf(")\n");
  return Result::Ok;
}

Result BinaryReaderObjdump::OnArrayType(Index index, TypeMut field) {
  if (!ShouldPrintDetails()) {
    return Result::Ok;
  }
  printf(" - type[%" PRIindex "] (array", index);
  if (field.mutable_) {
    printf(" (mut");
  }
  printf(" %s", field.type.GetName().c_str());
  if (field.mutable_) {
    printf(")");
  }
  printf(")\n");
  return Result::Ok;
}

Result BinaryReaderObjdump::OnFunctionCount(Index count) {
  return OnCount(count);
}

Result BinaryReaderObjdump::OnFunction(Index index, Index sig_index) {
  PrintDetails(" - func[%" PRIindex "] sig=%" PRIindex, index, sig_index);
  auto name = GetFunctionName(index);
  if (!name.empty()) {
    PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
  }
  PrintDetails("\n");
  return Result::Ok;
}

Result BinaryReaderObjdump::OnFunctionBodyCount(Index count) {
  return OnCount(count);
}

Result BinaryReaderObjdump::BeginFunctionBody(Index index, Offset size) {
  PrintDetails(" - func[%" PRIindex "] size=%" PRIzd, index, size);
  auto name = GetFunctionName(index);
  if (!name.empty()) {
    PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
  }
  PrintDetails("\n");
  return Result::Ok;
}

Result BinaryReaderObjdump::OnStartFunction(Index func_index) {
  if (options_->mode == ObjdumpMode::Headers) {
    printf("start: %" PRIindex "\n", func_index);
  } else {
    PrintDetails(" - start function: %" PRIindex, func_index);
    auto name = GetFunctionName(func_index);
    if (!name.empty()) {
      PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
    }
    PrintDetails("\n");
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::OnDataCount(Index count) {
  if (options_->mode == ObjdumpMode::Headers) {
    printf("count: %" PRIindex "\n", count);
  } else {
    PrintDetails(" - data count: %" PRIindex "\n", count);
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::OnImportCount(Index count) {
  return OnCount(count);
}

Result BinaryReaderObjdump::OnImportFunc(Index import_index,
                                         std::string_view module_name,
                                         std::string_view field_name,
                                         Index func_index,
                                         Index sig_index) {
  PrintDetails(" - func[%" PRIindex "] sig=%" PRIindex, func_index, sig_index);
  auto name = GetFunctionName(func_index);
  if (!name.empty()) {
    PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
  }
  PrintDetails(" <- " PRIstringview "." PRIstringview "\n",
               WABT_PRINTF_STRING_VIEW_ARG(module_name),
               WABT_PRINTF_STRING_VIEW_ARG(field_name));
  return Result::Ok;
}

Result BinaryReaderObjdump::OnImportTable(Index import_index,
                                          std::string_view module_name,
                                          std::string_view field_name,
                                          Index table_index,
                                          Type elem_type,
                                          const Limits* elem_limits) {
  PrintDetails(" - table[%" PRIindex "] type=%s initial=%" PRId64, table_index,
               elem_type.GetName().c_str(), elem_limits->initial);
  if (elem_limits->has_max) {
    PrintDetails(" max=%" PRId64, elem_limits->max);
  }
  PrintDetails(" <- " PRIstringview "." PRIstringview "\n",
               WABT_PRINTF_STRING_VIEW_ARG(module_name),
               WABT_PRINTF_STRING_VIEW_ARG(field_name));
  return Result::Ok;
}

Result BinaryReaderObjdump::OnImportMemory(Index import_index,
                                           std::string_view module_name,
                                           std::string_view field_name,
                                           Index memory_index,
                                           const Limits* page_limits) {
  PrintDetails(" - memory[%" PRIindex "] pages: initial=%" PRId64, memory_index,
               page_limits->initial);
  if (page_limits->has_max) {
    PrintDetails(" max=%" PRId64, page_limits->max);
  }
  if (page_limits->is_shared) {
    PrintDetails(" shared");
  }
  if (page_limits->is_64) {
    PrintDetails(" i64");
  }
  PrintDetails(" <- " PRIstringview "." PRIstringview "\n",
               WABT_PRINTF_STRING_VIEW_ARG(module_name),
               WABT_PRINTF_STRING_VIEW_ARG(field_name));
  return Result::Ok;
}

Result BinaryReaderObjdump::OnImportGlobal(Index import_index,
                                           std::string_view module_name,
                                           std::string_view field_name,
                                           Index global_index,
                                           Type type,
                                           bool mutable_) {
  PrintDetails(" - global[%" PRIindex "] %s mutable=%d", global_index,
               type.GetName().c_str(), mutable_);
  PrintDetails(" <- " PRIstringview "." PRIstringview "\n",
               WABT_PRINTF_STRING_VIEW_ARG(module_name),
               WABT_PRINTF_STRING_VIEW_ARG(field_name));
  return Result::Ok;
}

Result BinaryReaderObjdump::OnImportTag(Index import_index,
                                        std::string_view module_name,
                                        std::string_view field_name,
                                        Index tag_index,
                                        Index sig_index) {
  PrintDetails(" - tag[%" PRIindex "] sig=%" PRIindex, tag_index, sig_index);
  auto name = GetTagName(tag_index);
  if (!name.empty()) {
    PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
  }
  PrintDetails(" <- " PRIstringview "." PRIstringview "\n",
               WABT_PRINTF_STRING_VIEW_ARG(module_name),
               WABT_PRINTF_STRING_VIEW_ARG(field_name));
  return Result::Ok;
}

Result BinaryReaderObjdump::OnMemoryCount(Index count) {
  return OnCount(count);
}

Result BinaryReaderObjdump::OnMemory(Index index, const Limits* page_limits) {
  PrintDetails(" - memory[%" PRIindex "] pages: initial=%" PRId64, index,
               page_limits->initial);
  if (page_limits->has_max) {
    PrintDetails(" max=%" PRId64, page_limits->max);
  }
  if (page_limits->is_shared) {
    PrintDetails(" shared");
  }
  if (page_limits->is_64) {
    PrintDetails(" i64");
  }
  PrintDetails("\n");
  return Result::Ok;
}

Result BinaryReaderObjdump::OnTableCount(Index count) {
  return OnCount(count);
}

Result BinaryReaderObjdump::OnTable(Index index,
                                    Type elem_type,
                                    const Limits* elem_limits) {
  PrintDetails(" - table[%" PRIindex "] type=%s initial=%" PRId64, index,
               elem_type.GetName().c_str(), elem_limits->initial);
  if (elem_limits->has_max) {
    PrintDetails(" max=%" PRId64, elem_limits->max);
  }
  auto name = GetTableName(index);
  if (!name.empty()) {
    PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
  }
  PrintDetails("\n");
  return Result::Ok;
}

Result BinaryReaderObjdump::OnExportCount(Index count) {
  return OnCount(count);
}

Result BinaryReaderObjdump::OnExport(Index index,
                                     ExternalKind kind,
                                     Index item_index,
                                     std::string_view name) {
  PrintDetails(" - %s[%" PRIindex "]", GetKindName(kind), item_index);
  if (kind == ExternalKind::Func) {
    auto name = GetFunctionName(item_index);
    if (!name.empty()) {
      PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
    }
  }

  PrintDetails(" -> \"" PRIstringview "\"\n",
               WABT_PRINTF_STRING_VIEW_ARG(name));
  return Result::Ok;
}

Result BinaryReaderObjdump::OnElemSegmentElemExpr_RefNull(Index segment_index,
                                                          Type type) {
  PrintDetails("  - elem[%" PRIu64 "] = ref.null %s\n",
               elem_offset_ + elem_index_, type.GetName().c_str());
  elem_index_++;
  return Result::Ok;
}

Result BinaryReaderObjdump::OnElemSegmentElemExpr_RefFunc(Index segment_index,
                                                          Index func_index) {
  PrintDetails("  - elem[%" PRIu64 "] = func[%" PRIindex "]",
               elem_offset_ + elem_index_, func_index);
  auto name = GetFunctionName(func_index);
  if (!name.empty()) {
    PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
  }
  PrintDetails("\n");
  elem_index_++;
  return Result::Ok;
}

Result BinaryReaderObjdump::OnElemSegmentCount(Index count) {
  return OnCount(count);
}

Result BinaryReaderObjdump::BeginElemSegment(Index index,
                                             Index table_index,
                                             uint8_t flags) {
  table_index_ = table_index;
  elem_index_ = 0;
  elem_flags_ = flags;
  return Result::Ok;
}

Result BinaryReaderObjdump::OnElemSegmentElemType(Index index, Type elem_type) {
  // TODO: Add support for this.
  return Result::Ok;
}

Result BinaryReaderObjdump::OnElemSegmentElemExprCount(Index index,
                                                       Index count) {
  PrintDetails(" - segment[%" PRIindex "] flags=%d table=%" PRIindex
               " count=%" PRIindex,
               index, elem_flags_, table_index_, count);
  if (elem_flags_ & SegPassive) {
    PrintDetails("\n");
  } else {
    PrintInitExpr(current_init_expr_, /*as_unsigned=*/true);
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::OnGlobalCount(Index count) {
  return OnCount(count);
}

Result BinaryReaderObjdump::BeginGlobal(Index index, Type type, bool mutable_) {
  PrintDetails(" - global[%" PRIindex "] %s mutable=%d", index,
               type.GetName().c_str(), mutable_);
  std::string_view name = GetGlobalName(index);
  if (!name.empty()) {
    PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
  }
  return Result::Ok;
}

void BinaryReaderObjdump::PrintInitExpr(const InitExpr& expr,
                                        bool as_unsigned) {
  assert(expr.insts.size() > 0);

  // We have two different way to print init expressions.  One for
  // extended expressions involving more than one instruction, and
  // a short form for the more traditional single instruction form.
  if (expr.insts.size() > 1) {
    PrintDetails(" - init (");
    bool first = true;
    for (auto& inst : expr.insts) {
      if (!first) {
        PrintDetails(", ");
      }
      first = false;
      PrintDetails("%s", inst.opcode.GetName());
      switch (inst.opcode) {
        case Opcode::I32Const:
          PrintDetails(" %d", inst.imm.i32);
          break;
        case Opcode::I64Const:
          PrintDetails(" %" PRId64, inst.imm.i64);
          break;
        case Opcode::F32Const: {
          char buffer[WABT_MAX_FLOAT_HEX];
          WriteFloatHex(buffer, sizeof(buffer), inst.imm.f32);
          PrintDetails(" %s\n", buffer);
          break;
        }
        case Opcode::F64Const: {
          char buffer[WABT_MAX_DOUBLE_HEX];
          WriteDoubleHex(buffer, sizeof(buffer), inst.imm.f64);
          PrintDetails(" %s\n", buffer);
          break;
        }
        case Opcode::GlobalGet: {
          PrintDetails(" %" PRIindex, inst.imm.index);
          std::string_view name = GetGlobalName(inst.imm.index);
          if (!name.empty()) {
            PrintDetails(" <" PRIstringview ">",
                         WABT_PRINTF_STRING_VIEW_ARG(name));
          }
          break;
        }
        default:
          break;
      }
    }
    PrintDetails(")\n");
    return;
  }

  switch (expr.type) {
    case InitExprType::I32:
      if (as_unsigned) {
        PrintDetails(" - init i32=%u\n", expr.insts[0].imm.i32);
      } else {
        PrintDetails(" - init i32=%d\n", expr.insts[0].imm.i32);
      }
      break;
    case InitExprType::I64:
      if (as_unsigned) {
        PrintDetails(" - init i64=%" PRIu64 "\n", expr.insts[0].imm.i64);
      } else {
        PrintDetails(" - init i64=%" PRId64 "\n", expr.insts[0].imm.i64);
      }
      break;
    case InitExprType::F64: {
      char buffer[WABT_MAX_DOUBLE_HEX];
      WriteDoubleHex(buffer, sizeof(buffer), expr.insts[0].imm.f64);
      PrintDetails(" - init f64=%s\n", buffer);
      break;
    }
    case InitExprType::F32: {
      char buffer[WABT_MAX_FLOAT_HEX];
      WriteFloatHex(buffer, sizeof(buffer), expr.insts[0].imm.f32);
      PrintDetails(" - init f32=%s\n", buffer);
      break;
    }
    case InitExprType::V128: {
      PrintDetails(
          " - init v128=0x%08x 0x%08x 0x%08x 0x%08x \n",
          expr.insts[0].imm.v128_v.u32(0), expr.insts[0].imm.v128_v.u32(1),
          expr.insts[0].imm.v128_v.u32(2), expr.insts[0].imm.v128_v.u32(3));
      break;
    }
    case InitExprType::Global: {
      PrintDetails(" - init global=%" PRIindex, expr.insts[0].imm.index);
      std::string_view name = GetGlobalName(expr.insts[0].imm.index);
      if (!name.empty()) {
        PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
      }
      PrintDetails("\n");
      break;
    }
    case InitExprType::FuncRef: {
      PrintDetails(" - init ref.func:%" PRIindex, expr.insts[0].imm.index);
      std::string_view name = GetFunctionName(expr.insts[0].imm.index);
      if (!name.empty()) {
        PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
      }
      PrintDetails("\n");
      break;
    }
    case InitExprType::NullRef:
      PrintDetails(" - init null\n");
      break;
      break;
  }
}

static void InitExprToConstOffset(const InitExpr& expr, uint64_t* out_offset) {
  if (expr.insts.size() == 1) {
    switch (expr.type) {
      case InitExprType::I32:
        *out_offset = expr.insts[0].imm.i32;
        break;
      case InitExprType::I64:
        *out_offset = expr.insts[0].imm.i64;
        break;
      default:
        break;
    }
  }
}

Result BinaryReaderObjdump::EndInitExpr() {
  if (reading_data_init_expr_) {
    reading_data_init_expr_ = false;
    InitExprToConstOffset(current_init_expr_, &data_offset_);
  } else if (reading_elem_init_expr_) {
    reading_elem_init_expr_ = false;
    InitExprToConstOffset(current_init_expr_, &elem_offset_);
  } else if (reading_global_init_expr_) {
    reading_global_init_expr_ = false;
    PrintInitExpr(current_init_expr_);
  } else {
    WABT_UNREACHABLE;
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::OnI32ConstExpr(uint32_t value) {
  if (ReadingInitExpr()) {
    current_init_expr_.type = InitExprType::I32;
    current_init_expr_.insts.back().imm.i32 = value;
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::OnI64ConstExpr(uint64_t value) {
  if (ReadingInitExpr()) {
    current_init_expr_.type = InitExprType::I64;
    current_init_expr_.insts.back().imm.i64 = value;
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::OnF32ConstExpr(uint32_t value) {
  if (ReadingInitExpr()) {
    current_init_expr_.type = InitExprType::F32;
    current_init_expr_.insts.back().imm.f32 = value;
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::OnF64ConstExpr(uint64_t value) {
  if (ReadingInitExpr()) {
    current_init_expr_.type = InitExprType::F64;
    current_init_expr_.insts.back().imm.f64 = value;
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::OnOpcode(Opcode opcode) {
  BinaryReaderObjdumpBase::OnOpcode(opcode);
  if (ReadingInitExpr() && opcode != Opcode::End) {
    InitInst i;
    i.opcode = current_opcode;
    current_init_expr_.insts.push_back(i);
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::OnGlobalGetExpr(Index global_index) {
  if (ReadingInitExpr()) {
    current_init_expr_.type = InitExprType::Global;
    current_init_expr_.insts.back().imm.index = global_index;
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::OnModuleName(std::string_view name) {
  PrintDetails(" - module <" PRIstringview ">\n",
               WABT_PRINTF_STRING_VIEW_ARG(name));
  return Result::Ok;
}

Result BinaryReaderObjdump::OnFunctionName(Index index, std::string_view name) {
  PrintDetails(" - func[%" PRIindex "] <" PRIstringview ">\n", index,
               WABT_PRINTF_STRING_VIEW_ARG(name));
  return Result::Ok;
}

Result BinaryReaderObjdump::OnNameEntry(NameSectionSubsection type,
                                        Index index,
                                        std::string_view name) {
  PrintDetails(" - %s[%" PRIindex "] <" PRIstringview ">\n",
               GetNameSectionSubsectionName(type), index,
               WABT_PRINTF_STRING_VIEW_ARG(name));
  return Result::Ok;
}

Result BinaryReaderObjdump::OnLocalName(Index func_index,
                                        Index local_index,
                                        std::string_view name) {
  if (!name.empty()) {
    PrintDetails(" - func[%" PRIindex "] local[%" PRIindex "] <" PRIstringview
                 ">\n",
                 func_index, local_index, WABT_PRINTF_STRING_VIEW_ARG(name));
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::OnDataSegmentCount(Index count) {
  return OnCount(count);
}

Result BinaryReaderObjdump::BeginDataSegment(Index index,
                                             Index memory_index,
                                             uint8_t flags) {
  data_mem_index_ = memory_index;
  data_flags_ = flags;
  return Result::Ok;
}

Result BinaryReaderObjdump::OnDataSegmentData(Index index,
                                              const void* src_data,
                                              Address size) {
  if (!ShouldPrintDetails()) {
    return Result::Ok;
  }

  PrintDetails(" - segment[%" PRIindex "]", index);
  auto name = GetSegmentName(index);
  if (!name.empty()) {
    PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
  }
  if (data_flags_ & SegPassive) {
    PrintDetails(" passive");
  } else {
    PrintDetails(" memory=%" PRIindex, data_mem_index_);
  }
  PrintDetails(" size=%" PRIaddress, size);
  if (data_flags_ & SegPassive) {
    PrintDetails("\n");
  } else {
    PrintInitExpr(current_init_expr_, /*as_unsigned=*/true);
  }

  out_stream_->WriteMemoryDump(src_data, size, data_offset_, PrintChars::Yes,
                               "  - ");

  // Print relocations from this segment.
  if (!options_->relocs) {
    return Result::Ok;
  }

  Offset data_start = GetSectionStart(BinarySection::Data);
  Offset segment_start = state->offset - size;
  Offset segment_offset = segment_start - data_start;
  while (next_data_reloc_ < objdump_state_->data_relocations.size()) {
    const Reloc& reloc = objdump_state_->data_relocations[next_data_reloc_];
    Offset abs_offset = data_start + reloc.offset;
    if (abs_offset > state->offset) {
      break;
    }
    PrintRelocation(reloc, reloc.offset - segment_offset + data_offset_);
    next_data_reloc_++;
  }

  return Result::Ok;
}

Result BinaryReaderObjdump::OnDylinkInfo(uint32_t mem_size,
                                         uint32_t mem_align_log2,
                                         uint32_t table_size,
                                         uint32_t table_align_log2) {
  PrintDetails(" - mem_size     : %u\n", mem_size);
  PrintDetails(" - mem_p2align  : %u\n", mem_align_log2);
  PrintDetails(" - table_size   : %u\n", table_size);
  PrintDetails(" - table_p2align: %u\n", table_align_log2);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnDylinkNeededCount(Index count) {
  if (count) {
    PrintDetails(" - needed_dynlibs[%u]:\n", count);
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::OnDylinkImportCount(Index count) {
  PrintDetails(" - imports[%u]:\n", count);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnDylinkExportCount(Index count) {
  PrintDetails(" - exports[%u]:\n", count);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnDylinkExport(std::string_view name,
                                           uint32_t flags) {
  PrintDetails("  - " PRIstringview, WABT_PRINTF_STRING_VIEW_ARG(name));
  return PrintSymbolFlags(flags);
}

Result BinaryReaderObjdump::OnDylinkImport(std::string_view module,
                                           std::string_view name,
                                           uint32_t flags) {
  PrintDetails("  - " PRIstringview "." PRIstringview,
               WABT_PRINTF_STRING_VIEW_ARG(module),
               WABT_PRINTF_STRING_VIEW_ARG(name));
  return PrintSymbolFlags(flags);
}

Result BinaryReaderObjdump::OnDylinkNeeded(std::string_view so_name) {
  PrintDetails("  - " PRIstringview "\n", WABT_PRINTF_STRING_VIEW_ARG(so_name));
  return Result::Ok;
}

Result BinaryReaderObjdump::OnRelocCount(Index count, Index section_index) {
  BinaryReaderObjdumpBase::OnRelocCount(count, section_index);
  PrintDetails("  - relocations for section: %d (" PRIstringview ") [%d]\n",
               section_index,
               WABT_PRINTF_STRING_VIEW_ARG(GetSectionName(section_index)),
               count);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnReloc(RelocType type,
                                    Offset offset,
                                    Index index,
                                    uint32_t addend) {
  Offset total_offset = GetSectionStart(reloc_section_) + offset;
  PrintDetails("   - %-18s offset=%#08" PRIoffset "(file=%#08" PRIoffset ") ",
               GetRelocTypeName(type), offset, total_offset);
  if (type == RelocType::TypeIndexLEB) {
    PrintDetails("type=%" PRIindex, index);
  } else {
    PrintDetails("symbol=%" PRIindex " <" PRIstringview ">", index,
                 WABT_PRINTF_STRING_VIEW_ARG(GetSymbolName(index)));
  }

  if (addend) {
    int32_t signed_addend = static_cast<int32_t>(addend);
    if (signed_addend < 0) {
      PrintDetails("-");
      signed_addend = -signed_addend;
    } else {
      PrintDetails("+");
    }
    PrintDetails("%#x", signed_addend);
  }
  PrintDetails("\n");
  return Result::Ok;
}

Result BinaryReaderObjdump::OnFeature(uint8_t prefix, std::string_view name) {
  PrintDetails("  - [%c] " PRIstringview "\n", prefix,
               WABT_PRINTF_STRING_VIEW_ARG(name));
  return Result::Ok;
}

Result BinaryReaderObjdump::OnSymbolCount(Index count) {
  PrintDetails("  - symbol table [count=%d]\n", count);
  return Result::Ok;
}

Result BinaryReaderObjdump::PrintSymbolFlags(uint32_t flags) {
  if (flags > WABT_SYMBOL_FLAG_MAX) {
    err_stream_->Writef("Unknown symbols flags: %x\n", flags);
    return Result::Error;
  }

  const char* binding_name = nullptr;
  SymbolBinding binding =
      static_cast<SymbolBinding>(flags & WABT_SYMBOL_MASK_BINDING);
  switch (binding) {
    case SymbolBinding::Global:
      binding_name = "global";
      break;
    case SymbolBinding::Local:
      binding_name = "local";
      break;
    case SymbolBinding::Weak:
      binding_name = "weak";
      break;
  }
  flags &= ~WABT_SYMBOL_MASK_BINDING;

  const char* vis_name = nullptr;
  SymbolVisibility vis =
      static_cast<SymbolVisibility>(flags & WABT_SYMBOL_MASK_VISIBILITY);
  switch (vis) {
    case SymbolVisibility::Hidden:
      vis_name = "hidden";
      break;
    case SymbolVisibility::Default:
      vis_name = "default";
      break;
  }
  flags &= ~WABT_SYMBOL_MASK_VISIBILITY;

  PrintDetails(" [");
  if (flags & WABT_SYMBOL_FLAG_UNDEFINED) {
    PrintDetails(" undefined");
    flags &= ~WABT_SYMBOL_FLAG_UNDEFINED;
  }
  if (flags & WABT_SYMBOL_FLAG_EXPORTED) {
    PrintDetails(" exported");
    flags &= ~WABT_SYMBOL_FLAG_EXPORTED;
  }
  if (flags & WABT_SYMBOL_FLAG_EXPLICIT_NAME) {
    PrintDetails(" explicit_name");
    flags &= ~WABT_SYMBOL_FLAG_EXPLICIT_NAME;
  }
  if (flags & WABT_SYMBOL_FLAG_NO_STRIP) {
    PrintDetails(" no_strip");
    flags &= ~WABT_SYMBOL_FLAG_NO_STRIP;
  }
  if (flags & WABT_SYMBOL_FLAG_TLS) {
    PrintDetails(" tls");
    flags &= ~WABT_SYMBOL_FLAG_TLS;
  }
  if (flags != 0) {
    PrintDetails(" unknown_flags=%#x", flags);
  }
  PrintDetails(" binding=%s vis=%s ]\n", binding_name, vis_name);
  return Result::Ok;
}

Result BinaryReaderObjdump::PrintSegmentFlags(uint32_t flags) {
  if (flags > WABT_SYMBOL_FLAG_MAX) {
    err_stream_->Writef("Unknown symbols flags: %x\n", flags);
    return Result::Error;
  }
  PrintDetails(" [");
  if (flags & WABT_SEGMENT_FLAG_STRINGS) {
    PrintDetails(" STRINGS");
    flags &= ~WABT_SEGMENT_FLAG_STRINGS;
  }
  if (flags & WABT_SEGMENT_FLAG_TLS) {
    PrintDetails(" TLS");
    flags &= ~WABT_SEGMENT_FLAG_TLS;
  }
  if (flags != 0) {
    PrintDetails(" unknown_flags=%#x", flags);
  }
  PrintDetails(" ]\n");
  return Result::Ok;
}

Result BinaryReaderObjdump::OnDataSymbol(Index index,
                                         uint32_t flags,
                                         std::string_view name,
                                         Index segment,
                                         uint32_t offset,
                                         uint32_t size) {
  PrintDetails("   - %d: D <" PRIstringview ">", index,
               WABT_PRINTF_STRING_VIEW_ARG(name));
  if (!(flags & WABT_SYMBOL_FLAG_UNDEFINED))
    PrintDetails(" segment=%" PRIindex " offset=%d size=%d", segment, offset,
                 size);
  return PrintSymbolFlags(flags);
}

Result BinaryReaderObjdump::OnFunctionSymbol(Index index,
                                             uint32_t flags,
                                             std::string_view name,
                                             Index func_index) {
  if (name.empty()) {
    name = GetFunctionName(func_index);
  }
  PrintDetails("   - %d: F <" PRIstringview "> func=%" PRIindex, index,
               WABT_PRINTF_STRING_VIEW_ARG(name), func_index);
  return PrintSymbolFlags(flags);
}

Result BinaryReaderObjdump::OnGlobalSymbol(Index index,
                                           uint32_t flags,
                                           std::string_view name,
                                           Index global_index) {
  if (name.empty()) {
    name = GetGlobalName(global_index);
  }
  PrintDetails("   - %d: G <" PRIstringview "> global=%" PRIindex, index,
               WABT_PRINTF_STRING_VIEW_ARG(name), global_index);
  return PrintSymbolFlags(flags);
}

Result BinaryReaderObjdump::OnSectionSymbol(Index index,
                                            uint32_t flags,
                                            Index section_index) {
  auto sym_name = GetSectionName(section_index);
  assert(!sym_name.empty());
  PrintDetails("   - %d: S <" PRIstringview "> section=%" PRIindex, index,
               WABT_PRINTF_STRING_VIEW_ARG(sym_name), section_index);
  return PrintSymbolFlags(flags);
}

Result BinaryReaderObjdump::OnTagSymbol(Index index,
                                        uint32_t flags,
                                        std::string_view name,
                                        Index tag_index) {
  if (name.empty()) {
    name = GetTagName(tag_index);
  }
  PrintDetails("   - %d: E <" PRIstringview "> tag=%" PRIindex, index,
               WABT_PRINTF_STRING_VIEW_ARG(name), tag_index);
  return PrintSymbolFlags(flags);
}

Result BinaryReaderObjdump::OnTableSymbol(Index index,
                                          uint32_t flags,
                                          std::string_view name,
                                          Index table_index) {
  if (name.empty()) {
    name = GetTableName(table_index);
  }
  PrintDetails("   - %d: T <" PRIstringview "> table=%" PRIindex, index,
               WABT_PRINTF_STRING_VIEW_ARG(name), table_index);
  return PrintSymbolFlags(flags);
}

Result BinaryReaderObjdump::OnSegmentInfoCount(Index count) {
  PrintDetails("  - segment info [count=%d]\n", count);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnSegmentInfo(Index index,
                                          std::string_view name,
                                          Address alignment_log2,
                                          uint32_t flags) {
  PrintDetails("   - %d: " PRIstringview " p2align=%" PRIaddress, index,
               WABT_PRINTF_STRING_VIEW_ARG(name), alignment_log2);
  return PrintSegmentFlags(flags);
}

Result BinaryReaderObjdump::OnInitFunctionCount(Index count) {
  PrintDetails("  - init functions [count=%d]\n", count);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnInitFunction(uint32_t priority,
                                           Index symbol_index) {
  PrintDetails("   - %d: priority=%d", symbol_index, priority);
  auto name = GetSymbolName(symbol_index);
  if (!name.empty()) {
    PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
  }
  PrintDetails("\n");
  return Result::Ok;
}

Result BinaryReaderObjdump::OnComdatCount(Index count) {
  PrintDetails("  - comdat groups [count=%d]\n", count);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnComdatBegin(std::string_view name,
                                          uint32_t flags,
                                          Index count) {
  PrintDetails("   - " PRIstringview ": [count=%d]\n",
               WABT_PRINTF_STRING_VIEW_ARG(name), count);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnComdatEntry(ComdatType kind, Index index) {
  switch (kind) {
    case ComdatType::Data: {
      PrintDetails("    - segment[%" PRIindex "]", index);
      auto name = GetSegmentName(index);
      if (!name.empty()) {
        PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
      }
      break;
    }
    case ComdatType::Function: {
      PrintDetails("    - func[%" PRIindex "]", index);
      auto name = GetFunctionName(index);
      if (!name.empty()) {
        PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
      }
      break;
    }
  }
  PrintDetails("\n");
  return Result::Ok;
}

Result BinaryReaderObjdump::OnTagCount(Index count) {
  return OnCount(count);
}

Result BinaryReaderObjdump::OnTagType(Index index, Index sig_index) {
  if (!ShouldPrintDetails()) {
    return Result::Ok;
  }
  printf(" - tag[%" PRIindex "] sig=%" PRIindex "\n", index, sig_index);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnCodeMetadataCount(Index function_index,
                                                Index count) {
  if (!ShouldPrintDetails()) {
    return Result::Ok;
  }
  printf("   - func[%" PRIindex "]", function_index);
  auto name = GetFunctionName(function_index);
  if (!name.empty()) {
    printf(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
  }
  printf(":\n");
  return Result::Ok;
}
Result BinaryReaderObjdump::OnCodeMetadata(Offset code_offset,
                                           const void* data,
                                           Address size) {
  if (!ShouldPrintDetails()) {
    return Result::Ok;
  }
  printf("    - meta[%" PRIzx "]:\n", code_offset);

  out_stream_->WriteMemoryDump(data, size, 0, PrintChars::Yes, "     - ");
  return Result::Ok;
}

}  // end anonymous namespace

std::string_view ObjdumpNames::Get(Index index) const {
  auto iter = names.find(index);
  if (iter == names.end())
    return std::string_view();
  return iter->second;
}

void ObjdumpNames::Set(Index index, std::string_view name) {
  names[index] = std::string(name);
}

std::string_view ObjdumpLocalNames::Get(Index function_index,
                                        Index local_index) const {
  auto iter = names.find(std::pair<Index, Index>(function_index, local_index));
  if (iter == names.end())
    return std::string_view();
  return iter->second;
}

void ObjdumpLocalNames::Set(Index function_index,
                            Index local_index,
                            std::string_view name) {
  names[std::pair<Index, Index>(function_index, local_index)] =
      std::string(name);
}

Result ReadBinaryObjdump(const uint8_t* data,
                         size_t size,
                         ObjdumpOptions* options,
                         ObjdumpState* state) {
  Features features;
  features.EnableAll();
  const bool kReadDebugNames = true;
  const bool kStopOnFirstError = false;
  const bool kFailOnCustomSectionError = false;
  ReadBinaryOptions read_options(features, options->log_stream, kReadDebugNames,
                                 kStopOnFirstError, kFailOnCustomSectionError);

  switch (options->mode) {
    case ObjdumpMode::Prepass: {
      read_options.skip_function_bodies = true;
      BinaryReaderObjdumpPrepass reader(data, size, options, state);
      return ReadBinary(data, size, &reader, read_options);
    }
    case ObjdumpMode::Disassemble: {
      BinaryReaderObjdumpDisassemble reader(data, size, options, state);
      return ReadBinary(data, size, &reader, read_options);
    }
    default: {
      read_options.skip_function_bodies = true;
      BinaryReaderObjdump reader(data, size, options, state);
      return ReadBinary(data, size, &reader, read_options);
    }
  }
}

}  // namespace wabt
