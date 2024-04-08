/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::APIConverter;
use crate::attributes::ArgumentAttributes;
use crate::attributes::{ConstructorAttributes, FunctionAttributes, MethodAttributes};
use crate::literal::convert_default_value;
use crate::InterfaceCollector;
use anyhow::{bail, Result};

use uniffi_meta::{
    ConstructorMetadata, FieldMetadata, FnMetadata, FnParamMetadata, MethodMetadata,
    TraitMethodMetadata, Type,
};

impl APIConverter<FieldMetadata> for weedle::argument::Argument<'_> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<FieldMetadata> {
        match self {
            weedle::argument::Argument::Single(t) => t.convert(ci),
            weedle::argument::Argument::Variadic(_) => bail!("variadic arguments not supported"),
        }
    }
}

impl APIConverter<FieldMetadata> for weedle::argument::SingleArgument<'_> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<FieldMetadata> {
        let type_ = ci.resolve_type_expression(&self.type_)?;
        if let Type::Object { .. } = type_ {
            bail!("Objects cannot currently be used in enum variant data");
        }
        if self.default.is_some() {
            bail!("enum interface variant fields must not have default values");
        }
        if self.attributes.is_some() {
            bail!("enum interface variant fields must not have attributes");
        }
        // TODO: maybe we should use our own `Field` type here with just name and type,
        // rather than appropriating record::Field..?
        Ok(FieldMetadata {
            name: self.identifier.0.to_string(),
            ty: type_,
            default: None,
        })
    }
}

impl APIConverter<FnParamMetadata> for weedle::argument::Argument<'_> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<FnParamMetadata> {
        match self {
            weedle::argument::Argument::Single(t) => t.convert(ci),
            weedle::argument::Argument::Variadic(_) => bail!("variadic arguments not supported"),
        }
    }
}

impl APIConverter<FnParamMetadata> for weedle::argument::SingleArgument<'_> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<FnParamMetadata> {
        let type_ = ci.resolve_type_expression(&self.type_)?;
        let default = match self.default {
            None => None,
            Some(v) => Some(convert_default_value(&v.value, &type_)?),
        };
        let by_ref = ArgumentAttributes::try_from(self.attributes.as_ref())?.by_ref();
        Ok(FnParamMetadata {
            name: self.identifier.0.to_string(),
            ty: type_,
            by_ref,
            optional: self.optional.is_some(),
            default,
        })
    }
}

impl APIConverter<FnMetadata> for weedle::namespace::NamespaceMember<'_> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<FnMetadata> {
        match self {
            weedle::namespace::NamespaceMember::Operation(f) => f.convert(ci),
            _ => bail!("no support for namespace member type {:?} yet", self),
        }
    }
}

impl APIConverter<FnMetadata> for weedle::namespace::OperationNamespaceMember<'_> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<FnMetadata> {
        let return_type = ci.resolve_return_type_expression(&self.return_type)?;
        let name = match self.identifier {
            None => bail!("anonymous functions are not supported {:?}", self),
            Some(id) => id.0.to_string(),
        };
        let attrs = FunctionAttributes::try_from(self.attributes.as_ref())?;
        let throws = match attrs.get_throws_err() {
            None => None,
            Some(name) => match ci.get_type(name) {
                Some(t) => Some(t),
                None => bail!("unknown type for error: {name}"),
            },
        };
        Ok(FnMetadata {
            module_path: ci.module_path(),
            name,
            is_async: false,
            return_type,
            inputs: self.args.body.list.convert(ci)?,
            throws,
            checksum: None,
        })
    }
}

impl APIConverter<ConstructorMetadata> for weedle::interface::ConstructorInterfaceMember<'_> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<ConstructorMetadata> {
        let attributes = match &self.attributes {
            Some(attr) => ConstructorAttributes::try_from(attr)?,
            None => Default::default(),
        };
        let throws = attributes
            .get_throws_err()
            .map(|name| ci.get_type(name).expect("invalid throws type"));
        Ok(ConstructorMetadata {
            module_path: ci.module_path(),
            name: String::from(attributes.get_name().unwrap_or("new")),
            // We don't know the name of the containing `Object` at this point, fill it in later.
            self_name: Default::default(),
            // Also fill in checksum_fn_name later, since it depends on object_name
            inputs: self.args.body.list.convert(ci)?,
            throws,
            checksum: None,
        })
    }
}

impl APIConverter<MethodMetadata> for weedle::interface::OperationInterfaceMember<'_> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<MethodMetadata> {
        if self.special.is_some() {
            bail!("special operations not supported");
        }
        if self.modifier.is_some() {
            bail!("method modifiers are not supported")
        }
        let return_type = ci.resolve_return_type_expression(&self.return_type)?;
        let attributes = MethodAttributes::try_from(self.attributes.as_ref())?;

        let throws = match attributes.get_throws_err() {
            Some(name) => match ci.get_type(name) {
                Some(t) => Some(t),
                None => bail!("unknown type for error: {name}"),
            },
            None => None,
        };

        let takes_self_by_arc = attributes.get_self_by_arc();
        Ok(MethodMetadata {
            module_path: ci.module_path(),
            name: match self.identifier {
                None => bail!("anonymous methods are not supported {:?}", self),
                Some(id) => {
                    let name = id.0.to_string();
                    if name == "new" {
                        bail!("the method name \"new\" is reserved for the default constructor");
                    }
                    name
                }
            },
            // We don't know the name of the containing `Object` at this point, fill it in later.
            self_name: Default::default(),
            is_async: false, // not supported in UDL
            inputs: self.args.body.list.convert(ci)?,
            return_type,
            throws,
            takes_self_by_arc,
            checksum: None,
        })
    }
}

impl APIConverter<TraitMethodMetadata> for weedle::interface::OperationInterfaceMember<'_> {
    fn convert(&self, ci: &mut InterfaceCollector) -> Result<TraitMethodMetadata> {
        if self.special.is_some() {
            bail!("special operations not supported");
        }
        if self.modifier.is_some() {
            bail!("method modifiers are not supported")
        }
        let return_type = ci.resolve_return_type_expression(&self.return_type)?;
        let attributes = MethodAttributes::try_from(self.attributes.as_ref())?;

        let throws = match attributes.get_throws_err() {
            Some(name) => match ci.get_type(name) {
                Some(t) => Some(t),
                None => bail!("unknown type for error: {name}"),
            },
            None => None,
        };

        let takes_self_by_arc = attributes.get_self_by_arc();
        Ok(TraitMethodMetadata {
            module_path: ci.module_path(),
            trait_name: Default::default(), // we'll fill these in later.
            index: Default::default(),
            name: match self.identifier {
                None => bail!("anonymous methods are not supported {:?}", self),
                Some(id) => {
                    let name = id.0.to_string();
                    if name == "new" {
                        bail!("the method name \"new\" is reserved for the default constructor");
                    }
                    name
                }
            },
            is_async: false, // not supported in udl
            inputs: self.args.body.list.convert(ci)?,
            return_type,
            throws,
            takes_self_by_arc,
            checksum: None,
        })
    }
}
