/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

extern crate hashbrown;
extern crate hashglobe;
extern crate smallvec;

use hashbrown::hash_map::Entry;
use hashbrown::CollectionAllocErr;
use smallvec::Array;
use smallvec::SmallVec;
use std::alloc::{self, Layout};
use std::mem;
use std::ptr::copy_nonoverlapping;
use std::vec::Vec;

pub trait FallibleVec<T> {
    /// Append |val| to the end of |vec|.  Returns Ok(()) on success,
    /// Err(reason) if it fails, with |reason| describing the failure.
    fn try_push(&mut self, value: T) -> Result<(), CollectionAllocErr>;
}

pub trait FallibleHashMap<K, V, H> {
    fn try_insert(&mut self, k: K, v: V) -> Result<Option<V>, CollectionAllocErr>;
    fn try_entry(&mut self, k: K) -> Result<Entry<K, V, H>, CollectionAllocErr>;
}

pub trait FallibleHashSet<T, H> {
    fn try_insert(&mut self, x: T) -> Result<bool, CollectionAllocErr>;
}

impl<K, V, H> FallibleHashMap<K, V, H> for hashbrown::HashMap<K, V, H>
where
    K: Eq + std::hash::Hash,
    H: std::hash::BuildHasher,
{
    #[inline]
    fn try_insert(&mut self, k: K, v: V) -> Result<Option<V>, CollectionAllocErr> {
        self.try_reserve(1)?;
        Ok(self.insert(k, v))
    }

    #[inline]
    fn try_entry(&mut self, k: K) -> Result<Entry<K, V, H>, CollectionAllocErr> {
        self.try_reserve(1)?;
        Ok(self.entry(k))
    }
}

impl<T, H> FallibleHashSet<T, H> for hashbrown::HashSet<T, H>
where
    T: Eq + std::hash::Hash,
    H: std::hash::BuildHasher,
{
    #[inline]
    fn try_insert(&mut self, x: T) -> Result<bool, CollectionAllocErr> {
        self.try_reserve(1)?;
        Ok(self.insert(x))
    }
}

/////////////////////////////////////////////////////////////////
// Vec

impl<T> FallibleVec<T> for Vec<T> {
    #[inline(always)]
    fn try_push(&mut self, val: T) -> Result<(), CollectionAllocErr> {
        if self.capacity() == self.len() {
            try_double_vec(self)?;
            debug_assert!(self.capacity() > self.len());
        }
        self.push(val);
        Ok(())
    }
}

/// FIXME: use `Layout::array` when it’s stable https://github.com/rust-lang/rust/issues/55724
fn layout_array<T>(n: usize) -> Result<Layout, CollectionAllocErr> {
    let size = n.checked_mul(mem::size_of::<T>())
        .ok_or(CollectionAllocErr::CapacityOverflow)?;
    let align = std::mem::align_of::<T>();
    Layout::from_size_align(size, align)
        .map_err(|_| CollectionAllocErr::CapacityOverflow)
}

// Double the capacity of |vec|, or fail to do so due to lack of memory.
// Returns Ok(()) on success, Err(..) on failure.
#[inline(never)]
#[cold]
fn try_double_vec<T>(vec: &mut Vec<T>) -> Result<(), CollectionAllocErr> {

    let old_ptr = vec.as_mut_ptr();
    let old_len = vec.len();

    let old_cap: usize = vec.capacity();
    let new_cap: usize = if old_cap == 0 {
        4
    } else {
        old_cap
            .checked_mul(2)
            .ok_or(CollectionAllocErr::CapacityOverflow)?
    };

    let old_layout = layout_array::<T>(old_cap)?;
    let new_layout = layout_array::<T>(new_cap)?;

    let new_ptr = unsafe {
        if old_cap == 0 {
            alloc::alloc(new_layout)
        } else {
            alloc::realloc(old_ptr as *mut u8, old_layout, new_layout.size())
        }
    };

    if new_ptr.is_null() {
        return Err(CollectionAllocErr::AllocErr { layout: new_layout });
    }

    let new_vec = unsafe { Vec::from_raw_parts(new_ptr as *mut T, old_len, new_cap) };

    mem::forget(mem::replace(vec, new_vec));
    Ok(())
}

/////////////////////////////////////////////////////////////////
// SmallVec

impl<T: Array> FallibleVec<T::Item> for SmallVec<T> {
    #[inline(always)]
    fn try_push(&mut self, val: T::Item) -> Result<(), CollectionAllocErr> {
        if self.capacity() == self.len() {
            try_double_small_vec(self)?;
            debug_assert!(self.capacity() > self.len());
        }
        self.push(val);
        Ok(())
    }
}

// Double the capacity of |svec|, or fail to do so due to lack of memory.
// Returns Ok(()) on success, Err(..) on failure.
#[inline(never)]
#[cold]
fn try_double_small_vec<T>(svec: &mut SmallVec<T>) -> Result<(), CollectionAllocErr>
where
    T: Array,
{
    let old_ptr = svec.as_mut_ptr();
    let old_len = svec.len();

    let old_cap: usize = svec.capacity();
    let new_cap: usize = if old_cap == 0 {
        4
    } else {
        old_cap
            .checked_mul(2)
            .ok_or(CollectionAllocErr::CapacityOverflow)?
    };

    // This surely shouldn't fail, if |old_cap| was previously accepted as a
    // valid value.  But err on the side of caution.
    let old_layout = layout_array::<T>(old_cap)?;
    let new_layout = layout_array::<T>(new_cap)?;

    let new_ptr;
    if svec.spilled() {
        // There's an old block to free, and, presumably, old contents to
        // copy.  realloc takes care of both aspects.
        unsafe {
            new_ptr = alloc::realloc(old_ptr as *mut u8, old_layout, new_layout.size());
        }
    } else {
        // There's no old block to free.  There may be old contents to copy.
        unsafe {
            new_ptr = alloc::alloc(new_layout);
            if !new_ptr.is_null() && old_layout.size() > 0 {
                copy_nonoverlapping(old_ptr as *const u8, new_ptr as *mut u8, old_layout.size());
            }
        }
    }

    if new_ptr.is_null() {
        return Err(CollectionAllocErr::AllocErr { layout: new_layout });
    }

    let new_vec = unsafe { Vec::from_raw_parts(new_ptr as *mut T::Item, old_len, new_cap) };

    let new_svec = SmallVec::from_vec(new_vec);
    mem::forget(mem::replace(svec, new_svec));
    Ok(())
}
