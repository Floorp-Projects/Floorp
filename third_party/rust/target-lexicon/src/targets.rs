// This file defines all the identifier enums and target-aware logic.

use crate::triple::{Endianness, PointerWidth, Triple};
use alloc::boxed::Box;
use alloc::string::String;
use core::fmt;
use core::hash::{Hash, Hasher};
use core::str::FromStr;

/// The "architecture" field, which in some cases also specifies a specific
/// subarchitecture.
#[cfg_attr(feature = "rust_1_40", non_exhaustive)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum Architecture {
    Unknown,
    Arm(ArmArchitecture),
    AmdGcn,
    Aarch64(Aarch64Architecture),
    Asmjs,
    Avr,
    Hexagon,
    X86_32(X86_32Architecture),
    Mips32(Mips32Architecture),
    Mips64(Mips64Architecture),
    Msp430,
    Nvptx64,
    Powerpc,
    Powerpc64,
    Powerpc64le,
    Riscv32(Riscv32Architecture),
    Riscv64(Riscv64Architecture),
    S390x,
    Sparc,
    Sparc64,
    Sparcv9,
    Wasm32,
    Wasm64,
    X86_64,
}

#[cfg_attr(feature = "rust_1_40", non_exhaustive)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum ArmArchitecture {
    Arm, // Generic arm
    Armeb,
    Armv4,
    Armv4t,
    Armv5t,
    Armv5te,
    Armv5tej,
    Armv6,
    Armv6j,
    Armv6k,
    Armv6z,
    Armv6kz,
    Armv6t2,
    Armv6m,
    Armv7,
    Armv7a,
    Armv7ve,
    Armv7m,
    Armv7r,
    Armv7s,
    Armv8,
    Armv8a,
    Armv8_1a,
    Armv8_2a,
    Armv8_3a,
    Armv8_4a,
    Armv8_5a,
    Armv8mBase,
    Armv8mMain,
    Armv8r,

    Armebv7r,

    Thumbeb,
    Thumbv4t,
    Thumbv6m,
    Thumbv7a,
    Thumbv7em,
    Thumbv7m,
    Thumbv7neon,
    Thumbv8mBase,
    Thumbv8mMain,
}

#[cfg_attr(feature = "rust_1_40", non_exhaustive)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum Aarch64Architecture {
    Aarch64,
    Aarch64be,
}

// #[cfg_attr(feature = "rust_1_40", non_exhaustive)]
// #[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
// #[allow(missing_docs)]
// pub enum ArmFpu {
//     Vfp,
//     Vfpv2,
//     Vfpv3,
//     Vfpv3Fp16,
//     Vfpv3Xd,
//     Vfpv3XdFp16,
//     Neon,
//     NeonVfpv3,
//     NeonVfpv4,
//     Vfpv4,
//     Vfpv4D16,
//     Fpv4SpD16,
//     Fpv5SpD16,
//     Fpv5D16,
//     FpArmv8,
//     NeonFpArmv8,
//     CryptoNeonFpArmv8,
// }

impl ArmArchitecture {
    /// Test if this architecture uses the Thumb instruction set.
    #[rustfmt::skip]
    pub fn is_thumb(self) -> bool {
        use ArmArchitecture::*;

        match self {
            Arm
            | Armeb
            | Armv4
            | Armv4t
            | Armv5t
            | Armv5te
            | Armv5tej
            | Armv6
            | Armv6j
            | Armv6k
            | Armv6z
            | Armv6kz
            | Armv6t2
            | Armv6m
            | Armv7
            | Armv7a
            | Armv7ve
            | Armv7m
            | Armv7r
            | Armv7s
            | Armv8
            | Armv8a
            | Armv8_1a
            | Armv8_2a
            | Armv8_3a
            | Armv8_4a
            | Armv8_5a
            | Armv8mBase
            | Armv8mMain
            | Armv8r
            | Armebv7r => false,
            Thumbeb
            | Thumbv4t
            | Thumbv6m
            | Thumbv7a
            | Thumbv7em
            | Thumbv7m
            | Thumbv7neon
            | Thumbv8mBase
            | Thumbv8mMain => true,
        }
    }

    // pub fn has_fpu(self) -> Result<&'static [ArmFpu], ()> {

    // }

    /// Return the pointer bit width of this target's architecture.
    #[rustfmt::skip]
    pub fn pointer_width(self) -> PointerWidth {
        use ArmArchitecture::*;

        match self {
            Arm
            | Armeb
            | Armv4
            | Armv4t
            | Armv5t
            | Armv5te
            | Armv5tej
            | Armv6
            | Armv6j
            | Armv6k
            | Armv6z
            | Armv6kz
            | Armv6t2
            | Armv6m
            | Armv7
            | Armv7a
            | Armv7ve
            | Armv7m
            | Armv7r
            | Armv7s
            | Armv8
            | Armv8a
            | Armv8_1a
            | Armv8_2a
            | Armv8_3a
            | Armv8_4a
            | Armv8_5a
            | Armv8mBase
            | Armv8mMain
            | Armv8r
            | Armebv7r
            | Thumbeb
            | Thumbv4t
            | Thumbv6m
            | Thumbv7a
            | Thumbv7em
            | Thumbv7m
            | Thumbv7neon
            | Thumbv8mBase
            | Thumbv8mMain => PointerWidth::U32,
        }
    }

    /// Return the endianness of this architecture.
    #[rustfmt::skip]
    pub fn endianness(self) -> Endianness {
        use ArmArchitecture::*;

        match self {
            Arm
            | Armv4
            | Armv4t
            | Armv5t
            | Armv5te
            | Armv5tej
            | Armv6
            | Armv6j
            | Armv6k
            | Armv6z
            | Armv6kz
            | Armv6t2
            | Armv6m
            | Armv7
            | Armv7a
            | Armv7ve
            | Armv7m
            | Armv7r
            | Armv7s
            | Armv8
            | Armv8a
            | Armv8_1a
            | Armv8_2a
            | Armv8_3a
            | Armv8_4a
            | Armv8_5a
            | Armv8mBase
            | Armv8mMain
            | Armv8r
            | Thumbv4t
            | Thumbv6m
            | Thumbv7a
            | Thumbv7em
            | Thumbv7m
            | Thumbv7neon
            | Thumbv8mBase
            | Thumbv8mMain => Endianness::Little,
            Armeb | Armebv7r | Thumbeb => Endianness::Big,
        }
    }
}

impl Aarch64Architecture {
    /// Test if this architecture uses the Thumb instruction set.
    pub fn is_thumb(self) -> bool {
        match self {
            Aarch64Architecture::Aarch64 | Aarch64Architecture::Aarch64be => false,
        }
    }

    // pub fn has_fpu(self) -> Result<&'static [ArmFpu], ()> {

    // }

    /// Return the pointer bit width of this target's architecture.
    pub fn pointer_width(self) -> PointerWidth {
        match self {
            Aarch64Architecture::Aarch64 | Aarch64Architecture::Aarch64be => PointerWidth::U64,
        }
    }

    /// Return the endianness of this architecture.
    pub fn endianness(self) -> Endianness {
        match self {
            Aarch64Architecture::Aarch64 => Endianness::Little,
            Aarch64Architecture::Aarch64be => Endianness::Big,
        }
    }
}

/// An enum for all 32-bit RISC-V architectures.
#[cfg_attr(feature = "rust_1_40", non_exhaustive)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum Riscv32Architecture {
    Riscv32,
    Riscv32gc,
    Riscv32i,
    Riscv32imac,
    Riscv32imc,
}

/// An enum for all 64-bit RISC-V architectures.
#[cfg_attr(feature = "rust_1_40", non_exhaustive)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum Riscv64Architecture {
    Riscv64,
    Riscv64gc,
    Riscv64imac,
}

/// An enum for all 32-bit x86 architectures.
#[cfg_attr(feature = "rust_1_40", non_exhaustive)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum X86_32Architecture {
    I386,
    I586,
    I686,
}

/// An enum for all 32-bit MIPS architectures (not just "MIPS32").
#[cfg_attr(feature = "rust_1_40", non_exhaustive)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum Mips32Architecture {
    Mips,
    Mipsel,
    Mipsisa32r6,
    Mipsisa32r6el,
}

/// An enum for all 64-bit MIPS architectures (not just "MIPS64").
#[cfg_attr(feature = "rust_1_40", non_exhaustive)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum Mips64Architecture {
    Mips64,
    Mips64el,
    Mipsisa64r6,
    Mipsisa64r6el,
}

/// A string for a `Vendor::Custom` that can either be used in `const`
/// contexts or hold dynamic strings.
#[derive(Clone, Debug, Eq)]
pub enum CustomVendor {
    /// An owned `String`. This supports the general case.
    Owned(Box<String>),
    /// A static `str`, so that `CustomVendor` can be constructed in `const`
    /// contexts.
    Static(&'static str),
}

impl CustomVendor {
    /// Extracts a string slice.
    pub fn as_str(&self) -> &str {
        match self {
            CustomVendor::Owned(s) => s,
            CustomVendor::Static(s) => s,
        }
    }
}

impl PartialEq for CustomVendor {
    fn eq(&self, other: &Self) -> bool {
        self.as_str() == other.as_str()
    }
}

impl Hash for CustomVendor {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.as_str().hash(state)
    }
}

/// The "vendor" field, which in practice is little more than an arbitrary
/// modifier.
#[cfg_attr(feature = "rust_1_40", non_exhaustive)]
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum Vendor {
    Unknown,
    Amd,
    Apple,
    Experimental,
    Fortanix,
    Nvidia,
    Pc,
    Rumprun,
    Sun,
    Uwp,
    Wrs,

    /// A custom vendor. "Custom" in this context means that the vendor is
    /// not specifically recognized by upstream Autotools, LLVM, Rust, or other
    /// relevant authorities on triple naming. It's useful for people building
    /// and using locally patched toolchains.
    ///
    /// Outside of such patched environments, users of `target-lexicon` should
    /// treat `Custom` the same as `Unknown` and ignore the string.
    Custom(CustomVendor),
}

/// The "operating system" field, which sometimes implies an environment, and
/// sometimes isn't an actual operating system.
#[cfg_attr(feature = "rust_1_40", non_exhaustive)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum OperatingSystem {
    Unknown,
    AmdHsa,
    Bitrig,
    Cloudabi,
    Cuda,
    Darwin,
    Dragonfly,
    Emscripten,
    Freebsd,
    Fuchsia,
    Haiku,
    Hermit,
    Illumos,
    Ios,
    L4re,
    Linux,
    MacOSX { major: u16, minor: u16, patch: u16 },
    Nebulet,
    Netbsd,
    None_,
    Openbsd,
    Psp,
    Redox,
    Solaris,
    Tvos,
    Uefi,
    VxWorks,
    Wasi,
    Windows,
}

/// The "environment" field, which specifies an ABI environment on top of the
/// operating system. In many configurations, this field is omitted, and the
/// environment is implied by the operating system.
#[cfg_attr(feature = "rust_1_40", non_exhaustive)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum Environment {
    Unknown,
    AmdGiz,
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
    GnuIlp32,
    Macabi,
    Musl,
    Musleabi,
    Musleabihf,
    Muslabi64,
    Msvc,
    Kernel,
    Uclibc,
    Uclibceabi,
    Sgx,
    Softfloat,
    Spe,
}

/// The "binary format" field, which is usually omitted, and the binary format
/// is implied by the other fields.
#[cfg_attr(feature = "rust_1_40", non_exhaustive)]
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
    #[rustfmt::skip]
    pub fn endianness(self) -> Result<Endianness, ()> {
        use Architecture::*;

        match self {
            Unknown => Err(()),
            Arm(arm) => Ok(arm.endianness()),
            Aarch64(aarch) => Ok(aarch.endianness()),
            AmdGcn
            | Asmjs
            | Avr
            | Hexagon
            | X86_32(_)
            | Mips64(Mips64Architecture::Mips64el)
            | Mips32(Mips32Architecture::Mipsel)
            | Mips32(Mips32Architecture::Mipsisa32r6el)
            | Mips64(Mips64Architecture::Mipsisa64r6el)
            | Msp430
            | Nvptx64
            | Powerpc64le
            | Riscv32(_)
            | Riscv64(_)
            | Wasm32
            | Wasm64
            | X86_64 => Ok(Endianness::Little),
            Mips32(Mips32Architecture::Mips)
            | Mips64(Mips64Architecture::Mips64)
            | Mips32(Mips32Architecture::Mipsisa32r6)
            | Mips64(Mips64Architecture::Mipsisa64r6)
            | Powerpc
            | Powerpc64
            | S390x
            | Sparc
            | Sparc64
            | Sparcv9 => Ok(Endianness::Big),
        }
    }

    /// Return the pointer bit width of this target's architecture.
    #[rustfmt::skip]
    pub fn pointer_width(self) -> Result<PointerWidth, ()> {
        use Architecture::*;

        match self {
            Unknown => Err(()),
            Avr | Msp430 => Ok(PointerWidth::U16),
            Arm(arm) => Ok(arm.pointer_width()),
            Aarch64(aarch) => Ok(aarch.pointer_width()),
            Asmjs
            | Hexagon
            | X86_32(_)
            | Riscv32(_)
            | Sparc
            | Wasm32
            | Mips32(_)
            | Powerpc => Ok(PointerWidth::U32),
            AmdGcn
            | Powerpc64le
            | Riscv64(_)
            | X86_64
            | Mips64(_)
            | Nvptx64
            | Powerpc64
            | S390x
            | Sparc64
            | Sparcv9
            | Wasm64 => Ok(PointerWidth::U64),
        }
    }
}

/// Return the binary format implied by this target triple, ignoring its
/// `binary_format` field.
pub(crate) fn default_binary_format(triple: &Triple) -> BinaryFormat {
    match triple.operating_system {
        OperatingSystem::None_ => match triple.environment {
            Environment::Eabi | Environment::Eabihf => BinaryFormat::Elf,
            _ => BinaryFormat::Unknown,
        },
        OperatingSystem::Darwin
        | OperatingSystem::Ios
        | OperatingSystem::MacOSX { .. }
        | OperatingSystem::Tvos => BinaryFormat::Macho,
        OperatingSystem::Windows => BinaryFormat::Coff,
        OperatingSystem::Nebulet
        | OperatingSystem::Emscripten
        | OperatingSystem::VxWorks
        | OperatingSystem::Wasi
        | OperatingSystem::Unknown => match triple.architecture {
            Architecture::Wasm32 | Architecture::Wasm64 => BinaryFormat::Wasm,
            _ => BinaryFormat::Unknown,
        },
        _ => BinaryFormat::Elf,
    }
}

impl fmt::Display for ArmArchitecture {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use ArmArchitecture::*;

        let s = match *self {
            Arm => "arm",
            Armeb => "armeb",
            Armv4 => "armv4",
            Armv4t => "armv4t",
            Armv5t => "armv5t",
            Armv5te => "armv5te",
            Armv5tej => "armv5tej",
            Armv6 => "armv6",
            Armv6j => "armv6j",
            Armv6k => "armv6k",
            Armv6z => "armv6z",
            Armv6kz => "armv6kz",
            Armv6t2 => "armv6t2",
            Armv6m => "armv6m",
            Armv7 => "armv7",
            Armv7a => "armv7a",
            Armv7ve => "armv7ve",
            Armv7m => "armv7m",
            Armv7r => "armv7r",
            Armv7s => "armv7s",
            Armv8 => "armv8",
            Armv8a => "armv8a",
            Armv8_1a => "armv8.1a",
            Armv8_2a => "armv8.2a",
            Armv8_3a => "armv8.3a",
            Armv8_4a => "armv8.4a",
            Armv8_5a => "armv8.5a",
            Armv8mBase => "armv8m.base",
            Armv8mMain => "armv8m.main",
            Armv8r => "armv8r",
            Thumbeb => "thumbeb",
            Thumbv4t => "thumbv4t",
            Thumbv6m => "thumbv6m",
            Thumbv7a => "thumbv7a",
            Thumbv7em => "thumbv7em",
            Thumbv7m => "thumbv7m",
            Thumbv7neon => "thumbv7neon",
            Thumbv8mBase => "thumbv8m.base",
            Thumbv8mMain => "thumbv8m.main",
            Armebv7r => "armebv7r",
        };
        f.write_str(s)
    }
}

impl fmt::Display for Aarch64Architecture {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let s = match *self {
            Aarch64Architecture::Aarch64 => "aarch64",
            Aarch64Architecture::Aarch64be => "aarch64_be",
        };
        f.write_str(s)
    }
}

impl fmt::Display for Riscv32Architecture {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use Riscv32Architecture::*;

        let s = match *self {
            Riscv32 => "riscv32",
            Riscv32gc => "riscv32gc",
            Riscv32i => "riscv32i",
            Riscv32imac => "riscv32imac",
            Riscv32imc => "riscv32imc",
        };
        f.write_str(s)
    }
}

impl fmt::Display for Riscv64Architecture {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let s = match *self {
            Riscv64Architecture::Riscv64 => "riscv64",
            Riscv64Architecture::Riscv64gc => "riscv64gc",
            Riscv64Architecture::Riscv64imac => "riscv64imac",
        };
        f.write_str(s)
    }
}

impl fmt::Display for X86_32Architecture {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let s = match *self {
            X86_32Architecture::I386 => "i386",
            X86_32Architecture::I586 => "i586",
            X86_32Architecture::I686 => "i686",
        };
        f.write_str(s)
    }
}

impl fmt::Display for Mips32Architecture {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use Mips32Architecture::*;

        let s = match *self {
            Mips => "mips",
            Mipsel => "mipsel",
            Mipsisa32r6 => "mipsisa32r6",
            Mipsisa32r6el => "mipsisa32r6el",
        };
        f.write_str(s)
    }
}

impl fmt::Display for Mips64Architecture {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use Mips64Architecture::*;

        let s = match *self {
            Mips64 => "mips64",
            Mips64el => "mips64el",
            Mipsisa64r6 => "mipsisa64r6",
            Mipsisa64r6el => "mipsisa64r6el",
        };
        f.write_str(s)
    }
}

impl fmt::Display for Architecture {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use Architecture::*;

        match *self {
            Arm(arm) => arm.fmt(f),
            Aarch64(aarch) => aarch.fmt(f),
            Unknown => f.write_str("unknown"),
            AmdGcn => f.write_str("amdgcn"),
            Asmjs => f.write_str("asmjs"),
            Avr => f.write_str("avr"),
            Hexagon => f.write_str("hexagon"),
            X86_32(x86_32) => x86_32.fmt(f),
            Mips32(mips32) => mips32.fmt(f),
            Mips64(mips64) => mips64.fmt(f),
            Msp430 => f.write_str("msp430"),
            Nvptx64 => f.write_str("nvptx64"),
            Powerpc => f.write_str("powerpc"),
            Powerpc64 => f.write_str("powerpc64"),
            Powerpc64le => f.write_str("powerpc64le"),
            Riscv32(riscv32) => riscv32.fmt(f),
            Riscv64(riscv64) => riscv64.fmt(f),
            S390x => f.write_str("s390x"),
            Sparc => f.write_str("sparc"),
            Sparc64 => f.write_str("sparc64"),
            Sparcv9 => f.write_str("sparcv9"),
            Wasm32 => f.write_str("wasm32"),
            Wasm64 => f.write_str("wasm64"),
            X86_64 => f.write_str("x86_64"),
        }
    }
}

impl FromStr for ArmArchitecture {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        use ArmArchitecture::*;

        Ok(match s {
            "arm" => Arm,
            "armeb" => Armeb,
            "armv4" => Armv4,
            "armv4t" => Armv4t,
            "armv5t" => Armv5t,
            "armv5te" => Armv5te,
            "armv5tej" => Armv5tej,
            "armv6" => Armv6,
            "armv6j" => Armv6j,
            "armv6k" => Armv6k,
            "armv6z" => Armv6z,
            "armv6kz" => Armv6kz,
            "armv6t2" => Armv6t2,
            "armv6m" => Armv6m,
            "armv7" => Armv7,
            "armv7a" => Armv7a,
            "armv7ve" => Armv7ve,
            "armv7m" => Armv7m,
            "armv7r" => Armv7r,
            "armv7s" => Armv7s,
            "armv8" => Armv8,
            "armv8a" => Armv8a,
            "armv8.1a" => Armv8_1a,
            "armv8.2a" => Armv8_2a,
            "armv8.3a" => Armv8_3a,
            "armv8.4a" => Armv8_4a,
            "armv8.5a" => Armv8_5a,
            "armv8m.base" => Armv8mBase,
            "armv8m.main" => Armv8mMain,
            "armv8r" => Armv8r,
            "thumbeb" => Thumbeb,
            "thumbv4t" => Thumbv4t,
            "thumbv6m" => Thumbv6m,
            "thumbv7a" => Thumbv7a,
            "thumbv7em" => Thumbv7em,
            "thumbv7m" => Thumbv7m,
            "thumbv7neon" => Thumbv7neon,
            "thumbv8m.base" => Thumbv8mBase,
            "thumbv8m.main" => Thumbv8mMain,
            "armebv7r" => Armebv7r,
            _ => return Err(()),
        })
    }
}

impl FromStr for Aarch64Architecture {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        use Aarch64Architecture::*;

        Ok(match s {
            "aarch64" => Aarch64,
            "arm64" => Aarch64,
            "aarch64_be" => Aarch64be,
            _ => return Err(()),
        })
    }
}

impl FromStr for Riscv32Architecture {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        use Riscv32Architecture::*;

        Ok(match s {
            "riscv32" => Riscv32,
            "riscv32gc" => Riscv32gc,
            "riscv32i" => Riscv32i,
            "riscv32imac" => Riscv32imac,
            "riscv32imc" => Riscv32imc,
            _ => return Err(()),
        })
    }
}

impl FromStr for Riscv64Architecture {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        use Riscv64Architecture::*;

        Ok(match s {
            "riscv64" => Riscv64,
            "riscv64gc" => Riscv64gc,
            "riscv64imac" => Riscv64imac,
            _ => return Err(()),
        })
    }
}

impl FromStr for X86_32Architecture {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        use X86_32Architecture::*;

        Ok(match s {
            "i386" => I386,
            "i586" => I586,
            "i686" => I686,
            _ => return Err(()),
        })
    }
}

impl FromStr for Mips32Architecture {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        use Mips32Architecture::*;

        Ok(match s {
            "mips" => Mips,
            "mipsel" => Mipsel,
            "mipsisa32r6" => Mipsisa32r6,
            "mipsisa32r6el" => Mipsisa32r6el,
            _ => return Err(()),
        })
    }
}

impl FromStr for Mips64Architecture {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        use Mips64Architecture::*;

        Ok(match s {
            "mips64" => Mips64,
            "mips64el" => Mips64el,
            "mipsisa64r6" => Mipsisa64r6,
            "mipsisa64r6el" => Mipsisa64r6el,
            _ => return Err(()),
        })
    }
}

impl FromStr for Architecture {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        use Architecture::*;

        Ok(match s {
            "unknown" => Unknown,
            "amdgcn" => AmdGcn,
            "asmjs" => Asmjs,
            "avr" => Avr,
            "hexagon" => Hexagon,
            "msp430" => Msp430,
            "nvptx64" => Nvptx64,
            "powerpc" => Powerpc,
            "powerpc64" => Powerpc64,
            "powerpc64le" => Powerpc64le,
            "s390x" => S390x,
            "sparc" => Sparc,
            "sparc64" => Sparc64,
            "sparcv9" => Sparcv9,
            "wasm32" => Wasm32,
            "wasm64" => Wasm64,
            "x86_64" => X86_64,
            _ => {
                if let Ok(arm) = ArmArchitecture::from_str(s) {
                    Arm(arm)
                } else if let Ok(aarch64) = Aarch64Architecture::from_str(s) {
                    Aarch64(aarch64)
                } else if let Ok(riscv32) = Riscv32Architecture::from_str(s) {
                    Riscv32(riscv32)
                } else if let Ok(riscv64) = Riscv64Architecture::from_str(s) {
                    Riscv64(riscv64)
                } else if let Ok(x86_32) = X86_32Architecture::from_str(s) {
                    X86_32(x86_32)
                } else if let Ok(mips32) = Mips32Architecture::from_str(s) {
                    Mips32(mips32)
                } else if let Ok(mips64) = Mips64Architecture::from_str(s) {
                    Mips64(mips64)
                } else {
                    return Err(());
                }
            }
        })
    }
}

impl fmt::Display for Vendor {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use Vendor::*;

        let s = match *self {
            Unknown => "unknown",
            Amd => "amd",
            Apple => "apple",
            Experimental => "experimental",
            Fortanix => "fortanix",
            Nvidia => "nvidia",
            Pc => "pc",
            Rumprun => "rumprun",
            Sun => "sun",
            Uwp => "uwp",
            Wrs => "wrs",
            Custom(ref name) => name.as_str(),
        };
        f.write_str(s)
    }
}

impl FromStr for Vendor {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        use Vendor::*;

        Ok(match s {
            "unknown" => Unknown,
            "amd" => Amd,
            "apple" => Apple,
            "experimental" => Experimental,
            "fortanix" => Fortanix,
            "nvidia" => Nvidia,
            "pc" => Pc,
            "rumprun" => Rumprun,
            "sun" => Sun,
            "uwp" => Uwp,
            "wrs" => Wrs,
            custom => {
                #[cfg(not(feature = "std"))]
                use alloc::borrow::ToOwned;

                // A custom vendor. Since triple syntax is so loosely defined,
                // be as conservative as we can to avoid potential ambiguities.
                // We err on the side of being too strict here, as we can
                // always relax it if needed.

                // Don't allow empty string names.
                if custom.is_empty() {
                    return Err(());
                }

                // Don't allow any other recognized name as a custom vendor,
                // since vendors can be omitted in some contexts.
                if Architecture::from_str(custom).is_ok()
                    || OperatingSystem::from_str(custom).is_ok()
                    || Environment::from_str(custom).is_ok()
                    || BinaryFormat::from_str(custom).is_ok()
                {
                    return Err(());
                }

                // Require the first character to be an ascii lowercase.
                if !custom.chars().next().unwrap().is_ascii_lowercase() {
                    return Err(());
                }

                // Restrict the set of characters permitted in a custom vendor.
                let has_restricted = custom.chars().any(|c: char| {
                    !(c.is_ascii_lowercase() || c.is_ascii_digit() || c == '_' || c == '.')
                });

                if has_restricted {
                    return Err(());
                }

                Custom(CustomVendor::Owned(Box::new(custom.to_owned())))
            }
        })
    }
}

impl fmt::Display for OperatingSystem {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use OperatingSystem::*;

        let s = match *self {
            Unknown => "unknown",
            AmdHsa => "amdhsa",
            Bitrig => "bitrig",
            Cloudabi => "cloudabi",
            Cuda => "cuda",
            Darwin => "darwin",
            Dragonfly => "dragonfly",
            Emscripten => "emscripten",
            Freebsd => "freebsd",
            Fuchsia => "fuchsia",
            Haiku => "haiku",
            Hermit => "hermit",
            Illumos => "illumos",
            Ios => "ios",
            L4re => "l4re",
            Linux => "linux",
            MacOSX {
                major,
                minor,
                patch,
            } => {
                return write!(f, "macosx{}.{}.{}", major, minor, patch);
            }
            Nebulet => "nebulet",
            Netbsd => "netbsd",
            None_ => "none",
            Openbsd => "openbsd",
            Psp => "psp",
            Redox => "redox",
            Solaris => "solaris",
            Tvos => "tvos",
            Uefi => "uefi",
            VxWorks => "vxworks",
            Wasi => "wasi",
            Windows => "windows",
        };
        f.write_str(s)
    }
}

impl FromStr for OperatingSystem {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        use OperatingSystem::*;

        // TODO also parse version number for darwin and ios OSes
        if s.starts_with("macosx") {
            // Parse operating system names like `macosx10.7.0`.
            let s = &s["macosx".len()..];
            let mut parts = s.split('.').map(|num| num.parse::<u16>());

            macro_rules! get_part {
                () => {
                    if let Some(Ok(part)) = parts.next() {
                        part
                    } else {
                        return Err(());
                    }
                };
            }

            let major = get_part!();
            let minor = get_part!();
            let patch = get_part!();

            if parts.next().is_some() {
                return Err(());
            }

            return Ok(MacOSX {
                major,
                minor,
                patch,
            });
        }

        Ok(match s {
            "unknown" => Unknown,
            "amdhsa" => AmdHsa,
            "bitrig" => Bitrig,
            "cloudabi" => Cloudabi,
            "cuda" => Cuda,
            "darwin" => Darwin,
            "dragonfly" => Dragonfly,
            "emscripten" => Emscripten,
            "freebsd" => Freebsd,
            "fuchsia" => Fuchsia,
            "haiku" => Haiku,
            "hermit" => Hermit,
            "illumos" => Illumos,
            "ios" => Ios,
            "l4re" => L4re,
            "linux" => Linux,
            "nebulet" => Nebulet,
            "netbsd" => Netbsd,
            "none" => None_,
            "openbsd" => Openbsd,
            "psp" => Psp,
            "redox" => Redox,
            "solaris" => Solaris,
            "tvos" => Tvos,
            "uefi" => Uefi,
            "vxworks" => VxWorks,
            "wasi" => Wasi,
            "windows" => Windows,
            _ => return Err(()),
        })
    }
}

impl fmt::Display for Environment {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use Environment::*;

        let s = match *self {
            Unknown => "unknown",
            AmdGiz => "amdgiz",
            Android => "android",
            Androideabi => "androideabi",
            Eabi => "eabi",
            Eabihf => "eabihf",
            Gnu => "gnu",
            Gnuabi64 => "gnuabi64",
            Gnueabi => "gnueabi",
            Gnueabihf => "gnueabihf",
            Gnuspe => "gnuspe",
            Gnux32 => "gnux32",
            GnuIlp32 => "gnu_ilp32",
            Macabi => "macabi",
            Musl => "musl",
            Musleabi => "musleabi",
            Musleabihf => "musleabihf",
            Muslabi64 => "muslabi64",
            Msvc => "msvc",
            Kernel => "kernel",
            Uclibc => "uclibc",
            Uclibceabi => "uclibceabi",
            Sgx => "sgx",
            Softfloat => "softfloat",
            Spe => "spe",
        };
        f.write_str(s)
    }
}

impl FromStr for Environment {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        use Environment::*;

        Ok(match s {
            "unknown" => Unknown,
            "amdgiz" => AmdGiz,
            "android" => Android,
            "androideabi" => Androideabi,
            "eabi" => Eabi,
            "eabihf" => Eabihf,
            "gnu" => Gnu,
            "gnuabi64" => Gnuabi64,
            "gnueabi" => Gnueabi,
            "gnueabihf" => Gnueabihf,
            "gnuspe" => Gnuspe,
            "gnux32" => Gnux32,
            "gnu_ilp32" => GnuIlp32,
            "macabi" => Macabi,
            "musl" => Musl,
            "musleabi" => Musleabi,
            "musleabihf" => Musleabihf,
            "muslabi64" => Muslabi64,
            "msvc" => Msvc,
            "kernel" => Kernel,
            "uclibc" => Uclibc,
            "uclibceabi" => Uclibceabi,
            "sgx" => Sgx,
            "softfloat" => Softfloat,
            "spe" => Spe,
            _ => return Err(()),
        })
    }
}

impl fmt::Display for BinaryFormat {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use BinaryFormat::*;

        let s = match *self {
            Unknown => "unknown",
            Elf => "elf",
            Coff => "coff",
            Macho => "macho",
            Wasm => "wasm",
        };
        f.write_str(s)
    }
}

impl FromStr for BinaryFormat {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        use BinaryFormat::*;

        Ok(match s {
            "unknown" => Unknown,
            "elf" => Elf,
            "coff" => Coff,
            "macho" => Macho,
            "wasm" => Wasm,
            _ => return Err(()),
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use alloc::string::ToString;

    #[test]
    fn roundtrip_known_triples() {
        // This list is constructed from:
        //  - targets emitted by "rustup target list"
        //  - targets emitted by "rustc +nightly --print target-list"
        //  - targets contributors have added
        let targets = [
            "aarch64-apple-darwin",
            "aarch64-apple-ios",
            "aarch64-apple-ios-macabi",
            "aarch64-apple-tvos",
            "aarch64_be-unknown-linux-gnu",
            "aarch64_be-unknown-linux-gnu_ilp32",
            "aarch64-fuchsia",
            "aarch64-linux-android",
            "aarch64-pc-windows-msvc",
            "aarch64-unknown-cloudabi",
            "aarch64-unknown-freebsd",
            "aarch64-unknown-hermit",
            "aarch64-unknown-linux-gnu",
            "aarch64-unknown-linux-gnu_ilp32",
            "aarch64-unknown-linux-musl",
            "aarch64-unknown-netbsd",
            "aarch64-unknown-none",
            "aarch64-unknown-none-softfloat",
            "aarch64-unknown-openbsd",
            "aarch64-unknown-redox",
            "aarch64-uwp-windows-msvc",
            "aarch64-wrs-vxworks",
            "amdgcn-amd-amdhsa",
            "amdgcn-amd-amdhsa-amdgiz",
            "armebv7r-none-eabi",
            "armebv7r-none-eabihf",
            "arm-linux-androideabi",
            "arm-unknown-linux-gnueabi",
            "arm-unknown-linux-gnueabihf",
            "arm-unknown-linux-musleabi",
            "arm-unknown-linux-musleabihf",
            "armebv7r-none-eabi",
            "armebv7r-none-eabihf",
            "armv4t-unknown-linux-gnueabi",
            "armv5te-unknown-linux-gnueabi",
            "armv5te-unknown-linux-musleabi",
            "armv5te-unknown-linux-uclibceabi",
            "armv6-unknown-freebsd",
            "armv6-unknown-netbsd-eabihf",
            "armv7a-none-eabi",
            "armv7a-none-eabihf",
            "armv7-apple-ios",
            "armv7-linux-androideabi",
            "armv7r-none-eabi",
            "armv7r-none-eabihf",
            "armv7s-apple-ios",
            "armv7-unknown-cloudabi-eabihf",
            "armv7-unknown-freebsd",
            "armv7-unknown-linux-gnueabi",
            "armv7-unknown-linux-gnueabihf",
            "armv7-unknown-linux-musleabi",
            "armv7-unknown-linux-musleabihf",
            "armv7-unknown-netbsd-eabihf",
            "armv7-wrs-vxworks-eabihf",
            "armv7a-none-eabi",
            "armv7a-none-eabihf",
            "armv7r-none-eabi",
            "armv7r-none-eabihf",
            "armv7s-apple-ios",
            "asmjs-unknown-emscripten",
            "avr-unknown-unknown",
            "hexagon-unknown-linux-musl",
            "i386-apple-ios",
            "i586-pc-windows-msvc",
            "i586-unknown-linux-gnu",
            "i586-unknown-linux-musl",
            "i686-apple-darwin",
            "i686-linux-android",
            "i686-apple-macosx10.7.0",
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
            "i686-unknown-uefi",
            "i686-uwp-windows-gnu",
            "i686-uwp-windows-msvc",
            "i686-wrs-vxworks",
            "mips-unknown-linux-gnu",
            "mips-unknown-linux-musl",
            "mips-unknown-linux-uclibc",
            "mips64-unknown-linux-gnuabi64",
            "mips64-unknown-linux-muslabi64",
            "mips64el-unknown-linux-gnuabi64",
            "mips64el-unknown-linux-muslabi64",
            "mips64-unknown-linux-gnuabi64",
            "mips64-unknown-linux-muslabi64",
            "mipsel-sony-psp",
            "mipsel-unknown-linux-gnu",
            "mipsel-unknown-linux-musl",
            "mipsel-unknown-linux-uclibc",
            "mipsel-unknown-none",
            "mipsisa32r6el-unknown-linux-gnu",
            "mipsisa32r6-unknown-linux-gnu",
            "mipsisa64r6-unknown-linux-gnuabi64",
            "mipsisa64r6el-unknown-linux-gnuabi64",
            "mipsisa64r6-unknown-linux-gnuabi64",
            "mips-unknown-linux-gnu",
            "mips-unknown-linux-musl",
            "mips-unknown-linux-uclibc",
            "msp430-none-elf",
            "nvptx64-nvidia-cuda",
            "powerpc64le-unknown-linux-gnu",
            "powerpc64le-unknown-linux-musl",
            "powerpc64-unknown-freebsd",
            "powerpc64-unknown-linux-gnu",
            "powerpc64-unknown-linux-musl",
            "powerpc64-wrs-vxworks",
            "powerpc-unknown-linux-gnu",
            "powerpc-unknown-linux-gnuspe",
            "powerpc-unknown-linux-musl",
            "powerpc-unknown-netbsd",
            "powerpc-wrs-vxworks",
            "powerpc-wrs-vxworks-spe",
            "powerpc64-unknown-freebsd",
            "powerpc64-unknown-linux-gnu",
            "powerpc64-unknown-linux-musl",
            "powerpc64-wrs-vxworks",
            "powerpc64le-unknown-linux-gnu",
            "powerpc64le-unknown-linux-musl",
            "riscv32gc-unknown-linux-gnu",
            "riscv32i-unknown-none-elf",
            "riscv32imac-unknown-none-elf",
            "riscv32imc-unknown-none-elf",
            "riscv32i-unknown-none-elf",
            "riscv64gc-unknown-linux-gnu",
            "riscv64gc-unknown-none-elf",
            "riscv64imac-unknown-none-elf",
            "s390x-unknown-linux-gnu",
            "sparc-unknown-linux-gnu",
            "sparc64-unknown-linux-gnu",
            "sparc64-unknown-netbsd",
            "sparc64-unknown-openbsd",
            "sparc-unknown-linux-gnu",
            "sparcv9-sun-solaris",
            "thumbv4t-none-eabi",
            "thumbv6m-none-eabi",
            "thumbv4t-none-eabi",
            "thumbv7a-pc-windows-msvc",
            "thumbv7a-uwp-windows-msvc",
            "thumbv7em-none-eabi",
            "thumbv7em-none-eabihf",
            "thumbv7m-none-eabi",
            "thumbv7neon-linux-androideabi",
            "thumbv7neon-unknown-linux-gnueabihf",
            "thumbv7neon-unknown-linux-musleabihf",
            "thumbv8m.base-none-eabi",
            "thumbv8m.main-none-eabi",
            "thumbv8m.main-none-eabihf",
            "wasm32-experimental-emscripten",
            "wasm32-unknown-emscripten",
            "wasm32-unknown-unknown",
            "wasm64-unknown-unknown",
            "wasm32-wasi",
            "wasm64-wasi",
            "x86_64-apple-darwin",
            "x86_64-apple-ios",
            "x86_64-apple-ios-macabi",
            "x86_64-apple-tvos",
            "x86_64-fortanix-unknown-sgx",
            "x86_64-fuchsia",
            "x86_64-linux-android",
            "x86_64-linux-kernel",
            "x86_64-apple-macosx10.7.0",
            "x86_64-pc-solaris",
            "x86_64-pc-windows-gnu",
            "x86_64-pc-windows-msvc",
            "x86_64-rumprun-netbsd",
            "x86_64-sun-solaris",
            "x86_64-unknown-bitrig",
            "x86_64-unknown-cloudabi",
            "x86_64-unknown-dragonfly",
            "x86_64-unknown-freebsd",
            "x86_64-unknown-haiku",
            "x86_64-unknown-hermit",
            "x86_64-unknown-hermit-kernel",
            "x86_64-unknown-illumos",
            "x86_64-unknown-l4re-uclibc",
            "x86_64-unknown-linux-gnu",
            "x86_64-unknown-linux-gnux32",
            "x86_64-unknown-linux-musl",
            "x86_64-unknown-netbsd",
            "x86_64-unknown-openbsd",
            "x86_64-unknown-redox",
            "x86_64-unknown-uefi",
            "x86_64-uwp-windows-gnu",
            "x86_64-uwp-windows-msvc",
            "x86_64-wrs-vxworks",
        ];

        for target in targets.iter() {
            let t = Triple::from_str(target).expect("can't parse target");
            assert_ne!(t.architecture, Architecture::Unknown);
            assert_eq!(t.to_string(), *target);
        }
    }

    #[test]
    fn thumbv7em_none_eabihf() {
        let t = Triple::from_str("thumbv7em-none-eabihf").expect("can't parse target");
        assert_eq!(
            t.architecture,
            Architecture::Arm(ArmArchitecture::Thumbv7em)
        );
        assert_eq!(t.vendor, Vendor::Unknown);
        assert_eq!(t.operating_system, OperatingSystem::None_);
        assert_eq!(t.environment, Environment::Eabihf);
        assert_eq!(t.binary_format, BinaryFormat::Elf);
    }

    #[test]
    fn custom_vendors() {
        // Test various invalid cases.
        assert!(Triple::from_str("x86_64--linux").is_err());
        assert!(Triple::from_str("x86_64-42-linux").is_err());
        assert!(Triple::from_str("x86_64-__customvendor__-linux").is_err());
        assert!(Triple::from_str("x86_64-^-linux").is_err());
        assert!(Triple::from_str("x86_64- -linux").is_err());
        assert!(Triple::from_str("x86_64-CustomVendor-linux").is_err());
        assert!(Triple::from_str("x86_64-linux-linux").is_err());
        assert!(Triple::from_str("x86_64-x86_64-linux").is_err());
        assert!(Triple::from_str("x86_64-elf-linux").is_err());
        assert!(Triple::from_str("x86_64-gnu-linux").is_err());
        assert!(Triple::from_str("x86_64-linux-customvendor").is_err());
        assert!(Triple::from_str("customvendor").is_err());
        assert!(Triple::from_str("customvendor-x86_64").is_err());
        assert!(Triple::from_str("x86_64-").is_err());
        assert!(Triple::from_str("x86_64--").is_err());

        // Test various Unicode things.
        assert!(
            Triple::from_str("x86_64-𝓬𝓾𝓼𝓽𝓸𝓶𝓿𝓮𝓷𝓭𝓸𝓻-linux").is_err(),
            "unicode font hazard"
        );
        assert!(
            Triple::from_str("x86_64-ćúśtőḿvéńdőŕ-linux").is_err(),
            "diacritical mark stripping hazard"
        );
        assert!(
            Triple::from_str("x86_64-customvendοr-linux").is_err(),
            "homoglyph hazard"
        );
        assert!(Triple::from_str("x86_64-customvendor-linux").is_ok());
        assert!(
            Triple::from_str("x86_64-ﬃ-linux").is_err(),
            "normalization hazard"
        );
        assert!(Triple::from_str("x86_64-ffi-linux").is_ok());
        assert!(
            Triple::from_str("x86_64-custom‍vendor-linux").is_err(),
            "zero-width character hazard"
        );
        assert!(
            Triple::from_str("x86_64-﻿customvendor-linux").is_err(),
            "BOM hazard"
        );

        // Test some valid cases.
        let t = Triple::from_str("x86_64-customvendor-linux")
            .expect("can't parse target with custom vendor");
        assert_eq!(t.architecture, Architecture::X86_64);
        assert_eq!(
            t.vendor,
            Vendor::Custom(CustomVendor::Static("customvendor"))
        );
        assert_eq!(t.operating_system, OperatingSystem::Linux);
        assert_eq!(t.environment, Environment::Unknown);
        assert_eq!(t.binary_format, BinaryFormat::Elf);
        assert_eq!(t.to_string(), "x86_64-customvendor-linux");

        let t =
            Triple::from_str("x86_64-customvendor").expect("can't parse target with custom vendor");
        assert_eq!(t.architecture, Architecture::X86_64);
        assert_eq!(
            t.vendor,
            Vendor::Custom(CustomVendor::Static("customvendor"))
        );
        assert_eq!(t.operating_system, OperatingSystem::Unknown);
        assert_eq!(t.environment, Environment::Unknown);
        assert_eq!(t.binary_format, BinaryFormat::Unknown);

        assert_eq!(
            Triple::from_str("unknown-foo"),
            Ok(Triple {
                architecture: Architecture::Unknown,
                vendor: Vendor::Custom(CustomVendor::Static("foo")),
                operating_system: OperatingSystem::Unknown,
                environment: Environment::Unknown,
                binary_format: BinaryFormat::Unknown,
            })
        );
    }
}
