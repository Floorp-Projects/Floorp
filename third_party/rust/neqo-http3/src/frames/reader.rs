// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

use crate::{Error, RecvStream, Res};
use neqo_common::{
    hex_with_len, qtrace, Decoder, IncrementalDecoderBuffer, IncrementalDecoderIgnore,
    IncrementalDecoderUint,
};
use neqo_transport::{Connection, StreamId};
use std::convert::TryFrom;
use std::fmt::Debug;

const MAX_READ_SIZE: usize = 4096;

pub(crate) trait FrameDecoder<T> {
    fn is_known_type(frame_type: u64) -> bool;
    /// # Errors
    /// Returns `HttpFrameUnexpected` if frames is not alowed, i.e. is a `H3_RESERVED_FRAME_TYPES`.
    fn frame_type_allowed(_frame_type: u64) -> Res<()> {
        Ok(())
    }
    /// # Errors
    /// If a frame cannot be properly decoded.
    fn decode(frame_type: u64, frame_len: u64, data: Option<&[u8]>) -> Res<Option<T>>;
}

pub(crate) trait StreamReader {
    /// # Errors
    /// An error may happen while reading a stream, e.g. early close, protocol error, etc.
    /// Return an error if the stream was closed on the transport layer, but that information is not yet
    /// consumed on the  http/3 layer.
    fn read_data(&mut self, buf: &mut [u8]) -> Res<(usize, bool)>;
}

pub(crate) struct StreamReaderConnectionWrapper<'a> {
    conn: &'a mut Connection,
    stream_id: StreamId,
}

impl<'a> StreamReaderConnectionWrapper<'a> {
    pub fn new(conn: &'a mut Connection, stream_id: StreamId) -> Self {
        Self { conn, stream_id }
    }
}

impl<'a> StreamReader for StreamReaderConnectionWrapper<'a> {
    /// # Errors
    /// An error may happen while reading a stream, e.g. early close, protocol error, etc.
    fn read_data(&mut self, buf: &mut [u8]) -> Res<(usize, bool)> {
        let res = self.conn.stream_recv(self.stream_id, buf)?;
        Ok(res)
    }
}

pub(crate) struct StreamReaderRecvStreamWrapper<'a> {
    recv_stream: &'a mut Box<dyn RecvStream>,
    conn: &'a mut Connection,
}

impl<'a> StreamReaderRecvStreamWrapper<'a> {
    pub fn new(conn: &'a mut Connection, recv_stream: &'a mut Box<dyn RecvStream>) -> Self {
        Self { recv_stream, conn }
    }
}

impl<'a> StreamReader for StreamReaderRecvStreamWrapper<'a> {
    /// # Errors
    /// An error may happen while reading a stream, e.g. early close, protocol error, etc.
    fn read_data(&mut self, buf: &mut [u8]) -> Res<(usize, bool)> {
        self.recv_stream.read_data(self.conn, buf)
    }
}

#[derive(Clone, Debug)]
enum FrameReaderState {
    GetType { decoder: IncrementalDecoderUint },
    GetLength { decoder: IncrementalDecoderUint },
    GetData { decoder: IncrementalDecoderBuffer },
    UnknownFrameDischargeData { decoder: IncrementalDecoderIgnore },
}

#[allow(clippy::module_name_repetitions)]
#[derive(Debug)]
pub(crate) struct FrameReader {
    state: FrameReaderState,
    frame_type: u64,
    frame_len: u64,
}

impl Default for FrameReader {
    fn default() -> Self {
        Self::new()
    }
}

impl FrameReader {
    #[must_use]
    pub fn new() -> Self {
        Self {
            state: FrameReaderState::GetType {
                decoder: IncrementalDecoderUint::default(),
            },
            frame_type: 0,
            frame_len: 0,
        }
    }

    #[must_use]
    pub fn new_with_type(frame_type: u64) -> Self {
        Self {
            state: FrameReaderState::GetLength {
                decoder: IncrementalDecoderUint::default(),
            },
            frame_type,
            frame_len: 0,
        }
    }

    fn reset(&mut self) {
        self.state = FrameReaderState::GetType {
            decoder: IncrementalDecoderUint::default(),
        };
    }

    fn min_remaining(&self) -> usize {
        match &self.state {
            FrameReaderState::GetType { decoder } | FrameReaderState::GetLength { decoder } => {
                decoder.min_remaining()
            }
            FrameReaderState::GetData { decoder } => decoder.min_remaining(),
            FrameReaderState::UnknownFrameDischargeData { decoder } => decoder.min_remaining(),
        }
    }

    fn decoding_in_progress(&self) -> bool {
        if let FrameReaderState::GetType { decoder } = &self.state {
            decoder.decoding_in_progress()
        } else {
            true
        }
    }

    /// returns true if quic stream was closed.
    /// # Errors
    /// May return `HttpFrame` if a frame cannot be decoded.
    /// and `TransportStreamDoesNotExist` if `stream_recv` fails.
    pub fn receive<T: FrameDecoder<T>>(
        &mut self,
        stream_reader: &mut dyn StreamReader,
    ) -> Res<(Option<T>, bool)> {
        loop {
            let to_read = std::cmp::min(self.min_remaining(), MAX_READ_SIZE);
            let mut buf = vec![0; to_read];
            let (output, read, fin) = match stream_reader
                .read_data(&mut buf)
                .map_err(|e| Error::map_stream_recv_errors(&e))?
            {
                (0, f) => (None, false, f),
                (amount, f) => {
                    qtrace!("FrameReader::receive: reading {} byte, fin={}", amount, f);
                    (self.consume::<T>(Decoder::from(&buf[..amount]))?, true, f)
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
    fn consume<T: FrameDecoder<T>>(&mut self, mut input: Decoder) -> Res<Option<T>> {
        match &mut self.state {
            FrameReaderState::GetType { decoder } => {
                if let Some(v) = decoder.consume(&mut input) {
                    qtrace!("FrameReader::receive: read frame type {}", v);
                    self.frame_type_decoded::<T>(v)?;
                }
            }
            FrameReaderState::GetLength { decoder } => {
                if let Some(len) = decoder.consume(&mut input) {
                    qtrace!(
                        "FrameReader::receive: frame type {} length {}",
                        self.frame_type,
                        len
                    );
                    return self.frame_length_decoded::<T>(len);
                }
            }
            FrameReaderState::GetData { decoder } => {
                if let Some(data) = decoder.consume(&mut input) {
                    qtrace!(
                        "received frame {}: {}",
                        self.frame_type,
                        hex_with_len(&data[..])
                    );
                    return self.frame_data_decoded::<T>(&data);
                }
            }
            FrameReaderState::UnknownFrameDischargeData { decoder } => {
                if decoder.consume(&mut input) {
                    self.reset();
                }
            }
        }
        Ok(None)
    }
}

impl FrameReader {
    fn frame_type_decoded<T: FrameDecoder<T>>(&mut self, frame_type: u64) -> Res<()> {
        T::frame_type_allowed(frame_type)?;
        self.frame_type = frame_type;
        self.state = FrameReaderState::GetLength {
            decoder: IncrementalDecoderUint::default(),
        };
        Ok(())
    }

    fn frame_length_decoded<T: FrameDecoder<T>>(&mut self, len: u64) -> Res<Option<T>> {
        self.frame_len = len;
        if let Some(f) = T::decode(
            self.frame_type,
            self.frame_len,
            if len > 0 { None } else { Some(&[]) },
        )? {
            self.reset();
            return Ok(Some(f));
        } else if T::is_known_type(self.frame_type) {
            self.state = FrameReaderState::GetData {
                decoder: IncrementalDecoderBuffer::new(
                    usize::try_from(len).or(Err(Error::HttpFrame))?,
                ),
            };
        } else if self.frame_len == 0 {
            self.reset();
        } else {
            self.state = FrameReaderState::UnknownFrameDischargeData {
                decoder: IncrementalDecoderIgnore::new(
                    usize::try_from(len).or(Err(Error::HttpFrame))?,
                ),
            };
        }
        Ok(None)
    }

    fn frame_data_decoded<T: FrameDecoder<T>>(&mut self, data: &[u8]) -> Res<Option<T>> {
        let res = T::decode(self.frame_type, self.frame_len, Some(data))?;
        self.reset();
        Ok(res)
    }
}
