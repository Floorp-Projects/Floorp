// Copied from tokio-named-pipes/src/lib.rs revision 49ec1ba8bbc94ab6fc9636af2a00dfb3204080c8 (tokio-named-pipes 0.2.0)
// This file is dual licensed under the MIT and Apache-2.0 per upstream: https://github.com/NikVolf/tokio-named-pipes/blob/stable/LICENSE-MIT and https://github.com/NikVolf/tokio-named-pipes/blob/stable/LICENSE-APACHE
// - Implement AsyncWrite::shutdown

#![cfg(windows)]

use std::ffi::OsStr;
use std::fmt;
use std::io::{Read, Write};
use std::os::windows::io::*;

use bytes::{Buf, BufMut};
use futures::{Async, Poll};
use mio::Ready;
use tokio::io::{AsyncRead, AsyncWrite};
use tokio::reactor::{Handle, PollEvented2};

pub struct NamedPipe {
    io: PollEvented2<mio_named_pipes::NamedPipe>,
}

impl NamedPipe {
    pub fn new<P: AsRef<OsStr>>(p: P, handle: &Handle) -> std::io::Result<NamedPipe> {
        NamedPipe::_new(p.as_ref(), handle)
    }

    fn _new(p: &OsStr, handle: &Handle) -> std::io::Result<NamedPipe> {
        let inner = mio_named_pipes::NamedPipe::new(p)?;
        NamedPipe::from_pipe(inner, handle)
    }

    pub fn from_pipe(
        pipe: mio_named_pipes::NamedPipe,
        handle: &Handle,
    ) -> std::io::Result<NamedPipe> {
        Ok(NamedPipe {
            io: PollEvented2::new_with_handle(pipe, handle)?,
        })
    }

    pub fn connect(&self) -> std::io::Result<()> {
        self.io.get_ref().connect()
    }

    pub fn disconnect(&self) -> std::io::Result<()> {
        self.io.get_ref().disconnect()
    }

    pub fn poll_read_ready_readable(&mut self) -> tokio::io::Result<Async<Ready>> {
        self.io.poll_read_ready(Ready::readable())
    }

    pub fn poll_write_ready(&mut self) -> tokio::io::Result<Async<Ready>> {
        self.io.poll_write_ready()
    }

    fn io_mut(&mut self) -> &mut PollEvented2<mio_named_pipes::NamedPipe> {
        &mut self.io
    }
}

impl Read for NamedPipe {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        self.io.read(buf)
    }
}

impl Write for NamedPipe {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.io.write(buf)
    }
    fn flush(&mut self) -> std::io::Result<()> {
        self.io.flush()
    }
}

impl<'a> Read for &'a NamedPipe {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        (&self.io).read(buf)
    }
}

impl<'a> Write for &'a NamedPipe {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        (&self.io).write(buf)
    }

    fn flush(&mut self) -> std::io::Result<()> {
        (&self.io).flush()
    }
}

impl AsyncRead for NamedPipe {
    unsafe fn prepare_uninitialized_buffer(&self, _: &mut [u8]) -> bool {
        false
    }

    fn read_buf<B: BufMut>(&mut self, buf: &mut B) -> Poll<usize, std::io::Error> {
        if let Async::NotReady = self.io.poll_read_ready(Ready::readable())? {
            return Ok(Async::NotReady);
        }

        let mut stack_buf = [0u8; 1024];
        let bytes_read = self.io_mut().read(&mut stack_buf);
        match bytes_read {
            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                self.io_mut().clear_read_ready(Ready::readable())?;
                Ok(Async::NotReady)
            }
            Err(e) => Err(e),
            Ok(bytes_read) => {
                buf.put_slice(&stack_buf[0..bytes_read]);
                Ok(Async::Ready(bytes_read))
            }
        }
    }
}

impl AsyncWrite for NamedPipe {
    fn shutdown(&mut self) -> Poll<(), std::io::Error> {
        let _ = self.disconnect();
        Ok(().into())
    }

    fn write_buf<B: Buf>(&mut self, buf: &mut B) -> Poll<usize, std::io::Error> {
        if let Async::NotReady = self.io.poll_write_ready()? {
            return Ok(Async::NotReady);
        }

        let bytes_wrt = self.io_mut().write(buf.bytes());
        match bytes_wrt {
            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                self.io_mut().clear_write_ready()?;
                Ok(Async::NotReady)
            }
            Err(e) => Err(e),
            Ok(bytes_wrt) => {
                buf.advance(bytes_wrt);
                Ok(Async::Ready(bytes_wrt))
            }
        }
    }
}

impl fmt::Debug for NamedPipe {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.io.get_ref().fmt(f)
    }
}

impl AsRawHandle for NamedPipe {
    fn as_raw_handle(&self) -> RawHandle {
        self.io.get_ref().as_raw_handle()
    }
}
