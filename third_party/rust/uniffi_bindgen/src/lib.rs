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
//! Add to your crate `uniffi_build` under `[build-dependencies]`,
//! then add a `build.rs` script to your crate and have it call `uniffi_build::generate_scaffolding`
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
//! You will need ensure a local `uniffi-bindgen` - see <https://mozilla.github.io/uniffi-rs/tutorial/foreign_language_bindings.html>
//! This utility provides a command-line tool that can produce code to
//! consume the Rust library in any of several supported languages.
//! It is done by calling (in kotlin for example):
//!
//! ```text
//! cargo run --bin -p uniffi-bindgen --language kotlin ./src/example.udl
//! ```
//!
//! This will produce a file `example.kt` in the same directory as the .udl file, containing kotlin bindings
//! to load and use the compiled rust code via its C-compatible FFI.
//!

#![warn(rust_2018_idioms, unused_qualifications)]
#![allow(unknown_lints)]

use anyhow::{anyhow, bail, Context, Result};
use camino::{Utf8Path, Utf8PathBuf};
use fs_err::{self as fs, File};
use serde::{de::DeserializeOwned, Deserialize, Serialize};
use std::io::prelude::*;
use std::io::ErrorKind;
use std::{collections::HashMap, process::Command};

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

/// Binding generator config with no members
#[derive(Clone, Debug, Deserialize, Hash, PartialEq, PartialOrd, Ord, Eq)]
pub struct EmptyBindingsConfig;

impl BindingsConfig for EmptyBindingsConfig {
    fn update_from_ci(&mut self, _ci: &ComponentInterface) {}
    fn update_from_cdylib_name(&mut self, _cdylib_name: &str) {}
    fn update_from_dependency_configs(&mut self, _config_map: HashMap<&str, &Self>) {}
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
        ci: &ComponentInterface,
        config: &Self::Config,
        out_dir: &Utf8Path,
        try_format_code: bool,
    ) -> Result<()>;

    /// Check if `library_path` used by library mode is valid for this generator
    fn check_library_path(&self, library_path: &Utf8Path, cdylib_name: Option<&str>) -> Result<()>;
}

pub struct BindingGeneratorDefault {
    pub target_languages: Vec<TargetLanguage>,
    pub try_format_code: bool,
}

impl BindingGenerator for BindingGeneratorDefault {
    type Config = Config;

    fn write_bindings(
        &self,
        ci: &ComponentInterface,
        config: &Self::Config,
        out_dir: &Utf8Path,
        _try_format_code: bool,
    ) -> Result<()> {
        for &language in &self.target_languages {
            bindings::write_bindings(
                &config.bindings,
                ci,
                out_dir,
                language,
                self.try_format_code,
            )?;
        }
        Ok(())
    }

    fn check_library_path(&self, library_path: &Utf8Path, cdylib_name: Option<&str>) -> Result<()> {
        for &language in &self.target_languages {
            if cdylib_name.is_none() && language != TargetLanguage::Swift {
                bail!("Generate bindings for {language} requires a cdylib, but {library_path} was given");
            }
        }
        Ok(())
    }
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
/// - `library_file`: The path to a dynamic library to attempt to extract the definitions from and extend the component interface with. No extensions to component interface occur if it's [`None`]
/// - `crate_name`: Override the default crate name that is guessed from UDL file path.
pub fn generate_external_bindings<T: BindingGenerator>(
    binding_generator: &T,
    udl_file: impl AsRef<Utf8Path>,
    config_file_override: Option<impl AsRef<Utf8Path>>,
    out_dir_override: Option<impl AsRef<Utf8Path>>,
    library_file: Option<impl AsRef<Utf8Path>>,
    crate_name: Option<&str>,
    try_format_code: bool,
) -> Result<()> {
    let crate_name = crate_name
        .map(|c| Ok(c.to_string()))
        .unwrap_or_else(|| crate_name_from_cargo_toml(udl_file.as_ref()))?;
    let mut component = parse_udl(udl_file.as_ref(), &crate_name)?;
    if let Some(ref library_file) = library_file {
        macro_metadata::add_to_ci_from_library(&mut component, library_file.as_ref())?;
    }
    let crate_root = &guess_crate_root(udl_file.as_ref()).context("Failed to guess crate root")?;

    let config_file_override = config_file_override.as_ref().map(|p| p.as_ref());

    let config = {
        let mut config = load_initial_config::<T::Config>(crate_root, config_file_override)?;
        config.update_from_ci(&component);
        if let Some(ref library_file) = library_file {
            if let Some(cdylib_name) = crate::library_mode::calc_cdylib_name(library_file.as_ref())
            {
                config.update_from_cdylib_name(cdylib_name)
            }
        };
        config
    };

    let out_dir = get_out_dir(
        udl_file.as_ref(),
        out_dir_override.as_ref().map(|p| p.as_ref()),
    )?;
    binding_generator.write_bindings(&component, &config, &out_dir, try_format_code)
}

// Generate the infrastructural Rust code for implementing the UDL interface,
// such as the `extern "C"` function definitions and record data types.
// Locates and parses Cargo.toml to determine the name of the crate.
pub fn generate_component_scaffolding(
    udl_file: &Utf8Path,
    out_dir_override: Option<&Utf8Path>,
    format_code: bool,
) -> Result<()> {
    let component = parse_udl(udl_file, &crate_name_from_cargo_toml(udl_file)?)?;
    generate_component_scaffolding_inner(component, udl_file, out_dir_override, format_code)
}

// Generate the infrastructural Rust code for implementing the UDL interface,
// such as the `extern "C"` function definitions and record data types, using
// the specified crate name.
pub fn generate_component_scaffolding_for_crate(
    udl_file: &Utf8Path,
    crate_name: &str,
    out_dir_override: Option<&Utf8Path>,
    format_code: bool,
) -> Result<()> {
    let component = parse_udl(udl_file, crate_name)?;
    generate_component_scaffolding_inner(component, udl_file, out_dir_override, format_code)
}

fn generate_component_scaffolding_inner(
    component: ComponentInterface,
    udl_file: &Utf8Path,
    out_dir_override: Option<&Utf8Path>,
    format_code: bool,
) -> Result<()> {
    let file_stem = udl_file.file_stem().context("not a file")?;
    let filename = format!("{file_stem}.uniffi.rs");
    let out_path = get_out_dir(udl_file, out_dir_override)?.join(filename);
    let mut f = File::create(&out_path)?;
    write!(f, "{}", RustScaffolding::new(&component, file_stem))
        .context("Failed to write output file")?;
    if format_code {
        format_code_with_rustfmt(&out_path)?;
    }
    Ok(())
}

// Generate the bindings in the target languages that call the scaffolding
// Rust code.
pub fn generate_bindings<T: BindingGenerator>(
    udl_file: &Utf8Path,
    config_file_override: Option<&Utf8Path>,
    binding_generator: T,
    out_dir_override: Option<&Utf8Path>,
    library_file: Option<&Utf8Path>,
    crate_name: Option<&str>,
    try_format_code: bool,
) -> Result<()> {
    generate_external_bindings(
        &binding_generator,
        udl_file,
        config_file_override,
        out_dir_override,
        library_file,
        crate_name,
        try_format_code,
    )
}

pub fn print_repr(library_path: &Utf8Path) -> Result<()> {
    let metadata = macro_metadata::extract_from_library(library_path)?;
    println!("{metadata:#?}");
    Ok(())
}

// Given the path to a UDL file, locate and parse the corresponding Cargo.toml to determine
// the library crate name.
// Note that this is largely a copy of code in uniffi_macros/src/util.rs, but sharing it
// isn't trivial and it's not particularly complicated so we've just copied it.
fn crate_name_from_cargo_toml(udl_file: &Utf8Path) -> Result<String> {
    #[derive(Deserialize)]
    struct CargoToml {
        package: Package,
        #[serde(default)]
        lib: Lib,
    }

    #[derive(Deserialize)]
    struct Package {
        name: String,
    }

    #[derive(Default, Deserialize)]
    struct Lib {
        name: Option<String>,
    }

    let file = guess_crate_root(udl_file)?.join("Cargo.toml");
    let cargo_toml_bytes =
        fs::read(file).context("Can't find Cargo.toml to determine the crate name")?;

    let cargo_toml = toml::from_slice::<CargoToml>(&cargo_toml_bytes)?;

    let lib_crate_name = cargo_toml
        .lib
        .name
        .unwrap_or_else(|| cargo_toml.package.name.replace('-', "_"));

    Ok(lib_crate_name)
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

fn parse_udl(udl_file: &Utf8Path, crate_name: &str) -> Result<ComponentInterface> {
    let udl = fs::read_to_string(udl_file)
        .with_context(|| format!("Failed to read UDL from {udl_file}"))?;
    let group = uniffi_udl::parse_udl(&udl, crate_name)?;
    ComponentInterface::from_metadata(group)
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

/// Load TOML from file if the file exists.
fn load_toml_file(source: Option<&Utf8Path>) -> Result<Option<toml::value::Table>> {
    if let Some(source) = source {
        if source.exists() {
            let contents =
                fs::read_to_string(source).with_context(|| format!("read file: {:?}", source))?;
            return Ok(Some(
                toml::de::from_str(&contents)
                    .with_context(|| format!("parse toml: {:?}", source))?,
            ));
        }
    }

    Ok(None)
}

/// Load the default `uniffi.toml` config, merge TOML trees with `config_file_override` if specified.
fn load_initial_config<Config: DeserializeOwned>(
    crate_root: &Utf8Path,
    config_file_override: Option<&Utf8Path>,
) -> Result<Config> {
    let mut config = load_toml_file(Some(crate_root.join("uniffi.toml").as_path()))
        .context("default config")?
        .unwrap_or(toml::value::Table::default());

    let override_config = load_toml_file(config_file_override).context("override config")?;
    if let Some(override_config) = override_config {
        merge_toml(&mut config, override_config);
    }

    Ok(toml::Value::from(config).try_into()?)
}

fn merge_toml(a: &mut toml::value::Table, b: toml::value::Table) {
    for (key, value) in b.into_iter() {
        match a.get_mut(&key) {
            Some(existing_value) => match (existing_value, value) {
                (toml::Value::Table(ref mut t0), toml::Value::Table(t1)) => {
                    merge_toml(t0, t1);
                }
                (v, value) => *v = value,
            },
            None => {
                a.insert(key, value);
            }
        }
    }
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct Config {
    #[serde(default)]
    bindings: bindings::Config,
}

impl BindingsConfig for Config {
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

    #[test]
    fn test_merge_toml() {
        let default = r#"
            foo = "foo"
            bar = "bar"

            [table1]
            foo = "foo"
            bar = "bar"
        "#;
        let mut default = toml::de::from_str(default).unwrap();

        let override_toml = r#"
            # update key
            bar = "BAR"
            # insert new key
            baz = "BAZ"

            [table1]
            # update key
            bar = "BAR"
            # insert new key
            baz = "BAZ"

            # new table
            [table1.table2]
            bar = "BAR"
            baz = "BAZ"
        "#;
        let override_toml = toml::de::from_str(override_toml).unwrap();

        let expected = r#"
            foo = "foo"
            bar = "BAR"
            baz = "BAZ"

            [table1]
            foo = "foo"
            bar = "BAR"
            baz = "BAZ"

            [table1.table2]
            bar = "BAR"
            baz = "BAZ"
        "#;
        let expected: toml::value::Table = toml::de::from_str(expected).unwrap();

        merge_toml(&mut default, override_toml);

        assert_eq!(&expected, &default);
    }
}
