use crate::kw;
use crate::parser::{Cursor, Parse, Parser, Peek, Result};
use crate::token::{Index, Span};

/// A entry in a WebAssembly module's export section.
#[derive(Debug)]
pub struct Export<'a> {
    /// Where this export was defined.
    pub span: Span,
    /// The name of this export from the module.
    pub name: &'a str,
    /// The kind of item being exported.
    pub kind: ExportKind,
    /// What's being exported from the module.
    pub item: Index<'a>,
}

/// Different kinds of elements that can be exported from a WebAssembly module,
/// contained in an [`Export`].
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq)]
#[allow(missing_docs)]
pub enum ExportKind {
    Func,
    Table,
    Memory,
    Global,
    Tag,
}

impl<'a> Parse<'a> for Export<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::export>()?.0;
        let name = parser.parse()?;
        let (kind, item) = parser.parens(|p| Ok((p.parse()?, p.parse()?)))?;
        Ok(Export {
            span,
            name,
            kind,
            item,
        })
    }
}

impl<'a> Parse<'a> for ExportKind {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::func>() {
            parser.parse::<kw::func>()?;
            Ok(ExportKind::Func)
        } else if l.peek::<kw::table>() {
            parser.parse::<kw::table>()?;
            Ok(ExportKind::Table)
        } else if l.peek::<kw::memory>() {
            parser.parse::<kw::memory>()?;
            Ok(ExportKind::Memory)
        } else if l.peek::<kw::global>() {
            parser.parse::<kw::global>()?;
            Ok(ExportKind::Global)
        } else if l.peek::<kw::tag>() {
            parser.parse::<kw::tag>()?;
            Ok(ExportKind::Tag)
        } else {
            Err(l.error())
        }
    }
}

impl Peek for ExportKind {
    fn peek(cursor: Cursor<'_>) -> bool {
        kw::func::peek(cursor)
            || kw::table::peek(cursor)
            || kw::memory::peek(cursor)
            || kw::global::peek(cursor)
            || kw::tag::peek(cursor)
    }
    fn display() -> &'static str {
        "export kind"
    }
}

macro_rules! kw_conversions {
    ($($kw:ident => $kind:ident)*) => ($(
        impl From<kw::$kw> for ExportKind {
            fn from(_: kw::$kw) -> ExportKind {
                ExportKind::$kind
            }
        }

        impl Default for kw::$kw {
            fn default() -> kw::$kw {
                kw::$kw(Span::from_offset(0))
            }
        }
    )*);
}

kw_conversions! {
    func => Func
    table => Table
    global => Global
    tag => Tag
    memory => Memory
}

/// A listing of inline `(export "foo")` statements on a WebAssembly item in
/// its textual format.
#[derive(Debug, Default)]
pub struct InlineExport<'a> {
    /// The extra names to export an item as, if any.
    pub names: Vec<&'a str>,
}

impl<'a> Parse<'a> for InlineExport<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut names = Vec::new();
        while parser.peek::<Self>() {
            names.push(parser.parens(|p| {
                p.parse::<kw::export>()?;
                p.parse::<&str>()
            })?);
        }
        Ok(InlineExport { names })
    }
}

impl Peek for InlineExport<'_> {
    fn peek(cursor: Cursor<'_>) -> bool {
        let cursor = match cursor.lparen() {
            Some(cursor) => cursor,
            None => return false,
        };
        let cursor = match cursor.keyword() {
            Some(("export", cursor)) => cursor,
            _ => return false,
        };
        let cursor = match cursor.string() {
            Some((_, cursor)) => cursor,
            None => return false,
        };
        cursor.rparen().is_some()
    }

    fn display() -> &'static str {
        "inline export"
    }
}
