/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::{bail, Context, Result};
use camino::{Utf8Path, Utf8PathBuf};
use std::env;
use std::process::Command;
use uniffi_testing::UniFFITestHelper;

/// Run Kotlin tests for a UniFFI test fixture
pub fn run_test(tmp_dir: &str, fixture_name: &str, script_file: &str) -> Result<()> {
    let script_path = Utf8Path::new(".").join(script_file);
    let test_helper = UniFFITestHelper::new(fixture_name).context("UniFFITestHelper::new")?;
    let out_dir = test_helper
        .create_out_dir(tmp_dir, &script_path)
        .context("create_out_dir")?;
    test_helper
        .copy_cdylibs_to_out_dir(&out_dir)
        .context("copy_fixture_library_to_out_dir")?;
    generate_sources(&test_helper.cdylib_path()?, &out_dir, &test_helper)
        .context("generate_sources")?;
    let jar_file = build_jar(fixture_name, &out_dir).context("build_jar")?;

    let status = Command::new("kotlinc")
        .arg("-classpath")
        .arg(calc_classpath(vec![&out_dir, &jar_file]))
        // Enable runtime assertions, for easy testing etc.
        .arg("-J-ea")
        // Our test scripts should not produce any warnings.
        .arg("-Werror")
        .arg("-script")
        .arg(script_path)
        .spawn()
        .context("Failed to spawn `kotlinc` to run Kotlin script")?
        .wait()
        .context("Failed to wait for `kotlinc` when running Kotlin script")?;
    if !status.success() {
        anyhow::bail!("running `kotlinc` failed")
    }
    Ok(())
}

fn generate_sources(
    library_path: &Utf8Path,
    out_dir: &Utf8Path,
    test_helper: &UniFFITestHelper,
) -> Result<()> {
    for source in test_helper.get_compile_sources()? {
        println!(
            "Generating bindings: {:?} {:?} {:?}",
            source.udl_path, source.config_path, out_dir
        );
        crate::generate_bindings(
            &source.udl_path,
            source.config_path.as_deref(),
            vec!["kotlin"],
            Some(out_dir),
            Some(library_path),
            false,
        )?;
    }
    Ok(())
}

/// Generate kotlin bindings for the given namespace, then use the kotlin
/// command-line tools to compile them into a .jar file.
fn build_jar(fixture_name: &str, out_dir: &Utf8Path) -> Result<Utf8PathBuf> {
    let mut jar_file = Utf8PathBuf::from(out_dir);
    jar_file.push(format!("{}.jar", fixture_name));
    let sources = glob::glob(out_dir.join("**/*.kt").as_str())?
        .flatten()
        .map(|p| String::from(p.to_string_lossy()))
        .collect::<Vec<String>>();
    if sources.is_empty() {
        bail!("No kotlin sources found in {out_dir}")
    }
    println!("building jar from: {:?}", sources);

    let status = Command::new("kotlinc")
        // Our generated bindings should not produce any warnings; fail tests if they do.
        .arg("-Werror")
        .arg("-d")
        .arg(&jar_file)
        .arg("-classpath")
        .arg(calc_classpath(vec![]))
        .args(sources)
        .spawn()
        .context("Failed to spawn `kotlinc` to compile the bindings")?
        .wait()
        .context("Failed to wait for `kotlinc` when compiling the bindings")?;
    if !status.success() {
        bail!("running `kotlinc` failed")
    }
    Ok(jar_file)
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
