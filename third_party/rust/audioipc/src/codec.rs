// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

//! `Encoder`s and `Decoder`s from items to/from `BytesMut` buffers.

use bincode::{self, deserialize, serialized_size};
use bytes::{BufMut, ByteOrder, BytesMut, LittleEndian};
use serde::de::DeserializeOwned;
use serde::ser::Serialize;
use std::fmt::Debug;
use std::io;
use std::marker::PhantomData;

////////////////////////////////////////////////////////////////////////////////
// Split buffer into size delimited frames - This appears more complicated than
// might be necessary due to handling the possibility of messages being split
// across reads.

pub trait Codec {
    /// The type of items to be encoded into byte buffer
    type In;

    /// The type of items to be returned by decoding from byte buffer
    type Out;

    /// Attempts to decode a frame from the provided buffer of bytes.
    fn decode(&mut self, buf: &mut BytesMut) -> io::Result<Option<Self::Out>>;

    /// A default method available to be called when there are no more bytes
    /// available to be read from the I/O.
    fn decode_eof(&mut self, buf: &mut BytesMut) -> io::Result<Self::Out> {
        match self.decode(buf)? {
            Some(frame) => Ok(frame),
            None => Err(io::Error::new(
                io::ErrorKind::Other,
                "bytes remaining on stream",
            )),
        }
    }

    /// Encodes a frame intox the buffer provided.
    fn encode(&mut self, msg: Self::In, buf: &mut BytesMut) -> io::Result<()>;
}

/// Codec based upon bincode serialization
///
/// Messages that have been serialized using bincode are prefixed with
/// the length of the message to aid in deserialization, so that it's
/// known if enough data has been received to decode a complete
/// message.
pub struct LengthDelimitedCodec<In, Out> {
    state: State,
    __in: PhantomData<In>,
    __out: PhantomData<Out>,
}

enum State {
    Length,
    Data(usize),
}

const MAX_MESSAGE_LEN: u64 = 1024 * 1024;
const MESSAGE_LENGTH_SIZE: usize = std::mem::size_of::<u32>();
// TODO: static assert that MAX_MESSAGE_LEN can be encoded into MESSAGE_LENGTH_SIZE.

impl<In, Out> Default for LengthDelimitedCodec<In, Out> {
    fn default() -> Self {
        LengthDelimitedCodec {
            state: State::Length,
            __in: PhantomData,
            __out: PhantomData,
        }
    }
}

impl<In, Out> LengthDelimitedCodec<In, Out> {
    // Lengths are encoded as little endian u32
    fn decode_length(&mut self, buf: &mut BytesMut) -> Option<usize> {
        if buf.len() < MESSAGE_LENGTH_SIZE {
            // Not enough data
            return None;
        }

        let n = LittleEndian::read_u32(buf.as_ref());

        // Consume the length field
        let _ = buf.split_to(MESSAGE_LENGTH_SIZE);

        Some(n as usize)
    }

    fn decode_data(&mut self, buf: &mut BytesMut, n: usize) -> io::Result<Option<Out>>
    where
        Out: DeserializeOwned + Debug,
    {
        // At this point, the buffer has already had the required capacity
        // reserved. All there is to do is read.
        if buf.len() < n {
            return Ok(None);
        }

        let buf = buf.split_to(n).freeze();

        trace!("Attempting to decode");
        let msg = deserialize::<Out>(buf.as_ref()).map_err(|e| match *e {
            bincode::ErrorKind::Io(e) => e,
            _ => io::Error::new(io::ErrorKind::Other, *e),
        })?;

        trace!("... Decoded {:?}", msg);
        Ok(Some(msg))
    }
}

impl<In, Out> Codec for LengthDelimitedCodec<In, Out>
where
    In: Serialize + Debug,
    Out: DeserializeOwned + Debug,
{
    type In = In;
    type Out = Out;

    fn decode(&mut self, buf: &mut BytesMut) -> io::Result<Option<Self::Out>> {
        let n = match self.state {
            State::Length => {
                match self.decode_length(buf) {
                    Some(n) => {
                        self.state = State::Data(n);

                        // Ensure that the buffer has enough space to read the
                        // incoming payload
                        buf.reserve(n);

                        n
                    }
                    None => return Ok(None),
                }
            }
            State::Data(n) => n,
        };

        match self.decode_data(buf, n)? {
            Some(data) => {
                // Update the decode state
                self.state = State::Length;

                // Make sure the buffer has enough space to read the next head
                buf.reserve(MESSAGE_LENGTH_SIZE);

                Ok(Some(data))
            }
            None => Ok(None),
        }
    }

    fn encode(&mut self, item: Self::In, buf: &mut BytesMut) -> io::Result<()> {
        trace!("Attempting to encode");
        let encoded_len = serialized_size(&item).unwrap();
        if encoded_len > MAX_MESSAGE_LEN {
            trace!("oversized message {}", encoded_len);
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "encoded message too big",
            ));
        }

        buf.reserve((encoded_len as usize) + MESSAGE_LENGTH_SIZE);

        buf.put_u32_le(encoded_len as u32);

        #[allow(deprecated)]
        if let Err(e) = bincode::config()
            .limit(encoded_len)
            .serialize_into::<_, Self::In>(&mut buf.writer(), &item)
        {
            match *e {
                bincode::ErrorKind::Io(e) => return Err(e),
                _ => return Err(io::Error::new(io::ErrorKind::Other, *e)),
            }
        }

        Ok(())
    }
}
