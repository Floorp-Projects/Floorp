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

use std::iter;

use heck::ToUpperCamelCase;
use uniffi_meta::Checksum;

use super::ffi::{FfiArgument, FfiCallbackFunction, FfiField, FfiFunction, FfiStruct, FfiType};
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
    #[checksum_ignore]
    pub(super) docstring: Option<String>,
}

impl CallbackInterface {
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
        self.ffi_init_callback =
            FfiFunction::callback_init(&self.module_path, &self.name, vtable_name(&self.name));
    }

    /// FfiCallbacks to define for our methods.
    pub fn ffi_callbacks(&self) -> Vec<FfiCallbackFunction> {
        ffi_callbacks(&self.name, &self.methods)
    }

    /// The VTable FFI type
    pub fn vtable(&self) -> FfiType {
        FfiType::Struct(vtable_name(&self.name))
    }

    /// the VTable struct to define.
    pub fn vtable_definition(&self) -> FfiStruct {
        vtable_struct(&self.name, &self.methods)
    }

    /// Vec of (ffi_callback, method) pairs
    pub fn vtable_methods(&self) -> Vec<(FfiCallbackFunction, &Method)> {
        self.methods
            .iter()
            .enumerate()
            .map(|(i, method)| (method_ffi_callback(&self.name, method, i), method))
            .collect()
    }

    pub fn iter_types(&self) -> TypeIterator<'_> {
        Box::new(self.methods.iter().flat_map(Method::iter_types))
    }

    pub fn docstring(&self) -> Option<&str> {
        self.docstring.as_deref()
    }

    pub fn has_async_method(&self) -> bool {
        self.methods.iter().any(Method::is_async)
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

impl TryFrom<uniffi_meta::CallbackInterfaceMetadata> for CallbackInterface {
    type Error = anyhow::Error;

    fn try_from(meta: uniffi_meta::CallbackInterfaceMetadata) -> anyhow::Result<Self> {
        Ok(Self {
            name: meta.name,
            module_path: meta.module_path,
            methods: Default::default(),
            ffi_init_callback: Default::default(),
            docstring: meta.docstring.clone(),
        })
    }
}

/// [FfiCallbackFunction] functions for the methods of a callback/trait interface
pub fn ffi_callbacks(trait_name: &str, methods: &[Method]) -> Vec<FfiCallbackFunction> {
    methods
        .iter()
        .enumerate()
        .map(|(i, method)| method_ffi_callback(trait_name, method, i))
        .collect()
}

pub fn method_ffi_callback(trait_name: &str, method: &Method, index: usize) -> FfiCallbackFunction {
    if !method.is_async() {
        FfiCallbackFunction {
            name: method_ffi_callback_name(trait_name, index),
            arguments: iter::once(FfiArgument::new("uniffi_handle", FfiType::UInt64))
                .chain(method.arguments().into_iter().map(Into::into))
                .chain(iter::once(match method.return_type() {
                    Some(t) => FfiArgument::new("uniffi_out_return", FfiType::from(t).reference()),
                    None => FfiArgument::new("uniffi_out_return", FfiType::VoidPointer),
                }))
                .collect(),
            has_rust_call_status_arg: true,
            return_type: None,
        }
    } else {
        let completion_callback =
            ffi_foreign_future_complete(method.return_type().map(FfiType::from));
        FfiCallbackFunction {
            name: method_ffi_callback_name(trait_name, index),
            arguments: iter::once(FfiArgument::new("uniffi_handle", FfiType::UInt64))
                .chain(method.arguments().into_iter().map(Into::into))
                .chain([
                    FfiArgument::new(
                        "uniffi_future_callback",
                        FfiType::Callback(completion_callback.name),
                    ),
                    FfiArgument::new("uniffi_callback_data", FfiType::UInt64),
                    FfiArgument::new(
                        "uniffi_out_return",
                        FfiType::Struct("ForeignFuture".to_owned()).reference(),
                    ),
                ])
                .collect(),
            has_rust_call_status_arg: false,
            return_type: None,
        }
    }
}

/// Result struct to pass to the completion callback for async methods
pub fn foreign_future_ffi_result_struct(return_ffi_type: Option<FfiType>) -> FfiStruct {
    let return_type_name =
        FfiType::return_type_name(return_ffi_type.as_ref()).to_upper_camel_case();
    FfiStruct {
        name: format!("ForeignFutureStruct{return_type_name}"),
        fields: match return_ffi_type {
            Some(return_ffi_type) => vec![
                FfiField::new("return_value", return_ffi_type),
                FfiField::new("call_status", FfiType::RustCallStatus),
            ],
            None => vec![
                // In Rust, `return_value` is `()` -- a ZST.
                // ZSTs are not valid in `C`, but they also take up 0 space.
                // Skip the `return_value` field to make the layout correct.
                FfiField::new("call_status", FfiType::RustCallStatus),
            ],
        },
    }
}

/// Definition for callback functions to complete an async callback interface method
pub fn ffi_foreign_future_complete(return_ffi_type: Option<FfiType>) -> FfiCallbackFunction {
    let return_type_name =
        FfiType::return_type_name(return_ffi_type.as_ref()).to_upper_camel_case();
    FfiCallbackFunction {
        name: format!("ForeignFutureComplete{return_type_name}"),
        arguments: vec![
            FfiArgument::new("callback_data", FfiType::UInt64),
            FfiArgument::new(
                "result",
                FfiType::Struct(format!("ForeignFutureStruct{return_type_name}")),
            ),
        ],
        return_type: None,
        has_rust_call_status_arg: false,
    }
}

/// [FfiStruct] for a callback/trait interface VTable
///
/// This struct has a FfiCallbackFunction field for each method, plus extra fields for special
/// methods
pub fn vtable_struct(trait_name: &str, methods: &[Method]) -> FfiStruct {
    FfiStruct {
        name: vtable_name(trait_name),
        fields: methods
            .iter()
            .enumerate()
            .map(|(i, method)| {
                FfiField::new(
                    method.name(),
                    FfiType::Callback(format!("CallbackInterface{trait_name}Method{i}")),
                )
            })
            .chain([FfiField::new(
                "uniffi_free",
                FfiType::Callback("CallbackInterfaceFree".to_owned()),
            )])
            .collect(),
    }
}

pub fn method_ffi_callback_name(trait_name: &str, index: usize) -> String {
    format!("CallbackInterface{trait_name}Method{index}")
}

pub fn vtable_name(trait_name: &str) -> String {
    format!("VTableCallbackInterface{trait_name}")
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

    #[test]
    fn test_docstring_callback_interface() {
        const UDL: &str = r#"
            namespace test{};
            /// informative docstring
            callback interface Testing { };
        "#;
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
        assert_eq!(
            ci.get_callback_interface_definition("Testing")
                .unwrap()
                .docstring()
                .unwrap(),
            "informative docstring"
        );
    }
}
