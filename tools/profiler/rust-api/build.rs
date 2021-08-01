/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Build script for the Gecko Profiler bindings.
//!
//! This file is executed by cargo when this crate is built. It generates the
//! `$OUT_DIR/bindings.rs` file which is then included by `src/gecko_bindings/mod.rs`.

#[macro_use]
extern crate lazy_static;

use bindgen::{Builder, CargoCallbacks, CodegenConfig};
use std::env;
use std::fs;
use std::path::PathBuf;

lazy_static! {
    static ref OUTDIR_PATH: PathBuf = PathBuf::from(env::var_os("OUT_DIR").unwrap()).join("gecko");
}

const BINDINGS_FILE: &str = "bindings.rs";

lazy_static! {
    static ref BINDGEN_FLAGS: Vec<String> = {
        // Load build-specific config overrides.
        let path = PathBuf::from(env::var_os("MOZ_TOPOBJDIR").unwrap())
            .join("tools/profiler/rust-api/extra-bindgen-flags");
        println!("cargo:rerun-if-changed={}", path.to_str().unwrap());
        fs::read_to_string(path).expect("Failed to read extra-bindgen-flags file")
            .split_whitespace()
            .map(std::borrow::ToOwned::to_owned)
            .collect()
    };
    static ref DISTDIR_PATH: PathBuf = {
        let path = PathBuf::from(env::var_os("MOZ_DIST").unwrap());
        if !path.is_absolute() || !path.is_dir() {
            panic!("MOZ_DIST must be an absolute directory, was: {}", path.display());
        }
        path
    };
    static ref SEARCH_PATHS: Vec<PathBuf> = vec![
        DISTDIR_PATH.join("include"),
        DISTDIR_PATH.join("include/nspr"),
    ];
}

fn search_include(name: &str) -> Option<PathBuf> {
    for path in SEARCH_PATHS.iter() {
        let file = path.join(name);
        if file.is_file() {
            return Some(file);
        }
    }
    None
}

fn add_include(name: &str) -> String {
    let file = match search_include(name) {
        Some(file) => file,
        None => panic!("Include not found: {}", name),
    };
    String::from(file.to_str().unwrap())
}

fn generate_bindings() {
    let mut builder = Builder::default()
        .enable_cxx_namespaces()
        .with_codegen_config(CodegenConfig::TYPES | CodegenConfig::VARS | CodegenConfig::FUNCTIONS)
        .size_t_is_usize(true);

    for dir in SEARCH_PATHS.iter() {
        builder = builder.clang_arg("-I").clang_arg(dir.to_str().unwrap());
    }

    builder = builder
        .clang_arg("-include")
        .clang_arg(add_include("mozilla-config.h"));

    for item in &*BINDGEN_FLAGS {
        builder = builder.clang_arg(item);
    }

    let bindings = builder
        .header(add_include("GeckoProfiler.h"))
        .header(add_include("ProfilerBindings.h"))
        .whitelist_function("gecko_profiler_.*")
        .whitelist_var("mozilla::profiler::detail::RacyFeatures::sActiveAndFeatures")
        .whitelist_type("mozilla::profiler::detail::RacyFeatures")
        .raw_line("pub use self::root::*;")
        // Tell cargo to invalidate the built crate whenever any of the
        // included header files changed.
        .parse_callbacks(Box::new(CargoCallbacks))
        // Finish the builder and generate the bindings.
        .generate()
        // Unwrap the Result and panic on failure.
        .expect("Unable to generate bindings");

    let out_file = OUTDIR_PATH.join(BINDINGS_FILE);
    bindings
        .write_to_file(out_file)
        .expect("Couldn't write bindings!");
}

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:out_dir={}", env::var("OUT_DIR").unwrap());

    fs::create_dir_all(&*OUTDIR_PATH).unwrap();
    generate_bindings();
}
