// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

//! Various async helpers modelled after futures-rs and tokio-io.

#[cfg(unix)]
use crate::msg::{RecvMsg, SendMsg};
use bytes::{Buf, BufMut};
use futures::{Async, Poll};
#[cfg(unix)]
use iovec::IoVec;
#[cfg(unix)]
use mio::Ready;
use std::io;
use tokio_io::{AsyncRead, AsyncWrite};

pub trait AsyncRecvMsg: AsyncRead {
    /// Pull some bytes from this source into the specified `Buf`, returning
    /// how many bytes were read.
    ///
    /// The `buf` provided will have bytes read into it and the internal cursor
    /// will be advanced if any bytes were read. Note that this method typically
    /// will not reallocate the buffer provided.
    fn recv_msg_buf<B>(&mut self, buf: &mut B, cmsg: &mut B) -> Poll<(usize, i32), io::Error>
    where
        Self: Sized,
        B: BufMut;
}

/// A trait for writable objects which operated in an async fashion.
///
/// This trait inherits from `std::io::Write` and indicates that an I/O object is
/// **nonblocking**, meaning that it will return an error instead of blocking
/// when bytes cannot currently be written, but hasn't closed. Specifically
/// this means that the `write` function for types that implement this trait
/// can have a few return values:
///
/// * `Ok(n)` means that `n` bytes of data was immediately written .
/// * `Err(e) if e.kind() == ErrorKind::WouldBlock` means that no data was
///   written from the buffer provided. The I/O object is not currently
///   writable but may become writable in the future.
/// * `Err(e)` for other errors are standard I/O errors coming from the
///   underlying object.
pub trait AsyncSendMsg: AsyncWrite {
    /// Write a `Buf` into this value, returning how many bytes were written.
    ///
    /// Note that this method will advance the `buf` provided automatically by
    /// the number of bytes written.
    fn send_msg_buf<B, C>(&mut self, buf: &mut B, cmsg: &C) -> Poll<usize, io::Error>
    where
        Self: Sized,
        B: Buf,
        C: Buf;
}

////////////////////////////////////////////////////////////////////////////////

#[cfg(unix)]
impl AsyncRecvMsg for super::AsyncMessageStream {
    fn recv_msg_buf<B>(&mut self, buf: &mut B, cmsg: &mut B) -> Poll<(usize, i32), io::Error>
    where
        B: BufMut,
    {
        if let Async::NotReady =
            <super::AsyncMessageStream>::poll_read_ready(self, Ready::readable())?
        {
            return Ok(Async::NotReady);
        }
        let r = unsafe {
            // The `IoVec` type can't have a 0-length size, so we create a bunch
            // of dummy versions on the stack with 1 length which we'll quickly
            // overwrite.
            let b1: &mut [u8] = &mut [0];
            let b2: &mut [u8] = &mut [0];
            let b3: &mut [u8] = &mut [0];
            let b4: &mut [u8] = &mut [0];
            let b5: &mut [u8] = &mut [0];
            let b6: &mut [u8] = &mut [0];
            let b7: &mut [u8] = &mut [0];
            let b8: &mut [u8] = &mut [0];
            let b9: &mut [u8] = &mut [0];
            let b10: &mut [u8] = &mut [0];
            let b11: &mut [u8] = &mut [0];
            let b12: &mut [u8] = &mut [0];
            let b13: &mut [u8] = &mut [0];
            let b14: &mut [u8] = &mut [0];
            let b15: &mut [u8] = &mut [0];
            let b16: &mut [u8] = &mut [0];
            let mut bufs: [&mut IoVec; 16] = [
                b1.into(),
                b2.into(),
                b3.into(),
                b4.into(),
                b5.into(),
                b6.into(),
                b7.into(),
                b8.into(),
                b9.into(),
                b10.into(),
                b11.into(),
                b12.into(),
                b13.into(),
                b14.into(),
                b15.into(),
                b16.into(),
            ];
            let n = buf.bytes_vec_mut(&mut bufs);
            self.recv_msg(&mut bufs[..n], cmsg.bytes_mut())
        };

        match r {
            Ok((n, cmsg_len, flags)) => {
                unsafe {
                    buf.advance_mut(n);
                }
                unsafe {
                    cmsg.advance_mut(cmsg_len);
                }
                Ok((n, flags).into())
            }
            Err(ref e) if e.kind() == io::ErrorKind::WouldBlock => {
                self.clear_read_ready(mio::Ready::readable())?;
                Ok(Async::NotReady)
            }
            Err(e) => Err(e),
        }
    }
}

#[cfg(unix)]
impl AsyncSendMsg for super::AsyncMessageStream {
    fn send_msg_buf<B, C>(&mut self, buf: &mut B, cmsg: &C) -> Poll<usize, io::Error>
    where
        B: Buf,
        C: Buf,
    {
        if let Async::NotReady = <super::AsyncMessageStream>::poll_write_ready(self)? {
            return Ok(Async::NotReady);
        }
        let r = {
            // The `IoVec` type can't have a zero-length size, so create a dummy
            // version from a 1-length slice which we'll overwrite with the
            // `bytes_vec` method.
            static DUMMY: &[u8] = &[0];
            let nom = <&IoVec>::from(DUMMY);
            let mut bufs = [
                nom, nom, nom, nom, nom, nom, nom, nom, nom, nom, nom, nom, nom, nom, nom, nom,
            ];
            let n = buf.bytes_vec(&mut bufs);
            self.send_msg(&bufs[..n], cmsg.bytes())
        };
        match r {
            Ok(n) => {
                buf.advance(n);
                Ok(Async::Ready(n))
            }
            Err(ref e) if e.kind() == io::ErrorKind::WouldBlock => {
                self.clear_write_ready()?;
                Ok(Async::NotReady)
            }
            Err(e) => Err(e),
        }
    }
}

// AsyncRecvMsg/AsyncSendMsg are implemented for convenience on Windows and don't use the _cmsg parameter.
#[cfg(windows)]
impl AsyncRecvMsg for super::AsyncMessageStream {
    fn recv_msg_buf<B>(&mut self, buf: &mut B, _cmsg: &mut B) -> Poll<(usize, i32), io::Error>
    where
        B: BufMut,
    {
        // _cmsg unused on Windows.  Pass through to read_buf.
        match <super::AsyncMessageStream>::read_buf(self, buf) {
            Ok(Async::Ready(n)) => Ok(Async::Ready((n, 0))),
            Ok(Async::NotReady) => Ok(Async::NotReady),
            Err(e) => Err(e),
        }
    }
}

#[cfg(windows)]
impl AsyncSendMsg for super::AsyncMessageStream {
    fn send_msg_buf<B, C>(&mut self, buf: &mut B, _cmsg: &C) -> Poll<usize, io::Error>
    where
        B: Buf,
        C: Buf,
    {
        // _cmsg unused on Windows.  Pass through to write_buf.
        <super::AsyncMessageStream>::write_buf(self, buf)
    }
}
