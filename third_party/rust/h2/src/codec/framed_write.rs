use codec::UserError;
use codec::UserError::*;
use frame::{self, Frame, FrameSize};
use hpack;

use bytes::{Buf, BufMut, BytesMut};
use futures::*;
use tokio_io::{AsyncRead, AsyncWrite};

use std::io::{self, Cursor};

#[derive(Debug)]
pub struct FramedWrite<T, B> {
    /// Upstream `AsyncWrite`
    inner: T,

    /// HPACK encoder
    hpack: hpack::Encoder,

    /// Write buffer
    ///
    /// TODO: Should this be a ring buffer?
    buf: Cursor<BytesMut>,

    /// Next frame to encode
    next: Option<Next<B>>,

    /// Last data frame
    last_data_frame: Option<frame::Data<B>>,

    /// Max frame size, this is specified by the peer
    max_frame_size: FrameSize,
}

#[derive(Debug)]
enum Next<B> {
    Data(frame::Data<B>),
    Continuation(frame::Continuation),
}

/// Initialze the connection with this amount of write buffer.
const DEFAULT_BUFFER_CAPACITY: usize = 4 * 1_024;

/// Min buffer required to attempt to write a frame
const MIN_BUFFER_CAPACITY: usize = frame::HEADER_LEN + CHAIN_THRESHOLD;

/// Chain payloads bigger than this. The remote will never advertise a max frame
/// size less than this (well, the spec says the max frame size can't be less
/// than 16kb, so not even close).
const CHAIN_THRESHOLD: usize = 256;

// TODO: Make generic
impl<T, B> FramedWrite<T, B>
where
    T: AsyncWrite,
    B: Buf,
{
    pub fn new(inner: T) -> FramedWrite<T, B> {
        FramedWrite {
            inner: inner,
            hpack: hpack::Encoder::default(),
            buf: Cursor::new(BytesMut::with_capacity(DEFAULT_BUFFER_CAPACITY)),
            next: None,
            last_data_frame: None,
            max_frame_size: frame::DEFAULT_MAX_FRAME_SIZE,
        }
    }

    /// Returns `Ready` when `send` is able to accept a frame
    ///
    /// Calling this function may result in the current contents of the buffer
    /// to be flushed to `T`.
    pub fn poll_ready(&mut self) -> Poll<(), io::Error> {
        if !self.has_capacity() {
            // Try flushing
            self.flush()?;

            if !self.has_capacity() {
                return Ok(Async::NotReady);
            }
        }

        Ok(Async::Ready(()))
    }

    /// Buffer a frame.
    ///
    /// `poll_ready` must be called first to ensure that a frame may be
    /// accepted.
    pub fn buffer(&mut self, item: Frame<B>) -> Result<(), UserError> {
        // Ensure that we have enough capacity to accept the write.
        assert!(self.has_capacity());

        debug!("send; frame={:?}", item);

        match item {
            Frame::Data(mut v) => {
                // Ensure that the payload is not greater than the max frame.
                let len = v.payload().remaining();

                if len > self.max_frame_size() {
                    return Err(PayloadTooBig);
                }

                if len >= CHAIN_THRESHOLD {
                    let head = v.head();

                    // Encode the frame head to the buffer
                    head.encode(len, self.buf.get_mut());

                    // Save the data frame
                    self.next = Some(Next::Data(v));
                } else {
                    v.encode_chunk(self.buf.get_mut());

                    // The chunk has been fully encoded, so there is no need to
                    // keep it around
                    assert_eq!(v.payload().remaining(), 0, "chunk not fully encoded");

                    // Save off the last frame...
                    self.last_data_frame = Some(v);
                }
            },
            Frame::Headers(v) => {
                if let Some(continuation) = v.encode(&mut self.hpack, self.buf.get_mut()) {
                    self.next = Some(Next::Continuation(continuation));
                }
            },
            Frame::PushPromise(v) => {
                if let Some(continuation) = v.encode(&mut self.hpack, self.buf.get_mut()) {
                    self.next = Some(Next::Continuation(continuation));
                }
            },
            Frame::Settings(v) => {
                v.encode(self.buf.get_mut());
                trace!("encoded settings; rem={:?}", self.buf.remaining());
            },
            Frame::GoAway(v) => {
                v.encode(self.buf.get_mut());
                trace!("encoded go_away; rem={:?}", self.buf.remaining());
            },
            Frame::Ping(v) => {
                v.encode(self.buf.get_mut());
                trace!("encoded ping; rem={:?}", self.buf.remaining());
            },
            Frame::WindowUpdate(v) => {
                v.encode(self.buf.get_mut());
                trace!("encoded window_update; rem={:?}", self.buf.remaining());
            },

            Frame::Priority(_) => {
                /*
                v.encode(self.buf.get_mut());
                trace!("encoded priority; rem={:?}", self.buf.remaining());
                */
                unimplemented!();
            },
            Frame::Reset(v) => {
                v.encode(self.buf.get_mut());
                trace!("encoded reset; rem={:?}", self.buf.remaining());
            },
        }

        Ok(())
    }

    /// Flush buffered data to the wire
    pub fn flush(&mut self) -> Poll<(), io::Error> {
        trace!("flush");

        loop {
            while !self.is_empty() {
                match self.next {
                    Some(Next::Data(ref mut frame)) => {
                        trace!("  -> queued data frame");
                        let mut buf = Buf::by_ref(&mut self.buf).chain(frame.payload_mut());
                        try_ready!(self.inner.write_buf(&mut buf));
                    },
                    _ => {
                        trace!("  -> not a queued data frame");
                        try_ready!(self.inner.write_buf(&mut self.buf));
                    },
                }
            }

            // Clear internal buffer
            self.buf.set_position(0);
            self.buf.get_mut().clear();

            // The data frame has been written, so unset it
            match self.next.take() {
                Some(Next::Data(frame)) => {
                    self.last_data_frame = Some(frame);
                    debug_assert!(self.is_empty());
                    break;
                },
                Some(Next::Continuation(frame)) => {
                    // Buffer the continuation frame, then try to write again
                    if let Some(continuation) = frame.encode(&mut self.hpack, self.buf.get_mut()) {
                        self.next = Some(Next::Continuation(continuation));
                    }
                },
                None => {
                    break;
                }
            }
        }

        trace!("flushing buffer");
        // Flush the upstream
        try_nb!(self.inner.flush());

        Ok(Async::Ready(()))
    }

    /// Close the codec
    pub fn shutdown(&mut self) -> Poll<(), io::Error> {
        try_ready!(self.flush());
        self.inner.shutdown().map_err(Into::into)
    }

    fn has_capacity(&self) -> bool {
        self.next.is_none() && self.buf.get_ref().remaining_mut() >= MIN_BUFFER_CAPACITY
    }

    fn is_empty(&self) -> bool {
        match self.next {
            Some(Next::Data(ref frame)) => !frame.payload().has_remaining(),
            _ => !self.buf.has_remaining(),
        }
    }
}

impl<T, B> FramedWrite<T, B> {
    /// Returns the max frame size that can be sent
    pub fn max_frame_size(&self) -> usize {
        self.max_frame_size as usize
    }

    /// Set the peer's max frame size.
    pub fn set_max_frame_size(&mut self, val: usize) {
        assert!(val <= frame::MAX_MAX_FRAME_SIZE as usize);
        self.max_frame_size = val as FrameSize;
    }

    /// Retrieve the last data frame that has been sent
    pub fn take_last_data_frame(&mut self) -> Option<frame::Data<B>> {
        self.last_data_frame.take()
    }

    pub fn get_mut(&mut self) -> &mut T {
        &mut self.inner
    }
}

impl<T: io::Read, B> io::Read for FramedWrite<T, B> {
    fn read(&mut self, dst: &mut [u8]) -> io::Result<usize> {
        self.inner.read(dst)
    }
}

impl<T: AsyncRead, B> AsyncRead for FramedWrite<T, B> {
    fn read_buf<B2: BufMut>(&mut self, buf: &mut B2) -> Poll<usize, io::Error>
    where
        Self: Sized,
    {
        self.inner.read_buf(buf)
    }

    unsafe fn prepare_uninitialized_buffer(&self, buf: &mut [u8]) -> bool {
        self.inner.prepare_uninitialized_buffer(buf)
    }
}

#[cfg(feature = "unstable")]
mod unstable {
    use super::*;

    impl<T, B> FramedWrite<T, B> {
        pub fn get_ref(&self) -> &T {
            &self.inner
        }
    }
}
