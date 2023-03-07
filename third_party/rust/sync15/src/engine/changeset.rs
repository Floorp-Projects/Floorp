/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::bso::{IncomingBso, OutgoingBso};
use crate::{CollectionName, ServerTimestamp};

// Incoming and Outgoing changesets are almost identical except for the timestamp.
// Separate types still helps avoid confusion with that timestamp, so they're split.
#[derive(Debug)]
pub struct IncomingChangeset {
    pub changes: Vec<IncomingBso>,
    /// The server timestamp of the collection.
    pub timestamp: ServerTimestamp,
    pub collection: CollectionName,
}

impl IncomingChangeset {
    #[inline]
    pub fn new(collection: CollectionName, timestamp: ServerTimestamp) -> Self {
        Self::new_with_changes(collection, timestamp, Vec::new())
    }

    #[inline]
    pub fn new_with_changes(
        collection: CollectionName,
        timestamp: ServerTimestamp,
        changes: Vec<IncomingBso>,
    ) -> Self {
        Self {
            changes,
            timestamp,
            collection,
        }
    }
}

#[derive(Debug)]
pub struct OutgoingChangeset {
    pub changes: Vec<OutgoingBso>,
    pub collection: CollectionName,
}

impl OutgoingChangeset {
    #[inline]
    pub fn new(collection: CollectionName, changes: Vec<OutgoingBso>) -> Self {
        Self {
            collection,
            changes,
        }
    }
}
