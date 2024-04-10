/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::bindings::TargetLanguage;
use crate::{bindings::RunScriptOptions, library_mode::generate_bindings, BindingGeneratorDefault};
use anyhow::{Context, Result};
use camino::Utf8Path;
use std::env;
use std::ffi::OsString;
use std::process::Command;
use uniffi_testing::UniFFITestHelper;

/// Run Python tests for a UniFFI test fixture
pub fn run_test(tmp_dir: &str, fixture_name: &str, script_file: &str) -> Result<()> {
    run_script(
        tmp_dir,
        fixture_name,
        script_file,
        vec![],
        &RunScriptOptions::default(),
    )
}

/// Run a Python script
///
/// This function will set things up so that the script can import the UniFFI bindings for a crate
pub fn run_script(
    tmp_dir: &str,
    crate_name: &str,
    script_file: &str,
    args: Vec<String>,
    _options: &RunScriptOptions,
) -> Result<()> {
    let script_path = Utf8Path::new(script_file).canonicalize_utf8()?;
    let test_helper = UniFFITestHelper::new(crate_name)?;
    let out_dir = test_helper.create_out_dir(tmp_dir, &script_path)?;
    let cdylib_path = test_helper.copy_cdylib_to_out_dir(&out_dir)?;
    generate_bindings(
        &cdylib_path,
        None,
        &BindingGeneratorDefault {
            target_languages: vec![TargetLanguage::Python],
            try_format_code: false,
        },
        None,
        &out_dir,
        false,
    )?;

    let pythonpath = env::var_os("PYTHONPATH").unwrap_or_else(|| OsString::from(""));
    let pythonpath = env::join_paths(
        env::split_paths(&pythonpath).chain(vec![out_dir.to_path_buf().into_std_path_buf()]),
    )?;

    let mut command = Command::new("python3");
    command
        .current_dir(out_dir)
        .env("PYTHONPATH", pythonpath)
        .arg(script_path)
        .args(args);
    let status = command
        .spawn()
        .context("Failed to spawn `python3` when running script")?
        .wait()
        .context("Failed to wait for `python3` when running script")?;
    if !status.success() {
        anyhow::bail!("running `python3` failed");
    }
    Ok(())
}
