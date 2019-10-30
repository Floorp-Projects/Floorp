//! Macros and utilities.

/// Small Ascii String. Used to write errors in `no_std` environments.
pub struct TinyAsciiString {
    /// A buffer for the ascii string
    buf: [u8; 512],
}

impl TinyAsciiString {
    /// Creates a new string initialized to zero.
    pub fn new() -> Self {
        Self { buf: [0_u8; 512] }
    }
    /// Converts the Tiny Ascii String to an UTF-8 string (unchecked).
    pub unsafe fn as_str(&self) -> &str {
        crate::str::from_utf8_unchecked(&self.buf)
    }
}

impl crate::fmt::Write for TinyAsciiString {
    fn write_str(&mut self, s: &str) -> Result<(), crate::fmt::Error> {
        for (idx, b) in s.bytes().enumerate() {
            if let Some(v) = self.buf.get_mut(idx) {
                *v = b;
            } else {
                return Err(crate::fmt::Error);
            }
        }
        Ok(())
    }
}

macro_rules! tiny_str {
    ($($t:tt)*) => (
        {
            use crate::fmt::Write;
            let mut s: crate::macros::TinyAsciiString
                = crate::macros::TinyAsciiString::new();
            write!(&mut s, $($t)*).unwrap();
            s
        }
    )
}
