// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use ffi_support::FfiStr;
use std::ffi::CString;

/// Stores information about a ping.
///
/// This is required so that given metric data queued on disk we can send
/// pings with the correct settings, e.g. whether it has a client_id.
#[derive(Clone, Debug)]
pub struct PingType {
    pub(crate) name: String,
    pub(crate) handle: u64,
}

impl PingType {
    /// Create a new ping type for the given name, whether to include the client ID and whether to
    /// send this ping empty.
    ///
    /// ## Arguments
    ///
    /// * `name` - The name of the ping.
    /// * `include_client_id` - Whether to include the client ID in the assembled ping when.
    /// * `send_if_empty` - Whether the ping should be sent empty or not.
    pub fn new<A: Into<String>>(name: A, include_client_id: bool, send_if_empty: bool) -> Self {
        let name = name.into();
        let ffi_name = CString::new(name.clone()).unwrap();
        let handle = glean_ffi::ping_type::glean_new_ping_type(
            FfiStr::from_cstr(&ffi_name),
            include_client_id as u8,
            send_if_empty as u8,
        );
        Self { name, handle }
    }

    /// Send the ping.
    ///
    /// ## Return value
    ///
    /// Returns true if a ping was assembled and queued, false otherwise.
    pub fn send(&self) -> bool {
        crate::send_ping(self)
    }
}
