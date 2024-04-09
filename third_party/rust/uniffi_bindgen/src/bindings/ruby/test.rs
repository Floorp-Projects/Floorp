/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::bindings::TargetLanguage;
use crate::library_mode::generate_bindings;
use crate::BindingGeneratorDefault;
use anyhow::{bail, Context, Result};
use camino::Utf8Path;
use std::env;
use std::ffi::OsString;
use std::process::{Command, Stdio};
use uniffi_testing::UniFFITestHelper;

/// Run Ruby tests for a UniFFI test fixture
pub fn run_test(tmp_dir: &str, fixture_name: &str, script_file: &str) -> Result<()> {
    let status = test_script_command(tmp_dir, fixture_name, script_file)?
        .spawn()
        .context("Failed to spawn `ruby` when running script")?
        .wait()
        .context("Failed to wait for `ruby` when running script")?;
    if !status.success() {
        bail!("running `ruby` failed");
    }
    Ok(())
}

/// Create a `Command` instance that runs a test script
pub fn test_script_command(
    tmp_dir: &str,
    fixture_name: &str,
    script_file: &str,
) -> Result<Command> {
    let script_path = Utf8Path::new(script_file).canonicalize_utf8()?;
    let test_helper = UniFFITestHelper::new(fixture_name)?;
    let out_dir = test_helper.create_out_dir(tmp_dir, &script_path)?;
    let cdylib_path = test_helper.copy_cdylib_to_out_dir(&out_dir)?;
    generate_bindings(
        &cdylib_path,
        None,
        &BindingGeneratorDefault {
            target_languages: vec![TargetLanguage::Ruby],
            try_format_code: false,
        },
        None,
        &out_dir,
        false,
    )?;

    let rubypath = env::var_os("RUBYLIB").unwrap_or_else(|| OsString::from(""));
    let rubypath = env::join_paths(
        env::split_paths(&rubypath).chain(vec![out_dir.to_path_buf().into_std_path_buf()]),
    )?;

    let mut command = Command::new("ruby");
    command
        .current_dir(out_dir)
        .env("RUBYLIB", rubypath)
        .arg(script_path)
        .stderr(Stdio::inherit())
        .stdout(Stdio::inherit());
    Ok(command)
}
