// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{
    ackrate::AckRate,
    cid::ConnectionIdEntry,
    crypto::CryptoRecoveryToken,
    quic_datagrams::DatagramTracking,
    send_stream::SendStreamRecoveryToken,
    stream_id::{StreamId, StreamType},
    tracking::AckToken,
};

#[derive(Debug, Clone)]
#[allow(clippy::module_name_repetitions)]
pub enum StreamRecoveryToken {
    Stream(SendStreamRecoveryToken),
    ResetStream {
        stream_id: StreamId,
    },
    StopSending {
        stream_id: StreamId,
    },

    MaxData(u64),
    DataBlocked(u64),

    MaxStreamData {
        stream_id: StreamId,
        max_data: u64,
    },
    StreamDataBlocked {
        stream_id: StreamId,
        limit: u64,
    },

    MaxStreams {
        stream_type: StreamType,
        max_streams: u64,
    },
    StreamsBlocked {
        stream_type: StreamType,
        limit: u64,
    },
}

#[derive(Debug, Clone)]
#[allow(clippy::module_name_repetitions)]
pub enum RecoveryToken {
    Stream(StreamRecoveryToken),
    Ack(AckToken),
    Crypto(CryptoRecoveryToken),
    HandshakeDone,
    KeepAlive, // Special PING.
    NewToken(usize),
    NewConnectionId(ConnectionIdEntry<[u8; 16]>),
    RetireConnectionId(u64),
    AckFrequency(AckRate),
    Datagram(DatagramTracking),
}
