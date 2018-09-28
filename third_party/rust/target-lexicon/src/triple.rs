// This file defines the `Triple` type and support code shared by all targets.

use parse_error::ParseError;
use std::borrow::ToOwned;
use std::fmt;
use std::str::FromStr;
use targets::{default_binary_format, Architecture, BinaryFormat, Environment, OperatingSystem,
              Vendor};

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
    pub fn bits(&self) -> u8 {
        match *self {
            PointerWidth::U16 => 16,
            PointerWidth::U32 => 32,
            PointerWidth::U64 => 64,
        }
    }

    /// Return the number of bytes in a pointer.
    ///
    /// For these purposes, there are 8 bits in a byte.
    pub fn bytes(&self) -> u8 {
        match *self {
            PointerWidth::U16 => 2,
            PointerWidth::U32 => 4,
            PointerWidth::U64 => 8,
        }
    }
}

/// A target "triple", because historically such things had three fields, though
/// they've grown more features over time.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct Triple {
    /// The "architecture" (and sometimes the subarchitecture).
    pub architecture: Architecture,
    /// The "vendor" (whatever that means).
    pub vendor: Vendor,
    /// The "operating system" (sometimes also the environment).
    pub operating_system: OperatingSystem,
    /// The "environment" on top of the operating system.
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
        if self.vendor == Vendor::Unknown && self.operating_system == OperatingSystem::Unknown
            && (self.environment != Environment::Unknown
                || self.binary_format != implied_binary_format)
        {
            // "none" is special-case shorthand for unknown vendor and unknown operating system.
            f.write_str("-none")?;
        } else if self.operating_system == OperatingSystem::Linux
            && (self.environment == Environment::Android
                || self.environment == Environment::Androideabi)
        {
            // As a special case, omit the vendor for Android targets.
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
        let mut result = Triple::default();
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
            // "none" is special-case shorthand for unknown vendor and unknown operating system.
            if s == "none" {
                has_operating_system = true;
                has_vendor = true;
                current_part = parts.next();
                // "none" requires an explicit environment or binary format.
                if current_part.is_none() {
                    return Err(ParseError::NoneWithoutBinaryFormat);
                }
            } else if let Ok(vendor) = Vendor::from_str(s) {
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
            Err(if !has_vendor {
                ParseError::UnrecognizedVendor(s.to_owned())
            } else if !has_operating_system {
                ParseError::UnrecognizedOperatingSystem(s.to_owned())
            } else if !has_environment {
                ParseError::UnrecognizedEnvironment(s.to_owned())
            } else if !has_binary_format {
                ParseError::UnrecognizedBinaryFormat(s.to_owned())
            } else {
                ParseError::UnrecognizedField(s.to_owned())
            })
        } else {
            Ok(result)
        }
    }
}

/// A convenient syntax for triple "literals".
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
    }
}
