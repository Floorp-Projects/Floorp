/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::literal::convert_default_value;
use crate::InterfaceCollector;
use anyhow::{bail, Result};

use uniffi_meta::{
    CallbackInterfaceMetadata, FieldMetadata, RecordMetadata, TraitMethodMetadata, VariantMetadata,
};

mod callables;
mod enum_;
mod interface;

/// Trait to help convert WedIDL syntax nodes into `InterfaceCollector` objects.
///
/// This trait does structural matching on the various weedle AST nodes and converts
/// them into appropriate structs that we can use to build up the contents of a
/// `InterfaceCollector`. It is basically the `TryFrom` trait except that the conversion
/// always happens in the context of a given `InterfaceCollector`, which is used for
/// resolving e.g. type definitions.
///
/// The difference between this trait and `APIBuilder` is that `APIConverter` treats the
/// `InterfaceCollector` as a read-only data source for resolving types, while `APIBuilder`
/// actually mutates the `InterfaceCollector` to add new definitions.
pub(crate) trait APIConverter<T> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<T>;
}

// Convert UDL docstring into metadata docstring
pub(crate) fn convert_docstring(docstring: &str) -> String {
    textwrap::dedent(docstring)
}

/// Convert a list of weedle items into a list of `InterfaceCollector` items,
/// by doing a direct item-by-item mapping.
impl<U, T: APIConverter<U>> APIConverter<Vec<U>> for Vec<T> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<Vec<U>> {
        self.iter().map(|v| v.convert(ci)).collect::<Result<_>>()
    }
}

impl APIConverter<VariantMetadata> for weedle::interface::OperationInterfaceMember<'_> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<VariantMetadata> {
        if self.special.is_some() {
            bail!("special operations not supported");
        }
        if let Some(weedle::interface::StringifierOrStatic::Stringifier(_)) = self.modifier {
            bail!("stringifiers are not supported");
        }
        // OK, so this is a little weird.
        // The syntax we use for enum interface members is `Name(type arg, ...);`, which parses
        // as an anonymous operation where `Name` is the return type. We re-interpret it to
        // use `Name` as the name of the variant.
        if self.identifier.is_some() {
            bail!("enum interface members must not have a method name");
        }
        let name: String = {
            use weedle::types::{
                NonAnyType::{self, Identifier},
                ReturnType,
                SingleType::NonAny,
                Type::Single,
            };
            match &self.return_type {
                ReturnType::Type(Single(NonAny(Identifier(id)))) => id.type_.0.to_owned(),
                // Using recognized/parsed types as enum variant names can lead to the bail error because they match
                // before `Identifier`. `Error` is one that's likely to be common, so we're circumventing what is
                // likely a parsing issue here. As an example of the issue `Promise` (`Promise(PromiseType<'a>)`) as
                // a variant matches the `Identifier` arm, but `DataView` (`DataView(MayBeNull<term!(DataView)>)`)
                // fails.
                ReturnType::Type(Single(NonAny(NonAnyType::Error(_)))) => "Error".to_string(),
                _ => bail!("enum interface members must have plain identifiers as names"),
            }
        };
        Ok(VariantMetadata {
            name,
            discr: None,
            fields: self
                .args
                .body
                .list
                .iter()
                .map(|arg| arg.convert(ci))
                .collect::<Result<Vec<_>>>()?,
            docstring: self.docstring.as_ref().map(|v| convert_docstring(&v.0)),
        })
    }
}

impl APIConverter<RecordMetadata> for weedle::DictionaryDefinition<'_> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<RecordMetadata> {
        if self.attributes.is_some() {
            bail!("dictionary attributes are not supported yet");
        }
        if self.inheritance.is_some() {
            bail!("dictionary inheritance is not supported");
        }
        Ok(RecordMetadata {
            module_path: ci.module_path(),
            name: self.identifier.0.to_string(),
            fields: self.members.body.convert(ci)?,
            docstring: self.docstring.as_ref().map(|v| convert_docstring(&v.0)),
        })
    }
}

impl APIConverter<FieldMetadata> for weedle::dictionary::DictionaryMember<'_> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<FieldMetadata> {
        if self.attributes.is_some() {
            bail!("dictionary member attributes are not supported yet");
        }
        let type_ = ci.resolve_type_expression(&self.type_)?;
        let default = match self.default {
            None => None,
            Some(v) => Some(convert_default_value(&v.value, &type_)?),
        };
        Ok(FieldMetadata {
            name: self.identifier.0.to_string(),
            ty: type_,
            default,
            docstring: self.docstring.as_ref().map(|v| convert_docstring(&v.0)),
        })
    }
}

impl APIConverter<CallbackInterfaceMetadata> for weedle::CallbackInterfaceDefinition<'_> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<CallbackInterfaceMetadata> {
        if self.attributes.is_some() {
            bail!("callback interface attributes are not supported yet");
        }
        if self.inheritance.is_some() {
            bail!("callback interface inheritance is not supported");
        }
        let object_name = self.identifier.0;
        for (index, member) in self.members.body.iter().enumerate() {
            match member {
                weedle::interface::InterfaceMember::Operation(t) => {
                    let mut method: TraitMethodMetadata = t.convert(ci)?;
                    // A CallbackInterface is described in Rust as a trait, but uniffi
                    // generates a struct implementing the trait and passes the concrete version
                    // of that.
                    // This really just reflects the fact that CallbackInterface and Object
                    // should be merged; we'd still need a way to ask for a struct delegating to
                    // foreign implementations be done.
                    // But currently they are passed as a concrete type with no associated types.
                    method.trait_name = object_name.to_string();
                    method.index = index as u32;
                    ci.items.insert(method.into());
                }
                _ => bail!(
                    "no support for callback interface member type {:?} yet",
                    member
                ),
            }
        }
        Ok(CallbackInterfaceMetadata {
            module_path: ci.module_path(),
            name: object_name.to_string(),
            docstring: self.docstring.as_ref().map(|v| convert_docstring(&v.0)),
        })
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use uniffi_meta::{LiteralMetadata, Metadata, Radix, Type};

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
        let mut ci = InterfaceCollector::from_webidl(UDL, "crate-name").unwrap();
        assert_eq!(ci.items.len(), 3);
        match &ci.items.pop_first().unwrap() {
            Metadata::Record(record) => {
                assert_eq!(record.name, "Complex");
                assert_eq!(record.fields.len(), 3);
                assert_eq!(record.fields[0].name, "key");
                assert_eq!(
                    record.fields[0].ty,
                    Type::Optional {
                        inner_type: Box::new(Type::String)
                    }
                );
                assert!(record.fields[0].default.is_none());
                assert_eq!(record.fields[1].name, "value");
                assert_eq!(record.fields[1].ty, Type::UInt32);
                assert!(matches!(
                    record.fields[1].default,
                    Some(LiteralMetadata::UInt(0, Radix::Decimal, Type::UInt32))
                ));
                assert_eq!(record.fields[2].name, "spin");
                assert_eq!(record.fields[2].ty, Type::Boolean);
                assert!(record.fields[2].default.is_none());
            }
            _ => unreachable!(),
        }

        match &ci.items.pop_first().unwrap() {
            Metadata::Record(record) => {
                assert_eq!(record.name, "Empty");
                assert_eq!(record.fields.len(), 0);
            }
            _ => unreachable!(),
        }

        match &ci.items.pop_first().unwrap() {
            Metadata::Record(record) => {
                assert_eq!(record.name, "Simple");
                assert_eq!(record.fields.len(), 1);
                assert_eq!(record.fields[0].name, "field");
                assert_eq!(record.fields[0].ty, Type::UInt32);
                assert!(record.fields[0].default.is_none());
            }
            _ => unreachable!(),
        }
    }
}
