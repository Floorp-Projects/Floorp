// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

/// A Glean ping.
///
/// See [Glean Pings](https://mozilla.github.io/glean/book/user/pings/index.html).
#[derive(Clone, Debug)]
pub struct Ping(glean_core::metrics::PingType);

impl Ping {
    /// Create a new ping type for the given name, whether to include the client ID and whether to
    /// send this ping empty.
    ///
    /// ## Arguments
    ///
    /// * `name` - The name of the ping.
    /// * `include_client_id` - Whether to include the client ID in the assembled ping when submitting.
    /// * `send_if_empty` - Whether the ping should be sent empty or not.
    /// * `reason_codes` - The valid reason codes for this ping.
    pub fn new<S: Into<String>>(
        name: S,
        include_client_id: bool,
        send_if_empty: bool,
        reason_codes: Vec<String>,
    ) -> Self {
        let ping = glean_core::metrics::PingType::new(
            name,
            include_client_id,
            send_if_empty,
            reason_codes,
        );

        crate::with_glean(|glean| {
            glean.register_ping_type(&ping);
        });

        Self(ping)
    }

    /// Collect and submit the ping for eventual upload.
    ///
    /// This will collect all stored data to be included in the ping.
    /// Data with lifetime `ping` will then be reset.
    ///
    /// If the ping is configured with `send_if_empty = false`
    /// and the ping currently contains no content,
    /// it will not be queued for upload.
    /// If the ping is configured with `send_if_empty = true`
    /// it will be queued for upload even if otherwise empty.
    ///
    /// Pings always contain the `ping_info` and `client_info` sections.
    /// See [ping sections](https://mozilla.github.io/glean/book/user/pings/index.html#ping-sections)
    /// for details.
    ///
    /// ## Parameters
    /// * `reason` - The reason the ping is being submitted.
    ///              Must be one of the configured `reason_codes`.
    ///
    /// ## Return value
    ///
    /// Returns true if a ping was assembled and queued, false otherwise.
    pub fn submit(&self, reason: Option<&str>) -> bool {
        let res = crate::with_glean(|glean| self.0.submit(glean, reason).unwrap_or(false));
        if res {
            crate::ping_upload::check_for_uploads();
        }
        res
    }
}
