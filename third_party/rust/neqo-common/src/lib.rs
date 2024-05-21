// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)] // This lint doesn't work here.

mod codec;
mod datagram;
pub mod event;
#[cfg(feature = "build-fuzzing-corpus")]
mod fuzz;
pub mod header;
pub mod hrtime;
mod incrdecoder;
pub mod log;
pub mod qlog;
pub mod tos;

use std::fmt::Write;

use enum_map::Enum;

#[cfg(feature = "build-fuzzing-corpus")]
pub use self::fuzz::write_item_to_fuzzing_corpus;
pub use self::{
    codec::{Decoder, Encoder},
    datagram::Datagram,
    header::Header,
    incrdecoder::{IncrementalDecoderBuffer, IncrementalDecoderIgnore, IncrementalDecoderUint},
    tos::{IpTos, IpTosDscp, IpTosEcn},
};

#[must_use]
pub fn hex(buf: impl AsRef<[u8]>) -> String {
    let mut ret = String::with_capacity(buf.as_ref().len() * 2);
    for b in buf.as_ref() {
        write!(&mut ret, "{b:02x}").unwrap();
    }
    ret
}

#[must_use]
pub fn hex_snip_middle(buf: impl AsRef<[u8]>) -> String {
    const SHOW_LEN: usize = 8;
    let buf = buf.as_ref();
    if buf.len() <= SHOW_LEN * 2 {
        hex_with_len(buf)
    } else {
        let mut ret = String::with_capacity(SHOW_LEN * 2 + 16);
        write!(&mut ret, "[{}]: ", buf.len()).unwrap();
        for b in &buf[..SHOW_LEN] {
            write!(&mut ret, "{b:02x}").unwrap();
        }
        ret.push_str("..");
        for b in &buf[buf.len() - SHOW_LEN..] {
            write!(&mut ret, "{b:02x}").unwrap();
        }
        ret
    }
}

#[must_use]
pub fn hex_with_len(buf: impl AsRef<[u8]>) -> String {
    let buf = buf.as_ref();
    let mut ret = String::with_capacity(10 + buf.len() * 2);
    write!(&mut ret, "[{}]: ", buf.len()).unwrap();
    for b in buf {
        write!(&mut ret, "{b:02x}").unwrap();
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

#[derive(Debug, PartialEq, Eq, Copy, Clone, Enum)]
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
        write!(f, "{self:?}")
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MessageType {
    Request,
    Response,
}
