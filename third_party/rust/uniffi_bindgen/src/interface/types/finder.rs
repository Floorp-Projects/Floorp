/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Helpers for finding the named types defined in a UDL interface.
//!
//! This module provides the [`TypeFinder`] trait, an abstraction for walking
//! the weedle parse tree, looking for type definitions, and accumulating them
//! in a [`TypeUniverse`].
//!
//! The type-finding process only discovers very basic information about names
//! and their corresponding types. For example, it can discover that "Foobar"
//! names a Record, but it won't discover anything about the fields of that
//! record.
//!
//! Factoring this functionality out into a separate phase makes the subsequent
//! work of more *detailed* parsing of the UDL a lot simpler, we know how to resolve
//! names to types when building up the full interface definition.

use std::convert::TryFrom;

use anyhow::{bail, Result};

use super::super::attributes::{EnumAttributes, InterfaceAttributes, TypedefAttributes};
use super::{Type, TypeUniverse};

/// Trait to help with an early "type discovery" phase when processing the UDL.
///
/// Ths trait does structural matching against weedle AST nodes from a parsed
/// UDL file, looking for all the newly-defined types in the file and accumulating
/// them in the given `TypeUniverse`.
pub(in super::super) trait TypeFinder {
    fn add_type_definitions_to(&self, types: &mut TypeUniverse) -> Result<()>;
}

impl<T: TypeFinder> TypeFinder for &[T] {
    fn add_type_definitions_to(&self, types: &mut TypeUniverse) -> Result<()> {
        for item in self.iter() {
            item.add_type_definitions_to(types)?;
        }
        Ok(())
    }
}

impl TypeFinder for weedle::Definition<'_> {
    fn add_type_definitions_to(&self, types: &mut TypeUniverse) -> Result<()> {
        match self {
            weedle::Definition::Interface(d) => d.add_type_definitions_to(types),
            weedle::Definition::Dictionary(d) => d.add_type_definitions_to(types),
            weedle::Definition::Enum(d) => d.add_type_definitions_to(types),
            weedle::Definition::Typedef(d) => d.add_type_definitions_to(types),
            weedle::Definition::CallbackInterface(d) => d.add_type_definitions_to(types),
            _ => Ok(()),
        }
    }
}

impl TypeFinder for weedle::InterfaceDefinition<'_> {
    fn add_type_definitions_to(&self, types: &mut TypeUniverse) -> Result<()> {
        let name = self.identifier.0.to_string();
        // Some enum types are defined using an `interface` with a special attribute.
        if InterfaceAttributes::try_from(self.attributes.as_ref())?.contains_enum_attr() {
            types.add_type_definition(self.identifier.0, Type::Enum(name))
        } else if InterfaceAttributes::try_from(self.attributes.as_ref())?.contains_error_attr() {
            types.add_type_definition(self.identifier.0, Type::Error(name))
        } else {
            types.add_type_definition(self.identifier.0, Type::Object(name))
        }
    }
}

impl TypeFinder for weedle::DictionaryDefinition<'_> {
    fn add_type_definitions_to(&self, types: &mut TypeUniverse) -> Result<()> {
        let name = self.identifier.0.to_string();
        types.add_type_definition(self.identifier.0, Type::Record(name))
    }
}

impl TypeFinder for weedle::EnumDefinition<'_> {
    fn add_type_definitions_to(&self, types: &mut TypeUniverse) -> Result<()> {
        let name = self.identifier.0.to_string();
        // Our error types are defined using an `enum` with a special attribute.
        if EnumAttributes::try_from(self.attributes.as_ref())?.contains_error_attr() {
            types.add_type_definition(self.identifier.0, Type::Error(name))
        } else {
            types.add_type_definition(self.identifier.0, Type::Enum(name))
        }
    }
}

impl TypeFinder for weedle::TypedefDefinition<'_> {
    fn add_type_definitions_to(&self, types: &mut TypeUniverse) -> Result<()> {
        let name = self.identifier.0;
        let attrs = TypedefAttributes::try_from(self.attributes.as_ref())?;
        // If we wanted simple `typedef`s, it would be as easy as:
        // > let t = types.resolve_type_expression(&self.type_)?;
        // > types.add_type_definition(name, t)
        // But we don't - `typedef`s are reserved for external types.
        if attrs.is_custom() {
            // A local type which wraps a builtin and for which we will generate an
            // `FfiConverter` implementation.
            let builtin = types.resolve_type_expression(&self.type_)?;
            types.add_type_definition(
                name,
                Type::Custom {
                    name: name.to_string(),
                    builtin: builtin.into(),
                },
            )
        } else {
            // A crate which can supply an `FfiConverter`.
            // We don't reference `self._type`, so ideally we could insist on it being
            // the literal 'extern' but that's tricky
            types.add_type_definition(
                name,
                Type::External {
                    name: name.to_string(),
                    crate_name: attrs.get_crate_name(),
                },
            )
        }
    }
}

impl TypeFinder for weedle::CallbackInterfaceDefinition<'_> {
    fn add_type_definitions_to(&self, types: &mut TypeUniverse) -> Result<()> {
        if self.attributes.is_some() {
            bail!("no typedef attributes are currently supported");
        }
        let name = self.identifier.0.to_string();
        types.add_type_definition(self.identifier.0, Type::CallbackInterface(name))
    }
}

#[cfg(test)]
mod test {
    use super::*;

    // A helper to take valid UDL and a closure to check what's in it.
    fn test_a_finding<F>(udl: &str, tester: F)
    where
        F: FnOnce(TypeUniverse),
    {
        let idl = weedle::parse(udl).unwrap();
        let mut types = TypeUniverse::default();
        types.add_type_definitions_from(idl.as_ref()).unwrap();
        tester(types);
    }

    #[test]
    fn test_type_finding() {
        test_a_finding(
            r#"
            callback interface TestCallbacks {
                string hello(u32 count);
            };
        "#,
            |types| {
                assert!(
                    matches!(types.get_type_definition("TestCallbacks").unwrap(), Type::CallbackInterface(nm) if nm == "TestCallbacks")
                );
            },
        );

        test_a_finding(
            r#"
            dictionary TestRecord {
                u32 field;
            };
        "#,
            |types| {
                assert!(
                    matches!(types.get_type_definition("TestRecord").unwrap(), Type::Record(nm) if nm == "TestRecord")
                );
            },
        );

        test_a_finding(
            r#"
            enum TestItems { "one", "two" };

            [Error]
            enum TestError { "ErrorOne", "ErrorTwo" };
        "#,
            |types| {
                assert!(
                    matches!(types.get_type_definition("TestItems").unwrap(), Type::Enum(nm) if nm == "TestItems")
                );
                assert!(
                    matches!(types.get_type_definition("TestError").unwrap(), Type::Error(nm) if nm == "TestError")
                );
            },
        );

        test_a_finding(
            r#"
            interface TestObject {
                constructor();
            };
        "#,
            |types| {
                assert!(
                    matches!(types.get_type_definition("TestObject").unwrap(), Type::Object(nm) if nm == "TestObject")
                );
            },
        );

        test_a_finding(
            r#"
            [External="crate-name"]
            typedef extern ExternalType;

            [Custom]
            typedef string CustomType;
        "#,
            |types| {
                assert!(
                    matches!(types.get_type_definition("ExternalType").unwrap(), Type::External { name, crate_name }
                                                                                 if name == "ExternalType" && crate_name == "crate-name")
                );
                assert!(
                    matches!(types.get_type_definition("CustomType").unwrap(), Type::Custom { name, builtin }
                                                                                     if name == "CustomType" && builtin == Box::new(Type::String))
                );
            },
        );
    }

    fn get_err(udl: &str) -> String {
        let parsed = weedle::parse(udl).unwrap();
        let mut types = TypeUniverse::default();
        let err = types
            .add_type_definitions_from(parsed.as_ref())
            .unwrap_err();
        err.to_string()
    }

    #[test]
    #[should_panic]
    fn test_typedef_error_on_no_attr() {
        // Sorry, still working out what we want for non-imported typedefs..
        get_err("typedef string Custom;");
    }
}
