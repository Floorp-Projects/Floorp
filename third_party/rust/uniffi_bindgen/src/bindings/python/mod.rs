/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{env, io::Write, process::Command};

use anyhow::{bail, Context, Result};
use fs_err::File;

pub mod gen_python;
use camino::Utf8Path;
pub use gen_python::{generate_python_bindings, Config};

use super::super::interface::ComponentInterface;

// Generate python bindings for the given ComponentInterface, in the given output directory.
pub fn write_bindings(
    config: &Config,
    ci: &ComponentInterface,
    out_dir: &Utf8Path,
    try_format_code: bool,
) -> Result<()> {
    let py_file = out_dir.join(format!("{}.py", ci.namespace()));
    let mut f = File::create(&py_file)?;
    write!(f, "{}", generate_python_bindings(config, ci)?)?;

    if try_format_code {
        if let Err(e) = Command::new("yapf").arg(&py_file).output() {
            println!(
                "Warning: Unable to auto-format {} using yapf: {:?}",
                py_file.file_name().unwrap(),
                e
            )
        }
    }

    Ok(())
}

/// Execute the specifed python script, with environment based on the generated
/// artifacts in the given output directory.
pub fn run_script(out_dir: &Utf8Path, script_file: &Utf8Path) -> Result<()> {
    let mut cmd = Command::new("python3");
    // This helps python find the generated .py wrapper for rust component.
    let pythonpath = env::var_os("PYTHONPATH").unwrap_or_default();
    let pythonpath = env::join_paths(
        env::split_paths(&pythonpath).chain(vec![out_dir.as_std_path().to_owned()]),
    )?;
    cmd.env("PYTHONPATH", pythonpath);
    // We should now be able to execute the tests successfully.
    cmd.arg(script_file);
    let status = cmd
        .spawn()
        .context("Failed to spawn `python` when running script")?
        .wait()
        .context("Failed to wait for `python` when running script")?;
    if !status.success() {
        bail!("running `python` failed")
    }
    Ok(())
}
