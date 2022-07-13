/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::Result;
use askama::Template;
use heck::{ToShoutySnakeCase, ToSnakeCase, ToUpperCamelCase};
use serde::{Deserialize, Serialize};
use std::borrow::Borrow;
use std::collections::{HashMap, HashSet};

use crate::backend::{CodeDeclaration, CodeOracle, CodeType, TemplateExpression, TypeIdentifier};
use crate::interface::*;
use crate::MergeWith;

mod callback_interface;
mod compounds;
mod custom;
mod enum_;
mod error;
mod external;
mod function;
mod miscellany;
mod object;
mod primitives;
mod record;

// Config options to customize the generated python.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct Config {
    cdylib_name: Option<String>,
    #[serde(default)]
    custom_types: HashMap<String, CustomTypeConfig>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct CustomTypeConfig {
    // This `CustomTypeConfig` doesn't have a `type_name` like the others -- which is why we have
    // separate structs rather than a shared one.
    imports: Option<Vec<String>>,
    into_custom: TemplateExpression,
    from_custom: TemplateExpression,
}

impl Config {
    pub fn cdylib_name(&self) -> String {
        if let Some(cdylib_name) = &self.cdylib_name {
            cdylib_name.clone()
        } else {
            "uniffi".into()
        }
    }
}

impl From<&ComponentInterface> for Config {
    fn from(ci: &ComponentInterface) -> Self {
        Config {
            cdylib_name: Some(format!("uniffi_{}", ci.namespace())),
            custom_types: HashMap::new(),
        }
    }
}

impl MergeWith for Config {
    fn merge_with(&self, other: &Self) -> Self {
        Config {
            cdylib_name: self.cdylib_name.merge_with(&other.cdylib_name),
            custom_types: self.custom_types.merge_with(&other.custom_types),
        }
    }
}

// Generate python bindings for the given ComponentInterface, as a string.
pub fn generate_python_bindings(config: &Config, ci: &ComponentInterface) -> Result<String> {
    PythonWrapper::new(PythonCodeOracle, config.clone(), ci)
        .render()
        .map_err(|_| anyhow::anyhow!("failed to render python bindings"))
}

#[derive(Template)]
#[template(syntax = "py", escape = "none", path = "wrapper.py")]
pub struct PythonWrapper<'a> {
    ci: &'a ComponentInterface,
    config: Config,
    oracle: PythonCodeOracle,
}
impl<'a> PythonWrapper<'a> {
    pub fn new(oracle: PythonCodeOracle, config: Config, ci: &'a ComponentInterface) -> Self {
        Self { oracle, config, ci }
    }

    pub fn members(&self) -> Vec<Box<dyn CodeDeclaration + 'a>> {
        let ci = self.ci;
        vec![
            Box::new(callback_interface::PythonCallbackInterfaceRuntime::new(ci))
                as Box<dyn CodeDeclaration>,
        ]
        .into_iter()
        .chain(
            ci.iter_enum_definitions().into_iter().map(|inner| {
                Box::new(enum_::PythonEnum::new(inner, ci)) as Box<dyn CodeDeclaration>
            }),
        )
        .chain(ci.iter_function_definitions().into_iter().map(|inner| {
            Box::new(function::PythonFunction::new(inner, ci)) as Box<dyn CodeDeclaration>
        }))
        .chain(ci.iter_object_definitions().into_iter().map(|inner| {
            Box::new(object::PythonObject::new(inner, ci)) as Box<dyn CodeDeclaration>
        }))
        .chain(ci.iter_record_definitions().into_iter().map(|inner| {
            Box::new(record::PythonRecord::new(inner, ci)) as Box<dyn CodeDeclaration>
        }))
        .chain(
            ci.iter_error_definitions().into_iter().map(|inner| {
                Box::new(error::PythonError::new(inner, ci)) as Box<dyn CodeDeclaration>
            }),
        )
        .chain(
            ci.iter_callback_interface_definitions()
                .into_iter()
                .map(|inner| {
                    Box::new(callback_interface::PythonCallbackInterface::new(inner, ci))
                        as Box<dyn CodeDeclaration>
                }),
        )
        .chain(ci.iter_custom_types().into_iter().map(|(name, type_)| {
            let config = self.config.custom_types.get(&name).cloned();
            Box::new(custom::PythonCustomType::new(name, type_, config)) as Box<dyn CodeDeclaration>
        }))
        .collect()
    }

    pub fn initialization_code(&self) -> Vec<String> {
        let oracle = &self.oracle;
        self.members()
            .into_iter()
            .filter_map(|member| member.initialization_code(oracle))
            .collect()
    }

    pub fn declaration_code(&self) -> Vec<String> {
        let oracle = &self.oracle;
        self.members()
            .into_iter()
            .filter_map(|member| member.definition_code(oracle))
            .chain(
                self.ci
                    .iter_types()
                    .into_iter()
                    .filter_map(|type_| oracle.find(&type_).helper_code(oracle)),
            )
            .collect()
    }

    pub fn imports(&self) -> Vec<String> {
        let oracle = &self.oracle;
        let mut imports: Vec<String> = self
            .members()
            .into_iter()
            .filter_map(|member| member.imports(oracle))
            .flatten()
            .chain(
                self.ci
                    .iter_types()
                    .into_iter()
                    .filter_map(|type_| oracle.find(&type_).imports(oracle))
                    .flatten(),
            )
            .collect::<HashSet<String>>()
            .into_iter()
            .collect();

        imports.sort();
        imports
    }
}

#[derive(Clone, Default)]
pub struct PythonCodeOracle;

impl PythonCodeOracle {
    fn create_code_type(&self, type_: TypeIdentifier) -> Box<dyn CodeType> {
        // I really want access to the ComponentInterface here so I can look up the interface::{Enum, Record, Error, Object, etc}
        // However, there's some violence and gore I need to do to (temporarily) make the oracle usable from filters.

        // Some refactor of the templates is needed to make progress here: I think most of the filter functions need to take an &dyn CodeOracle
        match type_ {
            Type::UInt8 => Box::new(primitives::UInt8CodeType),
            Type::Int8 => Box::new(primitives::Int8CodeType),
            Type::UInt16 => Box::new(primitives::UInt16CodeType),
            Type::Int16 => Box::new(primitives::Int16CodeType),
            Type::UInt32 => Box::new(primitives::UInt32CodeType),
            Type::Int32 => Box::new(primitives::Int32CodeType),
            Type::UInt64 => Box::new(primitives::UInt64CodeType),
            Type::Int64 => Box::new(primitives::Int64CodeType),
            Type::Float32 => Box::new(primitives::Float32CodeType),
            Type::Float64 => Box::new(primitives::Float64CodeType),
            Type::Boolean => Box::new(primitives::BooleanCodeType),
            Type::String => Box::new(primitives::StringCodeType),

            Type::Timestamp => Box::new(miscellany::TimestampCodeType),
            Type::Duration => Box::new(miscellany::DurationCodeType),

            Type::Enum(id) => Box::new(enum_::EnumCodeType::new(id)),
            Type::Object(id) => Box::new(object::ObjectCodeType::new(id)),
            Type::Record(id) => Box::new(record::RecordCodeType::new(id)),
            Type::Error(id) => Box::new(error::ErrorCodeType::new(id)),
            Type::CallbackInterface(id) => {
                Box::new(callback_interface::CallbackInterfaceCodeType::new(id))
            }

            Type::Optional(ref inner) => {
                let outer = type_.clone();
                let inner = *inner.to_owned();
                Box::new(compounds::OptionalCodeType::new(inner, outer))
            }
            Type::Sequence(ref inner) => {
                let outer = type_.clone();
                let inner = *inner.to_owned();
                Box::new(compounds::SequenceCodeType::new(inner, outer))
            }
            Type::Map(ref key, ref value) => {
                let outer = type_.clone();
                let key = *key.to_owned();
                let value = *value.to_owned();
                Box::new(compounds::MapCodeType::new(key, value, outer))
            }
            Type::External { name, crate_name } => {
                Box::new(external::ExternalCodeType::new(name, crate_name))
            }
            Type::Custom { name, .. } => Box::new(custom::CustomCodeType::new(name)),
        }
    }
}

impl CodeOracle for PythonCodeOracle {
    fn find(&self, type_: &TypeIdentifier) -> Box<dyn CodeType> {
        self.create_code_type(type_.clone())
    }

    /// Get the idiomatic Python rendering of a class name (for enums, records, errors, etc).
    fn class_name(&self, nm: &str) -> String {
        nm.to_string().to_upper_camel_case()
    }

    /// Get the idiomatic Python rendering of a function name.
    fn fn_name(&self, nm: &str) -> String {
        nm.to_string().to_snake_case()
    }

    /// Get the idiomatic Python rendering of a variable name.
    fn var_name(&self, nm: &str) -> String {
        nm.to_string().to_snake_case()
    }

    /// Get the idiomatic Python rendering of an individual enum variant.
    fn enum_variant_name(&self, nm: &str) -> String {
        nm.to_string().to_shouty_snake_case()
    }

    /// Get the idiomatic Python rendering of an exception name
    ///
    /// This replaces "Error" at the end of the name with "Exception".  Rust code typically uses
    /// "Error" for any type of error but in the Java world, "Error" means a non-recoverable error
    /// and is distinguished from an "Exception".
    fn error_name(&self, nm: &str) -> String {
        let name = nm.to_string();
        match name.strip_suffix("Error") {
            None => name,
            Some(stripped) => {
                let mut py_exc_name = stripped.to_owned();
                py_exc_name.push_str("Exception");
                py_exc_name
            }
        }
    }

    fn ffi_type_label(&self, ffi_type: &FFIType) -> String {
        match ffi_type {
            FFIType::Int8 => "ctypes.c_int8".to_string(),
            FFIType::UInt8 => "ctypes.c_uint8".to_string(),
            FFIType::Int16 => "ctypes.c_int16".to_string(),
            FFIType::UInt16 => "ctypes.c_uint16".to_string(),
            FFIType::Int32 => "ctypes.c_int32".to_string(),
            FFIType::UInt32 => "ctypes.c_uint32".to_string(),
            FFIType::Int64 => "ctypes.c_int64".to_string(),
            FFIType::UInt64 => "ctypes.c_uint64".to_string(),
            FFIType::Float32 => "ctypes.c_float".to_string(),
            FFIType::Float64 => "ctypes.c_double".to_string(),
            FFIType::RustArcPtr => "ctypes.c_void_p".to_string(),
            FFIType::RustBuffer => "RustBuffer".to_string(),
            FFIType::ForeignBytes => "ForeignBytes".to_string(),
            FFIType::ForeignCallback => "FOREIGN_CALLBACK_T".to_string(),
        }
    }
}

pub mod filters {
    use super::*;

    fn oracle() -> &'static PythonCodeOracle {
        &PythonCodeOracle
    }

    pub fn type_name(codetype: &impl CodeType) -> Result<String, askama::Error> {
        let oracle = oracle();
        Ok(codetype.type_label(oracle))
    }

    pub fn ffi_converter_name(codetype: &impl CodeType) -> Result<String, askama::Error> {
        let oracle = oracle();
        Ok(codetype.ffi_converter_name(oracle))
    }

    pub fn canonical_name(codetype: &impl CodeType) -> Result<String, askama::Error> {
        let oracle = oracle();
        Ok(codetype.canonical_name(oracle))
    }

    pub fn lift_fn(codetype: &impl CodeType) -> Result<String, askama::Error> {
        Ok(format!("{}.lift", ffi_converter_name(codetype)?))
    }

    pub fn lower_fn(codetype: &impl CodeType) -> Result<String, askama::Error> {
        Ok(format!("{}.lower", ffi_converter_name(codetype)?))
    }

    pub fn read_fn(codetype: &impl CodeType) -> Result<String, askama::Error> {
        Ok(format!("{}.read", ffi_converter_name(codetype)?))
    }

    pub fn write_fn(codetype: &impl CodeType) -> Result<String, askama::Error> {
        Ok(format!("{}.write", ffi_converter_name(codetype)?))
    }

    pub fn literal_py(
        literal: &Literal,
        codetype: &impl CodeType,
    ) -> Result<String, askama::Error> {
        let oracle = oracle();
        Ok(codetype.literal(oracle, literal))
    }

    /// Get the Python syntax for representing a given low-level `FFIType`.
    pub fn ffi_type_name(type_: &FFIType) -> Result<String, askama::Error> {
        Ok(oracle().ffi_type_label(type_))
    }

    /// Get the idiomatic Python rendering of a class name (for enums, records, errors, etc).
    pub fn class_name(nm: &str) -> Result<String, askama::Error> {
        Ok(oracle().class_name(nm))
    }

    /// Get the idiomatic Python rendering of a function name.
    pub fn fn_name(nm: &str) -> Result<String, askama::Error> {
        Ok(oracle().fn_name(nm))
    }

    /// Get the idiomatic Python rendering of a variable name.
    pub fn var_name(nm: &str) -> Result<String, askama::Error> {
        Ok(oracle().var_name(nm))
    }

    /// Get the idiomatic Python rendering of an individual enum variant.
    pub fn enum_variant_py(nm: &str) -> Result<String, askama::Error> {
        Ok(oracle().enum_variant_name(nm))
    }

    /// Get the idiomatic Python rendering of an exception name
    ///
    /// This replaces "Error" at the end of the name with "Exception".  Rust code typically uses
    /// "Error" for any type of error but in the Java world, "Error" means a non-recoverable error
    /// and is distinguished from an "Exception".
    pub fn exception_name(nm: &str) -> Result<String, askama::Error> {
        Ok(oracle().error_name(nm))
    }

    pub fn coerce_py(nm: &str, type_: &Type) -> Result<String, askama::Error> {
        let oracle = oracle();
        Ok(oracle.find(type_).coerce(oracle, nm))
    }
}
