/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{FunctionIds, ObjectIds};
use askama::Template;
use extend::ext;
use heck::ToUpperCamelCase;
use std::collections::HashSet;
use std::iter;
use uniffi_bindgen::interface::{ComponentInterface, FFIArgument, FFIFunction, FFIType, Object};

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
    pub ci_list: &'a Vec<ComponentInterface>,
    pub function_ids: &'a FunctionIds<'a>,
    pub object_ids: &'a ObjectIds<'a>,
}

impl<'a> CPPScaffoldingTemplate<'a> {
    fn has_any_objects(&self) -> bool {
        self.ci_list
            .iter()
            .any(|ci| ci.object_definitions().len() > 0)
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
    // Note: this function should return `impl Iterator<&FFIFunction>`, but that's not currently
    // allowed for traits.
    fn exposed_functions(&self) -> Vec<&FFIFunction> {
        let excluded: HashSet<_> = self
            .object_definitions()
            .iter()
            .map(|o| o.ffi_object_free().name())
            .collect();
        self.iter_user_ffi_function_definitions()
            .filter(move |f| !excluded.contains(f.name()))
            .collect()
    }

    // ScaffoldingConverter class
    //
    // This is used to convert types between the JS code and Rust
    fn scaffolding_converter(&self, ffi_type: &FFIType) -> String {
        match ffi_type {
            FFIType::RustArcPtr(name) => {
                format!("ScaffoldingObjectConverter<&{}>", self._pointer_type(name),)
            }
            _ => format!("ScaffoldingConverter<{}>", ffi_type.rust_type()),
        }
    }

    // ScaffoldingCallHandler class
    fn scaffolding_call_handler(&self, func: &FFIFunction) -> String {
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
pub impl FFIFunction {
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
pub impl FFIType {
    // Type for the Rust scaffolding code
    fn rust_type(&self) -> String {
        match self {
            FFIType::UInt8 => "uint8_t",
            FFIType::Int8 => "int8_t",
            FFIType::UInt16 => "uint16_t",
            FFIType::Int16 => "int16_t",
            FFIType::UInt32 => "uint32_t",
            FFIType::Int32 => "int32_t",
            FFIType::UInt64 => "uint64_t",
            FFIType::Int64 => "int64_t",
            FFIType::Float32 => "float",
            FFIType::Float64 => "double",
            FFIType::RustBuffer => "RustBuffer",
            FFIType::RustArcPtr(_) => "void *",
            FFIType::ForeignBytes => unimplemented!("ForeignBytes not supported"),
            FFIType::ForeignCallback => unimplemented!("ForeignCallback not supported"),
        }
        .to_owned()
    }
}

#[ext(name=FFIArgumentCppExt)]
pub impl FFIArgument {
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
