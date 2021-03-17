// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::stream_id::{StreamIndex, StreamType};
use crate::tparams::PreferredAddress;
use crate::{CongestionControlAlgorithm, QuicVersion};

const LOCAL_STREAM_LIMIT_BIDI: StreamIndex = StreamIndex::new(16);
const LOCAL_STREAM_LIMIT_UNI: StreamIndex = StreamIndex::new(16);

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
/// This collect like initial limits, protocol version and congestion control.
#[derive(Debug, Clone)]
pub struct ConnectionParameters {
    quic_version: QuicVersion,
    cc_algorithm: CongestionControlAlgorithm,
    max_streams_bidi: StreamIndex,
    max_streams_uni: StreamIndex,
    preferred_address: PreferredAddressConfig,
}

impl Default for ConnectionParameters {
    fn default() -> Self {
        Self {
            quic_version: QuicVersion::default(),
            cc_algorithm: CongestionControlAlgorithm::NewReno,
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

    pub fn get_max_streams(&self, stream_type: StreamType) -> StreamIndex {
        match stream_type {
            StreamType::BiDi => self.max_streams_bidi,
            StreamType::UniDi => self.max_streams_uni,
        }
    }

    pub fn max_streams(mut self, stream_type: StreamType, v: StreamIndex) -> Self {
        assert!(v.as_u64() <= (1 << 60), "max_streams is too large");
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
}
