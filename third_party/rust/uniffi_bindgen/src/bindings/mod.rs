/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Generate foreign language bindings for a uniffi component.
//!
//! This module contains all the code for generating foreign language bindings,
//! along with some helpers for executing foreign language scripts or tests.

use anyhow::{bail, Result};
use camino::Utf8Path;
use serde::{Deserialize, Serialize};
use std::fmt;

use crate::interface::ComponentInterface;

pub mod kotlin;
pub mod python;
pub mod ruby;
pub mod swift;

/// Enumeration of all foreign language targets currently supported by this crate.
///
/// The functions in this module will delegate to a language-specific backend based
/// on the provided `TargetLanguage`. For convenience of calling code we also provide
/// a few `TryFrom` implementations to help guess the correct target language from
/// e.g. a file extension of command-line argument.
#[derive(Copy, Clone, Eq, PartialEq, Hash)]
#[cfg_attr(feature = "clap", derive(clap::ValueEnum))]
pub enum TargetLanguage {
    Kotlin,
    Swift,
    Python,
    Ruby,
}

impl fmt::Display for TargetLanguage {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Kotlin => write!(f, "kotlin"),
            Self::Swift => write!(f, "swift"),
            Self::Python => write!(f, "python"),
            Self::Ruby => write!(f, "ruby"),
        }
    }
}

/// Mode for the `run_script` function defined for each language
#[derive(Clone, Debug)]
pub struct RunScriptOptions {
    pub show_compiler_messages: bool,
}

impl Default for RunScriptOptions {
    fn default() -> Self {
        Self {
            show_compiler_messages: true,
        }
    }
}

impl TryFrom<&str> for TargetLanguage {
    type Error = anyhow::Error;
    fn try_from(value: &str) -> Result<Self> {
        Ok(match value.to_ascii_lowercase().as_str() {
            "kotlin" | "kt" | "kts" => TargetLanguage::Kotlin,
            "swift" => TargetLanguage::Swift,
            "python" | "py" => TargetLanguage::Python,
            "ruby" | "rb" => TargetLanguage::Ruby,
            _ => bail!("Unknown or unsupported target language: \"{value}\""),
        })
    }
}

impl TryFrom<&std::ffi::OsStr> for TargetLanguage {
    type Error = anyhow::Error;
    fn try_from(value: &std::ffi::OsStr) -> Result<Self> {
        match value.to_str() {
            None => bail!("Unreadable target language"),
            Some(s) => s.try_into(),
        }
    }
}

impl TryFrom<String> for TargetLanguage {
    type Error = anyhow::Error;
    fn try_from(value: String) -> Result<Self> {
        TryFrom::try_from(value.as_str())
    }
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct Config {
    #[serde(default)]
    pub(crate) kotlin: kotlin::Config,
    #[serde(default)]
    pub(crate) swift: swift::Config,
    #[serde(default)]
    pub(crate) python: python::Config,
    #[serde(default)]
    pub(crate) ruby: ruby::Config,
}

/// Generate foreign language bindings from a compiled `uniffi` library.
pub fn write_bindings(
    config: &Config,
    ci: &ComponentInterface,
    out_dir: &Utf8Path,
    language: TargetLanguage,
    try_format_code: bool,
) -> Result<()> {
    match language {
        TargetLanguage::Kotlin => {
            kotlin::write_bindings(&config.kotlin, ci, out_dir, try_format_code)?
        }
        TargetLanguage::Swift => {
            swift::write_bindings(&config.swift, ci, out_dir, try_format_code)?
        }
        TargetLanguage::Python => {
            python::write_bindings(&config.python, ci, out_dir, try_format_code)?
        }
        TargetLanguage::Ruby => ruby::write_bindings(&config.ruby, ci, out_dir, try_format_code)?,
    }
    Ok(())
}
