// Copyright 2017 GFX developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use super::*;

use cocoa::foundation::NSUInteger;
use foreign_types::ForeignType;
use objc::runtime::{Object, NO, YES};
use std::ffi::CStr;

pub enum MTLVertexAttribute {}

foreign_obj_type! {
    type CType = MTLVertexAttribute;
    pub struct VertexAttribute;
    pub struct VertexAttributeRef;
}

impl VertexAttributeRef {
    pub fn name(&self) -> &str {
        unsafe {
            let name = msg_send![self, name];
            crate::nsstring_as_str(name)
        }
    }

    pub fn attribute_index(&self) -> u64 {
        unsafe { msg_send![self, attributeIndex] }
    }

    pub fn attribute_type(&self) -> MTLDataType {
        unsafe { msg_send![self, attributeType] }
    }

    pub fn is_active(&self) -> bool {
        unsafe {
            match msg_send![self, isActive] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }
}

#[repr(u64)]
#[derive(Debug)]
pub enum MTLFunctionType {
    Vertex = 1,
    Fragment = 2,
    Kernel = 3,
}

pub enum MTLFunction {}

foreign_obj_type! {
    type CType = MTLFunction;
    pub struct Function;
    pub struct FunctionRef;
}

impl FunctionRef {
    pub fn name(&self) -> &str {
        unsafe {
            let name = msg_send![self, name];
            crate::nsstring_as_str(name)
        }
    }

    pub fn function_type(&self) -> MTLFunctionType {
        unsafe { msg_send![self, functionType] }
    }

    pub fn vertex_attributes(&self) -> &Array<VertexAttribute> {
        unsafe { msg_send![self, vertexAttributes] }
    }

    pub fn new_argument_encoder(&self, buffer_index: NSUInteger) -> ArgumentEncoder {
        unsafe {
            let ptr = msg_send![self, newArgumentEncoderWithBufferIndex: buffer_index];
            ArgumentEncoder::from_ptr(ptr)
        }
    }

    pub fn function_constants_dictionary(&self) -> *mut Object {
        unsafe { msg_send![self, functionConstantsDictionary] }
    }
}

#[repr(u64)]
#[allow(non_camel_case_types)]
#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq, Ord, PartialOrd)]
pub enum MTLLanguageVersion {
    V1_0 = 0x10000,
    V1_1 = 0x10001,
    V1_2 = 0x10002,
    V2_0 = 0x20000,
    V2_1 = 0x20001,
    V2_2 = 0x20002,
}

pub enum MTLFunctionConstantValues {}

foreign_obj_type! {
    type CType = MTLFunctionConstantValues;
    pub struct FunctionConstantValues;
    pub struct FunctionConstantValuesRef;
}

impl FunctionConstantValues {
    pub fn new() -> Self {
        unsafe {
            let class = class!(MTLFunctionConstantValues);
            msg_send![class, new]
        }
    }
}

impl FunctionConstantValuesRef {
    pub unsafe fn set_constant_value_at_index(
        &self,
        index: NSUInteger,
        ty: MTLDataType,
        value: *const std::os::raw::c_void,
    ) {
        msg_send![self, setConstantValue:value type:ty atIndex:index]
    }
}

pub enum MTLCompileOptions {}

foreign_obj_type! {
    type CType = MTLCompileOptions;
    pub struct CompileOptions;
    pub struct CompileOptionsRef;
}

impl CompileOptions {
    pub fn new() -> Self {
        unsafe {
            let class = class!(MTLCompileOptions);
            msg_send![class, new]
        }
    }
}

impl CompileOptionsRef {
    pub unsafe fn preprocessor_defines(&self) -> *mut Object {
        msg_send![self, preprocessorMacros]
    }

    pub unsafe fn set_preprocessor_defines(&self, defines: *mut Object) {
        msg_send![self, setPreprocessorMacros: defines]
    }

    pub fn is_fast_math_enabled(&self) -> bool {
        unsafe {
            match msg_send![self, fastMathEnabled] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn set_fast_math_enabled(&self, enabled: bool) {
        unsafe { msg_send![self, setFastMathEnabled: enabled] }
    }

    pub fn language_version(&self) -> MTLLanguageVersion {
        unsafe { msg_send![self, languageVersion] }
    }

    pub fn set_language_version(&self, version: MTLLanguageVersion) {
        unsafe { msg_send![self, setLanguageVersion: version] }
    }
}

#[repr(u64)]
#[allow(non_camel_case_types)]
pub enum MTLLibraryError {
    Unsupported = 1,
    Internal = 2,
    CompileFailure = 3,
    CompileWarning = 4,
}

pub enum MTLLibrary {}

foreign_obj_type! {
    type CType = MTLLibrary;
    pub struct Library;
    pub struct LibraryRef;
}

impl LibraryRef {
    pub fn label(&self) -> &str {
        unsafe {
            let label = msg_send![self, label];
            crate::nsstring_as_str(label)
        }
    }

    pub fn set_label(&self, label: &str) {
        unsafe {
            let nslabel = crate::nsstring_from_str(label);
            let () = msg_send![self, setLabel: nslabel];
        }
    }

    pub fn get_function(
        &self,
        name: &str,
        constants: Option<FunctionConstantValues>,
    ) -> Result<Function, String> {
        unsafe {
            let nsname = crate::nsstring_from_str(name);

            let function: *mut MTLFunction = match constants {
                Some(c) => try_objc! { err => msg_send![self,
                    newFunctionWithName: nsname.as_ref()
                    constantValues: c.as_ref()
                    error: &mut err
                ]},
                None => msg_send![self, newFunctionWithName: nsname.as_ref()],
            };

            if !function.is_null() {
                Ok(Function::from_ptr(function))
            } else {
                Err(format!("Function '{}' does not exist", name))
            }
        }
    }

    pub fn function_names(&self) -> Vec<String> {
        unsafe {
            let names: *mut Object = msg_send![self, functionNames];
            let count: NSUInteger = msg_send![names, count];
            let ret = (0..count)
                .map(|i| {
                    let name = msg_send![names, objectAtIndex: i];
                    nsstring_as_str(name).to_string()
                })
                .collect();
            let () = msg_send![names, release];
            ret
        }
    }
}
