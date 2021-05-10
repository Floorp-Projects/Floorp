// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use super::tokio_named_pipes;
use std::os::windows::fs::*;
use std::os::windows::io::{AsRawHandle, FromRawHandle, IntoRawHandle, RawHandle};
use std::sync::atomic::{AtomicUsize, Ordering};
use tokio_io::{AsyncRead, AsyncWrite};
use winapi::um::winbase::FILE_FLAG_OVERLAPPED;

#[derive(Debug)]
pub struct MessageStream(miow::pipe::NamedPipe);
pub struct AsyncMessageStream(tokio_named_pipes::NamedPipe);

impl MessageStream {
    fn new(stream: miow::pipe::NamedPipe) -> MessageStream {
        MessageStream(stream)
    }

    pub fn anonymous_ipc_pair(
    ) -> std::result::Result<(MessageStream, MessageStream), std::io::Error> {
        let pipe_name = get_pipe_name();
        let pipe_server = miow::pipe::NamedPipe::new(&pipe_name)?;
        let pipe_client = {
            let mut opts = std::fs::OpenOptions::new();
            opts.read(true)
                .write(true)
                .custom_flags(FILE_FLAG_OVERLAPPED);
            let file = opts.open(&pipe_name)?;
            unsafe { miow::pipe::NamedPipe::from_raw_handle(file.into_raw_handle()) }
        };
        Ok((
            MessageStream::new(pipe_server),
            MessageStream::new(pipe_client),
        ))
    }

    #[allow(clippy::missing_safety_doc)]
    pub unsafe fn from_raw_fd(raw: super::PlatformHandleType) -> MessageStream {
        MessageStream::new(miow::pipe::NamedPipe::from_raw_handle(raw))
    }

    pub fn into_tokio_ipc(
        self,
        handle: &tokio::reactor::Handle,
    ) -> std::result::Result<AsyncMessageStream, std::io::Error> {
        let pipe = unsafe { mio_named_pipes::NamedPipe::from_raw_handle(self.into_raw_handle()) };
        Ok(AsyncMessageStream::new(
            tokio_named_pipes::NamedPipe::from_pipe(pipe, handle)?,
        ))
    }
}

impl IntoRawHandle for MessageStream {
    fn into_raw_handle(self) -> RawHandle {
        self.0.into_raw_handle()
    }
}

impl AsyncMessageStream {
    fn new(stream: tokio_named_pipes::NamedPipe) -> AsyncMessageStream {
        AsyncMessageStream(stream)
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
        <tokio_named_pipes::NamedPipe>::read_buf(&mut self.0, buf)
    }
}

impl AsyncWrite for AsyncMessageStream {
    fn shutdown(&mut self) -> futures::Poll<(), std::io::Error> {
        <tokio_named_pipes::NamedPipe>::shutdown(&mut self.0)
    }

    fn write_buf<B: bytes::Buf>(&mut self, buf: &mut B) -> futures::Poll<usize, std::io::Error> {
        <tokio_named_pipes::NamedPipe>::write_buf(&mut self.0, buf)
    }
}

impl AsRawHandle for AsyncMessageStream {
    fn as_raw_handle(&self) -> RawHandle {
        self.0.as_raw_handle()
    }
}

static PIPE_ID: AtomicUsize = AtomicUsize::new(0);

fn get_pipe_name() -> String {
    let pid = std::process::id();
    let pipe_id = PIPE_ID.fetch_add(1, Ordering::SeqCst);
    format!("\\\\.\\pipe\\cubeb-pipe-{}-{}", pid, pipe_id)
}
