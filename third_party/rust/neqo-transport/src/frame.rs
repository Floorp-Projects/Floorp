// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Directly relating to QUIC frames.

use neqo_common::{qdebug, qtrace, Decoder, Encoder};

use crate::cid::MAX_CONNECTION_ID_LEN;
use crate::packet::PacketType;
use crate::stream_id::{StreamId, StreamIndex};
use crate::{AppError, ConnectionError, Error, Res, TransportError, ERROR_APPLICATION_CLOSE};

use std::cmp::{min, Ordering};
use std::convert::TryFrom;

#[allow(clippy::module_name_repetitions)]
pub type FrameType = u64;

const FRAME_TYPE_PADDING: FrameType = 0x0;
const FRAME_TYPE_PING: FrameType = 0x1;
const FRAME_TYPE_ACK: FrameType = 0x2;
const FRAME_TYPE_ACK_ECN: FrameType = 0x3;
const FRAME_TYPE_RST_STREAM: FrameType = 0x4;
const FRAME_TYPE_STOP_SENDING: FrameType = 0x5;
const FRAME_TYPE_CRYPTO: FrameType = 0x6;
const FRAME_TYPE_NEW_TOKEN: FrameType = 0x7;
const FRAME_TYPE_STREAM: FrameType = 0x8;
const FRAME_TYPE_STREAM_MAX: FrameType = 0xf;
const FRAME_TYPE_MAX_DATA: FrameType = 0x10;
const FRAME_TYPE_MAX_STREAM_DATA: FrameType = 0x11;
const FRAME_TYPE_MAX_STREAMS_BIDI: FrameType = 0x12;
const FRAME_TYPE_MAX_STREAMS_UNIDI: FrameType = 0x13;
const FRAME_TYPE_DATA_BLOCKED: FrameType = 0x14;
const FRAME_TYPE_STREAM_DATA_BLOCKED: FrameType = 0x15;
const FRAME_TYPE_STREAMS_BLOCKED_BIDI: FrameType = 0x16;
const FRAME_TYPE_STREAMS_BLOCKED_UNIDI: FrameType = 0x17;
const FRAME_TYPE_NEW_CONNECTION_ID: FrameType = 0x18;
const FRAME_TYPE_RETIRE_CONNECTION_ID: FrameType = 0x19;
const FRAME_TYPE_PATH_CHALLENGE: FrameType = 0x1a;
const FRAME_TYPE_PATH_RESPONSE: FrameType = 0x1b;
pub const FRAME_TYPE_CONNECTION_CLOSE_TRANSPORT: FrameType = 0x1c;
pub const FRAME_TYPE_CONNECTION_CLOSE_APPLICATION: FrameType = 0x1d;
const FRAME_TYPE_HANDSHAKE_DONE: FrameType = 0x1e;

const STREAM_FRAME_BIT_FIN: u64 = 0x01;
const STREAM_FRAME_BIT_LEN: u64 = 0x02;
const STREAM_FRAME_BIT_OFF: u64 = 0x04;

/// `FRAME_APPLICATION_CLOSE` is the default CONNECTION_CLOSE frame that
/// is sent when an application error code needs to be sent in an
/// Initial or Handshake packet.
const FRAME_APPLICATION_CLOSE: &Frame = &Frame::ConnectionClose {
    error_code: CloseError::Transport(ERROR_APPLICATION_CLOSE),
    frame_type: 0,
    reason_phrase: Vec::new(),
};

#[derive(PartialEq, Debug, Copy, Clone, PartialOrd, Eq, Ord, Hash)]
/// Bi-Directional or Uni-Directional.
pub enum StreamType {
    BiDi,
    UniDi,
}

impl StreamType {
    fn frame_type_bit(self) -> u64 {
        match self {
            Self::BiDi => 0,
            Self::UniDi => 1,
        }
    }
    fn from_type_bit(bit: u64) -> Self {
        if (bit & 0x01) == 0 {
            Self::BiDi
        } else {
            Self::UniDi
        }
    }
}

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

#[derive(PartialEq, Debug, Clone)]
pub enum Frame {
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
        data: Vec<u8>,
    },
    NewToken {
        token: Vec<u8>,
    },
    Stream {
        fin: bool,
        stream_id: StreamId,
        offset: u64,
        data: Vec<u8>,
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
        maximum_streams: StreamIndex,
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
        stream_limit: StreamIndex,
    },
    NewConnectionId {
        sequence_number: u64,
        retire_prior: u64,
        connection_id: Vec<u8>,
        stateless_reset_token: [u8; 16],
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
        reason_phrase: Vec<u8>,
    },
    HandshakeDone,
}

impl Frame {
    pub fn get_type(&self) -> FrameType {
        match self {
            Self::Padding => FRAME_TYPE_PADDING,
            Self::Ping => FRAME_TYPE_PING,
            Self::Ack { .. } => FRAME_TYPE_ACK, // We don't do ACK ECN.
            Self::ResetStream { .. } => FRAME_TYPE_RST_STREAM,
            Self::StopSending { .. } => FRAME_TYPE_STOP_SENDING,
            Self::Crypto { .. } => FRAME_TYPE_CRYPTO,
            Self::NewToken { .. } => FRAME_TYPE_NEW_TOKEN,
            Self::Stream {
                fin, offset, fill, ..
            } => {
                let mut t = FRAME_TYPE_STREAM;
                if *fin {
                    t |= STREAM_FRAME_BIT_FIN;
                }
                if *offset > 0 {
                    t |= STREAM_FRAME_BIT_OFF;
                }
                if !*fill {
                    t |= STREAM_FRAME_BIT_LEN;
                }
                t
            }
            Self::MaxData { .. } => FRAME_TYPE_MAX_DATA,
            Self::MaxStreamData { .. } => FRAME_TYPE_MAX_STREAM_DATA,
            Self::MaxStreams { stream_type, .. } => {
                FRAME_TYPE_MAX_STREAMS_BIDI + stream_type.frame_type_bit()
            }
            Self::DataBlocked { .. } => FRAME_TYPE_DATA_BLOCKED,
            Self::StreamDataBlocked { .. } => FRAME_TYPE_STREAM_DATA_BLOCKED,
            Self::StreamsBlocked { stream_type, .. } => {
                FRAME_TYPE_STREAMS_BLOCKED_BIDI + stream_type.frame_type_bit()
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

    /// Create a CRYPTO frame that fits the available space and its length.
    pub fn new_crypto(offset: u64, data: &[u8], space: usize) -> (Self, usize) {
        // Subtract the frame type and offset from available space.
        let mut remaining = space - 1 - Encoder::varint_len(offset);
        // Then subtract space for the length field.
        let data_len = min(remaining - 1, data.len());
        remaining -= Encoder::varint_len(u64::try_from(data_len).unwrap());
        remaining = min(data.len(), remaining);
        (
            Self::Crypto {
                offset,
                data: data[..remaining].to_vec(),
            },
            remaining,
        )
    }

    /// Create a STREAM frame that fits the available space.
    /// Return a tuple of a frame and the amount of data it carries.
    pub fn new_stream(
        stream_id: u64,
        offset: u64,
        data: &[u8],
        fin: bool,
        space: usize,
    ) -> Option<(Self, usize)> {
        let mut overhead = 1 + Encoder::varint_len(stream_id);
        if offset > 0 {
            overhead += Encoder::varint_len(offset);
        }

        let (fin, fill) = match (data.len() + overhead).cmp(&space) {
            // More data than fits, fill the packet and negate |fin|.
            Ordering::Greater => (false, true),
            // Exact fit, fill the packet, keep |fin|.
            Ordering::Equal => (fin, true),
            // Too small, so include a length.
            Ordering::Less => {
                let data_len = min(space.saturating_sub(overhead + 1), data.len());
                overhead += Encoder::varint_len(u64::try_from(data_len).unwrap());

                // If all data isn't going to make it in the frame, don't keep fin.
                let keep_fin = data.len() + overhead <= space;
                (fin && keep_fin, false)
            }
        };

        if overhead > space {
            qdebug!(
                "Frame::new_stream -> None; ovr {} > space {}",
                overhead,
                space
            );
            return None;
        }

        let data_len = min(data.len(), space - overhead);
        if data_len == 0 && !fin {
            qdebug!("Frame::new_stream -> None; no data, no fin");
            return None;
        }

        qdebug!(
            "Frame::new_stream fill {} fin {} data {} space {} ovr {}",
            fill,
            fin,
            data_len,
            space,
            overhead
        );

        Some((
            Self::Stream {
                stream_id: stream_id.into(),
                offset,
                data: data[..data_len].to_vec(),
                fin,
                fill,
            },
            data_len,
        ))
    }

    pub fn marshal(&self, enc: &mut Encoder) {
        enc.encode_varint(self.get_type());

        match self {
            Self::Padding | Self::Ping => (),
            Self::Ack {
                largest_acknowledged,
                ack_delay,
                first_ack_range,
                ack_ranges,
            } => {
                enc.encode_varint(*largest_acknowledged);
                enc.encode_varint(*ack_delay);
                enc.encode_varint(ack_ranges.len() as u64);
                enc.encode_varint(*first_ack_range);
                for r in ack_ranges {
                    enc.encode_varint(r.gap);
                    enc.encode_varint(r.range);
                }
            }
            Self::ResetStream {
                stream_id,
                application_error_code,
                final_size,
            } => {
                enc.encode_varint(stream_id.as_u64());
                enc.encode_varint(*application_error_code);
                enc.encode_varint(*final_size);
            }
            Self::StopSending {
                stream_id,
                application_error_code,
            } => {
                enc.encode_varint(stream_id.as_u64());
                enc.encode_varint(*application_error_code);
            }
            Self::Crypto { offset, data } => {
                enc.encode_varint(*offset);
                enc.encode_vvec(&data);
            }
            Self::NewToken { token } => {
                enc.encode_vvec(token);
            }
            Self::Stream {
                stream_id,
                offset,
                data,
                fill,
                ..
            } => {
                enc.encode_varint(stream_id.as_u64());
                if *offset > 0 {
                    enc.encode_varint(*offset);
                }
                if *fill {
                    enc.encode(&data);
                } else {
                    enc.encode_vvec(&data);
                }
            }
            Self::MaxData { maximum_data } => {
                enc.encode_varint(*maximum_data);
            }
            Self::MaxStreamData {
                stream_id,
                maximum_stream_data,
            } => {
                enc.encode_varint(stream_id.as_u64());
                enc.encode_varint(*maximum_stream_data);
            }
            Self::MaxStreams {
                maximum_streams, ..
            } => {
                enc.encode_varint(maximum_streams.as_u64());
            }
            Self::DataBlocked { data_limit } => {
                enc.encode_varint(*data_limit);
            }
            Self::StreamDataBlocked {
                stream_id,
                stream_data_limit,
            } => {
                enc.encode_varint(stream_id.as_u64());
                enc.encode_varint(*stream_data_limit);
            }
            Self::StreamsBlocked { stream_limit, .. } => {
                enc.encode_varint(stream_limit.as_u64());
            }
            Self::NewConnectionId {
                sequence_number,
                retire_prior,
                connection_id,
                stateless_reset_token,
            } => {
                enc.encode_varint(*sequence_number);
                enc.encode_varint(*retire_prior);
                enc.encode_uint(1, connection_id.len() as u64);
                enc.encode(connection_id);
                enc.encode(stateless_reset_token);
            }
            Self::RetireConnectionId { sequence_number } => {
                enc.encode_varint(*sequence_number);
            }
            Self::PathChallenge { data } => {
                enc.encode(data);
            }
            Self::PathResponse { data } => {
                enc.encode(data);
            }
            Self::ConnectionClose {
                error_code,
                frame_type,
                reason_phrase,
            } => {
                enc.encode_varint(error_code.code());
                enc.encode_varint(*frame_type);
                enc.encode_vvec(reason_phrase);
            }
            Self::HandshakeDone => (),
        }
    }

    /// Convert a CONNECTION_CLOSE into a nicer CONNECTION_CLOSE.
    pub fn sanitize_close(&self) -> &Self {
        if let Self::ConnectionClose { error_code, .. } = &self {
            if let CloseError::Application(_) = error_code {
                FRAME_APPLICATION_CLOSE
            } else {
                self
            }
        } else {
            panic!("Attempted to sanitize a non-close frame");
        }
    }

    pub fn ack_eliciting(&self) -> bool {
        !matches!(self, Self::Ack { .. } | Self::Padding | Self::ConnectionClose { .. })
    }

    /// Converts AckRanges as encoded in a ACK frame (see -transport
    /// 19.3.1) into ranges of acked packets (end, start), inclusive of
    /// start and end values.
    pub fn decode_ack_frame(
        largest_acked: u64,
        first_ack_range: u64,
        ack_ranges: &[AckRange],
    ) -> Res<Vec<(u64, u64)>> {
        let mut acked_ranges = Vec::new();

        if largest_acked < first_ack_range {
            return Err(Error::FrameEncodingError);
        }
        acked_ranges.push((largest_acked, largest_acked - first_ack_range));
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
            acked_ranges.push((cur, cur - r.range));

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
                "Stream {{ stream_id: {}, offset: {}, len: {}{} fin: {} }}",
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
        if matches!(self, Self::Padding | Self::Ping) {
            true
        } else if matches!(self, Self::Crypto {..} | Self::Ack {..} | Self::ConnectionClose { error_code: CloseError::Transport(_), .. })
        {
            pt != PacketType::ZeroRtt
        } else if matches!(self, Self::NewToken {..} | Self::ConnectionClose {..}) {
            pt == PacketType::Short
        } else {
            pt == PacketType::ZeroRtt || pt == PacketType::Short
        }
    }

    pub fn decode(dec: &mut Decoder) -> Res<Self> {
        macro_rules! d {
            ($d:expr) => {
                match $d {
                    Some(v) => v,
                    _ => return Err(Error::NoMoreData),
                }
            };
        }
        macro_rules! dv {
            ($d:expr) => {
                d!($d.decode_varint())
            };
        }

        // TODO(ekr@rtfm.com): check for minimal encoding
        let t = d!(dec.decode_varint());
        match t {
            FRAME_TYPE_PADDING => Ok(Self::Padding),
            FRAME_TYPE_PING => Ok(Self::Ping),
            FRAME_TYPE_RST_STREAM => Ok(Self::ResetStream {
                stream_id: dv!(dec).into(),
                application_error_code: d!(dec.decode_varint()),
                final_size: match dec.decode_varint() {
                    Some(v) => v,
                    _ => return Err(Error::NoMoreData),
                },
            }),
            FRAME_TYPE_ACK | FRAME_TYPE_ACK_ECN => {
                let la = dv!(dec);
                let ad = dv!(dec);
                let nr = dv!(dec);
                let fa = dv!(dec);
                let mut arr: Vec<AckRange> = Vec::with_capacity(nr as usize);
                for _ in 0..nr {
                    let ar = AckRange {
                        gap: dv!(dec),
                        range: dv!(dec),
                    };
                    arr.push(ar);
                }

                // Now check for the values for ACK_ECN.
                if t == FRAME_TYPE_ACK_ECN {
                    dv!(dec);
                    dv!(dec);
                    dv!(dec);
                }

                Ok(Self::Ack {
                    largest_acknowledged: la,
                    ack_delay: ad,
                    first_ack_range: fa,
                    ack_ranges: arr,
                })
            }
            FRAME_TYPE_STOP_SENDING => Ok(Self::StopSending {
                stream_id: dv!(dec).into(),
                application_error_code: d!(dec.decode_varint()),
            }),
            FRAME_TYPE_CRYPTO => {
                let o = dv!(dec);
                let data = d!(dec.decode_vvec());
                if o + u64::try_from(data.len()).unwrap() > ((1 << 62) - 1) {
                    return Err(Error::FrameEncodingError);
                }
                Ok(Self::Crypto {
                    offset: o,
                    data: data.to_vec(), // TODO(mt) unnecessary copy
                })
            }
            FRAME_TYPE_NEW_TOKEN => {
                Ok(Self::NewToken {
                    token: d!(dec.decode_vvec()).to_vec(), // TODO(mt) unnecessary copy
                })
            }
            FRAME_TYPE_STREAM..=FRAME_TYPE_STREAM_MAX => {
                let s = dv!(dec);
                let o = if t & STREAM_FRAME_BIT_OFF == 0 {
                    0
                } else {
                    dv!(dec)
                };
                let fill = (t & STREAM_FRAME_BIT_LEN) == 0;
                let data = if fill {
                    qtrace!("STREAM frame, extends to the end of the packet");
                    dec.decode_remainder()
                } else {
                    qtrace!("STREAM frame, with length");
                    d!(dec.decode_vvec())
                };
                if o + u64::try_from(data.len()).unwrap() > ((1 << 62) - 1) {
                    return Err(Error::FrameEncodingError);
                }
                Ok(Self::Stream {
                    fin: (t & STREAM_FRAME_BIT_FIN) != 0,
                    stream_id: s.into(),
                    offset: o,
                    data: data.to_vec(), // TODO(mt) unnecessary copy.
                    fill,
                })
            }
            FRAME_TYPE_MAX_DATA => Ok(Self::MaxData {
                maximum_data: dv!(dec),
            }),
            FRAME_TYPE_MAX_STREAM_DATA => Ok(Self::MaxStreamData {
                stream_id: dv!(dec).into(),
                maximum_stream_data: dv!(dec),
            }),
            FRAME_TYPE_MAX_STREAMS_BIDI | FRAME_TYPE_MAX_STREAMS_UNIDI => {
                let m = dv!(dec);
                if m > (1 << 60) {
                    return Err(Error::StreamLimitError);
                }
                Ok(Self::MaxStreams {
                    stream_type: StreamType::from_type_bit(t),
                    maximum_streams: StreamIndex::new(m),
                })
            }
            FRAME_TYPE_DATA_BLOCKED => Ok(Self::DataBlocked {
                data_limit: dv!(dec),
            }),
            FRAME_TYPE_STREAM_DATA_BLOCKED => Ok(Self::StreamDataBlocked {
                stream_id: dv!(dec).into(),
                stream_data_limit: dv!(dec),
            }),
            FRAME_TYPE_STREAMS_BLOCKED_BIDI | FRAME_TYPE_STREAMS_BLOCKED_UNIDI => {
                Ok(Self::StreamsBlocked {
                    stream_type: StreamType::from_type_bit(t),
                    stream_limit: StreamIndex::new(dv!(dec)),
                })
            }
            FRAME_TYPE_NEW_CONNECTION_ID => {
                let s = dv!(dec);
                let retire_prior = dv!(dec);
                let cid = d!(dec.decode_vec(1)).to_vec(); // TODO(mt) unnecessary copy
                if cid.len() > MAX_CONNECTION_ID_LEN {
                    return Err(Error::DecodingFrame);
                }
                let srt = d!(dec.decode(16));
                let mut srtv: [u8; 16] = [0; 16];
                srtv.copy_from_slice(&srt);

                Ok(Self::NewConnectionId {
                    sequence_number: s,
                    retire_prior,
                    connection_id: cid,
                    stateless_reset_token: srtv,
                })
            }
            FRAME_TYPE_RETIRE_CONNECTION_ID => Ok(Self::RetireConnectionId {
                sequence_number: dv!(dec),
            }),
            FRAME_TYPE_PATH_CHALLENGE => {
                let data = d!(dec.decode(8));
                let mut datav: [u8; 8] = [0; 8];
                datav.copy_from_slice(&data);
                Ok(Self::PathChallenge { data: datav })
            }
            FRAME_TYPE_PATH_RESPONSE => {
                let data = d!(dec.decode(8));
                let mut datav: [u8; 8] = [0; 8];
                datav.copy_from_slice(&data);
                Ok(Self::PathResponse { data: datav })
            }
            FRAME_TYPE_CONNECTION_CLOSE_TRANSPORT | FRAME_TYPE_CONNECTION_CLOSE_APPLICATION => {
                Ok(Self::ConnectionClose {
                    error_code: CloseError::from_type_bit(t, d!(dec.decode_varint())),
                    frame_type: dv!(dec),
                    reason_phrase: d!(dec.decode_vvec()).to_vec(), // TODO(mt) unnecessary copy
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
    use neqo_common::hex;

    fn enc_dec(f: &Frame, s: &str) {
        let mut d = Encoder::default();

        f.marshal(&mut d);
        assert_eq!(d, Encoder::from_hex(s));

        let f2 = Frame::decode(&mut d.as_decoder()).unwrap();
        assert_eq!(*f, f2);
    }

    #[test]
    fn test_padding_frame() {
        let f = Frame::Padding;
        enc_dec(&f, "00");
    }

    #[test]
    fn test_ping_frame() {
        let f = Frame::Ping;
        enc_dec(&f, "01");
    }

    #[test]
    fn test_ack() {
        let ar = vec![AckRange { gap: 1, range: 2 }, AckRange { gap: 3, range: 4 }];

        let f = Frame::Ack {
            largest_acknowledged: 0x1234,
            ack_delay: 0x1235,
            first_ack_range: 0x1236,
            ack_ranges: ar,
        };

        enc_dec(&f, "025234523502523601020304");

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
    fn test_reset_stream() {
        let f = Frame::ResetStream {
            stream_id: 0x1234.into(),
            application_error_code: 0x77,
            final_size: 0x3456,
        };

        enc_dec(&f, "04523440777456");
    }

    #[test]
    fn test_stop_sending() {
        let f = Frame::StopSending {
            stream_id: 63.into(),
            application_error_code: 0x77,
        };

        enc_dec(&f, "053F4077")
    }

    #[test]
    fn test_crypto() {
        let f = Frame::Crypto {
            offset: 1,
            data: vec![1, 2, 3],
        };

        enc_dec(&f, "060103010203");
    }

    #[test]
    fn test_new_token() {
        let f = Frame::NewToken {
            token: vec![0x12, 0x34, 0x56],
        };

        enc_dec(&f, "0703123456");
    }

    #[test]
    fn test_stream() {
        // First, just set the length bit.
        let f = Frame::Stream {
            fin: false,
            stream_id: 5.into(),
            offset: 0,
            data: vec![1, 2, 3],
            fill: false,
        };

        enc_dec(&f, "0a0503010203");

        // Now with offset != 0 and FIN
        let f = Frame::Stream {
            fin: true,
            stream_id: 5.into(),
            offset: 1,
            data: vec![1, 2, 3],
            fill: false,
        };
        enc_dec(&f, "0f050103010203");

        // Now to fill the packet.
        let f = Frame::Stream {
            fin: true,
            stream_id: 5.into(),
            offset: 0,
            data: vec![1, 2, 3],
            fill: true,
        };
        enc_dec(&f, "0905010203");
    }

    #[test]
    fn test_max_data() {
        let f = Frame::MaxData {
            maximum_data: 0x1234,
        };

        enc_dec(&f, "105234");
    }

    #[test]
    fn test_max_stream_data() {
        let f = Frame::MaxStreamData {
            stream_id: 5.into(),
            maximum_stream_data: 0x1234,
        };

        enc_dec(&f, "11055234");
    }

    #[test]
    fn test_max_streams() {
        let mut f = Frame::MaxStreams {
            stream_type: StreamType::BiDi,
            maximum_streams: StreamIndex::new(0x1234),
        };

        enc_dec(&f, "125234");

        f = Frame::MaxStreams {
            stream_type: StreamType::UniDi,
            maximum_streams: StreamIndex::new(0x1234),
        };

        enc_dec(&f, "135234");
    }

    #[test]
    fn test_data_blocked() {
        let f = Frame::DataBlocked { data_limit: 0x1234 };

        enc_dec(&f, "145234");
    }

    #[test]
    fn test_stream_data_blocked() {
        let f = Frame::StreamDataBlocked {
            stream_id: 5.into(),
            stream_data_limit: 0x1234,
        };

        enc_dec(&f, "15055234");
    }

    #[test]
    fn test_streams_blocked() {
        let mut f = Frame::StreamsBlocked {
            stream_type: StreamType::BiDi,
            stream_limit: StreamIndex::new(0x1234),
        };

        enc_dec(&f, "165234");

        f = Frame::StreamsBlocked {
            stream_type: StreamType::UniDi,
            stream_limit: StreamIndex::new(0x1234),
        };

        enc_dec(&f, "175234");
    }

    #[test]
    fn test_new_connection_id() {
        let f = Frame::NewConnectionId {
            sequence_number: 0x1234,
            retire_prior: 0,
            connection_id: vec![0x01, 0x02],
            stateless_reset_token: [9; 16],
        };

        enc_dec(&f, "1852340002010209090909090909090909090909090909");
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
    fn test_retire_connection_id() {
        let f = Frame::RetireConnectionId {
            sequence_number: 0x1234,
        };

        enc_dec(&f, "195234");
    }

    #[test]
    fn test_path_challenge() {
        let f = Frame::PathChallenge { data: [9; 8] };

        enc_dec(&f, "1a0909090909090909");
    }

    #[test]
    fn test_path_response() {
        let f = Frame::PathResponse { data: [9; 8] };

        enc_dec(&f, "1b0909090909090909");
    }

    #[test]
    fn test_connection_close() {
        let mut f = Frame::ConnectionClose {
            error_code: CloseError::Transport(0x5678),
            frame_type: 0x1234,
            reason_phrase: vec![0x01, 0x02, 0x03],
        };

        enc_dec(&f, "1c80005678523403010203");

        f = Frame::ConnectionClose {
            error_code: CloseError::Application(0x5678),
            frame_type: 0x1234,
            reason_phrase: vec![0x01, 0x02, 0x03],
        };

        enc_dec(&f, "1d80005678523403010203");
    }

    #[test]
    fn test_compare() {
        let f1 = Frame::Padding;
        let f2 = Frame::Padding;
        let f3 = Frame::Crypto {
            offset: 0,
            data: vec![1, 2, 3],
        };
        let f4 = Frame::Crypto {
            offset: 0,
            data: vec![1, 2, 3],
        };
        let f5 = Frame::Crypto {
            offset: 1,
            data: vec![1, 2, 3],
        };
        let f6 = Frame::Crypto {
            offset: 0,
            data: vec![1, 2, 4],
        };

        assert_eq!(f1, f2);
        assert_ne!(f1, f3);
        assert_eq!(f3, f4);
        assert_ne!(f3, f5);
        assert_ne!(f3, f6);
    }

    #[test]
    fn encode_ack_frame() {
        let ack_frame = Frame::Ack {
            largest_acknowledged: 7,
            ack_delay: 12_000,
            first_ack_range: 2, // [7], 6, 5
            ack_ranges: vec![AckRange {
                gap: 0,   // 4
                range: 1, // 3, 2
            }],
        };
        let mut enc = Encoder::default();
        ack_frame.marshal(&mut enc);
        println!("Encoded ACK={}", hex(&enc[..]));

        let f = Frame::decode(&mut enc.as_decoder()).unwrap();
        if let Frame::Ack {
            largest_acknowledged,
            ack_delay,
            first_ack_range,
            ack_ranges,
        } = f
        {
            assert_eq!(largest_acknowledged, 7);
            assert_eq!(ack_delay, 12_000);
            assert_eq!(first_ack_range, 2);
            assert_eq!(ack_ranges.len(), 1);
            assert_eq!(ack_ranges[0].gap, 0);
            assert_eq!(ack_ranges[0].range, 1);
        }
    }

    #[test]
    fn test_decode_ack_frame() {
        let res = Frame::decode_ack_frame(7, 2, &[AckRange { gap: 0, range: 3 }]);
        assert!(res.is_ok());
        assert_eq!(res.unwrap(), vec![(7, 5), (3, 0)]);
    }

    #[test]
    fn new_stream_empty() {
        // Stream frames with empty data and no fin never work.
        assert!(Frame::new_stream(0, 10, &[], false, 2).is_none());
        assert!(Frame::new_stream(0, 10, &[], false, 3).is_none());
        assert!(Frame::new_stream(0, 10, &[], false, 4).is_none());
        assert!(Frame::new_stream(0, 10, &[], false, 5).is_none());
        assert!(Frame::new_stream(0, 10, &[], false, 100).is_none());

        // Empty data with fin is only a problem if there is no space.
        assert!(Frame::new_stream(0, 0, &[], true, 1).is_none());
        assert!(Frame::new_stream(0, 0, &[], true, 2).is_some());
        assert!(Frame::new_stream(0, 10, &[], true, 2).is_none());
        assert!(Frame::new_stream(0, 10, &[], true, 3).is_some());
        assert!(Frame::new_stream(0, 10, &[], true, 4).is_some());
        assert!(Frame::new_stream(0, 10, &[], true, 5).is_some());
        assert!(Frame::new_stream(0, 10, &[], true, 100).is_some());
    }

    #[test]
    fn new_stream_minimum() {
        // Add minimum data
        assert!(Frame::new_stream(0, 10, &[0x42; 1], false, 3).is_none());
        assert!(Frame::new_stream(0, 10, &[0x42; 1], true, 3).is_none());
        assert!(Frame::new_stream(0, 10, &[0x42; 1], false, 4).is_some());
        assert!(Frame::new_stream(0, 10, &[0x42; 1], true, 4).is_some());
        assert!(Frame::new_stream(0, 10, &[0x42; 1], false, 5).is_some());
        assert!(Frame::new_stream(0, 10, &[0x42; 1], true, 5).is_some());
        assert!(Frame::new_stream(0, 10, &[0x42; 1], false, 100).is_some());
        assert!(Frame::new_stream(0, 10, &[0x42; 1], true, 100).is_some());
    }

    #[test]
    fn new_stream_more() {
        // Try more data
        assert!(Frame::new_stream(0, 10, &[0x42; 100], false, 3).is_none());
        assert!(Frame::new_stream(0, 10, &[0x42; 100], true, 3).is_none());
        assert!(Frame::new_stream(0, 10, &[0x42; 100], false, 4).is_some());
        assert!(Frame::new_stream(0, 10, &[0x42; 100], true, 4).is_some());
        assert!(Frame::new_stream(0, 10, &[0x42; 100], false, 5).is_some());
        assert!(Frame::new_stream(0, 10, &[0x42; 100], true, 5).is_some());
        assert!(Frame::new_stream(0, 10, &[0x42; 100], false, 100).is_some());
        assert!(Frame::new_stream(0, 10, &[0x42; 100], true, 100).is_some());

        assert!(Frame::new_stream(0, 10, &[0x42; 100], false, 1000).is_some());
        assert!(Frame::new_stream(0, 10, &[0x42; 100], true, 1000).is_some());
    }

    #[test]
    fn new_stream_big_id() {
        // A value that encodes to the largest varint.
        const BIG: u64 = 1 << 30;

        assert!(Frame::new_stream(BIG, BIG, &[], false, 16).is_none());
        assert!(Frame::new_stream(BIG, BIG, &[], true, 16).is_none());
        assert!(Frame::new_stream(BIG, BIG, &[], false, 17).is_none());
        assert!(Frame::new_stream(BIG, BIG, &[], true, 17).is_some());
        assert!(Frame::new_stream(BIG, BIG, &[], false, 18).is_none());
        assert!(Frame::new_stream(BIG, BIG, &[], true, 18).is_some());

        assert!(Frame::new_stream(BIG, BIG, &[0x42; 1], false, 17).is_none());
        assert!(Frame::new_stream(BIG, BIG, &[0x42; 1], true, 17).is_none());
        assert!(Frame::new_stream(BIG, BIG, &[0x42; 1], false, 18).is_some());
        assert!(Frame::new_stream(BIG, BIG, &[0x42; 1], true, 18).is_some());
        assert!(Frame::new_stream(BIG, BIG, &[0x42; 1], false, 19).is_some());
        assert!(Frame::new_stream(BIG, BIG, &[0x42; 1], true, 19).is_some());
        assert!(Frame::new_stream(BIG, BIG, &[0x42; 1], false, 100).is_some());
        assert!(Frame::new_stream(BIG, BIG, &[0x42; 1], true, 100).is_some());
    }

    #[test]
    fn new_stream_16384() {
        // 16383/16384 is an odd boundary in STREAM frame construction.
        // That is the boundary where a length goes from 2 bytes to 4 bytes.
        // If the data fits in the available space, then it is simple:
        let r = Frame::new_stream(0, 0, &[0x43; 16384], true, 16386);
        let (f, used) = r.expect("Fit frame");
        assert_eq!(used, 16384);
        if let Frame::Stream {
            fin, fill, data, ..
        } = f
        {
            assert_eq!(data.len(), 16384);
            assert!(fin);
            assert!(fill);
        } else {
            panic!("Wrong frame type");
        }

        // However, if there is one extra byte of space, we will try to add a length.
        // That length will then make the frame to be too large and the data will be
        // truncated.  The frame could carry one more byte of data, but it's a corner
        // case we don't want to address as it should be rare (if not impossible).
        let r = Frame::new_stream(0, 0, &[0x43; 16384], true, 16387);
        let (f, used) = r.expect("a frame");
        assert_eq!(used, 16381);
        if let Frame::Stream {
            fin, fill, data, ..
        } = f
        {
            assert_eq!(data.len(), 16381);
            assert!(!fin);
            assert!(!fill);
        } else {
            panic!("Wrong frame type");
        }
    }

    #[test]
    fn new_stream_64() {
        // Unlike 16383/16384, the boundary at 63/64 is easy because the difference
        // is just one byte.  We lose just the last byte when there is more space.
        let r = Frame::new_stream(0, 0, &[0x43; 64], true, 66);
        let (f, used) = r.expect("Fit frame");
        assert_eq!(used, 64);
        if let Frame::Stream {
            fin, fill, data, ..
        } = f
        {
            assert_eq!(data.len(), 64);
            assert!(fin);
            assert!(fill);
        } else {
            panic!("Wrong frame type");
        }

        let r = Frame::new_stream(0, 0, &[0x43; 64], true, 67);
        let (f, used) = r.expect("a frame");
        assert_eq!(used, 63);
        if let Frame::Stream {
            fin, fill, data, ..
        } = f
        {
            assert_eq!(data.len(), 63);
            assert!(!fin);
            assert!(!fill);
        } else {
            panic!("Wrong frame type");
        }
    }
}
