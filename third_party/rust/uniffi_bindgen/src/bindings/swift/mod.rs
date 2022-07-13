/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Swift bindings backend for UniFFI
//!
//! This module generates Swift bindings from a [`ComponentInterface`] definition,
//! using Swift's builtin support for loading C header files.
//!
//! Conceptually, the generated bindings are split into two Swift modules, one for the low-level
//! C FFI layer and one for the higher-level Swift bindings. For a UniFFI component named "example"
//! we generate:
//!
//!   * A C header file `exampleFFI.h` declaring the low-level structs and functions for calling
//!    into Rust, along with a corresponding `exampleFFI.modulemap` to expose them to Swift.
//!
//!   * A Swift source file `example.swift` that imports the `exampleFFI` module and wraps it
//!    to provide the higher-level Swift API.
//!
//! Most of the concepts in a [`ComponentInterface`] have an obvious counterpart in Swift,
//! with the details documented in inline comments where appropriate.
//!
//! To handle lifting/lowering/serializing types across the FFI boundary, the Swift code
//! defines a `protocol ViaFfi` that is analogous to the `uniffi::ViaFfi` Rust trait.
//! Each type that can traverse the FFI conforms to the `ViaFfi` protocol, which specifies:
//!
//!  * The corresponding low-level type.
//!  * How to lift from and lower into into that type.
//!  * How to read from and write into a byte buffer.
//!

use std::{ffi::OsString, io::Write, process::Command};

use anyhow::{bail, Context, Result};
use camino::Utf8Path;
use fs_err::File;

pub mod gen_swift;
pub use gen_swift::{generate_bindings, Config};

use super::super::interface::ComponentInterface;

/// The Swift bindings generated from a [`ComponentInterface`].
///
pub struct Bindings {
    /// The contents of the generated `.swift` file, as a string.
    library: String,
    /// The contents of the generated `.h` file, as a string.
    header: String,
    /// The contents of the generated `.modulemap` file, as a string.
    modulemap: Option<String>,
}

/// Write UniFFI component bindings for Swift as files on disk.
///
/// Unlike other target languages, binding to Rust code from Swift involves more than just
/// generating a `.swift` file. We also need to produce a `.h` file with the C-level API
/// declarations, and a `.modulemap` file to tell Swift how to use it.
pub fn write_bindings(
    config: &Config,
    ci: &ComponentInterface,
    out_dir: &Utf8Path,
    try_format_code: bool,
) -> Result<()> {
    let Bindings {
        header,
        library,
        modulemap,
    } = generate_bindings(config, ci)?;

    let source_file = out_dir.join(format!("{}.swift", config.module_name()));
    let mut l = File::create(&source_file)?;
    write!(l, "{}", library)?;

    let mut h = File::create(out_dir.join(config.header_filename()))?;
    write!(h, "{}", header)?;

    if let Some(modulemap) = modulemap {
        let mut m = File::create(out_dir.join(config.modulemap_filename()))?;
        write!(m, "{}", modulemap)?;
    }

    if try_format_code {
        if let Err(e) = Command::new("swiftformat")
            .arg(source_file.as_str())
            .output()
        {
            println!(
                "Warning: Unable to auto-format {} using swiftformat: {:?}",
                source_file.file_name().unwrap(),
                e
            )
        }
    }

    Ok(())
}

/// Compile UniFFI component bindings for Swift for use from the `swift` command-line.
///
/// This is a utility function to help with running Swift tests. While the `swift` command-line
/// tool is able to execute Swift code from source, that code can only load other Swift modules
/// if they have been pre-compiled into a `.dylib` and corresponding `.swiftmodule`. Since our
/// test scripts need to be able to import the generated bindings, we have to compile them
/// ahead of time before running the tests.
///
pub fn compile_bindings(
    config: &Config,
    ci: &ComponentInterface,
    out_dir: &Utf8Path,
) -> Result<()> {
    if !config.generate_module_map() {
        bail!("Cannot compile Swift bindings when `generate_module_map` is `false`")
    }

    let module_map_file = out_dir.join(config.modulemap_filename());
    let mut module_map_file_option = OsString::from("-fmodule-map-file=");
    module_map_file_option.push(module_map_file.as_os_str());

    let source_file = out_dir.join(format!("{}.swift", config.module_name()));
    let dylib_file = out_dir.join(format!("lib{}.dylib", config.module_name()));

    // `-emit-library -o <path>` generates a `.dylib`, so that we can use the
    // Swift module from the REPL. Otherwise, we'll get "Couldn't lookup
    // symbols" when we try to import the module.
    // See https://bugs.swift.org/browse/SR-1191.

    let status = Command::new("swiftc")
        .arg("-module-name")
        .arg(ci.namespace())
        .arg("-emit-library")
        .arg("-o")
        .arg(dylib_file)
        .arg("-emit-module")
        .arg("-emit-module-path")
        .arg(out_dir)
        .arg("-parse-as-library")
        .arg("-L")
        .arg(out_dir)
        .arg(format!("-l{}", config.cdylib_name()))
        .arg("-Xcc")
        .arg(module_map_file_option)
        .arg(source_file)
        .spawn()
        .context("Failed to spawn `swiftc` when compiling bindings")?
        .wait()
        .context("Failed to wait for `swiftc` when compiling bindings")?;
    if !status.success() {
        bail!("running `swiftc` failed")
    }
    Ok(())
}

/// Run a Swift script, allowing it to load modules from the given output directory.
///
/// This executes the given Swift script file in a way that allows it to import any other
/// Swift modules in the given output directory. The modules must have been pre-compiled
/// using the [`compile_bindings`] function.
///
pub fn run_script(out_dir: &Utf8Path, script_file: &Utf8Path) -> Result<()> {
    let mut cmd = Command::new("swift");

    // Find any module maps and/or dylibs in the target directory, and tell swift to use them.
    // Listing the directory like this is a little bit hacky - it would be nicer if we could tell
    // Swift to load only the module(s) for the component under test, but the way we're calling
    // this test function doesn't allow us to pass that name in to the call.

    cmd.arg("-I").arg(out_dir);
    for entry in out_dir
        .read_dir()
        .context("Failed to list target directory when running script")?
    {
        let entry = entry.context("Failed to list target directory when running script")?;
        if let Some(ext) = entry.path().extension() {
            if ext == "modulemap" {
                let mut option = OsString::from("-fmodule-map-file=");
                option.push(entry.path());
                cmd.arg("-Xcc");
                cmd.arg(option);
            } else if ext == "dylib" || ext == "so" {
                let mut option = OsString::from("-l");
                option.push(entry.path());
                cmd.arg(option);
            }
        }
    }
    cmd.arg(script_file);

    let status = cmd
        .spawn()
        .context("Failed to spawn `swift` when running script")?
        .wait()
        .context("Failed to wait for `swift` when running script")?;
    if !status.success() {
        bail!("running `swift` failed")
    }
    Ok(())
}
