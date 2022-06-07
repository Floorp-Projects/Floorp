use crate::component::*;
use crate::kw;
use crate::parser::{Parse, Parser, Result};
use crate::token::{Id, NameAnnotation, Span};

/// A definition of a type.
///
/// typeexpr          ::= <deftype>
///                     | <intertype>
#[derive(Debug)]
pub enum ComponentTypeDef<'a> {
    /// The type of an entity.
    DefType(DefType<'a>),
    /// The type of a value.
    InterType(InterType<'a>),
}

/// The type of an exported item from an component or instance.
#[derive(Debug)]
pub struct ComponentExportType<'a> {
    /// Where this export was defined.
    pub span: Span,
    /// The name of this export.
    pub name: &'a str,
    /// The signature of the item that's exported.
    pub item: ItemSig<'a>,
}

impl<'a> Parse<'a> for ComponentExportType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::export>()?.0;
        let name = parser.parse()?;
        let item = parser.parens(|p| p.parse())?;
        Ok(ComponentExportType { span, name, item })
    }
}
/// A type declaration in a component.
///
/// type              ::= (type <id>? <typeexpr>)
#[derive(Debug)]
pub struct TypeField<'a> {
    /// Where this type was defined.
    pub span: Span,
    /// An optional identifer to refer to this `type` by as part of name
    /// resolution.
    pub id: Option<Id<'a>>,
    /// An optional name for this function stored in the custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// The type that we're declaring.
    pub def: ComponentTypeDef<'a>,
}

impl<'a> Parse<'a> for TypeField<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::r#type>()?.0;
        let id = parser.parse()?;
        let name = parser.parse()?;
        let def = if parser.peek2::<DefTypeKind>() {
            ComponentTypeDef::DefType(parser.parens(|p| p.parse())?)
        } else {
            ComponentTypeDef::InterType(parser.parse()?)
        };
        Ok(TypeField {
            span,
            id,
            name,
            def,
        })
    }
}
/// A reference to a type defined in this component.
///
/// This is the same as `TypeUse`, but accepts `$T` as shorthand for
/// `(type $T)`.
#[derive(Debug, Clone)]
pub enum ComponentTypeUse<'a, T> {
    /// The type that we're referencing.
    Ref(ItemRef<'a, kw::r#type>),
    /// The inline type.
    Inline(T),
}

impl<'a, T: Parse<'a>> Parse<'a> for ComponentTypeUse<'a, T> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek::<ItemRef<'a, kw::r#type>>() {
            Ok(ComponentTypeUse::Ref(parser.parse()?))
        } else {
            Ok(ComponentTypeUse::Inline(parser.parse()?))
        }
    }
}
