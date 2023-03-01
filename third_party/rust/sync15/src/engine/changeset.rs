/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::bso::{IncomingBso, OutgoingBso};
use crate::ServerTimestamp;

#[derive(Debug, Clone)]
pub struct RecordChangeset<T> {
    pub changes: Vec<T>,
    /// For GETs, the last sync timestamp that should be persisted after
    /// applying the records.
    /// For POSTs, this is the XIUS timestamp.
    pub timestamp: ServerTimestamp,
    pub collection: std::borrow::Cow<'static, str>,
}

pub type IncomingChangeset = RecordChangeset<IncomingBso>;
pub type OutgoingChangeset = RecordChangeset<OutgoingBso>;

impl<T> RecordChangeset<T> {
    #[inline]
    pub fn new(
        collection: impl Into<std::borrow::Cow<'static, str>>,
        timestamp: ServerTimestamp,
    ) -> RecordChangeset<T> {
        Self::new_with_changes(collection, timestamp, Vec::new())
    }

    #[inline]
    pub fn new_with_changes(
        collection: impl Into<std::borrow::Cow<'static, str>>,
        timestamp: ServerTimestamp,
        changes: Vec<T>,
    ) -> RecordChangeset<T> {
        RecordChangeset {
            changes,
            timestamp,
            collection: collection.into(),
        }
    }
}
