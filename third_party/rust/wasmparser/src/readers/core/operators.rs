/* Copyright 2018 Mozilla Foundation
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

use crate::{BinaryReader, BinaryReaderError, Result, ValType};

/// Represents a block type.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum BlockType {
    /// The block produces consumes nor produces any values.
    Empty,
    /// The block produces a singular value of the given type ([] -> \[t]).
    Type(ValType),
    /// The block is described by a function type.
    ///
    /// The index is to a function type in the types section.
    FuncType(u32),
}

/// Represents a memory immediate in a WebAssembly memory instruction.
#[derive(Debug, Copy, Clone)]
pub struct MemoryImmediate {
    /// Alignment, stored as `n` where the actual alignment is `2^n`
    pub align: u8,
    /// A fixed byte-offset that this memory immediate specifies.
    ///
    /// Note that the memory64 proposal can specify a full 64-bit byte offset
    /// while otherwise only 32-bit offsets are allowed. Once validated
    /// memory immediates for 32-bit memories are guaranteed to be at most
    /// `u32::MAX` whereas 64-bit memories can use the full 64-bits.
    pub offset: u64,
    /// The index of the memory this immediate points to.
    ///
    /// Note that this points within the module's own memory index space, and
    /// is always zero unless the multi-memory proposal of WebAssembly is
    /// enabled.
    pub memory: u32,
}

/// A br_table entries representation.
#[derive(Clone)]
pub struct BrTable<'a> {
    pub(crate) reader: crate::BinaryReader<'a>,
    pub(crate) cnt: u32,
    pub(crate) default: u32,
}

/// An IEEE binary32 immediate floating point value, represented as a u32
/// containing the bit pattern.
///
/// All bit patterns are allowed.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct Ieee32(pub(crate) u32);

impl Ieee32 {
    /// Gets the underlying bits of the 32-bit float.
    pub fn bits(self) -> u32 {
        self.0
    }
}

/// An IEEE binary64 immediate floating point value, represented as a u64
/// containing the bit pattern.
///
/// All bit patterns are allowed.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct Ieee64(pub(crate) u64);

impl Ieee64 {
    /// Gets the underlying bits of the 64-bit float.
    pub fn bits(self) -> u64 {
        self.0
    }
}

/// Represents a 128-bit vector value.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct V128(pub(crate) [u8; 16]);

impl V128 {
    /// Gets the bytes of the vector value.
    pub fn bytes(&self) -> &[u8; 16] {
        &self.0
    }

    /// Gets a signed 128-bit integer value from the vector's bytes.
    pub fn i128(&self) -> i128 {
        i128::from_le_bytes(self.0)
    }
}

/// Represents a SIMD lane index.
pub type SIMDLaneIndex = u8;

/// Instructions as defined [here].
///
/// [here]: https://webassembly.github.io/spec/core/binary/instructions.html
#[derive(Debug, Clone)]
#[allow(missing_docs)]
pub enum Operator<'a> {
    Unreachable,
    Nop,
    Block {
        ty: BlockType,
    },
    Loop {
        ty: BlockType,
    },
    If {
        ty: BlockType,
    },
    Else,
    Try {
        ty: BlockType,
    },
    Catch {
        index: u32,
    },
    Throw {
        index: u32,
    },
    Rethrow {
        relative_depth: u32,
    },
    End,
    Br {
        relative_depth: u32,
    },
    BrIf {
        relative_depth: u32,
    },
    BrTable {
        table: BrTable<'a>,
    },
    Return,
    Call {
        function_index: u32,
    },
    CallIndirect {
        index: u32,
        table_index: u32,
        table_byte: u8,
    },
    ReturnCall {
        function_index: u32,
    },
    ReturnCallIndirect {
        index: u32,
        table_index: u32,
    },
    Delegate {
        relative_depth: u32,
    },
    CatchAll,
    Drop,
    Select,
    TypedSelect {
        ty: ValType,
    },
    LocalGet {
        local_index: u32,
    },
    LocalSet {
        local_index: u32,
    },
    LocalTee {
        local_index: u32,
    },
    GlobalGet {
        global_index: u32,
    },
    GlobalSet {
        global_index: u32,
    },
    I32Load {
        memarg: MemoryImmediate,
    },
    I64Load {
        memarg: MemoryImmediate,
    },
    F32Load {
        memarg: MemoryImmediate,
    },
    F64Load {
        memarg: MemoryImmediate,
    },
    I32Load8S {
        memarg: MemoryImmediate,
    },
    I32Load8U {
        memarg: MemoryImmediate,
    },
    I32Load16S {
        memarg: MemoryImmediate,
    },
    I32Load16U {
        memarg: MemoryImmediate,
    },
    I64Load8S {
        memarg: MemoryImmediate,
    },
    I64Load8U {
        memarg: MemoryImmediate,
    },
    I64Load16S {
        memarg: MemoryImmediate,
    },
    I64Load16U {
        memarg: MemoryImmediate,
    },
    I64Load32S {
        memarg: MemoryImmediate,
    },
    I64Load32U {
        memarg: MemoryImmediate,
    },
    I32Store {
        memarg: MemoryImmediate,
    },
    I64Store {
        memarg: MemoryImmediate,
    },
    F32Store {
        memarg: MemoryImmediate,
    },
    F64Store {
        memarg: MemoryImmediate,
    },
    I32Store8 {
        memarg: MemoryImmediate,
    },
    I32Store16 {
        memarg: MemoryImmediate,
    },
    I64Store8 {
        memarg: MemoryImmediate,
    },
    I64Store16 {
        memarg: MemoryImmediate,
    },
    I64Store32 {
        memarg: MemoryImmediate,
    },
    MemorySize {
        mem: u32,
        mem_byte: u8,
    },
    MemoryGrow {
        mem: u32,
        mem_byte: u8,
    },
    I32Const {
        value: i32,
    },
    I64Const {
        value: i64,
    },
    F32Const {
        value: Ieee32,
    },
    F64Const {
        value: Ieee64,
    },
    RefNull {
        ty: ValType,
    },
    RefIsNull,
    RefFunc {
        function_index: u32,
    },
    I32Eqz,
    I32Eq,
    I32Ne,
    I32LtS,
    I32LtU,
    I32GtS,
    I32GtU,
    I32LeS,
    I32LeU,
    I32GeS,
    I32GeU,
    I64Eqz,
    I64Eq,
    I64Ne,
    I64LtS,
    I64LtU,
    I64GtS,
    I64GtU,
    I64LeS,
    I64LeU,
    I64GeS,
    I64GeU,
    F32Eq,
    F32Ne,
    F32Lt,
    F32Gt,
    F32Le,
    F32Ge,
    F64Eq,
    F64Ne,
    F64Lt,
    F64Gt,
    F64Le,
    F64Ge,
    I32Clz,
    I32Ctz,
    I32Popcnt,
    I32Add,
    I32Sub,
    I32Mul,
    I32DivS,
    I32DivU,
    I32RemS,
    I32RemU,
    I32And,
    I32Or,
    I32Xor,
    I32Shl,
    I32ShrS,
    I32ShrU,
    I32Rotl,
    I32Rotr,
    I64Clz,
    I64Ctz,
    I64Popcnt,
    I64Add,
    I64Sub,
    I64Mul,
    I64DivS,
    I64DivU,
    I64RemS,
    I64RemU,
    I64And,
    I64Or,
    I64Xor,
    I64Shl,
    I64ShrS,
    I64ShrU,
    I64Rotl,
    I64Rotr,
    F32Abs,
    F32Neg,
    F32Ceil,
    F32Floor,
    F32Trunc,
    F32Nearest,
    F32Sqrt,
    F32Add,
    F32Sub,
    F32Mul,
    F32Div,
    F32Min,
    F32Max,
    F32Copysign,
    F64Abs,
    F64Neg,
    F64Ceil,
    F64Floor,
    F64Trunc,
    F64Nearest,
    F64Sqrt,
    F64Add,
    F64Sub,
    F64Mul,
    F64Div,
    F64Min,
    F64Max,
    F64Copysign,
    I32WrapI64,
    I32TruncF32S,
    I32TruncF32U,
    I32TruncF64S,
    I32TruncF64U,
    I64ExtendI32S,
    I64ExtendI32U,
    I64TruncF32S,
    I64TruncF32U,
    I64TruncF64S,
    I64TruncF64U,
    F32ConvertI32S,
    F32ConvertI32U,
    F32ConvertI64S,
    F32ConvertI64U,
    F32DemoteF64,
    F64ConvertI32S,
    F64ConvertI32U,
    F64ConvertI64S,
    F64ConvertI64U,
    F64PromoteF32,
    I32ReinterpretF32,
    I64ReinterpretF64,
    F32ReinterpretI32,
    F64ReinterpretI64,
    I32Extend8S,
    I32Extend16S,
    I64Extend8S,
    I64Extend16S,
    I64Extend32S,

    // 0xFC operators
    // Non-trapping Float-to-int Conversions
    I32TruncSatF32S,
    I32TruncSatF32U,
    I32TruncSatF64S,
    I32TruncSatF64U,
    I64TruncSatF32S,
    I64TruncSatF32U,
    I64TruncSatF64S,
    I64TruncSatF64U,

    // 0xFC operators
    // bulk memory https://github.com/WebAssembly/bulk-memory-operations/blob/master/proposals/bulk-memory-operations/Overview.md
    MemoryInit {
        segment: u32,
        mem: u32,
    },
    DataDrop {
        segment: u32,
    },
    MemoryCopy {
        src: u32,
        dst: u32,
    },
    MemoryFill {
        mem: u32,
    },
    TableInit {
        segment: u32,
        table: u32,
    },
    ElemDrop {
        segment: u32,
    },
    TableCopy {
        dst_table: u32,
        src_table: u32,
    },
    TableFill {
        table: u32,
    },
    TableGet {
        table: u32,
    },
    TableSet {
        table: u32,
    },
    TableGrow {
        table: u32,
    },
    TableSize {
        table: u32,
    },

    // 0xFE operators
    // https://github.com/WebAssembly/threads/blob/master/proposals/threads/Overview.md
    MemoryAtomicNotify {
        memarg: MemoryImmediate,
    },
    MemoryAtomicWait32 {
        memarg: MemoryImmediate,
    },
    MemoryAtomicWait64 {
        memarg: MemoryImmediate,
    },
    AtomicFence {
        flags: u8,
    },
    I32AtomicLoad {
        memarg: MemoryImmediate,
    },
    I64AtomicLoad {
        memarg: MemoryImmediate,
    },
    I32AtomicLoad8U {
        memarg: MemoryImmediate,
    },
    I32AtomicLoad16U {
        memarg: MemoryImmediate,
    },
    I64AtomicLoad8U {
        memarg: MemoryImmediate,
    },
    I64AtomicLoad16U {
        memarg: MemoryImmediate,
    },
    I64AtomicLoad32U {
        memarg: MemoryImmediate,
    },
    I32AtomicStore {
        memarg: MemoryImmediate,
    },
    I64AtomicStore {
        memarg: MemoryImmediate,
    },
    I32AtomicStore8 {
        memarg: MemoryImmediate,
    },
    I32AtomicStore16 {
        memarg: MemoryImmediate,
    },
    I64AtomicStore8 {
        memarg: MemoryImmediate,
    },
    I64AtomicStore16 {
        memarg: MemoryImmediate,
    },
    I64AtomicStore32 {
        memarg: MemoryImmediate,
    },
    I32AtomicRmwAdd {
        memarg: MemoryImmediate,
    },
    I64AtomicRmwAdd {
        memarg: MemoryImmediate,
    },
    I32AtomicRmw8AddU {
        memarg: MemoryImmediate,
    },
    I32AtomicRmw16AddU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw8AddU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw16AddU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw32AddU {
        memarg: MemoryImmediate,
    },
    I32AtomicRmwSub {
        memarg: MemoryImmediate,
    },
    I64AtomicRmwSub {
        memarg: MemoryImmediate,
    },
    I32AtomicRmw8SubU {
        memarg: MemoryImmediate,
    },
    I32AtomicRmw16SubU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw8SubU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw16SubU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw32SubU {
        memarg: MemoryImmediate,
    },
    I32AtomicRmwAnd {
        memarg: MemoryImmediate,
    },
    I64AtomicRmwAnd {
        memarg: MemoryImmediate,
    },
    I32AtomicRmw8AndU {
        memarg: MemoryImmediate,
    },
    I32AtomicRmw16AndU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw8AndU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw16AndU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw32AndU {
        memarg: MemoryImmediate,
    },
    I32AtomicRmwOr {
        memarg: MemoryImmediate,
    },
    I64AtomicRmwOr {
        memarg: MemoryImmediate,
    },
    I32AtomicRmw8OrU {
        memarg: MemoryImmediate,
    },
    I32AtomicRmw16OrU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw8OrU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw16OrU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw32OrU {
        memarg: MemoryImmediate,
    },
    I32AtomicRmwXor {
        memarg: MemoryImmediate,
    },
    I64AtomicRmwXor {
        memarg: MemoryImmediate,
    },
    I32AtomicRmw8XorU {
        memarg: MemoryImmediate,
    },
    I32AtomicRmw16XorU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw8XorU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw16XorU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw32XorU {
        memarg: MemoryImmediate,
    },
    I32AtomicRmwXchg {
        memarg: MemoryImmediate,
    },
    I64AtomicRmwXchg {
        memarg: MemoryImmediate,
    },
    I32AtomicRmw8XchgU {
        memarg: MemoryImmediate,
    },
    I32AtomicRmw16XchgU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw8XchgU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw16XchgU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw32XchgU {
        memarg: MemoryImmediate,
    },
    I32AtomicRmwCmpxchg {
        memarg: MemoryImmediate,
    },
    I64AtomicRmwCmpxchg {
        memarg: MemoryImmediate,
    },
    I32AtomicRmw8CmpxchgU {
        memarg: MemoryImmediate,
    },
    I32AtomicRmw16CmpxchgU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw8CmpxchgU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw16CmpxchgU {
        memarg: MemoryImmediate,
    },
    I64AtomicRmw32CmpxchgU {
        memarg: MemoryImmediate,
    },

    // 0xFD operators
    // SIMD https://webassembly.github.io/simd/core/binary/instructions.html
    V128Load {
        memarg: MemoryImmediate,
    },
    V128Load8x8S {
        memarg: MemoryImmediate,
    },
    V128Load8x8U {
        memarg: MemoryImmediate,
    },
    V128Load16x4S {
        memarg: MemoryImmediate,
    },
    V128Load16x4U {
        memarg: MemoryImmediate,
    },
    V128Load32x2S {
        memarg: MemoryImmediate,
    },
    V128Load32x2U {
        memarg: MemoryImmediate,
    },
    V128Load8Splat {
        memarg: MemoryImmediate,
    },
    V128Load16Splat {
        memarg: MemoryImmediate,
    },
    V128Load32Splat {
        memarg: MemoryImmediate,
    },
    V128Load64Splat {
        memarg: MemoryImmediate,
    },
    V128Load32Zero {
        memarg: MemoryImmediate,
    },
    V128Load64Zero {
        memarg: MemoryImmediate,
    },
    V128Store {
        memarg: MemoryImmediate,
    },
    V128Load8Lane {
        memarg: MemoryImmediate,
        lane: SIMDLaneIndex,
    },
    V128Load16Lane {
        memarg: MemoryImmediate,
        lane: SIMDLaneIndex,
    },
    V128Load32Lane {
        memarg: MemoryImmediate,
        lane: SIMDLaneIndex,
    },
    V128Load64Lane {
        memarg: MemoryImmediate,
        lane: SIMDLaneIndex,
    },
    V128Store8Lane {
        memarg: MemoryImmediate,
        lane: SIMDLaneIndex,
    },
    V128Store16Lane {
        memarg: MemoryImmediate,
        lane: SIMDLaneIndex,
    },
    V128Store32Lane {
        memarg: MemoryImmediate,
        lane: SIMDLaneIndex,
    },
    V128Store64Lane {
        memarg: MemoryImmediate,
        lane: SIMDLaneIndex,
    },
    V128Const {
        value: V128,
    },
    I8x16Shuffle {
        lanes: [SIMDLaneIndex; 16],
    },
    I8x16ExtractLaneS {
        lane: SIMDLaneIndex,
    },
    I8x16ExtractLaneU {
        lane: SIMDLaneIndex,
    },
    I8x16ReplaceLane {
        lane: SIMDLaneIndex,
    },
    I16x8ExtractLaneS {
        lane: SIMDLaneIndex,
    },
    I16x8ExtractLaneU {
        lane: SIMDLaneIndex,
    },
    I16x8ReplaceLane {
        lane: SIMDLaneIndex,
    },
    I32x4ExtractLane {
        lane: SIMDLaneIndex,
    },
    I32x4ReplaceLane {
        lane: SIMDLaneIndex,
    },
    I64x2ExtractLane {
        lane: SIMDLaneIndex,
    },
    I64x2ReplaceLane {
        lane: SIMDLaneIndex,
    },
    F32x4ExtractLane {
        lane: SIMDLaneIndex,
    },
    F32x4ReplaceLane {
        lane: SIMDLaneIndex,
    },
    F64x2ExtractLane {
        lane: SIMDLaneIndex,
    },
    F64x2ReplaceLane {
        lane: SIMDLaneIndex,
    },
    I8x16Swizzle,
    I8x16Splat,
    I16x8Splat,
    I32x4Splat,
    I64x2Splat,
    F32x4Splat,
    F64x2Splat,
    I8x16Eq,
    I8x16Ne,
    I8x16LtS,
    I8x16LtU,
    I8x16GtS,
    I8x16GtU,
    I8x16LeS,
    I8x16LeU,
    I8x16GeS,
    I8x16GeU,
    I16x8Eq,
    I16x8Ne,
    I16x8LtS,
    I16x8LtU,
    I16x8GtS,
    I16x8GtU,
    I16x8LeS,
    I16x8LeU,
    I16x8GeS,
    I16x8GeU,
    I32x4Eq,
    I32x4Ne,
    I32x4LtS,
    I32x4LtU,
    I32x4GtS,
    I32x4GtU,
    I32x4LeS,
    I32x4LeU,
    I32x4GeS,
    I32x4GeU,
    I64x2Eq,
    I64x2Ne,
    I64x2LtS,
    I64x2GtS,
    I64x2LeS,
    I64x2GeS,
    F32x4Eq,
    F32x4Ne,
    F32x4Lt,
    F32x4Gt,
    F32x4Le,
    F32x4Ge,
    F64x2Eq,
    F64x2Ne,
    F64x2Lt,
    F64x2Gt,
    F64x2Le,
    F64x2Ge,
    V128Not,
    V128And,
    V128AndNot,
    V128Or,
    V128Xor,
    V128Bitselect,
    V128AnyTrue,
    I8x16Abs,
    I8x16Neg,
    I8x16Popcnt,
    I8x16AllTrue,
    I8x16Bitmask,
    I8x16NarrowI16x8S,
    I8x16NarrowI16x8U,
    I8x16Shl,
    I8x16ShrS,
    I8x16ShrU,
    I8x16Add,
    I8x16AddSatS,
    I8x16AddSatU,
    I8x16Sub,
    I8x16SubSatS,
    I8x16SubSatU,
    I8x16MinS,
    I8x16MinU,
    I8x16MaxS,
    I8x16MaxU,
    I8x16RoundingAverageU,
    I16x8ExtAddPairwiseI8x16S,
    I16x8ExtAddPairwiseI8x16U,
    I16x8Abs,
    I16x8Neg,
    I16x8Q15MulrSatS,
    I16x8AllTrue,
    I16x8Bitmask,
    I16x8NarrowI32x4S,
    I16x8NarrowI32x4U,
    I16x8ExtendLowI8x16S,
    I16x8ExtendHighI8x16S,
    I16x8ExtendLowI8x16U,
    I16x8ExtendHighI8x16U,
    I16x8Shl,
    I16x8ShrS,
    I16x8ShrU,
    I16x8Add,
    I16x8AddSatS,
    I16x8AddSatU,
    I16x8Sub,
    I16x8SubSatS,
    I16x8SubSatU,
    I16x8Mul,
    I16x8MinS,
    I16x8MinU,
    I16x8MaxS,
    I16x8MaxU,
    I16x8RoundingAverageU,
    I16x8ExtMulLowI8x16S,
    I16x8ExtMulHighI8x16S,
    I16x8ExtMulLowI8x16U,
    I16x8ExtMulHighI8x16U,
    I32x4ExtAddPairwiseI16x8S,
    I32x4ExtAddPairwiseI16x8U,
    I32x4Abs,
    I32x4Neg,
    I32x4AllTrue,
    I32x4Bitmask,
    I32x4ExtendLowI16x8S,
    I32x4ExtendHighI16x8S,
    I32x4ExtendLowI16x8U,
    I32x4ExtendHighI16x8U,
    I32x4Shl,
    I32x4ShrS,
    I32x4ShrU,
    I32x4Add,
    I32x4Sub,
    I32x4Mul,
    I32x4MinS,
    I32x4MinU,
    I32x4MaxS,
    I32x4MaxU,
    I32x4DotI16x8S,
    I32x4ExtMulLowI16x8S,
    I32x4ExtMulHighI16x8S,
    I32x4ExtMulLowI16x8U,
    I32x4ExtMulHighI16x8U,
    I64x2Abs,
    I64x2Neg,
    I64x2AllTrue,
    I64x2Bitmask,
    I64x2ExtendLowI32x4S,
    I64x2ExtendHighI32x4S,
    I64x2ExtendLowI32x4U,
    I64x2ExtendHighI32x4U,
    I64x2Shl,
    I64x2ShrS,
    I64x2ShrU,
    I64x2Add,
    I64x2Sub,
    I64x2Mul,
    I64x2ExtMulLowI32x4S,
    I64x2ExtMulHighI32x4S,
    I64x2ExtMulLowI32x4U,
    I64x2ExtMulHighI32x4U,
    F32x4Ceil,
    F32x4Floor,
    F32x4Trunc,
    F32x4Nearest,
    F32x4Abs,
    F32x4Neg,
    F32x4Sqrt,
    F32x4Add,
    F32x4Sub,
    F32x4Mul,
    F32x4Div,
    F32x4Min,
    F32x4Max,
    F32x4PMin,
    F32x4PMax,
    F64x2Ceil,
    F64x2Floor,
    F64x2Trunc,
    F64x2Nearest,
    F64x2Abs,
    F64x2Neg,
    F64x2Sqrt,
    F64x2Add,
    F64x2Sub,
    F64x2Mul,
    F64x2Div,
    F64x2Min,
    F64x2Max,
    F64x2PMin,
    F64x2PMax,
    I32x4TruncSatF32x4S,
    I32x4TruncSatF32x4U,
    F32x4ConvertI32x4S,
    F32x4ConvertI32x4U,
    I32x4TruncSatF64x2SZero,
    I32x4TruncSatF64x2UZero,
    F64x2ConvertLowI32x4S,
    F64x2ConvertLowI32x4U,
    F32x4DemoteF64x2Zero,
    F64x2PromoteLowF32x4,
    I8x16RelaxedSwizzle,
    I32x4RelaxedTruncSatF32x4S,
    I32x4RelaxedTruncSatF32x4U,
    I32x4RelaxedTruncSatF64x2SZero,
    I32x4RelaxedTruncSatF64x2UZero,
    F32x4Fma,
    F32x4Fms,
    F64x2Fma,
    F64x2Fms,
    I8x16LaneSelect,
    I16x8LaneSelect,
    I32x4LaneSelect,
    I64x2LaneSelect,
    F32x4RelaxedMin,
    F32x4RelaxedMax,
    F64x2RelaxedMin,
    F64x2RelaxedMax,
}

/// A reader for a core WebAssembly function's operators.
#[derive(Clone)]
pub struct OperatorsReader<'a> {
    pub(crate) reader: BinaryReader<'a>,
}

impl<'a> OperatorsReader<'a> {
    pub(crate) fn new<'b>(data: &'a [u8], offset: usize) -> OperatorsReader<'b>
    where
        'a: 'b,
    {
        OperatorsReader {
            reader: BinaryReader::new_with_offset(data, offset),
        }
    }

    /// Determines if the reader is at the end of the operators.
    pub fn eof(&self) -> bool {
        self.reader.eof()
    }

    /// Gets the original position of the reader.
    pub fn original_position(&self) -> usize {
        self.reader.original_position()
    }

    /// Whether or not to allow 64-bit memory arguments in the
    /// the operators being read.
    ///
    /// This is intended to be `true` when support for the memory64
    /// WebAssembly proposal is also enabled.
    pub fn allow_memarg64(&mut self, allow: bool) {
        self.reader.allow_memarg64(allow);
    }

    /// Ensures the reader is at the end.
    ///
    /// This function returns an error if there is extra data after the operators.
    pub fn ensure_end(&self) -> Result<()> {
        if self.eof() {
            return Ok(());
        }
        Err(BinaryReaderError::new(
            "unexpected data at the end of operators",
            self.reader.original_position(),
        ))
    }

    /// Reads an operator from the reader.
    pub fn read<'b>(&mut self) -> Result<Operator<'b>>
    where
        'a: 'b,
    {
        self.reader.read_operator()
    }

    /// Converts to an iterator of operators paired with offsets.
    pub fn into_iter_with_offsets<'b>(self) -> OperatorsIteratorWithOffsets<'b>
    where
        'a: 'b,
    {
        OperatorsIteratorWithOffsets {
            reader: self,
            err: false,
        }
    }

    /// Reads an operator with its offset.
    pub fn read_with_offset<'b>(&mut self) -> Result<(Operator<'b>, usize)>
    where
        'a: 'b,
    {
        let pos = self.reader.original_position();
        Ok((self.read()?, pos))
    }

    /// Gets a binary reader from this operators reader.
    pub fn get_binary_reader(&self) -> BinaryReader<'a> {
        self.reader.clone()
    }
}

impl<'a> IntoIterator for OperatorsReader<'a> {
    type Item = Result<Operator<'a>>;
    type IntoIter = OperatorsIterator<'a>;

    /// Reads content of the code section.
    ///
    /// # Examples
    /// ```
    /// use wasmparser::{Operator, CodeSectionReader, Result};
    /// # let data: &[u8] = &[
    /// #     0x01, 0x03, 0x00, 0x01, 0x0b];
    /// let mut code_reader = CodeSectionReader::new(data, 0).unwrap();
    /// for _ in 0..code_reader.get_count() {
    ///     let body = code_reader.read().expect("function body");
    ///     let mut op_reader = body.get_operators_reader().expect("op reader");
    ///     let ops = op_reader.into_iter().collect::<Result<Vec<Operator>>>().expect("ops");
    ///     assert!(
    ///         if let [Operator::Nop, Operator::End] = ops.as_slice() { true } else { false },
    ///         "found {:?}",
    ///         ops
    ///     );
    /// }
    /// ```
    fn into_iter(self) -> Self::IntoIter {
        OperatorsIterator {
            reader: self,
            err: false,
        }
    }
}

/// An iterator over a function's operators.
pub struct OperatorsIterator<'a> {
    reader: OperatorsReader<'a>,
    err: bool,
}

impl<'a> Iterator for OperatorsIterator<'a> {
    type Item = Result<Operator<'a>>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.err || self.reader.eof() {
            return None;
        }
        let result = self.reader.read();
        self.err = result.is_err();
        Some(result)
    }
}

/// An iterator over a function's operators with offsets.
pub struct OperatorsIteratorWithOffsets<'a> {
    reader: OperatorsReader<'a>,
    err: bool,
}

impl<'a> Iterator for OperatorsIteratorWithOffsets<'a> {
    type Item = Result<(Operator<'a>, usize)>;

    /// Reads content of the code section with offsets.
    ///
    /// # Examples
    /// ```
    /// use wasmparser::{Operator, CodeSectionReader, Result};
    /// # let data: &[u8] = &[
    /// #     0x01, 0x03, 0x00, /* offset = 23 */ 0x01, 0x0b];
    /// let mut code_reader = CodeSectionReader::new(data, 20).unwrap();
    /// for _ in 0..code_reader.get_count() {
    ///     let body = code_reader.read().expect("function body");
    ///     let mut op_reader = body.get_operators_reader().expect("op reader");
    ///     let ops = op_reader.into_iter_with_offsets().collect::<Result<Vec<(Operator, usize)>>>().expect("ops");
    ///     assert!(
    ///         if let [(Operator::Nop, 23), (Operator::End, 24)] = ops.as_slice() { true } else { false },
    ///         "found {:?}",
    ///         ops
    ///     );
    /// }
    /// ```
    fn next(&mut self) -> Option<Self::Item> {
        if self.err || self.reader.eof() {
            return None;
        }
        let result = self.reader.read_with_offset();
        self.err = result.is_err();
        Some(result)
    }
}
