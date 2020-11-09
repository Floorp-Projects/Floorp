// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::Arc;

use crate::{dispatcher, ipc::need_ipc};

/// A Glean ping.
///
/// See [Glean Pings](https://mozilla.github.io/glean/book/user/pings/index.html).
#[derive(Clone, Debug)]
pub enum Ping {
    Parent(Arc<PingImpl>),
    Child,
}
#[derive(Debug)]
pub struct PingImpl(glean_core::metrics::PingType);

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
        if need_ipc() {
            Ping::Child
        } else {
            Ping::Parent(Arc::new(PingImpl::new(
                name,
                include_client_id,
                send_if_empty,
                reason_codes,
            )))
        }
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
    pub fn submit(&self, reason: Option<&str>) {
        match self {
            Ping::Parent(p) => {
                let ping = Arc::clone(&p);
                let reason = reason.map(|x| x.to_owned());
                dispatcher::launch(move || ping.submit(reason.as_deref()));
            }
            Ping::Child => {
                log::error!(
                    "Unable to submit ping {:?} in non-main process. Ignoring.",
                    self
                );
                // TODO: Record an error.
            }
        };
    }
}

impl PingImpl {
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

    pub fn submit(&self, reason: Option<&str>) {
        let res = crate::with_glean(|glean| self.0.submit(glean, reason).unwrap_or(false));
        if res {
            crate::ping_upload::check_for_uploads();
        }
    }
}

#[cfg(test)]
mod test {
    use once_cell::sync::Lazy;

    use super::*;
    use crate::common_test::*;

    // Smoke test for what should be the generated code.
    static PROTOTYPE_PING: Lazy<Ping> = Lazy::new(|| Ping::new("prototype", false, true, vec![]));

    #[test]
    fn smoke_test_custom_ping() {
        let _lock = lock_test();

        // We can only check that nothing explodes.
        // More comprehensive tests are blocked on bug 1673660.
        PROTOTYPE_PING.submit(None);
    }
}
