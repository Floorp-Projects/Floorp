//! The `deftype` production in the component-model AST, and its children.

use crate::component::*;
use crate::core;
use crate::kw;
use crate::parser::{Cursor, Parse, Parser, Peek, Result};

/// Different kinds of elements that can be exported from a WebAssembly component,
/// contained in a [`ComponentExport`].
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq)]
#[allow(missing_docs)]
pub enum DefTypeKind {
    Func,
    Module,
    Component,
    Instance,
    Value,
}

impl<'a> Parse<'a> for DefTypeKind {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::func>() {
            parser.parse::<kw::func>()?;
            Ok(DefTypeKind::Func)
        } else if l.peek::<kw::module>() {
            parser.parse::<kw::module>()?;
            Ok(DefTypeKind::Module)
        } else if l.peek::<kw::component>() {
            parser.parse::<kw::component>()?;
            Ok(DefTypeKind::Component)
        } else if l.peek::<kw::instance>() {
            parser.parse::<kw::instance>()?;
            Ok(DefTypeKind::Instance)
        } else if l.peek::<kw::value>() {
            parser.parse::<kw::value>()?;
            Ok(DefTypeKind::Value)
        } else {
            Err(l.error())
        }
    }
}

impl Peek for DefTypeKind {
    fn peek(cursor: Cursor<'_>) -> bool {
        kw::func::peek(cursor)
            || kw::module::peek(cursor)
            || kw::component::peek(cursor)
            || kw::instance::peek(cursor)
            || kw::value::peek(cursor)
    }
    fn display() -> &'static str {
        "deftype kind"
    }
}

/// deftype           ::= <moduletype>
///                     | <componenttype>
///                     | <instancetype>
///                     | <functype>
///                     | <valuetype>
#[derive(Debug)]
#[allow(missing_docs)]
pub enum DefType<'a> {
    Func(ComponentFunctionType<'a>),
    Module(ModuleType<'a>),
    Component(ComponentType<'a>),
    Instance(InstanceType<'a>),
    Value(ValueType<'a>),
}

impl<'a> Parse<'a> for DefType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek::<kw::func>() {
            parser.parse::<kw::func>()?;
            Ok(DefType::Func(parser.parse()?))
        } else if parser.peek::<kw::module>() {
            parser.parse::<kw::module>()?;
            Ok(DefType::Module(parser.parse()?))
        } else if parser.peek::<kw::component>() {
            parser.parse::<kw::component>()?;
            Ok(DefType::Component(parser.parse()?))
        } else if parser.peek::<kw::instance>() {
            parser.parse::<kw::instance>()?;
            Ok(DefType::Instance(parser.parse()?))
        } else if parser.peek::<kw::value>() {
            parser.parse::<kw::value>()?;
            Ok(DefType::Value(parser.parse()?))
        } else {
            Err(parser.error("expected a deftype"))
        }
    }
}

/// A component function type with parameters and results.
///
/// functype          ::= (func <id>? (param <name>? <intertype>)* (result <intertype>)?)
#[derive(Debug)]
pub struct ComponentFunctionType<'a> {
    /// The parameters of a function, optionally each having an identifier for
    /// name resolution and a name for the custom `name` section.
    pub params: Box<[ComponentFunctionParam<'a>]>,
    /// The result type of a function.
    pub result: InterTypeRef<'a>,
}

impl<'a> Parse<'a> for ComponentFunctionType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut params = Vec::new();
        while parser.peek2::<kw::param>() {
            parser.parens(|p| {
                p.parse::<kw::param>()?;
                params.push(ComponentFunctionParam {
                    name: p.parse()?,
                    type_: p.parse()?,
                });
                Ok(())
            })?;
        }
        let result = if parser.peek2::<kw::result>() {
            // Parse a `(result ...)`.
            parser.parens(|parser| {
                parser.parse::<kw::result>()?;
                parser.parse()
            })?
        } else {
            // If the result is omitted, use `unit`.
            InterTypeRef::Primitive(Primitive::Unit)
        };
        Ok(Self {
            params: params.into(),
            result,
        })
    }
}

/// A parameter of a [`ComponentFunctionType`].
#[derive(Clone, Debug)]
pub struct ComponentFunctionParam<'a> {
    /// An optionally-specified name of this parameter
    pub name: Option<&'a str>,
    /// The type of the parameter.
    pub type_: InterTypeRef<'a>,
}

/// A type for a nested module
#[derive(Debug)]
pub struct ModuleType<'a> {
    /// The fields of the module type.
    pub defs: Vec<ModuleTypeDef<'a>>,
}

impl<'a> Parse<'a> for ModuleType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        // See comments in `nested_module.rs` for why this is tested here.
        if parser.parens_depth() > 100 {
            return Err(parser.error("module type nesting too deep"));
        }

        let mut defs = Vec::new();
        while !parser.is_empty() {
            defs.push(parser.parens(|p| p.parse())?);
        }
        Ok(ModuleType { defs })
    }
}

/// The contents of a [`ModuleType`].
#[derive(Debug)]
pub enum ModuleTypeDef<'a> {
    /// A function type.
    Type(core::Type<'a>),
    /// An import.
    Import(core::Import<'a>),
    /// An export.
    Export(&'a str, core::ItemSig<'a>),
}

impl<'a> Parse<'a> for ModuleTypeDef<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::r#type>() {
            Ok(ModuleTypeDef::Type(parser.parse()?))
        } else if l.peek::<kw::import>() {
            Ok(ModuleTypeDef::Import(parser.parse()?))
        } else if l.peek::<kw::export>() {
            parser.parse::<kw::export>()?;
            let name = parser.parse()?;
            let et = parser.parens(|parser| parser.parse())?;
            Ok(ModuleTypeDef::Export(name, et))
        } else {
            Err(parser.error("Expected a moduletype-def"))
        }
    }
}

/// A type for a nested component
#[derive(Debug, Default)]
pub struct ComponentType<'a> {
    /// The fields of this `ComponentType`.
    pub fields: Vec<ComponentTypeField<'a>>,
}

impl<'a> Parse<'a> for ComponentType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        // See comments in `nested_module.rs` for why this is tested here.
        if parser.parens_depth() > 100 {
            return Err(parser.error("component type nesting too deep"));
        }

        let mut fields = Vec::new();
        while !parser.is_empty() {
            parser.parens(|parser| {
                if parser.peek::<kw::import>() {
                    fields.push(ComponentTypeField::Import(parser.parse()?));
                } else if parser.peek::<kw::export>() {
                    fields.push(ComponentTypeField::Export(parser.parse()?));
                } else if parser.peek::<kw::r#type>() {
                    fields.push(ComponentTypeField::Type(parser.parse()?));
                } else if parser.peek::<kw::alias>() {
                    fields.push(ComponentTypeField::Alias(parser.parse()?));
                }
                Ok(())
            })?;
        }
        Ok(ComponentType { fields })
    }
}

/// A field of a type for a nested component
#[derive(Debug)]
pub enum ComponentTypeField<'a> {
    /// A public type for this component.
    Type(TypeField<'a>),

    /// A public type relationships for this component.
    Alias(Alias<'a>),

    /// An import expected for this component type.
    Import(ComponentImport<'a>),

    /// An export this component type is expected to have.
    Export(ComponentExportType<'a>),
}

impl<'a> From<TypeField<'a>> for ComponentTypeField<'a> {
    fn from(field: TypeField<'a>) -> ComponentTypeField<'a> {
        ComponentTypeField::Type(field)
    }
}

impl<'a> From<Alias<'a>> for ComponentTypeField<'a> {
    fn from(field: Alias<'a>) -> ComponentTypeField<'a> {
        ComponentTypeField::Alias(field)
    }
}

/// A type for a nested instance
#[derive(Debug)]
pub struct InstanceType<'a> {
    /// The fields of this `InstanceType`.
    pub fields: Vec<InstanceTypeField<'a>>,
}

impl<'a> Parse<'a> for InstanceType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        // See comments in `nested_module.rs` for why this is tested here.
        if parser.parens_depth() > 100 {
            return Err(parser.error("instance type nesting too deep"));
        }

        let mut fields = Vec::new();
        while !parser.is_empty() {
            parser.parens(|parser| {
                let mut l = parser.lookahead1();
                if l.peek::<kw::export>() {
                    fields.push(InstanceTypeField::Export(parser.parse()?));
                } else if l.peek::<kw::r#type>() {
                    fields.push(InstanceTypeField::Type(parser.parse()?));
                } else if l.peek::<kw::alias>() {
                    fields.push(InstanceTypeField::Alias(parser.parse()?));
                } else {
                    return Err(l.error());
                }
                Ok(())
            })?;
        }
        Ok(InstanceType { fields })
    }
}

/// A field of a type for a nested instance
#[derive(Debug)]
pub enum InstanceTypeField<'a> {
    /// A public type for this component.
    Type(TypeField<'a>),

    /// A public type relationships for this component.
    Alias(Alias<'a>),

    /// An export this component type is expected to have.
    Export(ComponentExportType<'a>),
}

impl<'a> From<TypeField<'a>> for InstanceTypeField<'a> {
    fn from(field: TypeField<'a>) -> InstanceTypeField<'a> {
        InstanceTypeField::Type(field)
    }
}

impl<'a> From<Alias<'a>> for InstanceTypeField<'a> {
    fn from(field: Alias<'a>) -> InstanceTypeField<'a> {
        InstanceTypeField::Alias(field)
    }
}

/// A value type.
#[derive(Debug, Clone)]
pub struct ValueType<'a> {
    /// The type of the value.
    pub value_type: InterTypeRef<'a>,
}

impl<'a> Parse<'a> for ValueType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        Ok(ValueType {
            value_type: parser.parse()?,
        })
    }
}
