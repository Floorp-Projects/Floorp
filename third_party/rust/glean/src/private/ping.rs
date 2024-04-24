// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::{Arc, Mutex};

type BoxedCallback = Box<dyn FnOnce(Option<&str>) + Send + 'static>;

/// A ping is a bundle of related metrics, gathered in a payload to be transmitted.
///
/// The ping payload will be encoded in JSON format and contains shared information data.
#[derive(Clone)]
pub struct PingType {
    pub(crate) inner: glean_core::metrics::PingType,

    /// **Test-only API**
    ///
    /// A function to be called right before a ping is submitted.
    test_callback: Arc<Mutex<Option<BoxedCallback>>>,
}

impl PingType {
    /// Creates a new ping type.
    ///
    /// # Arguments
    ///
    /// * `name` - The name of the ping.
    /// * `include_client_id` - Whether to include the client ID in the assembled ping when.
    /// * `send_if_empty` - Whether the ping should be sent empty or not.
    /// * `precise_timestamps` - Whether the ping should use precise timestamps for the start and end time.
    /// * `include_info_sections` - Whether the ping should include the client/ping_info sections.
    /// * `enabled` - Whether or not this ping is enabled. Note: Data that would be sent on a disabled
    ///   ping will still be collected and is discarded instead of being submitted.
    /// * `schedules_pings` - A list of pings which are triggered for submission when this ping is
    ///   submitted.
    /// * `reason_codes` - The valid reason codes for this ping.
    #[allow(clippy::too_many_arguments)]
    pub fn new<A: Into<String>>(
        name: A,
        include_client_id: bool,
        send_if_empty: bool,
        precise_timestamps: bool,
        include_info_sections: bool,
        enabled: bool,
        schedules_pings: Vec<String>,
        reason_codes: Vec<String>,
    ) -> Self {
        let inner = glean_core::metrics::PingType::new(
            name.into(),
            include_client_id,
            send_if_empty,
            precise_timestamps,
            include_info_sections,
            enabled,
            schedules_pings,
            reason_codes,
        );

        Self {
            inner,
            test_callback: Arc::new(Default::default()),
        }
    }

    /// Submits the ping for eventual uploading.
    ///
    /// The ping content is assembled as soon as possible, but upload is not
    /// guaranteed to happen immediately, as that depends on the upload policies.
    ///
    /// If the ping currently contains no content, it will not be sent,
    /// unless it is configured to be sent if empty.
    ///
    /// # Arguments
    ///
    /// * `reason` - the reason the ping was triggered. Included in the
    ///   `ping_info.reason` part of the payload.
    pub fn submit(&self, reason: Option<&str>) {
        let mut cb = self.test_callback.lock().unwrap();
        let cb = cb.take();
        if let Some(cb) = cb {
            cb(reason)
        }

        self.inner.submit(reason.map(|s| s.to_string()))
    }

    /// **Test-only API**
    ///
    /// Attach a callback to be called right before a new ping is submitted.
    /// The provided function is called exactly once before submitting a ping.
    ///
    /// Note: The callback will be called on any call to submit.
    /// A ping might not be sent afterwards, e.g. if the ping is otherwise empty (and
    /// `send_if_empty` is `false`).
    pub fn test_before_next_submit(&self, cb: impl FnOnce(Option<&str>) + Send + 'static) {
        let mut test_callback = self.test_callback.lock().unwrap();

        let cb = Box::new(cb);
        *test_callback = Some(cb);
    }
}
