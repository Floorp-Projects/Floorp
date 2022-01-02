// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;

use super::{CommonMetricData, MetricId};

use glean::traits::StringList;

use crate::ipc::{need_ipc, with_ipc_payload};

/// A string list metric.
///
/// This allows appending a string value with arbitrary content to a list.
#[derive(Clone)]
pub enum StringListMetric {
    Parent {
        /// The metric's ID.
        ///
        /// **TEST-ONLY** - Do not use unless gated with `#[cfg(test)]`.
        id: MetricId,
        inner: glean::private::StringListMetric,
    },
    Child(StringListMetricIpc),
}
#[derive(Clone, Debug)]
pub struct StringListMetricIpc(MetricId);

impl StringListMetric {
    /// Create a new string list metric.
    pub fn new(id: MetricId, meta: CommonMetricData) -> Self {
        if need_ipc() {
            StringListMetric::Child(StringListMetricIpc(id))
        } else {
            let inner = glean::private::StringListMetric::new(meta);
            StringListMetric::Parent { id, inner }
        }
    }

    #[cfg(test)]
    pub(crate) fn child_metric(&self) -> Self {
        match self {
            StringListMetric::Parent { id, .. } => {
                StringListMetric::Child(StringListMetricIpc(*id))
            }
            StringListMetric::Child(_) => panic!("Can't get a child metric from a child metric"),
        }
    }
}

#[inherent(pub)]
impl StringList for StringListMetric {
    /// Add a new string to the list.
    ///
    /// ## Arguments
    ///
    /// * `value` - The string to add.
    ///
    /// ## Notes
    ///
    /// Truncates the value if it is longer than `MAX_STRING_LENGTH` bytes and logs an error.
    /// See [String list metric limits](https://mozilla.github.io/glean/book/user/metrics/string_list.html#limits).
    fn add<S: Into<String>>(&self, value: S) {
        match self {
            StringListMetric::Parent { inner, .. } => {
                StringList::add(&*inner, value);
            }
            StringListMetric::Child(c) => {
                with_ipc_payload(move |payload| {
                    if let Some(v) = payload.string_lists.get_mut(&c.0) {
                        v.push(value.into());
                    } else {
                        let v = vec![value.into()];
                        payload.string_lists.insert(c.0, v);
                    }
                });
            }
        }
    }

    /// Set to a specific list of strings.
    ///
    /// ## Arguments
    ///
    /// * `value` - The list of string to set the metric to.
    ///
    /// ## Notes
    ///
    /// If passed an empty list, records an error and returns.
    /// Truncates the list if it is longer than `MAX_LIST_LENGTH` and logs an error.
    /// Truncates any value in the list if it is longer than `MAX_STRING_LENGTH` and logs an error.
    fn set(&self, value: Vec<String>) {
        match self {
            StringListMetric::Parent { inner, .. } => {
                StringList::set(&*inner, value);
            }
            StringListMetric::Child(c) => {
                log::error!(
                    "Unable to set string list metric {:?} in non-main process. Ignoring.",
                    c.0
                );
                // TODO: Record an error.
            }
        }
    }

    /// **Test-only API.**
    ///
    /// Get the currently stored values.
    /// This doesn't clear the stored value.
    ///
    /// ## Arguments
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    fn test_get_value<'a, S: Into<Option<&'a str>>>(&self, ping_name: S) -> Option<Vec<String>> {
        match self {
            StringListMetric::Parent { inner, .. } => inner.test_get_value(ping_name),
            StringListMetric::Child(c) => {
                panic!("Cannot get test value for {:?} in non-parent process!", c.0)
            }
        }
    }

    /// **Exported for test purposes.**
    ///
    /// Gets the number of recorded errors for the given error type.
    ///
    /// # Arguments
    ///
    /// * `error` - The type of error
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    ///
    /// # Returns
    ///
    /// The number of errors recorded.
    fn test_get_num_recorded_errors<'a, S: Into<Option<&'a str>>>(
        &self,
        error: glean::ErrorType,
        ping_name: S,
    ) -> i32 {
        match self {
            StringListMetric::Parent { inner, .. } => {
                inner.test_get_num_recorded_errors(error, ping_name)
            }
            StringListMetric::Child(c) => panic!(
                "Cannot get the number of recorded errors for {:?} in non-parent process!",
                c.0
            ),
        }
    }
}

#[cfg(test)]
mod test {
    use crate::{common_test::*, ipc, metrics};

    #[test]
    #[ignore] // TODO: Enable them back when bug 1677454 lands.
    fn sets_string_list_value() {
        let _lock = lock_test();

        let metric = &metrics::test_only_ipc::a_string_list;

        metric.set(vec!["test_string_value".to_string()]);
        metric.add("another test value");

        assert_eq!(
            vec!["test_string_value", "another test value"],
            metric.test_get_value("store1").unwrap()
        );
    }

    #[test]
    #[ignore] // TODO: Enable them back when bug 1677454 lands.
    fn string_list_ipc() {
        // StringListMetric supports IPC only for `add`, not `set`.
        let _lock = lock_test();

        let parent_metric = &metrics::test_only_ipc::a_string_list;

        parent_metric.set(vec!["test_string_value".to_string()]);
        parent_metric.add("another test value");

        {
            let child_metric = parent_metric.child_metric();

            // scope for need_ipc RAII
            let _raii = ipc::test_set_need_ipc(true);

            // Recording APIs do not panic, even when they don't work.
            child_metric.set(vec!["not gonna be set".to_string()]);

            child_metric.add("child_value");
            assert!(ipc::take_buf().unwrap().len() > 0);
        }

        // TODO: implement replay. See bug 1646165.
        // Then perform the replay and assert we have the values from both "processes".
        assert!(ipc::replay_from_buf(&ipc::take_buf().unwrap()).is_ok());
        assert_eq!(
            vec!["test_string_value", "another test value"],
            parent_metric.test_get_value("store1").unwrap()
        );
    }
}
