// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

//! `Encoder`s and `Decoder`s from items to/from `BytesMut` buffers.

use bincode::{self, Options};
use byteorder::{ByteOrder, LittleEndian};
use bytes::{Buf, BufMut, BytesMut};
use serde::de::DeserializeOwned;
use serde::ser::Serialize;
use std::convert::TryInto;
use std::fmt::Debug;
use std::io;
use std::marker::PhantomData;
use std::mem::size_of;

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

    /// Encodes a frame into the buffer provided.
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
    encode_buf: Vec<u8>,
    __in: PhantomData<In>,
    __out: PhantomData<Out>,
}

enum State {
    Length,
    Data(u32),
}

const MAX_MESSAGE_LEN: u32 = 1024 * 1024;
const MAGIC: u64 = 0xa4d1_019c_c910_1d4a;
const HEADER_LEN: usize = size_of::<u32>() + size_of::<u64>();

impl<In, Out> Default for LengthDelimitedCodec<In, Out> {
    fn default() -> Self {
        Self {
            state: State::Length,
            encode_buf: Vec::with_capacity(crate::ipccore::IPC_CLIENT_BUFFER_SIZE),
            __in: PhantomData,
            __out: PhantomData,
        }
    }
}

impl<In, Out> LengthDelimitedCodec<In, Out> {
    // Lengths are encoded as little endian u32
    fn decode_length(buf: &mut BytesMut) -> Option<u32> {
        if buf.len() < HEADER_LEN {
            // Not enough data
            return None;
        }

        let magic = LittleEndian::read_u64(&buf[0..8]);
        assert_eq!(magic, MAGIC);

        // Consume the length field
        let n = LittleEndian::read_u32(&buf[8..12]);
        buf.advance(HEADER_LEN);
        Some(n)
    }

    fn decode_data(buf: &mut BytesMut, n: u32) -> io::Result<Option<Out>>
    where
        Out: DeserializeOwned + Debug,
    {
        let n = n.try_into().unwrap();

        // At this point, the buffer has already had the required capacity
        // reserved. All there is to do is read.
        if buf.len() < n {
            return Ok(None);
        }

        trace!("Attempting to decode");
        let msg = bincode::options()
            .with_limit(MAX_MESSAGE_LEN as u64)
            .deserialize::<Out>(&buf[..n])
            .map_err(|e| match *e {
                bincode::ErrorKind::Io(e) => e,
                _ => io::Error::new(io::ErrorKind::Other, *e),
            })?;
        buf.advance(n);

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
                match Self::decode_length(buf) {
                    Some(n) => {
                        assert!(
                            n <= MAX_MESSAGE_LEN,
                            "assertion failed: {} <= {}",
                            n,
                            MAX_MESSAGE_LEN
                        );
                        self.state = State::Data(n);

                        // Ensure that the buffer has enough space to read the
                        // incoming payload
                        buf.reserve(n.try_into().unwrap());

                        n
                    }
                    None => return Ok(None),
                }
            }
            State::Data(n) => n,
        };

        match Self::decode_data(buf, n)? {
            Some(data) => {
                // Update the decode state
                self.state = State::Length;

                // Make sure the buffer has enough space to read the next length header.
                buf.reserve(HEADER_LEN);

                Ok(Some(data))
            }
            None => Ok(None),
        }
    }

    fn encode(&mut self, item: Self::In, buf: &mut BytesMut) -> io::Result<()> {
        trace!("Attempting to encode");

        self.encode_buf.clear();
        if let Err(e) = bincode::options()
            .with_limit(MAX_MESSAGE_LEN as u64)
            .serialize_into::<_, Self::In>(&mut self.encode_buf, &item)
        {
            trace!("message encode failed: {:?}", *e);
            match *e {
                bincode::ErrorKind::Io(e) => return Err(e),
                _ => return Err(io::Error::new(io::ErrorKind::Other, *e)),
            }
        }

        let encoded_len = self.encode_buf.len();
        assert!(encoded_len <= MAX_MESSAGE_LEN as usize);
        buf.reserve(encoded_len + HEADER_LEN);
        buf.put_u64_le(MAGIC);
        buf.put_u32_le(encoded_len.try_into().unwrap());
        buf.extend_from_slice(&self.encode_buf);

        Ok(())
    }
}
