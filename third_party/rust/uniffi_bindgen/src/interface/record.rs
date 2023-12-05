/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Record definitions for a `ComponentInterface`.
//!
//! This module converts "dictionary" definitions from UDL into [`Record`] structures
//! that can be added to a `ComponentInterface`, which are the main way we define structured
//! data types for a UniFFI Rust Component. A [`Record`] has a fixed set of named fields,
//! each of a specific type.
//!
//! (The terminology mismatch between "dictionary" and "record" is a historical artifact
//! due to this tool being loosely inspired by WebAssembly Interface Types, which used
//! the term "record" for this sort of data).
//!
//! A declaration in the UDL like this:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! dictionary Example {
//!   string name;
//!   u32 value;
//! };
//! # "##)?;
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! Will result in a [`Record`] member with two [`Field`]s being added to the resulting
//! [`ComponentInterface`]:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! # dictionary Example {
//! #   string name;
//! #   u32 value;
//! # };
//! # "##)?;
//! let record = ci.get_record_definition("Example").unwrap();
//! assert_eq!(record.name(), "Example");
//! assert_eq!(record.fields()[0].name(), "name");
//! assert_eq!(record.fields()[1].name(), "value");
//! # Ok::<(), anyhow::Error>(())
//! ```

use anyhow::{bail, Result};
use uniffi_meta::Checksum;

use super::literal::{convert_default_value, Literal};
use super::types::{Type, TypeIterator};
use super::{APIConverter, AsType, ComponentInterface};

/// Represents a "data class" style object, for passing around complex values.
///
/// In the FFI these are represented as a byte buffer, which one side explicitly
/// serializes the data into and the other serializes it out of. So I guess they're
/// kind of like "pass by clone" values.
#[derive(Debug, Clone, PartialEq, Eq, Checksum)]
pub struct Record {
    pub(super) name: String,
    pub(super) fields: Vec<Field>,
}

impl Record {
    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn fields(&self) -> &[Field] {
        &self.fields
    }

    pub fn iter_types(&self) -> TypeIterator<'_> {
        Box::new(self.fields.iter().flat_map(Field::iter_types))
    }
}

impl AsType for Record {
    fn as_type(&self) -> Type {
        Type::Record(self.name.clone())
    }
}

impl TryFrom<uniffi_meta::RecordMetadata> for Record {
    type Error = anyhow::Error;

    fn try_from(meta: uniffi_meta::RecordMetadata) -> Result<Self> {
        Ok(Self {
            name: meta.name,
            fields: meta
                .fields
                .into_iter()
                .map(TryInto::try_into)
                .collect::<Result<_>>()?,
        })
    }
}

impl APIConverter<Record> for weedle::DictionaryDefinition<'_> {
    fn convert(&self, ci: &mut ComponentInterface) -> Result<Record> {
        if self.attributes.is_some() {
            bail!("dictionary attributes are not supported yet");
        }
        if self.inheritance.is_some() {
            bail!("dictionary inheritance is not supported");
        }
        Ok(Record {
            name: self.identifier.0.to_string(),
            fields: self.members.body.convert(ci)?,
        })
    }
}

// Represents an individual field on a Record.
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Checksum)]
pub struct Field {
    pub(super) name: String,
    pub(super) type_: Type,
    pub(super) default: Option<Literal>,
}

impl Field {
    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn default_value(&self) -> Option<&Literal> {
        self.default.as_ref()
    }

    pub fn iter_types(&self) -> TypeIterator<'_> {
        self.type_.iter_types()
    }
}

impl AsType for Field {
    fn as_type(&self) -> Type {
        self.type_.clone()
    }
}

impl TryFrom<uniffi_meta::FieldMetadata> for Field {
    type Error = anyhow::Error;

    fn try_from(meta: uniffi_meta::FieldMetadata) -> Result<Self> {
        let name = meta.name;
        let type_ = meta.ty.into();
        let default = meta
            .default
            .map(|d| Literal::from_metadata(&name, &type_, d))
            .transpose()?;
        Ok(Self {
            name,
            type_,
            default,
        })
    }
}

impl APIConverter<Field> for weedle::dictionary::DictionaryMember<'_> {
    fn convert(&self, ci: &mut ComponentInterface) -> Result<Field> {
        if self.attributes.is_some() {
            bail!("dictionary member attributes are not supported yet");
        }
        let type_ = ci.resolve_type_expression(&self.type_)?;
        let default = match self.default {
            None => None,
            Some(v) => Some(convert_default_value(&v.value, &type_)?),
        };
        Ok(Field {
            name: self.identifier.0.to_string(),
            type_,
            default,
        })
    }
}

#[cfg(test)]
mod test {
    use super::super::literal::Radix;
    use super::*;

    #[test]
    fn test_multiple_record_types() {
        const UDL: &str = r#"
            namespace test{};
            dictionary Empty {};
            dictionary Simple {
                u32 field;
            };
            dictionary Complex {
                string? key;
                u32 value = 0;
                required boolean spin;
            };
        "#;
        let ci = ComponentInterface::from_webidl(UDL).unwrap();
        assert_eq!(ci.record_definitions().count(), 3);

        let record = ci.get_record_definition("Empty").unwrap();
        assert_eq!(record.name(), "Empty");
        assert_eq!(record.fields().len(), 0);

        let record = ci.get_record_definition("Simple").unwrap();
        assert_eq!(record.name(), "Simple");
        assert_eq!(record.fields().len(), 1);
        assert_eq!(record.fields()[0].name(), "field");
        assert_eq!(record.fields()[0].as_type().canonical_name(), "u32");
        assert!(record.fields()[0].default_value().is_none());

        let record = ci.get_record_definition("Complex").unwrap();
        assert_eq!(record.name(), "Complex");
        assert_eq!(record.fields().len(), 3);
        assert_eq!(record.fields()[0].name(), "key");
        assert_eq!(
            record.fields()[0].as_type().canonical_name(),
            "Optionalstring"
        );
        assert!(record.fields()[0].default_value().is_none());
        assert_eq!(record.fields()[1].name(), "value");
        assert_eq!(record.fields()[1].as_type().canonical_name(), "u32");
        assert!(matches!(
            record.fields()[1].default_value(),
            Some(Literal::UInt(0, Radix::Decimal, Type::UInt32))
        ));
        assert_eq!(record.fields()[2].name(), "spin");
        assert_eq!(record.fields()[2].as_type().canonical_name(), "bool");
        assert!(record.fields()[2].default_value().is_none());
    }

    #[test]
    fn test_that_all_field_types_become_known() {
        const UDL: &str = r#"
            namespace test{};
            dictionary Testing {
                string? maybe_name;
                u32 value;
            };
        "#;
        let ci = ComponentInterface::from_webidl(UDL).unwrap();
        assert_eq!(ci.record_definitions().count(), 1);
        let record = ci.get_record_definition("Testing").unwrap();
        assert_eq!(record.fields().len(), 2);
        assert_eq!(record.fields()[0].name(), "maybe_name");
        assert_eq!(record.fields()[1].name(), "value");

        assert_eq!(ci.iter_types().count(), 4);
        assert!(ci.iter_types().any(|t| t.canonical_name() == "u32"));
        assert!(ci.iter_types().any(|t| t.canonical_name() == "string"));
        assert!(ci
            .iter_types()
            .any(|t| t.canonical_name() == "Optionalstring"));
        assert!(ci.iter_types().any(|t| t.canonical_name() == "TypeTesting"));
    }
}
