use crate::core::*;
use crate::kw;
use crate::parser::{Cursor, Parse, Parser, Peek, Result};
use crate::token::{Id, NameAnnotation, Span};

/// An `import` statement and entry in a WebAssembly module.
#[derive(Debug, Clone)]
pub struct Import<'a> {
    /// Where this `import` was defined
    pub span: Span,
    /// The module that this statement is importing from
    pub module: &'a str,
    /// The name of the field in the module this statement imports from.
    pub field: &'a str,
    /// The item that's being imported.
    pub item: ItemSig<'a>,
}

impl<'a> Parse<'a> for Import<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::import>()?.0;
        let module = parser.parse()?;
        let field = parser.parse()?;
        let item = parser.parens(|p| p.parse())?;
        Ok(Import {
            span,
            module,
            field,
            item,
        })
    }
}

#[derive(Debug, Clone)]
#[allow(missing_docs)]
pub struct ItemSig<'a> {
    /// Where this item is defined in the source.
    pub span: Span,
    /// An optional identifier used during name resolution to refer to this item
    /// from the rest of the module.
    pub id: Option<Id<'a>>,
    /// An optional name which, for functions, will be stored in the
    /// custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// What kind of item this is.
    pub kind: ItemKind<'a>,
}

#[derive(Debug, Clone)]
#[allow(missing_docs)]
pub enum ItemKind<'a> {
    Func(TypeUse<'a, FunctionType<'a>>),
    Table(TableType<'a>),
    Memory(MemoryType),
    Global(GlobalType<'a>),
    Tag(TagType<'a>),
}

impl<'a> Parse<'a> for ItemSig<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::func>()? {
            let span = parser.parse::<kw::func>()?.0;
            Ok(ItemSig {
                span,
                id: parser.parse()?,
                name: parser.parse()?,
                kind: ItemKind::Func(parser.parse()?),
            })
        } else if l.peek::<kw::table>()? {
            let span = parser.parse::<kw::table>()?.0;
            Ok(ItemSig {
                span,
                id: parser.parse()?,
                name: None,
                kind: ItemKind::Table(parser.parse()?),
            })
        } else if l.peek::<kw::memory>()? {
            let span = parser.parse::<kw::memory>()?.0;
            Ok(ItemSig {
                span,
                id: parser.parse()?,
                name: None,
                kind: ItemKind::Memory(parser.parse()?),
            })
        } else if l.peek::<kw::global>()? {
            let span = parser.parse::<kw::global>()?.0;
            Ok(ItemSig {
                span,
                id: parser.parse()?,
                name: None,
                kind: ItemKind::Global(parser.parse()?),
            })
        } else if l.peek::<kw::tag>()? {
            let span = parser.parse::<kw::tag>()?.0;
            Ok(ItemSig {
                span,
                id: parser.parse()?,
                name: None,
                kind: ItemKind::Tag(parser.parse()?),
            })
        } else {
            Err(l.error())
        }
    }
}

/// A listing of a inline `(import "foo")` statement.
///
/// Note that when parsing this type it is somewhat unconventional that it
/// parses its own surrounding parentheses. This is typically an optional type,
/// so it's so far been a bit nicer to have the optionality handled through
/// `Peek` rather than `Option<T>`.
#[derive(Debug, Copy, Clone)]
#[allow(missing_docs)]
pub struct InlineImport<'a> {
    pub module: &'a str,
    pub field: &'a str,
}

impl<'a> Parse<'a> for InlineImport<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parens(|p| {
            p.parse::<kw::import>()?;
            Ok(InlineImport {
                module: p.parse()?,
                field: p.parse()?,
            })
        })
    }
}

impl Peek for InlineImport<'_> {
    fn peek(cursor: Cursor<'_>) -> Result<bool> {
        let cursor = match cursor.lparen()? {
            Some(cursor) => cursor,
            None => return Ok(false),
        };
        let cursor = match cursor.keyword()? {
            Some(("import", cursor)) => cursor,
            _ => return Ok(false),
        };
        let cursor = match cursor.string()? {
            Some((_, cursor)) => cursor,
            None => return Ok(false),
        };
        let cursor = match cursor.string()? {
            Some((_, cursor)) => cursor,
            None => return Ok(false),
        };

        Ok(cursor.rparen()?.is_some())
    }

    fn display() -> &'static str {
        "inline import"
    }
}
