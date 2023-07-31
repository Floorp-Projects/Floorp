// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Congestion control
#![deny(clippy::pedantic)]

use crate::{path::PATH_MTU_V6, tracking::SentPacket, Error};
use neqo_common::qlog::NeqoQlog;

use std::{
    fmt::{Debug, Display},
    str::FromStr,
    time::{Duration, Instant},
};

mod classic_cc;
mod cubic;
mod new_reno;

pub use classic_cc::{ClassicCongestionControl, CWND_INITIAL, CWND_INITIAL_PKTS, CWND_MIN};
pub use cubic::Cubic;
pub use new_reno::NewReno;

pub const MAX_DATAGRAM_SIZE: usize = PATH_MTU_V6;
#[allow(clippy::cast_precision_loss)]
pub const MAX_DATAGRAM_SIZE_F64: f64 = MAX_DATAGRAM_SIZE as f64;

pub trait CongestionControl: Display + Debug {
    fn set_qlog(&mut self, qlog: NeqoQlog);

    #[must_use]
    fn cwnd(&self) -> usize;

    #[must_use]
    fn bytes_in_flight(&self) -> usize;

    #[must_use]
    fn cwnd_avail(&self) -> usize;

    fn on_packets_acked(&mut self, acked_pkts: &[SentPacket], min_rtt: Duration, now: Instant);

    /// Returns true if the congestion window was reduced.
    fn on_packets_lost(
        &mut self,
        first_rtt_sample_time: Option<Instant>,
        prev_largest_acked_sent: Option<Instant>,
        pto: Duration,
        lost_packets: &[SentPacket],
    ) -> bool;

    #[must_use]
    fn recovery_packet(&self) -> bool;

    fn discard(&mut self, pkt: &SentPacket);

    fn on_packet_sent(&mut self, pkt: &SentPacket);

    fn discard_in_flight(&mut self);
}

#[derive(Debug, Copy, Clone)]
pub enum CongestionControlAlgorithm {
    NewReno,
    Cubic,
}

// A `FromStr` implementation so that this can be used in command-line interfaces.
impl FromStr for CongestionControlAlgorithm {
    type Err = Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s.trim().to_ascii_lowercase().as_str() {
            "newreno" | "reno" => Ok(Self::NewReno),
            "cubic" => Ok(Self::Cubic),
            _ => Err(Error::InvalidInput),
        }
    }
}

#[cfg(test)]
mod tests;
