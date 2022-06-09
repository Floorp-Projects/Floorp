// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::frames::{
    reader::FrameDecoder, FrameReader, HFrame, StreamReaderConnectionWrapper, WebTransportFrame,
};
use neqo_common::Encoder;
use neqo_crypto::AuthenticationStatus;
use neqo_transport::StreamType;
use std::mem;
use test_fixture::{default_client, default_server, now};

#[allow(clippy::many_single_char_names)]
pub fn enc_dec<T: FrameDecoder<T>>(d: &Encoder, st: &str, remaining: usize) -> T {
    // For data, headers and push_promise we do not read all bytes from the buffer
    let d2 = Encoder::from_hex(st);
    assert_eq!(d.as_ref(), &d2.as_ref()[..d.as_ref().len()]);

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

    let mut fr: FrameReader = FrameReader::new();

    // conver string into u8 vector
    let buf = Encoder::from_hex(st);
    conn_s.stream_send(stream_id, buf.as_ref()).unwrap();
    let out = conn_s.process(None, now());
    mem::drop(conn_c.process(out.dgram(), now()));

    let (frame, fin) = fr
        .receive::<T>(&mut StreamReaderConnectionWrapper::new(
            &mut conn_c,
            stream_id,
        ))
        .unwrap();
    assert!(!fin);
    assert!(frame.is_some());

    // Check remaining data.
    let mut buf = [0_u8; 100];
    let (amount, _) = conn_c.stream_recv(stream_id, &mut buf).unwrap();
    assert_eq!(amount, remaining);

    frame.unwrap()
}

pub fn enc_dec_hframe(f: &HFrame, st: &str, remaining: usize) {
    let mut d = Encoder::default();

    f.encode(&mut d);

    let frame = enc_dec::<HFrame>(&d, st, remaining);

    assert_eq!(*f, frame);
}

pub fn enc_dec_wtframe(f: &WebTransportFrame, st: &str, remaining: usize) {
    let mut d = Encoder::default();

    f.encode(&mut d);

    let frame = enc_dec::<WebTransportFrame>(&d, st, remaining);

    assert_eq!(*f, frame);
}

mod hframe;
mod reader;
mod wtframe;
