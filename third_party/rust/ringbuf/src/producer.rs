use std::{
    io::{self, Read, Write},
    mem::{self, MaybeUninit},
    ptr::copy_nonoverlapping,
    sync::{atomic::Ordering, Arc},
};

use crate::{consumer::Consumer, ring_buffer::*};

/// Producer part of ring buffer.
pub struct Producer<T> {
    pub(crate) rb: Arc<RingBuffer<T>>,
}

impl<T: Sized> Producer<T> {
    /// Returns capacity of the ring buffer.
    ///
    /// The capacity of the buffer is constant.
    pub fn capacity(&self) -> usize {
        self.rb.capacity()
    }

    /// Checks if the ring buffer is empty.
    ///
    /// The result is relevant until you push items to the producer.
    pub fn is_empty(&self) -> bool {
        self.rb.is_empty()
    }

    /// Checks if the ring buffer is full.
    ///
    /// *The result may become irrelevant at any time because of concurring activity of the consumer.*
    pub fn is_full(&self) -> bool {
        self.rb.is_full()
    }

    /// The length of the data stored in the buffer.
    ///
    /// Actual length may be equal to or less than the returned value.
    pub fn len(&self) -> usize {
        self.rb.len()
    }

    /// The remaining space in the buffer.
    ///
    /// Actual remaining space may be equal to or greater than the returning value.
    pub fn remaining(&self) -> usize {
        self.rb.remaining()
    }

    /// Allows to write into ring buffer memory directry.
    ///
    /// *This function is unsafe because it gives access to possibly uninitialized memory*
    ///
    /// The method takes a function `f` as argument.
    /// `f` takes two slices of ring buffer content (the second one or both of them may be empty).
    /// First slice contains older elements.
    ///
    /// `f` should return number of elements been written.
    /// *There is no checks for returned number - it remains on the developer's conscience.*
    ///
    /// The method **always** calls `f` even if ring buffer is full.
    ///
    /// The method returns number returned from `f`.
    pub unsafe fn push_access<F>(&mut self, f: F) -> usize
    where
        F: FnOnce(&mut [MaybeUninit<T>], &mut [MaybeUninit<T>]) -> usize,
    {
        let head = self.rb.head.load(Ordering::Acquire);
        let tail = self.rb.tail.load(Ordering::Acquire);
        let len = self.rb.data.get_ref().len();

        let ranges = if tail >= head {
            if head > 0 {
                (tail..len, 0..(head - 1))
            } else if tail < len - 1 {
                (tail..(len - 1), 0..0)
            } else {
                (0..0, 0..0)
            }
        } else if tail < head - 1 {
            (tail..(head - 1), 0..0)
        } else {
            (0..0, 0..0)
        };

        let slices = (
            &mut self.rb.data.get_mut()[ranges.0],
            &mut self.rb.data.get_mut()[ranges.1],
        );

        let n = f(slices.0, slices.1);

        if n > 0 {
            let new_tail = (tail + n) % len;
            self.rb.tail.store(new_tail, Ordering::Release);
        }
        n
    }

    /// Copies data from the slice to the ring buffer in byte-to-byte manner.
    ///
    /// The `elems` slice should contain **initialized** data before the method call.
    /// After the call the copied part of data in `elems` should be interpreted as **un-initialized**.
    ///
    /// Returns the number of items been copied.
    pub unsafe fn push_copy(&mut self, elems: &[MaybeUninit<T>]) -> usize {
        self.push_access(|left, right| -> usize {
            if elems.len() < left.len() {
                copy_nonoverlapping(elems.as_ptr(), left.as_mut_ptr(), elems.len());
                elems.len()
            } else {
                copy_nonoverlapping(elems.as_ptr(), left.as_mut_ptr(), left.len());
                if elems.len() < left.len() + right.len() {
                    copy_nonoverlapping(
                        elems.as_ptr().add(left.len()),
                        right.as_mut_ptr(),
                        elems.len() - left.len(),
                    );
                    elems.len()
                } else {
                    copy_nonoverlapping(
                        elems.as_ptr().add(left.len()),
                        right.as_mut_ptr(),
                        right.len(),
                    );
                    left.len() + right.len()
                }
            }
        })
    }

    /// Appends an element to the ring buffer.
    /// On failure returns an error containing the element that hasn't beed appended.
    pub fn push(&mut self, elem: T) -> Result<(), T> {
        let mut elem_mu = MaybeUninit::new(elem);
        let n = unsafe {
            self.push_access(|slice, _| {
                if !slice.is_empty() {
                    mem::swap(slice.get_unchecked_mut(0), &mut elem_mu);
                    1
                } else {
                    0
                }
            })
        };
        match n {
            0 => Err(unsafe { elem_mu.assume_init() }),
            1 => Ok(()),
            _ => unreachable!(),
        }
    }

    /// Repeatedly calls the closure `f` and pushes elements returned from it to the ring buffer.
    ///
    /// The closure is called until it returns `None` or the ring buffer is full.
    ///
    /// The method returns number of elements been put into the buffer.
    pub fn push_each<F: FnMut() -> Option<T>>(&mut self, mut f: F) -> usize {
        unsafe {
            self.push_access(|left, right| {
                for (i, dst) in left.iter_mut().enumerate() {
                    match f() {
                        Some(e) => mem::replace(dst, MaybeUninit::new(e)),
                        None => return i,
                    };
                }
                for (i, dst) in right.iter_mut().enumerate() {
                    match f() {
                        Some(e) => mem::replace(dst, MaybeUninit::new(e)),
                        None => return i + left.len(),
                    };
                }
                left.len() + right.len()
            })
        }
    }

    /// Appends elements from an iterator to the ring buffer.
    /// Elements that haven't been added to the ring buffer remain in the iterator.
    ///
    /// Returns count of elements been appended to the ring buffer.
    pub fn push_iter<I: Iterator<Item = T>>(&mut self, elems: &mut I) -> usize {
        self.push_each(|| elems.next())
    }

    /// Removes at most `count` elements from the consumer and appends them to the producer.
    /// If `count` is `None` then as much as possible elements will be moved.
    /// The producer and consumer parts may be of different buffers as well as of the same one.
    ///
    /// On success returns number of elements been moved.
    pub fn move_from(&mut self, other: &mut Consumer<T>, count: Option<usize>) -> usize {
        move_items(other, self, count)
    }
}

impl<T: Sized + Copy> Producer<T> {
    /// Appends elements from slice to the ring buffer.
    /// Elements should be [`Copy`](https://doc.rust-lang.org/std/marker/trait.Copy.html).
    ///
    /// Returns count of elements been appended to the ring buffer.
    pub fn push_slice(&mut self, elems: &[T]) -> usize {
        unsafe { self.push_copy(&*(elems as *const [T] as *const [MaybeUninit<T>])) }
    }
}

impl Producer<u8> {
    /// Reads at most `count` bytes
    /// from [`Read`](https://doc.rust-lang.org/std/io/trait.Read.html) instance
    /// and appends them to the ring buffer.
    /// If `count` is `None` then as much as possible bytes will be read.
    ///
    /// Returns `Ok(n)` if `read` is succeded. `n` is number of bytes been read.
    /// `n == 0` means that either `read` returned zero or ring buffer is full.
    ///
    /// If `read` is failed then error is returned.
    pub fn read_from(&mut self, reader: &mut dyn Read, count: Option<usize>) -> io::Result<usize> {
        let mut err = None;
        let n = unsafe {
            self.push_access(|left, _| -> usize {
                let left = match count {
                    Some(c) => {
                        if c < left.len() {
                            &mut left[0..c]
                        } else {
                            left
                        }
                    }
                    None => left,
                };
                match reader
                    .read(&mut *(left as *mut [MaybeUninit<u8>] as *mut [u8]))
                    .and_then(|n| {
                        if n <= left.len() {
                            Ok(n)
                        } else {
                            Err(io::Error::new(
                                io::ErrorKind::InvalidInput,
                                "Read operation returned invalid number",
                            ))
                        }
                    }) {
                    Ok(n) => n,
                    Err(e) => {
                        err = Some(e);
                        0
                    }
                }
            })
        };
        match err {
            Some(e) => Err(e),
            None => Ok(n),
        }
    }
}

impl Write for Producer<u8> {
    fn write(&mut self, buffer: &[u8]) -> io::Result<usize> {
        let n = self.push_slice(buffer);
        if n == 0 && !buffer.is_empty() {
            Err(io::Error::new(
                io::ErrorKind::WouldBlock,
                "Ring buffer is full",
            ))
        } else {
            Ok(n)
        }
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}
