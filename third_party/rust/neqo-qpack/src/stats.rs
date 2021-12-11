// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Tracking of some useful statistics.

#[derive(Default, Debug, Clone)]
/// `QPack` statistics
pub struct Stats {
    pub dynamic_table_inserts: usize,
    // This is the munber of header blockes that reference the dynamic table.
    pub dynamic_table_references: usize,
    pub stream_cancelled_recv: usize,
    pub header_acks_recv: usize,
}
