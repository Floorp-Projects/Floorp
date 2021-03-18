// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;

use crate::ipc::need_ipc;

/// A Glean ping.
///
/// See [Glean Pings](https://mozilla.github.io/glean/book/user/pings/index.html).
#[derive(Clone)]
pub enum Ping {
    Parent(glean::private::PingType),
    Child,
}

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
            Ping::Parent(glean::private::PingType::new(
                name,
                include_client_id,
                send_if_empty,
                reason_codes,
            ))
        }
    }
}

#[inherent(pub)]
impl glean::traits::Ping for Ping {
    /// Submits the ping for eventual uploading
    ///
    /// # Arguments
    ///
    /// * `reason` - the reason the ping was triggered. Included in the
    ///   `ping_info.reason` part of the payload.
    fn submit(&self, reason: Option<&str>) {
        match self {
            Ping::Parent(p) => {
                glean::traits::Ping::submit(p, reason);
            }
            Ping::Child => {
                log::error!("Unable to submit ping in non-main process. Ignoring.");
                // TODO: Record an error.
            }
        };
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
