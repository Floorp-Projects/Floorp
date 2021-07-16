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

#include "src/opcode.h"

#include "src/feature.h"

namespace wabt {

// static
Opcode::Info Opcode::infos_[] = {
#define WABT_OPCODE(rtype, type1, type2, type3, mem_size, prefix, code, Name, \
                    text, decomp)                                             \
  {text,     decomp, Type::rtype, {Type::type1, Type::type2, Type::type3},    \
   mem_size, prefix, code,        PrefixCode(prefix, code)},
#include "src/opcode.def"
#undef WABT_OPCODE

  {"<invalid>", "", Type::Void, {Type::Void, Type::Void, Type::Void}, 0, 0, 0, 0},
};

#define WABT_OPCODE(rtype, type1, type2, type3, mem_size, prefix, code, Name, \
                    text, decomp)                                                     \
  /* static */ Opcode Opcode::Name##_Opcode(Opcode::Name);
#include "src/opcode.def"
#undef WABT_OPCODE

Opcode::Info Opcode::GetInfo() const {
  if (enum_ < Invalid) {
    return infos_[enum_];
  }

  Info invalid_info = infos_[Opcode::Invalid];
  DecodeInvalidOpcode(enum_, &invalid_info.prefix, &invalid_info.code);
  invalid_info.prefix_code = PrefixCode(invalid_info.prefix, invalid_info.code);
  return invalid_info;
}

bool Opcode::IsNaturallyAligned(Address alignment) const {
  Address opcode_align = GetMemorySize();
  return alignment == WABT_USE_NATURAL_ALIGNMENT || alignment == opcode_align;
}

Address Opcode::GetAlignment(Address alignment) const {
  if (alignment == WABT_USE_NATURAL_ALIGNMENT) {
    return GetMemorySize();
  }
  return alignment;
}

bool Opcode::IsEnabled(const Features& features) const {
  switch (enum_) {
    case Opcode::Try:
    case Opcode::Catch:
    case Opcode::Unwind:
    case Opcode::Delegate:
    case Opcode::Throw:
    case Opcode::Rethrow:
      return features.exceptions_enabled();

    case Opcode::ReturnCallIndirect:
    case Opcode::ReturnCall:
      return features.tail_call_enabled();

    case Opcode::I32TruncSatF32S:
    case Opcode::I32TruncSatF32U:
    case Opcode::I32TruncSatF64S:
    case Opcode::I32TruncSatF64U:
    case Opcode::I64TruncSatF32S:
    case Opcode::I64TruncSatF32U:
    case Opcode::I64TruncSatF64S:
    case Opcode::I64TruncSatF64U:
      return features.sat_float_to_int_enabled();

    case Opcode::I32Extend8S:
    case Opcode::I32Extend16S:
    case Opcode::I64Extend8S:
    case Opcode::I64Extend16S:
    case Opcode::I64Extend32S:
      return features.sign_extension_enabled();

    case Opcode::MemoryAtomicNotify:
    case Opcode::MemoryAtomicWait32:
    case Opcode::MemoryAtomicWait64:
    case Opcode::AtomicFence:
    case Opcode::I32AtomicLoad:
    case Opcode::I64AtomicLoad:
    case Opcode::I32AtomicLoad8U:
    case Opcode::I32AtomicLoad16U:
    case Opcode::I64AtomicLoad8U:
    case Opcode::I64AtomicLoad16U:
    case Opcode::I64AtomicLoad32U:
    case Opcode::I32AtomicStore:
    case Opcode::I64AtomicStore:
    case Opcode::I32AtomicStore8:
    case Opcode::I32AtomicStore16:
    case Opcode::I64AtomicStore8:
    case Opcode::I64AtomicStore16:
    case Opcode::I64AtomicStore32:
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
    case Opcode::I64AtomicRmw32XchgU:
    case Opcode::I32AtomicRmwCmpxchg:
    case Opcode::I64AtomicRmwCmpxchg:
    case Opcode::I32AtomicRmw8CmpxchgU:
    case Opcode::I32AtomicRmw16CmpxchgU:
    case Opcode::I64AtomicRmw8CmpxchgU:
    case Opcode::I64AtomicRmw16CmpxchgU:
    case Opcode::I64AtomicRmw32CmpxchgU:
      return features.threads_enabled();

    case Opcode::V128Const:
    case Opcode::V128Load:
    case Opcode::V128Store:
    case Opcode::I8X16Splat:
    case Opcode::I16X8Splat:
    case Opcode::I32X4Splat:
    case Opcode::I64X2Splat:
    case Opcode::F32X4Splat:
    case Opcode::F64X2Splat:
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
    case Opcode::F64X2ReplaceLane:
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
    case Opcode::I8X16Neg:
    case Opcode::I16X8Neg:
    case Opcode::I32X4Neg:
    case Opcode::I64X2Neg:
    case Opcode::I8X16AddSatS:
    case Opcode::I8X16AddSatU:
    case Opcode::I16X8AddSatS:
    case Opcode::I16X8AddSatU:
    case Opcode::I8X16SubSatS:
    case Opcode::I8X16SubSatU:
    case Opcode::I16X8SubSatS:
    case Opcode::I16X8SubSatU:
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
    case Opcode::V128Not:
    case Opcode::V128BitSelect:
    case Opcode::V128AnyTrue:
    case Opcode::I8X16Bitmask:
    case Opcode::I16X8Bitmask:
    case Opcode::I32X4Bitmask:
    case Opcode::I64X2Bitmask:
    case Opcode::I8X16AllTrue:
    case Opcode::I16X8AllTrue:
    case Opcode::I32X4AllTrue:
    case Opcode::I64X2AllTrue:
    case Opcode::I8X16Eq:
    case Opcode::I16X8Eq:
    case Opcode::I32X4Eq:
    case Opcode::F32X4Eq:
    case Opcode::F64X2Eq:
    case Opcode::I8X16Ne:
    case Opcode::I16X8Ne:
    case Opcode::I32X4Ne:
    case Opcode::F32X4Ne:
    case Opcode::F64X2Ne:
    case Opcode::I8X16LtS:
    case Opcode::I8X16LtU:
    case Opcode::I16X8LtS:
    case Opcode::I16X8LtU:
    case Opcode::I32X4LtS:
    case Opcode::I32X4LtU:
    case Opcode::F32X4Lt:
    case Opcode::F64X2Lt:
    case Opcode::I8X16LeS:
    case Opcode::I8X16LeU:
    case Opcode::I16X8LeS:
    case Opcode::I16X8LeU:
    case Opcode::I32X4LeS:
    case Opcode::I32X4LeU:
    case Opcode::F32X4Le:
    case Opcode::F64X2Le:
    case Opcode::I8X16GtS:
    case Opcode::I8X16GtU:
    case Opcode::I16X8GtS:
    case Opcode::I16X8GtU:
    case Opcode::I32X4GtS:
    case Opcode::I32X4GtU:
    case Opcode::F32X4Gt:
    case Opcode::F64X2Gt:
    case Opcode::I8X16GeS:
    case Opcode::I8X16GeU:
    case Opcode::I16X8GeS:
    case Opcode::I16X8GeU:
    case Opcode::I32X4GeS:
    case Opcode::I32X4GeU:
    case Opcode::F32X4Ge:
    case Opcode::F64X2Ge:
    case Opcode::F32X4Neg:
    case Opcode::F64X2Neg:
    case Opcode::F32X4Abs:
    case Opcode::F64X2Abs:
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
    case Opcode::F32X4Sqrt:
    case Opcode::F64X2Sqrt:
    case Opcode::F32X4ConvertI32X4S:
    case Opcode::F32X4ConvertI32X4U:
    case Opcode::I32X4TruncSatF32X4S:
    case Opcode::I32X4TruncSatF32X4U:
    case Opcode::I8X16Swizzle:
    case Opcode::I8X16Shuffle:
    case Opcode::V128Load8Splat:
    case Opcode::V128Load16Splat:
    case Opcode::V128Load32Splat:
    case Opcode::V128Load64Splat:
    case Opcode::V128Load8Lane:
    case Opcode::V128Load16Lane:
    case Opcode::V128Load32Lane:
    case Opcode::V128Load64Lane:
    case Opcode::V128Store8Lane:
    case Opcode::V128Store16Lane:
    case Opcode::V128Store32Lane:
    case Opcode::V128Store64Lane:
    case Opcode::I8X16Abs:
    case Opcode::I16X8Abs:
    case Opcode::I32X4Abs:
      return features.simd_enabled();

    case Opcode::MemoryInit:
    case Opcode::DataDrop:
    case Opcode::MemoryCopy:
    case Opcode::MemoryFill:
    case Opcode::TableInit:
    case Opcode::ElemDrop:
    case Opcode::TableCopy:
      return features.bulk_memory_enabled();

    case Opcode::TableGet:
    case Opcode::TableSet:
    case Opcode::TableGrow:
    case Opcode::TableSize:
    case Opcode::RefNull:
    case Opcode::RefIsNull:
      return features.reference_types_enabled();

    // Interpreter opcodes are never "enabled".
    case Opcode::InterpAlloca:
    case Opcode::InterpBrUnless:
    case Opcode::InterpCallImport:
    case Opcode::InterpData:
    case Opcode::InterpDropKeep:
      return false;

    default:
      return true;
  }
}

uint32_t Opcode::GetSimdLaneCount() const {
  switch (enum_) {
    case Opcode::I8X16ExtractLaneS:
    case Opcode::I8X16ExtractLaneU:
    case Opcode::I8X16ReplaceLane:
    case Opcode::V128Load8Lane:
    case Opcode::V128Store8Lane:
      return 16;
      break;
    case Opcode::I16X8ExtractLaneS:
    case Opcode::I16X8ExtractLaneU:
    case Opcode::I16X8ReplaceLane:
    case Opcode::V128Load16Lane:
    case Opcode::V128Store16Lane:
      return 8;
      break;
    case Opcode::F32X4ExtractLane:
    case Opcode::F32X4ReplaceLane:
    case Opcode::I32X4ExtractLane:
    case Opcode::I32X4ReplaceLane:
    case Opcode::V128Load32Lane:
    case Opcode::V128Store32Lane:
      return 4;
      break;
    case Opcode::F64X2ExtractLane:
    case Opcode::F64X2ReplaceLane:
    case Opcode::I64X2ExtractLane:
    case Opcode::I64X2ReplaceLane:
    case Opcode::V128Load64Lane:
    case Opcode::V128Store64Lane:
      return 2;
      break;
    default:
      WABT_UNREACHABLE;
  }
}

// Get the byte sequence for this opcode, including prefix.
std::vector<uint8_t> Opcode::GetBytes() const {
  std::vector<uint8_t> result;
  if (HasPrefix()) {
    result.push_back(GetPrefix());
    uint8_t buffer[5];
    Offset length =
        WriteU32Leb128Raw(buffer, buffer + sizeof(buffer), GetCode());
    assert(length != 0);
    result.insert(result.end(), buffer, buffer + length);
  } else {
    result.push_back(GetCode());
  }
  return result;
}

}  // namespace wabt
