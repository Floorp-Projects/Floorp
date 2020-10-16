// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Congestion control
#![deny(clippy::pedantic)]

use std::fmt::{self, Display};

use crate::cc::{classic_cc::WindowAdjustment, MAX_DATAGRAM_SIZE};
use neqo_common::qinfo;

#[derive(Debug)]
pub struct NewReno {}

impl Default for NewReno {
    fn default() -> Self {
        Self {}
    }
}

impl Display for NewReno {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "NewReno")?;
        Ok(())
    }
}

impl WindowAdjustment for NewReno {
    fn on_packets_acked(&mut self, mut curr_cwnd: usize, mut acked_bytes: usize) -> (usize, usize) {
        if acked_bytes >= curr_cwnd {
            acked_bytes -= curr_cwnd;
            curr_cwnd += MAX_DATAGRAM_SIZE;
            qinfo!([self], "congestion avoidance += {}", MAX_DATAGRAM_SIZE);
        }
        (curr_cwnd, acked_bytes)
    }

    fn on_congestion_event(&mut self, curr_cwnd: usize, acked_bytes: usize) -> (usize, usize) {
        (curr_cwnd / 2, acked_bytes / 2)
    }
}
