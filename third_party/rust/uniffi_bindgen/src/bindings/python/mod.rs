/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{io::Write, process::Command};

use anyhow::Result;
use camino::Utf8Path;
use fs_err::File;

pub mod gen_python;
mod test;

use super::super::interface::ComponentInterface;
pub use gen_python::{generate_python_bindings, Config};
pub use test::run_test;

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
