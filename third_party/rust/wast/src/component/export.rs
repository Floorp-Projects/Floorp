use super::{ComponentExternName, ItemRef, ItemSigNoName};
use crate::kw;
use crate::parser::{Cursor, Parse, Parser, Peek, Result};
use crate::token::{Id, Index, NameAnnotation, Span};

/// An entry in a WebAssembly component's export section.
#[derive(Debug)]
pub struct ComponentExport<'a> {
    /// Where this export was defined.
    pub span: Span,
    /// Optional identifier bound to this export.
    pub id: Option<Id<'a>>,
    /// An optional name for this instance stored in the custom `name` section.
    pub debug_name: Option<NameAnnotation<'a>>,
    /// The name of this export from the component.
    pub name: ComponentExternName<'a>,
    /// The kind of export.
    pub kind: ComponentExportKind<'a>,
    /// The kind of export.
    pub ty: Option<ItemSigNoName<'a>>,
}

impl<'a> Parse<'a> for ComponentExport<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::export>()?.0;
        let id = parser.parse()?;
        let debug_name = parser.parse()?;
        let name = parser.parse()?;
        let kind = parser.parse()?;
        let ty = if !parser.is_empty() {
            Some(parser.parens(|p| p.parse())?)
        } else {
            None
        };
        Ok(ComponentExport {
            span,
            id,
            debug_name,
            name,
            kind,
            ty,
        })
    }
}

impl<'a> Parse<'a> for Vec<ComponentExport<'a>> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut exports = Vec::new();
        while !parser.is_empty() {
            exports.push(parser.parens(|parser| parser.parse())?);
        }
        Ok(exports)
    }
}

/// The kind of exported item.
#[derive(Debug)]
pub enum ComponentExportKind<'a> {
    /// The export is a core module.
    ///
    /// Note this isn't a core item ref as currently only
    /// components can export core modules.
    CoreModule(ItemRef<'a, kw::module>),
    /// The export is a function.
    Func(ItemRef<'a, kw::func>),
    /// The export is a value.
    Value(ItemRef<'a, kw::value>),
    /// The export is a type.
    Type(ItemRef<'a, kw::r#type>),
    /// The export is a component.
    Component(ItemRef<'a, kw::component>),
    /// The export is an instance.
    Instance(ItemRef<'a, kw::instance>),
}

impl<'a> ComponentExportKind<'a> {
    pub(crate) fn module(span: Span, id: Id<'a>) -> Self {
        Self::CoreModule(ItemRef {
            kind: kw::module(span),
            idx: Index::Id(id),
            export_names: Default::default(),
        })
    }

    pub(crate) fn component(span: Span, id: Id<'a>) -> Self {
        Self::Component(ItemRef {
            kind: kw::component(span),
            idx: Index::Id(id),
            export_names: Default::default(),
        })
    }

    pub(crate) fn instance(span: Span, id: Id<'a>) -> Self {
        Self::Instance(ItemRef {
            kind: kw::instance(span),
            idx: Index::Id(id),
            export_names: Default::default(),
        })
    }

    pub(crate) fn func(span: Span, id: Id<'a>) -> Self {
        Self::Func(ItemRef {
            kind: kw::func(span),
            idx: Index::Id(id),
            export_names: Default::default(),
        })
    }

    pub(crate) fn ty(span: Span, id: Id<'a>) -> Self {
        Self::Type(ItemRef {
            kind: kw::r#type(span),
            idx: Index::Id(id),
            export_names: Default::default(),
        })
    }
}

impl<'a> Parse<'a> for ComponentExportKind<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parens(|parser| {
            let mut l = parser.lookahead1();
            if l.peek::<kw::core>()? {
                // Remove core prefix
                parser.parse::<kw::core>()?;
                Ok(Self::CoreModule(parser.parse()?))
            } else if l.peek::<kw::func>()? {
                Ok(Self::Func(parser.parse()?))
            } else if l.peek::<kw::value>()? {
                Ok(Self::Value(parser.parse()?))
            } else if l.peek::<kw::r#type>()? {
                Ok(Self::Type(parser.parse()?))
            } else if l.peek::<kw::component>()? {
                Ok(Self::Component(parser.parse()?))
            } else if l.peek::<kw::instance>()? {
                Ok(Self::Instance(parser.parse()?))
            } else {
                Err(l.error())
            }
        })
    }
}

impl Peek for ComponentExportKind<'_> {
    fn peek(cursor: Cursor) -> Result<bool> {
        let cursor = match cursor.lparen()? {
            Some(c) => c,
            None => return Ok(false),
        };

        let cursor = match cursor.keyword()? {
            Some(("core", c)) => match c.keyword()? {
                Some(("module", c)) => c,
                _ => return Ok(false),
            },
            Some(("func", c))
            | Some(("value", c))
            | Some(("type", c))
            | Some(("component", c))
            | Some(("instance", c)) => c,
            _ => return Ok(false),
        };

        Index::peek(cursor)
    }

    fn display() -> &'static str {
        "component export"
    }
}

/// A listing of inline `(export "foo" <url>)` statements on a WebAssembly
/// component item in its textual format.
#[derive(Debug, Default)]
pub struct InlineExport<'a> {
    /// The extra names to export an item as, if any.
    pub names: Vec<ComponentExternName<'a>>,
}

impl<'a> Parse<'a> for InlineExport<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut names = Vec::new();
        while parser.peek::<Self>()? {
            names.push(parser.parens(|p| {
                p.parse::<kw::export>()?;
                p.parse()
            })?);
        }
        Ok(InlineExport { names })
    }
}

impl Peek for InlineExport<'_> {
    fn peek(cursor: Cursor<'_>) -> Result<bool> {
        let cursor = match cursor.lparen()? {
            Some(cursor) => cursor,
            None => return Ok(false),
        };
        let cursor = match cursor.keyword()? {
            Some(("export", cursor)) => cursor,
            _ => return Ok(false),
        };

        // (export "foo")
        if let Some((_, cursor)) = cursor.string()? {
            return Ok(cursor.rparen()?.is_some());
        }

        // (export (interface "foo"))
        let cursor = match cursor.lparen()? {
            Some(cursor) => cursor,
            None => return Ok(false),
        };
        let cursor = match cursor.keyword()? {
            Some(("interface", cursor)) => cursor,
            _ => return Ok(false),
        };
        let cursor = match cursor.string()? {
            Some((_, cursor)) => cursor,
            _ => return Ok(false),
        };
        let cursor = match cursor.rparen()? {
            Some(cursor) => cursor,
            _ => return Ok(false),
        };
        Ok(cursor.rparen()?.is_some())
    }

    fn display() -> &'static str {
        "inline export"
    }
}
