use core::fmt::{Debug, Formatter, Result};

use super::BytesRef;
use crate::{Bytes, BytesMut};

/// Alternative implementation of `std::fmt::Debug` for byte slice.
///
/// Standard `Debug` implementation for `[u8]` is comma separated
/// list of numbers. Since large amount of byte strings are in fact
/// ASCII strings or contain a lot of ASCII strings (e. g. HTTP),
/// it is convenient to print strings as ASCII when possible.
impl Debug for BytesRef<'_> {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result {
        write!(f, "b\"")?;
        for &b in self.0 {
            // https://doc.rust-lang.org/reference/tokens.html#byte-escapes
            if b == b'\n' {
                write!(f, "\\n")?;
            } else if b == b'\r' {
                write!(f, "\\r")?;
            } else if b == b'\t' {
                write!(f, "\\t")?;
            } else if b == b'\\' || b == b'"' {
                write!(f, "\\{}", b as char)?;
            } else if b == b'\0' {
                write!(f, "\\0")?;
            // ASCII printable
            } else if b >= 0x20 && b < 0x7f {
                write!(f, "{}", b as char)?;
            } else {
                write!(f, "\\x{:02x}", b)?;
            }
        }
        write!(f, "\"")?;
        Ok(())
    }
}

impl Debug for Bytes {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result {
        Debug::fmt(&BytesRef(&self.as_ref()), f)
    }
}

impl Debug for BytesMut {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result {
        Debug::fmt(&BytesRef(&self.as_ref()), f)
    }
}
