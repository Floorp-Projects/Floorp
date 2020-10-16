// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Congestion control
#![deny(clippy::pedantic)]

use crate::path::PATH_MTU_V6;
use crate::tracking::SentPacket;
use neqo_common::qlog::NeqoQlog;

use std::fmt::{Debug, Display};
use std::time::{Duration, Instant};

mod classic_cc;
mod new_reno;

pub use classic_cc::ClassicCongestionControl;
pub use classic_cc::{CWND_INITIAL_PKTS, CWND_MIN};
pub use new_reno::NewReno;

pub const MAX_DATAGRAM_SIZE: usize = PATH_MTU_V6;

pub trait CongestionControl: Display + Debug {
    fn set_qlog(&mut self, qlog: NeqoQlog);

    fn cwnd(&self) -> usize;

    fn bytes_in_flight(&self) -> usize;

    fn cwnd_avail(&self) -> usize;

    fn on_packets_acked(&mut self, acked_pkts: &[SentPacket]);

    fn on_packets_lost(
        &mut self,
        first_rtt_sample_time: Option<Instant>,
        prev_largest_acked_sent: Option<Instant>,
        pto: Duration,
        lost_packets: &[SentPacket],
    );

    fn recovery_packet(&self) -> bool;

    fn discard(&mut self, pkt: &SentPacket);

    fn on_packet_sent(&mut self, pkt: &SentPacket);
}

pub enum CongestionControlAlgorithm {
    NewReno,
}
