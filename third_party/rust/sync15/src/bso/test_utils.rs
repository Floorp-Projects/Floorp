/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Utilities for tests to make IncomingBsos and Content from test data.
use super::{IncomingBso, IncomingEnvelope, OutgoingBso};
use crate::{Guid, ServerTimestamp};

/// Tests often want an IncomingBso to test, and the easiest way is often to
/// create an OutgoingBso convert it back to an incoming.
impl OutgoingBso {
    // These functions would ideally consume `self` and avoid the clones, but
    // this is more convenient for some tests and the extra overhead doesn't
    // really matter for tests.
    /// When a test has an [OutgoingBso] and wants it as an [IncomingBso]
    pub fn to_test_incoming(&self) -> IncomingBso {
        self.to_test_incoming_ts(ServerTimestamp::default())
    }

    /// When a test has an [OutgoingBso] and wants it as an [IncomingBso] with a specific timestamp.
    pub fn to_test_incoming_ts(&self, ts: ServerTimestamp) -> IncomingBso {
        IncomingBso {
            envelope: IncomingEnvelope {
                id: self.envelope.id.clone(),
                modified: ts,
                sortindex: self.envelope.sortindex,
                ttl: self.envelope.ttl,
            },
            payload: self.payload.clone(),
        }
    }

    /// When a test has an [OutgoingBso] and wants it as an [IncomingBso] with a specific T.
    pub fn to_test_incoming_t<T: for<'de> serde::Deserialize<'de>>(&self) -> T {
        self.to_test_incoming().into_content().content().unwrap()
    }
}

/// Helpers to create an IncomingBso from some T
impl IncomingBso {
    /// When a test has an T and wants it as an [IncomingBso]
    pub fn from_test_content<T: serde::Serialize>(json: T) -> Self {
        // Go via an OutgoingBso
        OutgoingBso::from_content_with_id(json)
            .unwrap()
            .to_test_incoming()
    }

    /// When a test has an T and wants it as an [IncomingBso] with a specific timestamp.
    pub fn from_test_content_ts<T: serde::Serialize>(json: T, ts: ServerTimestamp) -> Self {
        // Go via an OutgoingBso
        OutgoingBso::from_content_with_id(json)
            .unwrap()
            .to_test_incoming_ts(ts)
    }

    /// When a test wants a new incoming tombstone.
    pub fn new_test_tombstone(guid: Guid) -> Self {
        OutgoingBso::new_tombstone(guid.into()).to_test_incoming()
    }
}
