/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::borrow::Borrow;
use std::cell::RefCell;
use std::collections::{BTreeSet, HashMap, HashSet};

use anyhow::{Context, Result};
use askama::Template;
use heck::{ToLowerCamelCase, ToShoutySnakeCase, ToUpperCamelCase};
use serde::{Deserialize, Serialize};

use crate::backend::{CodeType, TemplateExpression};
use crate::interface::*;
use crate::BindingsConfig;

mod callback_interface;
mod compounds;
mod custom;
mod enum_;
mod error;
mod executor;
mod external;
mod miscellany;
mod object;
mod primitives;
mod record;
mod variant;

// config options to customize the generated Kotlin.
#[derive(Debug, Default, Clone, Serialize, Deserialize)]
pub struct Config {
    package_name: Option<String>,
    cdylib_name: Option<String>,
    #[serde(default)]
    custom_types: HashMap<String, CustomTypeConfig>,
    #[serde(default)]
    external_packages: HashMap<String, String>,
}

#[derive(Debug, Default, Clone, Serialize, Deserialize)]
pub struct CustomTypeConfig {
    imports: Option<Vec<String>>,
    type_name: Option<String>,
    into_custom: TemplateExpression,
    from_custom: TemplateExpression,
}

impl Config {
    pub fn package_name(&self) -> String {
        if let Some(package_name) = &self.package_name {
            package_name.clone()
        } else {
            "uniffi".into()
        }
    }

    pub fn cdylib_name(&self) -> String {
        if let Some(cdylib_name) = &self.cdylib_name {
            cdylib_name.clone()
        } else {
            "uniffi".into()
        }
    }
}

impl BindingsConfig for Config {
    const TOML_KEY: &'static str = "kotlin";

    fn update_from_ci(&mut self, ci: &ComponentInterface) {
        self.package_name
            .get_or_insert_with(|| format!("uniffi.{}", ci.namespace()));
        self.cdylib_name
            .get_or_insert_with(|| format!("uniffi_{}", ci.namespace()));
    }

    fn update_from_cdylib_name(&mut self, cdylib_name: &str) {
        self.cdylib_name
            .get_or_insert_with(|| cdylib_name.to_string());
    }

    fn update_from_dependency_configs(&mut self, config_map: HashMap<&str, &Self>) {
        for (crate_name, config) in config_map {
            if !self.external_packages.contains_key(crate_name) {
                self.external_packages
                    .insert(crate_name.to_string(), config.package_name());
            }
        }
    }
}

// Generate kotlin bindings for the given ComponentInterface, as a string.
pub fn generate_bindings(config: &Config, ci: &ComponentInterface) -> Result<String> {
    KotlinWrapper::new(config.clone(), ci)
        .render()
        .context("failed to render kotlin bindings")
}

/// A struct to record a Kotlin import statement.
#[derive(Clone, Debug, Eq, Ord, PartialEq, PartialOrd)]
pub enum ImportRequirement {
    /// The name we are importing.
    Import { name: String },
    /// Import the name with the specified local name.
    ImportAs { name: String, as_name: String },
}

impl ImportRequirement {
    /// Render the Kotlin import statement.
    fn render(&self) -> String {
        match &self {
            ImportRequirement::Import { name } => format!("import {name}"),
            ImportRequirement::ImportAs { name, as_name } => {
                format!("import {name} as {as_name}")
            }
        }
    }
}

/// Renders Kotlin helper code for all types
///
/// This template is a bit different than others in that it stores internal state from the render
/// process.  Make sure to only call `render()` once.
#[derive(Template)]
#[template(syntax = "kt", escape = "none", path = "Types.kt")]
pub struct TypeRenderer<'a> {
    kotlin_config: &'a Config,
    ci: &'a ComponentInterface,
    // Track included modules for the `include_once()` macro
    include_once_names: RefCell<HashSet<String>>,
    // Track imports added with the `add_import()` macro
    imports: RefCell<BTreeSet<ImportRequirement>>,
}

impl<'a> TypeRenderer<'a> {
    fn new(kotlin_config: &'a Config, ci: &'a ComponentInterface) -> Self {
        Self {
            kotlin_config,
            ci,
            include_once_names: RefCell::new(HashSet::new()),
            imports: RefCell::new(BTreeSet::new()),
        }
    }

    // Get the package name for an external type
    fn external_type_package_name(&self, crate_name: &str) -> String {
        match self.kotlin_config.external_packages.get(crate_name) {
            Some(name) => name.clone(),
            None => crate_name.to_string(),
        }
    }

    // The following methods are used by the `Types.kt` macros.

    // Helper for the including a template, but only once.
    //
    // The first time this is called with a name it will return true, indicating that we should
    // include the template.  Subsequent calls will return false.
    fn include_once_check(&self, name: &str) -> bool {
        self.include_once_names
            .borrow_mut()
            .insert(name.to_string())
    }

    // Helper to add an import statement
    //
    // Call this inside your template to cause an import statement to be added at the top of the
    // file.  Imports will be sorted and de-deuped.
    //
    // Returns an empty string so that it can be used inside an askama `{{ }}` block.
    fn add_import(&self, name: &str) -> &str {
        self.imports.borrow_mut().insert(ImportRequirement::Import {
            name: name.to_owned(),
        });
        ""
    }

    // Like add_import, but arranges for `import name as as_name`
    fn add_import_as(&self, name: &str, as_name: &str) -> &str {
        self.imports
            .borrow_mut()
            .insert(ImportRequirement::ImportAs {
                name: name.to_owned(),
                as_name: as_name.to_owned(),
            });
        ""
    }
}

#[derive(Template)]
#[template(syntax = "kt", escape = "none", path = "wrapper.kt")]
pub struct KotlinWrapper<'a> {
    config: Config,
    ci: &'a ComponentInterface,
    type_helper_code: String,
    type_imports: BTreeSet<ImportRequirement>,
}

impl<'a> KotlinWrapper<'a> {
    pub fn new(config: Config, ci: &'a ComponentInterface) -> Self {
        let type_renderer = TypeRenderer::new(&config, ci);
        let type_helper_code = type_renderer.render().unwrap();
        let type_imports = type_renderer.imports.into_inner();
        Self {
            config,
            ci,
            type_helper_code,
            type_imports,
        }
    }

    pub fn initialization_fns(&self) -> Vec<String> {
        self.ci
            .iter_types()
            .map(|t| KotlinCodeOracle.find(t))
            .filter_map(|ct| ct.initialization_fn())
            .collect()
    }

    pub fn imports(&self) -> Vec<ImportRequirement> {
        self.type_imports.iter().cloned().collect()
    }
}

#[derive(Clone)]
pub struct KotlinCodeOracle;

impl KotlinCodeOracle {
    fn find(&self, type_: &Type) -> Box<dyn CodeType> {
        type_.clone().as_type().as_codetype()
    }

    fn find_as_error(&self, type_: &Type) -> Box<dyn CodeType> {
        match type_ {
            Type::Enum(id) => Box::new(error::ErrorCodeType::new(id.clone())),
            // XXX - not sure how we are supposed to return askama::Error?
            _ => panic!("unsupported type for error: {type_:?}"),
        }
    }

    /// Get the idiomatic Kotlin rendering of a class name (for enums, records, errors, etc).
    fn class_name(&self, nm: &str) -> String {
        nm.to_string().to_upper_camel_case()
    }

    /// Get the idiomatic Kotlin rendering of a function name.
    fn fn_name(&self, nm: &str) -> String {
        format!("`{}`", nm.to_string().to_lower_camel_case())
    }

    /// Get the idiomatic Kotlin rendering of a variable name.
    fn var_name(&self, nm: &str) -> String {
        format!("`{}`", nm.to_string().to_lower_camel_case())
    }

    /// Get the idiomatic Kotlin rendering of an individual enum variant.
    fn enum_variant_name(&self, nm: &str) -> String {
        nm.to_string().to_shouty_snake_case()
    }

    /// Get the idiomatic Kotlin rendering of an exception name
    ///
    /// This replaces "Error" at the end of the name with "Exception".  Rust code typically uses
    /// "Error" for any type of error but in the Java world, "Error" means a non-recoverable error
    /// and is distinguished from an "Exception".
    fn error_name(&self, nm: &str) -> String {
        // errors are a class in kotlin.
        let name = self.class_name(nm);
        match name.strip_suffix("Error") {
            None => name,
            Some(stripped) => format!("{stripped}Exception"),
        }
    }

    fn ffi_type_label_by_value(ffi_type: &FfiType) -> String {
        match ffi_type {
            FfiType::RustBuffer(_) => format!("{}.ByValue", Self::ffi_type_label(ffi_type)),
            _ => Self::ffi_type_label(ffi_type),
        }
    }

    fn ffi_type_label(ffi_type: &FfiType) -> String {
        match ffi_type {
            // Note that unsigned integers in Kotlin are currently experimental, but java.nio.ByteBuffer does not
            // support them yet. Thus, we use the signed variants to represent both signed and unsigned
            // types from the component API.
            FfiType::Int8 | FfiType::UInt8 => "Byte".to_string(),
            FfiType::Int16 | FfiType::UInt16 => "Short".to_string(),
            FfiType::Int32 | FfiType::UInt32 => "Int".to_string(),
            FfiType::Int64 | FfiType::UInt64 => "Long".to_string(),
            FfiType::Float32 => "Float".to_string(),
            FfiType::Float64 => "Double".to_string(),
            FfiType::RustArcPtr(_) => "Pointer".to_string(),
            FfiType::RustBuffer(maybe_suffix) => {
                format!("RustBuffer{}", maybe_suffix.as_deref().unwrap_or_default())
            }
            FfiType::ForeignBytes => "ForeignBytes.ByValue".to_string(),
            FfiType::ForeignCallback => "ForeignCallback".to_string(),
            FfiType::ForeignExecutorHandle => "USize".to_string(),
            FfiType::ForeignExecutorCallback => "UniFfiForeignExecutorCallback".to_string(),
            FfiType::FutureCallback { return_type } => {
                format!("UniFfiFutureCallback{}", Self::ffi_type_label(return_type))
            }
            FfiType::FutureCallbackData => "USize".to_string(),
        }
    }
}

pub trait AsCodeType {
    fn as_codetype(&self) -> Box<dyn CodeType>;
}

impl<T: AsType> AsCodeType for T {
    fn as_codetype(&self) -> Box<dyn CodeType> {
        // Map `Type` instances to a `Box<dyn CodeType>` for that type.
        //
        // There is a companion match in `templates/Types.kt` which performs a similar function for the
        // template code.
        //
        //   - When adding additional types here, make sure to also add a match arm to the `Types.kt` template.
        //   - To keep things manageable, let's try to limit ourselves to these 2 mega-matches
        match self.as_type() {
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
            Type::Bytes => Box::new(primitives::BytesCodeType),

            Type::Timestamp => Box::new(miscellany::TimestampCodeType),
            Type::Duration => Box::new(miscellany::DurationCodeType),

            Type::Enum(id) => Box::new(enum_::EnumCodeType::new(id)),
            Type::Object { name, .. } => Box::new(object::ObjectCodeType::new(name)),
            Type::Record(id) => Box::new(record::RecordCodeType::new(id)),
            Type::CallbackInterface(id) => {
                Box::new(callback_interface::CallbackInterfaceCodeType::new(id))
            }
            Type::ForeignExecutor => Box::new(executor::ForeignExecutorCodeType),
            Type::Optional(inner) => Box::new(compounds::OptionalCodeType::new(*inner)),
            Type::Sequence(inner) => Box::new(compounds::SequenceCodeType::new(*inner)),
            Type::Map(key, value) => Box::new(compounds::MapCodeType::new(*key, *value)),
            Type::External { name, .. } => Box::new(external::ExternalCodeType::new(name)),
            Type::Custom { name, .. } => Box::new(custom::CustomCodeType::new(name)),
        }
    }
}

pub mod filters {
    use super::*;
    pub use crate::backend::filters::*;

    pub fn type_name(as_ct: &impl AsCodeType) -> Result<String, askama::Error> {
        Ok(as_ct.as_codetype().type_label())
    }

    pub fn canonical_name(as_ct: &impl AsCodeType) -> Result<String, askama::Error> {
        Ok(as_ct.as_codetype().canonical_name())
    }

    pub fn ffi_converter_name(as_ct: &impl AsCodeType) -> Result<String, askama::Error> {
        Ok(as_ct.as_codetype().ffi_converter_name())
    }

    pub fn lower_fn(as_ct: &impl AsCodeType) -> Result<String, askama::Error> {
        Ok(format!(
            "{}.lower",
            as_ct.as_codetype().ffi_converter_name()
        ))
    }

    pub fn allocation_size_fn(as_ct: &impl AsCodeType) -> Result<String, askama::Error> {
        Ok(format!(
            "{}.allocationSize",
            as_ct.as_codetype().ffi_converter_name()
        ))
    }

    pub fn write_fn(as_ct: &impl AsCodeType) -> Result<String, askama::Error> {
        Ok(format!(
            "{}.write",
            as_ct.as_codetype().ffi_converter_name()
        ))
    }

    pub fn lift_fn(as_ct: &impl AsCodeType) -> Result<String, askama::Error> {
        Ok(format!("{}.lift", as_ct.as_codetype().ffi_converter_name()))
    }

    pub fn read_fn(as_ct: &impl AsCodeType) -> Result<String, askama::Error> {
        Ok(format!("{}.read", as_ct.as_codetype().ffi_converter_name()))
    }

    pub fn error_handler(result_type: &ResultType) -> Result<String, askama::Error> {
        match &result_type.throws_type {
            Some(error_type) => Ok(KotlinCodeOracle.error_name(&type_name(error_type)?)),
            None => Ok("NullCallStatusErrorHandler".into()),
        }
    }

    pub fn future_callback_handler(result_type: &ResultType) -> Result<String, askama::Error> {
        let return_component = match &result_type.return_type {
            Some(return_type) => KotlinCodeOracle.find(return_type).canonical_name(),
            None => "Void".into(),
        };
        let throws_component = match &result_type.throws_type {
            Some(throws_type) => {
                format!("_{}", KotlinCodeOracle.find(throws_type).canonical_name())
            }
            None => "".into(),
        };
        Ok(format!(
            "UniFfiFutureCallbackHandler{return_component}{throws_component}"
        ))
    }

    pub fn future_continuation_type(result_type: &ResultType) -> Result<String, askama::Error> {
        let return_type_name = match &result_type.return_type {
            Some(t) => type_name(t)?,
            None => "Unit".into(),
        };
        Ok(format!("Continuation<{return_type_name}>"))
    }

    pub fn render_literal(
        literal: &Literal,
        as_ct: &impl AsCodeType,
    ) -> Result<String, askama::Error> {
        Ok(as_ct.as_codetype().literal(literal))
    }

    /// Get the Kotlin syntax for representing a given low-level `FfiType`.
    pub fn ffi_type_name(type_: &FfiType) -> Result<String, askama::Error> {
        Ok(KotlinCodeOracle::ffi_type_label(type_))
    }

    pub fn ffi_type_name_by_value(type_: &FfiType) -> Result<String, askama::Error> {
        Ok(KotlinCodeOracle::ffi_type_label_by_value(type_))
    }

    // Some FfiTypes have the same ffi_type_label - this makes a vec of them unique.
    pub fn unique_ffi_types(
        types: impl Iterator<Item = FfiType>,
    ) -> Result<impl Iterator<Item = FfiType>, askama::Error> {
        let mut seen = HashSet::new();
        let mut result = Vec::new();
        for t in types {
            if seen.insert(KotlinCodeOracle::ffi_type_label(&t)) {
                result.push(t)
            }
        }
        Ok(result.into_iter())
    }

    /// Get the idiomatic Kotlin rendering of a function name.
    pub fn fn_name(nm: &str) -> Result<String, askama::Error> {
        Ok(KotlinCodeOracle.fn_name(nm))
    }

    /// Get the idiomatic Kotlin rendering of a variable name.
    pub fn var_name(nm: &str) -> Result<String, askama::Error> {
        Ok(KotlinCodeOracle.var_name(nm))
    }

    pub fn variant_name(v: &Variant) -> Result<String, askama::Error> {
        Ok(KotlinCodeOracle.enum_variant_name(v.name()))
    }

    /// Get a codetype for idiomatic Kotlin rendering of an individual enum variant.
    pub fn enum_variant(v: &Variant) -> Result<impl AsCodeType, askama::Error> {
        Ok(v.clone())
    }

    /// Get a codetype for idiomatic Kotlin rendering of an individual enum variant
    /// when used in an error.
    pub fn error_variant(v: &Variant) -> Result<impl AsCodeType, askama::Error> {
        Ok(variant::ErrorVariantCodeTypeProvider { v: v.clone() })
    }

    /// Some of the above filters have different versions to help when the type
    /// is used as an error.
    pub fn error_type_name(as_type: &impl AsType) -> Result<String, askama::Error> {
        Ok(KotlinCodeOracle
            .find_as_error(&as_type.as_type())
            .type_label())
    }

    pub fn error_canonical_name(as_type: &impl AsType) -> Result<String, askama::Error> {
        Ok(KotlinCodeOracle
            .find_as_error(&as_type.as_type())
            .canonical_name())
    }

    pub fn error_ffi_converter_name(as_type: &impl AsType) -> Result<String, askama::Error> {
        Ok(KotlinCodeOracle
            .find_as_error(&as_type.as_type())
            .ffi_converter_name())
    }

    /// Remove the "`" chars we put around function/variable names
    ///
    /// These are used to avoid name clashes with kotlin identifiers, but sometimes you want to
    /// render the name unquoted.  One example is the message property for errors where we want to
    /// display the name for the user.
    pub fn unquote(nm: &str) -> Result<String, askama::Error> {
        Ok(nm.trim_matches('`').to_string())
    }
}
