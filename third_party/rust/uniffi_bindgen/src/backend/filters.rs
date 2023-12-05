/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Backend-agnostic askama filters

use crate::interface::{
    AsType, CallbackInterface, ComponentInterface, Enum, FfiType, Function, Object, Record,
};
use askama::Result;
use std::fmt;

// Need to define an error that implements std::error::Error, which neither String nor
// anyhow::Error do.
#[derive(Debug)]
struct UniFFIError {
    message: String,
}

impl UniFFIError {
    fn new(message: String) -> Self {
        Self { message }
    }
}

impl fmt::Display for UniFFIError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.message)
    }
}

impl std::error::Error for UniFFIError {}

macro_rules! lookup_error {
    ($($args:tt)*) => {
        askama::Error::Custom(Box::new(UniFFIError::new(format!($($args)*))))
    }
}

/// Get an Enum definition by name
pub fn get_enum_definition<'a>(ci: &'a ComponentInterface, name: &str) -> Result<&'a Enum> {
    ci.get_enum_definition(name)
        .ok_or_else(|| lookup_error!("enum {name} not found"))
}

/// Get a Record definition by name
pub fn get_record_definition<'a>(ci: &'a ComponentInterface, name: &str) -> Result<&'a Record> {
    ci.get_record_definition(name)
        .ok_or_else(|| lookup_error!("record {name} not found"))
}

/// Get a Function definition by name
pub fn get_function_definition<'a>(ci: &'a ComponentInterface, name: &str) -> Result<&'a Function> {
    ci.get_function_definition(name)
        .ok_or_else(|| lookup_error!("function {name} not found"))
}

/// Get an Object definition by name
pub fn get_object_definition<'a>(ci: &'a ComponentInterface, name: &str) -> Result<&'a Object> {
    ci.get_object_definition(name)
        .ok_or_else(|| lookup_error!("object {name} not found"))
}

/// Get an Callback Interface definition by name
pub fn get_callback_interface_definition<'a>(
    ci: &'a ComponentInterface,
    name: &str,
) -> Result<&'a CallbackInterface> {
    ci.get_callback_interface_definition(name)
        .ok_or_else(|| lookup_error!("callback interface {name} not found"))
}

/// Get the FfiType for a Type
pub fn ffi_type(type_: &impl AsType) -> Result<FfiType, askama::Error> {
    Ok(type_.as_type().into())
}
