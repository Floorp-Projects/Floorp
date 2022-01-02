//! build.rs file to obtain the host information.

// Allow dead code in triple.rs and targets.rs for our purposes here.
#![allow(dead_code)]

use std::env;
use std::fs::File;
use std::io::{self, Write};
use std::path::PathBuf;
use std::process::Command;
use std::str::FromStr;

#[cfg(not(feature = "std"))]
extern crate alloc;
#[cfg(feature = "std")]
extern crate std as alloc;

// Include triple.rs and targets.rs so we can parse the TARGET environment variable.
// targets.rs depends on data_model
mod data_model {
    include!("src/data_model.rs");
}
mod triple {
    include!("src/triple.rs");
}
mod targets {
    include!("src/targets.rs");
}

// Stub out `ParseError` to minimally support triple.rs and targets.rs.
mod parse_error {
    #[derive(Debug)]
    pub enum ParseError {
        UnrecognizedArchitecture(String),
        UnrecognizedVendor(String),
        UnrecognizedOperatingSystem(String),
        UnrecognizedEnvironment(String),
        UnrecognizedBinaryFormat(String),
        UnrecognizedField(String),
    }
}

use self::targets::Vendor;
use self::triple::Triple;

fn main() {
    let out_dir =
        PathBuf::from(env::var("OUT_DIR").expect("The OUT_DIR environment variable must be set"));
    let target = env::var("TARGET").expect("The TARGET environment variable must be set");
    let triple =
        Triple::from_str(&target).unwrap_or_else(|_| panic!("Invalid target name: '{}'", target));
    let out = File::create(out_dir.join("host.rs")).expect("error creating host.rs");
    write_host_rs(out, triple).expect("error writing host.rs");
    if using_1_40() {
        println!("cargo:rustc-cfg=feature=\"rust_1_40\"");
    }
}

fn using_1_40() -> bool {
    match (|| {
        let stdout = match Command::new("rustc").arg("--version").output() {
            Ok(output) => {
                if output.status.success() {
                    output.stdout
                } else {
                    return None;
                }
            }
            _ => return None,
        };
        std::str::from_utf8(&stdout)
            .ok()?
            .split(' ')
            .nth(1)?
            .split('.')
            .nth(1)?
            .parse::<i32>()
            .ok()
    })() {
        Some(version) => version >= 40,
        None => true, // assume we're using an up-to-date compiler
    }
}

fn write_host_rs(mut out: File, triple: Triple) -> io::Result<()> {
    // The generated Debug implementation for the inner architecture variants
    // doesn't print the enum name qualifier, so import them here. There
    // shouldn't be any conflicts because these enums all share a namespace
    // in the triple string format.
    writeln!(out, "#[allow(unused_imports)]")?;
    writeln!(out, "use crate::Aarch64Architecture::*;")?;
    writeln!(out, "#[allow(unused_imports)]")?;
    writeln!(out, "use crate::ArmArchitecture::*;")?;
    writeln!(out, "#[allow(unused_imports)]")?;
    writeln!(out, "use crate::CustomVendor;")?;
    writeln!(out, "#[allow(unused_imports)]")?;
    writeln!(out, "use crate::Mips32Architecture::*;")?;
    writeln!(out, "#[allow(unused_imports)]")?;
    writeln!(out, "use crate::Mips64Architecture::*;")?;
    writeln!(out, "#[allow(unused_imports)]")?;
    writeln!(out, "use crate::Riscv32Architecture::*;")?;
    writeln!(out, "#[allow(unused_imports)]")?;
    writeln!(out, "use crate::Riscv64Architecture::*;")?;
    writeln!(out, "#[allow(unused_imports)]")?;
    writeln!(out, "use crate::X86_32Architecture::*;")?;
    writeln!(out)?;
    writeln!(out, "/// The `Triple` of the current host.")?;
    writeln!(out, "pub const HOST: Triple = Triple {{")?;
    writeln!(
        out,
        "    architecture: Architecture::{:?},",
        triple.architecture
    )?;
    writeln!(
        out,
        "    vendor: Vendor::{},",
        vendor_display(&triple.vendor)
    )?;
    writeln!(
        out,
        "    operating_system: OperatingSystem::{:?},",
        triple.operating_system
    )?;
    writeln!(
        out,
        "    environment: Environment::{:?},",
        triple.environment
    )?;
    writeln!(
        out,
        "    binary_format: BinaryFormat::{:?},",
        triple.binary_format
    )?;
    writeln!(out, "}};")?;
    writeln!(out)?;

    writeln!(out, "impl Architecture {{")?;
    writeln!(out, "    /// Return the architecture for the current host.")?;
    writeln!(out, "    pub const fn host() -> Self {{")?;
    writeln!(out, "        Architecture::{:?}", triple.architecture)?;
    writeln!(out, "    }}")?;
    writeln!(out, "}}")?;
    writeln!(out)?;

    writeln!(out, "impl Vendor {{")?;
    writeln!(out, "    /// Return the vendor for the current host.")?;
    writeln!(out, "    pub const fn host() -> Self {{")?;
    writeln!(out, "        Vendor::{}", vendor_display(&triple.vendor))?;
    writeln!(out, "    }}")?;
    writeln!(out, "}}")?;
    writeln!(out)?;

    writeln!(out, "impl OperatingSystem {{")?;
    writeln!(
        out,
        "    /// Return the operating system for the current host."
    )?;
    writeln!(out, "    pub const fn host() -> Self {{")?;
    writeln!(
        out,
        "        OperatingSystem::{:?}",
        triple.operating_system
    )?;
    writeln!(out, "    }}")?;
    writeln!(out, "}}")?;
    writeln!(out)?;

    writeln!(out, "impl Environment {{")?;
    writeln!(out, "    /// Return the environment for the current host.")?;
    writeln!(out, "    pub const fn host() -> Self {{")?;
    writeln!(out, "        Environment::{:?}", triple.environment)?;
    writeln!(out, "    }}")?;
    writeln!(out, "}}")?;
    writeln!(out)?;

    writeln!(out, "impl BinaryFormat {{")?;
    writeln!(
        out,
        "    /// Return the binary format for the current host."
    )?;
    writeln!(out, "    pub const fn host() -> Self {{")?;
    writeln!(out, "        BinaryFormat::{:?}", triple.binary_format)?;
    writeln!(out, "    }}")?;
    writeln!(out, "}}")?;
    writeln!(out)?;

    writeln!(out, "impl Triple {{")?;
    writeln!(out, "    /// Return the triple for the current host.")?;
    writeln!(out, "    pub const fn host() -> Self {{")?;
    writeln!(out, "        Self {{")?;
    writeln!(
        out,
        "            architecture: Architecture::{:?},",
        triple.architecture
    )?;
    writeln!(
        out,
        "            vendor: Vendor::{},",
        vendor_display(&triple.vendor)
    )?;
    writeln!(
        out,
        "            operating_system: OperatingSystem::{:?},",
        triple.operating_system
    )?;
    writeln!(
        out,
        "            environment: Environment::{:?},",
        triple.environment
    )?;
    writeln!(
        out,
        "            binary_format: BinaryFormat::{:?},",
        triple.binary_format
    )?;
    writeln!(out, "        }}")?;
    writeln!(out, "    }}")?;
    writeln!(out, "}}")?;

    Ok(())
}

fn vendor_display(vendor: &Vendor) -> String {
    match vendor {
        Vendor::Custom(custom) => format!("Custom(CustomVendor::Static({:?}))", custom.as_str()),
        known => format!("{:?}", known),
    }
}
