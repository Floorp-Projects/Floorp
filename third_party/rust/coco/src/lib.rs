//! Concurrent collections.
//!
//! This crate offers several collections that are designed for performance in multithreaded
//! contexts. They can be freely shared among multiple threads running in parallel, and
//! concurrently modified without the overhead of locking.
//!
//! <!--
//! Some of these data structures are lock-free. Others are not strictly speaking lock-free, but
//! still scale well with respect to the number of threads accessing them.
//! -->
//!
//! # Collections
//!
//! The following collections are available:
//!
//! * [`Stack`]: A lock-free stack.
//! * [`deque`]: A lock-free work-stealing deque.
//!
//! # Which collection should you use?
//!
//! ### Use a [`Stack`] when:
//!
//! * You want a simple shared collection where objects can be insert and removed.
//! * You want to avoid performance degradation due to locking.
//! * You want the first-in first-out order of elements.
//!
//! ### Use a [`deque`] when:
//!
//! * You want one thread inserting and removing objects, and multiple threads just removing them.
//! * You don't care about the order of elements.
//!
//! # Garbage collection
//!
//! An interesting problem concurrent collections deal with comes from the remove operation.
//! Suppose that a thread removes an element from a lock-free map, while another thread is reading
//! that same element at the same time. The first thread must wait until the second thread stops
//! reading the element. Only then it is safe to destruct it.
//!
//! Programming languages that come with garbage collectors solve this problem trivially. The
//! garbage collector will destruct the removed element when no thread can hold a reference to it
//! anymore.
//!
//! This crate implements a basic garbage collection mechanism, which is based on epochs (see the
//! `epoch` module). When an element gets removed from a concurrent collection, it is inserted into
//! a pile of garbage and marked with the current epoch. Every time a thread accesses a collection,
//! it checks the current epoch, attempts to increment it, and destructs some garbage that became
//! so old that no thread can be referencing it anymore.
//!
//! That is the general mechanism behind garbage collection, but the details are a bit more
//! complicated. Anyhow, garbage collection is designed to be fully automatic and something users
//! of concurrent collections don't have to worry about.
//!
//! [`Stack`]: stack/struct.Stack.html
//! [`deque`]: deque/fn.new.html

extern crate either;

#[macro_use(defer)]
extern crate scopeguard;

pub mod deque;
pub mod epoch;
pub mod stack;

pub use stack::Stack;
