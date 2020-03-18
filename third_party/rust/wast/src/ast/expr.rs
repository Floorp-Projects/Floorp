use crate::ast::{self, kw};
use crate::parser::{Parse, Parser, Result};
use std::mem;

/// An expression, or a list of instructions, in the WebAssembly text format.
///
/// This expression type will parse s-expression-folded instructions into a flat
/// list of instructions for emission later on. The implicit `end` instruction
/// at the end of an expression is not included in the `instrs` field.
#[derive(Debug)]
#[allow(missing_docs)]
pub struct Expression<'a> {
    pub instrs: Vec<Instruction<'a>>,
}

impl<'a> Parse<'a> for Expression<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        ExpressionParser::default().parse(parser)
    }
}

/// Helper struct used to parse an `Expression` with helper methods and such.
///
/// The primary purpose of this is to avoid defining expression parsing as a
/// call-thread-stack recursive function. Since we're parsing user input that
/// runs the risk of blowing the call stack, so we want to be sure to use a heap
/// stack structure wherever possible.
#[derive(Default)]
struct ExpressionParser<'a> {
    /// The flat list of instructions that we've parsed so far, and will
    /// eventually become the final `Expression`.
    instrs: Vec<Instruction<'a>>,

    /// Descriptor of all our nested s-expr blocks. This only happens when
    /// instructions themselves are nested.
    stack: Vec<Level<'a>>,
}

enum Paren {
    None,
    Left,
    Right,
}

/// A "kind" of nested block that we can be parsing inside of.
enum Level<'a> {
    /// This is a normal `block` or `loop` or similar, where the instruction
    /// payload here is pushed when the block is exited.
    EndWith(Instruction<'a>),

    /// This is a pretty special variant which means that we're parsing an `if`
    /// statement, and the state of the `if` parsing is tracked internally in
    /// the payload.
    If(If<'a>),

    /// This means we're either parsing inside of `(then ...)` or `(else ...)`
    /// which don't correspond to terminating instructions, we're just in a
    /// nested block.
    IfArm,
}

/// Possible states of "what should be parsed next?" in an `if` expression.
enum If<'a> {
    /// Only the `if` has been parsed, next thing to parse is the clause, if
    /// any, of the `if` instruction.
    Clause(Instruction<'a>),
    /// Next thing to parse is the `then` block
    Then(Instruction<'a>),
    /// Next thing to parse is the `else` block
    Else,
    /// This `if` statement has finished parsing and if anything remains it's a
    /// syntax error.
    End,
}

impl<'a> ExpressionParser<'a> {
    fn parse(mut self, parser: Parser<'a>) -> Result<Expression<'a>> {
        // Here we parse instructions in a loop, and we do not recursively
        // invoke this parse function to avoid blowing the stack on
        // deeply-recursive parses.
        //
        // Our loop generally only finishes once there's no more input left int
        // the `parser`. If there's some unclosed delimiters though (on our
        // `stack`), then we also keep parsing to generate error messages if
        // there's no input left.
        while !parser.is_empty() || !self.stack.is_empty() {
            // As a small ease-of-life adjustment here, if we're parsing inside
            // of an `if block then we require that all sub-components are
            // s-expressions surrounded by `(` and `)`, so verify that here.
            if let Some(Level::If(_)) = self.stack.last() {
                if !parser.is_empty() && !parser.peek::<ast::LParen>() {
                    return Err(parser.error("expected `(`"));
                }
            }

            match self.paren(parser)? {
                // No parenthesis seen? Then we just parse the next instruction
                // and move on.
                Paren::None => self.instrs.push(parser.parse()?),

                // If we see a left-parenthesis then things are a little
                // special. We handle block-like instructions specially
                // (`block`, `loop`, and `if`), and otherwise all other
                // instructions simply get appended once we reach the end of the
                // s-expression.
                //
                // In all cases here we push something onto the `stack` to get
                // popped when the `)` character is seen.
                Paren::Left => {
                    // First up is handling `if` parsing, which is funky in a
                    // whole bunch of ways. See the method internally for more
                    // information.
                    if self.handle_if_lparen(parser)? {
                        continue;
                    }
                    match parser.parse()? {
                        // If block/loop show up then we just need to be sure to
                        // push an `end` instruction whenever the `)` token is
                        // seen
                        i @ Instruction::Block(_) | i @ Instruction::Loop(_) => {
                            self.instrs.push(i);
                            self.stack.push(Level::EndWith(Instruction::End(None)));
                        }

                        // Parsing an `if` instruction is super tricky, so we
                        // push an `If` scope and we let all our scope-based
                        // parsing handle the remaining items.
                        i @ Instruction::If(_) => {
                            self.stack.push(Level::If(If::Clause(i)));
                        }

                        // Anything else means that we're parsing a nested form
                        // such as `(i32.add ...)` which means that the
                        // instruction we parsed will be coming at the end.
                        other => self.stack.push(Level::EndWith(other)),
                    }
                }

                // If we registered a `)` token as being seen, then we're
                // guaranteed there's an item in the `stack` stack for us to
                // pop. We peel that off and take a look at what it says to do.
                Paren::Right => match self.stack.pop().unwrap() {
                    Level::EndWith(i) => self.instrs.push(i),
                    Level::IfArm => {}

                    // If an `if` statement hasn't parsed the clause or `then`
                    // block, then that's an error because there weren't enough
                    // items in the `if` statement. Otherwise we're just careful
                    // to terminate with an `end` instruction.
                    Level::If(If::Clause(_)) => {
                        return Err(parser.error("previous `if` had no clause"));
                    }
                    Level::If(If::Then(_)) => {
                        return Err(parser.error("previous `if` had no `then`"));
                    }
                    Level::If(_) => {
                        self.instrs.push(Instruction::End(None));
                    }
                },
            }
        }

        Ok(Expression {
            instrs: self.instrs,
        })
    }

    /// Parses either `(`, `)`, or nothing.
    fn paren(&self, parser: Parser<'a>) -> Result<Paren> {
        parser.step(|cursor| {
            Ok(match cursor.lparen() {
                Some(rest) => (Paren::Left, rest),
                None if self.stack.is_empty() => (Paren::None, cursor),
                None => match cursor.rparen() {
                    Some(rest) => (Paren::Right, rest),
                    None => (Paren::None, cursor),
                },
            })
        })
    }

    /// Handles all parsing of an `if` statement.
    ///
    /// The syntactical form of an `if` stament looks like:
    ///
    /// ```wat
    /// (if $clause (then $then) (else $else))
    /// ```
    ///
    /// but it turns out we practically see a few things in the wild:
    ///
    /// * inside the `(if ...)` every sub-thing is surrounded by parens
    /// * The `then` and `else` keywords are optional
    /// * The `$then` and `$else` blocks don't need to be surrounded by parens
    ///
    /// That's all attempted to be handled here. The part about all sub-parts
    /// being surrounded by `(` and `)` means that we hook into the `LParen`
    /// parsing above to call this method there unconditionally.
    ///
    /// Returns `true` if the rest of the arm above should be skipped, or
    /// `false` if we should parse the next item as an instruction (because we
    /// didn't handle the lparen here).
    fn handle_if_lparen(&mut self, parser: Parser<'a>) -> Result<bool> {
        // Only execute the code below if there's an `If` listed last.
        let i = match self.stack.last_mut() {
            Some(Level::If(i)) => i,
            _ => return Ok(false),
        };

        // The first thing parsed in an `if` statement is the clause. If the
        // clause starts with `then`, however, then we know to skip the clause
        // and fall through to below.
        if let If::Clause(if_instr) = i {
            let instr = mem::replace(if_instr, Instruction::End(None));
            *i = If::Then(instr);
            if !parser.peek::<kw::then>() {
                return Ok(false);
            }
        }

        // All `if` statements are required to have a `then`. This is either the
        // second s-expr (with or without a leading `then`) or the first s-expr
        // with a leading `then`. The optionality of `then` isn't strictly what
        // the text spec says but it matches wabt for now.
        //
        // Note that when we see the `then`, that's when we actually add the
        // original `if` instruction to the stream.
        if let If::Then(if_instr) = i {
            let instr = mem::replace(if_instr, Instruction::End(None));
            self.instrs.push(instr);
            *i = If::Else;
            if parser.parse::<Option<kw::then>>()?.is_some() {
                self.stack.push(Level::IfArm);
                return Ok(true);
            }
            return Ok(false);
        }

        // effectively the same as the `then` parsing above
        if let If::Else = i {
            self.instrs.push(Instruction::Else(None));
            if parser.parse::<Option<kw::r#else>>()?.is_some() {
                if parser.is_empty() {
                    self.instrs.pop();
                }
                self.stack.push(Level::IfArm);
                return Ok(true);
            }
            *i = If::End;
            return Ok(false);
        }

        // If we made it this far then we're at `If::End` which means that there
        // were too many s-expressions inside the `(if)` and we don't want to
        // parse anything else.
        Err(parser.error("too many payloads inside of `(if)`"))
    }
}

// TODO: document this obscenity
macro_rules! instructions {
    (pub enum Instruction<'a> {
        $(
            $(#[$doc:meta])*
            $name:ident $(($($arg:tt)*))? : [$($binary:tt)*] : $instr:tt $( | $deprecated:tt )?,
        )*
    }) => (
        /// A listing of all WebAssembly instructions that can be in a module
        /// that this crate currently parses.
        #[derive(Debug)]
        #[allow(missing_docs)]
        pub enum Instruction<'a> {
            $(
                $(#[$doc])*
                $name $(( instructions!(@ty $($arg)*) ))?,
            )*

        }

        #[allow(non_snake_case)]
        impl<'a> Parse<'a> for Instruction<'a> {
            fn parse(parser: Parser<'a>) -> Result<Self> {
                $(
                    fn $name<'a>(_parser: Parser<'a>) -> Result<Instruction<'a>> {
                        Ok(Instruction::$name $((
                            instructions!(@parse _parser $($arg)*)?
                        ))?)
                    }
                )*
                let parse_remainder = parser.step(|c| {
                    let (kw, rest) = match c.keyword() {
                        Some(pair) => pair,
                        None => return Err(c.error("expected an instruction")),
                    };
                    match kw {
                        $($instr $( | $deprecated )?=> Ok(($name as fn(_) -> _, rest)),)*
                        _ => return Err(c.error("unknown operator or unexpected token")),
                    }
                })?;
                parse_remainder(parser)
            }
        }

        impl crate::binary::Encode for Instruction<'_> {
            #[allow(non_snake_case)]
            fn encode(&self, v: &mut Vec<u8>) {
                match self {
                    $(
                        Instruction::$name $((instructions!(@first $($arg)*)))? => {
                            fn encode<'a>($(arg: &instructions!(@ty $($arg)*),)? v: &mut Vec<u8>) {
                                instructions!(@encode v $($binary)*);
                                $(<instructions!(@ty $($arg)*) as crate::binary::Encode>::encode(arg, v);)?
                            }
                            encode($( instructions!(@first $($arg)*), )? v)
                        }
                    )*
                }
            }
        }
    );

    (@ty MemArg<$amt:tt>) => (MemArg);
    (@ty $other:ty) => ($other);

    (@first $first:ident $($t:tt)*) => ($first);

    (@parse $parser:ident MemArg<$amt:tt>) => (MemArg::parse($parser, $amt));
    (@parse $parser:ident MemArg) => (compile_error!("must specify `MemArg` default"));
    (@parse $parser:ident $other:ty) => ($parser.parse::<$other>());

    // simd opcodes prefixed with `0xfd` get a varuint32 encoding for their payload
    (@encode $dst:ident 0xfd, $simd:tt) => ({
        $dst.push(0xfd);
        <u32 as crate::binary::Encode>::encode(&$simd, $dst);
    });
    (@encode $dst:ident $($bytes:tt)*) => ($dst.extend_from_slice(&[$($bytes)*]););
}

instructions! {
    pub enum Instruction<'a> {
        Block(BlockType<'a>) : [0x02] : "block",
        If(BlockType<'a>) : [0x04] : "if",
        Else(Option<ast::Id<'a>>) : [0x05] : "else",
        Loop(BlockType<'a>) : [0x03] : "loop",
        End(Option<ast::Id<'a>>) : [0x0b] : "end",

        Unreachable : [0x00] : "unreachable",
        Nop : [0x01] : "nop",
        Br(ast::Index<'a>) : [0x0c] : "br",
        BrIf(ast::Index<'a>) : [0x0d] : "br_if",
        BrTable(BrTableIndices<'a>) : [0x0e] : "br_table",
        Return : [0x0f] : "return",
        Call(ast::Index<'a>) : [0x10] : "call",
        CallIndirect(CallIndirect<'a>) : [0x11] : "call_indirect",
        ReturnCall(ast::Index<'a>) : [0x12] : "return_call",
        ReturnCallIndirect(CallIndirect<'a>) : [0x13] : "return_call_indirect",
        Drop : [0x1a] : "drop",
        Select(SelectTypes<'a>) : [] : "select",
        LocalGet(ast::Index<'a>) : [0x20] : "local.get" | "get_local",
        LocalSet(ast::Index<'a>) : [0x21] : "local.set" | "set_local",
        LocalTee(ast::Index<'a>) : [0x22] : "local.tee" | "tee_local",
        GlobalGet(ast::Index<'a>) : [0x23] : "global.get" | "get_global",
        GlobalSet(ast::Index<'a>) : [0x24] : "global.set" | "set_global",

        TableGet(TableArg<'a>) : [0x25] : "table.get",
        TableSet(TableArg<'a>) : [0x26] : "table.set",

        I32Load(MemArg<4>) : [0x28] : "i32.load",
        I64Load(MemArg<8>) : [0x29] : "i64.load",
        F32Load(MemArg<4>) : [0x2a] : "f32.load",
        F64Load(MemArg<8>) : [0x2b] : "f64.load",
        I32Load8s(MemArg<1>) : [0x2c] : "i32.load8_s",
        I32Load8u(MemArg<1>) : [0x2d] : "i32.load8_u",
        I32Load16s(MemArg<2>) : [0x2e] : "i32.load16_s",
        I32Load16u(MemArg<2>) : [0x2f] : "i32.load16_u",
        I64Load8s(MemArg<1>) : [0x30] : "i64.load8_s",
        I64Load8u(MemArg<1>) : [0x31] : "i64.load8_u",
        I64Load16s(MemArg<2>) : [0x32] : "i64.load16_s",
        I64Load16u(MemArg<2>) : [0x33] : "i64.load16_u",
        I64Load32s(MemArg<4>) : [0x34] : "i64.load32_s",
        I64Load32u(MemArg<4>) : [0x35] : "i64.load32_u",
        I32Store(MemArg<4>) : [0x36] : "i32.store",
        I64Store(MemArg<8>) : [0x37] : "i64.store",
        F32Store(MemArg<4>) : [0x38] : "f32.store",
        F64Store(MemArg<8>) : [0x39] : "f64.store",
        I32Store8(MemArg<1>) : [0x3a] : "i32.store8",
        I32Store16(MemArg<2>) : [0x3b] : "i32.store16",
        I64Store8(MemArg<1>) : [0x3c] : "i64.store8",
        I64Store16(MemArg<2>) : [0x3d] : "i64.store16",
        I64Store32(MemArg<4>) : [0x3e] : "i64.store32",

        // Lots of bulk memory proposal here as well
        MemorySize : [0x3f, 0x00] : "memory.size" | "current_memory",
        MemoryGrow : [0x40, 0x00] : "memory.grow" | "grow_memory",
        MemoryInit(MemoryInit<'a>) : [0xfc, 0x08] : "memory.init",
        MemoryCopy : [0xfc, 0x0a, 0x00, 0x00] : "memory.copy",
        MemoryFill : [0xfc, 0x0b, 0x00] : "memory.fill",
        DataDrop(ast::Index<'a>) : [0xfc, 0x09] : "data.drop",
        ElemDrop(ast::Index<'a>) : [0xfc, 0x0d] : "elem.drop",
        TableInit(TableInit<'a>) : [0xfc, 0x0c] : "table.init",
        TableCopy(TableCopy<'a>) : [0xfc, 0x0e] : "table.copy",
        TableFill(TableArg<'a>) : [0xfc, 0x11] : "table.fill",
        TableSize(TableArg<'a>) : [0xfc, 0x10] : "table.size",
        TableGrow(TableArg<'a>) : [0xfc, 0x0f] : "table.grow",

        RefNull : [0xd0] : "ref.null",
        RefIsNull : [0xd1] : "ref.is_null",
        RefEq : [0xf0] : "ref.eq", // unofficial, part of gc proposal
        RefHost(u32) : [0xff] : "ref.host", // only used in test harness
        RefFunc(ast::Index<'a>) : [0xd2] : "ref.func", // only used in test harness

        // unofficial, part of gc proposal
        StructNew(ast::Index<'a>) : [0xfc, 0x50] : "struct.new",
        StructGet(StructAccess<'a>) : [0xfc, 0x51] : "struct.get",
        StructSet(StructAccess<'a>) : [0xfc, 0x52] : "struct.set",
        StructNarrow(StructNarrow<'a>) : [0xfc, 0x53] : "struct.narrow",

        I32Const(i32) : [0x41] : "i32.const",
        I64Const(i64) : [0x42] : "i64.const",
        F32Const(ast::Float32) : [0x43] : "f32.const",
        F64Const(ast::Float64) : [0x44] : "f64.const",

        I32Clz : [0x67] : "i32.clz",
        I32Ctz : [0x68] : "i32.ctz",
        I32Popcnt : [0x69] : "i32.popcnt",
        I32Add : [0x6a] : "i32.add",
        I32Sub : [0x6b] : "i32.sub",
        I32Mul : [0x6c] : "i32.mul",
        I32DivS : [0x6d] : "i32.div_s",
        I32DivU : [0x6e] : "i32.div_u",
        I32RemS : [0x6f] : "i32.rem_s",
        I32RemU : [0x70] : "i32.rem_u",
        I32And : [0x71] : "i32.and",
        I32Or : [0x72] : "i32.or",
        I32Xor : [0x73] : "i32.xor",
        I32Shl : [0x74] : "i32.shl",
        I32ShrS : [0x75] : "i32.shr_s",
        I32ShrU : [0x76] : "i32.shr_u",
        I32Rotl : [0x77] : "i32.rotl",
        I32Rotr : [0x78] : "i32.rotr",

        I64Clz : [0x79] : "i64.clz",
        I64Ctz : [0x7a] : "i64.ctz",
        I64Popcnt : [0x7b] : "i64.popcnt",
        I64Add : [0x7c] : "i64.add",
        I64Sub : [0x7d] : "i64.sub",
        I64Mul : [0x7e] : "i64.mul",
        I64DivS : [0x7f] : "i64.div_s",
        I64DivU : [0x80] : "i64.div_u",
        I64RemS : [0x81] : "i64.rem_s",
        I64RemU : [0x82] : "i64.rem_u",
        I64And : [0x83] : "i64.and",
        I64Or : [0x84] : "i64.or",
        I64Xor : [0x85] : "i64.xor",
        I64Shl : [0x86] : "i64.shl",
        I64ShrS : [0x87] : "i64.shr_s",
        I64ShrU : [0x88] : "i64.shr_u",
        I64Rotl : [0x89] : "i64.rotl",
        I64Rotr : [0x8a] : "i64.rotr",

        F32Abs : [0x8b] : "f32.abs",
        F32Neg : [0x8c] : "f32.neg",
        F32Ceil : [0x8d] : "f32.ceil",
        F32Floor : [0x8e] : "f32.floor",
        F32Trunc : [0x8f] : "f32.trunc",
        F32Nearest : [0x90] : "f32.nearest",
        F32Sqrt : [0x91] : "f32.sqrt",
        F32Add : [0x92] : "f32.add",
        F32Sub : [0x93] : "f32.sub",
        F32Mul : [0x94] : "f32.mul",
        F32Div : [0x95] : "f32.div",
        F32Min : [0x96] : "f32.min",
        F32Max : [0x97] : "f32.max",
        F32Copysign : [0x98] : "f32.copysign",

        F64Abs : [0x99] : "f64.abs",
        F64Neg : [0x9a] : "f64.neg",
        F64Ceil : [0x9b] : "f64.ceil",
        F64Floor : [0x9c] : "f64.floor",
        F64Trunc : [0x9d] : "f64.trunc",
        F64Nearest : [0x9e] : "f64.nearest",
        F64Sqrt : [0x9f] : "f64.sqrt",
        F64Add : [0xa0] : "f64.add",
        F64Sub : [0xa1] : "f64.sub",
        F64Mul : [0xa2] : "f64.mul",
        F64Div : [0xa3] : "f64.div",
        F64Min : [0xa4] : "f64.min",
        F64Max : [0xa5] : "f64.max",
        F64Copysign : [0xa6] : "f64.copysign",

        I32Eqz : [0x45] : "i32.eqz",
        I32Eq : [0x46] : "i32.eq",
        I32Ne : [0x47] : "i32.ne",
        I32LtS : [0x48] : "i32.lt_s",
        I32LtU : [0x49] : "i32.lt_u",
        I32GtS : [0x4a] : "i32.gt_s",
        I32GtU : [0x4b] : "i32.gt_u",
        I32LeS : [0x4c] : "i32.le_s",
        I32LeU : [0x4d] : "i32.le_u",
        I32GeS : [0x4e] : "i32.ge_s",
        I32GeU : [0x4f] : "i32.ge_u",

        I64Eqz : [0x50] : "i64.eqz",
        I64Eq : [0x51] : "i64.eq",
        I64Ne : [0x52] : "i64.ne",
        I64LtS : [0x53] : "i64.lt_s",
        I64LtU : [0x54] : "i64.lt_u",
        I64GtS : [0x55] : "i64.gt_s",
        I64GtU : [0x56] : "i64.gt_u",
        I64LeS : [0x57] : "i64.le_s",
        I64LeU : [0x58] : "i64.le_u",
        I64GeS : [0x59] : "i64.ge_s",
        I64GeU : [0x5a] : "i64.ge_u",

        F32Eq : [0x5b] : "f32.eq",
        F32Ne : [0x5c] : "f32.ne",
        F32Lt : [0x5d] : "f32.lt",
        F32Gt : [0x5e] : "f32.gt",
        F32Le : [0x5f] : "f32.le",
        F32Ge : [0x60] : "f32.ge",

        F64Eq : [0x61] : "f64.eq",
        F64Ne : [0x62] : "f64.ne",
        F64Lt : [0x63] : "f64.lt",
        F64Gt : [0x64] : "f64.gt",
        F64Le : [0x65] : "f64.le",
        F64Ge : [0x66] : "f64.ge",

        I32WrapI64 : [0xa7] : "i32.wrap_i64" | "i32.wrap/i64",
        I32TruncF32S : [0xa8] : "i32.trunc_f32_s" | "i32.trunc_s/f32",
        I32TruncF32U : [0xa9] : "i32.trunc_f32_u" | "i32.trunc_u/f32",
        I32TruncF64S : [0xaa] : "i32.trunc_f64_s" | "i32.trunc_s/f64",
        I32TruncF64U : [0xab] : "i32.trunc_f64_u" | "i32.trunc_u/f64",
        I64ExtendI32S : [0xac] : "i64.extend_i32_s" | "i64.extend_s/i32",
        I64ExtendI32U : [0xad] : "i64.extend_i32_u" | "i64.extend_u/i32",
        I64TruncF32S : [0xae] : "i64.trunc_f32_s" | "i64.trunc_s/f32",
        I64TruncF32U : [0xaf] : "i64.trunc_f32_u" | "i64.trunc_u/f32",
        I64TruncF64S : [0xb0] : "i64.trunc_f64_s" | "i64.trunc_s/f64",
        I64TruncF64U : [0xb1] : "i64.trunc_f64_u" | "i64.trunc_u/f64",
        F32ConvertI32S : [0xb2] : "f32.convert_i32_s" | "f32.convert_s/i32",
        F32ConvertI32U : [0xb3] : "f32.convert_i32_u" | "f32.convert_u/i32",
        F32ConvertI64S : [0xb4] : "f32.convert_i64_s" | "f32.convert_s/i64",
        F32ConvertI64U : [0xb5] : "f32.convert_i64_u" | "f32.convert_u/i64",
        F32DemoteF64 : [0xb6] : "f32.demote_f64" | "f32.demote/f64",
        F64ConvertI32S : [0xb7] : "f64.convert_i32_s" | "f64.convert_s/i32",
        F64ConvertI32U : [0xb8] : "f64.convert_i32_u" | "f64.convert_u/i32",
        F64ConvertI64S : [0xb9] : "f64.convert_i64_s" | "f64.convert_s/i64",
        F64ConvertI64U : [0xba] : "f64.convert_i64_u" | "f64.convert_u/i64",
        F64PromoteF32 : [0xbb] : "f64.promote_f32" | "f64.promote/f32",
        I32ReinterpretF32 : [0xbc] : "i32.reinterpret_f32" | "i32.reinterpret/f32",
        I64ReinterpretF64 : [0xbd] : "i64.reinterpret_f64" | "i64.reinterpret/f64",
        F32ReinterpretI32 : [0xbe] : "f32.reinterpret_i32" | "f32.reinterpret/i32",
        F64ReinterpretI64 : [0xbf] : "f64.reinterpret_i64" | "f64.reinterpret/i64",

        // non-trapping float to int
        I32TruncSatF32S : [0xfc, 0x00] : "i32.trunc_sat_f32_s" | "i32.trunc_s:sat/f32",
        I32TruncSatF32U : [0xfc, 0x01] : "i32.trunc_sat_f32_u" | "i32.trunc_u:sat/f32",
        I32TruncSatF64S : [0xfc, 0x02] : "i32.trunc_sat_f64_s" | "i32.trunc_s:sat/f64",
        I32TruncSatF64U : [0xfc, 0x03] : "i32.trunc_sat_f64_u" | "i32.trunc_u:sat/f64",
        I64TruncSatF32S : [0xfc, 0x04] : "i64.trunc_sat_f32_s" | "i64.trunc_s:sat/f32",
        I64TruncSatF32U : [0xfc, 0x05] : "i64.trunc_sat_f32_u" | "i64.trunc_u:sat/f32",
        I64TruncSatF64S : [0xfc, 0x06] : "i64.trunc_sat_f64_s" | "i64.trunc_s:sat/f64",
        I64TruncSatF64U : [0xfc, 0x07] : "i64.trunc_sat_f64_u" | "i64.trunc_u:sat/f64",

        // sign extension proposal
        I32Extend8S : [0xc0] : "i32.extend8_s",
        I32Extend16S : [0xc1] : "i32.extend16_s",
        I64Extend8S : [0xc2] : "i64.extend8_s",
        I64Extend16S : [0xc3] : "i64.extend16_s",
        I64Extend32S : [0xc4] : "i64.extend32_s",

        // atomics proposal
        MemoryAtomicNotify(MemArg<4>) : [0xfe, 0x00] : "memory.atomic.notify" | "atomic.notify",
        MemoryAtomicWait32(MemArg<4>) : [0xfe, 0x01] : "memory.atomic.wait32" | "i32.atomic.wait",
        MemoryAtomicWait64(MemArg<8>) : [0xfe, 0x02] : "memory.atomic.wait64" | "i64.atomic.wait",
        AtomicFence : [0xfe, 0x03, 0x00] : "atomic.fence",

        I32AtomicLoad(MemArg<4>) : [0xfe, 0x10] : "i32.atomic.load",
        I64AtomicLoad(MemArg<8>) : [0xfe, 0x11] : "i64.atomic.load",
        I32AtomicLoad8u(MemArg<1>) : [0xfe, 0x12] : "i32.atomic.load8_u",
        I32AtomicLoad16u(MemArg<2>) : [0xfe, 0x13] : "i32.atomic.load16_u",
        I64AtomicLoad8u(MemArg<1>) : [0xfe, 0x14] : "i64.atomic.load8_u",
        I64AtomicLoad16u(MemArg<2>) : [0xfe, 0x15] : "i64.atomic.load16_u",
        I64AtomicLoad32u(MemArg<4>) : [0xfe, 0x16] : "i64.atomic.load32_u",
        I32AtomicStore(MemArg<4>) : [0xfe, 0x17] : "i32.atomic.store",
        I64AtomicStore(MemArg<8>) : [0xfe, 0x18] : "i64.atomic.store",
        I32AtomicStore8(MemArg<1>) : [0xfe, 0x19] : "i32.atomic.store8",
        I32AtomicStore16(MemArg<2>) : [0xfe, 0x1a] : "i32.atomic.store16",
        I64AtomicStore8(MemArg<1>) : [0xfe, 0x1b] : "i64.atomic.store8",
        I64AtomicStore16(MemArg<2>) : [0xfe, 0x1c] : "i64.atomic.store16",
        I64AtomicStore32(MemArg<4>) : [0xfe, 0x1d] : "i64.atomic.store32",

        I32AtomicRmwAdd(MemArg<4>) : [0xfe, 0x1e] : "i32.atomic.rmw.add",
        I64AtomicRmwAdd(MemArg<8>) : [0xfe, 0x1f] : "i64.atomic.rmw.add",
        I32AtomicRmw8AddU(MemArg<1>) : [0xfe, 0x20] : "i32.atomic.rmw8.add_u",
        I32AtomicRmw16AddU(MemArg<2>) : [0xfe, 0x21] : "i32.atomic.rmw16.add_u",
        I64AtomicRmw8AddU(MemArg<1>) : [0xfe, 0x22] : "i64.atomic.rmw8.add_u",
        I64AtomicRmw16AddU(MemArg<2>) : [0xfe, 0x23] : "i64.atomic.rmw16.add_u",
        I64AtomicRmw32AddU(MemArg<4>) : [0xfe, 0x24] : "i64.atomic.rmw32.add_u",

        I32AtomicRmwSub(MemArg<4>) : [0xfe, 0x25] : "i32.atomic.rmw.sub",
        I64AtomicRmwSub(MemArg<8>) : [0xfe, 0x26] : "i64.atomic.rmw.sub",
        I32AtomicRmw8SubU(MemArg<1>) : [0xfe, 0x27] : "i32.atomic.rmw8.sub_u",
        I32AtomicRmw16SubU(MemArg<2>) : [0xfe, 0x28] : "i32.atomic.rmw16.sub_u",
        I64AtomicRmw8SubU(MemArg<1>) : [0xfe, 0x29] : "i64.atomic.rmw8.sub_u",
        I64AtomicRmw16SubU(MemArg<2>) : [0xfe, 0x2a] : "i64.atomic.rmw16.sub_u",
        I64AtomicRmw32SubU(MemArg<4>) : [0xfe, 0x2b] : "i64.atomic.rmw32.sub_u",

        I32AtomicRmwAnd(MemArg<4>) : [0xfe, 0x2c] : "i32.atomic.rmw.and",
        I64AtomicRmwAnd(MemArg<8>) : [0xfe, 0x2d] : "i64.atomic.rmw.and",
        I32AtomicRmw8AndU(MemArg<1>) : [0xfe, 0x2e] : "i32.atomic.rmw8.and_u",
        I32AtomicRmw16AndU(MemArg<2>) : [0xfe, 0x2f] : "i32.atomic.rmw16.and_u",
        I64AtomicRmw8AndU(MemArg<1>) : [0xfe, 0x30] : "i64.atomic.rmw8.and_u",
        I64AtomicRmw16AndU(MemArg<2>) : [0xfe, 0x31] : "i64.atomic.rmw16.and_u",
        I64AtomicRmw32AndU(MemArg<4>) : [0xfe, 0x32] : "i64.atomic.rmw32.and_u",

        I32AtomicRmwOr(MemArg<4>) : [0xfe, 0x33] : "i32.atomic.rmw.or",
        I64AtomicRmwOr(MemArg<8>) : [0xfe, 0x34] : "i64.atomic.rmw.or",
        I32AtomicRmw8OrU(MemArg<1>) : [0xfe, 0x35] : "i32.atomic.rmw8.or_u",
        I32AtomicRmw16OrU(MemArg<2>) : [0xfe, 0x36] : "i32.atomic.rmw16.or_u",
        I64AtomicRmw8OrU(MemArg<1>) : [0xfe, 0x37] : "i64.atomic.rmw8.or_u",
        I64AtomicRmw16OrU(MemArg<2>) : [0xfe, 0x38] : "i64.atomic.rmw16.or_u",
        I64AtomicRmw32OrU(MemArg<4>) : [0xfe, 0x39] : "i64.atomic.rmw32.or_u",

        I32AtomicRmwXor(MemArg<4>) : [0xfe, 0x3a] : "i32.atomic.rmw.xor",
        I64AtomicRmwXor(MemArg<8>) : [0xfe, 0x3b] : "i64.atomic.rmw.xor",
        I32AtomicRmw8XorU(MemArg<1>) : [0xfe, 0x3c] : "i32.atomic.rmw8.xor_u",
        I32AtomicRmw16XorU(MemArg<2>) : [0xfe, 0x3d] : "i32.atomic.rmw16.xor_u",
        I64AtomicRmw8XorU(MemArg<1>) : [0xfe, 0x3e] : "i64.atomic.rmw8.xor_u",
        I64AtomicRmw16XorU(MemArg<2>) : [0xfe, 0x3f] : "i64.atomic.rmw16.xor_u",
        I64AtomicRmw32XorU(MemArg<4>) : [0xfe, 0x40] : "i64.atomic.rmw32.xor_u",

        I32AtomicRmwXchg(MemArg<4>) : [0xfe, 0x41] : "i32.atomic.rmw.xchg",
        I64AtomicRmwXchg(MemArg<8>) : [0xfe, 0x42] : "i64.atomic.rmw.xchg",
        I32AtomicRmw8XchgU(MemArg<1>) : [0xfe, 0x43] : "i32.atomic.rmw8.xchg_u",
        I32AtomicRmw16XchgU(MemArg<2>) : [0xfe, 0x44] : "i32.atomic.rmw16.xchg_u",
        I64AtomicRmw8XchgU(MemArg<1>) : [0xfe, 0x45] : "i64.atomic.rmw8.xchg_u",
        I64AtomicRmw16XchgU(MemArg<2>) : [0xfe, 0x46] : "i64.atomic.rmw16.xchg_u",
        I64AtomicRmw32XchgU(MemArg<4>) : [0xfe, 0x47] : "i64.atomic.rmw32.xchg_u",

        I32AtomicRmwCmpxchg(MemArg<4>) : [0xfe, 0x48] : "i32.atomic.rmw.cmpxchg",
        I64AtomicRmwCmpxchg(MemArg<8>) : [0xfe, 0x49] : "i64.atomic.rmw.cmpxchg",
        I32AtomicRmw8CmpxchgU(MemArg<1>) : [0xfe, 0x4a] : "i32.atomic.rmw8.cmpxchg_u",
        I32AtomicRmw16CmpxchgU(MemArg<2>) : [0xfe, 0x4b] : "i32.atomic.rmw16.cmpxchg_u",
        I64AtomicRmw8CmpxchgU(MemArg<1>) : [0xfe, 0x4c] : "i64.atomic.rmw8.cmpxchg_u",
        I64AtomicRmw16CmpxchgU(MemArg<2>) : [0xfe, 0x4d] : "i64.atomic.rmw16.cmpxchg_u",
        I64AtomicRmw32CmpxchgU(MemArg<4>) : [0xfe, 0x4e] : "i64.atomic.rmw32.cmpxchg_u",

        V128Load(MemArg<16>) : [0xfd, 0x00] : "v128.load",
        V128Store(MemArg<16>) : [0xfd, 0x01] : "v128.store",
        V128Const(V128Const) : [0xfd, 0x02] : "v128.const",

        I8x16Splat : [0xfd, 0x04] : "i8x16.splat",
        I8x16ExtractLaneS(u8) : [0xfd, 0x05] : "i8x16.extract_lane_s",
        I8x16ExtractLaneU(u8) : [0xfd, 0x06] : "i8x16.extract_lane_u",
        I8x16ReplaceLane(u8) : [0xfd, 0x07] : "i8x16.replace_lane",
        I16x8Splat : [0xfd, 0x08] : "i16x8.splat",
        I16x8ExtractLaneS(u8) : [0xfd, 0x09] : "i16x8.extract_lane_s",
        I16x8ExtractLaneU(u8) : [0xfd, 0x0a] : "i16x8.extract_lane_u",
        I16x8ReplaceLane(u8) : [0xfd, 0x0b] : "i16x8.replace_lane",
        I32x4Splat : [0xfd, 0x0c] : "i32x4.splat",
        I32x4ExtractLane(u8) : [0xfd, 0x0d] : "i32x4.extract_lane",
        I32x4ReplaceLane(u8) : [0xfd, 0x0e] : "i32x4.replace_lane",
        I64x2Splat : [0xfd, 0x0f] : "i64x2.splat",
        I64x2ExtractLane(u8) : [0xfd, 0x10] : "i64x2.extract_lane",
        I64x2ReplaceLane(u8) : [0xfd, 0x11] : "i64x2.replace_lane",
        F32x4Splat : [0xfd, 0x12] : "f32x4.splat",
        F32x4ExtractLane(u8) : [0xfd, 0x13] : "f32x4.extract_lane",
        F32x4ReplaceLane(u8) : [0xfd, 0x14] : "f32x4.replace_lane",
        F64x2Splat : [0xfd, 0x15] : "f64x2.splat",
        F64x2ExtractLane(u8) : [0xfd, 0x16] : "f64x2.extract_lane",
        F64x2ReplaceLane(u8) : [0xfd, 0x17] : "f64x2.replace_lane",

        I8x16Eq : [0xfd, 0x18] : "i8x16.eq",
        I8x16Ne : [0xfd, 0x19] : "i8x16.ne",
        I8x16LtS : [0xfd, 0x1a] : "i8x16.lt_s",
        I8x16LtU : [0xfd, 0x1b] : "i8x16.lt_u",
        I8x16GtS : [0xfd, 0x1c] : "i8x16.gt_s",
        I8x16GtU : [0xfd, 0x1d] : "i8x16.gt_u",
        I8x16LeS : [0xfd, 0x1e] : "i8x16.le_s",
        I8x16LeU : [0xfd, 0x1f] : "i8x16.le_u",
        I8x16GeS : [0xfd, 0x20] : "i8x16.ge_s",
        I8x16GeU : [0xfd, 0x21] : "i8x16.ge_u",
        I16x8Eq : [0xfd, 0x22] : "i16x8.eq",
        I16x8Ne : [0xfd, 0x23] : "i16x8.ne",
        I16x8LtS : [0xfd, 0x24] : "i16x8.lt_s",
        I16x8LtU : [0xfd, 0x25] : "i16x8.lt_u",
        I16x8GtS : [0xfd, 0x26] : "i16x8.gt_s",
        I16x8GtU : [0xfd, 0x27] : "i16x8.gt_u",
        I16x8LeS : [0xfd, 0x28] : "i16x8.le_s",
        I16x8LeU : [0xfd, 0x29] : "i16x8.le_u",
        I16x8GeS : [0xfd, 0x2a] : "i16x8.ge_s",
        I16x8GeU : [0xfd, 0x2b] : "i16x8.ge_u",
        I32x4Eq : [0xfd, 0x2c] : "i32x4.eq",
        I32x4Ne : [0xfd, 0x2d] : "i32x4.ne",
        I32x4LtS : [0xfd, 0x2e] : "i32x4.lt_s",
        I32x4LtU : [0xfd, 0x2f] : "i32x4.lt_u",
        I32x4GtS : [0xfd, 0x30] : "i32x4.gt_s",
        I32x4GtU : [0xfd, 0x31] : "i32x4.gt_u",
        I32x4LeS : [0xfd, 0x32] : "i32x4.le_s",
        I32x4LeU : [0xfd, 0x33] : "i32x4.le_u",
        I32x4GeS : [0xfd, 0x34] : "i32x4.ge_s",
        I32x4GeU : [0xfd, 0x35] : "i32x4.ge_u",

        F32x4Eq : [0xfd, 0x40] : "f32x4.eq",
        F32x4Ne : [0xfd, 0x41] : "f32x4.ne",
        F32x4Lt : [0xfd, 0x42] : "f32x4.lt",
        F32x4Gt : [0xfd, 0x43] : "f32x4.gt",
        F32x4Le : [0xfd, 0x44] : "f32x4.le",
        F32x4Ge : [0xfd, 0x45] : "f32x4.ge",
        F64x2Eq : [0xfd, 0x46] : "f64x2.eq",
        F64x2Ne : [0xfd, 0x47] : "f64x2.ne",
        F64x2Lt : [0xfd, 0x48] : "f64x2.lt",
        F64x2Gt : [0xfd, 0x49] : "f64x2.gt",
        F64x2Le : [0xfd, 0x4a] : "f64x2.le",
        F64x2Ge : [0xfd, 0x4b] : "f64x2.ge",

        V128Not : [0xfd, 0x4c] : "v128.not",
        V128And : [0xfd, 0x4d] : "v128.and",
        V128Or : [0xfd, 0x4e] : "v128.or",
        V128Xor : [0xfd, 0x4f] : "v128.xor",
        V128Bitselect : [0xfd, 0x50] : "v128.bitselect",

        I8x16Neg : [0xfd, 0x51] : "i8x16.neg",
        I8x16AnyTrue : [0xfd, 0x52] : "i8x16.any_true",
        I8x16AllTrue : [0xfd, 0x53] : "i8x16.all_true",
        I8x16Shl : [0xfd, 0x54] : "i8x16.shl",
        I8x16ShrS : [0xfd, 0x55] : "i8x16.shr_s",
        I8x16ShrU : [0xfd, 0x56] : "i8x16.shr_u",
        I8x16Add : [0xfd, 0x57] : "i8x16.add",
        I8x16AddSaturateS : [0xfd, 0x58] : "i8x16.add_saturate_s",
        I8x16AddSaturateU : [0xfd, 0x59] : "i8x16.add_saturate_u",
        I8x16Sub : [0xfd, 0x5a] : "i8x16.sub",
        I8x16SubSaturateS : [0xfd, 0x5b] : "i8x16.sub_saturate_s",
        I8x16SubSaturateU : [0xfd, 0x5c] : "i8x16.sub_saturate_u",
        I8x16Mul : [0xfd, 0x5d] : "i8x16.mul",
        I8x16MinS : [0xfd, 0x5e] : "i8x16.min_s",
        I8x16MinU : [0xfd, 0x5f] : "i8x16.min_u",
        I8x16MaxS : [0xfd, 0x60] : "i8x16.max_s",
        I8x16MaxU : [0xfd, 0x61] : "i8x16.max_u",

        I16x8Neg : [0xfd, 0x62] : "i16x8.neg",
        I16x8AnyTrue : [0xfd, 0x63] : "i16x8.any_true",
        I16x8AllTrue : [0xfd, 0x64] : "i16x8.all_true",
        I16x8Shl : [0xfd, 0x65] : "i16x8.shl",
        I16x8ShrS : [0xfd, 0x66] : "i16x8.shr_s",
        I16x8ShrU : [0xfd, 0x67] : "i16x8.shr_u",
        I16x8Add : [0xfd, 0x68] : "i16x8.add",
        I16x8AddSaturateS : [0xfd, 0x69] : "i16x8.add_saturate_s",
        I16x8AddSaturateU : [0xfd, 0x6a] : "i16x8.add_saturate_u",
        I16x8Sub : [0xfd, 0x6b] : "i16x8.sub",
        I16x8SubSaturateS : [0xfd, 0x6c] : "i16x8.sub_saturate_s",
        I16x8SubSaturateU : [0xfd, 0x6d] : "i16x8.sub_saturate_u",
        I16x8Mul : [0xfd, 0x6e] : "i16x8.mul",
        I16x8MinS : [0xfd, 0x6f] : "i16x8.min_s",
        I16x8MinU : [0xfd, 0x70] : "i16x8.min_u",
        I16x8MaxS : [0xfd, 0x71] : "i16x8.max_s",
        I16x8MaxU : [0xfd, 0x72] : "i16x8.max_u",

        I32x4Neg : [0xfd, 0x73] : "i32x4.neg",
        I32x4AnyTrue : [0xfd, 0x74] : "i32x4.any_true",
        I32x4AllTrue : [0xfd, 0x75] : "i32x4.all_true",
        I32x4Shl : [0xfd, 0x76] : "i32x4.shl",
        I32x4ShrS : [0xfd, 0x77] : "i32x4.shr_s",
        I32x4ShrU : [0xfd, 0x78] : "i32x4.shr_u",
        I32x4Add : [0xfd, 0x79] : "i32x4.add",
        I32x4Sub : [0xfd, 0x7c] : "i32x4.sub",
        I32x4Mul : [0xfd, 0x7f] : "i32x4.mul",
        I32x4MinS : [0xfd, 0x80] : "i32x4.min_s",
        I32x4MinU : [0xfd, 0x81] : "i32x4.min_u",
        I32x4MaxS : [0xfd, 0x82] : "i32x4.max_s",
        I32x4MaxU : [0xfd, 0x83] : "i32x4.max_u",

        I64x2Neg : [0xfd, 0x84] : "i64x2.neg",
        I64x2AnyTrue : [0xfd, 0x85] : "i64x2.any_true",
        I64x2AllTrue : [0xfd, 0x86] : "i64x2.all_true",
        I64x2Shl : [0xfd, 0x87] : "i64x2.shl",
        I64x2ShrS : [0xfd, 0x88] : "i64x2.shr_s",
        I64x2ShrU : [0xfd, 0x89] : "i64x2.shr_u",
        I64x2Add : [0xfd, 0x8a] : "i64x2.add",
        I64x2Sub : [0xfd, 0x8d] : "i64x2.sub",
        I64x2Mul : [0xfd, 0x90] : "i64x2.mul",

        F32x4Abs : [0xfd, 0x95] : "f32x4.abs",
        F32x4Neg : [0xfd, 0x96] : "f32x4.neg",
        F32x4Sqrt : [0xfd, 0x97] : "f32x4.sqrt",
        F32x4Add : [0xfd, 0x9a] : "f32x4.add",
        F32x4Sub : [0xfd, 0x9b] : "f32x4.sub",
        F32x4Mul : [0xfd, 0x9c] : "f32x4.mul",
        F32x4Div : [0xfd, 0x9d] : "f32x4.div",
        F32x4Min : [0xfd, 0x9e] : "f32x4.min",
        F32x4Max : [0xfd, 0x9f] : "f32x4.max",

        F64x2Abs : [0xfd, 0xa0] : "f64x2.abs",
        F64x2Neg : [0xfd, 0xa1] : "f64x2.neg",
        F64x2Sqrt : [0xfd, 0xa2] : "f64x2.sqrt",
        F64x2Add : [0xfd, 0xa5] : "f64x2.add",
        F64x2Sub : [0xfd, 0xa6] : "f64x2.sub",
        F64x2Mul : [0xfd, 0xa7] : "f64x2.mul",
        F64x2Div : [0xfd, 0xa8] : "f64x2.div",
        F64x2Min : [0xfd, 0xa9] : "f64x2.min",
        F64x2Max : [0xfd, 0xaa] : "f64x2.max",

        I32x4TruncSatF32x4S : [0xfd, 0xab] : "i32x4.trunc_sat_f32x4_s",
        I32x4TruncSatF32x4U : [0xfd, 0xac] : "i32x4.trunc_sat_f32x4_u",
        F32x4ConvertI32x4S : [0xfd, 0xaf] : "f32x4.convert_i32x4_s",
        F32x4ConvertI32x4U : [0xfd, 0xb0] : "f32x4.convert_i32x4_u",
        V8x16Swizzle : [0xfd, 0xc0] : "v8x16.swizzle",
        V8x16Shuffle(V8x16Shuffle) : [0xfd, 0x03] : "v8x16.shuffle",
        V8x16LoadSplat(MemArg<1>) : [0xfd, 0xc2] : "v8x16.load_splat",
        V16x8LoadSplat(MemArg<2>) : [0xfd, 0xc3] : "v16x8.load_splat",
        V32x4LoadSplat(MemArg<4>) : [0xfd, 0xc4] : "v32x4.load_splat",
        V64x2LoadSplat(MemArg<8>) : [0xfd, 0xc5] : "v64x2.load_splat",

        I8x16NarrowI16x8S : [0xfd, 0xc6] : "i8x16.narrow_i16x8_s",
        I8x16NarrowI16x8U : [0xfd, 0xc7] : "i8x16.narrow_i16x8_u",
        I16x8NarrowI32x4S : [0xfd, 0xc8] : "i16x8.narrow_i32x4_s",
        I16x8NarrowI32x4U : [0xfd, 0xc9] : "i16x8.narrow_i32x4_u",

        I16x8WidenLowI8x16S : [0xfd, 0xca] : "i16x8.widen_low_i8x16_s",
        I16x8WidenHighI8x16S : [0xfd, 0xcb] : "i16x8.widen_high_i8x16_s",
        I16x8WidenLowI8x16U : [0xfd, 0xcc] : "i16x8.widen_low_i8x16_u",
        I16x8WidenHighI8x16u : [0xfd, 0xcd] : "i16x8.widen_high_i8x16_u",
        I32x4WidenLowI16x8S : [0xfd, 0xce] : "i32x4.widen_low_i16x8_s",
        I32x4WidenHighI16x8S : [0xfd, 0xcf] : "i32x4.widen_high_i16x8_s",
        I32x4WidenLowI16x8U : [0xfd, 0xd0] : "i32x4.widen_low_i16x8_u",
        I32x4WidenHighI16x8u : [0xfd, 0xd1] : "i32x4.widen_high_i16x8_u",

        I16x8Load8x8S(MemArg<8>) : [0xfd, 0xd2] : "i16x8.load8x8_s",
        I16x8Load8x8U(MemArg<8>) : [0xfd, 0xd3] : "i16x8.load8x8_u",
        I32x4Load16x4S(MemArg<8>) : [0xfd, 0xd4] : "i32x4.load16x4_s",
        I32x4Load16x4U(MemArg<8>) : [0xfd, 0xd5] : "i32x4.load16x4_u",
        I64x2Load32x2S(MemArg<8>) : [0xfd, 0xd6] : "i64x2.load32x2_s",
        I64x2Load32x2U(MemArg<8>) : [0xfd, 0xd7] : "i64x2.load32x2_u",
        V128Andnot : [0xfd, 0xd8] : "v128.andnot",

        I8x16AvgrU : [0xfd, 0xd9] : "i8x16.avgr_u",
        I16x8AvgrU : [0xfd, 0xda] : "i16x8.avgr_u",

        Try(BlockType<'a>) : [0x06] : "try",
        Catch : [0x07] : "catch",
        Throw(ast::Index<'a>) : [0x08] : "throw",
        Rethrow : [0x09] : "rethrow",
        BrOnExn(BrOnExn<'a>) : [0x0a] : "br_on_exn",
    }
}

/// Extra information associated with block-related instructions.
///
/// This is used to label blocks and also annotate what types are expected for
/// the block.
#[derive(Debug)]
#[allow(missing_docs)]
pub struct BlockType<'a> {
    pub label: Option<ast::Id<'a>>,
    pub ty: ast::TypeUse<'a>,
}

impl<'a> Parse<'a> for BlockType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        Ok(BlockType {
            label: parser.parse()?,
            ty: ast::TypeUse::parse_no_names(parser)?,
        })
    }
}

/// Extra information associated with the `br_table` instruction.
#[allow(missing_docs)]
#[derive(Debug)]
pub struct BrTableIndices<'a> {
    pub labels: Vec<ast::Index<'a>>,
    pub default: ast::Index<'a>,
}

impl<'a> Parse<'a> for BrTableIndices<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut labels = Vec::new();
        labels.push(parser.parse()?);
        while parser.peek::<ast::Index>() {
            labels.push(parser.parse()?);
        }
        let default = labels.pop().unwrap();
        Ok(BrTableIndices { labels, default })
    }
}

/// Payload for memory-related instructions indicating offset/alignment of
/// memory accesses.
#[derive(Debug)]
pub struct MemArg {
    /// The alignment of this access.
    ///
    /// This is not stored as a log, this is the actual alignment (e.g. 1, 2, 4,
    /// 8, etc).
    pub align: u32,
    /// The offset, in bytes of this access.
    pub offset: u32,
}

impl MemArg {
    fn parse(parser: Parser<'_>, default_align: u32) -> Result<Self> {
        fn parse_field(name: &str, parser: Parser<'_>) -> Result<Option<u32>> {
            parser.step(|c| {
                let (kw, rest) = match c.keyword() {
                    Some(p) => p,
                    None => return Ok((None, c)),
                };
                if !kw.starts_with(name) {
                    return Ok((None, c));
                }
                let kw = &kw[name.len()..];
                if !kw.starts_with("=") {
                    return Ok((None, c));
                }
                let num = &kw[1..];
                let num = if num.starts_with("0x") {
                    match u32::from_str_radix(&num[2..], 16) {
                        Ok(n) => n,
                        Err(_) => return Err(c.error("i32 constant out of range")),
                    }
                } else {
                    match num.parse() {
                        Ok(n) => n,
                        Err(_) => return Err(c.error("i32 constant out of range")),
                    }
                };

                Ok((Some(num), rest))
            })
        }
        let offset = parse_field("offset", parser)?.unwrap_or(0);
        let align = match parse_field("align", parser)? {
            Some(n) if !n.is_power_of_two() => {
                return Err(parser.error("alignment must be a power of two"))
            }
            n => n.unwrap_or(default_align),
        };

        Ok(MemArg { offset, align })
    }
}

/// Extra data associated with the `call_indirect` instruction.
#[derive(Debug)]
pub struct CallIndirect<'a> {
    /// The table that this call is going to be indexing.
    pub table: ast::Index<'a>,
    /// Type type signature that this `call_indirect` instruction is using.
    pub ty: ast::TypeUse<'a>,
}

impl<'a> Parse<'a> for CallIndirect<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut table: Option<_> = parser.parse()?;
        let ty = ast::TypeUse::parse_no_names(parser)?;
        // Turns out the official test suite at this time thinks table
        // identifiers comes first but wabt's test suites asserts differently
        // putting them second. Let's just handle both.
        if table.is_none() {
            table = parser.parse()?;
        }
        Ok(CallIndirect {
            table: table.unwrap_or(ast::Index::Num(0)),
            ty,
        })
    }
}

/// Extra data associated with the `table.init` instruction
#[derive(Debug)]
pub struct TableInit<'a> {
    /// The index of the table we're copying into.
    pub table: ast::Index<'a>,
    /// The index of the element segment we're copying into a table.
    pub elem: ast::Index<'a>,
}

impl<'a> Parse<'a> for TableInit<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let table_or_elem = parser.parse()?;
        let (table, elem) = match parser.parse()? {
            Some(elem) => (table_or_elem, elem),
            None => (ast::Index::Num(0), table_or_elem),
        };
        Ok(TableInit { table, elem })
    }
}

/// Extra data associated with the `table.copy` instruction.
#[derive(Debug)]
pub struct TableCopy<'a> {
    /// The index of the destination table to copy into.
    pub dst: ast::Index<'a>,
    /// The index of the source table to copy from.
    pub src: ast::Index<'a>,
}

impl<'a> Parse<'a> for TableCopy<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let (dst, src) = if let Some(dst) = parser.parse()? {
            (dst, parser.parse()?)
        } else {
            (ast::Index::Num(0), ast::Index::Num(0))
        };
        Ok(TableCopy { dst, src })
    }
}

/// Extra data associated with unary table instructions.
#[derive(Debug)]
pub struct TableArg<'a> {
    /// The index of the table argument.
    pub dst: ast::Index<'a>,
}

impl<'a> Parse<'a> for TableArg<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let dst = if let Some(dst) = parser.parse()? {
            dst
        } else {
            ast::Index::Num(0)
        };
        Ok(TableArg { dst })
    }
}

/// Extra data associated with the `memory.init` instruction
#[derive(Debug)]
pub struct MemoryInit<'a> {
    /// The index of the data segment we're copying into memory.
    pub data: ast::Index<'a>,
}

impl<'a> Parse<'a> for MemoryInit<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        Ok(MemoryInit {
            data: parser.parse()?,
        })
    }
}

/// Extra data associated with the `struct.get/set` instructions
#[derive(Debug)]
pub struct StructAccess<'a> {
    /// The index of the struct type we're accessing.
    pub r#struct: ast::Index<'a>,
    /// The index of the field of the struct we're accessing
    pub field: ast::Index<'a>,
}

impl<'a> Parse<'a> for StructAccess<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        Ok(StructAccess {
            r#struct: parser.parse()?,
            field: parser.parse()?,
        })
    }
}

/// Extra data associated with the `struct.narrow` instruction
#[derive(Debug)]
pub struct StructNarrow<'a> {
    /// The type of the struct we're casting from
    pub from: ast::ValType<'a>,
    /// The type of the struct we're casting to
    pub to: ast::ValType<'a>,
}

impl<'a> Parse<'a> for StructNarrow<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        Ok(StructNarrow {
            from: parser.parse()?,
            to: parser.parse()?,
        })
    }
}

/// Different ways to specify a `v128.const` instruction
#[derive(Debug)]
#[rustfmt::skip]
#[allow(missing_docs)]
pub enum V128Const {
    I8x16([i8; 16]),
    I16x8([i16; 8]),
    I32x4([i32; 4]),
    I64x2([i64; 2]),
    F32x4([ast::Float32; 4]),
    F64x2([ast::Float64; 2]),
}

impl V128Const {
    /// Returns the raw little-ended byte sequence used to represent this
    /// `v128` constant`
    ///
    /// This is typically suitable for encoding as the payload of the
    /// `v128.const` instruction.
    #[rustfmt::skip]
    pub fn to_le_bytes(&self) -> [u8; 16] {
        match self {
            V128Const::I8x16(arr) => [
                arr[0] as u8,
                arr[1] as u8,
                arr[2] as u8,
                arr[3] as u8,
                arr[4] as u8,
                arr[5] as u8,
                arr[6] as u8,
                arr[7] as u8,
                arr[8] as u8,
                arr[9] as u8,
                arr[10] as u8,
                arr[11] as u8,
                arr[12] as u8,
                arr[13] as u8,
                arr[14] as u8,
                arr[15] as u8,
            ],
            V128Const::I16x8(arr) => {
                let a1 = arr[0].to_le_bytes();
                let a2 = arr[1].to_le_bytes();
                let a3 = arr[2].to_le_bytes();
                let a4 = arr[3].to_le_bytes();
                let a5 = arr[4].to_le_bytes();
                let a6 = arr[5].to_le_bytes();
                let a7 = arr[6].to_le_bytes();
                let a8 = arr[7].to_le_bytes();
                [
                    a1[0], a1[1],
                    a2[0], a2[1],
                    a3[0], a3[1],
                    a4[0], a4[1],
                    a5[0], a5[1],
                    a6[0], a6[1],
                    a7[0], a7[1],
                    a8[0], a8[1],
                ]
            }
            V128Const::I32x4(arr) => {
                let a1 = arr[0].to_le_bytes();
                let a2 = arr[1].to_le_bytes();
                let a3 = arr[2].to_le_bytes();
                let a4 = arr[3].to_le_bytes();
                [
                    a1[0], a1[1], a1[2], a1[3],
                    a2[0], a2[1], a2[2], a2[3],
                    a3[0], a3[1], a3[2], a3[3],
                    a4[0], a4[1], a4[2], a4[3],
                ]
            }
            V128Const::I64x2(arr) => {
                let a1 = arr[0].to_le_bytes();
                let a2 = arr[1].to_le_bytes();
                [
                    a1[0], a1[1], a1[2], a1[3], a1[4], a1[5], a1[6], a1[7],
                    a2[0], a2[1], a2[2], a2[3], a2[4], a2[5], a2[6], a2[7],
                ]
            }
            V128Const::F32x4(arr) => {
                let a1 = arr[0].bits.to_le_bytes();
                let a2 = arr[1].bits.to_le_bytes();
                let a3 = arr[2].bits.to_le_bytes();
                let a4 = arr[3].bits.to_le_bytes();
                [
                    a1[0], a1[1], a1[2], a1[3],
                    a2[0], a2[1], a2[2], a2[3],
                    a3[0], a3[1], a3[2], a3[3],
                    a4[0], a4[1], a4[2], a4[3],
                ]
            }
            V128Const::F64x2(arr) => {
                let a1 = arr[0].bits.to_le_bytes();
                let a2 = arr[1].bits.to_le_bytes();
                [
                    a1[0], a1[1], a1[2], a1[3], a1[4], a1[5], a1[6], a1[7],
                    a2[0], a2[1], a2[2], a2[3], a2[4], a2[5], a2[6], a2[7],
                ]
            }
        }
    }
}

impl<'a> Parse<'a> for V128Const {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::i8x16>() {
            parser.parse::<kw::i8x16>()?;
            Ok(V128Const::I8x16([
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
            ]))
        } else if l.peek::<kw::i16x8>() {
            parser.parse::<kw::i16x8>()?;
            Ok(V128Const::I16x8([
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
            ]))
        } else if l.peek::<kw::i32x4>() {
            parser.parse::<kw::i32x4>()?;
            Ok(V128Const::I32x4([
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
            ]))
        } else if l.peek::<kw::i64x2>() {
            parser.parse::<kw::i64x2>()?;
            Ok(V128Const::I64x2([parser.parse()?, parser.parse()?]))
        } else if l.peek::<kw::f32x4>() {
            parser.parse::<kw::f32x4>()?;
            Ok(V128Const::F32x4([
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
            ]))
        } else if l.peek::<kw::f64x2>() {
            parser.parse::<kw::f64x2>()?;
            Ok(V128Const::F64x2([parser.parse()?, parser.parse()?]))
        } else {
            Err(l.error())
        }
    }
}

/// Lanes being shuffled in the `v8x16.shuffle` instruction
#[derive(Debug)]
pub struct V8x16Shuffle {
    #[allow(missing_docs)]
    pub lanes: [u8; 16],
}

impl<'a> Parse<'a> for V8x16Shuffle {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        Ok(V8x16Shuffle {
            lanes: [
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
                parser.parse()?,
            ],
        })
    }
}

/// Payload of the `select` instructions
#[derive(Debug)]
pub struct SelectTypes<'a> {
    #[allow(missing_docs)]
    pub tys: Vec<ast::ValType<'a>>,
}

impl<'a> Parse<'a> for SelectTypes<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut tys = Vec::new();
        while parser.peek2::<kw::result>() {
            parser.parens(|p| {
                p.parse::<kw::result>()?;
                while !p.is_empty() {
                    tys.push(p.parse()?);
                }
                Ok(())
            })?;
        }
        Ok(SelectTypes { tys })
    }
}

/// Payload of the `br_on_exn` instruction
#[derive(Debug)]
#[allow(missing_docs)]
pub struct BrOnExn<'a> {
    pub label: ast::Index<'a>,
    pub exn: ast::Index<'a>,
}

impl<'a> Parse<'a> for BrOnExn<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let label = parser.parse()?;
        let exn = parser.parse()?;
        Ok(BrOnExn { label, exn })
    }
}
