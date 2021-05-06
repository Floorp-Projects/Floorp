// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Directly relating to QUIC frames.

use neqo_common::{qtrace, Decoder, Encoder};

use crate::cid::MAX_CONNECTION_ID_LEN;
use crate::packet::{PacketBuilder, PacketType};
use crate::stream_id::{StreamId, StreamType};
use crate::{AppError, ConnectionError, Error, Res, TransportError};

use std::convert::TryFrom;
use std::ops::RangeInclusive;

#[allow(clippy::module_name_repetitions)]
pub type FrameType = u64;

const FRAME_TYPE_PADDING: FrameType = 0x0;
pub const FRAME_TYPE_PING: FrameType = 0x1;
pub const FRAME_TYPE_ACK: FrameType = 0x2;
const FRAME_TYPE_ACK_ECN: FrameType = 0x3;
pub const FRAME_TYPE_RESET_STREAM: FrameType = 0x4;
pub const FRAME_TYPE_STOP_SENDING: FrameType = 0x5;
pub const FRAME_TYPE_CRYPTO: FrameType = 0x6;
pub const FRAME_TYPE_NEW_TOKEN: FrameType = 0x7;
const FRAME_TYPE_STREAM: FrameType = 0x8;
const FRAME_TYPE_STREAM_MAX: FrameType = 0xf;
pub const FRAME_TYPE_MAX_DATA: FrameType = 0x10;
pub const FRAME_TYPE_MAX_STREAM_DATA: FrameType = 0x11;
pub const FRAME_TYPE_MAX_STREAMS_BIDI: FrameType = 0x12;
pub const FRAME_TYPE_MAX_STREAMS_UNIDI: FrameType = 0x13;
pub const FRAME_TYPE_DATA_BLOCKED: FrameType = 0x14;
pub const FRAME_TYPE_STREAM_DATA_BLOCKED: FrameType = 0x15;
pub const FRAME_TYPE_STREAMS_BLOCKED_BIDI: FrameType = 0x16;
pub const FRAME_TYPE_STREAMS_BLOCKED_UNIDI: FrameType = 0x17;
pub const FRAME_TYPE_NEW_CONNECTION_ID: FrameType = 0x18;
pub const FRAME_TYPE_RETIRE_CONNECTION_ID: FrameType = 0x19;
pub const FRAME_TYPE_PATH_CHALLENGE: FrameType = 0x1a;
pub const FRAME_TYPE_PATH_RESPONSE: FrameType = 0x1b;
pub const FRAME_TYPE_CONNECTION_CLOSE_TRANSPORT: FrameType = 0x1c;
pub const FRAME_TYPE_CONNECTION_CLOSE_APPLICATION: FrameType = 0x1d;
pub const FRAME_TYPE_HANDSHAKE_DONE: FrameType = 0x1e;

const STREAM_FRAME_BIT_FIN: u64 = 0x01;
const STREAM_FRAME_BIT_LEN: u64 = 0x02;
const STREAM_FRAME_BIT_OFF: u64 = 0x04;

#[derive(PartialEq, Eq, Debug, PartialOrd, Ord, Clone, Copy)]
pub enum CloseError {
    Transport(TransportError),
    Application(AppError),
}

impl CloseError {
    fn frame_type_bit(self) -> u64 {
        match self {
            Self::Transport(_) => 0,
            Self::Application(_) => 1,
        }
    }

    fn from_type_bit(bit: u64, code: u64) -> Self {
        if (bit & 0x01) == 0 {
            Self::Transport(code)
        } else {
            Self::Application(code)
        }
    }

    pub fn code(&self) -> u64 {
        match self {
            Self::Transport(c) | Self::Application(c) => *c,
        }
    }
}

impl From<ConnectionError> for CloseError {
    fn from(err: ConnectionError) -> Self {
        match err {
            ConnectionError::Transport(c) => Self::Transport(c.code()),
            ConnectionError::Application(c) => Self::Application(c),
        }
    }
}

#[derive(PartialEq, Debug, Default, Clone)]
pub struct AckRange {
    pub(crate) gap: u64,
    pub(crate) range: u64,
}

/// A lot of frames here are just a collection of varints.
/// This helper functions writes a frame like that safely, returning `true` if
/// a frame was written.
pub fn write_varint_frame(builder: &mut PacketBuilder, values: &[u64]) -> Res<bool> {
    let write = builder.remaining()
        >= values
            .iter()
            .map(|&v| Encoder::varint_len(v))
            .sum::<usize>();
    if write {
        for v in values {
            builder.encode_varint(*v);
        }
        if builder.len() > builder.limit() {
            return Err(Error::InternalError(16));
        }
    };
    Ok(write)
}

#[derive(PartialEq, Debug, Clone)]
pub enum Frame<'a> {
    Padding,
    Ping,
    Ack {
        largest_acknowledged: u64,
        ack_delay: u64,
        first_ack_range: u64,
        ack_ranges: Vec<AckRange>,
    },
    ResetStream {
        stream_id: StreamId,
        application_error_code: AppError,
        final_size: u64,
    },
    StopSending {
        stream_id: StreamId,
        application_error_code: AppError,
    },
    Crypto {
        offset: u64,
        data: &'a [u8],
    },
    NewToken {
        token: &'a [u8],
    },
    Stream {
        stream_id: StreamId,
        offset: u64,
        data: &'a [u8],
        fin: bool,
        fill: bool,
    },
    MaxData {
        maximum_data: u64,
    },
    MaxStreamData {
        stream_id: StreamId,
        maximum_stream_data: u64,
    },
    MaxStreams {
        stream_type: StreamType,
        maximum_streams: u64,
    },
    DataBlocked {
        data_limit: u64,
    },
    StreamDataBlocked {
        stream_id: StreamId,
        stream_data_limit: u64,
    },
    StreamsBlocked {
        stream_type: StreamType,
        stream_limit: u64,
    },
    NewConnectionId {
        sequence_number: u64,
        retire_prior: u64,
        connection_id: &'a [u8],
        stateless_reset_token: &'a [u8; 16],
    },
    RetireConnectionId {
        sequence_number: u64,
    },
    PathChallenge {
        data: [u8; 8],
    },
    PathResponse {
        data: [u8; 8],
    },
    ConnectionClose {
        error_code: CloseError,
        frame_type: u64,
        // Not a reference as we use this to hold the value.
        // This is not used in optimized builds anyway.
        reason_phrase: Vec<u8>,
    },
    HandshakeDone,
}

impl<'a> Frame<'a> {
    fn get_stream_type_bit(stream_type: StreamType) -> u64 {
        match stream_type {
            StreamType::BiDi => 0,
            StreamType::UniDi => 1,
        }
    }

    fn stream_type_from_bit(bit: u64) -> StreamType {
        if (bit & 0x01) == 0 {
            StreamType::BiDi
        } else {
            StreamType::UniDi
        }
    }

    pub fn get_type(&self) -> FrameType {
        match self {
            Self::Padding => FRAME_TYPE_PADDING,
            Self::Ping => FRAME_TYPE_PING,
            Self::Ack { .. } => FRAME_TYPE_ACK, // We don't do ACK ECN.
            Self::ResetStream { .. } => FRAME_TYPE_RESET_STREAM,
            Self::StopSending { .. } => FRAME_TYPE_STOP_SENDING,
            Self::Crypto { .. } => FRAME_TYPE_CRYPTO,
            Self::NewToken { .. } => FRAME_TYPE_NEW_TOKEN,
            Self::Stream {
                fin, offset, fill, ..
            } => Self::stream_type(*fin, *offset > 0, *fill),
            Self::MaxData { .. } => FRAME_TYPE_MAX_DATA,
            Self::MaxStreamData { .. } => FRAME_TYPE_MAX_STREAM_DATA,
            Self::MaxStreams { stream_type, .. } => {
                FRAME_TYPE_MAX_STREAMS_BIDI + Self::get_stream_type_bit(*stream_type)
            }
            Self::DataBlocked { .. } => FRAME_TYPE_DATA_BLOCKED,
            Self::StreamDataBlocked { .. } => FRAME_TYPE_STREAM_DATA_BLOCKED,
            Self::StreamsBlocked { stream_type, .. } => {
                FRAME_TYPE_STREAMS_BLOCKED_BIDI + Self::get_stream_type_bit(*stream_type)
            }
            Self::NewConnectionId { .. } => FRAME_TYPE_NEW_CONNECTION_ID,
            Self::RetireConnectionId { .. } => FRAME_TYPE_RETIRE_CONNECTION_ID,
            Self::PathChallenge { .. } => FRAME_TYPE_PATH_CHALLENGE,
            Self::PathResponse { .. } => FRAME_TYPE_PATH_RESPONSE,
            Self::ConnectionClose { error_code, .. } => {
                FRAME_TYPE_CONNECTION_CLOSE_TRANSPORT + error_code.frame_type_bit()
            }
            Self::HandshakeDone => FRAME_TYPE_HANDSHAKE_DONE,
        }
    }

    pub fn is_stream(&self) -> bool {
        matches!(
            self,
            Self::ResetStream { .. }
                | Self::StopSending { .. }
                | Self::Stream { .. }
                | Self::MaxData { .. }
                | Self::MaxStreamData { .. }
                | Self::MaxStreams { .. }
                | Self::DataBlocked { .. }
                | Self::StreamDataBlocked { .. }
                | Self::StreamsBlocked { .. }
        )
    }

    pub fn stream_type(fin: bool, nonzero_offset: bool, fill: bool) -> u64 {
        let mut t = FRAME_TYPE_STREAM;
        if fin {
            t |= STREAM_FRAME_BIT_FIN;
        }
        if nonzero_offset {
            t |= STREAM_FRAME_BIT_OFF;
        }
        if !fill {
            t |= STREAM_FRAME_BIT_LEN;
        }
        t
    }

    /// If the frame causes a recipient to generate an ACK within its
    /// advertised maximum acknowledgement delay.
    pub fn ack_eliciting(&self) -> bool {
        !matches!(
            self,
            Self::Ack { .. } | Self::Padding | Self::ConnectionClose { .. }
        )
    }

    /// If the frame can be sent in a path probe
    /// without initiating migration to that path.
    pub fn path_probing(&self) -> bool {
        matches!(
            self,
            Self::Padding
                | Self::NewConnectionId { .. }
                | Self::PathChallenge { .. }
                | Self::PathResponse { .. }
        )
    }

    /// Converts AckRanges as encoded in a ACK frame (see -transport
    /// 19.3.1) into ranges of acked packets (end, start), inclusive of
    /// start and end values.
    pub fn decode_ack_frame(
        largest_acked: u64,
        first_ack_range: u64,
        ack_ranges: &[AckRange],
    ) -> Res<Vec<RangeInclusive<u64>>> {
        let mut acked_ranges = Vec::with_capacity(ack_ranges.len() + 1);

        if largest_acked < first_ack_range {
            return Err(Error::FrameEncodingError);
        }
        acked_ranges.push((largest_acked - first_ack_range)..=largest_acked);
        if !ack_ranges.is_empty() && largest_acked < first_ack_range + 1 {
            return Err(Error::FrameEncodingError);
        }
        let mut cur = if ack_ranges.is_empty() {
            0
        } else {
            largest_acked - first_ack_range - 1
        };
        for r in ack_ranges {
            if cur < r.gap + 1 {
                return Err(Error::FrameEncodingError);
            }
            cur = cur - r.gap - 1;

            if cur < r.range {
                return Err(Error::FrameEncodingError);
            }
            acked_ranges.push((cur - r.range)..=cur);

            if cur > r.range + 1 {
                cur -= r.range + 1;
            } else {
                cur -= r.range;
            }
        }

        Ok(acked_ranges)
    }

    pub fn dump(&self) -> Option<String> {
        match self {
            Self::Crypto { offset, data } => Some(format!(
                "Crypto {{ offset: {}, len: {} }}",
                offset,
                data.len()
            )),
            Self::Stream {
                stream_id,
                offset,
                fill,
                data,
                fin,
            } => Some(format!(
                "Stream {{ stream_id: {}, offset: {}, len: {}{}, fin: {} }}",
                stream_id.as_u64(),
                offset,
                if *fill { ">>" } else { "" },
                data.len(),
                fin,
            )),
            Self::Padding => None,
            _ => Some(format!("{:?}", self)),
        }
    }

    pub fn is_allowed(&self, pt: PacketType) -> bool {
        match self {
            Self::Padding | Self::Ping => true,
            Self::Crypto { .. }
            | Self::Ack { .. }
            | Self::ConnectionClose {
                error_code: CloseError::Transport(_),
                ..
            } => pt != PacketType::ZeroRtt,
            Self::NewToken { .. } | Self::ConnectionClose { .. } => pt == PacketType::Short,
            _ => pt == PacketType::ZeroRtt || pt == PacketType::Short,
        }
    }

    pub fn decode(dec: &mut Decoder<'a>) -> Res<Self> {
        fn d<T>(v: Option<T>) -> Res<T> {
            v.ok_or(Error::NoMoreData)
        }
        fn dv(dec: &mut Decoder) -> Res<u64> {
            d(dec.decode_varint())
        }

        // TODO(ekr@rtfm.com): check for minimal encoding
        let t = d(dec.decode_varint())?;
        match t {
            FRAME_TYPE_PADDING => Ok(Self::Padding),
            FRAME_TYPE_PING => Ok(Self::Ping),
            FRAME_TYPE_RESET_STREAM => Ok(Self::ResetStream {
                stream_id: StreamId::from(dv(dec)?),
                application_error_code: d(dec.decode_varint())?,
                final_size: match dec.decode_varint() {
                    Some(v) => v,
                    _ => return Err(Error::NoMoreData),
                },
            }),
            FRAME_TYPE_ACK | FRAME_TYPE_ACK_ECN => {
                let la = dv(dec)?;
                let ad = dv(dec)?;
                let nr = dv(dec)?;
                let fa = dv(dec)?;
                let mut arr: Vec<AckRange> = Vec::with_capacity(nr as usize);
                for _ in 0..nr {
                    let ar = AckRange {
                        gap: dv(dec)?,
                        range: dv(dec)?,
                    };
                    arr.push(ar);
                }

                // Now check for the values for ACK_ECN.
                if t == FRAME_TYPE_ACK_ECN {
                    dv(dec)?;
                    dv(dec)?;
                    dv(dec)?;
                }

                Ok(Self::Ack {
                    largest_acknowledged: la,
                    ack_delay: ad,
                    first_ack_range: fa,
                    ack_ranges: arr,
                })
            }
            FRAME_TYPE_STOP_SENDING => Ok(Self::StopSending {
                stream_id: StreamId::from(dv(dec)?),
                application_error_code: d(dec.decode_varint())?,
            }),
            FRAME_TYPE_CRYPTO => {
                let offset = dv(dec)?;
                let data = d(dec.decode_vvec())?;
                if offset + u64::try_from(data.len()).unwrap() > ((1 << 62) - 1) {
                    return Err(Error::FrameEncodingError);
                }
                Ok(Self::Crypto { offset, data })
            }
            FRAME_TYPE_NEW_TOKEN => {
                let token = d(dec.decode_vvec())?;
                if token.is_empty() {
                    return Err(Error::FrameEncodingError);
                }
                Ok(Self::NewToken { token })
            }
            FRAME_TYPE_STREAM..=FRAME_TYPE_STREAM_MAX => {
                let s = dv(dec)?;
                let o = if t & STREAM_FRAME_BIT_OFF == 0 {
                    0
                } else {
                    dv(dec)?
                };
                let fill = (t & STREAM_FRAME_BIT_LEN) == 0;
                let data = if fill {
                    qtrace!("STREAM frame, extends to the end of the packet");
                    dec.decode_remainder()
                } else {
                    qtrace!("STREAM frame, with length");
                    d(dec.decode_vvec())?
                };
                if o + u64::try_from(data.len()).unwrap() > ((1 << 62) - 1) {
                    return Err(Error::FrameEncodingError);
                }
                Ok(Self::Stream {
                    fin: (t & STREAM_FRAME_BIT_FIN) != 0,
                    stream_id: StreamId::from(s),
                    offset: o,
                    data,
                    fill,
                })
            }
            FRAME_TYPE_MAX_DATA => Ok(Self::MaxData {
                maximum_data: dv(dec)?,
            }),
            FRAME_TYPE_MAX_STREAM_DATA => Ok(Self::MaxStreamData {
                stream_id: StreamId::from(dv(dec)?),
                maximum_stream_data: dv(dec)?,
            }),
            FRAME_TYPE_MAX_STREAMS_BIDI | FRAME_TYPE_MAX_STREAMS_UNIDI => {
                let m = dv(dec)?;
                if m > (1 << 60) {
                    return Err(Error::StreamLimitError);
                }
                Ok(Self::MaxStreams {
                    stream_type: Self::stream_type_from_bit(t),
                    maximum_streams: m,
                })
            }
            FRAME_TYPE_DATA_BLOCKED => Ok(Self::DataBlocked {
                data_limit: dv(dec)?,
            }),
            FRAME_TYPE_STREAM_DATA_BLOCKED => Ok(Self::StreamDataBlocked {
                stream_id: dv(dec)?.into(),
                stream_data_limit: dv(dec)?,
            }),
            FRAME_TYPE_STREAMS_BLOCKED_BIDI | FRAME_TYPE_STREAMS_BLOCKED_UNIDI => {
                Ok(Self::StreamsBlocked {
                    stream_type: Self::stream_type_from_bit(t),
                    stream_limit: dv(dec)?,
                })
            }
            FRAME_TYPE_NEW_CONNECTION_ID => {
                let sequence_number = dv(dec)?;
                let retire_prior = dv(dec)?;
                let connection_id = d(dec.decode_vec(1))?;
                if connection_id.len() > MAX_CONNECTION_ID_LEN {
                    return Err(Error::DecodingFrame);
                }
                let srt = d(dec.decode(16))?;
                let stateless_reset_token = <&[_; 16]>::try_from(srt).unwrap();

                Ok(Self::NewConnectionId {
                    sequence_number,
                    retire_prior,
                    connection_id,
                    stateless_reset_token,
                })
            }
            FRAME_TYPE_RETIRE_CONNECTION_ID => Ok(Self::RetireConnectionId {
                sequence_number: dv(dec)?,
            }),
            FRAME_TYPE_PATH_CHALLENGE => {
                let data = d(dec.decode(8))?;
                let mut datav: [u8; 8] = [0; 8];
                datav.copy_from_slice(&data);
                Ok(Self::PathChallenge { data: datav })
            }
            FRAME_TYPE_PATH_RESPONSE => {
                let data = d(dec.decode(8))?;
                let mut datav: [u8; 8] = [0; 8];
                datav.copy_from_slice(&data);
                Ok(Self::PathResponse { data: datav })
            }
            FRAME_TYPE_CONNECTION_CLOSE_TRANSPORT | FRAME_TYPE_CONNECTION_CLOSE_APPLICATION => {
                let error_code = CloseError::from_type_bit(t, d(dec.decode_varint())?);
                let frame_type = if t == FRAME_TYPE_CONNECTION_CLOSE_TRANSPORT {
                    dv(dec)?
                } else {
                    0
                };
                // We can tolerate this copy for now.
                let reason_phrase = d(dec.decode_vvec())?.to_vec();
                Ok(Self::ConnectionClose {
                    error_code,
                    frame_type,
                    reason_phrase,
                })
            }
            FRAME_TYPE_HANDSHAKE_DONE => Ok(Self::HandshakeDone),
            _ => Err(Error::UnknownFrameType),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use neqo_common::{Decoder, Encoder};

    fn just_dec(f: &Frame, s: &str) {
        let encoded = Encoder::from_hex(s);
        let decoded = Frame::decode(&mut encoded.as_decoder()).unwrap();
        assert_eq!(*f, decoded);
    }

    #[test]
    fn padding() {
        let f = Frame::Padding;
        just_dec(&f, "00");
    }

    #[test]
    fn ping() {
        let f = Frame::Ping;
        just_dec(&f, "01");
    }

    #[test]
    fn ack() {
        let ar = vec![AckRange { gap: 1, range: 2 }, AckRange { gap: 3, range: 4 }];

        let f = Frame::Ack {
            largest_acknowledged: 0x1234,
            ack_delay: 0x1235,
            first_ack_range: 0x1236,
            ack_ranges: ar,
        };

        just_dec(&f, "025234523502523601020304");

        // Try to parse ACK_ECN without ECN values
        let enc = Encoder::from_hex("035234523502523601020304");
        let mut dec = enc.as_decoder();
        assert_eq!(Frame::decode(&mut dec).unwrap_err(), Error::NoMoreData);

        // Try to parse ACK_ECN without ECN values
        let enc = Encoder::from_hex("035234523502523601020304010203");
        let mut dec = enc.as_decoder();
        assert_eq!(Frame::decode(&mut dec).unwrap(), f);
    }

    #[test]
    fn reset_stream() {
        let f = Frame::ResetStream {
            stream_id: StreamId::from(0x1234),
            application_error_code: 0x77,
            final_size: 0x3456,
        };

        just_dec(&f, "04523440777456");
    }

    #[test]
    fn stop_sending() {
        let f = Frame::StopSending {
            stream_id: StreamId::from(63),
            application_error_code: 0x77,
        };

        just_dec(&f, "053F4077")
    }

    #[test]
    fn crypto() {
        let f = Frame::Crypto {
            offset: 1,
            data: &[1, 2, 3],
        };

        just_dec(&f, "060103010203");
    }

    #[test]
    fn new_token() {
        let f = Frame::NewToken {
            token: &[0x12, 0x34, 0x56],
        };

        just_dec(&f, "0703123456");
    }

    #[test]
    fn empty_new_token() {
        let mut dec = Decoder::from(&[0x07, 0x00][..]);
        assert_eq!(
            Frame::decode(&mut dec).unwrap_err(),
            Error::FrameEncodingError
        );
    }

    #[test]
    fn stream() {
        // First, just set the length bit.
        let f = Frame::Stream {
            fin: false,
            stream_id: StreamId::from(5),
            offset: 0,
            data: &[1, 2, 3],
            fill: false,
        };

        just_dec(&f, "0a0503010203");

        // Now with offset != 0 and FIN
        let f = Frame::Stream {
            fin: true,
            stream_id: StreamId::from(5),
            offset: 1,
            data: &[1, 2, 3],
            fill: false,
        };
        just_dec(&f, "0f050103010203");

        // Now to fill the packet.
        let f = Frame::Stream {
            fin: true,
            stream_id: StreamId::from(5),
            offset: 0,
            data: &[1, 2, 3],
            fill: true,
        };
        just_dec(&f, "0905010203");
    }

    #[test]
    fn max_data() {
        let f = Frame::MaxData {
            maximum_data: 0x1234,
        };

        just_dec(&f, "105234");
    }

    #[test]
    fn max_stream_data() {
        let f = Frame::MaxStreamData {
            stream_id: StreamId::from(5),
            maximum_stream_data: 0x1234,
        };

        just_dec(&f, "11055234");
    }

    #[test]
    fn max_streams() {
        let mut f = Frame::MaxStreams {
            stream_type: StreamType::BiDi,
            maximum_streams: 0x1234,
        };

        just_dec(&f, "125234");

        f = Frame::MaxStreams {
            stream_type: StreamType::UniDi,
            maximum_streams: 0x1234,
        };

        just_dec(&f, "135234");
    }

    #[test]
    fn data_blocked() {
        let f = Frame::DataBlocked { data_limit: 0x1234 };

        just_dec(&f, "145234");
    }

    #[test]
    fn stream_data_blocked() {
        let f = Frame::StreamDataBlocked {
            stream_id: StreamId::from(5),
            stream_data_limit: 0x1234,
        };

        just_dec(&f, "15055234");
    }

    #[test]
    fn streams_blocked() {
        let mut f = Frame::StreamsBlocked {
            stream_type: StreamType::BiDi,
            stream_limit: 0x1234,
        };

        just_dec(&f, "165234");

        f = Frame::StreamsBlocked {
            stream_type: StreamType::UniDi,
            stream_limit: 0x1234,
        };

        just_dec(&f, "175234");
    }

    #[test]
    fn new_connection_id() {
        let f = Frame::NewConnectionId {
            sequence_number: 0x1234,
            retire_prior: 0,
            connection_id: &[0x01, 0x02],
            stateless_reset_token: &[9; 16],
        };

        just_dec(&f, "1852340002010209090909090909090909090909090909");
    }

    #[test]
    fn too_large_new_connection_id() {
        let mut enc = Encoder::from_hex("18523400"); // up to the CID
        enc.encode_vvec(&[0x0c; MAX_CONNECTION_ID_LEN + 10]);
        enc.encode(&[0x11; 16][..]);
        assert_eq!(
            Frame::decode(&mut enc.as_decoder()).unwrap_err(),
            Error::DecodingFrame
        );
    }

    #[test]
    fn retire_connection_id() {
        let f = Frame::RetireConnectionId {
            sequence_number: 0x1234,
        };

        just_dec(&f, "195234");
    }

    #[test]
    fn path_challenge() {
        let f = Frame::PathChallenge { data: [9; 8] };

        just_dec(&f, "1a0909090909090909");
    }

    #[test]
    fn path_response() {
        let f = Frame::PathResponse { data: [9; 8] };

        just_dec(&f, "1b0909090909090909");
    }

    #[test]
    fn connection_close_transport() {
        let f = Frame::ConnectionClose {
            error_code: CloseError::Transport(0x5678),
            frame_type: 0x1234,
            reason_phrase: vec![0x01, 0x02, 0x03],
        };

        just_dec(&f, "1c80005678523403010203");
    }

    #[test]
    fn connection_close_application() {
        let f = Frame::ConnectionClose {
            error_code: CloseError::Application(0x5678),
            frame_type: 0,
            reason_phrase: vec![0x01, 0x02, 0x03],
        };

        just_dec(&f, "1d8000567803010203");
    }

    #[test]
    fn test_compare() {
        let f1 = Frame::Padding;
        let f2 = Frame::Padding;
        let f3 = Frame::Crypto {
            offset: 0,
            data: &[1, 2, 3],
        };
        let f4 = Frame::Crypto {
            offset: 0,
            data: &[1, 2, 3],
        };
        let f5 = Frame::Crypto {
            offset: 1,
            data: &[1, 2, 3],
        };
        let f6 = Frame::Crypto {
            offset: 0,
            data: &[1, 2, 4],
        };

        assert_eq!(f1, f2);
        assert_ne!(f1, f3);
        assert_eq!(f3, f4);
        assert_ne!(f3, f5);
        assert_ne!(f3, f6);
    }

    #[test]
    fn decode_ack_frame() {
        let res = Frame::decode_ack_frame(7, 2, &[AckRange { gap: 0, range: 3 }]);
        assert!(res.is_ok());
        assert_eq!(res.unwrap(), vec![5..=7, 0..=3]);
    }
}
