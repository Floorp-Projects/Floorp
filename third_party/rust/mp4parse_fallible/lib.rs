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

    /// Reserves capacity for at least `additional` more elements to
    /// be inserted in the vector. Does nothing if capacity is already
    /// sufficient. Return Ok(()) on success, Err(()) if it fails either
    /// due to lack of memory, or overflowing the `usize` used to store
    /// the capacity.
    fn try_reserve(&mut self, additional: usize) -> Result<(), ()>;

    /// Clones and appends all elements in a slice to the Vec.
    /// Returns Ok(()) on success, Err(()) if it fails, which can
    /// only be due to lack of memory.
    fn try_extend_from_slice(&mut self, other: &[T]) -> Result<(), ()> where T: Clone;
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
    fn try_reserve(&mut self, additional: usize) -> Result<(), ()> {
        let available = self.capacity().checked_sub(self.len()).expect("capacity >= len");
        if additional > available {
            let increase = additional.checked_sub(available).expect("additional > available");
            let new_cap = self.capacity().checked_add(increase).ok_or(())?;
            try_extend_vec(self, new_cap)?;
            debug_assert!(self.capacity() == new_cap);
        }
        Ok(())
    }

    #[inline]
    fn try_extend_from_slice(&mut self, other: &[T]) -> Result<(), ()> where T: Clone {
        FallibleVec::try_reserve(self, other.len())?;
        self.extend_from_slice(other);
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
fn oom() {
    let mut vec: Vec<char> = Vec::new();
    match FallibleVec::try_reserve(&mut vec, std::usize::MAX) {
        Ok(_) => panic!("it should be OOM"),
        _ => (),
    }
}

#[test]
fn try_reserve() {
    let mut vec = vec![1];
    let old_cap = vec.capacity();
    let new_cap = old_cap + 1;
    FallibleVec::try_reserve(&mut vec, new_cap).unwrap();
    assert!(vec.capacity() >= new_cap);
}

#[test]
fn try_reserve_idempotent() {
    let mut vec = vec![1];
    let old_cap = vec.capacity();
    let new_cap = old_cap + 1;
    FallibleVec::try_reserve(&mut vec, new_cap).unwrap();
    let cap_after_reserve = vec.capacity();
    FallibleVec::try_reserve(&mut vec, new_cap).unwrap();
    assert_eq!(cap_after_reserve, vec.capacity());
}

#[test]
fn capacity_overflow() {
    let mut vec = vec![1];
    match FallibleVec::try_reserve(&mut vec, std::usize::MAX) {
        Ok(_) => panic!("capacity calculation should overflow"),
        _ => (),
    }
}

#[test]
fn extend_from_slice() {
    let mut vec = b"foo".to_vec();
    FallibleVec::try_extend_from_slice(&mut vec, b"bar").unwrap();
    assert_eq!(&vec, b"foobar");
}
