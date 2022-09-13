/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Callback Interface definitions for a `ComponentInterface`.
//!
//! This module converts callback interface definitions from UDL into structures that
//! can be added to a `ComponentInterface`. A declaration in the UDL like this:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! callback interface Example {
//!   string hello();
//! };
//! # "##)?;
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! Will result in a [`CallbackInterface`] member being added to the resulting
//! [`ComponentInterface`]:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! # callback interface Example {
//! #  string hello();
//! # };
//! # "##)?;
//! let callback = ci.get_callback_interface_definition("Example").unwrap();
//! assert_eq!(callback.name(), "Example");
//! assert_eq!(callback.methods()[0].name(), "hello");
//! # Ok::<(), anyhow::Error>(())
//! ```

use std::hash::{Hash, Hasher};

use anyhow::{bail, Result};

use super::ffi::{FFIArgument, FFIFunction, FFIType};
use super::object::Method;
use super::types::{Type, TypeIterator};
use super::{APIConverter, ComponentInterface};

#[derive(Debug, Clone)]
pub struct CallbackInterface {
    pub(super) name: String,
    pub(super) methods: Vec<Method>,
    pub(super) ffi_init_callback: FFIFunction,
}

impl CallbackInterface {
    fn new(name: String) -> CallbackInterface {
        CallbackInterface {
            name,
            methods: Default::default(),
            ffi_init_callback: Default::default(),
        }
    }

    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn type_(&self) -> Type {
        Type::CallbackInterface(self.name.clone())
    }

    pub fn methods(&self) -> Vec<&Method> {
        self.methods.iter().collect()
    }

    pub fn ffi_init_callback(&self) -> &FFIFunction {
        &self.ffi_init_callback
    }

    pub(super) fn derive_ffi_funcs(&mut self, ci_prefix: &str) {
        self.ffi_init_callback.name = format!("ffi_{ci_prefix}_{}_init_callback", self.name);
        self.ffi_init_callback.arguments = vec![FFIArgument {
            name: "callback_stub".to_string(),
            type_: FFIType::ForeignCallback,
        }];
        self.ffi_init_callback.return_type = None;
    }

    pub fn iter_types(&self) -> TypeIterator<'_> {
        Box::new(self.methods.iter().flat_map(Method::iter_types))
    }
}

impl Hash for CallbackInterface {
    fn hash<H: Hasher>(&self, state: &mut H) {
        // We don't include the FFIFunc in the hash calculation, because:
        //  - it is entirely determined by the other fields,
        //    so excluding it is safe.
        //  - its `name` property includes a checksum derived from  the very
        //    hash value we're trying to calculate here, so excluding it
        //    avoids a weird circular depenendency in the calculation.
        self.name.hash(state);
        self.methods.hash(state);
    }
}

impl APIConverter<CallbackInterface> for weedle::CallbackInterfaceDefinition<'_> {
    fn convert(&self, ci: &mut ComponentInterface) -> Result<CallbackInterface> {
        if self.attributes.is_some() {
            bail!("callback interface attributes are not supported yet");
        }
        if self.inheritance.is_some() {
            bail!("callback interface inheritence is not supported");
        }
        let mut object = CallbackInterface::new(self.identifier.0.to_string());
        for member in &self.members.body {
            match member {
                weedle::interface::InterfaceMember::Operation(t) => {
                    let mut method: Method = t.convert(ci)?;
                    method.object_name = object.name.clone();
                    object.methods.push(method);
                }
                _ => bail!(
                    "no support for callback interface member type {:?} yet",
                    member
                ),
            }
        }
        Ok(object)
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_empty_interface() {
        const UDL: &str = r#"
            namespace test{};
            // Weird, but allowed.
            callback interface Testing {};
        "#;
        let ci = ComponentInterface::from_webidl(UDL).unwrap();
        assert_eq!(ci.callback_interface_definitions().len(), 1);
        assert_eq!(
            ci.get_callback_interface_definition("Testing")
                .unwrap()
                .methods()
                .len(),
            0
        );
    }

    #[test]
    fn test_multiple_interfaces() {
        const UDL: &str = r#"
            namespace test{};
            callback interface One {
                void one();
            };
            callback interface Two {
                u32 two();
                u64 too();
            };
        "#;
        let ci = ComponentInterface::from_webidl(UDL).unwrap();
        assert_eq!(ci.callback_interface_definitions().len(), 2);

        let callbacks_one = ci.get_callback_interface_definition("One").unwrap();
        assert_eq!(callbacks_one.methods().len(), 1);
        assert_eq!(callbacks_one.methods()[0].name(), "one");

        let callbacks_two = ci.get_callback_interface_definition("Two").unwrap();
        assert_eq!(callbacks_two.methods().len(), 2);
        assert_eq!(callbacks_two.methods()[0].name(), "two");
        assert_eq!(callbacks_two.methods()[1].name(), "too");
    }
}
