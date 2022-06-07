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
use clap::{Parser, Subcommand};
use serde::{Deserialize, Serialize};
use std::convert::TryInto;
use std::io::prelude::*;
use std::{
    collections::HashMap,
    env,
    ffi::OsString,
    fs::File,
    path::{Path, PathBuf},
    process::Command,
    str::FromStr,
};

pub mod backend;
pub mod bindings;
pub mod interface;
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
    udl_file: &Path,
    config_file_override: Option<&Path>,
) -> Result<BC> {
    // Load the config from the TOML value, falling back to an empty map if it doesn't exist
    let mut config_map: toml::value::Table =
        match load_bindings_config_toml::<BC>(udl_file, config_file_override)? {
            Some(value) => value
                .try_into()
                .context("Bindings config must be a TOML table")?,
            None => toml::map::Map::new(),
        };

    // Update it with the defaults from the component interface
    for (key, value) in BC::get_config_defaults(ci).into_iter() {
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
    udl_file: &Path,
    config_file_override: Option<&Path>,
) -> Result<Option<toml::Value>> {
    let config_path = match config_file_override {
        Some(cfg) => cfg.to_owned(),
        None => guess_crate_root(udl_file)?.join("uniffi.toml"),
    };

    if !config_path.exists() {
        return Ok(None);
    }

    let contents = slurp_file(&config_path)
        .with_context(|| format!("Failed to read config file from {:?}", config_path))?;
    let full_config = toml::Value::from_str(&contents)
        .with_context(|| format!("Failed to parse config file {:?}", config_path))?;

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
        out_dir: &Path,
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
    udl_file: impl AsRef<Path>,
    config_file_override: Option<impl AsRef<Path>>,
    out_dir_override: Option<impl AsRef<Path>>,
) -> Result<()> {
    let out_dir_override = out_dir_override.as_ref().map(|p| p.as_ref());
    let config_file_override = config_file_override.as_ref().map(|p| p.as_ref());
    let out_dir = get_out_dir(udl_file.as_ref(), out_dir_override)?;
    let component = parse_udl(udl_file.as_ref()).context("Error parsing UDL")?;
    let bindings_config =
        load_bindings_config(&component, udl_file.as_ref(), config_file_override)?;
    binding_generator.write_bindings(component, bindings_config, out_dir.as_path())
}

// Generate the infrastructural Rust code for implementing the UDL interface,
// such as the `extern "C"` function definitions and record data types.
pub fn generate_component_scaffolding<P: AsRef<Path>>(
    udl_file: P,
    config_file_override: Option<P>,
    out_dir_override: Option<P>,
    format_code: bool,
) -> Result<()> {
    let config_file_override = config_file_override.as_ref().map(|p| p.as_ref());
    let out_dir_override = out_dir_override.as_ref().map(|p| p.as_ref());
    let udl_file = udl_file.as_ref();
    let component = parse_udl(udl_file)?;
    let _config = get_config(
        &component,
        guess_crate_root(udl_file)?,
        config_file_override,
    );
    let mut filename = Path::new(&udl_file)
        .file_stem()
        .ok_or_else(|| anyhow!("not a file"))?
        .to_os_string();
    filename.push(".uniffi.rs");
    let mut out_dir = get_out_dir(udl_file, out_dir_override)?;
    out_dir.push(filename);
    let mut f =
        File::create(&out_dir).map_err(|e| anyhow!("Failed to create output file: {:?}", e))?;
    write!(f, "{}", RustScaffolding::new(&component))
        .map_err(|e| anyhow!("Failed to write output file: {:?}", e))?;
    if format_code {
        Command::new("rustfmt").arg(&out_dir).status()?;
    }
    Ok(())
}

// Generate the bindings in the target languages that call the scaffolding
// Rust code.
pub fn generate_bindings<P: AsRef<Path>>(
    udl_file: P,
    config_file_override: Option<P>,
    target_languages: Vec<&str>,
    out_dir_override: Option<P>,
    try_format_code: bool,
) -> Result<()> {
    let out_dir_override = out_dir_override.as_ref().map(|p| p.as_ref());
    let config_file_override = config_file_override.as_ref().map(|p| p.as_ref());
    let udl_file = udl_file.as_ref();

    let component = parse_udl(udl_file)?;
    let config = get_config(
        &component,
        guess_crate_root(udl_file)?,
        config_file_override,
    )?;
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

// Run tests against the foreign language bindings (generated and compiled at the same time).
// Note that the cdylib we're testing against must be built already.
pub fn run_tests<P: AsRef<Path>>(
    cdylib_dir: P,
    udl_files: &[&str],
    test_scripts: Vec<&str>,
    config_file_override: Option<P>,
) -> Result<()> {
    // XXX - this is just for tests, so one config_file_override for all .udl files doesn't really
    // make sense, so we don't let tests do this.
    // "Real" apps will build the .udl files one at a file and can therefore do whatever they want
    // with overrides, so don't have this problem.
    assert!(udl_files.len() == 1 || config_file_override.is_none());

    let cdylib_dir = cdylib_dir.as_ref();
    let config_file_override = config_file_override.as_ref().map(|p| p.as_ref());

    // Group the test scripts by language first.
    let mut language_tests: HashMap<TargetLanguage, Vec<String>> = HashMap::new();

    for test_script in test_scripts {
        let lang: TargetLanguage = PathBuf::from(test_script)
            .extension()
            .ok_or_else(|| anyhow!("File has no extension!"))?
            .try_into()?;
        language_tests
            .entry(lang)
            .or_default()
            .push(test_script.to_owned());
    }

    for (lang, test_scripts) in language_tests {
        for udl_file in udl_files {
            let crate_root = guess_crate_root(Path::new(udl_file))?;
            let component = parse_udl(Path::new(udl_file))?;
            let config = get_config(&component, crate_root, config_file_override)?;
            bindings::write_bindings(&config.bindings, &component, &cdylib_dir, lang, true)?;
            bindings::compile_bindings(&config.bindings, &component, &cdylib_dir, lang)?;
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
fn guess_crate_root(udl_file: &Path) -> Result<&Path> {
    let path_guess = udl_file
        .parent()
        .ok_or_else(|| anyhow!("UDL file has no parent folder!"))?
        .parent()
        .ok_or_else(|| anyhow!("UDL file has no grand-parent folder!"))?;
    if !path_guess.join("Cargo.toml").is_file() {
        bail!("UDL file does not appear to be inside a crate")
    }
    Ok(path_guess)
}

fn get_config(
    component: &ComponentInterface,
    crate_root: &Path,
    config_file_override: Option<&Path>,
) -> Result<Config> {
    let default_config: Config = component.into();

    let config_file: Option<PathBuf> = match config_file_override {
        Some(cfg) => Some(PathBuf::from(cfg)),
        None => crate_root.join("uniffi.toml").canonicalize().ok(),
    };

    match config_file {
        Some(path) => {
            let contents = slurp_file(&path)
                .with_context(|| format!("Failed to read config file from {:?}", &path))?;
            let loaded_config: Config = toml::de::from_str(&contents)
                .with_context(|| format!("Failed to generate config from file {:?}", &path))?;
            Ok(loaded_config.merge_with(&default_config))
        }
        None => Ok(default_config),
    }
}

fn get_out_dir(udl_file: &Path, out_dir_override: Option<&Path>) -> Result<PathBuf> {
    Ok(match out_dir_override {
        Some(s) => {
            // Create the directory if it doesn't exist yet.
            std::fs::create_dir_all(&s)?;
            s.canonicalize()
                .map_err(|e| anyhow!("Unable to find out-dir: {:?}", e))?
        }
        None => udl_file
            .parent()
            .ok_or_else(|| anyhow!("File has no parent directory"))?
            .to_owned(),
    })
}

fn parse_udl(udl_file: &Path) -> Result<ComponentInterface> {
    let udl =
        slurp_file(udl_file).map_err(|_| anyhow!("Failed to read UDL from {:?}", &udl_file))?;
    udl.parse::<interface::ComponentInterface>()
        .map_err(|e| anyhow!("Failed to parse UDL: {}", e))
}

fn slurp_file(file_name: &Path) -> Result<String> {
    let mut contents = String::new();
    let mut f = File::open(file_name)?;
    f.read_to_string(&mut contents)?;
    Ok(contents)
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

// structs to help our cmdline parsing.
#[derive(Parser)]
#[clap(name = "uniffi-bindgen")]
#[clap(version = clap::crate_version!())]
#[clap(about = "Scaffolding and bindings generator for Rust")]
#[clap(propagate_version = true)]
struct Cli {
    #[clap(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    #[clap(name = "generate", about = "Generate foreign language bindings")]
    Generate {
        #[clap(long, short, possible_values = &["kotlin", "python", "swift", "ruby"])]
        #[clap(help = "Foreign language(s) for which to build bindings.")]
        language: Vec<String>,

        #[clap(
            long,
            short,
            help = "Directory in which to write generated files. Default is same folder as .udl file."
        )]
        out_dir: Option<OsString>,

        #[clap(long, short, help = "Do not try to format the generated bindings.")]
        no_format: bool,

        #[clap(
            long,
            short,
            help = "Path to the optional uniffi config file. If not provided, uniffi-bindgen will try to guess it from the UDL's file location."
        )]
        config: Option<OsString>,

        #[clap(help = "Path to the UDL file.")]
        udl_file: OsString,
    },

    #[clap(name = "scaffolding", about = "Generate Rust scaffolding code")]
    Scaffolding {
        #[clap(
            long,
            short,
            help = "Directory in which to write generated files. Default is same folder as .udl file."
        )]
        out_dir: Option<OsString>,

        #[clap(
            long,
            short,
            help = "Path to the optional uniffi config file. If not provided, uniffi-bindgen will try to guess it from the UDL's file location."
        )]
        config: Option<OsString>,

        #[clap(long, short, help = "Do not try to format the generated bindings.")]
        no_format: bool,

        #[clap(help = "Path to the UDL file.")]
        udl_file: OsString,
    },

    #[clap(
        name = "test",
        about = "Run test scripts against foreign language bindings."
    )]
    Test {
        #[clap(
            help = "Path to the directory containing the cdylib the scripts will be testing against."
        )]
        cdylib_dir: OsString,

        #[clap(help = "Path to the UDL file.")]
        udl_file: OsString,

        #[clap(help = "Foreign language(s) test scripts to run.")]
        test_scripts: Vec<String>,

        #[clap(
            long,
            short,
            help = "Path to the optional uniffi config file. If not provided, uniffi-bindgen will try to guess it from the UDL's file location."
        )]
        config: Option<OsString>,
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
            udl_file,
        } => crate::generate_bindings(
            udl_file,
            config.as_ref(),
            language.iter().map(String::as_str).collect(),
            out_dir.as_ref(),
            !no_format,
        ),
        Commands::Scaffolding {
            out_dir,
            config,
            no_format,
            udl_file,
        } => crate::generate_component_scaffolding(
            udl_file,
            config.as_ref(),
            out_dir.as_ref(),
            !no_format,
        ),
        Commands::Test {
            cdylib_dir,
            udl_file,
            test_scripts,
            config,
        } => crate::run_tests(
            cdylib_dir,
            &[&udl_file.to_string_lossy()], // XXX - kinda defeats the purpose of OsString?
            test_scripts.iter().map(String::as_str).collect(),
            config.as_ref(),
        ),
    }?;
    Ok(())
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_guessing_of_crate_root_directory_from_udl_file() {
        // When running this test, this will be the ./uniffi_bindgen directory.
        let this_crate_root = PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());

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
