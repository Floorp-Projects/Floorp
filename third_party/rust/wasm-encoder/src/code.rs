use super::*;

/// An encoder for the code section.
///
/// # Example
///
/// ```
/// use wasm_encoder::{
///     CodeSection, Function, FunctionSection, Instruction, Module,
///     TypeSection, ValType
/// };
///
/// let mut types = TypeSection::new();
/// types.function(vec![], vec![ValType::I32]);
///
/// let mut functions = FunctionSection::new();
/// let type_index = 0;
/// functions.function(type_index);
///
/// let locals = vec![];
/// let mut func = Function::new(locals);
/// func.instruction(Instruction::I32Const(42));
/// let mut code = CodeSection::new();
/// code.function(&func);
///
/// let mut module = Module::new();
/// module
///     .section(&types)
///     .section(&functions)
///     .section(&code);
///
/// let wasm_bytes = module.finish();
/// ```
#[derive(Clone, Debug)]
pub struct CodeSection {
    bytes: Vec<u8>,
    num_added: u32,
}

impl CodeSection {
    /// Create a new code section encoder.
    pub fn new() -> CodeSection {
        CodeSection {
            bytes: vec![],
            num_added: 0,
        }
    }

    /// How many function bodies have been defined inside this section so far?
    pub fn len(&self) -> u32 {
        self.num_added
    }

    /// Write a function body into this code section.
    pub fn function(&mut self, func: &Function) -> &mut Self {
        func.encode(&mut self.bytes);
        self.num_added += 1;
        self
    }
}

impl Section for CodeSection {
    fn id(&self) -> u8 {
        SectionId::Code.into()
    }

    fn encode<S>(&self, sink: &mut S)
    where
        S: Extend<u8>,
    {
        let num_added = encoders::u32(self.num_added);
        let n = num_added.len();
        sink.extend(
            encoders::u32(u32::try_from(n + self.bytes.len()).unwrap())
                .chain(num_added)
                .chain(self.bytes.iter().copied()),
        );
    }
}

/// An encoder for a function body within the code section.
///
/// # Example
///
/// ```
/// use wasm_encoder::{CodeSection, Function, Instruction};
///
/// // Define the function body for:
/// //
/// //     (func (param i32 i32) (result i32)
/// //       local.get 0
/// //       local.get 1
/// //       i32.add)
/// let locals = vec![];
/// let mut func = Function::new(locals);
/// func.instruction(Instruction::LocalGet(0));
/// func.instruction(Instruction::LocalGet(1));
/// func.instruction(Instruction::I32Add);
///
/// // Add our function to the code section.
/// let mut code = CodeSection::new();
/// code.function(&func);
/// ```
#[derive(Clone, Debug)]
pub struct Function {
    bytes: Vec<u8>,
}

impl Function {
    /// Create a new function body with the given locals.
    pub fn new<L>(locals: L) -> Self
    where
        L: IntoIterator<Item = (u32, ValType)>,
        L::IntoIter: ExactSizeIterator,
    {
        let locals = locals.into_iter();
        let mut bytes = vec![];
        bytes.extend(encoders::u32(u32::try_from(locals.len()).unwrap()));
        for (count, ty) in locals {
            bytes.extend(encoders::u32(count));
            bytes.push(ty.into());
        }
        Function { bytes }
    }

    /// Write an instruction into this function body.
    pub fn instruction(&mut self, instruction: Instruction) -> &mut Self {
        instruction.encode(&mut self.bytes);
        self
    }

    /// Add raw bytes to this function's body.
    pub fn raw<B>(&mut self, bytes: B) -> &mut Self
    where
        B: IntoIterator<Item = u8>,
    {
        self.bytes.extend(bytes);
        self
    }

    fn encode(&self, bytes: &mut Vec<u8>) {
        bytes.extend(
            encoders::u32(u32::try_from(self.bytes.len()).unwrap())
                .chain(self.bytes.iter().copied()),
        );
    }
}

/// The immediate for a memory instruction.
#[derive(Clone, Copy, Debug)]
pub struct MemArg {
    /// A static offset to add to the instruction's dynamic address operand.
    ///
    /// This is a `u64` field for the memory64 proposal, but 32-bit memories
    /// limit offsets to at most `u32::MAX` bytes. This will be encoded as a LEB
    /// but it won't generate a valid module if an offset is specified which is
    /// larger than the maximum size of the index space for the memory indicated
    /// by `memory_index`.
    pub offset: u64,
    /// The expected alignment of the instruction's dynamic address operand
    /// (expressed the exponent of a power of two).
    pub align: u32,
    /// The index of the memory this instruction is operating upon.
    pub memory_index: u32,
}

impl MemArg {
    fn encode(&self, bytes: &mut Vec<u8>) {
        if self.memory_index == 0 {
            bytes.extend(encoders::u32(self.align));
            bytes.extend(encoders::u64(self.offset));
        } else {
            bytes.extend(encoders::u32(self.align | (1 << 6)));
            bytes.extend(encoders::u64(self.offset));
            bytes.extend(encoders::u32(self.memory_index));
        }
    }
}

/// Describe an unchecked SIMD lane index.
pub type Lane = u8;

/// The type for a `block`/`if`/`loop`.
#[derive(Clone, Copy, Debug)]
pub enum BlockType {
    /// `[] -> []`
    Empty,
    /// `[] -> [t]`
    Result(ValType),
    /// The `n`th function type.
    FunctionType(u32),
}

impl BlockType {
    fn encode(&self, bytes: &mut Vec<u8>) {
        match *self {
            BlockType::Empty => bytes.push(0x40),
            BlockType::Result(ty) => bytes.push(ty.into()),
            BlockType::FunctionType(f) => bytes.extend(encoders::s33(f.into())),
        }
    }
}

/// WebAssembly instructions.
#[derive(Clone, Copy, Debug)]
#[non_exhaustive]
#[allow(missing_docs, non_camel_case_types)]
pub enum Instruction<'a> {
    // Control instructions.
    Unreachable,
    Nop,
    Block(BlockType),
    Loop(BlockType),
    If(BlockType),
    Else,
    End,
    Br(u32),
    BrIf(u32),
    BrTable(&'a [u32], u32),
    Return,
    Call(u32),
    CallIndirect { ty: u32, table: u32 },

    // Parametric instructions.
    Drop,
    Select,

    // Variable instructions.
    LocalGet(u32),
    LocalSet(u32),
    LocalTee(u32),
    GlobalGet(u32),
    GlobalSet(u32),

    // Memory instructions.
    I32Load(MemArg),
    I64Load(MemArg),
    F32Load(MemArg),
    F64Load(MemArg),
    I32Load8_S(MemArg),
    I32Load8_U(MemArg),
    I32Load16_S(MemArg),
    I32Load16_U(MemArg),
    I64Load8_S(MemArg),
    I64Load8_U(MemArg),
    I64Load16_S(MemArg),
    I64Load16_U(MemArg),
    I64Load32_S(MemArg),
    I64Load32_U(MemArg),
    I32Store(MemArg),
    I64Store(MemArg),
    F32Store(MemArg),
    F64Store(MemArg),
    I32Store8(MemArg),
    I32Store16(MemArg),
    I64Store8(MemArg),
    I64Store16(MemArg),
    I64Store32(MemArg),
    MemorySize(u32),
    MemoryGrow(u32),
    MemoryInit { mem: u32, data: u32 },
    DataDrop(u32),
    MemoryCopy { src: u32, dst: u32 },
    MemoryFill(u32),

    // Numeric instructions.
    I32Const(i32),
    I64Const(i64),
    F32Const(f32),
    F64Const(f64),
    I32Eqz,
    I32Eq,
    I32Neq,
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
    I64Neq,
    I64LtS,
    I64LtU,
    I64GtS,
    I64GtU,
    I64LeS,
    I64LeU,
    I64GeS,
    I64GeU,
    F32Eq,
    F32Neq,
    F32Lt,
    F32Gt,
    F32Le,
    F32Ge,
    F64Eq,
    F64Neq,
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
    I32TruncSatF32S,
    I32TruncSatF32U,
    I32TruncSatF64S,
    I32TruncSatF64U,
    I64TruncSatF32S,
    I64TruncSatF32U,
    I64TruncSatF64S,
    I64TruncSatF64U,

    // Reference types instructions.
    TypedSelect(ValType),
    RefNull(ValType),
    RefIsNull,
    RefFunc(u32),

    // Bulk memory instructions.
    TableInit { segment: u32, table: u32 },
    ElemDrop { segment: u32 },
    TableFill { table: u32 },
    TableSet { table: u32 },
    TableGet { table: u32 },
    TableGrow { table: u32 },
    TableSize { table: u32 },
    TableCopy { src: u32, dst: u32 },

    // SIMD instructions.
    V128Load { memarg: MemArg },
    V128Load8x8S { memarg: MemArg },
    V128Load8x8U { memarg: MemArg },
    V128Load16x4S { memarg: MemArg },
    V128Load16x4U { memarg: MemArg },
    V128Load32x2S { memarg: MemArg },
    V128Load32x2U { memarg: MemArg },
    V128Load8Splat { memarg: MemArg },
    V128Load16Splat { memarg: MemArg },
    V128Load32Splat { memarg: MemArg },
    V128Load64Splat { memarg: MemArg },
    V128Load32Zero { memarg: MemArg },
    V128Load64Zero { memarg: MemArg },
    V128Store { memarg: MemArg },
    V128Load8Lane { memarg: MemArg, lane: Lane },
    V128Load16Lane { memarg: MemArg, lane: Lane },
    V128Load32Lane { memarg: MemArg, lane: Lane },
    V128Load64Lane { memarg: MemArg, lane: Lane },
    V128Store8Lane { memarg: MemArg, lane: Lane },
    V128Store16Lane { memarg: MemArg, lane: Lane },
    V128Store32Lane { memarg: MemArg, lane: Lane },
    V128Store64Lane { memarg: MemArg, lane: Lane },
    V128Const(i128),
    I8x16Shuffle { lanes: [Lane; 16] },
    I8x16ExtractLaneS { lane: Lane },
    I8x16ExtractLaneU { lane: Lane },
    I8x16ReplaceLane { lane: Lane },
    I16x8ExtractLaneS { lane: Lane },
    I16x8ExtractLaneU { lane: Lane },
    I16x8ReplaceLane { lane: Lane },
    I32x4ExtractLane { lane: Lane },
    I32x4ReplaceLane { lane: Lane },
    I64x2ExtractLane { lane: Lane },
    I64x2ReplaceLane { lane: Lane },
    F32x4ExtractLane { lane: Lane },
    F32x4ReplaceLane { lane: Lane },
    F64x2ExtractLane { lane: Lane },
    F64x2ReplaceLane { lane: Lane },
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
}

impl Instruction<'_> {
    pub(crate) fn encode(&self, bytes: &mut Vec<u8>) {
        match *self {
            // Control instructions.
            Instruction::Unreachable => bytes.push(0x00),
            Instruction::Nop => bytes.push(0x01),
            Instruction::Block(bt) => {
                bytes.push(0x02);
                bt.encode(bytes);
            }
            Instruction::Loop(bt) => {
                bytes.push(0x03);
                bt.encode(bytes);
            }
            Instruction::If(bt) => {
                bytes.push(0x04);
                bt.encode(bytes);
            }
            Instruction::Else => bytes.push(0x05),
            Instruction::End => bytes.push(0x0B),
            Instruction::Br(l) => {
                bytes.push(0x0C);
                bytes.extend(encoders::u32(l));
            }
            Instruction::BrIf(l) => {
                bytes.push(0x0D);
                bytes.extend(encoders::u32(l));
            }
            Instruction::BrTable(ls, l) => {
                bytes.push(0x0E);
                bytes.extend(encoders::u32(u32::try_from(ls.len()).unwrap()));
                for l in ls {
                    bytes.extend(encoders::u32(*l));
                }
                bytes.extend(encoders::u32(l));
            }
            Instruction::Return => bytes.push(0x0F),
            Instruction::Call(f) => {
                bytes.push(0x10);
                bytes.extend(encoders::u32(f));
            }
            Instruction::CallIndirect { ty, table } => {
                bytes.push(0x11);
                bytes.extend(encoders::u32(ty));
                bytes.extend(encoders::u32(table));
            }

            // Parametric instructions.
            Instruction::Drop => bytes.push(0x1A),
            Instruction::Select => bytes.push(0x1B),
            Instruction::TypedSelect(ty) => {
                bytes.push(0x1c);
                bytes.extend(encoders::u32(1));
                bytes.push(ty.into());
            }

            // Variable instructions.
            Instruction::LocalGet(l) => {
                bytes.push(0x20);
                bytes.extend(encoders::u32(l));
            }
            Instruction::LocalSet(l) => {
                bytes.push(0x21);
                bytes.extend(encoders::u32(l));
            }
            Instruction::LocalTee(l) => {
                bytes.push(0x22);
                bytes.extend(encoders::u32(l));
            }
            Instruction::GlobalGet(g) => {
                bytes.push(0x23);
                bytes.extend(encoders::u32(g));
            }
            Instruction::GlobalSet(g) => {
                bytes.push(0x24);
                bytes.extend(encoders::u32(g));
            }
            Instruction::TableGet { table } => {
                bytes.push(0x25);
                bytes.extend(encoders::u32(table));
            }
            Instruction::TableSet { table } => {
                bytes.push(0x26);
                bytes.extend(encoders::u32(table));
            }

            // Memory instructions.
            Instruction::I32Load(m) => {
                bytes.push(0x28);
                m.encode(bytes);
            }
            Instruction::I64Load(m) => {
                bytes.push(0x29);
                m.encode(bytes);
            }
            Instruction::F32Load(m) => {
                bytes.push(0x2A);
                m.encode(bytes);
            }
            Instruction::F64Load(m) => {
                bytes.push(0x2B);
                m.encode(bytes);
            }
            Instruction::I32Load8_S(m) => {
                bytes.push(0x2C);
                m.encode(bytes);
            }
            Instruction::I32Load8_U(m) => {
                bytes.push(0x2D);
                m.encode(bytes);
            }
            Instruction::I32Load16_S(m) => {
                bytes.push(0x2E);
                m.encode(bytes);
            }
            Instruction::I32Load16_U(m) => {
                bytes.push(0x2F);
                m.encode(bytes);
            }
            Instruction::I64Load8_S(m) => {
                bytes.push(0x30);
                m.encode(bytes);
            }
            Instruction::I64Load8_U(m) => {
                bytes.push(0x31);
                m.encode(bytes);
            }
            Instruction::I64Load16_S(m) => {
                bytes.push(0x32);
                m.encode(bytes);
            }
            Instruction::I64Load16_U(m) => {
                bytes.push(0x33);
                m.encode(bytes);
            }
            Instruction::I64Load32_S(m) => {
                bytes.push(0x34);
                m.encode(bytes);
            }
            Instruction::I64Load32_U(m) => {
                bytes.push(0x35);
                m.encode(bytes);
            }
            Instruction::I32Store(m) => {
                bytes.push(0x36);
                m.encode(bytes);
            }
            Instruction::I64Store(m) => {
                bytes.push(0x37);
                m.encode(bytes);
            }
            Instruction::F32Store(m) => {
                bytes.push(0x38);
                m.encode(bytes);
            }
            Instruction::F64Store(m) => {
                bytes.push(0x39);
                m.encode(bytes);
            }
            Instruction::I32Store8(m) => {
                bytes.push(0x3A);
                m.encode(bytes);
            }
            Instruction::I32Store16(m) => {
                bytes.push(0x3B);
                m.encode(bytes);
            }
            Instruction::I64Store8(m) => {
                bytes.push(0x3C);
                m.encode(bytes);
            }
            Instruction::I64Store16(m) => {
                bytes.push(0x3D);
                m.encode(bytes);
            }
            Instruction::I64Store32(m) => {
                bytes.push(0x3E);
                m.encode(bytes);
            }
            Instruction::MemorySize(i) => {
                bytes.push(0x3F);
                bytes.extend(encoders::u32(i));
            }
            Instruction::MemoryGrow(i) => {
                bytes.push(0x40);
                bytes.extend(encoders::u32(i));
            }
            Instruction::MemoryInit { mem, data } => {
                bytes.push(0xfc);
                bytes.extend(encoders::u32(8));
                bytes.extend(encoders::u32(data));
                bytes.extend(encoders::u32(mem));
            }
            Instruction::DataDrop(data) => {
                bytes.push(0xfc);
                bytes.extend(encoders::u32(9));
                bytes.extend(encoders::u32(data));
            }
            Instruction::MemoryCopy { src, dst } => {
                bytes.push(0xfc);
                bytes.extend(encoders::u32(10));
                bytes.extend(encoders::u32(dst));
                bytes.extend(encoders::u32(src));
            }
            Instruction::MemoryFill(mem) => {
                bytes.push(0xfc);
                bytes.extend(encoders::u32(11));
                bytes.extend(encoders::u32(mem));
            }

            // Numeric instructions.
            Instruction::I32Const(x) => {
                bytes.push(0x41);
                bytes.extend(encoders::s32(x));
            }
            Instruction::I64Const(x) => {
                bytes.push(0x42);
                bytes.extend(encoders::s64(x));
            }
            Instruction::F32Const(x) => {
                bytes.push(0x43);
                let x = x.to_bits();
                bytes.extend(x.to_le_bytes().iter().copied());
            }
            Instruction::F64Const(x) => {
                bytes.push(0x44);
                let x = x.to_bits();
                bytes.extend(x.to_le_bytes().iter().copied());
            }
            Instruction::I32Eqz => bytes.push(0x45),
            Instruction::I32Eq => bytes.push(0x46),
            Instruction::I32Neq => bytes.push(0x47),
            Instruction::I32LtS => bytes.push(0x48),
            Instruction::I32LtU => bytes.push(0x49),
            Instruction::I32GtS => bytes.push(0x4A),
            Instruction::I32GtU => bytes.push(0x4B),
            Instruction::I32LeS => bytes.push(0x4C),
            Instruction::I32LeU => bytes.push(0x4D),
            Instruction::I32GeS => bytes.push(0x4E),
            Instruction::I32GeU => bytes.push(0x4F),
            Instruction::I64Eqz => bytes.push(0x50),
            Instruction::I64Eq => bytes.push(0x51),
            Instruction::I64Neq => bytes.push(0x52),
            Instruction::I64LtS => bytes.push(0x53),
            Instruction::I64LtU => bytes.push(0x54),
            Instruction::I64GtS => bytes.push(0x55),
            Instruction::I64GtU => bytes.push(0x56),
            Instruction::I64LeS => bytes.push(0x57),
            Instruction::I64LeU => bytes.push(0x58),
            Instruction::I64GeS => bytes.push(0x59),
            Instruction::I64GeU => bytes.push(0x5A),
            Instruction::F32Eq => bytes.push(0x5B),
            Instruction::F32Neq => bytes.push(0x5C),
            Instruction::F32Lt => bytes.push(0x5D),
            Instruction::F32Gt => bytes.push(0x5E),
            Instruction::F32Le => bytes.push(0x5F),
            Instruction::F32Ge => bytes.push(0x60),
            Instruction::F64Eq => bytes.push(0x61),
            Instruction::F64Neq => bytes.push(0x62),
            Instruction::F64Lt => bytes.push(0x63),
            Instruction::F64Gt => bytes.push(0x64),
            Instruction::F64Le => bytes.push(0x65),
            Instruction::F64Ge => bytes.push(0x66),
            Instruction::I32Clz => bytes.push(0x67),
            Instruction::I32Ctz => bytes.push(0x68),
            Instruction::I32Popcnt => bytes.push(0x69),
            Instruction::I32Add => bytes.push(0x6A),
            Instruction::I32Sub => bytes.push(0x6B),
            Instruction::I32Mul => bytes.push(0x6C),
            Instruction::I32DivS => bytes.push(0x6D),
            Instruction::I32DivU => bytes.push(0x6E),
            Instruction::I32RemS => bytes.push(0x6F),
            Instruction::I32RemU => bytes.push(0x70),
            Instruction::I32And => bytes.push(0x71),
            Instruction::I32Or => bytes.push(0x72),
            Instruction::I32Xor => bytes.push(0x73),
            Instruction::I32Shl => bytes.push(0x74),
            Instruction::I32ShrS => bytes.push(0x75),
            Instruction::I32ShrU => bytes.push(0x76),
            Instruction::I32Rotl => bytes.push(0x77),
            Instruction::I32Rotr => bytes.push(0x78),
            Instruction::I64Clz => bytes.push(0x79),
            Instruction::I64Ctz => bytes.push(0x7A),
            Instruction::I64Popcnt => bytes.push(0x7B),
            Instruction::I64Add => bytes.push(0x7C),
            Instruction::I64Sub => bytes.push(0x7D),
            Instruction::I64Mul => bytes.push(0x7E),
            Instruction::I64DivS => bytes.push(0x7F),
            Instruction::I64DivU => bytes.push(0x80),
            Instruction::I64RemS => bytes.push(0x81),
            Instruction::I64RemU => bytes.push(0x82),
            Instruction::I64And => bytes.push(0x83),
            Instruction::I64Or => bytes.push(0x84),
            Instruction::I64Xor => bytes.push(0x85),
            Instruction::I64Shl => bytes.push(0x86),
            Instruction::I64ShrS => bytes.push(0x87),
            Instruction::I64ShrU => bytes.push(0x88),
            Instruction::I64Rotl => bytes.push(0x89),
            Instruction::I64Rotr => bytes.push(0x8A),
            Instruction::F32Abs => bytes.push(0x8B),
            Instruction::F32Neg => bytes.push(0x8C),
            Instruction::F32Ceil => bytes.push(0x8D),
            Instruction::F32Floor => bytes.push(0x8E),
            Instruction::F32Trunc => bytes.push(0x8F),
            Instruction::F32Nearest => bytes.push(0x90),
            Instruction::F32Sqrt => bytes.push(0x91),
            Instruction::F32Add => bytes.push(0x92),
            Instruction::F32Sub => bytes.push(0x93),
            Instruction::F32Mul => bytes.push(0x94),
            Instruction::F32Div => bytes.push(0x95),
            Instruction::F32Min => bytes.push(0x96),
            Instruction::F32Max => bytes.push(0x97),
            Instruction::F32Copysign => bytes.push(0x98),
            Instruction::F64Abs => bytes.push(0x99),
            Instruction::F64Neg => bytes.push(0x9A),
            Instruction::F64Ceil => bytes.push(0x9B),
            Instruction::F64Floor => bytes.push(0x9C),
            Instruction::F64Trunc => bytes.push(0x9D),
            Instruction::F64Nearest => bytes.push(0x9E),
            Instruction::F64Sqrt => bytes.push(0x9F),
            Instruction::F64Add => bytes.push(0xA0),
            Instruction::F64Sub => bytes.push(0xA1),
            Instruction::F64Mul => bytes.push(0xA2),
            Instruction::F64Div => bytes.push(0xA3),
            Instruction::F64Min => bytes.push(0xA4),
            Instruction::F64Max => bytes.push(0xA5),
            Instruction::F64Copysign => bytes.push(0xA6),
            Instruction::I32WrapI64 => bytes.push(0xA7),
            Instruction::I32TruncF32S => bytes.push(0xA8),
            Instruction::I32TruncF32U => bytes.push(0xA9),
            Instruction::I32TruncF64S => bytes.push(0xAA),
            Instruction::I32TruncF64U => bytes.push(0xAB),
            Instruction::I64ExtendI32S => bytes.push(0xAC),
            Instruction::I64ExtendI32U => bytes.push(0xAD),
            Instruction::I64TruncF32S => bytes.push(0xAE),
            Instruction::I64TruncF32U => bytes.push(0xAF),
            Instruction::I64TruncF64S => bytes.push(0xB0),
            Instruction::I64TruncF64U => bytes.push(0xB1),
            Instruction::F32ConvertI32S => bytes.push(0xB2),
            Instruction::F32ConvertI32U => bytes.push(0xB3),
            Instruction::F32ConvertI64S => bytes.push(0xB4),
            Instruction::F32ConvertI64U => bytes.push(0xB5),
            Instruction::F32DemoteF64 => bytes.push(0xB6),
            Instruction::F64ConvertI32S => bytes.push(0xB7),
            Instruction::F64ConvertI32U => bytes.push(0xB8),
            Instruction::F64ConvertI64S => bytes.push(0xB9),
            Instruction::F64ConvertI64U => bytes.push(0xBA),
            Instruction::F64PromoteF32 => bytes.push(0xBB),
            Instruction::I32ReinterpretF32 => bytes.push(0xBC),
            Instruction::I64ReinterpretF64 => bytes.push(0xBD),
            Instruction::F32ReinterpretI32 => bytes.push(0xBE),
            Instruction::F64ReinterpretI64 => bytes.push(0xBF),
            Instruction::I32Extend8S => bytes.push(0xC0),
            Instruction::I32Extend16S => bytes.push(0xC1),
            Instruction::I64Extend8S => bytes.push(0xC2),
            Instruction::I64Extend16S => bytes.push(0xC3),
            Instruction::I64Extend32S => bytes.push(0xC4),

            Instruction::I32TruncSatF32S => {
                bytes.push(0xFC);
                bytes.extend(encoders::u32(0));
            }
            Instruction::I32TruncSatF32U => {
                bytes.push(0xFC);
                bytes.extend(encoders::u32(1));
            }
            Instruction::I32TruncSatF64S => {
                bytes.push(0xFC);
                bytes.extend(encoders::u32(2));
            }
            Instruction::I32TruncSatF64U => {
                bytes.push(0xFC);
                bytes.extend(encoders::u32(3));
            }
            Instruction::I64TruncSatF32S => {
                bytes.push(0xFC);
                bytes.extend(encoders::u32(4));
            }
            Instruction::I64TruncSatF32U => {
                bytes.push(0xFC);
                bytes.extend(encoders::u32(5));
            }
            Instruction::I64TruncSatF64S => {
                bytes.push(0xFC);
                bytes.extend(encoders::u32(6));
            }
            Instruction::I64TruncSatF64U => {
                bytes.push(0xFC);
                bytes.extend(encoders::u32(7));
            }

            // Reference types instructions.
            Instruction::RefNull(ty) => {
                bytes.push(0xd0);
                bytes.push(ty.into());
            }
            Instruction::RefIsNull => bytes.push(0xd1),
            Instruction::RefFunc(f) => {
                bytes.push(0xd2);
                bytes.extend(encoders::u32(f));
            }

            // Bulk memory instructions.
            Instruction::TableInit { segment, table } => {
                bytes.push(0xfc);
                bytes.extend(encoders::u32(0x0c));
                bytes.extend(encoders::u32(segment));
                bytes.extend(encoders::u32(table));
            }
            Instruction::ElemDrop { segment } => {
                bytes.push(0xfc);
                bytes.extend(encoders::u32(0x0d));
                bytes.extend(encoders::u32(segment));
            }
            Instruction::TableCopy { src, dst } => {
                bytes.push(0xfc);
                bytes.extend(encoders::u32(0x0e));
                bytes.extend(encoders::u32(dst));
                bytes.extend(encoders::u32(src));
            }
            Instruction::TableGrow { table } => {
                bytes.push(0xfc);
                bytes.extend(encoders::u32(0x0f));
                bytes.extend(encoders::u32(table));
            }
            Instruction::TableSize { table } => {
                bytes.push(0xfc);
                bytes.extend(encoders::u32(0x10));
                bytes.extend(encoders::u32(table));
            }
            Instruction::TableFill { table } => {
                bytes.push(0xfc);
                bytes.extend(encoders::u32(0x11));
                bytes.extend(encoders::u32(table));
            }

            // SIMD instructions.
            Instruction::V128Load { memarg } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x00));
                memarg.encode(bytes);
            }
            Instruction::V128Load8x8S { memarg } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x01));
                memarg.encode(bytes);
            }
            Instruction::V128Load8x8U { memarg } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x02));
                memarg.encode(bytes);
            }
            Instruction::V128Load16x4S { memarg } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x03));
                memarg.encode(bytes);
            }
            Instruction::V128Load16x4U { memarg } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x04));
                memarg.encode(bytes);
            }
            Instruction::V128Load32x2S { memarg } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x05));
                memarg.encode(bytes);
            }
            Instruction::V128Load32x2U { memarg } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x06));
                memarg.encode(bytes);
            }
            Instruction::V128Load8Splat { memarg } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x07));
                memarg.encode(bytes);
            }
            Instruction::V128Load16Splat { memarg } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x08));
                memarg.encode(bytes);
            }
            Instruction::V128Load32Splat { memarg } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x09));
                memarg.encode(bytes);
            }
            Instruction::V128Load64Splat { memarg } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x0A));
                memarg.encode(bytes);
            }
            Instruction::V128Store { memarg } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x0B));
                memarg.encode(bytes);
            }
            Instruction::V128Const(x) => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x0C));
                bytes.extend(x.to_le_bytes().iter().copied());
            }
            Instruction::I8x16Shuffle { lanes } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x0D));
                assert!(lanes.iter().all(|l: &u8| *l < 32));
                bytes.extend(lanes.iter().copied());
            }
            Instruction::I8x16Swizzle => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x0E));
            }
            Instruction::I8x16Splat => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x0F));
            }
            Instruction::I16x8Splat => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x10));
            }
            Instruction::I32x4Splat => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x11));
            }
            Instruction::I64x2Splat => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x12));
            }
            Instruction::F32x4Splat => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x13));
            }
            Instruction::F64x2Splat => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x14));
            }
            Instruction::I8x16ExtractLaneS { lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x15));
                assert!(lane < 16);
                bytes.push(lane);
            }
            Instruction::I8x16ExtractLaneU { lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x16));
                assert!(lane < 16);
                bytes.push(lane);
            }
            Instruction::I8x16ReplaceLane { lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x17));
                assert!(lane < 16);
                bytes.push(lane);
            }
            Instruction::I16x8ExtractLaneS { lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x18));
                assert!(lane < 8);
                bytes.push(lane);
            }
            Instruction::I16x8ExtractLaneU { lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x19));
                assert!(lane < 8);
                bytes.push(lane);
            }
            Instruction::I16x8ReplaceLane { lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x1A));
                assert!(lane < 8);
                bytes.push(lane);
            }
            Instruction::I32x4ExtractLane { lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x1B));
                assert!(lane < 4);
                bytes.push(lane);
            }
            Instruction::I32x4ReplaceLane { lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x1C));
                assert!(lane < 4);
                bytes.push(lane);
            }
            Instruction::I64x2ExtractLane { lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x1D));
                assert!(lane < 2);
                bytes.push(lane);
            }
            Instruction::I64x2ReplaceLane { lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x1E));
                assert!(lane < 2);
                bytes.push(lane);
            }
            Instruction::F32x4ExtractLane { lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x1F));
                assert!(lane < 4);
                bytes.push(lane);
            }
            Instruction::F32x4ReplaceLane { lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x20));
                assert!(lane < 4);
                bytes.push(lane);
            }
            Instruction::F64x2ExtractLane { lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x21));
                assert!(lane < 2);
                bytes.push(lane);
            }
            Instruction::F64x2ReplaceLane { lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x22));
                assert!(lane < 2);
                bytes.push(lane);
            }

            Instruction::I8x16Eq => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x23));
            }
            Instruction::I8x16Ne => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x24));
            }
            Instruction::I8x16LtS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x25));
            }
            Instruction::I8x16LtU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x26));
            }
            Instruction::I8x16GtS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x27));
            }
            Instruction::I8x16GtU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x28));
            }
            Instruction::I8x16LeS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x29));
            }
            Instruction::I8x16LeU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x2A));
            }
            Instruction::I8x16GeS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x2B));
            }
            Instruction::I8x16GeU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x2C));
            }
            Instruction::I16x8Eq => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x2D));
            }
            Instruction::I16x8Ne => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x2E));
            }
            Instruction::I16x8LtS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x2F));
            }
            Instruction::I16x8LtU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x30));
            }
            Instruction::I16x8GtS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x31));
            }
            Instruction::I16x8GtU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x32));
            }
            Instruction::I16x8LeS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x33));
            }
            Instruction::I16x8LeU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x34));
            }
            Instruction::I16x8GeS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x35));
            }
            Instruction::I16x8GeU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x36));
            }
            Instruction::I32x4Eq => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x37));
            }
            Instruction::I32x4Ne => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x38));
            }
            Instruction::I32x4LtS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x39));
            }
            Instruction::I32x4LtU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x3A));
            }
            Instruction::I32x4GtS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x3B));
            }
            Instruction::I32x4GtU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x3C));
            }
            Instruction::I32x4LeS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x3D));
            }
            Instruction::I32x4LeU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x3E));
            }
            Instruction::I32x4GeS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x3F));
            }
            Instruction::I32x4GeU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x40));
            }
            Instruction::F32x4Eq => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x41));
            }
            Instruction::F32x4Ne => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x42));
            }
            Instruction::F32x4Lt => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x43));
            }
            Instruction::F32x4Gt => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x44));
            }
            Instruction::F32x4Le => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x45));
            }
            Instruction::F32x4Ge => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x46));
            }
            Instruction::F64x2Eq => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x47));
            }
            Instruction::F64x2Ne => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x48));
            }
            Instruction::F64x2Lt => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x49));
            }
            Instruction::F64x2Gt => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x4A));
            }
            Instruction::F64x2Le => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x4B));
            }
            Instruction::F64x2Ge => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x4C));
            }
            Instruction::V128Not => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x4D));
            }
            Instruction::V128And => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x4E));
            }
            Instruction::V128AndNot => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x4F));
            }
            Instruction::V128Or => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x50));
            }
            Instruction::V128Xor => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x51));
            }
            Instruction::V128Bitselect => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x52));
            }
            Instruction::V128AnyTrue => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x53));
            }
            Instruction::I8x16Abs => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x60));
            }
            Instruction::I8x16Neg => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x61));
            }
            Instruction::I8x16Popcnt => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x62));
            }
            Instruction::I8x16AllTrue => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x63));
            }
            Instruction::I8x16Bitmask => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x64));
            }
            Instruction::I8x16NarrowI16x8S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x65));
            }
            Instruction::I8x16NarrowI16x8U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x66));
            }
            Instruction::I8x16Shl => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x6b));
            }
            Instruction::I8x16ShrS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x6c));
            }
            Instruction::I8x16ShrU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x6d));
            }
            Instruction::I8x16Add => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x6e));
            }
            Instruction::I8x16AddSatS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x6f));
            }
            Instruction::I8x16AddSatU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x70));
            }
            Instruction::I8x16Sub => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x71));
            }
            Instruction::I8x16SubSatS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x72));
            }
            Instruction::I8x16SubSatU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x73));
            }
            Instruction::I8x16MinS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x76));
            }
            Instruction::I8x16MinU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x77));
            }
            Instruction::I8x16MaxS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x78));
            }
            Instruction::I8x16MaxU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x79));
            }
            Instruction::I8x16RoundingAverageU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x7B));
            }
            Instruction::I16x8ExtAddPairwiseI8x16S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x7C));
            }
            Instruction::I16x8ExtAddPairwiseI8x16U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x7D));
            }
            Instruction::I32x4ExtAddPairwiseI16x8S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x7E));
            }
            Instruction::I32x4ExtAddPairwiseI16x8U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x7F));
            }
            Instruction::I16x8Abs => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x80));
            }
            Instruction::I16x8Neg => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x81));
            }
            Instruction::I16x8Q15MulrSatS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x82));
            }
            Instruction::I16x8AllTrue => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x83));
            }
            Instruction::I16x8Bitmask => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x84));
            }
            Instruction::I16x8NarrowI32x4S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x85));
            }
            Instruction::I16x8NarrowI32x4U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x86));
            }
            Instruction::I16x8ExtendLowI8x16S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x87));
            }
            Instruction::I16x8ExtendHighI8x16S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x88));
            }
            Instruction::I16x8ExtendLowI8x16U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x89));
            }
            Instruction::I16x8ExtendHighI8x16U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x8A));
            }
            Instruction::I16x8Shl => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x8B));
            }
            Instruction::I16x8ShrS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x8C));
            }
            Instruction::I16x8ShrU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x8D));
            }
            Instruction::I16x8Add => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x8E));
            }
            Instruction::I16x8AddSatS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x8F));
            }
            Instruction::I16x8AddSatU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x90));
            }
            Instruction::I16x8Sub => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x91));
            }
            Instruction::I16x8SubSatS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x92));
            }
            Instruction::I16x8SubSatU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x93));
            }
            Instruction::I16x8Mul => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x95));
            }
            Instruction::I16x8MinS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x96));
            }
            Instruction::I16x8MinU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x97));
            }
            Instruction::I16x8MaxS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x98));
            }
            Instruction::I16x8MaxU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x99));
            }
            Instruction::I16x8RoundingAverageU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x9B));
            }
            Instruction::I16x8ExtMulLowI8x16S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x9C));
            }
            Instruction::I16x8ExtMulHighI8x16S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x9D));
            }
            Instruction::I16x8ExtMulLowI8x16U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x9E));
            }
            Instruction::I16x8ExtMulHighI8x16U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x9F));
            }
            Instruction::I32x4Abs => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xA0));
            }
            Instruction::I32x4Neg => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xA1));
            }
            Instruction::I32x4AllTrue => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xA3));
            }
            Instruction::I32x4Bitmask => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xA4));
            }
            Instruction::I32x4ExtendLowI16x8S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xA7));
            }
            Instruction::I32x4ExtendHighI16x8S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xA8));
            }
            Instruction::I32x4ExtendLowI16x8U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xA9));
            }
            Instruction::I32x4ExtendHighI16x8U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xAA));
            }
            Instruction::I32x4Shl => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xAB));
            }
            Instruction::I32x4ShrS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xAC));
            }
            Instruction::I32x4ShrU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xAD));
            }
            Instruction::I32x4Add => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xAE));
            }
            Instruction::I32x4Sub => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xB1));
            }
            Instruction::I32x4Mul => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xB5));
            }
            Instruction::I32x4MinS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xB6));
            }
            Instruction::I32x4MinU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xB7));
            }
            Instruction::I32x4MaxS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xB8));
            }
            Instruction::I32x4MaxU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xB9));
            }
            Instruction::I32x4DotI16x8S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xBA));
            }
            Instruction::I32x4ExtMulLowI16x8S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xBC));
            }
            Instruction::I32x4ExtMulHighI16x8S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xBD));
            }
            Instruction::I32x4ExtMulLowI16x8U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xBE));
            }
            Instruction::I32x4ExtMulHighI16x8U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xBF));
            }
            Instruction::I64x2Abs => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xC0));
            }
            Instruction::I64x2Neg => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xC1));
            }
            Instruction::I64x2AllTrue => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xC3));
            }
            Instruction::I64x2Bitmask => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xC4));
            }
            Instruction::I64x2ExtendLowI32x4S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xC7));
            }
            Instruction::I64x2ExtendHighI32x4S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xC8));
            }
            Instruction::I64x2ExtendLowI32x4U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xC9));
            }
            Instruction::I64x2ExtendHighI32x4U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xCA));
            }
            Instruction::I64x2Shl => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xCB));
            }
            Instruction::I64x2ShrS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xCC));
            }
            Instruction::I64x2ShrU => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xCD));
            }
            Instruction::I64x2Add => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xCE));
            }
            Instruction::I64x2Sub => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xD1));
            }
            Instruction::I64x2Mul => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xD5));
            }
            Instruction::I64x2ExtMulLowI32x4S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xDC));
            }
            Instruction::I64x2ExtMulHighI32x4S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xDD));
            }
            Instruction::I64x2ExtMulLowI32x4U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xDE));
            }
            Instruction::I64x2ExtMulHighI32x4U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xDF));
            }
            Instruction::F32x4Ceil => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x67));
            }
            Instruction::F32x4Floor => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x68));
            }
            Instruction::F32x4Trunc => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x69));
            }
            Instruction::F32x4Nearest => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x6A));
            }
            Instruction::F32x4Abs => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xE0));
            }
            Instruction::F32x4Neg => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xE1));
            }
            Instruction::F32x4Sqrt => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xE3));
            }
            Instruction::F32x4Add => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xE4));
            }
            Instruction::F32x4Sub => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xE5));
            }
            Instruction::F32x4Mul => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xE6));
            }
            Instruction::F32x4Div => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xE7));
            }
            Instruction::F32x4Min => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xE8));
            }
            Instruction::F32x4Max => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xE9));
            }
            Instruction::F32x4PMin => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xEA));
            }
            Instruction::F32x4PMax => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xEB));
            }
            Instruction::F64x2Ceil => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x74));
            }
            Instruction::F64x2Floor => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x75));
            }
            Instruction::F64x2Trunc => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x7A));
            }
            Instruction::F64x2Nearest => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x94));
            }
            Instruction::F64x2Abs => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xEC));
            }
            Instruction::F64x2Neg => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xED));
            }
            Instruction::F64x2Sqrt => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xEF));
            }
            Instruction::F64x2Add => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xF0));
            }
            Instruction::F64x2Sub => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xF1));
            }
            Instruction::F64x2Mul => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xF2));
            }
            Instruction::F64x2Div => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xF3));
            }
            Instruction::F64x2Min => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xF4));
            }
            Instruction::F64x2Max => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xF5));
            }
            Instruction::F64x2PMin => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xF6));
            }
            Instruction::F64x2PMax => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xF7));
            }
            Instruction::I32x4TruncSatF32x4S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xF8));
            }
            Instruction::I32x4TruncSatF32x4U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xF9));
            }
            Instruction::F32x4ConvertI32x4S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xFA));
            }
            Instruction::F32x4ConvertI32x4U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xFB));
            }
            Instruction::I32x4TruncSatF64x2SZero => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xFC));
            }
            Instruction::I32x4TruncSatF64x2UZero => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xFD));
            }
            Instruction::F64x2ConvertLowI32x4S => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xFE));
            }
            Instruction::F64x2ConvertLowI32x4U => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xFF));
            }
            Instruction::F32x4DemoteF64x2Zero => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x5E));
            }
            Instruction::F64x2PromoteLowF32x4 => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x5F));
            }
            Instruction::V128Load32Zero { memarg } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x5C));
                memarg.encode(bytes);
            }
            Instruction::V128Load64Zero { memarg } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x5D));
                memarg.encode(bytes);
            }
            Instruction::V128Load8Lane { memarg, lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x54));
                memarg.encode(bytes);
                assert!(lane < 16);
                bytes.push(lane);
            }
            Instruction::V128Load16Lane { memarg, lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x55));
                memarg.encode(bytes);
                assert!(lane < 8);
                bytes.push(lane);
            }
            Instruction::V128Load32Lane { memarg, lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x56));
                memarg.encode(bytes);
                assert!(lane < 4);
                bytes.push(lane);
            }
            Instruction::V128Load64Lane { memarg, lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x57));
                memarg.encode(bytes);
                assert!(lane < 2);
                bytes.push(lane);
            }
            Instruction::V128Store8Lane { memarg, lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x58));
                memarg.encode(bytes);
                assert!(lane < 16);
                bytes.push(lane);
            }
            Instruction::V128Store16Lane { memarg, lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x59));
                memarg.encode(bytes);
                assert!(lane < 8);
                bytes.push(lane);
            }
            Instruction::V128Store32Lane { memarg, lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x5A));
                memarg.encode(bytes);
                assert!(lane < 4);
                bytes.push(lane);
            }
            Instruction::V128Store64Lane { memarg, lane } => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0x5B));
                memarg.encode(bytes);
                assert!(lane < 2);
                bytes.push(lane);
            }
            Instruction::I64x2Eq => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xD6));
            }
            Instruction::I64x2Ne => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xD7));
            }
            Instruction::I64x2LtS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xD8));
            }
            Instruction::I64x2GtS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xD9));
            }
            Instruction::I64x2LeS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xDD));
            }
            Instruction::I64x2GeS => {
                bytes.push(0xFD);
                bytes.extend(encoders::u32(0xDB));
            }
        }
    }
}
