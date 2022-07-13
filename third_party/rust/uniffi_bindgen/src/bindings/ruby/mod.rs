/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{
    env,
    ffi::OsString,
    fs::File,
    io::Write,
    path::{Path, PathBuf},
    process::Command,
};

use anyhow::{bail, Context, Result};

pub mod gen_ruby;
pub use gen_ruby::{Config, RubyWrapper};

use super::super::interface::ComponentInterface;

// Generate ruby bindings for the given ComponentInterface, in the given output directory.

pub fn write_bindings(
    config: &Config,
    ci: &ComponentInterface,
    out_dir: &Path,
    try_format_code: bool,
) -> Result<()> {
    let mut rb_file = PathBuf::from(out_dir);
    rb_file.push(format!("{}.rb", ci.namespace()));
    let mut f = File::create(&rb_file).context("Failed to create .rb file for bindings")?;
    write!(f, "{}", generate_ruby_bindings(config, ci)?)?;

    if try_format_code {
        if let Err(e) = Command::new("rubocop")
            .arg("-A")
            .arg(rb_file.to_str().unwrap())
            .output()
        {
            println!(
                "Warning: Unable to auto-format {} using rubocop: {:?}",
                rb_file.file_name().unwrap().to_str().unwrap(),
                e
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
        .map_err(|_| anyhow::anyhow!("failed to render ruby bindings"))
}

/// Execute the specifed ruby script, with environment based on the generated
/// artifacts in the given output directory.
pub fn run_script(out_dir: &Path, script_file: &Path) -> Result<()> {
    let mut cmd = Command::new("ruby");
    // This helps ruby find the generated .rb wrapper for rust component.
    let rubypath = env::var_os("RUBYLIB").unwrap_or_else(|| OsString::from(""));
    let rubypath = env::join_paths(env::split_paths(&rubypath).chain(vec![out_dir.to_path_buf()]))?;

    cmd.env("RUBYLIB", rubypath);
    // We should now be able to execute the tests successfully.
    cmd.arg(script_file);
    let status = cmd
        .spawn()
        .context("Failed to spawn `ruby` when running script")?
        .wait()
        .context("Failed to wait for `ruby` when running script")?;
    if !status.success() {
        bail!("running `ruby` failed")
    }
    Ok(())
}
