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

#include "wabt/binary-reader.h"

#include <cassert>
#include <cinttypes>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "wabt/config.h"

#include "wabt/binary-reader-logging.h"
#include "wabt/binary.h"
#include "wabt/leb128.h"
#include "wabt/stream.h"
#include "wabt/utf8.h"

#if HAVE_ALLOCA
#include <alloca.h>
#endif

#define ERROR_IF(expr, ...)    \
  do {                         \
    if (expr) {                \
      PrintError(__VA_ARGS__); \
      return Result::Error;    \
    }                          \
  } while (0)

#define ERROR_UNLESS(expr, ...) ERROR_IF(!(expr), __VA_ARGS__)

#define ERROR_UNLESS_OPCODE_ENABLED(opcode)     \
  do {                                          \
    if (!opcode.IsEnabled(options_.features)) { \
      return ReportUnexpectedOpcode(opcode);    \
    }                                           \
  } while (0)

#define CALLBACK0(member) \
  ERROR_UNLESS(Succeeded(delegate_->member()), #member " callback failed")

#define CALLBACK(member, ...)                             \
  ERROR_UNLESS(Succeeded(delegate_->member(__VA_ARGS__)), \
               #member " callback failed")

namespace wabt {

namespace {

class BinaryReader {
 public:
  struct ReadModuleOptions {
    bool stop_on_first_error;
  };

  BinaryReader(const void* data,
               size_t size,
               BinaryReaderDelegate* delegate,
               const ReadBinaryOptions& options);

  Result ReadModule(const ReadModuleOptions& options);

 private:
  template <typename T, T BinaryReader::*member>
  struct ValueRestoreGuard {
    explicit ValueRestoreGuard(BinaryReader* this_)
        : this_(this_), previous_value_(this_->*member) {}
    ~ValueRestoreGuard() { this_->*member = previous_value_; }

    BinaryReader* this_;
    T previous_value_;
  };

  struct ReadSectionsOptions {
    bool stop_on_first_error;
  };

  void WABT_PRINTF_FORMAT(2, 3) PrintError(const char* format, ...);
  [[nodiscard]] Result ReadOpcode(Opcode* out_value, const char* desc);
  template <typename T>
  [[nodiscard]] Result ReadT(T* out_value,
                             const char* type_name,
                             const char* desc);
  [[nodiscard]] Result ReadU8(uint8_t* out_value, const char* desc);
  [[nodiscard]] Result ReadU32(uint32_t* out_value, const char* desc);
  [[nodiscard]] Result ReadF32(uint32_t* out_value, const char* desc);
  [[nodiscard]] Result ReadF64(uint64_t* out_value, const char* desc);
  [[nodiscard]] Result ReadV128(v128* out_value, const char* desc);
  [[nodiscard]] Result ReadU32Leb128(uint32_t* out_value, const char* desc);
  [[nodiscard]] Result ReadU64Leb128(uint64_t* out_value, const char* desc);
  [[nodiscard]] Result ReadS32Leb128(uint32_t* out_value, const char* desc);
  [[nodiscard]] Result ReadS64Leb128(uint64_t* out_value, const char* desc);
  [[nodiscard]] Result ReadType(Type* out_value, const char* desc);
  [[nodiscard]] Result ReadRefType(Type* out_value, const char* desc);
  [[nodiscard]] Result ReadExternalKind(ExternalKind* out_value,
                                        const char* desc);
  [[nodiscard]] Result ReadStr(std::string_view* out_str, const char* desc);
  [[nodiscard]] Result ReadBytes(const void** out_data,
                                 Address* out_data_size,
                                 const char* desc);
  [[nodiscard]] Result ReadIndex(Index* index, const char* desc);
  [[nodiscard]] Result ReadOffset(Offset* offset, const char* desc);
  [[nodiscard]] Result ReadAlignment(Address* align_log2, const char* desc);
  [[nodiscard]] Result ReadMemidx(Index* memidx, const char* desc);
  [[nodiscard]] Result ReadMemLocation(Address* alignment_log2,
                                       Index* memidx,
                                       Address* offset,
                                       const char* desc_align,
                                       const char* desc_memidx,
                                       const char* desc_offset,
                                       uint8_t* lane_val = nullptr);
  [[nodiscard]] Result CallbackMemLocation(const Address* alignment_log2,
                                           const Index* memidx,
                                           const Address* offset,
                                           const uint8_t* lane_val = nullptr);
  [[nodiscard]] Result ReadCount(Index* index, const char* desc);
  [[nodiscard]] Result ReadField(TypeMut* out_value);

  bool IsConcreteType(Type);
  bool IsBlockType(Type);

  Index NumTotalFuncs();

  [[nodiscard]] Result ReadInitExpr(Index index);
  [[nodiscard]] Result ReadTable(Type* out_elem_type, Limits* out_elem_limits);
  [[nodiscard]] Result ReadMemory(Limits* out_page_limits);
  [[nodiscard]] Result ReadGlobalHeader(Type* out_type, bool* out_mutable);
  [[nodiscard]] Result ReadTagType(Index* out_sig_index);
  [[nodiscard]] Result ReadAddress(Address* out_value,
                                   Index memory,
                                   const char* desc);
  [[nodiscard]] Result ReadFunctionBody(Offset end_offset);
  // ReadInstructions either until and END instruction, or until
  // the given end_offset.
  [[nodiscard]] Result ReadInstructions(bool stop_on_end,
                                        Offset end_offset,
                                        Opcode* final_opcode);
  [[nodiscard]] Result ReadNameSection(Offset section_size);
  [[nodiscard]] Result ReadRelocSection(Offset section_size);
  [[nodiscard]] Result ReadDylinkSection(Offset section_size);
  [[nodiscard]] Result ReadDylink0Section(Offset section_size);
  [[nodiscard]] Result ReadTargetFeaturesSections(Offset section_size);
  [[nodiscard]] Result ReadLinkingSection(Offset section_size);
  [[nodiscard]] Result ReadCodeMetadataSection(std::string_view name,
                                               Offset section_size);
  [[nodiscard]] Result ReadCustomSection(Index section_index,
                                         Offset section_size);
  [[nodiscard]] Result ReadTypeSection(Offset section_size);
  [[nodiscard]] Result ReadImportSection(Offset section_size);
  [[nodiscard]] Result ReadFunctionSection(Offset section_size);
  [[nodiscard]] Result ReadTableSection(Offset section_size);
  [[nodiscard]] Result ReadMemorySection(Offset section_size);
  [[nodiscard]] Result ReadGlobalSection(Offset section_size);
  [[nodiscard]] Result ReadExportSection(Offset section_size);
  [[nodiscard]] Result ReadStartSection(Offset section_size);
  [[nodiscard]] Result ReadElemSection(Offset section_size);
  [[nodiscard]] Result ReadCodeSection(Offset section_size);
  [[nodiscard]] Result ReadDataSection(Offset section_size);
  [[nodiscard]] Result ReadDataCountSection(Offset section_size);
  [[nodiscard]] Result ReadTagSection(Offset section_size);
  [[nodiscard]] Result ReadSections(const ReadSectionsOptions& options);
  Result ReportUnexpectedOpcode(Opcode opcode, const char* message = nullptr);

  size_t read_end_ = 0;  // Either the section end or data_size.
  BinaryReaderDelegate::State state_;
  BinaryReaderLogging logging_delegate_;
  BinaryReaderDelegate* delegate_ = nullptr;
  TypeVector param_types_;
  TypeVector result_types_;
  TypeMutVector fields_;
  std::vector<Index> target_depths_;
  const ReadBinaryOptions& options_;
  BinarySection last_known_section_ = BinarySection::Invalid;
  bool did_read_names_section_ = false;
  bool reading_custom_section_ = false;
  Index num_func_imports_ = 0;
  Index num_table_imports_ = 0;
  Index num_memory_imports_ = 0;
  Index num_global_imports_ = 0;
  Index num_tag_imports_ = 0;
  Index num_function_signatures_ = 0;
  Index num_function_bodies_ = 0;
  Index data_count_ = kInvalidIndex;
  std::vector<Limits> memories;

  using ReadEndRestoreGuard =
      ValueRestoreGuard<size_t, &BinaryReader::read_end_>;
};

BinaryReader::BinaryReader(const void* data,
                           size_t size,
                           BinaryReaderDelegate* delegate,
                           const ReadBinaryOptions& options)
    : read_end_(size),
      state_(static_cast<const uint8_t*>(data), size),
      logging_delegate_(options.log_stream, delegate),
      delegate_(options.log_stream ? &logging_delegate_ : delegate),
      options_(options),
      last_known_section_(BinarySection::Invalid) {
  delegate->OnSetState(&state_);
}

void WABT_PRINTF_FORMAT(2, 3) BinaryReader::PrintError(const char* format,
                                                       ...) {
  ErrorLevel error_level =
      reading_custom_section_ && !options_.fail_on_custom_section_error
          ? ErrorLevel::Warning
          : ErrorLevel::Error;

  WABT_SNPRINTF_ALLOCA(buffer, length, format);
  Error error(error_level, Location(state_.offset), buffer);
  bool handled = delegate_->OnError(error);

  if (!handled) {
    // Not great to just print, but we don't want to eat the error either.
    fprintf(stderr, "%07" PRIzx ": %s: %s\n", state_.offset,
            GetErrorLevelName(error_level), buffer);
  }
}

Result BinaryReader::ReportUnexpectedOpcode(Opcode opcode, const char* where) {
  std::string message = "unexpected opcode";
  if (where) {
    message += ' ';
    message += where;
  }

  message += ":";

  std::vector<uint8_t> bytes = opcode.GetBytes();
  assert(bytes.size() > 0);

  for (uint8_t byte : bytes) {
    message += StringPrintf(" 0x%x", byte);
  }

  PrintError("%s", message.c_str());
  return Result::Error;
}

Result BinaryReader::ReadOpcode(Opcode* out_value, const char* desc) {
  uint8_t value = 0;
  CHECK_RESULT(ReadU8(&value, desc));

  if (Opcode::IsPrefixByte(value)) {
    uint32_t code;
    CHECK_RESULT(ReadU32Leb128(&code, desc));
    *out_value = Opcode::FromCode(value, code);
  } else {
    *out_value = Opcode::FromCode(value);
  }
  return Result::Ok;
}

template <typename T>
Result BinaryReader::ReadT(T* out_value,
                           const char* type_name,
                           const char* desc) {
  if (state_.offset + sizeof(T) > read_end_) {
    PrintError("unable to read %s: %s", type_name, desc);
    return Result::Error;
  }
#if WABT_BIG_ENDIAN
  uint8_t tmp[sizeof(T)];
  memcpy(tmp, state_.data + state_.offset, sizeof(tmp));
  SwapBytesSized(tmp, sizeof(tmp));
  memcpy(out_value, tmp, sizeof(T));
#else
  memcpy(out_value, state_.data + state_.offset, sizeof(T));
#endif
  state_.offset += sizeof(T);
  return Result::Ok;
}

Result BinaryReader::ReadU8(uint8_t* out_value, const char* desc) {
  return ReadT(out_value, "uint8_t", desc);
}

Result BinaryReader::ReadU32(uint32_t* out_value, const char* desc) {
  return ReadT(out_value, "uint32_t", desc);
}

Result BinaryReader::ReadF32(uint32_t* out_value, const char* desc) {
  return ReadT(out_value, "float", desc);
}

Result BinaryReader::ReadF64(uint64_t* out_value, const char* desc) {
  return ReadT(out_value, "double", desc);
}

Result BinaryReader::ReadV128(v128* out_value, const char* desc) {
  return ReadT(out_value, "v128", desc);
}

Result BinaryReader::ReadU32Leb128(uint32_t* out_value, const char* desc) {
  const uint8_t* p = state_.data + state_.offset;
  const uint8_t* end = state_.data + read_end_;
  size_t bytes_read = wabt::ReadU32Leb128(p, end, out_value);
  ERROR_UNLESS(bytes_read > 0, "unable to read u32 leb128: %s", desc);
  state_.offset += bytes_read;
  return Result::Ok;
}

Result BinaryReader::ReadU64Leb128(uint64_t* out_value, const char* desc) {
  const uint8_t* p = state_.data + state_.offset;
  const uint8_t* end = state_.data + read_end_;
  size_t bytes_read = wabt::ReadU64Leb128(p, end, out_value);
  ERROR_UNLESS(bytes_read > 0, "unable to read u64 leb128: %s", desc);
  state_.offset += bytes_read;
  return Result::Ok;
}

Result BinaryReader::ReadS32Leb128(uint32_t* out_value, const char* desc) {
  const uint8_t* p = state_.data + state_.offset;
  const uint8_t* end = state_.data + read_end_;
  size_t bytes_read = wabt::ReadS32Leb128(p, end, out_value);
  ERROR_UNLESS(bytes_read > 0, "unable to read i32 leb128: %s", desc);
  state_.offset += bytes_read;
  return Result::Ok;
}

Result BinaryReader::ReadS64Leb128(uint64_t* out_value, const char* desc) {
  const uint8_t* p = state_.data + state_.offset;
  const uint8_t* end = state_.data + read_end_;
  size_t bytes_read = wabt::ReadS64Leb128(p, end, out_value);
  ERROR_UNLESS(bytes_read > 0, "unable to read i64 leb128: %s", desc);
  state_.offset += bytes_read;
  return Result::Ok;
}

Result BinaryReader::ReadType(Type* out_value, const char* desc) {
  uint32_t type = 0;
  CHECK_RESULT(ReadS32Leb128(&type, desc));
  if (static_cast<Type::Enum>(type) == Type::Reference) {
    uint32_t heap_type = 0;
    CHECK_RESULT(ReadS32Leb128(&heap_type, desc));
    *out_value = Type(Type::Reference, heap_type);
  } else {
    *out_value = static_cast<Type>(type);
  }
  return Result::Ok;
}

Result BinaryReader::ReadRefType(Type* out_value, const char* desc) {
  uint32_t type = 0;
  CHECK_RESULT(ReadS32Leb128(&type, desc));
  *out_value = static_cast<Type>(type);
  ERROR_UNLESS(out_value->IsRef(), "%s must be a reference type", desc);
  return Result::Ok;
}

Result BinaryReader::ReadExternalKind(ExternalKind* out_value,
                                      const char* desc) {
  uint8_t value = 0;
  CHECK_RESULT(ReadU8(&value, desc));
  ERROR_UNLESS(value < kExternalKindCount, "invalid export external kind: %d",
               value);
  *out_value = static_cast<ExternalKind>(value);
  return Result::Ok;
}

Result BinaryReader::ReadStr(std::string_view* out_str, const char* desc) {
  uint32_t str_len = 0;
  CHECK_RESULT(ReadU32Leb128(&str_len, "string length"));

  ERROR_UNLESS(state_.offset + str_len <= read_end_,
               "unable to read string: %s", desc);

  *out_str = std::string_view(
      reinterpret_cast<const char*>(state_.data) + state_.offset, str_len);
  state_.offset += str_len;

  ERROR_UNLESS(IsValidUtf8(out_str->data(), out_str->length()),
               "invalid utf-8 encoding: %s", desc);
  return Result::Ok;
}

Result BinaryReader::ReadBytes(const void** out_data,
                               Address* out_data_size,
                               const char* desc) {
  uint32_t data_size = 0;
  CHECK_RESULT(ReadU32Leb128(&data_size, "data size"));

  ERROR_UNLESS(state_.offset + data_size <= read_end_,
               "unable to read data: %s", desc);

  *out_data = static_cast<const uint8_t*>(state_.data) + state_.offset;
  *out_data_size = data_size;
  state_.offset += data_size;
  return Result::Ok;
}

Result BinaryReader::ReadIndex(Index* index, const char* desc) {
  uint32_t value;
  CHECK_RESULT(ReadU32Leb128(&value, desc));
  *index = value;
  return Result::Ok;
}

Result BinaryReader::ReadOffset(Offset* offset, const char* desc) {
  uint32_t value;
  CHECK_RESULT(ReadU32Leb128(&value, desc));
  *offset = value;
  return Result::Ok;
}

Result BinaryReader::ReadAlignment(Address* alignment_log2, const char* desc) {
  uint32_t value;
  CHECK_RESULT(ReadU32Leb128(&value, desc));
  if (value >= 128 ||
      (value >= 32 && !options_.features.multi_memory_enabled())) {
    PrintError("invalid %s: %u", desc, value);
    return Result::Error;
  }
  *alignment_log2 = value;
  return Result::Ok;
}

Result BinaryReader::ReadMemidx(Index* memidx, const char* desc) {
  CHECK_RESULT(ReadIndex(memidx, desc));
  ERROR_UNLESS(*memidx < memories.size(), "memory index %u out of range",
               *memidx);
  return Result::Ok;
}

Result BinaryReader::ReadMemLocation(Address* alignment_log2,
                                     Index* memidx,
                                     Address* offset,
                                     const char* desc_align,
                                     const char* desc_memidx,
                                     const char* desc_offset,
                                     uint8_t* lane_val) {
  CHECK_RESULT(ReadAlignment(alignment_log2, desc_align));
  *memidx = 0;
  if (*alignment_log2 >> 6) {
    ERROR_IF(!options_.features.multi_memory_enabled(),
             "multi_memory not allowed");
    *alignment_log2 = *alignment_log2 & ((1 << 6) - 1);
    CHECK_RESULT(ReadMemidx(memidx, desc_memidx));
  }
  CHECK_RESULT(ReadAddress(offset, 0, desc_offset));

  if (lane_val) {
    CHECK_RESULT(ReadU8(lane_val, "Lane idx"));
  }

  return Result::Ok;
}

Result BinaryReader::CallbackMemLocation(const Address* alignment_log2,
                                         const Index* memidx,
                                         const Address* offset,
                                         const uint8_t* lane_val) {
  if (lane_val) {
    if (*memidx) {
      CALLBACK(OnOpcodeUint32Uint32Uint32Uint32, *alignment_log2, *memidx,
               *offset, *lane_val);
    } else {
      CALLBACK(OnOpcodeUint32Uint32Uint32, *alignment_log2, *offset, *lane_val);
    }
  } else {
    if (*memidx) {
      CALLBACK(OnOpcodeUint32Uint32Uint32, *alignment_log2, *memidx, *offset);
    } else {
      CALLBACK(OnOpcodeUint32Uint32, *alignment_log2, *offset);
    }
  }

  return Result::Ok;
}

Result BinaryReader::ReadCount(Index* count, const char* desc) {
  CHECK_RESULT(ReadIndex(count, desc));

  // This check assumes that each item follows in this section, and takes at
  // least 1 byte. It's possible that this check passes but reading fails
  // later. It is still useful to check here, though, because it early-outs
  // when an erroneous large count is used, before allocating memory for it.
  size_t section_remaining = read_end_ - state_.offset;
  if (*count > section_remaining) {
    PrintError("invalid %s %" PRIindex ", only %" PRIzd
               " bytes left in section",
               desc, *count, section_remaining);
    return Result::Error;
  }
  return Result::Ok;
}

Result BinaryReader::ReadField(TypeMut* out_value) {
  // TODO: Reuse for global header too?
  Type field_type;
  CHECK_RESULT(ReadType(&field_type, "field type"));
  ERROR_UNLESS(IsConcreteType(field_type),
               "expected valid field type (got " PRItypecode ")",
               WABT_PRINTF_TYPE_CODE(field_type));

  uint8_t mutable_ = 0;
  CHECK_RESULT(ReadU8(&mutable_, "field mutability"));
  ERROR_UNLESS(mutable_ <= 1, "field mutability must be 0 or 1");
  out_value->type = field_type;
  out_value->mutable_ = mutable_;
  return Result::Ok;
}

bool BinaryReader::IsConcreteType(Type type) {
  switch (type) {
    case Type::I32:
    case Type::I64:
    case Type::F32:
    case Type::F64:
      return true;

    case Type::V128:
      return options_.features.simd_enabled();

    case Type::FuncRef:
    case Type::ExternRef:
      return options_.features.reference_types_enabled();

    case Type::Reference:
      return options_.features.function_references_enabled();

    default:
      return false;
  }
}

bool BinaryReader::IsBlockType(Type type) {
  if (IsConcreteType(type) || type == Type::Void) {
    return true;
  }

  if (!(options_.features.multi_value_enabled() && type.IsIndex())) {
    return false;
  }

  return true;
}

Index BinaryReader::NumTotalFuncs() {
  return num_func_imports_ + num_function_signatures_;
}

Result BinaryReader::ReadInitExpr(Index index) {
  // Read instructions until END opcode is reached.
  Opcode final_opcode(Opcode::Invalid);
  CHECK_RESULT(
      ReadInstructions(/*stop_on_end=*/true, read_end_, &final_opcode));
  ERROR_UNLESS(state_.offset <= read_end_,
               "init expression longer than given size");
  ERROR_UNLESS(final_opcode == Opcode::End,
               "init expression must end with END opcode");
  return Result::Ok;
}

Result BinaryReader::ReadTable(Type* out_elem_type, Limits* out_elem_limits) {
  CHECK_RESULT(ReadRefType(out_elem_type, "table elem type"));

  uint8_t flags;
  uint32_t initial;
  uint32_t max = 0;
  CHECK_RESULT(ReadU8(&flags, "table flags"));
  bool has_max = flags & WABT_BINARY_LIMITS_HAS_MAX_FLAG;
  bool is_shared = flags & WABT_BINARY_LIMITS_IS_SHARED_FLAG;
  bool is_64 = flags & WABT_BINARY_LIMITS_IS_64_FLAG;
  const uint8_t unknown_flags = flags & ~WABT_BINARY_LIMITS_ALL_FLAGS;
  ERROR_IF(is_shared, "tables may not be shared");
  ERROR_IF(is_64, "tables may not be 64-bit");
  ERROR_UNLESS(unknown_flags == 0, "malformed table limits flag: %d", flags);
  CHECK_RESULT(ReadU32Leb128(&initial, "table initial elem count"));
  if (has_max) {
    CHECK_RESULT(ReadU32Leb128(&max, "table max elem count"));
  }

  out_elem_limits->has_max = has_max;
  out_elem_limits->initial = initial;
  out_elem_limits->max = max;
  return Result::Ok;
}

Result BinaryReader::ReadMemory(Limits* out_page_limits) {
  uint8_t flags;
  uint64_t initial;
  uint64_t max = 0;
  CHECK_RESULT(ReadU8(&flags, "memory flags"));
  bool has_max = flags & WABT_BINARY_LIMITS_HAS_MAX_FLAG;
  bool is_shared = flags & WABT_BINARY_LIMITS_IS_SHARED_FLAG;
  bool is_64 = flags & WABT_BINARY_LIMITS_IS_64_FLAG;
  const uint8_t unknown_flags = flags & ~WABT_BINARY_LIMITS_ALL_FLAGS;
  ERROR_UNLESS(unknown_flags == 0, "malformed memory limits flag: %d", flags);
  ERROR_IF(is_shared && !options_.features.threads_enabled(),
           "memory may not be shared: threads not allowed");
  ERROR_IF(is_64 && !options_.features.memory64_enabled(),
           "memory64 not allowed");
  if (is_64) {
    CHECK_RESULT(ReadU64Leb128(&initial, "memory initial page count"));
    if (has_max) {
      CHECK_RESULT(ReadU64Leb128(&max, "memory max page count"));
    }
  } else {
    uint32_t initial32;
    CHECK_RESULT(ReadU32Leb128(&initial32, "memory initial page count"));
    initial = initial32;
    if (has_max) {
      uint32_t max32;
      CHECK_RESULT(ReadU32Leb128(&max32, "memory max page count"));
      max = max32;
    }
  }

  out_page_limits->has_max = has_max;
  out_page_limits->is_shared = is_shared;
  out_page_limits->is_64 = is_64;
  out_page_limits->initial = initial;
  out_page_limits->max = max;

  // Have to keep a copy of these, to know how to interpret load/stores.
  memories.push_back(*out_page_limits);
  return Result::Ok;
}

Result BinaryReader::ReadGlobalHeader(Type* out_type, bool* out_mutable) {
  Type global_type = Type::Void;
  uint8_t mutable_ = 0;
  CHECK_RESULT(ReadType(&global_type, "global type"));
  ERROR_UNLESS(IsConcreteType(global_type), "invalid global type: %#x",
               static_cast<int>(global_type));

  CHECK_RESULT(ReadU8(&mutable_, "global mutability"));
  ERROR_UNLESS(mutable_ <= 1, "global mutability must be 0 or 1");

  *out_type = global_type;
  *out_mutable = mutable_;
  return Result::Ok;
}

Result BinaryReader::ReadAddress(Address* out_value,
                                 Index memory,
                                 const char* desc) {
  ERROR_UNLESS(memory < memories.size(),
               "load/store memory %u out of range %zu", memory,
               memories.size());
  if (memories[memory].is_64) {
    return ReadU64Leb128(out_value, desc);
  } else {
    uint32_t val;
    Result res = ReadU32Leb128(&val, desc);
    *out_value = val;
    return res;
  }
}

Result BinaryReader::ReadFunctionBody(Offset end_offset) {
  Opcode final_opcode(Opcode::Invalid);
  CHECK_RESULT(
      ReadInstructions(/*stop_on_end=*/false, end_offset, &final_opcode));
  ERROR_UNLESS(state_.offset == end_offset,
               "function body longer than given size");
  ERROR_UNLESS(final_opcode == Opcode::End,
               "function body must end with END opcode");
  return Result::Ok;
}

Result BinaryReader::ReadInstructions(bool stop_on_end,
                                      Offset end_offset,
                                      Opcode* final_opcode) {
  while (state_.offset < end_offset) {
    Opcode opcode;
    CHECK_RESULT(ReadOpcode(&opcode, "opcode"));
    CALLBACK(OnOpcode, opcode);
    ERROR_UNLESS_OPCODE_ENABLED(opcode);
    if (final_opcode) {
      *final_opcode = opcode;
    }

    switch (opcode) {
      case Opcode::Unreachable:
        CALLBACK0(OnUnreachableExpr);
        CALLBACK0(OnOpcodeBare);
        break;

      case Opcode::Block: {
        Type sig_type;
        CHECK_RESULT(ReadType(&sig_type, "block signature type"));
        ERROR_UNLESS(IsBlockType(sig_type),
                     "expected valid block signature type");
        CALLBACK(OnBlockExpr, sig_type);
        CALLBACK(OnOpcodeBlockSig, sig_type);
        break;
      }

      case Opcode::Loop: {
        Type sig_type;
        CHECK_RESULT(ReadType(&sig_type, "loop signature type"));
        ERROR_UNLESS(IsBlockType(sig_type),
                     "expected valid block signature type");
        CALLBACK(OnLoopExpr, sig_type);
        CALLBACK(OnOpcodeBlockSig, sig_type);
        break;
      }

      case Opcode::If: {
        Type sig_type;
        CHECK_RESULT(ReadType(&sig_type, "if signature type"));
        ERROR_UNLESS(IsBlockType(sig_type),
                     "expected valid block signature type");
        CALLBACK(OnIfExpr, sig_type);
        CALLBACK(OnOpcodeBlockSig, sig_type);
        break;
      }

      case Opcode::Else:
        CALLBACK0(OnElseExpr);
        CALLBACK0(OnOpcodeBare);
        break;

      case Opcode::SelectT: {
        Index num_results;
        CHECK_RESULT(ReadCount(&num_results, "num result types"));

        result_types_.resize(num_results);
        for (Index i = 0; i < num_results; ++i) {
          Type result_type;
          CHECK_RESULT(ReadType(&result_type, "select result type"));
          ERROR_UNLESS(IsConcreteType(result_type),
                       "expected valid select result type (got " PRItypecode
                       ")",
                       WABT_PRINTF_TYPE_CODE(result_type));
          result_types_[i] = result_type;
        }

        if (num_results) {
          CALLBACK(OnSelectExpr, num_results, result_types_.data());
          CALLBACK(OnOpcodeType, result_types_[0]);
        } else {
          CALLBACK(OnSelectExpr, 0, NULL);
          CALLBACK0(OnOpcodeBare);
        }
        break;
      }

      case Opcode::Select:
        CALLBACK(OnSelectExpr, 0, nullptr);
        CALLBACK0(OnOpcodeBare);
        break;

      case Opcode::Br: {
        Index depth;
        CHECK_RESULT(ReadIndex(&depth, "br depth"));
        CALLBACK(OnBrExpr, depth);
        CALLBACK(OnOpcodeIndex, depth);
        break;
      }

      case Opcode::BrIf: {
        Index depth;
        CHECK_RESULT(ReadIndex(&depth, "br_if depth"));
        CALLBACK(OnBrIfExpr, depth);
        CALLBACK(OnOpcodeIndex, depth);
        break;
      }

      case Opcode::BrTable: {
        Index num_targets;
        CHECK_RESULT(ReadCount(&num_targets, "br_table target count"));
        target_depths_.resize(num_targets);

        for (Index i = 0; i < num_targets; ++i) {
          Index target_depth;
          CHECK_RESULT(ReadIndex(&target_depth, "br_table target depth"));
          target_depths_[i] = target_depth;
        }

        Index default_target_depth;
        CHECK_RESULT(
            ReadIndex(&default_target_depth, "br_table default target depth"));

        Index* target_depths = num_targets ? target_depths_.data() : nullptr;

        CALLBACK(OnBrTableExpr, num_targets, target_depths,
                 default_target_depth);
        break;
      }

      case Opcode::Return:
        CALLBACK0(OnReturnExpr);
        CALLBACK0(OnOpcodeBare);
        break;

      case Opcode::Nop:
        CALLBACK0(OnNopExpr);
        CALLBACK0(OnOpcodeBare);
        break;

      case Opcode::Drop:
        CALLBACK0(OnDropExpr);
        CALLBACK0(OnOpcodeBare);
        break;

      case Opcode::End:
        CALLBACK0(OnEndExpr);
        if (stop_on_end) {
          return Result::Ok;
        }
        break;

      case Opcode::I32Const: {
        uint32_t value;
        CHECK_RESULT(ReadS32Leb128(&value, "i32.const value"));
        CALLBACK(OnI32ConstExpr, value);
        CALLBACK(OnOpcodeUint32, value);
        break;
      }

      case Opcode::I64Const: {
        uint64_t value;
        CHECK_RESULT(ReadS64Leb128(&value, "i64.const value"));
        CALLBACK(OnI64ConstExpr, value);
        CALLBACK(OnOpcodeUint64, value);
        break;
      }

      case Opcode::F32Const: {
        uint32_t value_bits = 0;
        CHECK_RESULT(ReadF32(&value_bits, "f32.const value"));
        CALLBACK(OnF32ConstExpr, value_bits);
        CALLBACK(OnOpcodeF32, value_bits);
        break;
      }

      case Opcode::F64Const: {
        uint64_t value_bits = 0;
        CHECK_RESULT(ReadF64(&value_bits, "f64.const value"));
        CALLBACK(OnF64ConstExpr, value_bits);
        CALLBACK(OnOpcodeF64, value_bits);
        break;
      }

      case Opcode::V128Const: {
        v128 value_bits;
        ZeroMemory(value_bits);
        CHECK_RESULT(ReadV128(&value_bits, "v128.const value"));
        CALLBACK(OnV128ConstExpr, value_bits);
        CALLBACK(OnOpcodeV128, value_bits);
        break;
      }

      case Opcode::GlobalGet: {
        Index global_index;
        CHECK_RESULT(ReadIndex(&global_index, "global.get global index"));
        CALLBACK(OnGlobalGetExpr, global_index);
        CALLBACK(OnOpcodeIndex, global_index);
        break;
      }

      case Opcode::LocalGet: {
        Index local_index;
        CHECK_RESULT(ReadIndex(&local_index, "local.get local index"));
        CALLBACK(OnLocalGetExpr, local_index);
        CALLBACK(OnOpcodeIndex, local_index);
        break;
      }

      case Opcode::GlobalSet: {
        Index global_index;
        CHECK_RESULT(ReadIndex(&global_index, "global.set global index"));
        CALLBACK(OnGlobalSetExpr, global_index);
        CALLBACK(OnOpcodeIndex, global_index);
        break;
      }

      case Opcode::LocalSet: {
        Index local_index;
        CHECK_RESULT(ReadIndex(&local_index, "local.set local index"));
        CALLBACK(OnLocalSetExpr, local_index);
        CALLBACK(OnOpcodeIndex, local_index);
        break;
      }

      case Opcode::Call: {
        Index func_index;
        CHECK_RESULT(ReadIndex(&func_index, "call function index"));
        CALLBACK(OnCallExpr, func_index);
        CALLBACK(OnOpcodeIndex, func_index);
        break;
      }

      case Opcode::CallIndirect: {
        Index sig_index;
        CHECK_RESULT(ReadIndex(&sig_index, "call_indirect signature index"));
        Index table_index = 0;
        if (options_.features.reference_types_enabled()) {
          CHECK_RESULT(ReadIndex(&table_index, "call_indirect table index"));
        } else {
          uint8_t reserved;
          CHECK_RESULT(ReadU8(&reserved, "call_indirect reserved"));
          ERROR_UNLESS(reserved == 0, "call_indirect reserved value must be 0");
        }
        CALLBACK(OnCallIndirectExpr, sig_index, table_index);
        CALLBACK(OnOpcodeUint32Uint32, sig_index, table_index);
        break;
      }

      case Opcode::ReturnCall: {
        Index func_index;
        CHECK_RESULT(ReadIndex(&func_index, "return_call"));
        CALLBACK(OnReturnCallExpr, func_index);
        CALLBACK(OnOpcodeIndex, func_index);
        break;
      }

      case Opcode::ReturnCallIndirect: {
        Index sig_index;
        CHECK_RESULT(ReadIndex(&sig_index, "return_call_indirect"));
        Index table_index = 0;
        if (options_.features.reference_types_enabled()) {
          CHECK_RESULT(
              ReadIndex(&table_index, "return_call_indirect table index"));
        } else {
          uint8_t reserved;
          CHECK_RESULT(ReadU8(&reserved, "return_call_indirect reserved"));
          ERROR_UNLESS(reserved == 0,
                       "return_call_indirect reserved value must be 0");
        }
        CALLBACK(OnReturnCallIndirectExpr, sig_index, table_index);
        CALLBACK(OnOpcodeUint32Uint32, sig_index, table_index);
        break;
      }

      case Opcode::LocalTee: {
        Index local_index;
        CHECK_RESULT(ReadIndex(&local_index, "local.tee local index"));
        CALLBACK(OnLocalTeeExpr, local_index);
        CALLBACK(OnOpcodeIndex, local_index);
        break;
      }

      case Opcode::I32Load8S:
      case Opcode::I32Load8U:
      case Opcode::I32Load16S:
      case Opcode::I32Load16U:
      case Opcode::I64Load8S:
      case Opcode::I64Load8U:
      case Opcode::I64Load16S:
      case Opcode::I64Load16U:
      case Opcode::I64Load32S:
      case Opcode::I64Load32U:
      case Opcode::I32Load:
      case Opcode::I64Load:
      case Opcode::F32Load:
      case Opcode::F64Load:
      case Opcode::V128Load:
      case Opcode::V128Load8X8S:
      case Opcode::V128Load8X8U:
      case Opcode::V128Load16X4S:
      case Opcode::V128Load16X4U:
      case Opcode::V128Load32X2S:
      case Opcode::V128Load32X2U: {
        Address alignment_log2;
        Index memidx;
        Address offset;
        CHECK_RESULT(ReadMemLocation(&alignment_log2, &memidx, &offset,
                                     "load alignment", "load memidx",
                                     "load offset"));
        CALLBACK(OnLoadExpr, opcode, memidx, alignment_log2, offset);
        CHECK_RESULT(CallbackMemLocation(&alignment_log2, &memidx, &offset));
        break;
      }

      case Opcode::I32Store8:
      case Opcode::I32Store16:
      case Opcode::I64Store8:
      case Opcode::I64Store16:
      case Opcode::I64Store32:
      case Opcode::I32Store:
      case Opcode::I64Store:
      case Opcode::F32Store:
      case Opcode::F64Store:
      case Opcode::V128Store: {
        Address alignment_log2;
        Index memidx;
        Address offset;
        CHECK_RESULT(ReadMemLocation(&alignment_log2, &memidx, &offset,
                                     "store alignment", "store memidx",
                                     "store offset"));
        CALLBACK(OnStoreExpr, opcode, memidx, alignment_log2, offset);
        CHECK_RESULT(CallbackMemLocation(&alignment_log2, &memidx, &offset));
        break;
      }

      case Opcode::MemorySize: {
        Index memidx = 0;
        if (!options_.features.multi_memory_enabled()) {
          uint8_t reserved;
          CHECK_RESULT(ReadU8(&reserved, "memory.size reserved"));
          ERROR_UNLESS(reserved == 0, "memory.size reserved value must be 0");
        } else {
          CHECK_RESULT(ReadMemidx(&memidx, "memory.size memidx"));
        }
        CALLBACK(OnMemorySizeExpr, memidx);
        CALLBACK(OnOpcodeUint32, memidx);
        break;
      }

      case Opcode::MemoryGrow: {
        Index memidx = 0;
        if (!options_.features.multi_memory_enabled()) {
          uint8_t reserved;
          CHECK_RESULT(ReadU8(&reserved, "memory.grow reserved"));
          ERROR_UNLESS(reserved == 0, "memory.grow reserved value must be 0");
        } else {
          CHECK_RESULT(ReadMemidx(&memidx, "memory.grow memidx"));
        }
        CALLBACK(OnMemoryGrowExpr, memidx);
        CALLBACK(OnOpcodeUint32, memidx);
        break;
      }

      case Opcode::I32Add:
      case Opcode::I32Sub:
      case Opcode::I32Mul:
      case Opcode::I32DivS:
      case Opcode::I32DivU:
      case Opcode::I32RemS:
      case Opcode::I32RemU:
      case Opcode::I32And:
      case Opcode::I32Or:
      case Opcode::I32Xor:
      case Opcode::I32Shl:
      case Opcode::I32ShrU:
      case Opcode::I32ShrS:
      case Opcode::I32Rotr:
      case Opcode::I32Rotl:
      case Opcode::I64Add:
      case Opcode::I64Sub:
      case Opcode::I64Mul:
      case Opcode::I64DivS:
      case Opcode::I64DivU:
      case Opcode::I64RemS:
      case Opcode::I64RemU:
      case Opcode::I64And:
      case Opcode::I64Or:
      case Opcode::I64Xor:
      case Opcode::I64Shl:
      case Opcode::I64ShrU:
      case Opcode::I64ShrS:
      case Opcode::I64Rotr:
      case Opcode::I64Rotl:
      case Opcode::F32Add:
      case Opcode::F32Sub:
      case Opcode::F32Mul:
      case Opcode::F32Div:
      case Opcode::F32Min:
      case Opcode::F32Max:
      case Opcode::F32Copysign:
      case Opcode::F64Add:
      case Opcode::F64Sub:
      case Opcode::F64Mul:
      case Opcode::F64Div:
      case Opcode::F64Min:
      case Opcode::F64Max:
      case Opcode::F64Copysign:
      case Opcode::I8X16Add:
      case Opcode::I16X8Add:
      case Opcode::I32X4Add:
      case Opcode::I64X2Add:
      case Opcode::I8X16Sub:
      case Opcode::I16X8Sub:
      case Opcode::I32X4Sub:
      case Opcode::I64X2Sub:
      case Opcode::I16X8Mul:
      case Opcode::I32X4Mul:
      case Opcode::I64X2Mul:
      case Opcode::I8X16AddSatS:
      case Opcode::I8X16AddSatU:
      case Opcode::I16X8AddSatS:
      case Opcode::I16X8AddSatU:
      case Opcode::I8X16SubSatS:
      case Opcode::I8X16SubSatU:
      case Opcode::I16X8SubSatS:
      case Opcode::I16X8SubSatU:
      case Opcode::I8X16MinS:
      case Opcode::I16X8MinS:
      case Opcode::I32X4MinS:
      case Opcode::I8X16MinU:
      case Opcode::I16X8MinU:
      case Opcode::I32X4MinU:
      case Opcode::I8X16MaxS:
      case Opcode::I16X8MaxS:
      case Opcode::I32X4MaxS:
      case Opcode::I8X16MaxU:
      case Opcode::I16X8MaxU:
      case Opcode::I32X4MaxU:
      case Opcode::I8X16Shl:
      case Opcode::I16X8Shl:
      case Opcode::I32X4Shl:
      case Opcode::I64X2Shl:
      case Opcode::I8X16ShrS:
      case Opcode::I8X16ShrU:
      case Opcode::I16X8ShrS:
      case Opcode::I16X8ShrU:
      case Opcode::I32X4ShrS:
      case Opcode::I32X4ShrU:
      case Opcode::I64X2ShrS:
      case Opcode::I64X2ShrU:
      case Opcode::V128And:
      case Opcode::V128Or:
      case Opcode::V128Xor:
      case Opcode::F32X4Min:
      case Opcode::F32X4PMin:
      case Opcode::F64X2Min:
      case Opcode::F64X2PMin:
      case Opcode::F32X4Max:
      case Opcode::F32X4PMax:
      case Opcode::F64X2Max:
      case Opcode::F64X2PMax:
      case Opcode::F32X4Add:
      case Opcode::F64X2Add:
      case Opcode::F32X4Sub:
      case Opcode::F64X2Sub:
      case Opcode::F32X4Div:
      case Opcode::F64X2Div:
      case Opcode::F32X4Mul:
      case Opcode::F64X2Mul:
      case Opcode::I8X16Swizzle:
      case Opcode::I8X16NarrowI16X8S:
      case Opcode::I8X16NarrowI16X8U:
      case Opcode::I16X8NarrowI32X4S:
      case Opcode::I16X8NarrowI32X4U:
      case Opcode::V128Andnot:
      case Opcode::I8X16AvgrU:
      case Opcode::I16X8AvgrU:
      case Opcode::I16X8ExtmulLowI8X16S:
      case Opcode::I16X8ExtmulHighI8X16S:
      case Opcode::I16X8ExtmulLowI8X16U:
      case Opcode::I16X8ExtmulHighI8X16U:
      case Opcode::I32X4ExtmulLowI16X8S:
      case Opcode::I32X4ExtmulHighI16X8S:
      case Opcode::I32X4ExtmulLowI16X8U:
      case Opcode::I32X4ExtmulHighI16X8U:
      case Opcode::I64X2ExtmulLowI32X4S:
      case Opcode::I64X2ExtmulHighI32X4S:
      case Opcode::I64X2ExtmulLowI32X4U:
      case Opcode::I64X2ExtmulHighI32X4U:
      case Opcode::I16X8Q15mulrSatS:
      case Opcode::I32X4DotI16X8S:
      case Opcode::I8X16RelaxedSwizzle:
      case Opcode::F32X4RelaxedMin:
      case Opcode::F32X4RelaxedMax:
      case Opcode::F64X2RelaxedMin:
      case Opcode::F64X2RelaxedMax:
      case Opcode::I16X8RelaxedQ15mulrS:
      case Opcode::I16X8DotI8X16I7X16S:
        CALLBACK(OnBinaryExpr, opcode);
        CALLBACK0(OnOpcodeBare);
        break;

      case Opcode::I32Eq:
      case Opcode::I32Ne:
      case Opcode::I32LtS:
      case Opcode::I32LeS:
      case Opcode::I32LtU:
      case Opcode::I32LeU:
      case Opcode::I32GtS:
      case Opcode::I32GeS:
      case Opcode::I32GtU:
      case Opcode::I32GeU:
      case Opcode::I64Eq:
      case Opcode::I64Ne:
      case Opcode::I64LtS:
      case Opcode::I64LeS:
      case Opcode::I64LtU:
      case Opcode::I64LeU:
      case Opcode::I64GtS:
      case Opcode::I64GeS:
      case Opcode::I64GtU:
      case Opcode::I64GeU:
      case Opcode::F32Eq:
      case Opcode::F32Ne:
      case Opcode::F32Lt:
      case Opcode::F32Le:
      case Opcode::F32Gt:
      case Opcode::F32Ge:
      case Opcode::F64Eq:
      case Opcode::F64Ne:
      case Opcode::F64Lt:
      case Opcode::F64Le:
      case Opcode::F64Gt:
      case Opcode::F64Ge:
      case Opcode::I8X16Eq:
      case Opcode::I16X8Eq:
      case Opcode::I32X4Eq:
      case Opcode::I64X2Eq:
      case Opcode::F32X4Eq:
      case Opcode::F64X2Eq:
      case Opcode::I8X16Ne:
      case Opcode::I16X8Ne:
      case Opcode::I32X4Ne:
      case Opcode::I64X2Ne:
      case Opcode::F32X4Ne:
      case Opcode::F64X2Ne:
      case Opcode::I8X16LtS:
      case Opcode::I8X16LtU:
      case Opcode::I16X8LtS:
      case Opcode::I16X8LtU:
      case Opcode::I32X4LtS:
      case Opcode::I32X4LtU:
      case Opcode::I64X2LtS:
      case Opcode::F32X4Lt:
      case Opcode::F64X2Lt:
      case Opcode::I8X16LeS:
      case Opcode::I8X16LeU:
      case Opcode::I16X8LeS:
      case Opcode::I16X8LeU:
      case Opcode::I32X4LeS:
      case Opcode::I32X4LeU:
      case Opcode::I64X2LeS:
      case Opcode::F32X4Le:
      case Opcode::F64X2Le:
      case Opcode::I8X16GtS:
      case Opcode::I8X16GtU:
      case Opcode::I16X8GtS:
      case Opcode::I16X8GtU:
      case Opcode::I32X4GtS:
      case Opcode::I32X4GtU:
      case Opcode::I64X2GtS:
      case Opcode::F32X4Gt:
      case Opcode::F64X2Gt:
      case Opcode::I8X16GeS:
      case Opcode::I8X16GeU:
      case Opcode::I16X8GeS:
      case Opcode::I16X8GeU:
      case Opcode::I32X4GeS:
      case Opcode::I32X4GeU:
      case Opcode::I64X2GeS:
      case Opcode::F32X4Ge:
      case Opcode::F64X2Ge:
        CALLBACK(OnCompareExpr, opcode);
        CALLBACK0(OnOpcodeBare);
        break;

      case Opcode::I32Clz:
      case Opcode::I32Ctz:
      case Opcode::I32Popcnt:
      case Opcode::I64Clz:
      case Opcode::I64Ctz:
      case Opcode::I64Popcnt:
      case Opcode::F32Abs:
      case Opcode::F32Neg:
      case Opcode::F32Ceil:
      case Opcode::F32Floor:
      case Opcode::F32Trunc:
      case Opcode::F32Nearest:
      case Opcode::F32Sqrt:
      case Opcode::F64Abs:
      case Opcode::F64Neg:
      case Opcode::F64Ceil:
      case Opcode::F64Floor:
      case Opcode::F64Trunc:
      case Opcode::F64Nearest:
      case Opcode::F64Sqrt:
      case Opcode::I8X16Splat:
      case Opcode::I16X8Splat:
      case Opcode::I32X4Splat:
      case Opcode::I64X2Splat:
      case Opcode::F32X4Splat:
      case Opcode::F64X2Splat:
      case Opcode::I8X16Neg:
      case Opcode::I16X8Neg:
      case Opcode::I32X4Neg:
      case Opcode::I64X2Neg:
      case Opcode::V128Not:
      case Opcode::V128AnyTrue:
      case Opcode::I8X16Bitmask:
      case Opcode::I16X8Bitmask:
      case Opcode::I32X4Bitmask:
      case Opcode::I64X2Bitmask:
      case Opcode::I8X16AllTrue:
      case Opcode::I16X8AllTrue:
      case Opcode::I32X4AllTrue:
      case Opcode::I64X2AllTrue:
      case Opcode::F32X4Ceil:
      case Opcode::F64X2Ceil:
      case Opcode::F32X4Floor:
      case Opcode::F64X2Floor:
      case Opcode::F32X4Trunc:
      case Opcode::F64X2Trunc:
      case Opcode::F32X4Nearest:
      case Opcode::F64X2Nearest:
      case Opcode::F32X4Neg:
      case Opcode::F64X2Neg:
      case Opcode::F32X4Abs:
      case Opcode::F64X2Abs:
      case Opcode::F32X4Sqrt:
      case Opcode::F64X2Sqrt:
      case Opcode::I16X8ExtendLowI8X16S:
      case Opcode::I16X8ExtendHighI8X16S:
      case Opcode::I16X8ExtendLowI8X16U:
      case Opcode::I16X8ExtendHighI8X16U:
      case Opcode::I32X4ExtendLowI16X8S:
      case Opcode::I32X4ExtendHighI16X8S:
      case Opcode::I32X4ExtendLowI16X8U:
      case Opcode::I32X4ExtendHighI16X8U:
      case Opcode::I64X2ExtendLowI32X4S:
      case Opcode::I64X2ExtendHighI32X4S:
      case Opcode::I64X2ExtendLowI32X4U:
      case Opcode::I64X2ExtendHighI32X4U:
      case Opcode::I8X16Abs:
      case Opcode::I16X8Abs:
      case Opcode::I32X4Abs:
      case Opcode::I64X2Abs:
      case Opcode::I8X16Popcnt:
      case Opcode::I16X8ExtaddPairwiseI8X16S:
      case Opcode::I16X8ExtaddPairwiseI8X16U:
      case Opcode::I32X4ExtaddPairwiseI16X8S:
      case Opcode::I32X4ExtaddPairwiseI16X8U:
      case Opcode::I32X4RelaxedTruncF32X4S:
      case Opcode::I32X4RelaxedTruncF32X4U:
      case Opcode::I32X4RelaxedTruncF64X2SZero:
      case Opcode::I32X4RelaxedTruncF64X2UZero:
        CALLBACK(OnUnaryExpr, opcode);
        CALLBACK0(OnOpcodeBare);
        break;

      case Opcode::V128BitSelect:
      case Opcode::F32X4RelaxedMadd:
      case Opcode::F32X4RelaxedNmadd:
      case Opcode::F64X2RelaxedMadd:
      case Opcode::F64X2RelaxedNmadd:
      case Opcode::I8X16RelaxedLaneSelect:
      case Opcode::I16X8RelaxedLaneSelect:
      case Opcode::I32X4RelaxedLaneSelect:
      case Opcode::I64X2RelaxedLaneSelect:
      case Opcode::I32X4DotI8X16I7X16AddS:
        CALLBACK(OnTernaryExpr, opcode);
        CALLBACK0(OnOpcodeBare);
        break;

      case Opcode::I8X16ExtractLaneS:
      case Opcode::I8X16ExtractLaneU:
      case Opcode::I16X8ExtractLaneS:
      case Opcode::I16X8ExtractLaneU:
      case Opcode::I32X4ExtractLane:
      case Opcode::I64X2ExtractLane:
      case Opcode::F32X4ExtractLane:
      case Opcode::F64X2ExtractLane:
      case Opcode::I8X16ReplaceLane:
      case Opcode::I16X8ReplaceLane:
      case Opcode::I32X4ReplaceLane:
      case Opcode::I64X2ReplaceLane:
      case Opcode::F32X4ReplaceLane:
      case Opcode::F64X2ReplaceLane: {
        uint8_t lane_val;
        CHECK_RESULT(ReadU8(&lane_val, "Lane idx"));
        CALLBACK(OnSimdLaneOpExpr, opcode, lane_val);
        CALLBACK(OnOpcodeUint64, lane_val);
        break;
      }

      case Opcode::I8X16Shuffle: {
        v128 value;
        CHECK_RESULT(ReadV128(&value, "Lane idx [16]"));
        CALLBACK(OnSimdShuffleOpExpr, opcode, value);
        CALLBACK(OnOpcodeV128, value);
        break;
      }

      case Opcode::V128Load8Splat:
      case Opcode::V128Load16Splat:
      case Opcode::V128Load32Splat:
      case Opcode::V128Load64Splat: {
        Address alignment_log2;
        Index memidx;
        Address offset;
        CHECK_RESULT(ReadMemLocation(&alignment_log2, &memidx, &offset,
                                     "load alignment", "load memidx",
                                     "load offset"));
        CALLBACK(OnLoadSplatExpr, opcode, memidx, alignment_log2, offset);
        CHECK_RESULT(CallbackMemLocation(&alignment_log2, &memidx, &offset));
        break;
      }
      case Opcode::V128Load8Lane:
      case Opcode::V128Load16Lane:
      case Opcode::V128Load32Lane:
      case Opcode::V128Load64Lane: {
        Address alignment_log2;
        Index memidx;
        Address offset;
        uint8_t lane_val;
        CHECK_RESULT(ReadMemLocation(&alignment_log2, &memidx, &offset,
                                     "load alignment", "load memidx",
                                     "load offset", &lane_val));
        CALLBACK(OnSimdLoadLaneExpr, opcode, memidx, alignment_log2, offset,
                 lane_val);
        CHECK_RESULT(
            CallbackMemLocation(&alignment_log2, &memidx, &offset, &lane_val));
        break;
      }
      case Opcode::V128Store8Lane:
      case Opcode::V128Store16Lane:
      case Opcode::V128Store32Lane:
      case Opcode::V128Store64Lane: {
        Address alignment_log2;
        Index memidx;
        Address offset;
        uint8_t lane_val;
        CHECK_RESULT(ReadMemLocation(&alignment_log2, &memidx, &offset,
                                     "store alignment", "store memidx",
                                     "store offset", &lane_val));
        CALLBACK(OnSimdStoreLaneExpr, opcode, memidx, alignment_log2, offset,
                 lane_val);
        CHECK_RESULT(
            CallbackMemLocation(&alignment_log2, &memidx, &offset, &lane_val));
        break;
      }
      case Opcode::V128Load32Zero:
      case Opcode::V128Load64Zero: {
        Address alignment_log2;
        Index memidx;
        Address offset;
        CHECK_RESULT(ReadMemLocation(&alignment_log2, &memidx, &offset,
                                     "load alignment", "load memidx",
                                     "load offset"));
        CALLBACK(OnLoadZeroExpr, opcode, memidx, alignment_log2, offset);
        CHECK_RESULT(CallbackMemLocation(&alignment_log2, &memidx, &offset));
        break;
      }
      case Opcode::I32TruncF32S:
      case Opcode::I32TruncF64S:
      case Opcode::I32TruncF32U:
      case Opcode::I32TruncF64U:
      case Opcode::I32WrapI64:
      case Opcode::I64TruncF32S:
      case Opcode::I64TruncF64S:
      case Opcode::I64TruncF32U:
      case Opcode::I64TruncF64U:
      case Opcode::I64ExtendI32S:
      case Opcode::I64ExtendI32U:
      case Opcode::F32ConvertI32S:
      case Opcode::F32ConvertI32U:
      case Opcode::F32ConvertI64S:
      case Opcode::F32ConvertI64U:
      case Opcode::F32DemoteF64:
      case Opcode::F32ReinterpretI32:
      case Opcode::F64ConvertI32S:
      case Opcode::F64ConvertI32U:
      case Opcode::F64ConvertI64S:
      case Opcode::F64ConvertI64U:
      case Opcode::F64PromoteF32:
      case Opcode::F64ReinterpretI64:
      case Opcode::I32ReinterpretF32:
      case Opcode::I64ReinterpretF64:
      case Opcode::I32Eqz:
      case Opcode::I64Eqz:
      case Opcode::F32X4ConvertI32X4S:
      case Opcode::F32X4ConvertI32X4U:
      case Opcode::I32X4TruncSatF32X4S:
      case Opcode::I32X4TruncSatF32X4U:
      case Opcode::F32X4DemoteF64X2Zero:
      case Opcode::F64X2PromoteLowF32X4:
      case Opcode::I32X4TruncSatF64X2SZero:
      case Opcode::I32X4TruncSatF64X2UZero:
      case Opcode::F64X2ConvertLowI32X4S:
      case Opcode::F64X2ConvertLowI32X4U:
        CALLBACK(OnConvertExpr, opcode);
        CALLBACK0(OnOpcodeBare);
        break;

      case Opcode::Try: {
        Type sig_type;
        CHECK_RESULT(ReadType(&sig_type, "try signature type"));
        ERROR_UNLESS(IsBlockType(sig_type),
                     "expected valid block signature type");
        CALLBACK(OnTryExpr, sig_type);
        CALLBACK(OnOpcodeBlockSig, sig_type);
        break;
      }

      case Opcode::Catch: {
        Index index;
        CHECK_RESULT(ReadIndex(&index, "tag index"));
        CALLBACK(OnCatchExpr, index);
        CALLBACK(OnOpcodeIndex, index);
        break;
      }

      case Opcode::CatchAll: {
        CALLBACK(OnCatchAllExpr);
        CALLBACK(OnOpcodeBare);
        break;
      }

      case Opcode::Delegate: {
        Index index;
        CHECK_RESULT(ReadIndex(&index, "depth"));
        CALLBACK(OnDelegateExpr, index);
        CALLBACK(OnOpcodeIndex, index);
        break;
      }

      case Opcode::Rethrow: {
        Index depth;
        CHECK_RESULT(ReadIndex(&depth, "catch depth"));
        CALLBACK(OnRethrowExpr, depth);
        CALLBACK(OnOpcodeIndex, depth);
        break;
      }

      case Opcode::Throw: {
        Index index;
        CHECK_RESULT(ReadIndex(&index, "tag index"));
        CALLBACK(OnThrowExpr, index);
        CALLBACK(OnOpcodeIndex, index);
        break;
      }

      case Opcode::I32Extend8S:
      case Opcode::I32Extend16S:
      case Opcode::I64Extend8S:
      case Opcode::I64Extend16S:
      case Opcode::I64Extend32S:
        CALLBACK(OnUnaryExpr, opcode);
        CALLBACK0(OnOpcodeBare);
        break;

      case Opcode::I32TruncSatF32S:
      case Opcode::I32TruncSatF32U:
      case Opcode::I32TruncSatF64S:
      case Opcode::I32TruncSatF64U:
      case Opcode::I64TruncSatF32S:
      case Opcode::I64TruncSatF32U:
      case Opcode::I64TruncSatF64S:
      case Opcode::I64TruncSatF64U:
        CALLBACK(OnConvertExpr, opcode);
        CALLBACK0(OnOpcodeBare);
        break;

      case Opcode::MemoryAtomicNotify: {
        Address alignment_log2;
        Index memidx;
        Address offset;
        CHECK_RESULT(ReadMemLocation(&alignment_log2, &memidx, &offset,
                                     "notify alignment", "notify memidx",
                                     "notify offset"));
        CALLBACK(OnAtomicNotifyExpr, opcode, memidx, alignment_log2, offset);
        CHECK_RESULT(CallbackMemLocation(&alignment_log2, &memidx, &offset));
        break;
      }

      case Opcode::MemoryAtomicWait32:
      case Opcode::MemoryAtomicWait64: {
        Address alignment_log2;
        Index memidx;
        Address offset;
        CHECK_RESULT(ReadMemLocation(&alignment_log2, &memidx, &offset,
                                     "wait alignment", "wait memidx",
                                     "wait offset"));
        CALLBACK(OnAtomicWaitExpr, opcode, memidx, alignment_log2, offset);
        CHECK_RESULT(CallbackMemLocation(&alignment_log2, &memidx, &offset));
        break;
      }

      case Opcode::AtomicFence: {
        uint8_t consistency_model;
        CHECK_RESULT(ReadU8(&consistency_model, "consistency model"));
        ERROR_UNLESS(consistency_model == 0,
                     "atomic.fence consistency model must be 0");
        CALLBACK(OnAtomicFenceExpr, consistency_model);
        CALLBACK(OnOpcodeUint32, consistency_model);
        break;
      }

      case Opcode::I32AtomicLoad8U:
      case Opcode::I32AtomicLoad16U:
      case Opcode::I64AtomicLoad8U:
      case Opcode::I64AtomicLoad16U:
      case Opcode::I64AtomicLoad32U:
      case Opcode::I32AtomicLoad:
      case Opcode::I64AtomicLoad: {
        Address alignment_log2;
        Index memidx;
        Address offset;
        CHECK_RESULT(ReadMemLocation(&alignment_log2, &memidx, &offset,
                                     "load alignment", "load memidx",
                                     "load offset"));
        CALLBACK(OnAtomicLoadExpr, opcode, memidx, alignment_log2, offset);
        CHECK_RESULT(CallbackMemLocation(&alignment_log2, &memidx, &offset));
        break;
      }

      case Opcode::I32AtomicStore8:
      case Opcode::I32AtomicStore16:
      case Opcode::I64AtomicStore8:
      case Opcode::I64AtomicStore16:
      case Opcode::I64AtomicStore32:
      case Opcode::I32AtomicStore:
      case Opcode::I64AtomicStore: {
        Address alignment_log2;
        Index memidx;
        Address offset;
        CHECK_RESULT(ReadMemLocation(&alignment_log2, &memidx, &offset,
                                     "store alignment", "store memidx",
                                     "store offset"));
        CALLBACK(OnAtomicStoreExpr, opcode, memidx, alignment_log2, offset);
        CHECK_RESULT(CallbackMemLocation(&alignment_log2, &memidx, &offset));
        break;
      }

      case Opcode::I32AtomicRmwAdd:
      case Opcode::I64AtomicRmwAdd:
      case Opcode::I32AtomicRmw8AddU:
      case Opcode::I32AtomicRmw16AddU:
      case Opcode::I64AtomicRmw8AddU:
      case Opcode::I64AtomicRmw16AddU:
      case Opcode::I64AtomicRmw32AddU:
      case Opcode::I32AtomicRmwSub:
      case Opcode::I64AtomicRmwSub:
      case Opcode::I32AtomicRmw8SubU:
      case Opcode::I32AtomicRmw16SubU:
      case Opcode::I64AtomicRmw8SubU:
      case Opcode::I64AtomicRmw16SubU:
      case Opcode::I64AtomicRmw32SubU:
      case Opcode::I32AtomicRmwAnd:
      case Opcode::I64AtomicRmwAnd:
      case Opcode::I32AtomicRmw8AndU:
      case Opcode::I32AtomicRmw16AndU:
      case Opcode::I64AtomicRmw8AndU:
      case Opcode::I64AtomicRmw16AndU:
      case Opcode::I64AtomicRmw32AndU:
      case Opcode::I32AtomicRmwOr:
      case Opcode::I64AtomicRmwOr:
      case Opcode::I32AtomicRmw8OrU:
      case Opcode::I32AtomicRmw16OrU:
      case Opcode::I64AtomicRmw8OrU:
      case Opcode::I64AtomicRmw16OrU:
      case Opcode::I64AtomicRmw32OrU:
      case Opcode::I32AtomicRmwXor:
      case Opcode::I64AtomicRmwXor:
      case Opcode::I32AtomicRmw8XorU:
      case Opcode::I32AtomicRmw16XorU:
      case Opcode::I64AtomicRmw8XorU:
      case Opcode::I64AtomicRmw16XorU:
      case Opcode::I64AtomicRmw32XorU:
      case Opcode::I32AtomicRmwXchg:
      case Opcode::I64AtomicRmwXchg:
      case Opcode::I32AtomicRmw8XchgU:
      case Opcode::I32AtomicRmw16XchgU:
      case Opcode::I64AtomicRmw8XchgU:
      case Opcode::I64AtomicRmw16XchgU:
      case Opcode::I64AtomicRmw32XchgU: {
        Address alignment_log2;
        Index memidx;
        Address offset;
        CHECK_RESULT(ReadMemLocation(&alignment_log2, &memidx, &offset,
                                     "memory alignment", "memory memidx",
                                     "memory offset"));
        CALLBACK(OnAtomicRmwExpr, opcode, memidx, alignment_log2, offset);
        CHECK_RESULT(CallbackMemLocation(&alignment_log2, &memidx, &offset));
        break;
      }

      case Opcode::I32AtomicRmwCmpxchg:
      case Opcode::I64AtomicRmwCmpxchg:
      case Opcode::I32AtomicRmw8CmpxchgU:
      case Opcode::I32AtomicRmw16CmpxchgU:
      case Opcode::I64AtomicRmw8CmpxchgU:
      case Opcode::I64AtomicRmw16CmpxchgU:
      case Opcode::I64AtomicRmw32CmpxchgU: {
        Address alignment_log2;
        Index memidx;
        Address offset;
        CHECK_RESULT(ReadMemLocation(&alignment_log2, &memidx, &offset,
                                     "memory alignment", "memory memidx",
                                     "memory offset"));
        CALLBACK(OnAtomicRmwCmpxchgExpr, opcode, memidx, alignment_log2,
                 offset);
        CHECK_RESULT(CallbackMemLocation(&alignment_log2, &memidx, &offset));
        break;
      }

      case Opcode::TableInit: {
        Index segment;
        CHECK_RESULT(ReadIndex(&segment, "elem segment index"));
        Index table_index;
        CHECK_RESULT(ReadIndex(&table_index, "reserved table index"));
        CALLBACK(OnTableInitExpr, segment, table_index);
        CALLBACK(OnOpcodeUint32Uint32, segment, table_index);
        break;
      }

      case Opcode::MemoryInit: {
        Index segment;
        ERROR_IF(data_count_ == kInvalidIndex,
                 "memory.init requires data count section");
        CHECK_RESULT(ReadIndex(&segment, "elem segment index"));
        Index memidx = 0;
        if (!options_.features.multi_memory_enabled()) {
          uint8_t reserved;
          CHECK_RESULT(ReadU8(&reserved, "reserved memory index"));
          ERROR_UNLESS(reserved == 0, "reserved value must be 0");
        } else {
          CHECK_RESULT(ReadMemidx(&memidx, "memory.init memidx"));
        }
        CALLBACK(OnMemoryInitExpr, segment, memidx);
        CALLBACK(OnOpcodeUint32Uint32, segment, memidx);
        break;
      }

      case Opcode::DataDrop:
        ERROR_IF(data_count_ == kInvalidIndex,
                 "data.drop requires data count section");
        [[fallthrough]];
      case Opcode::ElemDrop: {
        Index segment;
        CHECK_RESULT(ReadIndex(&segment, "segment index"));
        if (opcode == Opcode::DataDrop) {
          CALLBACK(OnDataDropExpr, segment);
        } else {
          CALLBACK(OnElemDropExpr, segment);
        }
        CALLBACK(OnOpcodeUint32, segment);
        break;
      }

      case Opcode::MemoryFill: {
        Index memidx = 0;
        if (!options_.features.multi_memory_enabled()) {
          uint8_t reserved;
          CHECK_RESULT(ReadU8(&reserved, "memory.fill reserved"));
          ERROR_UNLESS(reserved == 0, "memory.fill reserved value must be 0");
        } else {
          CHECK_RESULT(ReadMemidx(&memidx, "memory.fill memidx"));
        }
        CALLBACK(OnMemoryFillExpr, memidx);
        CALLBACK(OnOpcodeUint32, memidx);
        break;
      }

      case Opcode::MemoryCopy: {
        Index srcmemidx = 0;
        Index destmemidx = 0;
        if (!options_.features.multi_memory_enabled()) {
          uint8_t reserved;
          CHECK_RESULT(ReadU8(&reserved, "reserved memory index"));
          ERROR_UNLESS(reserved == 0, "reserved value must be 0");
          CHECK_RESULT(ReadU8(&reserved, "reserved memory index"));
          ERROR_UNLESS(reserved == 0, "reserved value must be 0");
        } else {
          CHECK_RESULT(ReadMemidx(&srcmemidx, "memory.copy srcmemidx"));
          CHECK_RESULT(ReadMemidx(&destmemidx, "memory.copy destmemindex"));
        }
        CALLBACK(OnMemoryCopyExpr, srcmemidx, destmemidx);
        CALLBACK(OnOpcodeUint32Uint32, srcmemidx, destmemidx);
        break;
      }

      case Opcode::TableCopy: {
        Index table_dst;
        Index table_src;
        CHECK_RESULT(ReadIndex(&table_dst, "reserved table index"));
        CHECK_RESULT(ReadIndex(&table_src, "table src"));
        CALLBACK(OnTableCopyExpr, table_dst, table_src);
        CALLBACK(OnOpcodeUint32Uint32, table_dst, table_src);
        break;
      }

      case Opcode::TableGet: {
        Index table;
        CHECK_RESULT(ReadIndex(&table, "table index"));
        CALLBACK(OnTableGetExpr, table);
        CALLBACK(OnOpcodeUint32, table);
        break;
      }

      case Opcode::TableSet: {
        Index table;
        CHECK_RESULT(ReadIndex(&table, "table index"));
        CALLBACK(OnTableSetExpr, table);
        CALLBACK(OnOpcodeUint32, table);
        break;
      }

      case Opcode::TableGrow: {
        Index table;
        CHECK_RESULT(ReadIndex(&table, "table index"));
        CALLBACK(OnTableGrowExpr, table);
        CALLBACK(OnOpcodeUint32, table);
        break;
      }

      case Opcode::TableSize: {
        Index table;
        CHECK_RESULT(ReadIndex(&table, "table index"));
        CALLBACK(OnTableSizeExpr, table);
        CALLBACK(OnOpcodeUint32, table);
        break;
      }

      case Opcode::TableFill: {
        Index table;
        CHECK_RESULT(ReadIndex(&table, "table index"));
        CALLBACK(OnTableFillExpr, table);
        CALLBACK(OnOpcodeUint32, table);
        break;
      }

      case Opcode::RefFunc: {
        Index func;
        CHECK_RESULT(ReadIndex(&func, "func index"));
        CALLBACK(OnRefFuncExpr, func);
        CALLBACK(OnOpcodeUint32, func);
        break;
      }

      case Opcode::RefNull: {
        Type type;
        CHECK_RESULT(ReadRefType(&type, "ref.null type"));
        CALLBACK(OnRefNullExpr, type);
        CALLBACK(OnOpcodeType, type);
        break;
      }

      case Opcode::RefIsNull:
        CALLBACK(OnRefIsNullExpr);
        CALLBACK(OnOpcodeBare);
        break;

      case Opcode::CallRef:
        CALLBACK(OnCallRefExpr);
        CALLBACK(OnOpcodeBare);
        break;

      default:
        return ReportUnexpectedOpcode(opcode);
    }
  }
  return Result::Ok;
}

Result BinaryReader::ReadNameSection(Offset section_size) {
  CALLBACK(BeginNamesSection, section_size);
  Index i = 0;
  uint32_t previous_subsection_type = 0;
  while (state_.offset < read_end_) {
    uint32_t name_type;
    Offset subsection_size;
    CHECK_RESULT(ReadU32Leb128(&name_type, "name type"));
    if (i != 0) {
      ERROR_UNLESS(name_type != previous_subsection_type,
                   "duplicate sub-section");
      ERROR_UNLESS(name_type >= previous_subsection_type,
                   "out-of-order sub-section");
    }
    previous_subsection_type = name_type;
    CHECK_RESULT(ReadOffset(&subsection_size, "subsection size"));
    size_t subsection_end = state_.offset + subsection_size;
    ERROR_UNLESS(subsection_end <= read_end_,
                 "invalid sub-section size: extends past end");
    ReadEndRestoreGuard guard(this);
    read_end_ = subsection_end;

    NameSectionSubsection type = static_cast<NameSectionSubsection>(name_type);
    if (type <= NameSectionSubsection::Last) {
      CALLBACK(OnNameSubsection, i, type, subsection_size);
    }

    switch (type) {
      case NameSectionSubsection::Module:
        CALLBACK(OnModuleNameSubsection, i, name_type, subsection_size);
        if (subsection_size) {
          std::string_view name;
          CHECK_RESULT(ReadStr(&name, "module name"));
          CALLBACK(OnModuleName, name);
        }
        break;
      case NameSectionSubsection::Function:
        CALLBACK(OnFunctionNameSubsection, i, name_type, subsection_size);
        if (subsection_size) {
          Index num_names;
          CHECK_RESULT(ReadCount(&num_names, "name count"));
          CALLBACK(OnFunctionNamesCount, num_names);
          Index last_function_index = kInvalidIndex;

          for (Index j = 0; j < num_names; ++j) {
            Index function_index;
            std::string_view function_name;

            CHECK_RESULT(ReadIndex(&function_index, "function index"));
            ERROR_UNLESS(function_index != last_function_index,
                         "duplicate function name: %u", function_index);
            ERROR_UNLESS(last_function_index == kInvalidIndex ||
                             function_index > last_function_index,
                         "function index out of order: %u", function_index);
            last_function_index = function_index;
            ERROR_UNLESS(function_index < NumTotalFuncs(),
                         "invalid function index: %" PRIindex, function_index);
            CHECK_RESULT(ReadStr(&function_name, "function name"));
            CALLBACK(OnFunctionName, function_index, function_name);
          }
        }
        break;
      case NameSectionSubsection::Local:
        CALLBACK(OnLocalNameSubsection, i, name_type, subsection_size);
        if (subsection_size) {
          Index num_funcs;
          CHECK_RESULT(ReadCount(&num_funcs, "function count"));
          CALLBACK(OnLocalNameFunctionCount, num_funcs);
          Index last_function_index = kInvalidIndex;
          for (Index j = 0; j < num_funcs; ++j) {
            Index function_index;
            CHECK_RESULT(ReadIndex(&function_index, "function index"));
            ERROR_UNLESS(function_index < NumTotalFuncs(),
                         "invalid function index: %u", function_index);
            ERROR_UNLESS(last_function_index == kInvalidIndex ||
                             function_index > last_function_index,
                         "locals function index out of order: %u",
                         function_index);
            last_function_index = function_index;
            Index num_locals;
            CHECK_RESULT(ReadCount(&num_locals, "local count"));
            CALLBACK(OnLocalNameLocalCount, function_index, num_locals);
            Index last_local_index = kInvalidIndex;
            for (Index k = 0; k < num_locals; ++k) {
              Index local_index;
              std::string_view local_name;

              CHECK_RESULT(ReadIndex(&local_index, "named index"));
              ERROR_UNLESS(local_index != last_local_index,
                           "duplicate local index: %u", local_index);
              ERROR_UNLESS(last_local_index == kInvalidIndex ||
                               local_index > last_local_index,
                           "local index out of order: %u", local_index);
              last_local_index = local_index;
              CHECK_RESULT(ReadStr(&local_name, "name"));
              CALLBACK(OnLocalName, function_index, local_index, local_name);
            }
          }
        }
        break;
      case NameSectionSubsection::Label:
        // TODO(sbc): Implement label names. These are slightly more complicated
        // since they refer to offsets in the code section / instruction stream.
        state_.offset = subsection_end;
        break;
      case NameSectionSubsection::Type:
      case NameSectionSubsection::Table:
      case NameSectionSubsection::Memory:
      case NameSectionSubsection::Global:
      case NameSectionSubsection::ElemSegment:
      case NameSectionSubsection::DataSegment:
      case NameSectionSubsection::Tag:
        if (subsection_size) {
          Index num_names;
          CHECK_RESULT(ReadCount(&num_names, "name count"));
          CALLBACK(OnNameCount, num_names);
          for (Index j = 0; j < num_names; ++j) {
            Index index;
            std::string_view name;

            CHECK_RESULT(ReadIndex(&index, "index"));
            CHECK_RESULT(ReadStr(&name, "name"));
            CALLBACK(OnNameEntry, type, index, name);
          }
        }
        state_.offset = subsection_end;
        break;
      default:
        // Unknown subsection, skip it.
        state_.offset = subsection_end;
        break;
    }
    ++i;
    ERROR_UNLESS(state_.offset == subsection_end,
                 "unfinished sub-section (expected end: 0x%" PRIzx ")",
                 subsection_end);
  }
  CALLBACK0(EndNamesSection);
  return Result::Ok;
}

Result BinaryReader::ReadRelocSection(Offset section_size) {
  CALLBACK(BeginRelocSection, section_size);
  uint32_t section_index;
  CHECK_RESULT(ReadU32Leb128(&section_index, "section index"));
  Index num_relocs;
  CHECK_RESULT(ReadCount(&num_relocs, "relocation count"));
  CALLBACK(OnRelocCount, num_relocs, section_index);
  for (Index i = 0; i < num_relocs; ++i) {
    Offset offset;
    Index index;
    uint32_t reloc_type, addend = 0;
    CHECK_RESULT(ReadU32Leb128(&reloc_type, "relocation type"));
    CHECK_RESULT(ReadOffset(&offset, "offset"));
    CHECK_RESULT(ReadIndex(&index, "index"));
    RelocType type = static_cast<RelocType>(reloc_type);
    switch (type) {
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
        CHECK_RESULT(ReadS32Leb128(&addend, "addend"));
        break;

      case RelocType::FuncIndexLEB:
      case RelocType::TableIndexSLEB:
      case RelocType::TableIndexSLEB64:
      case RelocType::TableIndexI32:
      case RelocType::TableIndexI64:
      case RelocType::TypeIndexLEB:
      case RelocType::GlobalIndexLEB:
      case RelocType::GlobalIndexI32:
      case RelocType::TagIndexLEB:
      case RelocType::TableIndexRelSLEB:
      case RelocType::TableNumberLEB:
        break;

      default:
        PrintError("unknown reloc type: %s", GetRelocTypeName(type));
        return Result::Error;
    }
    CALLBACK(OnReloc, type, offset, index, addend);
  }
  CALLBACK0(EndRelocSection);
  return Result::Ok;
}

Result BinaryReader::ReadDylink0Section(Offset section_size) {
  CALLBACK(BeginDylinkSection, section_size);

  while (state_.offset < read_end_) {
    uint32_t dylink_type;
    Offset subsection_size;
    CHECK_RESULT(ReadU32Leb128(&dylink_type, "type"));
    CHECK_RESULT(ReadOffset(&subsection_size, "subsection size"));
    size_t subsection_end = state_.offset + subsection_size;
    ERROR_UNLESS(subsection_end <= read_end_,
                 "invalid sub-section size: extends past end");
    ReadEndRestoreGuard guard(this);
    read_end_ = subsection_end;

    uint32_t count;
    switch (static_cast<DylinkEntryType>(dylink_type)) {
      case DylinkEntryType::MemInfo: {
        uint32_t mem_size;
        uint32_t mem_align;
        uint32_t table_size;
        uint32_t table_align;

        CHECK_RESULT(ReadU32Leb128(&mem_size, "mem_size"));
        CHECK_RESULT(ReadU32Leb128(&mem_align, "mem_align"));
        CHECK_RESULT(ReadU32Leb128(&table_size, "table_size"));
        CHECK_RESULT(ReadU32Leb128(&table_align, "table_align"));
        CALLBACK(OnDylinkInfo, mem_size, mem_align, table_size, table_align);
        break;
      }
      case DylinkEntryType::Needed:
        CHECK_RESULT(ReadU32Leb128(&count, "needed_dynlibs"));
        CALLBACK(OnDylinkNeededCount, count);
        while (count--) {
          std::string_view so_name;
          CHECK_RESULT(ReadStr(&so_name, "dylib so_name"));
          CALLBACK(OnDylinkNeeded, so_name);
        }
        break;
      case DylinkEntryType::ImportInfo:
        CHECK_RESULT(ReadU32Leb128(&count, "count"));
        CALLBACK(OnDylinkImportCount, count);
        for (Index i = 0; i < count; ++i) {
          uint32_t flags = 0;
          std::string_view module;
          std::string_view field;
          CHECK_RESULT(ReadStr(&module, "module"));
          CHECK_RESULT(ReadStr(&field, "field"));
          CHECK_RESULT(ReadU32Leb128(&flags, "flags"));
          CALLBACK(OnDylinkImport, module, field, flags);
        }
        break;
      case DylinkEntryType::ExportInfo:
        CHECK_RESULT(ReadU32Leb128(&count, "count"));
        CALLBACK(OnDylinkExportCount, count);
        for (Index i = 0; i < count; ++i) {
          uint32_t flags = 0;
          std::string_view name;
          CHECK_RESULT(ReadStr(&name, "name"));
          CHECK_RESULT(ReadU32Leb128(&flags, "flags"));
          CALLBACK(OnDylinkExport, name, flags);
        }
        break;
      default:
        // Unknown subsection, skip it.
        state_.offset = subsection_end;
        break;
    }
    ERROR_UNLESS(state_.offset == subsection_end,
                 "unfinished sub-section (expected end: 0x%" PRIzx ")",
                 subsection_end);
  }

  CALLBACK0(EndDylinkSection);
  return Result::Ok;
}

Result BinaryReader::ReadDylinkSection(Offset section_size) {
  CALLBACK(BeginDylinkSection, section_size);
  uint32_t mem_size;
  uint32_t mem_align;
  uint32_t table_size;
  uint32_t table_align;

  CHECK_RESULT(ReadU32Leb128(&mem_size, "mem_size"));
  CHECK_RESULT(ReadU32Leb128(&mem_align, "mem_align"));
  CHECK_RESULT(ReadU32Leb128(&table_size, "table_size"));
  CHECK_RESULT(ReadU32Leb128(&table_align, "table_align"));
  CALLBACK(OnDylinkInfo, mem_size, mem_align, table_size, table_align);

  uint32_t count;
  CHECK_RESULT(ReadU32Leb128(&count, "needed_dynlibs"));
  CALLBACK(OnDylinkNeededCount, count);
  while (count--) {
    std::string_view so_name;
    CHECK_RESULT(ReadStr(&so_name, "dylib so_name"));
    CALLBACK(OnDylinkNeeded, so_name);
  }

  CALLBACK0(EndDylinkSection);
  return Result::Ok;
}

Result BinaryReader::ReadTargetFeaturesSections(Offset section_size) {
  CALLBACK(BeginTargetFeaturesSection, section_size);
  uint32_t count;
  CHECK_RESULT(ReadU32Leb128(&count, "sym count"));
  CALLBACK(OnFeatureCount, count);
  while (count--) {
    uint8_t prefix;
    std::string_view name;
    CHECK_RESULT(ReadU8(&prefix, "prefix"));
    CHECK_RESULT(ReadStr(&name, "feature name"));
    CALLBACK(OnFeature, prefix, name);
  }
  CALLBACK0(EndTargetFeaturesSection);
  return Result::Ok;
}

Result BinaryReader::ReadLinkingSection(Offset section_size) {
  CALLBACK(BeginLinkingSection, section_size);
  uint32_t version;
  CHECK_RESULT(ReadU32Leb128(&version, "version"));
  ERROR_UNLESS(version == 2, "invalid linking metadata version: %u", version);
  while (state_.offset < read_end_) {
    uint32_t linking_type;
    Offset subsection_size;
    CHECK_RESULT(ReadU32Leb128(&linking_type, "type"));
    CHECK_RESULT(ReadOffset(&subsection_size, "subsection size"));
    size_t subsection_end = state_.offset + subsection_size;
    ERROR_UNLESS(subsection_end <= read_end_,
                 "invalid sub-section size: extends past end");
    ReadEndRestoreGuard guard(this);
    read_end_ = subsection_end;

    uint32_t count;
    switch (static_cast<LinkingEntryType>(linking_type)) {
      case LinkingEntryType::SymbolTable:
        CHECK_RESULT(ReadU32Leb128(&count, "sym count"));
        CALLBACK(OnSymbolCount, count);
        for (Index i = 0; i < count; ++i) {
          std::string_view name;
          uint32_t flags = 0;
          uint32_t kind = 0;
          CHECK_RESULT(ReadU32Leb128(&kind, "sym type"));
          CHECK_RESULT(ReadU32Leb128(&flags, "sym flags"));
          SymbolType sym_type = static_cast<SymbolType>(kind);
          switch (sym_type) {
            case SymbolType::Function:
            case SymbolType::Global:
            case SymbolType::Tag:
            case SymbolType::Table: {
              uint32_t index = 0;
              CHECK_RESULT(ReadU32Leb128(&index, "index"));
              if ((flags & WABT_SYMBOL_FLAG_UNDEFINED) == 0 ||
                  (flags & WABT_SYMBOL_FLAG_EXPLICIT_NAME) != 0)
                CHECK_RESULT(ReadStr(&name, "symbol name"));
              switch (sym_type) {
                case SymbolType::Function:
                  CALLBACK(OnFunctionSymbol, i, flags, name, index);
                  break;
                case SymbolType::Global:
                  CALLBACK(OnGlobalSymbol, i, flags, name, index);
                  break;
                case SymbolType::Tag:
                  CALLBACK(OnTagSymbol, i, flags, name, index);
                  break;
                case SymbolType::Table:
                  CALLBACK(OnTableSymbol, i, flags, name, index);
                  break;
                default:
                  WABT_UNREACHABLE;
              }
              break;
            }
            case SymbolType::Data: {
              uint32_t segment = 0;
              uint32_t offset = 0;
              uint32_t size = 0;
              CHECK_RESULT(ReadStr(&name, "symbol name"));
              if ((flags & WABT_SYMBOL_FLAG_UNDEFINED) == 0) {
                CHECK_RESULT(ReadU32Leb128(&segment, "segment"));
                CHECK_RESULT(ReadU32Leb128(&offset, "offset"));
                CHECK_RESULT(ReadU32Leb128(&size, "size"));
              }
              CALLBACK(OnDataSymbol, i, flags, name, segment, offset, size);
              break;
            }
            case SymbolType::Section: {
              uint32_t index = 0;
              CHECK_RESULT(ReadU32Leb128(&index, "index"));
              CALLBACK(OnSectionSymbol, i, flags, index);
              break;
            }
          }
        }
        break;
      case LinkingEntryType::SegmentInfo:
        CHECK_RESULT(ReadU32Leb128(&count, "info count"));
        CALLBACK(OnSegmentInfoCount, count);
        for (Index i = 0; i < count; i++) {
          std::string_view name;
          Address alignment_log2;
          uint32_t flags;
          CHECK_RESULT(ReadStr(&name, "segment name"));
          CHECK_RESULT(ReadAlignment(&alignment_log2, "segment alignment"));
          CHECK_RESULT(ReadU32Leb128(&flags, "segment flags"));
          CALLBACK(OnSegmentInfo, i, name, alignment_log2, flags);
        }
        break;
      case LinkingEntryType::InitFunctions:
        CHECK_RESULT(ReadU32Leb128(&count, "info count"));
        CALLBACK(OnInitFunctionCount, count);
        while (count--) {
          uint32_t priority;
          uint32_t symbol;
          CHECK_RESULT(ReadU32Leb128(&priority, "priority"));
          CHECK_RESULT(ReadU32Leb128(&symbol, "symbol index"));
          CALLBACK(OnInitFunction, priority, symbol);
        }
        break;
      case LinkingEntryType::ComdatInfo:
        CHECK_RESULT(ReadU32Leb128(&count, "count"));
        CALLBACK(OnComdatCount, count);
        while (count--) {
          uint32_t flags;
          uint32_t entry_count;
          std::string_view name;
          CHECK_RESULT(ReadStr(&name, "comdat name"));
          CHECK_RESULT(ReadU32Leb128(&flags, "flags"));
          CHECK_RESULT(ReadU32Leb128(&entry_count, "entry count"));
          CALLBACK(OnComdatBegin, name, flags, entry_count);
          while (entry_count--) {
            uint32_t kind;
            uint32_t index;
            CHECK_RESULT(ReadU32Leb128(&kind, "kind"));
            CHECK_RESULT(ReadU32Leb128(&index, "index"));
            ComdatType comdat_type = static_cast<ComdatType>(kind);
            CALLBACK(OnComdatEntry, comdat_type, index);
          }
        }
        break;
      default:
        // Unknown subsection, skip it.
        state_.offset = subsection_end;
        break;
    }
    ERROR_UNLESS(state_.offset == subsection_end,
                 "unfinished sub-section (expected end: 0x%" PRIzx ")",
                 subsection_end);
  }
  CALLBACK0(EndLinkingSection);
  return Result::Ok;
}

Result BinaryReader::ReadTagType(Index* out_sig_index) {
  uint8_t attribute;
  CHECK_RESULT(ReadU8(&attribute, "tag attribute"));
  ERROR_UNLESS(attribute == 0, "tag attribute must be 0");
  CHECK_RESULT(ReadIndex(out_sig_index, "tag signature index"));
  return Result::Ok;
}

Result BinaryReader::ReadTagSection(Offset section_size) {
  CALLBACK(BeginTagSection, section_size);
  Index num_tags;
  CHECK_RESULT(ReadCount(&num_tags, "tag count"));
  CALLBACK(OnTagCount, num_tags);

  for (Index i = 0; i < num_tags; ++i) {
    Index tag_index = num_tag_imports_ + i;
    Index sig_index;
    CHECK_RESULT(ReadTagType(&sig_index));
    CALLBACK(OnTagType, tag_index, sig_index);
  }

  CALLBACK(EndTagSection);
  return Result::Ok;
}

Result BinaryReader::ReadCodeMetadataSection(std::string_view name,
                                             Offset section_size) {
  CALLBACK(BeginCodeMetadataSection, name, section_size);

  Index num_functions;
  CHECK_RESULT(ReadCount(&num_functions, "function count"));
  CALLBACK(OnCodeMetadataFuncCount, num_functions);

  Index last_function_index = kInvalidIndex;
  for (Index i = 0; i < num_functions; ++i) {
    Index function_index;
    CHECK_RESULT(ReadCount(&function_index, "function index"));
    ERROR_UNLESS(function_index >= num_func_imports_,
                 "function import can't have metadata (got %" PRIindex ")",
                 function_index);
    ERROR_UNLESS(function_index < NumTotalFuncs(),
                 "invalid function index: %" PRIindex, function_index);
    ERROR_UNLESS(function_index != last_function_index,
                 "duplicate function index: %" PRIindex, function_index);
    ERROR_UNLESS(last_function_index == kInvalidIndex ||
                     function_index > last_function_index,
                 "function index out of order: %" PRIindex, function_index);
    last_function_index = function_index;

    Index num_metadata;
    CHECK_RESULT(ReadCount(&num_metadata, "metadata instances count"));

    CALLBACK(OnCodeMetadataCount, function_index, num_metadata);

    Offset last_code_offset = kInvalidOffset;
    for (Index j = 0; j < num_metadata; ++j) {
      Offset code_offset;
      CHECK_RESULT(ReadOffset(&code_offset, "code offset"));
      ERROR_UNLESS(code_offset != last_code_offset,
                   "duplicate code offset: %" PRIzx, code_offset);
      ERROR_UNLESS(
          last_code_offset == kInvalidOffset || code_offset > last_code_offset,
          "code offset out of order: %" PRIzx, code_offset);
      last_code_offset = code_offset;

      Address data_size;
      const void* data;
      CHECK_RESULT(ReadBytes(&data, &data_size, "instance data"));
      CALLBACK(OnCodeMetadata, code_offset, data, data_size);
    }
  }

  CALLBACK(EndCodeMetadataSection);
  return Result::Ok;
}

Result BinaryReader::ReadCustomSection(Index section_index,
                                       Offset section_size) {
  std::string_view section_name;
  CHECK_RESULT(ReadStr(&section_name, "section name"));
  CALLBACK(BeginCustomSection, section_index, section_size, section_name);
  ValueRestoreGuard<bool, &BinaryReader::reading_custom_section_> guard(this);
  reading_custom_section_ = true;

  if (options_.read_debug_names && section_name == WABT_BINARY_SECTION_NAME) {
    CHECK_RESULT(ReadNameSection(section_size));
    did_read_names_section_ = true;
  } else if (section_name == WABT_BINARY_SECTION_DYLINK0) {
    CHECK_RESULT(ReadDylink0Section(section_size));
  } else if (section_name == WABT_BINARY_SECTION_DYLINK) {
    CHECK_RESULT(ReadDylinkSection(section_size));
  } else if (section_name.rfind(WABT_BINARY_SECTION_RELOC, 0) == 0) {
    // Reloc sections always begin with "reloc."
    CHECK_RESULT(ReadRelocSection(section_size));
  } else if (section_name == WABT_BINARY_SECTION_TARGET_FEATURES) {
    CHECK_RESULT(ReadTargetFeaturesSections(section_size));
  } else if (section_name == WABT_BINARY_SECTION_LINKING) {
    CHECK_RESULT(ReadLinkingSection(section_size));
  } else if (options_.features.code_metadata_enabled() &&
             section_name.find(WABT_BINARY_SECTION_CODE_METADATA) == 0) {
    std::string_view metadata_name = section_name;
    metadata_name.remove_prefix(sizeof(WABT_BINARY_SECTION_CODE_METADATA) - 1);
    CHECK_RESULT(ReadCodeMetadataSection(metadata_name, section_size));
  } else {
    // This is an unknown custom section, skip it.
    state_.offset = read_end_;
  }
  CALLBACK0(EndCustomSection);
  return Result::Ok;
}

Result BinaryReader::ReadTypeSection(Offset section_size) {
  CALLBACK(BeginTypeSection, section_size);
  Index num_signatures;
  CHECK_RESULT(ReadCount(&num_signatures, "type count"));
  CALLBACK(OnTypeCount, num_signatures);

  for (Index i = 0; i < num_signatures; ++i) {
    Type form;
    if (options_.features.gc_enabled()) {
      CHECK_RESULT(ReadType(&form, "type form"));
    } else {
      uint8_t type;
      CHECK_RESULT(ReadU8(&type, "type form"));
      ERROR_UNLESS(type == 0x60, "unexpected type form (got %#x)", type);
      form = Type::Func;
    }

    switch (form) {
      case Type::Func: {
        Index num_params;
        CHECK_RESULT(ReadCount(&num_params, "function param count"));

        param_types_.resize(num_params);

        for (Index j = 0; j < num_params; ++j) {
          Type param_type;
          CHECK_RESULT(ReadType(&param_type, "function param type"));
          ERROR_UNLESS(IsConcreteType(param_type),
                       "expected valid param type (got " PRItypecode ")",
                       WABT_PRINTF_TYPE_CODE(param_type));
          param_types_[j] = param_type;
        }

        Index num_results;
        CHECK_RESULT(ReadCount(&num_results, "function result count"));

        result_types_.resize(num_results);

        for (Index j = 0; j < num_results; ++j) {
          Type result_type;
          CHECK_RESULT(ReadType(&result_type, "function result type"));
          ERROR_UNLESS(IsConcreteType(result_type),
                       "expected valid result type (got " PRItypecode ")",
                       WABT_PRINTF_TYPE_CODE(result_type));
          result_types_[j] = result_type;
        }

        Type* param_types = num_params ? param_types_.data() : nullptr;
        Type* result_types = num_results ? result_types_.data() : nullptr;

        CALLBACK(OnFuncType, i, num_params, param_types, num_results,
                 result_types);
        break;
      }

      case Type::Struct: {
        ERROR_UNLESS(options_.features.gc_enabled(),
                     "invalid type form: struct not allowed");
        Index num_fields;
        CHECK_RESULT(ReadCount(&num_fields, "field count"));

        fields_.resize(num_fields);
        for (Index j = 0; j < num_fields; ++j) {
          CHECK_RESULT(ReadField(&fields_[j]));
        }

        CALLBACK(OnStructType, i, fields_.size(), fields_.data());
        break;
      }

      case Type::Array: {
        ERROR_UNLESS(options_.features.gc_enabled(),
                     "invalid type form: array not allowed");

        TypeMut field;
        CHECK_RESULT(ReadField(&field));
        CALLBACK(OnArrayType, i, field);
        break;
      };

      default:
        PrintError("unexpected type form (got " PRItypecode ")",
                   WABT_PRINTF_TYPE_CODE(form));
        return Result::Error;
    }
  }
  CALLBACK0(EndTypeSection);
  return Result::Ok;
}

Result BinaryReader::ReadImportSection(Offset section_size) {
  CALLBACK(BeginImportSection, section_size);
  Index num_imports;
  CHECK_RESULT(ReadCount(&num_imports, "import count"));
  CALLBACK(OnImportCount, num_imports);
  for (Index i = 0; i < num_imports; ++i) {
    std::string_view module_name;
    CHECK_RESULT(ReadStr(&module_name, "import module name"));
    std::string_view field_name;
    CHECK_RESULT(ReadStr(&field_name, "import field name"));

    uint8_t kind;
    CHECK_RESULT(ReadU8(&kind, "import kind"));
    CALLBACK(OnImport, i, static_cast<ExternalKind>(kind), module_name,
             field_name);
    switch (static_cast<ExternalKind>(kind)) {
      case ExternalKind::Func: {
        Index sig_index;
        CHECK_RESULT(ReadIndex(&sig_index, "import signature index"));
        CALLBACK(OnImportFunc, i, module_name, field_name, num_func_imports_,
                 sig_index);
        num_func_imports_++;
        break;
      }

      case ExternalKind::Table: {
        Type elem_type;
        Limits elem_limits;
        CHECK_RESULT(ReadTable(&elem_type, &elem_limits));
        CALLBACK(OnImportTable, i, module_name, field_name, num_table_imports_,
                 elem_type, &elem_limits);
        num_table_imports_++;
        break;
      }

      case ExternalKind::Memory: {
        Limits page_limits;
        CHECK_RESULT(ReadMemory(&page_limits));
        CALLBACK(OnImportMemory, i, module_name, field_name,
                 num_memory_imports_, &page_limits);
        num_memory_imports_++;
        break;
      }

      case ExternalKind::Global: {
        Type type;
        bool mutable_;
        CHECK_RESULT(ReadGlobalHeader(&type, &mutable_));
        CALLBACK(OnImportGlobal, i, module_name, field_name,
                 num_global_imports_, type, mutable_);
        num_global_imports_++;
        break;
      }

      case ExternalKind::Tag: {
        ERROR_UNLESS(options_.features.exceptions_enabled(),
                     "invalid import tag kind: exceptions not allowed");
        Index sig_index;
        CHECK_RESULT(ReadTagType(&sig_index));
        CALLBACK(OnImportTag, i, module_name, field_name, num_tag_imports_,
                 sig_index);
        num_tag_imports_++;
        break;
      }

      default:
        PrintError("malformed import kind: %d", kind);
        return Result::Error;
    }
  }

  CALLBACK0(EndImportSection);
  return Result::Ok;
}

Result BinaryReader::ReadFunctionSection(Offset section_size) {
  CALLBACK(BeginFunctionSection, section_size);
  CHECK_RESULT(
      ReadCount(&num_function_signatures_, "function signature count"));
  CALLBACK(OnFunctionCount, num_function_signatures_);
  for (Index i = 0; i < num_function_signatures_; ++i) {
    Index func_index = num_func_imports_ + i;
    Index sig_index;
    CHECK_RESULT(ReadIndex(&sig_index, "function signature index"));
    CALLBACK(OnFunction, func_index, sig_index);
  }
  CALLBACK0(EndFunctionSection);
  return Result::Ok;
}

Result BinaryReader::ReadTableSection(Offset section_size) {
  CALLBACK(BeginTableSection, section_size);
  Index num_tables;
  CHECK_RESULT(ReadCount(&num_tables, "table count"));
  CALLBACK(OnTableCount, num_tables);
  for (Index i = 0; i < num_tables; ++i) {
    Index table_index = num_table_imports_ + i;
    Type elem_type;
    Limits elem_limits;
    CHECK_RESULT(ReadTable(&elem_type, &elem_limits));
    CALLBACK(OnTable, table_index, elem_type, &elem_limits);
  }
  CALLBACK0(EndTableSection);
  return Result::Ok;
}

Result BinaryReader::ReadMemorySection(Offset section_size) {
  CALLBACK(BeginMemorySection, section_size);
  Index num_memories;
  CHECK_RESULT(ReadCount(&num_memories, "memory count"));
  CALLBACK(OnMemoryCount, num_memories);
  for (Index i = 0; i < num_memories; ++i) {
    Index memory_index = num_memory_imports_ + i;
    Limits page_limits;
    CHECK_RESULT(ReadMemory(&page_limits));
    CALLBACK(OnMemory, memory_index, &page_limits);
  }
  CALLBACK0(EndMemorySection);
  return Result::Ok;
}

Result BinaryReader::ReadGlobalSection(Offset section_size) {
  CALLBACK(BeginGlobalSection, section_size);
  Index num_globals;
  CHECK_RESULT(ReadCount(&num_globals, "global count"));
  CALLBACK(OnGlobalCount, num_globals);
  for (Index i = 0; i < num_globals; ++i) {
    Index global_index = num_global_imports_ + i;
    Type global_type;
    bool mutable_;
    CHECK_RESULT(ReadGlobalHeader(&global_type, &mutable_));
    CALLBACK(BeginGlobal, global_index, global_type, mutable_);
    CALLBACK(BeginGlobalInitExpr, global_index);
    CHECK_RESULT(ReadInitExpr(global_index));
    CALLBACK(EndGlobalInitExpr, global_index);
    CALLBACK(EndGlobal, global_index);
  }
  CALLBACK0(EndGlobalSection);
  return Result::Ok;
}

Result BinaryReader::ReadExportSection(Offset section_size) {
  CALLBACK(BeginExportSection, section_size);
  Index num_exports;
  CHECK_RESULT(ReadCount(&num_exports, "export count"));
  CALLBACK(OnExportCount, num_exports);
  for (Index i = 0; i < num_exports; ++i) {
    std::string_view name;
    CHECK_RESULT(ReadStr(&name, "export item name"));

    ExternalKind kind;
    CHECK_RESULT(ReadExternalKind(&kind, "export kind"));

    Index item_index;
    CHECK_RESULT(ReadIndex(&item_index, "export item index"));
    if (kind == ExternalKind::Tag) {
      ERROR_UNLESS(options_.features.exceptions_enabled(),
                   "invalid export tag kind: exceptions not allowed");
    }

    CALLBACK(OnExport, i, static_cast<ExternalKind>(kind), item_index, name);
  }
  CALLBACK0(EndExportSection);
  return Result::Ok;
}

Result BinaryReader::ReadStartSection(Offset section_size) {
  CALLBACK(BeginStartSection, section_size);
  Index func_index;
  CHECK_RESULT(ReadIndex(&func_index, "start function index"));
  CALLBACK(OnStartFunction, func_index);
  CALLBACK0(EndStartSection);
  return Result::Ok;
}

Result BinaryReader::ReadElemSection(Offset section_size) {
  CALLBACK(BeginElemSection, section_size);
  Index num_elem_segments;
  CHECK_RESULT(ReadCount(&num_elem_segments, "elem segment count"));
  CALLBACK(OnElemSegmentCount, num_elem_segments);
  for (Index i = 0; i < num_elem_segments; ++i) {
    uint32_t flags;
    CHECK_RESULT(ReadU32Leb128(&flags, "elem segment flags"));
    ERROR_IF(flags > SegFlagMax, "invalid elem segment flags: %#x", flags);
    Index table_index(0);
    if ((flags & (SegPassive | SegExplicitIndex)) == SegExplicitIndex) {
      CHECK_RESULT(ReadIndex(&table_index, "elem segment table index"));
    }
    Type elem_type = Type::FuncRef;

    CALLBACK(BeginElemSegment, i, table_index, flags);

    if (!(flags & SegPassive)) {
      CALLBACK(BeginElemSegmentInitExpr, i);
      CHECK_RESULT(ReadInitExpr(i));
      CALLBACK(EndElemSegmentInitExpr, i);
    }

    // For backwards compat we support not declaring the element kind.
    if (flags & (SegPassive | SegExplicitIndex)) {
      if (flags & SegUseElemExprs) {
        CHECK_RESULT(ReadRefType(&elem_type, "table elem type"));
      } else {
        ExternalKind kind;
        CHECK_RESULT(ReadExternalKind(&kind, "export kind"));
        ERROR_UNLESS(kind == ExternalKind::Func,
                     "segment elem type must be func (%s)",
                     elem_type.GetName().c_str());
        elem_type = Type::FuncRef;
      }
    }

    CALLBACK(OnElemSegmentElemType, i, elem_type);

    Index num_elem_exprs;
    CHECK_RESULT(ReadCount(&num_elem_exprs, "elem count"));

    CALLBACK(OnElemSegmentElemExprCount, i, num_elem_exprs);
    for (Index j = 0; j < num_elem_exprs; ++j) {
      if (flags & SegUseElemExprs) {
        Opcode opcode;
        CHECK_RESULT(ReadOpcode(&opcode, "elem expr opcode"));
        if (opcode == Opcode::RefNull) {
          Type type;
          CHECK_RESULT(ReadRefType(&type, "elem expr ref.null type"));
          CALLBACK(OnElemSegmentElemExpr_RefNull, i, type);
        } else if (opcode == Opcode::RefFunc) {
          Index func_index;
          CHECK_RESULT(ReadIndex(&func_index, "elem expr func index"));
          CALLBACK(OnElemSegmentElemExpr_RefFunc, i, func_index);
        } else {
          PrintError(
              "expected ref.null or ref.func in passive element segment");
        }
        CHECK_RESULT(ReadOpcode(&opcode, "opcode"));
        ERROR_UNLESS(opcode == Opcode::End,
                     "expected END opcode after element expression");
      } else {
        Index func_index;
        CHECK_RESULT(ReadIndex(&func_index, "elem expr func index"));
        CALLBACK(OnElemSegmentElemExpr_RefFunc, i, func_index);
      }
    }
    CALLBACK(EndElemSegment, i);
  }
  CALLBACK0(EndElemSection);
  return Result::Ok;
}

Result BinaryReader::ReadCodeSection(Offset section_size) {
  CALLBACK(BeginCodeSection, section_size);
  CHECK_RESULT(ReadCount(&num_function_bodies_, "function body count"));
  ERROR_UNLESS(num_function_signatures_ == num_function_bodies_,
               "function signature count != function body count");
  CALLBACK(OnFunctionBodyCount, num_function_bodies_);
  for (Index i = 0; i < num_function_bodies_; ++i) {
    Index func_index = num_func_imports_ + i;
    Offset func_offset = state_.offset;
    state_.offset = func_offset;
    uint32_t body_size;
    CHECK_RESULT(ReadU32Leb128(&body_size, "function body size"));
    Offset body_start_offset = state_.offset;
    Offset end_offset = body_start_offset + body_size;
    CALLBACK(BeginFunctionBody, func_index, body_size);

    uint64_t total_locals = 0;
    Index num_local_decls;
    CHECK_RESULT(ReadCount(&num_local_decls, "local declaration count"));
    CALLBACK(OnLocalDeclCount, num_local_decls);
    for (Index k = 0; k < num_local_decls; ++k) {
      Index num_local_types;
      CHECK_RESULT(ReadIndex(&num_local_types, "local type count"));
      total_locals += num_local_types;
      ERROR_UNLESS(total_locals <= UINT32_MAX, "local count must be <= 0x%x",
                   UINT32_MAX);
      Type local_type;
      CHECK_RESULT(ReadType(&local_type, "local type"));
      ERROR_UNLESS(IsConcreteType(local_type), "expected valid local type");
      CALLBACK(OnLocalDecl, k, num_local_types, local_type);
    }

    if (options_.skip_function_bodies) {
      state_.offset = end_offset;
    } else {
      CHECK_RESULT(ReadFunctionBody(end_offset));
    }

    CALLBACK(EndFunctionBody, func_index);
  }
  CALLBACK0(EndCodeSection);
  return Result::Ok;
}

Result BinaryReader::ReadDataSection(Offset section_size) {
  CALLBACK(BeginDataSection, section_size);
  Index num_data_segments;
  CHECK_RESULT(ReadCount(&num_data_segments, "data segment count"));
  CALLBACK(OnDataSegmentCount, num_data_segments);
  // If the DataCount section is not present, then data_count_ will be invalid.
  ERROR_UNLESS(data_count_ == kInvalidIndex || data_count_ == num_data_segments,
               "data segment count does not equal count in DataCount section");
  for (Index i = 0; i < num_data_segments; ++i) {
    uint32_t flags;
    CHECK_RESULT(ReadU32Leb128(&flags, "data segment flags"));
    ERROR_IF(flags != 0 && !options_.features.bulk_memory_enabled(),
             "invalid memory index %d: bulk memory not allowed", flags);
    ERROR_IF(flags > SegFlagMax, "invalid data segment flags: %#x", flags);
    Index memory_index(0);
    if (flags & SegExplicitIndex) {
      CHECK_RESULT(ReadIndex(&memory_index, "data segment memory index"));
    }
    CALLBACK(BeginDataSegment, i, memory_index, flags);
    if (!(flags & SegPassive)) {
      ERROR_UNLESS(memories.size() > 0, "no memory to copy data to");
      CALLBACK(BeginDataSegmentInitExpr, i);
      CHECK_RESULT(ReadInitExpr(i));
      CALLBACK(EndDataSegmentInitExpr, i);
    }

    Address data_size;
    const void* data;
    CHECK_RESULT(ReadBytes(&data, &data_size, "data segment data"));
    CALLBACK(OnDataSegmentData, i, data, data_size);
    CALLBACK(EndDataSegment, i);
  }
  CALLBACK0(EndDataSection);
  return Result::Ok;
}

Result BinaryReader::ReadDataCountSection(Offset section_size) {
  CALLBACK(BeginDataCountSection, section_size);
  Index data_count;
  CHECK_RESULT(ReadIndex(&data_count, "data count"));
  CALLBACK(OnDataCount, data_count);
  CALLBACK0(EndDataCountSection);
  data_count_ = data_count;
  return Result::Ok;
}

Result BinaryReader::ReadSections(const ReadSectionsOptions& options) {
  Result result = Result::Ok;
  Index section_index = 0;
  bool seen_section_code[static_cast<int>(BinarySection::Last) + 1] = {false};

  for (; state_.offset < state_.size; ++section_index) {
    uint8_t section_code;
    Offset section_size;
    CHECK_RESULT(ReadU8(&section_code, "section code"));
    CHECK_RESULT(ReadOffset(&section_size, "section size"));
    ReadEndRestoreGuard guard(this);
    read_end_ = state_.offset + section_size;
    if (section_code >= kBinarySectionCount) {
      PrintError("invalid section code: %u", section_code);
      if (options.stop_on_first_error) {
        return Result::Error;
      }
      // If we don't have to stop on first error, continue reading
      // sections, because although we could not understand the
      // current section, we can continue and correctly parse
      // subsequent sections, so we can give back as much information
      // as we can understand.
      result = Result::Error;
      state_.offset = read_end_;
      continue;
    }

    BinarySection section = static_cast<BinarySection>(section_code);
    if (section != BinarySection::Custom) {
      if (seen_section_code[section_code]) {
        PrintError("multiple %s sections", GetSectionName(section));
        return Result::Error;
      }
      seen_section_code[section_code] = true;
    }

    ERROR_UNLESS(read_end_ <= state_.size,
                 "invalid section size: extends past end");

    ERROR_UNLESS(
        last_known_section_ == BinarySection::Invalid ||
            section == BinarySection::Custom ||
            GetSectionOrder(section) > GetSectionOrder(last_known_section_),
        "section %s out of order", GetSectionName(section));

    ERROR_UNLESS(!did_read_names_section_ || section == BinarySection::Custom,
                 "%s section can not occur after Name section",
                 GetSectionName(section));

    CALLBACK(BeginSection, section_index, section, section_size);

    bool stop_on_first_error = options_.stop_on_first_error;
    Result section_result = Result::Error;
    switch (section) {
      case BinarySection::Custom:
        section_result = ReadCustomSection(section_index, section_size);
        if (options_.fail_on_custom_section_error) {
          result |= section_result;
        } else {
          stop_on_first_error = false;
        }
        break;
      case BinarySection::Type:
        section_result = ReadTypeSection(section_size);
        result |= section_result;
        break;
      case BinarySection::Import:
        section_result = ReadImportSection(section_size);
        result |= section_result;
        break;
      case BinarySection::Function:
        section_result = ReadFunctionSection(section_size);
        result |= section_result;
        break;
      case BinarySection::Table:
        section_result = ReadTableSection(section_size);
        result |= section_result;
        break;
      case BinarySection::Memory:
        section_result = ReadMemorySection(section_size);
        result |= section_result;
        break;
      case BinarySection::Global:
        section_result = ReadGlobalSection(section_size);
        result |= section_result;
        break;
      case BinarySection::Export:
        section_result = ReadExportSection(section_size);
        result |= section_result;
        break;
      case BinarySection::Start:
        section_result = ReadStartSection(section_size);
        result |= section_result;
        break;
      case BinarySection::Elem:
        section_result = ReadElemSection(section_size);
        result |= section_result;
        break;
      case BinarySection::Code:
        section_result = ReadCodeSection(section_size);
        result |= section_result;
        break;
      case BinarySection::Data:
        section_result = ReadDataSection(section_size);
        result |= section_result;
        break;
      case BinarySection::Tag:
        ERROR_UNLESS(options_.features.exceptions_enabled(),
                     "invalid section code: %u",
                     static_cast<unsigned int>(section));
        section_result = ReadTagSection(section_size);
        result |= section_result;
        break;
      case BinarySection::DataCount:
        ERROR_UNLESS(options_.features.bulk_memory_enabled(),
                     "invalid section code: %u",
                     static_cast<unsigned int>(section));
        section_result = ReadDataCountSection(section_size);
        result |= section_result;
        break;
      case BinarySection::Invalid:
        WABT_UNREACHABLE;
    }

    if (Succeeded(section_result) && state_.offset != read_end_) {
      PrintError("unfinished section (expected end: 0x%" PRIzx ")", read_end_);
      section_result = Result::Error;
      result |= section_result;
    }

    if (Failed(section_result)) {
      if (stop_on_first_error) {
        return Result::Error;
      }

      // If we're continuing after failing to read this section, move the
      // offset to the expected section end. This way we may be able to read
      // further sections.
      state_.offset = read_end_;
    }

    if (section != BinarySection::Custom) {
      last_known_section_ = section;
    }
  }

  return result;
}

Result BinaryReader::ReadModule(const ReadModuleOptions& options) {
  uint32_t magic = 0;
  CHECK_RESULT(ReadU32(&magic, "magic"));
  ERROR_UNLESS(magic == WABT_BINARY_MAGIC, "bad magic value");
  uint32_t version = 0;
  CHECK_RESULT(ReadU32(&version, "version"));
  ERROR_UNLESS(version == WABT_BINARY_VERSION,
               "bad wasm file version: %#x (expected %#x)", version,
               WABT_BINARY_VERSION);

  CALLBACK(BeginModule, version);
  CHECK_RESULT(ReadSections(ReadSectionsOptions{options.stop_on_first_error}));
  // This is checked in ReadCodeSection, but it must be checked at the end too,
  // in case the code section was omitted.
  ERROR_UNLESS(num_function_signatures_ == num_function_bodies_,
               "function signature count != function body count");
  CALLBACK0(EndModule);

  return Result::Ok;
}

}  // end anonymous namespace

Result ReadBinary(const void* data,
                  size_t size,
                  BinaryReaderDelegate* delegate,
                  const ReadBinaryOptions& options) {
  BinaryReader reader(data, size, delegate, options);
  return reader.ReadModule(
      BinaryReader::ReadModuleOptions{options.stop_on_first_error});
}

}  // namespace wabt
