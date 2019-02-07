// Copyright 2013-2014 The Rust Project Developers.
// Copyright 2018 The Uuid Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use byteorder;
use prelude::*;

impl Uuid {
    /// Creates a new [`Uuid`] from a `u128` value.
    ///
    /// To create a [`Uuid`] from `u128`s, you need `u128` feature enabled for
    /// this crate.
    ///
    /// [`Uuid`]: ../struct.Uuid.html
    #[inline]
    pub fn from_u128(quad: u128) -> Self {
        Uuid::from(quad)
    }
}

impl From<u128> for Uuid {
    fn from(f: u128) -> Self {
        let mut bytes: ::Bytes = [0; 16];

        {
            use byteorder::ByteOrder;

            byteorder::NativeEndian::write_u128(&mut bytes[..], f);
        }

        Uuid::from_bytes(bytes)
    }
}

#[cfg(test)]
mod tests {
    use prelude::*;

    #[test]
    fn test_from_u128() {
        const U128: u128 = 0x3a0724b4_93a0_4d87_ac28_759c6caa13c4;

        let uuid = Uuid::from(U128);

        let uuid2: Uuid = U128.into();

        assert_eq!(uuid, uuid2)
    }

}
