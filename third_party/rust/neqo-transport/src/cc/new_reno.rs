// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Congestion control
#![deny(clippy::pedantic)]

use std::fmt::{self, Display};

use crate::cc::classic_cc::WindowAdjustment;
use std::time::{Duration, Instant};

#[derive(Debug, Default)]
pub struct NewReno {}

impl Display for NewReno {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "NewReno")?;
        Ok(())
    }
}

impl WindowAdjustment for NewReno {
    fn bytes_for_cwnd_increase(
        &mut self,
        curr_cwnd: usize,
        _new_acked_bytes: usize,
        _min_rtt: Duration,
        _now: Instant,
    ) -> usize {
        curr_cwnd
    }

    fn reduce_cwnd(&mut self, curr_cwnd: usize, acked_bytes: usize) -> (usize, usize) {
        (curr_cwnd / 2, acked_bytes / 2)
    }

    fn on_app_limited(&mut self) {}

    #[cfg(test)]
    fn last_max_cwnd(&self) -> f64 {
        0.0
    }

    #[cfg(test)]
    fn set_last_max_cwnd(&mut self, _last_max_cwnd: f64) {}
}
