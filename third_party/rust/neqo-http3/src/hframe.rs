// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::hsettings_frame::HSettings;
use neqo_common::{
    hex, qdebug, qtrace, Decoder, Encoder, IncrementalDecoder, IncrementalDecoderResult,
};
use neqo_transport::Connection;

use std::mem;

use crate::{Error, Res};

pub type HFrameType = u64;

pub const H3_FRAME_TYPE_DATA: HFrameType = 0x0;
pub const H3_FRAME_TYPE_HEADERS: HFrameType = 0x1;
const H3_FRAME_TYPE_CANCEL_PUSH: HFrameType = 0x3;
const H3_FRAME_TYPE_SETTINGS: HFrameType = 0x4;
const H3_FRAME_TYPE_PUSH_PROMISE: HFrameType = 0x5;
const H3_FRAME_TYPE_GOAWAY: HFrameType = 0x7;
const H3_FRAME_TYPE_MAX_PUSH_ID: HFrameType = 0xd;
const H3_FRAME_TYPE_DUPLICATE_PUSH: HFrameType = 0xe;

#[derive(Copy, Clone, PartialEq)]
pub enum HStreamType {
    Control,
    Request,
    Push,
}

// data for DATA and header blocks for HEADERS anf PUSH_PROMISE are not read into HFrame.
#[derive(PartialEq, Debug)]
pub enum HFrame {
    Data {
        len: u64, // length of the data
    },
    Headers {
        len: u64, // length of the header block
    },
    CancelPush {
        push_id: u64,
    },
    Settings {
        settings: HSettings,
    },
    PushPromise {
        push_id: u64,
        header_block: Vec<u8>,
    },
    Goaway {
        stream_id: u64,
    },
    MaxPushId {
        push_id: u64,
    },
    DuplicatePush {
        push_id: u64,
    },
}

impl HFrame {
    fn get_type(&self) -> HFrameType {
        match self {
            HFrame::Data { .. } => H3_FRAME_TYPE_DATA,
            HFrame::Headers { .. } => H3_FRAME_TYPE_HEADERS,
            HFrame::CancelPush { .. } => H3_FRAME_TYPE_CANCEL_PUSH,
            HFrame::Settings { .. } => H3_FRAME_TYPE_SETTINGS,
            HFrame::PushPromise { .. } => H3_FRAME_TYPE_PUSH_PROMISE,
            HFrame::Goaway { .. } => H3_FRAME_TYPE_GOAWAY,
            HFrame::MaxPushId { .. } => H3_FRAME_TYPE_MAX_PUSH_ID,
            HFrame::DuplicatePush { .. } => H3_FRAME_TYPE_DUPLICATE_PUSH,
        }
    }

    pub fn encode(&self, enc: &mut Encoder) {
        enc.encode_varint(self.get_type());

        match self {
            HFrame::Data { len } | HFrame::Headers { len } => {
                // DATA and HEADERS frames only encode the length here.
                enc.encode_varint(*len);
            }
            HFrame::CancelPush { push_id } => {
                enc.encode_vvec_with(|enc_inner| {
                    enc_inner.encode_varint(*push_id);
                });
            }
            HFrame::Settings { settings } => {
                settings.encode_frame_contents(enc);
            }
            HFrame::PushPromise {
                push_id,
                header_block,
            } => {
                enc.encode_varint((header_block.len() + (Encoder::varint_len(*push_id))) as u64);
                enc.encode_varint(*push_id);
                enc.encode(header_block);
            }
            HFrame::Goaway { stream_id } => {
                enc.encode_vvec_with(|enc_inner| {
                    enc_inner.encode_varint(*stream_id);
                });
            }
            HFrame::MaxPushId { push_id } => {
                enc.encode_vvec_with(|enc_inner| {
                    enc_inner.encode_varint(*push_id);
                });
            }
            HFrame::DuplicatePush { push_id } => {
                enc.encode_vvec_with(|enc_inner| {
                    enc_inner.encode_varint(*push_id);
                });
            }
        }
    }

    pub fn is_allowed(&self, s: HStreamType) -> bool {
        match self {
            HFrame::Data { .. } => !(s == HStreamType::Control),
            HFrame::Headers { .. } => !(s == HStreamType::Control),
            HFrame::CancelPush { .. } => (s == HStreamType::Control),
            HFrame::Settings { .. } => (s == HStreamType::Control),
            HFrame::PushPromise { .. } => (s == HStreamType::Request),
            HFrame::Goaway { .. } => (s == HStreamType::Control),
            HFrame::MaxPushId { .. } => (s == HStreamType::Control),
            HFrame::DuplicatePush { .. } => (s == HStreamType::Request),
        }
    }
}

#[derive(Copy, Clone, PartialEq, Debug)]
enum HFrameReaderState {
    BeforeFrame,
    GetType,
    GetLength,
    GetData,
    UnknownFrameDischargeData,
    Done,
}

#[derive(Debug)]
pub struct HFrameReader {
    state: HFrameReaderState,
    decoder: IncrementalDecoder,
    hframe_type: u64,
    hframe_len: u64,
    payload: Vec<u8>,
}

impl Default for HFrameReader {
    fn default() -> Self {
        Self::new()
    }
}

impl HFrameReader {
    pub fn new() -> HFrameReader {
        HFrameReader {
            state: HFrameReaderState::BeforeFrame,
            hframe_type: 0,
            hframe_len: 0,
            decoder: IncrementalDecoder::decode_varint(),
            payload: Vec::new(),
        }
    }

    pub fn reset(&mut self) {
        self.state = HFrameReaderState::BeforeFrame;
        self.decoder = IncrementalDecoder::decode_varint();
    }

    // returns true if quic stream was closed.
    pub fn receive(&mut self, conn: &mut Connection, stream_id: u64) -> Res<bool> {
        loop {
            let to_read = std::cmp::min(self.decoder.min_remaining(), 4096);
            let mut buf = vec![0; to_read];
            let fin;
            let mut input = match conn.stream_recv(stream_id, &mut buf[..]) {
                Ok((0, true)) => {
                    qtrace!([conn], "HFrameReader::receive: stream has been closed");
                    break match self.state {
                        HFrameReaderState::BeforeFrame => Ok(true),
                        _ => Err(Error::HttpFrameError),
                    };
                }
                Ok((0, false)) => break Ok(false),
                Ok((amount, f)) => {
                    qtrace!(
                        [conn],
                        "HFrameReader::receive: reading {} byte, fin={}",
                        amount,
                        f
                    );
                    fin = f;
                    Decoder::from(&buf[..amount])
                }
                Err(e) => {
                    qdebug!(
                        [conn],
                        "HFrameReader::receive: error reading data from stream {}: {:?}",
                        stream_id,
                        e
                    );
                    break Err(e.into());
                }
            };

            let progress = self.decoder.consume(&mut input);
            match self.state {
                HFrameReaderState::BeforeFrame | HFrameReaderState::GetType => match progress {
                    IncrementalDecoderResult::Uint(v) => {
                        qtrace!([conn], "HFrameReader::receive: read frame type {}", v);
                        self.hframe_type = v;
                        self.decoder = IncrementalDecoder::decode_varint();
                        self.state = HFrameReaderState::GetLength;
                    }
                    IncrementalDecoderResult::InProgress => {
                        self.state = HFrameReaderState::GetType;
                    }
                    _ => panic!("We must be in one of the states above"),
                },

                HFrameReaderState::GetLength => {
                    match progress {
                        IncrementalDecoderResult::Uint(len) => {
                            qtrace!(
                                [conn],
                                "HFrameReader::receive: frame type {} length {}",
                                self.hframe_type,
                                len
                            );
                            self.hframe_len = len;
                            self.state = match self.hframe_type {
                                // DATA and HEADERS payload are left on the quic stream and picked up separately
                                H3_FRAME_TYPE_DATA | H3_FRAME_TYPE_HEADERS => {
                                    HFrameReaderState::Done
                                }

                                // for other frames get all data before decoding.
                                H3_FRAME_TYPE_CANCEL_PUSH
                                | H3_FRAME_TYPE_SETTINGS
                                | H3_FRAME_TYPE_GOAWAY
                                | H3_FRAME_TYPE_MAX_PUSH_ID
                                | H3_FRAME_TYPE_DUPLICATE_PUSH
                                | H3_FRAME_TYPE_PUSH_PROMISE => {
                                    if len == 0 {
                                        HFrameReaderState::Done
                                    } else {
                                        self.decoder = IncrementalDecoder::decode(len as usize);
                                        HFrameReaderState::GetData
                                    }
                                }
                                _ => {
                                    if len == 0 {
                                        self.decoder = IncrementalDecoder::decode_varint();
                                        HFrameReaderState::BeforeFrame
                                    } else {
                                        self.decoder = IncrementalDecoder::ignore(len as usize);
                                        HFrameReaderState::UnknownFrameDischargeData
                                    }
                                }
                            };
                        }
                        IncrementalDecoderResult::InProgress => {}
                        _ => panic!("We must be in one of the states above"),
                    }
                }
                HFrameReaderState::GetData => {
                    match progress {
                        IncrementalDecoderResult::Buffer(data) => {
                            qtrace!(
                                [conn],
                                "received frame {}: {}",
                                self.hframe_type,
                                hex(&data[..])
                            );
                            self.payload = data;
                            self.state = HFrameReaderState::Done;
                        }
                        IncrementalDecoderResult::InProgress => {}
                        _ => panic!("We must be in one of the states above"),
                    };
                }
                HFrameReaderState::UnknownFrameDischargeData => {
                    match progress {
                        IncrementalDecoderResult::Ignored => {
                            self.reset();
                        }
                        IncrementalDecoderResult::InProgress => {}
                        _ => panic!("We must be in one of the states above"),
                    };
                }
                HFrameReaderState::Done => {
                    break Ok(fin);
                }
            };

            if self.state == HFrameReaderState::Done {
                break Ok(fin);
            }

            if fin {
                if self.state == HFrameReaderState::BeforeFrame {
                    break Ok(fin);
                } else {
                    break Err(Error::HttpFrameError);
                }
            }
        }
    }

    pub fn done(&self) -> bool {
        self.state == HFrameReaderState::Done
    }

    pub fn get_frame(&mut self) -> Res<HFrame> {
        if self.state != HFrameReaderState::Done {
            return Err(Error::NotEnoughData);
        }

        let payload = mem::replace(&mut self.payload, Vec::new());
        let mut dec = Decoder::from(&payload[..]);
        let f = match self.hframe_type {
            H3_FRAME_TYPE_DATA => HFrame::Data {
                len: self.hframe_len,
            },
            H3_FRAME_TYPE_HEADERS => HFrame::Headers {
                len: self.hframe_len,
            },
            H3_FRAME_TYPE_CANCEL_PUSH => HFrame::CancelPush {
                push_id: match dec.decode_varint() {
                    Some(v) => v,
                    _ => return Err(Error::NotEnoughData),
                },
            },
            H3_FRAME_TYPE_SETTINGS => {
                let mut settings = HSettings::default();
                settings.decode_frame_contents(&mut dec)?;
                HFrame::Settings { settings }
            }
            H3_FRAME_TYPE_PUSH_PROMISE => {
                let push_id = match dec.decode_varint() {
                    Some(v) => v,
                    _ => return Err(Error::NotEnoughData),
                };
                HFrame::PushPromise {
                    push_id,
                    header_block: dec.decode_remainder().to_vec(),
                }
            }
            H3_FRAME_TYPE_GOAWAY => HFrame::Goaway {
                stream_id: match dec.decode_varint() {
                    Some(v) => v,
                    _ => return Err(Error::NotEnoughData),
                },
            },
            H3_FRAME_TYPE_MAX_PUSH_ID => HFrame::MaxPushId {
                push_id: match dec.decode_varint() {
                    Some(v) => v,
                    _ => return Err(Error::NotEnoughData),
                },
            },
            H3_FRAME_TYPE_DUPLICATE_PUSH => HFrame::DuplicatePush {
                push_id: match dec.decode_varint() {
                    Some(v) => v,
                    _ => return Err(Error::NotEnoughData),
                },
            },
            _ => panic!("We should not be in state Done with unknown frame type!"),
        };
        self.reset();
        Ok(f)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::hsettings_frame::{HSetting, HSettingType};
    use neqo_crypto::AuthenticationStatus;
    use neqo_transport::StreamType;
    use num_traits::Num;
    use test_fixture::*;

    #[allow(clippy::many_single_char_names)]
    fn enc_dec(f: &HFrame, st: &str, remaining: usize) {
        let mut d = Encoder::default();

        f.encode(&mut d);

        // For data, headers and push_promise we do not read all bytes from the buffer
        let d2 = Encoder::from_hex(st);
        assert_eq!(&d[..], &d2[..d.len()]);

        let mut conn_c = default_client();
        let mut conn_s = default_server();
        let out = conn_c.process(None, now());
        let out = conn_s.process(out.dgram(), now());
        let out = conn_c.process(out.dgram(), now());
        conn_s.process(out.dgram(), now());
        conn_c.authenticated(AuthenticationStatus::Ok, now());
        let out = conn_c.process(None, now());
        conn_s.process(out.dgram(), now());

        // create a stream
        let stream_id = conn_s.stream_create(StreamType::BiDi).unwrap();

        let mut fr: HFrameReader = HFrameReader::new();

        // conver string into u8 vector
        let mut buf: Vec<u8> = Vec::new();
        if st.len() % 2 != 0 {
            panic!("Needs to be even length");
        }
        for i in 0..st.len() / 2 {
            let x = st.get(i * 2..i * 2 + 2);
            let v = <u8 as Num>::from_str_radix(x.unwrap(), 16).unwrap();
            buf.push(v);
        }
        conn_s.stream_send(stream_id, &buf).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());

        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        // Check remaining data.
        let mut buf = [0u8; 100];
        let (amount, _) = conn_c.stream_recv(stream_id, &mut buf).unwrap();
        assert_eq!(amount, remaining);

        assert!(fr.done());
        if let Ok(f2) = fr.get_frame() {
            assert_eq!(*f, f2);
        } else {
            panic!("wrong frame type");
        }
    }

    #[test]
    fn test_data_frame() {
        let f = HFrame::Data { len: 3 };
        enc_dec(&f, "0003010203", 3);
    }

    #[test]
    fn test_headers_frame() {
        let f = HFrame::Headers { len: 3 };
        enc_dec(&f, "0103010203", 3);
    }

    #[test]
    fn test_cancel_push_frame4() {
        let f = HFrame::CancelPush { push_id: 5 };
        enc_dec(&f, "030105", 0);
    }

    #[test]
    fn test_settings_frame4() {
        let f = HFrame::Settings {
            settings: HSettings::new(&[HSetting::new(HSettingType::MaxHeaderListSize, 4)]),
        };
        enc_dec(&f, "04020604", 0);
    }

    #[test]
    fn test_push_promise_frame4() {
        let f = HFrame::PushPromise {
            push_id: 4,
            header_block: vec![0x61, 0x62, 0x63, 0x64],
        };
        enc_dec(&f, "05050461626364", 0);
    }

    #[test]
    fn test_goaway_frame4() {
        let f = HFrame::Goaway { stream_id: 5 };
        enc_dec(&f, "070105", 0);
    }

    #[test]
    fn test_max_push_id_frame4() {
        let f = HFrame::MaxPushId { push_id: 5 };
        enc_dec(&f, "0d0105", 0);
    }

    #[test]
    fn test_duplicate_push_frame4() {
        let f = HFrame::DuplicatePush { push_id: 5 };
        enc_dec(&f, "0e0105", 0);
    }

    // We have 3 code paths in frame_reader:
    // 1) All frames except DATA, HEADERES and PUSH_PROMISE (here we test SETTING and SETTINGS with larger varints)
    // 2) PUSH_PROMISE and
    // 1) DATA and HEADERS frame (for this we will test DATA)

    // Test SETTINGS
    #[test]
    fn test_frame_reading_with_stream_settings1() {
        let (mut conn_c, mut conn_s) = connect();

        // create a stream
        let stream_id = conn_s.stream_create(StreamType::BiDi).unwrap();

        let mut fr: HFrameReader = HFrameReader::new();

        // Send and read settings frame 040406040804
        conn_s.stream_send(stream_id, &[0x4]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        conn_s.stream_send(stream_id, &[0x4]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        conn_s.stream_send(stream_id, &[0x6]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        conn_s.stream_send(stream_id, &[0x4]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        conn_s.stream_send(stream_id, &[0x8]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        conn_s.stream_send(stream_id, &[0x4]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        assert!(fr.done());
        let f = fr.get_frame();
        if let HFrame::Settings { settings } = f.unwrap() {
            assert!(settings.len() == 1);
            //            for i in settings.iter() {
            assert!(settings[0] == HSetting::new(HSettingType::MaxHeaderListSize, 4));
        } else {
            panic!("wrong frame type");
        }
    }

    // Test SETTINGS with larger varints
    #[test]
    fn test_frame_reading_with_stream_settings2() {
        let (mut conn_c, mut conn_s) = connect();

        // create a stream
        let stream_id = conn_s.stream_create(StreamType::BiDi).unwrap();

        let mut fr: HFrameReader = HFrameReader::new();

        // Read settings frame 400406064004084100
        conn_s.stream_send(stream_id, &[0x40]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        conn_s.stream_send(stream_id, &[0x4]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        conn_s.stream_send(stream_id, &[0x6]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        conn_s.stream_send(stream_id, &[0x6]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        conn_s.stream_send(stream_id, &[0x40]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        conn_s.stream_send(stream_id, &[0x4]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        conn_s.stream_send(stream_id, &[0x8]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        conn_s.stream_send(stream_id, &[0x41]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        conn_s.stream_send(stream_id, &[0x0]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        assert!(fr.done());
        let f = fr.get_frame();
        assert!(f.is_ok());
        if let HFrame::Settings { settings } = f.unwrap() {
            assert!(settings.len() == 1);
            assert!(settings[0] == HSetting::new(HSettingType::MaxHeaderListSize, 4));
        } else {
            panic!("wrong frame type");
        }
    }

    // Test PUSH_PROMISE
    #[test]
    fn test_frame_reading_with_stream_push_promise() {
        let (mut conn_c, mut conn_s) = connect();

        // create a stream
        let stream_id = conn_s.stream_create(StreamType::BiDi).unwrap();

        let mut fr: HFrameReader = HFrameReader::new();

        // Read pushpromise frame 05054101010203
        conn_s.stream_send(stream_id, &[0x5]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        conn_s.stream_send(stream_id, &[0x5]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        conn_s.stream_send(stream_id, &[0x41]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        conn_s
            .stream_send(stream_id, &[0x1, 0x1, 0x2, 0x3])
            .unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        // headers are still on the stream.
        // assert that we do not have any more date on the stream
        let mut buf = [0u8; 100];
        let (amount, _) = conn_c.stream_recv(stream_id, &mut buf).unwrap();
        assert_eq!(amount, 0);

        assert!(fr.done());
        let f = fr.get_frame();
        assert!(f.is_ok());
        if let HFrame::PushPromise {
            push_id,
            header_block,
        } = f.unwrap()
        {
            assert_eq!(push_id, 257);
            assert_eq!(header_block, &[0x1, 0x2, 0x3]);
        } else {
            panic!("wrong frame type");
        }
    }

    // Test DATA
    #[test]
    fn test_frame_reading_with_stream_data() {
        let (mut conn_c, mut conn_s) = connect();

        // create a stream
        let stream_id = conn_s.stream_create(StreamType::BiDi).unwrap();

        let mut fr: HFrameReader = HFrameReader::new();

        // Read data frame 0003010203
        conn_s
            .stream_send(stream_id, &[0x0, 0x3, 0x1, 0x2, 0x3])
            .unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        // payloead is still on the stream.
        // assert that we have 3 bytes in the stream
        let mut buf = [0u8; 100];
        let (amount, _) = conn_c.stream_recv(stream_id, &mut buf).unwrap();
        assert_eq!(amount, 3);

        assert!(fr.done());
        let f = fr.get_frame();
        assert!(f.is_ok());
        if let HFrame::Data { len } = f.unwrap() {
            assert!(len == 3);
        } else {
            panic!("wrong frame type");
        }
    }

    // Test an unknown frame
    #[test]
    fn test_unknown_frame() {
        let (mut conn_c, mut conn_s) = connect();

        // create a stream
        let stream_id = conn_s.stream_create(StreamType::BiDi).unwrap();

        let mut fr: HFrameReader = HFrameReader::new();

        // Construct an unknown frame.
        const UNKNOWN_FRAME_LEN: usize = 832;
        let mut enc = Encoder::with_capacity(UNKNOWN_FRAME_LEN + 4);
        enc.encode_varint(1028u64); // Arbitrary type.
        enc.encode_varint(UNKNOWN_FRAME_LEN as u64);
        let mut buf: Vec<_> = enc.into();
        buf.resize(UNKNOWN_FRAME_LEN + buf.len(), 0);
        conn_s.stream_send(stream_id, &buf).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        // now receive a CANCEL_PUSH fram to see that frame reader is ok.
        conn_s.stream_send(stream_id, &[0x03, 0x01, 0x05]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());
        assert_eq!(Ok(false), fr.receive(&mut conn_c, stream_id));

        assert!(fr.done());
        let f = fr.get_frame();
        assert!(f.is_ok());
        if let HFrame::CancelPush { push_id } = f.unwrap() {
            assert!(push_id == 5);
        } else {
            panic!("wrong frame type");
        }
    }

    enum FrameReadingTestSend {
        OnlyData,
        DataWithFin,
        DataThenFin,
    }

    enum FrameReadingTestExpect {
        Error,
        Incomplete,
        FrameComplete,
        FrameAndStreamComplete,
        StreamDoneWithoutFrame,
    }

    fn test_reading_frame(
        buf: &[u8],
        test_to_send: FrameReadingTestSend,
        expected_result: FrameReadingTestExpect,
    ) {
        let (mut conn_c, mut conn_s) = connect();

        // create a stream
        let stream_id = conn_s.stream_create(StreamType::BiDi).unwrap();

        conn_s.stream_send(stream_id, &buf).unwrap();
        if let FrameReadingTestSend::DataWithFin = test_to_send {
            conn_s.stream_close_send(stream_id).unwrap();
        }

        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());

        if let FrameReadingTestSend::DataThenFin = test_to_send {
            conn_s.stream_close_send(stream_id).unwrap();
            let out = conn_s.process(None, now());
            conn_c.process(out.dgram(), now());
        }

        let mut fr = HFrameReader::new();
        let rv = fr.receive(&mut conn_c, stream_id);

        match expected_result {
            FrameReadingTestExpect::Error => assert_eq!(Err(Error::HttpFrameError), rv),
            FrameReadingTestExpect::Incomplete => {
                assert_eq!(Ok(false), rv);
                assert_eq!(false, fr.done());
            }
            FrameReadingTestExpect::FrameComplete => {
                assert_eq!(Ok(false), rv);
                assert_eq!(true, fr.done());
            }
            FrameReadingTestExpect::FrameAndStreamComplete => {
                assert_eq!(Ok(true), rv);
                assert_eq!(true, fr.done());
            }
            FrameReadingTestExpect::StreamDoneWithoutFrame => {
                assert_eq!(Ok(true), rv);
                assert_eq!(false, fr.done());
            }
        };
    }

    #[test]
    fn test_complete_and_incomplete_unknown_frame() {
        // Construct an unknown frame.
        const UNKNOWN_FRAME_LEN: usize = 832;
        let mut enc = Encoder::with_capacity(UNKNOWN_FRAME_LEN + 4);
        enc.encode_varint(1028u64); // Arbitrary type.
        enc.encode_varint(UNKNOWN_FRAME_LEN as u64);
        let mut buf: Vec<_> = enc.into();
        buf.resize(UNKNOWN_FRAME_LEN + buf.len(), 0);

        let len = std::cmp::min(buf.len() - 1, 10);
        for i in 1..len {
            test_reading_frame(
                &buf[..i],
                FrameReadingTestSend::OnlyData,
                FrameReadingTestExpect::Incomplete,
            );
            test_reading_frame(
                &buf[..i],
                FrameReadingTestSend::DataWithFin,
                FrameReadingTestExpect::Error,
            );
            test_reading_frame(
                &buf[..i],
                FrameReadingTestSend::DataThenFin,
                FrameReadingTestExpect::Error,
            );
        }
        test_reading_frame(
            &buf,
            FrameReadingTestSend::OnlyData,
            FrameReadingTestExpect::Incomplete,
        );
        test_reading_frame(
            &buf,
            FrameReadingTestSend::DataWithFin,
            FrameReadingTestExpect::StreamDoneWithoutFrame,
        );
        test_reading_frame(
            &buf,
            FrameReadingTestSend::DataThenFin,
            FrameReadingTestExpect::StreamDoneWithoutFrame,
        );
    }

    // if we read more than done_state bytes HFrameReader will be in done state.
    fn test_complete_and_incomplete_frame(buf: &[u8], done_state: usize) {
        use std::cmp::Ordering;
        // Let's consume partial frames. It is enough to test partal frames
        // up to 10 byte. 10 byte is greater than frame type and frame
        // length and bit of data.
        let len = std::cmp::min(buf.len() - 1, 10);
        for i in 1..len {
            test_reading_frame(
                &buf[..i],
                FrameReadingTestSend::OnlyData,
                if i >= done_state {
                    FrameReadingTestExpect::FrameComplete
                } else {
                    FrameReadingTestExpect::Incomplete
                },
            );
            test_reading_frame(
                &buf[..i],
                FrameReadingTestSend::DataWithFin,
                match i.cmp(&done_state) {
                    Ordering::Greater => FrameReadingTestExpect::FrameComplete,
                    Ordering::Equal => FrameReadingTestExpect::FrameAndStreamComplete,
                    Ordering::Less => FrameReadingTestExpect::Error,
                },
            );
            test_reading_frame(
                &buf[..i],
                FrameReadingTestSend::DataThenFin,
                match i.cmp(&done_state) {
                    Ordering::Greater => FrameReadingTestExpect::FrameComplete,
                    Ordering::Equal => FrameReadingTestExpect::FrameAndStreamComplete,
                    Ordering::Less => FrameReadingTestExpect::Error,
                },
            );
        }
        test_reading_frame(
            buf,
            FrameReadingTestSend::OnlyData,
            FrameReadingTestExpect::FrameComplete,
        );
        test_reading_frame(
            buf,
            FrameReadingTestSend::DataWithFin,
            if buf.len() == done_state {
                FrameReadingTestExpect::FrameAndStreamComplete
            } else {
                FrameReadingTestExpect::FrameComplete
            },
        );
        test_reading_frame(
            buf,
            FrameReadingTestSend::DataThenFin,
            if buf.len() == done_state {
                FrameReadingTestExpect::FrameAndStreamComplete
            } else {
                FrameReadingTestExpect::FrameComplete
            },
        );
    }

    #[test]
    fn test_complete_and_incomplete_frames() {
        const FRAME_LEN: usize = 10;

        // H3_FRAME_TYPE_DATA len=0
        let f = HFrame::Data { len: 0 };
        let mut enc = Encoder::with_capacity(2);
        f.encode(&mut enc);
        let buf: Vec<_> = enc.into();
        test_complete_and_incomplete_frame(&buf, 2);

        // H3_FRAME_TYPE_DATA len=FRAME_LEN
        let f = HFrame::Data {
            len: FRAME_LEN as u64,
        };
        let mut enc = Encoder::with_capacity(2);
        f.encode(&mut enc);
        let mut buf: Vec<_> = enc.into();
        buf.resize(FRAME_LEN + buf.len(), 0);
        test_complete_and_incomplete_frame(&buf, 2);

        // H3_FRAME_TYPE_HEADERS len=0
        let f = HFrame::Data { len: 0 };
        let mut enc = Encoder::with_capacity(2);
        f.encode(&mut enc);
        let buf: Vec<_> = enc.into();
        test_complete_and_incomplete_frame(&buf, 2);

        // H3_FRAME_TYPE_HEADERS len=FRAME_LEN
        let f = HFrame::Headers {
            len: FRAME_LEN as u64,
        };
        let mut enc = Encoder::with_capacity(2);
        f.encode(&mut enc);
        let mut buf: Vec<_> = enc.into();
        buf.resize(FRAME_LEN + buf.len(), 0);
        test_complete_and_incomplete_frame(&buf, 2);

        // H3_FRAME_TYPE_CANCEL_PUSH
        let f = HFrame::CancelPush { push_id: 5 };
        let mut enc = Encoder::default();
        f.encode(&mut enc);
        let buf: Vec<_> = enc.into();
        test_complete_and_incomplete_frame(&buf, buf.len());

        // H3_FRAME_TYPE_SETTINGS
        let f = HFrame::Settings {
            settings: HSettings::new(&[HSetting::new(HSettingType::MaxHeaderListSize, 4)]),
        };
        let mut enc = Encoder::default();
        f.encode(&mut enc);
        let buf: Vec<_> = enc.into();
        test_complete_and_incomplete_frame(&buf, buf.len());

        // H3_FRAME_TYPE_PUSH_PROMISE
        let f = HFrame::PushPromise {
            push_id: 4,
            header_block: vec![0x01, 0x02, 0x03, 0x04],
        };
        let mut enc = Encoder::default();
        f.encode(&mut enc);
        let buf: Vec<_> = enc.into();
        test_complete_and_incomplete_frame(&buf, buf.len());

        // H3_FRAME_TYPE_GOAWAY
        let f = HFrame::Goaway { stream_id: 5 };
        let mut enc = Encoder::default();
        f.encode(&mut enc);
        let buf: Vec<_> = enc.into();
        test_complete_and_incomplete_frame(&buf, buf.len());

        // H3_FRAME_TYPE_MAX_PUSH_ID
        let f = HFrame::MaxPushId { push_id: 5 };
        let mut enc = Encoder::default();
        f.encode(&mut enc);
        let buf: Vec<_> = enc.into();
        test_complete_and_incomplete_frame(&buf, buf.len());

        // H3_FRAME_TYPE_DUPLICATE_PUSH
        let f = HFrame::DuplicatePush { push_id: 5 };
        let mut enc = Encoder::default();
        f.encode(&mut enc);
        let buf: Vec<_> = enc.into();
        test_complete_and_incomplete_frame(&buf, buf.len());
    }

    // Test closing a stream before any frame is sent should not cause an error.
    #[test]
    fn test_frame_reading_when_stream_is_closed_before_sending_data() {
        let (mut conn_c, mut conn_s) = connect();

        // create a stream
        let stream_id = conn_s.stream_create(StreamType::BiDi).unwrap();
        conn_s.stream_send(stream_id, &[0x00]).unwrap();
        let out = conn_s.process(None, now());
        conn_c.process(out.dgram(), now());

        let mut fr: HFrameReader = HFrameReader::new();
        assert_eq!(Ok(()), conn_c.stream_close_send(stream_id));
        let out = conn_c.process(None, now());
        conn_s.process(out.dgram(), now());
        assert_eq!(Ok(true), fr.receive(&mut conn_s, stream_id));
    }
}
