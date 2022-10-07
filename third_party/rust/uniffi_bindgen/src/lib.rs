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
//! uniffi_build::generate_scaffolding("./src/example.udl")
//! ```
//!
//! This would output a rust file named `example.uniffi.rs`, ready to be
//! included into the code of your rust crate like this:
//!
//! ```text
//! include!(concat!(env!("OUT_DIR"), "/example.uniffi.rs"));
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

#![warn(rust_2018_idioms)]
#![allow(unknown_lints)]

const BINDGEN_VERSION: &str = env!("CARGO_PKG_VERSION");

use anyhow::{anyhow, bail, Context, Result};
use camino::{Utf8Path, Utf8PathBuf};
use clap::{Parser, Subcommand};
use fs_err::{self as fs, File};
use serde::{Deserialize, Serialize};
use std::io::prelude::*;
use std::io::ErrorKind;
use std::{collections::HashMap, env, process::Command, str::FromStr};

pub mod backend;
pub mod bindings;
pub mod interface;
pub mod macro_metadata;
pub mod scaffolding;

use bindings::TargetLanguage;
pub use interface::ComponentInterface;
use scaffolding::RustScaffolding;

/// A trait representing a Binding Generator Configuration
///
/// External crates that implement binding generators need to implement this trait and set it as
/// the `BindingGenerator.config` associated type.  `generate_external_bindings()` then uses it to
/// generate the config that's passed to `BindingGenerator.write_bindings()`
pub trait BindingGeneratorConfig: for<'de> Deserialize<'de> {
    /// Get the entry for this config from the `bindings` table.
    fn get_entry_from_bindings_table(bindings: &toml::Value) -> Option<toml::Value>;

    /// Get default config values from the `ComponentInterface`
    ///
    /// These will replace missing entries in the bindings-specific config
    fn get_config_defaults(ci: &ComponentInterface) -> Vec<(String, toml::Value)>;
}

fn load_bindings_config<BC: BindingGeneratorConfig>(
    ci: &ComponentInterface,
    crate_root: &Utf8Path,
    config_file_override: Option<&Utf8Path>,
) -> Result<BC> {
    // Load the config from the TOML value, falling back to an empty map if it doesn't exist
    let mut config_map: toml::value::Table =
        match load_bindings_config_toml::<BC>(crate_root, config_file_override)? {
            Some(value) => value
                .try_into()
                .context("Bindings config must be a TOML table")?,
            None => toml::map::Map::new(),
        };

    // Update it with the defaults from the component interface
    for (key, value) in BC::get_config_defaults(ci) {
        config_map.entry(key).or_insert(value);
    }

    // Leverage serde to convert toml::Value into the config type
    toml::Value::from(config_map)
        .try_into()
        .context("Generating bindings config from toml::Value")
}

/// Binding generator config with no members
#[derive(Clone, Debug, Hash, PartialEq, PartialOrd, Ord, Eq)]
pub struct EmptyBindingGeneratorConfig;

impl BindingGeneratorConfig for EmptyBindingGeneratorConfig {
    fn get_entry_from_bindings_table(_bindings: &toml::Value) -> Option<toml::Value> {
        None
    }

    fn get_config_defaults(_ci: &ComponentInterface) -> Vec<(String, toml::Value)> {
        Vec::new()
    }
}

// EmptyBindingGeneratorConfig is a unit struct, so the `derive(Deserialize)` implementation
// expects a null value rather than the empty map that we pass it.  So we need to implement
// `Deserialize` ourselves.
impl<'de> Deserialize<'de> for EmptyBindingGeneratorConfig {
    fn deserialize<D>(_deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        Ok(EmptyBindingGeneratorConfig)
    }
}

// Load the binding-specific config
//
// This function calulates the location of the config TOML file, parses it, and returns the result
// as a toml::Value
//
// If there is an error parsing the file then Err will be returned. If the file is missing or the
// entry for the bindings is missing, then Ok(None) will be returned.
fn load_bindings_config_toml<BC: BindingGeneratorConfig>(
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
    let full_config = toml::Value::from_str(&contents)
        .with_context(|| format!("Failed to parse config file {config_path}"))?;

    Ok(full_config
        .get("bindings")
        .and_then(BC::get_entry_from_bindings_table))
}

/// A trait representing a UniFFI Binding Generator
///
/// External crates that implement binding generators, should implement this type
/// and call the [`generate_external_bindings`] using a type that implements this trait.
pub trait BindingGenerator: Sized {
    /// Associated type representing a the bindings-specifig configuration parsed from the
    /// uniffi.toml
    type Config: BindingGeneratorConfig;

    /// Writes the bindings to the output directory
    ///
    /// # Arguments
    /// - `ci`: A [`ComponentInterface`] representing the interface
    /// - `config`: A instance of the BindingGeneratorConfig associated with this type
    /// - `out_dir`: The path to where the binding generator should write the output bindings
    fn write_bindings(
        &self,
        ci: ComponentInterface,
        config: Self::Config,
        out_dir: &Utf8Path,
    ) -> anyhow::Result<()>;
}

/// Generate bindings for an external binding generator
/// Ideally, this should replace the [`generate_bindings`] function below
///
/// Implements an entry point for external binding generators.
/// The function does the following:
/// - It parses the `udl` in a [`ComponentInterface`]
/// - Parses the `uniffi.toml` and loads it into the type that implements [`BindingGeneratorConfig`]
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
    config_file_override: Option<&Utf8Path>,
    out_dir_override: Option<&Utf8Path>,
    format_code: bool,
) -> Result<()> {
    let component = parse_udl(udl_file)?;
    let _config = get_config(
        &component,
        guess_crate_root(udl_file)?,
        config_file_override,
    );
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
    target_languages: Vec<&str>,
    out_dir_override: Option<&Utf8Path>,
    library_file: Option<&Utf8Path>,
    try_format_code: bool,
) -> Result<()> {
    let mut component = parse_udl(udl_file)?;
    if let Some(library_file) = library_file {
        macro_metadata::add_to_ci_from_library(&mut component, library_file)?;
    }
    let crate_root = &guess_crate_root(udl_file)?;

    let config = get_config(&component, crate_root, config_file_override)?;
    let out_dir = get_out_dir(udl_file, out_dir_override)?;
    for language in target_languages {
        bindings::write_bindings(
            &config.bindings,
            &component,
            &out_dir,
            language.try_into()?,
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

// Run tests against the foreign language bindings (generated and compiled at the same time).
// Note that the cdylib we're testing against must be built already.
pub fn run_tests(
    library_file: impl AsRef<Utf8Path>,
    udl_files: &[impl AsRef<Utf8Path>],
    test_scripts: &[impl AsRef<Utf8Path>],
    config_file_override: Option<&Utf8Path>,
) -> Result<()> {
    // XXX - this is just for tests, so one config_file_override for all .udl files doesn't really
    // make sense, so we don't let tests do this.
    // "Real" apps will build the .udl files one at a file and can therefore do whatever they want
    // with overrides, so don't have this problem.
    assert!(udl_files.len() == 1 || config_file_override.is_none());

    let library_file = library_file.as_ref();
    let cdylib_dir = library_file
        .parent()
        .context("Generated cdylib has no parent directory")?;

    let metadata_items = macro_metadata::extract_from_library(library_file)?;

    // Group the test scripts by language first.
    let mut language_tests: HashMap<TargetLanguage, Vec<_>> = HashMap::new();

    for test_script in test_scripts {
        let test_script = test_script.as_ref();
        let lang: TargetLanguage = test_script
            .extension()
            .context("File has no extension!")?
            .try_into()?;
        language_tests
            .entry(lang)
            .or_default()
            .push(test_script.to_owned());
    }

    for (lang, test_scripts) in language_tests {
        for udl_file in udl_files {
            let udl_file = udl_file.as_ref();
            let crate_root = guess_crate_root(udl_file)?;
            let mut component = parse_udl(udl_file)?;
            macro_metadata::add_to_ci(&mut component, metadata_items.clone())?;

            let config = get_config(&component, crate_root, config_file_override)?;
            bindings::write_bindings(&config.bindings, &component, cdylib_dir, lang, true)?;
            bindings::compile_bindings(&config.bindings, &component, cdylib_dir, lang)?;
        }

        for test_script in test_scripts {
            bindings::run_script(cdylib_dir, &test_script, lang)?;
        }
    }
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

fn get_config(
    component: &ComponentInterface,
    crate_root: &Utf8Path,
    config_file_override: Option<&Utf8Path>,
) -> Result<Config> {
    let default_config: Config = component.into();

    let config_file = match config_file_override {
        Some(cfg) => Some(cfg.to_owned()),
        None => crate_root.join("uniffi.toml").canonicalize_utf8().ok(),
    };

    match config_file {
        Some(path) => {
            let contents = fs::read_to_string(&path)
                .with_context(|| format!("Failed to read config file from {path}"))?;
            let loaded_config: Config = toml::de::from_str(&contents)
                .with_context(|| format!("Failed to generate config from file {path}"))?;
            Ok(loaded_config.merge_with(&default_config))
        }
        None => Ok(default_config),
    }
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
struct Config {
    #[serde(default)]
    bindings: bindings::Config,
}

impl From<&ComponentInterface> for Config {
    fn from(ci: &ComponentInterface) -> Self {
        Config {
            bindings: ci.into(),
        }
    }
}

pub trait MergeWith {
    fn merge_with(&self, other: &Self) -> Self;
}

impl MergeWith for Config {
    fn merge_with(&self, other: &Self) -> Self {
        Config {
            bindings: self.bindings.merge_with(&other.bindings),
        }
    }
}

impl<T: Clone> MergeWith for Option<T> {
    fn merge_with(&self, other: &Self) -> Self {
        match (self, other) {
            (Some(_), _) => self.clone(),
            (None, Some(_)) => other.clone(),
            (None, None) => None,
        }
    }
}

impl<V: Clone> MergeWith for HashMap<String, V> {
    fn merge_with(&self, other: &Self) -> Self {
        let mut merged = HashMap::new();
        // Iterate through other first so our keys override theirs
        for (key, value) in other.iter().chain(self) {
            merged.insert(key.clone(), value.clone());
        }
        merged
    }
}

// structs to help our cmdline parsing. Note that docstrings below form part
// of the "help" output.
/// Scaffolding and bindings generator for Rust
#[derive(Parser)]
#[clap(name = "uniffi-bindgen")]
#[clap(version = clap::crate_version!())]
#[clap(propagate_version = true)]
struct Cli {
    #[clap(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Generate foreign language bindings
    Generate {
        /// Foreign language(s) for which to build bindings.
        #[clap(long, short, possible_values = &["kotlin", "python", "swift", "ruby"])]
        language: Vec<String>,

        /// Directory in which to write generated files. Default is same folder as .udl file.
        #[clap(long, short)]
        out_dir: Option<Utf8PathBuf>,

        /// Do not try to format the generated bindings.
        #[clap(long, short)]
        no_format: bool,

        /// Path to the optional uniffi config file. If not provided, uniffi-bindgen will try to guess it from the UDL's file location.
        #[clap(long, short)]
        config: Option<Utf8PathBuf>,

        /// Extract proc-macro metadata from a native lib (cdylib or staticlib) for this crate.
        #[clap(long)]
        lib_file: Option<Utf8PathBuf>,

        /// Path to the UDL file.
        udl_file: Utf8PathBuf,
    },

    /// Generate Rust scaffolding code
    Scaffolding {
        /// Directory in which to write generated files. Default is same folder as .udl file.
        #[clap(long, short)]
        out_dir: Option<Utf8PathBuf>,

        /// Path to the optional uniffi config file. If not provided, uniffi-bindgen will try to guess it from the UDL's file location.
        #[clap(long, short)]
        config: Option<Utf8PathBuf>,

        /// Do not try to format the generated bindings.
        #[clap(long, short)]
        no_format: bool,

        /// Path to the UDL file.
        udl_file: Utf8PathBuf,
    },

    /// Run test scripts against foreign language bindings.
    Test {
        /// Path to the library the scripts will be testing against.
        library_file: Utf8PathBuf,

        /// Path to the UDL file.
        udl_file: Utf8PathBuf,

        /// Foreign language(s) test scripts to run.
        test_scripts: Vec<Utf8PathBuf>,

        /// Path to the optional uniffi config file. If not provided, uniffi-bindgen will try to guess it from the UDL's file location.
        #[clap(long, short)]
        config: Option<Utf8PathBuf>,
    },

    /// Print the JSON representation of the interface from a dynamic library
    PrintJson {
        /// Path to the library file (.so, .dll, .dylib, or .a)
        path: Utf8PathBuf,
    },
}

pub fn run_main() -> Result<()> {
    let cli = Cli::parse();
    match &cli.command {
        Commands::Generate {
            language,
            out_dir,
            no_format,
            config,
            lib_file,
            udl_file,
        } => crate::generate_bindings(
            udl_file,
            config.as_deref(),
            language.iter().map(String::as_str).collect(),
            out_dir.as_deref(),
            lib_file.as_deref(),
            !no_format,
        ),
        Commands::Scaffolding {
            out_dir,
            config,
            no_format,
            udl_file,
        } => crate::generate_component_scaffolding(
            udl_file,
            config.as_deref(),
            out_dir.as_deref(),
            !no_format,
        ),
        Commands::Test {
            library_file,
            udl_file,
            test_scripts,
            config,
        } => crate::run_tests(library_file, &[udl_file], test_scripts, config.as_deref()),
        Commands::PrintJson { path } => print_json(path),
    }?;
    Ok(())
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
            .join("./examples/arithmetic");
        assert_eq!(
            guess_crate_root(&example_crate_root.join("./src/arthmetic.udl")).unwrap(),
            example_crate_root
        );

        let not_a_crate_root = &this_crate_root.join("./src/templates");
        assert!(guess_crate_root(&not_a_crate_root.join("./src/example.udl")).is_err());
    }
}
