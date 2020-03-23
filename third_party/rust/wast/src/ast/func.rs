use crate::ast::{self, kw};
use crate::parser::{Parse, Parser, Result};

/// A WebAssembly function to be inserted into a module.
///
/// This is a member of both the function and code sections.
#[derive(Debug)]
pub struct Func<'a> {
    /// Where this `func` was defined.
    pub span: ast::Span,
    /// An identifier that this function is resolved with (optionally) for name
    /// resolution.
    pub id: Option<ast::Id<'a>>,
    /// An optional name for this function stored in the custom `name` section.
    pub name: Option<ast::NameAnnotation<'a>>,
    /// If present, inline export annotations which indicate names this
    /// definition should be exported under.
    pub exports: ast::InlineExport<'a>,
    /// What kind of function this is, be it an inline-defined or imported
    /// function.
    pub kind: FuncKind<'a>,
    /// The type that this function will have.
    pub ty: ast::TypeUse<'a>,
}

/// Possible ways to define a function in the text format.
#[derive(Debug)]
pub enum FuncKind<'a> {
    /// A function which is actually defined as an import, such as:
    ///
    /// ```text
    /// (func (type 3) (import "foo" "bar"))
    /// ```
    Import {
        /// The module that this function is imported from
        module: &'a str,
        /// The module field name this function is imported from
        field: &'a str,
    },

    /// Almost all functions, those defined inline in a wasm module.
    Inline {
        /// The list of locals, if any, for this function. Each local has an
        /// optional identifier for name resolution and name for the custom
        /// `name` section associated with it.
        locals: Vec<(Option<ast::Id<'a>>, Option<ast::NameAnnotation<'a>>, ast::ValType<'a>)>,

        /// The instructions of the function.
        expression: ast::Expression<'a>,
    },
}

impl<'a> Parse<'a> for Func<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::func>()?.0;
        let id = parser.parse()?;
        let name = parser.parse()?;
        let exports = parser.parse()?;

        let (ty, kind) = if parser.peek2::<kw::import>() {
            let (module, field) = parser.parens(|p| {
                p.parse::<kw::import>()?;
                Ok((p.parse()?, p.parse()?))
            })?;
            (parser.parse()?, FuncKind::Import { module, field })
        } else {
            let ty = parser.parse()?;
            let mut locals = Vec::new();
            while parser.peek2::<kw::local>() {
                parser.parens(|p| {
                    p.parse::<kw::local>()?;
                    if p.is_empty() {
                        return Ok(());
                    }
                    let id: Option<_> = p.parse()?;
                    let name: Option<_> = p.parse()?;
                    let ty = p.parse()?;
                    let parse_more = id.is_none() && name.is_none();
                    locals.push((id, name, ty));
                    while parse_more && !p.is_empty() {
                        locals.push((None, None, p.parse()?));
                    }
                    Ok(())
                })?;
            }
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
