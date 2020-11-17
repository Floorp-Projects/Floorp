// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::error::Result;

/// A description for the `PingType` type.
///
/// When changing this trait, make sure all the operations are
/// implemented in the related type in `../metrics/`.
pub trait Ping {
    /// Submits the ping for eventual uploading
    ///
    /// # Arguments
    ///
    /// * `reason` - the reason the ping was triggered. Included in the
    ///   `ping_info.reason` part of the payload.
    ///
    /// # Returns
    ///
    /// See [`Glean#submit_ping`](../struct.Glean.html#method.submit_ping) for details.
    fn submit(&self, reason: Option<&str>) -> Result<bool>;
}
