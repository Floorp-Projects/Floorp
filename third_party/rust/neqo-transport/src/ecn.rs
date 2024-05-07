// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::ops::{AddAssign, Deref, DerefMut, Sub};

use enum_map::EnumMap;
use neqo_common::{qdebug, qinfo, qwarn, IpTosEcn};

use crate::{packet::PacketNumber, tracking::SentPacket};

/// The number of packets to use for testing a path for ECN capability.
pub const ECN_TEST_COUNT: usize = 10;

/// The state information related to testing a path for ECN capability.
/// See RFC9000, Appendix A.4.
#[derive(Debug, PartialEq, Clone)]
enum EcnValidationState {
    /// The path is currently being tested for ECN capability, with the number of probes sent so
    /// far on the path during the ECN validation.
    Testing(usize),
    /// The validation test has concluded but the path's ECN capability is not yet known.
    Unknown,
    /// The path is known to **not** be ECN capable.
    Failed,
    /// The path is known to be ECN capable.
    Capable,
}

impl Default for EcnValidationState {
    fn default() -> Self {
        EcnValidationState::Testing(0)
    }
}

/// The counts for different ECN marks.
#[derive(PartialEq, Eq, Debug, Clone, Copy, Default)]
pub struct EcnCount(EnumMap<IpTosEcn, u64>);

impl Deref for EcnCount {
    type Target = EnumMap<IpTosEcn, u64>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl DerefMut for EcnCount {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl EcnCount {
    pub fn new(not_ect: u64, ect0: u64, ect1: u64, ce: u64) -> Self {
        // Yes, the enum array order is different from the argument order.
        Self(EnumMap::from_array([not_ect, ect1, ect0, ce]))
    }

    /// Whether any of the ECN counts are non-zero.
    pub fn is_some(&self) -> bool {
        self[IpTosEcn::Ect0] > 0 || self[IpTosEcn::Ect1] > 0 || self[IpTosEcn::Ce] > 0
    }
}

impl Sub<EcnCount> for EcnCount {
    type Output = EcnCount;

    /// Subtract the ECN counts in `other` from `self`.
    fn sub(self, other: EcnCount) -> EcnCount {
        let mut diff = EcnCount::default();
        for (ecn, count) in &mut *diff {
            *count = self[ecn].saturating_sub(other[ecn]);
        }
        diff
    }
}

impl AddAssign<IpTosEcn> for EcnCount {
    fn add_assign(&mut self, ecn: IpTosEcn) {
        self[ecn] += 1;
    }
}

#[derive(Debug, Default)]
pub struct EcnInfo {
    /// The current state of ECN validation on this path.
    state: EcnValidationState,

    /// The largest ACK seen so far.
    largest_acked: PacketNumber,

    /// The ECN counts from the last ACK frame that increased `largest_acked`.
    baseline: EcnCount,
}

impl EcnInfo {
    /// Set the baseline (= the ECN counts from the last ACK Frame).
    pub fn set_baseline(&mut self, baseline: EcnCount) {
        self.baseline = baseline;
    }

    /// Expose the current baseline.
    pub fn baseline(&self) -> EcnCount {
        self.baseline
    }

    /// Count the number of packets sent out on this path during ECN validation.
    /// Exit ECN validation if the number of packets sent exceeds `ECN_TEST_COUNT`.
    /// We do not implement the part of the RFC that says to exit ECN validation if the time since
    /// the start of ECN validation exceeds 3 * PTO, since this seems to happen much too quickly.
    pub fn on_packet_sent(&mut self) {
        if let EcnValidationState::Testing(ref mut probes_sent) = &mut self.state {
            *probes_sent += 1;
            qdebug!("ECN probing: sent {} probes", probes_sent);
            if *probes_sent == ECN_TEST_COUNT {
                qdebug!("ECN probing concluded with {} probes sent", probes_sent);
                self.state = EcnValidationState::Unknown;
            }
        }
    }

    /// Process ECN counts from an ACK frame.
    ///
    /// Returns whether ECN counts contain new valid ECN CE marks.
    pub fn on_packets_acked(
        &mut self,
        acked_packets: &[SentPacket],
        ack_ecn: Option<EcnCount>,
    ) -> bool {
        let prev_baseline = self.baseline;

        self.validate_ack_ecn_and_update(acked_packets, ack_ecn);

        matches!(self.state, EcnValidationState::Capable)
            && (self.baseline - prev_baseline)[IpTosEcn::Ce] > 0
    }

    /// After the ECN validation test has ended, check if the path is ECN capable.
    pub fn validate_ack_ecn_and_update(
        &mut self,
        acked_packets: &[SentPacket],
        ack_ecn: Option<EcnCount>,
    ) {
        // RFC 9000, Appendix A.4:
        //
        // > From the "unknown" state, successful validation of the ECN counts in an ACK frame
        // > (see Section 13.4.2.1) causes the ECN state for the path to become "capable", unless
        // > no marked packet has been acknowledged.
        match self.state {
            EcnValidationState::Testing { .. } | EcnValidationState::Failed => return,
            EcnValidationState::Unknown | EcnValidationState::Capable => {}
        }

        // RFC 9000, Section 13.4.2.1:
        //
        // > Validating ECN counts from reordered ACK frames can result in failure. An endpoint MUST
        // > NOT fail ECN validation as a result of processing an ACK frame that does not increase
        // > the largest acknowledged packet number.
        let largest_acked = acked_packets.first().expect("must be there").pn;
        if largest_acked <= self.largest_acked {
            return;
        }

        // RFC 9000, Section 13.4.2.1:
        //
        // > An endpoint that receives an ACK frame with ECN counts therefore validates
        // > the counts before using them. It performs this validation by comparing newly
        // > received counts against those from the last successfully processed ACK frame.
        //
        // > If an ACK frame newly acknowledges a packet that the endpoint sent with
        // > either the ECT(0) or ECT(1) codepoint set, ECN validation fails if the
        // > corresponding ECN counts are not present in the ACK frame.
        let Some(ack_ecn) = ack_ecn else {
            qwarn!("ECN validation failed, no ECN counts in ACK frame");
            self.state = EcnValidationState::Failed;
            return;
        };

        // We always mark with ECT(0) - if at all - so we only need to check for that.
        //
        // > ECN validation also fails if the sum of the increase in ECT(0) and ECN-CE counts is
        // > less than the number of newly acknowledged packets that were originally sent with an
        // > ECT(0) marking.
        let newly_acked_sent_with_ect0: u64 = acked_packets
            .iter()
            .filter(|p| p.ecn_mark == IpTosEcn::Ect0)
            .count()
            .try_into()
            .unwrap();
        if newly_acked_sent_with_ect0 == 0 {
            qwarn!("ECN validation failed, no ECT(0) packets were newly acked");
            self.state = EcnValidationState::Failed;
            return;
        }
        let ecn_diff = ack_ecn - self.baseline;
        let sum_inc = ecn_diff[IpTosEcn::Ect0] + ecn_diff[IpTosEcn::Ce];
        if sum_inc < newly_acked_sent_with_ect0 {
            qwarn!(
                "ECN validation failed, ACK counted {} new marks, but {} of newly acked packets were sent with ECT(0)",
                sum_inc,
                newly_acked_sent_with_ect0
            );
            self.state = EcnValidationState::Failed;
        } else if ecn_diff[IpTosEcn::Ect1] > 0 {
            qwarn!("ECN validation failed, ACK counted ECT(1) marks that were never sent");
            self.state = EcnValidationState::Failed;
        } else {
            qinfo!("ECN validation succeeded, path is capable",);
            self.state = EcnValidationState::Capable;
        }
        self.baseline = ack_ecn;
        self.largest_acked = largest_acked;
    }

    /// The ECN mark to use for packets sent on this path.
    pub fn ecn_mark(&self) -> IpTosEcn {
        match self.state {
            EcnValidationState::Testing { .. } | EcnValidationState::Capable => IpTosEcn::Ect0,
            EcnValidationState::Failed | EcnValidationState::Unknown => IpTosEcn::NotEct,
        }
    }
}
