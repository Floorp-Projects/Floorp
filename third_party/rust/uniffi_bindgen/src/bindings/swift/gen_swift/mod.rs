/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use once_cell::sync::Lazy;
use std::borrow::Borrow;
use std::cell::RefCell;
use std::collections::{BTreeSet, HashMap, HashSet};
use std::fmt::Debug;

use anyhow::{Context, Result};
use askama::Template;
use heck::{ToLowerCamelCase, ToUpperCamelCase};
use serde::{Deserialize, Serialize};

use super::Bindings;
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

    // XXX - the below should be removed and replace with the ffi_converter_name reference in the template.
    /// An expression for lowering a value into something we can pass over the FFI.
    fn lower(&self) -> String {
        format!("{}.lower", self.ffi_converter_name())
    }

    /// An expression for writing a value into a byte buffer.
    fn write(&self) -> String {
        format!("{}.write", self.ffi_converter_name())
    }

    /// An expression for lifting a value from something we received over the FFI.
    fn lift(&self) -> String {
        format!("{}.lift", self.ffi_converter_name())
    }

    /// An expression for reading a value from a byte buffer.
    fn read(&self) -> String {
        format!("{}.read", self.ffi_converter_name())
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

/// From <https://docs.swift.org/swift-book/documentation/the-swift-programming-language/lexicalstructure/#Keywords-and-Punctuation>
static KEYWORDS: Lazy<HashSet<String>> = Lazy::new(|| {
    [
        // Keywords used in declarations:
        "associatedtype",
        "class",
        "deinit",
        "enum",
        "extension",
        "fileprivate",
        "func",
        "import",
        "init",
        "inout",
        "internal",
        "let",
        "open",
        "operator",
        "private",
        "precedencegroup",
        "protocol",
        "public",
        "rethrows",
        "static",
        "struct",
        "subscript",
        "typealias",
        "var",
        // Keywords used in statements:
        "break",
        "case",
        "catch",
        "continue",
        "default",
        "defer",
        "do",
        "else",
        "fallthrough",
        "for",
        "guard",
        "if",
        "in",
        "repeat",
        "return",
        "throw",
        "switch",
        "where",
        "while",
        // Keywords used in expressions and types:
        "Any",
        "as",
        "await",
        "catch",
        "false",
        "is",
        "nil",
        "rethrows",
        "self",
        "Self",
        "super",
        "throw",
        "throws",
        "true",
        "try",
    ]
    .iter()
    .map(ToString::to_string)
    .collect::<HashSet<_>>()
});

/// Quote a name for use in a context where keywords must be quoted
pub fn quote_general_keyword(nm: String) -> String {
    if KEYWORDS.contains(&nm) {
        format!("`{nm}`")
    } else {
        nm
    }
}

/// Per <https://docs.swift.org/swift-book/documentation/the-swift-programming-language/lexicalstructure/#Keywords-and-Punctuation> subset of keywords which need quoting in arg context.
static ARG_KEYWORDS: Lazy<HashSet<String>> = Lazy::new(|| {
    ["inout", "var", "let"]
        .iter()
        .map(ToString::to_string)
        .collect::<HashSet<_>>()
});

/// Quote a name for use in arg context where fewer keywords must be quoted
pub fn quote_arg_keyword(nm: String) -> String {
    if ARG_KEYWORDS.contains(&nm) {
        format!("`{nm}`")
    } else {
        nm
    }
}

/// Config options for the caller to customize the generated Swift.
///
/// Note that this can only be used to control details of the Swift *that do not affect the underlying component*,
/// since the details of the underlying component are entirely determined by the `ComponentInterface`.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct Config {
    cdylib_name: Option<String>,
    module_name: Option<String>,
    ffi_module_name: Option<String>,
    ffi_module_filename: Option<String>,
    generate_module_map: Option<bool>,
    omit_argument_labels: Option<bool>,
    #[serde(default)]
    custom_types: HashMap<String, CustomTypeConfig>,
}

#[derive(Debug, Default, Clone, Serialize, Deserialize)]
pub struct CustomTypeConfig {
    imports: Option<Vec<String>>,
    type_name: Option<String>,
    into_custom: TemplateExpression,
    from_custom: TemplateExpression,
}

impl Config {
    /// The name of the Swift module containing the high-level foreign-language bindings.
    pub fn module_name(&self) -> String {
        match self.module_name.as_ref() {
            Some(name) => name.clone(),
            None => "uniffi".into(),
        }
    }

    /// The name of the lower-level C module containing the FFI declarations.
    pub fn ffi_module_name(&self) -> String {
        match self.ffi_module_name.as_ref() {
            Some(name) => name.clone(),
            None => format!("{}FFI", self.module_name()),
        }
    }

    /// The filename stem for the lower-level C module containing the FFI declarations.
    pub fn ffi_module_filename(&self) -> String {
        match self.ffi_module_filename.as_ref() {
            Some(name) => name.clone(),
            None => self.ffi_module_name(),
        }
    }

    /// The name of the `.modulemap` file for the lower-level C module with FFI declarations.
    pub fn modulemap_filename(&self) -> String {
        format!("{}.modulemap", self.ffi_module_filename())
    }

    /// The name of the `.h` file for the lower-level C module with FFI declarations.
    pub fn header_filename(&self) -> String {
        format!("{}.h", self.ffi_module_filename())
    }

    /// The name of the compiled Rust library containing the FFI implementation.
    pub fn cdylib_name(&self) -> String {
        if let Some(cdylib_name) = &self.cdylib_name {
            cdylib_name.clone()
        } else {
            "uniffi".into()
        }
    }

    /// Whether to generate a `.modulemap` file for the lower-level C module with FFI declarations.
    pub fn generate_module_map(&self) -> bool {
        self.generate_module_map.unwrap_or(true)
    }

    /// Whether to omit argument labels in Swift function definitions.
    pub fn omit_argument_labels(&self) -> bool {
        self.omit_argument_labels.unwrap_or(false)
    }
}

impl BindingsConfig for Config {
    fn update_from_ci(&mut self, ci: &ComponentInterface) {
        self.module_name
            .get_or_insert_with(|| ci.namespace().into());
        self.cdylib_name
            .get_or_insert_with(|| format!("uniffi_{}", ci.namespace()));
    }

    fn update_from_cdylib_name(&mut self, cdylib_name: &str) {
        self.cdylib_name
            .get_or_insert_with(|| cdylib_name.to_string());
    }

    fn update_from_dependency_configs(&mut self, _config_map: HashMap<&str, &Self>) {}
}

/// Generate UniFFI component bindings for Swift, as strings in memory.
///
pub fn generate_bindings(config: &Config, ci: &ComponentInterface) -> Result<Bindings> {
    let header = BridgingHeader::new(config, ci)
        .render()
        .context("failed to render Swift bridging header")?;
    let library = SwiftWrapper::new(config.clone(), ci)
        .render()
        .context("failed to render Swift library")?;
    let modulemap = if config.generate_module_map() {
        Some(
            ModuleMap::new(config, ci)
                .render()
                .context("failed to render Swift modulemap")?,
        )
    } else {
        None
    };
    Ok(Bindings {
        library,
        header,
        modulemap,
    })
}

/// Renders Swift helper code for all types
///
/// This template is a bit different than others in that it stores internal state from the render
/// process.  Make sure to only call `render()` once.
#[derive(Template)]
#[template(syntax = "swift", escape = "none", path = "Types.swift")]
pub struct TypeRenderer<'a> {
    config: &'a Config,
    ci: &'a ComponentInterface,
    // Track included modules for the `include_once()` macro
    include_once_names: RefCell<HashSet<String>>,
    // Track imports added with the `add_import()` macro
    imports: RefCell<BTreeSet<String>>,
}

impl<'a> TypeRenderer<'a> {
    fn new(config: &'a Config, ci: &'a ComponentInterface) -> Self {
        Self {
            config,
            ci,
            include_once_names: RefCell::new(HashSet::new()),
            imports: RefCell::new(BTreeSet::new()),
        }
    }

    // The following methods are used by the `Types.swift` macros.

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
        self.imports.borrow_mut().insert(name.to_owned());
        ""
    }
}

/// Template for generating the `.h` file that defines the low-level C FFI.
///
/// This file defines only the low-level structs and functions that are exposed
/// by the compiled Rust code. It gets wrapped into a higher-level API by the
/// code from [`SwiftWrapper`].
#[derive(Template)]
#[template(syntax = "c", escape = "none", path = "BridgingHeaderTemplate.h")]
pub struct BridgingHeader<'config, 'ci> {
    _config: &'config Config,
    ci: &'ci ComponentInterface,
}

impl<'config, 'ci> BridgingHeader<'config, 'ci> {
    pub fn new(config: &'config Config, ci: &'ci ComponentInterface) -> Self {
        Self {
            _config: config,
            ci,
        }
    }
}

/// Template for generating the `.modulemap` file that exposes the low-level C FFI.
///
/// This file defines how the low-level C FFI from [`BridgingHeader`] gets exposed
/// as a Swift module that can be called by other Swift code. In our case, its only
/// job is to define the *name* of the Swift module that will contain the FFI functions
/// so that it can be imported by the higher-level code in from [`SwiftWrapper`].
#[derive(Template)]
#[template(syntax = "c", escape = "none", path = "ModuleMapTemplate.modulemap")]
pub struct ModuleMap<'config, 'ci> {
    config: &'config Config,
    _ci: &'ci ComponentInterface,
}

impl<'config, 'ci> ModuleMap<'config, 'ci> {
    pub fn new(config: &'config Config, _ci: &'ci ComponentInterface) -> Self {
        Self { config, _ci }
    }
}

#[derive(Template)]
#[template(syntax = "swift", escape = "none", path = "wrapper.swift")]
pub struct SwiftWrapper<'a> {
    config: Config,
    ci: &'a ComponentInterface,
    type_helper_code: String,
    type_imports: BTreeSet<String>,
    has_async_fns: bool,
}
impl<'a> SwiftWrapper<'a> {
    pub fn new(config: Config, ci: &'a ComponentInterface) -> Self {
        let type_renderer = TypeRenderer::new(&config, ci);
        let type_helper_code = type_renderer.render().unwrap();
        let type_imports = type_renderer.imports.into_inner();
        Self {
            config,
            ci,
            type_helper_code,
            type_imports,
            has_async_fns: ci.has_async_fns(),
        }
    }

    pub fn imports(&self) -> Vec<String> {
        self.type_imports.iter().cloned().collect()
    }

    pub fn initialization_fns(&self) -> Vec<String> {
        self.ci
            .iter_types()
            .map(|t| SwiftCodeOracle.find(t))
            .filter_map(|ct| ct.initialization_fn())
            .chain(
                self.has_async_fns
                    .then(|| "uniffiInitContinuationCallback".into()),
            )
            .collect()
    }
}

#[derive(Clone)]
pub struct SwiftCodeOracle;

impl SwiftCodeOracle {
    // Map `Type` instances to a `Box<dyn CodeType>` for that type.
    //
    // There is a companion match in `templates/Types.swift` which performs a similar function for the
    // template code.
    //
    //   - When adding additional types here, make sure to also add a match arm to the `Types.swift` template.
    //   - To keep things manageable, let's try to limit ourselves to these 2 mega-matches
    fn create_code_type(&self, type_: Type) -> Box<dyn CodeType> {
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

    fn find(&self, type_: &Type) -> Box<dyn CodeType> {
        self.create_code_type(type_.clone())
    }

    /// Get the idiomatic Swift rendering of a class name (for enums, records, errors, etc).
    fn class_name(&self, nm: &str) -> String {
        nm.to_string().to_upper_camel_case()
    }

    /// Get the idiomatic Swift rendering of a function name.
    fn fn_name(&self, nm: &str) -> String {
        nm.to_string().to_lower_camel_case()
    }

    /// Get the idiomatic Swift rendering of a variable name.
    fn var_name(&self, nm: &str) -> String {
        nm.to_string().to_lower_camel_case()
    }

    /// Get the idiomatic Swift rendering of an individual enum variant.
    fn enum_variant_name(&self, nm: &str) -> String {
        nm.to_string().to_lower_camel_case()
    }

    fn ffi_type_label_raw(&self, ffi_type: &FfiType) -> String {
        match ffi_type {
            FfiType::Int8 => "Int8".into(),
            FfiType::UInt8 => "UInt8".into(),
            FfiType::Int16 => "Int16".into(),
            FfiType::UInt16 => "UInt16".into(),
            FfiType::Int32 => "Int32".into(),
            FfiType::UInt32 => "UInt32".into(),
            FfiType::Int64 => "Int64".into(),
            FfiType::UInt64 => "UInt64".into(),
            FfiType::Float32 => "Float".into(),
            FfiType::Float64 => "Double".into(),
            FfiType::RustArcPtr(_) => "UnsafeMutableRawPointer".into(),
            FfiType::RustBuffer(_) => "RustBuffer".into(),
            FfiType::ForeignBytes => "ForeignBytes".into(),
            FfiType::ForeignCallback => "ForeignCallback".into(),
            FfiType::ForeignExecutorHandle => "Int".into(),
            FfiType::ForeignExecutorCallback => "ForeignExecutorCallback".into(),
            FfiType::RustFutureContinuationCallback => "UniFfiRustFutureContinuation".into(),
            FfiType::RustFutureHandle | FfiType::RustFutureContinuationData => {
                "UnsafeMutableRawPointer".into()
            }
        }
    }

    fn ffi_type_label(&self, ffi_type: &FfiType) -> String {
        match ffi_type {
            FfiType::ForeignCallback
            | FfiType::ForeignExecutorCallback
            | FfiType::RustFutureHandle
            | FfiType::RustFutureContinuationCallback
            | FfiType::RustFutureContinuationData => {
                format!("{} _Nonnull", self.ffi_type_label_raw(ffi_type))
            }
            _ => self.ffi_type_label_raw(ffi_type),
        }
    }

    fn ffi_canonical_name(&self, ffi_type: &FfiType) -> String {
        self.ffi_type_label_raw(ffi_type)
    }
}

pub mod filters {
    use super::*;
    pub use crate::backend::filters::*;

    fn oracle() -> &'static SwiftCodeOracle {
        &SwiftCodeOracle
    }

    pub fn type_name(as_type: &impl AsType) -> Result<String, askama::Error> {
        Ok(oracle().find(&as_type.as_type()).type_label())
    }

    pub fn canonical_name(as_type: &impl AsType) -> Result<String, askama::Error> {
        Ok(oracle().find(&as_type.as_type()).canonical_name())
    }

    pub fn ffi_converter_name(as_type: &impl AsType) -> Result<String, askama::Error> {
        Ok(oracle().find(&as_type.as_type()).ffi_converter_name())
    }

    pub fn lower_fn(as_type: &impl AsType) -> Result<String, askama::Error> {
        Ok(oracle().find(&as_type.as_type()).lower())
    }

    pub fn write_fn(as_type: &impl AsType) -> Result<String, askama::Error> {
        Ok(oracle().find(&as_type.as_type()).write())
    }

    pub fn lift_fn(as_type: &impl AsType) -> Result<String, askama::Error> {
        Ok(oracle().find(&as_type.as_type()).lift())
    }

    pub fn read_fn(as_type: &impl AsType) -> Result<String, askama::Error> {
        Ok(oracle().find(&as_type.as_type()).read())
    }

    pub fn literal_swift(
        literal: &Literal,
        as_type: &impl AsType,
    ) -> Result<String, askama::Error> {
        Ok(oracle().find(&as_type.as_type()).literal(literal))
    }

    /// Get the Swift type for an FFIType
    pub fn ffi_type_name(ffi_type: &FfiType) -> Result<String, askama::Error> {
        Ok(oracle().ffi_type_label(ffi_type))
    }

    pub fn ffi_canonical_name(ffi_type: &FfiType) -> Result<String, askama::Error> {
        Ok(oracle().ffi_canonical_name(ffi_type))
    }

    /// Like `ffi_type_name`, but used in `BridgingHeaderTemplate.h` which uses a slightly different
    /// names.
    pub fn header_ffi_type_name(ffi_type: &FfiType) -> Result<String, askama::Error> {
        Ok(match ffi_type {
            FfiType::Int8 => "int8_t".into(),
            FfiType::UInt8 => "uint8_t".into(),
            FfiType::Int16 => "int16_t".into(),
            FfiType::UInt16 => "uint16_t".into(),
            FfiType::Int32 => "int32_t".into(),
            FfiType::UInt32 => "uint32_t".into(),
            FfiType::Int64 => "int64_t".into(),
            FfiType::UInt64 => "uint64_t".into(),
            FfiType::Float32 => "float".into(),
            FfiType::Float64 => "double".into(),
            FfiType::RustArcPtr(_) => "void*_Nonnull".into(),
            FfiType::RustBuffer(_) => "RustBuffer".into(),
            FfiType::ForeignBytes => "ForeignBytes".into(),
            FfiType::ForeignCallback => "ForeignCallback _Nonnull".into(),
            FfiType::ForeignExecutorCallback => "UniFfiForeignExecutorCallback _Nonnull".into(),
            FfiType::ForeignExecutorHandle => "size_t".into(),
            FfiType::RustFutureContinuationCallback => {
                "UniFfiRustFutureContinuation _Nonnull".into()
            }
            FfiType::RustFutureHandle | FfiType::RustFutureContinuationData => {
                "void* _Nonnull".into()
            }
        })
    }

    /// Get the idiomatic Swift rendering of a class name (for enums, records, errors, etc).
    pub fn class_name(nm: &str) -> Result<String, askama::Error> {
        Ok(oracle().class_name(nm))
    }

    /// Get the idiomatic Swift rendering of a function name.
    pub fn fn_name(nm: &str) -> Result<String, askama::Error> {
        Ok(quote_general_keyword(oracle().fn_name(nm)))
    }

    /// Get the idiomatic Swift rendering of a variable name.
    pub fn var_name(nm: &str) -> Result<String, askama::Error> {
        Ok(quote_general_keyword(oracle().var_name(nm)))
    }

    /// Get the idiomatic Swift rendering of an arguments name.
    /// This is the same as the var name but quoting is not required.
    pub fn arg_name(nm: &str) -> Result<String, askama::Error> {
        Ok(quote_arg_keyword(oracle().var_name(nm)))
    }

    /// Get the idiomatic Swift rendering of an individual enum variant, quoted if it is a keyword (for use in e.g. declarations)
    pub fn enum_variant_swift_quoted(nm: &str) -> Result<String, askama::Error> {
        Ok(quote_general_keyword(oracle().enum_variant_name(nm)))
    }

    /// Get the idiomatic Swift rendering of an individual enum variant, for contexts (for use in non-declaration contexts where quoting is not needed)
    pub fn enum_variant_swift(nm: &str) -> Result<String, askama::Error> {
        Ok(oracle().enum_variant_name(nm))
    }

    pub fn error_handler(result: &ResultType) -> Result<String, askama::Error> {
        Ok(match &result.throws_type {
            Some(t) => format!("{}.lift", ffi_converter_name(t)?),
            None => "nil".into(),
        })
    }

    /// Name of the callback function to handle an async result
    pub fn future_callback(result: &ResultType) -> Result<String, askama::Error> {
        Ok(format!(
            "uniffiFutureCallbackHandler{}{}",
            match &result.return_type {
                Some(t) => SwiftCodeOracle.find(t).canonical_name(),
                None => "Void".into(),
            },
            match &result.throws_type {
                Some(t) => SwiftCodeOracle.find(t).canonical_name(),
                None => "".into(),
            }
        ))
    }
}
