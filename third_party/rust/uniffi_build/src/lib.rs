/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::Result;
use std::env;

#[cfg(not(feature = "builtin-bindgen"))]
use anyhow::{bail, Context};
#[cfg(not(feature = "builtin-bindgen"))]
use std::process::Command;

/// Generate the rust "scaffolding" required to build a uniffi component.
///
/// Given the path to an UDL file, this function will call the `uniffi-bindgen`
/// command-line tool to generate the `pub extern "C"` functions and other supporting
/// code required to expose the defined interface from Rust. The expectation is that
/// this will be called from a crate's build script, and the resulting file will
/// be `include!()`ed into the build.
///
/// Given an UDL file named `example.udl`, the generated scaffolding will be written
/// into a file named `example.uniffi.rs` in the `$OUT_DIR` directory.
///
/// If the "builtin-bindgen" feature is enabled then this will take a dependency on
/// the `uniffi_bindgen` crate and call its methods directly, rather than using the
/// command-line tool. This is mostly useful for developers who are working on uniffi
/// itself and need to test out their changes to the bindings generator.
pub fn generate_scaffolding(udl_file: &str) -> Result<()> {
    println!("cargo:rerun-if-changed={}", udl_file);
    // The UNIFFI_TESTS_DISABLE_EXTENSIONS variable disables some bindings, but it is evaluated
    // at *build* time, so we need to rebuild when it changes.
    println!("cargo:rerun-if-env-changed=UNIFFI_TESTS_DISABLE_EXTENSIONS");
    // Why don't we just depend on uniffi-bindgen and call the public functions?
    // Calling the command line helps making sure that the generated swift/Kotlin/whatever
    // bindings were generated with the same version of uniffi as the Rust scaffolding code.
    let out_dir = env::var("OUT_DIR").map_err(|_| anyhow::anyhow!("$OUT_DIR missing?!"))?;
    run_uniffi_bindgen_scaffolding(&out_dir, udl_file)
}

#[cfg(not(feature = "builtin-bindgen"))]
fn run_uniffi_bindgen_scaffolding(out_dir: &str, udl_file: &str) -> Result<()> {
    let status = Command::new("uniffi-bindgen")
        .args(&["scaffolding", "--out-dir", out_dir, udl_file])
        .status()
        .context("failed to run `uniffi-bindgen` - have you installed it via `cargo install uniffi_bindgen`?")?;
    if !status.success() {
        bail!("Error while generating scaffolding code");
    }
    Ok(())
}

#[cfg(feature = "builtin-bindgen")]
fn run_uniffi_bindgen_scaffolding(out_dir: &str, udl_file: &str) -> Result<()> {
    uniffi_bindgen::generate_component_scaffolding(udl_file, None, Some(out_dir), false)
}
