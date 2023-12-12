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
//! # "##, "crate_name")?;
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! Will result in a [`CallbackInterface`] member being added to the resulting
//! [`crate::ComponentInterface`]:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! # callback interface Example {
//! #  string hello();
//! # };
//! # "##, "crate_name")?;
//! let callback = ci.get_callback_interface_definition("Example").unwrap();
//! assert_eq!(callback.name(), "Example");
//! assert_eq!(callback.methods()[0].name(), "hello");
//! # Ok::<(), anyhow::Error>(())
//! ```

use uniffi_meta::Checksum;

use super::ffi::{FfiArgument, FfiFunction, FfiType};
use super::object::Method;
use super::{AsType, Type, TypeIterator};

#[derive(Debug, Clone, Checksum)]
pub struct CallbackInterface {
    pub(super) name: String,
    pub(super) module_path: String,
    pub(super) methods: Vec<Method>,
    // We don't include the FFIFunc in the hash calculation, because:
    //  - it is entirely determined by the other fields,
    //    so excluding it is safe.
    //  - its `name` property includes a checksum derived from  the very
    //    hash value we're trying to calculate here, so excluding it
    //    avoids a weird circular dependency in the calculation.
    #[checksum_ignore]
    pub(super) ffi_init_callback: FfiFunction,
}

impl CallbackInterface {
    pub fn new(name: String, module_path: String) -> CallbackInterface {
        CallbackInterface {
            name,
            module_path,
            methods: Default::default(),
            ffi_init_callback: Default::default(),
        }
    }

    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn methods(&self) -> Vec<&Method> {
        self.methods.iter().collect()
    }

    pub fn ffi_init_callback(&self) -> &FfiFunction {
        &self.ffi_init_callback
    }

    pub(super) fn derive_ffi_funcs(&mut self) {
        self.ffi_init_callback.name =
            uniffi_meta::init_callback_fn_symbol_name(&self.module_path, &self.name);
        self.ffi_init_callback.arguments = vec![FfiArgument {
            name: "callback_stub".to_string(),
            type_: FfiType::ForeignCallback,
        }];
        self.ffi_init_callback.return_type = None;
    }

    pub fn iter_types(&self) -> TypeIterator<'_> {
        Box::new(self.methods.iter().flat_map(Method::iter_types))
    }
}

impl AsType for CallbackInterface {
    fn as_type(&self) -> Type {
        Type::CallbackInterface {
            name: self.name.clone(),
            module_path: self.module_path.clone(),
        }
    }
}

#[cfg(test)]
mod test {
    use super::super::ComponentInterface;

    #[test]
    fn test_empty_interface() {
        const UDL: &str = r#"
            namespace test{};
            // Weird, but allowed.
            callback interface Testing {};
        "#;
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
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
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
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
