/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::Result;
use askama::Template;
use std::borrow::Borrow;

use super::interface::*;
use heck::{ToShoutySnakeCase, ToSnakeCase};

#[derive(Template)]
#[template(syntax = "rs", escape = "none", path = "scaffolding_template.rs")]
pub struct RustScaffolding<'a> {
    ci: &'a ComponentInterface,
    uniffi_version: &'static str,
}
impl<'a> RustScaffolding<'a> {
    pub fn new(ci: &'a ComponentInterface) -> Self {
        Self {
            ci,
            uniffi_version: crate::BINDGEN_VERSION,
        }
    }
}
mod filters {
    use super::*;

    pub fn type_rs(type_: &Type) -> Result<String, askama::Error> {
        Ok(match type_ {
            Type::Int8 => "i8".into(),
            Type::UInt8 => "u8".into(),
            Type::Int16 => "i16".into(),
            Type::UInt16 => "u16".into(),
            Type::Int32 => "i32".into(),
            Type::UInt32 => "u32".into(),
            Type::Int64 => "i64".into(),
            Type::UInt64 => "u64".into(),
            Type::Float32 => "f32".into(),
            Type::Float64 => "f64".into(),
            Type::Boolean => "bool".into(),
            Type::String => "String".into(),
            Type::Bytes => "Vec<u8>".into(),
            Type::Timestamp => "std::time::SystemTime".into(),
            Type::Duration => "std::time::Duration".into(),
            Type::Enum(name) | Type::Record(name) => format!("r#{name}"),
            Type::Object { name, imp } => format!("std::sync::Arc<{}>", imp.rust_name_for(name)),
            Type::CallbackInterface(name) => format!("Box<dyn r#{name}>"),
            Type::ForeignExecutor => "::uniffi::ForeignExecutor".into(),
            Type::Optional(t) => format!("std::option::Option<{}>", type_rs(t)?),
            Type::Sequence(t) => format!("std::vec::Vec<{}>", type_rs(t)?),
            Type::Map(k, v) => format!(
                "std::collections::HashMap<{}, {}>",
                type_rs(k)?,
                type_rs(v)?
            ),
            Type::Custom { name, .. } => format!("r#{name}"),
            Type::External {
                name,
                kind: ExternalKind::Interface,
                ..
            } => format!("::std::sync::Arc<r#{name}>"),
            Type::External { name, .. } => format!("r#{name}"),
        })
    }

    pub fn type_ffi(type_: &FfiType) -> Result<String, askama::Error> {
        Ok(match type_ {
            FfiType::Int8 => "i8".into(),
            FfiType::UInt8 => "u8".into(),
            FfiType::Int16 => "i16".into(),
            FfiType::UInt16 => "u16".into(),
            FfiType::Int32 => "i32".into(),
            FfiType::UInt32 => "u32".into(),
            FfiType::Int64 => "i64".into(),
            FfiType::UInt64 => "u64".into(),
            FfiType::Float32 => "f32".into(),
            FfiType::Float64 => "f64".into(),
            FfiType::RustArcPtr(_) => "*const std::os::raw::c_void".into(),
            FfiType::RustBuffer(_) => "::uniffi::RustBuffer".into(),
            FfiType::ForeignBytes => "::uniffi::ForeignBytes".into(),
            FfiType::ForeignCallback => "::uniffi::ForeignCallback".into(),
            FfiType::ForeignExecutorHandle => "::uniffi::ForeignExecutorHandle".into(),
            FfiType::FutureCallback { return_type } => {
                format!("::uniffi::FutureCallback<{}>", type_ffi(return_type)?)
            }
            FfiType::FutureCallbackData => "*const ()".into(),
            FfiType::ForeignExecutorCallback => "::uniffi::ForeignExecutorCallback".into(),
        })
    }

    // Map a type to Rust code that specifies the FfiConverter implementation.
    //
    // This outputs something like `<TheFfiConverterStruct as FfiConverter>`
    pub fn ffi_converter(type_: &Type) -> Result<String, askama::Error> {
        Ok(match type_ {
            Type::External {
                name,
                kind: ExternalKind::Interface,
                ..
            } => {
                format!("<::std::sync::Arc<r#{name}> as ::uniffi::FfiConverter<crate::UniFfiTag>>")
            }
            _ => format!(
                "<{} as ::uniffi::FfiConverter<crate::UniFfiTag>>",
                type_rs(type_)?
            ),
        })
    }

    pub fn return_type<T: Callable>(callable: &T) -> Result<String, askama::Error> {
        let return_type = match callable.return_type() {
            Some(t) => type_rs(&t)?,
            None => "()".to_string(),
        };
        match callable.throws_type() {
            Some(t) => type_rs(&t)?,
            None => "()".to_string(),
        };
        Ok(match callable.throws_type() {
            Some(e) => format!("::std::result::Result<{return_type}, {}>", type_rs(&e)?),
            None => return_type,
        })
    }

    // Map return types to their fully-qualified `FfiConverter` impl.
    pub fn return_ffi_converter<T: Callable>(callable: &T) -> Result<String, askama::Error> {
        let return_type = return_type(callable)?;
        Ok(format!(
            "<{return_type} as ::uniffi::FfiConverter<crate::UniFfiTag>>"
        ))
    }

    // Turns a `crate-name` into the `crate_name` the .rs code needs to specify.
    pub fn crate_name_rs(nm: &str) -> Result<String, askama::Error> {
        Ok(format!("r#{}", nm.to_string().to_snake_case()))
    }
}
