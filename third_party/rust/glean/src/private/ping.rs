// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

/// Stores information about a ping.
///
/// This is required so that given metric data queued on disk we can send
/// pings with the correct settings, e.g. whether it has a client_id.
#[derive(Clone, Debug)]
pub struct PingType {
    pub(crate) name: String,
    pub(crate) ping_type: glean_core::metrics::PingType,
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
        Self { name, ping_type }
    }

    /// Submits the ping.
    pub fn submit(&self, reason: Option<&str>) {
        crate::submit_ping(self, reason)
    }
}
