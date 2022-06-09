// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use neqo_qpack::QpackSettings;
use neqo_transport::ConnectionParameters;
use std::cmp::min;

const QPACK_MAX_TABLE_SIZE_DEFAULT: u64 = 65536;
const QPACK_TABLE_SIZE_LIMIT: u64 = (1 << 30) - 1;
const QPACK_MAX_BLOCKED_STREAMS_DEFAULT: u16 = 20;
const MAX_PUSH_STREAM_DEFAULT: u64 = 0;
const WEBTRANSPORT_DEFAULT: bool = false;

#[derive(Debug, Clone)]
pub struct Http3Parameters {
    conn_params: ConnectionParameters,
    qpack_settings: QpackSettings,
    max_concurrent_push_streams: u64,
    webtransport: bool,
}

impl Default for Http3Parameters {
    fn default() -> Self {
        Self {
            conn_params: ConnectionParameters::default(),
            qpack_settings: QpackSettings {
                max_table_size_encoder: QPACK_MAX_TABLE_SIZE_DEFAULT,
                max_table_size_decoder: QPACK_MAX_TABLE_SIZE_DEFAULT,
                max_blocked_streams: QPACK_MAX_BLOCKED_STREAMS_DEFAULT,
            },
            max_concurrent_push_streams: MAX_PUSH_STREAM_DEFAULT,
            webtransport: WEBTRANSPORT_DEFAULT,
        }
    }
}

impl Http3Parameters {
    #[must_use]
    pub fn get_connection_parameters(&self) -> &ConnectionParameters {
        &self.conn_params
    }

    #[must_use]
    pub fn connection_parameters(mut self, conn_params: ConnectionParameters) -> Self {
        self.conn_params = conn_params;
        self
    }

    /// # Panics
    /// The table size must be smaller than 1 << 30 by the spec.
    #[must_use]
    pub fn max_table_size_encoder(mut self, mut max_table: u64) -> Self {
        assert!(max_table <= QPACK_TABLE_SIZE_LIMIT);
        max_table = min(max_table, QPACK_TABLE_SIZE_LIMIT);
        self.qpack_settings.max_table_size_encoder = max_table;
        self
    }

    #[must_use]
    pub fn get_max_table_size_encoder(&self) -> u64 {
        self.qpack_settings.max_table_size_encoder
    }

    /// # Panics
    /// The table size must be smaller than 1 << 30 by the spec.
    #[must_use]
    pub fn max_table_size_decoder(mut self, mut max_table: u64) -> Self {
        assert!(max_table <= QPACK_TABLE_SIZE_LIMIT);
        max_table = min(max_table, QPACK_TABLE_SIZE_LIMIT);
        self.qpack_settings.max_table_size_decoder = max_table;
        self
    }

    #[must_use]
    pub fn get_max_table_size_decoder(&self) -> u64 {
        self.qpack_settings.max_table_size_decoder
    }

    #[must_use]
    pub fn max_blocked_streams(mut self, max_blocked: u16) -> Self {
        self.qpack_settings.max_blocked_streams = max_blocked;
        self
    }

    #[must_use]
    pub fn get_max_blocked_streams(&self) -> u16 {
        self.qpack_settings.max_blocked_streams
    }

    #[must_use]
    pub fn get_qpack_settings(&self) -> &QpackSettings {
        &self.qpack_settings
    }

    #[must_use]
    pub fn max_concurrent_push_streams(mut self, max_push_streams: u64) -> Self {
        self.max_concurrent_push_streams = max_push_streams;
        self
    }

    #[must_use]
    pub fn get_max_concurrent_push_streams(&self) -> u64 {
        self.max_concurrent_push_streams
    }

    #[must_use]
    pub fn webtransport(mut self, webtransport: bool) -> Self {
        self.webtransport = webtransport;
        self
    }

    #[must_use]
    pub fn get_webtransport(&self) -> bool {
        self.webtransport
    }
}
