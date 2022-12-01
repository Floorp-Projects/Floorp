#[cfg(feature = "alloc")]
use alloc::vec::Vec;
#[cfg(not(feature = "std"))]
use core::fmt;
#[cfg(feature = "std")]
use std::io;

use crate::error;

#[cfg(not(feature = "unsealed_read_write"))]
/// A sink for serialized CBOR.
///
/// This trait is similar to the [`Write`]() trait in the standard library,
/// but has a smaller and more general API.
///
/// Any object implementing `std::io::Write`
/// can be wrapped in an [`IoWrite`](../write/struct.IoWrite.html) that implements
/// this trait for the underlying object.
pub trait Write: private::Sealed {
    /// The type of error returned when a write operation fails.
    #[doc(hidden)]
    type Error: Into<error::Error>;

    /// Attempts to write an entire buffer into this write.
    #[doc(hidden)]
    fn write_all(&mut self, buf: &[u8]) -> Result<(), Self::Error>;
}

#[cfg(feature = "unsealed_read_write")]
/// A sink for serialized CBOR.
///
/// This trait is similar to the [`Write`]() trait in the standard library,
/// but has a smaller and more general API.
///
/// Any object implementing `std::io::Write`
/// can be wrapped in an [`IoWrite`](../write/struct.IoWrite.html) that implements
/// this trait for the underlying object.
///
/// This trait is sealed by default, enabling the `unsealed_read_write` feature removes this bound
/// to allow objects outside of this crate to implement this trait.
pub trait Write {
    /// The type of error returned when a write operation fails.
    type Error: Into<error::Error>;

    /// Attempts to write an entire buffer into this write.
    fn write_all(&mut self, buf: &[u8]) -> Result<(), Self::Error>;
}

#[cfg(not(feature = "unsealed_read_write"))]
mod private {
    pub trait Sealed {}
}

impl<W> Write for &mut W
where
    W: Write,
{
    type Error = W::Error;

    fn write_all(&mut self, buf: &[u8]) -> Result<(), Self::Error> {
        (*self).write_all(buf)
    }
}

#[cfg(not(feature = "unsealed_read_write"))]
impl<W> private::Sealed for &mut W where W: Write {}

#[cfg(feature = "std")]
/// A wrapper for types that implement
/// [`std::io::Write`](https://doc.rust-lang.org/std/io/trait.Write.html) to implement the local
/// [`Write`](trait.Write.html) trait.
#[derive(Debug)]
pub struct IoWrite<W>(W);

#[cfg(feature = "std")]
impl<W: io::Write> IoWrite<W> {
    /// Wraps an `io::Write` writer to make it compatible with [`Write`](trait.Write.html)
    pub fn new(w: W) -> IoWrite<W> {
        IoWrite(w)
    }
}

#[cfg(feature = "std")]
impl<W: io::Write> Write for IoWrite<W> {
    type Error = io::Error;

    fn write_all(&mut self, buf: &[u8]) -> Result<(), Self::Error> {
        self.0.write_all(buf)
    }
}

#[cfg(all(feature = "std", not(feature = "unsealed_read_write")))]
impl<W> private::Sealed for IoWrite<W> where W: io::Write {}

#[cfg(any(feature = "std", feature = "alloc"))]
impl Write for Vec<u8> {
    type Error = error::Error;

    fn write_all(&mut self, buf: &[u8]) -> Result<(), Self::Error> {
        self.extend_from_slice(buf);
        Ok(())
    }
}

#[cfg(all(
    any(feature = "std", feature = "alloc"),
    not(feature = "unsealed_read_write")
))]
impl private::Sealed for Vec<u8> {}

#[cfg(not(feature = "std"))]
#[derive(Debug)]
pub struct FmtWrite<'a, W: Write>(&'a mut W);

#[cfg(not(feature = "std"))]
impl<'a, W: Write> FmtWrite<'a, W> {
    /// Wraps an `fmt::Write` writer to make it compatible with [`Write`](trait.Write.html)
    pub fn new(w: &'a mut W) -> FmtWrite<'a, W> {
        FmtWrite(w)
    }
}

#[cfg(not(feature = "std"))]
impl<'a, W: Write> fmt::Write for FmtWrite<'a, W> {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        self.0.write_all(s.as_bytes()).map_err(|_| fmt::Error)
    }
}

#[cfg(all(not(feature = "std"), not(feature = "unsealed_read_write")))]
impl<'a, W> private::Sealed for FmtWrite<'a, W> where W: Write {}

/// Implements [`Write`](trait.Write.html) for mutable byte slices (`&mut [u8]`).
///
/// Returns an error if the value to serialize is too large to fit in the slice.
#[derive(Debug)]
pub struct SliceWrite<'a> {
    slice: &'a mut [u8],
    index: usize,
}

impl<'a> SliceWrite<'a> {
    /// Wraps a mutable slice so it can be used as a `Write`.
    pub fn new(slice: &'a mut [u8]) -> SliceWrite<'a> {
        SliceWrite { slice, index: 0 }
    }

    /// Returns the number of bytes written to the underlying slice.
    pub fn bytes_written(&self) -> usize {
        self.index
    }

    /// Returns the underlying slice.
    pub fn into_inner(self) -> &'a mut [u8] {
        self.slice
    }
}

impl<'a> Write for SliceWrite<'a> {
    type Error = error::Error;

    fn write_all(&mut self, buf: &[u8]) -> Result<(), Self::Error> {
        if self.slice.len() - self.index < buf.len() {
            // This buffer will not fit in our slice
            return Err(error::Error::scratch_too_small(self.index as u64));
        }
        let end = self.index + buf.len();
        self.slice[self.index..end].copy_from_slice(buf);
        self.index = end;
        Ok(())
    }
}

#[cfg(not(feature = "unsealed_read_write"))]
impl<'a> private::Sealed for SliceWrite<'a> {}
