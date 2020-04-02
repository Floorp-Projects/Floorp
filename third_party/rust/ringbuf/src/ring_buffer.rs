use std::{
    cell::UnsafeCell,
    cmp::min,
    mem::{self, MaybeUninit},
    ptr::{self, copy},
    sync::{
        atomic::{AtomicUsize, Ordering},
        Arc,
    },
};

use crate::{consumer::Consumer, producer::Producer};

pub(crate) struct SharedVec<T: Sized> {
    cell: UnsafeCell<Vec<T>>,
}

unsafe impl<T: Sized> Sync for SharedVec<T> {}

impl<T: Sized> SharedVec<T> {
    pub fn new(data: Vec<T>) -> Self {
        Self {
            cell: UnsafeCell::new(data),
        }
    }
    pub unsafe fn get_ref(&self) -> &Vec<T> {
        &*self.cell.get()
    }
    #[allow(clippy::mut_from_ref)]
    pub unsafe fn get_mut(&self) -> &mut Vec<T> {
        &mut *self.cell.get()
    }
}

/// Ring buffer itself.
pub struct RingBuffer<T: Sized> {
    pub(crate) data: SharedVec<MaybeUninit<T>>,
    pub(crate) head: AtomicUsize,
    pub(crate) tail: AtomicUsize,
}

impl<T: Sized> RingBuffer<T> {
    /// Creates a new instance of a ring buffer.
    pub fn new(capacity: usize) -> Self {
        let mut data = Vec::new();
        data.resize_with(capacity + 1, MaybeUninit::uninit);
        Self {
            data: SharedVec::new(data),
            head: AtomicUsize::new(0),
            tail: AtomicUsize::new(0),
        }
    }

    /// Splits ring buffer into producer and consumer.
    pub fn split(self) -> (Producer<T>, Consumer<T>) {
        let arc = Arc::new(self);
        (Producer { rb: arc.clone() }, Consumer { rb: arc })
    }

    /// Returns capacity of the ring buffer.
    pub fn capacity(&self) -> usize {
        unsafe { self.data.get_ref() }.len() - 1
    }

    /// Checks if the ring buffer is empty.
    pub fn is_empty(&self) -> bool {
        let head = self.head.load(Ordering::Acquire);
        let tail = self.tail.load(Ordering::Acquire);
        head == tail
    }

    /// Checks if the ring buffer is full.
    pub fn is_full(&self) -> bool {
        let head = self.head.load(Ordering::Acquire);
        let tail = self.tail.load(Ordering::Acquire);
        (tail + 1) % (self.capacity() + 1) == head
    }

    /// The length of the data in the buffer.
    pub fn len(&self) -> usize {
        let head = self.head.load(Ordering::Acquire);
        let tail = self.tail.load(Ordering::Acquire);
        (tail + self.capacity() + 1 - head) % (self.capacity() + 1)
    }

    /// The remaining space in the buffer.
    pub fn remaining(&self) -> usize {
        self.capacity() - self.len()
    }
}

impl<T: Sized> Drop for RingBuffer<T> {
    fn drop(&mut self) {
        let data = unsafe { self.data.get_mut() };

        let head = self.head.load(Ordering::Acquire);
        let tail = self.tail.load(Ordering::Acquire);
        let len = data.len();

        let slices = if head <= tail {
            (head..tail, 0..0)
        } else {
            (head..len, 0..tail)
        };

        let drop = |elem_ref: &mut MaybeUninit<T>| unsafe {
            mem::replace(elem_ref, MaybeUninit::uninit()).assume_init();
        };
        for elem in data[slices.0].iter_mut() {
            drop(elem);
        }
        for elem in data[slices.1].iter_mut() {
            drop(elem);
        }
    }
}

struct SlicePtr<T: Sized> {
    pub ptr: *mut T,
    pub len: usize,
}

impl<T> SlicePtr<T> {
    fn null() -> Self {
        Self {
            ptr: ptr::null_mut(),
            len: 0,
        }
    }
    fn new(slice: &mut [T]) -> Self {
        Self {
            ptr: slice.as_mut_ptr(),
            len: slice.len(),
        }
    }
    unsafe fn shift(&mut self, count: usize) {
        self.ptr = self.ptr.add(count);
        self.len -= count;
    }
}

/// Moves at most `count` items from the `src` consumer to the `dst` producer.
/// Consumer and producer may be of different buffers as well as of the same one.
///
/// `count` is the number of items being moved, if `None` - as much as possible items will be moved.
///
/// Returns number of items been moved.
pub fn move_items<T>(src: &mut Consumer<T>, dst: &mut Producer<T>, count: Option<usize>) -> usize {
    unsafe {
        src.pop_access(|src_left, src_right| -> usize {
            dst.push_access(|dst_left, dst_right| -> usize {
                let n = count.unwrap_or_else(|| {
                    min(
                        src_left.len() + src_right.len(),
                        dst_left.len() + dst_right.len(),
                    )
                });
                let mut m = 0;
                let mut src = (SlicePtr::new(src_left), SlicePtr::new(src_right));
                let mut dst = (SlicePtr::new(dst_left), SlicePtr::new(dst_right));

                loop {
                    let k = min(n - m, min(src.0.len, dst.0.len));
                    if k == 0 {
                        break;
                    }
                    copy(src.0.ptr, dst.0.ptr, k);
                    if src.0.len == k {
                        src.0 = src.1;
                        src.1 = SlicePtr::null();
                    } else {
                        src.0.shift(k);
                    }
                    if dst.0.len == k {
                        dst.0 = dst.1;
                        dst.1 = SlicePtr::null();
                    } else {
                        dst.0.shift(k);
                    }
                    m += k
                }

                m
            })
        })
    }
}
