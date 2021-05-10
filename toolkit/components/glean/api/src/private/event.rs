// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::HashMap;

use inherent::inherent;

use super::{CommonMetricData, MetricId, RecordedEvent};

use crate::ipc::{need_ipc, with_ipc_payload};

use glean::traits::Event;
pub use glean::traits::{EventRecordingError, ExtraKeys, NoExtraKeys};

/// An event metric.
///
/// Events allow recording of e.g. individual occurences of user actions, say
/// every time a view was open and from where. Each time you record an event, it
/// records a timestamp, the event's name and a set of custom values.
pub enum EventMetric<K> {
    Parent {
        /// The metric's ID.
        ///
        /// **TEST-ONLY** - Do not use unless gated with `#[cfg(test)]`.
        id: MetricId,
        inner: glean::private::EventMetric<K>,
    },
    Child(EventMetricIpc),
}

#[derive(Debug)]
pub struct EventMetricIpc(MetricId);

impl<K: 'static + ExtraKeys + Send + Sync> EventMetric<K> {
    /// Create a new event metric.
    pub fn new(id: MetricId, meta: CommonMetricData) -> Self {
        if need_ipc() {
            EventMetric::Child(EventMetricIpc(id))
        } else {
            let inner = glean::private::EventMetric::new(meta);
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

    pub(crate) fn record_with_time(&self, timestamp: u64, extra: HashMap<i32, String>) {
        match self {
            EventMetric::Parent { inner, .. } => {
                inner.record_with_time(timestamp, extra);
            }
            EventMetric::Child(_) => {
                // TODO: Instrument this error
                log::error!("Can't record an event with a timestamp from a child metric");
            }
        }
    }
}

#[inherent(pub)]
impl<K: 'static + ExtraKeys + Send + Sync> Event for EventMetric<K> {
    type Extra = K;

    fn record<M: Into<Option<HashMap<K, String>>>>(&self, extra: M) {
        match self {
            EventMetric::Parent { inner, .. } => {
                inner.record(extra);
            }
            EventMetric::Child(c) => {
                let now = glean::get_timestamp_ms();
                let extra = extra.into().map(|hash_map| {
                    hash_map
                        .iter()
                        .map(|(k, v)| (k.index(), v.clone()))
                        .collect()
                });
                let extra = extra.unwrap_or_else(HashMap::new);
                with_ipc_payload(move |payload| {
                    if let Some(v) = payload.events.get_mut(&c.0) {
                        v.push((now, extra));
                    } else {
                        let v = vec![(now, extra)];
                        payload.events.insert(c.0, v);
                    }
                });
            }
        }
    }

    fn test_get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<Vec<RecordedEvent>> {
        match self {
            EventMetric::Parent { inner, .. } => inner.test_get_value(ping_name),
            EventMetric::Child(_) => {
                panic!("Cannot get test value for event metric in non-parent process!",)
            }
        }
    }

    fn test_get_num_recorded_errors<'a, S: Into<Option<&'a str>>>(
        &self,
        error: glean::ErrorType,
        ping_name: S,
    ) -> i32 {
        match self {
            EventMetric::Parent { inner, .. } => {
                inner.test_get_num_recorded_errors(error, ping_name)
            }
            EventMetric::Child(c) => panic!(
                "Cannot get the number of recorded errors for {:?} in non-parent process!",
                c.0
            ),
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::{common_test::*, ipc, metrics};

    #[test]
    #[ignore] // TODO: Enable them back when bug 1673668 lands.
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
    #[ignore] // TODO: Enable them back when bug 1673668 lands.
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
    #[ignore] // TODO: Enable them back when bug 1673668 lands.
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
