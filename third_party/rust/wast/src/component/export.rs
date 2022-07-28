use super::ItemRef;
use crate::kw;
use crate::parser::{Cursor, Parse, Parser, Peek, Result};
use crate::token::{Id, Index, Span};

/// An entry in a WebAssembly component's export section.
#[derive(Debug)]
pub struct ComponentExport<'a> {
    /// Where this export was defined.
    pub span: Span,
    /// The name of this export from the component.
    pub name: &'a str,
    /// The kind of export.
    pub kind: ComponentExportKind<'a>,
}

impl<'a> Parse<'a> for ComponentExport<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::export>()?.0;
        let name = parser.parse()?;
        let kind = parser.parse()?;
        Ok(ComponentExport { span, name, kind })
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
}

impl<'a> Parse<'a> for ComponentExportKind<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parens(|parser| {
            let mut l = parser.lookahead1();
            if l.peek::<kw::core>() {
                // Remove core prefix
                parser.parse::<kw::core>()?;
                Ok(Self::CoreModule(parser.parse()?))
            } else if l.peek::<kw::func>() {
                Ok(Self::Func(parser.parse()?))
            } else if l.peek::<kw::value>() {
                Ok(Self::Value(parser.parse()?))
            } else if l.peek::<kw::r#type>() {
                Ok(Self::Type(parser.parse()?))
            } else if l.peek::<kw::component>() {
                Ok(Self::Component(parser.parse()?))
            } else if l.peek::<kw::instance>() {
                Ok(Self::Instance(parser.parse()?))
            } else {
                Err(l.error())
            }
        })
    }
}

impl Peek for ComponentExportKind<'_> {
    fn peek(cursor: Cursor) -> bool {
        let cursor = match cursor.lparen() {
            Some(c) => c,
            None => return false,
        };

        let cursor = match cursor.keyword() {
            Some(("core", c)) => match c.keyword() {
                Some(("module", c)) => c,
                _ => return false,
            },
            Some(("func", c))
            | Some(("value", c))
            | Some(("type", c))
            | Some(("component", c))
            | Some(("instance", c)) => c,
            _ => return false,
        };

        Index::peek(cursor)
    }

    fn display() -> &'static str {
        "component export"
    }
}
