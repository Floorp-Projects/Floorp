/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::process::Command;

use anyhow::{Context, Result};
use camino::Utf8Path;
use fs_err as fs;

pub mod gen_ruby;
mod test;
pub use gen_ruby::{Config, RubyWrapper};
pub use test::{run_test, test_script_command};

use super::super::interface::ComponentInterface;

// Generate ruby bindings for the given ComponentInterface, in the given output directory.

pub fn write_bindings(
    config: &Config,
    ci: &ComponentInterface,
    out_dir: &Utf8Path,
    try_format_code: bool,
) -> Result<()> {
    let rb_file = out_dir.join(format!("{}.rb", ci.namespace()));
    fs::write(&rb_file, generate_ruby_bindings(config, ci)?)?;

    if try_format_code {
        if let Err(e) = Command::new("rubocop").arg("-A").arg(&rb_file).output() {
            println!(
                "Warning: Unable to auto-format {} using rubocop: {e:?}",
                rb_file.file_name().unwrap(),
            )
        }
    }

    Ok(())
}

// Generate ruby bindings for the given ComponentInterface, as a string.

pub fn generate_ruby_bindings(config: &Config, ci: &ComponentInterface) -> Result<String> {
    use askama::Template;
    RubyWrapper::new(config.clone(), ci)
        .render()
        .context("failed to render ruby bindings")
}
