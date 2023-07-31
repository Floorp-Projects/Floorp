// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Stream ID and stream index handling.

use neqo_common::Role;

#[derive(PartialEq, Debug, Copy, Clone, PartialOrd, Eq, Ord, Hash)]

/// The type of stream, either Bi-Directional or Uni-Directional.
pub enum StreamType {
    BiDi,
    UniDi,
}

#[derive(Debug, Eq, PartialEq, Clone, Copy, Ord, PartialOrd, Hash)]
pub struct StreamId(u64);

impl StreamId {
    pub const fn new(id: u64) -> Self {
        Self(id)
    }

    pub fn init(stream_type: StreamType, role: Role) -> Self {
        let type_val = match stream_type {
            StreamType::BiDi => 0,
            StreamType::UniDi => 2,
        };
        Self(type_val + Self::role_bit(role))
    }

    pub fn as_u64(self) -> u64 {
        self.0
    }

    pub fn is_bidi(self) -> bool {
        self.as_u64() & 0x02 == 0
    }

    pub fn is_uni(self) -> bool {
        !self.is_bidi()
    }

    pub fn stream_type(self) -> StreamType {
        if self.is_bidi() {
            StreamType::BiDi
        } else {
            StreamType::UniDi
        }
    }

    pub fn is_client_initiated(self) -> bool {
        self.as_u64() & 0x01 == 0
    }

    pub fn is_server_initiated(self) -> bool {
        !self.is_client_initiated()
    }

    pub fn role(self) -> Role {
        if self.is_client_initiated() {
            Role::Client
        } else {
            Role::Server
        }
    }

    pub fn is_self_initiated(self, my_role: Role) -> bool {
        match my_role {
            Role::Client if self.is_client_initiated() => true,
            Role::Server if self.is_server_initiated() => true,
            _ => false,
        }
    }

    pub fn is_remote_initiated(self, my_role: Role) -> bool {
        !self.is_self_initiated(my_role)
    }

    pub fn is_send_only(self, my_role: Role) -> bool {
        self.is_uni() && self.is_self_initiated(my_role)
    }

    pub fn is_recv_only(self, my_role: Role) -> bool {
        self.is_uni() && self.is_remote_initiated(my_role)
    }

    pub fn next(&mut self) {
        self.0 += 4;
    }

    /// This returns a bit that is shared by all streams created by this role.
    pub fn role_bit(role: Role) -> u64 {
        match role {
            Role::Server => 1,
            Role::Client => 0,
        }
    }
}

impl From<u64> for StreamId {
    fn from(val: u64) -> Self {
        Self::new(val)
    }
}

impl From<&u64> for StreamId {
    fn from(val: &u64) -> Self {
        Self::new(*val)
    }
}

impl PartialEq<u64> for StreamId {
    fn eq(&self, other: &u64) -> bool {
        self.as_u64() == *other
    }
}

impl AsRef<u64> for StreamId {
    fn as_ref(&self) -> &u64 {
        &self.0
    }
}

impl ::std::fmt::Display for StreamId {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "{}", self.as_u64())
    }
}

#[cfg(test)]
mod test {
    use super::StreamId;
    use neqo_common::Role;

    #[test]
    fn bidi_stream_properties() {
        let id1 = StreamId::from(16);
        assert!(id1.is_bidi());
        assert!(!id1.is_uni());
        assert!(id1.is_client_initiated());
        assert!(!id1.is_server_initiated());
        assert_eq!(id1.role(), Role::Client);
        assert!(id1.is_self_initiated(Role::Client));
        assert!(!id1.is_self_initiated(Role::Server));
        assert!(!id1.is_remote_initiated(Role::Client));
        assert!(id1.is_remote_initiated(Role::Server));
        assert!(!id1.is_send_only(Role::Server));
        assert!(!id1.is_send_only(Role::Client));
        assert!(!id1.is_recv_only(Role::Server));
        assert!(!id1.is_recv_only(Role::Client));
        assert_eq!(id1.as_u64(), 16);
    }

    #[test]
    fn uni_stream_properties() {
        let id2 = StreamId::from(35);
        assert!(!id2.is_bidi());
        assert!(id2.is_uni());
        assert!(!id2.is_client_initiated());
        assert!(id2.is_server_initiated());
        assert_eq!(id2.role(), Role::Server);
        assert!(!id2.is_self_initiated(Role::Client));
        assert!(id2.is_self_initiated(Role::Server));
        assert!(id2.is_remote_initiated(Role::Client));
        assert!(!id2.is_remote_initiated(Role::Server));
        assert!(id2.is_send_only(Role::Server));
        assert!(!id2.is_send_only(Role::Client));
        assert!(!id2.is_recv_only(Role::Server));
        assert!(id2.is_recv_only(Role::Client));
        assert_eq!(id2.as_u64(), 35);
    }
}
