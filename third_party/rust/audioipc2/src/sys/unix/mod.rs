// Copyright © 2021 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use std::io::Result;
use std::os::unix::prelude::{AsRawFd, FromRawFd, RawFd};

use bytes::{BufMut, BytesMut};
use iovec::IoVec;
use mio::net::UnixStream;

use crate::{close_platform_handle, PlatformHandle};

use super::{RecvMsg, SendMsg};

pub mod cmsg;
mod msg;

pub struct Pipe(pub UnixStream);

// Create a connected "pipe" pair.  The `Pipe` is the server end,
// the `PlatformHandle` is the client end to be remoted.
pub fn make_pipe_pair() -> Result<(Pipe, PlatformHandle)> {
    let (server, client) = UnixStream::pair()?;
    Ok((Pipe(server), PlatformHandle::from(client)))
}

impl Pipe {
    #[allow(clippy::missing_safety_doc)]
    pub unsafe fn from_raw_handle(handle: crate::PlatformHandle) -> Pipe {
        Pipe(UnixStream::from_raw_fd(handle.into_raw()))
    }

    pub fn shutdown(&mut self) -> Result<()> {
        self.0.shutdown(std::net::Shutdown::Both)
    }
}

impl RecvMsg for Pipe {
    // Receive data (and fds) from the associated connection.  `recv_msg` expects the capacity of
    // the `ConnectionBuffer` members has been adjusted appropriate by the caller.
    fn recv_msg(&mut self, buf: &mut ConnectionBuffer) -> Result<usize> {
        assert!(buf.buf.remaining_mut() > 0);
        assert!(buf.cmsg.remaining_mut() > 0);
        // TODO: MSG_CMSG_CLOEXEC not portable.
        // TODO: MSG_NOSIGNAL not portable; macOS can set socket option SO_NOSIGPIPE instead.
        #[cfg(target_os = "linux")]
        let flags = libc::MSG_CMSG_CLOEXEC | libc::MSG_NOSIGNAL;
        #[cfg(not(target_os = "linux"))]
        let flags = 0;
        let r = unsafe {
            let mut iovec = [<&mut IoVec>::from(buf.buf.bytes_mut())];
            msg::recv_msg_with_flags(self.0.as_raw_fd(), &mut iovec, buf.cmsg.bytes_mut(), flags)
        };
        match r {
            Ok((n, cmsg_n, msg_flags)) => unsafe {
                trace!("recv_msg_with_flags flags={}", msg_flags);
                buf.buf.advance_mut(n);
                buf.cmsg.advance_mut(cmsg_n);
                Ok(n)
            },
            Err(e) => Err(e),
        }
    }
}

impl SendMsg for Pipe {
    // Send data (and fds) on the associated connection.  `send_msg` adjusts the length of the
    // `ConnectionBuffer` members based on the size of the successful send operation.
    fn send_msg(&mut self, buf: &mut ConnectionBuffer) -> Result<usize> {
        assert!(!buf.buf.is_empty());
        let r = {
            // TODO: MSG_NOSIGNAL not portable; macOS can set socket option SO_NOSIGPIPE instead.
            #[cfg(target_os = "linux")]
            let flags = libc::MSG_NOSIGNAL;
            #[cfg(not(target_os = "linux"))]
            let flags = 0;
            let iovec = [<&IoVec>::from(&buf.buf[..buf.buf.len()])];
            msg::send_msg_with_flags(
                self.0.as_raw_fd(),
                &iovec,
                &buf.cmsg[..buf.cmsg.len()],
                flags,
            )
        };
        match r {
            Ok(n) => {
                buf.buf.advance(n);
                // Close sent fds.
                close_fds(&mut buf.cmsg);
                Ok(n)
            }
            Err(e) => Err(e),
        }
    }
}

// Platform-specific wrapper around `BytesMut`.
// `cmsg` is a secondary buffer used for sending/receiving
// fds via `sendmsg`/`recvmsg` on a Unix Domain Socket.
#[derive(Debug)]
pub struct ConnectionBuffer {
    pub buf: BytesMut,
    pub cmsg: BytesMut,
}

impl ConnectionBuffer {
    pub fn with_capacity(cap: usize) -> Self {
        ConnectionBuffer {
            buf: BytesMut::with_capacity(cap),
            cmsg: BytesMut::with_capacity(cmsg::space(std::mem::size_of::<RawFd>())),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.buf.is_empty() && self.cmsg.is_empty()
    }
}

// Close any unprocessed fds in cmsg buffer.
impl Drop for ConnectionBuffer {
    fn drop(&mut self) {
        if !self.cmsg.is_empty() {
            debug!(
                "ConnectionBuffer dropped with {} bytes in cmsg",
                self.cmsg.len()
            );
            close_fds(&mut self.cmsg);
        }
    }
}

fn close_fds(cmsg: &mut BytesMut) {
    while !cmsg.is_empty() {
        let fd = cmsg::decode_handle(cmsg);
        unsafe {
            close_platform_handle(fd);
        }
    }
    assert!(cmsg.is_empty());
}
