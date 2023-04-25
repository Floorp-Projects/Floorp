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

#include "wabt/binary-reader-logging.h"

#include <cinttypes>

#include "wabt/stream.h"

namespace wabt {

#define INDENT_SIZE 2

#define LOGF_NOINDENT(...) stream_->Writef(__VA_ARGS__)

#define LOGF(...)               \
  do {                          \
    WriteIndent();              \
    LOGF_NOINDENT(__VA_ARGS__); \
  } while (0)

namespace {

void SPrintLimits(char* dst, size_t size, const Limits* limits) {
  int result;
  if (limits->has_max) {
    result = wabt_snprintf(dst, size, "initial: %" PRIu64 ", max: %" PRIu64,
                           limits->initial, limits->max);
  } else {
    result = wabt_snprintf(dst, size, "initial: %" PRIu64, limits->initial);
  }
  WABT_USE(result);
  assert(static_cast<size_t>(result) < size);
}

}  // end anonymous namespace

BinaryReaderLogging::BinaryReaderLogging(Stream* stream,
                                         BinaryReaderDelegate* forward)
    : stream_(stream), reader_(forward), indent_(0) {}

void BinaryReaderLogging::Indent() {
  indent_ += INDENT_SIZE;
}

void BinaryReaderLogging::Dedent() {
  indent_ -= INDENT_SIZE;
  assert(indent_ >= 0);
}

void BinaryReaderLogging::WriteIndent() {
  static char s_indent[] =
      "                                                                       "
      "                                                                       ";
  static const size_t s_indent_len = sizeof(s_indent) - 1;
  size_t i = indent_;
  while (i > s_indent_len) {
    stream_->WriteData(s_indent, s_indent_len);
    i -= s_indent_len;
  }
  if (i > 0) {
    stream_->WriteData(s_indent, indent_);
  }
}

void BinaryReaderLogging::LogType(Type type) {
  if (type.IsIndex()) {
    LOGF_NOINDENT("typeidx[%d]", type.GetIndex());
  } else {
    LOGF_NOINDENT("%s", type.GetName().c_str());
  }
}

void BinaryReaderLogging::LogTypes(Index type_count, Type* types) {
  LOGF_NOINDENT("[");
  for (Index i = 0; i < type_count; ++i) {
    LogType(types[i]);
    if (i != type_count - 1) {
      LOGF_NOINDENT(", ");
    }
  }
  LOGF_NOINDENT("]");
}

void BinaryReaderLogging::LogTypes(TypeVector& types) {
  LogTypes(types.size(), types.data());
}

void BinaryReaderLogging::LogField(TypeMut field) {
  if (field.mutable_) {
    LOGF_NOINDENT("(mut ");
  }
  LogType(field.type);
  if (field.mutable_) {
    LOGF_NOINDENT(")");
  }
}

bool BinaryReaderLogging::OnError(const Error& error) {
  return reader_->OnError(error);
}

void BinaryReaderLogging::OnSetState(const State* s) {
  BinaryReaderDelegate::OnSetState(s);
  reader_->OnSetState(s);
}

Result BinaryReaderLogging::BeginModule(uint32_t version) {
  LOGF("BeginModule(version: %u)\n", version);
  Indent();
  return reader_->BeginModule(version);
}

Result BinaryReaderLogging::BeginSection(Index section_index,
                                         BinarySection section_type,
                                         Offset size) {
  return reader_->BeginSection(section_index, section_type, size);
}

Result BinaryReaderLogging::BeginCustomSection(Index section_index,
                                               Offset size,
                                               std::string_view section_name) {
  LOGF("BeginCustomSection('" PRIstringview "', size: %" PRIzd ")\n",
       WABT_PRINTF_STRING_VIEW_ARG(section_name), size);
  Indent();
  return reader_->BeginCustomSection(section_index, size, section_name);
}

Result BinaryReaderLogging::OnFuncType(Index index,
                                       Index param_count,
                                       Type* param_types,
                                       Index result_count,
                                       Type* result_types) {
  LOGF("OnFuncType(index: %" PRIindex ", params: ", index);
  LogTypes(param_count, param_types);
  LOGF_NOINDENT(", results: ");
  LogTypes(result_count, result_types);
  LOGF_NOINDENT(")\n");
  return reader_->OnFuncType(index, param_count, param_types, result_count,
                             result_types);
}

Result BinaryReaderLogging::OnStructType(Index index,
                                         Index field_count,
                                         TypeMut* fields) {
  LOGF("OnStructType(index: %" PRIindex ", fields: ", index);
  LOGF_NOINDENT("[");
  for (Index i = 0; i < field_count; ++i) {
    LogField(fields[i]);
    if (i != field_count - 1) {
      LOGF_NOINDENT(", ");
    }
  }
  LOGF_NOINDENT("])\n");
  return reader_->OnStructType(index, field_count, fields);
}

Result BinaryReaderLogging::OnArrayType(Index index, TypeMut field) {
  LOGF("OnArrayType(index: %" PRIindex ", field: ", index);
  LogField(field);
  LOGF_NOINDENT(")\n");
  return reader_->OnArrayType(index, field);
}

Result BinaryReaderLogging::OnImport(Index index,
                                     ExternalKind kind,
                                     std::string_view module_name,
                                     std::string_view field_name) {
  LOGF("OnImport(index: %" PRIindex ", kind: %s, module: \"" PRIstringview
       "\", field: \"" PRIstringview "\")\n",
       index, GetKindName(kind), WABT_PRINTF_STRING_VIEW_ARG(module_name),
       WABT_PRINTF_STRING_VIEW_ARG(field_name));
  return reader_->OnImport(index, kind, module_name, field_name);
}

Result BinaryReaderLogging::OnImportFunc(Index import_index,
                                         std::string_view module_name,
                                         std::string_view field_name,
                                         Index func_index,
                                         Index sig_index) {
  LOGF("OnImportFunc(import_index: %" PRIindex ", func_index: %" PRIindex
       ", sig_index: %" PRIindex ")\n",
       import_index, func_index, sig_index);
  return reader_->OnImportFunc(import_index, module_name, field_name,
                               func_index, sig_index);
}

Result BinaryReaderLogging::OnImportTable(Index import_index,
                                          std::string_view module_name,
                                          std::string_view field_name,
                                          Index table_index,
                                          Type elem_type,
                                          const Limits* elem_limits) {
  char buf[100];
  SPrintLimits(buf, sizeof(buf), elem_limits);
  LOGF("OnImportTable(import_index: %" PRIindex ", table_index: %" PRIindex
       ", elem_type: %s, %s)\n",
       import_index, table_index, elem_type.GetName().c_str(), buf);
  return reader_->OnImportTable(import_index, module_name, field_name,
                                table_index, elem_type, elem_limits);
}

Result BinaryReaderLogging::OnImportMemory(Index import_index,
                                           std::string_view module_name,
                                           std::string_view field_name,
                                           Index memory_index,
                                           const Limits* page_limits) {
  char buf[100];
  SPrintLimits(buf, sizeof(buf), page_limits);
  LOGF("OnImportMemory(import_index: %" PRIindex ", memory_index: %" PRIindex
       ", %s)\n",
       import_index, memory_index, buf);
  return reader_->OnImportMemory(import_index, module_name, field_name,
                                 memory_index, page_limits);
}

Result BinaryReaderLogging::OnImportGlobal(Index import_index,
                                           std::string_view module_name,
                                           std::string_view field_name,
                                           Index global_index,
                                           Type type,
                                           bool mutable_) {
  LOGF("OnImportGlobal(import_index: %" PRIindex ", global_index: %" PRIindex
       ", type: %s, mutable: "
       "%s)\n",
       import_index, global_index, type.GetName().c_str(),
       mutable_ ? "true" : "false");
  return reader_->OnImportGlobal(import_index, module_name, field_name,
                                 global_index, type, mutable_);
}

Result BinaryReaderLogging::OnImportTag(Index import_index,
                                        std::string_view module_name,
                                        std::string_view field_name,
                                        Index tag_index,
                                        Index sig_index) {
  LOGF("OnImportTag(import_index: %" PRIindex ", tag_index: %" PRIindex
       ", sig_index: %" PRIindex ")\n",
       import_index, tag_index, sig_index);
  return reader_->OnImportTag(import_index, module_name, field_name, tag_index,
                              sig_index);
}

Result BinaryReaderLogging::OnTable(Index index,
                                    Type elem_type,
                                    const Limits* elem_limits) {
  char buf[100];
  SPrintLimits(buf, sizeof(buf), elem_limits);
  LOGF("OnTable(index: %" PRIindex ", elem_type: %s, %s)\n", index,
       elem_type.GetName().c_str(), buf);
  return reader_->OnTable(index, elem_type, elem_limits);
}

Result BinaryReaderLogging::OnMemory(Index index, const Limits* page_limits) {
  char buf[100];
  SPrintLimits(buf, sizeof(buf), page_limits);
  LOGF("OnMemory(index: %" PRIindex ", %s)\n", index, buf);
  return reader_->OnMemory(index, page_limits);
}

Result BinaryReaderLogging::BeginGlobal(Index index, Type type, bool mutable_) {
  LOGF("BeginGlobal(index: %" PRIindex ", type: %s, mutable: %s)\n", index,
       type.GetName().c_str(), mutable_ ? "true" : "false");
  return reader_->BeginGlobal(index, type, mutable_);
}

Result BinaryReaderLogging::OnExport(Index index,
                                     ExternalKind kind,
                                     Index item_index,
                                     std::string_view name) {
  LOGF("OnExport(index: %" PRIindex ", kind: %s, item_index: %" PRIindex
       ", name: \"" PRIstringview "\")\n",
       index, GetKindName(kind), item_index, WABT_PRINTF_STRING_VIEW_ARG(name));
  return reader_->OnExport(index, kind, item_index, name);
}

Result BinaryReaderLogging::BeginFunctionBody(Index value, Offset size) {
  LOGF("BeginFunctionBody(%" PRIindex ", size:%" PRIzd ")\n", value, size);
  return reader_->BeginFunctionBody(value, size);
}

Result BinaryReaderLogging::OnLocalDecl(Index decl_index,
                                        Index count,
                                        Type type) {
  LOGF("OnLocalDecl(index: %" PRIindex ", count: %" PRIindex ", type: %s)\n",
       decl_index, count, type.GetName().c_str());
  return reader_->OnLocalDecl(decl_index, count, type);
}

Result BinaryReaderLogging::OnBlockExpr(Type sig_type) {
  LOGF("OnBlockExpr(sig: ");
  LogType(sig_type);
  LOGF_NOINDENT(")\n");
  return reader_->OnBlockExpr(sig_type);
}

Result BinaryReaderLogging::OnBrExpr(Index depth) {
  LOGF("OnBrExpr(depth: %" PRIindex ")\n", depth);
  return reader_->OnBrExpr(depth);
}

Result BinaryReaderLogging::OnBrIfExpr(Index depth) {
  LOGF("OnBrIfExpr(depth: %" PRIindex ")\n", depth);
  return reader_->OnBrIfExpr(depth);
}

Result BinaryReaderLogging::OnBrTableExpr(Index num_targets,
                                          Index* target_depths,
                                          Index default_target_depth) {
  LOGF("OnBrTableExpr(num_targets: %" PRIindex ", depths: [", num_targets);
  for (Index i = 0; i < num_targets; ++i) {
    LOGF_NOINDENT("%" PRIindex, target_depths[i]);
    if (i != num_targets - 1) {
      LOGF_NOINDENT(", ");
    }
  }
  LOGF_NOINDENT("], default: %" PRIindex ")\n", default_target_depth);
  return reader_->OnBrTableExpr(num_targets, target_depths,
                                default_target_depth);
}

Result BinaryReaderLogging::OnF32ConstExpr(uint32_t value_bits) {
  float value;
  memcpy(&value, &value_bits, sizeof(value));
  LOGF("OnF32ConstExpr(%g (0x%08x))\n", value, value_bits);
  return reader_->OnF32ConstExpr(value_bits);
}

Result BinaryReaderLogging::OnF64ConstExpr(uint64_t value_bits) {
  double value;
  memcpy(&value, &value_bits, sizeof(value));
  LOGF("OnF64ConstExpr(%g (0x%016" PRIx64 "))\n", value, value_bits);
  return reader_->OnF64ConstExpr(value_bits);
}

Result BinaryReaderLogging::OnV128ConstExpr(v128 value_bits) {
  LOGF("OnV128ConstExpr(0x%08x 0x%08x 0x%08x 0x%08x)\n", value_bits.u32(0),
       value_bits.u32(1), value_bits.u32(2), value_bits.u32(3));
  return reader_->OnV128ConstExpr(value_bits);
}

Result BinaryReaderLogging::OnI32ConstExpr(uint32_t value) {
  LOGF("OnI32ConstExpr(%u (0x%x))\n", value, value);
  return reader_->OnI32ConstExpr(value);
}

Result BinaryReaderLogging::OnI64ConstExpr(uint64_t value) {
  LOGF("OnI64ConstExpr(%" PRIu64 " (0x%" PRIx64 "))\n", value, value);
  return reader_->OnI64ConstExpr(value);
}

Result BinaryReaderLogging::OnIfExpr(Type sig_type) {
  LOGF("OnIfExpr(sig: ");
  LogType(sig_type);
  LOGF_NOINDENT(")\n");
  return reader_->OnIfExpr(sig_type);
}

Result BinaryReaderLogging::OnLoopExpr(Type sig_type) {
  LOGF("OnLoopExpr(sig: ");
  LogType(sig_type);
  LOGF_NOINDENT(")\n");
  return reader_->OnLoopExpr(sig_type);
}

Result BinaryReaderLogging::OnSelectExpr(Index result_count,
                                         Type* result_types) {
  LOGF("OnSelectExpr(return_type: ");
  LogTypes(result_count, result_types);
  LOGF_NOINDENT(")\n");
  return reader_->OnSelectExpr(result_count, result_types);
}

Result BinaryReaderLogging::OnTryExpr(Type sig_type) {
  LOGF("OnTryExpr(sig: ");
  LogType(sig_type);
  LOGF_NOINDENT(")\n");
  return reader_->OnTryExpr(sig_type);
}

Result BinaryReaderLogging::OnSimdLaneOpExpr(Opcode opcode, uint64_t value) {
  LOGF("OnSimdLaneOpExpr (lane: %" PRIu64 ")\n", value);
  return reader_->OnSimdLaneOpExpr(opcode, value);
}

Result BinaryReaderLogging::OnSimdShuffleOpExpr(Opcode opcode, v128 value) {
  LOGF("OnSimdShuffleOpExpr (lane: 0x%08x %08x %08x %08x)\n", value.u32(0),
       value.u32(1), value.u32(2), value.u32(3));
  return reader_->OnSimdShuffleOpExpr(opcode, value);
}

Result BinaryReaderLogging::BeginElemSegment(Index index,
                                             Index table_index,
                                             uint8_t flags) {
  LOGF("BeginElemSegment(index: %" PRIindex ", table_index: %" PRIindex
       ", flags: %d)\n",
       index, table_index, flags);
  return reader_->BeginElemSegment(index, table_index, flags);
}

Result BinaryReaderLogging::OnElemSegmentElemType(Index index, Type elem_type) {
  LOGF("OnElemSegmentElemType(index: %" PRIindex ", type: %s)\n", index,
       elem_type.GetName().c_str());
  return reader_->OnElemSegmentElemType(index, elem_type);
}

Result BinaryReaderLogging::OnDataSegmentData(Index index,
                                              const void* data,
                                              Address size) {
  LOGF("OnDataSegmentData(index:%" PRIindex ", size:%" PRIaddress ")\n", index,
       size);
  return reader_->OnDataSegmentData(index, data, size);
}

Result BinaryReaderLogging::OnModuleNameSubsection(Index index,
                                                   uint32_t name_type,
                                                   Offset subsection_size) {
  LOGF("OnModuleNameSubsection(index:%" PRIindex ", nametype:%u, size:%" PRIzd
       ")\n",
       index, name_type, subsection_size);
  return reader_->OnModuleNameSubsection(index, name_type, subsection_size);
}

Result BinaryReaderLogging::OnModuleName(std::string_view name) {
  LOGF("OnModuleName(name: \"" PRIstringview "\")\n",
       WABT_PRINTF_STRING_VIEW_ARG(name));
  return reader_->OnModuleName(name);
}

Result BinaryReaderLogging::OnFunctionNameSubsection(Index index,
                                                     uint32_t name_type,
                                                     Offset subsection_size) {
  LOGF("OnFunctionNameSubsection(index:%" PRIindex ", nametype:%u, size:%" PRIzd
       ")\n",
       index, name_type, subsection_size);
  return reader_->OnFunctionNameSubsection(index, name_type, subsection_size);
}

Result BinaryReaderLogging::OnFunctionName(Index index, std::string_view name) {
  LOGF("OnFunctionName(index: %" PRIindex ", name: \"" PRIstringview "\")\n",
       index, WABT_PRINTF_STRING_VIEW_ARG(name));
  return reader_->OnFunctionName(index, name);
}

Result BinaryReaderLogging::OnLocalNameSubsection(Index index,
                                                  uint32_t name_type,
                                                  Offset subsection_size) {
  LOGF("OnLocalNameSubsection(index:%" PRIindex ", nametype:%u, size:%" PRIzd
       ")\n",
       index, name_type, subsection_size);
  return reader_->OnLocalNameSubsection(index, name_type, subsection_size);
}

Result BinaryReaderLogging::OnLocalName(Index func_index,
                                        Index local_index,
                                        std::string_view name) {
  LOGF("OnLocalName(func_index: %" PRIindex ", local_index: %" PRIindex
       ", name: \"" PRIstringview "\")\n",
       func_index, local_index, WABT_PRINTF_STRING_VIEW_ARG(name));
  return reader_->OnLocalName(func_index, local_index, name);
}

Result BinaryReaderLogging::OnNameSubsection(
    Index index,
    NameSectionSubsection subsection_type,
    Offset subsection_size) {
  LOGF("OnNameSubsection(index: %" PRIindex ", type: %s, size:%" PRIzd ")\n",
       index, GetNameSectionSubsectionName(subsection_type), subsection_size);
  return reader_->OnNameSubsection(index, subsection_type, subsection_size);
}

Result BinaryReaderLogging::OnNameEntry(NameSectionSubsection type,
                                        Index index,
                                        std::string_view name) {
  LOGF("OnNameEntry(type: %s, index: %" PRIindex ", name: \"" PRIstringview
       "\")\n",
       GetNameSectionSubsectionName(type), index,
       WABT_PRINTF_STRING_VIEW_ARG(name));
  return reader_->OnNameEntry(type, index, name);
}

Result BinaryReaderLogging::OnDylinkInfo(uint32_t mem_size,
                                         uint32_t mem_align,
                                         uint32_t table_size,
                                         uint32_t table_align) {
  LOGF(
      "OnDylinkInfo(mem_size: %u, mem_align: %u, table_size: %u, table_align: "
      "%u)\n",
      mem_size, mem_align, table_size, table_align);
  return reader_->OnDylinkInfo(mem_size, mem_align, table_size, table_align);
}

Result BinaryReaderLogging::OnDylinkNeeded(std::string_view so_name) {
  LOGF("OnDylinkNeeded(name: " PRIstringview ")\n",
       WABT_PRINTF_STRING_VIEW_ARG(so_name));
  return reader_->OnDylinkNeeded(so_name);
}

Result BinaryReaderLogging::OnDylinkExport(std::string_view name,
                                           uint32_t flags) {
  LOGF("OnDylinkExport(name: " PRIstringview ", flags: 0x%x)\n",
       WABT_PRINTF_STRING_VIEW_ARG(name), flags);
  return reader_->OnDylinkExport(name, flags);
}

Result BinaryReaderLogging::OnDylinkImport(std::string_view module,
                                           std::string_view name,
                                           uint32_t flags) {
  LOGF("OnDylinkImport(module: " PRIstringview ", name: " PRIstringview
       ", flags: 0x%x)\n",
       WABT_PRINTF_STRING_VIEW_ARG(module), WABT_PRINTF_STRING_VIEW_ARG(name),
       flags);
  return reader_->OnDylinkImport(module, name, flags);
}

Result BinaryReaderLogging::OnRelocCount(Index count, Index section_index) {
  LOGF("OnRelocCount(count: %" PRIindex ", section: %" PRIindex ")\n", count,
       section_index);
  return reader_->OnRelocCount(count, section_index);
}

Result BinaryReaderLogging::OnReloc(RelocType type,
                                    Offset offset,
                                    Index index,
                                    uint32_t addend) {
  int32_t signed_addend = static_cast<int32_t>(addend);
  LOGF("OnReloc(type: %s, offset: %" PRIzd ", index: %" PRIindex
       ", addend: %d)\n",
       GetRelocTypeName(type), offset, index, signed_addend);
  return reader_->OnReloc(type, offset, index, addend);
}

Result BinaryReaderLogging::OnFeature(uint8_t prefix, std::string_view name) {
  LOGF("OnFeature(prefix: '%c', name: '" PRIstringview "')\n", prefix,
       WABT_PRINTF_STRING_VIEW_ARG(name));
  return reader_->OnFeature(prefix, name);
}

Result BinaryReaderLogging::OnDataSymbol(Index index,
                                         uint32_t flags,
                                         std::string_view name,
                                         Index segment,
                                         uint32_t offset,
                                         uint32_t size) {
  LOGF("OnDataSymbol(name: " PRIstringview " flags: 0x%x)\n",
       WABT_PRINTF_STRING_VIEW_ARG(name), flags);
  return reader_->OnDataSymbol(index, flags, name, segment, offset, size);
}

Result BinaryReaderLogging::OnFunctionSymbol(Index index,
                                             uint32_t flags,
                                             std::string_view name,
                                             Index func_index) {
  LOGF("OnFunctionSymbol(name: " PRIstringview " flags: 0x%x index: %" PRIindex
       ")\n",
       WABT_PRINTF_STRING_VIEW_ARG(name), flags, func_index);
  return reader_->OnFunctionSymbol(index, flags, name, func_index);
}

Result BinaryReaderLogging::OnGlobalSymbol(Index index,
                                           uint32_t flags,
                                           std::string_view name,
                                           Index global_index) {
  LOGF("OnGlobalSymbol(name: " PRIstringview " flags: 0x%x index: %" PRIindex
       ")\n",
       WABT_PRINTF_STRING_VIEW_ARG(name), flags, global_index);
  return reader_->OnGlobalSymbol(index, flags, name, global_index);
}

Result BinaryReaderLogging::OnSectionSymbol(Index index,
                                            uint32_t flags,
                                            Index section_index) {
  LOGF("OnSectionSymbol(flags: 0x%x index: %" PRIindex ")\n", flags,
       section_index);
  return reader_->OnSectionSymbol(index, flags, section_index);
}

Result BinaryReaderLogging::OnTagSymbol(Index index,
                                        uint32_t flags,
                                        std::string_view name,
                                        Index tag_index) {
  LOGF("OnTagSymbol(name: " PRIstringview " flags: 0x%x index: %" PRIindex
       ")\n",
       WABT_PRINTF_STRING_VIEW_ARG(name), flags, tag_index);
  return reader_->OnTagSymbol(index, flags, name, tag_index);
}

Result BinaryReaderLogging::OnTableSymbol(Index index,
                                          uint32_t flags,
                                          std::string_view name,
                                          Index table_index) {
  LOGF("OnTableSymbol(name: " PRIstringview " flags: 0x%x index: %" PRIindex
       ")\n",
       WABT_PRINTF_STRING_VIEW_ARG(name), flags, table_index);
  return reader_->OnTableSymbol(index, flags, name, table_index);
}

Result BinaryReaderLogging::OnSegmentInfo(Index index,
                                          std::string_view name,
                                          Address alignment,
                                          uint32_t flags) {
  LOGF("OnSegmentInfo(%d name: " PRIstringview ", alignment: %" PRIaddress
       ", flags: 0x%x)\n",
       index, WABT_PRINTF_STRING_VIEW_ARG(name), alignment, flags);
  return reader_->OnSegmentInfo(index, name, alignment, flags);
}

Result BinaryReaderLogging::OnInitFunction(uint32_t priority,
                                           Index symbol_index) {
  LOGF("OnInitFunction(%d priority: %d)\n", symbol_index, priority);
  return reader_->OnInitFunction(priority, symbol_index);
}

Result BinaryReaderLogging::OnComdatBegin(std::string_view name,
                                          uint32_t flags,
                                          Index count) {
  LOGF("OnComdatBegin(" PRIstringview ", flags: %d, count: %" PRIindex ")\n",
       WABT_PRINTF_STRING_VIEW_ARG(name), flags, count);
  return reader_->OnComdatBegin(name, flags, count);
}

Result BinaryReaderLogging::OnComdatEntry(ComdatType kind, Index index) {
  LOGF("OnComdatEntry(kind: %d, index: %" PRIindex ")\n",
       static_cast<int>(kind), index);
  return reader_->OnComdatEntry(kind, index);
}

Result BinaryReaderLogging::BeginCodeMetadataSection(std::string_view name,
                                                     Offset size) {
  LOGF("BeginCodeMetadataSection('" PRIstringview "', size:%" PRIzd ")\n",
       WABT_PRINTF_STRING_VIEW_ARG(name), size);
  Indent();
  return reader_->BeginCodeMetadataSection(name, size);
}
Result BinaryReaderLogging::OnCodeMetadata(Offset code_offset,
                                           const void* data,
                                           Address size) {
  std::string_view content(static_cast<const char*>(data), size);
  LOGF("OnCodeMetadata(offset: %" PRIzd ", data: \"" PRIstringview "\")\n",
       code_offset, WABT_PRINTF_STRING_VIEW_ARG(content));
  return reader_->OnCodeMetadata(code_offset, data, size);
}

#define DEFINE_BEGIN(name)                        \
  Result BinaryReaderLogging::name(Offset size) { \
    LOGF(#name "(%" PRIzd ")\n", size);           \
    Indent();                                     \
    return reader_->name(size);                   \
  }

#define DEFINE_END(name)               \
  Result BinaryReaderLogging::name() { \
    Dedent();                          \
    LOGF(#name "\n");                  \
    return reader_->name();            \
  }

#define DEFINE_INDEX(name)                        \
  Result BinaryReaderLogging::name(Index value) { \
    LOGF(#name "(%" PRIindex ")\n", value);       \
    return reader_->name(value);                  \
  }

#define DEFINE_TYPE(name)                         \
  Result BinaryReaderLogging::name(Type type) {   \
    LOGF(#name "(%s)\n", type.GetName().c_str()); \
    return reader_->name(type);                   \
  }

#define DEFINE_INDEX_DESC(name, desc)                 \
  Result BinaryReaderLogging::name(Index value) {     \
    LOGF(#name "(" desc ": %" PRIindex ")\n", value); \
    return reader_->name(value);                      \
  }

#define DEFINE_INDEX_TYPE(name)                              \
  Result BinaryReaderLogging::name(Index value, Type type) { \
    LOGF(#name "(index: %" PRIindex ", type: %s)\n", value,  \
         type.GetName().c_str());                            \
    return reader_->name(value, type);                       \
  }

#define DEFINE_INDEX_INDEX(name, desc0, desc1)                           \
  Result BinaryReaderLogging::name(Index value0, Index value1) {         \
    LOGF(#name "(" desc0 ": %" PRIindex ", " desc1 ": %" PRIindex ")\n", \
         value0, value1);                                                \
    return reader_->name(value0, value1);                                \
  }

#define DEFINE_INDEX_INDEX_U8(name, desc0, desc1, desc2)                     \
  Result BinaryReaderLogging::name(Index value0, Index value1,               \
                                   uint8_t value2) {                         \
    LOGF(#name "(" desc0 ": %" PRIindex ", " desc1 ": %" PRIindex ", " desc2 \
               ": %d)\n",                                                    \
         value0, value1, value2);                                            \
    return reader_->name(value0, value1, value2);                            \
  }

#define DEFINE_OPCODE(name)                                            \
  Result BinaryReaderLogging::name(Opcode opcode) {                    \
    LOGF(#name "(\"%s\" (%u))\n", opcode.GetName(), opcode.GetCode()); \
    return reader_->name(opcode);                                      \
  }

#define DEFINE_LOAD_STORE_OPCODE(name)                                        \
  Result BinaryReaderLogging::name(Opcode opcode, Index memidx,               \
                                   Address alignment_log2, Address offset) {  \
    LOGF(#name "(opcode: \"%s\" (%u), memidx: %" PRIindex                     \
               ", align log2: %" PRIaddress ", offset: %" PRIaddress ")\n",   \
         opcode.GetName(), opcode.GetCode(), memidx, alignment_log2, offset); \
    return reader_->name(opcode, memidx, alignment_log2, offset);             \
  }

#define DEFINE_SIMD_LOAD_STORE_LANE_OPCODE(name)                             \
  Result BinaryReaderLogging::name(Opcode opcode, Index memidx,              \
                                   Address alignment_log2, Address offset,   \
                                   uint64_t value) {                         \
    LOGF(#name "(opcode: \"%s\" (%u), memidx: %" PRIindex                    \
               ", align log2: %" PRIaddress ", offset: %" PRIaddress         \
               ", lane: %" PRIu64 ")\n",                                     \
         opcode.GetName(), opcode.GetCode(), memidx, alignment_log2, offset, \
         value);                                                             \
    return reader_->name(opcode, memidx, alignment_log2, offset, value);     \
  }

#define DEFINE0(name)                  \
  Result BinaryReaderLogging::name() { \
    LOGF(#name "\n");                  \
    return reader_->name();            \
  }

DEFINE_END(EndModule)

DEFINE_END(EndCustomSection)

DEFINE_BEGIN(BeginTypeSection)
DEFINE_INDEX(OnTypeCount)
DEFINE_END(EndTypeSection)

DEFINE_BEGIN(BeginImportSection)
DEFINE_INDEX(OnImportCount)
DEFINE_END(EndImportSection)

DEFINE_BEGIN(BeginFunctionSection)
DEFINE_INDEX(OnFunctionCount)
DEFINE_INDEX_INDEX(OnFunction, "index", "sig_index")
DEFINE_END(EndFunctionSection)

DEFINE_BEGIN(BeginTableSection)
DEFINE_INDEX(OnTableCount)
DEFINE_END(EndTableSection)

DEFINE_BEGIN(BeginMemorySection)
DEFINE_INDEX(OnMemoryCount)
DEFINE_END(EndMemorySection)

DEFINE_BEGIN(BeginGlobalSection)
DEFINE_INDEX(OnGlobalCount)
DEFINE_INDEX(BeginGlobalInitExpr)
DEFINE_INDEX(EndGlobalInitExpr)
DEFINE_INDEX(EndGlobal)
DEFINE_END(EndGlobalSection)

DEFINE_BEGIN(BeginExportSection)
DEFINE_INDEX(OnExportCount)
DEFINE_END(EndExportSection)

DEFINE_BEGIN(BeginStartSection)
DEFINE_INDEX(OnStartFunction)
DEFINE_END(EndStartSection)

DEFINE_BEGIN(BeginCodeSection)
DEFINE_INDEX(OnFunctionBodyCount)
DEFINE_INDEX(EndFunctionBody)
DEFINE_INDEX(OnLocalDeclCount)
DEFINE_LOAD_STORE_OPCODE(OnAtomicLoadExpr);
DEFINE_LOAD_STORE_OPCODE(OnAtomicRmwExpr);
DEFINE_LOAD_STORE_OPCODE(OnAtomicRmwCmpxchgExpr);
DEFINE_LOAD_STORE_OPCODE(OnAtomicStoreExpr);
DEFINE_LOAD_STORE_OPCODE(OnAtomicWaitExpr);
DEFINE_INDEX_DESC(OnAtomicFenceExpr, "consistency_model");
DEFINE_LOAD_STORE_OPCODE(OnAtomicNotifyExpr);
DEFINE_OPCODE(OnBinaryExpr)
DEFINE_INDEX_DESC(OnCallExpr, "func_index")
DEFINE_INDEX_INDEX(OnCallIndirectExpr, "sig_index", "table_index")
DEFINE0(OnCallRefExpr)
DEFINE_INDEX_DESC(OnCatchExpr, "tag_index");
DEFINE0(OnCatchAllExpr);
DEFINE_OPCODE(OnCompareExpr)
DEFINE_OPCODE(OnConvertExpr)
DEFINE_INDEX_DESC(OnDelegateExpr, "depth");
DEFINE0(OnDropExpr)
DEFINE0(OnElseExpr)
DEFINE0(OnEndExpr)
DEFINE_INDEX_DESC(OnGlobalGetExpr, "index")
DEFINE_INDEX_DESC(OnGlobalSetExpr, "index")
DEFINE_LOAD_STORE_OPCODE(OnLoadExpr);
DEFINE_INDEX_DESC(OnLocalGetExpr, "index")
DEFINE_INDEX_DESC(OnLocalSetExpr, "index")
DEFINE_INDEX_DESC(OnLocalTeeExpr, "index")
DEFINE_INDEX_INDEX(OnMemoryCopyExpr, "src_memory_index", "dest_memory_index")
DEFINE_INDEX(OnDataDropExpr)
DEFINE_INDEX(OnMemoryFillExpr)
DEFINE_INDEX(OnMemoryGrowExpr)
DEFINE_INDEX_INDEX(OnMemoryInitExpr, "segment_index", "memory_index")
DEFINE_INDEX(OnMemorySizeExpr)
DEFINE_INDEX_INDEX(OnTableCopyExpr, "dst_index", "src_index")
DEFINE_INDEX(OnElemDropExpr)
DEFINE_INDEX_INDEX(OnTableInitExpr, "segment_index", "table_index")
DEFINE_INDEX(OnTableSetExpr)
DEFINE_INDEX(OnTableGetExpr)
DEFINE_INDEX(OnTableGrowExpr)
DEFINE_INDEX(OnTableSizeExpr)
DEFINE_INDEX_DESC(OnTableFillExpr, "table index")
DEFINE_INDEX(OnRefFuncExpr)
DEFINE_TYPE(OnRefNullExpr)
DEFINE0(OnRefIsNullExpr)
DEFINE0(OnNopExpr)
DEFINE_INDEX_DESC(OnRethrowExpr, "depth");
DEFINE_INDEX_DESC(OnReturnCallExpr, "func_index")

DEFINE_INDEX_INDEX(OnReturnCallIndirectExpr, "sig_index", "table_index")
DEFINE0(OnReturnExpr)
DEFINE_LOAD_STORE_OPCODE(OnLoadSplatExpr);
DEFINE_LOAD_STORE_OPCODE(OnLoadZeroExpr);
DEFINE_LOAD_STORE_OPCODE(OnStoreExpr);
DEFINE_INDEX_DESC(OnThrowExpr, "tag_index")
DEFINE0(OnUnreachableExpr)
DEFINE_OPCODE(OnUnaryExpr)
DEFINE_OPCODE(OnTernaryExpr)
DEFINE_SIMD_LOAD_STORE_LANE_OPCODE(OnSimdLoadLaneExpr);
DEFINE_SIMD_LOAD_STORE_LANE_OPCODE(OnSimdStoreLaneExpr);
DEFINE_END(EndCodeSection)

DEFINE_BEGIN(BeginElemSection)
DEFINE_INDEX(OnElemSegmentCount)
DEFINE_INDEX(BeginElemSegmentInitExpr)
DEFINE_INDEX(EndElemSegmentInitExpr)
DEFINE_INDEX_INDEX(OnElemSegmentElemExprCount, "index", "count")
DEFINE_INDEX_TYPE(OnElemSegmentElemExpr_RefNull)
DEFINE_INDEX_INDEX(OnElemSegmentElemExpr_RefFunc, "index", "func_index")
DEFINE_INDEX(EndElemSegment)
DEFINE_END(EndElemSection)

DEFINE_BEGIN(BeginDataSection)
DEFINE_INDEX(OnDataSegmentCount)
DEFINE_INDEX_INDEX_U8(BeginDataSegment, "index", "memory_index", "flags")
DEFINE_INDEX(BeginDataSegmentInitExpr)
DEFINE_INDEX(EndDataSegmentInitExpr)
DEFINE_INDEX(EndDataSegment)
DEFINE_END(EndDataSection)

DEFINE_BEGIN(BeginDataCountSection)
DEFINE_INDEX(OnDataCount)
DEFINE_END(EndDataCountSection)

DEFINE_BEGIN(BeginNamesSection)
DEFINE_INDEX(OnFunctionNamesCount)
DEFINE_INDEX(OnLocalNameFunctionCount)
DEFINE_INDEX_INDEX(OnLocalNameLocalCount, "index", "count")
DEFINE_INDEX(OnNameCount);
DEFINE_END(EndNamesSection)

DEFINE_BEGIN(BeginRelocSection)
DEFINE_END(EndRelocSection)

DEFINE_BEGIN(BeginDylinkSection)
DEFINE_INDEX(OnDylinkNeededCount)
DEFINE_INDEX(OnDylinkExportCount)
DEFINE_INDEX(OnDylinkImportCount)
DEFINE_END(EndDylinkSection)

DEFINE_BEGIN(BeginTargetFeaturesSection)
DEFINE_INDEX(OnFeatureCount)
DEFINE_END(EndTargetFeaturesSection)

DEFINE_BEGIN(BeginLinkingSection)
DEFINE_INDEX(OnSymbolCount)
DEFINE_INDEX(OnSegmentInfoCount)
DEFINE_INDEX(OnInitFunctionCount)
DEFINE_INDEX(OnComdatCount)
DEFINE_END(EndLinkingSection)

DEFINE_BEGIN(BeginTagSection);
DEFINE_INDEX(OnTagCount);
DEFINE_INDEX_INDEX(OnTagType, "index", "sig_index")
DEFINE_END(EndTagSection);

DEFINE_INDEX(OnCodeMetadataFuncCount);
DEFINE_INDEX_INDEX(OnCodeMetadataCount, "func_index", "count");
DEFINE_END(EndCodeMetadataSection);

// We don't need to log these (the individual opcodes are logged instead), but
// we still need to forward the calls.
Result BinaryReaderLogging::OnOpcode(Opcode opcode) {
  return reader_->OnOpcode(opcode);
}

Result BinaryReaderLogging::OnOpcodeBare() {
  return reader_->OnOpcodeBare();
}

Result BinaryReaderLogging::OnOpcodeIndex(Index value) {
  return reader_->OnOpcodeIndex(value);
}

Result BinaryReaderLogging::OnOpcodeIndexIndex(Index value, Index value2) {
  return reader_->OnOpcodeIndexIndex(value, value2);
}

Result BinaryReaderLogging::OnOpcodeUint32(uint32_t value) {
  return reader_->OnOpcodeUint32(value);
}

Result BinaryReaderLogging::OnOpcodeUint32Uint32(uint32_t value,
                                                 uint32_t value2) {
  return reader_->OnOpcodeUint32Uint32(value, value2);
}

Result BinaryReaderLogging::OnOpcodeUint32Uint32Uint32(uint32_t value,
                                                       uint32_t value2,
                                                       uint32_t value3) {
  return reader_->OnOpcodeUint32Uint32Uint32(value, value2, value3);
}

Result BinaryReaderLogging::OnOpcodeUint32Uint32Uint32Uint32(uint32_t value,
                                                             uint32_t value2,
                                                             uint32_t value3,
                                                             uint32_t value4) {
  return reader_->OnOpcodeUint32Uint32Uint32Uint32(value, value2, value3,
                                                   value4);
}

Result BinaryReaderLogging::OnOpcodeUint64(uint64_t value) {
  return reader_->OnOpcodeUint64(value);
}

Result BinaryReaderLogging::OnOpcodeF32(uint32_t value) {
  return reader_->OnOpcodeF32(value);
}

Result BinaryReaderLogging::OnOpcodeF64(uint64_t value) {
  return reader_->OnOpcodeF64(value);
}

Result BinaryReaderLogging::OnOpcodeV128(v128 value) {
  return reader_->OnOpcodeV128(value);
}

Result BinaryReaderLogging::OnOpcodeBlockSig(Type sig_type) {
  return reader_->OnOpcodeBlockSig(sig_type);
}

Result BinaryReaderLogging::OnOpcodeType(Type type) {
  return reader_->OnOpcodeType(type);
}

}  // namespace wabt
