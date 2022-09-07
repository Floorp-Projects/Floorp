// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::net::SocketAddr;
use std::ops::Deref;

use crate::hex_with_len;

#[derive(PartialEq, Eq, Clone)]
pub struct Datagram {
    src: SocketAddr,
    dst: SocketAddr,
    d: Vec<u8>,
}

impl Datagram {
    pub fn new<V: Into<Vec<u8>>>(src: SocketAddr, dst: SocketAddr, d: V) -> Self {
        Self {
            src,
            dst,
            d: d.into(),
        }
    }

    #[must_use]
    pub fn source(&self) -> SocketAddr {
        self.src
    }

    #[must_use]
    pub fn destination(&self) -> SocketAddr {
        self.dst
    }
}

impl Deref for Datagram {
    type Target = Vec<u8>;
    #[must_use]
    fn deref(&self) -> &Self::Target {
        &self.d
    }
}

impl std::fmt::Debug for Datagram {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(
            f,
            "Datagram {:?}->{:?}: {}",
            self.src,
            self.dst,
            hex_with_len(&self.d)
        )
    }
}
