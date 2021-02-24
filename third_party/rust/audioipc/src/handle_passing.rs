// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use crate::codec::Codec;
use crate::messages::AssocRawPlatformHandle;
use bytes::{Bytes, BytesMut, IntoBuf};
use futures::{task, AsyncSink, Poll, Sink, StartSend, Stream};
use std::collections::VecDeque;
use std::{fmt, io};
use tokio_io::{AsyncRead, AsyncWrite};

const INITIAL_CAPACITY: usize = 1024;
const BACKPRESSURE_THRESHOLD: usize = 4 * INITIAL_CAPACITY;

#[derive(Debug)]
struct Frame {
    msgs: Bytes,
}

/// A unified `Stream` and `Sink` interface over an I/O object, using
/// the `Codec` trait to encode and decode the payload.
pub struct FramedWithPlatformHandles<A, C> {
    io: A,
    codec: C,
    // Stream
    read_buf: BytesMut,
    is_readable: bool,
    eof: bool,
    // Sink
    frames: VecDeque<Frame>,
    write_buf: BytesMut,
}

impl<A, C> FramedWithPlatformHandles<A, C>
where
    A: AsyncWrite,
{
    // If there is a buffered frame, try to write it to `A`
    fn do_write(&mut self) -> Poll<(), io::Error> {
        trace!("do_write...");
        // Create a frame from any pending message in `write_buf`.
        if !self.write_buf.is_empty() {
            self.set_frame();
        }

        trace!("pending frames: {:?}", self.frames);

        let mut processed = 0;

        loop {
            let n = match self.frames.front() {
                Some(frame) => {
                    trace!("sending msg {:?}", frame.msgs);
                    let mut msgs = frame.msgs.clone().into_buf();
                    try_ready!(self.io.write_buf(&mut msgs))
                }
                _ => {
                    // No pending frames.
                    return Ok(().into());
                }
            };

            match self.frames.pop_front() {
                Some(mut frame) => {
                    processed += 1;

                    if n != frame.msgs.len() {
                        // If only part of the message was sent then
                        // re-queue the remaining message at the head
                        // of the queue. (Don't need to resend the
                        // handles since they've been sent with the
                        // first part.)
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

    fn set_frame(&mut self) {
        if self.write_buf.is_empty() {
            trace!("set_frame: No pending messages...");
            return;
        }

        let msgs = self.write_buf.take().freeze();
        trace!("set_frame: msgs={:?}", msgs);

        self.frames.push_back(Frame { msgs });
    }
}

impl<A, C> Stream for FramedWithPlatformHandles<A, C>
where
    A: AsyncRead,
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
                    let item = self.codec.decode_eof(&mut self.read_buf)?;
                    return Ok(Some(item).into());
                }

                trace!("attempting to decode a frame");

                if let Some(item) = self.codec.decode(&mut self.read_buf)? {
                    trace!("frame decoded from buffer");
                    return Ok(Some(item).into());
                }

                self.is_readable = false;
            }

            assert!(!self.eof);

            // XXX(kinetik): work around tokio_named_pipes assuming at least 1kB available.
            self.read_buf.reserve(INITIAL_CAPACITY);

            // Otherwise, try to read more data and try again. Make sure we've
            // got room for at least one byte to read to ensure that we don't
            // get a spurious 0 that looks like EOF
            let n = try_ready!(self.io.read_buf(&mut self.read_buf));

            if n == 0 {
                self.eof = true;
            }

            self.is_readable = true;
        }
    }
}

impl<A, C> Sink for FramedWithPlatformHandles<A, C>
where
    A: AsyncWrite,
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

        // Take handle ownership here.
        let handles = item.platform_handles();
        if let Some((handles, target_pid)) = handles {
            // TODO: This could leak target handles if a duplicate fails - make this more robust.
            let remote_handles = unsafe {
                // Attempt to duplicate all 3 handles before checking
                // result, since we rely on duplicate_platformhandle closing
                // our source handles.
                let r1 = duplicate_platformhandle(handles[0], Some(target_pid), true);
                let r2 = duplicate_platformhandle(handles[1], Some(target_pid), true);
                let r3 = duplicate_platformhandle(handles[2], Some(target_pid), true);
                [r1?, r2?, r3?]
            };
            trace!(
                "item handles: {:?} remote_handles: {:?}",
                handles,
                remote_handles
            );
            item.take_platform_handles(|| Some(remote_handles));
        }

        self.codec.encode(item, &mut self.write_buf)?;

        if handles.is_some() {
            // Enforce splitting sends on messages that contain file
            // descriptors.
            self.set_frame();
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

pub fn framed_with_platformhandles<A, C>(io: A, codec: C) -> FramedWithPlatformHandles<A, C> {
    FramedWithPlatformHandles {
        io,
        codec,
        read_buf: BytesMut::with_capacity(INITIAL_CAPACITY),
        is_readable: false,
        eof: false,
        frames: VecDeque::new(),
        write_buf: BytesMut::with_capacity(INITIAL_CAPACITY),
    }
}

use super::PlatformHandleType;
use winapi::shared::minwindef::{DWORD, FALSE};
use winapi::um::{handleapi, processthreadsapi, winnt};

pub(crate) unsafe fn duplicate_platformhandle(
    source_handle: PlatformHandleType,
    target_pid: Option<DWORD>,
    close_source: bool,
) -> Result<PlatformHandleType, std::io::Error> {
    let source = processthreadsapi::GetCurrentProcess();
    let target = if let Some(pid) = target_pid {
        let target = processthreadsapi::OpenProcess(winnt::PROCESS_DUP_HANDLE, FALSE, pid);
        if !super::valid_handle(target) {
            return Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "invalid target process",
            ));
        }
        target
    } else {
        source
    };

    let mut target_handle = std::ptr::null_mut();
    let mut options = winnt::DUPLICATE_SAME_ACCESS;
    if close_source {
        options |= winnt::DUPLICATE_CLOSE_SOURCE;
    }
    let ok = handleapi::DuplicateHandle(
        source,
        source_handle,
        target,
        &mut target_handle,
        0,
        FALSE,
        options,
    );
    handleapi::CloseHandle(target);
    if ok == FALSE {
        return Err(std::io::Error::new(
            std::io::ErrorKind::Other,
            "DuplicateHandle failed",
        ));
    }
    Ok(target_handle)
}
