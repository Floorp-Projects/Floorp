// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::ErrorType;

/// A description for the [`UrlMetric`](crate::metrics::UrlMetric) type.
///
/// When changing this trait, make sure all the operations are
/// implemented in the related type in `../metrics/`.
pub trait Url {
    /// Sets to the specified stringified URL.
    ///
    /// # Arguments
    ///
    /// * `glean` - The Glean instance this metric belongs to.
    /// * `value` - The stringified URL to set the metric to.
    ///
    /// ## Notes
    ///
    /// Truncates the value if it is longer than `MAX_URL_LENGTH` bytes and logs an error.
    fn set<S: Into<std::string::String>>(&self, value: S);

    /// **Exported for test purposes.**
    ///
    /// Gets the currently stored value as a string.
    ///
    /// This doesn't clear the stored value.
    ///
    /// # Arguments
    ///
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    fn test_get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<std::string::String>;

    /// **Exported for test purposes.**
    ///
    /// Gets the number of recorded errors for the given metric and error type.
    ///
    /// # Arguments
    ///
    /// * `error` - The type of error
    ///
    /// # Returns
    ///
    /// The number of errors reported.
    fn test_get_num_recorded_errors(&self, error: ErrorType) -> i32;
}
