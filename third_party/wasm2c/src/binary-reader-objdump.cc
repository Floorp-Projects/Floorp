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

#include "src/binary-reader-objdump.h"

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <vector>

#if HAVE_STRCASECMP
#include <strings.h>
#endif

#include "src/binary-reader-nop.h"
#include "src/filenames.h"
#include "src/literal.h"

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

  Result OnRelocCount(Index count, Index section_index) override;

 protected:
  string_view GetFunctionName(Index index) const;
  string_view GetGlobalName(Index index) const;
  string_view GetSectionName(Index index) const;
  string_view GetEventName(Index index) const;
  string_view GetSymbolName(Index index) const;
  string_view GetSegmentName(Index index) const;
  string_view GetTableName(Index index) const;
  void PrintRelocation(const Reloc& reloc, Offset offset) const;
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
      string_view basename = GetBasename(options_->filename);
      if (basename == "-") {
        basename = "<stdin>";
      }
      printf("%s:\tfile format wasm %#x\n", basename.to_string().c_str(),
             version);
      break;
    }
    case ObjdumpMode::RawData:
      break;
  }

  return Result::Ok;
}

string_view BinaryReaderObjdumpBase::GetFunctionName(Index index) const {
  return objdump_state_->function_names.Get(index);
}

string_view BinaryReaderObjdumpBase::GetGlobalName(Index index) const {
  return objdump_state_->global_names.Get(index);
}

string_view BinaryReaderObjdumpBase::GetSectionName(Index index) const {
  return objdump_state_->section_names.Get(index);
}

string_view BinaryReaderObjdumpBase::GetEventName(Index index) const {
  return objdump_state_->event_names.Get(index);
}

string_view BinaryReaderObjdumpBase::GetSegmentName(Index index) const {
  return objdump_state_->segment_names.Get(index);
}

string_view BinaryReaderObjdumpBase::GetTableName(Index index) const {
  return objdump_state_->table_names.Get(index);
}

string_view BinaryReaderObjdumpBase::GetSymbolName(Index symbol_index) const {
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
    case SymbolType::Event:
      return GetEventName(sym.index);
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
                            string_view section_name) override {
    objdump_state_->section_names.Set(section_index, section_name);
    return Result::Ok;
  }

  Result OnFunctionName(Index index, string_view name) override {
    SetFunctionName(index, name);
    return Result::Ok;
  }

  Result OnNameEntry(NameSectionSubsection type,
                     Index index,
                     string_view name) override {
    switch (type) {
      // TODO(sbc): remove OnFunctionName in favor of just using
      // OnNameEntry so that this works
      /*
      case NameSectionSubsection::Function:
        SetFunctionName(index, name);
        break;
      */
      case NameSectionSubsection::Global:
        SetGlobalName(index, name);
        break;
      case NameSectionSubsection::Table:
        SetTableName(index, name);
        break;
      case NameSectionSubsection::DataSegment:
        SetSegmentName(index, name);
        break;
      default:
        break;
    }
    return Result::Ok;
  }

  Result OnSymbolCount(Index count) override {
    objdump_state_->symtab.resize(count);
    return Result::Ok;
  }

  Result OnDataSymbol(Index index,
                      uint32_t flags,
                      string_view name,
                      Index segment,
                      uint32_t offset,
                      uint32_t size) override {
    objdump_state_->symtab[index] = {SymbolType::Data, name.to_string(), 0};
    return Result::Ok;
  }

  Result OnFunctionSymbol(Index index,
                          uint32_t flags,
                          string_view name,
                          Index func_index) override {
    if (!name.empty()) {
      SetFunctionName(func_index, name);
    }
    objdump_state_->symtab[index] = {SymbolType::Function, name.to_string(),
                                     func_index};
    return Result::Ok;
  }

  Result OnGlobalSymbol(Index index,
                        uint32_t flags,
                        string_view name,
                        Index global_index) override {
    if (!name.empty()) {
      SetGlobalName(global_index, name);
    }
    objdump_state_->symtab[index] = {SymbolType::Global, name.to_string(),
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

  Result OnEventSymbol(Index index,
                        uint32_t flags,
                        string_view name,
                        Index event_index) override {
    if (!name.empty()) {
      SetEventName(event_index, name);
    }
    objdump_state_->symtab[index] = {SymbolType::Event, name.to_string(),
                                     event_index};
    return Result::Ok;
  }

  Result OnTableSymbol(Index index,
                       uint32_t flags,
                       string_view name,
                       Index table_index) override {
    if (!name.empty()) {
      SetTableName(table_index, name);
    }
    objdump_state_->symtab[index] = {SymbolType::Table, name.to_string(),
                                     table_index};
    return Result::Ok;
  }

  Result OnImportFunc(Index import_index,
                      string_view module_name,
                      string_view field_name,
                      Index func_index,
                      Index sig_index) override {
    SetFunctionName(func_index,
                    module_name.to_string() + "." + field_name.to_string());
    return Result::Ok;
  }

  Result OnImportEvent(Index import_index,
                       string_view module_name,
                       string_view field_name,
                       Index event_index,
                       Index sig_index) override {
    SetEventName(event_index,
                 module_name.to_string() + "." + field_name.to_string());
    return Result::Ok;
  }

  Result OnImportGlobal(Index import_index,
                        string_view module_name,
                        string_view field_name,
                        Index global_index,
                        Type type,
                        bool mutable_) override {
    SetGlobalName(global_index,
                  module_name.to_string() + "." + field_name.to_string());
    return Result::Ok;
  }

  Result OnImportTable(Index import_index,
                       string_view module_name,
                       string_view field_name,
                       Index table_index,
                       Type elem_type,
                       const Limits* elem_limits) override {
    SetTableName(table_index,
                 module_name.to_string() + "." + field_name.to_string());
    return Result::Ok;
  }

  Result OnExport(Index index,
                  ExternalKind kind,
                  Index item_index,
                  string_view name) override {
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

  Result OnModuleName(string_view name) override {
    if (options_->mode == ObjdumpMode::Prepass) {
      printf("module name: <" PRIstringview ">\n",
             WABT_PRINTF_STRING_VIEW_ARG(name));
    }
    return Result::Ok;
  }

  Result OnSegmentInfo(Index index,
                       string_view name,
                       Address alignment_log2,
                       uint32_t flags) override {
    SetSegmentName(index, name);
    return Result::Ok;
  }

 protected:
  void SetFunctionName(Index index, string_view name);
  void SetGlobalName(Index index, string_view name);
  void SetEventName(Index index, string_view name);
  void SetTableName(Index index, string_view name);
  void SetSegmentName(Index index, string_view name);
};

void BinaryReaderObjdumpPrepass::SetFunctionName(Index index,
                                                 string_view name) {
  objdump_state_->function_names.Set(index, name);
}

void BinaryReaderObjdumpPrepass::SetGlobalName(Index index, string_view name) {
  objdump_state_->global_names.Set(index, name);
}

void BinaryReaderObjdumpPrepass::SetEventName(Index index, string_view name) {
  objdump_state_->event_names.Set(index, name);
}

void BinaryReaderObjdumpPrepass::SetTableName(Index index, string_view name) {
  objdump_state_->table_names.Set(index, name);
}

void BinaryReaderObjdumpPrepass::SetSegmentName(Index index, string_view name) {
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

  Result OnLocalDeclCount(Index count) override;
  Result OnLocalDecl(Index decl_index, Index count, Type type) override;

  Result OnOpcode(Opcode Opcode) override;
  Result OnOpcodeBare() override;
  Result OnOpcodeIndex(Index value) override;
  Result OnOpcodeIndexIndex(Index value, Index value2) override;
  Result OnOpcodeUint32(uint32_t value) override;
  Result OnOpcodeUint32Uint32(uint32_t value, uint32_t value2) override;
  Result OnOpcodeUint32Uint32Uint32(uint32_t value, uint32_t value2, uint32_t value3) override;
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
  Result OnEndFunc() override;

 private:
  void LogOpcode(size_t data_size, const char* fmt, ...);

  Opcode current_opcode = Opcode::Unreachable;
  Offset current_opcode_offset = 0;
  Offset last_opcode_end = 0;
  int indent_level = 0;
  Index next_reloc = 0;
  Index local_index_ = 0;
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
  if (options_->debug) {
    const char* opcode_name = opcode.GetName();
    err_stream_->Writef("on_opcode: %#" PRIzx ": %s\n", state->offset,
                        opcode_name);
  }

  if (last_opcode_end) {
    if (state->offset != last_opcode_end + opcode.GetLength()) {
      Opcode missing_opcode = Opcode::FromCode(data_[last_opcode_end]);
      const char* opcode_name = missing_opcode.GetName();
      fprintf(stderr,
              "warning: %#" PRIzx " missing opcode callback at %#" PRIzx
              " (%#02x=%s)\n",
              state->offset, last_opcode_end + 1, data_[last_opcode_end],
              opcode_name);
      return Result::Error;
    }
  }

  current_opcode_offset = state->offset;
  current_opcode = opcode;
  return Result::Ok;
}

#define IMMEDIATE_OCTET_COUNT 9

Result BinaryReaderObjdumpDisassemble::OnLocalDeclCount(Index count) {
  local_index_ = 0;
  current_opcode_offset = state->offset;
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnLocalDecl(Index decl_index,
                                                   Index count,
                                                   Type type) {
  Offset offset = current_opcode_offset;
  size_t data_size = state->offset - offset;

  printf(" %06" PRIzx ":", offset);
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

  printf("] type=%s\n", type.GetName());

  last_opcode_end = current_opcode_offset + data_size;
  current_opcode_offset = last_opcode_end;

  return Result::Ok;
}

void BinaryReaderObjdumpDisassemble::LogOpcode(size_t data_size,
                                               const char* fmt,
                                               ...) {
  const Offset opcode_size = current_opcode.GetLength();
  const Offset total_size = opcode_size + data_size;
  // current_opcode_offset has already read past this opcode; rewind it by the
  // size of this opcode, which may be more than one byte.
  Offset offset = current_opcode_offset - opcode_size;
  const Offset offset_end = offset + total_size;

  bool first_line = true;
  while (offset < offset_end) {
    // Print bytes, but only display a maximum of IMMEDIATE_OCTET_COUNT on each
    // line.
    printf(" %06" PRIzx ":", offset);
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
        case Opcode::Unwind:
          indent_level--;
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

  last_opcode_end = offset_end;

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
  LogOpcode(0, nullptr);
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeIndex(Index value) {
  Offset immediate_len = state->offset - current_opcode_offset;
  string_view name;
  if (current_opcode == Opcode::Call &&
      !(name = GetFunctionName(value)).empty()) {
    LogOpcode(immediate_len, "%d <" PRIstringview ">", value,
              WABT_PRINTF_STRING_VIEW_ARG(name));
  } else if ((current_opcode == Opcode::GlobalGet ||
              current_opcode == Opcode::GlobalSet) &&
             !(name = GetGlobalName(value)).empty()) {
    LogOpcode(immediate_len, "%d <" PRIstringview ">", value,
              WABT_PRINTF_STRING_VIEW_ARG(name));
  } else {
    LogOpcode(immediate_len, "%d", value);
  }
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeIndexIndex(Index value,
                                                          Index value2) {
  Offset immediate_len = state->offset - current_opcode_offset;
  LogOpcode(immediate_len, "%" PRIindex " %" PRIindex, value, value2);
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeUint32(uint32_t value) {
  Offset immediate_len = state->offset - current_opcode_offset;
  string_view name;
  if (current_opcode == Opcode::DataDrop &&
      !(name = GetSegmentName(value)).empty()) {
    LogOpcode(immediate_len, "%d <" PRIstringview ">", value,
              WABT_PRINTF_STRING_VIEW_ARG(name));
  } else {
    LogOpcode(immediate_len, "%u", value);
  }
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeUint32Uint32(uint32_t value,
                                                            uint32_t value2) {
  Offset immediate_len = state->offset - current_opcode_offset;
  string_view name;
  if (current_opcode == Opcode::MemoryInit &&
      !(name = GetSegmentName(value)).empty()) {
    LogOpcode(immediate_len, "%u %u <" PRIstringview ">", value, value2,
              WABT_PRINTF_STRING_VIEW_ARG(name));
  } else {
    LogOpcode(immediate_len, "%u %u", value, value2);
  }
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeUint32Uint32Uint32(
    uint32_t value,
    uint32_t value2,
    uint32_t value3) {
  Offset immediate_len = state->offset - current_opcode_offset;
  LogOpcode(immediate_len, "%u %u %u", value, value2, value3);
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeUint64(uint64_t value) {
  Offset immediate_len = state->offset - current_opcode_offset;
  LogOpcode(immediate_len, "%" PRId64, value);
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeF32(uint32_t value) {
  Offset immediate_len = state->offset - current_opcode_offset;
  char buffer[WABT_MAX_FLOAT_HEX];
  WriteFloatHex(buffer, sizeof(buffer), value);
  LogOpcode(immediate_len, buffer);
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeF64(uint64_t value) {
  Offset immediate_len = state->offset - current_opcode_offset;
  char buffer[WABT_MAX_DOUBLE_HEX];
  WriteDoubleHex(buffer, sizeof(buffer), value);
  LogOpcode(immediate_len, buffer);
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeV128(v128 value) {
  Offset immediate_len = state->offset - current_opcode_offset;
  // v128 is always dumped as i32x4:
  LogOpcode(immediate_len, "0x%08x 0x%08x 0x%08x 0x%08x", value.u32(0),
            value.u32(1), value.u32(2), value.u32(3));
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeType(Type type) {
  Offset immediate_len = state->offset - current_opcode_offset;
  LogOpcode(immediate_len, type.GetRefKindName());
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnBrTableExpr(
    Index num_targets,
    Index* target_depths,
    Index default_target_depth) {
  Offset immediate_len = state->offset - current_opcode_offset;

  std::string buffer = std::string();
  for (Index i = 0; i < num_targets; i++) {
    buffer.append(std::to_string(target_depths[i])).append(" ");
  }
  buffer.append(std::to_string(default_target_depth));

  LogOpcode(immediate_len, "%s", buffer.c_str());
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnDelegateExpr(Index depth) {
  // Because `delegate` ends the block we need to dedent here, and
  // we don't need to dedent it in LogOpcode.
  if (indent_level > 0) {
    indent_level--;
  }
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnEndFunc() {
  LogOpcode(0, nullptr);
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnEndExpr() {
  if (indent_level > 0) {
    indent_level--;
  }
  LogOpcode(0, nullptr);
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::BeginFunctionBody(Index index,
                                                         Offset size) {
  printf("%06" PRIzx " func[%" PRIindex "]", state->offset, index);
  auto name = GetFunctionName(index);
  if (!name.empty()) {
    printf(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
  }
  printf(":\n");

  last_opcode_end = 0;
  return Result::Ok;
}

Result BinaryReaderObjdumpDisassemble::OnOpcodeBlockSig(Type sig_type) {
  Offset immediate_len = state->offset - current_opcode_offset;
  if (sig_type != Type::Void) {
    LogOpcode(immediate_len, "%s", BlockSigToString(sig_type).c_str());
  } else {
    LogOpcode(immediate_len, nullptr);
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

struct InitExpr {
  InitExprType type;
  union {
    Index index;
    uint32_t i32;
    uint32_t f32;
    uint64_t i64;
    uint64_t f64;
    v128 v128_v;
    Type type;
  } value;
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
                            string_view section_name) override;

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
                      string_view module_name,
                      string_view field_name,
                      Index func_index,
                      Index sig_index) override;
  Result OnImportTable(Index import_index,
                       string_view module_name,
                       string_view field_name,
                       Index table_index,
                       Type elem_type,
                       const Limits* elem_limits) override;
  Result OnImportMemory(Index import_index,
                        string_view module_name,
                        string_view field_name,
                        Index memory_index,
                        const Limits* page_limits) override;
  Result OnImportGlobal(Index import_index,
                        string_view module_name,
                        string_view field_name,
                        Index global_index,
                        Type type,
                        bool mutable_) override;
  Result OnImportEvent(Index import_index,
                       string_view module_name,
                       string_view field_name,
                       Index event_index,
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
                  string_view name) override;

  Result OnStartFunction(Index func_index) override;
  Result OnDataCount(Index count) override;

  Result OnFunctionBodyCount(Index count) override;
  Result BeginFunctionBody(Index index, Offset size) override;

  Result BeginElemSection(Offset size) override {
    in_elem_section_ = true;
    return Result::Ok;
  }
  Result EndElemSection() override {
    in_elem_section_ = false;
    return Result::Ok;
  }

  Result OnElemSegmentCount(Index count) override;
  Result BeginElemSegment(Index index,
                          Index table_index,
                          uint8_t flags) override;
  Result OnElemSegmentElemType(Index index, Type elem_type) override;
  Result OnElemSegmentElemExprCount(Index index, Index count) override;
  Result OnElemSegmentElemExpr_RefNull(Index segment_index, Type type) override;
  Result OnElemSegmentElemExpr_RefFunc(Index segment_index,
                                       Index func_index) override;

  Result BeginDataSection(Offset size) override {
    in_data_section_ = true;
    return Result::Ok;
  }
  Result EndDataSection() override {
    in_data_section_ = false;
    return Result::Ok;
  }

  Result OnDataSegmentCount(Index count) override;
  Result BeginDataSegment(Index index,
                          Index memory_index,
                          uint8_t flags) override;
  Result OnDataSegmentData(Index index,
                           const void* data,
                           Address size) override;

  Result OnModuleName(string_view name) override;
  Result OnFunctionName(Index function_index,
                        string_view function_name) override;
  Result OnLocalName(Index function_index,
                     Index local_index,
                     string_view local_name) override;
  Result OnNameEntry(NameSectionSubsection type,
                     Index index,
                     string_view name) override;

  Result OnInitExprF32ConstExpr(Index index, uint32_t value) override;
  Result OnInitExprF64ConstExpr(Index index, uint64_t value) override;
  Result OnInitExprV128ConstExpr(Index index, v128 value) override;
  Result OnInitExprGlobalGetExpr(Index index, Index global_index) override;
  Result OnInitExprI32ConstExpr(Index index, uint32_t value) override;
  Result OnInitExprI64ConstExpr(Index index, uint64_t value) override;
  Result OnInitExprRefNull(Index index, Type type) override;
  Result OnInitExprRefFunc(Index index, Index func_index) override;

  Result OnDylinkInfo(uint32_t mem_size,
                      uint32_t mem_align_log2,
                      uint32_t table_size,
                      uint32_t table_align_log2) override;
  Result OnDylinkNeededCount(Index count) override;
  Result OnDylinkNeeded(string_view so_name) override;

  Result OnRelocCount(Index count, Index section_index) override;
  Result OnReloc(RelocType type,
                 Offset offset,
                 Index index,
                 uint32_t addend) override;

  Result OnSymbolCount(Index count) override;
  Result OnDataSymbol(Index index,
                      uint32_t flags,
                      string_view name,
                      Index segment,
                      uint32_t offset,
                      uint32_t size) override;
  Result OnFunctionSymbol(Index index,
                          uint32_t flags,
                          string_view name,
                          Index func_index) override;
  Result OnGlobalSymbol(Index index,
                        uint32_t flags,
                        string_view name,
                        Index global_index) override;
  Result OnSectionSymbol(Index index,
                         uint32_t flags,
                         Index section_index) override;
  Result OnEventSymbol(Index index,
                       uint32_t flags,
                       string_view name,
                       Index event_index) override;
  Result OnTableSymbol(Index index,
                       uint32_t flags,
                       string_view name,
                       Index table_index) override;
  Result OnSegmentInfoCount(Index count) override;
  Result OnSegmentInfo(Index index,
                       string_view name,
                       Address alignment_log2,
                       uint32_t flags) override;
  Result OnInitFunctionCount(Index count) override;
  Result OnInitFunction(uint32_t priority, Index function_index) override;
  Result OnComdatCount(Index count) override;
  Result OnComdatBegin(string_view name, uint32_t flags, Index count) override;
  Result OnComdatEntry(ComdatType kind, Index index) override;
  Result EndLinkingSection() override { return Result::Ok; }

  Result OnEventCount(Index count) override;
  Result OnEventType(Index index, Index sig_index) override;

 private:
  Result InitExprToConstOffset(const InitExpr& expr, uint32_t* out_offset);
  Result HandleInitExpr(const InitExpr& expr);
  bool ShouldPrintDetails();
  void PrintDetails(const char* fmt, ...);
  Result PrintSymbolFlags(uint32_t flags);
  void PrintInitExpr(const InitExpr& expr);
  Result OnCount(Index count);

  std::unique_ptr<FileStream> out_stream_;
  Index elem_index_ = 0;
  Index table_index_ = 0;
  Index next_data_reloc_ = 0;
  bool in_data_section_ = false;
  bool in_elem_section_ = false;
  InitExpr data_init_expr_;
  InitExpr elem_init_expr_;
  uint8_t data_flags_ = 0;
  uint8_t elem_flags_ = 0;
  Index data_mem_index_ = 0;
  uint32_t data_offset_ = 0;
  uint32_t elem_offset_ = 0;
};

BinaryReaderObjdump::BinaryReaderObjdump(const uint8_t* data,
                                         size_t size,
                                         ObjdumpOptions* options,
                                         ObjdumpState* objdump_state)
    : BinaryReaderObjdumpBase(data, size, options, objdump_state),
      out_stream_(FileStream::CreateStdout()) {}

Result BinaryReaderObjdump::BeginCustomSection(Index section_index,
                                               Offset size,
                                               string_view section_name) {
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
  std::string match_name = GetSectionName(section_index).to_string();

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

  if (options_->relocs) {
    if (next_data_reloc_ != objdump_state_->data_relocations.size()) {
      err_stream_->Writef("Data reloctions outside of segments\n");
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
    printf("%s", param_types[i].GetName());
  }
  printf(") -> ");
  switch (result_count) {
    case 0:
      printf("nil");
      break;
    case 1:
      printf("%s", result_types[0].GetName());
      break;
    default:
      printf("(");
      for (Index i = 0; i < result_count; i++) {
        if (i != 0) {
          printf(", ");
        }
        printf("%s", result_types[i].GetName());
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
    printf(" %s", fields[i].type.GetName());
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
  printf(" %s", field.type.GetName());
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
                                         string_view module_name,
                                         string_view field_name,
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
                                          string_view module_name,
                                          string_view field_name,
                                          Index table_index,
                                          Type elem_type,
                                          const Limits* elem_limits) {
  PrintDetails(" - table[%" PRIindex "] type=%s initial=%" PRId64, table_index,
               elem_type.GetName(), elem_limits->initial);
  if (elem_limits->has_max) {
    PrintDetails(" max=%" PRId64, elem_limits->max);
  }
  PrintDetails(" <- " PRIstringview "." PRIstringview "\n",
               WABT_PRINTF_STRING_VIEW_ARG(module_name),
               WABT_PRINTF_STRING_VIEW_ARG(field_name));
  return Result::Ok;
}

Result BinaryReaderObjdump::OnImportMemory(Index import_index,
                                           string_view module_name,
                                           string_view field_name,
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
                                           string_view module_name,
                                           string_view field_name,
                                           Index global_index,
                                           Type type,
                                           bool mutable_) {
  PrintDetails(" - global[%" PRIindex "] %s mutable=%d", global_index,
               type.GetName(), mutable_);
  PrintDetails(" <- " PRIstringview "." PRIstringview "\n",
               WABT_PRINTF_STRING_VIEW_ARG(module_name),
               WABT_PRINTF_STRING_VIEW_ARG(field_name));
  return Result::Ok;
}

Result BinaryReaderObjdump::OnImportEvent(Index import_index,
                                          string_view module_name,
                                          string_view field_name,
                                          Index event_index,
                                          Index sig_index) {
  PrintDetails(" - event[%" PRIindex "] sig=%" PRIindex, event_index,
               sig_index);
  auto name = GetEventName(event_index);
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
               elem_type.GetName(), elem_limits->initial);
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
                                     string_view name) {
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
  PrintDetails("  - elem[%" PRIindex "] = ref.null %s\n",
               elem_offset_ + elem_index_, type.GetName());
  elem_index_++;
  return Result::Ok;
}

Result BinaryReaderObjdump::OnElemSegmentElemExpr_RefFunc(Index segment_index,
                                                          Index func_index) {
  PrintDetails("  - elem[%" PRIindex "] = func[%" PRIindex "]",
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
    PrintInitExpr(elem_init_expr_);
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::OnGlobalCount(Index count) {
  return OnCount(count);
}

Result BinaryReaderObjdump::BeginGlobal(Index index, Type type, bool mutable_) {
  PrintDetails(" - global[%" PRIindex "] %s mutable=%d", index, type.GetName(),
               mutable_);
  string_view name = GetGlobalName(index);
  if (!name.empty()) {
    PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
  }
  return Result::Ok;
}

void BinaryReaderObjdump::PrintInitExpr(const InitExpr& expr) {
  switch (expr.type) {
    case InitExprType::I32:
      PrintDetails(" - init i32=%d\n", expr.value.i32);
      break;
    case InitExprType::I64:
      PrintDetails(" - init i64=%" PRId64 "\n", expr.value.i64);
      break;
    case InitExprType::F64: {
      char buffer[WABT_MAX_DOUBLE_HEX];
      WriteDoubleHex(buffer, sizeof(buffer), expr.value.f64);
      PrintDetails(" - init f64=%s\n", buffer);
      break;
    }
    case InitExprType::F32: {
      char buffer[WABT_MAX_FLOAT_HEX];
      WriteFloatHex(buffer, sizeof(buffer), expr.value.f32);
      PrintDetails(" - init f32=%s\n", buffer);
      break;
    }
    case InitExprType::V128: {
      PrintDetails(" - init v128=0x%08x 0x%08x 0x%08x 0x%08x \n",
                   expr.value.v128_v.u32(0), expr.value.v128_v.u32(1),
                   expr.value.v128_v.u32(2), expr.value.v128_v.u32(3));
      break;
    }
    case InitExprType::Global: {
      PrintDetails(" - init global=%" PRIindex, expr.value.index);
      string_view name = GetGlobalName(expr.value.index);
      if (!name.empty()) {
        PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
      }
      PrintDetails("\n");
      break;
    }
    case InitExprType::FuncRef: {
      PrintDetails(" - init ref.func:%" PRIindex, expr.value.index);
      string_view name = GetFunctionName(expr.value.index);
      if (!name.empty()) {
        PrintDetails(" <" PRIstringview ">", WABT_PRINTF_STRING_VIEW_ARG(name));
      }
      PrintDetails("\n");
      break;
    }
    case InitExprType::NullRef: {
      PrintDetails(" - init null\n");
      break;
    }
  }
}

Result BinaryReaderObjdump::InitExprToConstOffset(const InitExpr& expr,
                                                  uint32_t* out_offset) {
  switch (expr.type) {
    case InitExprType::I32:
      *out_offset = expr.value.i32;
      break;
    case InitExprType::Global:
      *out_offset = 0;
      break;
    case InitExprType::I64:
    case InitExprType::F32:
    case InitExprType::F64:
    case InitExprType::V128:
    case InitExprType::FuncRef:
    case InitExprType::NullRef:
      err_stream_->Writef("Segment/Elem offset must be an i32 init expr");
      return Result::Error;
      break;
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::HandleInitExpr(const InitExpr& expr) {
  if (in_data_section_) {
    data_init_expr_ = expr;
    return InitExprToConstOffset(expr, &data_offset_);
  } else if (in_elem_section_) {
    elem_init_expr_ = expr;
    return InitExprToConstOffset(expr, &elem_offset_);
  } else {
    PrintInitExpr(expr);
  }
  return Result::Ok;
}

Result BinaryReaderObjdump::OnInitExprF32ConstExpr(Index index,
                                                   uint32_t value) {
  InitExpr expr;
  expr.type = InitExprType::F32;
  expr.value.f32 = value;
  HandleInitExpr(expr);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnInitExprF64ConstExpr(Index index,
                                                   uint64_t value) {
  InitExpr expr;
  expr.type = InitExprType::F64;
  expr.value.f64 = value;
  HandleInitExpr(expr);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnInitExprV128ConstExpr(Index index, v128 value) {
  InitExpr expr;
  expr.type = InitExprType::V128;
  expr.value.v128_v = value;
  HandleInitExpr(expr);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnInitExprGlobalGetExpr(Index index,
                                                    Index global_index) {
  InitExpr expr;
  expr.type = InitExprType::Global;
  expr.value.index = global_index;
  HandleInitExpr(expr);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnInitExprI32ConstExpr(Index index,
                                                   uint32_t value) {
  InitExpr expr;
  expr.type = InitExprType::I32;
  expr.value.i32 = value;
  HandleInitExpr(expr);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnInitExprI64ConstExpr(Index index,
                                                   uint64_t value) {
  InitExpr expr;
  expr.type = InitExprType::I64;
  expr.value.i64 = value;
  HandleInitExpr(expr);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnInitExprRefNull(Index index, Type type) {
  InitExpr expr;
  expr.type = InitExprType::NullRef;
  expr.value.type = type;
  HandleInitExpr(expr);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnInitExprRefFunc(Index index, Index func_index) {
  InitExpr expr{InitExprType::FuncRef, {func_index}};
  HandleInitExpr(expr);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnModuleName(string_view name) {
  PrintDetails(" - module <" PRIstringview ">\n",
               WABT_PRINTF_STRING_VIEW_ARG(name));
  return Result::Ok;
}

Result BinaryReaderObjdump::OnFunctionName(Index index, string_view name) {
  PrintDetails(" - func[%" PRIindex "] <" PRIstringview ">\n", index,
               WABT_PRINTF_STRING_VIEW_ARG(name));
  return Result::Ok;
}

Result BinaryReaderObjdump::OnNameEntry(NameSectionSubsection type,
                                        Index index,
                                        string_view name) {
  PrintDetails(" - %s[%" PRIindex "] <" PRIstringview ">\n",
               GetNameSectionSubsectionName(type), index,
               WABT_PRINTF_STRING_VIEW_ARG(name));
  return Result::Ok;
}

Result BinaryReaderObjdump::OnLocalName(Index func_index,
                                        Index local_index,
                                        string_view name) {
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
    PrintInitExpr(data_init_expr_);
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

Result BinaryReaderObjdump::OnDylinkNeeded(string_view so_name) {
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
  if (flags & WABT_SYMBOL_FLAG_UNDEFINED)
    PrintDetails(" undefined");
  if (flags & WABT_SYMBOL_FLAG_EXPORTED)
    PrintDetails(" exported");
  if (flags & WABT_SYMBOL_FLAG_EXPLICIT_NAME)
    PrintDetails(" explicit_name");
  if (flags & WABT_SYMBOL_FLAG_NO_STRIP)
    PrintDetails(" no_strip");

  PrintDetails(" binding=%s vis=%s\n", binding_name, vis_name);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnDataSymbol(Index index,
                                         uint32_t flags,
                                         string_view name,
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
                                             string_view name,
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
                                           string_view name,
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

Result BinaryReaderObjdump::OnEventSymbol(Index index,
                                          uint32_t flags,
                                          string_view name,
                                          Index event_index) {
  if (name.empty()) {
    name = GetEventName(event_index);
  }
  PrintDetails("   - %d: E <" PRIstringview "> event=%" PRIindex, index,
               WABT_PRINTF_STRING_VIEW_ARG(name), event_index);
  return PrintSymbolFlags(flags);
}

Result BinaryReaderObjdump::OnTableSymbol(Index index,
                                          uint32_t flags,
                                          string_view name,
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
                                          string_view name,
                                          Address alignment_log2,
                                          uint32_t flags) {
  PrintDetails("   - %d: " PRIstringview " p2align=%" PRIaddress " flags=%#x\n",
               index, WABT_PRINTF_STRING_VIEW_ARG(name), alignment_log2, flags);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnInitFunctionCount(Index count) {
  PrintDetails("  - init functions [count=%d]\n", count);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnInitFunction(uint32_t priority,
                                           Index function_index) {
  PrintDetails("   - %d: priority=%d\n", function_index, priority);
  return Result::Ok;
}
Result BinaryReaderObjdump::OnComdatCount(Index count) {
  PrintDetails("  - comdat groups [count=%d]\n", count);
  return Result::Ok;
}

Result BinaryReaderObjdump::OnComdatBegin(string_view name, uint32_t flags, Index count) {
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

Result BinaryReaderObjdump::OnEventCount(Index count) {
  return OnCount(count);
}

Result BinaryReaderObjdump::OnEventType(Index index, Index sig_index) {
  if (!ShouldPrintDetails()) {
    return Result::Ok;
  }
  printf(" - event[%" PRIindex "] sig=%" PRIindex "\n", index, sig_index);
  return Result::Ok;
}

}  // end anonymous namespace

string_view ObjdumpNames::Get(Index index) const {
  auto iter = names.find(index);
  if (iter == names.end())
    return string_view();
  return iter->second;
}

void ObjdumpNames::Set(Index index, string_view name) {
  names[index] = name.to_string();
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
      BinaryReaderObjdumpPrepass reader(data, size, options, state);
      return ReadBinary(data, size, &reader, read_options);
    }
    case ObjdumpMode::Disassemble: {
      BinaryReaderObjdumpDisassemble reader(data, size, options, state);
      return ReadBinary(data, size, &reader, read_options);
    }
    default: {
      BinaryReaderObjdump reader(data, size, options, state);
      return ReadBinary(data, size, &reader, read_options);
    }
  }
}

}  // namespace wabt
