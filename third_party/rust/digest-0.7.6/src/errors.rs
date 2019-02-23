use core::fmt;
#[cfg(feature = "std")]
use std::error;

/// The error type for variable hasher initialization
#[derive(Clone, Copy, Debug, Default)]
pub struct InvalidOutputSize;

/// The error type for variable hasher result
#[derive(Clone, Copy, Debug, Default)]
pub struct InvalidBufferLength;

impl fmt::Display for InvalidOutputSize {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("invalid output size")
    }
}

impl fmt::Display for InvalidBufferLength {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("invalid buffer length")
    }
}

#[cfg(feature = "std")]
impl error::Error for InvalidOutputSize {
    fn description(&self) -> &str {
        "invalid output size"
    }
}

#[cfg(feature = "std")]
impl error::Error for InvalidBufferLength {
    fn description(&self) -> &str {
        "invalid buffer size"
    }
}
