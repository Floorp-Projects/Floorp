/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Uniffi: easily build cross-platform software components in Rust
//!
//! This is a highly-experimental crate for building cross-language software components
//! in Rust, based on things we've learned and patterns we've developed in the
//! [mozilla/application-services](https://github.com/mozilla/application-services) project.
//!
//! The idea is to let you write your code once, in Rust, and then re-use it from many
//! other programming languages via Rust's C-compatible FFI layer and some automagically
//! generated binding code. If you think of it as a kind of [wasm-bindgen](https://github.com/rustwasm/wasm-bindgen)
//! wannabe, with a clunkier developer experience but support for more target languages,
//! you'll be pretty close to the mark.
//!
//! Currently supported target languages include Kotlin, Swift and Python.
//!
//! ## Usage
//
//! To build a cross-language component using `uniffi`, follow these steps.
//!
//! ### 1) Specify your Component Interface
//!
//! Start by thinking about the interface you want to expose for use
//! from other languages. Use the Interface Definition Language to specify your interface
//! in a `.udl` file, where it can be processed by the tools from this crate.
//! For example you might define an interface like this:
//!
//! ```text
//! namespace example {
//!   u32 foo(u32 bar);
//! }
//!
//! dictionary MyData {
//!   u32 num_foos;
//!   bool has_a_bar;
//! }
//! ```
//!
//! ### 2) Implement the Component Interface as a Rust crate
//!
//! With the interface, defined, provide a corresponding implementation of that interface
//! as a standard-looking Rust crate, using functions and structs and so-on. For example
//! an implementation of the above Component Interface might look like this:
//!
//! ```text
//! fn foo(bar: u32) -> u32 {
//!     // TODO: a better example!
//!     bar + 42
//! }
//!
//! struct MyData {
//!   num_foos: u32,
//!   has_a_bar: bool
//! }
//! ```
//!
//! ### 3) Generate and include component scaffolding from the UDL file
//!
//! First you will need to install `uniffi-bindgen` on your system using `cargo install uniffi_bindgen`.
//! Then add to your crate `uniffi_build` under `[build-dependencies]`.
//! Finally, add a `build.rs` script to your crate and have it call `uniffi_build::generate_scaffolding`
//! to process your `.udl` file. This will generate some Rust code to be included in the top-level source
//! code of your crate. If your UDL file is named `example.udl`, then your build script would call:
//!
//! ```text
//! uniffi_build::generate_scaffolding("src/example.udl")
//! ```
//!
//! This would output a rust file named `example.uniffi.rs`, ready to be
//! included into the code of your rust crate like this:
//!
//! ```text
//! include_scaffolding!("example");
//! ```
//!
//! ### 4) Generate foreign language bindings for the library
//!
//! The `uniffi-bindgen` utility provides a command-line tool that can produce code to
//! consume the Rust library in any of several supported languages.
//! It is done by calling (in kotlin for example):
//!
//! ```text
//! uniffi-bindgen --language kotlin ./src/example.udl
//! ```
//!
//! This will produce a file `example.kt` in the same directory as the .udl file, containing kotlin bindings
//! to load and use the compiled rust code via its C-compatible FFI.
//!

#![warn(rust_2018_idioms, unused_qualifications)]
#![allow(unknown_lints)]

const BINDGEN_VERSION: &str = env!("CARGO_PKG_VERSION");

use anyhow::{anyhow, bail, Context, Result};
use camino::{Utf8Path, Utf8PathBuf};
use fs_err::{self as fs, File};
use serde::{de::DeserializeOwned, Deserialize, Serialize};
use std::io::prelude::*;
use std::io::ErrorKind;
use std::{collections::HashMap, env, process::Command, str::FromStr};

pub mod backend;
pub mod bindings;
pub mod interface;
pub mod library_mode;
pub mod macro_metadata;
pub mod scaffolding;

use bindings::TargetLanguage;
pub use interface::ComponentInterface;
use scaffolding::RustScaffolding;

/// Trait for bindings configuration.  Each bindings language defines one of these.
///
/// BindingsConfigs are initially loaded from `uniffi.toml` file.  Then the trait methods are used
/// to fill in missing values.
pub trait BindingsConfig: DeserializeOwned {
    /// key in the `bindings` table from `uniffi.toml` for this configuration
    const TOML_KEY: &'static str;

    /// Update missing values using the `ComponentInterface`
    fn update_from_ci(&mut self, ci: &ComponentInterface);

    /// Update missing values using the dylib file for the main crate, when in library mode.
    ///
    /// cdylib_name will be the library filename without the leading `lib` and trailing extension
    fn update_from_cdylib_name(&mut self, cdylib_name: &str);

    /// Update missing values from config instances from dependent crates
    ///
    /// config_map maps crate names to config instances. This is mostly used to set up external
    /// types.
    fn update_from_dependency_configs(&mut self, config_map: HashMap<&str, &Self>);
}

fn load_bindings_config<BC: BindingsConfig>(
    ci: &ComponentInterface,
    crate_root: &Utf8Path,
    config_file_override: Option<&Utf8Path>,
) -> Result<BC> {
    // Load the config from the TOML value, falling back to an empty map if it doesn't exist
    let toml_config = load_bindings_config_toml(crate_root, config_file_override)?
        .and_then(|mut v| v.as_table_mut().and_then(|t| t.remove(BC::TOML_KEY)))
        .unwrap_or_else(|| toml::Value::from(toml::value::Table::default()));

    let mut config: BC = toml_config.try_into()?;
    config.update_from_ci(ci);
    Ok(config)
}

/// Binding generator config with no members
#[derive(Clone, Debug, Deserialize, Hash, PartialEq, PartialOrd, Ord, Eq)]
pub struct EmptyBindingsConfig;

impl BindingsConfig for EmptyBindingsConfig {
    const TOML_KEY: &'static str = "";

    fn update_from_ci(&mut self, _ci: &ComponentInterface) {}
    fn update_from_cdylib_name(&mut self, _cdylib_name: &str) {}
    fn update_from_dependency_configs(&mut self, _config_map: HashMap<&str, &Self>) {}
}

// Load the binding-specific config
//
// This function calculates the location of the config TOML file, parses it, and returns the result
// as a toml::Value
//
// If there is an error parsing the file then Err will be returned. If the file is missing or the
// entry for the bindings is missing, then Ok(None) will be returned.
fn load_bindings_config_toml(
    crate_root: &Utf8Path,
    config_file_override: Option<&Utf8Path>,
) -> Result<Option<toml::Value>> {
    let config_path = match config_file_override {
        Some(cfg) => cfg.to_owned(),
        None => crate_root.join("uniffi.toml"),
    };

    if !config_path.exists() {
        return Ok(None);
    }

    let contents = fs::read_to_string(&config_path)
        .with_context(|| format!("Failed to read config file from {config_path}"))?;
    let mut full_config = toml::Value::from_str(&contents)
        .with_context(|| format!("Failed to parse config file {config_path}"))?;

    Ok(full_config
        .as_table_mut()
        .and_then(|t| t.remove("bindings")))
}

/// A trait representing a UniFFI Binding Generator
///
/// External crates that implement binding generators, should implement this type
/// and call the [`generate_external_bindings`] using a type that implements this trait.
pub trait BindingGenerator: Sized {
    /// Handles configuring the bindings
    type Config: BindingsConfig;

    /// Writes the bindings to the output directory
    ///
    /// # Arguments
    /// - `ci`: A [`ComponentInterface`] representing the interface
    /// - `config`: An instance of the [`BindingsConfig`] associated with this type
    /// - `out_dir`: The path to where the binding generator should write the output bindings
    fn write_bindings(
        &self,
        ci: ComponentInterface,
        config: Self::Config,
        out_dir: &Utf8Path,
    ) -> Result<()>;
}

/// Generate bindings for an external binding generator
/// Ideally, this should replace the [`generate_bindings`] function below
///
/// Implements an entry point for external binding generators.
/// The function does the following:
/// - It parses the `udl` in a [`ComponentInterface`]
/// - Parses the `uniffi.toml` and loads it into the type that implements [`BindingsConfig`]
/// - Creates an instance of [`BindingGenerator`], based on type argument `B`, and run [`BindingGenerator::write_bindings`] on it
///
/// # Arguments
/// - `binding_generator`: Type that implements BindingGenerator
/// - `udl_file`: The path to the UDL file
/// - `config_file_override`: The path to the configuration toml file, most likely called `uniffi.toml`. If [`None`], the function will try to guess based on the crate's root.
/// - `out_dir_override`: The path to write the bindings to. If [`None`], it will be the path to the parent directory of the `udl_file`
pub fn generate_external_bindings(
    binding_generator: impl BindingGenerator,
    udl_file: impl AsRef<Utf8Path>,
    config_file_override: Option<impl AsRef<Utf8Path>>,
    out_dir_override: Option<impl AsRef<Utf8Path>>,
) -> Result<()> {
    let out_dir_override = out_dir_override.as_ref().map(|p| p.as_ref());
    let config_file_override = config_file_override.as_ref().map(|p| p.as_ref());

    let crate_root = guess_crate_root(udl_file.as_ref())?;
    let out_dir = get_out_dir(udl_file.as_ref(), out_dir_override)?;
    let component = parse_udl(udl_file.as_ref()).context("Error parsing UDL")?;
    let bindings_config = load_bindings_config(&component, crate_root, config_file_override)?;
    binding_generator.write_bindings(component, bindings_config, &out_dir)
}

// Generate the infrastructural Rust code for implementing the UDL interface,
// such as the `extern "C"` function definitions and record data types.
pub fn generate_component_scaffolding(
    udl_file: &Utf8Path,
    out_dir_override: Option<&Utf8Path>,
    format_code: bool,
) -> Result<()> {
    let component = parse_udl(udl_file)?;
    let file_stem = udl_file.file_stem().context("not a file")?;
    let filename = format!("{file_stem}.uniffi.rs");
    let out_path = get_out_dir(udl_file, out_dir_override)?.join(filename);
    let mut f = File::create(&out_path)?;
    write!(f, "{}", RustScaffolding::new(&component)).context("Failed to write output file")?;
    if format_code {
        format_code_with_rustfmt(&out_path)?;
    }
    Ok(())
}

// Generate the bindings in the target languages that call the scaffolding
// Rust code.
pub fn generate_bindings(
    udl_file: &Utf8Path,
    config_file_override: Option<&Utf8Path>,
    target_languages: Vec<TargetLanguage>,
    out_dir_override: Option<&Utf8Path>,
    library_file: Option<&Utf8Path>,
    try_format_code: bool,
) -> Result<()> {
    let mut component = parse_udl(udl_file)?;
    if let Some(library_file) = library_file {
        macro_metadata::add_to_ci_from_library(&mut component, library_file)?;
    }
    let crate_root = &guess_crate_root(udl_file).context("Failed to guess crate root")?;

    let mut config = Config::load_initial(crate_root, config_file_override)?;
    config.update_from_ci(&component);
    let out_dir = get_out_dir(udl_file, out_dir_override)?;
    for language in target_languages {
        bindings::write_bindings(
            &config.bindings,
            &component,
            &out_dir,
            language,
            try_format_code,
        )?;
    }

    Ok(())
}

pub fn dump_json(library_path: &Utf8Path) -> Result<String> {
    let metadata = macro_metadata::extract_from_library(library_path)?;
    Ok(serde_json::to_string_pretty(&metadata)?)
}

pub fn print_json(library_path: &Utf8Path) -> Result<()> {
    println!("{}", dump_json(library_path)?);
    Ok(())
}

/// Guess the root directory of the crate from the path of its UDL file.
///
/// For now, we assume that the UDL file is in `./src/something.udl` relative
/// to the crate root. We might consider something more sophisticated in
/// future.
pub fn guess_crate_root(udl_file: &Utf8Path) -> Result<&Utf8Path> {
    let path_guess = udl_file
        .parent()
        .context("UDL file has no parent folder!")?
        .parent()
        .context("UDL file has no grand-parent folder!")?;
    if !path_guess.join("Cargo.toml").is_file() {
        bail!("UDL file does not appear to be inside a crate")
    }
    Ok(path_guess)
}

fn get_out_dir(udl_file: &Utf8Path, out_dir_override: Option<&Utf8Path>) -> Result<Utf8PathBuf> {
    Ok(match out_dir_override {
        Some(s) => {
            // Create the directory if it doesn't exist yet.
            fs::create_dir_all(s)?;
            s.canonicalize_utf8().context("Unable to find out-dir")?
        }
        None => udl_file
            .parent()
            .context("File has no parent directory")?
            .to_owned(),
    })
}

fn parse_udl(udl_file: &Utf8Path) -> Result<ComponentInterface> {
    let udl = fs::read_to_string(udl_file)
        .with_context(|| format!("Failed to read UDL from {udl_file}"))?;
    ComponentInterface::from_webidl(&udl).context("Failed to parse UDL")
}

fn format_code_with_rustfmt(path: &Utf8Path) -> Result<()> {
    let status = Command::new("rustfmt").arg(path).status().map_err(|e| {
        let ctx = match e.kind() {
            ErrorKind::NotFound => "formatting was requested, but rustfmt was not found",
            _ => "unknown error when calling rustfmt",
        };
        anyhow!(e).context(ctx)
    })?;
    if !status.success() {
        bail!("rustmt failed when formatting scaffolding. Note: --no-format can be used to skip formatting");
    }
    Ok(())
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct Config {
    #[serde(default)]
    bindings: bindings::Config,
}

impl Config {
    fn load_initial(
        crate_root: &Utf8Path,
        config_file_override: Option<&Utf8Path>,
    ) -> Result<Self> {
        let path = match config_file_override {
            Some(cfg) => Some(cfg.to_owned()),
            None => crate_root.join("uniffi.toml").canonicalize_utf8().ok(),
        };
        let toml_config = match path {
            Some(path) => {
                let contents = fs::read_to_string(path).context("Failed to read config file")?;
                toml::de::from_str(&contents)?
            }
            None => toml::Value::from(toml::value::Table::default()),
        };
        Ok(toml_config.try_into()?)
    }

    fn update_from_ci(&mut self, ci: &ComponentInterface) {
        self.bindings.kotlin.update_from_ci(ci);
        self.bindings.swift.update_from_ci(ci);
        self.bindings.python.update_from_ci(ci);
        self.bindings.ruby.update_from_ci(ci);
    }

    fn update_from_cdylib_name(&mut self, cdylib_name: &str) {
        self.bindings.kotlin.update_from_cdylib_name(cdylib_name);
        self.bindings.swift.update_from_cdylib_name(cdylib_name);
        self.bindings.python.update_from_cdylib_name(cdylib_name);
        self.bindings.ruby.update_from_cdylib_name(cdylib_name);
    }

    fn update_from_dependency_configs(&mut self, config_map: HashMap<&str, &Self>) {
        self.bindings.kotlin.update_from_dependency_configs(
            config_map
                .iter()
                .map(|(key, config)| (*key, &config.bindings.kotlin))
                .collect(),
        );
        self.bindings.swift.update_from_dependency_configs(
            config_map
                .iter()
                .map(|(key, config)| (*key, &config.bindings.swift))
                .collect(),
        );
        self.bindings.python.update_from_dependency_configs(
            config_map
                .iter()
                .map(|(key, config)| (*key, &config.bindings.python))
                .collect(),
        );
        self.bindings.ruby.update_from_dependency_configs(
            config_map
                .iter()
                .map(|(key, config)| (*key, &config.bindings.ruby))
                .collect(),
        );
    }
}

// FIXME(HACK):
// Include the askama config file into the build.
// That way cargo tracks the file and other tools relying on file tracking see it as well.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1774585
// In the future askama should handle that itself by using the `track_path::path` API,
// see https://github.com/rust-lang/rust/pull/84029
#[allow(dead_code)]
mod __unused {
    const _: &[u8] = include_bytes!("../askama.toml");
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_guessing_of_crate_root_directory_from_udl_file() {
        // When running this test, this will be the ./uniffi_bindgen directory.
        let this_crate_root = Utf8PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());

        let example_crate_root = this_crate_root
            .parent()
            .expect("should have a parent directory")
            .join("examples/arithmetic");
        assert_eq!(
            guess_crate_root(&example_crate_root.join("src/arthmetic.udl")).unwrap(),
            example_crate_root
        );

        let not_a_crate_root = &this_crate_root.join("src/templates");
        assert!(guess_crate_root(&not_a_crate_root.join("src/example.udl")).is_err());
    }
}
