// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

//! This library exposes a low-level API for creating your own efficient
//! synchronization primitives.
//!
//! # The parking lot
//!
//! To keep synchronization primitives small, all thread queuing and suspending
//! functionality is offloaded to the *parking lot*. The idea behind this is
//! based on the Webkit [`WTF::ParkingLot`]
//! (https://webkit.org/blog/6161/locking-in-webkit/) class, which essentially
//! consists of a hash table mapping of lock addresses to queues of parked
//! (sleeping) threads. The Webkit parking lot was itself inspired by Linux
//! [futexes](http://man7.org/linux/man-pages/man2/futex.2.html), but it is more
//! powerful since it allows invoking callbacks while holding a queue lock.
//!
//! There are two main operations that can be performed on the parking lot:
//!  - *Parking* refers to suspending the thread while simultaneously enqueuing it
//! on a queue keyed by some address.
//! - *Unparking* refers to dequeuing a thread from a queue keyed by some address
//! and resuming it.
//!
//! See the documentation of the individual functions for more details.
//!
//! # Building custom synchronization primitives
//!
//! Building custom synchronization primitives is very simple since the parking
//! lot takes care of all the hard parts for you. A simple example for a
//! custom primitive would be to integrate a `Mutex` inside another data type.
//! Since a mutex only requires 2 bits, it can share space with other data.
//! For example, one could create an `ArcMutex` type that combines the atomic
//! reference count and the two mutex bits in the same atomic word.

#![warn(missing_docs)]
#![cfg_attr(feature = "nightly", feature(const_fn))]
#![cfg_attr(all(feature = "nightly", target_os = "linux"), feature(integer_atomics))]
#![cfg_attr(feature = "nightly", feature(asm))]

extern crate smallvec;
extern crate rand;

#[cfg(unix)]
extern crate libc;

#[cfg(windows)]
extern crate winapi;
#[cfg(windows)]
extern crate kernel32;

#[cfg(all(feature = "nightly", target_os = "linux"))]
#[path = "thread_parker/linux.rs"]
mod thread_parker;
#[cfg(all(unix, not(all(feature = "nightly", target_os = "linux"))))]
#[path = "thread_parker/unix.rs"]
mod thread_parker;
#[cfg(windows)]
#[path = "thread_parker/windows/mod.rs"]
mod thread_parker;
#[cfg(not(any(windows, unix)))]
#[path = "thread_parker/generic.rs"]
mod thread_parker;

#[cfg(not(feature = "nightly"))]
mod stable;

mod util;
mod spinwait;
mod word_lock;
mod parking_lot;

pub use parking_lot::{ParkResult, UnparkResult, RequeueOp, UnparkToken, ParkToken, FilterOp};
pub use parking_lot::{DEFAULT_UNPARK_TOKEN, DEFAULT_PARK_TOKEN};
pub use parking_lot::{park, unpark_one, unpark_all, unpark_requeue, unpark_filter};
pub use spinwait::SpinWait;
