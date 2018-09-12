/* Copyright 2017 Mozilla Foundation
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
// See https://github.com/WebAssembly/design/blob/master/BinaryEncoding.md

use std::result;
use std::vec::Vec;

use limits::{MAX_WASM_FUNCTION_LOCALS, MAX_WASM_FUNCTION_PARAMS, MAX_WASM_FUNCTION_RETURNS,
             MAX_WASM_FUNCTION_SIZE, MAX_WASM_STRING_SIZE, MAX_WASM_FUNCTIONS,
             MAX_WASM_TABLE_ENTRIES};

const MAX_WASM_BR_TABLE_SIZE: usize = MAX_WASM_FUNCTION_SIZE;

const MAX_DATA_CHUNK_SIZE: usize = MAX_WASM_STRING_SIZE;

#[derive(Debug,Copy,Clone)]
pub struct BinaryReaderError {
    pub message: &'static str,
    pub offset: usize,
}

pub type Result<T> = result::Result<T, BinaryReaderError>;

#[derive(Debug,Copy,Clone,PartialEq,Eq,PartialOrd,Ord)]
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
#[derive(Debug,Copy,Clone,PartialEq,Eq,PartialOrd,Ord)]
pub enum SectionCode<'a> {
    Custom {
        name: &'a [u8],
        kind: CustomSectionKind,
    },
    Type, // Function signature declarations
    Import, // Import declarations
    Function, // Function declarations
    Table, // Indirect function table and other tables
    Memory, // Memory attributes
    Global, // Global declarations
    Export, // Exports
    Start, // Start function declaration
    Element, // Elements section
    Code, // Function bodies (code)
    Data, // Data segments
}

/// Types as defined [here].
///
/// [here]: https://webassembly.github.io/spec/syntax/types.html#types
#[derive(Debug,Copy,Clone,PartialEq,Eq)]
pub enum Type {
    I32,
    I64,
    F32,
    F64,
    AnyFunc,
    Func,
    EmptyBlockType,
}

#[derive(Debug)]
pub enum NameType {
    Module,
    Function,
    Local,
}

#[derive(Debug)]
pub struct Naming<'a> {
    pub index: u32,
    pub name: &'a [u8],
}

#[derive(Debug)]
pub struct LocalName<'a> {
    pub index: u32,
    pub locals: Vec<Naming<'a>>,
}

#[derive(Debug)]
pub enum NameEntry<'a> {
    Module(&'a [u8]),
    Function(Vec<Naming<'a>>),
    Local(Vec<LocalName<'a>>),
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

#[derive(Debug,Clone)]
pub struct FuncType {
    pub form: Type,
    pub params: Vec<Type>,
    pub returns: Vec<Type>,
}

#[derive(Debug,Copy,Clone)]
pub struct ResizableLimits {
    pub initial: u32,
    pub maximum: Option<u32>,
}

#[derive(Debug,Copy,Clone)]
pub struct TableType {
    pub element_type: Type,
    pub limits: ResizableLimits,
}

#[derive(Debug,Copy,Clone)]
pub struct MemoryType {
    pub limits: ResizableLimits,
    pub shared: bool,
}

#[derive(Debug,Copy,Clone)]
pub struct GlobalType {
    pub content_type: Type,
    pub mutable: bool,
}

#[derive(Debug)]
pub struct MemoryImmediate {
    pub flags: u32,
    pub offset: u32,
}

/// A br_table entries representation.
#[derive(Debug)]
pub struct BrTable<'a> {
    buffer: &'a [u8],
}

impl<'a> BrTable<'a> {
    /// Reads br_table entries.
    ///
    /// # Examples
    /// ```rust
    /// let buf = vec![0x0e, 0x02, 0x01, 0x02, 0x00];
    /// let mut reader = wasmparser::BinaryReader::new(&buf);
    /// let op = reader.read_operator().unwrap();
    /// if let wasmparser::Operator::BrTable { ref table } = op {
    ///     let br_table_depths = table.read_table();
    ///     assert!(br_table_depths.0 == vec![1,2] &&
    ///             br_table_depths.1 == 0);
    /// } else {
    ///     unreachable!();
    /// }
    /// ```
    pub fn read_table(&self) -> (Vec<u32>, u32) {
        let mut reader = BinaryReader::new(self.buffer);
        let mut table = Vec::new();
        while !reader.eof() {
            table.push(reader.read_var_u32().unwrap());
        }
        let default_target = table.pop().unwrap();
        (table, default_target)
    }
}

/// Iterator for `BrTable`.
///
/// #Examples
/// ```rust
/// let buf = vec![0x0e, 0x02, 0x01, 0x02, 0x00];
/// let mut reader = wasmparser::BinaryReader::new(&buf);
/// let op = reader.read_operator().unwrap();
/// if let wasmparser::Operator::BrTable { ref table } = op {
///     for depth in table {
///         println!("BrTable depth: {}", depth);
///     }
/// }
/// ```
pub struct BrTableIterator<'a> {
    reader: BinaryReader<'a>,
}

impl<'a> IntoIterator for &'a BrTable<'a> {
    type Item = u32;
    type IntoIter = BrTableIterator<'a>;

    fn into_iter(self) -> Self::IntoIter {
        BrTableIterator { reader: BinaryReader::new(self.buffer) }
    }
}

impl<'a> Iterator for BrTableIterator<'a> {
    type Item = u32;

    fn next(&mut self) -> Option<u32> {
        if self.reader.eof() {
            return None;
        }
        Some(self.reader.read_var_u32().unwrap())
    }
}

#[derive(Debug)]
pub enum ImportSectionEntryType {
    Function(u32),
    Table(TableType),
    Memory(MemoryType),
    Global(GlobalType),
}

#[derive(Debug)]
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

#[derive(Debug)]
pub enum LinkingType {
    StackPointer(u32),
}

#[derive(Debug)]
pub struct RelocEntry {
    pub ty: RelocType,
    pub offset: u32,
    pub index: u32,
    pub addend: Option<u32>,
}

/// An IEEE binary32 immediate floating point value, represented as a u32
/// containing the bitpattern.
///
/// All bit patterns are allowed.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct Ieee32(u32);

impl Ieee32 {
    pub fn bits(&self) -> u32 {
        self.0
    }
}

/// An IEEE binary64 immediate floating point value, represented as a u64
/// containing the bitpattern.
///
/// All bit patterns are allowed.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct Ieee64(u64);

impl Ieee64 {
    pub fn bits(&self) -> u64 {
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

fn is_name(name: &[u8], expected: &'static str) -> bool {
    if name.len() != expected.len() {
        return false;
    }
    let expected_bytes = expected.as_bytes();
    for i in 0..name.len() {
        if name[i] != expected_bytes[i] {
            return false;
        }
    }
    true
}

fn is_name_prefix(name: &[u8], prefix: &'static str) -> bool {
    if name.len() < prefix.len() {
        return false;
    }
    let expected_bytes = prefix.as_bytes();
    for i in 0..expected_bytes.len() {
        if name[i] != expected_bytes[i] {
            return false;
        }
    }
    true
}

enum InitExpressionContinuation {
    GlobalSection,
    ElementSection,
    DataSection,
}

/// A binary reader of the WebAssembly structures and types.
#[derive(Clone)]
pub struct BinaryReader<'a> {
    buffer: &'a [u8],
    position: usize,
}

impl<'a> BinaryReader<'a> {
    /// Constructs `BinaryReader` type.
    ///
    /// # Examples
    /// ```
    /// let fn_body = &vec![0x41, 0x00, 0x10, 0x00, 0x0B];
    /// let mut reader = wasmparser::BinaryReader::new(fn_body);
    /// while !reader.eof() {
    ///     let op = reader.read_operator();
    ///     println!("{:?}", op)
    /// }
    /// ```
    pub fn new(data: &[u8]) -> BinaryReader {
        BinaryReader {
            buffer: data,
            position: 0,
        }
    }

    fn ensure_has_byte(&self) -> Result<()> {
        if self.position < self.buffer.len() {
            Ok(())
        } else {
            Err(BinaryReaderError {
                    message: "Unexpected EOF",
                    offset: self.position,
                })
        }
    }

    fn ensure_has_bytes(&self, len: usize) -> Result<()> {
        if self.position + len <= self.buffer.len() {
            Ok(())
        } else {
            Err(BinaryReaderError {
                    message: "Unexpected EOF",
                    offset: self.position,
                })
        }
    }

    fn read_var_u1(&mut self) -> Result<u32> {
        let b = self.read_u8()?;
        if (b & 0xFE) != 0 {
            return Err(BinaryReaderError {
                           message: "Invalid var_u1",
                           offset: self.position - 1,
                       });
        }
        Ok(b)
    }

    fn read_var_i7(&mut self) -> Result<i32> {
        let b = self.read_u8()?;
        if (b & 0x80) != 0 {
            return Err(BinaryReaderError {
                           message: "Invalid var_i7",
                           offset: self.position - 1,
                       });
        }
        Ok((b << 25) as i32 >> 25)
    }

    fn read_var_u7(&mut self) -> Result<u32> {
        let b = self.read_u8()?;
        if (b & 0x80) != 0 {
            return Err(BinaryReaderError {
                           message: "Invalid var_u7",
                           offset: self.position - 1,
                       });
        }
        Ok(b)
    }

    fn read_type(&mut self) -> Result<Type> {
        let code = self.read_var_i7()?;
        match code {
            -0x01 => Ok(Type::I32),
            -0x02 => Ok(Type::I64),
            -0x03 => Ok(Type::F32),
            -0x04 => Ok(Type::F64),
            -0x10 => Ok(Type::AnyFunc),
            -0x20 => Ok(Type::Func),
            -0x40 => Ok(Type::EmptyBlockType),
            _ => {
                Err(BinaryReaderError {
                        message: "Invalid type",
                        offset: self.position - 1,
                    })
            }
        }
    }

    /// Read a `count` indicating the number of times to call `read_local_decl`.
    pub fn read_local_count(&mut self) -> Result<usize> {
        let local_count = self.read_var_u32()? as usize;
        if local_count > MAX_WASM_FUNCTION_LOCALS {
            return Err(BinaryReaderError {
                           message: "local_count is out of bounds",
                           offset: self.position - 1,
                       });
        }
        Ok(local_count)
    }

    /// Read a `(count, value_type)` declaration of local variables of the same type.
    pub fn read_local_decl(&mut self, locals_total: &mut usize) -> Result<(u32, Type)> {
        let count = self.read_var_u32()?;
        let value_type = self.read_type()?;
        *locals_total = locals_total
            .checked_add(count as usize)
            .ok_or_else(|| {
                            BinaryReaderError {
                                message: "locals_total is out of bounds",
                                offset: self.position - 1,
                            }
                        })?;
        if *locals_total > MAX_WASM_FUNCTION_LOCALS {
            return Err(BinaryReaderError {
                           message: "locals_total is out of bounds",
                           offset: self.position - 1,
                       });
        }
        Ok((count, value_type))
    }

    fn read_external_kind(&mut self) -> Result<ExternalKind> {
        let code = self.read_u8()?;
        match code {
            0 => Ok(ExternalKind::Function),
            1 => Ok(ExternalKind::Table),
            2 => Ok(ExternalKind::Memory),
            3 => Ok(ExternalKind::Global),
            _ => {
                Err(BinaryReaderError {
                        message: "Invalid external kind",
                        offset: self.position - 1,
                    })
            }
        }
    }

    fn read_func_type(&mut self) -> Result<FuncType> {
        let form = self.read_type()?;
        let params_len = self.read_var_u32()? as usize;
        if params_len > MAX_WASM_FUNCTION_PARAMS {
            return Err(BinaryReaderError {
                           message: "function params size is out of bound",
                           offset: self.position - 1,
                       });
        }
        let mut params: Vec<Type> = Vec::with_capacity(params_len);
        for _ in 0..params_len {
            params.push(self.read_type()?);
        }
        let returns_len = self.read_var_u32()? as usize;
        if returns_len > MAX_WASM_FUNCTION_RETURNS {
            return Err(BinaryReaderError {
                           message: "function params size is out of bound",
                           offset: self.position - 1,
                       });
        }
        let mut returns: Vec<Type> = Vec::with_capacity(returns_len);
        for _ in 0..returns_len {
            returns.push(self.read_type()?);
        }
        Ok(FuncType {
               form: form,
               params: params,
               returns: returns,
           })
    }

    fn read_resizable_limits(&mut self, max_present: bool) -> Result<ResizableLimits> {
        let initial = self.read_var_u32()?;
        let maximum = if max_present {
            Some(self.read_var_u32()?)
        } else {
            None
        };
        Ok(ResizableLimits { initial, maximum })
    }

    fn read_table_type(&mut self) -> Result<TableType> {
        let element_type = self.read_type()?;
        let flags = self.read_var_u32()?;
        if (flags & !0x1) != 0 {
            return Err(BinaryReaderError {
                           message: "invalid table resizable limits flags",
                           offset: self.position - 1,
                       });
        }
        let limits = self.read_resizable_limits((flags & 0x1) != 0)?;
        Ok(TableType {
               element_type,
               limits,
           })
    }

    fn read_memory_type(&mut self) -> Result<MemoryType> {
        let flags = self.read_var_u32()?;
        if (flags & !0x3) != 0 {
            return Err(BinaryReaderError {
                           message: "invalid table resizable limits flags",
                           offset: self.position - 1,
                       });
        }
        let limits = self.read_resizable_limits((flags & 0x1) != 0)?;
        let shared = (flags & 0x2) != 0;
        Ok(MemoryType { limits, shared })
    }

    fn read_global_type(&mut self) -> Result<GlobalType> {
        Ok(GlobalType {
               content_type: self.read_type()?,
               mutable: self.read_var_u1()? != 0,
           })
    }

    fn read_memarg(&mut self) -> Result<MemoryImmediate> {
        Ok(MemoryImmediate {
               flags: self.read_var_u32()?,
               offset: self.read_var_u32()?,
           })
    }

    fn read_section_code(&mut self, id: u32, offset: usize) -> Result<SectionCode<'a>> {
        match id {
            0 => {
                let name = self.read_string()?;
                let kind = if is_name(name, "name") {
                    CustomSectionKind::Name
                } else if is_name(name, "sourceMappingURL") {
                    CustomSectionKind::SourceMappingURL
                } else if is_name_prefix(name, "reloc.") {
                    CustomSectionKind::Reloc
                } else if is_name(name, "linking") {
                    CustomSectionKind::Linking
                } else {
                    CustomSectionKind::Unknown
                };
                Ok(SectionCode::Custom {
                       name: name,
                       kind: kind,
                   })
            }
            1 => Ok(SectionCode::Type),
            2 => Ok(SectionCode::Import),
            3 => Ok(SectionCode::Function),
            4 => Ok(SectionCode::Table),
            5 => Ok(SectionCode::Memory),
            6 => Ok(SectionCode::Global),
            7 => Ok(SectionCode::Export),
            8 => Ok(SectionCode::Start),
            9 => Ok(SectionCode::Element),
            10 => Ok(SectionCode::Code),
            11 => Ok(SectionCode::Data),
            _ => {
                Err(BinaryReaderError {
                        message: "Invalid section code",
                        offset,
                    })
            }
        }
    }

    fn read_br_table(&mut self) -> Result<BrTable<'a>> {
        let targets_len = self.read_var_u32()? as usize;
        if targets_len > MAX_WASM_BR_TABLE_SIZE {
            return Err(BinaryReaderError {
                           message: "br_table size is out of bound",
                           offset: self.position - 1,
                       });
        }
        let start = self.position;
        for _ in 0..targets_len {
            self.skip_var_32()?;
        }
        self.skip_var_32()?;
        Ok(BrTable { buffer: &self.buffer[start..self.position] })
    }

    fn read_name_map(&mut self, limit: usize) -> Result<Vec<Naming<'a>>> {
        let count = self.read_var_u32()? as usize;
        if count > limit {
            return Err(BinaryReaderError {
                           message: "name map size is out of bound",
                           offset: self.position - 1,
                       });
        }
        let mut result = Vec::with_capacity(count);
        for _ in 0..count {
            let index = self.read_var_u32()?;
            let name = self.read_string()?;
            result.push(Naming {
                            index: index,
                            name: name,
                        });
        }
        Ok(result)
    }

    pub fn eof(&self) -> bool {
        self.position >= self.buffer.len()
    }

    pub fn current_position(&self) -> usize {
        self.position
    }

    pub fn bytes_remaining(&self) -> usize {
        self.buffer.len() - self.position
    }

    pub fn read_bytes(&mut self, size: usize) -> Result<&'a [u8]> {
        self.ensure_has_bytes(size)?;
        let start = self.position;
        self.position += size;
        Ok(&self.buffer[start..self.position])
    }

    pub fn read_u32(&mut self) -> Result<u32> {
        self.ensure_has_bytes(4)?;
        let b1 = self.buffer[self.position] as u32;
        let b2 = self.buffer[self.position + 1] as u32;
        let b3 = self.buffer[self.position + 2] as u32;
        let b4 = self.buffer[self.position + 3] as u32;
        self.position += 4;
        Ok(b1 | (b2 << 8) | (b3 << 16) | (b4 << 24))
    }

    pub fn read_u64(&mut self) -> Result<u64> {
        let w1 = self.read_u32()? as u64;
        let w2 = self.read_u32()? as u64;
        Ok(w1 | (w2 << 32))
    }

    pub fn read_u8(&mut self) -> Result<u32> {
        self.ensure_has_byte()?;
        let b = self.buffer[self.position] as u32;
        self.position += 1;
        Ok(b)
    }

    pub fn read_var_u32(&mut self) -> Result<u32> {
        // Optimization for single byte i32.
        let byte = self.read_u8()?;
        if (byte & 0x80) == 0 {
            return Ok(byte);
        }

        let mut result = byte & 0x7F;
        let mut shift = 7;
        loop {
            let byte = self.read_u8()?;
            result |= ((byte & 0x7F) as u32) << shift;
            if shift >= 25 && (byte >> (32 - shift)) != 0 {
                // The continuation bit or unused bits are set.
                return Err(BinaryReaderError {
                               message: "Invalid var_u32",
                               offset: self.position - 1,
                           });
            }
            shift += 7;
            if (byte & 0x80) == 0 {
                break;
            }
        }
        Ok(result)
    }

    pub fn skip_var_32(&mut self) -> Result<()> {
        for _ in 0..5 {
            let byte = self.read_u8()?;
            if (byte & 0x80) == 0 {
                return Ok(());
            }
        }
        Err(BinaryReaderError {
                message: "Invalid var_32",
                offset: self.position - 1,
            })
    }

    pub fn read_var_i32(&mut self) -> Result<i32> {
        // Optimization for single byte i32.
        let byte = self.read_u8()?;
        if (byte & 0x80) == 0 {
            return Ok(((byte as i32) << 25) >> 25);
        }

        let mut result = (byte & 0x7F) as i32;
        let mut shift = 7;
        loop {
            let byte = self.read_u8()?;
            result |= ((byte & 0x7F) as i32) << shift;
            if shift >= 25 {
                let continuation_bit = (byte & 0x80) != 0;
                let sign_and_unused_bit = (byte << 1) as i8 >> (32 - shift);
                if continuation_bit || (sign_and_unused_bit != 0 && sign_and_unused_bit != -1) {
                    return Err(BinaryReaderError {
                                   message: "Invalid var_i32",
                                   offset: self.position - 1,
                               });
                }
                return Ok(result);
            }
            shift += 7;
            if (byte & 0x80) == 0 {
                break;
            }
        }
        let ashift = 32 - shift;
        Ok((result << ashift) >> ashift)
    }

    pub fn read_var_i64(&mut self) -> Result<i64> {
        let mut result: i64 = 0;
        let mut shift = 0;
        loop {
            let byte = self.read_u8()?;
            result |= ((byte & 0x7F) as i64) << shift;
            if shift >= 57 {
                let continuation_bit = (byte & 0x80) != 0;
                let sign_and_unused_bit = ((byte << 1) as i8) >> (64 - shift);
                if continuation_bit || (sign_and_unused_bit != 0 && sign_and_unused_bit != -1) {
                    return Err(BinaryReaderError {
                                   message: "Invalid var_i64",
                                   offset: self.position - 1,
                               });
                }
                return Ok(result);
            }
            shift += 7;
            if (byte & 0x80) == 0 {
                break;
            }
        }
        let ashift = 64 - shift;
        Ok((result << ashift) >> ashift)
    }

    pub fn read_f32(&mut self) -> Result<Ieee32> {
        let value = self.read_u32()?;
        Ok(Ieee32(value))
    }

    pub fn read_f64(&mut self) -> Result<Ieee64> {
        let value = self.read_u64()?;
        Ok(Ieee64(value))
    }

    pub fn read_string(&mut self) -> Result<&'a [u8]> {
        let len = self.read_var_u32()? as usize;
        if len > MAX_WASM_STRING_SIZE {
            return Err(BinaryReaderError {
                           message: "string size in out of bounds",
                           offset: self.position - 1,
                       });
        }
        self.read_bytes(len)
    }

    fn read_memarg_of_align(&mut self, align: u32) -> Result<MemoryImmediate> {
        let imm = self.read_memarg()?;
        if align != imm.flags {
            return Err(BinaryReaderError {
                           message: "Unexpected memarg alignment",
                           offset: self.position - 1,
                       });
        }
        Ok(imm)
    }

    fn read_0xfe_operator(&mut self) -> Result<Operator<'a>> {
        let code = self.read_u8()? as u8;
        Ok(match code {
               0x00 => Operator::Wake { memarg: self.read_memarg_of_align(2)? },
               0x01 => Operator::I32Wait { memarg: self.read_memarg_of_align(2)? },
               0x02 => Operator::I64Wait { memarg: self.read_memarg_of_align(3)? },
               0x10 => Operator::I32AtomicLoad { memarg: self.read_memarg_of_align(2)? },
               0x11 => Operator::I64AtomicLoad { memarg: self.read_memarg_of_align(3)? },
               0x12 => Operator::I32AtomicLoad8U { memarg: self.read_memarg_of_align(0)? },
               0x13 => Operator::I32AtomicLoad16U { memarg: self.read_memarg_of_align(1)? },
               0x14 => Operator::I64AtomicLoad8U { memarg: self.read_memarg_of_align(0)? },
               0x15 => Operator::I64AtomicLoad16U { memarg: self.read_memarg_of_align(1)? },
               0x16 => Operator::I64AtomicLoad32U { memarg: self.read_memarg_of_align(2)? },
               0x17 => Operator::I32AtomicStore { memarg: self.read_memarg_of_align(2)? },
               0x18 => Operator::I64AtomicStore { memarg: self.read_memarg_of_align(3)? },
               0x19 => Operator::I32AtomicStore8 { memarg: self.read_memarg_of_align(0)? },
               0x1a => Operator::I32AtomicStore16 { memarg: self.read_memarg_of_align(1)? },
               0x1b => Operator::I64AtomicStore8 { memarg: self.read_memarg_of_align(0)? },
               0x1c => Operator::I64AtomicStore16 { memarg: self.read_memarg_of_align(1)? },
               0x1d => Operator::I64AtomicStore32 { memarg: self.read_memarg_of_align(2)? },
               0x1e => Operator::I32AtomicRmwAdd { memarg: self.read_memarg_of_align(2)? },
               0x1f => Operator::I64AtomicRmwAdd { memarg: self.read_memarg_of_align(3)? },
               0x20 => Operator::I32AtomicRmw8UAdd { memarg: self.read_memarg_of_align(0)? },
               0x21 => Operator::I32AtomicRmw16UAdd { memarg: self.read_memarg_of_align(1)? },
               0x22 => Operator::I64AtomicRmw8UAdd { memarg: self.read_memarg_of_align(0)? },
               0x23 => Operator::I64AtomicRmw16UAdd { memarg: self.read_memarg_of_align(1)? },
               0x24 => Operator::I64AtomicRmw32UAdd { memarg: self.read_memarg_of_align(2)? },
               0x25 => Operator::I32AtomicRmwSub { memarg: self.read_memarg_of_align(2)? },
               0x26 => Operator::I64AtomicRmwSub { memarg: self.read_memarg_of_align(3)? },
               0x27 => Operator::I32AtomicRmw8USub { memarg: self.read_memarg_of_align(0)? },
               0x28 => Operator::I32AtomicRmw16USub { memarg: self.read_memarg_of_align(1)? },
               0x29 => Operator::I64AtomicRmw8USub { memarg: self.read_memarg_of_align(0)? },
               0x2a => Operator::I64AtomicRmw16USub { memarg: self.read_memarg_of_align(1)? },
               0x2b => Operator::I64AtomicRmw32USub { memarg: self.read_memarg_of_align(2)? },
               0x2c => Operator::I32AtomicRmwAnd { memarg: self.read_memarg_of_align(2)? },
               0x2d => Operator::I64AtomicRmwAnd { memarg: self.read_memarg_of_align(3)? },
               0x2e => Operator::I32AtomicRmw8UAnd { memarg: self.read_memarg_of_align(0)? },
               0x2f => Operator::I32AtomicRmw16UAnd { memarg: self.read_memarg_of_align(1)? },
               0x30 => Operator::I64AtomicRmw8UAnd { memarg: self.read_memarg_of_align(0)? },
               0x31 => Operator::I64AtomicRmw16UAnd { memarg: self.read_memarg_of_align(1)? },
               0x32 => Operator::I64AtomicRmw32UAnd { memarg: self.read_memarg_of_align(2)? },
               0x33 => Operator::I32AtomicRmwOr { memarg: self.read_memarg_of_align(2)? },
               0x34 => Operator::I64AtomicRmwOr { memarg: self.read_memarg_of_align(3)? },
               0x35 => Operator::I32AtomicRmw8UOr { memarg: self.read_memarg_of_align(0)? },
               0x36 => Operator::I32AtomicRmw16UOr { memarg: self.read_memarg_of_align(1)? },
               0x37 => Operator::I64AtomicRmw8UOr { memarg: self.read_memarg_of_align(0)? },
               0x38 => Operator::I64AtomicRmw16UOr { memarg: self.read_memarg_of_align(1)? },
               0x39 => Operator::I64AtomicRmw32UOr { memarg: self.read_memarg_of_align(2)? },
               0x3a => Operator::I32AtomicRmwXor { memarg: self.read_memarg_of_align(2)? },
               0x3b => Operator::I64AtomicRmwXor { memarg: self.read_memarg_of_align(3)? },
               0x3c => Operator::I32AtomicRmw8UXor { memarg: self.read_memarg_of_align(0)? },
               0x3d => Operator::I32AtomicRmw16UXor { memarg: self.read_memarg_of_align(1)? },
               0x3e => Operator::I64AtomicRmw8UXor { memarg: self.read_memarg_of_align(0)? },
               0x3f => Operator::I64AtomicRmw16UXor { memarg: self.read_memarg_of_align(1)? },
               0x40 => Operator::I64AtomicRmw32UXor { memarg: self.read_memarg_of_align(2)? },
               0x41 => Operator::I32AtomicRmwXchg { memarg: self.read_memarg_of_align(2)? },
               0x42 => Operator::I64AtomicRmwXchg { memarg: self.read_memarg_of_align(3)? },
               0x43 => Operator::I32AtomicRmw8UXchg { memarg: self.read_memarg_of_align(0)? },
               0x44 => Operator::I32AtomicRmw16UXchg { memarg: self.read_memarg_of_align(1)? },
               0x45 => Operator::I64AtomicRmw8UXchg { memarg: self.read_memarg_of_align(0)? },
               0x46 => Operator::I64AtomicRmw16UXchg { memarg: self.read_memarg_of_align(1)? },
               0x47 => Operator::I64AtomicRmw32UXchg { memarg: self.read_memarg_of_align(2)? },
               0x48 => Operator::I32AtomicRmwCmpxchg { memarg: self.read_memarg_of_align(2)? },
               0x49 => Operator::I64AtomicRmwCmpxchg { memarg: self.read_memarg_of_align(3)? },
               0x4a => Operator::I32AtomicRmw8UCmpxchg { memarg: self.read_memarg_of_align(0)? },
               0x4b => Operator::I32AtomicRmw16UCmpxchg { memarg: self.read_memarg_of_align(1)? },
               0x4c => Operator::I64AtomicRmw8UCmpxchg { memarg: self.read_memarg_of_align(0)? },
               0x4d => Operator::I64AtomicRmw16UCmpxchg { memarg: self.read_memarg_of_align(1)? },
               0x4e => Operator::I64AtomicRmw32UCmpxchg { memarg: self.read_memarg_of_align(2)? },

               _ => {
                   return Err(BinaryReaderError {
                                  message: "Unknown 0xFE opcode",
                                  offset: self.position - 1,
                              })
               }
           })
    }

    pub fn read_operator(&mut self) -> Result<Operator<'a>> {
        let code = self.read_u8()? as u8;
        Ok(match code {
               0x00 => Operator::Unreachable,
               0x01 => Operator::Nop,
               0x02 => Operator::Block { ty: self.read_type()? },
               0x03 => Operator::Loop { ty: self.read_type()? },
               0x04 => Operator::If { ty: self.read_type()? },
               0x05 => Operator::Else,
               0x0b => Operator::End,
               0x0c => Operator::Br { relative_depth: self.read_var_u32()? },
               0x0d => Operator::BrIf { relative_depth: self.read_var_u32()? },
               0x0e => Operator::BrTable { table: self.read_br_table()? },
               0x0f => Operator::Return,
               0x10 => Operator::Call { function_index: self.read_var_u32()? },
               0x11 => {
                   Operator::CallIndirect {
                       index: self.read_var_u32()?,
                       table_index: self.read_var_u1()?,
                   }
               }
               0x1a => Operator::Drop,
               0x1b => Operator::Select,
               0x20 => Operator::GetLocal { local_index: self.read_var_u32()? },
               0x21 => Operator::SetLocal { local_index: self.read_var_u32()? },
               0x22 => Operator::TeeLocal { local_index: self.read_var_u32()? },
               0x23 => Operator::GetGlobal { global_index: self.read_var_u32()? },
               0x24 => Operator::SetGlobal { global_index: self.read_var_u32()? },
               0x28 => Operator::I32Load { memarg: self.read_memarg()? },
               0x29 => Operator::I64Load { memarg: self.read_memarg()? },
               0x2a => Operator::F32Load { memarg: self.read_memarg()? },
               0x2b => Operator::F64Load { memarg: self.read_memarg()? },
               0x2c => Operator::I32Load8S { memarg: self.read_memarg()? },
               0x2d => Operator::I32Load8U { memarg: self.read_memarg()? },
               0x2e => Operator::I32Load16S { memarg: self.read_memarg()? },
               0x2f => Operator::I32Load16U { memarg: self.read_memarg()? },
               0x30 => Operator::I64Load8S { memarg: self.read_memarg()? },
               0x31 => Operator::I64Load8U { memarg: self.read_memarg()? },
               0x32 => Operator::I64Load16S { memarg: self.read_memarg()? },
               0x33 => Operator::I64Load16U { memarg: self.read_memarg()? },
               0x34 => Operator::I64Load32S { memarg: self.read_memarg()? },
               0x35 => Operator::I64Load32U { memarg: self.read_memarg()? },
               0x36 => Operator::I32Store { memarg: self.read_memarg()? },
               0x37 => Operator::I64Store { memarg: self.read_memarg()? },
               0x38 => Operator::F32Store { memarg: self.read_memarg()? },
               0x39 => Operator::F64Store { memarg: self.read_memarg()? },
               0x3a => Operator::I32Store8 { memarg: self.read_memarg()? },
               0x3b => Operator::I32Store16 { memarg: self.read_memarg()? },
               0x3c => Operator::I64Store8 { memarg: self.read_memarg()? },
               0x3d => Operator::I64Store16 { memarg: self.read_memarg()? },
               0x3e => Operator::I64Store32 { memarg: self.read_memarg()? },
               0x3f => Operator::MemorySize { reserved: self.read_var_u1()? },
               0x40 => Operator::MemoryGrow { reserved: self.read_var_u1()? },
               0x41 => Operator::I32Const { value: self.read_var_i32()? },
               0x42 => Operator::I64Const { value: self.read_var_i64()? },
               0x43 => Operator::F32Const { value: self.read_f32()? },
               0x44 => Operator::F64Const { value: self.read_f64()? },
               0x45 => Operator::I32Eqz,
               0x46 => Operator::I32Eq,
               0x47 => Operator::I32Ne,
               0x48 => Operator::I32LtS,
               0x49 => Operator::I32LtU,
               0x4a => Operator::I32GtS,
               0x4b => Operator::I32GtU,
               0x4c => Operator::I32LeS,
               0x4d => Operator::I32LeU,
               0x4e => Operator::I32GeS,
               0x4f => Operator::I32GeU,
               0x50 => Operator::I64Eqz,
               0x51 => Operator::I64Eq,
               0x52 => Operator::I64Ne,
               0x53 => Operator::I64LtS,
               0x54 => Operator::I64LtU,
               0x55 => Operator::I64GtS,
               0x56 => Operator::I64GtU,
               0x57 => Operator::I64LeS,
               0x58 => Operator::I64LeU,
               0x59 => Operator::I64GeS,
               0x5a => Operator::I64GeU,
               0x5b => Operator::F32Eq,
               0x5c => Operator::F32Ne,
               0x5d => Operator::F32Lt,
               0x5e => Operator::F32Gt,
               0x5f => Operator::F32Le,
               0x60 => Operator::F32Ge,
               0x61 => Operator::F64Eq,
               0x62 => Operator::F64Ne,
               0x63 => Operator::F64Lt,
               0x64 => Operator::F64Gt,
               0x65 => Operator::F64Le,
               0x66 => Operator::F64Ge,
               0x67 => Operator::I32Clz,
               0x68 => Operator::I32Ctz,
               0x69 => Operator::I32Popcnt,
               0x6a => Operator::I32Add,
               0x6b => Operator::I32Sub,
               0x6c => Operator::I32Mul,
               0x6d => Operator::I32DivS,
               0x6e => Operator::I32DivU,
               0x6f => Operator::I32RemS,
               0x70 => Operator::I32RemU,
               0x71 => Operator::I32And,
               0x72 => Operator::I32Or,
               0x73 => Operator::I32Xor,
               0x74 => Operator::I32Shl,
               0x75 => Operator::I32ShrS,
               0x76 => Operator::I32ShrU,
               0x77 => Operator::I32Rotl,
               0x78 => Operator::I32Rotr,
               0x79 => Operator::I64Clz,
               0x7a => Operator::I64Ctz,
               0x7b => Operator::I64Popcnt,
               0x7c => Operator::I64Add,
               0x7d => Operator::I64Sub,
               0x7e => Operator::I64Mul,
               0x7f => Operator::I64DivS,
               0x80 => Operator::I64DivU,
               0x81 => Operator::I64RemS,
               0x82 => Operator::I64RemU,
               0x83 => Operator::I64And,
               0x84 => Operator::I64Or,
               0x85 => Operator::I64Xor,
               0x86 => Operator::I64Shl,
               0x87 => Operator::I64ShrS,
               0x88 => Operator::I64ShrU,
               0x89 => Operator::I64Rotl,
               0x8a => Operator::I64Rotr,
               0x8b => Operator::F32Abs,
               0x8c => Operator::F32Neg,
               0x8d => Operator::F32Ceil,
               0x8e => Operator::F32Floor,
               0x8f => Operator::F32Trunc,
               0x90 => Operator::F32Nearest,
               0x91 => Operator::F32Sqrt,
               0x92 => Operator::F32Add,
               0x93 => Operator::F32Sub,
               0x94 => Operator::F32Mul,
               0x95 => Operator::F32Div,
               0x96 => Operator::F32Min,
               0x97 => Operator::F32Max,
               0x98 => Operator::F32Copysign,
               0x99 => Operator::F64Abs,
               0x9a => Operator::F64Neg,
               0x9b => Operator::F64Ceil,
               0x9c => Operator::F64Floor,
               0x9d => Operator::F64Trunc,
               0x9e => Operator::F64Nearest,
               0x9f => Operator::F64Sqrt,
               0xa0 => Operator::F64Add,
               0xa1 => Operator::F64Sub,
               0xa2 => Operator::F64Mul,
               0xa3 => Operator::F64Div,
               0xa4 => Operator::F64Min,
               0xa5 => Operator::F64Max,
               0xa6 => Operator::F64Copysign,
               0xa7 => Operator::I32WrapI64,
               0xa8 => Operator::I32TruncSF32,
               0xa9 => Operator::I32TruncUF32,
               0xaa => Operator::I32TruncSF64,
               0xab => Operator::I32TruncUF64,
               0xac => Operator::I64ExtendSI32,
               0xad => Operator::I64ExtendUI32,
               0xae => Operator::I64TruncSF32,
               0xaf => Operator::I64TruncUF32,
               0xb0 => Operator::I64TruncSF64,
               0xb1 => Operator::I64TruncUF64,
               0xb2 => Operator::F32ConvertSI32,
               0xb3 => Operator::F32ConvertUI32,
               0xb4 => Operator::F32ConvertSI64,
               0xb5 => Operator::F32ConvertUI64,
               0xb6 => Operator::F32DemoteF64,
               0xb7 => Operator::F64ConvertSI32,
               0xb8 => Operator::F64ConvertUI32,
               0xb9 => Operator::F64ConvertSI64,
               0xba => Operator::F64ConvertUI64,
               0xbb => Operator::F64PromoteF32,
               0xbc => Operator::I32ReinterpretF32,
               0xbd => Operator::I64ReinterpretF64,
               0xbe => Operator::F32ReinterpretI32,
               0xbf => Operator::F64ReinterpretI64,

               0xc0 => Operator::I32Extend8S,
               0xc1 => Operator::I32Extend16S,
               0xc2 => Operator::I64Extend8S,
               0xc3 => Operator::I64Extend16S,
               0xc4 => Operator::I64Extend32S,

               0xfc => self.read_0xfc_operator()?,

               0xfe => self.read_0xfe_operator()?,

               _ => {
                   return Err(BinaryReaderError {
                                  message: "Unknown opcode",
                                  offset: self.position - 1,
                              })
               }
           })
    }

    fn read_0xfc_operator(&mut self) -> Result<Operator<'a>> {
        let code = self.read_u8()? as u8;
        Ok(match code {
               0x00 => Operator::I32TruncSSatF32,
               0x01 => Operator::I32TruncUSatF32,
               0x02 => Operator::I32TruncSSatF64,
               0x03 => Operator::I32TruncUSatF64,
               0x04 => Operator::I64TruncSSatF32,
               0x05 => Operator::I64TruncUSatF32,
               0x06 => Operator::I64TruncSSatF64,
               0x07 => Operator::I64TruncUSatF64,

               _ => {
                   return Err(BinaryReaderError {
                                  message: "Unknown 0xfc opcode",
                                  offset: self.position - 1,
                              })
               }
           })
    }
}


/// Bytecode range in the WebAssembly module.
#[derive(Debug, Copy, Clone)]
pub struct Range {
    pub start: usize,
    pub end: usize,
}

impl Range {
    pub fn new(start: usize, end: usize) -> Range {
        assert!(start <= end);
        Range { start, end }
    }

    pub fn slice<'a>(&self, data: &'a [u8]) -> &'a [u8] {
        &data[self.start..self.end]
    }
}

#[derive(Debug)]
pub enum ParserState<'a> {
    Error(BinaryReaderError),
    Initial,
    BeginWasm { version: u32 },
    EndWasm,
    BeginSection { code: SectionCode<'a>, range: Range },
    EndSection,
    SkippingSection,
    ReadingCustomSection(CustomSectionKind),
    ReadingSectionRawData,
    SectionRawData(&'a [u8]),

    TypeSectionEntry(FuncType),
    ImportSectionEntry {
        module: &'a [u8],
        field: &'a [u8],
        ty: ImportSectionEntryType,
    },
    FunctionSectionEntry(u32),
    TableSectionEntry(TableType),
    MemorySectionEntry(MemoryType),
    ExportSectionEntry {
        field: &'a [u8],
        kind: ExternalKind,
        index: u32,
    },
    NameSectionEntry(NameEntry<'a>),
    StartSectionEntry(u32),

    BeginInitExpressionBody,
    InitExpressionOperator(Operator<'a>),
    EndInitExpressionBody,

    BeginFunctionBody { range: Range },
    FunctionBodyLocals { locals: Vec<(u32, Type)> },
    CodeOperator(Operator<'a>),
    EndFunctionBody,
    SkippingFunctionBody,

    BeginElementSectionEntry(u32),
    ElementSectionEntryBody(Vec<u32>),
    EndElementSectionEntry,

    BeginDataSectionEntry(u32),
    EndDataSectionEntry,
    BeginDataSectionEntryBody(u32),
    DataSectionEntryBodyChunk(&'a [u8]),
    EndDataSectionEntryBody,

    BeginGlobalSectionEntry(GlobalType),
    EndGlobalSectionEntry,

    RelocSectionHeader(SectionCode<'a>),
    RelocSectionEntry(RelocEntry),
    LinkingSectionEntry(LinkingType),

    SourceMappingURL(&'a [u8]),
}

#[derive(Debug, Copy, Clone)]
pub enum ParserInput {
    Default,
    SkipSection,
    SkipFunctionBody,
    ReadCustomSection,
    ReadSectionRawData,
}

pub trait WasmDecoder<'a> {
    fn read(&mut self) -> &ParserState<'a>;
    fn push_input(&mut self, input: ParserInput);
    fn read_with_input(&mut self, input: ParserInput) -> &ParserState<'a>;
    fn create_binary_reader<'b>(&mut self) -> BinaryReader<'b> where 'a: 'b;
    fn last_state(&self) -> &ParserState<'a>;
}

/// The `Parser` type. A simple event-driven parser of WebAssembly binary
/// format. The `read(&mut self)` is used to iterate through WebAssembly records.
pub struct Parser<'a> {
    reader: BinaryReader<'a>,
    state: ParserState<'a>,
    section_range: Option<Range>,
    function_range: Option<Range>,
    init_expr_continuation: Option<InitExpressionContinuation>,
    read_data_bytes: Option<u32>,
    section_entries_left: u32,
}

const WASM_MAGIC_NUMBER: u32 = 0x6d736100;
const WASM_EXPERIMENTAL_VERSION: u32 = 0xd;
const WASM_SUPPORTED_VERSION: u32 = 0x1;

impl<'a> Parser<'a> {
    /// Constructs `Parser` type.
    ///
    /// # Examples
    /// ```
    /// let data: &[u8] = &[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    ///     0x01, 0x4, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00,
    ///     0x0a, 0x05, 0x01, 0x03, 0x00, 0x01, 0x0b];
    /// let mut parser = wasmparser::Parser::new(data);
    /// ```
    pub fn new(data: &[u8]) -> Parser {
        Parser {
            reader: BinaryReader::new(data),
            state: ParserState::Initial,
            section_range: None,
            function_range: None,
            init_expr_continuation: None,
            read_data_bytes: None,
            section_entries_left: 0,
        }
    }

    pub fn eof(&self) -> bool {
        self.reader.eof()
    }

    pub fn current_position(&self) -> usize {
        self.reader.current_position()
    }

    fn read_header(&mut self) -> Result<()> {
        let magic_number = self.reader.read_u32()?;
        if magic_number != WASM_MAGIC_NUMBER {
            return Err(BinaryReaderError {
                           message: "Bad magic number",
                           offset: self.reader.position - 4,
                       });
        }
        let version = self.reader.read_u32()?;
        if version != WASM_SUPPORTED_VERSION && version != WASM_EXPERIMENTAL_VERSION {
            return Err(BinaryReaderError {
                           message: "Bad version number",
                           offset: self.reader.position - 4,
                       });
        }
        self.state = ParserState::BeginWasm { version: version };
        Ok(())
    }

    fn read_section_header(&mut self) -> Result<()> {
        let id_position = self.reader.position;
        let id = self.reader.read_var_u7()?;
        let payload_len = self.reader.read_var_u32()? as usize;
        let payload_end = self.reader.position + payload_len;
        let code = self.reader.read_section_code(id, id_position)?;
        if self.reader.buffer.len() < payload_end {
            return Err(BinaryReaderError {
                           message: "Section body extends past end of file",
                           offset: self.reader.buffer.len(),
                       });
        }
        if self.reader.position > payload_end {
            return Err(BinaryReaderError {
                           message: "Section header is too big to fit into section body",
                           offset: payload_end,
                       });
        }
        let range = Range {
            start: self.reader.position,
            end: payload_end,
        };
        self.state = ParserState::BeginSection { code, range };
        self.section_range = Some(range);
        Ok(())
    }

    fn read_type_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        self.state = ParserState::TypeSectionEntry(self.reader.read_func_type()?);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_import_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        let module = self.reader.read_string()?;
        let field = self.reader.read_string()?;
        let kind = self.reader.read_external_kind()?;
        let ty: ImportSectionEntryType;
        match kind {
            ExternalKind::Function => {
                ty = ImportSectionEntryType::Function(self.reader.read_var_u32()?)
            }
            ExternalKind::Table => {
                ty = ImportSectionEntryType::Table(self.reader.read_table_type()?)
            }
            ExternalKind::Memory => {
                ty = ImportSectionEntryType::Memory(self.reader.read_memory_type()?)
            }
            ExternalKind::Global => {
                ty = ImportSectionEntryType::Global(self.reader.read_global_type()?)
            }
        }

        self.state = ParserState::ImportSectionEntry {
            module: module,
            field: field,
            ty: ty,
        };
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_function_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        self.state = ParserState::FunctionSectionEntry(self.reader.read_var_u32()?);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_memory_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        self.state = ParserState::MemorySectionEntry(self.reader.read_memory_type()?);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_global_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        self.state = ParserState::BeginGlobalSectionEntry(self.reader.read_global_type()?);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_init_expression_body(&mut self, cont: InitExpressionContinuation) {
        self.state = ParserState::BeginInitExpressionBody;
        self.init_expr_continuation = Some(cont);
    }

    fn read_init_expression_operator(&mut self) -> Result<()> {
        let op = self.reader.read_operator()?;
        if let Operator::End = op {
            self.state = ParserState::EndInitExpressionBody;
            return Ok(());
        }
        self.state = ParserState::InitExpressionOperator(op);
        Ok(())
    }

    fn read_export_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        let field = self.reader.read_string()?;
        let kind = self.reader.read_external_kind()?;
        let index = self.reader.read_var_u32()?;
        self.state = ParserState::ExportSectionEntry {
            field: field,
            kind: kind,
            index: index,
        };
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_element_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        self.state = ParserState::BeginElementSectionEntry(self.reader.read_var_u32()?);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_element_entry_body(&mut self) -> Result<()> {
        let num_elements = self.reader.read_var_u32()? as usize;
        if num_elements > MAX_WASM_TABLE_ENTRIES {
            return Err(BinaryReaderError {
                           message: "num_elements is out of bounds",
                           offset: self.reader.position - 1,
                       });
        }
        let mut elements: Vec<u32> = Vec::with_capacity(num_elements);
        for _ in 0..num_elements {
            elements.push(self.reader.read_var_u32()?);
        }
        self.state = ParserState::ElementSectionEntryBody(elements);
        Ok(())
    }

    fn read_function_body(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        let size = self.reader.read_var_u32()? as usize;
        let body_end = self.reader.position + size;
        let range = Range {
            start: self.reader.position,
            end: body_end,
        };
        self.state = ParserState::BeginFunctionBody { range };
        self.function_range = Some(range);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_function_body_locals(&mut self) -> Result<()> {
        let local_count = self.reader.read_local_count()?;
        let mut locals: Vec<(u32, Type)> = Vec::with_capacity(local_count);
        let mut locals_total = 0;
        for _ in 0..local_count {
            let (count, ty) = self.reader.read_local_decl(&mut locals_total)?;
            locals.push((count, ty));
        }
        self.state = ParserState::FunctionBodyLocals { locals };
        Ok(())
    }

    fn read_code_operator(&mut self) -> Result<()> {
        if self.reader.position >= self.function_range.unwrap().end {
            if let ParserState::CodeOperator(Operator::End) = self.state {
                self.state = ParserState::EndFunctionBody;
                self.function_range = None;
                return Ok(());
            }
            return Err(BinaryReaderError {
                           message: "Expected end of function marker",
                           offset: self.function_range.unwrap().end,
                       });
        }
        let op = self.reader.read_operator()?;
        self.state = ParserState::CodeOperator(op);
        Ok(())
    }

    fn read_table_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        self.state = ParserState::TableSectionEntry(self.reader.read_table_type()?);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_data_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        let index = self.reader.read_var_u32()?;
        self.state = ParserState::BeginDataSectionEntry(index);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_data_entry_body(&mut self) -> Result<()> {
        let size = self.reader.read_var_u32()?;
        self.state = ParserState::BeginDataSectionEntryBody(size);
        self.read_data_bytes = Some(size);
        Ok(())
    }

    fn read_name_type(&mut self) -> Result<NameType> {
        let code = self.reader.read_var_u7()?;
        match code {
            0 => Ok(NameType::Module),
            1 => Ok(NameType::Function),
            2 => Ok(NameType::Local),
            _ => {
                Err(BinaryReaderError {
                        message: "Invalid name type",
                        offset: self.reader.position - 1,
                    })
            }
        }
    }

    fn read_name_entry(&mut self) -> Result<()> {
        if self.reader.position >= self.section_range.unwrap().end {
            return self.position_to_section_end();
        }
        let ty = self.read_name_type()?;
        self.reader.read_var_u32()?; // payload_len
        let entry = match ty {
            NameType::Module => NameEntry::Module(self.reader.read_string()?),
            NameType::Function => {
                NameEntry::Function(self.reader.read_name_map(MAX_WASM_FUNCTIONS)?)
            }
            NameType::Local => {
                let funcs_len = self.reader.read_var_u32()? as usize;
                if funcs_len > MAX_WASM_FUNCTIONS {
                    return Err(BinaryReaderError {
                                   message: "function count is out of bounds",
                                   offset: self.reader.position - 1,
                               });
                }
                let mut funcs: Vec<LocalName<'a>> = Vec::with_capacity(funcs_len);
                for _ in 0..funcs_len {
                    funcs.push(LocalName {
                                   index: self.reader.read_var_u32()?,
                                   locals: self.reader.read_name_map(MAX_WASM_FUNCTION_LOCALS)?,
                               });
                }
                NameEntry::Local(funcs)
            }
        };
        self.state = ParserState::NameSectionEntry(entry);
        Ok(())
    }

    fn read_source_mapping(&mut self) -> Result<()> {
        self.state = ParserState::SourceMappingURL(self.reader.read_string()?);
        Ok(())
    }

    // See https://github.com/WebAssembly/tool-conventions/blob/master/Linking.md
    fn read_reloc_header(&mut self) -> Result<()> {
        let section_id_position = self.reader.position;
        let section_id = self.reader.read_var_u7()?;
        let section_code = self.reader
            .read_section_code(section_id, section_id_position)?;
        self.state = ParserState::RelocSectionHeader(section_code);
        Ok(())
    }

    fn read_reloc_type(&mut self) -> Result<RelocType> {
        let code = self.reader.read_var_u7()?;
        match code {
            0 => Ok(RelocType::FunctionIndexLEB),
            1 => Ok(RelocType::TableIndexSLEB),
            2 => Ok(RelocType::TableIndexI32),
            3 => Ok(RelocType::GlobalAddrLEB),
            4 => Ok(RelocType::GlobalAddrSLEB),
            5 => Ok(RelocType::GlobalAddrI32),
            6 => Ok(RelocType::TypeIndexLEB),
            7 => Ok(RelocType::GlobalIndexLEB),
            _ => {
                Err(BinaryReaderError {
                        message: "Invalid reloc type",
                        offset: self.reader.position - 1,
                    })
            }
        }
    }

    fn read_reloc_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        let ty = self.read_reloc_type()?;
        let offset = self.reader.read_var_u32()?;
        let index = self.reader.read_var_u32()?;
        let addend = match ty {
            RelocType::FunctionIndexLEB |
            RelocType::TableIndexSLEB |
            RelocType::TableIndexI32 |
            RelocType::TypeIndexLEB |
            RelocType::GlobalIndexLEB => None,
            RelocType::GlobalAddrLEB |
            RelocType::GlobalAddrSLEB |
            RelocType::GlobalAddrI32 => Some(self.reader.read_var_u32()?),
        };
        self.state = ParserState::RelocSectionEntry(RelocEntry {
                                                        ty: ty,
                                                        offset: offset,
                                                        index: index,
                                                        addend: addend,
                                                    });
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_linking_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        let ty = self.reader.read_var_u32()?;
        let entry = match ty {
            1 => LinkingType::StackPointer(self.reader.read_var_u32()?),
            _ => {
                return Err(BinaryReaderError {
                               message: "Invalid linking type",
                               offset: self.reader.position - 1,
                           });
            }
        };
        self.state = ParserState::LinkingSectionEntry(entry);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_section_body(&mut self) -> Result<()> {
        match self.state {
            ParserState::BeginSection { code: SectionCode::Type, .. } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_type_entry()?;
            }
            ParserState::BeginSection { code: SectionCode::Import, .. } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_import_entry()?;
            }
            ParserState::BeginSection { code: SectionCode::Function, .. } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_function_entry()?;
            }
            ParserState::BeginSection { code: SectionCode::Memory, .. } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_memory_entry()?;
            }
            ParserState::BeginSection { code: SectionCode::Global, .. } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_global_entry()?;
            }
            ParserState::BeginSection { code: SectionCode::Export, .. } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_export_entry()?;
            }
            ParserState::BeginSection { code: SectionCode::Element, .. } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_element_entry()?;
            }
            ParserState::BeginSection { code: SectionCode::Code, .. } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_function_body()?;
            }
            ParserState::BeginSection { code: SectionCode::Table, .. } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_table_entry()?;
            }
            ParserState::BeginSection { code: SectionCode::Data, .. } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_data_entry()?;
            }
            ParserState::BeginSection { code: SectionCode::Start, .. } => {
                self.state = ParserState::StartSectionEntry(self.reader.read_var_u32()?);
            }
            ParserState::BeginSection { code: SectionCode::Custom { .. }, .. } => {
                self.read_section_body_bytes()?;
            }
            _ => unreachable!(),
        }
        Ok(())
    }

    fn read_custom_section_body(&mut self) -> Result<()> {
        match self.state {
            ParserState::ReadingCustomSection(CustomSectionKind::Name) => {
                self.read_name_entry()?;
            }
            ParserState::ReadingCustomSection(CustomSectionKind::SourceMappingURL) => {
                self.read_source_mapping()?;
            }
            ParserState::ReadingCustomSection(CustomSectionKind::Reloc) => {
                self.read_reloc_header()?;
            }
            ParserState::ReadingCustomSection(CustomSectionKind::Linking) => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_linking_entry()?;
            }
            ParserState::ReadingCustomSection(CustomSectionKind::Unknown) => {
                self.read_section_body_bytes()?;
            }
            _ => unreachable!(),
        }
        Ok(())
    }

    fn ensure_reader_position_in_section_range(&self) -> Result<()> {
        if self.section_range.unwrap().end < self.reader.position {
            return Err(BinaryReaderError {
                           message: "Position past the section end",
                           offset: self.section_range.unwrap().end,
                       });
        }
        Ok(())
    }

    fn position_to_section_end(&mut self) -> Result<()> {
        self.ensure_reader_position_in_section_range()?;
        self.reader.position = self.section_range.unwrap().end;
        self.section_range = None;
        self.state = ParserState::EndSection;
        Ok(())
    }

    fn read_section_body_bytes(&mut self) -> Result<()> {
        self.ensure_reader_position_in_section_range()?;
        if self.section_range.unwrap().end == self.reader.position {
            self.state = ParserState::EndSection;
            self.section_range = None;
            return Ok(());
        }
        let to_read = if self.section_range.unwrap().end - self.reader.position <
                         MAX_DATA_CHUNK_SIZE {
            self.section_range.unwrap().end - self.reader.position
        } else {
            MAX_DATA_CHUNK_SIZE
        };
        let bytes = self.reader.read_bytes(to_read)?;
        self.state = ParserState::SectionRawData(bytes);
        Ok(())
    }

    fn read_data_chunk(&mut self) -> Result<()> {
        if self.read_data_bytes.unwrap() == 0 {
            self.state = ParserState::EndDataSectionEntryBody;
            self.read_data_bytes = None;
            return Ok(());
        }
        let to_read = if self.read_data_bytes.unwrap() as usize > MAX_DATA_CHUNK_SIZE {
            MAX_DATA_CHUNK_SIZE
        } else {
            self.read_data_bytes.unwrap() as usize
        };
        let chunk = self.reader.read_bytes(to_read)?;
        *self.read_data_bytes.as_mut().unwrap() -= to_read as u32;
        self.state = ParserState::DataSectionEntryBodyChunk(chunk);
        Ok(())
    }

    fn read_next_section(&mut self) -> Result<()> {
        if self.reader.eof() {
            self.state = ParserState::EndWasm;
        } else {
            self.read_section_header()?;
        }
        Ok(())
    }

    fn read_wrapped(&mut self) -> Result<()> {
        match self.state {
            ParserState::EndWasm => panic!("Parser in end state"),
            ParserState::Error(_) => panic!("Parser in error state"),
            ParserState::Initial => self.read_header()?,
            ParserState::BeginWasm { .. } |
            ParserState::EndSection => self.read_next_section()?,
            ParserState::BeginSection { .. } => self.read_section_body()?,
            ParserState::SkippingSection => {
                self.position_to_section_end()?;
                self.read_next_section()?;
            }
            ParserState::TypeSectionEntry(_) => self.read_type_entry()?,
            ParserState::ImportSectionEntry { .. } => self.read_import_entry()?,
            ParserState::FunctionSectionEntry(_) => self.read_function_entry()?,
            ParserState::MemorySectionEntry(_) => self.read_memory_entry()?,
            ParserState::TableSectionEntry(_) => self.read_table_entry()?,
            ParserState::ExportSectionEntry { .. } => self.read_export_entry()?,
            ParserState::BeginGlobalSectionEntry(_) => {
                self.read_init_expression_body(InitExpressionContinuation::GlobalSection)
            }
            ParserState::EndGlobalSectionEntry => self.read_global_entry()?,
            ParserState::BeginElementSectionEntry(_) => {
                self.read_init_expression_body(InitExpressionContinuation::ElementSection)
            }
            ParserState::BeginInitExpressionBody |
            ParserState::InitExpressionOperator(_) => self.read_init_expression_operator()?,
            ParserState::BeginDataSectionEntry(_) => {
                self.read_init_expression_body(InitExpressionContinuation::DataSection)
            }
            ParserState::EndInitExpressionBody => {
                match self.init_expr_continuation {
                    Some(InitExpressionContinuation::GlobalSection) => {
                        self.state = ParserState::EndGlobalSectionEntry
                    }
                    Some(InitExpressionContinuation::ElementSection) => {
                        self.read_element_entry_body()?
                    }
                    Some(InitExpressionContinuation::DataSection) => self.read_data_entry_body()?,
                    None => unreachable!(),
                }
                self.init_expr_continuation = None;
            }
            ParserState::BeginFunctionBody { .. } => self.read_function_body_locals()?,
            ParserState::FunctionBodyLocals { .. } |
            ParserState::CodeOperator(_) => self.read_code_operator()?,
            ParserState::EndFunctionBody => self.read_function_body()?,
            ParserState::SkippingFunctionBody => {
                assert!(self.reader.position <= self.function_range.unwrap().end);
                self.reader.position = self.function_range.unwrap().end;
                self.function_range = None;
                self.read_function_body()?;
            }
            ParserState::EndDataSectionEntry => self.read_data_entry()?,
            ParserState::BeginDataSectionEntryBody(_) |
            ParserState::DataSectionEntryBodyChunk(_) => self.read_data_chunk()?,
            ParserState::EndDataSectionEntryBody => {
                self.state = ParserState::EndDataSectionEntry;
            }
            ParserState::ElementSectionEntryBody(_) => {
                self.state = ParserState::EndElementSectionEntry;
            }
            ParserState::EndElementSectionEntry => self.read_element_entry()?,
            ParserState::StartSectionEntry(_) => self.position_to_section_end()?,
            ParserState::NameSectionEntry(_) => self.read_name_entry()?,
            ParserState::SourceMappingURL(_) => self.position_to_section_end()?,
            ParserState::RelocSectionHeader(_) => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_reloc_entry()?;
            }
            ParserState::RelocSectionEntry(_) => self.read_reloc_entry()?,
            ParserState::LinkingSectionEntry(_) => self.read_linking_entry()?,
            ParserState::ReadingCustomSection(_) => self.read_custom_section_body()?,
            ParserState::ReadingSectionRawData |
            ParserState::SectionRawData(_) => self.read_section_body_bytes()?,
        }
        Ok(())
    }

    fn skip_section(&mut self) {
        match self.state {
            ParserState::Initial |
            ParserState::EndWasm |
            ParserState::Error(_) |
            ParserState::BeginWasm { .. } |
            ParserState::EndSection => panic!("Invalid reader state during skip section"),
            _ => self.state = ParserState::SkippingSection,
        }
    }

    fn skip_function_body(&mut self) {
        match self.state {
            ParserState::BeginFunctionBody { .. } |
            ParserState::FunctionBodyLocals { .. } |
            ParserState::CodeOperator(_) => self.state = ParserState::SkippingFunctionBody,
            _ => panic!("Invalid reader state during skip function body"),
        }
    }

    fn read_custom_section(&mut self) {
        match self.state {
            ParserState::BeginSection { code: SectionCode::Custom { kind, .. }, .. } => {
                self.state = ParserState::ReadingCustomSection(kind);
            }
            _ => panic!("Invalid reader state during reading custom section"),
        }
    }

    fn read_raw_section_data(&mut self) {
        match self.state {
            ParserState::BeginSection { .. } => self.state = ParserState::ReadingSectionRawData,
            _ => panic!("Invalid reader state during reading raw section data"),
        }
    }
}

impl<'a> WasmDecoder<'a> for Parser<'a> {
    /// Reads next record from the WebAssembly binary data. The methods returns
    /// reference to current state of the parser. See `ParserState` num.
    ///
    /// # Examples
    /// ```
    /// # let data: &[u8] = &[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    /// #     0x01, 0x4, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00,
    /// #     0x0a, 0x05, 0x01, 0x03, 0x00, 0x01, 0x0b];
    /// use wasmparser::WasmDecoder;
    /// let mut parser = wasmparser::Parser::new(data);
    /// {
    ///     let state = parser.read();
    ///     println!("First state {:?}", state);
    /// }
    /// {
    ///     let state = parser.read();
    ///     println!("Second state {:?}", state);
    /// }
    /// ```
    fn read(&mut self) -> &ParserState<'a> {
        let result = self.read_wrapped();
        if result.is_err() {
            self.state = ParserState::Error(result.err().unwrap());
        }
        &self.state
    }

    fn push_input(&mut self, input: ParserInput) {
        match input {
            ParserInput::Default => (),
            ParserInput::SkipSection => self.skip_section(),
            ParserInput::SkipFunctionBody => self.skip_function_body(),
            ParserInput::ReadCustomSection => self.read_custom_section(),
            ParserInput::ReadSectionRawData => self.read_raw_section_data(),
        }
    }

    /// Creates a BinaryReader when current state is ParserState::BeginSection
    /// or ParserState::BeginFunctionBody.
    ///
    /// # Examples
    /// ```
    /// # let data = &[0x0, 0x61, 0x73, 0x6d, 0x1, 0x0, 0x0, 0x0, 0x1, 0x84,
    /// #              0x80, 0x80, 0x80, 0x0, 0x1, 0x60, 0x0, 0x0, 0x3, 0x83,
    /// #              0x80, 0x80, 0x80, 0x0, 0x2, 0x0, 0x0, 0x6, 0x81, 0x80,
    /// #              0x80, 0x80, 0x0, 0x0, 0xa, 0x91, 0x80, 0x80, 0x80, 0x0,
    /// #              0x2, 0x83, 0x80, 0x80, 0x80, 0x0, 0x0, 0x1, 0xb, 0x83,
    /// #              0x80, 0x80, 0x80, 0x0, 0x0, 0x0, 0xb];
    /// use wasmparser::{WasmDecoder, Parser, ParserState};
    /// let mut parser = Parser::new(data);
    /// let mut function_readers = Vec::new();
    /// loop {
    ///     match *parser.read() {
    ///         ParserState::Error(_) |
    ///         ParserState::EndWasm => break,
    ///         ParserState::BeginFunctionBody {..} => {
    ///             let reader = parser.create_binary_reader();
    ///             function_readers.push(reader);
    ///         }
    ///         _ => continue
    ///     }
    /// }
    /// for (i, reader) in function_readers.iter_mut().enumerate() {
    ///     println!("Function {}", i);
    ///     while let Ok(ref op) = reader.read_operator() {
    ///       println!("  {:?}", op);
    ///     }
    /// }
    /// ```
    fn create_binary_reader<'b>(&mut self) -> BinaryReader<'b>
        where 'a: 'b
    {
        let range;
        match self.state {
            ParserState::BeginSection { .. } => {
                range = self.section_range.unwrap();
                self.skip_section();
            }
            ParserState::BeginFunctionBody { .. } |
            ParserState::FunctionBodyLocals { .. } => {
                range = self.function_range.unwrap();
                self.skip_function_body();
            }
            _ => panic!("Invalid reader state during get binary reader operation"),
        };
        BinaryReader::new(range.slice(self.reader.buffer))
    }

    /// Reads next record from the WebAssembly binary data. It also allows to
    /// control how parser will treat the next record(s). The method accepts the
    /// `ParserInput` parameter that allows e.g. to skip section or function
    /// operators. The methods returns reference to current state of the parser.
    ///
    /// # Examples
    /// ```
    /// # let data: &[u8] = &[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    /// #     0x01, 0x4, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00,
    /// #     0x0a, 0x05, 0x01, 0x03, 0x00, 0x01, 0x0b];
    /// use wasmparser::WasmDecoder;
    /// let mut parser = wasmparser::Parser::new(data);
    /// let mut next_input = wasmparser::ParserInput::Default;
    /// loop {
    ///     let state = parser.read_with_input(next_input);
    ///     match *state {
    ///         wasmparser::ParserState::EndWasm => break,
    ///         wasmparser::ParserState::BeginWasm { .. } |
    ///         wasmparser::ParserState::EndSection =>
    ///             next_input = wasmparser::ParserInput::Default,
    ///         wasmparser::ParserState::BeginSection { ref code, .. } => {
    ///             println!("Found section: {:?}", code);
    ///             next_input = wasmparser::ParserInput::SkipSection;
    ///         },
    ///         _ => unreachable!()
    ///     }
    /// }
    /// ```
    fn read_with_input(&mut self, input: ParserInput) -> &ParserState<'a> {
        self.push_input(input);
        self.read()
    }

    fn last_state(&self) -> &ParserState<'a> {
        &self.state
    }
}
