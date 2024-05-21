// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{net::SocketAddr, ops::Deref};

use crate::{hex_with_len, IpTos};

#[derive(Clone, PartialEq, Eq)]
pub struct Datagram {
    src: SocketAddr,
    dst: SocketAddr,
    tos: IpTos,
    ttl: Option<u8>,
    d: Vec<u8>,
}

impl Datagram {
    pub fn new<V: Into<Vec<u8>>>(
        src: SocketAddr,
        dst: SocketAddr,
        tos: IpTos,
        ttl: Option<u8>,
        d: V,
    ) -> Self {
        Self {
            src,
            dst,
            tos,
            ttl,
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

    #[must_use]
    pub fn tos(&self) -> IpTos {
        self.tos
    }

    #[must_use]
    pub fn ttl(&self) -> Option<u8> {
        self.ttl
    }

    pub fn set_tos(&mut self, tos: IpTos) {
        self.tos = tos;
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
            "Datagram {:?} TTL {:?} {:?}->{:?}: {}",
            self.tos,
            self.ttl,
            self.src,
            self.dst,
            hex_with_len(&self.d)
        )
    }
}

#[cfg(test)]
use test_fixture::datagram;

#[test]
fn fmt_datagram() {
    let d = datagram([0; 1].to_vec());
    assert_eq!(
        &format!("{d:?}"),
        "Datagram IpTos(Cs0, Ect0) TTL Some(128) [fe80::1]:443->[fe80::1]:443: [1]: 00"
    );
}
