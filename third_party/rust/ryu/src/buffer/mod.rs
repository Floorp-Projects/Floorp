use core::{mem, slice, str};

use pretty;

#[cfg(feature = "no-panic")]
use no_panic::no_panic;

/// Safe API for formatting floating point numbers to text.
///
/// ## Example
///
/// ```rust
/// let mut buffer = ryu::Buffer::new();
/// let printed = buffer.format(1.234);
/// assert_eq!(printed, "1.234");
/// ```
#[derive(Copy, Clone)]
pub struct Buffer {
    bytes: [u8; 24],
}

impl Buffer {
    /// This is a cheap operation; you don't need to worry about reusing buffers
    /// for efficiency.
    #[inline]
    #[cfg_attr(feature = "no-panic", no_panic)]
    pub fn new() -> Self {
        Buffer {
            bytes: unsafe { mem::uninitialized() },
        }
    }

    /// Print a floating point number into this buffer and return a reference to
    /// its string representation within the buffer.
    ///
    /// # Special cases
    ///
    /// This function **does not** check for NaN or infinity. If the input
    /// number is not a finite float, the printed representation will be some
    /// correctly formatted but unspecified numerical value.
    ///
    /// Please check [`is_finite`] yourself before calling this function, or
    /// check [`is_nan`] and [`is_infinite`] and handle those cases yourself.
    ///
    /// [`is_finite`]: https://doc.rust-lang.org/std/primitive.f64.html#method.is_finite
    /// [`is_nan`]: https://doc.rust-lang.org/std/primitive.f64.html#method.is_nan
    /// [`is_infinite`]: https://doc.rust-lang.org/std/primitive.f64.html#method.is_infinite
    #[inline]
    #[cfg_attr(feature = "no-panic", no_panic)]
    pub fn format<F: Float>(&mut self, f: F) -> &str {
        unsafe {
            let n = f.write_to_ryu_buffer(&mut self.bytes[0]);
            debug_assert!(n <= self.bytes.len());
            let slice = slice::from_raw_parts(&self.bytes[0], n);
            str::from_utf8_unchecked(slice)
        }
    }
}

impl Default for Buffer {
    #[inline]
    #[cfg_attr(feature = "no-panic", no_panic)]
    fn default() -> Self {
        Buffer::new()
    }
}

/// A floating point number, f32 or f64, that can be written into a
/// [`ryu::Buffer`][Buffer].
///
/// This trait is sealed and cannot be implemented for types outside of the
/// `ryu` crate.
pub trait Float: Sealed {
    // Not public API.
    #[doc(hidden)]
    unsafe fn write_to_ryu_buffer(self, result: *mut u8) -> usize;
}

impl Float for f32 {
    #[inline]
    #[cfg_attr(feature = "no-panic", no_panic)]
    unsafe fn write_to_ryu_buffer(self, result: *mut u8) -> usize {
        pretty::f2s_buffered_n(self, result)
    }
}

impl Float for f64 {
    #[inline]
    #[cfg_attr(feature = "no-panic", no_panic)]
    unsafe fn write_to_ryu_buffer(self, result: *mut u8) -> usize {
        pretty::d2s_buffered_n(self, result)
    }
}

pub trait Sealed {}
impl Sealed for f32 {}
impl Sealed for f64 {}
