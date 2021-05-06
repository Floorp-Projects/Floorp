// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Tracking of sent packets and detecting their loss.

#![deny(clippy::pedantic)]

use std::cmp::{max, min};
use std::time::{Duration, Instant};

use neqo_common::{qlog::NeqoQlog, qtrace};

use crate::qlog::{self, QlogMetric};
use crate::tracking::PacketNumberSpace;

/// The smallest time that the system timer (via `sleep()`, `nanosleep()`,
/// `select()`, or similar) can reliably deliver; see `neqo_common::hrtime`.
const GRANULARITY: Duration = Duration::from_millis(1);
/// The default value for the maximum time a peer can delay acknowledgment
/// of an ack-eliciting packet.
const DEFAULT_MAX_ACK_DELAY: Duration = Duration::from_millis(25);
// Defined in -recovery 6.2 as 333ms but using lower value.
const INITIAL_RTT: Duration = Duration::from_millis(100);

#[derive(Debug)]
#[allow(clippy::module_name_repetitions)]
pub struct RttEstimate {
    first_sample_time: Option<Instant>,
    latest_rtt: Duration,
    smoothed_rtt: Duration,
    rttvar: Duration,
    min_rtt: Duration,
    max_ack_delay: Duration,
}

impl RttEstimate {
    pub fn set_initial(&mut self, rtt: Duration) {
        qtrace!("initial RTT={:?}", rtt);
        if rtt >= GRANULARITY {
            // Ignore if the value is too small.
            self.init(rtt)
        }
    }

    fn init(&mut self, rtt: Duration) {
        // Only allow this when there are no samples.
        debug_assert!(self.first_sample_time.is_none());
        self.latest_rtt = rtt;
        self.min_rtt = rtt;
        self.smoothed_rtt = rtt;
        self.rttvar = rtt / 2;
    }

    pub fn set_max_ack_delay(&mut self, mad: Duration) {
        self.max_ack_delay = mad;
    }

    pub fn update(
        &mut self,
        mut qlog: &mut NeqoQlog,
        mut rtt_sample: Duration,
        ack_delay: Duration,
        confirmed: bool,
        now: Instant,
    ) {
        // Limit ack delay by max_ack_delay if confirmed.
        let ack_delay = if confirmed && ack_delay > self.max_ack_delay {
            self.max_ack_delay
        } else {
            ack_delay
        };

        // min_rtt ignores ack delay.
        self.min_rtt = min(self.min_rtt, rtt_sample);
        // Adjust for ack delay unless it goes below `min_rtt`.
        if rtt_sample - self.min_rtt >= ack_delay {
            rtt_sample -= ack_delay;
        }

        if self.first_sample_time.is_none() {
            self.init(rtt_sample);
            self.first_sample_time = Some(now);
        } else {
            // Calculate EWMA RTT (based on {{?RFC6298}}).
            let rttvar_sample = if self.smoothed_rtt > rtt_sample {
                self.smoothed_rtt - rtt_sample
            } else {
                rtt_sample - self.smoothed_rtt
            };

            self.latest_rtt = rtt_sample;
            self.rttvar = (self.rttvar * 3 + rttvar_sample) / 4;
            self.smoothed_rtt = (self.smoothed_rtt * 7 + rtt_sample) / 8;
        }
        qtrace!(
            "RTT latest={:?} -> estimate={:?}~{:?}",
            self.latest_rtt,
            self.smoothed_rtt,
            self.rttvar
        );
        qlog::metrics_updated(
            &mut qlog,
            &[
                QlogMetric::LatestRtt(self.latest_rtt),
                QlogMetric::MinRtt(self.min_rtt),
                QlogMetric::SmoothedRtt(self.smoothed_rtt),
            ],
        );
    }

    /// Get the estimated value.
    pub fn estimate(&self) -> Duration {
        self.smoothed_rtt
    }

    pub fn pto(&self, pn_space: PacketNumberSpace) -> Duration {
        self.estimate()
            + max(4 * self.rttvar, GRANULARITY)
            + if pn_space == PacketNumberSpace::ApplicationData {
                self.max_ack_delay
            } else {
                Duration::from_millis(0)
            }
    }

    /// Calculate the loss delay based on the current estimate and the last
    /// RTT measurement received.
    pub fn loss_delay(&self) -> Duration {
        // kTimeThreshold = 9/8
        // loss_delay = kTimeThreshold * max(latest_rtt, smoothed_rtt)
        // loss_delay = max(loss_delay, kGranularity)
        let rtt = max(self.latest_rtt, self.smoothed_rtt);
        max(rtt * 9 / 8, GRANULARITY)
    }

    pub fn first_sample_time(&self) -> Option<Instant> {
        self.first_sample_time
    }

    #[cfg(test)]
    pub fn latest(&self) -> Duration {
        self.latest_rtt
    }

    #[cfg(test)]
    pub fn rttvar(&self) -> Duration {
        self.rttvar
    }

    pub fn minimum(&self) -> Duration {
        self.min_rtt
    }
}

impl Default for RttEstimate {
    fn default() -> Self {
        Self {
            first_sample_time: None,
            latest_rtt: INITIAL_RTT,
            smoothed_rtt: INITIAL_RTT,
            rttvar: INITIAL_RTT / 2,
            min_rtt: INITIAL_RTT,
            max_ack_delay: DEFAULT_MAX_ACK_DELAY,
        }
    }
}
