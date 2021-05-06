// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::connection::{ConnectionIdManager, Role, LOCAL_ACTIVE_CID_LIMIT, LOCAL_IDLE_TIMEOUT};
use crate::recv_stream::RECV_BUFFER_SIZE;
use crate::stream_id::StreamType;
use crate::tparams::{self, PreferredAddress, TransportParameter, TransportParametersHandler};
use crate::{CongestionControlAlgorithm, QuicVersion, Res};
use std::convert::TryFrom;

const LOCAL_MAX_DATA: u64 = 0x3FFF_FFFF_FFFF_FFFF; // 2^62-1
const LOCAL_STREAM_LIMIT_BIDI: u64 = 16;
const LOCAL_STREAM_LIMIT_UNI: u64 = 16;

/// What to do with preferred addresses.
#[derive(Debug, Clone)]
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
#[derive(Debug, Clone)]
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
    preferred_address: PreferredAddressConfig,
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
            preferred_address: PreferredAddressConfig::Default,
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
    /// Asserts if `StreamType::UniDi` and `false` are passed as that is not a valid combination.
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
    /// Asserts if `StreamType::UniDi` and `false` are passed as that is not a valid combination.
    pub fn max_stream_data(mut self, stream_type: StreamType, remote: bool, v: u64) -> Self {
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

    pub fn create_transport_parameter(
        &self,
        role: Role,
        cid_manager: &mut ConnectionIdManager,
    ) -> Res<TransportParametersHandler> {
        let mut tps = TransportParametersHandler::default();
        // default parameters
        tps.local.set_integer(
            tparams::IDLE_TIMEOUT,
            u64::try_from(LOCAL_IDLE_TIMEOUT.as_millis()).unwrap(),
        );
        tps.local.set_integer(
            tparams::ACTIVE_CONNECTION_ID_LIMIT,
            u64::try_from(LOCAL_ACTIVE_CID_LIMIT).unwrap(),
        );
        tps.local.set_empty(tparams::DISABLE_MIGRATION);
        tps.local.set_empty(tparams::GREASE_QUIC_BIT);

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
        Ok(tps)
    }
}
