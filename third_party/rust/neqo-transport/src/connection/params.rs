// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::connection::{ConnectionIdManager, Role, LOCAL_ACTIVE_CID_LIMIT};
pub use crate::recovery::FAST_PTO_SCALE;
use crate::recv_stream::RECV_BUFFER_SIZE;
use crate::rtt::GRANULARITY;
use crate::stream_id::StreamType;
use crate::tparams::{self, PreferredAddress, TransportParameter, TransportParametersHandler};
use crate::tracking::DEFAULT_ACK_DELAY;
use crate::{CongestionControlAlgorithm, QuicVersion, Res};
use std::cmp::max;
use std::convert::TryFrom;
use std::time::Duration;

const LOCAL_MAX_DATA: u64 = 0x3FFF_FFFF_FFFF_FFFF; // 2^62-1
const LOCAL_STREAM_LIMIT_BIDI: u64 = 16;
const LOCAL_STREAM_LIMIT_UNI: u64 = 16;
/// See `ConnectionParameters.ack_ratio` for a discussion of this value.
pub const ACK_RATIO_SCALE: u8 = 10;
/// By default, aim to have the peer acknowledge 4 times per round trip time.
/// See `ConnectionParameters.ack_ratio` for more.
const DEFAULT_ACK_RATIO: u8 = 4 * ACK_RATIO_SCALE;
/// The local value for the idle timeout period.
const DEFAULT_IDLE_TIMEOUT: Duration = Duration::from_secs(30);
const MAX_QUEUED_DATAGRAMS_DEFAULT: usize = 10;

/// What to do with preferred addresses.
#[derive(Debug, Clone, Copy)]
pub enum PreferredAddressConfig {
    /// Disabled, whether for client or server.
    Disabled,
    /// Enabled at a client, disabled at a server.
    Default,
    /// Enabled at both client and server.
    Address(PreferredAddress),
}

/// ConnectionParameters use for setting intitial value for QUIC parameters.
/// This collects configuration like initial limits, protocol version, and
/// congestion control algorithm.
#[derive(Debug, Clone, Copy)]
pub struct ConnectionParameters {
    quic_version: QuicVersion,
    cc_algorithm: CongestionControlAlgorithm,
    /// Initial connection-level flow control limit.
    max_data: u64,
    /// Initial flow control limit for receiving data on bidirectional streams that the peer creates.
    max_stream_data_bidi_remote: u64,
    /// Initial flow control limit for receiving data on bidirectional streams that this endpoint creates.
    max_stream_data_bidi_local: u64,
    /// Initial flow control limit for receiving data on unidirectional streams that the peer creates.
    max_stream_data_uni: u64,
    /// Initial limit on bidirectional streams that the peer creates.
    max_streams_bidi: u64,
    /// Initial limit on unidirectional streams that this endpoint creates.
    max_streams_uni: u64,
    /// The ACK ratio determines how many acknowledgements we will request as a
    /// fraction of both the current congestion window (expressed in packets) and
    /// as a fraction of the current round trip time.  This value is scaled by
    /// `ACK_RATIO_SCALE`; that is, if the goal is to have at least five
    /// acknowledgments every round trip, set the value to `5 * ACK_RATIO_SCALE`.
    /// Values less than `ACK_RATIO_SCALE` are clamped to `ACK_RATIO_SCALE`.
    ack_ratio: u8,
    /// The duration of the idle timeout for the connection.
    idle_timeout: Duration,
    preferred_address: PreferredAddressConfig,
    datagram_size: u64,
    outgoing_datagram_queue: usize,
    incoming_datagram_queue: usize,
    fast_pto: u8,
}

impl Default for ConnectionParameters {
    fn default() -> Self {
        Self {
            quic_version: QuicVersion::default(),
            cc_algorithm: CongestionControlAlgorithm::NewReno,
            max_data: LOCAL_MAX_DATA,
            max_stream_data_bidi_remote: u64::try_from(RECV_BUFFER_SIZE).unwrap(),
            max_stream_data_bidi_local: u64::try_from(RECV_BUFFER_SIZE).unwrap(),
            max_stream_data_uni: u64::try_from(RECV_BUFFER_SIZE).unwrap(),
            max_streams_bidi: LOCAL_STREAM_LIMIT_BIDI,
            max_streams_uni: LOCAL_STREAM_LIMIT_UNI,
            ack_ratio: DEFAULT_ACK_RATIO,
            idle_timeout: DEFAULT_IDLE_TIMEOUT,
            preferred_address: PreferredAddressConfig::Default,
            datagram_size: 0,
            outgoing_datagram_queue: MAX_QUEUED_DATAGRAMS_DEFAULT,
            incoming_datagram_queue: MAX_QUEUED_DATAGRAMS_DEFAULT,
            fast_pto: FAST_PTO_SCALE,
        }
    }
}

impl ConnectionParameters {
    pub fn get_quic_version(&self) -> QuicVersion {
        self.quic_version
    }

    pub fn quic_version(mut self, v: QuicVersion) -> Self {
        self.quic_version = v;
        self
    }

    pub fn get_cc_algorithm(&self) -> CongestionControlAlgorithm {
        self.cc_algorithm
    }

    pub fn cc_algorithm(mut self, v: CongestionControlAlgorithm) -> Self {
        self.cc_algorithm = v;
        self
    }

    pub fn get_max_data(&self) -> u64 {
        self.max_data
    }

    pub fn max_data(mut self, v: u64) -> Self {
        self.max_data = v;
        self
    }

    pub fn get_max_streams(&self, stream_type: StreamType) -> u64 {
        match stream_type {
            StreamType::BiDi => self.max_streams_bidi,
            StreamType::UniDi => self.max_streams_uni,
        }
    }

    /// # Panics
    /// If v > 2^60 (the maximum allowed by the protocol).
    pub fn max_streams(mut self, stream_type: StreamType, v: u64) -> Self {
        assert!(v <= (1 << 60), "max_streams is too large");
        match stream_type {
            StreamType::BiDi => {
                self.max_streams_bidi = v;
            }
            StreamType::UniDi => {
                self.max_streams_uni = v;
            }
        }
        self
    }

    /// Get the maximum stream data that we will accept on different types of streams.
    /// # Panics
    /// If `StreamType::UniDi` and `false` are passed as that is not a valid combination.
    pub fn get_max_stream_data(&self, stream_type: StreamType, remote: bool) -> u64 {
        match (stream_type, remote) {
            (StreamType::BiDi, false) => self.max_stream_data_bidi_local,
            (StreamType::BiDi, true) => self.max_stream_data_bidi_remote,
            (StreamType::UniDi, false) => {
                panic!("Can't get receive limit on a stream that can only be sent.")
            }
            (StreamType::UniDi, true) => self.max_stream_data_uni,
        }
    }

    /// Set the maximum stream data that we will accept on different types of streams.
    /// # Panics
    /// If `StreamType::UniDi` and `false` are passed as that is not a valid combination
    /// or if v >= 62 (the maximum allowed by the protocol).
    pub fn max_stream_data(mut self, stream_type: StreamType, remote: bool, v: u64) -> Self {
        assert!(v < (1 << 62), "max stream data is too large");
        match (stream_type, remote) {
            (StreamType::BiDi, false) => {
                self.max_stream_data_bidi_local = v;
            }
            (StreamType::BiDi, true) => {
                self.max_stream_data_bidi_remote = v;
            }
            (StreamType::UniDi, false) => {
                panic!("Can't set receive limit on a stream that can only be sent.")
            }
            (StreamType::UniDi, true) => {
                self.max_stream_data_uni = v;
            }
        }
        self
    }

    /// Set a preferred address (which only has an effect for a server).
    pub fn preferred_address(mut self, preferred: PreferredAddress) -> Self {
        self.preferred_address = PreferredAddressConfig::Address(preferred);
        self
    }

    /// Disable the use of preferred addresses.
    pub fn disable_preferred_address(mut self) -> Self {
        self.preferred_address = PreferredAddressConfig::Disabled;
        self
    }

    pub fn get_preferred_address(&self) -> &PreferredAddressConfig {
        &self.preferred_address
    }

    pub fn ack_ratio(mut self, ack_ratio: u8) -> Self {
        self.ack_ratio = ack_ratio;
        self
    }

    pub fn get_ack_ratio(&self) -> u8 {
        self.ack_ratio
    }

    /// # Panics
    /// If `timeout` is 2^62 milliseconds or more.
    pub fn idle_timeout(mut self, timeout: Duration) -> Self {
        assert!(timeout.as_millis() < (1 << 62), "idle timeout is too long");
        self.idle_timeout = timeout;
        self
    }

    pub fn get_idle_timeout(&self) -> Duration {
        self.idle_timeout
    }

    pub fn get_datagram_size(&self) -> u64 {
        self.datagram_size
    }

    pub fn datagram_size(mut self, v: u64) -> Self {
        self.datagram_size = v;
        self
    }

    pub fn get_outgoing_datagram_queue(&self) -> usize {
        self.outgoing_datagram_queue
    }

    pub fn outgoing_datagram_queue(mut self, v: usize) -> Self {
        // The max queue length must be at least 1.
        self.outgoing_datagram_queue = max(v, 1);
        self
    }

    pub fn get_incoming_datagram_queue(&self) -> usize {
        self.incoming_datagram_queue
    }

    pub fn incoming_datagram_queue(mut self, v: usize) -> Self {
        // The max queue length must be at least 1.
        self.incoming_datagram_queue = max(v, 1);
        self
    }

    pub fn get_fast_pto(&self) -> u8 {
        self.fast_pto
    }

    /// Scale the PTO timer.  A value of `FAST_PTO_SCALE` follows the spec, a smaller
    /// value does not, but produces more probes with the intent of ensuring lower
    /// latency in the event of tail loss. A value of `FAST_PTO_SCALE/4` is quite
    /// aggressive. Smaller values (other than zero) are not rejected, but could be
    /// very wasteful. Values greater than `FAST_PTO_SCALE` delay probes and could
    /// reduce performance. It should not be possible to increase the PTO timer by
    /// too much based on the range of valid values, but a maximum value of 255 will
    /// result in very poor performance.
    /// Scaling PTO this way does not affect when persistent congestion is declared,
    /// but may change how many retransmissions are sent before declaring persistent
    /// congestion.
    ///
    /// # Panics
    /// A value of 0 is invalid and will cause a panic.
    pub fn fast_pto(mut self, scale: u8) -> Self {
        assert_ne!(scale, 0);
        self.fast_pto = scale;
        self
    }

    pub fn create_transport_parameter(
        &self,
        role: Role,
        cid_manager: &mut ConnectionIdManager,
    ) -> Res<TransportParametersHandler> {
        let mut tps = TransportParametersHandler::default();
        // default parameters
        tps.local.set_integer(
            tparams::ACTIVE_CONNECTION_ID_LIMIT,
            u64::try_from(LOCAL_ACTIVE_CID_LIMIT).unwrap(),
        );
        tps.local.set_empty(tparams::DISABLE_MIGRATION);
        tps.local.set_empty(tparams::GREASE_QUIC_BIT);
        tps.local.set_integer(
            tparams::MAX_ACK_DELAY,
            u64::try_from(DEFAULT_ACK_DELAY.as_millis()).unwrap(),
        );
        tps.local.set_integer(
            tparams::MIN_ACK_DELAY,
            u64::try_from(GRANULARITY.as_micros()).unwrap(),
        );

        // set configurable parameters
        tps.local
            .set_integer(tparams::INITIAL_MAX_DATA, self.max_data);
        tps.local.set_integer(
            tparams::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL,
            self.max_stream_data_bidi_local,
        );
        tps.local.set_integer(
            tparams::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE,
            self.max_stream_data_bidi_remote,
        );
        tps.local.set_integer(
            tparams::INITIAL_MAX_STREAM_DATA_UNI,
            self.max_stream_data_uni,
        );
        tps.local
            .set_integer(tparams::INITIAL_MAX_STREAMS_BIDI, self.max_streams_bidi);
        tps.local
            .set_integer(tparams::INITIAL_MAX_STREAMS_UNI, self.max_streams_uni);
        tps.local.set_integer(
            tparams::IDLE_TIMEOUT,
            u64::try_from(self.idle_timeout.as_millis()).unwrap_or(0),
        );
        if let PreferredAddressConfig::Address(preferred) = &self.preferred_address {
            if role == Role::Server {
                let (cid, srt) = cid_manager.preferred_address_cid()?;
                tps.local.set(
                    tparams::PREFERRED_ADDRESS,
                    TransportParameter::PreferredAddress {
                        v4: preferred.ipv4(),
                        v6: preferred.ipv6(),
                        cid,
                        srt,
                    },
                );
            }
        }
        tps.local
            .set_integer(tparams::MAX_DATAGRAM_FRAME_SIZE, self.datagram_size);
        Ok(tps)
    }
}
