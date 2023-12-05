/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::{Context, Result};
use askama::Template;
use heck::{ToShoutySnakeCase, ToSnakeCase, ToUpperCamelCase};
use once_cell::sync::Lazy;
use serde::{Deserialize, Serialize};
use std::borrow::Borrow;
use std::cell::RefCell;
use std::collections::{BTreeSet, HashMap, HashSet};
use std::fmt::Debug;

use crate::backend::TemplateExpression;
use crate::interface::*;
use crate::BindingsConfig;

mod callback_interface;
mod compounds;
mod custom;
mod enum_;
mod executor;
mod external;
mod miscellany;
mod object;
mod primitives;
mod record;

/// A trait tor the implementation.
trait CodeType: Debug {
    /// The language specific label used to reference this type. This will be used in
    /// method signatures and property declarations.
    fn type_label(&self) -> String;

    /// A representation of this type label that can be used as part of another
    /// identifier. e.g. `read_foo()`, or `FooInternals`.
    ///
    /// This is especially useful when creating specialized objects or methods to deal
    /// with this type only.
    fn canonical_name(&self) -> String {
        self.type_label()
    }

    fn literal(&self, _literal: &Literal) -> String {
        unimplemented!("Unimplemented for {}", self.type_label())
    }

    /// Name of the FfiConverter
    ///
    /// This is the object that contains the lower, write, lift, and read methods for this type.
    fn ffi_converter_name(&self) -> String {
        format!("FfiConverter{}", self.canonical_name())
    }

    /// A list of imports that are needed if this type is in use.
    /// Classes are imported exactly once.
    fn imports(&self) -> Option<Vec<String>> {
        None
    }

    /// Function to run at startup
    fn initialization_fn(&self) -> Option<String> {
        None
    }
}

// Taken from Python's `keyword.py` module.
static KEYWORDS: Lazy<HashSet<String>> = Lazy::new(|| {
    let kwlist = vec![
        "False",
        "None",
        "True",
        "__peg_parser__",
        "and",
        "as",
        "assert",
        "async",
        "await",
        "break",
        "class",
        "continue",
        "def",
        "del",
        "elif",
        "else",
        "except",
        "finally",
        "for",
        "from",
        "global",
        "if",
        "import",
        "in",
        "is",
        "lambda",
        "nonlocal",
        "not",
        "or",
        "pass",
        "raise",
        "return",
        "try",
        "while",
        "with",
        "yield",
    ];
    HashSet::from_iter(kwlist.into_iter().map(|s| s.to_string()))
});

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

impl BindingsConfig for Config {
    fn update_from_ci(&mut self, ci: &ComponentInterface) {
        self.cdylib_name
            .get_or_insert_with(|| format!("uniffi_{}", ci.namespace()));
    }

    fn update_from_cdylib_name(&mut self, cdylib_name: &str) {
        self.cdylib_name
            .get_or_insert_with(|| cdylib_name.to_string());
    }

    fn update_from_dependency_configs(&mut self, _config_map: HashMap<&str, &Self>) {}
}

// Generate python bindings for the given ComponentInterface, as a string.
pub fn generate_python_bindings(config: &Config, ci: &ComponentInterface) -> Result<String> {
    PythonWrapper::new(config.clone(), ci)
        .render()
        .context("failed to render python bindings")
}

/// A struct to record a Python import statement.
#[derive(Clone, Debug, Eq, Ord, PartialEq, PartialOrd)]
pub enum ImportRequirement {
    /// A simple module import.
    Module { mod_name: String },
    /// A single symbol from a module.
    Symbol {
        mod_name: String,
        symbol_name: String,
    },
    /// A single symbol from a module with the specified local name.
    SymbolAs {
        mod_name: String,
        symbol_name: String,
        as_name: String,
    },
}

impl ImportRequirement {
    /// Render the Python import statement.
    fn render(&self) -> String {
        match &self {
            ImportRequirement::Module { mod_name } => format!("import {mod_name}"),
            ImportRequirement::Symbol {
                mod_name,
                symbol_name,
            } => format!("from {mod_name} import {symbol_name}"),
            ImportRequirement::SymbolAs {
                mod_name,
                symbol_name,
                as_name,
            } => format!("from {mod_name} import {symbol_name} as {as_name}"),
        }
    }
}

/// Renders Python helper code for all types
///
/// This template is a bit different than others in that it stores internal state from the render
/// process.  Make sure to only call `render()` once.
#[derive(Template)]
#[template(syntax = "py", escape = "none", path = "Types.py")]
pub struct TypeRenderer<'a> {
    python_config: &'a Config,
    ci: &'a ComponentInterface,
    // Track included modules for the `include_once()` macro
    include_once_names: RefCell<HashSet<String>>,
    // Track imports added with the `add_import()` macro
    imports: RefCell<BTreeSet<ImportRequirement>>,
}

impl<'a> TypeRenderer<'a> {
    fn new(python_config: &'a Config, ci: &'a ComponentInterface) -> Self {
        Self {
            python_config,
            ci,
            include_once_names: RefCell::new(HashSet::new()),
            imports: RefCell::new(BTreeSet::new()),
        }
    }

    // The following methods are used by the `Types.py` macros.

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
        self.imports.borrow_mut().insert(ImportRequirement::Module {
            mod_name: name.to_owned(),
        });
        ""
    }

    // Like add_import, but arranges for `from module import name`.
    fn add_import_of(&self, mod_name: &str, name: &str) -> &str {
        self.imports.borrow_mut().insert(ImportRequirement::Symbol {
            mod_name: mod_name.to_owned(),
            symbol_name: name.to_owned(),
        });
        ""
    }

    // Like add_import, but arranges for `from module import name as other`.
    fn add_import_of_as(&self, mod_name: &str, symbol_name: &str, as_name: &str) -> &str {
        self.imports
            .borrow_mut()
            .insert(ImportRequirement::SymbolAs {
                mod_name: mod_name.to_owned(),
                symbol_name: symbol_name.to_owned(),
                as_name: as_name.to_owned(),
            });
        ""
    }
}

#[derive(Template)]
#[template(syntax = "py", escape = "none", path = "wrapper.py")]
pub struct PythonWrapper<'a> {
    ci: &'a ComponentInterface,
    config: Config,
    type_helper_code: String,
    type_imports: BTreeSet<ImportRequirement>,
}
impl<'a> PythonWrapper<'a> {
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

    pub fn imports(&self) -> Vec<ImportRequirement> {
        self.type_imports.iter().cloned().collect()
    }
}

fn fixup_keyword(name: String) -> String {
    if KEYWORDS.contains(&name) {
        format!("_{name}")
    } else {
        name
    }
}

#[derive(Clone, Default)]
pub struct PythonCodeOracle;

impl PythonCodeOracle {
    fn find(&self, type_: &Type) -> Box<dyn CodeType> {
        type_.clone().as_type().as_codetype()
    }

    /// Get the idiomatic Python rendering of a class name (for enums, records, errors, etc).
    fn class_name(&self, nm: &str) -> String {
        fixup_keyword(nm.to_string().to_upper_camel_case())
    }

    /// Get the idiomatic Python rendering of a function name.
    fn fn_name(&self, nm: &str) -> String {
        fixup_keyword(nm.to_string().to_snake_case())
    }

    /// Get the idiomatic Python rendering of a variable name.
    fn var_name(&self, nm: &str) -> String {
        fixup_keyword(nm.to_string().to_snake_case())
    }

    /// Get the idiomatic Python rendering of an individual enum variant.
    fn enum_variant_name(&self, nm: &str) -> String {
        fixup_keyword(nm.to_string().to_shouty_snake_case())
    }

    fn ffi_type_label(ffi_type: &FfiType) -> String {
        match ffi_type {
            FfiType::Int8 => "ctypes.c_int8".to_string(),
            FfiType::UInt8 => "ctypes.c_uint8".to_string(),
            FfiType::Int16 => "ctypes.c_int16".to_string(),
            FfiType::UInt16 => "ctypes.c_uint16".to_string(),
            FfiType::Int32 => "ctypes.c_int32".to_string(),
            FfiType::UInt32 => "ctypes.c_uint32".to_string(),
            FfiType::Int64 => "ctypes.c_int64".to_string(),
            FfiType::UInt64 => "ctypes.c_uint64".to_string(),
            FfiType::Float32 => "ctypes.c_float".to_string(),
            FfiType::Float64 => "ctypes.c_double".to_string(),
            FfiType::RustArcPtr(_) => "ctypes.c_void_p".to_string(),
            FfiType::RustBuffer(maybe_suffix) => match maybe_suffix {
                Some(suffix) => format!("_UniffiRustBuffer{suffix}"),
                None => "_UniffiRustBuffer".to_string(),
            },
            FfiType::ForeignBytes => "_UniffiForeignBytes".to_string(),
            FfiType::ForeignCallback => "_UNIFFI_FOREIGN_CALLBACK_T".to_string(),
            // Pointer to an `asyncio.EventLoop` instance
            FfiType::ForeignExecutorHandle => "ctypes.c_size_t".to_string(),
            FfiType::ForeignExecutorCallback => "_UNIFFI_FOREIGN_EXECUTOR_CALLBACK_T".to_string(),
            FfiType::RustFutureHandle => "ctypes.c_void_p".to_string(),
            FfiType::RustFutureContinuationCallback => "_UNIFFI_FUTURE_CONTINUATION_T".to_string(),
            FfiType::RustFutureContinuationData => "ctypes.c_size_t".to_string(),
        }
    }
}

trait AsCodeType {
    fn as_codetype(&self) -> Box<dyn CodeType>;
}

impl<T: AsType> AsCodeType for T {
    fn as_codetype(&self) -> Box<dyn CodeType> {
        // Map `Type` instances to a `Box<dyn CodeType>` for that type.
        //
        // There is a companion match in `templates/Types.py` which performs a similar function for the
        // template code.
        //
        //   - When adding additional types here, make sure to also add a match arm to the `Types.py` template.
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

            Type::Enum { name, .. } => Box::new(enum_::EnumCodeType::new(name)),
            Type::Object { name, .. } => Box::new(object::ObjectCodeType::new(name)),
            Type::Record { name, .. } => Box::new(record::RecordCodeType::new(name)),
            Type::CallbackInterface { name, .. } => {
                Box::new(callback_interface::CallbackInterfaceCodeType::new(name))
            }
            Type::ForeignExecutor => Box::new(executor::ForeignExecutorCodeType),
            Type::Optional { inner_type } => {
                Box::new(compounds::OptionalCodeType::new(*inner_type))
            }
            Type::Sequence { inner_type } => {
                Box::new(compounds::SequenceCodeType::new(*inner_type))
            }
            Type::Map {
                key_type,
                value_type,
            } => Box::new(compounds::MapCodeType::new(*key_type, *value_type)),
            Type::External { name, .. } => Box::new(external::ExternalCodeType::new(name)),
            Type::Custom { name, .. } => Box::new(custom::CustomCodeType::new(name)),
        }
    }
}

pub mod filters {
    use super::*;
    pub use crate::backend::filters::*;

    pub(super) fn type_name(as_ct: &impl AsCodeType) -> Result<String, askama::Error> {
        Ok(as_ct.as_codetype().type_label())
    }

    pub(super) fn ffi_converter_name(as_ct: &impl AsCodeType) -> Result<String, askama::Error> {
        Ok(String::from("_Uniffi") + &as_ct.as_codetype().ffi_converter_name()[3..])
    }

    pub(super) fn canonical_name(as_ct: &impl AsCodeType) -> Result<String, askama::Error> {
        Ok(as_ct.as_codetype().canonical_name())
    }

    pub(super) fn lift_fn(as_ct: &impl AsCodeType) -> Result<String, askama::Error> {
        Ok(format!("{}.lift", ffi_converter_name(as_ct)?))
    }

    pub(super) fn lower_fn(as_ct: &impl AsCodeType) -> Result<String, askama::Error> {
        Ok(format!("{}.lower", ffi_converter_name(as_ct)?))
    }

    pub(super) fn read_fn(as_ct: &impl AsCodeType) -> Result<String, askama::Error> {
        Ok(format!("{}.read", ffi_converter_name(as_ct)?))
    }

    pub(super) fn write_fn(as_ct: &impl AsCodeType) -> Result<String, askama::Error> {
        Ok(format!("{}.write", ffi_converter_name(as_ct)?))
    }

    pub(super) fn literal_py(
        literal: &Literal,
        as_ct: &impl AsCodeType,
    ) -> Result<String, askama::Error> {
        Ok(as_ct.as_codetype().literal(literal))
    }

    pub fn ffi_type_name(type_: &FfiType) -> Result<String, askama::Error> {
        Ok(PythonCodeOracle::ffi_type_label(type_))
    }

    /// Get the idiomatic Python rendering of a class name (for enums, records, errors, etc).
    pub fn class_name(nm: &str) -> Result<String, askama::Error> {
        Ok(PythonCodeOracle.class_name(nm))
    }

    /// Get the idiomatic Python rendering of a function name.
    pub fn fn_name(nm: &str) -> Result<String, askama::Error> {
        Ok(PythonCodeOracle.fn_name(nm))
    }

    /// Get the idiomatic Python rendering of a variable name.
    pub fn var_name(nm: &str) -> Result<String, askama::Error> {
        Ok(PythonCodeOracle.var_name(nm))
    }

    /// Get the idiomatic Python rendering of an individual enum variant.
    pub fn enum_variant_py(nm: &str) -> Result<String, askama::Error> {
        Ok(PythonCodeOracle.enum_variant_name(nm))
    }
}
