/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    bindings::{RunScriptOptions, TargetLanguage},
    library_mode::generate_bindings,
};
use anyhow::{bail, Context, Result};
use camino::{Utf8Path, Utf8PathBuf};
use std::env;
use std::process::Command;
use uniffi_testing::UniFFITestHelper;

/// Run Kotlin tests for a UniFFI test fixture
pub fn run_test(tmp_dir: &str, fixture_name: &str, script_file: &str) -> Result<()> {
    run_script(
        tmp_dir,
        fixture_name,
        script_file,
        vec![],
        &RunScriptOptions::default(),
    )
}

/// Run a Kotlin script
///
/// This function will set things up so that the script can import the UniFFI bindings for a crate
pub fn run_script(
    tmp_dir: &str,
    crate_name: &str,
    script_file: &str,
    args: Vec<String>,
    options: &RunScriptOptions,
) -> Result<()> {
    let script_path = Utf8Path::new(".").join(script_file);
    let test_helper = UniFFITestHelper::new(crate_name)?;
    let out_dir = test_helper.create_out_dir(tmp_dir, &script_path)?;
    let cdylib_path = test_helper.copy_cdylib_to_out_dir(&out_dir)?;
    generate_bindings(
        &cdylib_path,
        None,
        &[TargetLanguage::Kotlin],
        &out_dir,
        false,
    )?;
    let jar_file = build_jar(crate_name, &out_dir, options)?;

    let mut command = kotlinc_command(options);
    command
        .arg("-classpath")
        .arg(calc_classpath(vec![&out_dir, &jar_file]))
        // Enable runtime assertions, for easy testing etc.
        .arg("-J-ea")
        // Our test scripts should not produce any warnings.
        .arg("-Werror")
        .arg("-script")
        .arg(script_path)
        .args(if args.is_empty() {
            vec![]
        } else {
            std::iter::once(String::from("--")).chain(args).collect()
        });

    let status = command
        .spawn()
        .context("Failed to spawn `kotlinc` to run Kotlin script")?
        .wait()
        .context("Failed to wait for `kotlinc` when running Kotlin script")?;
    if !status.success() {
        anyhow::bail!("running `kotlinc` failed")
    }
    Ok(())
}

/// Generate kotlin bindings for the given namespace, then use the kotlin
/// command-line tools to compile them into a .jar file.
fn build_jar(
    crate_name: &str,
    out_dir: &Utf8Path,
    options: &RunScriptOptions,
) -> Result<Utf8PathBuf> {
    let mut jar_file = Utf8PathBuf::from(out_dir);
    jar_file.push(format!("{crate_name}.jar"));
    let sources = glob::glob(out_dir.join("**/*.kt").as_str())?
        .flatten()
        .map(|p| String::from(p.to_string_lossy()))
        .collect::<Vec<String>>();
    if sources.is_empty() {
        bail!("No kotlin sources found in {out_dir}")
    }

    let mut command = kotlinc_command(options);
    command
        // Our generated bindings should not produce any warnings; fail tests if they do.
        .arg("-Werror")
        .arg("-d")
        .arg(&jar_file)
        .arg("-classpath")
        .arg(calc_classpath(vec![]))
        .args(sources);

    let status = command
        .spawn()
        .context("Failed to spawn `kotlinc` to compile the bindings")?
        .wait()
        .context("Failed to wait for `kotlinc` when compiling the bindings")?;
    if !status.success() {
        bail!("running `kotlinc` failed")
    }
    Ok(jar_file)
}

fn kotlinc_command(options: &RunScriptOptions) -> Command {
    let mut command = Command::new("kotlinc");
    if !options.show_compiler_messages {
        command.arg("-nowarn");
    }
    command
}

fn calc_classpath(extra_paths: Vec<&Utf8Path>) -> String {
    extra_paths
        .into_iter()
        .map(|p| p.to_string())
        // Add the system classpath as a component, using the fact that env::var returns an Option,
        // which implement Iterator
        .chain(env::var("CLASSPATH"))
        .collect::<Vec<String>>()
        .join(":")
}
