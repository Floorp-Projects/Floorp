use crate::component::*;
use crate::kw;
use crate::parser::{Cursor, Parse, Parser, Peek, Result};
use crate::token::{Id, NameAnnotation, Span};

/// An `import` statement and entry in a WebAssembly component.
#[derive(Debug)]
pub struct ComponentImport<'a> {
    /// Where this `import` was defined
    pub span: Span,
    /// The name of the item to import.
    pub name: &'a str,
    /// The item that's being imported.
    pub item: ItemSig<'a>,
}

impl<'a> Parse<'a> for ComponentImport<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::import>()?.0;
        let name = parser.parse()?;
        let item = parser.parens(|p| p.parse())?;
        Ok(ComponentImport { span, name, item })
    }
}

#[derive(Debug)]
#[allow(missing_docs)]
pub struct ItemSig<'a> {
    /// Where this item is defined in the source.
    pub span: Span,
    /// An optional identifier used during name resolution to refer to this item
    /// from the rest of the component.
    pub id: Option<Id<'a>>,
    /// An optional name which, for functions, will be stored in the
    /// custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// What kind of item this is.
    pub kind: ItemKind<'a>,
}

#[derive(Debug)]
#[allow(missing_docs)]
pub enum ItemKind<'a> {
    Component(ComponentTypeUse<'a, ComponentType<'a>>),
    Module(ComponentTypeUse<'a, ModuleType<'a>>),
    Instance(ComponentTypeUse<'a, InstanceType<'a>>),
    Value(ComponentTypeUse<'a, ValueType<'a>>),
    Func(ComponentTypeUse<'a, ComponentFunctionType<'a>>),
}

impl<'a> Parse<'a> for ItemSig<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        let (span, parse_kind): (_, fn(Parser<'a>) -> Result<ItemKind>) =
            if l.peek::<kw::component>() {
                let span = parser.parse::<kw::component>()?.0;
                (span, |parser| Ok(ItemKind::Component(parser.parse()?)))
            } else if l.peek::<kw::module>() {
                let span = parser.parse::<kw::module>()?.0;
                (span, |parser| Ok(ItemKind::Module(parser.parse()?)))
            } else if l.peek::<kw::instance>() {
                let span = parser.parse::<kw::instance>()?.0;
                (span, |parser| Ok(ItemKind::Instance(parser.parse()?)))
            } else if l.peek::<kw::func>() {
                let span = parser.parse::<kw::func>()?.0;
                (span, |parser| Ok(ItemKind::Func(parser.parse()?)))
            } else if l.peek::<kw::value>() {
                let span = parser.parse::<kw::value>()?.0;
                (span, |parser| Ok(ItemKind::Value(parser.parse()?)))
            } else {
                return Err(l.error());
            };
        Ok(ItemSig {
            span,
            id: parser.parse()?,
            name: parser.parse()?,
            kind: parse_kind(parser)?,
        })
    }
}

/// A listing of a inline `(import "foo")` statement.
///
/// This is the same as `core::InlineImport` except only one string import is
/// required.
#[derive(Debug, Copy, Clone)]
#[allow(missing_docs)]
pub struct InlineImport<'a> {
    pub name: &'a str,
}

impl<'a> Parse<'a> for InlineImport<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parens(|p| {
            p.parse::<kw::import>()?;
            Ok(InlineImport { name: p.parse()? })
        })
    }
}

impl Peek for InlineImport<'_> {
    fn peek(cursor: Cursor<'_>) -> bool {
        let cursor = match cursor.lparen() {
            Some(cursor) => cursor,
            None => return false,
        };
        let cursor = match cursor.keyword() {
            Some(("import", cursor)) => cursor,
            _ => return false,
        };
        let cursor = match cursor.string() {
            Some((_, cursor)) => cursor,
            None => return false,
        };
        cursor.rparen().is_some()
    }

    fn display() -> &'static str {
        "inline import"
    }
}
