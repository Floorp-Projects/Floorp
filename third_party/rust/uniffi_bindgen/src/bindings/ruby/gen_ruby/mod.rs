/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::Result;
use askama::Template;
use heck::{ToShoutySnakeCase, ToSnakeCase, ToUpperCamelCase};
use serde::{Deserialize, Serialize};
use std::borrow::Borrow;

use crate::interface::*;
use crate::MergeWith;

const RESERVED_WORDS: &[&str] = &[
    "alias", "and", "BEGIN", "begin", "break", "case", "class", "def", "defined?", "do", "else",
    "elsif", "END", "end", "ensure", "false", "for", "if", "module", "next", "nil", "not", "or",
    "redo", "rescue", "retry", "return", "self", "super", "then", "true", "undef", "unles",
    "until", "when", "while", "yield", "__FILE__", "__LINE__",
];

fn is_reserved_word(word: &str) -> bool {
    RESERVED_WORDS.contains(&word)
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

impl From<&ComponentInterface> for Config {
    fn from(ci: &ComponentInterface) -> Self {
        Config {
            cdylib_name: Some(format!("uniffi_{}", ci.namespace())),
            cdylib_path: None,
        }
    }
}

impl MergeWith for Config {
    fn merge_with(&self, other: &Self) -> Self {
        Config {
            cdylib_name: self.cdylib_name.merge_with(&other.cdylib_name),
            cdylib_path: self.cdylib_path.merge_with(&other.cdylib_path),
        }
    }
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

    pub fn type_ffi(type_: &FFIType) -> Result<String, askama::Error> {
        Ok(match type_ {
            FFIType::Int8 => ":int8".to_string(),
            FFIType::UInt8 => ":uint8".to_string(),
            FFIType::Int16 => ":int16".to_string(),
            FFIType::UInt16 => ":uint16".to_string(),
            FFIType::Int32 => ":int32".to_string(),
            FFIType::UInt32 => ":uint32".to_string(),
            FFIType::Int64 => ":int64".to_string(),
            FFIType::UInt64 => ":uint64".to_string(),
            FFIType::Float32 => ":float".to_string(),
            FFIType::Float64 => ":double".to_string(),
            FFIType::RustArcPtr(_) => ":pointer".to_string(),
            FFIType::RustBuffer => "RustBuffer.by_value".to_string(),
            FFIType::ForeignBytes => "ForeignBytes".to_string(),
            FFIType::ForeignCallback => unimplemented!("Callback interfaces are not implemented"),
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
            Literal::String(s) => format!("\"{}\"", s),
            Literal::Null => "nil".into(),
            Literal::EmptySequence => "[]".into(),
            Literal::EmptyMap => "{}".into(),
            Literal::Enum(v, type_) => match type_ {
                Type::Enum(name) => format!("{}::{}", class_name_rb(name)?, enum_name_rb(v)?),
                _ => panic!("Unexpected type in enum literal: {:?}", type_),
            },
            // https://docs.ruby-lang.org/en/2.0.0/syntax/literals_rdoc.html
            Literal::Int(i, radix, _) => match radix {
                Radix::Octal => format!("0o{:o}", i),
                Radix::Decimal => format!("{}", i),
                Radix::Hexadecimal => format!("{:#x}", i),
            },
            Literal::UInt(i, radix, _) => match radix {
                Radix::Octal => format!("0o{:o}", i),
                Radix::Decimal => format!("{}", i),
                Radix::Hexadecimal => format!("{:#x}", i),
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

        Ok(format!("{}{}", prefix, nm.to_snake_case()))
    }

    pub fn enum_name_rb(nm: &str) -> Result<String, askama::Error> {
        Ok(nm.to_string().to_shouty_snake_case())
    }

    pub fn coerce_rb(nm: &str, type_: &Type) -> Result<String, askama::Error> {
        Ok(match type_ {
            Type::Int8
            | Type::UInt8
            | Type::Int16
            | Type::UInt16
            | Type::Int32
            | Type::UInt32
            | Type::Int64
            | Type::UInt64 => format!("{}.to_i", nm), // TODO: check max/min value
            Type::Float32 | Type::Float64 => format!("{}.to_f", nm),
            Type::Boolean => format!("{} ? true : false", nm),
            Type::Object(_) | Type::Enum(_) | Type::Error(_) | Type::Record(_) => nm.to_string(),
            Type::String => format!("{}.to_s", nm),
            Type::Timestamp | Type::Duration => nm.to_string(),
            Type::CallbackInterface(_) => panic!("No support for coercing callback interfaces yet"),
            Type::Optional(t) => format!("({} ? {} : nil)", nm, coerce_rb(nm, t)?),
            Type::Sequence(t) => {
                let coerce_code = coerce_rb("v", t)?;
                if coerce_code == "v" {
                    nm.to_string()
                } else {
                    format!("{}.map {{ |v| {} }}", nm, coerce_code)
                }
            }
            Type::Map(_k, t) => {
                let k_coerce_code = coerce_rb("k", &Type::String)?;
                let v_coerce_code = coerce_rb("v", t)?;

                if k_coerce_code == "k" && v_coerce_code == "v" {
                    nm.to_string()
                } else {
                    format!(
                        "{}.each.with_object({{}}) {{ |(k, v), res| res[{}] = {} }}",
                        nm, k_coerce_code, v_coerce_code,
                    )
                }
            }
            Type::External { .. } => panic!("No support for external types, yet"),
            Type::Custom { .. } => panic!("No support for custom types, yet"),
            Type::Unresolved { .. } => {
                unreachable!("Type must be resolved before calling coerce_rb")
            }
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
            Type::Boolean => format!("({} ? 1 : 0)", nm),
            Type::String => format!("RustBuffer.allocFromString({})", nm),
            Type::Object(name) => format!("({}._uniffi_lower {})", class_name_rb(name)?, nm),
            Type::CallbackInterface(_) => panic!("No support for lowering callback interfaces yet"),
            Type::Error(_) => panic!("No support for lowering errors, yet"),
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
            Type::Unresolved { .. } => {
                unreachable!("Type must be resolved before calling lower_rb")
            }
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
            | Type::UInt64 => format!("{}.to_i", nm),
            Type::Float32 | Type::Float64 => format!("{}.to_f", nm),
            Type::Boolean => format!("1 == {}", nm),
            Type::String => format!("{}.consumeIntoString", nm),
            Type::Object(name) => format!("{}._uniffi_allocate({})", class_name_rb(name)?, nm),
            Type::CallbackInterface(_) => panic!("No support for lifting callback interfaces, yet"),
            Type::Error(_) => panic!("No support for lowering errors, yet"),
            Type::Enum(_)
            | Type::Record(_)
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
            Type::Unresolved { .. } => {
                unreachable!("Type must be resolved before calling lift_rb")
            }
        })
    }
}

#[cfg(test)]
mod tests;
