// This file defines all the identifier enums and target-aware logic.

use crate::triple::{Endianness, PointerWidth, Triple};
use core::fmt;
use core::str::FromStr;

/// The "architecture" field, which in some cases also specifies a specific
/// subarchitecture.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum Architecture {
    Unknown,
    Arm(ArmArchitecture),
    AmdGcn,
    Aarch64(Aarch64Architecture),
    Asmjs,
    Hexagon,
    I386,
    I586,
    I686,
    Mips,
    Mips64,
    Mips64el,
    Mipsel,
    Mipsisa32r6,
    Mipsisa32r6el,
    Mipsisa64r6,
    Mipsisa64r6el,
    Msp430,
    Nvptx64,
    Powerpc,
    Powerpc64,
    Powerpc64le,
    Riscv32,
    Riscv32i,
    Riscv32imac,
    Riscv32imc,
    Riscv64,
    Riscv64gc,
    Riscv64imac,
    S390x,
    Sparc,
    Sparc64,
    Sparcv9,
    Wasm32,
    X86_64,
}

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
    Thumbv6m,
    Thumbv7a,
    Thumbv7em,
    Thumbv7m,
    Thumbv7neon,
    Thumbv8mBase,
    Thumbv8mMain,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum Aarch64Architecture {
    Aarch64,
    Aarch64be,
}

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
    pub fn is_thumb(self) -> bool {
        match self {
            ArmArchitecture::Arm
            | ArmArchitecture::Armeb
            | ArmArchitecture::Armv4
            | ArmArchitecture::Armv4t
            | ArmArchitecture::Armv5t
            | ArmArchitecture::Armv5te
            | ArmArchitecture::Armv5tej
            | ArmArchitecture::Armv6
            | ArmArchitecture::Armv6j
            | ArmArchitecture::Armv6k
            | ArmArchitecture::Armv6z
            | ArmArchitecture::Armv6kz
            | ArmArchitecture::Armv6t2
            | ArmArchitecture::Armv6m
            | ArmArchitecture::Armv7
            | ArmArchitecture::Armv7a
            | ArmArchitecture::Armv7ve
            | ArmArchitecture::Armv7m
            | ArmArchitecture::Armv7r
            | ArmArchitecture::Armv7s
            | ArmArchitecture::Armv8
            | ArmArchitecture::Armv8a
            | ArmArchitecture::Armv8_1a
            | ArmArchitecture::Armv8_2a
            | ArmArchitecture::Armv8_3a
            | ArmArchitecture::Armv8_4a
            | ArmArchitecture::Armv8_5a
            | ArmArchitecture::Armv8mBase
            | ArmArchitecture::Armv8mMain
            | ArmArchitecture::Armv8r
            | ArmArchitecture::Armebv7r => false,
            ArmArchitecture::Thumbeb
            | ArmArchitecture::Thumbv6m
            | ArmArchitecture::Thumbv7a
            | ArmArchitecture::Thumbv7em
            | ArmArchitecture::Thumbv7m
            | ArmArchitecture::Thumbv7neon
            | ArmArchitecture::Thumbv8mBase
            | ArmArchitecture::Thumbv8mMain => true,
        }
    }

    // pub fn has_fpu(self) -> Result<&'static [ArmFpu], ()> {

    // }

    /// Return the pointer bit width of this target's architecture.
    pub fn pointer_width(self) -> PointerWidth {
        match self {
            ArmArchitecture::Arm
            | ArmArchitecture::Armeb
            | ArmArchitecture::Armv4
            | ArmArchitecture::Armv4t
            | ArmArchitecture::Armv5t
            | ArmArchitecture::Armv5te
            | ArmArchitecture::Armv5tej
            | ArmArchitecture::Armv6
            | ArmArchitecture::Armv6j
            | ArmArchitecture::Armv6k
            | ArmArchitecture::Armv6z
            | ArmArchitecture::Armv6kz
            | ArmArchitecture::Armv6t2
            | ArmArchitecture::Armv6m
            | ArmArchitecture::Armv7
            | ArmArchitecture::Armv7a
            | ArmArchitecture::Armv7ve
            | ArmArchitecture::Armv7m
            | ArmArchitecture::Armv7r
            | ArmArchitecture::Armv7s
            | ArmArchitecture::Armv8
            | ArmArchitecture::Armv8a
            | ArmArchitecture::Armv8_1a
            | ArmArchitecture::Armv8_2a
            | ArmArchitecture::Armv8_3a
            | ArmArchitecture::Armv8_4a
            | ArmArchitecture::Armv8_5a
            | ArmArchitecture::Armv8mBase
            | ArmArchitecture::Armv8mMain
            | ArmArchitecture::Armv8r
            | ArmArchitecture::Armebv7r
            | ArmArchitecture::Thumbeb
            | ArmArchitecture::Thumbv6m
            | ArmArchitecture::Thumbv7a
            | ArmArchitecture::Thumbv7em
            | ArmArchitecture::Thumbv7m
            | ArmArchitecture::Thumbv7neon
            | ArmArchitecture::Thumbv8mBase
            | ArmArchitecture::Thumbv8mMain => PointerWidth::U32,
        }
    }

    /// Return the endianness of this architecture.
    pub fn endianness(self) -> Endianness {
        match self {
            ArmArchitecture::Arm
            | ArmArchitecture::Armv4
            | ArmArchitecture::Armv4t
            | ArmArchitecture::Armv5t
            | ArmArchitecture::Armv5te
            | ArmArchitecture::Armv5tej
            | ArmArchitecture::Armv6
            | ArmArchitecture::Armv6j
            | ArmArchitecture::Armv6k
            | ArmArchitecture::Armv6z
            | ArmArchitecture::Armv6kz
            | ArmArchitecture::Armv6t2
            | ArmArchitecture::Armv6m
            | ArmArchitecture::Armv7
            | ArmArchitecture::Armv7a
            | ArmArchitecture::Armv7ve
            | ArmArchitecture::Armv7m
            | ArmArchitecture::Armv7r
            | ArmArchitecture::Armv7s
            | ArmArchitecture::Armv8
            | ArmArchitecture::Armv8a
            | ArmArchitecture::Armv8_1a
            | ArmArchitecture::Armv8_2a
            | ArmArchitecture::Armv8_3a
            | ArmArchitecture::Armv8_4a
            | ArmArchitecture::Armv8_5a
            | ArmArchitecture::Armv8mBase
            | ArmArchitecture::Armv8mMain
            | ArmArchitecture::Armv8r
            | ArmArchitecture::Thumbv6m
            | ArmArchitecture::Thumbv7a
            | ArmArchitecture::Thumbv7em
            | ArmArchitecture::Thumbv7m
            | ArmArchitecture::Thumbv7neon
            | ArmArchitecture::Thumbv8mBase
            | ArmArchitecture::Thumbv8mMain => Endianness::Little,
            ArmArchitecture::Armeb | ArmArchitecture::Armebv7r | ArmArchitecture::Thumbeb => {
                Endianness::Big
            }
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

/// The "vendor" field, which in practice is little more than an arbitrary
/// modifier.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
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
}

/// The "operating system" field, which sometimes implies an environment, and
/// sometimes isn't an actual operating system.
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
    Ios,
    L4re,
    Linux,
    MacOSX { major: u16, minor: u16, patch: u16 },
    Nebulet,
    Netbsd,
    None_,
    Openbsd,
    Redox,
    Solaris,
    Uefi,
    VxWorks,
    Wasi,
    Windows,
}

/// The "environment" field, which specifies an ABI environment on top of the
/// operating system. In many configurations, this field is omitted, and the
/// environment is implied by the operating system.
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
    Musl,
    Musleabi,
    Musleabihf,
    Muslabi64,
    Msvc,
    Uclibc,
    Sgx,
    Spe,
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
    pub fn endianness(self) -> Result<Endianness, ()> {
        match self {
            Architecture::Unknown => Err(()),
            Architecture::Arm(arm) => Ok(arm.endianness()),
            Architecture::Aarch64(aarch) => Ok(aarch.endianness()),
            Architecture::AmdGcn
            | Architecture::Asmjs
            | Architecture::Hexagon
            | Architecture::I386
            | Architecture::I586
            | Architecture::I686
            | Architecture::Mips64el
            | Architecture::Mipsel
            | Architecture::Mipsisa32r6el
            | Architecture::Mipsisa64r6el
            | Architecture::Msp430
            | Architecture::Nvptx64
            | Architecture::Powerpc64le
            | Architecture::Riscv32
            | Architecture::Riscv32i
            | Architecture::Riscv32imac
            | Architecture::Riscv32imc
            | Architecture::Riscv64
            | Architecture::Riscv64gc
            | Architecture::Riscv64imac
            | Architecture::Wasm32
            | Architecture::X86_64 => Ok(Endianness::Little),
            Architecture::Mips
            | Architecture::Mips64
            | Architecture::Mipsisa32r6
            | Architecture::Mipsisa64r6
            | Architecture::Powerpc
            | Architecture::Powerpc64
            | Architecture::S390x
            | Architecture::Sparc
            | Architecture::Sparc64
            | Architecture::Sparcv9 => Ok(Endianness::Big),
        }
    }

    /// Return the pointer bit width of this target's architecture.
    pub fn pointer_width(self) -> Result<PointerWidth, ()> {
        match self {
            Architecture::Unknown => Err(()),
            Architecture::Msp430 => Ok(PointerWidth::U16),
            Architecture::Arm(arm) => Ok(arm.pointer_width()),
            Architecture::Aarch64(aarch) => Ok(aarch.pointer_width()),
            Architecture::Asmjs
            | Architecture::Hexagon
            | Architecture::I386
            | Architecture::I586
            | Architecture::I686
            | Architecture::Mipsel
            | Architecture::Mipsisa32r6
            | Architecture::Mipsisa32r6el
            | Architecture::Riscv32
            | Architecture::Riscv32i
            | Architecture::Riscv32imac
            | Architecture::Riscv32imc
            | Architecture::Sparc
            | Architecture::Wasm32
            | Architecture::Mips
            | Architecture::Powerpc => Ok(PointerWidth::U32),
            Architecture::AmdGcn
            | Architecture::Mips64el
            | Architecture::Mipsisa64r6
            | Architecture::Mipsisa64r6el
            | Architecture::Powerpc64le
            | Architecture::Riscv64
            | Architecture::Riscv64gc
            | Architecture::Riscv64imac
            | Architecture::X86_64
            | Architecture::Mips64
            | Architecture::Nvptx64
            | Architecture::Powerpc64
            | Architecture::S390x
            | Architecture::Sparc64
            | Architecture::Sparcv9 => Ok(PointerWidth::U64),
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
        OperatingSystem::Darwin | OperatingSystem::Ios | OperatingSystem::MacOSX { .. } => {
            BinaryFormat::Macho
        }
        OperatingSystem::Windows => BinaryFormat::Coff,
        OperatingSystem::Nebulet
        | OperatingSystem::Emscripten
        | OperatingSystem::VxWorks
        | OperatingSystem::Wasi
        | OperatingSystem::Unknown => match triple.architecture {
            Architecture::Wasm32 => BinaryFormat::Wasm,
            _ => BinaryFormat::Unknown,
        },
        _ => BinaryFormat::Elf,
    }
}

impl fmt::Display for ArmArchitecture {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let s = match *self {
            ArmArchitecture::Arm => "arm",
            ArmArchitecture::Armeb => "armeb",
            ArmArchitecture::Armv4 => "armv4",
            ArmArchitecture::Armv4t => "armv4t",
            ArmArchitecture::Armv5t => "armv5t",
            ArmArchitecture::Armv5te => "armv5te",
            ArmArchitecture::Armv5tej => "armv5tej",
            ArmArchitecture::Armv6 => "armv6",
            ArmArchitecture::Armv6j => "armv6j",
            ArmArchitecture::Armv6k => "armv6k",
            ArmArchitecture::Armv6z => "armv6z",
            ArmArchitecture::Armv6kz => "armv6kz",
            ArmArchitecture::Armv6t2 => "armv6t2",
            ArmArchitecture::Armv6m => "armv6m",
            ArmArchitecture::Armv7 => "armv7",
            ArmArchitecture::Armv7a => "armv7a",
            ArmArchitecture::Armv7ve => "armv7ve",
            ArmArchitecture::Armv7m => "armv7m",
            ArmArchitecture::Armv7r => "armv7r",
            ArmArchitecture::Armv7s => "armv7s",
            ArmArchitecture::Armv8 => "armv8",
            ArmArchitecture::Armv8a => "armv8a",
            ArmArchitecture::Armv8_1a => "armv8.1a",
            ArmArchitecture::Armv8_2a => "armv8.2a",
            ArmArchitecture::Armv8_3a => "armv8.3a",
            ArmArchitecture::Armv8_4a => "armv8.4a",
            ArmArchitecture::Armv8_5a => "armv8.5a",
            ArmArchitecture::Armv8mBase => "armv8m.base",
            ArmArchitecture::Armv8mMain => "armv8m.main",
            ArmArchitecture::Armv8r => "armv8r",
            ArmArchitecture::Thumbeb => "thumbeb",
            ArmArchitecture::Thumbv6m => "thumbv6m",
            ArmArchitecture::Thumbv7a => "thumbv7a",
            ArmArchitecture::Thumbv7em => "thumbv7em",
            ArmArchitecture::Thumbv7m => "thumbv7m",
            ArmArchitecture::Thumbv7neon => "thumbv7neon",
            ArmArchitecture::Thumbv8mBase => "thumbv8m.base",
            ArmArchitecture::Thumbv8mMain => "thumbv8m.main",
            ArmArchitecture::Armebv7r => "armebv7r",
        };
        f.write_str(s)
    }
}

impl fmt::Display for Aarch64Architecture {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let s = match *self {
            Aarch64Architecture::Aarch64 => "aarch64",
            Aarch64Architecture::Aarch64be => "aarch64be",
        };
        f.write_str(s)
    }
}

impl fmt::Display for Architecture {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            Architecture::Arm(arm) => arm.fmt(f),
            Architecture::Aarch64(aarch) => aarch.fmt(f),
            Architecture::Unknown => f.write_str("unknown"),
            Architecture::AmdGcn => f.write_str("amdgcn"),
            Architecture::Asmjs => f.write_str("asmjs"),
            Architecture::Hexagon => f.write_str("hexagon"),
            Architecture::I386 => f.write_str("i386"),
            Architecture::I586 => f.write_str("i586"),
            Architecture::I686 => f.write_str("i686"),
            Architecture::Mips => f.write_str("mips"),
            Architecture::Mips64 => f.write_str("mips64"),
            Architecture::Mips64el => f.write_str("mips64el"),
            Architecture::Mipsel => f.write_str("mipsel"),
            Architecture::Mipsisa32r6 => f.write_str("mipsisa32r6"),
            Architecture::Mipsisa32r6el => f.write_str("mipsisa32r6el"),
            Architecture::Mipsisa64r6 => f.write_str("mipsisa64r6"),
            Architecture::Mipsisa64r6el => f.write_str("mipsisa64r6el"),
            Architecture::Msp430 => f.write_str("msp430"),
            Architecture::Nvptx64 => f.write_str("nvptx64"),
            Architecture::Powerpc => f.write_str("powerpc"),
            Architecture::Powerpc64 => f.write_str("powerpc64"),
            Architecture::Powerpc64le => f.write_str("powerpc64le"),
            Architecture::Riscv32 => f.write_str("riscv32"),
            Architecture::Riscv32i => f.write_str("riscv32i"),
            Architecture::Riscv32imac => f.write_str("riscv32imac"),
            Architecture::Riscv32imc => f.write_str("riscv32imc"),
            Architecture::Riscv64 => f.write_str("riscv64"),
            Architecture::Riscv64gc => f.write_str("riscv64gc"),
            Architecture::Riscv64imac => f.write_str("riscv64imac"),
            Architecture::S390x => f.write_str("s390x"),
            Architecture::Sparc => f.write_str("sparc"),
            Architecture::Sparc64 => f.write_str("sparc64"),
            Architecture::Sparcv9 => f.write_str("sparcv9"),
            Architecture::Wasm32 => f.write_str("wasm32"),
            Architecture::X86_64 => f.write_str("x86_64"),
        }
    }
}

impl FromStr for ArmArchitecture {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        Ok(match s {
            "arm" => ArmArchitecture::Arm,
            "armeb" => ArmArchitecture::Armeb,
            "armv4" => ArmArchitecture::Armv4,
            "armv4t" => ArmArchitecture::Armv4t,
            "armv5t" => ArmArchitecture::Armv5t,
            "armv5te" => ArmArchitecture::Armv5te,
            "armv5tej" => ArmArchitecture::Armv5tej,
            "armv6" => ArmArchitecture::Armv6,
            "armv6j" => ArmArchitecture::Armv6j,
            "armv6k" => ArmArchitecture::Armv6k,
            "armv6z" => ArmArchitecture::Armv6z,
            "armv6kz" => ArmArchitecture::Armv6kz,
            "armv6t2" => ArmArchitecture::Armv6t2,
            "armv6m" => ArmArchitecture::Armv6m,
            "armv7" => ArmArchitecture::Armv7,
            "armv7a" => ArmArchitecture::Armv7a,
            "armv7ve" => ArmArchitecture::Armv7ve,
            "armv7m" => ArmArchitecture::Armv7m,
            "armv7r" => ArmArchitecture::Armv7r,
            "armv7s" => ArmArchitecture::Armv7s,
            "armv8" => ArmArchitecture::Armv8,
            "armv8a" => ArmArchitecture::Armv8a,
            "armv8.1a" => ArmArchitecture::Armv8_1a,
            "armv8.2a" => ArmArchitecture::Armv8_2a,
            "armv8.3a" => ArmArchitecture::Armv8_3a,
            "armv8.4a" => ArmArchitecture::Armv8_4a,
            "armv8.5a" => ArmArchitecture::Armv8_5a,
            "armv8m.base" => ArmArchitecture::Armv8mBase,
            "armv8m.main" => ArmArchitecture::Armv8mMain,
            "armv8r" => ArmArchitecture::Armv8r,
            "thumbeb" => ArmArchitecture::Thumbeb,
            "thumbv6m" => ArmArchitecture::Thumbv6m,
            "thumbv7a" => ArmArchitecture::Thumbv7a,
            "thumbv7em" => ArmArchitecture::Thumbv7em,
            "thumbv7m" => ArmArchitecture::Thumbv7m,
            "thumbv7neon" => ArmArchitecture::Thumbv7neon,
            "thumbv8m.base" => ArmArchitecture::Thumbv8mBase,
            "thumbv8m.main" => ArmArchitecture::Thumbv8mMain,
            "armebv7r" => ArmArchitecture::Armebv7r,
            _ => return Err(()),
        })
    }
}

impl FromStr for Aarch64Architecture {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        Ok(match s {
            "aarch64" => Aarch64Architecture::Aarch64,
            "arm64" => Aarch64Architecture::Aarch64,
            "aarch64be" => Aarch64Architecture::Aarch64be,
            _ => return Err(()),
        })
    }
}

impl FromStr for Architecture {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        Ok(match s {
            "unknown" => Architecture::Unknown,
            "amdgcn" => Architecture::AmdGcn,
            "asmjs" => Architecture::Asmjs,
            "hexagon" => Architecture::Hexagon,
            "i386" => Architecture::I386,
            "i586" => Architecture::I586,
            "i686" => Architecture::I686,
            "mips" => Architecture::Mips,
            "mips64" => Architecture::Mips64,
            "mips64el" => Architecture::Mips64el,
            "mipsel" => Architecture::Mipsel,
            "mipsisa32r6" => Architecture::Mipsisa32r6,
            "mipsisa32r6el" => Architecture::Mipsisa32r6el,
            "mipsisa64r6" => Architecture::Mipsisa64r6,
            "mipsisa64r6el" => Architecture::Mipsisa64r6el,
            "msp430" => Architecture::Msp430,
            "nvptx64" => Architecture::Nvptx64,
            "powerpc" => Architecture::Powerpc,
            "powerpc64" => Architecture::Powerpc64,
            "powerpc64le" => Architecture::Powerpc64le,
            "riscv32" => Architecture::Riscv32,
            "riscv32i" => Architecture::Riscv32i,
            "riscv32imac" => Architecture::Riscv32imac,
            "riscv32imc" => Architecture::Riscv32imc,
            "riscv64" => Architecture::Riscv64,
            "riscv64gc" => Architecture::Riscv64gc,
            "riscv64imac" => Architecture::Riscv64imac,
            "s390x" => Architecture::S390x,
            "sparc" => Architecture::Sparc,
            "sparc64" => Architecture::Sparc64,
            "sparcv9" => Architecture::Sparcv9,
            "wasm32" => Architecture::Wasm32,
            "x86_64" => Architecture::X86_64,
            _ => {
                if let Ok(arm) = ArmArchitecture::from_str(s) {
                    Architecture::Arm(arm)
                } else if let Ok(aarch64) = Aarch64Architecture::from_str(s) {
                    Architecture::Aarch64(aarch64)
                } else {
                    return Err(());
                }
            }
        })
    }
}

impl fmt::Display for Vendor {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let s = match *self {
            Vendor::Unknown => "unknown",
            Vendor::Amd => "amd",
            Vendor::Apple => "apple",
            Vendor::Experimental => "experimental",
            Vendor::Fortanix => "fortanix",
            Vendor::Nvidia => "nvidia",
            Vendor::Pc => "pc",
            Vendor::Rumprun => "rumprun",
            Vendor::Sun => "sun",
            Vendor::Uwp => "uwp",
            Vendor::Wrs => "wrs",
        };
        f.write_str(s)
    }
}

impl FromStr for Vendor {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        Ok(match s {
            "unknown" => Vendor::Unknown,
            "amd" => Vendor::Amd,
            "apple" => Vendor::Apple,
            "experimental" => Vendor::Experimental,
            "fortanix" => Vendor::Fortanix,
            "nvidia" => Vendor::Nvidia,
            "pc" => Vendor::Pc,
            "rumprun" => Vendor::Rumprun,
            "sun" => Vendor::Sun,
            "uwp" => Vendor::Uwp,
            "wrs" => Vendor::Wrs,
            _ => return Err(()),
        })
    }
}

impl fmt::Display for OperatingSystem {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let s = match *self {
            OperatingSystem::Unknown => "unknown",
            OperatingSystem::AmdHsa => "amdhsa",
            OperatingSystem::Bitrig => "bitrig",
            OperatingSystem::Cloudabi => "cloudabi",
            OperatingSystem::Cuda => "cuda",
            OperatingSystem::Darwin => "darwin",
            OperatingSystem::Dragonfly => "dragonfly",
            OperatingSystem::Emscripten => "emscripten",
            OperatingSystem::Freebsd => "freebsd",
            OperatingSystem::Fuchsia => "fuchsia",
            OperatingSystem::Haiku => "haiku",
            OperatingSystem::Hermit => "hermit",
            OperatingSystem::Ios => "ios",
            OperatingSystem::L4re => "l4re",
            OperatingSystem::Linux => "linux",
            OperatingSystem::MacOSX {
                major,
                minor,
                patch,
            } => {
                return write!(f, "macosx{}.{}.{}", major, minor, patch);
            }
            OperatingSystem::Nebulet => "nebulet",
            OperatingSystem::Netbsd => "netbsd",
            OperatingSystem::None_ => "none",
            OperatingSystem::Openbsd => "openbsd",
            OperatingSystem::Redox => "redox",
            OperatingSystem::Solaris => "solaris",
            OperatingSystem::Uefi => "uefi",
            OperatingSystem::VxWorks => "vxworks",
            OperatingSystem::Wasi => "wasi",
            OperatingSystem::Windows => "windows",
        };
        f.write_str(s)
    }
}

impl FromStr for OperatingSystem {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
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

            return Ok(OperatingSystem::MacOSX {
                major,
                minor,
                patch,
            });
        }

        Ok(match s {
            "unknown" => OperatingSystem::Unknown,
            "amdhsa" => OperatingSystem::AmdHsa,
            "bitrig" => OperatingSystem::Bitrig,
            "cloudabi" => OperatingSystem::Cloudabi,
            "cuda" => OperatingSystem::Cuda,
            "darwin" => OperatingSystem::Darwin,
            "dragonfly" => OperatingSystem::Dragonfly,
            "emscripten" => OperatingSystem::Emscripten,
            "freebsd" => OperatingSystem::Freebsd,
            "fuchsia" => OperatingSystem::Fuchsia,
            "haiku" => OperatingSystem::Haiku,
            "hermit" => OperatingSystem::Hermit,
            "ios" => OperatingSystem::Ios,
            "l4re" => OperatingSystem::L4re,
            "linux" => OperatingSystem::Linux,
            "nebulet" => OperatingSystem::Nebulet,
            "netbsd" => OperatingSystem::Netbsd,
            "none" => OperatingSystem::None_,
            "openbsd" => OperatingSystem::Openbsd,
            "redox" => OperatingSystem::Redox,
            "solaris" => OperatingSystem::Solaris,
            "uefi" => OperatingSystem::Uefi,
            "vxworks" => OperatingSystem::VxWorks,
            "wasi" => OperatingSystem::Wasi,
            "windows" => OperatingSystem::Windows,
            _ => return Err(()),
        })
    }
}

impl fmt::Display for Environment {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let s = match *self {
            Environment::Unknown => "unknown",
            Environment::AmdGiz => "amdgiz",
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
            Environment::Muslabi64 => "muslabi64",
            Environment::Msvc => "msvc",
            Environment::Uclibc => "uclibc",
            Environment::Sgx => "sgx",
            Environment::Spe => "spe",
        };
        f.write_str(s)
    }
}

impl FromStr for Environment {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, ()> {
        Ok(match s {
            "unknown" => Environment::Unknown,
            "amdgiz" => Environment::AmdGiz,
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
            "muslabi64" => Environment::Muslabi64,
            "msvc" => Environment::Msvc,
            "uclibc" => Environment::Uclibc,
            "sgx" => Environment::Sgx,
            "spe" => Environment::Spe,
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
    use alloc::string::ToString;

    #[test]
    fn roundtrip_known_triples() {
        // This list is constructed from:
        //  - targets emitted by "rustup target list"
        //  - targets emitted by "rustc +nightly --print target-list"
        //  - targets contributors have added
        let targets = [
            "aarch64-apple-ios",
            "aarch64-fuchsia",
            "aarch64-linux-android",
            "aarch64-pc-windows-msvc",
            "aarch64-unknown-cloudabi",
            "aarch64-unknown-freebsd",
            "aarch64-unknown-hermit",
            "aarch64-unknown-linux-gnu",
            "aarch64-unknown-linux-musl",
            "aarch64-unknown-netbsd",
            "aarch64-unknown-none",
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
            "armv4t-unknown-linux-gnueabi",
            "armv5te-unknown-linux-gnueabi",
            "armv5te-unknown-linux-musleabi",
            "armv6-unknown-freebsd",
            "armv6-unknown-netbsd-eabihf",
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
            "asmjs-unknown-emscripten",
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
            "i686-uwp-windows-gnu",
            "i686-uwp-windows-msvc",
            "i686-wrs-vxworks",
            "mips64el-unknown-linux-gnuabi64",
            "mips64el-unknown-linux-muslabi64",
            "mips64-unknown-linux-gnuabi64",
            "mips64-unknown-linux-muslabi64",
            "mipsel-unknown-linux-gnu",
            "mipsel-unknown-linux-musl",
            "mipsel-unknown-linux-uclibc",
            "mipsisa32r6el-unknown-linux-gnu",
            "mipsisa32r6-unknown-linux-gnu",
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
            "riscv32imac-unknown-none-elf",
            "riscv32imc-unknown-none-elf",
            "riscv32i-unknown-none-elf",
            "riscv64gc-unknown-none-elf",
            "riscv64imac-unknown-none-elf",
            "s390x-unknown-linux-gnu",
            "sparc64-unknown-linux-gnu",
            "sparc64-unknown-netbsd",
            "sparc64-unknown-openbsd",
            "sparc-unknown-linux-gnu",
            "sparcv9-sun-solaris",
            "thumbv6m-none-eabi",
            "thumbv7a-pc-windows-msvc",
            "thumbv7em-none-eabi",
            "thumbv7em-none-eabihf",
            "thumbv7m-none-eabi",
            "thumbv7neon-linux-androideabi",
            "thumbv7neon-unknown-linux-gnueabihf",
            "thumbv8m.base-none-eabi",
            "thumbv8m.main-none-eabi",
            "thumbv8m.main-none-eabihf",
            "wasm32-experimental-emscripten",
            "wasm32-unknown-emscripten",
            "wasm32-unknown-unknown",
            "wasm32-wasi",
            "x86_64-apple-darwin",
            "x86_64-apple-ios",
            "x86_64-fortanix-unknown-sgx",
            "x86_64-fuchsia",
            "x86_64-linux-android",
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
}
