// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{
    frames::{
        reader::FrameDecoder, FrameReader, HFrame, StreamReaderConnectionWrapper, WebTransportFrame,
    },
    settings::{HSetting, HSettingType, HSettings},
    Error,
};
use neqo_common::Encoder;
use neqo_transport::{Connection, StreamId, StreamType};
use std::fmt::Debug;
use std::mem;
use test_fixture::{connect, now};

struct FrameReaderTest {
    pub fr: FrameReader,
    pub conn_c: Connection,
    pub conn_s: Connection,
    pub stream_id: StreamId,
}

impl FrameReaderTest {
    pub fn new() -> Self {
        let (conn_c, mut conn_s) = connect();
        let stream_id = conn_s.stream_create(StreamType::BiDi).unwrap();
        Self {
            fr: FrameReader::new(),
            conn_c,
            conn_s,
            stream_id,
        }
    }

    fn process<T: FrameDecoder<T>>(&mut self, v: &[u8]) -> Option<T> {
        self.conn_s.stream_send(self.stream_id, v).unwrap();
        let out = self.conn_s.process(None, now());
        mem::drop(self.conn_c.process(out.dgram(), now()));
        let (frame, fin) = self
            .fr
            .receive::<T>(&mut StreamReaderConnectionWrapper::new(
                &mut self.conn_c,
                self.stream_id,
            ))
            .unwrap();
        assert!(!fin);
        frame
    }
}

// Test receiving byte by byte for a SETTINGS frame.
#[test]
fn test_frame_reading_with_stream_settings1() {
    let mut fr = FrameReaderTest::new();

    // Send and read settings frame 040406040804
    assert!(fr.process::<HFrame>(&[0x4]).is_none());
    assert!(fr.process::<HFrame>(&[0x4]).is_none());
    assert!(fr.process::<HFrame>(&[0x6]).is_none());
    assert!(fr.process::<HFrame>(&[0x4]).is_none());
    assert!(fr.process::<HFrame>(&[0x8]).is_none());
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
    let mut fr = FrameReaderTest::new();

    // Read settings frame 400406064004084100
    for i in &[0x40, 0x04, 0x06, 0x06, 0x40, 0x04, 0x08, 0x41] {
        assert!(fr.process::<HFrame>(&[*i]).is_none());
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
    let mut fr = FrameReaderTest::new();

    // Read push-promise frame 05054101010203
    for i in &[0x05, 0x05, 0x41, 0x01, 0x01, 0x02] {
        assert!(fr.process::<HFrame>(&[*i]).is_none());
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
    let mut fr = FrameReaderTest::new();

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

    let mut fr = FrameReaderTest::new();

    let mut enc = Encoder::with_capacity(UNKNOWN_FRAME_LEN + 4);
    enc.encode_varint(1028_u64); // Arbitrary type.
    enc.encode_varint(UNKNOWN_FRAME_LEN as u64);
    let mut buf: Vec<_> = enc.into();
    buf.resize(UNKNOWN_FRAME_LEN + buf.len(), 0);
    assert!(fr.process::<HFrame>(&buf).is_none());

    // now receive a CANCEL_PUSH fram to see that frame reader is ok.
    let frame = fr.process(&[0x03, 0x01, 0x05]);
    assert!(frame.is_some());
    if let HFrame::CancelPush { push_id } = frame.unwrap() {
        assert!(push_id == 5);
    } else {
        panic!("wrong frame type");
    }
}

// Test receiving byte by byte for a WT_FRAME_CLOSE_SESSION frame.
#[test]
fn test_frame_reading_with_stream_wt_close_session() {
    let mut fr = FrameReaderTest::new();

    // Read CloseSession frame 6843090000000548656c6c6f
    for i in &[
        0x68, 0x43, 0x09, 0x00, 0x00, 0x00, 0x05, 0x48, 0x65, 0x6c, 0x6c,
    ] {
        assert!(fr.process::<WebTransportFrame>(&[*i]).is_none());
    }
    let frame = fr.process::<WebTransportFrame>(&[0x6f]);

    assert!(frame.is_some());
    let WebTransportFrame::CloseSession { error, message } = frame.unwrap();
    assert_eq!(error, 5);
    assert_eq!(message, "Hello".to_string());
}

// Test an unknown frame for WebTransportFrames.
#[test]
fn test_unknown_wt_frame() {
    // Construct an unknown frame.
    const UNKNOWN_FRAME_LEN: usize = 832;

    let mut fr = FrameReaderTest::new();

    let mut enc = Encoder::with_capacity(UNKNOWN_FRAME_LEN + 4);
    enc.encode_varint(1028_u64); // Arbitrary type.
    enc.encode_varint(UNKNOWN_FRAME_LEN as u64);
    let mut buf: Vec<_> = enc.into();
    buf.resize(UNKNOWN_FRAME_LEN + buf.len(), 0);
    assert!(fr.process::<WebTransportFrame>(&buf).is_none());

    // now receive a WT_FRAME_CLOSE_SESSION fram to see that frame reader is ok.
    let frame = fr.process(&[
        0x68, 0x43, 0x09, 0x00, 0x00, 0x00, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f,
    ]);
    assert!(frame.is_some());
    let WebTransportFrame::CloseSession { error, message } = frame.unwrap();
    assert_eq!(error, 5);
    assert_eq!(message, "Hello".to_string());
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

fn test_reading_frame<T: FrameDecoder<T> + PartialEq + Debug>(
    buf: &[u8],
    test_to_send: &FrameReadingTestSend,
    expected_result: &FrameReadingTestExpect,
) {
    let mut fr = FrameReaderTest::new();

    fr.conn_s.stream_send(fr.stream_id, buf).unwrap();
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

    let rv = fr.fr.receive::<T>(&mut StreamReaderConnectionWrapper::new(
        &mut fr.conn_c,
        fr.stream_id,
    ));

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
        test_reading_frame::<HFrame>(
            &buf[..i],
            &FrameReadingTestSend::OnlyData,
            &FrameReadingTestExpect::Incomplete,
        );
        test_reading_frame::<HFrame>(
            &buf[..i],
            &FrameReadingTestSend::DataWithFin,
            &FrameReadingTestExpect::Error,
        );
        test_reading_frame::<HFrame>(
            &buf[..i],
            &FrameReadingTestSend::DataThenFin,
            &FrameReadingTestExpect::Error,
        );
    }
    test_reading_frame::<HFrame>(
        &buf,
        &FrameReadingTestSend::OnlyData,
        &FrameReadingTestExpect::Incomplete,
    );
    test_reading_frame::<HFrame>(
        &buf,
        &FrameReadingTestSend::DataWithFin,
        &FrameReadingTestExpect::StreamDoneWithoutFrame,
    );
    test_reading_frame::<HFrame>(
        &buf,
        &FrameReadingTestSend::DataThenFin,
        &FrameReadingTestExpect::StreamDoneWithoutFrame,
    );
}

// if we read more than done_state bytes FrameReader will be in done state.
fn test_complete_and_incomplete_frame<T: FrameDecoder<T> + PartialEq + Debug>(
    buf: &[u8],
    done_state: usize,
) {
    use std::cmp::Ordering;
    // Let's consume partial frames. It is enough to test partal frames
    // up to 10 byte. 10 byte is greater than frame type and frame
    // length and bit of data.
    let len = std::cmp::min(buf.len() - 1, 10);
    for i in 1..len {
        test_reading_frame::<T>(
            &buf[..i],
            &FrameReadingTestSend::OnlyData,
            if i >= done_state {
                &FrameReadingTestExpect::FrameComplete
            } else {
                &FrameReadingTestExpect::Incomplete
            },
        );
        test_reading_frame::<T>(
            &buf[..i],
            &FrameReadingTestSend::DataWithFin,
            match i.cmp(&done_state) {
                Ordering::Greater => &FrameReadingTestExpect::FrameComplete,
                Ordering::Equal => &FrameReadingTestExpect::FrameAndStreamComplete,
                Ordering::Less => &FrameReadingTestExpect::Error,
            },
        );
        test_reading_frame::<T>(
            &buf[..i],
            &FrameReadingTestSend::DataThenFin,
            match i.cmp(&done_state) {
                Ordering::Greater => &FrameReadingTestExpect::FrameComplete,
                Ordering::Equal => &FrameReadingTestExpect::FrameAndStreamComplete,
                Ordering::Less => &FrameReadingTestExpect::Error,
            },
        );
    }
    test_reading_frame::<T>(
        buf,
        &FrameReadingTestSend::OnlyData,
        &FrameReadingTestExpect::FrameComplete,
    );
    test_reading_frame::<T>(
        buf,
        &FrameReadingTestSend::DataWithFin,
        if buf.len() == done_state {
            &FrameReadingTestExpect::FrameAndStreamComplete
        } else {
            &FrameReadingTestExpect::FrameComplete
        },
    );
    test_reading_frame::<T>(
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
    test_complete_and_incomplete_frame::<HFrame>(&buf, 2);

    // H3_FRAME_TYPE_DATA len=FRAME_LEN
    let f = HFrame::Data {
        len: FRAME_LEN as u64,
    };
    let mut enc = Encoder::with_capacity(2);
    f.encode(&mut enc);
    let mut buf: Vec<_> = enc.into();
    buf.resize(FRAME_LEN + buf.len(), 0);
    test_complete_and_incomplete_frame::<HFrame>(&buf, 2);

    // H3_FRAME_TYPE_HEADERS empty header block
    let f = HFrame::Headers {
        header_block: Vec::new(),
    };
    let mut enc = Encoder::default();
    f.encode(&mut enc);
    let buf: Vec<_> = enc.into();
    test_complete_and_incomplete_frame::<HFrame>(&buf, 2);

    // H3_FRAME_TYPE_HEADERS
    let f = HFrame::Headers {
        header_block: HEADER_BLOCK.to_vec(),
    };
    let mut enc = Encoder::default();
    f.encode(&mut enc);
    let buf: Vec<_> = enc.into();
    test_complete_and_incomplete_frame::<HFrame>(&buf, buf.len());

    // H3_FRAME_TYPE_CANCEL_PUSH
    let f = HFrame::CancelPush { push_id: 5 };
    let mut enc = Encoder::default();
    f.encode(&mut enc);
    let buf: Vec<_> = enc.into();
    test_complete_and_incomplete_frame::<HFrame>(&buf, buf.len());

    // H3_FRAME_TYPE_SETTINGS
    let f = HFrame::Settings {
        settings: HSettings::new(&[HSetting::new(HSettingType::MaxHeaderListSize, 4)]),
    };
    let mut enc = Encoder::default();
    f.encode(&mut enc);
    let buf: Vec<_> = enc.into();
    test_complete_and_incomplete_frame::<HFrame>(&buf, buf.len());

    // H3_FRAME_TYPE_PUSH_PROMISE
    let f = HFrame::PushPromise {
        push_id: 4,
        header_block: HEADER_BLOCK.to_vec(),
    };
    let mut enc = Encoder::default();
    f.encode(&mut enc);
    let buf: Vec<_> = enc.into();
    test_complete_and_incomplete_frame::<HFrame>(&buf, buf.len());

    // H3_FRAME_TYPE_GOAWAY
    let f = HFrame::Goaway {
        stream_id: StreamId::new(5),
    };
    let mut enc = Encoder::default();
    f.encode(&mut enc);
    let buf: Vec<_> = enc.into();
    test_complete_and_incomplete_frame::<HFrame>(&buf, buf.len());

    // H3_FRAME_TYPE_MAX_PUSH_ID
    let f = HFrame::MaxPushId { push_id: 5 };
    let mut enc = Encoder::default();
    f.encode(&mut enc);
    let buf: Vec<_> = enc.into();
    test_complete_and_incomplete_frame::<HFrame>(&buf, buf.len());
}

#[test]
fn test_complete_and_incomplete_wt_frames() {
    // H3_FRAME_TYPE_MAX_PUSH_ID
    let f = WebTransportFrame::CloseSession {
        error: 5,
        message: "Hello".to_string(),
    };
    let mut enc = Encoder::default();
    f.encode(&mut enc);
    let buf: Vec<_> = enc.into();
    test_complete_and_incomplete_frame::<WebTransportFrame>(&buf, buf.len());
}

// Test closing a stream before any frame is sent should not cause an error.
#[test]
fn test_frame_reading_when_stream_is_closed_before_sending_data() {
    let mut fr = FrameReaderTest::new();

    fr.conn_s.stream_send(fr.stream_id, &[0x00]).unwrap();
    let out = fr.conn_s.process(None, now());
    mem::drop(fr.conn_c.process(out.dgram(), now()));

    assert_eq!(Ok(()), fr.conn_c.stream_close_send(fr.stream_id));
    let out = fr.conn_c.process(None, now());
    mem::drop(fr.conn_s.process(out.dgram(), now()));
    assert_eq!(
        Ok((None, true)),
        fr.fr
            .receive::<HFrame>(&mut StreamReaderConnectionWrapper::new(
                &mut fr.conn_s,
                fr.stream_id
            ))
    );
}

// Test closing a stream before any frame is sent should not cause an error.
// This is the same as the previous just for WebTransportFrame.
#[test]
fn test_wt_frame_reading_when_stream_is_closed_before_sending_data() {
    let mut fr = FrameReaderTest::new();

    fr.conn_s.stream_send(fr.stream_id, &[0x00]).unwrap();
    let out = fr.conn_s.process(None, now());
    mem::drop(fr.conn_c.process(out.dgram(), now()));

    assert_eq!(Ok(()), fr.conn_c.stream_close_send(fr.stream_id));
    let out = fr.conn_c.process(None, now());
    mem::drop(fr.conn_s.process(out.dgram(), now()));
    assert_eq!(
        Ok((None, true)),
        fr.fr
            .receive::<WebTransportFrame>(&mut StreamReaderConnectionWrapper::new(
                &mut fr.conn_s,
                fr.stream_id
            ))
    );
}
