/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use crate::{Guid, ServerTimestamp};
use std::borrow::Cow;
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CollectionRequest {
    pub collection: Cow<'static, str>,
    pub full: bool,
    pub ids: Option<Vec<Guid>>,
    pub limit: usize,
    pub older: Option<ServerTimestamp>,
    pub newer: Option<ServerTimestamp>,
    pub order: Option<RequestOrder>,
    pub commit: bool,
    pub batch: Option<String>,
}

impl CollectionRequest {
    #[inline]
    pub fn new<S>(collection: S) -> CollectionRequest
    where
        S: Into<Cow<'static, str>>,
    {
        CollectionRequest {
            collection: collection.into(),
            full: false,
            ids: None,
            limit: 0,
            older: None,
            newer: None,
            order: None,
            commit: false,
            batch: None,
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
    pub fn sort_by(mut self, order: RequestOrder) -> CollectionRequest {
        self.order = Some(order);
        self
    }

    #[inline]
    pub fn limit(mut self, num: usize) -> CollectionRequest {
        self.limit = num;
        self
    }

    #[inline]
    pub fn batch(mut self, batch: Option<String>) -> CollectionRequest {
        self.batch = batch;
        self
    }

    #[inline]
    pub fn commit(mut self, v: bool) -> CollectionRequest {
        self.commit = v;
        self
    }
}

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
