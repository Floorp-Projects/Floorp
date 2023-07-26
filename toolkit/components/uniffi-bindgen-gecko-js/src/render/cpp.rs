/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{CallbackIds, Config, FunctionIds, ObjectIds};
use askama::Template;
use extend::ext;
use heck::{ToShoutySnakeCase, ToUpperCamelCase};
use std::collections::HashSet;
use std::iter;
use uniffi_bindgen::interface::{
    CallbackInterface, ComponentInterface, FfiArgument, FfiFunction, FfiType, Object,
};

#[derive(Template)]
#[template(path = "UniFFIScaffolding.cpp", escape = "none")]
pub struct CPPScaffoldingTemplate<'a> {
    // Prefix for each function name in.  This is related to how we handle the test fixtures.  For
    // each function defined in the UniFFI namespace in UniFFI.webidl we:
    //   - Generate a function in to handle it using the real UDL files
    //   - Generate a different function in for handle it using the fixture UDL files
    //   - Have a hand-written stub function that always calls the first function and only calls
    //     the second function in if MOZ_UNIFFI_FIXTURES is defined.
    pub prefix: &'a str,
    pub components: &'a Vec<(ComponentInterface, Config)>,
    pub function_ids: &'a FunctionIds<'a>,
    pub object_ids: &'a ObjectIds<'a>,
    pub callback_ids: &'a CallbackIds<'a>,
}

impl<'a> CPPScaffoldingTemplate<'a> {
    fn has_any_objects(&self) -> bool {
        self.components
            .iter()
            .any(|(ci, _)| ci.object_definitions().len() > 0)
    }
}

// Define extension traits with methods used in our template code

#[ext(name=ComponentInterfaceCppExt)]
pub impl ComponentInterface {
    // C++ pointer type name.  This needs to be a valid C++ type name and unique across all UDL
    // files.
    fn pointer_type(&self, object: &Object) -> String {
        self._pointer_type(object.name())
    }

    fn _pointer_type(&self, name: &str) -> String {
        format!(
            "k{}{}PointerType",
            self.namespace().to_upper_camel_case(),
            name.to_upper_camel_case()
        )
    }

    // Iterate over all functions to expose via the UniFFIScaffolding class
    //
    // This is basically all the user functions, except we don't expose the free methods for
    // objects.  Freeing is handled by the UniFFIPointer class.
    //
    // Note: this function should return `impl Iterator<&FfiFunction>`, but that's not currently
    // allowed for traits.
    fn exposed_functions(&self) -> Vec<&FfiFunction> {
        let excluded: HashSet<_> = self
            .object_definitions()
            .iter()
            .map(|o| o.ffi_object_free().name())
            .chain(
                self.callback_interface_definitions()
                    .iter()
                    .map(|cbi| cbi.ffi_init_callback().name()),
            )
            .collect();
        self.iter_user_ffi_function_definitions()
            .filter(move |f| !excluded.contains(f.name()))
            .collect()
    }

    // ScaffoldingConverter class
    //
    // This is used to convert types between the JS code and Rust
    fn scaffolding_converter(&self, ffi_type: &FfiType) -> String {
        match ffi_type {
            FfiType::RustArcPtr(name) => {
                format!("ScaffoldingObjectConverter<&{}>", self._pointer_type(name),)
            }
            _ => format!("ScaffoldingConverter<{}>", ffi_type.rust_type()),
        }
    }

    // ScaffoldingCallHandler class
    fn scaffolding_call_handler(&self, func: &FfiFunction) -> String {
        let return_param = match func.return_type() {
            Some(return_type) => self.scaffolding_converter(return_type),
            None => "ScaffoldingConverter<void>".to_string(),
        };
        let all_params = iter::once(return_param)
            .chain(
                func.arguments()
                    .into_iter()
                    .map(|a| self.scaffolding_converter(&a.type_())),
            )
            .collect::<Vec<_>>()
            .join(", ");
        return format!("ScaffoldingCallHandler<{}>", all_params);
    }
}

#[ext(name=FFIFunctionCppExt)]
pub impl FfiFunction {
    fn nm(&self) -> String {
        self.name().to_upper_camel_case()
    }

    fn rust_name(&self) -> String {
        self.name().to_string()
    }

    fn rust_return_type(&self) -> String {
        match self.return_type() {
            Some(t) => t.rust_type(),
            None => "void".to_owned(),
        }
    }

    fn rust_arg_list(&self) -> String {
        let mut parts: Vec<String> = self.arguments().iter().map(|a| a.rust_type()).collect();
        parts.push("RustCallStatus*".to_owned());
        parts.join(", ")
    }
}

#[ext(name=FFITypeCppExt)]
pub impl FfiType {
    // Type for the Rust scaffolding code
    fn rust_type(&self) -> String {
        match self {
            FfiType::UInt8 => "uint8_t",
            FfiType::Int8 => "int8_t",
            FfiType::UInt16 => "uint16_t",
            FfiType::Int16 => "int16_t",
            FfiType::UInt32 => "uint32_t",
            FfiType::Int32 => "int32_t",
            FfiType::UInt64 => "uint64_t",
            FfiType::Int64 => "int64_t",
            FfiType::Float32 => "float",
            FfiType::Float64 => "double",
            FfiType::RustBuffer(_) => "RustBuffer",
            FfiType::RustArcPtr(_) => "void *",
            FfiType::ForeignCallback => "ForeignCallback",
            FfiType::ForeignBytes => unimplemented!("ForeignBytes not supported"),
            FfiType::ForeignExecutorHandle => unimplemented!("ForeignExecutorHandle not supported"),
            FfiType::ForeignExecutorCallback => {
                unimplemented!("ForeignExecutorCallback not supported")
            }
            FfiType::FutureCallback { .. } => unimplemented!("FutureCallback not supported"),
            FfiType::FutureCallbackData => unimplemented!("FutureCallbackData not supported"),
        }
        .to_owned()
    }
}

#[ext(name=FFIArgumentCppExt)]
pub impl FfiArgument {
    fn rust_type(&self) -> String {
        self.type_().rust_type()
    }
}

#[ext(name=ObjectCppExt)]
pub impl Object {
    fn nm(&self) -> String {
        self.name().to_upper_camel_case()
    }
}

#[ext(name=CallbackInterfaceCppExt)]
pub impl CallbackInterface {
    fn nm(&self) -> String {
        self.name().to_upper_camel_case()
    }

    /// Name of the static pointer to the JS callback handler
    fn js_handler(&self) -> String {
        format!("JS_CALLBACK_HANDLER_{}", self.name().to_shouty_snake_case())
    }

    /// Name of the C function handler
    fn c_handler(&self, prefix: &str) -> String {
        format!(
            "{prefix}CallbackHandler{}",
            self.name().to_upper_camel_case()
        )
    }
}
