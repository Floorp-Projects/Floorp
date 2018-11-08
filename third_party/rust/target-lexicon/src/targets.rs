// This file defines all the identifier enums and target-aware logic.

use std::fmt;
use std::str::FromStr;
use triple::{Endianness, PointerWidth, Triple};

/// The "architecture" field, which in some cases also specifies a specific
/// subarchitecture.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum Architecture {
    Unknown,
    Aarch64,
    Arm,
    Armv4t,
    Armv5te,
    Armv7,
    Armv7s,
    Asmjs,
    I386,
    I586,
    I686,
    Mips,
    Mips64,
    Mips64el,
    Mipsel,
    Msp430,
    Powerpc,
    Powerpc64,
    Powerpc64le,
    Riscv32,
    Riscv64,
    S390x,
    Sparc,
    Sparc64,
    Sparcv9,
    Thumbv6m,
    Thumbv7em,
    Thumbv7m,
    Wasm32,
    X86_64,
}

/// The "vendor" field, which in practice is little more than an arbitrary
/// modifier.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum Vendor {
    Unknown,
    Apple,
    Experimental,
    Pc,
    Rumprun,
    Sun,
}

/// The "operating system" field, which sometimes implies an environment, and
/// sometimes isn't an actual operating system.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum OperatingSystem {
    Unknown,
    Bitrig,
    Cloudabi,
    Darwin,
    Dragonfly,
    Emscripten,
    Freebsd,
    Fuchsia,
    Haiku,
    Ios,
    L4re,
    Linux,
    Nebulet,
    Netbsd,
    Openbsd,
    Redox,
    Solaris,
    Windows,
}

/// The "environment" field, which specifies an ABI environment on top of the
/// operating system. In many configurations, this field is omitted, and the
/// environment is implied by the operating system.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum Environment {
    Unknown,
    Android,
    Androideabi,
    Eabi,
    Eabihf,
    Gnu,
    Gnuabi64,
    Gnueabi,
    Gnueabihf,
    Gnuspe,
    Gnux32,
    Musl,
    Musleabi,
    Musleabihf,
    Msvc,
    Uclibc,
}

/// The "binary format" field, which is usually omitted, and the binary format
/// is implied by the other fields.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum BinaryFormat {
    Unknown,
    Elf,
    Coff,
    Macho,
    Wasm,
}

impl Architecture {
    /// Return the endianness of this architecture.
    pub fn endianness(&self) -> Result<Endianness, ()> {
        match *self {
            Architecture::Unknown => Err(()),
            Architecture::Aarch64
            | Architecture::Arm
            | Architecture::Armv4t
            | Architecture::Armv5te
            | Architecture::Armv7
            | Architecture::Armv7s
            | Architecture::Asmjs
            | Architecture::I386
            | Architecture::I586
            | Architecture::I686
            | Architecture::Mips64el
            | Architecture::Mipsel
            | Architecture::Msp430
            | Architecture::Powerpc64le
            | Architecture::Riscv32
            | Architecture::Riscv64
            | Architecture::Thumbv6m
            | Architecture::Thumbv7em
            | Architecture::Thumbv7m
            | Architecture::Wasm32
            | Architecture::X86_64 => Ok(Endianness::Little),
            Architecture::Mips
            | Architecture::Mips64
            | Architecture::Powerpc
            | Architecture::Powerpc64
            | Architecture::S390x
            | Architecture::Sparc
            | Architecture::Sparc64
            | Architecture::Sparcv9 => Ok(Endianness::Big),
        }
    }

    /// Return the pointer bit width of this target's architecture.
    pub fn pointer_width(&self) -> Result<PointerWidth, ()> {
        match *self {
            Architecture::Unknown => Err(()),
            Architecture::Msp430 => Ok(PointerWidth::U16),
            Architecture::Arm
            | Architecture::Armv4t
            | Architecture::Armv5te
            | Architecture::Armv7
            | Architecture::Armv7s
            | Architecture::Asmjs
            | Architecture::I386
            | Architecture::I586
            | Architecture::I686
            | Architecture::Mipsel
            | Architecture::Riscv32
            | Architecture::Sparc
            | Architecture::Thumbv6m
            | Architecture::Thumbv7em
            | Architecture::Thumbv7m
            | Architecture::Wasm32
            | Architecture::Mips
            | Architecture::Powerpc => Ok(PointerWidth::U32),
            Architecture::Aarch64
            | Architecture::Mips64el
            | Architecture::Powerpc64le
            | Architecture::Riscv64
            | Architecture::X86_64
            | Architecture::Mips64
            | Architecture::Powerpc64
            | Architecture::S390x
            | Architecture::Sparc64
            | Architecture::Sparcv9 => Ok(PointerWidth::U64),
        }
    }
}

/// Return the binary format implied by this target triple, ignoring its
/// `binary_format` field.
pub fn default_binary_format(triple: &Triple) -> BinaryFormat {
    match triple.operating_system {
        OperatingSystem::Darwin | OperatingSystem::Ios => BinaryFormat::Macho,
        OperatingSystem::Windows => BinaryFormat::Coff,
        OperatingSystem::Nebulet | OperatingSystem::Emscripten | OperatingSystem::Unknown => {
            match triple.architecture {
                Architecture::Wasm32 => BinaryFormat::Wasm,
                _ => BinaryFormat::Unknown,
            }
        }
        _ => BinaryFormat::Elf,
    }
}

impl fmt::Display for Architecture {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let s = match *self {
            Architecture::Unknown => "unknown",
            Architecture::Aarch64 => "aarch64",
            Architecture::Arm => "arm",
            Architecture::Armv4t => "armv4t",
            Architecture::Armv5te => "armv5te",
            Architecture::Armv7 => "armv7",
            Architecture::Armv7s => "armv7s",
            Architecture::Asmjs => "asmjs",
            Architecture::I386 => "i386",
            Architecture::I586 => "i586",
            Architecture::I686 => "i686",
            Architecture::Mips => "mips",
            Architecture::Mips64 => "mips64",
            Architecture::Mips64el => "mips64el",
            Architecture::Mipsel => "mipsel",
            Architecture::Msp430 => "msp430",
            Architecture::Powerpc => "powerpc",
            Architecture::Powerpc64 => "powerpc64",
            Architecture::Powerpc64le => "powerpc64le",
            Architecture::Riscv32 => "riscv32",
            Architecture::Riscv64 => "riscv64",
            Architecture::S390x => "s390x",
            Architecture::Sparc => "sparc",
            Architecture::Sparc64 => "sparc64",
            Architecture::Sparcv9 => "sparcv9",
            Architecture::Thumbv6m => "thumbv6m",
            Architecture::Thumbv7em => "thumbv7em",
            Architecture::Thumbv7m => "thumbv7m",
            Architecture::Wasm32 => "wasm32",
            Architecture::X86_64 => "x86_64",
        };
        f.write_str(s)
    }
}

impl FromStr for Architecture {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        Ok(match s {
            "unknown" => Architecture::Unknown,
            "aarch64" => Architecture::Aarch64,
            "arm" => Architecture::Arm,
            "armv4t" => Architecture::Armv4t,
            "armv5te" => Architecture::Armv5te,
            "armv7" => Architecture::Armv7,
            "armv7s" => Architecture::Armv7s,
            "asmjs" => Architecture::Asmjs,
            "i386" => Architecture::I386,
            "i586" => Architecture::I586,
            "i686" => Architecture::I686,
            "mips" => Architecture::Mips,
            "mips64" => Architecture::Mips64,
            "mips64el" => Architecture::Mips64el,
            "mipsel" => Architecture::Mipsel,
            "msp430" => Architecture::Msp430,
            "powerpc" => Architecture::Powerpc,
            "powerpc64" => Architecture::Powerpc64,
            "powerpc64le" => Architecture::Powerpc64le,
            "riscv32" => Architecture::Riscv32,
            "riscv64" => Architecture::Riscv64,
            "s390x" => Architecture::S390x,
            "sparc" => Architecture::Sparc,
            "sparc64" => Architecture::Sparc64,
            "sparcv9" => Architecture::Sparcv9,
            "thumbv6m" => Architecture::Thumbv6m,
            "thumbv7em" => Architecture::Thumbv7em,
            "thumbv7m" => Architecture::Thumbv7m,
            "wasm32" => Architecture::Wasm32,
            "x86_64" => Architecture::X86_64,
            _ => return Err(()),
        })
    }
}

impl fmt::Display for Vendor {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let s = match *self {
            Vendor::Unknown => "unknown",
            Vendor::Apple => "apple",
            Vendor::Experimental => "experimental",
            Vendor::Pc => "pc",
            Vendor::Rumprun => "rumprun",
            Vendor::Sun => "sun",
        };
        f.write_str(s)
    }
}

impl FromStr for Vendor {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        Ok(match s {
            "unknown" => Vendor::Unknown,
            "apple" => Vendor::Apple,
            "experimental" => Vendor::Experimental,
            "pc" => Vendor::Pc,
            "rumprun" => Vendor::Rumprun,
            "sun" => Vendor::Sun,
            _ => return Err(()),
        })
    }
}

impl fmt::Display for OperatingSystem {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let s = match *self {
            OperatingSystem::Unknown => "unknown",
            OperatingSystem::Bitrig => "bitrig",
            OperatingSystem::Cloudabi => "cloudabi",
            OperatingSystem::Darwin => "darwin",
            OperatingSystem::Dragonfly => "dragonfly",
            OperatingSystem::Emscripten => "emscripten",
            OperatingSystem::Freebsd => "freebsd",
            OperatingSystem::Fuchsia => "fuchsia",
            OperatingSystem::Haiku => "haiku",
            OperatingSystem::Ios => "ios",
            OperatingSystem::L4re => "l4re",
            OperatingSystem::Linux => "linux",
            OperatingSystem::Nebulet => "nebulet",
            OperatingSystem::Netbsd => "netbsd",
            OperatingSystem::Openbsd => "openbsd",
            OperatingSystem::Redox => "redox",
            OperatingSystem::Solaris => "solaris",
            OperatingSystem::Windows => "windows",
        };
        f.write_str(s)
    }
}

impl FromStr for OperatingSystem {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        Ok(match s {
            "unknown" => OperatingSystem::Unknown,
            "bitrig" => OperatingSystem::Bitrig,
            "cloudabi" => OperatingSystem::Cloudabi,
            "darwin" => OperatingSystem::Darwin,
            "dragonfly" => OperatingSystem::Dragonfly,
            "emscripten" => OperatingSystem::Emscripten,
            "freebsd" => OperatingSystem::Freebsd,
            "fuchsia" => OperatingSystem::Fuchsia,
            "haiku" => OperatingSystem::Haiku,
            "ios" => OperatingSystem::Ios,
            "l4re" => OperatingSystem::L4re,
            "linux" => OperatingSystem::Linux,
            "nebulet" => OperatingSystem::Nebulet,
            "netbsd" => OperatingSystem::Netbsd,
            "openbsd" => OperatingSystem::Openbsd,
            "redox" => OperatingSystem::Redox,
            "solaris" => OperatingSystem::Solaris,
            "windows" => OperatingSystem::Windows,
            _ => return Err(()),
        })
    }
}

impl fmt::Display for Environment {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let s = match *self {
            Environment::Unknown => "unknown",
            Environment::Android => "android",
            Environment::Androideabi => "androideabi",
            Environment::Eabi => "eabi",
            Environment::Eabihf => "eabihf",
            Environment::Gnu => "gnu",
            Environment::Gnuabi64 => "gnuabi64",
            Environment::Gnueabi => "gnueabi",
            Environment::Gnueabihf => "gnueabihf",
            Environment::Gnuspe => "gnuspe",
            Environment::Gnux32 => "gnux32",
            Environment::Musl => "musl",
            Environment::Musleabi => "musleabi",
            Environment::Musleabihf => "musleabihf",
            Environment::Msvc => "msvc",
            Environment::Uclibc => "uclibc",
        };
        f.write_str(s)
    }
}

impl FromStr for Environment {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        Ok(match s {
            "unknown" => Environment::Unknown,
            "android" => Environment::Android,
            "androideabi" => Environment::Androideabi,
            "eabi" => Environment::Eabi,
            "eabihf" => Environment::Eabihf,
            "gnu" => Environment::Gnu,
            "gnuabi64" => Environment::Gnuabi64,
            "gnueabi" => Environment::Gnueabi,
            "gnueabihf" => Environment::Gnueabihf,
            "gnuspe" => Environment::Gnuspe,
            "gnux32" => Environment::Gnux32,
            "musl" => Environment::Musl,
            "musleabi" => Environment::Musleabi,
            "musleabihf" => Environment::Musleabihf,
            "msvc" => Environment::Msvc,
            "uclibc" => Environment::Uclibc,
            _ => return Err(()),
        })
    }
}

impl fmt::Display for BinaryFormat {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let s = match *self {
            BinaryFormat::Unknown => "unknown",
            BinaryFormat::Elf => "elf",
            BinaryFormat::Coff => "coff",
            BinaryFormat::Macho => "macho",
            BinaryFormat::Wasm => "wasm",
        };
        f.write_str(s)
    }
}

impl FromStr for BinaryFormat {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        Ok(match s {
            "unknown" => BinaryFormat::Unknown,
            "elf" => BinaryFormat::Elf,
            "coff" => BinaryFormat::Coff,
            "macho" => BinaryFormat::Macho,
            "wasm" => BinaryFormat::Wasm,
            _ => return Err(()),
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::string::ToString;

    #[test]
    fn rust_targets() {
        // At the time of writing this, these are all the targets emitted by
        // "rustup target list" and "rustc --print target-list".
        let targets = [
            "aarch64-apple-ios",
            "aarch64-linux-android",
            "aarch64-unknown-fuchsia",
            "aarch64-unknown-linux-gnu",
            "aarch64-unknown-linux-musl",
            "arm-linux-androideabi",
            "arm-unknown-linux-gnueabi",
            "arm-unknown-linux-gnueabihf",
            "arm-unknown-linux-musleabi",
            "arm-unknown-linux-musleabihf",
            "armv5te-unknown-linux-gnueabi",
            "armv7-apple-ios",
            "armv7-linux-androideabi",
            "armv7-unknown-linux-gnueabihf",
            "armv7-unknown-linux-musleabihf",
            "armv7s-apple-ios",
            "asmjs-unknown-emscripten",
            "i386-apple-ios",
            "i586-pc-windows-msvc",
            "i586-unknown-linux-gnu",
            "i586-unknown-linux-musl",
            "i686-apple-darwin",
            "i686-linux-android",
            "i686-pc-windows-gnu",
            "i686-pc-windows-msvc",
            "i686-unknown-freebsd",
            "i686-unknown-linux-gnu",
            "i686-unknown-linux-musl",
            "mips-unknown-linux-gnu",
            "mips-unknown-linux-musl",
            "mips64-unknown-linux-gnuabi64",
            "mips64el-unknown-linux-gnuabi64",
            "mipsel-unknown-linux-gnu",
            "mipsel-unknown-linux-musl",
            "powerpc-unknown-linux-gnu",
            "powerpc64-unknown-linux-gnu",
            "powerpc64le-unknown-linux-gnu",
            "s390x-unknown-linux-gnu",
            "sparc64-unknown-linux-gnu",
            "sparcv9-sun-solaris",
            "thumbv6m-none-eabi",
            "thumbv7em-none-eabi",
            "thumbv7em-none-eabihf",
            "thumbv7m-none-eabi",
            "wasm32-unknown-emscripten",
            "wasm32-unknown-unknown",
            "x86_64-apple-darwin",
            "x86_64-apple-ios",
            "x86_64-linux-android",
            "x86_64-pc-windows-gnu",
            "x86_64-pc-windows-msvc",
            "x86_64-rumprun-netbsd",
            "x86_64-sun-solaris",
            "x86_64-unknown-cloudabi",
            "x86_64-unknown-freebsd",
            "x86_64-unknown-fuchsia",
            "x86_64-unknown-linux-gnu",
            "x86_64-unknown-linux-gnux32",
            "x86_64-unknown-linux-musl",
            "x86_64-unknown-netbsd",
            "x86_64-unknown-redox",
            "aarch64-linux-android",
            "aarch64-unknown-cloudabi",
            "aarch64-unknown-freebsd",
            "aarch64-unknown-fuchsia",
            "aarch64-unknown-linux-gnu",
            "aarch64-unknown-linux-musl",
            "arm-linux-androideabi",
            "arm-unknown-linux-gnueabi",
            "arm-unknown-linux-gnueabihf",
            "arm-unknown-linux-musleabi",
            "arm-unknown-linux-musleabihf",
            "armv4t-unknown-linux-gnueabi",
            "armv5te-unknown-linux-gnueabi",
            "armv7-linux-androideabi",
            "armv7-unknown-cloudabi-eabihf",
            "armv7-unknown-linux-gnueabihf",
            "armv7-unknown-linux-musleabihf",
            "asmjs-unknown-emscripten",
            "i586-pc-windows-msvc",
            "i586-unknown-linux-gnu",
            "i586-unknown-linux-musl",
            "i686-apple-darwin",
            "i686-linux-android",
            "i686-pc-windows-gnu",
            "i686-pc-windows-msvc",
            "i686-unknown-cloudabi",
            "i686-unknown-dragonfly",
            "i686-unknown-freebsd",
            "i686-unknown-haiku",
            "i686-unknown-linux-gnu",
            "i686-unknown-linux-musl",
            "i686-unknown-netbsd",
            "i686-unknown-openbsd",
            "mips-unknown-linux-gnu",
            "mips-unknown-linux-musl",
            "mips-unknown-linux-uclibc",
            "mips64-unknown-linux-gnuabi64",
            "mips64el-unknown-linux-gnuabi64",
            "mipsel-unknown-linux-gnu",
            "mipsel-unknown-linux-musl",
            "mipsel-unknown-linux-uclibc",
            "msp430-none-elf",
            "powerpc-unknown-linux-gnu",
            "powerpc-unknown-linux-gnuspe",
            "powerpc-unknown-netbsd",
            "powerpc64-unknown-linux-gnu",
            "powerpc64le-unknown-linux-gnu",
            "s390x-unknown-linux-gnu",
            "sparc-unknown-linux-gnu",
            "sparc64-unknown-linux-gnu",
            "sparc64-unknown-netbsd",
            "sparcv9-sun-solaris",
            "thumbv6m-none-eabi",
            "thumbv7em-none-eabi",
            "thumbv7em-none-eabihf",
            "thumbv7m-none-eabi",
            "wasm32-experimental-emscripten",
            "wasm32-unknown-emscripten",
            "wasm32-unknown-unknown",
            "x86_64-apple-darwin",
            "x86_64-linux-android",
            "x86_64-pc-windows-gnu",
            "x86_64-pc-windows-msvc",
            "x86_64-rumprun-netbsd",
            "x86_64-sun-solaris",
            "x86_64-unknown-bitrig",
            "x86_64-unknown-cloudabi",
            "x86_64-unknown-dragonfly",
            "x86_64-unknown-freebsd",
            "x86_64-unknown-fuchsia",
            "x86_64-unknown-haiku",
            "x86_64-unknown-l4re-uclibc",
            "x86_64-unknown-linux-gnu",
            "x86_64-unknown-linux-gnux32",
            "x86_64-unknown-linux-musl",
            "x86_64-unknown-netbsd",
            "x86_64-unknown-openbsd",
            "x86_64-unknown-redox",
        ];

        for target in targets.iter() {
            let t = Triple::from_str(target).expect("can't parse target");
            assert_ne!(t.architecture, Architecture::Unknown);
            assert_eq!(t.to_string(), *target);
        }
    }
}
