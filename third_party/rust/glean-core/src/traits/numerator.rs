// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::ErrorType;
use crate::Rate;

// When changing this trait, ensure all operations are implemented in the
// related type in `../metrics`. (Except test_get_num_errors)
/// A description for the `NumeratorMetric` subtype of the [`RateMetric`](crate::metrics::RateMetric) type.
pub trait Numerator {
    /// Increases the numerator by `amount`.
    ///
    /// # Arguments
    ///
    /// * `amount` - The amount to increase by. Should be non-negative.
    ///
    /// ## Notes
    ///
    /// Logs an error if the `amount` is negative.
    fn add_to_numerator(&self, amount: i32);

    /// **Exported for test purposes.**
    ///
    /// Gets the currently stored value as a pair of integers.
    ///
    /// # Arguments
    ///
    /// * `ping_name` - the optional name of the ping to retrieve the metric
    ///                 for. Defaults to the first value in `send_in_pings`.
    ///
    /// This doesn't clear the stored value.
    fn test_get_value<'a, S: Into<Option<&'a str>>>(&self, ping_name: S) -> Option<Rate>;

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
