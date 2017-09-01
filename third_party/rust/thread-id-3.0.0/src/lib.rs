// Thread-ID -- Get a unique thread ID
// Copyright 2016 Ruud van Asseldonk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// A copy of the License has been included in the root of the repository.

//! Thread-ID: get a unique ID for the current thread.
//!
//! For diagnostics and debugging it can often be useful to get an ID that is
//! different for every thread. This crate provides that functionality.
//!
//! # Example
//!
//! ```
//! use std::thread;
//! use thread_id;
//!
//! thread::spawn(move || {
//!     println!("spawned thread has id {}", thread_id::get());
//! });
//!
//! println!("main thread has id {}", thread_id::get());
//! ```

#![warn(missing_docs)]

#[cfg(unix)]
extern crate libc;

#[cfg(windows)]
extern crate kernel32;

/// Returns a number that is unique to the calling thread.
///
/// Calling this function twice from the same thread will return the same
/// number. Calling this function from a different thread will return a
/// different number.
#[inline]
pub fn get() -> usize {
    get_internal()
}

#[cfg(unix)]
#[inline]
fn get_internal() -> usize {
    unsafe { libc::pthread_self() as usize }
}

#[cfg(windows)]
#[inline]
fn get_internal() -> usize {
    unsafe { kernel32::GetCurrentThreadId() as usize }
}

#[test]
fn distinct_threads_have_distinct_ids() {
    use std::sync::mpsc;
    use std::thread;

    let (tx, rx) = mpsc::channel();
    thread::spawn(move || tx.send(::get()).unwrap()).join().unwrap();

    let main_tid = ::get();
    let other_tid = rx.recv().unwrap();
    assert!(main_tid != other_tid);
}
