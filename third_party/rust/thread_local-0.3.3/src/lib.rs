// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

//! Per-object thread-local storage
//!
//! This library provides the `ThreadLocal` type which allows a separate copy of
//! an object to be used for each thread. This allows for per-object
//! thread-local storage, unlike the standard library's `thread_local!` macro
//! which only allows static thread-local storage.
//!
//! Per-thread objects are not destroyed when a thread exits. Instead, objects
//! are only destroyed when the `ThreadLocal` containing them is destroyed.
//!
//! You can also iterate over the thread-local values of all thread in a
//! `ThreadLocal` object using the `iter_mut` and `into_iter` methods. This can
//! only be done if you have mutable access to the `ThreadLocal` object, which
//! guarantees that you are the only thread currently accessing it.
//!
//! A `CachedThreadLocal` type is also provided which wraps a `ThreadLocal` but
//! also uses a special fast path for the first thread that writes into it. The
//! fast path has very low overhead (<1ns per access) while keeping the same
//! performance as `ThreadLocal` for other threads.
//!
//! Note that since thread IDs are recycled when a thread exits, it is possible
//! for one thread to retrieve the object of another thread. Since this can only
//! occur after a thread has exited this does not lead to any race conditions.
//!
//! # Examples
//!
//! Basic usage of `ThreadLocal`:
//!
//! ```rust
//! use thread_local::ThreadLocal;
//! let tls: ThreadLocal<u32> = ThreadLocal::new();
//! assert_eq!(tls.get(), None);
//! assert_eq!(tls.get_or(|| Box::new(5)), &5);
//! assert_eq!(tls.get(), Some(&5));
//! ```
//!
//! Combining thread-local values into a single result:
//!
//! ```rust
//! use thread_local::ThreadLocal;
//! use std::sync::Arc;
//! use std::cell::Cell;
//! use std::thread;
//!
//! let tls = Arc::new(ThreadLocal::new());
//!
//! // Create a bunch of threads to do stuff
//! for _ in 0..5 {
//!     let tls2 = tls.clone();
//!     thread::spawn(move || {
//!         // Increment a counter to count some event...
//!         let cell = tls2.get_or(|| Box::new(Cell::new(0)));
//!         cell.set(cell.get() + 1);
//!     }).join().unwrap();
//! }
//!
//! // Once all threads are done, collect the counter values and return the
//! // sum of all thread-local counter values.
//! let tls = Arc::try_unwrap(tls).unwrap();
//! let total = tls.into_iter().fold(0, |x, y| x + y.get());
//! assert_eq!(total, 5);
//! ```

#![warn(missing_docs)]

#[cfg(not(target_os = "emscripten"))]
extern crate thread_id;
extern crate unreachable;

use std::sync::atomic::{AtomicPtr, AtomicUsize, Ordering};
use std::sync::Mutex;
use std::marker::PhantomData;
use std::cell::UnsafeCell;
use std::fmt;
use std::iter::Chain;
use std::option::IntoIter as OptionIter;
use std::panic::UnwindSafe;
use unreachable::{UncheckedOptionExt, UncheckedResultExt};

/// Thread-local variable wrapper
///
/// See the [module-level documentation](index.html) for more.
pub struct ThreadLocal<T: ?Sized + Send> {
    // Pointer to the current top-level hash table
    table: AtomicPtr<Table<T>>,

    // Lock used to guard against concurrent modifications. This is only taken
    // while writing to the table, not when reading from it. This also guards
    // the counter for the total number of values in the hash table.
    lock: Mutex<usize>,

    // PhantomData to indicate that we logically own T
    marker: PhantomData<T>,
}

struct Table<T: ?Sized + Send> {
    // Hash entries for the table
    entries: Box<[TableEntry<T>]>,

    // Number of bits used for the hash function
    hash_bits: usize,

    // Previous table, half the size of the current one
    prev: Option<Box<Table<T>>>,
}

struct TableEntry<T: ?Sized + Send> {
    // Current owner of this entry, or 0 if this is an empty entry
    owner: AtomicUsize,

    // The object associated with this entry. This is only ever accessed by the
    // owner of the entry.
    data: UnsafeCell<Option<Box<T>>>,
}

// ThreadLocal is always Sync, even if T isn't
unsafe impl<T: ?Sized + Send> Sync for ThreadLocal<T> {}

impl<T: ?Sized + Send> Default for ThreadLocal<T> {
    fn default() -> ThreadLocal<T> {
        ThreadLocal::new()
    }
}

impl<T: ?Sized + Send> Drop for ThreadLocal<T> {
    fn drop(&mut self) {
        unsafe {
            Box::from_raw(self.table.load(Ordering::Relaxed));
        }
    }
}

// Implementation of Clone for TableEntry, needed to make vec![] work
impl<T: ?Sized + Send> Clone for TableEntry<T> {
    fn clone(&self) -> TableEntry<T> {
        TableEntry {
            owner: AtomicUsize::new(0),
            data: UnsafeCell::new(None),
        }
    }
}

// Helper function to get a thread id
#[cfg(not(target_os = "emscripten"))]
fn get_thread_id() -> usize {
    thread_id::get()
}
#[cfg(target_os = "emscripten")]
fn get_thread_id() -> usize {
    // pthread_self returns 0 on enscripten, but we use that as a
    // reserved value to indicate an empty slot. We instead fall
    // back to using the address of a thread-local variable, which
    // is slightly slower but guaranteed to produce a non-zero value.
    thread_local!(static KEY: u8 = unsafe { std::mem::uninitialized() });
    KEY.with(|x| x as *const _ as usize)
}

// Hash function for the thread id
#[cfg(target_pointer_width = "32")]
#[inline]
fn hash(id: usize, bits: usize) -> usize {
    id.wrapping_mul(0x9E3779B9) >> (32 - bits)
}
#[cfg(target_pointer_width = "64")]
#[inline]
fn hash(id: usize, bits: usize) -> usize {
    id.wrapping_mul(0x9E3779B97F4A7C15) >> (64 - bits)
}

impl<T: ?Sized + Send> ThreadLocal<T> {
    /// Creates a new empty `ThreadLocal`.
    pub fn new() -> ThreadLocal<T> {
        let entry = TableEntry {
            owner: AtomicUsize::new(0),
            data: UnsafeCell::new(None),
        };
        let table = Table {
            entries: vec![entry; 2].into_boxed_slice(),
            hash_bits: 1,
            prev: None,
        };
        ThreadLocal {
            table: AtomicPtr::new(Box::into_raw(Box::new(table))),
            lock: Mutex::new(0),
            marker: PhantomData,
        }
    }

    /// Returns the element for the current thread, if it exists.
    pub fn get(&self) -> Option<&T> {
        let id = get_thread_id();
        self.get_fast(id)
    }

    /// Returns the element for the current thread, or creates it if it doesn't
    /// exist.
    pub fn get_or<F>(&self, create: F) -> &T
        where F: FnOnce() -> Box<T>
    {
        unsafe { self.get_or_try(|| Ok::<Box<T>, ()>(create())).unchecked_unwrap_ok() }
    }

    /// Returns the element for the current thread, or creates it if it doesn't
    /// exist. If `create` fails, that error is returned and no element is
    /// added.
    pub fn get_or_try<F, E>(&self, create: F) -> Result<&T, E>
        where F: FnOnce() -> Result<Box<T>, E>
    {
        let id = get_thread_id();
        match self.get_fast(id) {
            Some(x) => Ok(x),
            None => Ok(self.insert(id, try!(create()), true)),
        }
    }

    // Simple hash table lookup function
    fn lookup(id: usize, table: &Table<T>) -> Option<&UnsafeCell<Option<Box<T>>>> {
        // Because we use a Mutex to prevent concurrent modifications (but not
        // reads) of the hash table, we can avoid any memory barriers here. No
        // elements between our hash bucket and our value can have been modified
        // since we inserted our thread-local value into the table.
        for entry in table.entries.iter().cycle().skip(hash(id, table.hash_bits)) {
            let owner = entry.owner.load(Ordering::Relaxed);
            if owner == id {
                return Some(&entry.data);
            }
            if owner == 0 {
                return None;
            }
        }
        unreachable!();
    }

    // Fast path: try to find our thread in the top-level hash table
    fn get_fast(&self, id: usize) -> Option<&T> {
        let table = unsafe { &*self.table.load(Ordering::Relaxed) };
        match Self::lookup(id, table) {
            Some(x) => unsafe { Some((*x.get()).as_ref().unchecked_unwrap()) },
            None => self.get_slow(id, table),
        }
    }

    // Slow path: try to find our thread in the other hash tables, and then
    // move it to the top-level hash table.
    #[cold]
    fn get_slow(&self, id: usize, table_top: &Table<T>) -> Option<&T> {
        let mut current = &table_top.prev;
        while let Some(ref table) = *current {
            if let Some(x) = Self::lookup(id, table) {
                let data = unsafe { (*x.get()).take().unchecked_unwrap() };
                return Some(self.insert(id, data, false));
            }
            current = &table.prev;
        }
        None
    }

    #[cold]
    fn insert(&self, id: usize, data: Box<T>, new: bool) -> &T {
        // Lock the Mutex to ensure only a single thread is modify the hash
        // table at once.
        let mut count = self.lock.lock().unwrap();
        if new {
            *count += 1;
        }
        let table_raw = self.table.load(Ordering::Relaxed);
        let table = unsafe { &*table_raw };

        // If the current top-level hash table is more than 75% full, add a new
        // level with 2x the capacity. Elements will be moved up to the new top
        // level table as they are accessed.
        let table = if *count > table.entries.len() * 3 / 4 {
            let entry = TableEntry {
                owner: AtomicUsize::new(0),
                data: UnsafeCell::new(None),
            };
            let new_table = Box::into_raw(Box::new(Table {
                entries: vec![entry; table.entries.len() * 2].into_boxed_slice(),
                hash_bits: table.hash_bits + 1,
                prev: unsafe { Some(Box::from_raw(table_raw)) },
            }));
            self.table.store(new_table, Ordering::Release);
            unsafe { &*new_table }
        } else {
            table
        };

        // Insert the new element into the top-level hash table
        for entry in table.entries.iter().cycle().skip(hash(id, table.hash_bits)) {
            let owner = entry.owner.load(Ordering::Relaxed);
            if owner == 0 {
                unsafe {
                    entry.owner.store(id, Ordering::Relaxed);
                    *entry.data.get() = Some(data);
                    return (*entry.data.get()).as_ref().unchecked_unwrap();
                }
            }
            if owner == id {
                // This can happen if create() inserted a value into this
                // ThreadLocal between our calls to get_fast() and insert(). We
                // just return the existing value and drop the newly-allocated
                // Box.
                unsafe {
                    return (*entry.data.get()).as_ref().unchecked_unwrap();
                }
            }
        }
        unreachable!();
    }

    /// Returns a mutable iterator over the local values of all threads.
    ///
    /// Since this call borrows the `ThreadLocal` mutably, this operation can
    /// be done safely---the mutable borrow statically guarantees no other
    /// threads are currently accessing their associated values.
    pub fn iter_mut(&mut self) -> IterMut<T> {
        let raw = RawIter {
            remaining: *self.lock.lock().unwrap(),
            index: 0,
            table: self.table.load(Ordering::Relaxed),
        };
        IterMut {
            raw: raw,
            marker: PhantomData,
        }
    }

    /// Removes all thread-specific values from the `ThreadLocal`, effectively
    /// reseting it to its original state.
    ///
    /// Since this call borrows the `ThreadLocal` mutably, this operation can
    /// be done safely---the mutable borrow statically guarantees no other
    /// threads are currently accessing their associated values.
    pub fn clear(&mut self) {
        *self = ThreadLocal::new();
    }
}

impl<T: ?Sized + Send> IntoIterator for ThreadLocal<T> {
    type Item = Box<T>;
    type IntoIter = IntoIter<T>;

    fn into_iter(self) -> IntoIter<T> {
        let raw = RawIter {
            remaining: *self.lock.lock().unwrap(),
            index: 0,
            table: self.table.load(Ordering::Relaxed),
        };
        IntoIter {
            raw: raw,
            _thread_local: self,
        }
    }
}

impl<'a, T: ?Sized + Send + 'a> IntoIterator for &'a mut ThreadLocal<T> {
    type Item = &'a mut Box<T>;
    type IntoIter = IterMut<'a, T>;

    fn into_iter(self) -> IterMut<'a, T> {
        self.iter_mut()
    }
}

impl<T: Send + Default> ThreadLocal<T> {
    /// Returns the element for the current thread, or creates a default one if
    /// it doesn't exist.
    pub fn get_default(&self) -> &T {
        self.get_or(|| Box::new(T::default()))
    }
}

impl<T: ?Sized + Send + fmt::Debug> fmt::Debug for ThreadLocal<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "ThreadLocal {{ local_data: {:?} }}", self.get())
    }
}

impl<T: ?Sized + Send + UnwindSafe> UnwindSafe for ThreadLocal<T> {}

struct RawIter<T: ?Sized + Send> {
    remaining: usize,
    index: usize,
    table: *const Table<T>,
}

impl<T: ?Sized + Send> RawIter<T> {
    fn next(&mut self) -> Option<*mut Option<Box<T>>> {
        if self.remaining == 0 {
            return None;
        }

        loop {
            let entries = unsafe { &(*self.table).entries[..] };
            while self.index < entries.len() {
                let val = entries[self.index].data.get();
                self.index += 1;
                if unsafe { (*val).is_some() } {
                    self.remaining -= 1;
                    return Some(val);
                }
            }
            self.index = 0;
            self.table = unsafe { &**(*self.table).prev.as_ref().unchecked_unwrap() };
        }
    }
}

/// Mutable iterator over the contents of a `ThreadLocal`.
pub struct IterMut<'a, T: ?Sized + Send + 'a> {
    raw: RawIter<T>,
    marker: PhantomData<&'a mut ThreadLocal<T>>,
}

impl<'a, T: ?Sized + Send + 'a> Iterator for IterMut<'a, T> {
    type Item = &'a mut Box<T>;

    fn next(&mut self) -> Option<&'a mut Box<T>> {
        self.raw.next().map(|x| unsafe { (*x).as_mut().unchecked_unwrap() })
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.raw.remaining, Some(self.raw.remaining))
    }
}

impl<'a, T: ?Sized + Send + 'a> ExactSizeIterator for IterMut<'a, T> {}

/// An iterator that moves out of a `ThreadLocal`.
pub struct IntoIter<T: ?Sized + Send> {
    raw: RawIter<T>,
    _thread_local: ThreadLocal<T>,
}

impl<T: ?Sized + Send> Iterator for IntoIter<T> {
    type Item = Box<T>;

    fn next(&mut self) -> Option<Box<T>> {
        self.raw.next().map(|x| unsafe { (*x).take().unchecked_unwrap() })
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.raw.remaining, Some(self.raw.remaining))
    }
}

impl<T: ?Sized + Send> ExactSizeIterator for IntoIter<T> {}

/// Wrapper around `ThreadLocal` which adds a fast path for a single thread.
///
/// This has the same API as `ThreadLocal`, but will register the first thread
/// that sets a value as its owner. All accesses by the owner will go through
/// a special fast path which is much faster than the normal `ThreadLocal` path.
pub struct CachedThreadLocal<T: ?Sized + Send> {
    owner: AtomicUsize,
    local: UnsafeCell<Option<Box<T>>>,
    global: ThreadLocal<T>,
}

// CachedThreadLocal is always Sync, even if T isn't
unsafe impl<T: ?Sized + Send> Sync for CachedThreadLocal<T> {}

impl<T: ?Sized + Send> Default for CachedThreadLocal<T> {
    fn default() -> CachedThreadLocal<T> {
        CachedThreadLocal::new()
    }
}

impl<T: ?Sized + Send> CachedThreadLocal<T> {
    /// Creates a new empty `CachedThreadLocal`.
    pub fn new() -> CachedThreadLocal<T> {
        CachedThreadLocal {
            owner: AtomicUsize::new(0),
            local: UnsafeCell::new(None),
            global: ThreadLocal::new(),
        }
    }

    /// Returns the element for the current thread, if it exists.
    pub fn get(&self) -> Option<&T> {
        let id = get_thread_id();
        let owner = self.owner.load(Ordering::Relaxed);
        if owner == id {
            return unsafe { Some((*self.local.get()).as_ref().unchecked_unwrap()) };
        }
        if owner == 0 {
            return None;
        }
        self.global.get_fast(id)
    }

    /// Returns the element for the current thread, or creates it if it doesn't
    /// exist.
    #[inline(always)]
    pub fn get_or<F>(&self, create: F) -> &T
        where F: FnOnce() -> Box<T>
    {
        unsafe { self.get_or_try(|| Ok::<Box<T>, ()>(create())).unchecked_unwrap_ok() }
    }

    /// Returns the element for the current thread, or creates it if it doesn't
    /// exist. If `create` fails, that error is returned and no element is
    /// added.
    pub fn get_or_try<F, E>(&self, create: F) -> Result<&T, E>
        where F: FnOnce() -> Result<Box<T>, E>
    {
        let id = get_thread_id();
        let owner = self.owner.load(Ordering::Relaxed);
        if owner == id {
            return Ok(unsafe { (*self.local.get()).as_ref().unchecked_unwrap() });
        }
        self.get_or_try_slow(id, owner, create)
    }

    #[cold]
    #[inline(never)]
    fn get_or_try_slow<F, E>(&self, id: usize, owner: usize, create: F) -> Result<&T, E>
        where F: FnOnce() -> Result<Box<T>, E>
    {
        if owner == 0 && self.owner.compare_and_swap(0, id, Ordering::Relaxed) == 0 {
            unsafe {
                (*self.local.get()) = Some(try!(create()));
                return Ok((*self.local.get()).as_ref().unchecked_unwrap());
            }
        }
        match self.global.get_fast(id) {
            Some(x) => Ok(x),
            None => Ok(self.global.insert(id, try!(create()), true)),
        }
    }

    /// Returns a mutable iterator over the local values of all threads.
    ///
    /// Since this call borrows the `ThreadLocal` mutably, this operation can
    /// be done safely---the mutable borrow statically guarantees no other
    /// threads are currently accessing their associated values.
    pub fn iter_mut(&mut self) -> CachedIterMut<T> {
        unsafe { (*self.local.get()).as_mut().into_iter().chain(self.global.iter_mut()) }
    }

    /// Removes all thread-specific values from the `ThreadLocal`, effectively
    /// reseting it to its original state.
    ///
    /// Since this call borrows the `ThreadLocal` mutably, this operation can
    /// be done safely---the mutable borrow statically guarantees no other
    /// threads are currently accessing their associated values.
    pub fn clear(&mut self) {
        *self = CachedThreadLocal::new();
    }
}

impl<T: ?Sized + Send> IntoIterator for CachedThreadLocal<T> {
    type Item = Box<T>;
    type IntoIter = CachedIntoIter<T>;

    fn into_iter(self) -> CachedIntoIter<T> {
        unsafe { (*self.local.get()).take().into_iter().chain(self.global.into_iter()) }
    }
}

impl<'a, T: ?Sized + Send + 'a> IntoIterator for &'a mut CachedThreadLocal<T> {
    type Item = &'a mut Box<T>;
    type IntoIter = CachedIterMut<'a, T>;

    fn into_iter(self) -> CachedIterMut<'a, T> {
        self.iter_mut()
    }
}

impl<T: Send + Default> CachedThreadLocal<T> {
    /// Returns the element for the current thread, or creates a default one if
    /// it doesn't exist.
    pub fn get_default(&self) -> &T {
        self.get_or(|| Box::new(T::default()))
    }
}

impl<T: ?Sized + Send + fmt::Debug> fmt::Debug for CachedThreadLocal<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "ThreadLocal {{ local_data: {:?} }}", self.get())
    }
}

/// Mutable iterator over the contents of a `CachedThreadLocal`.
pub type CachedIterMut<'a, T> = Chain<OptionIter<&'a mut Box<T>>, IterMut<'a, T>>;

/// An iterator that moves out of a `CachedThreadLocal`.
pub type CachedIntoIter<T> = Chain<OptionIter<Box<T>>, IntoIter<T>>;

impl<T: ?Sized + Send + UnwindSafe> UnwindSafe for CachedThreadLocal<T> {}

#[cfg(test)]
mod tests {
    use std::cell::RefCell;
    use std::sync::Arc;
    use std::sync::atomic::AtomicUsize;
    use std::sync::atomic::Ordering::Relaxed;
    use std::thread;
    use super::{ThreadLocal, CachedThreadLocal};

    fn make_create() -> Arc<Fn() -> Box<usize> + Send + Sync> {
        let count = AtomicUsize::new(0);
        Arc::new(move || Box::new(count.fetch_add(1, Relaxed)))
    }

    #[test]
    fn same_thread() {
        let create = make_create();
        let mut tls = ThreadLocal::new();
        assert_eq!(None, tls.get());
        assert_eq!("ThreadLocal { local_data: None }", format!("{:?}", &tls));
        assert_eq!(0, *tls.get_or(|| create()));
        assert_eq!(Some(&0), tls.get());
        assert_eq!(0, *tls.get_or(|| create()));
        assert_eq!(Some(&0), tls.get());
        assert_eq!(0, *tls.get_or(|| create()));
        assert_eq!(Some(&0), tls.get());
        assert_eq!("ThreadLocal { local_data: Some(0) }", format!("{:?}", &tls));
        tls.clear();
        assert_eq!(None, tls.get());
    }

    #[test]
    fn same_thread_cached() {
        let create = make_create();
        let mut tls = CachedThreadLocal::new();
        assert_eq!(None, tls.get());
        assert_eq!("ThreadLocal { local_data: None }", format!("{:?}", &tls));
        assert_eq!(0, *tls.get_or(|| create()));
        assert_eq!(Some(&0), tls.get());
        assert_eq!(0, *tls.get_or(|| create()));
        assert_eq!(Some(&0), tls.get());
        assert_eq!(0, *tls.get_or(|| create()));
        assert_eq!(Some(&0), tls.get());
        assert_eq!("ThreadLocal { local_data: Some(0) }", format!("{:?}", &tls));
        tls.clear();
        assert_eq!(None, tls.get());
    }

    #[test]
    fn different_thread() {
        let create = make_create();
        let tls = Arc::new(ThreadLocal::new());
        assert_eq!(None, tls.get());
        assert_eq!(0, *tls.get_or(|| create()));
        assert_eq!(Some(&0), tls.get());

        let tls2 = tls.clone();
        let create2 = create.clone();
        thread::spawn(move || {
                assert_eq!(None, tls2.get());
                assert_eq!(1, *tls2.get_or(|| create2()));
                assert_eq!(Some(&1), tls2.get());
            })
            .join()
            .unwrap();

        assert_eq!(Some(&0), tls.get());
        assert_eq!(0, *tls.get_or(|| create()));
    }

    #[test]
    fn different_thread_cached() {
        let create = make_create();
        let tls = Arc::new(CachedThreadLocal::new());
        assert_eq!(None, tls.get());
        assert_eq!(0, *tls.get_or(|| create()));
        assert_eq!(Some(&0), tls.get());

        let tls2 = tls.clone();
        let create2 = create.clone();
        thread::spawn(move || {
                assert_eq!(None, tls2.get());
                assert_eq!(1, *tls2.get_or(|| create2()));
                assert_eq!(Some(&1), tls2.get());
            })
            .join()
            .unwrap();

        assert_eq!(Some(&0), tls.get());
        assert_eq!(0, *tls.get_or(|| create()));
    }

    #[test]
    fn iter() {
        let tls = Arc::new(ThreadLocal::new());
        tls.get_or(|| Box::new(1));

        let tls2 = tls.clone();
        thread::spawn(move || {
                tls2.get_or(|| Box::new(2));
                let tls3 = tls2.clone();
                thread::spawn(move || {
                        tls3.get_or(|| Box::new(3));
                    })
                    .join()
                    .unwrap();
            })
            .join()
            .unwrap();

        let mut tls = Arc::try_unwrap(tls).unwrap();
        let mut v = tls.iter_mut().map(|x| **x).collect::<Vec<i32>>();
        v.sort();
        assert_eq!(vec![1, 2, 3], v);
        let mut v = tls.into_iter().map(|x| *x).collect::<Vec<i32>>();
        v.sort();
        assert_eq!(vec![1, 2, 3], v);
    }

    #[test]
    fn iter_cached() {
        let tls = Arc::new(CachedThreadLocal::new());
        tls.get_or(|| Box::new(1));

        let tls2 = tls.clone();
        thread::spawn(move || {
                tls2.get_or(|| Box::new(2));
                let tls3 = tls2.clone();
                thread::spawn(move || {
                        tls3.get_or(|| Box::new(3));
                    })
                    .join()
                    .unwrap();
            })
            .join()
            .unwrap();

        let mut tls = Arc::try_unwrap(tls).unwrap();
        let mut v = tls.iter_mut().map(|x| **x).collect::<Vec<i32>>();
        v.sort();
        assert_eq!(vec![1, 2, 3], v);
        let mut v = tls.into_iter().map(|x| *x).collect::<Vec<i32>>();
        v.sort();
        assert_eq!(vec![1, 2, 3], v);
    }

    #[test]
    fn is_sync() {
        fn foo<T: Sync>() {}
        foo::<ThreadLocal<String>>();
        foo::<ThreadLocal<RefCell<String>>>();
        foo::<CachedThreadLocal<String>>();
        foo::<CachedThreadLocal<RefCell<String>>>();
    }
}
