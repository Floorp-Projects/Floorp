/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use crate::{CollectionName, Guid, ServerTimestamp};
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct CollectionRequest {
    pub collection: CollectionName,
    pub full: bool,
    pub ids: Option<Vec<Guid>>,

    pub limit: Option<RequestLimit>,
    pub older: Option<ServerTimestamp>,
    pub newer: Option<ServerTimestamp>,
}

impl CollectionRequest {
    #[inline]
    pub fn new(collection: CollectionName) -> CollectionRequest {
        CollectionRequest {
            collection,
            ..Default::default()
        }
    }

    #[inline]
    pub fn ids<V>(mut self, v: V) -> CollectionRequest
    where
        V: IntoIterator,
        V::Item: Into<Guid>,
    {
        self.ids = Some(v.into_iter().map(|id| id.into()).collect());
        self
    }

    #[inline]
    pub fn full(mut self) -> CollectionRequest {
        self.full = true;
        self
    }

    #[inline]
    pub fn older_than(mut self, ts: ServerTimestamp) -> CollectionRequest {
        self.older = Some(ts);
        self
    }

    #[inline]
    pub fn newer_than(mut self, ts: ServerTimestamp) -> CollectionRequest {
        self.newer = Some(ts);
        self
    }

    #[inline]
    pub fn limit(mut self, num: usize, order: RequestOrder) -> CollectionRequest {
        self.limit = Some(RequestLimit { num, order });
        self
    }
}

// This is just used interally - consumers just provide the content, not request params.
#[cfg(feature = "sync-client")]
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub(crate) struct CollectionPost {
    pub collection: CollectionName,
    pub commit: bool,
    pub batch: Option<String>,
}

#[cfg(feature = "sync-client")]
impl CollectionPost {
    #[inline]
    pub fn new(collection: CollectionName) -> Self {
        Self {
            collection,
            ..Default::default()
        }
    }

    #[inline]
    pub fn batch(mut self, batch: Option<String>) -> Self {
        self.batch = batch;
        self
    }

    #[inline]
    pub fn commit(mut self, v: bool) -> Self {
        self.commit = v;
        self
    }
}

// Asking for the order of records only makes sense if you are limiting them
// in some way - consumers don't care about the order otherwise as everything
// is processed as a set.
#[derive(Debug, Clone, Copy, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum RequestOrder {
    Oldest,
    Newest,
    Index,
}

impl RequestOrder {
    #[inline]
    pub fn as_str(self) -> &'static str {
        match self {
            RequestOrder::Oldest => "oldest",
            RequestOrder::Newest => "newest",
            RequestOrder::Index => "index",
        }
    }
}

impl std::fmt::Display for RequestOrder {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.as_str())
    }
}

// If you specify a numerical limit you must provide the order so backfilling
// is possible (ie, so you know which ones you got!)
#[derive(Debug, Clone, Copy, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct RequestLimit {
    pub(crate) num: usize,
    pub(crate) order: RequestOrder,
}
