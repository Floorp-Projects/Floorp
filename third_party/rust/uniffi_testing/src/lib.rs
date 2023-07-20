/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::{bail, Result};
use camino::{Utf8Path, Utf8PathBuf};
use cargo_metadata::{Message, Metadata, MetadataCommand, Package, Target};
use fs_err as fs;
use once_cell::sync::Lazy;
use std::{
    collections::hash_map::DefaultHasher,
    env,
    env::consts::DLL_EXTENSION,
    hash::{Hash, Hasher},
    process::{Command, Stdio},
};

// A source to compile for a test
#[derive(Debug)]
pub struct CompileSource {
    pub udl_path: Utf8PathBuf,
    pub config_path: Option<Utf8PathBuf>,
}

// Store Cargo output to in a static to avoid calling it more than once.

static CARGO_METADATA: Lazy<Metadata> = Lazy::new(get_cargo_metadata);
static CARGO_BUILD_MESSAGES: Lazy<Vec<Message>> = Lazy::new(get_cargo_build_messages);

/// Struct for running fixture and example tests for bindings generators
///
/// Expectations:
///   - Used from a integration test (a `.rs` file in the tests/ directory)
///   - The working directory is the project root for the bindings crate. This is the normal case
///     for test code, just make sure you don't cd somewhere else.
///   - The bindings crate has a dev-dependency on the fixture crate
///   - The fixture crate produces a cdylib library
///   - The fixture crate, and any external-crates, has 1 UDL file in it's src/ directory
pub struct UniFFITestHelper {
    name: String,
    package: Package,
}

impl UniFFITestHelper {
    pub fn new(name: &str) -> Result<Self> {
        let package = Self::find_package(name)?;
        Ok(Self {
            name: name.to_string(),
            package,
        })
    }

    fn find_package(name: &str) -> Result<Package> {
        let matching: Vec<&Package> = CARGO_METADATA
            .packages
            .iter()
            .filter(|p| p.name == name)
            .collect();
        match matching.len() {
            1 => Ok(matching[0].clone()),
            n => bail!("cargo metadata return {n} packages named {name}"),
        }
    }

    fn find_cdylib_path(package: &Package) -> Result<Utf8PathBuf> {
        let cdylib_targets: Vec<&Target> = package
            .targets
            .iter()
            .filter(|t| t.crate_types.iter().any(|t| t == "cdylib"))
            .collect();
        let target = match cdylib_targets.len() {
            1 => cdylib_targets[0],
            n => bail!("Found {n} cdylib targets for {}", package.name),
        };

        let artifacts = CARGO_BUILD_MESSAGES
            .iter()
            .filter_map(|message| match message {
                Message::CompilerArtifact(artifact) => {
                    if artifact.target == *target {
                        Some(artifact.clone())
                    } else {
                        None
                    }
                }
                _ => None,
            });
        let cdylib_files: Vec<Utf8PathBuf> = artifacts
            .into_iter()
            .flat_map(|artifact| {
                artifact
                    .filenames
                    .into_iter()
                    .filter(|nm| matches!(nm.extension(), Some(DLL_EXTENSION)))
                    .collect::<Vec<Utf8PathBuf>>()
            })
            .collect();

        match cdylib_files.len() {
            1 => Ok(cdylib_files[0].to_owned()),
            n => bail!("Found {n} cdylib files for {}", package.name),
        }
    }

    /// Create at `out_dir` for testing
    ///
    /// This directory can be used for:
    ///   - Generated bindings files (usually via the `--out-dir` param)
    ///   - cdylib libraries that the bindings depend on
    ///   - Anything else that's useful for testing
    ///
    /// This directory typically created as a subdirectory of `CARGO_TARGET_TMPDIR` when running an
    /// integration test.
    ///
    /// We use the script path to create a hash included in the outpuit directory.  This avoids
    /// path collutions when 2 scripts run against the same fixture.
    pub fn create_out_dir(
        &self,
        temp_dir: impl AsRef<Utf8Path>,
        script_path: impl AsRef<Utf8Path>,
    ) -> Result<Utf8PathBuf> {
        let dirname = format!("{}-{}", self.name, hash_path(script_path.as_ref()));
        let out_dir = temp_dir.as_ref().join(dirname);
        if out_dir.exists() {
            // Clean out any files from previous runs
            fs::remove_dir_all(&out_dir)?;
        }
        fs::create_dir(&out_dir)?;
        println!("Creating testing out_dir: {out_dir}");
        Ok(out_dir)
    }

    /// Copy the `cdylib` for a fixture into the out_dir
    ///
    /// This is typically needed for the bindings to open it when running the tests
    ///
    /// Returns the path to the copied library
    pub fn copy_cdylib_to_out_dir(&self, out_dir: impl AsRef<Utf8Path>) -> Result<Utf8PathBuf> {
        let path = self.cdylib_path()?;
        let dest = out_dir.as_ref().join(path.file_name().unwrap());
        fs::copy(&path, &dest)?;
        Ok(dest)
    }

    /// Get the path to the cdylib file for this package
    pub fn cdylib_path(&self) -> Result<Utf8PathBuf> {
        Self::find_cdylib_path(&self.package)
    }
}

fn get_cargo_metadata() -> Metadata {
    MetadataCommand::new()
        .exec()
        .expect("error running cargo metadata")
}

fn get_cargo_build_messages() -> Vec<Message> {
    let mut child = Command::new(env!("CARGO"))
        .arg("build")
        .arg("--message-format=json")
        .stdout(Stdio::piped())
        .spawn()
        .expect("Error running cargo build");
    let output = std::io::BufReader::new(child.stdout.take().unwrap());
    Message::parse_stream(output)
        .map(|m| m.expect("Error parsing cargo build messages"))
        .collect()
}

fn hash_path(path: &Utf8Path) -> String {
    let mut hasher = DefaultHasher::new();
    path.hash(&mut hasher);
    format!("{:x}", hasher.finish())
}
