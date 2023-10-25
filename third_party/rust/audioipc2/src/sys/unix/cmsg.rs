// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use crate::sys::HANDLE_QUEUE_LIMIT;
use bytes::{BufMut, BytesMut};
use libc::{self, cmsghdr};
use std::convert::TryInto;
use std::os::unix::io::RawFd;
use std::{mem, slice};

trait AsBytes {
    fn as_bytes(&self) -> &[u8];
}

impl<'a, T: Sized> AsBytes for &'a [T] {
    fn as_bytes(&self) -> &[u8] {
        // TODO: This should account for the alignment of T
        let byte_count = std::mem::size_of_val(*self);
        unsafe { slice::from_raw_parts(self.as_ptr() as *const _, byte_count) }
    }
}

// Encode `handles` into a cmsghdr in `buf`.
pub fn encode_handles(cmsg: &mut BytesMut, handles: &[RawFd]) {
    assert!(handles.len() <= HANDLE_QUEUE_LIMIT);
    let msg = handles.as_bytes();

    let cmsg_space = space(msg.len());
    assert!(cmsg.remaining_mut() >= cmsg_space);

    // Some definitions of cmsghdr contain padding.  Rather
    // than try to keep an up-to-date #cfg list to handle
    // that, just use a pre-zeroed struct to fill out any
    // fields we don't care about.
    let zeroed = unsafe { mem::zeroed() };
    #[allow(clippy::needless_update)]
    // `cmsg_len` is `usize` on some platforms, `u32` on others.
    #[allow(clippy::useless_conversion)]
    let cmsghdr = cmsghdr {
        cmsg_len: len(msg.len()).try_into().unwrap(),
        cmsg_level: libc::SOL_SOCKET,
        cmsg_type: libc::SCM_RIGHTS,
        ..zeroed
    };

    unsafe {
        let cmsghdr_ptr = cmsg.chunk_mut().as_mut_ptr();
        std::ptr::copy_nonoverlapping(
            &cmsghdr as *const _ as *const _,
            cmsghdr_ptr,
            mem::size_of::<cmsghdr>(),
        );
        let cmsg_data_ptr = libc::CMSG_DATA(cmsghdr_ptr as _);
        std::ptr::copy_nonoverlapping(msg.as_ptr(), cmsg_data_ptr, msg.len());
        cmsg.advance_mut(cmsg_space);
    }
}

// Decode `buf` containing a cmsghdr with one or more handle(s).
pub fn decode_handles(buf: &mut BytesMut) -> arrayvec::ArrayVec<RawFd, HANDLE_QUEUE_LIMIT> {
    let mut fds = arrayvec::ArrayVec::<RawFd, HANDLE_QUEUE_LIMIT>::new();

    let cmsghdr_len = len(0);

    if buf.len() < cmsghdr_len {
        // No more entries---not enough data in `buf` for a
        // complete message.
        return fds;
    }

    let cmsg: &cmsghdr = unsafe { &*(buf.as_ptr() as *const _) };
    #[allow(clippy::unnecessary_cast)] // `cmsg_len` type is platform-dependent.
    let cmsg_len = cmsg.cmsg_len as usize;

    match (cmsg.cmsg_level, cmsg.cmsg_type) {
        (libc::SOL_SOCKET, libc::SCM_RIGHTS) => {
            trace!("Found SCM_RIGHTS...");
            let slice = &buf[cmsghdr_len..cmsg_len];
            let slice = unsafe {
                slice::from_raw_parts(
                    slice.as_ptr() as *const _,
                    slice.len() / mem::size_of::<i32>(),
                )
            };
            fds.try_extend_from_slice(slice).unwrap();
        }
        (level, kind) => {
            trace!("Skipping cmsg level, {}, type={}...", level, kind);
        }
    }

    assert!(fds.len() <= HANDLE_QUEUE_LIMIT);
    fds
}

fn len(len: usize) -> usize {
    unsafe { libc::CMSG_LEN(len.try_into().unwrap()) as usize }
}

pub fn space(len: usize) -> usize {
    unsafe { libc::CMSG_SPACE(len.try_into().unwrap()) as usize }
}
