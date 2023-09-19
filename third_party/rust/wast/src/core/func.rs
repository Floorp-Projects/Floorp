use crate::core::*;
use crate::kw;
use crate::parser::{Parse, Parser, Result};
use crate::token::{Id, NameAnnotation, Span};

/// A WebAssembly function to be inserted into a module.
///
/// This is a member of both the function and code sections.
#[derive(Debug)]
pub struct Func<'a> {
    /// Where this `func` was defined.
    pub span: Span,
    /// An identifier that this function is resolved with (optionally) for name
    /// resolution.
    pub id: Option<Id<'a>>,
    /// An optional name for this function stored in the custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// If present, inline export annotations which indicate names this
    /// definition should be exported under.
    pub exports: InlineExport<'a>,
    /// What kind of function this is, be it an inline-defined or imported
    /// function.
    pub kind: FuncKind<'a>,
    /// The type that this function will have.
    pub ty: TypeUse<'a, FunctionType<'a>>,
}

/// Possible ways to define a function in the text format.
#[derive(Debug)]
pub enum FuncKind<'a> {
    /// A function which is actually defined as an import, such as:
    ///
    /// ```text
    /// (func (type 3) (import "foo" "bar"))
    /// ```
    Import(InlineImport<'a>),

    /// Almost all functions, those defined inline in a wasm module.
    Inline {
        /// The list of locals, if any, for this function.
        locals: Box<[Local<'a>]>,

        /// The instructions of the function.
        expression: Expression<'a>,
    },
}

impl<'a> Parse<'a> for Func<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::func>()?.0;
        let id = parser.parse()?;
        let name = parser.parse()?;
        let exports = parser.parse()?;

        let (ty, kind) = if let Some(import) = parser.parse()? {
            (parser.parse()?, FuncKind::Import(import))
        } else {
            let ty = parser.parse()?;
            let locals = Local::parse_remainder(parser)?.into();
            (
                ty,
                FuncKind::Inline {
                    locals,
                    expression: parser.parse()?,
                },
            )
        };

        Ok(Func {
            span,
            id,
            name,
            exports,
            ty,
            kind,
        })
    }
}

/// A local for a `func` or `let` instruction.
///
/// Each local has an optional identifier for name resolution, an optional name
/// for the custom `name` section, and a value type.
#[derive(Debug)]
pub struct Local<'a> {
    /// An identifier that this local is resolved with (optionally) for name
    /// resolution.
    pub id: Option<Id<'a>>,
    /// An optional name for this local stored in the custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// The value type of this local.
    pub ty: ValType<'a>,
}

/// Parser for `local` instruction.
///
/// A single `local` instruction can generate multiple locals, hence this parser
pub struct LocalParser<'a> {
    /// All the locals associated with this `local` instruction.
    pub locals: Vec<Local<'a>>,
}

impl<'a> Parse<'a> for LocalParser<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut locals = Vec::new();
        parser.parse::<kw::local>()?;
        if !parser.is_empty() {
            let id: Option<_> = parser.parse()?;
            let name: Option<_> = parser.parse()?;
            let ty = parser.parse()?;
            let parse_more = id.is_none() && name.is_none();
            locals.push(Local { id, name, ty });
            while parse_more && !parser.is_empty() {
                locals.push(Local {
                    id: None,
                    name: None,
                    ty: parser.parse()?,
                });
            }
        }
        Ok(LocalParser { locals })
    }
}

impl<'a> Local<'a> {
    pub(crate) fn parse_remainder(parser: Parser<'a>) -> Result<Vec<Local<'a>>> {
        let mut locals = Vec::new();
        while parser.peek2::<kw::local>()? {
            parser.parens(|p| {
                locals.extend(p.parse::<LocalParser>()?.locals);
                Ok(())
            })?;
        }
        Ok(locals)
    }
}
