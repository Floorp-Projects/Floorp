// Copyright 2019 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for macOS
use crate::util_libc::{last_os_error, Weak};
use crate::{use_file, Error};
use core::mem;

type GetEntropyFn = unsafe extern "C" fn(*mut u8, libc::size_t) -> libc::c_int;

pub fn getrandom_inner(dest: &mut [u8]) -> Result<(), Error> {
    static GETENTROPY: Weak = unsafe { Weak::new("getentropy\0") };
    if let Some(fptr) = GETENTROPY.ptr() {
        let func: GetEntropyFn = unsafe { mem::transmute(fptr) };
        for chunk in dest.chunks_mut(256) {
            let ret = unsafe { func(chunk.as_mut_ptr(), chunk.len()) };
            if ret != 0 {
                let err = last_os_error();
                error!("getentropy syscall failed");
                return Err(err);
            }
        }
        Ok(())
    } else {
        // We fallback to reading from /dev/random instead of SecRandomCopyBytes
        // to avoid high startup costs and linking the Security framework.
        use_file::getrandom_inner(dest)
    }
}
