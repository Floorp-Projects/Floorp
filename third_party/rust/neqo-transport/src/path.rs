// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::net::SocketAddr;

use crate::cid::{ConnectionId, ConnectionIdRef};

use neqo_common::Datagram;

/// This is the MTU that we assume when using IPv6.
/// We use this size for Initial packets, so we don't need to worry about probing for support.
/// If the path doesn't support this MTU, we will assume that it doesn't support QUIC.
///
/// This is a multiple of 16 greater than the largest possible short header (1 + 20 + 4).
pub const PATH_MTU_V6: usize = 1337;
/// The path MTU for IPv4 can be 20 bytes larger than for v6.
pub const PATH_MTU_V4: usize = PATH_MTU_V6 + 20;

#[derive(Clone, Debug, PartialEq)]
pub struct Path {
    local: SocketAddr,
    remote: SocketAddr,
    local_cids: Vec<ConnectionId>,
    remote_cid: ConnectionId,
    reset_token: Option<[u8; 16]>,
}

impl Path {
    /// Create a path from addresses and connection IDs.
    pub fn new(
        local: SocketAddr,
        remote: SocketAddr,
        local_cid: ConnectionId,
        remote_cid: ConnectionId,
    ) -> Self {
        Self {
            local,
            remote,
            local_cids: vec![local_cid],
            remote_cid,
            reset_token: None,
        }
    }

    pub fn received_on(&self, d: &Datagram) -> bool {
        self.local == d.destination() && self.remote == d.source()
    }

    pub fn mtu(&self) -> usize {
        if self.local.is_ipv4() {
            PATH_MTU_V4
        } else {
            PATH_MTU_V6 // IPv6
        }
    }

    /// Add a connection ID to the local set.
    pub fn add_local_cid(&mut self, cid: ConnectionId) {
        self.local_cids.push(cid);
    }

    /// Determine if the given connection ID is valid.
    pub fn valid_local_cid(&self, cid: &ConnectionIdRef) -> bool {
        self.local_cids.iter().any(|c| c == cid)
    }

    /// Get the first local connection ID.
    pub fn local_cid(&self) -> &ConnectionId {
        self.local_cids.first().as_ref().unwrap()
    }

    /// Set the remote connection ID based on the peer's choice.
    pub fn set_remote_cid(&mut self, cid: &ConnectionIdRef) {
        self.remote_cid = ConnectionId::from(cid);
    }

    /// Access the remote connection ID.
    pub fn remote_cid(&self) -> &ConnectionId {
        &self.remote_cid
    }

    /// Set the stateless reset token for the connection ID that is currently in use.
    pub fn set_reset_token(&mut self, token: [u8; 16]) {
        self.reset_token = Some(token);
    }

    /// Access the reset token.
    pub fn reset_token(&self) -> Option<&[u8; 16]> {
        self.reset_token.as_ref()
    }

    /// Make a datagram.
    pub fn datagram<V: Into<Vec<u8>>>(&self, payload: V) -> Datagram {
        Datagram::new(self.local, self.remote, payload)
    }

    /// Get local address as `SocketAddr`
    pub fn local_address(&self) -> SocketAddr {
        self.local
    }

    /// Get remote address as `SocketAddr`
    pub fn remote_address(&self) -> SocketAddr {
        self.remote
    }
}
