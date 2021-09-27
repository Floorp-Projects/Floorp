// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use crate::codec::Codec;
use bytes::{Buf, Bytes, BytesMut, IntoBuf};
use futures::{task, AsyncSink, Poll, Sink, StartSend, Stream};
use std::io;
use tokio_io::{AsyncRead, AsyncWrite};

const INITIAL_CAPACITY: usize = 1024;
const BACKPRESSURE_THRESHOLD: usize = 4 * INITIAL_CAPACITY;

/// A unified `Stream` and `Sink` interface over an I/O object, using
/// the `Codec` trait to encode and decode the payload.
pub struct Framed<A, C> {
    io: A,
    codec: C,
    read_buf: BytesMut,
    write_buf: BytesMut,
    frame: Option<<Bytes as IntoBuf>::Buf>,
    is_readable: bool,
    eof: bool,
}

impl<A, C> Framed<A, C>
where
    A: AsyncWrite,
{
    // If there is a buffered frame, try to write it to `A`
    fn do_write(&mut self) -> Poll<(), io::Error> {
        loop {
            if self.frame.is_none() {
                self.set_frame();
            }

            if self.frame.is_none() {
                return Ok(().into());
            }

            let done = {
                let frame = self.frame.as_mut().unwrap();
                try_ready!(self.io.write_buf(frame));
                !frame.has_remaining()
            };

            if done {
                self.frame = None;
            }
        }
    }

    fn set_frame(&mut self) {
        if self.write_buf.is_empty() {
            return;
        }

        debug_assert!(self.frame.is_none());

        self.frame = Some(self.write_buf.take().freeze().into_buf());
    }
}

impl<A, C> Stream for Framed<A, C>
where
    A: AsyncRead,
    C: Codec,
{
    type Item = C::Out;
    type Error = io::Error;

    fn poll(&mut self) -> Poll<Option<Self::Item>, Self::Error> {
        loop {
            // Repeatedly call `decode` or `decode_eof` as long as it is
            // "readable". Readable is defined as not having returned `None`. If
            // the upstream has returned EOF, and the decoder is no longer
            // readable, it can be assumed that the decoder will never become
            // readable again, at which point the stream is terminated.
            if self.is_readable {
                if self.eof {
                    let frame = self.codec.decode_eof(&mut self.read_buf)?;
                    return Ok(Some(frame).into());
                }

                trace!("attempting to decode a frame");

                if let Some(frame) = self.codec.decode(&mut self.read_buf)? {
                    trace!("frame decoded from buffer");
                    return Ok(Some(frame).into());
                }

                self.is_readable = false;
            }

            assert!(!self.eof);

            // XXX(kinetik): work around tokio_named_pipes assuming at least 1kB available.
            self.read_buf.reserve(INITIAL_CAPACITY);

            // Otherwise, try to read more data and try again. Make sure we've
            // got room for at least one byte to read to ensure that we don't
            // get a spurious 0 that looks like EOF
            if try_ready!(self.io.read_buf(&mut self.read_buf)) == 0 {
                self.eof = true;
            }

            self.is_readable = true;
        }
    }
}

impl<A, C> Sink for Framed<A, C>
where
    A: AsyncWrite,
    C: Codec,
{
    type SinkItem = C::In;
    type SinkError = io::Error;

    fn start_send(&mut self, item: Self::SinkItem) -> StartSend<Self::SinkItem, Self::SinkError> {
        // If the buffer is already over BACKPRESSURE_THRESHOLD,
        // then attempt to flush it. If after flush it's *still*
        // over BACKPRESSURE_THRESHOLD, then reject the send.
        if self.write_buf.len() > BACKPRESSURE_THRESHOLD {
            self.poll_complete()?;
            if self.write_buf.len() > BACKPRESSURE_THRESHOLD {
                return Ok(AsyncSink::NotReady(item));
            }
        }

        self.codec.encode(item, &mut self.write_buf)?;
        Ok(AsyncSink::Ready)
    }

    fn poll_complete(&mut self) -> Poll<(), Self::SinkError> {
        trace!("flushing framed transport");

        try_ready!(self.do_write());

        try_nb!(self.io.flush());

        trace!("framed transport flushed");
        Ok(().into())
    }

    fn close(&mut self) -> Poll<(), Self::SinkError> {
        if task::is_in_task() {
            try_ready!(self.poll_complete());
        }
        self.io.shutdown()
    }
}

pub fn framed<A, C>(io: A, codec: C) -> Framed<A, C> {
    Framed {
        io,
        codec,
        read_buf: BytesMut::with_capacity(INITIAL_CAPACITY),
        write_buf: BytesMut::with_capacity(INITIAL_CAPACITY),
        frame: None,
        is_readable: false,
        eof: false,
    }
}
