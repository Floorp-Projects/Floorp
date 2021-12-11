use core::fmt;
#[cfg(feature = "std")]
use std::error;

/// The error type for variable hasher initialization
#[derive(Clone, Copy, Debug, Default)]
pub struct InvalidOutputSize;

impl fmt::Display for InvalidOutputSize {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("invalid output size")
    }
}

#[cfg(feature = "std")]
impl error::Error for InvalidOutputSize {
    fn description(&self) -> &str {
        "invalid output size"
    }
}
