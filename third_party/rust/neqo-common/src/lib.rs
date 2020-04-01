// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]

mod codec;
mod datagram;
mod incrdecoder;
pub mod log;
pub mod timer;

pub use self::codec::{Decoder, Encoder};
pub use self::datagram::Datagram;
pub use self::incrdecoder::{IncrementalDecoder, IncrementalDecoderResult};

#[macro_use]
extern crate lazy_static;

// Cribbed from the |matches| crate, for simplicity.
#[macro_export]
macro_rules! matches {
    ($expression:expr, $($pattern:tt)+) => {
        match $expression {
            $($pattern)+ => true,
            _ => false
        }
    }
}

#[must_use]
pub fn hex(buf: &[u8]) -> String {
    let mut ret = String::with_capacity(10 + buf.len() * 3);
    ret.push_str(&format!("[{}]: ", buf.len()));
    for b in buf {
        ret.push_str(&format!("{:02x}", b));
    }
    ret
}

#[must_use]
pub const fn const_max(a: usize, b: usize) -> usize {
    [a, b][(a < b) as usize]
}
#[must_use]
pub const fn const_min(a: usize, b: usize) -> usize {
    [a, b][(a >= b) as usize]
}
