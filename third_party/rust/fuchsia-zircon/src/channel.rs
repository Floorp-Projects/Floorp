// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Type-safe bindings for Zircon channel objects.

use {AsHandleRef, HandleBased, Handle, HandleRef, INVALID_HANDLE, Peered, Status, Time, usize_into_u32, size_to_u32_sat};
use {sys, handle_drop, into_result};
use std::mem;

/// An object representing a Zircon
/// [channel](https://fuchsia.googlesource.com/zircon/+/master/docs/objects/channel.md).
///
/// As essentially a subtype of `Handle`, it can be freely interconverted.
#[derive(Debug, Eq, PartialEq)]
pub struct Channel(Handle);
impl_handle_based!(Channel);
impl Peered for Channel {}

impl Channel {
    /// Create a channel, resulting an a pair of `Channel` objects representing both
    /// sides of the channel. Messages written into one maybe read from the opposite.
    ///
    /// Wraps the
    /// [zx_channel_create](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/channel_create.md)
    /// syscall.
    pub fn create(opts: ChannelOpts) -> Result<(Channel, Channel), Status> {
        unsafe {
            let mut handle0 = 0;
            let mut handle1 = 0;
            let status = sys::zx_channel_create(opts as u32, &mut handle0, &mut handle1);
            into_result(status, ||
                (Self::from(Handle(handle0)),
                    Self::from(Handle(handle1))))
        }
    }

    /// Read a message from a channel. Wraps the
    /// [zx_channel_read](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/channel_read.md)
    /// syscall.
    ///
    /// If the `MessageBuf` lacks the capacity to hold the pending message,
    /// returns an `Err` with the number of bytes and number of handles needed.
    /// Otherwise returns an `Ok` with the result as usual.
    pub fn read_raw(&self, opts: u32, buf: &mut MessageBuf)
        -> Result<Result<(), Status>, (usize, usize)>
    {
        unsafe {
            buf.reset_handles();
            let raw_handle = self.raw_handle();
            let mut num_bytes: u32 = size_to_u32_sat(buf.bytes.capacity());
            let mut num_handles: u32 = size_to_u32_sat(buf.handles.capacity());
            let status = sys::zx_channel_read(raw_handle, opts,
                buf.bytes.as_mut_ptr(), buf.handles.as_mut_ptr(),
                num_bytes, num_handles, &mut num_bytes, &mut num_handles);
            if status == sys::ZX_ERR_BUFFER_TOO_SMALL {
                Err((num_bytes as usize, num_handles as usize))
            } else {
                Ok(into_result(status, || {
                    buf.bytes.set_len(num_bytes as usize);
                    buf.handles.set_len(num_handles as usize);
                }))
            }
        }
    }

    /// Read a message from a channel.
    ///
    /// Note that this method can cause internal reallocations in the `MessageBuf`
    /// if it is lacks capacity to hold the full message. If such reallocations
    /// are not desirable, use `read_raw` instead.
    pub fn read(&self, opts: u32, buf: &mut MessageBuf) -> Result<(), Status> {
        loop {
            match self.read_raw(opts, buf) {
                Ok(result) => return result,
                Err((num_bytes, num_handles)) => {
                    buf.ensure_capacity_bytes(num_bytes);
                    buf.ensure_capacity_handles(num_handles);
                }
            }
        }
    }

    /// Write a message to a channel. Wraps the
    /// [zx_channel_write](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/channel_write.md)
    /// syscall.
    pub fn write(&self, bytes: &[u8], handles: &mut Vec<Handle>, opts: u32)
            -> Result<(), Status>
    {
        let n_bytes = try!(usize_into_u32(bytes.len()).map_err(|_| Status::ErrOutOfRange));
        let n_handles = try!(usize_into_u32(handles.len()).map_err(|_| Status::ErrOutOfRange));
        unsafe {
            let status = sys::zx_channel_write(self.raw_handle(), opts, bytes.as_ptr(), n_bytes,
                handles.as_ptr() as *const sys::zx_handle_t, n_handles);
            into_result(status, || {
                // Handles were successfully transferred, forget them on sender side
                handles.set_len(0);
            })
        }
    }

    /// Send a message consisting of the given bytes and handles to a channel and await a reply. The
    /// bytes should start with a four byte 'txid' which is used to identify the matching reply.
    ///
    /// Wraps the
    /// [zx_channel_call](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/channel_call.md)
    /// syscall.
    ///
    /// Note that unlike [`read`][read], the caller must ensure that the MessageBuf has enough
    /// capacity for the bytes and handles which will be received, as replies which are too large
    /// are discarded.
    ///
    /// On failure returns the both the main and read status.
    ///
    /// [read]: struct.Channel.html#method.read
    pub fn call(&self, options: u32, timeout: Time, bytes: &[u8], handles: &mut Vec<Handle>,
        buf: &mut MessageBuf) -> Result<(), (Status, Status)>
    {
        let write_num_bytes = try!(usize_into_u32(bytes.len()).map_err(
            |_| (Status::ErrOutOfRange, Status::NoError)));
        let write_num_handles = try!(usize_into_u32(handles.len()).map_err(
            |_| (Status::ErrOutOfRange, Status::NoError)));
        buf.reset_handles();
        let read_num_bytes: u32 = size_to_u32_sat(buf.bytes.capacity());
        let read_num_handles: u32 = size_to_u32_sat(buf.handles.capacity());
        let args = sys::zx_channel_call_args_t {
            wr_bytes: bytes.as_ptr(),
            wr_handles: handles.as_ptr() as *const sys::zx_handle_t,
            rd_bytes: buf.bytes.as_mut_ptr(),
            rd_handles: buf.handles.as_mut_ptr(),
            wr_num_bytes: write_num_bytes,
            wr_num_handles: write_num_handles,
            rd_num_bytes: read_num_bytes,
            rd_num_handles: read_num_handles,
        };
        let mut actual_read_bytes: u32 = 0;
        let mut actual_read_handles: u32 = 0;
        let mut read_status = sys::ZX_OK;
        let status = unsafe {
            sys::zx_channel_call(self.raw_handle(), options, timeout, &args, &mut actual_read_bytes,
                &mut actual_read_handles, &mut read_status)
        };
        if status == sys::ZX_OK || status == sys::ZX_ERR_TIMED_OUT || status == sys::ZX_ERR_CALL_FAILED
        {
            // Handles were successfully transferred, even if we didn't get a response, so forget
            // them on the sender side.
            unsafe { handles.set_len(0); }
        }
        unsafe {
            buf.bytes.set_len(actual_read_bytes as usize);
            buf.handles.set_len(actual_read_handles as usize);
        }
        if status == sys::ZX_OK {
            Ok(())
        } else {
            Err((Status::from_raw(status), Status::from_raw(read_status)))
        }
    }
}

/// Options for creating a channel.
#[repr(u32)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum ChannelOpts {
    /// A normal channel.
    Normal = 0,
}

impl Default for ChannelOpts {
    fn default() -> Self {
        ChannelOpts::Normal
    }
}

/// A buffer for _receiving_ messages from a channel.
///
/// A `MessageBuf` is essentially a byte buffer and a vector of
/// handles, but move semantics for "taking" handles requires special handling.
///
/// Note that for sending messages to a channel, the caller manages the buffers,
/// using a plain byte slice and `Vec<Handle>`.
#[derive(Default)]
#[derive(Debug)]
pub struct MessageBuf {
    bytes: Vec<u8>,
    handles: Vec<sys::zx_handle_t>,
}

impl MessageBuf {
    /// Create a new, empty, message buffer.
    pub fn new() -> Self {
        Default::default()
    }

    /// Ensure that the buffer has the capacity to hold at least `n_bytes` bytes.
    pub fn ensure_capacity_bytes(&mut self, n_bytes: usize) {
        ensure_capacity(&mut self.bytes, n_bytes);
    }

    /// Ensure that the buffer has the capacity to hold at least `n_handles` handles.
    pub fn ensure_capacity_handles(&mut self, n_handles: usize) {
        ensure_capacity(&mut self.handles, n_handles);
    }

    /// Get a reference to the bytes of the message buffer, as a `&[u8]` slice.
    pub fn bytes(&self) -> &[u8] {
        self.bytes.as_slice()
    }

    /// The number of handles in the message buffer. Note this counts the number
    /// available when the message was received; `take_handle` does not affect
    /// the count.
    pub fn n_handles(&self) -> usize {
        self.handles.len()
    }

    /// Take the handle at the specified index from the message buffer. If the
    /// method is called again with the same index, it will return `None`, as
    /// will happen if the index exceeds the number of handles available.
    pub fn take_handle(&mut self, index: usize) -> Option<Handle> {
        self.handles.get_mut(index).and_then(|handleref|
            if *handleref == INVALID_HANDLE {
                None
            } else {
                Some(Handle(mem::replace(handleref, INVALID_HANDLE)))
            }
        )
    }

    fn drop_handles(&mut self) {
        for &handle in &self.handles {
            if handle != 0 {
                handle_drop(handle);
            }
        }
    }

    fn reset_handles(&mut self) {
        self.drop_handles();
        self.handles.clear();
    }
}

impl Drop for MessageBuf {
    fn drop(&mut self) {
        self.drop_handles();
    }
}

fn ensure_capacity<T>(vec: &mut Vec<T>, size: usize) {
    let len = vec.len();
    if size > len {
        vec.reserve(size - len);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use {Duration, ZX_CHANNEL_READABLE, ZX_CHANNEL_WRITABLE, ZX_RIGHT_SAME_RIGHTS, ZX_SIGNAL_LAST_HANDLE, Vmo, VmoOpts};
    use deadline_after;
    use std::thread;

    #[test]
    fn channel_basic() {
        let (p1, p2) = Channel::create(ChannelOpts::Normal).unwrap();

        let mut empty = vec![];
        assert!(p1.write(b"hello", &mut empty, 0).is_ok());

        let mut buf = MessageBuf::new();
        assert!(p2.read(0, &mut buf).is_ok());
        assert_eq!(buf.bytes(), b"hello");
    }

    #[test]
    fn channel_read_raw_too_small() {
        let (p1, p2) = Channel::create(ChannelOpts::Normal).unwrap();

        let mut empty = vec![];
        assert!(p1.write(b"hello", &mut empty, 0).is_ok());

        let mut buf = MessageBuf::new();
        let result = p2.read_raw(0, &mut buf);
        assert_eq!(result, Err((5, 0)));
        assert_eq!(buf.bytes(), b"");
    }

    #[test]
    fn channel_send_handle() {
        let hello_length: usize = 5;

        // Create a pair of channels and a virtual memory object.
        let (p1, p2) = Channel::create(ChannelOpts::Normal).unwrap();
        let vmo = Vmo::create(hello_length as u64, VmoOpts::Default).unwrap();

        // Duplicate VMO handle and send it down the channel.
        let duplicate_vmo_handle = vmo.duplicate_handle(ZX_RIGHT_SAME_RIGHTS).unwrap().into();
        let mut handles_to_send: Vec<Handle> = vec![duplicate_vmo_handle];
        assert!(p1.write(b"", &mut handles_to_send, 0).is_ok());
        // Handle should be removed from vector.
        assert!(handles_to_send.is_empty());

        // Read the handle from the receiving channel.
        let mut buf = MessageBuf::new();
        assert!(p2.read(0, &mut buf).is_ok());
        assert_eq!(buf.n_handles(), 1);
        // Take the handle from the buffer.
        let received_handle = buf.take_handle(0).unwrap();
        // Should not affect number of handles.
        assert_eq!(buf.n_handles(), 1);
        // Trying to take it again should fail.
        assert!(buf.take_handle(0).is_none());

        // Now to test that we got the right handle, try writing something to it...
        let received_vmo = Vmo::from(received_handle);
        assert_eq!(received_vmo.write(b"hello", 0).unwrap(), hello_length);

        // ... and reading it back from the original VMO.
        let mut read_vec = vec![0; hello_length];
        assert_eq!(vmo.read(&mut read_vec, 0).unwrap(), hello_length);
        assert_eq!(read_vec, b"hello");
    }

    #[test]
    fn channel_call_timeout() {
        let ten_ms: Duration = 10_000_000;

        // Create a pair of channels and a virtual memory object.
        let (p1, p2) = Channel::create(ChannelOpts::Normal).unwrap();
        let vmo = Vmo::create(0 as u64, VmoOpts::Default).unwrap();

        // Duplicate VMO handle and send it along with the call.
        let duplicate_vmo_handle = vmo.duplicate_handle(ZX_RIGHT_SAME_RIGHTS).unwrap().into();
        let mut handles_to_send: Vec<Handle> = vec![duplicate_vmo_handle];
        let mut buf = MessageBuf::new();
        assert_eq!(p1.call(0, deadline_after(ten_ms), b"call", &mut handles_to_send, &mut buf),
            Err((Status::ErrTimedOut, Status::NoError)));
        // Handle should be removed from vector even though we didn't get a response, as it was
        // still sent over the channel.
        assert!(handles_to_send.is_empty());

        // Should be able to read call even though it timed out waiting for a response.
        let mut buf = MessageBuf::new();
        assert!(p2.read(0, &mut buf).is_ok());
        assert_eq!(buf.bytes(), b"call");
        assert_eq!(buf.n_handles(), 1);
    }

    #[test]
    fn channel_call() {
        let hundred_ms: Duration = 100_000_000;

        // Create a pair of channels
        let (p1, p2) = Channel::create(ChannelOpts::Normal).unwrap();

        // Start a new thread to respond to the call.
        let server = thread::spawn(move || {
            assert_eq!(p2.wait_handle(ZX_CHANNEL_READABLE, deadline_after(hundred_ms)),
                Ok(ZX_CHANNEL_READABLE | ZX_CHANNEL_WRITABLE | ZX_SIGNAL_LAST_HANDLE));
            let mut buf = MessageBuf::new();
            assert_eq!(p2.read(0, &mut buf), Ok(()));
            assert_eq!(buf.bytes(), b"txidcall");
            assert_eq!(buf.n_handles(), 0);
            let mut empty = vec![];
            assert_eq!(p2.write(b"txidresponse", &mut empty, 0), Ok(()));
        });

        // Make the call.
        let mut empty = vec![];
        let mut buf = MessageBuf::new();
        buf.ensure_capacity_bytes(12);
        assert_eq!(p1.call(0, deadline_after(hundred_ms), b"txidcall", &mut empty, &mut buf),
            Ok(()));
        assert_eq!(buf.bytes(), b"txidresponse");
        assert_eq!(buf.n_handles(), 0);

        assert!(server.join().is_ok());
    }
}
