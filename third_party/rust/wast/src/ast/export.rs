use crate::ast::{self, kw};
use crate::parser::{Parse, Parser, Result};

/// A entry in a WebAssembly module's export section.
#[derive(Debug)]
pub struct Export<'a> {
    /// The name of this export from the module.
    pub name: &'a str,
    /// What's being exported from the module.
    pub kind: ExportKind<'a>,
}

/// Different kinds of elements that can be exported from a WebAssembly module,
/// contained in an [`Export`].
#[derive(Debug)]
#[allow(missing_docs)]
pub enum ExportKind<'a> {
    Func(ast::Index<'a>),
    Table(ast::Index<'a>),
    Memory(ast::Index<'a>),
    Global(ast::Index<'a>),
    Event(ast::Index<'a>),
}

impl<'a> Parse<'a> for Export<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::export>()?;
        let name = parser.parse()?;
        let kind = parser.parens(|parser| {
            let mut l = parser.lookahead1();
            if l.peek::<kw::func>() {
                parser.parse::<kw::func>()?;
                Ok(ExportKind::Func(parser.parse()?))
            } else if l.peek::<kw::table>() {
                parser.parse::<kw::table>()?;
                Ok(ExportKind::Table(parser.parse()?))
            } else if l.peek::<kw::memory>() {
                parser.parse::<kw::memory>()?;
                Ok(ExportKind::Memory(parser.parse()?))
            } else if l.peek::<kw::global>() {
                parser.parse::<kw::global>()?;
                Ok(ExportKind::Global(parser.parse()?))
            } else if l.peek::<kw::event>() {
                parser.parse::<kw::event>()?;
                Ok(ExportKind::Event(parser.parse()?))
            } else {
                Err(l.error())
            }
        })?;
        Ok(Export { name, kind })
    }
}

/// A listing of inline `(export "foo")` statements on a WebAssembly item in
/// its textual format.
#[derive(Debug)]
pub struct InlineExport<'a> {
    /// The extra names to export an item as, if any.
    pub names: Vec<&'a str>,
}

impl<'a> Parse<'a> for InlineExport<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut names = Vec::new();
        while parser.peek2::<kw::export>() {
            names.push(parser.parens(|p| {
                p.parse::<kw::export>()?;
                p.parse::<&str>()
            })?);
        }
        Ok(InlineExport { names })
    }
}
