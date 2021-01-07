#![forbid(unsafe_code)]
#![warn(rust_2018_idioms, single_use_lifetimes)]

use std::{
    env, fs,
    path::{Path, PathBuf},
    process::Command,
    str,
};

// The rustc-cfg strings below are *not* public API. Please let us know by
// opening a GitHub issue if your build environment requires some way to enable
// these cfgs other than by executing our build script.
fn main() {
    let rustc: PathBuf = env::var_os("RUSTC").unwrap_or_else(|| "rustc".into()).into();
    let version = match Version::from_rustc(&rustc) {
        Ok(version) => version.print(),
        Err(e) => {
            println!(
                "cargo:warning={}: unable to determine rustc version: {}",
                env!("CARGO_PKG_NAME"),
                e
            );
            return;
        }
    };

    let out_dir: PathBuf = env::var_os("OUT_DIR").expect("OUT_DIR not set").into();
    let out_file = &out_dir.join("version");
    fs::write(out_file, version)
        .unwrap_or_else(|e| panic!("failed to write {}: {}", out_file.display(), e));

    // Mark as build script has been run successfully.
    println!("cargo:rustc-cfg=const_fn_has_build_script");
}

struct Version {
    minor: u32,
    nightly: bool,
}

impl Version {
    // Based on https://github.com/cuviper/autocfg/blob/1.0.1/src/version.rs#L25-L59
    //
    // TODO: use autocfg if https://github.com/cuviper/autocfg/issues/28 merged
    // or https://github.com/taiki-e/const_fn/issues/27 rejected.
    fn from_rustc(rustc: &Path) -> Result<Self, String> {
        let output =
            Command::new(rustc).args(&["--version", "--verbose"]).output().map_err(|e| {
                format!("could not execute `{} --version --verbose`: {}", rustc.display(), e)
            })?;
        if !output.status.success() {
            return Err(format!(
                "process didn't exit successfully: `{} --version --verbose`",
                rustc.display()
            ));
        }
        let output = str::from_utf8(&output.stdout).map_err(|e| {
            format!("failed to parse output of `{} --version --verbose`: {}", rustc.display(), e)
        })?;

        // Find the release line in the verbose version output.
        let release = output
            .lines()
            .find(|line| line.starts_with("release: "))
            .map(|line| &line["release: ".len()..])
            .ok_or_else(|| {
                format!(
                    "could not find rustc release from output of `{} --version --verbose`: {}",
                    rustc.display(),
                    output
                )
            })?;

        // Split the version and channel info.
        let mut version_channel = release.split('-');
        let version = version_channel.next().unwrap();
        let channel = version_channel.next();

        let minor = (|| {
            // Split the version into semver components.
            let mut digits = version.splitn(3, '.');
            let major = digits.next()?;
            if major != "1" {
                return None;
            }
            let minor = digits.next()?.parse().ok()?;
            let _patch = digits.next()?;
            Some(minor)
        })()
        .ok_or_else(|| {
            format!("unexpected output from `{} --version --verbose`: {}", rustc.display(), output)
        })?;

        let nightly = channel.map_or(false, |c| c == "dev" || c == "nightly");
        Ok(Self { minor, nightly })
    }

    fn print(&self) -> String {
        format!("Version {{ minor: {}, nightly: {} }}\n", self.minor, self.nightly)
    }
}
