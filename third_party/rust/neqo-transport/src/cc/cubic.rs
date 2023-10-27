// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![deny(clippy::pedantic)]

use std::fmt::{self, Display};
use std::time::{Duration, Instant};

use crate::cc::{classic_cc::WindowAdjustment, MAX_DATAGRAM_SIZE_F64};
use neqo_common::qtrace;
use std::convert::TryFrom;

// CUBIC congestion control

// C is a constant fixed to determine the aggressiveness of window
// increase  in high BDP networks.
pub const CUBIC_C: f64 = 0.4;
pub const CUBIC_ALPHA: f64 = 3.0 * (1.0 - 0.7) / (1.0 + 0.7);

// CUBIC_BETA = 0.7;
pub const CUBIC_BETA_USIZE_DIVIDEND: usize = 7;
pub const CUBIC_BETA_USIZE_DIVISOR: usize = 10;

/// The fast convergence ratio further reduces the congestion window when a congestion event
/// occurs before reaching the previous `W_max`.
pub const CUBIC_FAST_CONVERGENCE: f64 = 0.85; // (1.0 + CUBIC_BETA) / 2.0;

/// The minimum number of multiples of the datagram size that need
/// to be received to cause an increase in the congestion window.
/// When there is no loss, Cubic can return to exponential increase, but
/// this value reduces the magnitude of the resulting growth by a constant factor.
/// A value of 1.0 would mean a return to the rate used in slow start.
const EXPONENTIAL_GROWTH_REDUCTION: f64 = 2.0;

/// Convert an integer congestion window value into a floating point value.
/// This has the effect of reducing larger values to `1<<53`.
/// If you have a congestion window that large, something is probably wrong.
fn convert_to_f64(v: usize) -> f64 {
    let mut f_64 = f64::try_from(u32::try_from(v >> 21).unwrap_or(u32::MAX)).unwrap();
    f_64 *= 2_097_152.0; // f_64 <<= 21
    f_64 += f64::try_from(u32::try_from(v & 0x1f_ffff).unwrap()).unwrap();
    f_64
}

#[derive(Debug)]
pub struct Cubic {
    last_max_cwnd: f64,
    estimated_tcp_cwnd: f64,
    k: f64,
    w_max: f64,
    ca_epoch_start: Option<Instant>,
    tcp_acked_bytes: f64,
}

impl Default for Cubic {
    fn default() -> Self {
        Self {
            last_max_cwnd: 0.0,
            estimated_tcp_cwnd: 0.0,
            k: 0.0,
            w_max: 0.0,
            ca_epoch_start: None,
            tcp_acked_bytes: 0.0,
        }
    }
}

impl Display for Cubic {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "Cubic [last_max_cwnd: {}, k: {}, w_max: {}, ca_epoch_start: {:?}]",
            self.last_max_cwnd, self.k, self.w_max, self.ca_epoch_start
        )?;
        Ok(())
    }
}

#[allow(clippy::doc_markdown)]
impl Cubic {
    /// Original equations is:
    /// K = cubic_root(W_max*(1-beta_cubic)/C) (Eq. 2 RFC8312)
    /// W_max is number of segments of the maximum segment size (MSS).
    ///
    /// K is actually the time that W_cubic(t) = C*(t-K)^3 + W_max (Eq. 1) would
    /// take to increase to W_max. We use bytes not MSS units, therefore this
    /// equation will be: W_cubic(t) = C*MSS*(t-K)^3 + W_max.
    ///
    /// From that equation we can calculate K as:
    /// K = cubic_root((W_max - W_cubic) / C / MSS);
    fn calc_k(&self, curr_cwnd: f64) -> f64 {
        ((self.w_max - curr_cwnd) / CUBIC_C / MAX_DATAGRAM_SIZE_F64).cbrt()
    }

    /// W_cubic(t) = C*(t-K)^3 + W_max (Eq. 1)
    /// t is relative to the start of the congestion avoidance phase and it is in seconds.
    fn w_cubic(&self, t: f64) -> f64 {
        CUBIC_C * (t - self.k).powi(3) * MAX_DATAGRAM_SIZE_F64 + self.w_max
    }

    fn start_epoch(&mut self, curr_cwnd_f64: f64, new_acked_f64: f64, now: Instant) {
        self.ca_epoch_start = Some(now);
        // reset tcp_acked_bytes and estimated_tcp_cwnd;
        self.tcp_acked_bytes = new_acked_f64;
        self.estimated_tcp_cwnd = curr_cwnd_f64;
        if self.last_max_cwnd <= curr_cwnd_f64 {
            self.w_max = curr_cwnd_f64;
            self.k = 0.0;
        } else {
            self.w_max = self.last_max_cwnd;
            self.k = self.calc_k(curr_cwnd_f64);
        }
        qtrace!([self], "New epoch");
    }
}

impl WindowAdjustment for Cubic {
    // This is because of the cast in the last line from f64 to usize.
    #[allow(clippy::cast_possible_truncation)]
    #[allow(clippy::cast_sign_loss)]
    fn bytes_for_cwnd_increase(
        &mut self,
        curr_cwnd: usize,
        new_acked_bytes: usize,
        min_rtt: Duration,
        now: Instant,
    ) -> usize {
        let curr_cwnd_f64 = convert_to_f64(curr_cwnd);
        let new_acked_f64 = convert_to_f64(new_acked_bytes);
        if self.ca_epoch_start.is_none() {
            // This is a start of a new congestion avoidance phase.
            self.start_epoch(curr_cwnd_f64, new_acked_f64, now);
        } else {
            self.tcp_acked_bytes += new_acked_f64;
        }

        let time_ca = self
            .ca_epoch_start
            .map_or(min_rtt, |t| {
                if now + min_rtt < t {
                    // This only happens when processing old packets
                    // that were saved and replayed with old timestamps.
                    min_rtt
                } else {
                    now + min_rtt - t
                }
            })
            .as_secs_f64();
        let target_cubic = self.w_cubic(time_ca);

        let tcp_cnt = self.estimated_tcp_cwnd / CUBIC_ALPHA;
        while self.tcp_acked_bytes > tcp_cnt {
            self.tcp_acked_bytes -= tcp_cnt;
            self.estimated_tcp_cwnd += MAX_DATAGRAM_SIZE_F64;
        }

        let target_cwnd = target_cubic.max(self.estimated_tcp_cwnd);

        // Calculate the number of bytes that would need to be acknowledged for an increase
        // of `MAX_DATAGRAM_SIZE` to match the increase of `target - cwnd / cwnd` as defined
        // in the specification (Sections 4.4 and 4.5).
        // The amount of data required therefore reduces asymptotically as the target increases.
        // If the target is not significantly higher than the congestion window, require a very large
        // amount of acknowledged data (effectively block increases).
        let mut acked_to_increase =
            MAX_DATAGRAM_SIZE_F64 * curr_cwnd_f64 / (target_cwnd - curr_cwnd_f64).max(1.0);

        // Limit increase to max 1 MSS per EXPONENTIAL_GROWTH_REDUCTION ack packets.
        // This effectively limits target_cwnd to (1 + 1 / EXPONENTIAL_GROWTH_REDUCTION) cwnd.
        acked_to_increase =
            acked_to_increase.max(EXPONENTIAL_GROWTH_REDUCTION * MAX_DATAGRAM_SIZE_F64);
        acked_to_increase as usize
    }

    fn reduce_cwnd(&mut self, curr_cwnd: usize, acked_bytes: usize) -> (usize, usize) {
        let curr_cwnd_f64 = convert_to_f64(curr_cwnd);
        // Fast Convergence
        // If congestion event occurs before the maximum congestion window before the last congestion event,
        // we reduce the the maximum congestion window and thereby W_max.
        // check cwnd + MAX_DATAGRAM_SIZE instead of cwnd because with cwnd in bytes, cwnd may be slightly off.
        self.last_max_cwnd = if curr_cwnd_f64 + MAX_DATAGRAM_SIZE_F64 < self.last_max_cwnd {
            curr_cwnd_f64 * CUBIC_FAST_CONVERGENCE
        } else {
            curr_cwnd_f64
        };
        self.ca_epoch_start = None;
        (
            curr_cwnd * CUBIC_BETA_USIZE_DIVIDEND / CUBIC_BETA_USIZE_DIVISOR,
            acked_bytes * CUBIC_BETA_USIZE_DIVIDEND / CUBIC_BETA_USIZE_DIVISOR,
        )
    }

    fn on_app_limited(&mut self) {
        // Reset ca_epoch_start. Let it start again when the congestion controller
        // exits the app-limited period.
        self.ca_epoch_start = None;
    }

    #[cfg(test)]
    fn last_max_cwnd(&self) -> f64 {
        self.last_max_cwnd
    }

    #[cfg(test)]
    fn set_last_max_cwnd(&mut self, last_max_cwnd: f64) {
        self.last_max_cwnd = last_max_cwnd;
    }
}
