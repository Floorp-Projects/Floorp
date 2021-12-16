// Copyright © 2021 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use std::{
    fs::OpenOptions,
    io::{Read, Write},
    os::windows::prelude::{FromRawHandle, OpenOptionsExt},
    sync::atomic::{AtomicUsize, Ordering},
};

use std::io::Result;

use bytes::{BufMut, BytesMut};
use mio::windows::NamedPipe;
use winapi::um::winbase::FILE_FLAG_OVERLAPPED;

use crate::PlatformHandle;

use super::{RecvMsg, SendMsg};

pub struct Pipe(pub NamedPipe);

// Create a connected "pipe" pair.  The `Pipe` is the server end,
// the `PlatformHandle` is the client end to be remoted.
pub fn make_pipe_pair() -> Result<(Pipe, PlatformHandle)> {
    let pipe_name = get_pipe_name();
    let server = NamedPipe::new(&pipe_name)?;

    let client = {
        let mut opts = OpenOptions::new();
        opts.read(true)
            .write(true)
            .custom_flags(FILE_FLAG_OVERLAPPED);
        let file = opts.open(&pipe_name)?;
        PlatformHandle::from(file)
    };

    Ok((Pipe(server), client))
}

static PIPE_ID: AtomicUsize = AtomicUsize::new(0);

fn get_pipe_name() -> String {
    let pid = std::process::id();
    let pipe_id = PIPE_ID.fetch_add(1, Ordering::Relaxed);
    format!("\\\\.\\pipe\\LOCAL\\cubeb-pipe-{}-{}", pid, pipe_id)
}

impl Pipe {
    #[allow(clippy::missing_safety_doc)]
    pub unsafe fn from_raw_handle(handle: crate::PlatformHandle) -> Pipe {
        Pipe(NamedPipe::from_raw_handle(handle.into_raw()))
    }

    pub fn shutdown(&mut self) -> Result<()> {
        self.0.disconnect()
    }
}

impl RecvMsg for Pipe {
    // Receive data from the associated connection.  `recv_msg` expects the capacity of
    // the `ConnectionBuffer` members has been adjusted appropriate by the caller.
    fn recv_msg(&mut self, buf: &mut ConnectionBuffer) -> Result<usize> {
        assert!(buf.buf.remaining_mut() > 0);
        let r = unsafe { self.0.read(buf.buf.bytes_mut()) };
        match r {
            Ok(n) => unsafe {
                buf.buf.advance_mut(n);
                Ok(n)
            },
            e => e,
        }
    }
}

impl SendMsg for Pipe {
    // Send data on the associated connection.  `send_msg` adjusts the length of the
    // `ConnectionBuffer` members based on the size of the successful send operation.
    fn send_msg(&mut self, buf: &mut ConnectionBuffer) -> Result<usize> {
        assert!(!buf.buf.is_empty());
        let r = self.0.write(&buf.buf[..buf.buf.len()]);
        if let Ok(n) = r {
            buf.buf.advance(n);
        }
        r
    }
}

// Platform-specific wrapper around `BytesMut`.
#[derive(Debug)]
pub struct ConnectionBuffer {
    pub buf: BytesMut,
}

impl ConnectionBuffer {
    pub fn with_capacity(cap: usize) -> Self {
        ConnectionBuffer {
            buf: BytesMut::with_capacity(cap),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.buf.is_empty()
    }
}
