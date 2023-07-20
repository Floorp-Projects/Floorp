/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::{bail, Context, Result};
use camino::Utf8Path;
use std::env;
use std::ffi::OsString;
use std::process::{Command, Stdio};
use uniffi_testing::UniFFITestHelper;

/// Run Ruby tests for a UniFFI test fixture
pub fn run_test(tmp_dir: &str, fixture_name: &str, script_file: &str) -> Result<()> {
    let script_path = Utf8Path::new(".").join(script_file).canonicalize_utf8()?;
    let test_helper = UniFFITestHelper::new(fixture_name).context("UniFFITestHelper::new")?;
    let out_dir = test_helper
        .create_out_dir(tmp_dir, &script_path)
        .context("create_out_dir")?;
    test_helper
        .copy_cdylibs_to_out_dir(&out_dir)
        .context("copy_cdylibs_to_out_dir")?;
    generate_sources(&test_helper.cdylib_path()?, &out_dir, &test_helper)
        .context("generate_sources")?;

    let rubypath = env::var_os("RUBYLIB").unwrap_or_else(|| OsString::from(""));
    let rubypath = env::join_paths(
        env::split_paths(&rubypath).chain(vec![out_dir.to_path_buf().into_std_path_buf()]),
    )?;

    let status = Command::new("ruby")
        .current_dir(out_dir)
        .env("RUBYLIB", rubypath)
        .arg(script_path)
        .stderr(Stdio::inherit())
        .stdout(Stdio::inherit())
        .spawn()
        .context("Failed to spawn `ruby` when running script")?
        .wait()
        .context("Failed to wait for `ruby` when running script")?;
    if !status.success() {
        bail!("running `ruby` failed");
    }
    Ok(())
}

fn generate_sources(
    library_path: &Utf8Path,
    out_dir: &Utf8Path,
    test_helper: &UniFFITestHelper,
) -> Result<()> {
    for source in test_helper.get_compile_sources()? {
        crate::generate_bindings(
            &source.udl_path,
            source.config_path.as_deref(),
            vec!["ruby"],
            Some(out_dir),
            Some(library_path),
            false,
        )?;
    }
    Ok(())
}
