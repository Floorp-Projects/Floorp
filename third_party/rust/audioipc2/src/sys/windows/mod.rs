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

use bytes::{Buf, BufMut};
use mio::windows::NamedPipe;
use windows_sys::Win32::Storage::FileSystem::FILE_FLAG_OVERLAPPED;

use crate::PlatformHandle;

use super::{ConnectionBuffer, RecvMsg, SendMsg};

pub struct Pipe {
    pub(crate) io: NamedPipe,
}

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

    Ok((Pipe::new(server), client))
}

static PIPE_ID: AtomicUsize = AtomicUsize::new(0);

fn get_pipe_name() -> String {
    let pid = std::process::id();
    let pipe_id = PIPE_ID.fetch_add(1, Ordering::Relaxed);
    format!("\\\\.\\pipe\\LOCAL\\cubeb-pipe-{pid}-{pipe_id}")
}

impl Pipe {
    pub fn new(io: NamedPipe) -> Self {
        Self { io }
    }

    #[allow(clippy::missing_safety_doc)]
    pub unsafe fn from_raw_handle(handle: crate::PlatformHandle) -> Pipe {
        Pipe::new(NamedPipe::from_raw_handle(handle.into_raw()))
    }

    pub fn shutdown(&mut self) -> Result<()> {
        self.io.disconnect()
    }
}

impl RecvMsg for Pipe {
    // Receive data from the associated connection.  `recv_msg` expects the capacity of
    // the `ConnectionBuffer` members has been adjusted appropriate by the caller.
    fn recv_msg(&mut self, buf: &mut ConnectionBuffer) -> Result<usize> {
        assert!(buf.buf.remaining_mut() > 0);
        let r = unsafe {
            let chunk = buf.buf.chunk_mut();
            let slice = std::slice::from_raw_parts_mut(chunk.as_mut_ptr(), chunk.len());
            self.io.read(slice)
        };
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
        let r = self.io.write(&buf.buf[..buf.buf.len()]);
        if let Ok(n) = r {
            buf.buf.advance(n);
            while let Some(mut handle) = buf.pop_handle() {
                handle.mark_sent()
            }
        }
        r
    }
}
