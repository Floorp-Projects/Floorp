use crate::ast::{self, kw};
use crate::parser::{Parse, Parser, Result};

/// An `import` statement and entry in a WebAssembly module.
#[derive(Debug)]
pub struct Import<'a> {
    /// Where this `import` was defined
    pub span: ast::Span,
    /// The module that this statement is importing from
    pub module: &'a str,
    /// The name of the field in the module this statement imports from.
    pub field: &'a str,
    /// An optional identifier used during name resolution to refer to this item
    /// from the rest of the module.
    pub id: Option<ast::Id<'a>>,
    /// An optional name which, for functions, will be stored in the
    /// custom `name` section.
    pub name: Option<ast::NameAnnotation<'a>>,
    /// What kind of item is being imported.
    pub kind: ImportKind<'a>,
}

/// All possible types of items that can be imported into a wasm module.
#[derive(Debug)]
#[allow(missing_docs)]
pub enum ImportKind<'a> {
    Func(ast::TypeUse<'a>),
    Table(ast::TableType),
    Memory(ast::MemoryType),
    Global(ast::GlobalType<'a>),
    Event(ast::EventType<'a>),
}

impl<'a> Parse<'a> for Import<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::import>()?.0;
        let module = parser.parse()?;
        let field = parser.parse()?;
        let (id, name, kind) = parser.parens(|parser| {
            let mut l = parser.lookahead1();
            if l.peek::<kw::func>() {
                parser.parse::<kw::func>()?;
                Ok((
                    parser.parse()?,
                    parser.parse()?,
                    ImportKind::Func(parser.parse()?),
                ))
            } else if l.peek::<kw::table>() {
                parser.parse::<kw::table>()?;
                Ok((parser.parse()?, None, ImportKind::Table(parser.parse()?)))
            } else if l.peek::<kw::memory>() {
                parser.parse::<kw::memory>()?;
                Ok((parser.parse()?, None, ImportKind::Memory(parser.parse()?)))
            } else if l.peek::<kw::global>() {
                parser.parse::<kw::global>()?;
                Ok((parser.parse()?, None, ImportKind::Global(parser.parse()?)))
            } else if l.peek::<kw::event>() {
                parser.parse::<kw::event>()?;
                Ok((parser.parse()?, None, ImportKind::Event(parser.parse()?)))
            } else {
                Err(l.error())
            }
        })?;
        Ok(Import {
            span,
            module,
            field,
            id,
            kind,
            name,
        })
    }
}
