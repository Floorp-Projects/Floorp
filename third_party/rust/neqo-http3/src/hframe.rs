// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::settings::HSettings;
use neqo_common::{
    hex_with_len, qtrace, Decoder, Encoder, IncrementalDecoderBuffer, IncrementalDecoderIgnore,
    IncrementalDecoderUint,
};
use neqo_crypto::random;
use neqo_transport::Connection;
use std::convert::TryFrom;
use std::mem;

use crate::{Error, Res};

pub(crate) type HFrameType = u64;

pub(crate) const H3_FRAME_TYPE_DATA: HFrameType = 0x0;
pub(crate) const H3_FRAME_TYPE_HEADERS: HFrameType = 0x1;
const H3_FRAME_TYPE_CANCEL_PUSH: HFrameType = 0x3;
pub(crate) const H3_FRAME_TYPE_SETTINGS: HFrameType = 0x4;
const H3_FRAME_TYPE_PUSH_PROMISE: HFrameType = 0x5;
const H3_FRAME_TYPE_GOAWAY: HFrameType = 0x7;
const H3_FRAME_TYPE_MAX_PUSH_ID: HFrameType = 0xd;

pub const H3_RESERVED_FRAME_TYPES: &[HFrameType] = &[0x2, 0x6, 0x8, 0x9];

const MAX_READ_SIZE: usize = 4096;
// data for DATA frame is not read into HFrame::Data.
#[derive(PartialEq, Debug)]
pub enum HFrame {
    Data {
        len: u64, // length of the data
    },
    Headers {
        header_block: Vec<u8>,
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
    Grease,
}

impl HFrame {
    fn get_type(&self) -> HFrameType {
        match self {
            Self::Data { .. } => H3_FRAME_TYPE_DATA,
            Self::Headers { .. } => H3_FRAME_TYPE_HEADERS,
            Self::CancelPush { .. } => H3_FRAME_TYPE_CANCEL_PUSH,
            Self::Settings { .. } => H3_FRAME_TYPE_SETTINGS,
            Self::PushPromise { .. } => H3_FRAME_TYPE_PUSH_PROMISE,
            Self::Goaway { .. } => H3_FRAME_TYPE_GOAWAY,
            Self::MaxPushId { .. } => H3_FRAME_TYPE_MAX_PUSH_ID,
            Self::Grease => {
                let r = random(7);
                Decoder::from(&r).decode_uint(7).unwrap() * 0x1f + 0x21
            }
        }
    }

    pub fn encode(&self, enc: &mut Encoder) {
        enc.encode_varint(self.get_type());

        match self {
            Self::Data { len } => {
                // DATA frame only encode the length here.
                enc.encode_varint(*len);
            }
            Self::Headers { header_block } => {
                enc.encode_vvec(header_block);
            }
            Self::CancelPush { push_id } => {
                enc.encode_vvec_with(|enc_inner| {
                    enc_inner.encode_varint(*push_id);
                });
            }
            Self::Settings { settings } => {
                settings.encode_frame_contents(enc);
            }
            Self::PushPromise {
                push_id,
                header_block,
            } => {
                enc.encode_varint((header_block.len() + (Encoder::varint_len(*push_id))) as u64);
                enc.encode_varint(*push_id);
                enc.encode(header_block);
            }
            Self::Goaway { stream_id } => {
                enc.encode_vvec_with(|enc_inner| {
                    enc_inner.encode_varint(*stream_id);
                });
            }
            Self::MaxPushId { push_id } => {
                enc.encode_vvec_with(|enc_inner| {
                    enc_inner.encode_varint(*push_id);
                });
            }
            Self::Grease => {
                // Encode some number of random bytes.
                let r = random(8);
                enc.encode_vvec(&r[1..usize::from(1 + (r[0] & 0x7))]);
            }
        }
    }
}

#[derive(Clone, Debug)]
enum HFrameReaderState {
    GetType { decoder: IncrementalDecoderUint },
    GetLength { decoder: IncrementalDecoderUint },
    GetData { decoder: IncrementalDecoderBuffer },
    UnknownFrameDischargeData { decoder: IncrementalDecoderIgnore },
}

#[derive(Debug)]
pub struct HFrameReader {
    state: HFrameReaderState,
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
    #[must_use]
    pub fn new() -> Self {
        Self {
            state: HFrameReaderState::GetType {
                decoder: IncrementalDecoderUint::default(),
            },
            hframe_type: 0,
            hframe_len: 0,
            payload: Vec::new(),
        }
    }

    fn reset(&mut self) {
        self.state = HFrameReaderState::GetType {
            decoder: IncrementalDecoderUint::default(),
        };
    }

    fn min_remaining(&self) -> usize {
        match &self.state {
            HFrameReaderState::GetType { decoder } | HFrameReaderState::GetLength { decoder } => {
                decoder.min_remaining()
            }
            HFrameReaderState::GetData { decoder } => decoder.min_remaining(),
            HFrameReaderState::UnknownFrameDischargeData { decoder } => decoder.min_remaining(),
        }
    }

    fn decoding_in_progress(&self) -> bool {
        if let HFrameReaderState::GetType { decoder } = &self.state {
            decoder.decoding_in_progress()
        } else {
            true
        }
    }

    /// returns true if quic stream was closed.
    /// # Errors
    /// May return `HttpFrame` if a frame cannot be decoded.
    /// and `TransportStreamDoesNotExist` if `stream_recv` fails.
    pub fn receive(
        &mut self,
        conn: &mut Connection,
        stream_id: u64,
    ) -> Res<(Option<HFrame>, bool)> {
        loop {
            let to_read = std::cmp::min(self.min_remaining(), MAX_READ_SIZE);
            let mut buf = vec![0; to_read];
            let (output, read, fin) = match conn
                .stream_recv(stream_id, &mut buf)
                .map_err(|e| Error::map_stream_recv_errors(&e))?
            {
                (0, f) => (None, false, f),
                (amount, f) => {
                    qtrace!(
                        [conn],
                        "HFrameReader::receive: reading {} byte, fin={}",
                        amount,
                        f
                    );
                    (self.consume(Decoder::from(&buf[..amount]))?, true, f)
                }
            };

            if output.is_some() {
                break Ok((output, fin));
            }

            if fin {
                if self.decoding_in_progress() {
                    break Err(Error::HttpFrame);
                }
                break Ok((None, fin));
            }

            if !read {
                // There was no new data, exit the loop.
                break Ok((None, false));
            }
        }
    }

    /// # Errors
    /// May return `HttpFrame` if a frame cannot be decoded.
    fn consume(&mut self, mut input: Decoder) -> Res<Option<HFrame>> {
        match &mut self.state {
            HFrameReaderState::GetType { decoder } => {
                if let Some(v) = decoder.consume(&mut input) {
                    qtrace!("HFrameReader::receive: read frame type {}", v);
                    self.hframe_type = v;
                    if H3_RESERVED_FRAME_TYPES.contains(&self.hframe_type) {
                        return Err(Error::HttpFrameUnexpected);
                    }
                    self.state = HFrameReaderState::GetLength {
                        decoder: IncrementalDecoderUint::default(),
                    };
                }
            }

            HFrameReaderState::GetLength { decoder } => {
                if let Some(len) = decoder.consume(&mut input) {
                    qtrace!(
                        "HFrameReader::receive: frame type {} length {}",
                        self.hframe_type,
                        len
                    );
                    self.hframe_len = len;
                    self.state = match self.hframe_type {
                        // DATA payload are left on the quic stream and picked up separately
                        H3_FRAME_TYPE_DATA => {
                            return Ok(Some(self.get_frame()?));
                        }

                        // for other frames get all data before decoding.
                        H3_FRAME_TYPE_CANCEL_PUSH
                        | H3_FRAME_TYPE_SETTINGS
                        | H3_FRAME_TYPE_GOAWAY
                        | H3_FRAME_TYPE_MAX_PUSH_ID
                        | H3_FRAME_TYPE_PUSH_PROMISE
                        | H3_FRAME_TYPE_HEADERS => {
                            if len == 0 {
                                return Ok(Some(self.get_frame()?));
                            }
                            HFrameReaderState::GetData {
                                decoder: IncrementalDecoderBuffer::new(
                                    usize::try_from(len).or(Err(Error::HttpFrame))?,
                                ),
                            }
                        }
                        _ => {
                            if len == 0 {
                                HFrameReaderState::GetType {
                                    decoder: IncrementalDecoderUint::default(),
                                }
                            } else {
                                HFrameReaderState::UnknownFrameDischargeData {
                                    decoder: IncrementalDecoderIgnore::new(
                                        usize::try_from(len).or(Err(Error::HttpFrame))?,
                                    ),
                                }
                            }
                        }
                    };
                }
            }
            HFrameReaderState::GetData { decoder } => {
                if let Some(data) = decoder.consume(&mut input) {
                    qtrace!(
                        "received frame {}: {}",
                        self.hframe_type,
                        hex_with_len(&data[..])
                    );
                    self.payload = data;
                    return Ok(Some(self.get_frame()?));
                }
            }
            HFrameReaderState::UnknownFrameDischargeData { decoder } => {
                if decoder.consume(&mut input) {
                    self.reset();
                }
            }
        }
        Ok(None)
    }

    /// # Errors
    /// May return `HttpFrame` if a frame cannot be decoded.
    fn get_frame(&mut self) -> Res<HFrame> {
        let payload = mem::take(&mut self.payload);
        let mut dec = Decoder::from(&payload[..]);
        let f = match self.hframe_type {
            H3_FRAME_TYPE_DATA => HFrame::Data {
                len: self.hframe_len,
            },
            H3_FRAME_TYPE_HEADERS => HFrame::Headers {
                header_block: dec.decode_remainder().to_vec(),
            },
            H3_FRAME_TYPE_CANCEL_PUSH => HFrame::CancelPush {
                push_id: dec.decode_varint().ok_or(Error::HttpFrame)?,
            },
            H3_FRAME_TYPE_SETTINGS => {
                let mut settings = HSettings::default();
                settings.decode_frame_contents(&mut dec).map_err(|e| {
                    if e == Error::HttpSettings {
                        e
                    } else {
                        Error::HttpFrame
                    }
                })?;
                HFrame::Settings { settings }
            }
            H3_FRAME_TYPE_PUSH_PROMISE => HFrame::PushPromise {
                push_id: dec.decode_varint().ok_or(Error::HttpFrame)?,
                header_block: dec.decode_remainder().to_vec(),
            },
            H3_FRAME_TYPE_GOAWAY => HFrame::Goaway {
                stream_id: dec.decode_varint().ok_or(Error::HttpFrame)?,
            },
            H3_FRAME_TYPE_MAX_PUSH_ID => HFrame::MaxPushId {
                push_id: dec.decode_varint().ok_or(Error::HttpFrame)?,
            },
            _ => panic!("We should not be calling this function with unknown frame type!"),
        };
        self.reset();
        Ok(f)
    }
}

#[cfg(test)]
mod tests {
    use super::{Decoder, Encoder, Error, HFrame, HFrameReader, HSettings};
    use crate::settings::{HSetting, HSettingType};
    use neqo_crypto::AuthenticationStatus;
    use neqo_transport::{Connection, StreamType};
    use std::mem;
    use test_fixture::{connect, default_client, default_server, fixture_init, now};

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
        mem::drop(conn_s.process(out.dgram(), now()));
        conn_c.authenticated(AuthenticationStatus::Ok, now());
        let out = conn_c.process(None, now());
        mem::drop(conn_s.process(out.dgram(), now()));

        // create a stream
        let stream_id = conn_s.stream_create(StreamType::BiDi).unwrap();

        let mut fr: HFrameReader = HFrameReader::new();

        // conver string into u8 vector
        let buf = Encoder::from_hex(st);
        conn_s.stream_send(stream_id, &buf[..]).unwrap();
        let out = conn_s.process(None, now());
        mem::drop(conn_c.process(out.dgram(), now()));

        let (frame, fin) = fr.receive(&mut conn_c, stream_id).unwrap();
        assert!(!fin);
        assert!(frame.is_some());
        assert_eq!(*f, frame.unwrap());

        // Check remaining data.
        let mut buf = [0_u8; 100];
        let (amount, _) = conn_c.stream_recv(stream_id, &mut buf).unwrap();
        assert_eq!(amount, remaining);
    }

    #[test]
    fn test_data_frame() {
        let f = HFrame::Data { len: 3 };
        enc_dec(&f, "0003010203", 3);
    }

    #[test]
    fn test_headers_frame() {
        let f = HFrame::Headers {
            header_block: vec![0x01, 0x02, 0x03],
        };
        enc_dec(&f, "0103010203", 0);
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
    fn grease() {
        fn make_grease() -> u64 {
            let mut enc = Encoder::default();
            HFrame::Grease.encode(&mut enc);
            let mut dec = Decoder::from(&enc);
            let ft = dec.decode_varint().unwrap();
            assert_eq!((ft - 0x21) % 0x1f, 0);
            let body = dec.decode_vvec().unwrap();
            assert!(body.len() <= 7);
            ft
        }

        fixture_init();
        let t1 = make_grease();
        let t2 = make_grease();
        assert_ne!(t1, t2);
    }

    struct HFrameReaderTest {
        pub fr: HFrameReader,
        pub conn_c: Connection,
        pub conn_s: Connection,
        pub stream_id: u64,
    }

    impl HFrameReaderTest {
        pub fn new() -> Self {
            let (conn_c, mut conn_s) = connect();
            let stream_id = conn_s.stream_create(StreamType::BiDi).unwrap();
            Self {
                fr: HFrameReader::new(),
                conn_c,
                conn_s,
                stream_id,
            }
        }

        fn process(&mut self, v: &[u8]) -> Option<HFrame> {
            self.conn_s.stream_send(self.stream_id, v).unwrap();
            let out = self.conn_s.process(None, now());
            mem::drop(self.conn_c.process(out.dgram(), now()));
            let (frame, fin) = self.fr.receive(&mut self.conn_c, self.stream_id).unwrap();
            assert!(!fin);
            frame
        }
    }

    // Test receiving byte by byte for a SETTINGS frame.
    #[test]
    fn test_frame_reading_with_stream_settings1() {
        let mut fr = HFrameReaderTest::new();

        // Send and read settings frame 040406040804
        assert!(fr.process(&[0x4]).is_none());
        assert!(fr.process(&[0x4]).is_none());
        assert!(fr.process(&[0x6]).is_none());
        assert!(fr.process(&[0x4]).is_none());
        assert!(fr.process(&[0x8]).is_none());
        let frame = fr.process(&[0x4]);

        assert!(frame.is_some());
        if let HFrame::Settings { settings } = frame.unwrap() {
            assert!(settings.len() == 1);
            assert!(settings[0] == HSetting::new(HSettingType::MaxHeaderListSize, 4));
        } else {
            panic!("wrong frame type");
        }
    }

    // Test receiving byte by byte for a SETTINGS frame with larger varints
    #[test]
    fn test_frame_reading_with_stream_settings2() {
        let mut fr = HFrameReaderTest::new();

        // Read settings frame 400406064004084100
        for i in &[0x40, 0x04, 0x06, 0x06, 0x40, 0x04, 0x08, 0x41] {
            assert!(fr.process(&[*i]).is_none());
        }
        let frame = fr.process(&[0x0]);

        assert!(frame.is_some());
        if let HFrame::Settings { settings } = frame.unwrap() {
            assert!(settings.len() == 1);
            assert!(settings[0] == HSetting::new(HSettingType::MaxHeaderListSize, 4));
        } else {
            panic!("wrong frame type");
        }
    }

    // Test receiving byte by byte for a PUSH_PROMISE frame.
    #[test]
    fn test_frame_reading_with_stream_push_promise() {
        let mut fr = HFrameReaderTest::new();

        // Read push-promise frame 05054101010203
        for i in &[0x05, 0x05, 0x41, 0x01, 0x01, 0x02] {
            assert!(fr.process(&[*i]).is_none());
        }
        let frame = fr.process(&[0x3]);

        assert!(frame.is_some());
        if let HFrame::PushPromise {
            push_id,
            header_block,
        } = frame.unwrap()
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
        let mut fr = HFrameReaderTest::new();

        // Read data frame 0003010203
        let frame = fr.process(&[0x0, 0x3, 0x1, 0x2, 0x3]).unwrap();
        assert!(matches!(frame, HFrame::Data { len } if len == 3));

        // payloead is still on the stream.
        // assert that we have 3 bytes in the stream
        let mut buf = [0_u8; 100];
        let (amount, _) = fr.conn_c.stream_recv(fr.stream_id, &mut buf).unwrap();
        assert_eq!(amount, 3);
    }

    // Test an unknown frame
    #[test]
    fn test_unknown_frame() {
        // Construct an unknown frame.
        const UNKNOWN_FRAME_LEN: usize = 832;

        let mut fr = HFrameReaderTest::new();

        let mut enc = Encoder::with_capacity(UNKNOWN_FRAME_LEN + 4);
        enc.encode_varint(1028_u64); // Arbitrary type.
        enc.encode_varint(UNKNOWN_FRAME_LEN as u64);
        let mut buf: Vec<_> = enc.into();
        buf.resize(UNKNOWN_FRAME_LEN + buf.len(), 0);
        assert!(fr.process(&buf).is_none());

        // now receive a CANCEL_PUSH fram to see that frame reader is ok.
        let frame = fr.process(&[0x03, 0x01, 0x05]);
        assert!(frame.is_some());
        if let HFrame::CancelPush { push_id } = frame.unwrap() {
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
        test_to_send: &FrameReadingTestSend,
        expected_result: &FrameReadingTestExpect,
    ) {
        let mut fr = HFrameReaderTest::new();

        fr.conn_s.stream_send(fr.stream_id, &buf).unwrap();
        if let FrameReadingTestSend::DataWithFin = test_to_send {
            fr.conn_s.stream_close_send(fr.stream_id).unwrap();
        }

        let out = fr.conn_s.process(None, now());
        mem::drop(fr.conn_c.process(out.dgram(), now()));

        if let FrameReadingTestSend::DataThenFin = test_to_send {
            fr.conn_s.stream_close_send(fr.stream_id).unwrap();
            let out = fr.conn_s.process(None, now());
            mem::drop(fr.conn_c.process(out.dgram(), now()));
        }

        let rv = fr.fr.receive(&mut fr.conn_c, fr.stream_id);

        match expected_result {
            FrameReadingTestExpect::Error => assert_eq!(Err(Error::HttpFrame), rv),
            FrameReadingTestExpect::Incomplete => {
                assert_eq!(Ok((None, false)), rv);
            }
            FrameReadingTestExpect::FrameComplete => {
                let (f, fin) = rv.unwrap();
                assert!(!fin);
                assert!(f.is_some());
            }
            FrameReadingTestExpect::FrameAndStreamComplete => {
                let (f, fin) = rv.unwrap();
                assert!(fin);
                assert!(f.is_some());
            }
            FrameReadingTestExpect::StreamDoneWithoutFrame => {
                let (f, fin) = rv.unwrap();
                assert!(fin);
                assert!(f.is_none());
            }
        };
    }

    #[test]
    fn test_complete_and_incomplete_unknown_frame() {
        // Construct an unknown frame.
        const UNKNOWN_FRAME_LEN: usize = 832;
        let mut enc = Encoder::with_capacity(UNKNOWN_FRAME_LEN + 4);
        enc.encode_varint(1028_u64); // Arbitrary type.
        enc.encode_varint(UNKNOWN_FRAME_LEN as u64);
        let mut buf: Vec<_> = enc.into();
        buf.resize(UNKNOWN_FRAME_LEN + buf.len(), 0);

        let len = std::cmp::min(buf.len() - 1, 10);
        for i in 1..len {
            test_reading_frame(
                &buf[..i],
                &FrameReadingTestSend::OnlyData,
                &FrameReadingTestExpect::Incomplete,
            );
            test_reading_frame(
                &buf[..i],
                &FrameReadingTestSend::DataWithFin,
                &FrameReadingTestExpect::Error,
            );
            test_reading_frame(
                &buf[..i],
                &FrameReadingTestSend::DataThenFin,
                &FrameReadingTestExpect::Error,
            );
        }
        test_reading_frame(
            &buf,
            &FrameReadingTestSend::OnlyData,
            &FrameReadingTestExpect::Incomplete,
        );
        test_reading_frame(
            &buf,
            &FrameReadingTestSend::DataWithFin,
            &FrameReadingTestExpect::StreamDoneWithoutFrame,
        );
        test_reading_frame(
            &buf,
            &FrameReadingTestSend::DataThenFin,
            &FrameReadingTestExpect::StreamDoneWithoutFrame,
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
                &FrameReadingTestSend::OnlyData,
                if i >= done_state {
                    &FrameReadingTestExpect::FrameComplete
                } else {
                    &FrameReadingTestExpect::Incomplete
                },
            );
            test_reading_frame(
                &buf[..i],
                &FrameReadingTestSend::DataWithFin,
                match i.cmp(&done_state) {
                    Ordering::Greater => &FrameReadingTestExpect::FrameComplete,
                    Ordering::Equal => &FrameReadingTestExpect::FrameAndStreamComplete,
                    Ordering::Less => &FrameReadingTestExpect::Error,
                },
            );
            test_reading_frame(
                &buf[..i],
                &FrameReadingTestSend::DataThenFin,
                match i.cmp(&done_state) {
                    Ordering::Greater => &FrameReadingTestExpect::FrameComplete,
                    Ordering::Equal => &FrameReadingTestExpect::FrameAndStreamComplete,
                    Ordering::Less => &FrameReadingTestExpect::Error,
                },
            );
        }
        test_reading_frame(
            buf,
            &FrameReadingTestSend::OnlyData,
            &FrameReadingTestExpect::FrameComplete,
        );
        test_reading_frame(
            buf,
            &FrameReadingTestSend::DataWithFin,
            if buf.len() == done_state {
                &FrameReadingTestExpect::FrameAndStreamComplete
            } else {
                &FrameReadingTestExpect::FrameComplete
            },
        );
        test_reading_frame(
            buf,
            &FrameReadingTestSend::DataThenFin,
            if buf.len() == done_state {
                &FrameReadingTestExpect::FrameAndStreamComplete
            } else {
                &FrameReadingTestExpect::FrameComplete
            },
        );
    }

    #[test]
    fn test_complete_and_incomplete_frames() {
        const FRAME_LEN: usize = 10;
        const HEADER_BLOCK: &[u8] = &[0x01, 0x02, 0x03, 0x04];

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

        // H3_FRAME_TYPE_HEADERS empty header block
        let f = HFrame::Headers {
            header_block: Vec::new(),
        };
        let mut enc = Encoder::default();
        f.encode(&mut enc);
        let buf: Vec<_> = enc.into();
        test_complete_and_incomplete_frame(&buf, 2);

        // H3_FRAME_TYPE_HEADERS
        let f = HFrame::Headers {
            header_block: HEADER_BLOCK.to_vec(),
        };
        let mut enc = Encoder::default();
        f.encode(&mut enc);
        let buf: Vec<_> = enc.into();
        test_complete_and_incomplete_frame(&buf, buf.len());

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
            header_block: HEADER_BLOCK.to_vec(),
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
    }

    // Test closing a stream before any frame is sent should not cause an error.
    #[test]
    fn test_frame_reading_when_stream_is_closed_before_sending_data() {
        let mut fr = HFrameReaderTest::new();

        fr.conn_s.stream_send(fr.stream_id, &[0x00]).unwrap();
        let out = fr.conn_s.process(None, now());
        mem::drop(fr.conn_c.process(out.dgram(), now()));

        assert_eq!(Ok(()), fr.conn_c.stream_close_send(fr.stream_id));
        let out = fr.conn_c.process(None, now());
        mem::drop(fr.conn_s.process(out.dgram(), now()));
        assert_eq!(
            Ok((None, true)),
            fr.fr.receive(&mut fr.conn_s, fr.stream_id)
        );
    }
}
