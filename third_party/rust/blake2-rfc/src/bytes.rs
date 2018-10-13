// Copyright 2015 blake2-rfc Developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

//! Operations on byte slices.

use core::ptr;

/// Operations on byte slices.
pub trait BytesExt {
    /// Set all bytes of this slice to the same value.
    ///
    /// Equivalent to C's memset().
    fn set_bytes(&mut self, value: u8);

    /// Copy all bytes from a source slice to the start of this slice.
    ///
    /// Equivalent to C's memcpy().
    fn copy_bytes_from(&mut self, src: &[u8]);
}

impl BytesExt for [u8] {
    #[inline]
    fn set_bytes(&mut self, value: u8) {
        unsafe {
            ptr::write_bytes(self.as_mut_ptr(), value, self.len());
        }
    }

    #[inline]
    fn copy_bytes_from(&mut self, src: &[u8]) {
        assert!(src.len() <= self.len());
        unsafe {
            ptr::copy_nonoverlapping(src.as_ptr(), self.as_mut_ptr(), src.len());
        }
    }
}
