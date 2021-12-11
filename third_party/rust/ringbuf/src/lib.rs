//! Lock-free single-producer single-consumer (SPSC) FIFO ring buffer with direct access to inner data.
//!
//! # Overview
//!
//! `RingBuffer` is the initial structure representing ring buffer itself.
//! Ring buffer can be splitted into pair of `Producer` and `Consumer`.
//!
//! `Producer` and `Consumer` are used to append/remove elements to/from the ring buffer accordingly. They can be safely transfered between threads.
//! Operations with `Producer` and `Consumer` are lock-free - they're succeded or failed immediately without blocking or waiting.
//!
//! Elements can be effectively appended/removed one by one or many at once.
//! Also data could be loaded/stored directly into/from [`Read`]/[`Write`] instances.
//! And finally, there are `unsafe` methods allowing thread-safe direct access in place to the inner memory being appended/removed.
//!
//! [`Read`]: https://doc.rust-lang.org/std/io/trait.Read.html
//! [`Write`]: https://doc.rust-lang.org/std/io/trait.Write.html
//!
//! When building with nightly toolchain it is possible to run benchmarks via `cargo bench --features benchmark`.
//!
//! # Examples
//!
//! ## Simple example
//!
//! ```rust
//! # extern crate ringbuf;
//! use ringbuf::RingBuffer;
//! # fn main() {
//! let rb = RingBuffer::<i32>::new(2);
//! let (mut prod, mut cons) = rb.split();
//!
//! prod.push(0).unwrap();
//! prod.push(1).unwrap();
//! assert_eq!(prod.push(2), Err(2));
//!
//! assert_eq!(cons.pop().unwrap(), 0);
//!
//! prod.push(2).unwrap();
//!
//! assert_eq!(cons.pop().unwrap(), 1);
//! assert_eq!(cons.pop().unwrap(), 2);
//! assert_eq!(cons.pop(), None);
//! # }
//! ```

#![no_std]
#![cfg_attr(feature = "benchmark", feature(test))]

extern crate alloc;
#[cfg(feature = "std")]
extern crate std;

#[cfg(feature = "benchmark")]
extern crate test;

#[cfg(feature = "benchmark")]
mod benchmark;

#[cfg(test)]
mod tests;

mod consumer;
mod producer;
mod ring_buffer;

pub use consumer::*;
pub use producer::*;
pub use ring_buffer::*;
