// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

use crate::control_stream_local::HTTP3_UNI_STREAM_TYPE_CONTROL;
use crate::frames::H3_FRAME_TYPE_HEADERS;
use crate::{CloseType, Error, Http3StreamType, ReceiveOutput, RecvStream, Res, Stream};
use neqo_common::{qtrace, Decoder, IncrementalDecoderUint, Role};
use neqo_qpack::decoder::QPACK_UNI_STREAM_TYPE_DECODER;
use neqo_qpack::encoder::QPACK_UNI_STREAM_TYPE_ENCODER;
use neqo_transport::{Connection, StreamId, StreamType};

pub const HTTP3_UNI_STREAM_TYPE_PUSH: u64 = 0x1;
pub const WEBTRANSPORT_UNI_STREAM: u64 = 0x54;
pub const WEBTRANSPORT_STREAM: u64 = 0x41;

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum NewStreamType {
    Control,
    Decoder,
    Encoder,
    Push(u64),
    WebTransportStream(u64),
    Http,
    Unknown,
}

impl NewStreamType {
    /// Get the final `NewStreamType` from a stream type. All streams, except Push stream,
    /// are identified by the type only. This function will return None for the Push stream
    /// because it needs the ID besides the type.
    /// # Error
    /// Push streams received by the server are not allowed and this function will return
    /// `HttpStreamCreation` error.
    fn final_stream_type(
        stream_type: u64,
        trans_stream_type: StreamType,
        role: Role,
    ) -> Res<Option<NewStreamType>> {
        match (stream_type, trans_stream_type, role) {
            (HTTP3_UNI_STREAM_TYPE_CONTROL, StreamType::UniDi, _) => {
                Ok(Some(NewStreamType::Control))
            }
            (QPACK_UNI_STREAM_TYPE_ENCODER, StreamType::UniDi, _) => {
                Ok(Some(NewStreamType::Decoder))
            }
            (QPACK_UNI_STREAM_TYPE_DECODER, StreamType::UniDi, _) => {
                Ok(Some(NewStreamType::Encoder))
            }
            (HTTP3_UNI_STREAM_TYPE_PUSH, StreamType::UniDi, Role::Client)
            | (WEBTRANSPORT_UNI_STREAM, StreamType::UniDi, _)
            | (WEBTRANSPORT_STREAM, StreamType::BiDi, _) => Ok(None),
            (H3_FRAME_TYPE_HEADERS, StreamType::BiDi, Role::Server) => {
                Ok(Some(NewStreamType::Http))
            }
            (_, StreamType::BiDi, Role::Server) => Err(Error::HttpFrame),
            (HTTP3_UNI_STREAM_TYPE_PUSH, StreamType::UniDi, Role::Server)
            | (_, StreamType::BiDi, Role::Client) => Err(Error::HttpStreamCreation),
            _ => Ok(Some(NewStreamType::Unknown)),
        }
    }
}

/// `NewStreamHeadReader` reads the head of an unidirectional stream to identify the stream.
/// There are 2 type of streams:
///  - streams identified by the single type (varint encoded). Most streams belong to
///    this category. The `NewStreamHeadReader` will switch from `ReadType`to `Done` state.
///  - streams identified by the type and the ID (both varint encoded). For example, a
///    push stream is identified by the type and `PushId`. After reading the type in
///    the `ReadType` state, `NewStreamHeadReader` changes to `ReadId` state and from there
///    to `Done` state
#[derive(Debug)]
pub enum NewStreamHeadReader {
    ReadType {
        role: Role,
        reader: IncrementalDecoderUint,
        stream_id: StreamId,
    },
    ReadId {
        stream_type: u64,
        reader: IncrementalDecoderUint,
        stream_id: StreamId,
    },
    Done,
}

impl NewStreamHeadReader {
    pub fn new(stream_id: StreamId, role: Role) -> Self {
        NewStreamHeadReader::ReadType {
            role,
            reader: IncrementalDecoderUint::default(),
            stream_id,
        }
    }

    fn read(&mut self, conn: &mut Connection) -> Res<(Option<u64>, bool)> {
        if let NewStreamHeadReader::ReadType {
            reader, stream_id, ..
        }
        | NewStreamHeadReader::ReadId {
            reader, stream_id, ..
        } = self
        {
            loop {
                let to_read = reader.min_remaining();
                let mut buf = vec![0; to_read];
                match conn.stream_recv(*stream_id, &mut buf[..])? {
                    (0, f) => return Ok((None, f)),
                    (amount, f) => {
                        let res = reader.consume(&mut Decoder::from(&buf[..amount]));
                        if res.is_some() || f {
                            return Ok((res, f));
                        }
                    }
                }
            }
        } else {
            Ok((None, false))
        }
    }

    pub fn get_type(&mut self, conn: &mut Connection) -> Res<Option<NewStreamType>> {
        loop {
            let (output, fin) = self.read(conn)?;
            let output = if let Some(o) = output {
                o
            } else {
                if fin {
                    *self = NewStreamHeadReader::Done;
                    return Err(Error::HttpStreamCreation);
                }
                return Ok(None);
            };

            qtrace!("Decoded uint {}", output);
            match self {
                NewStreamHeadReader::ReadType {
                    role, stream_id, ..
                } => {
                    // final_stream_type may return:
                    //  - an error if a stream type is not allowed for the role, e.g. Push
                    //    stream received at the server.
                    //  - a final type if a stream is only identify by the type
                    //  - None - if a stream is not identified by the type only, but it needs
                    //    additional data from the header to produce the final type, e.g.
                    //    a push stream needs pushId as well.
                    let final_type =
                        NewStreamType::final_stream_type(output, stream_id.stream_type(), *role);
                    match (&final_type, fin) {
                        (Err(_), _) => {
                            *self = NewStreamHeadReader::Done;
                            return final_type;
                        }
                        (Ok(t), true) => {
                            *self = NewStreamHeadReader::Done;
                            return Self::map_stream_fin(*t);
                        }
                        (Ok(Some(t)), false) => {
                            qtrace!("Decoded stream type {:?}", *t);
                            *self = NewStreamHeadReader::Done;
                            return final_type;
                        }
                        (Ok(None), false) => {
                            // This is a push stream and it needs more data to be decoded.
                            *self = NewStreamHeadReader::ReadId {
                                reader: IncrementalDecoderUint::default(),
                                stream_id: *stream_id,
                                stream_type: output,
                            }
                        }
                    }
                }
                NewStreamHeadReader::ReadId { stream_type, .. } => {
                    let is_push = *stream_type == HTTP3_UNI_STREAM_TYPE_PUSH;
                    *self = NewStreamHeadReader::Done;
                    qtrace!("New Stream stream push_id={}", output);
                    if fin {
                        return Err(Error::HttpGeneralProtocol);
                    }
                    return if is_push {
                        Ok(Some(NewStreamType::Push(output)))
                    } else {
                        Ok(Some(NewStreamType::WebTransportStream(output)))
                    };
                }
                NewStreamHeadReader::Done => {
                    unreachable!("Cannot be in state NewStreamHeadReader::Done");
                }
            }
        }
    }

    fn map_stream_fin(decoded: Option<NewStreamType>) -> Res<Option<NewStreamType>> {
        match decoded {
            Some(NewStreamType::Control | NewStreamType::Encoder | NewStreamType::Decoder) => {
                Err(Error::HttpClosedCriticalStream)
            }
            None => Err(Error::HttpStreamCreation),
            Some(NewStreamType::Http) => Err(Error::HttpFrame),
            Some(NewStreamType::Unknown) => Ok(decoded),
            Some(NewStreamType::Push(_) | NewStreamType::WebTransportStream(_)) => {
                unreachable!("PushStream and WebTransport are mapped to None at this stage.")
            }
        }
    }

    fn done(&self) -> bool {
        matches!(self, NewStreamHeadReader::Done)
    }
}

impl Stream for NewStreamHeadReader {
    fn stream_type(&self) -> Http3StreamType {
        Http3StreamType::NewStream
    }
}

impl RecvStream for NewStreamHeadReader {
    fn reset(&mut self, _close_type: CloseType) -> Res<()> {
        *self = NewStreamHeadReader::Done;
        Ok(())
    }

    fn receive(&mut self, conn: &mut Connection) -> Res<(ReceiveOutput, bool)> {
        Ok((
            self.get_type(conn)?
                .map_or(ReceiveOutput::NoOutput, ReceiveOutput::NewStream),
            self.done(),
        ))
    }
}

#[cfg(test)]
mod tests {
    use super::{
        NewStreamHeadReader, HTTP3_UNI_STREAM_TYPE_PUSH, WEBTRANSPORT_STREAM,
        WEBTRANSPORT_UNI_STREAM,
    };
    use neqo_transport::{Connection, StreamId, StreamType};
    use std::mem;
    use test_fixture::{connect, now};

    use crate::control_stream_local::HTTP3_UNI_STREAM_TYPE_CONTROL;
    use crate::frames::H3_FRAME_TYPE_HEADERS;
    use crate::{CloseType, Error, NewStreamType, ReceiveOutput, RecvStream, Res};
    use neqo_common::{Encoder, Role};
    use neqo_qpack::decoder::QPACK_UNI_STREAM_TYPE_DECODER;
    use neqo_qpack::encoder::QPACK_UNI_STREAM_TYPE_ENCODER;

    struct Test {
        conn_c: Connection,
        conn_s: Connection,
        stream_id: StreamId,
        decoder: NewStreamHeadReader,
    }

    impl Test {
        fn new(stream_type: StreamType, role: Role) -> Self {
            let (mut conn_c, mut conn_s) = connect();
            // create a stream
            let stream_id = conn_s.stream_create(stream_type).unwrap();
            let out = conn_s.process(None, now());
            mem::drop(conn_c.process(out.dgram(), now()));

            Self {
                conn_c,
                conn_s,
                stream_id,
                decoder: NewStreamHeadReader::new(stream_id, role),
            }
        }

        fn decode_buffer(
            &mut self,
            enc: &[u8],
            fin: bool,
            outcome: &Res<(ReceiveOutput, bool)>,
            done: bool,
        ) {
            let len = enc.len() - 1;
            for i in 0..len {
                self.conn_s
                    .stream_send(self.stream_id, &enc[i..=i])
                    .unwrap();
                let out = self.conn_s.process(None, now());
                mem::drop(self.conn_c.process(out.dgram(), now()));
                assert_eq!(
                    self.decoder.receive(&mut self.conn_c).unwrap(),
                    (ReceiveOutput::NoOutput, false)
                );
                assert!(!self.decoder.done());
            }
            self.conn_s
                .stream_send(self.stream_id, &enc[enc.len() - 1..])
                .unwrap();
            if fin {
                self.conn_s.stream_close_send(self.stream_id).unwrap();
            }
            let out = self.conn_s.process(None, now());
            mem::drop(self.conn_c.process(out.dgram(), now()));
            assert_eq!(&self.decoder.receive(&mut self.conn_c), outcome);
            assert_eq!(self.decoder.done(), done);
        }

        fn decode(
            &mut self,
            to_encode: &[u64],
            fin: bool,
            outcome: &Res<(ReceiveOutput, bool)>,
            done: bool,
        ) {
            let mut enc = Encoder::default();
            for i in to_encode {
                enc.encode_varint(*i);
            }
            self.decode_buffer(enc.as_ref(), fin, outcome, done);
        }
    }

    #[test]
    fn decode_stream_decoder() {
        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[QPACK_UNI_STREAM_TYPE_DECODER],
            false,
            &Ok((ReceiveOutput::NewStream(NewStreamType::Encoder), true)),
            true,
        );
    }

    #[test]
    fn decode_stream_encoder() {
        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[QPACK_UNI_STREAM_TYPE_ENCODER],
            false,
            &Ok((ReceiveOutput::NewStream(NewStreamType::Decoder), true)),
            true,
        );
    }

    #[test]
    fn decode_stream_control() {
        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[HTTP3_UNI_STREAM_TYPE_CONTROL],
            false,
            &Ok((ReceiveOutput::NewStream(NewStreamType::Control), true)),
            true,
        );
    }

    #[test]
    fn decode_stream_push() {
        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[HTTP3_UNI_STREAM_TYPE_PUSH, 0xaaaa_aaaa],
            false,
            &Ok((
                ReceiveOutput::NewStream(NewStreamType::Push(0xaaaa_aaaa)),
                true,
            )),
            true,
        );

        let mut t = Test::new(StreamType::UniDi, Role::Server);
        t.decode(
            &[HTTP3_UNI_STREAM_TYPE_PUSH],
            false,
            &Err(Error::HttpStreamCreation),
            true,
        );
    }

    #[test]
    fn decode_stream_unknown() {
        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[0x3fff_ffff_ffff_ffff],
            false,
            &Ok((ReceiveOutput::NewStream(NewStreamType::Unknown), true)),
            true,
        );
    }

    #[test]
    fn decode_stream_http() {
        let mut t = Test::new(StreamType::BiDi, Role::Server);
        t.decode(
            &[H3_FRAME_TYPE_HEADERS],
            false,
            &Ok((ReceiveOutput::NewStream(NewStreamType::Http), true)),
            true,
        );

        let mut t = Test::new(StreamType::UniDi, Role::Server);
        t.decode(
            &[H3_FRAME_TYPE_HEADERS], // this is the same as a HTTP3_UNI_STREAM_TYPE_PUSH which is not aallowed on the server side.
            false,
            &Err(Error::HttpStreamCreation),
            true,
        );

        let mut t = Test::new(StreamType::BiDi, Role::Client);
        t.decode(
            &[H3_FRAME_TYPE_HEADERS],
            false,
            &Err(Error::HttpStreamCreation),
            true,
        );

        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[H3_FRAME_TYPE_HEADERS, 0xaaaa_aaaa], // this is the same as a HTTP3_UNI_STREAM_TYPE_PUSH
            false,
            &Ok((
                ReceiveOutput::NewStream(NewStreamType::Push(0xaaaa_aaaa)),
                true,
            )),
            true,
        );
    }

    #[test]
    fn decode_stream_wt_bidi() {
        let mut t = Test::new(StreamType::BiDi, Role::Server);
        t.decode(
            &[WEBTRANSPORT_STREAM, 0xaaaa_aaaa],
            false,
            &Ok((
                ReceiveOutput::NewStream(NewStreamType::WebTransportStream(0xaaaa_aaaa)),
                true,
            )),
            true,
        );

        let mut t = Test::new(StreamType::UniDi, Role::Server);
        t.decode(
            &[WEBTRANSPORT_STREAM],
            false,
            &Ok((ReceiveOutput::NewStream(NewStreamType::Unknown), true)),
            true,
        );

        let mut t = Test::new(StreamType::BiDi, Role::Client);
        t.decode(
            &[WEBTRANSPORT_STREAM, 0xaaaa_aaaa],
            false,
            &Ok((
                ReceiveOutput::NewStream(NewStreamType::WebTransportStream(0xaaaa_aaaa)),
                true,
            )),
            true,
        );

        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[WEBTRANSPORT_STREAM],
            false,
            &Ok((ReceiveOutput::NewStream(NewStreamType::Unknown), true)),
            true,
        );
    }

    #[test]
    fn decode_stream_wt_unidi() {
        let mut t = Test::new(StreamType::UniDi, Role::Server);
        t.decode(
            &[WEBTRANSPORT_UNI_STREAM, 0xaaaa_aaaa],
            false,
            &Ok((
                ReceiveOutput::NewStream(NewStreamType::WebTransportStream(0xaaaa_aaaa)),
                true,
            )),
            true,
        );

        let mut t = Test::new(StreamType::BiDi, Role::Server);
        t.decode(
            &[WEBTRANSPORT_UNI_STREAM],
            false,
            &Err(Error::HttpFrame),
            true,
        );

        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[WEBTRANSPORT_UNI_STREAM, 0xaaaa_aaaa],
            false,
            &Ok((
                ReceiveOutput::NewStream(NewStreamType::WebTransportStream(0xaaaa_aaaa)),
                true,
            )),
            true,
        );

        let mut t = Test::new(StreamType::BiDi, Role::Client);
        t.decode(
            &[WEBTRANSPORT_UNI_STREAM],
            false,
            &Err(Error::HttpStreamCreation),
            true,
        );
    }

    #[test]
    fn done_decoding() {
        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[0x3fff],
            false,
            &Ok((ReceiveOutput::NewStream(NewStreamType::Unknown), true)),
            true,
        );
        // NewStreamHeadReader is done, it will not continue reading from the stream.
        t.decode(
            &[QPACK_UNI_STREAM_TYPE_DECODER],
            false,
            &Ok((ReceiveOutput::NoOutput, true)),
            true,
        );
    }

    #[test]
    fn decoding_truncate() {
        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode_buffer(&[0xff], false, &Ok((ReceiveOutput::NoOutput, false)), false);
    }

    #[test]
    fn reset() {
        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decoder.reset(CloseType::ResetRemote(0x100)).unwrap();
        // after a reset NewStreamHeadReader will not read more data.
        t.decode(
            &[QPACK_UNI_STREAM_TYPE_DECODER],
            false,
            &Ok((ReceiveOutput::NoOutput, true)),
            true,
        );
    }

    #[test]
    fn stream_fin_decoder() {
        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[QPACK_UNI_STREAM_TYPE_DECODER],
            true,
            &Err(Error::HttpClosedCriticalStream),
            true,
        );
    }

    #[test]
    fn stream_fin_encoder() {
        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[QPACK_UNI_STREAM_TYPE_ENCODER],
            true,
            &Err(Error::HttpClosedCriticalStream),
            true,
        );
    }

    #[test]
    fn stream_fin_control() {
        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[HTTP3_UNI_STREAM_TYPE_CONTROL],
            true,
            &Err(Error::HttpClosedCriticalStream),
            true,
        );
    }

    #[test]
    fn stream_fin_push() {
        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[HTTP3_UNI_STREAM_TYPE_PUSH, 0xaaaa_aaaa],
            true,
            &Err(Error::HttpGeneralProtocol),
            true,
        );

        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[HTTP3_UNI_STREAM_TYPE_PUSH],
            true,
            &Err(Error::HttpStreamCreation),
            true,
        );
    }

    #[test]
    fn stream_fin_wt() {
        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[WEBTRANSPORT_UNI_STREAM, 0xaaaa_aaaa],
            true,
            &Err(Error::HttpGeneralProtocol),
            true,
        );

        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[WEBTRANSPORT_UNI_STREAM],
            true,
            &Err(Error::HttpStreamCreation),
            true,
        );

        let mut t = Test::new(StreamType::UniDi, Role::Server);
        t.decode(
            &[WEBTRANSPORT_UNI_STREAM, 0xaaaa_aaaa],
            true,
            &Err(Error::HttpGeneralProtocol),
            true,
        );

        let mut t = Test::new(StreamType::UniDi, Role::Server);
        t.decode(
            &[WEBTRANSPORT_UNI_STREAM],
            true,
            &Err(Error::HttpStreamCreation),
            true,
        );

        let mut t = Test::new(StreamType::BiDi, Role::Client);
        t.decode(
            &[WEBTRANSPORT_STREAM, 0xaaaa_aaaa],
            true,
            &Err(Error::HttpGeneralProtocol),
            true,
        );

        let mut t = Test::new(StreamType::BiDi, Role::Client);
        t.decode(
            &[WEBTRANSPORT_STREAM],
            true,
            &Err(Error::HttpStreamCreation),
            true,
        );

        let mut t = Test::new(StreamType::BiDi, Role::Server);
        t.decode(
            &[WEBTRANSPORT_STREAM, 0xaaaa_aaaa],
            true,
            &Err(Error::HttpGeneralProtocol),
            true,
        );

        let mut t = Test::new(StreamType::BiDi, Role::Server);
        t.decode(
            &[WEBTRANSPORT_STREAM],
            true,
            &Err(Error::HttpStreamCreation),
            true,
        );
    }

    #[test]
    fn stream_fin_uknown() {
        let mut t = Test::new(StreamType::UniDi, Role::Client);
        t.decode(
            &[0x3fff_ffff_ffff_ffff],
            true,
            &Ok((ReceiveOutput::NewStream(NewStreamType::Unknown), true)),
            true,
        );

        let mut t = Test::new(StreamType::UniDi, Role::Client);
        // A stream ID of 0x3fff_ffff_ffff_ffff is encoded into [0xff; 8].
        // For this test the stream type is truncated.
        // This should cause an error.
        t.decode_buffer(&[0xff; 7], true, &Err(Error::HttpStreamCreation), true);
    }
}
