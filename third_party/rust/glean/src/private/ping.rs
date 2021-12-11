// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::{Arc, Mutex};

use inherent::inherent;

type BoxedCallback = Box<dyn FnOnce(Option<&str>) + Send + 'static>;

/// A ping is a bundle of related metrics, gathered in a payload to be transmitted.
///
/// The ping payload will be encoded in JSON format and contains shared information data.
#[derive(Clone)]
pub struct PingType {
    pub(crate) name: String,
    pub(crate) ping_type: glean_core::metrics::PingType,

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
    /// * `reason_codes` - The valid reason codes for this ping.
    pub fn new<A: Into<String>>(
        name: A,
        include_client_id: bool,
        send_if_empty: bool,
        reason_codes: Vec<String>,
    ) -> Self {
        let name = name.into();
        let ping_type = glean_core::metrics::PingType::new(
            name.clone(),
            include_client_id,
            send_if_empty,
            reason_codes,
        );

        let me = Self {
            name,
            ping_type,
            test_callback: Arc::new(Default::default()),
        };
        crate::register_ping_type(&me);
        me
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

#[inherent(pub)]
impl glean_core::traits::Ping for PingType {
    fn submit(&self, reason: Option<&str>) {
        let mut cb = self.test_callback.lock().unwrap();
        let cb = cb.take();
        if let Some(cb) = cb {
            cb(reason)
        }

        crate::submit_ping(self, reason)
    }
}
