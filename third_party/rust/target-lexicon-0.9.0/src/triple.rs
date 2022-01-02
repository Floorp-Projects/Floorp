// This file defines the `Triple` type and support code shared by all targets.

use crate::parse_error::ParseError;
use crate::targets::{
    default_binary_format, Architecture, ArmArchitecture, BinaryFormat, Environment,
    OperatingSystem, Vendor,
};
use alloc::borrow::ToOwned;
use core::fmt;
use core::str::FromStr;

/// The target memory endianness.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum Endianness {
    Little,
    Big,
}

/// The width of a pointer (in the default address space).
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum PointerWidth {
    U16,
    U32,
    U64,
}

impl PointerWidth {
    /// Return the number of bits in a pointer.
    pub fn bits(self) -> u8 {
        match self {
            PointerWidth::U16 => 16,
            PointerWidth::U32 => 32,
            PointerWidth::U64 => 64,
        }
    }

    /// Return the number of bytes in a pointer.
    ///
    /// For these purposes, there are 8 bits in a byte.
    pub fn bytes(self) -> u8 {
        match self {
            PointerWidth::U16 => 2,
            PointerWidth::U32 => 4,
            PointerWidth::U64 => 8,
        }
    }
}

/// The calling convention, which specifies things like which registers are
/// used for passing arguments, which registers are callee-saved, and so on.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum CallingConvention {
    /// "System V", which is used on most Unix-like platfoms. Note that the
    /// specific conventions vary between hardware architectures; for example,
    /// x86-32's "System V" is entirely different from x86-64's "System V".
    SystemV,

    /// The WebAssembly C ABI.
    /// https://github.com/WebAssembly/tool-conventions/blob/master/BasicCABI.md
    WasmBasicCAbi,

    /// "Windows Fastcall", which is used on Windows. Note that like "System V",
    /// this varies between hardware architectures. On x86-32 it describes what
    /// Windows documentation calls "fastcall", and on x86-64 it describes what
    /// Windows documentation often just calls the Windows x64 calling convention
    /// (though the compiler still recognizes "fastcall" as an alias for it).
    WindowsFastcall,
}

/// A target "triple". Historically such things had three fields, though they've
/// added additional fields over time.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct Triple {
    /// The "architecture" (and sometimes the subarchitecture).
    pub architecture: Architecture,
    /// The "vendor" (whatever that means).
    pub vendor: Vendor,
    /// The "operating system" (sometimes also the environment).
    pub operating_system: OperatingSystem,
    /// The "environment" on top of the operating system (often omitted for
    /// operating systems with a single predominant environment).
    pub environment: Environment,
    /// The "binary format" (rarely used).
    pub binary_format: BinaryFormat,
}

impl Triple {
    /// Return the endianness of this target's architecture.
    pub fn endianness(&self) -> Result<Endianness, ()> {
        self.architecture.endianness()
    }

    /// Return the pointer width of this target's architecture.
    pub fn pointer_width(&self) -> Result<PointerWidth, ()> {
        self.architecture.pointer_width()
    }

    /// Return the default calling convention for the given target triple.
    pub fn default_calling_convention(&self) -> Result<CallingConvention, ()> {
        Ok(match self.operating_system {
            OperatingSystem::Bitrig
            | OperatingSystem::Cloudabi
            | OperatingSystem::Darwin
            | OperatingSystem::Dragonfly
            | OperatingSystem::Freebsd
            | OperatingSystem::Fuchsia
            | OperatingSystem::Haiku
            | OperatingSystem::Ios
            | OperatingSystem::L4re
            | OperatingSystem::Linux
            | OperatingSystem::MacOSX { .. }
            | OperatingSystem::Netbsd
            | OperatingSystem::Openbsd
            | OperatingSystem::Redox
            | OperatingSystem::Solaris => CallingConvention::SystemV,
            OperatingSystem::Windows => CallingConvention::WindowsFastcall,
            OperatingSystem::Nebulet
            | OperatingSystem::Emscripten
            | OperatingSystem::Wasi
            | OperatingSystem::Unknown => match self.architecture {
                Architecture::Wasm32 => CallingConvention::WasmBasicCAbi,
                _ => return Err(()),
            },
            _ => return Err(()),
        })
    }
}

impl Default for Triple {
    fn default() -> Self {
        Self {
            architecture: Architecture::Unknown,
            vendor: Vendor::Unknown,
            operating_system: OperatingSystem::Unknown,
            environment: Environment::Unknown,
            binary_format: BinaryFormat::Unknown,
        }
    }
}

impl Default for Architecture {
    fn default() -> Self {
        Architecture::Unknown
    }
}

impl Default for Vendor {
    fn default() -> Self {
        Vendor::Unknown
    }
}

impl Default for OperatingSystem {
    fn default() -> Self {
        OperatingSystem::Unknown
    }
}

impl Default for Environment {
    fn default() -> Self {
        Environment::Unknown
    }
}

impl Default for BinaryFormat {
    fn default() -> Self {
        BinaryFormat::Unknown
    }
}

impl fmt::Display for Triple {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let implied_binary_format = default_binary_format(&self);

        write!(f, "{}", self.architecture)?;
        if self.vendor == Vendor::Unknown
            && ((self.operating_system == OperatingSystem::Linux
                && (self.environment == Environment::Android
                    || self.environment == Environment::Androideabi
                    || self.environment == Environment::Kernel))
                || self.operating_system == OperatingSystem::Fuchsia
                || self.operating_system == OperatingSystem::Wasi
                || (self.operating_system == OperatingSystem::None_
                    && (self.architecture == Architecture::Arm(ArmArchitecture::Armebv7r)
                        || self.architecture == Architecture::Arm(ArmArchitecture::Armv7r)
                        || self.architecture == Architecture::Arm(ArmArchitecture::Thumbv6m)
                        || self.architecture == Architecture::Arm(ArmArchitecture::Thumbv7em)
                        || self.architecture == Architecture::Arm(ArmArchitecture::Thumbv7m)
                        || self.architecture == Architecture::Arm(ArmArchitecture::Thumbv8mBase)
                        || self.architecture == Architecture::Arm(ArmArchitecture::Thumbv8mMain)
                        || self.architecture == Architecture::Msp430
                        || self.architecture == Architecture::X86_64)))
        {
            // As a special case, omit the vendor for Android, Fuchsia, Wasi, and sometimes
            // None_, depending on the hardware architecture. This logic is entirely
            // ad-hoc, and is just sufficient to handle the current set of recognized
            // triples.
            write!(f, "-{}", self.operating_system)?;
        } else {
            write!(f, "-{}-{}", self.vendor, self.operating_system)?;
        }
        if self.environment != Environment::Unknown {
            write!(f, "-{}", self.environment)?;
        }

        if self.binary_format != implied_binary_format {
            write!(f, "-{}", self.binary_format)?;
        }
        Ok(())
    }
}

impl FromStr for Triple {
    type Err = ParseError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let mut parts = s.split('-');
        let mut result = Self::default();
        let mut current_part;

        current_part = parts.next();
        if let Some(s) = current_part {
            if let Ok(architecture) = Architecture::from_str(s) {
                result.architecture = architecture;
                current_part = parts.next();
            } else {
                // Insist that the triple start with a valid architecture.
                return Err(ParseError::UnrecognizedArchitecture(s.to_owned()));
            }
        }

        let mut has_vendor = false;
        let mut has_operating_system = false;
        if let Some(s) = current_part {
            if let Ok(vendor) = Vendor::from_str(s) {
                has_vendor = true;
                result.vendor = vendor;
                current_part = parts.next();
            }
        }

        if !has_operating_system {
            if let Some(s) = current_part {
                if let Ok(operating_system) = OperatingSystem::from_str(s) {
                    has_operating_system = true;
                    result.operating_system = operating_system;
                    current_part = parts.next();
                }
            }
        }

        let mut has_environment = false;
        if let Some(s) = current_part {
            if let Ok(environment) = Environment::from_str(s) {
                has_environment = true;
                result.environment = environment;
                current_part = parts.next();
            }
        }

        let mut has_binary_format = false;
        if let Some(s) = current_part {
            if let Ok(binary_format) = BinaryFormat::from_str(s) {
                has_binary_format = true;
                result.binary_format = binary_format;
                current_part = parts.next();
            }
        }

        // The binary format is frequently omitted; if that's the case here,
        // infer it from the other fields.
        if !has_binary_format {
            result.binary_format = default_binary_format(&result);
        }

        if let Some(s) = current_part {
            Err(
                if !has_vendor && !has_operating_system && !has_environment && !has_binary_format {
                    ParseError::UnrecognizedVendor(s.to_owned())
                } else if !has_operating_system && !has_environment && !has_binary_format {
                    ParseError::UnrecognizedOperatingSystem(s.to_owned())
                } else if !has_environment && !has_binary_format {
                    ParseError::UnrecognizedEnvironment(s.to_owned())
                } else if !has_binary_format {
                    ParseError::UnrecognizedBinaryFormat(s.to_owned())
                } else {
                    ParseError::UnrecognizedField(s.to_owned())
                },
            )
        } else {
            Ok(result)
        }
    }
}

/// A convenient syntax for triple literals.
///
/// This currently expands to code that just calls `Triple::from_str` and does
/// an `expect`, though in the future it would be cool to use procedural macros
/// or so to report errors at compile time instead.
#[macro_export]
macro_rules! triple {
    ($str:tt) => {
        target_lexicon::Triple::from_str($str).expect("invalid triple literal")
    };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_errors() {
        assert_eq!(
            Triple::from_str(""),
            Err(ParseError::UnrecognizedArchitecture("".to_owned()))
        );
        assert_eq!(
            Triple::from_str("foo"),
            Err(ParseError::UnrecognizedArchitecture("foo".to_owned()))
        );
        assert_eq!(
            Triple::from_str("unknown-foo"),
            Err(ParseError::UnrecognizedVendor("foo".to_owned()))
        );
        assert_eq!(
            Triple::from_str("unknown-unknown-foo"),
            Err(ParseError::UnrecognizedOperatingSystem("foo".to_owned()))
        );
        assert_eq!(
            Triple::from_str("unknown-unknown-unknown-foo"),
            Err(ParseError::UnrecognizedEnvironment("foo".to_owned()))
        );
        assert_eq!(
            Triple::from_str("unknown-unknown-unknown-unknown-foo"),
            Err(ParseError::UnrecognizedBinaryFormat("foo".to_owned()))
        );
        assert_eq!(
            Triple::from_str("unknown-unknown-unknown-unknown-unknown-foo"),
            Err(ParseError::UnrecognizedField("foo".to_owned()))
        );
    }

    #[test]
    fn defaults() {
        assert_eq!(
            Triple::from_str("unknown-unknown-unknown"),
            Ok(Triple::default())
        );
        assert_eq!(
            Triple::from_str("unknown-unknown-unknown-unknown"),
            Ok(Triple::default())
        );
        assert_eq!(
            Triple::from_str("unknown-unknown-unknown-unknown-unknown"),
            Ok(Triple::default())
        );
    }

    #[test]
    fn unknown_properties() {
        assert_eq!(Triple::default().endianness(), Err(()));
        assert_eq!(Triple::default().pointer_width(), Err(()));
        assert_eq!(Triple::default().default_calling_convention(), Err(()));
    }
}
