/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! **Golden Gate** 🌉 is a crate for bridging Desktop Sync to our suite of
//! Rust sync and storage components. It connects Sync's `BridgedEngine` class
//! to the Rust `BridgedEngine` trait via the `mozIBridgedSyncEngine` XPCOM
//! interface.
//!
//! Due to limitations in implementing XPCOM interfaces for generic types,
//! Golden Gate doesn't implement `mozIBridgedSyncEngine` directly. Instead,
//! it provides helpers, called "ferries", for passing Sync records between
//! JavaScript and Rust. The ferries also handle threading and type
//! conversions.
//!
//! Here's a step-by-step guide for adding a new Rust Sync engine to Firefox.
//!
//! ## Step 1: Create your (XPCOM) bridge
//!
//! In your consuming crate, define a type for your `mozIBridgedSyncEngine`
//! implementation. We'll call this type the **brige**. The bridge is
//! responsible for exposing your Sync engine to XPIDL [^1], in a way that lets
//! JavaScript call it.
//!
//! For your bridge type, you'll need to implement an xpcom interface with the
//! `#[xpcom(implement(mozIBridgedSyncEngine), nonatomic)]` attribute then
//! define `xpcom_method!()` stubs for the `mozIBridgedSyncEngine` methods.  For
//! more details about implementing XPCOM methods in Rust, check out the docs in
//! `xpcom/rust/xpcom/src/method.rs`.
//!
//! You'll also need to add an entry for your bridge type to `components.conf`,
//! and define C++ and Rust constructors for it, so that JavaScript code can
//! create instances of it. Check out `NS_NewWebExtStorage` (and, in C++,
//! `mozilla::extensions::storageapi::NewWebExtStorage`) and
//! `NS_NewSyncedBookmarksMerger` (`mozilla::places::NewSyncedBookmarksMerger`
//! in C++) for how to do this.
//!
//! [^1]: You can think of XPIDL as a souped-up C FFI, with richer types and a
//! degree of type safety.
//!
//! ## Step 2: Add a background task queue to your bridge
//!
//! A task queue lets your engine do I/O, merging, and other syncing tasks on a
//! background thread pool. This is important because database reads and writes
//! can take an unpredictable amount of time. Doing these on the main thread can
//! cause jank, and, in the worst case, lock up the browser UI for seconds at a
//! time.
//!
//! The `moz_task` crate provides a `create_background_task_queue` function to
//! do this. Once you have a queue, you can use it to call into your Rust
//! engine. Golden Gate takes care of ferrying arguments back and forth across
//! the thread boundary.
//!
//! Since it's a queue, ferries arrive in the order they're scheduled, so
//! your engine's `store_incoming` method will always be called before `apply`,
//! which is likewise called before `set_uploaded`. The thread manager scales
//! the pool for you; you don't need to create or manage your own threads.
//!
//! ## Step 3: Create your Rust engine
//!
//! Next, you'll need to implement the Rust side of the bridge. This is a type
//! that implements the `BridgedEngine` trait.
//!
//! Bridged engines handle storing incoming Sync records, merging changes,
//! resolving conflicts, and fetching outgoing records for upload. Under the
//! hood, your engine will hold either a database connection directly, or
//! another object that does.
//!
//! Although outside the scope of Golden Gate, your engine will also likely
//! expose a data storage API, for fetching, updating, and deleting items
//! locally. Golden Gate provides the syncing layer on top of this local store.
//!
//! A `BridgedEngine` itself doesn't need to be `Send` or `Sync`, but the
//! ferries require both, since they're calling into your bridge on the
//! background task queue.
//!
//! In practice, this means your bridge will need to hold a thread-safe owned
//! reference to the engine, via `Arc<Mutex<BridgedEngine>>`. In fact, this
//! pattern is so common that Golden Gate implements `BridgedEngine` for any
//! `Mutex<BridgedEngine>`, which automatically locks the mutex before calling
//! into the engine.
//!
//! ## Step 4: Connect the bridge to the JavaScript and Rust sides
//!
//! On the JavaScript side, you'll need to subclass Sync's `BridgedEngine`
//! class, and give it a handle to your XPCOM bridge. The base class has all the
//! machinery for hooking up any `mozIBridgedSyncEngine` implementation so that
//! Sync can drive it.
//!
//! On the Rust side, each `mozIBridgedSyncEngine` method should create a
//! Golden Gate ferry, and dispatch it to the background task queue. The
//! ferries correspond to the method names. For example, `ensureCurrentSyncId`
//! should create a `Ferry::ensure_current_sync_id(...)`; `storeIncoming`, a
//! `Ferry::store_incoming(...)`; and so on. This is mostly boilerplate.
//!
//! And that's it! Each ferry will, in turn, call into your Rust
//! `BridgedEngine`, and send the results back to JavaScript.
//!
//! For an example of how all this works, including exposing a storage (not
//! just syncing!) API to JS via XPIDL, check out `webext_storage::Bridge` for
//! the `storage.sync` API!

#[macro_use]
extern crate cstr;

pub mod error;
mod ferry;
pub mod log;
pub mod task;

pub use crate::log::LogSink;
pub use error::{Error, Result};
// Re-export items from `interrupt-support` and `sync15-traits`, so that
// consumers of `golden_gate` don't have to depend on them.
pub use interrupt_support::{Interrupted, Interruptee};
pub use sync15_traits::{ApplyResults, BridgedEngine, Guid, IncomingEnvelope, OutgoingEnvelope};
pub use task::{ApplyTask, FerryTask};
