// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use crate::async_msg::{AsyncRecvMsg, AsyncSendMsg};
#[cfg(unix)]
use crate::cmsg;
use crate::codec::Codec;
#[cfg(windows)]
use crate::duplicate_platform_handle;
use crate::messages::AssocRawPlatformHandle;
use bytes::{Bytes, BytesMut, IntoBuf};
use futures::{task, AsyncSink, Poll, Sink, StartSend, Stream};
use std::collections::VecDeque;
#[cfg(unix)]
use std::mem;
#[cfg(unix)]
use std::os::unix::io::RawFd;
use std::{fmt, io};

const INITIAL_CAPACITY: usize = 1024;
const BACKPRESSURE_THRESHOLD: usize = 4 * INITIAL_CAPACITY;

#[cfg(unix)]
struct IncomingFd {
    cmsg: BytesMut,
    recv_fd: Option<cmsg::ControlMsgIter>,
}

#[cfg(unix)]
impl IncomingFd {
    pub fn new() -> Self {
        IncomingFd {
            cmsg: BytesMut::with_capacity(cmsg::space(mem::size_of::<RawFd>())),
            recv_fd: None,
        }
    }

    pub fn take_fd(&mut self) -> Option<RawFd> {
        loop {
            let fd = self
                .recv_fd
                .as_mut()
                .and_then(|recv_fd| recv_fd.next())
                .map(|fd| {
                    assert_eq!(fd.len(), 1);
                    fd[0]
                });

            if fd.is_some() {
                return fd;
            }

            if self.cmsg.is_empty() {
                return None;
            }

            self.recv_fd = Some(cmsg::iterator(self.cmsg.take().freeze()));
        }
    }

    pub fn cmsg(&mut self) -> &mut BytesMut {
        self.cmsg.reserve(cmsg::space(mem::size_of::<RawFd>()));
        &mut self.cmsg
    }
}

#[derive(Debug)]
struct Frame {
    msgs: Bytes,
    handle: Option<Bytes>,
}

/// A unified `Stream` and `Sink` interface over an I/O object, using
/// the `Codec` trait to encode and decode the payload.
pub struct Framed<A, C> {
    io: A,
    codec: C,
    // Stream
    read_buf: BytesMut,
    #[cfg(unix)]
    incoming_fd: IncomingFd,
    is_readable: bool,
    eof: bool,
    // Sink
    frames: VecDeque<Frame>,
    write_buf: BytesMut,
    #[cfg(unix)]
    outgoing_fd: BytesMut,
}

impl<A, C> Framed<A, C>
where
    A: AsyncSendMsg,
{
    // If there is a buffered frame, try to write it to `A`
    fn do_write(&mut self) -> Poll<(), io::Error> {
        trace!("do_write...");
        // Create a frame from any pending message in `write_buf`.
        if !self.write_buf.is_empty() {
            self.set_frame(None);
        }

        trace!("pending frames: {:?}", self.frames);

        let mut processed = 0;

        loop {
            let n = match self.frames.front() {
                Some(frame) => {
                    trace!("sending msg {:?}, handle {:?}", frame.msgs, frame.handle);
                    let mut msgs = frame.msgs.clone().into_buf();
                    let handle = match frame.handle {
                        Some(ref handle) => handle.clone(),
                        None => Bytes::new(),
                    }
                    .into_buf();
                    try_ready!(self.io.send_msg_buf(&mut msgs, &handle))
                }
                _ => {
                    // No pending frames.
                    return Ok(().into());
                }
            };

            match self.frames.pop_front() {
                Some(mut frame) => {
                    processed += 1;

                    // Close any handle that was sent. The handle is
                    // encoded in cmsg format inside frame.handle. Use
                    // the cmsg iterator to access msg and extract
                    // raw handle to close.
                    #[cfg(unix)]
                    if let Some(cmsg) = frame.handle.take() {
                        for handle in cmsg::iterator(cmsg) {
                            assert_eq!(handle.len(), 1);
                            unsafe {
                                super::close_platform_handle(handle[0]);
                            }
                        }
                    }

                    if n != frame.msgs.len() {
                        // If only part of the message was sent then
                        // re-queue the remaining message at the head
                        // of the queue. (Don't need to resend the handle
                        // since it has been sent with the first
                        // part.)
                        drop(frame.msgs.split_to(n));
                        self.frames.push_front(frame);
                        break;
                    }
                }
                _ => panic!(),
            }
        }
        trace!("process {} frames", processed);
        trace!("pending frames: {:?}", self.frames);

        Ok(().into())
    }

    fn set_frame(&mut self, handle: Option<Bytes>) {
        if self.write_buf.is_empty() {
            assert!(handle.is_none());
            trace!("set_frame: No pending messages...");
            return;
        }

        let msgs = self.write_buf.take().freeze();
        trace!("set_frame: msgs={:?} handle={:?}", msgs, handle);

        self.frames.push_back(Frame { msgs, handle });
    }
}

impl<A, C> Stream for Framed<A, C>
where
    A: AsyncRecvMsg,
    C: Codec,
    C::Out: AssocRawPlatformHandle,
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
                    #[allow(unused_mut)]
                    let mut item = self.codec.decode_eof(&mut self.read_buf)?;
                    #[cfg(unix)]
                    item.set_owned_handle(|| self.incoming_fd.take_fd());
                    return Ok(Some(item).into());
                }

                trace!("attempting to decode a frame");

                #[allow(unused_mut)]
                if let Some(mut item) = self.codec.decode(&mut self.read_buf)? {
                    trace!("frame decoded from buffer");
                    #[cfg(unix)]
                    item.set_owned_handle(|| self.incoming_fd.take_fd());
                    return Ok(Some(item).into());
                }

                self.is_readable = false;
            }

            assert!(!self.eof);

            // XXX(kinetik): work around tokio_named_pipes assuming at least 1kB available.
            #[cfg(windows)]
            self.read_buf.reserve(INITIAL_CAPACITY);

            // Otherwise, try to read more data and try again. Make sure we've
            // got room for at least one byte to read to ensure that we don't
            // get a spurious 0 that looks like EOF
            #[cfg(unix)]
            let incoming_handle = self.incoming_fd.cmsg();
            #[cfg(windows)]
            let incoming_handle = &mut BytesMut::new();
            let (n, _) = try_ready!(self.io.recv_msg_buf(&mut self.read_buf, incoming_handle));

            if n == 0 {
                self.eof = true;
            }

            self.is_readable = true;
        }
    }
}

impl<A, C> Sink for Framed<A, C>
where
    A: AsyncSendMsg,
    C: Codec,
    C::In: AssocRawPlatformHandle + fmt::Debug,
{
    type SinkItem = C::In;
    type SinkError = io::Error;

    fn start_send(
        &mut self,
        mut item: Self::SinkItem,
    ) -> StartSend<Self::SinkItem, Self::SinkError> {
        trace!("start_send: item={:?}", item);

        // If the buffer is already over BACKPRESSURE_THRESHOLD,
        // then attempt to flush it. If after flush it's *still*
        // over BACKPRESSURE_THRESHOLD, then reject the send.
        if self.write_buf.len() > BACKPRESSURE_THRESHOLD {
            self.poll_complete()?;
            if self.write_buf.len() > BACKPRESSURE_THRESHOLD {
                return Ok(AsyncSink::NotReady(item));
            }
        }

        // Take handle ownership here for `set_frame` to keep handle alive until `do_write`,
        // otherwise handle would be closed too early (i.e. when `item` is dropped).
        let handle = item.take_handle_for_send();

        // On Windows, the handle is transferred by duplicating it into the target remote process during message send.
        #[cfg(windows)]
        if let Some((handle, target_pid)) = handle {
            let remote_handle = unsafe { duplicate_platform_handle(handle, Some(target_pid))? };
            trace!(
                "item handle: {:?} remote_handle: {:?}",
                handle,
                remote_handle
            );
            // The new handle in the remote process is indicated by updating the handle stored in the item with the expected
            // value on the remote.
            item.set_remote_handle_value(|| Some(remote_handle));
        }
        // On Unix, the handle is encoded into a cmsg buffer for out-of-band transport via sendmsg.
        #[cfg(unix)]
        if let Some((handle, _)) = handle {
            item.set_remote_handle_value(|| Some(handle));
        }

        self.codec.encode(item, &mut self.write_buf)?;

        if handle.is_some() {
            // Handle ownership is transferred into the cmsg buffer here; the local handle is closed
            // after sending in `do_write`.
            #[cfg(unix)]
            let handle = handle.and_then(|handle| {
                cmsg::builder(&mut self.outgoing_fd)
                    .rights(&[handle.0])
                    .finish()
                    .ok()
            });

            // No cmsg buffer on Windows, but we still want to trigger `set_frame`, so just pass `None`.
            #[cfg(windows)]
            let handle = None;

            // Enforce splitting sends on messages that contain handles.
            self.set_frame(handle);
        }

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
        #[cfg(unix)]
        incoming_fd: IncomingFd::new(),
        is_readable: false,
        eof: false,
        frames: VecDeque::new(),
        write_buf: BytesMut::with_capacity(INITIAL_CAPACITY),
        #[cfg(unix)]
        outgoing_fd: BytesMut::with_capacity(cmsg::space(mem::size_of::<RawFd>())),
    }
}

#[cfg(all(test, unix))]
mod tests {
    use bytes::BufMut;

    extern "C" {
        fn cmsghdr_bytes(size: *mut libc::size_t) -> *const u8;
    }

    fn cmsg_bytes() -> &'static [u8] {
        let mut size = 0;
        unsafe {
            let ptr = cmsghdr_bytes(&mut size);
            std::slice::from_raw_parts(ptr, size)
        }
    }

    #[test]
    fn single_cmsg() {
        let mut incoming = super::IncomingFd::new();

        incoming.cmsg().put_slice(cmsg_bytes());
        assert!(incoming.take_fd().is_some());
        assert!(incoming.take_fd().is_none());
    }

    #[test]
    fn multiple_cmsg_1() {
        let mut incoming = super::IncomingFd::new();

        incoming.cmsg().put_slice(cmsg_bytes());
        assert!(incoming.take_fd().is_some());
        incoming.cmsg().put_slice(cmsg_bytes());
        assert!(incoming.take_fd().is_some());
        assert!(incoming.take_fd().is_none());
    }

    #[test]
    fn multiple_cmsg_2() {
        let mut incoming = super::IncomingFd::new();

        incoming.cmsg().put_slice(cmsg_bytes());
        incoming.cmsg().put_slice(cmsg_bytes());
        assert!(incoming.take_fd().is_some());
        incoming.cmsg().put_slice(cmsg_bytes());
        assert!(incoming.take_fd().is_some());
        assert!(incoming.take_fd().is_some());
        assert!(incoming.take_fd().is_none());
    }
}
