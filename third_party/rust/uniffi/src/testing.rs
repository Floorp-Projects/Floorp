/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Runtime support functionality for testing generated bindings.
//!
//! This module helps you run a foreign language script as a testcase to exercise the
//! bindings generated from your rust code. You probably don't want to use it directly,
//! and should instead use the `build_foreign_language_testcases!` macro provided by
//! the `uniffi_macros` crate.

use anyhow::{bail, Context, Result};
use camino::{Utf8Path, Utf8PathBuf};
use cargo_metadata::Message;
use lazy_static::lazy_static;
use std::{
    collections::HashMap,
    process::{Command, Stdio},
    sync::Mutex,
};

// These statics are used for a bit of simple caching and concurrency control.
// They map uniffi component crate directories to data about build steps that have already
// been executed by this process.
lazy_static! {
    static ref COMPILED_COMPONENTS: Mutex<HashMap<Utf8PathBuf, Utf8PathBuf>> = Mutex::new(HashMap::new());
    // Since uniffi-bindgen does the actual generating/compiling of bindings and script files,
    // we ensure that only one call happens at once (making tests pretty much serialized sorry :/).
    static ref UNIFFI_BINDGEN: Mutex<i32> = Mutex::new(0);
}

/// Execute the given foreign-language script as part of a rust test suite.
///
/// This function takes the top-level directory of a uniffi component crate, and the path to
/// a foreign-language test file that exercises that component's bindings. It ensures that the
/// component is compiled and available for use and then executes the foreign language script,
/// returning successfully iff the script exits successfully.
pub fn run_foreign_language_testcase(
    pkg_dir: &str,
    udl_files: &[&str],
    test_file: &str,
) -> Result<()> {
    let cdylib_file = ensure_compiled_cdylib(pkg_dir)?;
    let out_dir = cdylib_file
        .parent()
        .context("Generated cdylib has no parent directory")?
        .as_str();
    let _lock = UNIFFI_BINDGEN.lock();
    run_uniffi_bindgen_test(out_dir, udl_files, test_file)?;
    Ok(())
}

/// Ensure that a uniffi component crate is compiled and ready for use.
///
/// This function takes the top-level directory of a uniffi component crate, ensures that the
/// component's cdylib is compiled and available for use in generating bindings and running
/// foreign language code.
///
/// Internally, this function does a bit of caching and concurrency management to avoid rebuilding
/// the component for multiple testcases.
pub fn ensure_compiled_cdylib(pkg_dir: &str) -> Result<Utf8PathBuf> {
    let pkg_dir = Utf8Path::new(pkg_dir);

    // Have we already compiled this component?
    let mut compiled_components = COMPILED_COMPONENTS.lock().unwrap();
    if let Some(cdylib_file) = compiled_components.get(pkg_dir) {
        return Ok(cdylib_file.to_owned());
    }
    // Nope, looks like we'll have to compile it afresh.
    let mut cmd = Command::new("cargo");
    cmd.arg("build").arg("--message-format=json").arg("--lib");
    cmd.current_dir(pkg_dir);
    cmd.stdout(Stdio::piped());
    let mut child = cmd.spawn()?;
    let output = std::io::BufReader::new(child.stdout.take().unwrap());
    // Build the crate, looking for any cdylibs that it might produce.
    let cdylibs = Message::parse_stream(output)
        .filter_map(|message| match message {
            Err(e) => Some(Err(e.into())),
            Ok(Message::CompilerArtifact(artifact)) => {
                if artifact.target.kind.iter().any(|item| item == "cdylib") {
                    Some(Ok(artifact))
                } else {
                    None
                }
            }
            _ => None,
        })
        .collect::<Result<Vec<_>>>()?;
    if !child.wait()?.success() {
        bail!("Failed to execute `cargo build`");
    }
    // If we didn't just build exactly one cdylib, we're going to use the one most likely to be produced by `pkg_dir`,
    // or we will have a bad time.
    let cdylib = match cdylibs.len() {
        0 => bail!("Crate did not produce any cdylibs, it must not be a uniffi component"),
        1 => &cdylibs[0],
        _ => {
            match cdylibs
                .iter()
                .find(|cdylib| cdylib.target.src_path.starts_with(pkg_dir))
            {
                Some(cdylib) => {
                    log::warn!(
                        "Crate produced multiple cdylibs, using the one produced by {}",
                        pkg_dir
                    );
                    cdylib
                }
                None => {
                    bail!(
                        "Crate produced multiple cdylibs, none of which is produced by {}",
                        pkg_dir
                    );
                }
            }
        }
    };
    let cdylib_files: Vec<_> = cdylib
        .filenames
        .iter()
        .filter(|nm| matches!(nm.extension(), Some(std::env::consts::DLL_EXTENSION)))
        .collect();
    if cdylib_files.len() != 1 {
        bail!("Failed to build exactly one cdylib file, it must not be a uniffi component");
    }
    let cdylib_file = cdylib_files[0];
    // Cache the result for subsequent tests.
    compiled_components.insert(pkg_dir.to_owned(), cdylib_file.to_owned());
    Ok(cdylib_file.to_owned())
}

/// Execute the `uniffi-bindgen test` command.
///
/// The default behaviour, suitable for most consumers, is to shell out to the `uniffi-bindgen`
/// command found on the system.
///
/// If the "builtin-bindgen" feature is enabled then this will instead take a direct dependency
/// on the `uniffi_bindgen` crate and execute its methods in-process. This is useful for folks
/// who are working on uniffi itself and want to test out their changes to the bindings generator.
#[cfg(not(feature = "builtin-bindgen"))]
fn run_uniffi_bindgen_test(out_dir: &str, udl_files: &[&str], test_file: &str) -> Result<()> {
    let udl_files = udl_files.join("\n");
    let status = Command::new("uniffi-bindgen")
        .args(&["test", out_dir, &udl_files, test_file])
        .status()?;
    if !status.success() {
        bail!("Error while running tests: {}", status);
    }
    Ok(())
}

#[cfg(feature = "builtin-bindgen")]
fn run_uniffi_bindgen_test(out_dir: &str, udl_files: &[&str], test_file: &str) -> Result<()> {
    uniffi_bindgen::run_tests(out_dir, udl_files, &[test_file], None)
}
