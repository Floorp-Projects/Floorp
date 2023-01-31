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

use crate::backend::{CodeOracle, CodeType, TemplateExpression, TypeIdentifier};
use crate::interface::*;
use crate::MergeWith;

mod callback_interface;
mod compounds;
mod custom;
mod enum_;
mod error;
mod external;
mod miscellany;
mod object;
mod primitives;
mod record;

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
    // Map `Type` instances to a `Box<dyn CodeType>` for that type.
    //
    // There is a companion match in `templates/Types.py` which performs a similar function for the
    // template code.
    //
    //   - When adding additional types here, make sure to also add a match arm to the `Types.py` template.
    //   - To keep things manageable, let's try to limit ourselves to these 2 mega-matches
    fn create_code_type(&self, type_: TypeIdentifier) -> Box<dyn CodeType> {
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

            Type::Optional(inner) => Box::new(compounds::OptionalCodeType::new(*inner)),
            Type::Sequence(inner) => Box::new(compounds::SequenceCodeType::new(*inner)),
            Type::Map(key, value) => Box::new(compounds::MapCodeType::new(*key, *value)),
            Type::External { name, .. } => Box::new(external::ExternalCodeType::new(name)),
            Type::Custom { name, .. } => Box::new(custom::CustomCodeType::new(name)),
            Type::Unresolved { name } => {
                unreachable!("Type `{name}` must be resolved before calling create_code_type")
            }
        }
    }
}

impl CodeOracle for PythonCodeOracle {
    fn find(&self, type_: &TypeIdentifier) -> Box<dyn CodeType> {
        self.create_code_type(type_.clone())
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

    /// Get the idiomatic Python rendering of an exception name
    /// This replaces "Error" at the end of the name with "Exception".
    fn error_name(&self, nm: &str) -> String {
        let name = fixup_keyword(self.class_name(nm));
        match name.strip_suffix("Error") {
            None => name,
            Some(stripped) => format!("{stripped}Exception"),
        }
    }

    fn ffi_type_label(&self, ffi_type: &FfiType) -> String {
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
                Some(suffix) => format!("RustBuffer{}", suffix),
                None => "RustBuffer".to_string(),
            },
            FfiType::ForeignBytes => "ForeignBytes".to_string(),
            FfiType::ForeignCallback => "FOREIGN_CALLBACK_T".to_string(),
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

    /// Get the Python syntax for representing a given low-level `FfiType`.
    pub fn ffi_type_name(type_: &FfiType) -> Result<String, askama::Error> {
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

    pub fn coerce_py(nm: &str, type_: &Type) -> Result<String, askama::Error> {
        let oracle = oracle();
        Ok(oracle.find(type_).coerce(oracle, nm))
    }
}
