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

use std::boxed::Box;
use std::result;

#[derive(Debug, Copy, Clone)]
pub struct BinaryReaderError {
    pub message: &'static str,
    pub offset: usize,
}

pub type Result<T> = result::Result<T, BinaryReaderError>;

#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub enum CustomSectionKind {
    Unknown,
    Name,
    SourceMappingURL,
    Reloc,
    Linking,
}

/// Section code as defined [here].
///
/// [here]: https://webassembly.github.io/spec/binary/modules.html#sections
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub enum SectionCode<'a> {
    Custom {
        name: &'a [u8],
        kind: CustomSectionKind,
    },
    Type,     // Function signature declarations
    Import,   // Import declarations
    Function, // Function declarations
    Table,    // Indirect function table and other tables
    Memory,   // Memory attributes
    Global,   // Global declarations
    Export,   // Exports
    Start,    // Start function declaration
    Element,  // Elements section
    Code,     // Function bodies (code)
    Data,     // Data segments
}

/// Types as defined [here].
///
/// [here]: https://webassembly.github.io/spec/syntax/types.html#types
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum Type {
    I32,
    I64,
    F32,
    F64,
    AnyFunc,
    AnyRef,
    Func,
    EmptyBlockType,
}

/// External types as defined [here].
///
/// [here]: https://webassembly.github.io/spec/syntax/types.html#external-types
#[derive(Debug, Copy, Clone)]
pub enum ExternalKind {
    Function,
    Table,
    Memory,
    Global,
}

#[derive(Debug, Clone)]
pub struct FuncType {
    pub form: Type,
    pub params: Box<[Type]>,
    pub returns: Box<[Type]>,
}

#[derive(Debug, Copy, Clone)]
pub struct ResizableLimits {
    pub initial: u32,
    pub maximum: Option<u32>,
}

#[derive(Debug, Copy, Clone)]
pub struct TableType {
    pub element_type: Type,
    pub limits: ResizableLimits,
}

#[derive(Debug, Copy, Clone)]
pub struct MemoryType {
    pub limits: ResizableLimits,
    pub shared: bool,
}

#[derive(Debug, Copy, Clone)]
pub struct GlobalType {
    pub content_type: Type,
    pub mutable: bool,
}

#[derive(Debug, Copy, Clone)]
pub enum ImportSectionEntryType {
    Function(u32),
    Table(TableType),
    Memory(MemoryType),
    Global(GlobalType),
}

#[derive(Debug)]
pub struct MemoryImmediate {
    pub flags: u32,
    pub offset: u32,
}

#[derive(Debug, Copy, Clone)]
pub struct Naming<'a> {
    pub index: u32,
    pub name: &'a [u8],
}

#[derive(Debug, Copy, Clone)]
pub enum NameType {
    Module,
    Function,
    Local,
}

#[derive(Debug, Copy, Clone)]
pub enum LinkingType {
    StackPointer(u32),
}

#[derive(Debug, Copy, Clone)]
pub enum RelocType {
    FunctionIndexLEB,
    TableIndexSLEB,
    TableIndexI32,
    GlobalAddrLEB,
    GlobalAddrSLEB,
    GlobalAddrI32,
    TypeIndexLEB,
    GlobalIndexLEB,
}

/// A br_table entries representation.
#[derive(Debug)]
pub struct BrTable<'a> {
    pub(crate) buffer: &'a [u8],
}

/// An IEEE binary32 immediate floating point value, represented as a u32
/// containing the bitpattern.
///
/// All bit patterns are allowed.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct Ieee32(pub(crate) u32);

impl Ieee32 {
    pub fn bits(self) -> u32 {
        self.0
    }
}

/// An IEEE binary64 immediate floating point value, represented as a u64
/// containing the bitpattern.
///
/// All bit patterns are allowed.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct Ieee64(pub(crate) u64);

impl Ieee64 {
    pub fn bits(self) -> u64 {
        self.0
    }
}

/// Instructions as defined [here].
///
/// [here]: https://webassembly.github.io/spec/binary/instructions.html
#[derive(Debug)]
pub enum Operator<'a> {
    Unreachable,
    Nop,
    Block { ty: Type },
    Loop { ty: Type },
    If { ty: Type },
    Else,
    End,
    Br { relative_depth: u32 },
    BrIf { relative_depth: u32 },
    BrTable { table: BrTable<'a> },
    Return,
    Call { function_index: u32 },
    CallIndirect { index: u32, table_index: u32 },
    Drop,
    Select,
    GetLocal { local_index: u32 },
    SetLocal { local_index: u32 },
    TeeLocal { local_index: u32 },
    GetGlobal { global_index: u32 },
    SetGlobal { global_index: u32 },
    I32Load { memarg: MemoryImmediate },
    I64Load { memarg: MemoryImmediate },
    F32Load { memarg: MemoryImmediate },
    F64Load { memarg: MemoryImmediate },
    I32Load8S { memarg: MemoryImmediate },
    I32Load8U { memarg: MemoryImmediate },
    I32Load16S { memarg: MemoryImmediate },
    I32Load16U { memarg: MemoryImmediate },
    I64Load8S { memarg: MemoryImmediate },
    I64Load8U { memarg: MemoryImmediate },
    I64Load16S { memarg: MemoryImmediate },
    I64Load16U { memarg: MemoryImmediate },
    I64Load32S { memarg: MemoryImmediate },
    I64Load32U { memarg: MemoryImmediate },
    I32Store { memarg: MemoryImmediate },
    I64Store { memarg: MemoryImmediate },
    F32Store { memarg: MemoryImmediate },
    F64Store { memarg: MemoryImmediate },
    I32Store8 { memarg: MemoryImmediate },
    I32Store16 { memarg: MemoryImmediate },
    I64Store8 { memarg: MemoryImmediate },
    I64Store16 { memarg: MemoryImmediate },
    I64Store32 { memarg: MemoryImmediate },
    MemorySize { reserved: u32 },
    MemoryGrow { reserved: u32 },
    I32Const { value: i32 },
    I64Const { value: i64 },
    F32Const { value: Ieee32 },
    F64Const { value: Ieee64 },
    RefNull,
    RefIsNull,
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
    I32TruncSF32,
    I32TruncUF32,
    I32TruncSF64,
    I32TruncUF64,
    I64ExtendSI32,
    I64ExtendUI32,
    I64TruncSF32,
    I64TruncUF32,
    I64TruncSF64,
    I64TruncUF64,
    F32ConvertSI32,
    F32ConvertUI32,
    F32ConvertSI64,
    F32ConvertUI64,
    F32DemoteF64,
    F64ConvertSI32,
    F64ConvertUI32,
    F64ConvertSI64,
    F64ConvertUI64,
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
    I32TruncSSatF32,
    I32TruncUSatF32,
    I32TruncSSatF64,
    I32TruncUSatF64,
    I64TruncSSatF32,
    I64TruncUSatF32,
    I64TruncSSatF64,
    I64TruncUSatF64,

    // 0xFE operators
    // https://github.com/WebAssembly/threads/blob/master/proposals/threads/Overview.md
    Wake { memarg: MemoryImmediate },
    I32Wait { memarg: MemoryImmediate },
    I64Wait { memarg: MemoryImmediate },
    I32AtomicLoad { memarg: MemoryImmediate },
    I64AtomicLoad { memarg: MemoryImmediate },
    I32AtomicLoad8U { memarg: MemoryImmediate },
    I32AtomicLoad16U { memarg: MemoryImmediate },
    I64AtomicLoad8U { memarg: MemoryImmediate },
    I64AtomicLoad16U { memarg: MemoryImmediate },
    I64AtomicLoad32U { memarg: MemoryImmediate },
    I32AtomicStore { memarg: MemoryImmediate },
    I64AtomicStore { memarg: MemoryImmediate },
    I32AtomicStore8 { memarg: MemoryImmediate },
    I32AtomicStore16 { memarg: MemoryImmediate },
    I64AtomicStore8 { memarg: MemoryImmediate },
    I64AtomicStore16 { memarg: MemoryImmediate },
    I64AtomicStore32 { memarg: MemoryImmediate },
    I32AtomicRmwAdd { memarg: MemoryImmediate },
    I64AtomicRmwAdd { memarg: MemoryImmediate },
    I32AtomicRmw8UAdd { memarg: MemoryImmediate },
    I32AtomicRmw16UAdd { memarg: MemoryImmediate },
    I64AtomicRmw8UAdd { memarg: MemoryImmediate },
    I64AtomicRmw16UAdd { memarg: MemoryImmediate },
    I64AtomicRmw32UAdd { memarg: MemoryImmediate },
    I32AtomicRmwSub { memarg: MemoryImmediate },
    I64AtomicRmwSub { memarg: MemoryImmediate },
    I32AtomicRmw8USub { memarg: MemoryImmediate },
    I32AtomicRmw16USub { memarg: MemoryImmediate },
    I64AtomicRmw8USub { memarg: MemoryImmediate },
    I64AtomicRmw16USub { memarg: MemoryImmediate },
    I64AtomicRmw32USub { memarg: MemoryImmediate },
    I32AtomicRmwAnd { memarg: MemoryImmediate },
    I64AtomicRmwAnd { memarg: MemoryImmediate },
    I32AtomicRmw8UAnd { memarg: MemoryImmediate },
    I32AtomicRmw16UAnd { memarg: MemoryImmediate },
    I64AtomicRmw8UAnd { memarg: MemoryImmediate },
    I64AtomicRmw16UAnd { memarg: MemoryImmediate },
    I64AtomicRmw32UAnd { memarg: MemoryImmediate },
    I32AtomicRmwOr { memarg: MemoryImmediate },
    I64AtomicRmwOr { memarg: MemoryImmediate },
    I32AtomicRmw8UOr { memarg: MemoryImmediate },
    I32AtomicRmw16UOr { memarg: MemoryImmediate },
    I64AtomicRmw8UOr { memarg: MemoryImmediate },
    I64AtomicRmw16UOr { memarg: MemoryImmediate },
    I64AtomicRmw32UOr { memarg: MemoryImmediate },
    I32AtomicRmwXor { memarg: MemoryImmediate },
    I64AtomicRmwXor { memarg: MemoryImmediate },
    I32AtomicRmw8UXor { memarg: MemoryImmediate },
    I32AtomicRmw16UXor { memarg: MemoryImmediate },
    I64AtomicRmw8UXor { memarg: MemoryImmediate },
    I64AtomicRmw16UXor { memarg: MemoryImmediate },
    I64AtomicRmw32UXor { memarg: MemoryImmediate },
    I32AtomicRmwXchg { memarg: MemoryImmediate },
    I64AtomicRmwXchg { memarg: MemoryImmediate },
    I32AtomicRmw8UXchg { memarg: MemoryImmediate },
    I32AtomicRmw16UXchg { memarg: MemoryImmediate },
    I64AtomicRmw8UXchg { memarg: MemoryImmediate },
    I64AtomicRmw16UXchg { memarg: MemoryImmediate },
    I64AtomicRmw32UXchg { memarg: MemoryImmediate },
    I32AtomicRmwCmpxchg { memarg: MemoryImmediate },
    I64AtomicRmwCmpxchg { memarg: MemoryImmediate },
    I32AtomicRmw8UCmpxchg { memarg: MemoryImmediate },
    I32AtomicRmw16UCmpxchg { memarg: MemoryImmediate },
    I64AtomicRmw8UCmpxchg { memarg: MemoryImmediate },
    I64AtomicRmw16UCmpxchg { memarg: MemoryImmediate },
    I64AtomicRmw32UCmpxchg { memarg: MemoryImmediate },
}
