// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

//! This library provides implementations of `Mutex`, `RwLock`, `Condvar` and
//! `Once` that are smaller, faster and more flexible than those in the Rust
//! standard library. It also provides a `ReentrantMutex` type.

#![warn(missing_docs)]
#![cfg_attr(feature = "nightly", feature(const_fn))]
#![cfg_attr(feature = "nightly", feature(integer_atomics))]
#![cfg_attr(feature = "nightly", feature(asm))]

#[cfg(feature = "owning_ref")]
extern crate owning_ref;

#[cfg(not(target_os = "emscripten"))]
extern crate thread_id;

extern crate parking_lot_core;

#[cfg(not(feature = "nightly"))]
mod stable;

mod util;
mod elision;
mod raw_mutex;
mod raw_remutex;
mod raw_rwlock;
mod condvar;
mod mutex;
mod remutex;
mod rwlock;
mod once;

pub use once::{Once, ONCE_INIT, OnceState};
pub use mutex::{Mutex, MutexGuard};
pub use remutex::{ReentrantMutex, ReentrantMutexGuard};
pub use condvar::{Condvar, WaitTimeoutResult};
pub use rwlock::{RwLock, RwLockReadGuard, RwLockWriteGuard};

#[cfg(feature = "owning_ref")]
use owning_ref::OwningRef;

/// Typedef of an owning reference that uses a `MutexGuard` as the owner.
#[cfg(feature = "owning_ref")]
pub type MutexGuardRef<'a, T, U = T> = OwningRef<MutexGuard<'a, T>, U>;

/// Typedef of an owning reference that uses a `ReentrantMutexGuard` as the owner.
#[cfg(feature = "owning_ref")]
pub type ReentrantMutexGuardRef<'a, T, U = T> = OwningRef<ReentrantMutexGuard<'a, T>, U>;

/// Typedef of an owning reference that uses a `RwLockReadGuard` as the owner.
#[cfg(feature = "owning_ref")]
pub type RwLockReadGuardRef<'a, T, U = T> = OwningRef<RwLockReadGuard<'a, T>, U>;

/// Typedef of an owning reference that uses a `RwLockWriteGuard` as the owner.
#[cfg(feature = "owning_ref")]
pub type RwLockWriteGuardRef<'a, T, U = T> = OwningRef<RwLockWriteGuard<'a, T>, U>;
