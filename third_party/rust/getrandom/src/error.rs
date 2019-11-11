// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
use core::num::NonZeroU32;
use core::convert::From;
use core::fmt;

// A randomly-chosen 24-bit prefix for our codes
pub(crate) const CODE_PREFIX: u32 = 0x57f4c500;
const CODE_UNKNOWN: u32 = CODE_PREFIX | 0x00;
const CODE_UNAVAILABLE: u32 = CODE_PREFIX | 0x01;

/// The error type.
///
/// This type is small and no-std compatible.
#[derive(Copy, Clone, Eq, PartialEq)]
pub struct Error(pub(crate) NonZeroU32);

impl Error {
    /// An unknown error.
    pub const UNKNOWN: Error = Error(unsafe {
        NonZeroU32::new_unchecked(CODE_UNKNOWN)
    });

    /// No generator is available.
    pub const UNAVAILABLE: Error = Error(unsafe {
        NonZeroU32::new_unchecked(CODE_UNAVAILABLE)
    });

    /// Extract the error code.
    ///
    /// This may equal one of the codes defined in this library or may be a
    /// system error code.
    ///
    /// One may attempt to format this error via the `Display` implementation.
    pub fn code(&self) -> NonZeroU32 {
        self.0
    }

    pub(crate) fn msg(&self) -> Option<&'static str> {
        if let Some(msg) = super::error_msg_inner(self.0) {
            Some(msg)
        } else {
            match *self {
                Error::UNKNOWN => Some("getrandom: unknown error"),
                Error::UNAVAILABLE => Some("getrandom: unavailable"),
                _ => None
            }
        }
    }
}

impl fmt::Debug for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        match self.msg() {
            Some(msg) => write!(f, "Error(\"{}\")", msg),
            None => write!(f, "Error(0x{:08X})", self.0),
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        match self.msg() {
            Some(msg) => write!(f, "{}", msg),
            None => write!(f, "getrandom: unknown code 0x{:08X}", self.0),
        }
    }
}

impl From<NonZeroU32> for Error {
    fn from(code: NonZeroU32) -> Self {
        Error(code)
    }
}

#[cfg(test)]
mod tests {
    use core::mem::size_of;
    use super::Error;

    #[test]
    fn test_size() {
        assert_eq!(size_of::<Error>(), 4);
        assert_eq!(size_of::<Result<(), Error>>(), 4);
    }
}
