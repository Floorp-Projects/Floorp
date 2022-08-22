use crate::component::*;
use crate::kw;
use crate::parser::{Cursor, Parse, Parser, Peek, Result};
use crate::token::Index;
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

/// An item signature for imported items.
#[derive(Debug)]
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
    pub kind: ItemSigKind<'a>,
}

impl<'a> Parse<'a> for ItemSig<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        let (span, parse_kind): (_, fn(Parser<'a>) -> Result<ItemSigKind>) = if l.peek::<kw::core>()
        {
            let span = parser.parse::<kw::core>()?.0;
            parser.parse::<kw::module>()?;
            (span, |parser| Ok(ItemSigKind::CoreModule(parser.parse()?)))
        } else if l.peek::<kw::func>() {
            let span = parser.parse::<kw::func>()?.0;
            (span, |parser| Ok(ItemSigKind::Func(parser.parse()?)))
        } else if l.peek::<kw::component>() {
            let span = parser.parse::<kw::component>()?.0;
            (span, |parser| Ok(ItemSigKind::Component(parser.parse()?)))
        } else if l.peek::<kw::instance>() {
            let span = parser.parse::<kw::instance>()?.0;
            (span, |parser| Ok(ItemSigKind::Instance(parser.parse()?)))
        } else if l.peek::<kw::value>() {
            let span = parser.parse::<kw::value>()?.0;
            (span, |parser| Ok(ItemSigKind::Value(parser.parse()?)))
        } else if l.peek::<kw::r#type>() {
            let span = parser.parse::<kw::r#type>()?.0;
            (span, |parser| {
                Ok(ItemSigKind::Type(parser.parens(|parser| parser.parse())?))
            })
        } else {
            return Err(l.error());
        };
        Ok(Self {
            span,
            id: parser.parse()?,
            name: parser.parse()?,
            kind: parse_kind(parser)?,
        })
    }
}

/// The kind of signatures for imported items.
#[derive(Debug)]
pub enum ItemSigKind<'a> {
    /// The item signature is for a core module.
    CoreModule(CoreTypeUse<'a, ModuleType<'a>>),
    /// The item signature is for a function.
    Func(ComponentTypeUse<'a, ComponentFunctionType<'a>>),
    /// The item signature is for a component.
    Component(ComponentTypeUse<'a, ComponentType<'a>>),
    /// The item signature is for an instance.
    Instance(ComponentTypeUse<'a, InstanceType<'a>>),
    /// The item signature is for a value.
    Value(ComponentValTypeUse<'a>),
    /// The item signature is for a type.
    Type(TypeBounds<'a>),
}

/// Represents the bounds applied to types being imported.
#[derive(Debug)]
pub enum TypeBounds<'a> {
    /// The equality type bounds.
    Eq(Index<'a>),
}

impl<'a> Parse<'a> for TypeBounds<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        // Currently this is the only supported type bounds.
        parser.parse::<kw::eq>()?;
        Ok(Self::Eq(parser.parse()?))
    }
}

/// A listing of a inline `(import "foo")` statement.
///
/// This is the same as `core::InlineImport` except only one string import is
/// required.
#[derive(Debug, Copy, Clone)]
pub struct InlineImport<'a> {
    /// The name of the item being imported.
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
