use crate::component::*;
use crate::core;
use crate::kw;
use crate::parser::Lookahead1;
use crate::parser::{Parse, Parser, Result};
use crate::token::Index;
use crate::token::LParen;
use crate::token::{Id, NameAnnotation, Span};

/// A core type declaration.
#[derive(Debug)]
pub struct CoreType<'a> {
    /// Where this type was defined.
    pub span: Span,
    /// An optional identifier to refer to this `core type` by as part of name
    /// resolution.
    pub id: Option<Id<'a>>,
    /// An optional name for this type stored in the custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// The core type's definition.
    pub def: CoreTypeDef<'a>,
}

impl<'a> Parse<'a> for CoreType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::core>()?.0;
        parser.parse::<kw::r#type>()?;
        let id = parser.parse()?;
        let name = parser.parse()?;
        let def = parser.parens(|p| p.parse())?;

        Ok(Self {
            span,
            id,
            name,
            def,
        })
    }
}

/// Represents a core type definition.
///
/// In the future this may be removed when module types are a part of
/// a core module.
#[derive(Debug)]
pub enum CoreTypeDef<'a> {
    /// The type definition is one of the core types.
    Def(core::TypeDef<'a>),
    /// The type definition is a module type.
    Module(ModuleType<'a>),
}

impl<'a> Parse<'a> for CoreTypeDef<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek::<kw::module>() {
            parser.parse::<kw::module>()?;
            Ok(Self::Module(parser.parse()?))
        } else {
            Ok(Self::Def(parser.parse()?))
        }
    }
}

/// A type definition for a core module.
#[derive(Debug)]
pub struct ModuleType<'a> {
    /// The declarations of the module type.
    pub decls: Vec<ModuleTypeDecl<'a>>,
}

impl<'a> Parse<'a> for ModuleType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.depth_check()?;
        Ok(Self {
            decls: parser.parse()?,
        })
    }
}

/// The declarations of a [`ModuleType`].
#[derive(Debug)]
pub enum ModuleTypeDecl<'a> {
    /// A core type.
    Type(core::Type<'a>),
    /// An alias local to the module type.
    Alias(CoreAlias<'a>),
    /// An import.
    Import(core::Import<'a>),
    /// An export.
    Export(&'a str, core::ItemSig<'a>),
}

impl<'a> Parse<'a> for ModuleTypeDecl<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::r#type>() {
            Ok(Self::Type(parser.parse()?))
        } else if l.peek::<kw::alias>() {
            let span = parser.parse::<kw::alias>()?.0;
            Ok(Self::Alias(CoreAlias::parse_outer_type_alias(
                span, parser,
            )?))
        } else if l.peek::<kw::import>() {
            Ok(Self::Import(parser.parse()?))
        } else if l.peek::<kw::export>() {
            parser.parse::<kw::export>()?;
            let name = parser.parse()?;
            let et = parser.parens(|parser| parser.parse())?;
            Ok(Self::Export(name, et))
        } else {
            Err(l.error())
        }
    }
}

impl<'a> Parse<'a> for Vec<ModuleTypeDecl<'a>> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut decls = Vec::new();
        while !parser.is_empty() {
            decls.push(parser.parens(|parser| parser.parse())?);
        }
        Ok(decls)
    }
}

/// A type declaration in a component.
#[derive(Debug)]
pub struct Type<'a> {
    /// Where this type was defined.
    pub span: Span,
    /// An optional identifier to refer to this `type` by as part of name
    /// resolution.
    pub id: Option<Id<'a>>,
    /// An optional name for this type stored in the custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// If present, inline export annotations which indicate names this
    /// definition should be exported under.
    pub exports: core::InlineExport<'a>,
    /// The type definition.
    pub def: TypeDef<'a>,
}

impl<'a> Parse<'a> for Type<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::r#type>()?.0;
        let id = parser.parse()?;
        let name = parser.parse()?;
        let exports = parser.parse()?;
        let def = parser.parse()?;

        Ok(Self {
            span,
            id,
            name,
            exports,
            def,
        })
    }
}

/// A definition of a component type.
#[derive(Debug)]
pub enum TypeDef<'a> {
    /// A defined value type.
    Defined(ComponentDefinedType<'a>),
    /// A component function type.
    Func(ComponentFunctionType<'a>),
    /// A component type.
    Component(ComponentType<'a>),
    /// An instance type.
    Instance(InstanceType<'a>),
}

impl<'a> Parse<'a> for TypeDef<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek::<LParen>() {
            parser.parens(|parser| {
                let mut l = parser.lookahead1();
                if l.peek::<kw::func>() {
                    parser.parse::<kw::func>()?;
                    Ok(Self::Func(parser.parse()?))
                } else if l.peek::<kw::component>() {
                    parser.parse::<kw::component>()?;
                    Ok(Self::Component(parser.parse()?))
                } else if l.peek::<kw::instance>() {
                    parser.parse::<kw::instance>()?;
                    Ok(Self::Instance(parser.parse()?))
                } else {
                    Ok(Self::Defined(ComponentDefinedType::parse_non_primitive(
                        parser, l,
                    )?))
                }
            })
        } else {
            // Only primitive types have no parens
            Ok(Self::Defined(ComponentDefinedType::Primitive(
                parser.parse()?,
            )))
        }
    }
}

/// A primitive value type.
#[allow(missing_docs)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PrimitiveValType {
    Unit,
    Bool,
    S8,
    U8,
    S16,
    U16,
    S32,
    U32,
    S64,
    U64,
    Float32,
    Float64,
    Char,
    String,
}

impl<'a> Parse<'a> for PrimitiveValType {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::unit>() {
            parser.parse::<kw::unit>()?;
            Ok(Self::Unit)
        } else if l.peek::<kw::bool_>() {
            parser.parse::<kw::bool_>()?;
            Ok(Self::Bool)
        } else if l.peek::<kw::s8>() {
            parser.parse::<kw::s8>()?;
            Ok(Self::S8)
        } else if l.peek::<kw::u8>() {
            parser.parse::<kw::u8>()?;
            Ok(Self::U8)
        } else if l.peek::<kw::s16>() {
            parser.parse::<kw::s16>()?;
            Ok(Self::S16)
        } else if l.peek::<kw::u16>() {
            parser.parse::<kw::u16>()?;
            Ok(Self::U16)
        } else if l.peek::<kw::s32>() {
            parser.parse::<kw::s32>()?;
            Ok(Self::S32)
        } else if l.peek::<kw::u32>() {
            parser.parse::<kw::u32>()?;
            Ok(Self::U32)
        } else if l.peek::<kw::s64>() {
            parser.parse::<kw::s64>()?;
            Ok(Self::S64)
        } else if l.peek::<kw::u64>() {
            parser.parse::<kw::u64>()?;
            Ok(Self::U64)
        } else if l.peek::<kw::float32>() {
            parser.parse::<kw::float32>()?;
            Ok(Self::Float32)
        } else if l.peek::<kw::float64>() {
            parser.parse::<kw::float64>()?;
            Ok(Self::Float64)
        } else if l.peek::<kw::char>() {
            parser.parse::<kw::char>()?;
            Ok(Self::Char)
        } else if l.peek::<kw::string>() {
            parser.parse::<kw::string>()?;
            Ok(Self::String)
        } else {
            Err(l.error())
        }
    }
}

/// A component value type.
#[allow(missing_docs)]
#[derive(Debug)]
pub enum ComponentValType<'a> {
    /// The value type is an inline defined type.
    Inline(ComponentDefinedType<'a>),
    /// The value type is an index reference to a defined type.
    Ref(Index<'a>),
}

impl<'a> Parse<'a> for ComponentValType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek::<Index<'_>>() {
            Ok(Self::Ref(parser.parse()?))
        } else {
            Ok(Self::Inline(InlineComponentValType::parse(parser)?.0))
        }
    }
}

/// An inline-only component value type.
///
/// This variation does not parse type indexes.
#[allow(missing_docs)]
#[derive(Debug)]
pub struct InlineComponentValType<'a>(ComponentDefinedType<'a>);

impl<'a> Parse<'a> for InlineComponentValType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek::<LParen>() {
            parser.parens(|parser| {
                Ok(Self(ComponentDefinedType::parse_non_primitive(
                    parser,
                    parser.lookahead1(),
                )?))
            })
        } else {
            Ok(Self(ComponentDefinedType::Primitive(parser.parse()?)))
        }
    }
}

// A component defined type.
#[allow(missing_docs)]
#[derive(Debug)]
pub enum ComponentDefinedType<'a> {
    Primitive(PrimitiveValType),
    Record(Record<'a>),
    Variant(Variant<'a>),
    List(List<'a>),
    Tuple(Tuple<'a>),
    Flags(Flags<'a>),
    Enum(Enum<'a>),
    Union(Union<'a>),
    Option(OptionType<'a>),
    Expected(Expected<'a>),
}

impl<'a> ComponentDefinedType<'a> {
    fn parse_non_primitive(parser: Parser<'a>, mut l: Lookahead1<'a>) -> Result<Self> {
        parser.depth_check()?;
        if l.peek::<kw::record>() {
            Ok(Self::Record(parser.parse()?))
        } else if l.peek::<kw::variant>() {
            Ok(Self::Variant(parser.parse()?))
        } else if l.peek::<kw::list>() {
            Ok(Self::List(parser.parse()?))
        } else if l.peek::<kw::tuple>() {
            Ok(Self::Tuple(parser.parse()?))
        } else if l.peek::<kw::flags>() {
            Ok(Self::Flags(parser.parse()?))
        } else if l.peek::<kw::enum_>() {
            Ok(Self::Enum(parser.parse()?))
        } else if l.peek::<kw::union>() {
            Ok(Self::Union(parser.parse()?))
        } else if l.peek::<kw::option>() {
            Ok(Self::Option(parser.parse()?))
        } else if l.peek::<kw::expected>() {
            Ok(Self::Expected(parser.parse()?))
        } else {
            Err(l.error())
        }
    }
}

impl Default for ComponentDefinedType<'_> {
    fn default() -> Self {
        Self::Primitive(PrimitiveValType::Unit)
    }
}

/// A record defined type.
#[derive(Debug)]
pub struct Record<'a> {
    /// The fields of the record.
    pub fields: Vec<RecordField<'a>>,
}

impl<'a> Parse<'a> for Record<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::record>()?;
        let mut fields = Vec::new();
        while !parser.is_empty() {
            fields.push(parser.parens(|p| p.parse())?);
        }
        Ok(Self { fields })
    }
}

/// A record type field.
#[derive(Debug)]
pub struct RecordField<'a> {
    /// The name of the field.
    pub name: &'a str,
    /// The type of the field.
    pub ty: ComponentValType<'a>,
}

impl<'a> Parse<'a> for RecordField<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::field>()?;
        Ok(Self {
            name: parser.parse()?,
            ty: parser.parse()?,
        })
    }
}

/// A variant defined type.
#[derive(Debug)]
pub struct Variant<'a> {
    /// The cases of the variant type.
    pub cases: Vec<VariantCase<'a>>,
}

impl<'a> Parse<'a> for Variant<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::variant>()?;
        let mut cases = Vec::new();
        while !parser.is_empty() {
            cases.push(parser.parens(|p| p.parse())?);
        }
        Ok(Self { cases })
    }
}

/// A case of a variant type.
#[derive(Debug)]
pub struct VariantCase<'a> {
    /// Where this `case` was defined
    pub span: Span,
    /// An optional identifier to refer to this case by as part of name
    /// resolution.
    pub id: Option<Id<'a>>,
    /// The name of the case.
    pub name: &'a str,
    /// The type of the case.
    pub ty: ComponentValType<'a>,
    /// The optional refinement.
    pub refines: Option<Refinement<'a>>,
}

impl<'a> Parse<'a> for VariantCase<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::case>()?.0;
        let id = parser.parse()?;
        let name = parser.parse()?;
        let ty = parser.parse()?;
        let refines = if !parser.is_empty() {
            Some(parser.parse()?)
        } else {
            None
        };
        Ok(Self {
            span,
            id,
            name,
            ty,
            refines,
        })
    }
}

/// A refinement for a variant case.
#[derive(Debug)]
pub enum Refinement<'a> {
    /// The refinement is referenced by index.
    Index(Span, Index<'a>),
    /// The refinement has been resolved to an index into
    /// the cases of the variant.
    Resolved(u32),
}

impl<'a> Parse<'a> for Refinement<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parens(|parser| {
            let span = parser.parse::<kw::refines>()?.0;
            let id = parser.parse()?;
            Ok(Self::Index(span, id))
        })
    }
}

/// A list type.
#[derive(Debug)]
pub struct List<'a> {
    /// The element type of the array.
    pub element: Box<ComponentValType<'a>>,
}

impl<'a> Parse<'a> for List<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::list>()?;
        Ok(Self {
            element: Box::new(parser.parse()?),
        })
    }
}

/// A tuple type.
#[derive(Debug)]
pub struct Tuple<'a> {
    /// The types of the fields of the tuple.
    pub fields: Vec<ComponentValType<'a>>,
}

impl<'a> Parse<'a> for Tuple<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::tuple>()?;
        let mut fields = Vec::new();
        while !parser.is_empty() {
            fields.push(parser.parse()?);
        }
        Ok(Self { fields })
    }
}

/// A flags type.
#[derive(Debug)]
pub struct Flags<'a> {
    /// The names of the individual flags.
    pub names: Vec<&'a str>,
}

impl<'a> Parse<'a> for Flags<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::flags>()?;
        let mut names = Vec::new();
        while !parser.is_empty() {
            names.push(parser.parse()?);
        }
        Ok(Self { names })
    }
}

/// An enum type.
#[derive(Debug)]
pub struct Enum<'a> {
    /// The tag names of the enum.
    pub names: Vec<&'a str>,
}

impl<'a> Parse<'a> for Enum<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::enum_>()?;
        let mut names = Vec::new();
        while !parser.is_empty() {
            names.push(parser.parse()?);
        }
        Ok(Self { names })
    }
}

/// A union type.
#[derive(Debug)]
pub struct Union<'a> {
    /// The types of the union.
    pub types: Vec<ComponentValType<'a>>,
}

impl<'a> Parse<'a> for Union<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::union>()?;
        let mut types = Vec::new();
        while !parser.is_empty() {
            types.push(parser.parse()?);
        }
        Ok(Self { types })
    }
}

/// An optional type.
#[derive(Debug)]
pub struct OptionType<'a> {
    /// The type of the value, when a value is present.
    pub element: Box<ComponentValType<'a>>,
}

impl<'a> Parse<'a> for OptionType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::option>()?;
        Ok(Self {
            element: Box::new(parser.parse()?),
        })
    }
}

/// An expected type.
#[derive(Debug)]
pub struct Expected<'a> {
    /// The type on success.
    pub ok: Box<ComponentValType<'a>>,
    /// The type on failure.
    pub err: Box<ComponentValType<'a>>,
}

impl<'a> Parse<'a> for Expected<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::expected>()?;
        let ok = parser.parse()?;
        let err = parser.parse()?;
        Ok(Self {
            ok: Box::new(ok),
            err: Box::new(err),
        })
    }
}

/// A component function type with parameters and result.
#[derive(Debug)]
pub struct ComponentFunctionType<'a> {
    /// The parameters of a function, optionally each having an identifier for
    /// name resolution and a name for the custom `name` section.
    pub params: Box<[ComponentFunctionParam<'a>]>,
    /// The result type of a function.
    pub result: ComponentValType<'a>,
}

impl<'a> Parse<'a> for ComponentFunctionType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut params = Vec::new();
        while parser.peek2::<kw::param>() {
            params.push(parser.parens(|p| p.parse())?);
        }

        let result = if parser.peek2::<kw::result>() {
            // Parse a `(result ...)`.
            parser.parens(|parser| {
                parser.parse::<kw::result>()?;
                parser.parse()
            })?
        } else {
            // If the result is omitted, use `unit`.
            ComponentValType::Inline(ComponentDefinedType::Primitive(PrimitiveValType::Unit))
        };

        Ok(Self {
            params: params.into(),
            result,
        })
    }
}

/// A parameter of a [`ComponentFunctionType`].
#[derive(Debug)]
pub struct ComponentFunctionParam<'a> {
    /// An optionally-specified name of this parameter
    pub name: Option<&'a str>,
    /// The type of the parameter.
    pub ty: ComponentValType<'a>,
}

impl<'a> Parse<'a> for ComponentFunctionParam<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::param>()?;
        Ok(Self {
            name: parser.parse()?,
            ty: parser.parse()?,
        })
    }
}

/// The type of an exported item from an component or instance type.
#[derive(Debug)]
pub struct ComponentExportType<'a> {
    /// Where this export was defined.
    pub span: Span,
    /// The name of this export.
    pub name: &'a str,
    /// The signature of the item.
    pub item: ItemSig<'a>,
}

impl<'a> Parse<'a> for ComponentExportType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::export>()?.0;
        let name = parser.parse()?;
        let item = parser.parens(|p| p.parse())?;
        Ok(Self { span, name, item })
    }
}

/// A type definition for a component type.
#[derive(Debug, Default)]
pub struct ComponentType<'a> {
    /// The declarations of the component type.
    pub decls: Vec<ComponentTypeDecl<'a>>,
}

impl<'a> Parse<'a> for ComponentType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.depth_check()?;
        Ok(Self {
            decls: parser.parse()?,
        })
    }
}

/// A declaration of a component type.
#[derive(Debug)]
pub enum ComponentTypeDecl<'a> {
    /// A core type definition local to the component type.
    CoreType(CoreType<'a>),
    /// A type definition local to the component type.
    Type(Type<'a>),
    /// An alias local to the component type.
    Alias(Alias<'a>),
    /// An import of the component type.
    Import(ComponentImport<'a>),
    /// An export of the component type.
    Export(ComponentExportType<'a>),
}

impl<'a> Parse<'a> for ComponentTypeDecl<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::core>() {
            Ok(Self::CoreType(parser.parse()?))
        } else if l.peek::<kw::r#type>() {
            Ok(Self::Type(parser.parse()?))
        } else if l.peek::<kw::alias>() {
            Ok(Self::Alias(Alias::parse_outer_type_alias(parser)?))
        } else if l.peek::<kw::import>() {
            Ok(Self::Import(parser.parse()?))
        } else if l.peek::<kw::export>() {
            Ok(Self::Export(parser.parse()?))
        } else {
            Err(l.error())
        }
    }
}

impl<'a> Parse<'a> for Vec<ComponentTypeDecl<'a>> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut decls = Vec::new();
        while !parser.is_empty() {
            decls.push(parser.parens(|parser| parser.parse())?);
        }
        Ok(decls)
    }
}

/// A type definition for an instance type.
#[derive(Debug)]
pub struct InstanceType<'a> {
    /// The declarations of the instance type.
    pub decls: Vec<InstanceTypeDecl<'a>>,
}

impl<'a> Parse<'a> for InstanceType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.depth_check()?;
        Ok(Self {
            decls: parser.parse()?,
        })
    }
}

/// A declaration of an instance type.
#[derive(Debug)]
pub enum InstanceTypeDecl<'a> {
    /// A core type definition local to the component type.
    CoreType(CoreType<'a>),
    /// A type definition local to the instance type.
    Type(Type<'a>),
    /// An alias local to the instance type.
    Alias(Alias<'a>),
    /// An export of the instance type.
    Export(ComponentExportType<'a>),
}

impl<'a> Parse<'a> for InstanceTypeDecl<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::core>() {
            Ok(Self::CoreType(parser.parse()?))
        } else if l.peek::<kw::r#type>() {
            Ok(Self::Type(parser.parse()?))
        } else if l.peek::<kw::alias>() {
            Ok(Self::Alias(Alias::parse_outer_type_alias(parser)?))
        } else if l.peek::<kw::export>() {
            Ok(Self::Export(parser.parse()?))
        } else {
            Err(l.error())
        }
    }
}

impl<'a> Parse<'a> for Vec<InstanceTypeDecl<'a>> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut decls = Vec::new();
        while !parser.is_empty() {
            decls.push(parser.parens(|parser| parser.parse())?);
        }
        Ok(decls)
    }
}

/// A value type declaration used for values in import signatures.
#[derive(Debug)]
pub struct ComponentValTypeUse<'a>(pub ComponentValType<'a>);

impl<'a> Parse<'a> for ComponentValTypeUse<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        match ComponentTypeUse::<'a, InlineComponentValType<'a>>::parse(parser)? {
            ComponentTypeUse::Ref(i) => Ok(Self(ComponentValType::Ref(i.idx))),
            ComponentTypeUse::Inline(t) => Ok(Self(ComponentValType::Inline(t.0))),
        }
    }
}

/// A reference to a core type defined in this component.
///
/// This is the same as `TypeUse`, but accepts `$T` as shorthand for
/// `(type $T)`.
#[derive(Debug, Clone)]
pub enum CoreTypeUse<'a, T> {
    /// The type that we're referencing.
    Ref(CoreItemRef<'a, kw::r#type>),
    /// The inline type.
    Inline(T),
}

impl<'a, T: Parse<'a>> Parse<'a> for CoreTypeUse<'a, T> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        // Here the core context is assumed, so no core prefix is expected
        if parser.peek::<LParen>() && parser.peek2::<CoreItemRef<'a, kw::r#type>>() {
            Ok(Self::Ref(parser.parens(|parser| parser.parse())?))
        } else {
            Ok(Self::Inline(parser.parse()?))
        }
    }
}

impl<T> Default for CoreTypeUse<'_, T> {
    fn default() -> Self {
        let span = Span::from_offset(0);
        Self::Ref(CoreItemRef {
            idx: Index::Num(0, span),
            kind: kw::r#type(span),
            export_name: None,
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
        if parser.peek::<LParen>() && parser.peek2::<ItemRef<'a, kw::r#type>>() {
            Ok(Self::Ref(parser.parens(|parser| parser.parse())?))
        } else {
            Ok(Self::Inline(parser.parse()?))
        }
    }
}

impl<T> Default for ComponentTypeUse<'_, T> {
    fn default() -> Self {
        let span = Span::from_offset(0);
        Self::Ref(ItemRef {
            idx: Index::Num(0, span),
            kind: kw::r#type(span),
            export_names: Vec::new(),
        })
    }
}
