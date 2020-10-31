// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Tracks possibly-redundant flow control signals from other code and converts
// into flow control frames needing to be sent to the remote.

use std::collections::HashMap;
use std::mem;

use neqo_common::{qinfo, qwarn, Encoder};
use smallvec::{smallvec, SmallVec};

use crate::frame::{Frame, StreamType};
use crate::packet::PacketBuilder;
use crate::recovery::RecoveryToken;
use crate::recv_stream::RecvStreams;
use crate::send_stream::SendStreams;
use crate::stats::FrameStats;
use crate::stream_id::{StreamId, StreamIndex, StreamIndexes};
use crate::AppError;

type FlowFrame = Frame<'static>;
pub type FlowControlRecoveryToken = FlowFrame;

#[derive(Debug, Default)]
pub struct FlowMgr {
    // Discriminant as key ensures only 1 of every frame type will be queued.
    from_conn: HashMap<mem::Discriminant<FlowFrame>, FlowFrame>,

    // (id, discriminant) as key ensures only 1 of every frame type per stream
    // will be queued.
    from_streams: HashMap<(StreamId, mem::Discriminant<FlowFrame>), FlowFrame>,

    // (stream_type, discriminant) as key ensures only 1 of every frame type
    // per stream type will be queued.
    from_stream_types: HashMap<(StreamType, mem::Discriminant<FlowFrame>), FlowFrame>,

    used_data: u64,
    max_data: u64,
}

impl FlowMgr {
    pub fn conn_credit_avail(&self) -> u64 {
        self.max_data - self.used_data
    }

    pub fn conn_increase_credit_used(&mut self, amount: u64) {
        self.used_data += amount;
        assert!(self.used_data <= self.max_data)
    }

    // Dummy DataBlocked frame for discriminant use below

    /// Returns whether max credit was actually increased.
    pub fn conn_increase_max_credit(&mut self, new: u64) -> bool {
        const DB_FRAME: FlowFrame = Frame::DataBlocked { data_limit: 0 };

        if new > self.max_data {
            self.max_data = new;
            self.from_conn.remove(&mem::discriminant(&DB_FRAME));

            true
        } else {
            false
        }
    }

    // -- frames scoped on connection --

    pub fn data_blocked(&mut self) {
        let frame = Frame::DataBlocked {
            data_limit: self.max_data,
        };
        self.from_conn.insert(mem::discriminant(&frame), frame);
    }

    pub fn path_response(&mut self, data: [u8; 8]) {
        let frame = Frame::PathResponse { data };
        self.from_conn.insert(mem::discriminant(&frame), frame);
    }

    pub fn max_data(&mut self, maximum_data: u64) {
        let frame = Frame::MaxData { maximum_data };
        self.from_conn.insert(mem::discriminant(&frame), frame);
    }

    // -- frames scoped on stream --

    /// Indicate to receiving remote the stream is reset
    pub fn stream_reset(
        &mut self,
        stream_id: StreamId,
        application_error_code: AppError,
        final_size: u64,
    ) {
        let frame = Frame::ResetStream {
            stream_id,
            application_error_code,
            final_size,
        };
        self.from_streams
            .insert((stream_id, mem::discriminant(&frame)), frame);
    }

    /// Indicate to sending remote we are no longer interested in the stream
    pub fn stop_sending(&mut self, stream_id: StreamId, application_error_code: AppError) {
        let frame = Frame::StopSending {
            stream_id,
            application_error_code,
        };
        self.from_streams
            .insert((stream_id, mem::discriminant(&frame)), frame);
    }

    /// Update sending remote with more credits
    pub fn max_stream_data(&mut self, stream_id: StreamId, maximum_stream_data: u64) {
        let frame = Frame::MaxStreamData {
            stream_id,
            maximum_stream_data,
        };
        self.from_streams
            .insert((stream_id, mem::discriminant(&frame)), frame);
    }

    /// Don't send stream data updates if no more data is coming
    pub fn clear_max_stream_data(&mut self, stream_id: StreamId) {
        let frame = Frame::MaxStreamData {
            stream_id,
            maximum_stream_data: 0,
        };
        self.from_streams
            .remove(&(stream_id, mem::discriminant(&frame)));
    }

    /// Indicate to receiving remote we need more credits
    pub fn stream_data_blocked(&mut self, stream_id: StreamId, stream_data_limit: u64) {
        let frame = Frame::StreamDataBlocked {
            stream_id,
            stream_data_limit,
        };
        self.from_streams
            .insert((stream_id, mem::discriminant(&frame)), frame);
    }

    // -- frames scoped on stream type --

    pub fn max_streams(&mut self, stream_limit: StreamIndex, stream_type: StreamType) {
        let frame = Frame::MaxStreams {
            stream_type,
            maximum_streams: stream_limit,
        };
        self.from_stream_types
            .insert((stream_type, mem::discriminant(&frame)), frame);
    }

    pub fn streams_blocked(&mut self, stream_limit: StreamIndex, stream_type: StreamType) {
        let frame = Frame::StreamsBlocked {
            stream_type,
            stream_limit,
        };
        self.from_stream_types
            .insert((stream_type, mem::discriminant(&frame)), frame);
    }

    pub fn peek(&self) -> Option<&Frame> {
        if let Some(key) = self.from_conn.keys().next() {
            self.from_conn.get(key)
        } else if let Some(key) = self.from_streams.keys().next() {
            self.from_streams.get(key)
        } else if let Some(key) = self.from_stream_types.keys().next() {
            self.from_stream_types.get(key)
        } else {
            None
        }
    }

    pub(crate) fn acked(
        &mut self,
        token: &FlowControlRecoveryToken,
        send_streams: &mut SendStreams,
    ) {
        const RESET_STREAM: &Frame = &Frame::ResetStream {
            stream_id: StreamId::new(0),
            application_error_code: 0,
            final_size: 0,
        };

        if let Frame::ResetStream { stream_id, .. } = token {
            qinfo!("Reset received stream={}", stream_id.as_u64());

            if self
                .from_streams
                .remove(&(*stream_id, mem::discriminant(RESET_STREAM)))
                .is_some()
            {
                qinfo!("Removed RESET_STREAM frame for {}", stream_id.as_u64());
            }

            send_streams.reset_acked(*stream_id);
        }
    }

    pub(crate) fn lost(
        &mut self,
        token: &FlowControlRecoveryToken,
        send_streams: &mut SendStreams,
        recv_streams: &mut RecvStreams,
        indexes: &mut StreamIndexes,
    ) {
        match *token {
            // Always resend ResetStream if lost
            Frame::ResetStream {
                stream_id,
                application_error_code,
                final_size,
            } => {
                qinfo!(
                    "Reset lost stream={} err={} final_size={}",
                    stream_id.as_u64(),
                    application_error_code,
                    final_size
                );
                if send_streams.get(stream_id).is_ok() {
                    self.stream_reset(stream_id, application_error_code, final_size);
                }
            }
            // Resend MaxStreams if lost (with updated value)
            Frame::MaxStreams { stream_type, .. } => {
                let local_max = match stream_type {
                    StreamType::BiDi => &mut indexes.local_max_stream_bidi,
                    StreamType::UniDi => &mut indexes.local_max_stream_uni,
                };

                self.max_streams(*local_max, stream_type)
            }
            // Only resend "*Blocked" frames if still blocked
            Frame::DataBlocked { .. } => {
                if self.conn_credit_avail() == 0 {
                    self.data_blocked()
                }
            }
            Frame::StreamDataBlocked { stream_id, .. } => {
                if let Ok(ss) = send_streams.get(stream_id) {
                    if ss.credit_avail() == 0 {
                        self.stream_data_blocked(stream_id, ss.max_stream_data())
                    }
                }
            }
            Frame::StreamsBlocked { stream_type, .. } => match stream_type {
                StreamType::UniDi => {
                    if indexes.remote_next_stream_uni >= indexes.remote_max_stream_uni {
                        self.streams_blocked(indexes.remote_max_stream_uni, StreamType::UniDi);
                    }
                }
                StreamType::BiDi => {
                    if indexes.remote_next_stream_bidi >= indexes.remote_max_stream_bidi {
                        self.streams_blocked(indexes.remote_max_stream_bidi, StreamType::BiDi);
                    }
                }
            },
            // Resend StopSending
            Frame::StopSending {
                stream_id,
                application_error_code,
            } => self.stop_sending(stream_id, application_error_code),
            Frame::MaxStreamData { stream_id, .. } => {
                if let Some(rs) = recv_streams.get_mut(&stream_id) {
                    if let Some(msd) = rs.max_stream_data() {
                        self.max_stream_data(stream_id, msd)
                    }
                }
            }
            Frame::PathResponse { .. } => qinfo!("Path Response lost, not re-sent"),
            _ => qwarn!("Unexpected Flow frame {:?} lost, not re-sent", token),
        }
    }

    pub(crate) fn write_frames(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        while let Some(frame) = self.peek() {
            // All these frames are bags of varints, so we can just extract the
            // varints and use common code for writing.
            let values: SmallVec<[_; 3]> = match frame {
                Frame::ResetStream {
                    stream_id,
                    application_error_code,
                    final_size,
                } => {
                    stats.reset_stream += 1;
                    smallvec![stream_id.as_u64(), *application_error_code, *final_size]
                }
                Frame::StopSending {
                    stream_id,
                    application_error_code,
                } => {
                    stats.stop_sending += 1;
                    smallvec![stream_id.as_u64(), *application_error_code]
                }

                Frame::MaxStreams {
                    maximum_streams, ..
                } => {
                    stats.max_streams += 1;
                    smallvec![maximum_streams.as_u64()]
                }
                Frame::StreamsBlocked { stream_limit, .. } => {
                    stats.streams_blocked += 1;
                    smallvec![stream_limit.as_u64()]
                }

                Frame::MaxData { maximum_data } => {
                    stats.max_data += 1;
                    smallvec![*maximum_data]
                }
                Frame::DataBlocked { data_limit } => {
                    stats.data_blocked += 1;
                    smallvec![*data_limit]
                }

                Frame::MaxStreamData {
                    stream_id,
                    maximum_stream_data,
                } => {
                    stats.max_stream_data += 1;
                    smallvec![stream_id.as_u64(), *maximum_stream_data]
                }
                Frame::StreamDataBlocked {
                    stream_id,
                    stream_data_limit,
                } => {
                    stats.stream_data_blocked += 1;
                    smallvec![stream_id.as_u64(), *stream_data_limit]
                }

                // A special case, just write it out and move on..
                Frame::PathResponse { data } => {
                    stats.path_response += 1;
                    if builder.remaining() < 1 + data.len() {
                        builder.encode_varint(frame.get_type());
                        builder.encode(data);
                        tokens.push(RecoveryToken::Flow(self.next().unwrap()));
                        continue;
                    } else {
                        return;
                    }
                }

                _ => unreachable!("{:?}", frame),
            };
            debug_assert!(!values.spilled());

            if builder.remaining() >= values.iter().map(|&v| Encoder::varint_len(v)).sum() {
                builder.encode_varint(frame.get_type());
                for v in values {
                    builder.encode_varint(v);
                }
                tokens.push(RecoveryToken::Flow(self.next().unwrap()));
            } else {
                return;
            }
        }
    }
}

impl Iterator for FlowMgr {
    type Item = FlowFrame;

    /// Used by generator to get a flow control frame.
    fn next(&mut self) -> Option<Self::Item> {
        let first_key = self.from_conn.keys().next();
        if let Some(&first_key) = first_key {
            return self.from_conn.remove(&first_key);
        }

        let first_key = self.from_streams.keys().next();
        if let Some(&first_key) = first_key {
            return self.from_streams.remove(&first_key);
        }

        let first_key = self.from_stream_types.keys().next();
        if let Some(&first_key) = first_key {
            return self.from_stream_types.remove(&first_key);
        }

        None
    }
}
