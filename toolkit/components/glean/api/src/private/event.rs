// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::HashMap;
use std::hash::Hash;
use std::marker::PhantomData;
use std::sync::Arc;

use super::{CommonMetricData, Instant, MetricId, RecordedEvent};

use crate::dispatcher;
use crate::ipc::{need_ipc, with_ipc_payload};

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
    Parent {
        /// The metric's ID.
        ///
        /// **TEST-ONLY** - Do not use unless gated with `#[cfg(test)]`.
        id: MetricId,
        inner: Arc<EventMetricImpl<K>>,
    },
    Child(EventMetricIpc),
}
#[derive(Clone, Debug)]
pub struct EventMetricImpl<K> {
    inner: glean_core::metrics::EventMetric,
    extra_keys: PhantomData<K>,
}
#[derive(Debug)]
pub struct EventMetricIpc(MetricId);

impl<K: 'static + ExtraKeys + Send + Sync> EventMetric<K> {
    /// Create a new event metric.
    pub fn new(id: MetricId, meta: CommonMetricData) -> Self {
        if need_ipc() {
            EventMetric::Child(EventMetricIpc(id))
        } else {
            let inner = Arc::new(EventMetricImpl::new(meta));
            EventMetric::Parent { id, inner }
        }
    }

    #[cfg(test)]
    pub(crate) fn child_metric(&self) -> Self {
        match self {
            EventMetric::Parent { id, .. } => EventMetric::Child(EventMetricIpc(*id)),
            EventMetric::Child(_) => panic!("Can't get a child metric from a child metric"),
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
            EventMetric::Parent { inner, .. } => {
                let metric = Arc::clone(&inner);
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
            EventMetric::Parent { inner, .. } => {
                dispatcher::block_on_queue();
                inner.test_get_value(storage_name)
            }
            EventMetric::Child(_) => {
                panic!("Cannot get test value for event metric in non-parent process!",)
            }
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

#[cfg(test)]
mod test {
    use super::*;
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn smoke_test_event() {
        let _lock = lock_test();

        let metric = EventMetric::<NoExtraKeys>::new(
            0.into(),
            CommonMetricData {
                name: "event_metric".into(),
                category: "telemetry".into(),
                send_in_pings: vec!["store1".into()],
                disabled: false,
                ..Default::default()
            },
        );

        // No extra keys
        metric.record(None);

        let recorded = metric.test_get_value("store1").unwrap();

        assert!(recorded.iter().any(|e| e.name == "event_metric"));
    }

    #[test]
    fn smoke_test_event_with_extra() {
        let _lock = lock_test();

        #[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
        enum TestKeys {
            Extra1,
        }

        impl ExtraKeys for TestKeys {
            const ALLOWED_KEYS: &'static [&'static str] = &["extra1"];

            fn index(self) -> i32 {
                self as i32
            }
        }

        let metric = EventMetric::<TestKeys>::new(
            0.into(),
            CommonMetricData {
                name: "event_metric_with_extra".into(),
                category: "telemetry".into(),
                send_in_pings: vec!["store1".into()],
                disabled: false,
                ..Default::default()
            },
        );

        // No extra keys
        metric.record(None);

        // A valid extra key
        let mut map = HashMap::new();
        map.insert(TestKeys::Extra1, "a-valid-value".into());
        metric.record(map);

        let recorded = metric.test_get_value("store1").unwrap();

        let events: Vec<&RecordedEvent> = recorded
            .iter()
            .filter(|&e| e.category == "telemetry" && e.name == "event_metric_with_extra")
            .collect();
        assert_eq!(events.len(), 2);
        assert_eq!(events[0].extra, None);
        assert!(events[1].extra.as_ref().unwrap().get("extra1").unwrap() == "a-valid-value");
    }

    #[test]
    fn event_ipc() {
        use metrics::test_only_ipc::AnEventKeys;

        let _lock = lock_test();

        let parent_metric = &metrics::test_only_ipc::an_event;

        // No extra keys
        parent_metric.record(None);

        {
            let child_metric = parent_metric.child_metric();

            // scope for need_ipc RAII.
            let _raii = ipc::test_set_need_ipc(true);

            child_metric.record(None);

            let mut map = HashMap::new();
            map.insert(AnEventKeys::Extra1, "a-child-value".into());
            child_metric.record(map);
        }

        // Record in the parent after the child.
        let mut map = HashMap::new();
        map.insert(AnEventKeys::Extra1, "a-valid-value".into());
        parent_metric.record(map);

        let recorded = parent_metric.test_get_value("store1").unwrap();

        let events: Vec<&RecordedEvent> = recorded
            .iter()
            .filter(|&e| e.category == "test_only.ipc" && e.name == "an_event")
            .collect();
        assert_eq!(events.len(), 2);
        assert_eq!(events[0].extra, None);

        assert!(events[1].extra.as_ref().unwrap().get("extra1").unwrap() == "a-valid-value");
        // TODO: implement replay. See bug 1646165. Then change and add asserts for a-child-value.
        // Ensure the replay values apply without error, at least.
        let buf = ipc::take_buf().unwrap();
        assert!(buf.len() > 0);
        assert!(ipc::replay_from_buf(&ipc::take_buf().unwrap()).is_ok());
    }
}
