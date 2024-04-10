/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::{bail, Result};
use askama::Template;
use camino::Utf8Path;
use heck::{ToShoutySnakeCase, ToSnakeCase, ToUpperCamelCase};
use serde::{Deserialize, Serialize};
use std::borrow::Borrow;
use std::collections::HashMap;

use crate::bindings::ruby;
use crate::interface::*;
use crate::{BindingGenerator, BindingsConfig};

pub struct RubyBindingGenerator;
impl BindingGenerator for RubyBindingGenerator {
    type Config = Config;

    fn write_bindings(
        &self,
        ci: &ComponentInterface,
        config: &Config,
        out_dir: &Utf8Path,
        try_format_code: bool,
    ) -> Result<()> {
        ruby::write_bindings(config, ci, out_dir, try_format_code)
    }

    fn check_library_path(&self, library_path: &Utf8Path, cdylib_name: Option<&str>) -> Result<()> {
        if cdylib_name.is_none() {
            bail!("Generate bindings for Ruby requires a cdylib, but {library_path} was given");
        }
        Ok(())
    }
}

const RESERVED_WORDS: &[&str] = &[
    "alias", "and", "BEGIN", "begin", "break", "case", "class", "def", "defined?", "do", "else",
    "elsif", "END", "end", "ensure", "false", "for", "if", "module", "next", "nil", "not", "or",
    "redo", "rescue", "retry", "return", "self", "super", "then", "true", "undef", "unless",
    "until", "when", "while", "yield", "__FILE__", "__LINE__",
];

fn is_reserved_word(word: &str) -> bool {
    RESERVED_WORDS.contains(&word)
}

/// Get the canonical, unique-within-this-component name for a type.
///
/// When generating helper code for foreign language bindings, it's sometimes useful to be
/// able to name a particular type in order to e.g. call a helper function that is specific
/// to that type. We support this by defining a naming convention where each type gets a
/// unique canonical name, constructed recursively from the names of its component types (if any).
pub fn canonical_name(t: &Type) -> String {
    match t {
        // Builtin primitive types, with plain old names.
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
        Type::String => "string".into(),
        Type::Bytes => "bytes".into(),
        Type::Boolean => "bool".into(),
        // API defined types.
        // Note that these all get unique names, and the parser ensures that the names do not
        // conflict with a builtin type. We add a prefix to the name to guard against pathological
        // cases like a record named `SequenceRecord` interfering with `sequence<Record>`.
        // However, types that support importing all end up with the same prefix of "Type", so
        // that the import handling code knows how to find the remote reference.
        Type::Object { name, .. } => format!("Type{name}"),
        Type::Enum { name, .. } => format!("Type{name}"),
        Type::Record { name, .. } => format!("Type{name}"),
        Type::CallbackInterface { name, .. } => format!("CallbackInterface{name}"),
        Type::Timestamp => "Timestamp".into(),
        Type::Duration => "Duration".into(),
        // Recursive types.
        // These add a prefix to the name of the underlying type.
        // The component API definition cannot give names to recursive types, so as long as the
        // prefixes we add here are all unique amongst themselves, then we have no chance of
        // acccidentally generating name collisions.
        Type::Optional { inner_type } => format!("Optional{}", canonical_name(inner_type)),
        Type::Sequence { inner_type } => format!("Sequence{}", canonical_name(inner_type)),
        Type::Map {
            key_type,
            value_type,
        } => format!(
            "Map{}{}",
            canonical_name(key_type).to_upper_camel_case(),
            canonical_name(value_type).to_upper_camel_case()
        ),
        // A type that exists externally.
        Type::External { name, .. } | Type::Custom { name, .. } => format!("Type{name}"),
    }
}

// Some config options for it the caller wants to customize the generated ruby.
// Note that this can only be used to control details of the ruby *that do not affect the underlying component*,
// since the details of the underlying component are entirely determined by the `ComponentInterface`.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct Config {
    cdylib_name: Option<String>,
    cdylib_path: Option<String>,
}

impl Config {
    pub fn cdylib_name(&self) -> String {
        self.cdylib_name
            .clone()
            .unwrap_or_else(|| "uniffi".to_string())
    }

    pub fn custom_cdylib_path(&self) -> bool {
        self.cdylib_path.is_some()
    }

    pub fn cdylib_path(&self) -> String {
        self.cdylib_path.clone().unwrap_or_default()
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

#[derive(Template)]
#[template(syntax = "rb", escape = "none", path = "wrapper.rb")]
pub struct RubyWrapper<'a> {
    config: Config,
    ci: &'a ComponentInterface,
    canonical_name: &'a dyn Fn(&Type) -> String,
}
impl<'a> RubyWrapper<'a> {
    pub fn new(config: Config, ci: &'a ComponentInterface) -> Self {
        Self {
            config,
            ci,
            canonical_name: &canonical_name,
        }
    }
}

mod filters {
    use super::*;
    pub use crate::backend::filters::*;

    pub fn type_ffi(type_: &FfiType) -> Result<String, askama::Error> {
        Ok(match type_ {
            FfiType::Int8 => ":int8".to_string(),
            FfiType::UInt8 => ":uint8".to_string(),
            FfiType::Int16 => ":int16".to_string(),
            FfiType::UInt16 => ":uint16".to_string(),
            FfiType::Int32 => ":int32".to_string(),
            FfiType::UInt32 => ":uint32".to_string(),
            FfiType::Int64 => ":int64".to_string(),
            FfiType::UInt64 => ":uint64".to_string(),
            FfiType::Float32 => ":float".to_string(),
            FfiType::Float64 => ":double".to_string(),
            FfiType::Handle => ":uint64".to_string(),
            FfiType::RustArcPtr(_) => ":pointer".to_string(),
            FfiType::RustBuffer(_) => "RustBuffer.by_value".to_string(),
            FfiType::RustCallStatus => "RustCallStatus".to_string(),
            FfiType::ForeignBytes => "ForeignBytes".to_string(),
            FfiType::Callback(_) => unimplemented!("FFI Callbacks not implemented"),
            // Note: this can't just be `unimplemented!()` because some of the FFI function
            // definitions use references.  Those FFI functions aren't actually used, so we just
            // pick something that runs and makes some sense.  Revisit this once the references
            // are actually implemented.
            FfiType::Reference(_) => ":pointer".to_string(),
            FfiType::VoidPointer => ":pointer".to_string(),
            FfiType::Struct(_) => {
                unimplemented!("Structs are not implemented")
            }
        })
    }

    pub fn literal_rb(literal: &Literal) -> Result<String, askama::Error> {
        Ok(match literal {
            Literal::Boolean(v) => {
                if *v {
                    "true".into()
                } else {
                    "false".into()
                }
            }
            // use the double-quote form to match with the other languages, and quote escapes.
            Literal::String(s) => format!("\"{s}\""),
            Literal::None => "nil".into(),
            Literal::Some { inner } => literal_rb(inner)?,
            Literal::EmptySequence => "[]".into(),
            Literal::EmptyMap => "{}".into(),
            Literal::Enum(v, type_) => match type_ {
                Type::Enum { name, .. } => {
                    format!("{}::{}", class_name_rb(name)?, enum_name_rb(v)?)
                }
                _ => panic!("Unexpected type in enum literal: {type_:?}"),
            },
            // https://docs.ruby-lang.org/en/2.0.0/syntax/literals_rdoc.html
            Literal::Int(i, radix, _) => match radix {
                Radix::Octal => format!("0o{i:o}"),
                Radix::Decimal => format!("{i}"),
                Radix::Hexadecimal => format!("{i:#x}"),
            },
            Literal::UInt(i, radix, _) => match radix {
                Radix::Octal => format!("0o{i:o}"),
                Radix::Decimal => format!("{i}"),
                Radix::Hexadecimal => format!("{i:#x}"),
            },
            Literal::Float(string, _type_) => string.clone(),
        })
    }

    pub fn class_name_rb(nm: &str) -> Result<String, askama::Error> {
        Ok(nm.to_string().to_upper_camel_case())
    }

    pub fn fn_name_rb(nm: &str) -> Result<String, askama::Error> {
        Ok(nm.to_string().to_snake_case())
    }

    pub fn var_name_rb(nm: &str) -> Result<String, askama::Error> {
        let nm = nm.to_string();
        let prefix = if is_reserved_word(&nm) { "_" } else { "" };

        Ok(format!("{prefix}{}", nm.to_snake_case()))
    }

    pub fn enum_name_rb(nm: &str) -> Result<String, askama::Error> {
        Ok(nm.to_string().to_shouty_snake_case())
    }

    pub fn coerce_rb(nm: &str, ns: &str, type_: &Type) -> Result<String, askama::Error> {
        Ok(match type_ {
            Type::Int8 => format!("{ns}::uniffi_in_range({nm}, \"i8\", -2**7, 2**7)"),
            Type::Int16 => format!("{ns}::uniffi_in_range({nm}, \"i16\", -2**15, 2**15)"),
            Type::Int32 => format!("{ns}::uniffi_in_range({nm}, \"i32\", -2**31, 2**31)"),
            Type::Int64 => format!("{ns}::uniffi_in_range({nm}, \"i64\", -2**63, 2**63)"),
            Type::UInt8 => format!("{ns}::uniffi_in_range({nm}, \"u8\", 0, 2**8)"),
            Type::UInt16 => format!("{ns}::uniffi_in_range({nm}, \"u16\", 0, 2**16)"),
            Type::UInt32 => format!("{ns}::uniffi_in_range({nm}, \"u32\", 0, 2**32)"),
            Type::UInt64 => format!("{ns}::uniffi_in_range({nm}, \"u64\", 0, 2**64)"),
            Type::Float32 | Type::Float64 => nm.to_string(),
            Type::Boolean => format!("{nm} ? true : false"),
            Type::Object { .. } | Type::Enum { .. } | Type::Record { .. } => nm.to_string(),
            Type::String => format!("{ns}::uniffi_utf8({nm})"),
            Type::Bytes => format!("{ns}::uniffi_bytes({nm})"),
            Type::Timestamp | Type::Duration => nm.to_string(),
            Type::CallbackInterface { .. } => {
                panic!("No support for coercing callback interfaces yet")
            }
            Type::Optional { inner_type: t } => format!("({nm} ? {} : nil)", coerce_rb(nm, ns, t)?),
            Type::Sequence { inner_type: t } => {
                let coerce_code = coerce_rb("v", ns, t)?;
                if coerce_code == "v" {
                    nm.to_string()
                } else {
                    format!("{nm}.map {{ |v| {coerce_code} }}")
                }
            }
            Type::Map { value_type: t, .. } => {
                let k_coerce_code = coerce_rb("k", ns, &Type::String)?;
                let v_coerce_code = coerce_rb("v", ns, t)?;

                if k_coerce_code == "k" && v_coerce_code == "v" {
                    nm.to_string()
                } else {
                    format!(
                        "{nm}.each.with_object({{}}) {{ |(k, v), res| res[{k_coerce_code}] = {v_coerce_code} }}"
                    )
                }
            }
            Type::External { .. } => panic!("No support for external types, yet"),
            Type::Custom { .. } => panic!("No support for custom types, yet"),
        })
    }

    pub fn check_lower_rb(nm: &str, type_: &Type) -> Result<String, askama::Error> {
        Ok(match type_ {
            Type::Object { name, .. } => {
                format!("({}.uniffi_check_lower {nm})", class_name_rb(name)?)
            }
            Type::Enum { .. }
            | Type::Record { .. }
            | Type::Optional { .. }
            | Type::Sequence { .. }
            | Type::Map { .. } => format!(
                "RustBuffer.check_lower_{}({})",
                class_name_rb(&canonical_name(type_))?,
                nm
            ),
            _ => "".to_owned(),
        })
    }

    pub fn lower_rb(nm: &str, type_: &Type) -> Result<String, askama::Error> {
        Ok(match type_ {
            Type::Int8
            | Type::UInt8
            | Type::Int16
            | Type::UInt16
            | Type::Int32
            | Type::UInt32
            | Type::Int64
            | Type::UInt64
            | Type::Float32
            | Type::Float64 => nm.to_string(),
            Type::Boolean => format!("({nm} ? 1 : 0)"),
            Type::String => format!("RustBuffer.allocFromString({nm})"),
            Type::Bytes => format!("RustBuffer.allocFromBytes({nm})"),
            Type::Object { name, .. } => format!("({}.uniffi_lower {nm})", class_name_rb(name)?),
            Type::CallbackInterface { .. } => {
                panic!("No support for lowering callback interfaces yet")
            }
            Type::Enum { .. }
            | Type::Record { .. }
            | Type::Optional { .. }
            | Type::Sequence { .. }
            | Type::Timestamp
            | Type::Duration
            | Type::Map { .. } => format!(
                "RustBuffer.alloc_from_{}({})",
                class_name_rb(&canonical_name(type_))?,
                nm
            ),
            Type::External { .. } => panic!("No support for lowering external types, yet"),
            Type::Custom { .. } => panic!("No support for lowering custom types, yet"),
        })
    }

    pub fn lift_rb(nm: &str, type_: &Type) -> Result<String, askama::Error> {
        Ok(match type_ {
            Type::Int8
            | Type::UInt8
            | Type::Int16
            | Type::UInt16
            | Type::Int32
            | Type::UInt32
            | Type::Int64
            | Type::UInt64 => format!("{nm}.to_i"),
            Type::Float32 | Type::Float64 => format!("{nm}.to_f"),
            Type::Boolean => format!("1 == {nm}"),
            Type::String => format!("{nm}.consumeIntoString"),
            Type::Bytes => format!("{nm}.consumeIntoBytes"),
            Type::Object { name, .. } => format!("{}.uniffi_allocate({nm})", class_name_rb(name)?),
            Type::CallbackInterface { .. } => {
                panic!("No support for lifting callback interfaces, yet")
            }
            Type::Enum { .. } => {
                format!(
                    "{}.consumeInto{}",
                    nm,
                    class_name_rb(&canonical_name(type_))?
                )
            }
            Type::Record { .. }
            | Type::Optional { .. }
            | Type::Sequence { .. }
            | Type::Timestamp
            | Type::Duration
            | Type::Map { .. } => format!(
                "{}.consumeInto{}",
                nm,
                class_name_rb(&canonical_name(type_))?
            ),
            Type::External { .. } => panic!("No support for lifting external types, yet"),
            Type::Custom { .. } => panic!("No support for lifting custom types, yet"),
        })
    }
}

#[cfg(test)]
mod test_type {
    use super::*;

    #[test]
    fn test_canonical_names() {
        // Non-exhaustive, but gives a bit of a flavour of what we want.
        assert_eq!(canonical_name(&Type::UInt8), "u8");
        assert_eq!(canonical_name(&Type::String), "string");
        assert_eq!(canonical_name(&Type::Bytes), "bytes");
        assert_eq!(
            canonical_name(&Type::Optional {
                inner_type: Box::new(Type::Sequence {
                    inner_type: Box::new(Type::Object {
                        module_path: "anything".to_string(),
                        name: "Example".into(),
                        imp: ObjectImpl::Struct,
                    })
                })
            }),
            "OptionalSequenceTypeExample"
        );
    }
}

#[cfg(test)]
mod tests;
