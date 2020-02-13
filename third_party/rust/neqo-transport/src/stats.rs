// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Tracking of some useful statistics.

#[derive(Default, Debug)]
/// Connection statistics
pub struct Stats {
    /// Total packets received
    pub packets_rx: usize,
    /// Total packets sent
    pub packets_tx: usize,
    /// Duplicate packets received
    pub dups_rx: usize,
    /// Dropped datagrams, or parts thereof
    pub dropped_rx: usize,
}
