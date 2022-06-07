use crate::core::*;
use crate::kw;
use crate::parser::{Parse, Parser, Result};
use crate::token::{Id, NameAnnotation, Span};

/// A WebAssembly global in a module
#[derive(Debug)]
pub struct Global<'a> {
    /// Where this `global` was defined.
    pub span: Span,
    /// An optional name to reference this global by
    pub id: Option<Id<'a>>,
    /// An optional name for this function stored in the custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// If present, inline export annotations which indicate names this
    /// definition should be exported under.
    pub exports: InlineExport<'a>,
    /// The type of this global, both its value type and whether it's mutable.
    pub ty: GlobalType<'a>,
    /// What kind of global this defined as.
    pub kind: GlobalKind<'a>,
}

/// Different kinds of globals that can be defined in a module.
#[derive(Debug)]
pub enum GlobalKind<'a> {
    /// A global which is actually defined as an import, such as:
    ///
    /// ```text
    /// (global i32 (import "foo" "bar"))
    /// ```
    Import(InlineImport<'a>),

    /// A global defined inline in the module itself
    Inline(Expression<'a>),
}

impl<'a> Parse<'a> for Global<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::global>()?.0;
        let id = parser.parse()?;
        let name = parser.parse()?;
        let exports = parser.parse()?;

        let (ty, kind) = if let Some(import) = parser.parse()? {
            (parser.parse()?, GlobalKind::Import(import))
        } else {
            (parser.parse()?, GlobalKind::Inline(parser.parse()?))
        };
        Ok(Global {
            span,
            id,
            name,
            exports,
            ty,
            kind,
        })
    }
}
