/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::Result;
use askama::Template;
use heck::{ToShoutySnakeCase, ToSnakeCase, ToUpperCamelCase};
use serde::{Deserialize, Serialize};
use std::borrow::Borrow;
use std::collections::HashMap;

use crate::interface::*;
use crate::BindingsConfig;

const RESERVED_WORDS: &[&str] = &[
    "alias", "and", "BEGIN", "begin", "break", "case", "class", "def", "defined?", "do", "else",
    "elsif", "END", "end", "ensure", "false", "for", "if", "module", "next", "nil", "not", "or",
    "redo", "rescue", "retry", "return", "self", "super", "then", "true", "undef", "unless",
    "until", "when", "while", "yield", "__FILE__", "__LINE__",
];

fn is_reserved_word(word: &str) -> bool {
    RESERVED_WORDS.contains(&word)
}

impl Type {
    /// Get the canonical, unique-within-this-component name for a type.
    ///
    /// When generating helper code for foreign language bindings, it's sometimes useful to be
    /// able to name a particular type in order to e.g. call a helper function that is specific
    /// to that type. We support this by defining a naming convention where each type gets a
    /// unique canonical name, constructed recursively from the names of its component types (if any).
    pub fn canonical_name(&self) -> String {
        match self {
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
            Type::Enum(nm) => format!("Type{nm}"),
            Type::Record(nm) => format!("Type{nm}"),
            Type::CallbackInterface(nm) => format!("CallbackInterface{nm}"),
            Type::Timestamp => "Timestamp".into(),
            Type::Duration => "Duration".into(),
            Type::ForeignExecutor => "ForeignExecutor".into(),
            // Recursive types.
            // These add a prefix to the name of the underlying type.
            // The component API definition cannot give names to recursive types, so as long as the
            // prefixes we add here are all unique amongst themselves, then we have no chance of
            // acccidentally generating name collisions.
            Type::Optional(t) => format!("Optional{}", t.canonical_name()),
            Type::Sequence(t) => format!("Sequence{}", t.canonical_name()),
            Type::Map(k, v) => format!(
                "Map{}{}",
                k.canonical_name().to_upper_camel_case(),
                v.canonical_name().to_upper_camel_case()
            ),
            // A type that exists externally.
            Type::External { name, .. } | Type::Custom { name, .. } => format!("Type{name}"),
        }
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
    const TOML_KEY: &'static str = "ruby";

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
}
impl<'a> RubyWrapper<'a> {
    pub fn new(config: Config, ci: &'a ComponentInterface) -> Self {
        Self { config, ci }
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
            FfiType::RustArcPtr(_) => ":pointer".to_string(),
            FfiType::RustBuffer(_) => "RustBuffer.by_value".to_string(),
            FfiType::ForeignBytes => "ForeignBytes".to_string(),
            FfiType::ForeignCallback => unimplemented!("Callback interfaces are not implemented"),
            FfiType::ForeignExecutorCallback => {
                unimplemented!("Foreign executors are not implemented")
            }
            FfiType::ForeignExecutorHandle => {
                unimplemented!("Foreign executors are not implemented")
            }
            FfiType::FutureCallback { .. } | FfiType::FutureCallbackData => {
                unimplemented!("Async functions are not implemented")
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
            Literal::Null => "nil".into(),
            Literal::EmptySequence => "[]".into(),
            Literal::EmptyMap => "{}".into(),
            Literal::Enum(v, type_) => match type_ {
                Type::Enum(name) => format!("{}::{}", class_name_rb(name)?, enum_name_rb(v)?),
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
            Type::Object { .. } | Type::Enum(_) | Type::Record(_) => nm.to_string(),
            Type::String => format!("{ns}::uniffi_utf8({nm})"),
            Type::Bytes => format!("{ns}::uniffi_bytes({nm})"),
            Type::Timestamp | Type::Duration => nm.to_string(),
            Type::CallbackInterface(_) => panic!("No support for coercing callback interfaces yet"),
            Type::Optional(t) => format!("({nm} ? {} : nil)", coerce_rb(nm, ns, t)?),
            Type::Sequence(t) => {
                let coerce_code = coerce_rb("v", ns, t)?;
                if coerce_code == "v" {
                    nm.to_string()
                } else {
                    format!("{nm}.map {{ |v| {coerce_code} }}")
                }
            }
            Type::Map(_k, t) => {
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
            Type::ForeignExecutor => unimplemented!("Foreign executors are not implemented"),
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
            Type::Object { name, .. } => format!("({}._uniffi_lower {nm})", class_name_rb(name)?),
            Type::CallbackInterface(_) => panic!("No support for lowering callback interfaces yet"),
            Type::Enum(_)
            | Type::Record(_)
            | Type::Optional(_)
            | Type::Sequence(_)
            | Type::Timestamp
            | Type::Duration
            | Type::Map(_, _) => format!(
                "RustBuffer.alloc_from_{}({})",
                class_name_rb(&type_.canonical_name())?,
                nm
            ),
            Type::External { .. } => panic!("No support for lowering external types, yet"),
            Type::Custom { .. } => panic!("No support for lowering custom types, yet"),
            Type::ForeignExecutor => unimplemented!("Foreign executors are not implemented"),
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
            Type::Object { name, .. } => format!("{}._uniffi_allocate({nm})", class_name_rb(name)?),
            Type::CallbackInterface(_) => panic!("No support for lifting callback interfaces, yet"),
            Type::Enum(_) => {
                format!(
                    "{}.consumeInto{}",
                    nm,
                    class_name_rb(&type_.canonical_name())?
                )
            }
            Type::Record(_)
            | Type::Optional(_)
            | Type::Sequence(_)
            | Type::Timestamp
            | Type::Duration
            | Type::Map(_, _) => format!(
                "{}.consumeInto{}",
                nm,
                class_name_rb(&type_.canonical_name())?
            ),
            Type::External { .. } => panic!("No support for lifting external types, yet"),
            Type::Custom { .. } => panic!("No support for lifting custom types, yet"),
            Type::ForeignExecutor => unimplemented!("Foreign executors are not implemented"),
        })
    }
}

#[cfg(test)]
mod test_type {
    use super::*;

    #[test]
    fn test_canonical_names() {
        // Non-exhaustive, but gives a bit of a flavour of what we want.
        assert_eq!(Type::UInt8.canonical_name(), "u8");
        assert_eq!(Type::String.canonical_name(), "string");
        assert_eq!(Type::Bytes.canonical_name(), "bytes");
        assert_eq!(
            Type::Optional(Box::new(Type::Sequence(Box::new(Type::Object {
                name: "Example".into(),
                imp: ObjectImpl::Struct,
            }))))
            .canonical_name(),
            "OptionalSequenceTypeExample"
        );
    }
}

#[cfg(test)]
mod tests;
