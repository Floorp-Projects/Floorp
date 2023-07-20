/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::Result;
use camino::{Utf8Path, Utf8PathBuf};
use fs_err::{self as fs, File};
use std::{io::Write, process::Command};

pub mod gen_kotlin;
pub use gen_kotlin::{generate_bindings, Config};
mod test;

use super::super::interface::ComponentInterface;
pub use test::run_test;

pub fn write_bindings(
    config: &Config,
    ci: &ComponentInterface,
    out_dir: &Utf8Path,
    try_format_code: bool,
) -> Result<()> {
    let mut kt_file = full_bindings_path(config, out_dir);
    fs::create_dir_all(&kt_file)?;
    kt_file.push(format!("{}.kt", ci.namespace()));
    let mut f = File::create(&kt_file)?;
    write!(f, "{}", generate_bindings(config, ci)?)?;
    if try_format_code {
        if let Err(e) = Command::new("ktlint").arg("-F").arg(&kt_file).output() {
            println!(
                "Warning: Unable to auto-format {} using ktlint: {:?}",
                kt_file.file_name().unwrap(),
                e
            )
        }
    }
    Ok(())
}

fn full_bindings_path(config: &Config, out_dir: &Utf8Path) -> Utf8PathBuf {
    let package_path: Utf8PathBuf = config.package_name().split('.').collect();
    Utf8PathBuf::from(out_dir).join(package_path)
}
