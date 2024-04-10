/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::APIConverter;
use crate::attributes::InterfaceAttributes;
use crate::{converters::convert_docstring, InterfaceCollector};
use anyhow::{bail, Result};
use std::collections::HashSet;
use uniffi_meta::{
    ConstructorMetadata, FnParamMetadata, MethodMetadata, ObjectImpl, ObjectMetadata, Type,
    UniffiTraitMetadata,
};

impl APIConverter<ObjectMetadata> for weedle::InterfaceDefinition<'_> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<ObjectMetadata> {
        if self.inheritance.is_some() {
            bail!("interface inheritance is not supported");
        }
        let attributes = match &self.attributes {
            Some(attrs) => InterfaceAttributes::try_from(attrs)?,
            None => Default::default(),
        };

        let object_name = self.identifier.0;
        let object_impl = attributes.object_impl()?;
        // Convert each member into a constructor or method, guarding against duplicate names.
        // They get added to the ci and aren't carried in ObjectMetadata.
        let mut member_names = HashSet::new();
        for member in &self.members.body {
            match member {
                weedle::interface::InterfaceMember::Constructor(t) => {
                    let mut cons: ConstructorMetadata = t.convert(ci)?;
                    if object_impl == ObjectImpl::Trait {
                        bail!(
                            "Trait interfaces can not have constructors: \"{}\"",
                            cons.name
                        )
                    }
                    if !member_names.insert(cons.name.clone()) {
                        bail!("Duplicate interface member name: \"{}\"", cons.name)
                    }
                    cons.self_name = object_name.to_string();
                    ci.items.insert(cons.into());
                }
                weedle::interface::InterfaceMember::Operation(t) => {
                    let mut method: MethodMetadata = t.convert(ci)?;
                    if !member_names.insert(method.name.clone()) {
                        bail!("Duplicate interface member name: \"{}\"", method.name)
                    }
                    method.self_name = object_name.to_string();
                    ci.items.insert(method.into());
                }
                _ => bail!("no support for interface member type {:?} yet", member),
            }
        }
        // A helper for our trait methods
        let make_trait_method = |name: &str,
                                 inputs: Vec<FnParamMetadata>,
                                 return_type: Option<Type>|
         -> Result<MethodMetadata> {
            Ok(MethodMetadata {
                module_path: ci.module_path(),
                // The name is used to create the ffi function for the method.
                name: name.to_string(),
                self_name: object_name.to_string(),
                is_async: false,
                inputs,
                return_type,
                throws: None,
                takes_self_by_arc: false,
                checksum: None,
                docstring: None,
            })
        };
        // Trait methods are in the Metadata.
        let uniffi_traits = attributes
            .get_traits()
            .into_iter()
            .map(|trait_name| {
                Ok(match trait_name.as_str() {
                    "Debug" => UniffiTraitMetadata::Debug {
                        fmt: make_trait_method("uniffi_trait_debug", vec![], Some(Type::String))?,
                    },
                    "Display" => UniffiTraitMetadata::Display {
                        fmt: make_trait_method("uniffi_trait_display", vec![], Some(Type::String))?,
                    },
                    "Eq" => UniffiTraitMetadata::Eq {
                        eq: make_trait_method(
                            "uniffi_trait_eq_eq",
                            vec![FnParamMetadata {
                                name: "other".to_string(),
                                ty: Type::Object {
                                    module_path: ci.module_path(),
                                    name: object_name.to_string(),
                                    imp: object_impl,
                                },
                                by_ref: true,
                                default: None,
                                optional: false,
                            }],
                            Some(Type::Boolean),
                        )?,
                        ne: make_trait_method(
                            "uniffi_trait_eq_ne",
                            vec![FnParamMetadata {
                                name: "other".to_string(),
                                ty: Type::Object {
                                    module_path: ci.module_path(),
                                    name: object_name.to_string(),
                                    imp: object_impl,
                                },
                                by_ref: true,
                                default: None,
                                optional: false,
                            }],
                            Some(Type::Boolean),
                        )?,
                    },
                    "Hash" => UniffiTraitMetadata::Hash {
                        hash: make_trait_method("uniffi_trait_hash", vec![], Some(Type::UInt64))?,
                    },
                    _ => bail!("Invalid trait name: {}", trait_name),
                })
            })
            .collect::<Result<Vec<_>>>()?;
        for ut in uniffi_traits {
            ci.items.insert(ut.into());
        }
        Ok(ObjectMetadata {
            module_path: ci.module_path(),
            name: object_name.to_string(),
            imp: object_impl,
            docstring: self.docstring.as_ref().map(|v| convert_docstring(&v.0)),
        })
    }
}
