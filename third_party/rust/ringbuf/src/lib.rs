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
//! # Examples
//!
//! ## Simple example
//!
//! ```rust
//! # extern crate ringbuf;
//! use ringbuf::{RingBuffer, PushError, PopError};
//! # fn main() {
//! let rb = RingBuffer::<i32>::new(2);
//! let (mut prod, mut cons) = rb.split();
//! 
//! prod.push(0).unwrap();
//! prod.push(1).unwrap();
//! assert_eq!(prod.push(2), Err(PushError::Full(2)));
//! 
//! assert_eq!(cons.pop().unwrap(), 0);
//! 
//! prod.push(2).unwrap();
//! 
//! assert_eq!(cons.pop().unwrap(), 1);
//! assert_eq!(cons.pop().unwrap(), 2);
//! assert_eq!(cons.pop(), Err(PopError::Empty));
//! # }
//! ```
//!
//! ## Message transfer
//!
//! This is more complicated example of transfering text message between threads.
//!
//! ```rust
//! # extern crate ringbuf;
//! use std::io::{Read};
//! use std::thread;
//! use std::time::{Duration};
//! 
//! use ringbuf::{RingBuffer};
//! # fn main() {
//! let rb = RingBuffer::<u8>::new(10);
//! let (mut prod, mut cons) = rb.split();
//! 
//! let smsg = "The quick brown fox jumps over the lazy dog";
//! 
//! let pjh = thread::spawn(move || {
//!     println!("-> sending message: '{}'", smsg);
//! 
//!     let zero = [0 as u8];
//!     let mut bytes = smsg.as_bytes().chain(&zero[..]);
//!     loop {
//!         match prod.read_from(&mut bytes, None) {
//!             Ok(n) => {
//!                 if n == 0 {
//!                     break;
//!                 }
//!                 println!("-> {} bytes sent", n);
//!             },
//!             Err(_) => {
//!                 println!("-> buffer is full, waiting");
//!                 thread::sleep(Duration::from_millis(1));
//!             },
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
//!         match cons.write_into(&mut bytes, None) {
//!             Ok(n) => println!("<- {} bytes received", n),
//!             Err(_) => {
//!                 if bytes.ends_with(&[0]) {
//!                     break;
//!                 } else {
//!                     println!("<- buffer is empty, waiting");
//!                     thread::sleep(Duration::from_millis(1));
//!                 }
//!             },
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


#![cfg_attr(rustc_nightly, feature(test))]

#[cfg(test)]
#[cfg(rustc_nightly)]
extern crate test;

#[cfg(test)]
mod tests;

#[cfg(test)]
#[cfg(rustc_nightly)]
mod benchmarks;


use std::mem;
use std::cell::{UnsafeCell};
use std::sync::{Arc, atomic::{Ordering, AtomicUsize}};
use std::io::{self, Read, Write};


#[derive(Debug, PartialEq, Eq)]
/// `Producer::push` error.
pub enum PushError<T: Sized> {
    /// Cannot push: ring buffer is full.
    Full(T),
}

#[derive(Debug, PartialEq, Eq)]
/// `Consumer::pop` error.
pub enum PopError {
    /// Cannot pop: ring buffer is empty.
    Empty,
}

#[derive(Debug, PartialEq, Eq)]
/// `Producer::push_slice` error.
pub enum PushSliceError {
    /// Cannot push: ring buffer is full.
    Full,
}

#[derive(Debug, PartialEq, Eq)]
/// `Consumer::pop_slice` error.
pub enum PopSliceError {
    /// Cannot pop: ring buffer is empty.
    Empty,
}

#[derive(Debug, PartialEq, Eq)]
/// `{Producer, Consumer}::move_slice` error.
pub enum MoveSliceError {
    /// Cannot pop: ring buffer is empty.
    Empty,
    /// Cannot push: ring buffer is full.
    Full,
}

#[derive(Debug, PartialEq, Eq)]
/// `Producer::push_access` error.
pub enum PushAccessError {
    /// Cannot push: ring buffer is full.
    Full,
    /// User function returned invalid length.
    BadLen,
}

#[derive(Debug, PartialEq, Eq)]
/// `Consumer::pop_access` error.
pub enum PopAccessError {
    /// Cannot pop: ring buffer is empty.
    Empty,
    /// User function returned invalid length.
    BadLen,
}

#[derive(Debug)]
/// `Producer::read_from` error.
pub enum ReadFromError {
    /// Error returned by [`Read`](https://doc.rust-lang.org/std/io/trait.Read.html).
    Read(io::Error),
    /// Ring buffer is full.
    RbFull,
}

#[derive(Debug)]
/// `Consumer::write_into` error.
pub enum WriteIntoError {
    /// Error returned by [`Write`](https://doc.rust-lang.org/std/io/trait.Write.html).
    Write(io::Error),
    /// Ring buffer is empty.
    RbEmpty,
}

struct SharedVec<T: Sized> {
    cell: UnsafeCell<Vec<T>>,
}

unsafe impl<T: Sized> Sync for SharedVec<T> {}

impl<T: Sized> SharedVec<T> {
    fn new(data: Vec<T>) -> Self {
        Self { cell: UnsafeCell::new(data) }
    }
    unsafe fn get_ref(&self) -> &Vec<T> {
        self.cell.get().as_ref().unwrap()
    }
    unsafe fn get_mut(&self) -> &mut Vec<T> {
        self.cell.get().as_mut().unwrap()
    }
}

/// Ring buffer itself.
pub struct RingBuffer<T: Sized> {
    data: SharedVec<T>,
    head: AtomicUsize,
    tail: AtomicUsize,
}

/// Producer part of ring buffer.
pub struct Producer<T> {
    rb: Arc<RingBuffer<T>>,
}

/// Consumer part of ring buffer.
pub struct Consumer<T> {
    rb: Arc<RingBuffer<T>>,
}

impl<T: Sized> RingBuffer<T> {
    /// Creates a new instance of a ring buffer.
    pub fn new(capacity: usize) -> Self {
        let vec_cap = capacity + 1;
        let mut data = Vec::with_capacity(vec_cap);
        unsafe { data.set_len(vec_cap); }
        Self {
            data: SharedVec::new(data),
            head: AtomicUsize::new(0),
            tail: AtomicUsize::new(0),
        }
    }

    /// Splits ring buffer into producer and consumer.
    pub fn split(self) -> (Producer<T>, Consumer<T>) {
        let arc = Arc::new(self);
        (
            Producer { rb: arc.clone() },
            Consumer { rb: arc },
        )
    }

    /// Returns capacity of the ring buffer.
    pub fn capacity(&self) -> usize {
        unsafe { self.data.get_ref() }.len() - 1
    }

    /// Checks if the ring buffer is empty.
    pub fn is_empty(&self) -> bool {
        let head = self.head.load(Ordering::SeqCst);
        let tail = self.tail.load(Ordering::SeqCst);
        head == tail
    }

    /// Checks if the ring buffer is full.
    pub fn is_full(&self) -> bool {
        let head = self.head.load(Ordering::SeqCst);
        let tail = self.tail.load(Ordering::SeqCst);
        (tail + 1) % (self.capacity() + 1) == head
    }
}

impl<T: Sized> Drop for RingBuffer<T> {
    fn drop(&mut self) {
        let data = unsafe { self.data.get_mut() };

        let head = self.head.load(Ordering::SeqCst);
        let tail = self.tail.load(Ordering::SeqCst);
        let len = data.len();
        
        let slices = if head <= tail {
            (head..tail, 0..0)
        } else {
            (head..len, 0..tail)
        };

        let drop = |elem_ref: &mut T| {
            mem::drop(mem::replace(elem_ref, unsafe { mem::uninitialized() }));
        };
        for elem in data[slices.0].iter_mut() {
            drop(elem);
        }
        for elem in data[slices.1].iter_mut() {
            drop(elem);
        }

        unsafe { data.set_len(0); }
    }
}

impl<T: Sized> Producer<T> {
    /// Returns capacity of the ring buffer.
    pub fn capacity(&self) -> usize {
        self.rb.capacity()
    }

    /// Checks if the ring buffer is empty.
    pub fn is_empty(&self) -> bool {
        self.rb.is_empty()
    }

    /// Checks if the ring buffer is full.
    pub fn is_full(&self) -> bool {
        self.rb.is_full()
    }
    
    /// Appends an element to the ring buffer.
    /// On failure returns an error containing the element that hasn't beed appended.
    pub fn push(&mut self, elem: T) -> Result<(), PushError<T>> {
        let mut elem_opt = Some(elem);
        match unsafe { self.push_access(|slice, _| {
            mem::forget(mem::replace(&mut slice[0], elem_opt.take().unwrap()));
            Ok((1, ()))
        }) } {
            Ok(res) => match res {
                Ok((n, ())) => {
                    debug_assert_eq!(n, 1);
                    Ok(())
                },
                Err(()) => unreachable!(),
            },
            Err(e) => match e {
                PushAccessError::Full => Err(PushError::Full(elem_opt.unwrap())),
                PushAccessError::BadLen => unreachable!(),
            }
        }
    }
}

impl<T: Sized + Copy> Producer<T> {
    /// Appends elements from slice to the ring buffer.
    /// Elements should be [`Copy`](https://doc.rust-lang.org/std/marker/trait.Copy.html).
    ///
    /// On success returns count of elements been appended to the ring buffer.
    pub fn push_slice(&mut self, elems: &[T]) -> Result<usize, PushSliceError> {
        let push_fn = |left: &mut [T], right: &mut [T]| {
            Ok((if elems.len() < left.len() {
                left[0..elems.len()].copy_from_slice(elems);
                elems.len()
            } else {
                left.copy_from_slice(&elems[0..left.len()]);
                if elems.len() < left.len() + right.len() {
                    right[0..(elems.len() - left.len())]
                        .copy_from_slice(&elems[left.len()..elems.len()]);
                    elems.len()
                } else {
                    right.copy_from_slice(&elems[left.len()..(left.len() + right.len())]);
                    left.len() + right.len()
                }
            }, ()))
        };
        match unsafe { self.push_access(push_fn) } {
            Ok(res) => match res {
                Ok((n, ())) => {
                    Ok(n)
                },
                Err(()) => unreachable!(),
            },
            Err(e) => match e {
                PushAccessError::Full => Err(PushSliceError::Full),
                PushAccessError::BadLen => unreachable!(),
            }
        }
    }

    /// Removes at most `count` elements from the `Consumer` of the ring buffer
    /// and appends them to the `Producer` of the another one.
    /// If `count` is `None` then as much as possible elements will be moved.
    ///
    /// Elements should be [`Copy`](https://doc.rust-lang.org/std/marker/trait.Copy.html).
    ///
    /// On success returns count of elements been moved.
    pub fn move_slice(&mut self, other: &mut Consumer<T>, count: Option<usize>)
    -> Result<usize, MoveSliceError> {
        let move_fn = |left: &mut [T], right: &mut [T]|
        -> Result<(usize, ()), PopSliceError> {
            let (left, right) = match count {
                Some(c) => {
                    if c < left.len() {
                        (&mut left[0..c], &mut right[0..0])
                    } else if c < left.len() + right.len() {
                        let l = c - left.len();
                        (left, &mut right[0..l])
                    } else {
                        (left, right)
                    }
                },
                None => (left, right)
            };
            other.pop_slice(left).and_then(|n| {
                if n == left.len() {
                    other.pop_slice(right).and_then(|m| {
                        Ok((n + m, ()))
                    }).or_else(|e| {
                        match e {
                            PopSliceError::Empty => Ok((n, ())),
                        }
                    })
                } else {
                    debug_assert!(n < left.len());
                    Ok((n, ()))
                }
            })
        };
        match unsafe { self.push_access(move_fn) } {
            Ok(res) => match res {
                Ok((n, ())) => Ok(n),
                Err(e) => match e {
                    PopSliceError::Empty => Err(MoveSliceError::Empty),
                },
            },
            Err(e) => match e {
                PushAccessError::Full => Err(MoveSliceError::Full),
                PushAccessError::BadLen => unreachable!(),
            }
        }
    }
}

impl Producer<u8> {
    /// Reads at most `count` bytes
    /// from [`Read`](https://doc.rust-lang.org/std/io/trait.Read.html) instance
    /// and appends them to the ring buffer.
    /// If `count` is `None` then as much as possible bytes will be read.
    pub fn read_from(&mut self, reader: &mut dyn Read, count: Option<usize>)
    -> Result<usize, ReadFromError> {
        let push_fn = |left: &mut [u8], _right: &mut [u8]|
        -> Result<(usize, ()), io::Error> {
            let left = match count {
                Some(c) => {
                    if c < left.len() {
                        &mut left[0..c]
                    } else {
                        left
                    }
                },
                None => left,
            };
            reader.read(left).and_then(|n| {
                if n <= left.len() {
                    Ok((n, ()))
                } else {
                    Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        "Read operation returned invalid number",
                    ))
                }
            })
        };
        match unsafe { self.push_access(push_fn) } {
            Ok(res) => match res {
                Ok((n, ())) => Ok(n),
                Err(e) => Err(ReadFromError::Read(e)),
            },
            Err(e) => match e {
                PushAccessError::Full => Err(ReadFromError::RbFull),
                PushAccessError::BadLen => unreachable!(),
            }
        }
    }
}

impl Write for Producer<u8> {
    fn write(&mut self, buffer: &[u8]) -> io::Result<usize> {
        self.push_slice(buffer).or_else(|e| match e {
            PushSliceError::Full => Err(io::Error::new(
                io::ErrorKind::WouldBlock,
                "Ring buffer is full",
            ))
        })
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

impl<T: Sized> Producer<T> {
    /// Allows to write into ring buffer memory directry.
    ///
    /// *This function is unsafe beacuse it gives access to possibly uninitialized memory
    /// and transfers to the user the responsibility of manually calling destructors*
    ///
    /// Takes a function `f` as argument.
    /// `f` takes two slices of ring buffer content (the second one may be empty). First slice contains older elements.
    ///
    /// `f` should return:
    /// + On success: pair of number of elements been written, and some arbitrary data.
    /// + On failure: some another arbitrary data.
    ///
    /// On success returns data returned from `f`.
    pub unsafe fn push_access<R, E, F>(&mut self, f: F) -> Result<Result<(usize, R), E>, PushAccessError>
    where R: Sized, E: Sized, F: FnOnce(&mut [T], &mut [T]) -> Result<(usize, R), E> {
        let head = self.rb.head.load(Ordering::SeqCst);
        let tail = self.rb.tail.load(Ordering::SeqCst);
        let len = self.rb.data.get_ref().len();

        let ranges = if tail >= head {
            if head > 0 {
                Ok((tail..len, 0..(head - 1)))
            } else {
                if tail < len - 1 {
                    Ok((tail..(len - 1), 0..0))
                } else {
                    Err(PushAccessError::Full)
                }
            }
        } else {
            if tail < head - 1 {
                Ok((tail..(head - 1), 0..0))
            } else {
                Err(PushAccessError::Full)
            }
        }?;

        let slices = (
            &mut self.rb.data.get_mut()[ranges.0],
            &mut self.rb.data.get_mut()[ranges.1],
        );

        match f(slices.0, slices.1) {
            Ok((n, r)) => {
                if n > slices.0.len() + slices.1.len() {
                    Err(PushAccessError::BadLen)
                } else {
                    let new_tail = (tail + n) % len;
                    self.rb.tail.store(new_tail, Ordering::SeqCst);
                    Ok(Ok((n, r)))
                }
            },
            Err(e) => {
                Ok(Err(e))
            }
        }
    }
}

impl<T: Sized> Consumer<T> {
    /// Returns capacity of the ring buffer.
    pub fn capacity(&self) -> usize {
        self.rb.capacity()
    }

    /// Checks if the ring buffer is empty.
    pub fn is_empty(&self) -> bool {
        self.rb.is_empty()
    }

    /// Checks if the ring buffer is full.
    pub fn is_full(&self) -> bool {
        self.rb.is_full()
    }

    /// Removes first element from the ring buffer and returns it.
    pub fn pop(&mut self) -> Result<T, PopError> {
        match unsafe { self.pop_access(|slice, _| {
            let elem = mem::replace(&mut slice[0], mem::uninitialized());
            Ok((1, elem))
        }) } {
            Ok(res) => match res {
                Ok((n, elem)) => {
                    debug_assert_eq!(n, 1);
                    Ok(elem)
                },
                Err(()) => unreachable!(),
            },
            Err(e) => match e {
                PopAccessError::Empty => Err(PopError::Empty),
                PopAccessError::BadLen => unreachable!(),
            }
        }
    }
}

impl<T: Sized + Copy> Consumer<T> {
    /// Removes first elements from the ring buffer and writes them into a slice.
    /// Elements should be [`Copy`](https://doc.rust-lang.org/std/marker/trait.Copy.html).
    ///
    /// On success returns count of elements been removed from the ring buffer.
    pub fn pop_slice(&mut self, elems: &mut [T]) -> Result<usize, PopSliceError> {
        let pop_fn = |left: &mut [T], right: &mut [T]| {
            let elems_len = elems.len();
            Ok((if elems_len < left.len() {
                elems.copy_from_slice(&left[0..elems_len]);
                elems_len
            } else {
                elems[0..left.len()].copy_from_slice(left);
                if elems_len < left.len() + right.len() {
                    elems[left.len()..elems_len]
                        .copy_from_slice(&right[0..(elems_len - left.len())]);
                    elems_len
                } else {
                    elems[left.len()..(left.len() + right.len())].copy_from_slice(right);
                    left.len() + right.len()
                }
            }, ()))
        };
        match unsafe { self.pop_access(pop_fn) } {
            Ok(res) => match res {
                Ok((n, ())) => {
                    Ok(n)
                },
                Err(()) => unreachable!(),
            },
            Err(e) => match e {
                PopAccessError::Empty => Err(PopSliceError::Empty),
                PopAccessError::BadLen => unreachable!(),
            }
        }
    }

    /// Removes at most `count` elements from the `Consumer` of the ring buffer
    /// and appends them to the `Producer` of the another one.
    /// If `count` is `None` then as much as possible elements will be moved.
    ///
    /// Elements should be [`Copy`](https://doc.rust-lang.org/std/marker/trait.Copy.html).
    ///
    /// On success returns count of elements been moved.
    pub fn move_slice(&mut self, other: &mut Producer<T>, count: Option<usize>)
    -> Result<usize, MoveSliceError> {
        other.move_slice(self, count)
    }
}

impl Consumer<u8> {
    /// Removes at most first `count` bytes from the ring buffer and writes them into
    /// a [`Write`](https://doc.rust-lang.org/std/io/trait.Write.html) instance.
    /// If `count` is `None` then as much as possible bytes will be written.
    pub fn write_into(&mut self, writer: &mut dyn Write, count: Option<usize>)
    -> Result<usize, WriteIntoError> {
        let pop_fn = |left: &mut [u8], _right: &mut [u8]|
        -> Result<(usize, ()), io::Error> {
            let left = match count {
                Some(c) => {
                    if c < left.len() {
                        &mut left[0..c]
                    } else {
                        left
                    }
                },
                None => left,
            };
            writer.write(left).and_then(|n| {
                if n <= left.len() {
                    Ok((n, ()))
                } else {
                    Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        "Write operation returned invalid number",
                    ))
                }
            })
        };
        match unsafe { self.pop_access(pop_fn) } {
            Ok(res) => match res {
                Ok((n, ())) => Ok(n),
                Err(e) => Err(WriteIntoError::Write(e)),
            },
            Err(e) => match e {
                PopAccessError::Empty => Err(WriteIntoError::RbEmpty),
                PopAccessError::BadLen => unreachable!(),
            }
        }
    }
}

impl Read for Consumer<u8> {
    fn read(&mut self, buffer: &mut [u8]) -> io::Result<usize> {
        self.pop_slice(buffer).or_else(|e| match e {
            PopSliceError::Empty => Err(io::Error::new(
                io::ErrorKind::WouldBlock,
                "Ring buffer is empty",
            ))
        })
    }
}

impl<T: Sized> Consumer<T> {
    /// Allows to read from ring buffer memory directry.
    ///
    /// *This function is unsafe beacuse it gives access to possibly uninitialized memory
    /// and transfers to the user the responsibility of manually calling destructors*
    ///
    /// Takes a function `f` as argument.
    /// `f` takes two slices of ring buffer content (the second one may be empty). First slice contains older elements.
    ///
    /// `f` should return:
    /// + On success: pair of number of elements been read, and some arbitrary data.
    /// + On failure: some another arbitrary data.
    ///
    /// On success returns data returned from `f`.
    pub unsafe fn pop_access<R, E, F>(&mut self, f: F) -> Result<Result<(usize, R), E>, PopAccessError>
    where R: Sized, E: Sized, F: FnOnce(&mut [T], &mut [T]) -> Result<(usize, R), E> {
        let head = self.rb.head.load(Ordering::SeqCst);
        let tail = self.rb.tail.load(Ordering::SeqCst);
        let len = self.rb.data.get_ref().len();

        let ranges = if head < tail {
            Ok((head..tail, 0..0))
        } else if head > tail {
            Ok((head..len, 0..tail))
        } else {
            Err(PopAccessError::Empty)
        }?;

        let slices = (
            &mut self.rb.data.get_mut()[ranges.0],
            &mut self.rb.data.get_mut()[ranges.1],
        );

        match f(slices.0, slices.1) {
            Ok((n, r)) => {
                if n > slices.0.len() + slices.1.len() {
                    Err(PopAccessError::BadLen)
                } else {
                    let new_head = (head + n) % len;
                    self.rb.head.store(new_head, Ordering::SeqCst);
                    Ok(Ok((n, r)))
                }
            },
            Err(e) => {
                Ok(Err(e))
            }
        }
    }
}

#[cfg(test)]
#[test]
fn dummy_test() {
    let rb = RingBuffer::<i32>::new(16);
    rb.split();
}
