use core::fmt;

/// Alternative implementation of `fmt::Debug` for byte slice.
///
/// Standard `Debug` implementation for `[u8]` is comma separated
/// list of numbers. Since large amount of byte strings are in fact
/// ASCII strings or contain a lot of ASCII strings (e. g. HTTP),
/// it is convenient to print strings as ASCII when possible.
///
/// This struct wraps `&[u8]` just to override `fmt::Debug`.
///
/// `BsDebug` is not a part of public API of bytes crate.
pub struct BsDebug<'a>(pub &'a [u8]);

impl fmt::Debug for BsDebug<'_> {
    fn fmt(&self, fmt: &mut fmt::Formatter<'_>) -> Result<(), fmt::Error> {
        write!(fmt, "b\"")?;
        for &c in self.0 {
            // https://doc.rust-lang.org/reference.html#byte-escapes
            if c == b'\n' {
                write!(fmt, "\\n")?;
            } else if c == b'\r' {
                write!(fmt, "\\r")?;
            } else if c == b'\t' {
                write!(fmt, "\\t")?;
            } else if c == b'\\' || c == b'"' {
                write!(fmt, "\\{}", c as char)?;
            } else if c == b'\0' {
                write!(fmt, "\\0")?;
            // ASCII printable
            } else if c >= 0x20 && c < 0x7f {
                write!(fmt, "{}", c as char)?;
            } else {
                write!(fmt, "\\x{:02x}", c)?;
            }
        }
        write!(fmt, "\"")?;
        Ok(())
    }
}
