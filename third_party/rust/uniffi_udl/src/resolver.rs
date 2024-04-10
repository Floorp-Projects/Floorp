/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! #  Helpers for resolving UDL type expressions into concrete types.
//!
//! This module provides the [`TypeResolver`] trait, an abstraction for walking
//! the parse tree of a weedle type expression and using a [`TypeCollector`] to
//! convert it into a concrete type definition (so it assumes that you're already
//! used a [`TypeFinder`](super::TypeFinder) to populate the universe).
//!
//! Perhaps most importantly, it knows how to error out if the UDL tries to reference
//! an undefined or invalid type.

use anyhow::{bail, Result};

use super::{Type, TypeCollector};

/// Trait to help resolving an UDL type node to a [`Type`].
///
/// This trait does structural matching against type-related weedle AST nodes from
/// a parsed UDL file, turning them into a corresponding [`Type`] struct. It uses the
/// known type definitions in a [`TypeCollector`] to resolve names to types.
///
/// As a side-effect, resolving a type expression will grow the type universe with
/// references to the types seen during traversal. For example resolving the type
/// expression `sequence<<TestRecord>?` will:
///
///   * add `Optional<Sequence<TestRecord>` and `Sequence<TestRecord>` to the
///     known types in the universe.
///   * error out if the type name `TestRecord` is not already known.
///
pub(crate) trait TypeResolver {
    fn resolve_type_expression(&self, types: &mut TypeCollector) -> Result<Type>;
}

impl TypeResolver for &weedle::types::Type<'_> {
    fn resolve_type_expression(&self, types: &mut TypeCollector) -> Result<Type> {
        (*self).resolve_type_expression(types)
    }
}

impl TypeResolver for weedle::types::Type<'_> {
    fn resolve_type_expression(&self, types: &mut TypeCollector) -> Result<Type> {
        match self {
            weedle::types::Type::Single(t) => match t {
                weedle::types::SingleType::Any(_) => bail!("no support for `any` types"),
                weedle::types::SingleType::NonAny(t) => t.resolve_type_expression(types),
            },
            weedle::types::Type::Union(_) => bail!("no support for union types yet"),
        }
    }
}

impl TypeResolver for weedle::types::NonAnyType<'_> {
    fn resolve_type_expression(&self, types: &mut TypeCollector) -> Result<Type> {
        match self {
            weedle::types::NonAnyType::Boolean(t) => t.resolve_type_expression(types),
            weedle::types::NonAnyType::Identifier(t) => t.resolve_type_expression(types),
            weedle::types::NonAnyType::Integer(t) => t.resolve_type_expression(types),
            weedle::types::NonAnyType::FloatingPoint(t) => t.resolve_type_expression(types),
            weedle::types::NonAnyType::Sequence(t) => t.resolve_type_expression(types),
            weedle::types::NonAnyType::RecordType(t) => t.resolve_type_expression(types),
            _ => bail!("no support for type {:?}", self),
        }
    }
}

impl TypeResolver for &weedle::types::AttributedNonAnyType<'_> {
    fn resolve_type_expression(&self, types: &mut TypeCollector) -> Result<Type> {
        if self.attributes.is_some() {
            bail!("type attributes are not supported yet");
        }
        self.type_.resolve_type_expression(types)
    }
}

impl TypeResolver for &weedle::types::AttributedType<'_> {
    fn resolve_type_expression(&self, types: &mut TypeCollector) -> Result<Type> {
        if self.attributes.is_some() {
            bail!("type attributes are not supported yet");
        }
        self.type_.resolve_type_expression(types)
    }
}

impl<T: TypeResolver> TypeResolver for weedle::types::MayBeNull<T> {
    fn resolve_type_expression(&self, types: &mut TypeCollector) -> Result<Type> {
        let type_ = self.type_.resolve_type_expression(types)?;
        match self.q_mark {
            None => Ok(type_),
            Some(_) => {
                let ty = Type::Optional {
                    inner_type: Box::new(type_),
                };
                Ok(ty)
            }
        }
    }
}

impl TypeResolver for weedle::types::IntegerType {
    fn resolve_type_expression(&self, _types: &mut TypeCollector) -> Result<Type> {
        bail!(
            "WebIDL integer types not implemented ({:?}); consider using u8, u16, u32 or u64",
            self
        )
    }
}

impl TypeResolver for weedle::types::FloatingPointType {
    fn resolve_type_expression(&self, types: &mut TypeCollector) -> Result<Type> {
        match self {
            weedle::types::FloatingPointType::Float(t) => t.resolve_type_expression(types),
            weedle::types::FloatingPointType::Double(t) => t.resolve_type_expression(types),
        }
    }
}

impl TypeResolver for weedle::types::SequenceType<'_> {
    fn resolve_type_expression(&self, types: &mut TypeCollector) -> Result<Type> {
        let t = self.generics.body.as_ref().resolve_type_expression(types)?;
        let ty = Type::Sequence {
            inner_type: Box::new(t),
        };
        Ok(ty)
    }
}

impl TypeResolver for weedle::types::RecordKeyType<'_> {
    fn resolve_type_expression(&self, types: &mut TypeCollector) -> Result<Type> {
        use weedle::types::RecordKeyType::*;
        match self {
            Byte(_) | USV(_) => bail!(
                "WebIDL Byte or USV string type not implemented ({self:?}); \
                 consider using a string",
            ),
            DOM(_) => Ok(Type::String),
            NonAny(t) => t.resolve_type_expression(types),
        }
    }
}

impl TypeResolver for weedle::types::RecordType<'_> {
    fn resolve_type_expression(&self, types: &mut TypeCollector) -> Result<Type> {
        let key_type = self.generics.body.0.resolve_type_expression(types)?;
        let value_type = self.generics.body.2.resolve_type_expression(types)?;
        let map = Type::Map {
            key_type: Box::new(key_type),
            value_type: Box::new(value_type),
        };
        Ok(map)
    }
}

impl TypeResolver for weedle::common::Identifier<'_> {
    fn resolve_type_expression(&self, types: &mut TypeCollector) -> Result<Type> {
        match resolve_builtin_type(self.0) {
            Some(type_) => Ok(type_),
            None => match types.get_type_definition(self.0) {
                Some(type_) => Ok(type_),
                None => bail!("unknown type reference: {}", self.0),
            },
        }
    }
}

impl TypeResolver for weedle::term::Boolean {
    fn resolve_type_expression(&self, _types: &mut TypeCollector) -> Result<Type> {
        Ok(Type::Boolean)
    }
}

impl TypeResolver for weedle::types::FloatType {
    fn resolve_type_expression(&self, _types: &mut TypeCollector) -> Result<Type> {
        if self.unrestricted.is_some() {
            bail!("we don't support `unrestricted float`");
        }
        Ok(Type::Float32)
    }
}

impl TypeResolver for weedle::types::DoubleType {
    fn resolve_type_expression(&self, _types: &mut TypeCollector) -> Result<Type> {
        if self.unrestricted.is_some() {
            bail!("we don't support `unrestricted double`");
        }
        Ok(Type::Float64)
    }
}

/// Resolve built-in API types by name.
///
/// Given an identifier from the UDL, this will return `Some(Type)` if it names one of the
/// built-in primitive types or `None` if it names something else.
pub(crate) fn resolve_builtin_type(name: &str) -> Option<Type> {
    match name {
        "string" => Some(Type::String),
        "bytes" => Some(Type::Bytes),
        "u8" => Some(Type::UInt8),
        "i8" => Some(Type::Int8),
        "u16" => Some(Type::UInt16),
        "i16" => Some(Type::Int16),
        "u32" => Some(Type::UInt32),
        "i32" => Some(Type::Int32),
        "u64" => Some(Type::UInt64),
        "i64" => Some(Type::Int64),
        "f32" => Some(Type::Float32),
        "f64" => Some(Type::Float64),
        "timestamp" => Some(Type::Timestamp),
        "duration" => Some(Type::Duration),
        "ForeignExecutor" => Some(Type::ForeignExecutor),
        _ => None,
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use weedle::Parse;

    #[test]
    fn test_named_type_resolution() -> Result<()> {
        let mut types = TypeCollector::default();
        types.add_type_definition(
            "TestRecord",
            Type::Record {
                name: "TestRecord".into(),
                module_path: "".into(),
            },
        )?;
        assert_eq!(types.type_definitions.len(), 1);

        let (_, expr) = weedle::types::Type::parse("TestRecord").unwrap();
        let t = types.resolve_type_expression(expr).unwrap();
        assert!(matches!(t, Type::Record { name, .. } if name == "TestRecord"));
        assert_eq!(types.type_definitions.len(), 1);

        let (_, expr) = weedle::types::Type::parse("TestRecord?").unwrap();
        let t = types.resolve_type_expression(expr).unwrap();
        assert!(matches!(t, Type::Optional { .. }));
        assert!(match t {
            Type::Optional { inner_type } =>
                matches!(*inner_type, Type::Record { name, .. } if name == "TestRecord"),
            _ => false,
        });
        //        assert_eq!(types.type_definitions.len(), 2);

        Ok(())
    }

    #[test]
    fn test_error_on_unknown_type() -> Result<()> {
        let mut types = TypeCollector::default();
        types.add_type_definition(
            "TestRecord",
            Type::Record {
                name: "TestRecord".into(),
                module_path: "".into(),
            },
        )?;
        // Oh no, someone made a typo in the type-o...
        let (_, expr) = weedle::types::Type::parse("TestRecrd").unwrap();
        let err = types.resolve_type_expression(expr).unwrap_err();
        assert_eq!(err.to_string(), "unknown type reference: TestRecrd");
        Ok(())
    }

    #[test]
    fn test_error_on_union_type() -> Result<()> {
        let mut types = TypeCollector::default();
        types.add_type_definition(
            "TestRecord",
            Type::Record {
                name: "TestRecord".into(),
                module_path: "".into(),
            },
        )?;
        let (_, expr) = weedle::types::Type::parse("(TestRecord or u32)").unwrap();
        let err = types.resolve_type_expression(expr).unwrap_err();
        assert_eq!(err.to_string(), "no support for union types yet");
        Ok(())
    }
}
