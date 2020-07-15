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
pub mod qlog;
pub mod timer;

pub use self::codec::{Decoder, Encoder};
pub use self::datagram::Datagram;
pub use self::incrdecoder::{
    IncrementalDecoderBuffer, IncrementalDecoderIgnore, IncrementalDecoderUint,
};

#[macro_use]
extern crate lazy_static;

#[must_use]
pub fn hex(buf: &[u8]) -> String {
    let mut ret = String::with_capacity(buf.len() * 2);
    for b in buf {
        ret.push_str(&format!("{:02x}", b));
    }
    ret
}

#[must_use]
pub fn hex_snip_middle(buf: &[u8]) -> String {
    const SHOW_LEN: usize = 8;
    if buf.len() <= SHOW_LEN * 2 {
        hex_with_len(buf)
    } else {
        let mut ret = String::with_capacity(SHOW_LEN * 2 + 16);
        ret.push_str(&format!("[{}]: ", buf.len()));
        for b in &buf[..SHOW_LEN] {
            ret.push_str(&format!("{:02x}", b));
        }
        ret.push_str("..");
        for b in &buf[buf.len() - SHOW_LEN..] {
            ret.push_str(&format!("{:02x}", b));
        }
        ret
    }
}

#[must_use]
pub fn hex_with_len(buf: &[u8]) -> String {
    let mut ret = String::with_capacity(10 + buf.len() * 2);
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

#[derive(Debug, PartialEq, Copy, Clone)]
/// Client or Server.
pub enum Role {
    Client,
    Server,
}

impl Role {
    #[must_use]
    pub fn remote(self) -> Self {
        match self {
            Self::Client => Self::Server,
            Self::Server => Self::Client,
        }
    }
}

impl ::std::fmt::Display for Role {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "{:?}", self)
    }
}
