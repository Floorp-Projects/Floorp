/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::{bail, Context, Result};
use camino::{Utf8Path, Utf8PathBuf};
use fs_err::{self as fs, File};
use std::{env, ffi::OsString, io::Write, process::Command};

pub mod gen_kotlin;
pub use gen_kotlin::{generate_bindings, Config};

use super::super::interface::ComponentInterface;

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

/// Generate kotlin bindings for the given namespace, then use the kotlin
/// command-line tools to compile them into a .jar file.
pub fn compile_bindings(
    config: &Config,
    ci: &ComponentInterface,
    out_dir: &Utf8Path,
) -> Result<()> {
    let mut kt_file = full_bindings_path(config, out_dir);
    kt_file.push(format!("{}.kt", ci.namespace()));
    let jar_file = out_dir.join(format!("{}.jar", ci.namespace()));
    let status = Command::new("kotlinc")
        // Our generated bindings should not produce any warnings; fail tests if they do.
        .arg("-Werror")
        .arg("-classpath")
        .arg(classpath_for_testing(out_dir)?)
        .arg(&kt_file)
        .arg("-d")
        .arg(jar_file)
        .spawn()
        .context("Failed to spawn `kotlinc` to compile the bindings")?
        .wait()
        .context("Failed to wait for `kotlinc` when compiling the bindings")?;
    if !status.success() {
        bail!("running `kotlinc` failed")
    }
    Ok(())
}

/// Execute the specifed kotlin script, with classpath based on the generated
// artifacts in the given output directory.
pub fn run_script(out_dir: &Utf8Path, script_file: &Utf8Path) -> Result<()> {
    let mut cmd = Command::new("kotlinc");
    // Make sure it can load the .jar and its dependencies.
    cmd.arg("-classpath").arg(classpath_for_testing(out_dir)?);
    // Enable runtime assertions, for easy testing etc.
    cmd.arg("-J-ea");
    // Our test scripts should not produce any warnings.
    cmd.arg("-Werror");
    cmd.arg("-script").arg(script_file);
    let status = cmd
        .spawn()
        .context("Failed to spawn `kotlinc` to run Kotlin script")?
        .wait()
        .context("Failed to wait for `kotlinc` when running Kotlin script")?;
    if !status.success() {
        bail!("running `kotlinc` failed")
    }
    Ok(())
}

// Calculate the classpath string to use for testing
pub fn classpath_for_testing(out_dir: &Utf8Path) -> Result<OsString> {
    let mut classpath = env::var_os("CLASSPATH").unwrap_or_default();
    // This lets java find the compiled library for the rust component.
    classpath.push(":");
    classpath.push(out_dir);
    // This lets java use any generated .jar files from the output directory.
    //
    // Including all .jar files is needed for tests like ext-types that use multiple UDL files.
    // TODO: Instead of including all .jar files, we should only include jar files that we
    // previously built for this test.
    for entry in out_dir
        .read_dir()
        .context("Failed to list target directory when running Kotlin script")?
    {
        let entry = entry.context("Directory listing failed while running Kotlin script")?;
        if let Some(ext) = entry.path().extension() {
            if ext == "jar" {
                classpath.push(":");
                classpath.push(entry.path());
            }
        }
    }
    Ok(classpath)
}
