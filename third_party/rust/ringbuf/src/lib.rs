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
//!
//! ## Message transfer
//!
//! This is more complicated example of transfering text message between threads.
//!
//! ```rust
//! # extern crate ringbuf;
//! use std::io::Read;
//! use std::thread;
//! use std::time::Duration;
//!
//! use ringbuf::RingBuffer;
//!
//! # fn main() {
//! let buf = RingBuffer::<u8>::new(10);
//! let (mut prod, mut cons) = buf.split();
//!
//! let smsg = "The quick brown fox jumps over the lazy dog";
//!
//! let pjh = thread::spawn(move || {
//!     println!("-> sending message: '{}'", smsg);
//!
//!     let zero = [0 as u8];
//!     let mut bytes = smsg.as_bytes().chain(&zero[..]);
//!     loop {
//!         if prod.is_full() {
//!             println!("-> buffer is full, waiting");
//!             thread::sleep(Duration::from_millis(1));
//!         } else {
//!             let n = prod.read_from(&mut bytes, None).unwrap();
//!             if n == 0 {
//!                 break;
//!             }
//!             println!("-> {} bytes sent", n);
//!         }
//!     }
//!
//!     println!("-> message sent");
//! });
//!
//! let cjh = thread::spawn(move || {
//!     println!("<- receiving message");
//!
//!     let mut bytes = Vec::<u8>::new();
//!     loop {
//!         if cons.is_empty() {
//!             if bytes.ends_with(&[0]) {
//!                 break;
//!             } else {
//!                 println!("<- buffer is empty, waiting");
//!                 thread::sleep(Duration::from_millis(1));
//!             }
//!         } else {
//!             let n = cons.write_into(&mut bytes, None).unwrap();
//!             println!("<- {} bytes received", n);
//!         }
//!     }
//!
//!     assert_eq!(bytes.pop().unwrap(), 0);
//!     let msg = String::from_utf8(bytes).unwrap();
//!     println!("<- message received: '{}'", msg);
//!
//!     msg
//! });
//!
//! pjh.join().unwrap();
//! let rmsg = cjh.join().unwrap();
//!
//! assert_eq!(smsg, rmsg);
//! # }
//! ```
//!

#![cfg_attr(feature = "benchmark", feature(test))]

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
