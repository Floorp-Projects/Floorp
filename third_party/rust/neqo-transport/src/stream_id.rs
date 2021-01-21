// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Stream ID and stream index handling.

use std::ops::AddAssign;

use neqo_common::Role;

#[derive(PartialEq, Debug, Copy, Clone, PartialOrd, Eq, Ord, Hash)]

/// The type of stream, either Bi-Directional or Uni-Directional.
pub enum StreamType {
    BiDi,
    UniDi,
}

pub(crate) struct StreamIndexes {
    pub local_max_stream_uni: StreamIndex,
    pub local_max_stream_bidi: StreamIndex,
    pub local_next_stream_uni: StreamIndex,
    pub local_next_stream_bidi: StreamIndex,
    pub remote_max_stream_uni: StreamIndex,
    pub remote_max_stream_bidi: StreamIndex,
    pub remote_next_stream_uni: StreamIndex,
    pub remote_next_stream_bidi: StreamIndex,
}

impl StreamIndexes {
    pub fn new(local_max_stream_bidi: StreamIndex, local_max_stream_uni: StreamIndex) -> Self {
        Self {
            local_max_stream_bidi,
            local_max_stream_uni,
            local_next_stream_uni: StreamIndex::new(0),
            local_next_stream_bidi: StreamIndex::new(0),
            remote_max_stream_bidi: StreamIndex::new(0),
            remote_max_stream_uni: StreamIndex::new(0),
            remote_next_stream_uni: StreamIndex::new(0),
            remote_next_stream_bidi: StreamIndex::new(0),
        }
    }
}

#[derive(Debug, Eq, PartialEq, Clone, Copy, Ord, PartialOrd, Hash)]
pub struct StreamId(u64);

impl StreamId {
    pub const fn new(id: u64) -> Self {
        Self(id)
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
}

impl From<u64> for StreamId {
    fn from(val: u64) -> Self {
        Self::new(val)
    }
}

impl PartialEq<u64> for StreamId {
    fn eq(&self, other: &u64) -> bool {
        self.as_u64() == *other
    }
}

impl ::std::fmt::Display for StreamId {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "{}", self.as_u64())
    }
}

#[derive(Debug, Eq, PartialEq, Clone, Copy, Ord, PartialOrd, Hash)]
pub struct StreamIndex(u64);

impl StreamIndex {
    pub const fn new(val: u64) -> Self {
        Self(val)
    }

    pub fn to_stream_id(self, stream_type: StreamType, role: Role) -> StreamId {
        let type_val = match stream_type {
            StreamType::BiDi => 0,
            StreamType::UniDi => 2,
        };
        let role_val = match role {
            Role::Server => 1,
            Role::Client => 0,
        };

        StreamId::from((self.0 << 2) + type_val + role_val)
    }

    pub fn as_u64(self) -> u64 {
        self.0
    }
}

impl From<StreamId> for StreamIndex {
    fn from(val: StreamId) -> Self {
        Self(val.as_u64() >> 2)
    }
}

impl AddAssign<u64> for StreamIndex {
    fn add_assign(&mut self, other: u64) {
        *self = Self::new(self.as_u64() + other)
    }
}

#[cfg(test)]
mod test {
    use super::{StreamIndex, StreamType};
    use neqo_common::Role;

    #[test]
    fn bidi_stream_properties() {
        let id1 = StreamIndex::new(4).to_stream_id(StreamType::BiDi, Role::Client);
        assert_eq!(id1.is_bidi(), true);
        assert_eq!(id1.is_uni(), false);
        assert_eq!(id1.is_client_initiated(), true);
        assert_eq!(id1.is_server_initiated(), false);
        assert_eq!(id1.role(), Role::Client);
        assert_eq!(id1.is_self_initiated(Role::Client), true);
        assert_eq!(id1.is_self_initiated(Role::Server), false);
        assert_eq!(id1.is_remote_initiated(Role::Client), false);
        assert_eq!(id1.is_remote_initiated(Role::Server), true);
        assert_eq!(id1.is_send_only(Role::Server), false);
        assert_eq!(id1.is_send_only(Role::Client), false);
        assert_eq!(id1.is_recv_only(Role::Server), false);
        assert_eq!(id1.is_recv_only(Role::Client), false);
        assert_eq!(id1.as_u64(), 16);
    }

    #[test]
    fn uni_stream_properties() {
        let id2 = StreamIndex::new(8).to_stream_id(StreamType::UniDi, Role::Server);
        assert_eq!(id2.is_bidi(), false);
        assert_eq!(id2.is_uni(), true);
        assert_eq!(id2.is_client_initiated(), false);
        assert_eq!(id2.is_server_initiated(), true);
        assert_eq!(id2.role(), Role::Server);
        assert_eq!(id2.is_self_initiated(Role::Client), false);
        assert_eq!(id2.is_self_initiated(Role::Server), true);
        assert_eq!(id2.is_remote_initiated(Role::Client), true);
        assert_eq!(id2.is_remote_initiated(Role::Server), false);
        assert_eq!(id2.is_send_only(Role::Server), true);
        assert_eq!(id2.is_send_only(Role::Client), false);
        assert_eq!(id2.is_recv_only(Role::Server), false);
        assert_eq!(id2.is_recv_only(Role::Client), true);
        assert_eq!(id2.as_u64(), 35);
    }
}
