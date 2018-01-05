/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::mem;
use std::vec::Vec;

extern "C" {
    fn realloc(ptr: *mut u8, bytes: usize) -> *mut u8;
    fn malloc(bytes: usize) -> *mut u8;
}

pub trait FallibleVec<T> {
    /// Append |val| to the end of |vec|.  Returns Ok(()) on success,
    /// Err(()) if it fails, which can only be due to lack of memory.
    fn try_push(&mut self, value: T) -> Result<(), ()>;

    /// Expand the vector size. Return Ok(()) on success, Err(()) if it
    /// fails.
    fn try_reserve(&mut self, new_cap: usize) -> Result<(), ()>;
}

/////////////////////////////////////////////////////////////////
// Vec

impl<T> FallibleVec<T> for Vec<T> {
    #[inline]
    fn try_push(&mut self, val: T) -> Result<(), ()> {
        if self.capacity() == self.len() {
            let old_cap: usize = self.capacity();
            let new_cap: usize
                = if old_cap == 0 { 4 } else { old_cap.checked_mul(2).ok_or(()) ? };

            try_extend_vec(self, new_cap)?;
            debug_assert!(self.capacity() > self.len());
        }
        self.push(val);
        Ok(())
    }

    #[inline]
    fn try_reserve(&mut self, cap: usize) -> Result<(), ()> {
        let new_cap = cap + self.capacity();
        try_extend_vec(self, new_cap)?;
        debug_assert!(self.capacity() == new_cap);
        Ok(())
    }
}

#[inline(never)]
#[cold]
fn try_extend_vec<T>(vec: &mut Vec<T>, new_cap: usize) -> Result<(), ()> {
    let old_ptr = vec.as_mut_ptr();
    let old_len = vec.len();

    let old_cap: usize = vec.capacity();

    if old_cap >= new_cap {
        return Ok(());
    }

    let new_size_bytes
        = new_cap.checked_mul(mem::size_of::<T>()).ok_or(()) ? ;

    let new_ptr = unsafe {
        if old_cap == 0 {
            malloc(new_size_bytes)
        } else {
            realloc(old_ptr as *mut u8, new_size_bytes)
        }
    };

    if new_ptr.is_null() {
        return Err(());
    }

    let new_vec = unsafe {
        Vec::from_raw_parts(new_ptr as *mut T, old_len, new_cap)
    };

    mem::forget(mem::replace(vec, new_vec));
    Ok(())
}

#[test]
fn oom_test() {
    let mut vec: Vec<char> = Vec::new();
    match vec.try_reserve(std::usize::MAX) {
        Ok(_) => panic!("it should be OOM"),
        _ => (),
    }
}
