use std::{
    cmp::min,
    io::{self, Read, Write},
    mem::{self, MaybeUninit},
    ops::Range,
    ptr::copy_nonoverlapping,
    sync::{atomic::Ordering, Arc},
};

use crate::{producer::Producer, ring_buffer::*};

/// Consumer part of ring buffer.
pub struct Consumer<T> {
    pub(crate) rb: Arc<RingBuffer<T>>,
}

impl<T: Sized> Consumer<T> {
    /// Returns capacity of the ring buffer.
    ///
    /// The capacity of the buffer is constant.
    pub fn capacity(&self) -> usize {
        self.rb.capacity()
    }

    /// Checks if the ring buffer is empty.
    ///
    /// *The result may become irrelevant at any time because of concurring activity of the producer.*
    pub fn is_empty(&self) -> bool {
        self.rb.is_empty()
    }

    /// Checks if the ring buffer is full.
    ///
    /// The result is relevant until you remove items from the consumer.
    pub fn is_full(&self) -> bool {
        self.rb.is_full()
    }

    /// The length of the data stored in the buffer
    ///
    /// Actual length may be equal to or greater than the returned value.
    pub fn len(&self) -> usize {
        self.rb.len()
    }

    /// The remaining space in the buffer.
    ///
    /// Actual remaining space may be equal to or less than the returning value.
    pub fn remaining(&self) -> usize {
        self.rb.remaining()
    }

    fn get_ranges(&self) -> (Range<usize>, Range<usize>) {
        let head = self.rb.head.load(Ordering::Acquire);
        let tail = self.rb.tail.load(Ordering::Acquire);
        let len = unsafe { self.rb.data.get_ref().len() };

        if head < tail {
            (head..tail, 0..0)
        } else if head > tail {
            (head..len, 0..tail)
        } else {
            (0..0, 0..0)
        }
    }

    /// Gives immutable access to the elements contained by the ring buffer without removing them.
    ///
    /// The method takes a function `f` as argument.
    /// `f` takes two slices of ring buffer content (the second one or both of them may be empty).
    /// First slice contains older elements.
    ///
    /// *The slices may not include elements pushed to the buffer by concurring producer after the method call.*
    pub fn access<F: FnOnce(&[T], &[T])>(&self, f: F) {
        let ranges = self.get_ranges();

        unsafe {
            let left = &self.rb.data.get_ref()[ranges.0];
            let right = &self.rb.data.get_ref()[ranges.1];

            f(
                &*(left as *const [MaybeUninit<T>] as *const [T]),
                &*(right as *const [MaybeUninit<T>] as *const [T]),
            );
        }
    }

    /// Gives mutable access to the elements contained by the ring buffer without removing them.
    ///
    /// The method takes a function `f` as argument.
    /// `f` takes two slices of ring buffer content (the second one or both of them may be empty).
    /// First slice contains older elements.
    ///
    /// *The iteration may not include elements pushed to the buffer by concurring producer after the method call.*
    pub fn access_mut<F: FnOnce(&mut [T], &mut [T])>(&mut self, f: F) {
        let ranges = self.get_ranges();

        unsafe {
            let left = &mut self.rb.data.get_mut()[ranges.0];
            let right = &mut self.rb.data.get_mut()[ranges.1];

            f(
                &mut *(left as *mut [MaybeUninit<T>] as *mut [T]),
                &mut *(right as *mut [MaybeUninit<T>] as *mut [T]),
            );
        }
    }

    /// Allows to read from ring buffer memory directry.
    ///
    /// *This function is unsafe because it gives access to possibly uninitialized memory*
    ///
    /// The method takes a function `f` as argument.
    /// `f` takes two slices of ring buffer content (the second one or both of them may be empty).
    /// First slice contains older elements.
    ///
    /// `f` should return number of elements been read.
    /// *There is no checks for returned number - it remains on the developer's conscience.*
    ///
    /// The method **always** calls `f` even if ring buffer is empty.
    ///
    /// The method returns number returned from `f`.
    pub unsafe fn pop_access<F>(&mut self, f: F) -> usize
    where
        F: FnOnce(&mut [MaybeUninit<T>], &mut [MaybeUninit<T>]) -> usize,
    {
        let head = self.rb.head.load(Ordering::Acquire);
        let tail = self.rb.tail.load(Ordering::Acquire);
        let len = self.rb.data.get_ref().len();

        let ranges = if head < tail {
            (head..tail, 0..0)
        } else if head > tail {
            (head..len, 0..tail)
        } else {
            (0..0, 0..0)
        };

        let slices = (
            &mut self.rb.data.get_mut()[ranges.0],
            &mut self.rb.data.get_mut()[ranges.1],
        );

        let n = f(slices.0, slices.1);

        if n > 0 {
            let new_head = (head + n) % len;
            self.rb.head.store(new_head, Ordering::Release);
        }
        n
    }

    /// Copies data from the ring buffer to the slice in byte-to-byte manner.
    ///
    /// The `elems` slice should contain **un-initialized** data before the method call.
    /// After the call the copied part of data in `elems` should be interpreted as **initialized**.
    /// The remaining part is still **un-iniitilized**.
    ///
    /// Returns the number of items been copied.
    pub unsafe fn pop_copy(&mut self, elems: &mut [MaybeUninit<T>]) -> usize {
        self.pop_access(|left, right| {
            if elems.len() < left.len() {
                copy_nonoverlapping(left.as_ptr(), elems.as_mut_ptr(), elems.len());
                elems.len()
            } else {
                copy_nonoverlapping(left.as_ptr(), elems.as_mut_ptr(), left.len());
                if elems.len() < left.len() + right.len() {
                    copy_nonoverlapping(
                        right.as_ptr(),
                        elems.as_mut_ptr().add(left.len()),
                        elems.len() - left.len(),
                    );
                    elems.len()
                } else {
                    copy_nonoverlapping(
                        right.as_ptr(),
                        elems.as_mut_ptr().add(left.len()),
                        right.len(),
                    );
                    left.len() + right.len()
                }
            }
        })
    }

    /// Removes latest element from the ring buffer and returns it.
    /// Returns `None` if the ring buffer is empty.
    pub fn pop(&mut self) -> Option<T> {
        let mut elem_mu = MaybeUninit::uninit();
        let n = unsafe {
            self.pop_access(|slice, _| {
                if !slice.is_empty() {
                    mem::swap(slice.get_unchecked_mut(0), &mut elem_mu);
                    1
                } else {
                    0
                }
            })
        };
        match n {
            0 => None,
            1 => Some(unsafe { elem_mu.assume_init() }),
            _ => unreachable!(),
        }
    }

    /// Repeatedly calls the closure `f` passing elements removed from the ring buffer to it.
    ///
    /// The closure is called until it returns `false` or the ring buffer is empty.
    ///
    /// The method returns number of elements been removed from the buffer.
    pub fn pop_each<F: FnMut(T) -> bool>(&mut self, mut f: F, count: Option<usize>) -> usize {
        unsafe {
            self.pop_access(|left, right| {
                let lb = match count {
                    Some(n) => min(n, left.len()),
                    None => left.len(),
                };
                for (i, dst) in left[0..lb].iter_mut().enumerate() {
                    if !f(mem::replace(dst, MaybeUninit::uninit()).assume_init()) {
                        return i + 1;
                    }
                }
                if lb < left.len() {
                    return lb;
                }

                let rb = match count {
                    Some(n) => min(n - lb, right.len()),
                    None => right.len(),
                };
                for (i, dst) in right[0..rb].iter_mut().enumerate() {
                    if !f(mem::replace(dst, MaybeUninit::uninit()).assume_init()) {
                        return i + lb + 1;
                    }
                }
                left.len() + right.len()
            })
        }
    }

    /// Iterate immutably over the elements contained by the ring buffer without removing them.
    ///
    /// *The iteration may not include elements pushed to the buffer by concurring producer after the method call.*
    pub fn for_each<F: FnMut(&T)>(&self, mut f: F) {
        self.access(|left, right| {
            for c in left.iter() {
                f(c);
            }
            for c in right.iter() {
                f(c);
            }
        });
    }

    /// Iterate mutably over the elements contained by the ring buffer without removing them.
    ///
    /// *The iteration may not include elements pushed to the buffer by concurring producer after the method call.*
    pub fn for_each_mut<F: FnMut(&mut T)>(&mut self, mut f: F) {
        self.access_mut(|left, right| {
            for c in left.iter_mut() {
                f(c);
            }
            for c in right.iter_mut() {
                f(c);
            }
        });
    }

    /// Removes at most `count` elements from the consumer and appends them to the producer.
    /// If `count` is `None` then as much as possible elements will be moved.
    /// The producer and consumer parts may be of different buffers as well as of the same one.
    ///
    /// On success returns count of elements been moved.
    pub fn move_to(&mut self, other: &mut Producer<T>, count: Option<usize>) -> usize {
        move_items(self, other, count)
    }
}

impl<T: Sized + Copy> Consumer<T> {
    /// Removes first elements from the ring buffer and writes them into a slice.
    /// Elements should be [`Copy`](https://doc.rust-lang.org/std/marker/trait.Copy.html).
    ///
    /// On success returns count of elements been removed from the ring buffer.
    pub fn pop_slice(&mut self, elems: &mut [T]) -> usize {
        unsafe { self.pop_copy(&mut *(elems as *mut [T] as *mut [MaybeUninit<T>])) }
    }
}

impl Consumer<u8> {
    /// Removes at most first `count` bytes from the ring buffer and writes them into
    /// a [`Write`](https://doc.rust-lang.org/std/io/trait.Write.html) instance.
    /// If `count` is `None` then as much as possible bytes will be written.
    ///
    /// Returns `Ok(n)` if `write` is succeded. `n` is number of bytes been written.
    /// `n == 0` means that either `write` returned zero or ring buffer is empty.
    ///
    /// If `write` is failed then error is returned.
    pub fn write_into(
        &mut self,
        writer: &mut dyn Write,
        count: Option<usize>,
    ) -> io::Result<usize> {
        let mut err = None;
        let n = unsafe {
            self.pop_access(|left, _| -> usize {
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
                match writer
                    .write(&*(left as *const [MaybeUninit<u8>] as *const [u8]))
                    .and_then(|n| {
                        if n <= left.len() {
                            Ok(n)
                        } else {
                            Err(io::Error::new(
                                io::ErrorKind::InvalidInput,
                                "Write operation returned invalid number",
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

impl Read for Consumer<u8> {
    fn read(&mut self, buffer: &mut [u8]) -> io::Result<usize> {
        let n = self.pop_slice(buffer);
        if n == 0 && !buffer.is_empty() {
            Err(io::Error::new(
                io::ErrorKind::WouldBlock,
                "Ring buffer is empty",
            ))
        } else {
            Ok(n)
        }
    }
}
