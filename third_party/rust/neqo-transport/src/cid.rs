// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Encoding and decoding packets off the wire.

use neqo_common::{hex, hex_with_len, Decoder};
use neqo_crypto::random;

use std::borrow::Borrow;
use std::cmp::max;
use std::convert::AsRef;

pub const MAX_CONNECTION_ID_LEN: usize = 20;

#[derive(Clone, Default, Eq, Hash, PartialEq)]
pub struct ConnectionId {
    pub(crate) cid: Vec<u8>,
}

impl ConnectionId {
    pub fn generate(len: usize) -> Self {
        assert!(matches!(len, 0..=MAX_CONNECTION_ID_LEN));
        Self { cid: random(len) }
    }

    // Apply a wee bit of greasing here in picking a length between 8 and 20 bytes long.
    pub fn generate_initial() -> Self {
        let v = random(1);
        // Bias selection toward picking 8 (>50% of the time).
        let len: usize = max(8, 5 + (v[0] & (v[0] >> 4))).into();
        Self::generate(len)
    }

    pub fn as_cid_ref(&self) -> ConnectionIdRef {
        ConnectionIdRef::from(&self.cid[..])
    }
}

impl AsRef<[u8]> for ConnectionId {
    fn as_ref(&self) -> &[u8] {
        self.borrow()
    }
}

impl Borrow<[u8]> for ConnectionId {
    fn borrow(&self) -> &[u8] {
        &self.cid
    }
}

impl From<&[u8]> for ConnectionId {
    fn from(buf: &[u8]) -> Self {
        Self {
            cid: Vec::from(buf),
        }
    }
}

impl<'a> From<&ConnectionIdRef<'a>> for ConnectionId {
    fn from(cidref: &ConnectionIdRef<'a>) -> Self {
        Self {
            cid: Vec::from(cidref.cid),
        }
    }
}

impl std::ops::Deref for ConnectionId {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        &self.cid
    }
}

impl ::std::fmt::Debug for ConnectionId {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "CID {}", hex_with_len(&self.cid))
    }
}

impl ::std::fmt::Display for ConnectionId {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "{}", hex(&self.cid))
    }
}

impl<'a> PartialEq<ConnectionIdRef<'a>> for ConnectionId {
    fn eq(&self, other: &ConnectionIdRef<'a>) -> bool {
        &self.cid[..] == other.cid
    }
}

#[derive(Hash, Eq, PartialEq)]
pub struct ConnectionIdRef<'a> {
    cid: &'a [u8],
}

impl<'a> ::std::fmt::Debug for ConnectionIdRef<'a> {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "CID {}", hex_with_len(&self.cid))
    }
}

impl<'a> ::std::fmt::Display for ConnectionIdRef<'a> {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "{}", hex_with_len(&self.cid))
    }
}

impl<'a> From<&'a [u8]> for ConnectionIdRef<'a> {
    fn from(cid: &'a [u8]) -> Self {
        Self { cid }
    }
}

impl<'a> std::ops::Deref for ConnectionIdRef<'a> {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        &self.cid
    }
}

impl<'a> PartialEq<ConnectionId> for ConnectionIdRef<'a> {
    fn eq(&self, other: &ConnectionId) -> bool {
        self.cid == &other.cid[..]
    }
}

pub trait ConnectionIdDecoder {
    fn decode_cid<'a>(&self, dec: &mut Decoder<'a>) -> Option<ConnectionIdRef<'a>>;
}

pub trait ConnectionIdManager: ConnectionIdDecoder {
    fn generate_cid(&mut self) -> ConnectionId;
    fn as_decoder(&self) -> &dyn ConnectionIdDecoder;
}

#[cfg(test)]
mod tests {
    use super::*;
    use test_fixture::fixture_init;

    #[test]
    fn generate_initial_cid() {
        fixture_init();
        for _ in 0..100 {
            let cid = ConnectionId::generate_initial();
            if !matches!(cid.len(), 8..=MAX_CONNECTION_ID_LEN) {
                panic!("connection ID {:?}", cid);
            }
        }
    }
}
