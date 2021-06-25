// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

use crate::{AppError, Http3StreamType, HttpRecvStream, ReceiveOutput, RecvStream, Res, ResetType};
use neqo_common::{Decoder, IncrementalDecoderUint};
use neqo_transport::Connection;

#[derive(Debug)]
pub enum NewStreamTypeReader {
    Read {
        reader: IncrementalDecoderUint,
        stream_id: u64,
    },
    Done,
}

impl NewStreamTypeReader {
    pub fn new(stream_id: u64) -> Self {
        NewStreamTypeReader::Read {
            reader: IncrementalDecoderUint::default(),
            stream_id,
        }
    }

    pub fn get_type(&mut self, conn: &mut Connection) -> Option<Http3StreamType> {
        // On any error we will only close this stream!
        loop {
            if let NewStreamTypeReader::Read {
                ref mut reader,
                stream_id,
            } = self
            {
                let to_read = reader.min_remaining();
                let mut buf = vec![0; to_read];
                match conn.stream_recv(*stream_id, &mut buf[..]) {
                    Ok((0, false)) => {
                        return None;
                    }
                    Ok((amount, false)) => {
                        if let Some(res) = reader.consume(&mut Decoder::from(&buf[..amount])) {
                            *self = NewStreamTypeReader::Done;
                            return Some(res.into());
                        }
                    }
                    Ok((_, true)) | Err(_) => {
                        *self = NewStreamTypeReader::Done;
                        return None;
                    }
                }
            } else {
                return None;
            }
        }
    }
}

impl RecvStream for NewStreamTypeReader {
    fn stream_reset(&mut self, _error: AppError, _reset_type: ResetType) -> Res<()> {
        *self = NewStreamTypeReader::Done;
        Ok(())
    }

    fn receive(&mut self, conn: &mut Connection) -> Res<ReceiveOutput> {
        Ok(self
            .get_type(conn)
            .map_or(ReceiveOutput::NoOutput, ReceiveOutput::NewStream))
    }

    fn done(&self) -> bool {
        matches!(*self, NewStreamTypeReader::Done)
    }

    fn stream_type(&self) -> Http3StreamType {
        Http3StreamType::NewStream
    }

    fn http_stream(&mut self) -> Option<&mut dyn HttpRecvStream> {
        None
    }
}

#[cfg(test)]
mod tests {
    use super::NewStreamTypeReader;
    use neqo_transport::{Connection, StreamType};
    use std::mem;
    use test_fixture::{connect, now};

    use crate::{
        Http3StreamType, ReceiveOutput, RecvStream, ResetType, HTTP3_UNI_STREAM_TYPE_CONTROL,
        HTTP3_UNI_STREAM_TYPE_PUSH,
    };
    use neqo_common::Encoder;
    use neqo_qpack::decoder::QPACK_UNI_STREAM_TYPE_DECODER;
    use neqo_qpack::encoder::QPACK_UNI_STREAM_TYPE_ENCODER;

    struct Test {
        conn_c: Connection,
        conn_s: Connection,
        stream_id: u64,
        decoder: NewStreamTypeReader,
    }

    impl Test {
        fn new() -> Self {
            let (mut conn_c, mut conn_s) = connect();
            // create a stream
            let stream_id = conn_s.stream_create(StreamType::UniDi).unwrap();
            let out = conn_s.process(None, now());
            mem::drop(conn_c.process(out.dgram(), now()));

            Self {
                conn_c,
                conn_s,
                stream_id,
                decoder: NewStreamTypeReader::new(stream_id),
            }
        }

        fn decode_buffer(&mut self, enc: &[u8], outcome: &ReceiveOutput, done: bool) {
            let len = enc.len() - 1;
            for i in 0..len {
                self.conn_s
                    .stream_send(self.stream_id, &enc[i..=i])
                    .unwrap();
                let out = self.conn_s.process(None, now());
                mem::drop(self.conn_c.process(out.dgram(), now()));
                assert_eq!(
                    self.decoder.receive(&mut self.conn_c).unwrap(),
                    ReceiveOutput::NoOutput
                );
                assert_eq!(self.decoder.done(), false);
            }
            self.conn_s
                .stream_send(self.stream_id, &enc[enc.len() - 1..])
                .unwrap();
            let out = self.conn_s.process(None, now());
            mem::drop(self.conn_c.process(out.dgram(), now()));
            assert_eq!(self.decoder.receive(&mut self.conn_c).unwrap(), *outcome);
            assert_eq!(self.decoder.done(), done);
        }

        fn decode(&mut self, stream_type: u64, outcome: &ReceiveOutput, done: bool) {
            let mut enc = Encoder::default();
            enc.encode_varint(stream_type);
            self.decode_buffer(&enc[..], outcome, done);
        }
    }

    #[test]
    fn decode_streams() {
        let mut t = Test::new();
        t.decode(
            QPACK_UNI_STREAM_TYPE_DECODER,
            &ReceiveOutput::NewStream(Http3StreamType::Encoder),
            true,
        );

        let mut t = Test::new();
        t.decode(
            QPACK_UNI_STREAM_TYPE_ENCODER,
            &ReceiveOutput::NewStream(Http3StreamType::Decoder),
            true,
        );

        let mut t = Test::new();
        t.decode(
            HTTP3_UNI_STREAM_TYPE_CONTROL,
            &ReceiveOutput::NewStream(Http3StreamType::Control),
            true,
        );

        let mut t = Test::new();
        t.decode(
            HTTP3_UNI_STREAM_TYPE_PUSH,
            &ReceiveOutput::NewStream(Http3StreamType::Push),
            true,
        );

        let mut t = Test::new();
        t.decode(
            0x3fff_ffff_ffff_ffff,
            &ReceiveOutput::NewStream(Http3StreamType::Unknown),
            true,
        );
    }

    #[test]
    fn done_decoding() {
        let mut t = Test::new();
        t.decode(
            0x3fff,
            &ReceiveOutput::NewStream(Http3StreamType::Unknown),
            true,
        );
        t.decode(HTTP3_UNI_STREAM_TYPE_PUSH, &ReceiveOutput::NoOutput, true);
    }

    #[test]
    fn decoding_truncate() {
        let mut t = Test::new();
        t.decode_buffer(&[0xff], &ReceiveOutput::NoOutput, false);
    }

    #[test]
    fn reset() {
        let mut t = Test::new();
        t.decoder.stream_reset(0x100, ResetType::Remote).unwrap();
        t.decode(HTTP3_UNI_STREAM_TYPE_PUSH, &ReceiveOutput::NoOutput, true);
    }
}
