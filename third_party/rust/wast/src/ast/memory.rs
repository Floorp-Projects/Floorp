use crate::ast::{self, kw};
use crate::parser::{Parse, Parser, Result};

/// A defined WebAssembly memory instance inside of a module.
#[derive(Debug)]
pub struct Memory<'a> {
    /// Where this `memory` was defined
    pub span: ast::Span,
    /// An optional name to refer to this memory by.
    pub id: Option<ast::Id<'a>>,
    /// If present, inline export annotations which indicate names this
    /// definition should be exported under.
    pub exports: ast::InlineExport<'a>,
    /// How this memory is defined in the module.
    pub kind: MemoryKind<'a>,
}

/// Different syntactical ways a memory can be defined in a module.
#[derive(Debug)]
pub enum MemoryKind<'a> {
    /// This memory is actually an inlined import definition.
    #[allow(missing_docs)]
    Import {
        module: &'a str,
        field: &'a str,
        ty: ast::MemoryType,
    },

    /// A typical memory definition which simply says the limits of the memory
    Normal(ast::MemoryType),

    /// The data of this memory, starting from 0, explicitly listed
    Inline(Vec<&'a [u8]>),
}

impl<'a> Parse<'a> for Memory<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::memory>()?.0;
        let id = parser.parse()?;
        let exports = parser.parse()?;

        // Afterwards figure out which style this is, either:
        //
        //  *   `(data ...)`
        //  *   `(import "a" "b") limits`
        //  *   `limits`
        let mut l = parser.lookahead1();
        let kind = if l.peek::<ast::LParen>() {
            enum Which<'a, T> {
                Inline(Vec<T>),
                Import(&'a str, &'a str),
            }
            let result = parser.parens(|parser| {
                let mut l = parser.lookahead1();
                if l.peek::<kw::data>() {
                    parser.parse::<kw::data>()?;
                    let mut data = Vec::new();
                    while !parser.is_empty() {
                        data.push(parser.parse()?);
                    }
                    Ok(Which::Inline(data))
                } else if l.peek::<kw::import>() {
                    parser.parse::<kw::import>()?;
                    Ok(Which::Import(parser.parse()?, parser.parse()?))
                } else {
                    Err(l.error())
                }
            })?;
            match result {
                Which::Inline(data) => MemoryKind::Inline(data),
                Which::Import(module, field) => MemoryKind::Import {
                    module,
                    field,
                    ty: parser.parse()?,
                },
            }
        } else if l.peek::<u32>() {
            MemoryKind::Normal(parser.parse()?)
        } else {
            return Err(l.error());
        };
        Ok(Memory {
            span,
            id,
            exports,
            kind,
        })
    }
}

/// A `data` directive in a WebAssembly module.
#[derive(Debug)]
pub struct Data<'a> {
    /// Where this `data` was defined
    pub span: ast::Span,

    /// The optional name of this data segment
    pub id: Option<ast::Id<'a>>,

    /// Whether this data segment is passive or active
    pub kind: DataKind<'a>,

    /// Bytes for this `Data` segment, viewed as the concatenation of all the
    /// contained slices.
    pub data: Vec<&'a [u8]>,
}

/// Different kinds of data segments, either passive or active.
#[derive(Debug)]
pub enum DataKind<'a> {
    /// A passive data segment which isn't associated with a memory and is
    /// referenced from various instructions.
    Passive,

    /// An active data segment which is associated and loaded into a particular
    /// memory on module instantiation.
    Active {
        /// The memory that this `Data` will be associated with.
        memory: ast::Index<'a>,

        /// Initial offset to load this data segment at
        offset: ast::Expression<'a>,
    },
}

impl<'a> Parse<'a> for Data<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::data>()?.0;
        let id = parser.parse()?;

        // The `passive` keyword is mentioned in the current spec but isn't
        // mentioned in `wabt` tests, so consider it optional for now
        let kind = if parser.peek::<kw::passive>() {
            parser.parse::<kw::passive>()?;
            DataKind::Passive

        // If data directly follows then assume this is a passive segment
        } else if parser.peek::<&[u8]>() {
            DataKind::Passive

        // ... and otherwise we must be attached to a particular memory as well
        // as having an initialization offset.
        } else {
            let memory = if parser.peek2::<kw::memory>() {
                Some(parser.parens(|p| {
                    p.parse::<kw::memory>()?;
                    p.parse()
                })?)
            } else {
                parser.parse()?
            };
            let offset = parser.parens(|parser| {
                if parser.peek::<kw::offset>() {
                    parser.parse::<kw::offset>()?;
                }
                parser.parse()
            })?;
            DataKind::Active {
                memory: memory.unwrap_or(ast::Index::Num(0)),
                offset,
            }
        };

        let mut data = Vec::new();
        while !parser.is_empty() {
            data.push(parser.parse()?);
        }
        Ok(Data {
            span,
            id,
            kind,
            data,
        })
    }
}
