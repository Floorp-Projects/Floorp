// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::HashMap;
use std::hash::Hash;
use std::marker::PhantomData;
use std::sync::Arc;

use super::{CommonMetricData, Instant, RecordedEvent};

use crate::dispatcher;
use crate::ipc::{need_ipc, with_ipc_payload, MetricId};

/// Extra keys for events.
///
/// Extra keys need to be pre-defined and map to a string representation.
///
/// For user-defined `EventMetric`s these will be defined as `enums`.
/// Each variant will correspond to an entry in the `ALLOWED_KEYS` list.
/// The Glean SDK requires the keys as strings for submission in pings,
/// whereas in code we want to provide users a type to work with
/// (e.g. to avoid typos or misuse of the API).
pub trait ExtraKeys: Hash + Eq + PartialEq + Copy {
    /// List of allowed extra keys as strings.
    const ALLOWED_KEYS: &'static [&'static str];

    /// The index of the extra key.
    ///
    /// It corresponds to its position in the associated `ALLOWED_KEYS` list.
    ///
    /// *Note*: An index of `-1` indicates an invalid / non-existing extra key.
    /// Invalid / non-existing extra keys will be recorded as an error.
    /// This cannot happen for generated code.
    fn index(self) -> i32;
}

/// Default of no extra keys for events.
///
/// An enum with no values for convenient use as the default set of extra keys
/// that an `EventMetric` can accept.
///
/// *Note*: There exist no values for this enum, it can never exist.
/// It its equivalent to the [`never / !` type](https://doc.rust-lang.org/std/primitive.never.html).
#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
pub enum NoExtraKeys {}

impl ExtraKeys for NoExtraKeys {
    const ALLOWED_KEYS: &'static [&'static str] = &[];

    fn index(self) -> i32 {
        // This index will never be used.
        -1
    }
}

/// An event metric.
///
/// Events allow recording of e.g. individual occurences of user actions, say
/// every time a view was open and from where. Each time you record an event, it
/// records a timestamp, the event's name and a set of custom values.
#[derive(Debug)]
pub enum EventMetric<K> {
    Parent(Arc<EventMetricImpl<K>>),
    Child(EventMetricIpc),
}
#[derive(Clone, Debug)]
pub struct EventMetricImpl<K> {
    inner: glean_core::metrics::EventMetric,
    extra_keys: PhantomData<K>,
}
#[derive(Debug)]
pub struct EventMetricIpc(MetricId);

impl<K: 'static + ExtraKeys + std::fmt::Debug + Send + Sync> EventMetric<K> {
    /// Create a new event metric.
    pub fn new(meta: CommonMetricData) -> Self {
        if need_ipc() {
            EventMetric::Child(EventMetricIpc(MetricId::new(meta)))
        } else {
            EventMetric::Parent(Arc::new(EventMetricImpl::new(meta)))
        }
    }

    /// Record an event.
    ///
    /// Records an event under this metric's name and attaches a timestamp.
    /// If given a map of extra values is added to the event log.
    ///
    /// ## Arguments
    ///
    /// * `extra` - An (optional) map of (key, value) pairs.
    pub fn record<M: Into<Option<HashMap<K, String>>> + Send + Sync>(&self, extra: M) {
        match self {
            EventMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                let extra = extra.into().clone();
                let now = Instant::now();
                dispatcher::launch(move || metric.record(now, extra));
            }
            EventMetric::Child(c) => {
                let extra = extra.into().and_then(|hash_map| {
                    Some(
                        hash_map
                            .iter()
                            .map(|(k, v)| (k.index(), v.clone()))
                            .collect(),
                    )
                });
                let now = Instant::now();
                with_ipc_payload(move |payload| {
                    if let Some(v) = payload.events.get_mut(&c.0) {
                        v.push((now, extra));
                    } else {
                        let mut v = vec![];
                        v.push((now, extra));
                        payload.events.insert(c.0.clone(), v);
                    }
                });
            }
        }
    }

    /// **Test-only API.**
    ///
    /// Get the currently stored events for this event metric as a JSON-encoded string.
    /// This doesn't clear the stored value.
    ///
    /// ## Note
    ///
    /// This currently returns the value as a JSON encoded string.
    /// `glean_core` doesn't expose the underlying recorded event type.
    /// This will eventually change to a proper `RecordedEventData` type.
    /// See [Bug 1635074](https://bugzilla.mozilla.org/show_bug.cgi?id=1635074).
    ///
    /// ## Arguments
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value(&self, storage_name: &str) -> Option<Vec<RecordedEvent>> {
        match self {
            EventMetric::Parent(p) => {
                dispatcher::block_on_queue();
                p.test_get_value(storage_name)
            }
            EventMetric::Child(_) => panic!(
                "Cannot get test value for {:?} in non-parent process!",
                self
            ),
        }
    }
}

impl<K: ExtraKeys> EventMetricImpl<K> {
    pub fn new(meta: CommonMetricData) -> Self {
        let allowed_extra_keys = K::ALLOWED_KEYS.iter().map(|s| s.to_string()).collect();
        let inner = glean_core::metrics::EventMetric::new(meta, allowed_extra_keys);

        Self {
            inner,
            extra_keys: PhantomData,
        }
    }

    pub fn record<M: Into<Option<HashMap<K, String>>>>(&self, then: Instant, extra: M) {
        // Translate from [ExtraKey -> String] to a [Int -> String] map
        let extra = extra
            .into()
            .map(|h| h.into_iter().map(|(k, v)| (k.index(), v)).collect());

        crate::with_glean(|glean| self.inner.record(glean, then.as_millis(), extra))
    }

    pub fn test_get_value(&self, storage_name: &str) -> Option<Vec<RecordedEvent>> {
        crate::with_glean(|glean| self.inner.test_get_value(glean, storage_name))
    }
}
