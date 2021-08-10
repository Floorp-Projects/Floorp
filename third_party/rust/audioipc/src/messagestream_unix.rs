// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use super::tokio_uds_stream as tokio_uds;
use futures::Poll;
use mio::Ready;
use std::os::unix::io::{AsRawFd, FromRawFd, IntoRawFd, RawFd};
use std::os::unix::net;
use tokio_io::{AsyncRead, AsyncWrite};

#[derive(Debug)]
pub struct MessageStream(net::UnixStream);
pub struct AsyncMessageStream(tokio_uds::UnixStream);

impl MessageStream {
    fn new(stream: net::UnixStream) -> MessageStream {
        MessageStream(stream)
    }

    pub fn anonymous_ipc_pair(
    ) -> std::result::Result<(MessageStream, MessageStream), std::io::Error> {
        let pair = net::UnixStream::pair()?;
        Ok((MessageStream::new(pair.0), MessageStream::new(pair.1)))
    }

    #[allow(clippy::missing_safety_doc)]
    pub unsafe fn from_raw_fd(raw: super::PlatformHandleType) -> MessageStream {
        MessageStream::new(net::UnixStream::from_raw_fd(raw))
    }

    pub fn into_tokio_ipc(
        self,
        handle: &tokio::reactor::Handle,
    ) -> std::result::Result<AsyncMessageStream, std::io::Error> {
        Ok(AsyncMessageStream::new(tokio_uds::UnixStream::from_std(
            self.0, handle,
        )?))
    }
}

impl IntoRawFd for MessageStream {
    fn into_raw_fd(self) -> RawFd {
        self.0.into_raw_fd()
    }
}

impl AsyncMessageStream {
    fn new(stream: tokio_uds::UnixStream) -> AsyncMessageStream {
        AsyncMessageStream(stream)
    }

    pub fn poll_read_ready(&self, ready: Ready) -> Poll<Ready, std::io::Error> {
        self.0.poll_read_ready(ready)
    }

    pub fn clear_read_ready(&self, ready: Ready) -> Result<(), std::io::Error> {
        self.0.clear_read_ready(ready)
    }

    pub fn poll_write_ready(&self) -> Poll<Ready, std::io::Error> {
        self.0.poll_write_ready()
    }

    pub fn clear_write_ready(&self) -> Result<(), std::io::Error> {
        self.0.clear_write_ready()
    }
}

impl std::io::Read for AsyncMessageStream {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        self.0.read(buf)
    }
}

impl std::io::Write for AsyncMessageStream {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.0.write(buf)
    }
    fn flush(&mut self) -> std::io::Result<()> {
        self.0.flush()
    }
}

impl AsyncRead for AsyncMessageStream {
    fn read_buf<B: bytes::BufMut>(&mut self, buf: &mut B) -> futures::Poll<usize, std::io::Error> {
        <&tokio_uds::UnixStream>::read_buf(&mut &self.0, buf)
    }
}

impl AsyncWrite for AsyncMessageStream {
    fn shutdown(&mut self) -> futures::Poll<(), std::io::Error> {
        <&tokio_uds::UnixStream>::shutdown(&mut &self.0)
    }

    fn write_buf<B: bytes::Buf>(&mut self, buf: &mut B) -> futures::Poll<usize, std::io::Error> {
        <&tokio_uds::UnixStream>::write_buf(&mut &self.0, buf)
    }
}

impl AsRawFd for AsyncMessageStream {
    fn as_raw_fd(&self) -> RawFd {
        self.0.as_raw_fd()
    }
}
