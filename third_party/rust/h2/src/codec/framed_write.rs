use crate::codec::UserError;
use crate::codec::UserError::*;
use crate::frame::{self, Frame, FrameSize};
use crate::hpack;

use bytes::{
    buf::{BufExt, BufMutExt},
    Buf, BufMut, BytesMut,
};
use std::pin::Pin;
use std::task::{Context, Poll};
use tokio::io::{AsyncRead, AsyncWrite};

use std::io::{self, Cursor};

// A macro to get around a method needing to borrow &mut self
macro_rules! limited_write_buf {
    ($self:expr) => {{
        let limit = $self.max_frame_size() + frame::HEADER_LEN;
        $self.buf.get_mut().limit(limit)
    }};
}

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
///
/// The minimum MAX_FRAME_SIZE is 16kb, so always be able to send a HEADERS
/// frame that big.
const DEFAULT_BUFFER_CAPACITY: usize = 16 * 1_024;

/// Min buffer required to attempt to write a frame
const MIN_BUFFER_CAPACITY: usize = frame::HEADER_LEN + CHAIN_THRESHOLD;

/// Chain payloads bigger than this. The remote will never advertise a max frame
/// size less than this (well, the spec says the max frame size can't be less
/// than 16kb, so not even close).
const CHAIN_THRESHOLD: usize = 256;

// TODO: Make generic
impl<T, B> FramedWrite<T, B>
where
    T: AsyncWrite + Unpin,
    B: Buf,
{
    pub fn new(inner: T) -> FramedWrite<T, B> {
        FramedWrite {
            inner,
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
    pub fn poll_ready(&mut self, cx: &mut Context) -> Poll<io::Result<()>> {
        if !self.has_capacity() {
            // Try flushing
            ready!(self.flush(cx))?;

            if !self.has_capacity() {
                return Poll::Pending;
            }
        }

        Poll::Ready(Ok(()))
    }

    /// Buffer a frame.
    ///
    /// `poll_ready` must be called first to ensure that a frame may be
    /// accepted.
    pub fn buffer(&mut self, item: Frame<B>) -> Result<(), UserError> {
        // Ensure that we have enough capacity to accept the write.
        assert!(self.has_capacity());

        log::debug!("send; frame={:?}", item);

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
            }
            Frame::Headers(v) => {
                let mut buf = limited_write_buf!(self);
                if let Some(continuation) = v.encode(&mut self.hpack, &mut buf) {
                    self.next = Some(Next::Continuation(continuation));
                }
            }
            Frame::PushPromise(v) => {
                let mut buf = limited_write_buf!(self);
                if let Some(continuation) = v.encode(&mut self.hpack, &mut buf) {
                    self.next = Some(Next::Continuation(continuation));
                }
            }
            Frame::Settings(v) => {
                v.encode(self.buf.get_mut());
                log::trace!("encoded settings; rem={:?}", self.buf.remaining());
            }
            Frame::GoAway(v) => {
                v.encode(self.buf.get_mut());
                log::trace!("encoded go_away; rem={:?}", self.buf.remaining());
            }
            Frame::Ping(v) => {
                v.encode(self.buf.get_mut());
                log::trace!("encoded ping; rem={:?}", self.buf.remaining());
            }
            Frame::WindowUpdate(v) => {
                v.encode(self.buf.get_mut());
                log::trace!("encoded window_update; rem={:?}", self.buf.remaining());
            }

            Frame::Priority(_) => {
                /*
                v.encode(self.buf.get_mut());
                log::trace!("encoded priority; rem={:?}", self.buf.remaining());
                */
                unimplemented!();
            }
            Frame::Reset(v) => {
                v.encode(self.buf.get_mut());
                log::trace!("encoded reset; rem={:?}", self.buf.remaining());
            }
        }

        Ok(())
    }

    /// Flush buffered data to the wire
    pub fn flush(&mut self, cx: &mut Context) -> Poll<io::Result<()>> {
        log::trace!("flush");

        loop {
            while !self.is_empty() {
                match self.next {
                    Some(Next::Data(ref mut frame)) => {
                        log::trace!("  -> queued data frame");
                        let mut buf = (&mut self.buf).chain(frame.payload_mut());
                        ready!(Pin::new(&mut self.inner).poll_write_buf(cx, &mut buf))?;
                    }
                    _ => {
                        log::trace!("  -> not a queued data frame");
                        ready!(Pin::new(&mut self.inner).poll_write_buf(cx, &mut self.buf))?;
                    }
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
                }
                Some(Next::Continuation(frame)) => {
                    // Buffer the continuation frame, then try to write again
                    let mut buf = limited_write_buf!(self);
                    if let Some(continuation) = frame.encode(&mut self.hpack, &mut buf) {
                        // We previously had a CONTINUATION, and after encoding
                        // it, we got *another* one? Let's just double check
                        // that at least some progress is being made...
                        if self.buf.get_ref().len() == frame::HEADER_LEN {
                            // If *only* the CONTINUATION frame header was
                            // written, and *no* header fields, we're stuck
                            // in a loop...
                            panic!("CONTINUATION frame write loop; header value too big to encode");
                        }

                        self.next = Some(Next::Continuation(continuation));
                    }
                }
                None => {
                    break;
                }
            }
        }

        log::trace!("flushing buffer");
        // Flush the upstream
        ready!(Pin::new(&mut self.inner).poll_flush(cx))?;

        Poll::Ready(Ok(()))
    }

    /// Close the codec
    pub fn shutdown(&mut self, cx: &mut Context) -> Poll<io::Result<()>> {
        ready!(self.flush(cx))?;
        Pin::new(&mut self.inner).poll_shutdown(cx)
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

impl<T: AsyncRead + Unpin, B> AsyncRead for FramedWrite<T, B> {
    unsafe fn prepare_uninitialized_buffer(&self, buf: &mut [std::mem::MaybeUninit<u8>]) -> bool {
        self.inner.prepare_uninitialized_buffer(buf)
    }

    fn poll_read(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        buf: &mut [u8],
    ) -> Poll<io::Result<usize>> {
        Pin::new(&mut self.inner).poll_read(cx, buf)
    }

    fn poll_read_buf<Buf: BufMut>(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        buf: &mut Buf,
    ) -> Poll<io::Result<usize>> {
        Pin::new(&mut self.inner).poll_read_buf(cx, buf)
    }
}

// We never project the Pin to `B`.
impl<T: Unpin, B> Unpin for FramedWrite<T, B> {}

#[cfg(feature = "unstable")]
mod unstable {
    use super::*;

    impl<T, B> FramedWrite<T, B> {
        pub fn get_ref(&self) -> &T {
            &self.inner
        }
    }
}
