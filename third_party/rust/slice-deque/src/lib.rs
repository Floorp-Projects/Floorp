//! A double-ended queue that `Deref`s into a slice.
//!
//! The double-ended queue in the standard library ([`VecDeque`]) is
//! implemented using a growable ring buffer (`0` represents uninitialized
//! memory, and `T` represents one element in the queue):
//!
//! ```rust
//! // [ 0 | 0 | 0 | T | T | T | 0 ]
//! //               ^:head  ^:tail
//! ```
//!
//! When the queue grows beyond the end of the allocated buffer, its tail wraps
//! around:
//!
//! ```rust
//! // [ T | T | 0 | T | T | T | T ]
//! //       ^:tail  ^:head
//! ```
//!
//! As a consequence, [`VecDeque`] cannot `Deref` into a slice, since its
//! elements do not, in general, occupy a contiguous memory region. This
//! complicates the implementation and its interface (for example, there is no
//! `as_slice` method, but [`as_slices`] returns a pair of slices) and has
//! negative performance consequences (e.g. need to account for wrap around
//! while iterating over the elements).
//!
//! This crates provides [`SliceDeque`], a double-ended queue implemented with
//! a growable *virtual* ring-buffer.
//!
//! A virtual ring-buffer implementation is very similar to the one used in
//! `VecDeque`. The main difference is that a virtual ring-buffer maps two
//! adjacent regions of virtual memory to the same region of physical memory:
//!
//! ```rust
//! // Virtual memory:
//! //
//! //  __________region_0_________ __________region_1_________
//! // [ 0 | 0 | 0 | T | T | T | 0 | 0 | 0 | 0 | T | T | T | 0 ]
//! //               ^:head  ^:tail
//! //
//! // Physical memory:
//! //
//! // [ 0 | 0 | 0 | T | T | T | 0 ]
//! //               ^:head  ^:tail
//! ```
//!
//! That is, both the virtual memory regions `0` and `1` above (top) map to
//! the same physical memory (bottom). Just like `VecDeque`, when the queue
//! grows beyond the end of the allocated physical memory region, the queue
//! wraps around, and new elements continue to be appended at the beginning of
//! the queue. However, because `SliceDeque` maps the physical memory to two
//! adjacent memory regions, in virtual memory space the queue maintais the
//! ilusion of a contiguous memory layout:
//!
//! ```rust
//! // Virtual memory:
//! //
//! //  __________region_0_________ __________region_1_________
//! // [ T | T | 0 | T | T | T | T | T | T | 0 | T | T | T | T ]
//! //               ^:head              ^:tail
//! //
//! // Physical memory:
//! //
//! // [ T | T | 0 | T | T | T | T ]
//! //       ^:tail  ^:head
//! ```
//!
//! Since processes in many Operating Systems only deal with virtual memory
//! addresses, leaving the mapping to physical memory to the CPU Memory
//! Management Unit (MMU), [`SliceDeque`] is able to `Deref`s into a slice in
//! those systems.
//!
//! This simplifies [`SliceDeque`]'s API and implementation, giving it a
//! performance advantage over [`VecDeque`] in some situations.
//!
//! In general, you can think of [`SliceDeque`] as a `Vec` with `O(1)`
//! `pop_front` and amortized `O(1)` `push_front` methods.
//!
//! The main drawbacks of [`SliceDeque`] are:
//!
//! * constrained platform support: by necessity [`SliceDeque`] must use the
//! platform-specific virtual memory facilities of the underlying operating
//! system. While [`SliceDeque`] can work on all major operating systems,
//! currently only `MacOS X` is supported.
//!
//! * no global allocator support: since the `Alloc`ator API does not support
//! virtual memory, to use platform-specific virtual memory support
//! [`SliceDeque`] must bypass the global allocator and talk directly to the
//! operating system. This can have negative performance consequences since
//! growing [`SliceDeque`] is always going to incur the cost of some system
//! calls.
//!
//! * capacity constrained by virtual memory facilities: [`SliceDeque`] must
//! allocate two adjacent memory regions that map to the same region of
//! physical memory. Most operating systems allow this operation to be
//! performed exclusively on memory pages (or memory allocations that are
//! multiples of a memory page). As a consequence, the smalles [`SliceDeque`]
//! that can be created has typically a capacity of 2 memory pages, and it can
//! grow only to capacities that are a multiple of a memory page.
//!
//! The main advantages of [`SliceDeque`] are:
//!
//! * nicer API: since it `Deref`s to a slice, all operations that work on
//! slices are available for `SliceDeque`.
//!
//! * efficient iteration: as efficient as for slices.
//!
//! * simpler serialization: since one can just serialize/deserialize a single
//! slice.
//!
//! All in all, if your double-ended queues are small (smaller than a memory
//! page) or they get resized very often, `VecDeque` can perform better than
//! [`SliceDeque`]. Otherwise, [`SliceDeque`] typically performs better (see
//! the benchmarks), but platform support and global allocator bypass are two
//! reasons to weight in against its usage.
//!
//! [`VecDeque`]: https://doc.rust-lang.org/std/collections/struct.VecDeque.html
//! [`as_slices`]: https://doc.rust-lang.org/std/collections/struct.VecDeque.html#method.as_slices
//! [`SliceDeque`]: struct.SliceDeque.html

#![cfg_attr(
    feature = "unstable",
    feature(
        core_intrinsics,
        exact_size_is_empty,
        dropck_eyepatch,
        trusted_len,
        ptr_wrapping_offset_from,
        specialization,
    )
)]
#![cfg_attr(all(test, feature = "unstable"), feature(box_syntax))]
#![allow(
    clippy::len_without_is_empty,
    clippy::shadow_reuse,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::cast_possible_truncation,
    clippy::inline_always,
    clippy::indexing_slicing
)]
#![cfg_attr(not(any(feature = "use_std", test)), no_std)]

#[macro_use]
mod macros;

#[cfg(any(feature = "use_std", test))]
extern crate core;

#[cfg(all(
    any(target_os = "macos", target_os = "ios"),
    not(feature = "unix_sysv")
))]
extern crate mach;

#[cfg(unix)]
extern crate libc;

#[cfg(target_os = "windows")]
extern crate winapi;

#[cfg(all(feature = "bytes_buf", feature = "use_std"))]
extern crate bytes;

mod mirrored;
pub use mirrored::{AllocError, Buffer};

#[cfg(all(feature = "bytes_buf", feature = "use_std"))]
use std::io;

use core::{cmp, convert, fmt, hash, iter, mem, ops, ptr, slice, str};

use core::ptr::NonNull;

#[cfg(feature = "unstable")]
use core::intrinsics;

/// A stable version of the `core::intrinsics` module.
#[cfg(not(feature = "unstable"))]
mod intrinsics {
    /// Like `core::intrinsics::unlikely` but does nothing.
    #[inline(always)]
    pub unsafe fn unlikely<T>(x: T) -> T {
        x
    }

    /// Like `core::intrinsics::assume` but does nothing.
    #[inline(always)]
    pub unsafe fn assume<T>(x: T) -> T {
        x
    }

    /// Like `core::intrinsics::arith_offset` but doing pointer to integer
    /// conversions.
    #[inline(always)]
    pub unsafe fn arith_offset<T>(dst: *const T, offset: isize) -> *const T {
        let r = if offset >= 0 {
            (dst as usize).wrapping_add(offset as usize)
        } else {
            (dst as usize).wrapping_sub((-offset) as usize)
        };
        r as *const T
    }
}

/// Stable implementation of `.wrapping_offset_from` for pointers.
trait WrappingOffsetFrom {
    /// Stable implementation of `.wrapping_offset_from` for pointers.
    fn wrapping_offset_from_(self, other: Self) -> Option<isize>;
}

#[cfg(not(feature = "unstable"))]
impl<T: Sized> WrappingOffsetFrom for *const T {
    #[inline(always)]
    fn wrapping_offset_from_(self, other: Self) -> Option<isize>
    where
        T: Sized,
    {
        let size = mem::size_of::<T>();
        if size == 0 {
            None
        } else {
            let diff = (other as isize).wrapping_sub(self as isize);
            Some(diff / size as isize)
        }
    }
}

#[cfg(feature = "unstable")]
impl<T: Sized> WrappingOffsetFrom for *const T {
    #[inline(always)]
    fn wrapping_offset_from_(self, other: Self) -> Option<isize>
    where
        T: Sized,
    {
        let size = mem::size_of::<T>();
        if size == 0 {
            None
        } else {
            Some(other.wrapping_offset_from(self))
        }
    }
}

/// Is `p` in bounds of `s` ?
///
/// Does it point to an element of `s` ? That is, one past the end of `s` is
/// not in bounds.
fn in_bounds<T>(s: &[T], p: *mut T) -> bool {
    let p = p as usize;
    let s_begin = s.as_ptr() as usize;
    let s_end = s_begin + mem::size_of::<T>() * s.len();
    s_begin <= p && p < s_end
}

unsafe fn nonnull_raw_slice<T>(ptr: *mut T, len: usize) -> NonNull<[T]> {
    NonNull::new_unchecked(slice::from_raw_parts_mut(ptr, len))
}

/// A double-ended queue that derefs into a slice.
///
/// It is implemented with a growable virtual ring buffer.
pub struct SliceDeque<T> {
    /// Elements in the queue.
    elems_: NonNull<[T]>,
    /// Mirrored memory buffer.
    buf: Buffer<T>,
}

// Safe because it is possible to free this from a different thread
unsafe impl<T> Send for SliceDeque<T> where T: Send {}
// Safe because this doesn't use any kind of interior mutability.
unsafe impl<T> Sync for SliceDeque<T> where T: Sync {}

/// Implementation detail of the sdeq! macro.
#[doc(hidden)]
pub use mem::forget as __mem_forget;

/// Creates a [`SliceDeque`] containing the arguments.
///
/// `sdeq!` allows `SliceDeque`s to be defined with the same syntax as array
/// expressions. There are two forms of this macro:
///
/// - Create a [`SliceDeque`] containing a given list of elements:
///
/// ```
/// # #[macro_use] extern crate slice_deque;
/// # use slice_deque::SliceDeque;
/// # fn main() {
/// let v: SliceDeque<i32> = sdeq![1, 2, 3];
/// assert_eq!(v[0], 1);
/// assert_eq!(v[1], 2);
/// assert_eq!(v[2], 3);
/// # }
/// ```
///
/// - Create a [`SliceDeque`] from a given element and size:
///
/// ```
/// # #[macro_use] extern crate slice_deque;
/// # use slice_deque::SliceDeque;
/// # fn main() {
/// let v = sdeq![7; 3];
/// assert_eq!(v, [7, 7, 7]);
/// # }
/// ```
///
/// Note that unlike array expressions this syntax supports all elements
/// which implement `Clone` and the number of elements doesn't have to be
/// a constant.
///
/// This will use `clone` to duplicate an expression, so one should be careful
/// using this with types having a nonstandard `Clone` implementation. For
/// example, `sdeq![Rc::new(1); 5]` will create a deque of five references
/// to the same boxed integer value, not five references pointing to
/// independently boxed integers.
///
/// ```
/// # #[macro_use] extern crate slice_deque;
/// # use slice_deque::SliceDeque;
/// # use std::rc::Rc;
/// # fn main() {
/// let v = sdeq![Rc::new(1_i32); 5];
/// let ptr: *const i32 = &*v[0] as *const i32;
/// for i in v.iter() {
///     assert_eq!(Rc::into_raw(i.clone()), ptr);
/// }
/// # }
/// ```
///
/// [`SliceDeque`]: struct.SliceDeque.html
#[macro_export]
macro_rules! sdeq {
    ($elem:expr; $n:expr) => (
        {
            let mut deq = $crate::SliceDeque::with_capacity($n);
            deq.resize($n, $elem);
            deq
        }
    );
    () => ( $crate::SliceDeque::new() );
    ($($x:expr),*) => (
        {
            unsafe {
                let array = [$($x),*];
                let deq = $crate::SliceDeque::steal_from_slice(&array);
                #[allow(clippy::forget_copy)]
                $crate::__mem_forget(array);
                deq
            }
        }
    );
    ($($x:expr,)*) => (sdeq![$($x),*])
}

impl<T> SliceDeque<T> {
    /// Creates a new empty deque.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use slice_deque::SliceDeque;
    /// let deq = SliceDeque::new();
    /// # let o: SliceDeque<u32> = deq;
    /// ```
    #[inline]
    pub fn new() -> Self {
        unsafe {
            let buf = Buffer::new();
            Self {
                elems_: nonnull_raw_slice(buf.ptr(), 0),
                buf,
            }
        }
    }

    /// Creates a SliceDeque from its raw components.
    ///
    /// The `ptr` must be a pointer to the beginning of the memory buffer from
    /// another `SliceDeque`, `capacity` the capacity of this `SliceDeque`, and
    /// `elems` the elements of this `SliceDeque`.
    #[inline]
    pub unsafe fn from_raw_parts(
        ptr: *mut T, capacity: usize, elems: &mut [T],
    ) -> Self {
        let begin = elems.as_mut_ptr();
        debug_assert!(in_bounds(slice::from_raw_parts(ptr, capacity), begin));
        debug_assert!(elems.len() < capacity);

        Self {
            elems_: NonNull::new_unchecked(elems),
            buf: Buffer::from_raw_parts(ptr, capacity * 2),
        }
    }

    /// Create an empty deque with capacity to hold `n` elements.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use slice_deque::SliceDeque;
    /// let deq = SliceDeque::with_capacity(10);
    /// # let o: SliceDeque<u32> = deq;
    /// ```
    #[inline]
    pub fn with_capacity(n: usize) -> Self {
        unsafe {
            let buf = Buffer::uninitialized(2 * n).unwrap_or_else(|e| {
                let s = tiny_str!(
                    "failed to allocate a buffer with capacity \"{}\" due to \"{:?}\"",
                    n, e
                );
                panic!("{}", s.as_str())
            });
            Self {
                elems_: nonnull_raw_slice(buf.ptr(), 0),
                buf,
            }
        }
    }

    /// Returns the number of elements that the deque can hold without
    /// reallocating.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use slice_deque::SliceDeque;
    /// let deq = SliceDeque::with_capacity(10);
    /// assert!(deq.capacity() >= 10);
    /// # let o: SliceDeque<u32> = deq;
    /// ```
    #[inline]
    pub fn capacity(&self) -> usize {
        // Note: the buffer length is not necessarily a power of two
        // debug_assert!(self.buf.len() % 2 == 0);
        self.buf.len() / 2
    }

    /// Number of elements in the ring buffer.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use slice_deque::SliceDeque;
    /// let mut deq = SliceDeque::with_capacity(10);
    /// assert!(deq.len() == 0);
    /// deq.push_back(3);
    /// assert!(deq.len() == 1);
    /// ```
    #[inline]
    pub fn len(&self) -> usize {
        let l = self.as_slice().len();
        debug_assert!(l <= self.capacity());
        l
    }

    /// Is the ring buffer full ?
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use slice_deque::SliceDeque;
    /// let mut deq = SliceDeque::with_capacity(10);
    /// assert!(!deq.is_full());
    /// # let o: SliceDeque<u32> = deq;
    /// ```
    #[inline]
    pub fn is_full(&self) -> bool {
        self.len() == self.capacity()
    }

    /// Extracts a slice containing the entire deque.
    #[inline]
    pub fn as_slice(&self) -> &[T] {
        unsafe { self.elems_.as_ref() }
    }

    /// Extracts a mutable slice containing the entire deque.
    #[inline]
    pub fn as_mut_slice(&mut self) -> &mut [T] {
        unsafe { self.elems_.as_mut() }
    }

    /// Returns a pair of slices, where the first slice contains the contents
    /// of the deque and the second one is empty.
    #[inline]
    pub fn as_slices(&self) -> (&[T], &[T]) {
        unsafe {
            let left = self.as_slice();
            let right =
                slice::from_raw_parts(usize::max_value() as *const _, 0);
            (left, right)
        }
    }

    /// Returns a pair of slices, where the first slice contains the contents
    /// of the deque and the second one is empty.
    #[inline]
    pub fn as_mut_slices(&mut self) -> (&mut [T], &mut [T]) {
        unsafe {
            let left = self.as_mut_slice();
            let right =
                slice::from_raw_parts_mut(usize::max_value() as *mut _, 0);
            (left, right)
        }
    }

    /// Returns the slice of uninitialized memory between the `tail` and the
    /// `begin`.
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # fn main() {
    /// let mut d = sdeq![1, 2, 3];
    /// let cap = d.capacity();
    /// let len = d.len();
    /// unsafe {
    ///     {
    ///         // This slice contains the uninitialized elements in
    ///         // the deque:
    ///         let mut s = d.tail_head_slice();
    ///         assert_eq!(s.len(), cap - len);
    ///         // We can write to them and for example bump the tail of
    ///         // the deque:
    ///         s[0] = 4;
    ///         s[1] = 5;
    ///     }
    ///     d.move_tail(2);
    /// }
    /// assert_eq!(d, sdeq![1, 2, 3, 4, 5]);
    /// # }
    /// ```
    pub unsafe fn tail_head_slice(&mut self) -> &mut [T] {
        let ptr = self.as_mut_slice().as_mut_ptr().add(self.len());
        slice::from_raw_parts_mut(ptr, self.capacity() - self.len())
    }

    /// Attempts to reserve capacity for inserting at least `additional`
    /// elements without reallocating. Does nothing if the capacity is already
    /// sufficient.
    ///
    /// The collection always reserves memory in multiples of the page size.
    ///
    /// # Panics
    ///
    /// Panics if the new capacity overflows `usize`.
    #[inline]
    pub fn try_reserve(
        &mut self, additional: usize,
    ) -> Result<(), AllocError> {
        let old_len = self.len();
        let new_cap = self.grow_policy(additional);
        self.reserve_capacity(new_cap)?;
        debug_assert!(self.capacity() >= old_len + additional);
        Ok(())
    }

    /// Reserves capacity for inserting at least `additional` elements without
    /// reallocating. Does nothing if the capacity is already sufficient.
    ///
    /// The collection always reserves memory in multiples of the page size.
    ///
    /// # Panics
    ///
    /// Panics if the new capacity overflows `usize` or on OOM.
    #[inline]
    pub fn reserve(&mut self, additional: usize) {
        self.try_reserve(additional).unwrap();
    }

    /// Attempts to reserve capacity for `new_capacity` elements. Does nothing
    /// if the capacity is already sufficient.
    #[inline]
    fn reserve_capacity(
        &mut self, new_capacity: usize,
    ) -> Result<(), AllocError> {
        unsafe {
            if new_capacity <= self.capacity() {
                return Ok(());
            }

            let mut new_buffer = Buffer::uninitialized(2 * new_capacity)?;
            debug_assert!(new_buffer.len() >= 2 * new_capacity);

            let len = self.len();
            // Move the elements from the current buffer
            // to the beginning of the new buffer:
            {
                let from_ptr = self.as_mut_ptr();
                let to_ptr = new_buffer.as_mut_slice().as_mut_ptr();
                crate::ptr::copy_nonoverlapping(from_ptr, to_ptr, len);
            }

            // Exchange buffers
            crate::mem::swap(&mut self.buf, &mut new_buffer);

            // Correct the slice - we copied to the
            // beginning of the of the new buffer:
            self.elems_ = nonnull_raw_slice(self.buf.ptr(), len);
            Ok(())
        }
    }

    /// Reserves the minimum capacity for exactly `additional` more elements to
    /// be inserted in the given `SliceDeq<T>`. After calling `reserve_exact`,
    /// capacity will be greater than or equal to `self.len() + additional`.
    /// Does nothing if the capacity is already sufficient.
    ///
    /// Note that the allocator may give the collection more space than it
    /// requests. Therefore capacity can not be relied upon to be precisely
    /// minimal. Prefer `reserve` if future insertions are expected.
    ///
    /// # Panics
    ///
    /// Panics if the new capacity overflows `usize`.
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # fn main() {
    /// let mut deq = sdeq![1];
    /// deq.reserve_exact(10);
    /// assert!(deq.capacity() >= 11);
    /// # }
    /// ```
    #[inline]
    pub fn reserve_exact(&mut self, additional: usize) {
        let old_len = self.len();
        let new_cap = old_len.checked_add(additional).expect("overflow");
        self.reserve_capacity(new_cap).unwrap();
        debug_assert!(self.capacity() >= old_len + additional);
    }

    /// Growth policy of the deque. The capacity is going to be a multiple of
    /// the page-size anyways, so we just double capacity when needed.
    #[inline]
    fn grow_policy(&self, additional: usize) -> usize {
        let cur_cap = self.capacity();
        let old_len = self.len();
        let req_cap = old_len.checked_add(additional).expect("overflow");
        if req_cap > cur_cap {
            let dbl_cap = cur_cap.saturating_mul(2);
            cmp::max(req_cap, dbl_cap)
        } else {
            req_cap
        }
    }

    /// Moves the deque head by `x`.
    ///
    /// # Panics
    ///
    /// If the head wraps over the tail the behavior is undefined, that is,
    /// if `x` is out-of-range `[-(capacity() - len()), len()]`.
    ///
    /// If `-C debug-assertions=1` violating this pre-condition `panic!`s.
    ///
    /// # Unsafe
    ///
    /// It does not `drop` nor initialize elements, it just moves where the
    /// tail of the deque points to within the allocated buffer.
    #[inline]
    pub unsafe fn move_head_unchecked(&mut self, x: isize) {
        let cap = self.capacity();
        let len = self.len();
        // Make sure that the begin does not wrap over the end:
        debug_assert!(x >= -((cap - len) as isize));
        debug_assert!(x <= len as isize);

        // Obtain the begin of the slice and offset it by x:
        let mut new_begin = self.as_mut_ptr().offset(x) as usize;

        // Compute the boundaries of the first and second memory regions:
        let first_region_begin = self.buf.ptr() as usize;
        let region_size = Buffer::<T>::size_in_bytes(self.buf.len()) / 2;
        debug_assert!(cap * mem::size_of::<T>() <= region_size);
        let second_region_begin = first_region_begin + region_size;

        // If the new begin is not inside the first memory region, we shift it
        // by the region size into it:
        if new_begin < first_region_begin {
            new_begin += region_size;
        } else if new_begin >= second_region_begin {
            // Should be within the second region:
            debug_assert!(new_begin < second_region_begin + region_size);
            new_begin -= region_size;
        }
        debug_assert!(new_begin >= first_region_begin);
        debug_assert!(new_begin < second_region_begin);

        // The new begin is now in the first memory region:
        let new_begin = new_begin as *mut T;
        debug_assert!(in_bounds(
            slice::from_raw_parts(self.buf.ptr() as *mut u8, region_size),
            new_begin as *mut u8
        ));

        let new_len = len as isize - x;
        debug_assert!(
            new_len >= 0,
            "len = {}, x = {}, new_len = {}",
            len,
            x,
            new_len
        );
        debug_assert!(new_len <= cap as isize);
        self.elems_ = nonnull_raw_slice(new_begin, new_len as usize);
    }

    /// Moves the deque head by `x`.
    ///
    /// # Panics
    ///
    /// If the `head` wraps over the `tail`, that is, if `x` is out-of-range
    /// `[-(capacity() - len()), len()]`.
    ///
    /// # Unsafe
    ///
    /// It does not `drop` nor initialize elements, it just moves where the
    /// tail of the deque points to within the allocated buffer.
    #[inline]
    pub unsafe fn move_head(&mut self, x: isize) {
        assert!(
            x >= -((self.capacity() - self.len()) as isize)
                && x <= self.len() as isize
        );
        self.move_head_unchecked(x)
    }

    /// Moves the deque tail by `x`.
    ///
    /// # Panics
    ///
    /// If the `tail` wraps over the `head` the behavior is undefined, that is,
    /// if `x` is out-of-range `[-len(), capacity() - len()]`.
    ///
    /// If `-C debug-assertions=1` violating this pre-condition `panic!`s.
    ///
    /// # Unsafe
    ///
    /// It does not `drop` nor initialize elements, it just moves where the
    /// tail of the deque points to within the allocated buffer.
    #[inline]
    pub unsafe fn move_tail_unchecked(&mut self, x: isize) {
        // Make sure that the end does not wrap over the begin:
        let len = self.len();
        let cap = self.capacity();
        debug_assert!(x >= -(len as isize));
        debug_assert!(x <= (cap - len) as isize);

        let new_len = len as isize + x;
        debug_assert!(new_len >= 0);
        debug_assert!(new_len <= cap as isize);

        self.elems_ = nonnull_raw_slice(self.as_mut_ptr(), new_len as usize);
    }

    /// Moves the deque tail by `x`.
    ///
    /// # Panics
    ///
    /// If the `tail` wraps over the `head`, that is, if `x` is out-of-range
    /// `[-len(), capacity() - len()]`.
    ///
    /// # Unsafe
    ///
    /// It does not `drop` nor initialize elements, it just moves where the
    /// tail of the deque points to within the allocated buffer.
    #[inline]
    pub unsafe fn move_tail(&mut self, x: isize) {
        assert!(
            x >= -(self.len() as isize)
                && x <= (self.capacity() - self.len()) as isize
        );
        self.move_tail_unchecked(x);
    }

    /// Appends elements to `self` from `other`.
    #[inline]
    unsafe fn append_elements(&mut self, other: *const [T]) {
        let count = (*other).len();
        self.reserve(count);
        let len = self.len();
        ptr::copy_nonoverlapping(
            other as *const T,
            self.get_unchecked_mut(len),
            count,
        );
        self.move_tail_unchecked(count as isize);
    }

    /// Steal the elements from the slice `s`. You should `mem::forget` the
    /// slice afterwards.
    pub unsafe fn steal_from_slice(s: &[T]) -> Self {
        let mut deq = Self::new();
        deq.append_elements(s as *const _);
        deq
    }

    /// Moves all the elements of `other` into `Self`, leaving `other` empty.
    ///
    /// # Panics
    ///
    /// Panics if the number of elements in the deque overflows a `isize`.
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq![1, 2, 3];
    /// let mut deq2 = sdeq![4, 5, 6];
    /// deq.append(&mut deq2);
    /// assert_eq!(deq, [1, 2, 3, 4, 5, 6]);
    /// assert_eq!(deq2, []);
    /// # }
    /// ```
    #[inline]
    pub fn append(&mut self, other: &mut Self) {
        unsafe {
            self.append_elements(other.as_slice() as _);
            other.elems_ = nonnull_raw_slice(other.buf.ptr(), 0);
        }
    }

    /// Provides a reference to the first element, or `None` if the deque is
    /// empty.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slice_deque::SliceDeque;
    /// let mut deq = SliceDeque::new();
    /// assert_eq!(deq.front(), None);
    ///
    /// deq.push_back(1);
    /// deq.push_back(2);
    /// assert_eq!(deq.front(), Some(&1));
    /// deq.push_front(3);
    /// assert_eq!(deq.front(), Some(&3));
    /// ```
    #[inline]
    pub fn front(&self) -> Option<&T> {
        self.get(0)
    }

    /// Provides a mutable reference to the first element, or `None` if the
    /// deque is empty.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slice_deque::SliceDeque;
    /// let mut deq = SliceDeque::new();
    /// assert_eq!(deq.front(), None);
    ///
    /// deq.push_back(1);
    /// deq.push_back(2);
    /// assert_eq!(deq.front(), Some(&1));
    /// (*deq.front_mut().unwrap()) = 3;
    /// assert_eq!(deq.front(), Some(&3));
    /// ```
    #[inline]
    pub fn front_mut(&mut self) -> Option<&mut T> {
        self.get_mut(0)
    }

    /// Provides a reference to the last element, or `None` if the deque is
    /// empty.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slice_deque::SliceDeque;
    /// let mut deq = SliceDeque::new();
    /// assert_eq!(deq.back(), None);
    ///
    /// deq.push_back(1);
    /// deq.push_back(2);
    /// assert_eq!(deq.back(), Some(&2));
    /// deq.push_front(3);
    /// assert_eq!(deq.back(), Some(&2));
    /// ```
    #[inline]
    pub fn back(&self) -> Option<&T> {
        let last_idx = self.len().wrapping_sub(1);
        self.get(last_idx)
    }

    /// Provides a mutable reference to the last element, or `None` if the
    /// deque is empty.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slice_deque::SliceDeque;
    /// let mut deq = SliceDeque::new();
    /// assert_eq!(deq.front(), None);
    ///
    /// deq.push_back(1);
    /// deq.push_back(2);
    /// assert_eq!(deq.back(), Some(&2));
    /// (*deq.back_mut().unwrap()) = 3;
    /// assert_eq!(deq.back(), Some(&3));
    /// ```
    #[inline]
    pub fn back_mut(&mut self) -> Option<&mut T> {
        let last_idx = self.len().wrapping_sub(1);
        self.get_mut(last_idx)
    }

    /// Attempts to prepend `value` to the deque.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use slice_deque::SliceDeque;
    /// let mut deq = SliceDeque::new();
    /// deq.try_push_front(1).unwrap();
    /// deq.try_push_front(2).unwrap();
    /// assert_eq!(deq.front(), Some(&2));
    /// ```
    #[inline]
    pub fn try_push_front(&mut self, value: T) -> Result<(), (T, AllocError)> {
        unsafe {
            if intrinsics::unlikely(self.is_full()) {
                if let Err(e) = self.try_reserve(1) {
                    return Err((value, e));
                }
            }

            self.move_head_unchecked(-1);
            ptr::write(self.get_mut(0).unwrap(), value);
            Ok(())
        }
    }

    /// Prepends `value` to the deque.
    ///
    /// # Panics
    ///
    /// On OOM.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use slice_deque::SliceDeque;
    /// let mut deq = SliceDeque::new();
    /// deq.push_front(1);
    /// deq.push_front(2);
    /// assert_eq!(deq.front(), Some(&2));
    /// ```
    #[inline]
    pub fn push_front(&mut self, value: T) {
        if let Err(e) = self.try_push_front(value) {
            panic!("{:?}", e.1);
        }
    }

    /// Attempts to appends `value` to the deque.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use slice_deque::SliceDeque;
    /// let mut deq = SliceDeque::new();
    /// deq.try_push_back(1).unwrap();
    /// deq.try_push_back(3).unwrap();
    /// assert_eq!(deq.back(), Some(&3));
    /// ```
    #[inline]
    pub fn try_push_back(&mut self, value: T) -> Result<(), (T, AllocError)> {
        unsafe {
            if intrinsics::unlikely(self.is_full()) {
                if let Err(e) = self.try_reserve(1) {
                    return Err((value, e));
                }
            }
            self.move_tail_unchecked(1);
            let len = self.len();
            ptr::write(self.get_mut(len - 1).unwrap(), value);
            Ok(())
        }
    }

    /// Appends `value` to the deque.
    ///
    /// # Panics
    ///
    /// On OOM.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use slice_deque::SliceDeque;
    /// let mut deq = SliceDeque::new();
    /// deq.push_back(1);
    /// deq.push_back(3);
    /// assert_eq!(deq.back(), Some(&3));
    /// ```
    #[inline]
    pub fn push_back(&mut self, value: T) {
        if let Err(e) = self.try_push_back(value) {
            panic!("{:?}", e.1);
        }
    }

    /// Removes the first element and returns it, or `None` if the deque is
    /// empty.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slice_deque::SliceDeque;
    /// let mut deq = SliceDeque::new();
    /// assert_eq!(deq.pop_front(), None);
    ///
    /// deq.push_back(1);
    /// deq.push_back(2);
    ///
    /// assert_eq!(deq.pop_front(), Some(1));
    /// assert_eq!(deq.pop_front(), Some(2));
    /// assert_eq!(deq.pop_front(), None);
    /// ```
    #[inline]
    pub fn pop_front(&mut self) -> Option<T> {
        unsafe {
            let v = match self.get_mut(0) {
                None => return None,
                Some(v) => ptr::read(v),
            };
            self.move_head_unchecked(1);
            Some(v)
        }
    }

    /// Removes the last element from the deque and returns it, or `None` if it
    /// is empty.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slice_deque::SliceDeque;
    /// let mut deq = SliceDeque::new();
    /// assert_eq!(deq.pop_back(), None);
    ///
    /// deq.push_back(1);
    /// deq.push_back(3);
    ///
    /// assert_eq!(deq.pop_back(), Some(3));
    /// assert_eq!(deq.pop_back(), Some(1));
    /// assert_eq!(deq.pop_back(), None);
    /// ```
    #[inline]
    pub fn pop_back(&mut self) -> Option<T> {
        unsafe {
            let len = self.len();
            let v = match self.get_mut(len.wrapping_sub(1)) {
                None => return None,
                Some(v) => ptr::read(v),
            };
            self.move_tail_unchecked(-1);
            Some(v)
        }
    }

    /// Shrinks the capacity of the deque as much as possible.
    ///
    /// It will drop down as close as possible to the length, but because
    /// `SliceDeque` allocates memory in multiples of the page size the deque
    /// might still have capacity for inserting new elements without
    /// reallocating.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use slice_deque::SliceDeque;
    /// let mut deq = SliceDeque::with_capacity(15);
    /// deq.extend(0..4);
    /// assert!(deq.capacity() >= 15);
    /// deq.shrink_to_fit();
    /// assert!(deq.capacity() >= 4);
    /// # let o: SliceDeque<u32> = deq;
    /// ```
    #[inline]
    pub fn shrink_to_fit(&mut self) {
        if unsafe { intrinsics::unlikely(self.is_empty()) } {
            return;
        }

        // FIXME: we should compute the capacity and only allocate a shrunk
        // deque if that's worth it.
        let mut new_sdeq = Self::with_capacity(self.len());
        if new_sdeq.capacity() < self.capacity() {
            unsafe {
                crate::ptr::copy_nonoverlapping(
                    self.as_mut_ptr(),
                    new_sdeq.as_mut_ptr(),
                    self.len(),
                );
                new_sdeq.elems_ =
                    nonnull_raw_slice(new_sdeq.buf.ptr(), self.len());
                mem::swap(self, &mut new_sdeq);
            }
        }
    }

    /// Shortens the deque by removing excess elements from the back.
    ///
    /// If `len` is greater than the SliceDeque's current length, this has no
    /// effect.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq![5, 10, 15];
    /// assert_eq!(deq, [5, 10, 15]);
    /// deq.truncate_back(1);
    /// assert_eq!(deq, [5]);
    /// # }
    /// ```
    #[inline]
    pub fn truncate_back(&mut self, len: usize) {
        unsafe {
            if len >= self.len() {
                return;
            }

            let diff = self.len() - len;
            let s = &mut self[len..] as *mut [_];
            // decrement tail before the drop_in_place(), so a panic on
            // Drop doesn't re-drop the just-failed value.
            self.move_tail(-(diff as isize));
            ptr::drop_in_place(&mut *s);
            debug_assert_eq!(self.len(), len);
        }
    }

    /// Shortens the deque by removing excess elements from the back.
    ///
    /// If `len` is greater than the SliceDeque's current length, this has no
    /// effect. See `truncate_back` for examples.
    #[inline]
    pub fn truncate(&mut self, len: usize) {
        self.truncate_back(len);
    }

    /// Shortens the deque by removing excess elements from the front.
    ///
    /// If `len` is greater than the SliceDeque's current length, this has no
    /// effect.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq![5, 10, 15];
    /// assert_eq!(deq, [5, 10, 15]);
    /// deq.truncate_front(1);
    /// assert_eq!(deq, [15]);
    /// # }
    /// ```
    #[inline]
    pub fn truncate_front(&mut self, len: usize) {
        unsafe {
            if len >= self.len() {
                return;
            }

            let diff = self.len() - len;
            let s = &mut self[..diff] as *mut [_];
            // increment head before the drop_in_place(), so a panic on
            // Drop doesn't re-drop the just-failed value.
            self.move_head(diff as isize);
            ptr::drop_in_place(&mut *s);
            debug_assert_eq!(self.len(), len);
        }
    }

    /// Creates a draining iterator that removes the specified range in the
    /// deque and yields the removed items.
    ///
    /// Note 1: The element range is removed even if the iterator is only
    /// partially consumed or not consumed at all.
    ///
    /// Note 2: It is unspecified how many elements are removed from the deque
    /// if the `Drain` value is leaked.
    ///
    /// # Panics
    ///
    /// Panics if the starting point is greater than the end point or if
    /// the end point is greater than the length of the deque.
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq![1, 2, 3];
    /// let u: Vec<_> = deq.drain(1..).collect();
    /// assert_eq!(deq, &[1]);
    /// assert_eq!(u, &[2, 3]);
    ///
    /// // A full range clears the deque
    /// deq.drain(..);
    /// assert_eq!(deq, &[]);
    /// # }
    /// ```
    #[inline]
    #[allow(clippy::needless_pass_by_value)]
    pub fn drain<R>(&mut self, range: R) -> Drain<T>
    where
        R: ops::RangeBounds<usize>,
    {
        use ops::Bound::{Excluded, Included, Unbounded};
        // Memory safety
        //
        // When the Drain is first created, it shortens the length of
        // the source deque to make sure no uninitalized or moved-from
        // elements are accessible at all if the Drain's destructor
        // never gets to run.
        //
        // Drain will ptr::read out the values to remove.
        // When finished, remaining tail of the deque is copied back to cover
        // the hole, and the deque length is restored to the new length.
        //
        let len = self.len();
        let start = match range.start_bound() {
            Included(&n) => n,
            Excluded(&n) => n + 1,
            Unbounded => 0,
        };
        let end = match range.end_bound() {
            Included(&n) => n + 1,
            Excluded(&n) => n,
            Unbounded => len,
        };
        assert!(start <= end);
        assert!(end <= len);

        unsafe {
            // set self.deq length's to start, to be safe in case Drain is
            // leaked
            self.elems_ = nonnull_raw_slice(self.as_mut_ptr(), start);
            // Use the borrow in the IterMut to indicate borrowing behavior of
            // the whole Drain iterator (like &mut T).
            let range_slice = slice::from_raw_parts_mut(
                if mem::size_of::<T>() == 0 {
                    intrinsics::arith_offset(
                        self.as_mut_ptr() as *mut i8,
                        start as _,
                    ) as *mut _
                } else {
                    self.as_mut_ptr().add(start)
                },
                end - start,
            );
            Drain {
                tail_start: end,
                tail_len: len - end,
                iter: range_slice.iter(),
                deq: NonNull::from(self),
            }
        }
    }

    /// Removes all values from the deque.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq![1];
    /// assert!(!deq.is_empty());
    /// deq.clear();
    /// assert!(deq.is_empty());
    /// # }
    /// ```
    #[inline]
    pub fn clear(&mut self) {
        self.truncate(0);
    }

    /// Removes the element at `index` and return it in `O(1)` by swapping the
    /// last element into its place.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use slice_deque::SliceDeque;
    /// let mut deq = SliceDeque::new();
    /// assert_eq!(deq.swap_remove_back(0), None);
    /// deq.extend(1..4);
    /// assert_eq!(deq, [1, 2, 3]);
    ///
    /// assert_eq!(deq.swap_remove_back(0), Some(1));
    /// assert_eq!(deq, [3, 2]);
    /// ```
    #[inline]
    pub fn swap_remove_back(&mut self, index: usize) -> Option<T> {
        let len = self.len();
        if self.is_empty() {
            None
        } else {
            self.swap(index, len - 1);
            self.pop_back()
        }
    }

    /// Removes the element at `index` and returns it in `O(1)` by swapping the
    /// first element into its place.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slice_deque::SliceDeque;
    /// let mut deq = SliceDeque::new();
    /// assert_eq!(deq.swap_remove_front(0), None);
    /// deq.extend(1..4);
    /// assert_eq!(deq, [1, 2, 3]);
    ///
    /// assert_eq!(deq.swap_remove_front(2), Some(3));
    /// assert_eq!(deq, [2, 1]);
    /// ```
    #[inline]
    pub fn swap_remove_front(&mut self, index: usize) -> Option<T> {
        if self.is_empty() {
            None
        } else {
            self.swap(index, 0);
            self.pop_front()
        }
    }

    /// Inserts an `element` at `index` within the deque, shifting all elements
    /// with indices greater than or equal to `index` towards the back.
    ///
    /// Element at index 0 is the front of the queue.
    ///
    /// # Panics
    ///
    /// Panics if `index` is greater than deque's length
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq!['a', 'b', 'c'];
    /// assert_eq!(deq, &['a', 'b', 'c']);
    ///
    /// deq.insert(1, 'd');
    /// assert_eq!(deq, &['a', 'd', 'b', 'c']);
    /// # }
    /// ```
    #[inline]
    pub fn insert(&mut self, index: usize, element: T) {
        unsafe {
            let len = self.len();
            assert!(index <= len);

            if intrinsics::unlikely(self.is_full()) {
                self.reserve(1);
                // TODO: when the deque needs to grow, reserve should
                // copy the memory to the new storage leaving a whole
                // at the index where the new elements are to be inserted
                // to avoid having to copy the memory again
            }

            let p = if index > self.len() / 2 {
                let p = self.as_mut_ptr().add(index);
                // Shift elements towards the back
                ptr::copy(p, p.add(1), len - index);
                self.move_tail_unchecked(1);
                p
            } else {
                // Shift elements towards the front
                self.move_head_unchecked(-1);
                let p = self.as_mut_ptr().add(index);
                ptr::copy(p, p.sub(1), index);
                p
            };
            ptr::write(p, element); // Overwritte
        }
    }

    /// Removes and returns the element at position `index` within the deque.
    ///
    /// # Panics
    ///
    /// Panics if `index` is out of bounds.
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq![1, 2, 3, 4, 5];
    /// assert_eq!(deq.remove(1), 2);
    /// assert_eq!(deq, [1, 3, 4, 5]);
    /// # }
    /// ```
    #[inline]
    #[allow(clippy::shadow_unrelated)] // FIXME: bug in clippy due to ptr
    pub fn remove(&mut self, index: usize) -> T {
        let len = self.len();
        assert!(index < len);
        unsafe {
            // copy element at pointer:
            let ptr = self.as_mut_ptr().add(index);
            let ret = ptr::read(ptr);
            if index > self.len() / 2 {
                // If the index is close to the back, shift elements from the
                // back towards the front
                ptr::copy(ptr.add(1), ptr, len - index - 1);
                self.move_tail_unchecked(-1);
            } else {
                // If the index is close to the front, shift elements from the
                // front towards the back
                let ptr = self.as_mut_ptr();
                ptr::copy(ptr, ptr.add(1), index);
                self.move_head_unchecked(1);
            }

            ret
        }
    }

    /// Splits the collection into two at the given index.
    ///
    /// Returns a newly allocated `Self`. `self` contains elements `[0, at)`,
    /// and the returned `Self` contains elements `[at, len)`.
    ///
    /// Note that the capacity of `self` does not change.
    ///
    /// # Panics
    ///
    /// Panics if `at > len`.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq![1, 2, 3];
    /// let deq2 = deq.split_off(1);
    /// assert_eq!(deq, [1]);
    /// assert_eq!(deq2, [2, 3]);
    /// # }
    /// ```
    #[inline]
    pub fn split_off(&mut self, at: usize) -> Self {
        assert!(at <= self.len(), "`at` out of bounds");

        let other_len = self.len() - at;
        let mut other = Self::with_capacity(other_len);

        unsafe {
            self.move_tail_unchecked(-(other_len as isize));
            other.move_tail_unchecked(other_len as isize);

            ptr::copy_nonoverlapping(
                self.as_ptr().add(at),
                other.as_mut_ptr(),
                other.len(),
            );
        }
        other
    }

    /// Retains only the elements specified by the predicate.
    ///
    /// That is, remove all elements `e` such that `f(&e)` returns `false`.
    /// This method operates in place and preserves the order of the
    /// retained elements.
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq![1, 2, 3, 4];
    /// deq.retain(|&x| x % 2 == 0);
    /// assert_eq!(deq, [2, 4]);
    /// # }
    /// ```
    #[inline]
    pub fn retain<F>(&mut self, mut f: F)
    where
        F: FnMut(&T) -> bool,
    {
        let len = self.len();
        let mut del = 0;
        {
            let v = &mut **self;

            for i in 0..len {
                if !f(&v[i]) {
                    del += 1;
                } else if del > 0 {
                    v.swap(i - del, i);
                }
            }
        }
        if del > 0 {
            self.truncate(len - del);
        }
    }

    /// Removes all but the first of consecutive elements in the deque that
    /// resolve to the same key.
    ///
    /// If the deque is sorted, this removes all duplicates.
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq![10, 20, 21, 30, 20];
    ///
    /// deq.dedup_by_key(|i| *i / 10);
    /// assert_eq!(deq, [10, 20, 30, 20]);
    /// # }
    /// ```
    #[inline]
    pub fn dedup_by_key<F, K>(&mut self, mut key: F)
    where
        F: FnMut(&mut T) -> K,
        K: PartialEq,
    {
        self.dedup_by(|a, b| key(a) == key(b))
    }

    /// Removes all but the first of consecutive elements in the deque
    /// satisfying a given equality relation.
    ///
    /// The `same_bucket` function is passed references to two elements from
    /// the deque, and returns `true` if the elements compare equal, or
    /// `false` if they do not. The elements are passed in opposite order
    /// from their order in the deque, so if `same_bucket(a, b)` returns
    /// `true`, `a` is removed.
    ///
    /// If the deque is sorted, this removes all duplicates.
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq!["foo", "bar", "Bar", "baz", "bar"];
    ///
    /// deq.dedup_by(|a, b| a.eq_ignore_ascii_case(b));
    ///
    /// assert_eq!(deq, ["foo", "bar", "baz", "bar"]);
    /// # }
    /// ```
    #[inline]
    pub fn dedup_by<F>(&mut self, mut same_bucket: F)
    where
        F: FnMut(&mut T, &mut T) -> bool,
    {
        unsafe {
            // Although we have a mutable reference to `self`, we cannot make
            // *arbitrary* changes. The `same_bucket` calls could panic, so we
            // must ensure that the deque is in a valid state at all time.
            //
            // The way that we handle this is by using swaps; we iterate
            // over all the elements, swapping as we go so that at the end
            // the elements we wish to keep are in the front, and those we
            // wish to reject are at the back. We can then truncate the
            // deque. This operation is still O(n).
            //
            // Example: We start in this state, where `r` represents "next
            // read" and `w` represents "next_write`.
            //
            //           r
            //     +---+---+---+---+---+---+
            //     | 0 | 1 | 1 | 2 | 3 | 3 |
            //     +---+---+---+---+---+---+
            //           w
            //
            // Comparing self[r] against self[w-1], this is not a duplicate, so
            // we swap self[r] and self[w] (no effect as r==w) and then
            // increment both r and w, leaving us with:
            //
            //               r
            //     +---+---+---+---+---+---+
            //     | 0 | 1 | 1 | 2 | 3 | 3 |
            //     +---+---+---+---+---+---+
            //               w
            //
            // Comparing self[r] against self[w-1], this value is a duplicate,
            // so we increment `r` but leave everything else unchanged:
            //
            //                   r
            //     +---+---+---+---+---+---+
            //     | 0 | 1 | 1 | 2 | 3 | 3 |
            //     +---+---+---+---+---+---+
            //               w
            //
            // Comparing self[r] against self[w-1], this is not a duplicate,
            // so swap self[r] and self[w] and advance r and w:
            //
            //                       r
            //     +---+---+---+---+---+---+
            //     | 0 | 1 | 2 | 1 | 3 | 3 |
            //     +---+---+---+---+---+---+
            //                   w
            //
            // Not a duplicate, repeat:
            //
            //                           r
            //     +---+---+---+---+---+---+
            //     | 0 | 1 | 2 | 3 | 1 | 3 |
            //     +---+---+---+---+---+---+
            //                       w
            //
            // Duplicate, advance r. End of deque. Truncate to w.

            let ln = self.len();
            if intrinsics::unlikely(ln <= 1) {
                return;
            }

            // Avoid bounds checks by using raw pointers.
            let p = self.as_mut_ptr();
            let mut r: usize = 1;
            let mut w: usize = 1;

            while r < ln {
                let p_r = p.add(r);
                let p_wm1 = p.add(w - 1);
                if !same_bucket(&mut *p_r, &mut *p_wm1) {
                    if r != w {
                        let p_w = p_wm1.add(1);
                        mem::swap(&mut *p_r, &mut *p_w);
                    }
                    w += 1;
                }
                r += 1;
            }

            self.truncate(w);
        }
    }

    /// Extend the `SliceDeque` by `n` values, using the given generator.
    #[inline]
    fn extend_with<E: ExtendWith<T>>(&mut self, n: usize, value: E) {
        self.reserve(n);

        unsafe {
            let mut ptr = self.as_mut_ptr().add(self.len());

            // Write all elements except the last one
            for _ in 1..n {
                ptr::write(ptr, value.next());
                ptr = ptr.add(1);
                // Increment the length in every step in case next() panics
                self.move_tail_unchecked(1);
            }

            if n > 0 {
                // We can write the last element directly without cloning
                // needlessly
                ptr::write(ptr, value.last());
                self.move_tail_unchecked(1);
            }

            // len set by scope guard
        }
    }

    /// Extend for a general iterator.
    ///
    /// This function should be the moral equivalent of:
    ///
    /// >  for item in iterator {
    /// >      self.push_back(item);
    /// >  }
    #[inline]
    fn extend_desugared<I: Iterator<Item = T>>(&mut self, mut iterator: I) {
        #[allow(clippy::while_let_on_iterator)]
        while let Some(element) = iterator.next() {
            let len = self.len();
            let cap = self.capacity();
            if len == cap {
                let (lower, upper) = iterator.size_hint();
                let additional_cap = if let Some(upper) = upper {
                    upper
                } else {
                    lower
                }
                .checked_add(1)
                .expect("overflow");
                self.reserve(additional_cap);
            }
            debug_assert!(self.len() < self.capacity());
            unsafe {
                ptr::write(self.get_unchecked_mut(len), element);
                // NB can't overflow since we would have had to alloc the
                // address space
                self.move_tail_unchecked(1);
            }
        }
    }

    /// Creates a splicing iterator that replaces the specified range in the
    /// deque with the given `replace_with` iterator and yields the
    /// removed items. `replace_with` does not need to be the same length
    /// as `range`.
    ///
    /// Note 1: The element range is removed even if the iterator is not
    /// consumed until the end.
    ///
    /// Note 2: It is unspecified how many elements are removed from the deque,
    /// if the `Splice` value is leaked.
    ///
    /// Note 3: The input iterator `replace_with` is only consumed
    /// when the `Splice` value is dropped.
    ///
    /// Note 4: This is optimal if:
    ///
    /// * The tail (elements in the deque after `range`) is empty,
    /// * or `replace_with` yields fewer elements than `range`’s length
    /// * or the lower bound of its `size_hint()` is exact.
    ///
    /// Otherwise, a temporary deque is allocated and the tail is moved twice.
    ///
    /// # Panics
    ///
    /// Panics if the starting point is greater than the end point or if
    /// the end point is greater than the length of the deque.
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq![1, 2, 3];
    /// let new = [7, 8];
    /// let u: SliceDeque<_> = deq.splice(..2, new.iter().cloned()).collect();
    /// assert_eq!(deq, &[7, 8, 3]);
    /// assert_eq!(u, &[1, 2]);
    /// # }
    /// ```
    #[inline]
    pub fn splice<R, I>(
        &mut self, range: R, replace_with: I,
    ) -> Splice<I::IntoIter>
    where
        R: ops::RangeBounds<usize>,
        I: IntoIterator<Item = T>,
    {
        Splice {
            drain: self.drain(range),
            replace_with: replace_with.into_iter(),
        }
    }

    /// Creates an iterator which uses a closure to determine if an element
    /// should be removed.
    ///
    /// If the closure returns `true`, then the element is removed and yielded.
    /// If the closure returns `false`, it will try again, and call the closure
    /// on the next element, seeing if it passes the test.
    ///
    /// Using this method is equivalent to the following code:
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// # let some_predicate = |x: &mut i32| { *x == 2 || *x == 3 || *x == 6
    /// # };
    /// let mut deq = SliceDeque::new();
    /// deq.extend(1..7);
    /// let mut i = 0;
    /// while i != deq.len() {
    ///     if some_predicate(&mut deq[i]) {
    ///         let val = deq.remove(i);
    ///     // your code here
    ///     } else {
    ///         i += 1;
    ///     }
    /// }
    /// # let mut expected = sdeq![1, 4, 5];
    /// # assert_eq!(deq, expected);
    /// # }
    /// ```
    ///
    /// But `drain_filter` is easier to use. `drain_filter` is also more
    /// efficient, because it can backshift the elements of the deque in
    /// bulk.
    ///
    /// Note that `drain_filter` also lets you mutate every element in the
    /// filter closure, regardless of whether you choose to keep or remove
    /// it.
    ///
    ///
    /// # Examples
    ///
    /// Splitting a deque into evens and odds, reusing the original allocation:
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut numbers = sdeq![1, 2, 3, 4, 5, 6, 8, 9, 11, 13, 14, 15];
    ///
    /// let evens = numbers
    ///     .drain_filter(|x| *x % 2 == 0)
    ///     .collect::<SliceDeque<_>>();
    /// let odds = numbers;
    ///
    /// assert_eq!(sdeq![2, 4, 6, 8, 14], evens);
    /// assert_eq!(odds, sdeq![1, 3, 5, 9, 11, 13, 15]);
    /// # }
    /// ```
    #[inline]
    pub fn drain_filter<F>(&mut self, filter: F) -> DrainFilter<T, F>
    where
        F: FnMut(&mut T) -> bool,
    {
        let old_len = self.len();

        // Guard against us getting leaked (leak amplification)
        unsafe {
            self.move_tail_unchecked(-(old_len as isize));
        }

        DrainFilter {
            deq: self,
            idx: 0,
            del: 0,
            old_len,
            pred: filter,
        }
    }
}

impl<T> SliceDeque<T>
where
    T: Clone,
{
    /// Clones and appends all elements in a slice to the `SliceDeque`.
    ///
    /// Iterates over the slice `other`, clones each element, and then appends
    /// it to this `SliceDeque`. The `other` slice is traversed in-order.
    ///
    /// Note that this function is same as `extend` except that it is
    /// specialized to work with slices instead. If and when Rust gets
    /// specialization this function will likely be deprecated (but still
    /// available).
    ///
    /// # Examples
    ///
    /// ```
    /// # use slice_deque::SliceDeque;
    /// let mut deq = SliceDeque::new();
    /// deq.push_back(1);
    /// deq.extend_from_slice(&[2, 3, 4]);
    /// assert_eq!(deq, [1, 2, 3, 4]);
    /// ```
    #[inline]
    pub fn extend_from_slice(&mut self, other: &[T]) {
        #[cfg(feature = "unstable")]
        {
            self.spec_extend(other.iter())
        }
        #[cfg(not(feature = "unstable"))]
        {
            self.reserve(other.len());
            unsafe {
                let len = self.len();
                self.move_tail_unchecked(other.len() as isize);
                self.get_unchecked_mut(len..).clone_from_slice(other);
            }
        }
    }

    /// Modifies the `SliceDeque` in-place so that `len()` is equal to
    /// `new_len`, either by removing excess elements or by appending clones of
    /// `value` to the back.
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq![5, 10, 15];
    /// assert_eq!(deq, [5, 10, 15]);
    ///
    /// deq.resize(2, 0);
    /// assert_eq!(deq, [5, 10]);
    ///
    /// deq.resize(5, 20);
    /// assert_eq!(deq, [5, 10, 20, 20, 20]);
    /// # }
    /// ```
    #[inline]
    pub fn resize(&mut self, new_len: usize, value: T) {
        let len = self.len();

        if new_len > len {
            self.reserve(new_len - len);
            while self.len() < new_len {
                self.push_back(value.clone());
            }
        } else {
            self.truncate(new_len);
        }
        debug_assert!(self.len() == new_len);
    }
}

impl<T: Default> SliceDeque<T> {
    /// Resizes the `SliceDeque` in-place so that `len` is equal to `new_len`.
    ///
    /// If `new_len` is greater than `len`, the `SliceDeque` is extended by the
    /// difference, with each additional slot filled with `Default::default()`.
    /// If `new_len` is less than `len`, the `SliceDeque` is simply truncated.
    ///
    /// This method uses `Default` to create new values on every push. If
    /// you'd rather `Clone` a given value, use [`resize`].
    ///
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq![1, 2, 3];
    /// deq.resize_default(5);
    /// assert_eq!(deq, [1, 2, 3, 0, 0]);
    ///
    /// deq.resize_default(2);
    /// assert_eq!(deq, [1, 2]);
    /// # }
    /// ```
    ///
    /// [`resize`]: #method.resize
    #[inline]
    pub fn resize_default(&mut self, new_len: usize) {
        let len = self.len();

        if new_len > len {
            self.extend_with(new_len - len, ExtendDefault);
        } else {
            self.truncate(new_len);
        }
    }
}

impl<T: PartialEq> SliceDeque<T> {
    /// Removes consecutive repeated elements in the deque.
    ///
    /// If the deque is sorted, this removes all duplicates.
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq![1, 2, 2, 3, 2];
    ///
    /// deq.dedup();
    /// assert_eq!(deq, [1, 2, 3, 2]);
    ///
    /// deq.sort();
    /// assert_eq!(deq, [1, 2, 2, 3]);
    ///
    /// deq.dedup();
    /// assert_eq!(deq, [1, 2, 3]);
    /// # }
    /// ```
    #[inline]
    pub fn dedup(&mut self) {
        self.dedup_by(|a, b| a == b)
    }

    /// Removes the first instance of `item` from the deque if the item exists.
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq![1, 2, 3, 1];
    ///
    /// deq.remove_item(&1);
    /// assert_eq!(deq, &[2, 3, 1]);
    /// deq.remove_item(&1);
    /// assert_eq!(deq, &[2, 3]);
    /// # }
    /// ```
    #[inline]
    pub fn remove_item(&mut self, item: &T) -> Option<T> {
        let pos = match self.iter().position(|x| *x == *item) {
            Some(x) => x,
            None => return None,
        };
        Some(self.remove(pos))
    }
}

impl<T: fmt::Debug> fmt::Debug for SliceDeque<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "{:?}", self.as_slice())
        /*
         write!(
             f,
             // TODO: "SliceDeque({:?})",
             "SliceDeque(len: {}, cap: {}, head: {}, tail: {}, elems: {:?})",
             self.len(),
             self.capacity(),
             self.head(),
             self.tail(),
             self.as_slice()
         )
        */
    }
}

impl<T> Drop for SliceDeque<T> {
    #[inline]
    fn drop(&mut self) {
        // In Rust, if Drop::drop panics, the value must be leaked,
        // therefore we don't need to make sure that we handle that case
        // here:
        unsafe {
            // use drop for [T]
            ptr::drop_in_place(&mut self[..]);
        }
        // Buffer handles deallocation
    }
}

impl<T> ops::Deref for SliceDeque<T> {
    type Target = [T];
    #[inline]
    fn deref(&self) -> &Self::Target {
        self.as_slice()
    }
}

impl<T> ops::DerefMut for SliceDeque<T> {
    #[inline]
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.as_mut_slice()
    }
}

impl<T> Default for SliceDeque<T> {
    #[inline]
    fn default() -> Self {
        Self::new()
    }
}

impl<T: Clone> Clone for SliceDeque<T> {
    #[inline]
    fn clone(&self) -> Self {
        let mut new = Self::with_capacity(self.len());
        for i in self.iter() {
            new.push_back(i.clone());
        }
        new
    }
    #[inline]
    fn clone_from(&mut self, other: &Self) {
        self.clear();
        for i in other.iter() {
            self.push_back(i.clone());
        }
    }
}

impl<'a, T: Clone> From<&'a [T]> for SliceDeque<T> {
    #[inline]
    fn from(s: &'a [T]) -> Self {
        let mut new = Self::with_capacity(s.len());
        for i in s {
            new.push_back(i.clone());
        }
        new
    }
}

impl<'a, T: Clone> From<&'a mut [T]> for SliceDeque<T> {
    #[inline]
    fn from(s: &'a mut [T]) -> Self {
        let mut new = Self::with_capacity(s.len());
        for i in s {
            new.push_back(i.clone());
        }
        new
    }
}

impl<T: hash::Hash> hash::Hash for SliceDeque<T> {
    #[inline]
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        hash::Hash::hash(&**self, state)
    }
}

///////////////////////////////////////////////////////////////////////////////
// PartialEq implementations:

macro_rules! __impl_slice_eq1 {
    ($Lhs:ty, $Rhs:ty) => {
        __impl_slice_eq1! { $Lhs, $Rhs, Sized }
    };
    ($Lhs:ty, $Rhs:ty, $Bound:ident) => {
        impl<'a, 'b, A: $Bound, B> PartialEq<$Rhs> for $Lhs
        where
            A: PartialEq<B>,
        {
            #[inline]
            fn eq(&self, other: &$Rhs) -> bool {
                self[..] == other[..]
            }
        }
    };
}

__impl_slice_eq1! { SliceDeque<A>, SliceDeque<B> }
__impl_slice_eq1! { SliceDeque<A>, &'b [B] }
__impl_slice_eq1! { SliceDeque<A>, &'b mut [B] }

#[cfg(feature = "use_std")]
__impl_slice_eq1! { SliceDeque<A>, Vec<B> }

macro_rules! array_impls {
    ($($N: expr)+) => {
        $(
            // NOTE: some less important impls are omitted to reduce code bloat
            __impl_slice_eq1! { SliceDeque<A>, [B; $N] }
            __impl_slice_eq1! { SliceDeque<A>, &'b [B; $N] }
        )+
    }
}

array_impls! {
    0  1  2  3  4  5  6  7  8  9
        10 11 12 13 14 15 16 17 18 19
        20 21 22 23 24 25 26 27 28 29
        30 31 32
}

///////////////////////////////////////////////////////////////////////////////

impl<T: Eq> Eq for SliceDeque<T> {}

impl<T: PartialOrd> PartialOrd for SliceDeque<T> {
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<cmp::Ordering> {
        PartialOrd::partial_cmp(&**self, &**other)
    }
}

impl<'a, T: PartialOrd> PartialOrd<&'a [T]> for SliceDeque<T> {
    #[inline]
    fn partial_cmp(&self, other: &&'a [T]) -> Option<cmp::Ordering> {
        PartialOrd::partial_cmp(&**self, other)
    }
}

/// A draining iterator for `SliceDeque<T>`.
///
/// This `struct` is created by the [`drain`] method on [`SliceDeque`].
///
/// [`drain`]: struct.SliceDeque.html#method.drain
/// [`SliceDeque`]: struct.SliceDeque.html
pub struct Drain<'a, T: 'a> {
    /// Index of tail to preserve
    tail_start: usize,
    /// Length of tail
    tail_len: usize,
    /// Current remaining range to remove
    iter: slice::Iter<'a, T>,
    /// A shared mutable pointer to the deque (with shared ownership).
    deq: NonNull<SliceDeque<T>>,
}

impl<'a, T: 'a + fmt::Debug> fmt::Debug for Drain<'a, T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_tuple("Drain").field(&self.iter.as_slice()).finish()
    }
}

unsafe impl<'a, T: Sync> Sync for Drain<'a, T> {}
unsafe impl<'a, T: Send> Send for Drain<'a, T> {}

impl<'a, T> Iterator for Drain<'a, T> {
    type Item = T;

    #[inline]
    fn next(&mut self) -> Option<T> {
        self.iter
            .next()
            .map(|elt| unsafe { ptr::read(elt as *const _) })
    }
    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<'a, T> DoubleEndedIterator for Drain<'a, T> {
    #[inline]
    fn next_back(&mut self) -> Option<T> {
        self.iter
            .next_back()
            .map(|elt| unsafe { ptr::read(elt as *const _) })
    }
}

impl<'a, T> Drop for Drain<'a, T> {
    #[inline]
    fn drop(&mut self) {
        // exhaust self first
        self.for_each(|_| {});

        if self.tail_len > 0 {
            unsafe {
                let source_deq = self.deq.as_mut();
                // memmove back untouched tail, update to new length
                let start = source_deq.len();
                let tail = self.tail_start;
                let src = source_deq.as_ptr().add(tail);
                let dst = source_deq.as_mut_ptr().add(start);
                ptr::copy(src, dst, self.tail_len);
                source_deq.move_tail_unchecked(self.tail_len as isize);
            }
        }
    }
}

#[cfg(feature = "unstable")]
impl<'a, T> ExactSizeIterator for Drain<'a, T> {
    #[inline]
    fn is_empty(&self) -> bool {
        self.iter.is_empty()
    }
}

#[cfg(feature = "unstable")]
impl<'a, T> iter::FusedIterator for Drain<'a, T> {}

/// An iterator that moves out of a deque.
///
/// This `struct` is created by the `into_iter` method on
/// [`SliceDeque`][`SliceDeque`] (provided by the [`IntoIterator`] trait).
///
/// [`SliceDeque`]: struct.SliceDeque.html
/// [`IntoIterator`]: ../../std/iter/trait.IntoIterator.html
pub struct IntoIter<T> {
    /// NonNull pointer to the buffer
    buf: NonNull<T>,
    /// Capacity of the buffer.
    cap: usize,
    /// Pointer to the first element.
    ptr: *const T,
    /// Pointer to one-past-the-end.
    end: *const T,
}

impl<T: fmt::Debug> fmt::Debug for IntoIter<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_tuple("IntoIter").field(&self.as_slice()).finish()
    }
}

impl<T> IntoIter<T> {
    /// Returns the element slice
    #[cfg(feature = "unstable")]
    #[allow(clippy::option_unwrap_used)]
    #[inline]
    fn elems(&mut self) -> &mut [T] {
        unsafe {
            slice::from_raw_parts_mut(
                self.ptr as *mut _,
                (self.end as usize - self.ptr as usize) / mem::size_of::<T>(),
            )
        }
    }

    /// Returns the remaining items of this iterator as a slice.
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq!['a', 'b', 'c'];
    /// let mut into_iter = deq.into_iter();
    /// assert_eq!(into_iter.as_slice(), ['a', 'b', 'c']);
    /// let _ = into_iter.next().unwrap();
    /// assert_eq!(into_iter.as_slice(), ['b', 'c']);
    /// # }
    /// ```
    #[inline]
    pub fn as_slice(&self) -> &[T] {
        unsafe { slice::from_raw_parts(self.ptr, self.size_hint().0) }
    }

    /// Returns the remaining items of this iterator as a mutable slice.
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq!['a', 'b', 'c'];
    /// let mut into_iter = deq.into_iter();
    /// assert_eq!(into_iter.as_slice(), ['a', 'b', 'c']);
    /// into_iter.as_mut_slice()[2] = 'z';
    /// assert_eq!(into_iter.next().unwrap(), 'a');
    /// assert_eq!(into_iter.next().unwrap(), 'b');
    /// assert_eq!(into_iter.next().unwrap(), 'z');
    /// # }
    /// ```
    #[inline]
    pub fn as_mut_slice(&mut self) -> &mut [T] {
        unsafe {
            slice::from_raw_parts_mut(self.ptr as *mut T, self.size_hint().0)
        }
    }
}

unsafe impl<T: Send> Send for IntoIter<T> {}
unsafe impl<T: Sync> Sync for IntoIter<T> {}

impl<T> Iterator for IntoIter<T> {
    type Item = T;

    #[inline]
    fn next(&mut self) -> Option<T> {
        unsafe {
            if self.ptr as *const _ == self.end {
                None
            } else if mem::size_of::<T>() == 0 {
                // purposefully don't use 'ptr.offset' because for
                // deques with 0-size elements this would return the
                // same pointer.
                self.ptr = intrinsics::arith_offset(self.ptr as *const i8, 1)
                    as *mut T;

                // Use a non-null pointer value
                // (self.ptr might be null because of wrapping)
                Some(ptr::read(1 as *mut T))
            } else {
                let old = self.ptr;
                self.ptr = self.ptr.add(1);

                Some(ptr::read(old))
            }
        }
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        let exact = match self.ptr.wrapping_offset_from_(self.end) {
            Some(x) => x as usize,
            None => (self.end as usize).wrapping_sub(self.ptr as usize),
        };
        (exact, Some(exact))
    }

    #[inline]
    fn count(self) -> usize {
        self.size_hint().0
    }
}

impl<T> DoubleEndedIterator for IntoIter<T> {
    #[inline]
    fn next_back(&mut self) -> Option<T> {
        unsafe {
            if self.end == self.ptr {
                None
            } else if mem::size_of::<T>() == 0 {
                // See above for why 'ptr.offset' isn't used
                self.end = intrinsics::arith_offset(self.end as *const i8, -1)
                    as *mut T;

                // Use a non-null pointer value
                // (self.end might be null because of wrapping)
                Some(ptr::read(1 as *mut T))
            } else {
                self.end = self.end.offset(-1);

                Some(ptr::read(self.end))
            }
        }
    }
}

#[cfg(feature = "unstable")]
impl<T> ExactSizeIterator for IntoIter<T> {
    #[inline]
    fn is_empty(&self) -> bool {
        self.ptr == self.end
    }
}

#[cfg(feature = "unstable")]
impl<T> iter::FusedIterator for IntoIter<T> {}

#[cfg(feature = "unstable")]
unsafe impl<T> iter::TrustedLen for IntoIter<T> {}

impl<T: Clone> Clone for IntoIter<T> {
    #[inline]
    fn clone(&self) -> Self {
        let mut deq = SliceDeque::<T>::with_capacity(self.size_hint().0);
        unsafe {
            deq.append_elements(self.as_slice());
        }
        deq.into_iter()
    }
}

#[cfg(feature = "unstable")]
unsafe impl<#[may_dangle] T> Drop for IntoIter<T> {
    #[inline]
    fn drop(&mut self) {
        // destroy the remaining elements
        for _x in self.by_ref() {}

        // Buffer handles deallocation
        let _ =
            unsafe { Buffer::from_raw_parts(self.buf.as_ptr(), 2 * self.cap) };
    }
}

#[cfg(not(feature = "unstable"))]
impl<T> Drop for IntoIter<T> {
    #[inline]
    fn drop(&mut self) {
        // destroy the remaining elements
        for _x in self.by_ref() {}

        // Buffer handles deallocation
        let _ =
            unsafe { Buffer::from_raw_parts(self.buf.as_ptr(), 2 * self.cap) };
    }
}

impl<T> IntoIterator for SliceDeque<T> {
    type Item = T;
    type IntoIter = IntoIter<T>;

    /// Creates a consuming iterator, that is, one that moves each value out of
    /// the deque (from start to end). The deque cannot be used after calling
    /// this.
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate slice_deque;
    /// # use slice_deque::SliceDeque;
    /// # fn main() {
    /// let mut deq = sdeq!["a".to_string(), "b".to_string()];
    /// let expected = ["a".to_string(), "b".to_string()];
    /// for (i, s) in deq.into_iter().enumerate() {
    ///     // s has type String, not &String
    ///     println!("{}", s);
    ///     assert_eq!(s, expected[i]);
    /// }
    /// # }
    /// ```
    #[inline]
    fn into_iter(self) -> IntoIter<T> {
        unsafe {
            let buf_ptr = self.buf.ptr();
            intrinsics::assume(!buf_ptr.is_null());
            let begin = self.as_ptr();
            let end = if mem::size_of::<T>() == 0 {
                intrinsics::arith_offset(begin as *const i8, self.len() as _)
                    as *const _
            } else {
                begin.add(self.len())
            };
            assert!(begin as usize <= end as usize);
            let it = IntoIter {
                buf: NonNull::new_unchecked(buf_ptr),
                cap: self.capacity(),
                ptr: begin,
                end,
            };
            debug_assert_eq!(self.len(), it.size_hint().0);
            #[allow(clippy::mem_forget)]
            mem::forget(self);
            it
        }
    }
}

impl<'a, T> IntoIterator for &'a SliceDeque<T> {
    type Item = &'a T;
    type IntoIter = slice::Iter<'a, T>;
    #[inline]
    fn into_iter(self) -> slice::Iter<'a, T> {
        self.iter()
    }
}

impl<'a, T> IntoIterator for &'a mut SliceDeque<T> {
    type Item = &'a mut T;
    type IntoIter = slice::IterMut<'a, T>;
    #[inline]
    fn into_iter(self) -> slice::IterMut<'a, T> {
        self.iter_mut()
    }
}

impl<T> Extend<T> for SliceDeque<T> {
    #[inline]
    fn extend<I: IntoIterator<Item = T>>(&mut self, iter: I) {
        <Self as SpecExtend<T, I::IntoIter>>::spec_extend(
            self,
            iter.into_iter(),
        )
    }
}

/// Specialization trait used for `SliceDeque::from_iter` and
/// `SliceDeque::extend`.
trait SpecExtend<T, I> {
    /// Specialization for `SliceDeque::from_iter`.
    fn from_iter(iter: I) -> Self;
    /// Specialization for `SliceDeque::extend`.
    fn spec_extend(&mut self, iter: I);
}

/// Default implementation of `SpecExtend::from_iter`.
#[inline(always)]
fn from_iter_default<T, I: Iterator<Item = T>>(
    mut iterator: I,
) -> SliceDeque<T> {
    // Unroll the first iteration, as the deque is going to be
    // expanded on this iteration in every case when the iterable is not
    // empty, but the loop in extend_desugared() is not going to see the
    // deque being full in the few subsequent loop iterations.
    // So we get better branch prediction.
    let mut deque = match iterator.next() {
        None => return SliceDeque::<T>::new(),
        Some(element) => {
            let (lower, _) = iterator.size_hint();
            let mut deque =
                SliceDeque::<T>::with_capacity(lower.saturating_add(1));
            unsafe {
                ptr::write(deque.get_unchecked_mut(0), element);
                deque.move_tail_unchecked(1);
            }
            deque
        }
    };
    <SliceDeque<T> as SpecExtend<T, I>>::spec_extend(&mut deque, iterator);
    deque
}

impl<T, I> SpecExtend<T, I> for SliceDeque<T>
where
    I: Iterator<Item = T>,
{
    #[cfg(feature = "unstable")]
    default fn from_iter(iterator: I) -> Self {
        from_iter_default(iterator)
    }

    #[cfg(feature = "unstable")]
    default fn spec_extend(&mut self, iter: I) {
        self.extend_desugared(iter)
    }

    #[cfg(not(feature = "unstable"))]
    fn from_iter(iterator: I) -> Self {
        from_iter_default(iterator)
    }

    #[cfg(not(feature = "unstable"))]
    fn spec_extend(&mut self, iter: I) {
        self.extend_desugared(iter)
    }
}

#[cfg(feature = "unstable")]
impl<T, I> SpecExtend<T, I> for SliceDeque<T>
where
    I: iter::TrustedLen<Item = T>,
{
    default fn from_iter(iterator: I) -> Self {
        let mut deque = Self::new();
        <Self as SpecExtend<T, I>>::spec_extend(&mut deque, iterator);
        deque
    }

    #[allow(clippy::use_debug)]
    default fn spec_extend(&mut self, iterator: I) {
        // This is the case for a TrustedLen iterator.
        let (low, high) = iterator.size_hint();
        if let Some(high_value) = high {
            debug_assert_eq!(
                low,
                high_value,
                "TrustedLen iterator's size hint is not exact: {:?}",
                (low, high)
            );
        }
        if let Some(additional) = high {
            self.reserve(additional);
            unsafe {
                let mut ptr = self.as_mut_ptr().add(self.len());
                for element in iterator {
                    ptr::write(ptr, element);
                    ptr = ptr.add(1);
                    // NB can't overflow since we would have had to alloc the
                    // address space
                    self.move_tail_unchecked(1);
                }
            }
        } else {
            self.extend_desugared(iterator)
        }
    }
}

#[cfg(feature = "unstable")]
impl<T> SpecExtend<T, IntoIter<T>> for SliceDeque<T> {
    fn from_iter(mut iterator: IntoIter<T>) -> Self {
        // A common case is passing a deque into a function which immediately
        // re-collects into a deque. We can short circuit this if the IntoIter
        // has not been advanced at all.
        if iterator.buf.as_ptr() as *const _ == iterator.ptr {
            unsafe {
                let deq = Self::from_raw_parts(
                    iterator.buf.as_ptr(),
                    iterator.cap,
                    iterator.elems(),
                );
                #[allow(clippy::mem_forget)]
                mem::forget(iterator);
                deq
            }
        } else {
            let mut deque = Self::new();
            deque.spec_extend(iterator);
            deque
        }
    }

    fn spec_extend(&mut self, mut iterator: IntoIter<T>) {
        unsafe {
            self.append_elements(iterator.as_slice() as _);
        }
        iterator.ptr = iterator.end;
    }
}

#[cfg(not(feature = "unstable"))]
impl<'a, T: 'a, I> SpecExtend<&'a T, I> for SliceDeque<T>
where
    I: Iterator<Item = &'a T>,
    T: Clone,
{
    fn from_iter(iterator: I) -> Self {
        SpecExtend::from_iter(iterator.cloned())
    }

    fn spec_extend(&mut self, iterator: I) {
        self.spec_extend(iterator.cloned())
    }
}

#[cfg(feature = "unstable")]
impl<'a, T: 'a, I> SpecExtend<&'a T, I> for SliceDeque<T>
where
    I: Iterator<Item = &'a T>,
    T: Clone,
{
    default fn from_iter(iterator: I) -> Self {
        SpecExtend::from_iter(iterator.cloned())
    }

    default fn spec_extend(&mut self, iterator: I) {
        self.spec_extend(iterator.cloned())
    }
}

#[cfg(feature = "unstable")]
impl<'a, T: 'a> SpecExtend<&'a T, slice::Iter<'a, T>> for SliceDeque<T>
where
    T: Copy,
{
    fn spec_extend(&mut self, iterator: slice::Iter<'a, T>) {
        let slice = iterator.as_slice();
        self.reserve(slice.len());
        unsafe {
            let len = self.len();
            self.move_tail_unchecked(slice.len() as isize);
            self.get_unchecked_mut(len..).copy_from_slice(slice);
        }
    }
}

impl<T> iter::FromIterator<T> for SliceDeque<T> {
    fn from_iter<I: IntoIterator<Item = T>>(iter: I) -> Self {
        <Self as SpecExtend<T, I::IntoIter>>::from_iter(iter.into_iter())
    }
}

/// This code generalises `extend_with_{element,default}`.
trait ExtendWith<T> {
    /// TODO: docs
    fn next(&self) -> T;
    /// TODO: docs
    fn last(self) -> T;
}

/// TODO: docs
struct ExtendElement<T>(T);
impl<T: Clone> ExtendWith<T> for ExtendElement<T> {
    fn next(&self) -> T {
        self.0.clone()
    }
    fn last(self) -> T {
        self.0
    }
}

/// TODO: docs
struct ExtendDefault;
impl<T: Default> ExtendWith<T> for ExtendDefault {
    fn next(&self) -> T {
        Default::default()
    }
    fn last(self) -> T {
        Default::default()
    }
}

/// TODO: docs
/// FIXME: not used, this should be used by the sdeq! macro? Remove this maybe.
#[doc(hidden)]
pub fn from_elem<T: Clone>(elem: T, n: usize) -> SliceDeque<T> {
    <T as SpecFromElem>::from_elem(elem, n)
}

/// Specialization trait used for `SliceDeque::from_elem`.
trait SpecFromElem: Sized {
    /// TODO: docs
    fn from_elem(elem: Self, n: usize) -> SliceDeque<Self>;
}

impl<T: Clone> SpecFromElem for T {
    #[cfg(feature = "unstable")]
    default fn from_elem(elem: Self, n: usize) -> SliceDeque<Self> {
        let mut v = SliceDeque::with_capacity(n);
        v.extend_with(n, ExtendElement(elem));
        v
    }

    #[cfg(not(feature = "unstable"))]
    fn from_elem(elem: Self, n: usize) -> SliceDeque<Self> {
        let mut v = SliceDeque::with_capacity(n);
        v.extend_with(n, ExtendElement(elem));
        v
    }
}

#[cfg(feature = "unstable")]
impl SpecFromElem for u8 {
    #[inline]
    fn from_elem(elem: Self, n: usize) -> SliceDeque<Self> {
        unsafe {
            let mut v = SliceDeque::with_capacity(n);
            ptr::write_bytes(v.as_mut_ptr(), elem, n);
            v.move_tail_unchecked(n as isize);
            v
        }
    }
}

macro_rules! impl_spec_from_elem {
    ($t:ty, $is_zero:expr) => {
        #[cfg(feature = "unstable")]
        impl SpecFromElem for $t {
            #[inline]
            fn from_elem(elem: Self, n: usize) -> SliceDeque<Self> {
                let mut v = SliceDeque::with_capacity(n);
                v.extend_with(n, ExtendElement(elem));
                v
            }
        }
    };
}

impl_spec_from_elem!(i8, |x| x == 0);
impl_spec_from_elem!(i16, |x| x == 0);
impl_spec_from_elem!(i32, |x| x == 0);
impl_spec_from_elem!(i64, |x| x == 0);
#[cfg(feature = "unstable")]
impl_spec_from_elem!(i128, |x| x == 0);
impl_spec_from_elem!(isize, |x| x == 0);

impl_spec_from_elem!(u16, |x| x == 0);
impl_spec_from_elem!(u32, |x| x == 0);
impl_spec_from_elem!(u64, |x| x == 0);
#[cfg(feature = "unstable")]
impl_spec_from_elem!(u128, |x| x == 0);
impl_spec_from_elem!(usize, |x| x == 0);

impl_spec_from_elem!(f32, |x: f32| x == 0. && x.is_sign_positive());
impl_spec_from_elem!(f64, |x: f64| x == 0. && x.is_sign_positive());

/// Extend implementation that copies elements out of references before
/// pushing them onto the `SliceDeque`.
///
/// This implementation is specialized for slice iterators, where it uses
/// [`copy_from_slice`] to append the entire slice at once.
///
/// [`copy_from_slice`]: ../../std/primitive.slice.html#method.copy_from_slice
impl<'a, T: 'a + Copy> Extend<&'a T> for SliceDeque<T> {
    fn extend<I: IntoIterator<Item = &'a T>>(&mut self, iter: I) {
        self.spec_extend(iter.into_iter())
    }
}

/// A splicing iterator for `SliceDeque`.
///
/// This struct is created by the [`splice()`] method on [`SliceDeque`]. See
/// its documentation for more.
///
/// [`splice()`]: struct.SliceDeque.html#method.splice
/// [`SliceDeque`]: struct.SliceDeque.html
#[derive(Debug)]
pub struct Splice<'a, I: Iterator + 'a> {
    /// TODO: docs
    drain: Drain<'a, I::Item>,
    /// TODO: docs
    replace_with: I,
}

impl<'a, I: Iterator> Iterator for Splice<'a, I> {
    type Item = I::Item;
    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        self.drain.next()
    }
    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.drain.size_hint()
    }
}

impl<'a, I: Iterator> DoubleEndedIterator for Splice<'a, I> {
    #[inline]
    fn next_back(&mut self) -> Option<Self::Item> {
        self.drain.next_back()
    }
}

#[cfg(feature = "unstable")]
impl<'a, I: Iterator> ExactSizeIterator for Splice<'a, I> {}

// TODO: re-evaluate this
#[cfg(feature = "unstable")]
impl<'a, I: Iterator> iter::FusedIterator for Splice<'a, I> {}

impl<'a, I: Iterator> Drop for Splice<'a, I> {
    fn drop(&mut self) {
        // exhaust drain first
        while let Some(_) = self.drain.next() {}

        unsafe {
            if self.drain.tail_len == 0 {
                self.drain.deq.as_mut().extend(self.replace_with.by_ref());
                return;
            }

            // First fill the range left by drain().
            if !self.drain.fill(&mut self.replace_with) {
                return;
            }

            // There may be more elements. Use the lower bound as an estimate.
            // FIXME: Is the upper bound a better guess? Or something else?
            let (lower_bound, _upper_bound) = self.replace_with.size_hint();
            if lower_bound > 0 {
                self.drain.move_tail_unchecked(lower_bound);
                if !self.drain.fill(&mut self.replace_with) {
                    return;
                }
            }

            // Collect any remaining elements.
            // This is a zero-length deque which does not allocate if
            // `lower_bound` was exact.
            let mut collected = self
                .replace_with
                .by_ref()
                .collect::<SliceDeque<I::Item>>()
                .into_iter();
            // Now we have an exact count.
            if collected.size_hint().0 > 0 {
                self.drain.move_tail_unchecked(collected.size_hint().0);
                let filled = self.drain.fill(&mut collected);
                debug_assert!(filled);
                debug_assert_eq!(collected.size_hint().0, 0);
            }
        }
        // Let `Drain::drop` move the tail back if necessary and restore
        // `deq.tail`.
    }
}

/// Private helper methods for `Splice::drop`
impl<'a, T> Drain<'a, T> {
    /// The range from `self.deq.tail` to `self.tail()_start` contains elements
    /// that have been moved out.
    /// Fill that range as much as possible with new elements from the
    /// `replace_with` iterator. Return whether we filled the entire
    /// range. (`replace_with.next()` didn’t return `None`.)
    unsafe fn fill<I: Iterator<Item = T>>(
        &mut self, replace_with: &mut I,
    ) -> bool {
        let deq = self.deq.as_mut();
        let range_start = deq.len();
        let range_end = self.tail_start;
        let range_slice = slice::from_raw_parts_mut(
            deq.as_mut_ptr().add(range_start),
            range_end - range_start,
        );

        for place in range_slice {
            if let Some(new_item) = replace_with.next() {
                ptr::write(place, new_item);
                deq.move_tail_unchecked(1);
            } else {
                return false;
            }
        }
        true
    }

    /// Make room for inserting more elements before the tail.
    unsafe fn move_tail_unchecked(&mut self, extra_capacity: usize) {
        let deq = self.deq.as_mut();
        let used_capacity = self.tail_start + self.tail_len;
        deq.reserve_capacity(used_capacity + extra_capacity)
            .expect("oom");

        let new_tail_start = self.tail_start + extra_capacity;
        let src = deq.as_ptr().add(self.tail_start);
        let dst = deq.as_mut_ptr().add(new_tail_start);
        ptr::copy(src, dst, self.tail_len);
        self.tail_start = new_tail_start;
    }
}

/// An iterator produced by calling `drain_filter` on `SliceDeque`.
#[derive(Debug)]
pub struct DrainFilter<'a, T: 'a, F>
where
    F: FnMut(&mut T) -> bool,
{
    /// TODO: docs
    deq: &'a mut SliceDeque<T>,
    /// TODO: docs
    idx: usize,
    /// TODO: docs
    del: usize,
    /// TODO: docs
    old_len: usize,
    /// TODO: docs
    pred: F,
}

impl<'a, T, F> Iterator for DrainFilter<'a, T, F>
where
    F: FnMut(&mut T) -> bool,
{
    type Item = T;

    fn next(&mut self) -> Option<T> {
        unsafe {
            while self.idx != self.old_len {
                let i = self.idx;
                self.idx += 1;
                let v = slice::from_raw_parts_mut(
                    self.deq.as_mut_ptr(),
                    self.old_len,
                );
                if (self.pred)(&mut v[i]) {
                    self.del += 1;
                    return Some(ptr::read(&v[i]));
                } else if self.del > 0 {
                    let del = self.del;
                    let src: *const T = &v[i];
                    let dst: *mut T = &mut v[i - del];
                    // This is safe because self.deq has length 0
                    // thus its elements will not have Drop::drop
                    // called on them in the event of a panic.
                    ptr::copy_nonoverlapping(src, dst, 1);
                }
            }
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (0, Some(self.old_len - self.idx))
    }
}

impl<'a, T, F> Drop for DrainFilter<'a, T, F>
where
    F: FnMut(&mut T) -> bool,
{
    fn drop(&mut self) {
        for _ in self.by_ref() {}

        unsafe {
            let new_len = self.old_len - self.del;
            self.deq.move_tail_unchecked(new_len as isize);
        }
    }
}

impl<T> convert::AsRef<[T]> for SliceDeque<T> {
    fn as_ref(&self) -> &[T] {
        &*self
    }
}

impl<T> convert::AsMut<[T]> for SliceDeque<T> {
    fn as_mut(&mut self) -> &mut [T] {
        &mut *self
    }
}

#[cfg(all(feature = "bytes_buf", feature = "use_std"))]
impl ::bytes::BufMut for SliceDeque<u8> {
    #[inline]
    fn remaining_mut(&self) -> usize {
        usize::max_value() - self.len()
    }
    #[inline]
    unsafe fn bytes_mut(&mut self) -> &mut [u8] {
        if self.capacity() == self.len() {
            self.reserve(64); // Grow the deque
        }

        let cap = self.capacity();
        let len = self.len();

        let ptr = self.as_mut_ptr();
        &mut slice::from_raw_parts_mut(ptr, cap)[len..]
    }
    #[inline]
    unsafe fn advance_mut(&mut self, cnt: usize) {
        let len = self.len();
        let remaining = self.capacity() - len;
        if cnt > remaining {
            // Reserve additional capacity, and ensure that the total length
            // will not overflow usize.
            self.reserve(cnt);
        }

        self.move_tail_unchecked(cnt as isize);
    }
}

#[cfg(all(feature = "bytes_buf", feature = "use_std"))]
impl ::bytes::IntoBuf for SliceDeque<u8> {
    type Buf = io::Cursor<SliceDeque<u8>>;

    fn into_buf(self) -> Self::Buf {
        io::Cursor::new(self)
    }
}

#[cfg(all(feature = "bytes_buf", feature = "use_std"))]
impl<'a> ::bytes::IntoBuf for &'a SliceDeque<u8> {
    type Buf = io::Cursor<&'a [u8]>;

    fn into_buf(self) -> Self::Buf {
        io::Cursor::new(&self[..])
    }
}

#[cfg(all(feature = "bytes_buf", feature = "use_std"))]
impl ::bytes::buf::FromBuf for SliceDeque<u8> {
    fn from_buf<T>(buf: T) -> Self
    where
        T: ::bytes::IntoBuf,
    {
        use bytes::{Buf, BufMut};
        let buf = buf.into_buf();
        let mut ret = SliceDeque::with_capacity(buf.remaining());
        ret.put(buf);
        ret
    }
}

#[cfg(test)]
mod tests {
    use self::collections::HashMap;
    use super::SliceDeque;
    use std::cell::RefCell;
    use std::rc::Rc;
    use std::{collections, fmt, hash, mem};

    #[derive(Clone, Debug)]
    struct WithDrop {
        counter: Rc<RefCell<usize>>,
    }

    impl Drop for WithDrop {
        fn drop(&mut self) {
            *self.counter.borrow_mut() += 1;
        }
    }

    fn sizes_to_test() -> Vec<usize> {
        let sample = vec![
            /* powers of 2 */ 2, 4, 8, 16, 32, 64,
            128, /*
                256,
                512,
                1024,
                2048,
                4096,
                8192, 16_384, 32_768,  65_536, 131_072, 262_144,
                */
                /*
                // powers of 2 - 1 or primes
                1, 3, 7, 13, 17, 31, 61, 127, 257, 509, 1021, 2039, 4093,
                8191, 16_381, 32_749,  65_537, 131_071, 262_143, 4_194_301,
                // powers of 10
                10, 100, 1000, 10_000, 100_000, 1_000_000_usize,*/
        ];
        sample.into_iter().collect()
    }

    fn linear_usize_deque(size: usize) -> SliceDeque<usize> {
        let mut v: SliceDeque<usize> = SliceDeque::new();
        for i in 0..size {
            v.push_back(i);
            assert_eq!(v.len(), i + 1);
            for j in 0..v.len() {
                assert_eq!(*v.get(j).unwrap(), j);
            }
        }
        assert_eq!(v.len(), size);
        for i in 0..size {
            assert_eq!(*v.get(i).unwrap(), i);
        }
        v
    }

    fn constant_deque<T: Clone + fmt::Debug>(
        size: usize, val: &T,
    ) -> SliceDeque<T> {
        let mut v: SliceDeque<T> = SliceDeque::with_capacity(size);
        for i in 0..size {
            let copy = val.clone();
            v.push_back(copy);
            assert_eq!(v.len(), i + 1);
        }
        assert_eq!(v.len(), size);
        v
    }

    #[test]
    fn get() {
        let mut deq = SliceDeque::new();
        deq.push_back(3);
        deq.push_back(4);
        deq.push_back(5);
        assert_eq!(deq.get(1), Some(&4));
    }

    #[test]
    fn get_mut() {
        let mut deq = SliceDeque::new();
        deq.push_back(3);
        deq.push_back(4);
        deq.push_back(5);
        assert_eq!(deq.get(1), Some(&4));
        if let Some(elem) = deq.get_mut(1) {
            *elem = 7;
        }
        assert_eq!(deq[1], 7);
    }

    #[test]
    fn is_empty() {
        let mut deq = SliceDeque::new();
        assert!(deq.is_empty());
        deq.push_back(4);
        assert!(!deq.is_empty());
        deq.pop_front();
        assert!(deq.is_empty());
    }

    #[test]
    fn push_pop_front() {
        for size in sizes_to_test() {
            let mut v: SliceDeque<usize> = SliceDeque::new();
            for i in 0..size {
                v.push_front(i);
                assert_eq!(v.len(), i + 1);
                for j in 0..v.len() {
                    assert_eq!(*v.get(v.len() - j - 1).unwrap(), j);
                }
            }
            assert_eq!(v.len(), size);
            for i in 0..size {
                assert_eq!(*v.get(i).unwrap(), size - i - 1);
            }
            for i in 0..size {
                assert_eq!(v.len(), size - i);
                v.pop_front();
                for j in 0..v.len() {
                    assert_eq!(*v.get(v.len() - j - 1).unwrap(), j);
                }
            }
            assert_eq!(v.len(), 0);
        }
    }

    #[test]
    fn push_pop_back() {
        for size in sizes_to_test() {
            let mut v = linear_usize_deque(size);
            for i in 0..size {
                assert_eq!(v.len(), size - i);
                v.pop_back();
                for j in 0..v.len() {
                    assert_eq!(*v.get(j).unwrap(), j);
                }
            }
            assert_eq!(v.len(), 0);
        }
    }

    #[test]
    fn all_head_tails() {
        for size in sizes_to_test() {
            let mut v = linear_usize_deque(size);
            let permutations = 6 * v.capacity();

            // rotate from left to right
            for _ in 0..permutations {
                v.push_back(0);
                for j in (0..v.len() - 1).rev() {
                    *v.get_mut(j + 1).unwrap() = *v.get(j).unwrap();
                }
                v.pop_front();
                assert_eq!(v.len(), size);
                for k in 0..size {
                    assert_eq!(*v.get(k).unwrap(), k);
                }
            }

            // rotate from right to left
            for _ in 0..permutations {
                v.push_front(0);
                for j in 0..v.len() - 1 {
                    *v.get_mut(j).unwrap() = *v.get(j + 1).unwrap()
                }
                v.pop_back();
                assert_eq!(v.len(), size);
                for k in 0..size {
                    assert_eq!(*v.get(k).unwrap(), k);
                }
            }
        }
    }

    #[test]
    fn drop() {
        for size in sizes_to_test() {
            let counter = Rc::new(RefCell::new(0));
            let val = WithDrop {
                counter: counter.clone(),
            };
            {
                let _v = constant_deque(size, &val);
            }
            assert_eq!(*counter.borrow(), size);
        }
    }

    #[test]
    fn clear() {
        for size in sizes_to_test() {
            let counter = Rc::new(RefCell::new(0));
            let val = WithDrop {
                counter: counter.clone(),
            };
            assert_eq!(*counter.borrow(), 0);
            let mut v = constant_deque(size, &val);
            assert_eq!(*counter.borrow(), 0);
            v.clear();
            assert_eq!(*counter.borrow(), size);
            assert_eq!(v.len(), 0);
        }
    }

    #[test]
    fn reserve_no_cap_change() {
        let mut slice = SliceDeque::<u8>::with_capacity(4096);
        let cap = slice.capacity();
        assert!(cap >= 4096);
        slice.reserve(cap);
        // capacity should not change if the existing capacity is already
        // sufficient.
        assert_eq!(slice.capacity(), cap);
    }

    #[test]
    fn resize() {
        for size in sizes_to_test() {
            let mut v = linear_usize_deque(size);
            let mut v_ref = linear_usize_deque(size / 2);
            v.resize(size / 2, 0);
            assert_eq!(v.len(), size / 2);
            assert_eq!(v.as_slice(), v_ref.as_slice());
            while v_ref.len() < size {
                v_ref.push_back(3);
            }
            v.resize(size, 3);
            assert_eq!(v.len(), size);
            assert_eq!(v_ref.len(), size);
            assert_eq!(v.as_slice(), v_ref.as_slice());

            v.resize(0, 3);
            assert_eq!(v.len(), 0);

            v.resize(size, 3);
            let v_ref = constant_deque(size, &3);
            assert_eq!(v.len(), size);
            assert_eq!(v_ref.len(), size);
            assert_eq!(v.as_slice(), v_ref.as_slice());
        }
    }

    #[test]
    fn default() {
        let d = SliceDeque::<u8>::default();
        let r = SliceDeque::<u8>::new();
        assert_eq!(d.as_slice(), r.as_slice());
    }

    #[test]
    fn shrink_to_fit() {
        let page_size = 4096;
        for size in sizes_to_test() {
            let mut deq = constant_deque(size, &(3 as u8));
            let old_cap = deq.capacity();
            deq.resize(size / 4, 3);
            deq.shrink_to_fit();
            if size <= page_size {
                assert_eq!(deq.capacity(), old_cap);
            } else {
                assert!(deq.capacity() < old_cap);
            }
        }
    }

    #[test]
    fn iter() {
        let mut deq = SliceDeque::new();
        deq.push_back(5);
        deq.push_back(3);
        deq.push_back(4);
        let b: &[_] = &[&5, &3, &4];
        let c: Vec<&i32> = deq.iter().collect();
        assert_eq!(&c[..], b);
    }

    #[test]
    fn iter_mut() {
        let mut deq = SliceDeque::new();
        deq.push_back(5);
        deq.push_back(3);
        deq.push_back(4);
        for num in deq.iter_mut() {
            *num = *num - 2;
        }
        let b: &[_] = &[&mut 3, &mut 1, &mut 2];
        assert_eq!(&deq.iter_mut().collect::<Vec<&mut i32>>()[..], b);
    }

    #[test]
    fn hash_map() {
        let mut hm: HashMap<SliceDeque<u32>, u32> = HashMap::new();
        let mut a = SliceDeque::new();
        a.push_back(1);
        a.push_back(2);
        hm.insert(a.clone(), 3);
        let b = SliceDeque::new();
        assert_eq!(hm.get(&a), Some(&3));
        assert_eq!(hm.get(&b), None);
    }

    #[test]
    fn partial_ord_eq() {
        let mut a = SliceDeque::new();
        a.push_back(1);
        a.push_back(2);
        a.push_back(3);
        assert!(a == a);
        assert!(!(a != a));

        let mut b = SliceDeque::new();
        b.push_back(1);
        b.push_back(3);
        b.push_back(2);
        assert!(a < b);
        assert!(b > a);
        assert!(a != b);

        let mut c = SliceDeque::new();
        c.push_back(2);
        assert!(c > a);
        assert!(a < c);
    }

    struct DropCounter<'a> {
        count: &'a mut u32,
    }

    impl<'a> Drop for DropCounter<'a> {
        fn drop(&mut self) {
            *self.count += 1;
        }
    }

    #[test]
    fn vec_double_drop() {
        struct TwoSliceDeque<T> {
            x: SliceDeque<T>,
            y: SliceDeque<T>,
        }

        let (mut count_x, mut count_y) = (0, 0);
        {
            let mut tv = TwoSliceDeque {
                x: SliceDeque::new(),
                y: SliceDeque::new(),
            };
            tv.x.push_back(DropCounter {
                count: &mut count_x,
            });
            tv.y.push_back(DropCounter {
                count: &mut count_y,
            });

            // If SliceDeque had a drop flag, here is where it would be zeroed.
            // Instead, it should rely on its internal state to prevent
            // doing anything significant when dropped multiple times.
            mem::drop(tv.x);

            // Here tv goes out of scope, tv.y should be dropped, but not tv.x.
        }

        assert_eq!(count_x, 1);
        assert_eq!(count_y, 1);
    }

    #[test]
    fn vec_reserve() {
        let mut v = SliceDeque::new();
        assert_eq!(v.capacity(), 0);

        v.reserve(2);
        assert!(v.capacity() >= 2);

        for i in 0..16 {
            v.push_back(i);
        }

        assert!(v.capacity() >= 16);
        v.reserve(16);
        assert!(v.capacity() >= 32);

        v.push_back(16);

        v.reserve(16);
        assert!(v.capacity() >= 33)
    }

    #[test]
    fn vec_extend() {
        let mut v = SliceDeque::new();
        let mut w = SliceDeque::new();

        v.extend(w.clone());
        assert_eq!(v, &[]);

        v.extend(0..3);
        for i in 0..3 {
            w.push_back(i)
        }

        assert_eq!(v, w);

        v.extend(3..10);
        for i in 3..10 {
            w.push_back(i)
        }

        assert_eq!(v, w);

        v.extend(w.clone()); // specializes to `append`
        assert!(v.iter().eq(w.iter().chain(w.iter())));

        // Double drop
        let mut count_x = 0;
        {
            let mut x = SliceDeque::new();
            let mut y = SliceDeque::new();
            y.push_back(DropCounter {
                count: &mut count_x,
            });
            x.extend(y);
        }
        assert_eq!(count_x, 1);
    }

    #[test]
    fn vec_extend_zst() {
        // Zero sized types
        #[derive(PartialEq, Debug)]
        struct Foo;

        let mut a = SliceDeque::new();
        let b = sdeq![Foo, Foo];

        a.extend(b);
        assert_eq!(a, &[Foo, Foo]);
    }

    #[test]
    fn vec_extend_ref() {
        let mut v = SliceDeque::new();
        for &i in &[1, 2] {
            v.push_back(i);
        }
        v.extend(&[3, 4, 5]);

        assert_eq!(v.len(), 5);
        assert_eq!(v, [1, 2, 3, 4, 5]);

        let mut w = SliceDeque::new();
        for &i in &[6, 7] {
            w.push_back(i);
        }
        v.extend(&w);

        assert_eq!(v.len(), 7);
        assert_eq!(v, [1, 2, 3, 4, 5, 6, 7]);
    }

    #[test]
    fn vec_slice_from_mut() {
        let mut values = sdeq![1, 2, 3, 4, 5];
        {
            let slice = &mut values[2..];
            assert!(slice == [3, 4, 5]);
            for p in slice {
                *p += 2;
            }
        }

        assert!(values == [1, 2, 5, 6, 7]);
    }

    #[test]
    fn vec_slice_to_mut() {
        let mut values = sdeq![1, 2, 3, 4, 5];
        {
            let slice = &mut values[..2];
            assert!(slice == [1, 2]);
            for p in slice {
                *p += 1;
            }
        }

        assert!(values == [2, 3, 3, 4, 5]);
    }

    #[test]
    fn vec_split_at_mut() {
        let mut values = sdeq![1, 2, 3, 4, 5];
        {
            let (left, right) = values.split_at_mut(2);
            {
                let left: &[_] = left;
                assert!(&left[..left.len()] == &[1, 2]);
            }
            for p in left {
                *p += 1;
            }

            {
                let right: &[_] = right;
                assert!(&right[..right.len()] == &[3, 4, 5]);
            }
            for p in right {
                *p += 2;
            }
        }

        assert_eq!(values, [2, 3, 5, 6, 7]);
    }

    #[test]
    fn vec_clone() {
        let v: SliceDeque<i32> = sdeq![];
        let w = sdeq![1, 2, 3];

        assert_eq!(v, v.clone());

        let z = w.clone();
        assert_eq!(w, z);
        // they should be disjoint in memory.
        assert!(w.as_ptr() != z.as_ptr())
    }

    #[cfg(feature = "unstable")]
    #[test]
    fn vec_clone_from() {
        let mut v = sdeq![];
        let three: SliceDeque<Box<_>> = sdeq![box 1, box 2, box 3];
        let two: SliceDeque<Box<_>> = sdeq![box 4, box 5];

        // zero, long
        v.clone_from(&three);
        assert_eq!(v, three);

        // equal
        v.clone_from(&three);
        assert_eq!(v, three);

        // long, short
        v.clone_from(&two);
        assert_eq!(v, two);

        // short, long
        v.clone_from(&three);
        assert_eq!(v, three)
    }

    #[test]
    fn vec_retain() {
        let mut deq = sdeq![1, 2, 3, 4];
        deq.retain(|&x| x % 2 == 0);
        assert_eq!(deq, [2, 4]);
    }

    #[test]
    fn vec_dedup() {
        fn case(a: SliceDeque<i32>, b: SliceDeque<i32>) {
            let mut v = a;
            v.dedup();
            assert_eq!(v, b);
        }
        case(sdeq![], sdeq![]);
        case(sdeq![1], sdeq![1]);
        case(sdeq![1, 1], sdeq![1]);
        case(sdeq![1, 2, 3], sdeq![1, 2, 3]);
        case(sdeq![1, 1, 2, 3], sdeq![1, 2, 3]);
        case(sdeq![1, 2, 2, 3], sdeq![1, 2, 3]);
        case(sdeq![1, 2, 3, 3], sdeq![1, 2, 3]);
        case(sdeq![1, 1, 2, 2, 2, 3, 3], sdeq![1, 2, 3]);
    }

    #[test]
    fn vec_dedup_by_key() {
        fn case(a: SliceDeque<i32>, b: SliceDeque<i32>) {
            let mut v = a;
            v.dedup_by_key(|i| *i / 10);
            assert_eq!(v, b);
        }
        case(sdeq![], sdeq![]);
        case(sdeq![10], sdeq![10]);
        case(sdeq![10, 11], sdeq![10]);
        case(sdeq![10, 20, 30], sdeq![10, 20, 30]);
        case(sdeq![10, 11, 20, 30], sdeq![10, 20, 30]);
        case(sdeq![10, 20, 21, 30], sdeq![10, 20, 30]);
        case(sdeq![10, 20, 30, 31], sdeq![10, 20, 30]);
        case(sdeq![10, 11, 20, 21, 22, 30, 31], sdeq![10, 20, 30]);
    }

    #[test]
    fn vec_dedup_by() {
        let mut deq = sdeq!["foo", "bar", "Bar", "baz", "bar"];
        deq.dedup_by(|a, b| a.eq_ignore_ascii_case(b));

        assert_eq!(deq, ["foo", "bar", "baz", "bar"]);

        let mut deq: SliceDeque<(&'static str, i32)> =
            sdeq![("foo", 1), ("foo", 2), ("bar", 3), ("bar", 4), ("bar", 5)];
        deq.dedup_by(|a, b| {
            a.0 == b.0 && {
                b.1 += a.1;
                true
            }
        });

        assert_eq!(deq, [("foo", 3), ("bar", 12)]);
    }

    #[cfg(feature = "unstable")]
    #[test]
    fn vec_dedup_unique() {
        let mut v0: SliceDeque<Box<_>> = sdeq![box 1, box 1, box 2, box 3];
        v0.dedup();
        let mut v1: SliceDeque<Box<_>> = sdeq![box 1, box 2, box 2, box 3];
        v1.dedup();
        let mut v2: SliceDeque<Box<_>> = sdeq![box 1, box 2, box 3, box 3];
        v2.dedup();
        // If the boxed pointers were leaked or otherwise misused, valgrind
        // and/or rt should raise errors.
    }

    #[test]
    fn zero_sized_values() {
        let mut v = SliceDeque::new();
        assert_eq!(v.len(), 0);
        v.push_back(());
        assert_eq!(v.len(), 1);
        v.push_back(());
        assert_eq!(v.len(), 2);
        assert_eq!(v.pop_back(), Some(()));
        assert_eq!(v.pop_back(), Some(()));
        assert_eq!(v.pop_back(), None);

        assert_eq!(v.iter().count(), 0);
        v.push_back(());
        assert_eq!(v.iter().count(), 1);
        v.push_back(());
        assert_eq!(v.iter().count(), 2);

        for &() in &v {}

        assert_eq!(v.iter_mut().count(), 2);
        v.push_back(());
        assert_eq!(v.iter_mut().count(), 3);
        v.push_back(());
        assert_eq!(v.iter_mut().count(), 4);

        for &mut () in &mut v {}
        unsafe {
            let len = v.len() as isize;
            v.move_tail_unchecked(-len);
        }
        assert_eq!(v.iter_mut().count(), 0);
    }

    #[test]
    fn vec_partition() {
        assert_eq!(
            sdeq![].into_iter().partition(|x: &i32| *x < 3),
            (sdeq![], sdeq![])
        );
        assert_eq!(
            sdeq![1, 2, 3].into_iter().partition(|x| *x < 4),
            (sdeq![1, 2, 3], sdeq![])
        );
        assert_eq!(
            sdeq![1, 2, 3].into_iter().partition(|x| *x < 2),
            (sdeq![1], sdeq![2, 3])
        );
        assert_eq!(
            sdeq![1, 2, 3].into_iter().partition(|x| *x < 0),
            (sdeq![], sdeq![1, 2, 3])
        );
    }

    #[test]
    fn vec_zip_unzip() {
        let z1 = sdeq![(1, 4), (2, 5), (3, 6)];

        let (left, right): (SliceDeque<_>, SliceDeque<_>) =
            z1.iter().cloned().unzip();

        assert_eq!((1, 4), (left[0], right[0]));
        assert_eq!((2, 5), (left[1], right[1]));
        assert_eq!((3, 6), (left[2], right[2]));
    }

    #[test]
    fn vec_vec_truncate_drop() {
        static mut DROPS: u32 = 0;
        struct Elem(i32);
        impl Drop for Elem {
            fn drop(&mut self) {
                unsafe {
                    DROPS += 1;
                }
            }
        }

        let mut v = sdeq![Elem(1), Elem(2), Elem(3), Elem(4), Elem(5)];
        assert_eq!(unsafe { DROPS }, 0);
        v.truncate(3);
        assert_eq!(unsafe { DROPS }, 2);
        v.truncate(0);
        assert_eq!(unsafe { DROPS }, 5);
    }

    #[test]
    fn vec_vec_truncate_front_drop() {
        static mut DROPS: u32 = 0;
        struct Elem(i32);
        impl Drop for Elem {
            fn drop(&mut self) {
                unsafe {
                    DROPS += 1;
                }
            }
        }

        let mut v = sdeq![Elem(1), Elem(2), Elem(3), Elem(4), Elem(5)];
        assert_eq!(unsafe { DROPS }, 0);
        v.truncate_front(3);
        assert_eq!(unsafe { DROPS }, 2);
        v.truncate_front(0);
        assert_eq!(unsafe { DROPS }, 5);
    }

    #[test]
    #[should_panic]
    fn vec_vec_truncate_fail() {
        struct BadElem(i32);
        impl Drop for BadElem {
            fn drop(&mut self) {
                let BadElem(ref mut x) = *self;
                if *x == 0xbadbeef {
                    panic!("BadElem panic: 0xbadbeef")
                }
            }
        }

        let mut v =
            sdeq![BadElem(1), BadElem(2), BadElem(0xbadbeef), BadElem(4)];
        v.truncate(0);
    }

    #[test]
    fn vec_index() {
        let deq = sdeq![1, 2, 3];
        assert!(deq[1] == 2);
    }

    #[test]
    #[should_panic]
    fn vec_index_out_of_bounds() {
        let deq = sdeq![1, 2, 3];
        let _ = deq[3];
    }

    #[test]
    #[should_panic]
    fn vec_slice_out_of_bounds_1() {
        let x = sdeq![1, 2, 3, 4, 5];
        &x[!0..];
    }

    #[test]
    #[should_panic]
    fn vec_slice_out_of_bounds_2() {
        let x = sdeq![1, 2, 3, 4, 5];
        &x[..6];
    }

    #[test]
    #[should_panic]
    fn vec_slice_out_of_bounds_3() {
        let x = sdeq![1, 2, 3, 4, 5];
        &x[!0..4];
    }

    #[test]
    #[should_panic]
    fn vec_slice_out_of_bounds_4() {
        let x = sdeq![1, 2, 3, 4, 5];
        &x[1..6];
    }

    #[test]
    #[should_panic]
    fn vec_slice_out_of_bounds_5() {
        let x = sdeq![1, 2, 3, 4, 5];
        &x[3..2];
    }

    #[test]
    fn vec_swap_remove_empty() {
        let mut deq = SliceDeque::<i32>::new();
        assert_eq!(deq.swap_remove_back(0), None);
    }

    #[test]
    fn vec_move_items() {
        let deq = sdeq![1, 2, 3];
        let mut deq2 = sdeq![];
        for i in deq {
            deq2.push_back(i);
        }
        assert_eq!(deq2, [1, 2, 3]);
    }

    #[test]
    fn vec_move_items_reverse() {
        let deq = sdeq![1, 2, 3];
        let mut deq2 = sdeq![];
        for i in deq.into_iter().rev() {
            deq2.push_back(i);
        }
        assert_eq!(deq2, [3, 2, 1]);
    }

    #[test]
    fn vec_move_items_zero_sized() {
        let deq = sdeq![(), (), ()];
        let mut deq2 = sdeq![];
        for i in deq {
            deq2.push_back(i);
        }
        assert_eq!(deq2, [(), (), ()]);
    }

    #[test]
    fn vec_drain_items() {
        let mut deq = sdeq![1, 2, 3];
        let mut deq2 = sdeq![];
        for i in deq.drain(..) {
            deq2.push_back(i);
        }
        assert_eq!(deq, []);
        assert_eq!(deq2, [1, 2, 3]);
    }

    #[test]
    fn vec_drain_items_reverse() {
        let mut deq = sdeq![1, 2, 3];
        let mut deq2 = sdeq![];
        for i in deq.drain(..).rev() {
            deq2.push_back(i);
        }
        assert_eq!(deq, []);
        assert_eq!(deq2, [3, 2, 1]);
    }

    #[test]
    fn vec_drain_items_zero_sized() {
        let mut deq = sdeq![(), (), ()];
        let mut deq2 = sdeq![];
        for i in deq.drain(..) {
            deq2.push_back(i);
        }
        assert_eq!(deq, []);
        assert_eq!(deq2, [(), (), ()]);
    }

    #[test]
    #[should_panic]
    fn vec_drain_out_of_bounds() {
        let mut v = sdeq![1, 2, 3, 4, 5];
        v.drain(5..6);
    }

    #[test]
    fn vec_drain_range() {
        let mut v = sdeq![1, 2, 3, 4, 5];
        for _ in v.drain(4..) {}
        assert_eq!(v, &[1, 2, 3, 4]);

        let mut v: SliceDeque<_> = (1..6).map(|x| x.to_string()).collect();
        for _ in v.drain(1..4) {}
        assert_eq!(v, &[1.to_string(), 5.to_string()]);

        let mut v: SliceDeque<_> = (1..6).map(|x| x.to_string()).collect();
        for _ in v.drain(1..4).rev() {}
        assert_eq!(v, &[1.to_string(), 5.to_string()]);
    }

    #[test]
    fn vec_drain_range_zst() {
        let mut v: SliceDeque<_> = sdeq![(); 5];
        for _ in v.drain(1..4).rev() {}
        assert_eq!(v, &[(), ()]);
    }

    #[test]
    fn vec_drain_inclusive_range() {
        let mut v = sdeq!['a', 'b', 'c', 'd', 'e'];
        for _ in v.drain(1..=3) {}
        assert_eq!(v, &['a', 'e']);

        let mut v: SliceDeque<_> = (0..=5).map(|x| x.to_string()).collect();
        for _ in v.drain(1..=5) {}
        assert_eq!(v, &["0".to_string()]);

        let mut v: SliceDeque<String> =
            (0..=5).map(|x| x.to_string()).collect();
        for _ in v.drain(0..=5) {}
        assert_eq!(v, SliceDeque::<String>::new());

        let mut v: SliceDeque<_> = (0..=5).map(|x| x.to_string()).collect();
        for _ in v.drain(0..=3) {}
        assert_eq!(v, &["4".to_string(), "5".to_string()]);

        let mut v: SliceDeque<_> = (0..=1).map(|x| x.to_string()).collect();
        for _ in v.drain(..=0) {}
        assert_eq!(v, &["1".to_string()]);
    }

    #[test]
    fn vec_drain_max_vec_size() {
        const M: usize = isize::max_value() as usize;
        let mut v = SliceDeque::<()>::with_capacity(M);
        unsafe { v.move_tail_unchecked(M as isize) };
        assert_eq!(v.len(), M as usize);
        for _ in v.drain(M - 1..) {}
        assert_eq!(v.len(), M - 1);

        let mut v = SliceDeque::<()>::with_capacity(M);
        unsafe { v.move_tail_unchecked(M as isize) };
        assert_eq!(v.len(), M as usize);
        for _ in v.drain(M - 1..=M - 1) {}
        assert_eq!(v.len(), M - 1);
    }

    #[test]
    #[should_panic]
    fn vec_drain_inclusive_out_of_bounds() {
        let mut v = sdeq![1, 2, 3, 4, 5];
        v.drain(5..=5);
    }

    #[test]
    fn vec_splice() {
        let mut v = sdeq![1, 2, 3, 4, 5];
        let a = [10, 11, 12];
        v.splice(2..4, a.iter().cloned());
        assert_eq!(v, &[1, 2, 10, 11, 12, 5]);
        v.splice(1..3, Some(20));
        assert_eq!(v, &[1, 20, 11, 12, 5]);
    }

    #[test]
    fn vec_splice_inclusive_range() {
        let mut v = sdeq![1, 2, 3, 4, 5];
        let a = [10, 11, 12];
        let t1: SliceDeque<_> = v.splice(2..=3, a.iter().cloned()).collect();
        assert_eq!(v, &[1, 2, 10, 11, 12, 5]);
        assert_eq!(t1, &[3, 4]);
        let t2: SliceDeque<_> = v.splice(1..=2, Some(20)).collect();
        assert_eq!(v, &[1, 20, 11, 12, 5]);
        assert_eq!(t2, &[2, 10]);
    }

    #[test]
    #[should_panic]
    fn vec_splice_out_of_bounds() {
        let mut v = sdeq![1, 2, 3, 4, 5];
        let a = [10, 11, 12];
        v.splice(5..6, a.iter().cloned());
    }

    #[test]
    #[should_panic]
    fn vec_splice_inclusive_out_of_bounds() {
        let mut v = sdeq![1, 2, 3, 4, 5];
        let a = [10, 11, 12];
        v.splice(5..=5, a.iter().cloned());
    }

    #[test]
    fn vec_splice_items_zero_sized() {
        let mut deq = sdeq![(), (), ()];
        let deq2 = sdeq![];
        let t: SliceDeque<_> =
            deq.splice(1..2, deq2.iter().cloned()).collect();
        assert_eq!(deq, &[(), ()]);
        assert_eq!(t, &[()]);
    }

    #[test]
    fn vec_splice_unbounded() {
        let mut deq = sdeq![1, 2, 3, 4, 5];
        let t: SliceDeque<_> = deq.splice(.., None).collect();
        assert_eq!(deq, &[]);
        assert_eq!(t, &[1, 2, 3, 4, 5]);
    }

    #[test]
    fn vec_splice_forget() {
        let mut v = sdeq![1, 2, 3, 4, 5];
        let a = [10, 11, 12];
        mem::forget(v.splice(2..4, a.iter().cloned()));
        assert_eq!(v, &[1, 2]);
    }

    /* into_boxed_slice probably can't be supported portably
    #[test]
    fn vec_into_boxed_slice() {
        let xs = sdeq![1, 2, 3];
        let ys = xs.into_boxed_slice();
        assert_eq!(&*ys, [1, 2, 3]);
    }
    */

    #[test]
    fn vec_append() {
        let mut deq = sdeq![1, 2, 3];
        let mut deq2 = sdeq![4, 5, 6];
        deq.append(&mut deq2);
        assert_eq!(deq, [1, 2, 3, 4, 5, 6]);
        assert_eq!(deq2, []);
    }

    #[test]
    fn vec_split_off() {
        let mut deq = sdeq![1, 2, 3, 4, 5, 6];
        let deq2 = deq.split_off(4);
        assert_eq!(deq, [1, 2, 3, 4]);
        assert_eq!(deq2, [5, 6]);
    }

    #[test]
    fn vec_into_iter_as_slice() {
        let deq = sdeq!['a', 'b', 'c'];
        let mut into_iter = deq.into_iter();
        assert_eq!(into_iter.as_slice(), &['a', 'b', 'c']);
        let _ = into_iter.next().unwrap();
        assert_eq!(into_iter.as_slice(), &['b', 'c']);
        let _ = into_iter.next().unwrap();
        let _ = into_iter.next().unwrap();
        assert_eq!(into_iter.as_slice(), &[]);
    }

    #[test]
    fn vec_into_iter_as_mut_slice() {
        let deq = sdeq!['a', 'b', 'c'];
        let mut into_iter = deq.into_iter();
        assert_eq!(into_iter.as_slice(), &['a', 'b', 'c']);
        into_iter.as_mut_slice()[0] = 'x';
        into_iter.as_mut_slice()[1] = 'y';
        assert_eq!(into_iter.next().unwrap(), 'x');
        assert_eq!(into_iter.as_slice(), &['y', 'c']);
    }

    #[test]
    fn vec_into_iter_debug() {
        let deq = sdeq!['a', 'b', 'c'];
        let into_iter = deq.into_iter();
        let debug = format!("{:?}", into_iter);
        assert_eq!(debug, "IntoIter(['a', 'b', 'c'])");
    }

    #[test]
    fn vec_into_iter_count() {
        assert_eq!(sdeq![1, 2, 3].into_iter().count(), 3);
    }

    #[test]
    fn vec_into_iter_clone() {
        fn iter_equal<I: Iterator<Item = i32>>(it: I, slice: &[i32]) {
            let v: SliceDeque<i32> = it.collect();
            assert_eq!(&v[..], slice);
        }
        let deq = sdeq![1, 2, 3];
        let mut it = deq.into_iter();
        let it_c = it.clone();
        iter_equal(it_c, &[1, 2, 3]);
        assert_eq!(it.next(), Some(1));
        let mut it = it.rev();
        iter_equal(it.clone(), &[3, 2]);
        assert_eq!(it.next(), Some(3));
        iter_equal(it.clone(), &[2]);
        assert_eq!(it.next(), Some(2));
        iter_equal(it.clone(), &[]);
        assert_eq!(it.next(), None);
    }

    /* TODO: Cow support
    #[test]
        fn vec_cow_from() {
            use std::borrow::Cow;
        let borrowed: &[_] = &["borrowed", "(slice)"];
        let owned = sdeq!["owned", "(vec)"];
        match (Cow::from(owned.clone()), Cow::from(borrowed)) {
            (Cow::Owned(o), Cow::Borrowed(b)) => assert!(o == owned && b == borrowed),
            _ => panic!("invalid `Cow::from`"),
        }
    }

    #[test]
        fn vec_from_cow() {
            use std::borrow::Cow;
        let borrowed: &[_] = &["borrowed", "(slice)"];
        let owned = sdeq!["owned", "(vec)"];
        assert_eq!(SliceDeque::from(Cow::Borrowed(borrowed)), sdeq!["borrowed", "(slice)"]);
        assert_eq!(SliceDeque::from(Cow::Owned(owned)), sdeq!["owned", "(vec)"]);
    }
         */

    /* TODO: covariance
    use super::{Drain, IntoIter};

    #[allow(dead_code)]
    fn assert_covariance() {
        fn drain<'new>(d: Drain<'static, &'static str>) -> Drain<'new, &'new str> {
            d
        }
        fn into_iter<'new>(i: IntoIter<&'static str>) -> IntoIter<&'new str> {
            i
        }
    }
        */

    #[test]
    fn from_into_inner() {
        let deq = sdeq![1, 2, 3];
        #[allow(unused_variables)]
        let ptr = deq.as_ptr();
        let deq = deq.into_iter().collect::<SliceDeque<_>>();
        assert_eq!(deq, [1, 2, 3]);
        #[cfg(feature = "unstable")]
        {
            assert_eq!(deq.as_ptr(), ptr);
        }

        let ptr = &deq[1] as *const _;
        let mut it = deq.into_iter();
        it.next().unwrap();
        let deq = it.collect::<SliceDeque<_>>();
        assert_eq!(deq, [2, 3]);
        assert!(ptr != deq.as_ptr());
    }

    #[cfg(feature = "unstable")]
    #[test]
    fn overaligned_allocations() {
        #[repr(align(256))]
        struct Foo(usize);
        let mut v = sdeq![Foo(273)];
        for i in 0..0x1000 {
            v.reserve_exact(i);
            assert!(v[0].0 == 273);
            assert!(v.as_ptr() as usize & 0xff == 0);
            v.shrink_to_fit();
            assert!(v[0].0 == 273);
            assert!(v.as_ptr() as usize & 0xff == 0);
        }
    }

    #[test]
    fn drain_filter_empty() {
        let mut deq: SliceDeque<i32> = sdeq![];

        {
            let mut iter = deq.drain_filter(|_| true);
            assert_eq!(iter.size_hint(), (0, Some(0)));
            assert_eq!(iter.next(), None);
            assert_eq!(iter.size_hint(), (0, Some(0)));
            assert_eq!(iter.next(), None);
            assert_eq!(iter.size_hint(), (0, Some(0)));
        }
        assert_eq!(deq.len(), 0);
        assert_eq!(deq, sdeq![]);
    }

    #[test]
    fn drain_filter_zst() {
        let mut deq = sdeq![(), (), (), (), ()];
        let initial_len = deq.len();
        let mut count = 0;
        {
            let mut iter = deq.drain_filter(|_| true);
            assert_eq!(iter.size_hint(), (0, Some(initial_len)));
            while let Some(_) = iter.next() {
                count += 1;
                assert_eq!(iter.size_hint(), (0, Some(initial_len - count)));
            }
            assert_eq!(iter.size_hint(), (0, Some(0)));
            assert_eq!(iter.next(), None);
            assert_eq!(iter.size_hint(), (0, Some(0)));
        }

        assert_eq!(count, initial_len);
        assert_eq!(deq.len(), 0);
        assert_eq!(deq, sdeq![]);
    }

    #[test]
    fn drain_filter_false() {
        let mut deq = sdeq![1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

        let initial_len = deq.len();
        let mut count = 0;
        {
            let mut iter = deq.drain_filter(|_| false);
            assert_eq!(iter.size_hint(), (0, Some(initial_len)));
            for _ in iter.by_ref() {
                count += 1;
            }
            assert_eq!(iter.size_hint(), (0, Some(0)));
            assert_eq!(iter.next(), None);
            assert_eq!(iter.size_hint(), (0, Some(0)));
        }

        assert_eq!(count, 0);
        assert_eq!(deq.len(), initial_len);
        assert_eq!(deq, sdeq![1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    }

    #[test]
    fn drain_filter_true() {
        let mut deq = sdeq![1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

        let initial_len = deq.len();
        let mut count = 0;
        {
            let mut iter = deq.drain_filter(|_| true);
            assert_eq!(iter.size_hint(), (0, Some(initial_len)));
            while let Some(_) = iter.next() {
                count += 1;
                assert_eq!(iter.size_hint(), (0, Some(initial_len - count)));
            }
            assert_eq!(iter.size_hint(), (0, Some(0)));
            assert_eq!(iter.next(), None);
            assert_eq!(iter.size_hint(), (0, Some(0)));
        }

        assert_eq!(count, initial_len);
        assert_eq!(deq.len(), 0);
        assert_eq!(deq, sdeq![]);
    }

    #[test]
    fn drain_filter_complex() {
        {
            //                [+xxx++++++xxxxx++++x+x++]
            let mut deq = sdeq![
                1, 2, 4, 6, 7, 9, 11, 13, 15, 17, 18, 20, 22, 24, 26, 27, 29,
                31, 33, 34, 35, 36, 37, 39,
            ];

            let removed =
                deq.drain_filter(|x| *x % 2 == 0).collect::<SliceDeque<_>>();
            assert_eq!(removed.len(), 10);
            assert_eq!(removed, sdeq![2, 4, 6, 18, 20, 22, 24, 26, 34, 36]);

            assert_eq!(deq.len(), 14);
            assert_eq!(
                deq,
                sdeq![1, 7, 9, 11, 13, 15, 17, 27, 29, 31, 33, 35, 37, 39]
            );
        }

        {
            //                [xxx++++++xxxxx++++x+x++]
            let mut deq = sdeq![
                2, 4, 6, 7, 9, 11, 13, 15, 17, 18, 20, 22, 24, 26, 27, 29, 31,
                33, 34, 35, 36, 37, 39,
            ];

            let removed =
                deq.drain_filter(|x| *x % 2 == 0).collect::<SliceDeque<_>>();
            assert_eq!(removed.len(), 10);
            assert_eq!(removed, sdeq![2, 4, 6, 18, 20, 22, 24, 26, 34, 36]);

            assert_eq!(deq.len(), 13);
            assert_eq!(
                deq,
                sdeq![7, 9, 11, 13, 15, 17, 27, 29, 31, 33, 35, 37, 39]
            );
        }

        {
            //                [xxx++++++xxxxx++++x+x]
            let mut deq = sdeq![
                2, 4, 6, 7, 9, 11, 13, 15, 17, 18, 20, 22, 24, 26, 27, 29, 31,
                33, 34, 35, 36,
            ];

            let removed =
                deq.drain_filter(|x| *x % 2 == 0).collect::<SliceDeque<_>>();
            assert_eq!(removed.len(), 10);
            assert_eq!(removed, sdeq![2, 4, 6, 18, 20, 22, 24, 26, 34, 36]);

            assert_eq!(deq.len(), 11);
            assert_eq!(deq, sdeq![7, 9, 11, 13, 15, 17, 27, 29, 31, 33, 35]);
        }

        {
            //                [xxxxxxxxxx+++++++++++]
            let mut deq = sdeq![
                2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 1, 3, 5, 7, 9, 11, 13, 15,
                17, 19,
            ];

            let removed =
                deq.drain_filter(|x| *x % 2 == 0).collect::<SliceDeque<_>>();
            assert_eq!(removed.len(), 10);
            assert_eq!(removed, sdeq![2, 4, 6, 8, 10, 12, 14, 16, 18, 20]);

            assert_eq!(deq.len(), 10);
            assert_eq!(deq, sdeq![1, 3, 5, 7, 9, 11, 13, 15, 17, 19]);
        }

        {
            //                [+++++++++++xxxxxxxxxx]
            let mut deq = sdeq![
                1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 2, 4, 6, 8, 10, 12, 14, 16,
                18, 20,
            ];

            let removed =
                deq.drain_filter(|x| *x % 2 == 0).collect::<SliceDeque<_>>();
            assert_eq!(removed.len(), 10);
            assert_eq!(removed, sdeq![2, 4, 6, 8, 10, 12, 14, 16, 18, 20]);

            assert_eq!(deq.len(), 10);
            assert_eq!(deq, sdeq![1, 3, 5, 7, 9, 11, 13, 15, 17, 19]);
        }
    }

    #[test]
    fn vecdeque_simple() {
        let mut d = SliceDeque::new();
        assert_eq!(d.len(), 0);
        d.push_front(17);
        d.push_front(42);
        d.push_back(137);
        assert_eq!(d.len(), 3);
        d.push_back(137);
        assert_eq!(d.len(), 4);
        assert_eq!(*d.front().unwrap(), 42);
        assert_eq!(*d.back().unwrap(), 137);
        let mut i = d.pop_front();
        assert_eq!(i, Some(42));
        i = d.pop_back();
        assert_eq!(i, Some(137));
        i = d.pop_back();
        assert_eq!(i, Some(137));
        i = d.pop_back();
        assert_eq!(i, Some(17));
        assert_eq!(d.len(), 0);
        d.push_back(3);
        assert_eq!(d.len(), 1);
        d.push_front(2);
        assert_eq!(d.len(), 2);
        d.push_back(4);
        assert_eq!(d.len(), 3);
        d.push_front(1);
        assert_eq!(d.len(), 4);
        assert_eq!(d[0], 1);
        assert_eq!(d[1], 2);
        assert_eq!(d[2], 3);
        assert_eq!(d[3], 4);
    }

    #[cfg(test)]
    fn vecdeque_parameterized<T: Clone + PartialEq + fmt::Debug>(
        a: T, b: T, c: T, d: T,
    ) {
        let mut deq = SliceDeque::new();
        assert_eq!(deq.len(), 0);
        deq.push_front(a.clone());
        deq.push_front(b.clone());
        deq.push_back(c.clone());
        assert_eq!(deq.len(), 3);
        deq.push_back(d.clone());
        assert_eq!(deq.len(), 4);
        assert_eq!((*deq.front().unwrap()).clone(), b.clone());
        assert_eq!((*deq.back().unwrap()).clone(), d.clone());
        assert_eq!(deq.pop_front().unwrap(), b.clone());
        assert_eq!(deq.pop_back().unwrap(), d.clone());
        assert_eq!(deq.pop_back().unwrap(), c.clone());
        assert_eq!(deq.pop_back().unwrap(), a.clone());
        assert_eq!(deq.len(), 0);
        deq.push_back(c.clone());
        assert_eq!(deq.len(), 1);
        deq.push_front(b.clone());
        assert_eq!(deq.len(), 2);
        deq.push_back(d.clone());
        assert_eq!(deq.len(), 3);
        deq.push_front(a.clone());
        assert_eq!(deq.len(), 4);
        assert_eq!(deq[0].clone(), a.clone());
        assert_eq!(deq[1].clone(), b.clone());
        assert_eq!(deq[2].clone(), c.clone());
        assert_eq!(deq[3].clone(), d.clone());
    }

    #[test]
    fn vecdeque_push_front_grow() {
        let mut deq = SliceDeque::new();
        for i in 0..66 {
            deq.push_front(i);
        }
        assert_eq!(deq.len(), 66);

        for i in 0..66 {
            assert_eq!(deq[i], 65 - i);
        }

        let mut deq = SliceDeque::new();
        for i in 0..66 {
            deq.push_back(i);
        }

        for i in 0..66 {
            assert_eq!(deq[i], i);
        }
    }

    #[test]
    fn vecdeque_index() {
        let mut deq = SliceDeque::new();
        for i in 1..4 {
            deq.push_front(i);
        }
        assert_eq!(deq[1], 2);
    }

    #[test]
    #[should_panic]
    fn vecdeque_index_out_of_bounds() {
        let mut deq = SliceDeque::new();
        for i in 1..4 {
            deq.push_front(i);
        }
        deq[3];
    }

    #[derive(Clone, PartialEq, Debug)]
    enum Taggy {
        One(i32),
        Two(i32, i32),
        Three(i32, i32, i32),
    }

    #[derive(Clone, PartialEq, Debug)]
    enum Taggypar<T> {
        Onepar(T),
        Twopar(T, T),
        Threepar(T, T, T),
    }

    #[derive(Clone, PartialEq, Debug)]
    struct RecCy {
        x: i32,
        y: i32,
        t: Taggy,
    }

    use self::Taggy::*;
    use self::Taggypar::*;

    fn hash<T: hash::Hash>(t: &T) -> u64 {
        let mut s = collections::hash_map::DefaultHasher::new();
        use hash::Hasher;
        t.hash(&mut s);
        s.finish()
    }

    #[test]
    fn vecdeque_param_int() {
        vecdeque_parameterized::<i32>(5, 72, 64, 175);
    }

    #[test]
    fn vecdeque_param_taggy() {
        vecdeque_parameterized::<Taggy>(
            One(1),
            Two(1, 2),
            Three(1, 2, 3),
            Two(17, 42),
        );
    }

    #[test]
    fn vecdeque_param_taggypar() {
        vecdeque_parameterized::<Taggypar<i32>>(
            Onepar::<i32>(1),
            Twopar::<i32>(1, 2),
            Threepar::<i32>(1, 2, 3),
            Twopar::<i32>(17, 42),
        );
    }

    #[test]
    fn vecdeque_param_reccy() {
        let reccy1 = RecCy {
            x: 1,
            y: 2,
            t: One(1),
        };
        let reccy2 = RecCy {
            x: 345,
            y: 2,
            t: Two(1, 2),
        };
        let reccy3 = RecCy {
            x: 1,
            y: 777,
            t: Three(1, 2, 3),
        };
        let reccy4 = RecCy {
            x: 19,
            y: 252,
            t: Two(17, 42),
        };
        vecdeque_parameterized::<RecCy>(reccy1, reccy2, reccy3, reccy4);
    }

    #[test]
    fn vecdeque_with_capacity() {
        let mut d = SliceDeque::with_capacity(0);
        d.push_back(1);
        assert_eq!(d.len(), 1);
        let mut d = SliceDeque::with_capacity(50);
        d.push_back(1);
        assert_eq!(d.len(), 1);
    }

    #[test]
    fn vecdeque_with_capacity_non_power_two() {
        let mut d3 = SliceDeque::with_capacity(3);
        d3.push_back(1);

        // X = None, | = lo
        // [|1, X, X]
        assert_eq!(d3.pop_front(), Some(1));
        // [X, |X, X]
        assert_eq!(d3.front(), None);

        // [X, |3, X]
        d3.push_back(3);
        // [X, |3, 6]
        d3.push_back(6);
        // [X, X, |6]
        assert_eq!(d3.pop_front(), Some(3));

        // Pushing the lo past half way point to trigger
        // the 'B' scenario for growth
        // [9, X, |6]
        d3.push_back(9);
        // [9, 12, |6]
        d3.push_back(12);

        d3.push_back(15);
        // There used to be a bug here about how the
        // SliceDeque made growth assumptions about the
        // underlying Vec which didn't hold and lead
        // to corruption.
        // (Vec grows to next power of two)
        // good- [9, 12, 15, X, X, X, X, |6]
        // bug-  [15, 12, X, X, X, |6, X, X]
        assert_eq!(d3.pop_front(), Some(6));

        // Which leads us to the following state which
        // would be a failure case.
        // bug-  [15, 12, X, X, X, X, |X, X]
        assert_eq!(d3.front(), Some(&9));
    }

    #[test]
    fn vecdeque_reserve_exact() {
        let mut d = SliceDeque::new();
        d.push_back(0);
        d.reserve_exact(50);
        assert!(d.capacity() >= 51);
    }

    #[test]
    fn vecdeque_reserve() {
        let mut d = SliceDeque::new();
        d.push_back(0);
        d.reserve(50);
        assert!(d.capacity() >= 51);
    }

    #[test]
    fn vecdeque_swap() {
        let mut d: SliceDeque<_> = (0..5).collect();
        d.pop_front();
        d.swap(0, 3);
        assert_eq!(d.iter().cloned().collect::<Vec<_>>(), [4, 2, 3, 1]);
    }

    #[test]
    fn vecdeque_iter() {
        let mut d = SliceDeque::new();
        assert_eq!(d.iter().next(), None);
        assert_eq!(d.iter().size_hint(), (0, Some(0)));

        for i in 0..5 {
            d.push_back(i);
        }
        {
            let b: &[_] = &[&0, &1, &2, &3, &4];
            assert_eq!(d.iter().collect::<Vec<_>>(), b);
        }

        for i in 6..9 {
            d.push_front(i);
        }
        {
            let b: &[_] = &[&8, &7, &6, &0, &1, &2, &3, &4];
            assert_eq!(d.iter().collect::<Vec<_>>(), b);
        }

        let mut it = d.iter();
        let mut len = d.len();
        loop {
            match it.next() {
                None => break,
                _ => {
                    len -= 1;
                    assert_eq!(it.size_hint(), (len, Some(len)))
                }
            }
        }
    }

    #[test]
    fn vecdeque_rev_iter() {
        let mut d = SliceDeque::new();
        assert_eq!(d.iter().rev().next(), None);

        for i in 0..5 {
            d.push_back(i);
        }
        {
            let b: &[_] = &[&4, &3, &2, &1, &0];
            assert_eq!(d.iter().rev().collect::<Vec<_>>(), b);
        }

        for i in 6..9 {
            d.push_front(i);
        }
        let b: &[_] = &[&4, &3, &2, &1, &0, &6, &7, &8];
        assert_eq!(d.iter().rev().collect::<Vec<_>>(), b);
    }

    #[test]
    fn vecdeque_mut_rev_iter_wrap() {
        let mut d = SliceDeque::with_capacity(3);
        assert!(d.iter_mut().rev().next().is_none());

        d.push_back(1);
        d.push_back(2);
        d.push_back(3);
        assert_eq!(d.pop_front(), Some(1));
        d.push_back(4);

        assert_eq!(
            d.iter_mut().rev().map(|x| *x).collect::<Vec<_>>(),
            vec![4, 3, 2]
        );
    }

    #[test]
    fn vecdeque_mut_iter() {
        let mut d = SliceDeque::new();
        assert!(d.iter_mut().next().is_none());

        for i in 0..3 {
            d.push_front(i);
        }

        for (i, elt) in d.iter_mut().enumerate() {
            assert_eq!(*elt, 2 - i);
            *elt = i;
        }

        {
            let mut it = d.iter_mut();
            assert_eq!(*it.next().unwrap(), 0);
            assert_eq!(*it.next().unwrap(), 1);
            assert_eq!(*it.next().unwrap(), 2);
            assert!(it.next().is_none());
        }
    }

    #[test]
    fn vecdeque_mut_rev_iter() {
        let mut d = SliceDeque::new();
        assert!(d.iter_mut().rev().next().is_none());

        for i in 0..3 {
            d.push_front(i);
        }

        for (i, elt) in d.iter_mut().rev().enumerate() {
            assert_eq!(*elt, i);
            *elt = i;
        }

        {
            let mut it = d.iter_mut().rev();
            assert_eq!(*it.next().unwrap(), 0);
            assert_eq!(*it.next().unwrap(), 1);
            assert_eq!(*it.next().unwrap(), 2);
            assert!(it.next().is_none());
        }
    }

    #[test]
    fn vecdeque_into_iter() {
        // Empty iter
        {
            let d: SliceDeque<i32> = SliceDeque::new();
            let mut iter = d.into_iter();

            assert_eq!(iter.size_hint(), (0, Some(0)));
            assert_eq!(iter.next(), None);
            assert_eq!(iter.size_hint(), (0, Some(0)));
        }

        // simple iter
        {
            let mut d = SliceDeque::new();
            for i in 0..5 {
                d.push_back(i);
            }

            let b = vec![0, 1, 2, 3, 4];
            assert_eq!(d.into_iter().collect::<Vec<_>>(), b);
        }

        // wrapped iter
        {
            let mut d = SliceDeque::new();
            for i in 0..5 {
                d.push_back(i);
            }
            for i in 6..9 {
                d.push_front(i);
            }

            let b = vec![8, 7, 6, 0, 1, 2, 3, 4];
            assert_eq!(d.into_iter().collect::<Vec<_>>(), b);
        }

        // partially used
        {
            let mut d = SliceDeque::new();
            for i in 0..5 {
                d.push_back(i);
            }
            for i in 6..9 {
                d.push_front(i);
            }

            let mut it = d.into_iter();
            assert_eq!(it.size_hint(), (8, Some(8)));
            assert_eq!(it.next(), Some(8));
            assert_eq!(it.size_hint(), (7, Some(7)));
            assert_eq!(it.next_back(), Some(4));
            assert_eq!(it.size_hint(), (6, Some(6)));
            assert_eq!(it.next(), Some(7));
            assert_eq!(it.size_hint(), (5, Some(5)));
        }
    }

    #[test]
    fn vecdeque_drain() {
        // Empty iter
        {
            let mut d: SliceDeque<i32> = SliceDeque::new();

            {
                let mut iter = d.drain(..);

                assert_eq!(iter.size_hint(), (0, Some(0)));
                assert_eq!(iter.next(), None);
                assert_eq!(iter.size_hint(), (0, Some(0)));
            }

            assert!(d.is_empty());
        }

        // simple iter
        {
            let mut d = SliceDeque::new();
            for i in 0..5 {
                d.push_back(i);
            }

            assert_eq!(d.drain(..).collect::<Vec<_>>(), [0, 1, 2, 3, 4]);
            assert!(d.is_empty());
        }

        // wrapped iter
        {
            let mut d = SliceDeque::new();
            for i in 0..5 {
                d.push_back(i);
            }
            for i in 6..9 {
                d.push_front(i);
            }

            assert_eq!(
                d.drain(..).collect::<Vec<_>>(),
                [8, 7, 6, 0, 1, 2, 3, 4]
            );
            assert!(d.is_empty());
        }

        // partially used
        {
            let mut d: SliceDeque<_> = SliceDeque::new();
            for i in 0..5 {
                d.push_back(i);
            }
            for i in 6..9 {
                d.push_front(i);
            }

            {
                let mut it = d.drain(..);
                assert_eq!(it.size_hint(), (8, Some(8)));
                assert_eq!(it.next(), Some(8));
                assert_eq!(it.size_hint(), (7, Some(7)));
                assert_eq!(it.next_back(), Some(4));
                assert_eq!(it.size_hint(), (6, Some(6)));
                assert_eq!(it.next(), Some(7));
                assert_eq!(it.size_hint(), (5, Some(5)));
            }
            assert!(d.is_empty());
        }
    }

    #[cfg(feature = "unstable")]
    #[test]
    fn vecdeque_from_iter() {
        let v = vec![1, 2, 3, 4, 5, 6, 7];
        let deq: SliceDeque<_> = v.iter().cloned().collect();
        let u: Vec<_> = deq.iter().cloned().collect();
        assert_eq!(u, v);

        let seq = (0..).step_by(2).take(256);
        let deq: SliceDeque<_> = seq.collect();
        for (i, &x) in deq.iter().enumerate() {
            assert_eq!(2 * i, x);
        }
        assert_eq!(deq.len(), 256);
    }

    #[test]
    fn vecdeque_clone() {
        let mut d = SliceDeque::new();
        d.push_front(17);
        d.push_front(42);
        d.push_back(137);
        d.push_back(137);
        assert_eq!(d.len(), 4);
        let mut e = d.clone();
        assert_eq!(e.len(), 4);
        while !d.is_empty() {
            assert_eq!(d.pop_back(), e.pop_back());
        }
        assert_eq!(d.len(), 0);
        assert_eq!(e.len(), 0);
    }

    #[test]
    fn vecdeque_eq() {
        let mut d = SliceDeque::new();
        assert!(d == SliceDeque::with_capacity(0));
        d.push_front(137);
        d.push_front(17);
        d.push_front(42);
        d.push_back(137);
        let mut e = SliceDeque::with_capacity(0);
        e.push_back(42);
        e.push_back(17);
        e.push_back(137);
        e.push_back(137);
        assert!(&e == &d);
        e.pop_back();
        e.push_back(0);
        assert!(e != d);
        e.clear();
        assert!(e == SliceDeque::new());
    }

    #[test]
    fn vecdeque_partial_eq_array() {
        let d = SliceDeque::<char>::new();
        assert!(d == []);

        let mut d = SliceDeque::new();
        d.push_front('a');
        assert!(d == ['a']);

        let mut d = SliceDeque::new();
        d.push_back('a');
        assert!(d == ['a']);

        let mut d = SliceDeque::new();
        d.push_back('a');
        d.push_back('b');
        assert!(d == ['a', 'b']);
    }

    #[test]
    fn vecdeque_hash() {
        let mut x = SliceDeque::new();
        let mut y = SliceDeque::new();

        x.push_back(1);
        x.push_back(2);
        x.push_back(3);

        y.push_back(0);
        y.push_back(1);
        y.pop_front();
        y.push_back(2);
        y.push_back(3);

        assert!(hash(&x) == hash(&y));
    }

    #[test]
    fn vecdeque_hash_after_rotation() {
        // test that two deques hash equal even if elements are laid out
        // differently
        let len = 28;
        let mut ring: SliceDeque<i32> = (0..len as i32).collect();
        let orig = ring.clone();
        for _ in 0..ring.capacity() {
            // shift values 1 step to the right by pop, sub one, push
            ring.pop_front();
            for elt in &mut ring {
                *elt -= 1;
            }
            ring.push_back(len - 1);
            assert_eq!(hash(&orig), hash(&ring));
            assert_eq!(orig, ring);
            assert_eq!(ring, orig);
        }
    }

    #[test]
    fn vecdeque_eq_after_rotation() {
        // test that two deques are equal even if elements are laid out
        // differently
        let len = 28;
        let mut ring: SliceDeque<i32> = (0..len as i32).collect();
        let mut shifted = ring.clone();
        for _ in 0..10 {
            // shift values 1 step to the right by pop, sub one, push
            ring.pop_front();
            for elt in &mut ring {
                *elt -= 1;
            }
            ring.push_back(len - 1);
        }

        // try every shift
        for _ in 0..shifted.capacity() {
            shifted.pop_front();
            for elt in &mut shifted {
                *elt -= 1;
            }
            shifted.push_back(len - 1);
            assert_eq!(shifted, ring);
            assert_eq!(ring, shifted);
        }
    }

    #[test]
    fn vecdeque_ord() {
        let x = SliceDeque::new();
        let mut y = SliceDeque::new();
        y.push_back(1);
        y.push_back(2);
        y.push_back(3);
        assert!(x < y);
        assert!(y > x);
        assert!(x <= x);
        assert!(x >= x);
    }

    #[test]
    fn vecdeque_show() {
        let ringbuf: SliceDeque<_> = (0..10).collect();
        assert_eq!(format!("{:?}", ringbuf), "[0, 1, 2, 3, 4, 5, 6, 7, 8, 9]");

        let ringbuf: SliceDeque<_> = vec!["just", "one", "test", "more"]
            .iter()
            .cloned()
            .collect();
        assert_eq!(
            format!("{:?}", ringbuf),
            "[\"just\", \"one\", \"test\", \"more\"]"
        );
    }

    #[test]
    fn vecdeque_drop_zst() {
        static mut DROPS: i32 = 0;
        struct Elem;
        impl Drop for Elem {
            fn drop(&mut self) {
                unsafe {
                    DROPS += 1;
                }
            }
        }

        let mut ring = SliceDeque::new();
        ring.push_back(Elem);
        ring.push_front(Elem);
        ring.push_back(Elem);
        ring.push_front(Elem);
        mem::drop(ring);

        assert_eq!(unsafe { DROPS }, 4);
    }

    #[test]
    fn vecdeque_drop() {
        static mut DROPS: i32 = 0;
        #[derive(Clone)]
        struct Elem(i32);
        impl Drop for Elem {
            fn drop(&mut self) {
                unsafe {
                    DROPS += 1;
                }
            }
        }

        let mut ring = SliceDeque::new();
        ring.push_back(Elem(0));
        ring.push_front(Elem(1));
        ring.push_back(Elem(2));
        ring.push_front(Elem(3));
        mem::drop(ring);

        assert_eq!(unsafe { DROPS }, 4);
    }

    #[test]
    fn vecdeque_drop_with_pop_zst() {
        static mut DROPS: i32 = 0;
        struct Elem;
        impl Drop for Elem {
            fn drop(&mut self) {
                unsafe {
                    DROPS += 1;
                }
            }
        }

        let mut ring = SliceDeque::new();
        ring.push_back(Elem);
        ring.push_front(Elem);
        ring.push_back(Elem);
        ring.push_front(Elem);

        mem::drop(ring.pop_back());
        mem::drop(ring.pop_front());
        assert_eq!(unsafe { DROPS }, 2);

        mem::drop(ring);
        assert_eq!(unsafe { DROPS }, 4);
    }

    #[test]
    fn vecdeque_drop_with_pop() {
        static mut DROPS: i32 = 0;
        struct Elem(i32);
        impl Drop for Elem {
            fn drop(&mut self) {
                unsafe {
                    DROPS += 1;
                }
            }
        }

        let mut ring = SliceDeque::new();
        ring.push_back(Elem(0));
        ring.push_front(Elem(0));
        ring.push_back(Elem(0));
        ring.push_front(Elem(0));

        mem::drop(ring.pop_back());
        mem::drop(ring.pop_front());
        assert_eq!(unsafe { DROPS }, 2);

        mem::drop(ring);
        assert_eq!(unsafe { DROPS }, 4);
    }

    #[test]
    fn vecdeque_drop_clear_zst() {
        static mut DROPS: i32 = 0;
        struct Elem;
        impl Drop for Elem {
            fn drop(&mut self) {
                unsafe {
                    DROPS += 1;
                }
            }
        }

        let mut ring = SliceDeque::new();
        ring.push_back(Elem);
        ring.push_front(Elem);
        ring.push_back(Elem);
        ring.push_front(Elem);
        ring.clear();
        assert_eq!(unsafe { DROPS }, 4);

        mem::drop(ring);
        assert_eq!(unsafe { DROPS }, 4);
    }

    #[test]
    fn vecdeque_drop_clear() {
        static mut DROPS: i32 = 0;
        struct Elem(i32);
        impl Drop for Elem {
            fn drop(&mut self) {
                unsafe {
                    DROPS += 1;
                }
            }
        }

        let mut ring = SliceDeque::new();
        ring.push_back(Elem(0));
        ring.push_front(Elem(0));
        ring.push_back(Elem(0));
        ring.push_front(Elem(0));
        ring.clear();
        assert_eq!(unsafe { DROPS }, 4);

        mem::drop(ring);
        assert_eq!(unsafe { DROPS }, 4);
    }

    #[test]
    fn vecdeque_reserve_grow() {
        // test growth path A
        // [T o o H] -> [T o o H . . . . ]
        let mut ring = SliceDeque::with_capacity(4);
        for i in 0..3 {
            ring.push_back(i);
        }
        ring.reserve(7);
        for i in 0..3 {
            assert_eq!(ring.pop_front(), Some(i));
        }

        // test growth path B
        // [H T o o] -> [. T o o H . . . ]
        let mut ring = SliceDeque::with_capacity(4);
        for i in 0..1 {
            ring.push_back(i);
            assert_eq!(ring.pop_front(), Some(i));
        }
        for i in 0..3 {
            ring.push_back(i);
        }
        ring.reserve(7);
        for i in 0..3 {
            assert_eq!(ring.pop_front(), Some(i));
        }

        // test growth path C
        // [o o H T] -> [o o H . . . . T ]
        let mut ring = SliceDeque::with_capacity(4);
        for i in 0..3 {
            ring.push_back(i);
            assert_eq!(ring.pop_front(), Some(i));
        }
        for i in 0..3 {
            ring.push_back(i);
        }
        ring.reserve(7);
        for i in 0..3 {
            assert_eq!(ring.pop_front(), Some(i));
        }
    }

    #[test]
    fn vecdeque_get() {
        let mut ring = SliceDeque::new();
        ring.push_back(0);
        assert_eq!(ring.get(0), Some(&0));
        assert_eq!(ring.get(1), None);

        ring.push_back(1);
        assert_eq!(ring.get(0), Some(&0));
        assert_eq!(ring.get(1), Some(&1));
        assert_eq!(ring.get(2), None);

        ring.push_back(2);
        assert_eq!(ring.get(0), Some(&0));
        assert_eq!(ring.get(1), Some(&1));
        assert_eq!(ring.get(2), Some(&2));
        assert_eq!(ring.get(3), None);

        assert_eq!(ring.pop_front(), Some(0));
        assert_eq!(ring.get(0), Some(&1));
        assert_eq!(ring.get(1), Some(&2));
        assert_eq!(ring.get(2), None);

        assert_eq!(ring.pop_front(), Some(1));
        assert_eq!(ring.get(0), Some(&2));
        assert_eq!(ring.get(1), None);

        assert_eq!(ring.pop_front(), Some(2));
        assert_eq!(ring.get(0), None);
        assert_eq!(ring.get(1), None);
    }

    #[test]
    fn vecdeque_get_mut() {
        let mut ring = SliceDeque::new();
        for i in 0..3 {
            ring.push_back(i);
        }

        match ring.get_mut(1) {
            Some(x) => *x = -1,
            None => (),
        };

        assert_eq!(ring.get_mut(0), Some(&mut 0));
        assert_eq!(ring.get_mut(1), Some(&mut -1));
        assert_eq!(ring.get_mut(2), Some(&mut 2));
        assert_eq!(ring.get_mut(3), None);

        assert_eq!(ring.pop_front(), Some(0));
        assert_eq!(ring.get_mut(0), Some(&mut -1));
        assert_eq!(ring.get_mut(1), Some(&mut 2));
        assert_eq!(ring.get_mut(2), None);
    }

    #[test]
    fn vecdeque_front() {
        let mut ring = SliceDeque::new();
        ring.push_back(10);
        ring.push_back(20);
        assert_eq!(ring.front(), Some(&10));
        ring.pop_front();
        assert_eq!(ring.front(), Some(&20));
        ring.pop_front();
        assert_eq!(ring.front(), None);
    }

    #[test]
    fn vecdeque_as_slices() {
        let mut ring: SliceDeque<i32> = SliceDeque::with_capacity(127);
        let cap = ring.capacity() as i32;
        let first = cap / 2;
        let last = cap - first;
        for i in 0..first {
            ring.push_back(i);

            let (left, right) = ring.as_slices();
            let expected: Vec<_> = (0..i + 1).collect();
            assert_eq!(left, &expected[..]);
            assert_eq!(right, []);
        }

        for j in -last..0 {
            ring.push_front(j);
            let (left, right) = ring.as_slices();
            let mut expected_left: Vec<_> = (-last..j + 1).rev().collect();
            expected_left.extend(0..first);
            assert_eq!(left, &expected_left[..]);
            assert_eq!(right, []);
        }

        assert_eq!(ring.len() as i32, cap);
        assert_eq!(ring.capacity() as i32, cap);
    }

    #[test]
    fn vecdeque_as_mut_slices() {
        let mut ring: SliceDeque<i32> = SliceDeque::with_capacity(127);
        let cap = ring.capacity() as i32;
        let first = cap / 2;
        let last = cap - first;
        for i in 0..first {
            ring.push_back(i);

            let (left, right) = ring.as_mut_slices();
            let expected: Vec<_> = (0..i + 1).collect();
            assert_eq!(left, &expected[..]);
            assert_eq!(right, []);
        }

        for j in -last..0 {
            ring.push_front(j);
            let (left, right) = ring.as_mut_slices();
            let mut expected_left: Vec<_> = (-last..j + 1).rev().collect();
            expected_left.extend(0..first);
            assert_eq!(left, &expected_left[..]);
            assert_eq!(right, []);
        }

        assert_eq!(ring.len() as i32, cap);
        assert_eq!(ring.capacity() as i32, cap);
    }

    #[test]
    fn vecdeque_append() {
        let mut a: SliceDeque<_> = vec![1, 2, 3].into_iter().collect();
        let mut b: SliceDeque<_> = vec![4, 5, 6].into_iter().collect();

        // normal append
        a.append(&mut b);
        assert_eq!(a.iter().cloned().collect::<Vec<_>>(), [1, 2, 3, 4, 5, 6]);
        assert_eq!(b.iter().cloned().collect::<Vec<_>>(), []);

        // append nothing to something
        a.append(&mut b);
        assert_eq!(a.iter().cloned().collect::<Vec<_>>(), [1, 2, 3, 4, 5, 6]);
        assert_eq!(b.iter().cloned().collect::<Vec<_>>(), []);

        // append something to nothing
        b.append(&mut a);
        assert_eq!(b.iter().cloned().collect::<Vec<_>>(), [1, 2, 3, 4, 5, 6]);
        assert_eq!(a.iter().cloned().collect::<Vec<_>>(), []);
    }

    #[test]
    fn vecdeque_retain() {
        let mut buf = SliceDeque::new();
        buf.extend(1..5);
        buf.retain(|&x| x % 2 == 0);
        let v: Vec<_> = buf.into_iter().collect();
        assert_eq!(&v[..], &[2, 4]);
    }

    #[test]
    fn vecdeque_extend_ref() {
        let mut v = SliceDeque::new();
        v.push_back(1);
        v.extend(&[2, 3, 4]);

        assert_eq!(v.len(), 4);
        assert_eq!(v[0], 1);
        assert_eq!(v[1], 2);
        assert_eq!(v[2], 3);
        assert_eq!(v[3], 4);

        let mut w = SliceDeque::new();
        w.push_back(5);
        w.push_back(6);
        v.extend(&w);

        assert_eq!(v.len(), 6);
        assert_eq!(v[0], 1);
        assert_eq!(v[1], 2);
        assert_eq!(v[2], 3);
        assert_eq!(v[3], 4);
        assert_eq!(v[4], 5);
        assert_eq!(v[5], 6);
    }

    #[test]
    fn vecdeque_contains() {
        let mut v = SliceDeque::new();
        v.extend(&[2, 3, 4]);

        assert!(v.contains(&3));
        assert!(!v.contains(&1));

        v.clear();

        assert!(!v.contains(&3));
    }

    /* TODO: covariance
    #[allow(dead_code)]
    fn assert_covariance() {
        fn drain<'new>(d: Drain<'static, &'static str>) -> Drain<'new, &'new str> {
            d
        }
    }
        */

    #[cfg(feature = "unstable")]
    #[test]
    fn vecdeque_is_empty() {
        let mut v = SliceDeque::<i32>::new();
        assert!(v.is_empty());
        assert!(v.iter().is_empty());
        assert!(v.iter_mut().is_empty());
        v.extend(&[2, 3, 4]);
        assert!(!v.is_empty());
        assert!(!v.iter().is_empty());
        assert!(!v.iter_mut().is_empty());
        while let Some(_) = v.pop_front() {
            assert_eq!(v.is_empty(), v.len() == 0);
            assert_eq!(v.iter().is_empty(), v.iter().len() == 0);
            assert_eq!(v.iter_mut().is_empty(), v.iter_mut().len() == 0);
        }
        assert!(v.is_empty());
        assert!(v.iter().is_empty());
        assert!(v.iter_mut().is_empty());
        assert!(v.into_iter().is_empty());
    }

    #[cfg(all(feature = "bytes_buf", feature = "use_std"))]
    #[test]
    fn bytes_bufmut() {
        use bytes::{BigEndian, BufMut};
        use std::io::Write;

        {
            let mut buf = sdeq![];

            buf.put("hello world");

            assert_eq!(buf, b"hello world");
        }
        {
            let mut buf = SliceDeque::with_capacity(16);

            unsafe {
                buf.bytes_mut()[0] = b'h';
                buf.bytes_mut()[1] = b'e';

                buf.advance_mut(2);

                buf.bytes_mut()[0] = b'l';
                buf.bytes_mut()[1..3].copy_from_slice(b"lo");

                buf.advance_mut(3);
            }

            assert_eq!(5, buf.len());
            assert_eq!(buf, b"hello");
        }
        {
            let mut buf = SliceDeque::with_capacity(16);

            unsafe {
                buf.bytes_mut()[0] = b'h';
                buf.bytes_mut()[1] = b'e';

                buf.advance_mut(2);

                buf.bytes_mut()[0] = b'l';
                buf.bytes_mut()[1..3].copy_from_slice(b"lo");

                buf.advance_mut(3);
            }

            assert_eq!(5, buf.len());
            assert_eq!(buf, b"hello");
        }
        {
            let mut buf = sdeq![];

            buf.put(b'h');
            buf.put(&b"ello"[..]);
            buf.put(" world");

            assert_eq!(buf, b"hello world");
        }
        {
            let mut buf = sdeq![];
            buf.put_u8(0x01);
            assert_eq!(buf, b"\x01");
        }
        {
            let mut buf = sdeq![];
            buf.put_i8(0x01);
            assert_eq!(buf, b"\x01");
        }
        {
            let mut buf = sdeq![];
            buf.put_u16::<BigEndian>(0x0809);
            assert_eq!(buf, b"\x08\x09");
        }
        {
            let mut buf = sdeq![];

            {
                let reference = buf.by_ref();

                // Adapt reference to `std::io::Write`.
                let mut writer = reference.writer();

                // Use the buffer as a writter
                Write::write(&mut writer, &b"hello world"[..]).unwrap();
            } // drop our &mut reference so that we can use `buf` again

            assert_eq!(buf, &b"hello world"[..]);
        }
        {
            let mut buf = sdeq![].writer();

            let num = buf.write(&b"hello world"[..]).unwrap();
            assert_eq!(11, num);

            let buf = buf.into_inner();

            assert_eq!(*buf, b"hello world"[..]);
        }
    }

    #[cfg(all(feature = "bytes_buf", feature = "use_std"))]
    #[test]
    fn bytes_buf() {
        {
            use bytes::{Buf, Bytes, IntoBuf};

            let buf = Bytes::from(&b"hello world"[..]).into_buf();
            let vec: SliceDeque<u8> = buf.collect();

            assert_eq!(vec, &b"hello world"[..]);
        }
        {
            use bytes::{Buf, BufMut};
            use std::io::Cursor;

            let mut buf = Cursor::new("hello world").take(5);
            let mut dst = sdeq![];

            dst.put(&mut buf);
            assert_eq!(dst, b"hello");

            let mut buf = buf.into_inner();
            dst.clear();
            dst.put(&mut buf);
            assert_eq!(dst, b" world");
        }
        {
            use bytes::{Buf, BufMut};
            use std::io::Cursor;

            let mut buf = Cursor::new("hello world");
            let mut dst = sdeq![];

            {
                let reference = buf.by_ref();
                dst.put(&mut reference.take(5));
                assert_eq!(dst, b"hello");
            } // drop our &mut reference so we can use `buf` again

            dst.clear();
            dst.put(&mut buf);
            assert_eq!(dst, b" world");
        }
    }

    #[test]
    fn issue_42() {
        // https://github.com/gnzlbg/slice_deque/issues/42
        let page_size = crate::mirrored::allocation_granularity();
        let mut deque = SliceDeque::<u8>::with_capacity(page_size);
        let page_size = page_size as isize;

        let slice = unsafe {
            deque.move_tail(page_size);
            deque.move_head(page_size / 100 * 99);
            deque.move_tail(page_size / 100 * 99);
            deque.move_head(page_size / 100 * 99);
            deque.tail_head_slice()
        };

        for i in 0..slice.len() {
            // segfault:
            slice[i] = 0;
        }
    }

    #[test]
    fn issue_45() {
        // https://github.com/gnzlbg/slice_deque/issues/45
        fn refill(buf: &mut SliceDeque<u8>) {
            let data = [0u8; MAX_SAMPLES_PER_FRAME * 5];
            buf.extend(data.iter());
        }

        const MAX_SAMPLES_PER_FRAME: usize = 1152 * 2;
        const BUFFER_SIZE: usize = MAX_SAMPLES_PER_FRAME * 15;
        const REFILL_TRIGGER: usize = MAX_SAMPLES_PER_FRAME * 8;

        let mut buf = SliceDeque::with_capacity(BUFFER_SIZE);
        for _ in 0..10_000 {
            if buf.len() < REFILL_TRIGGER {
                refill(&mut buf);
            }

            let cur_len = buf.len();
            buf.truncate_front(cur_len - 10);
        }
    }

    #[test]
    fn issue_47() {
        let page_size = crate::mirrored::allocation_granularity();
        let mut sdq = SliceDeque::<u8>::new();
        let vec = vec![0_u8; page_size + 1];
        sdq.extend(vec);
    }

    #[test]
    fn issue_50() {
        use std::fs::File;
        use std::io::Write;
        use std::path::Path;

        let out_buffer = SliceDeque::new();

        let p = if cfg!(target_os = "windows") {
            "slice_deque_test"
        } else {
            "/tmp/slice_deque_test"
        };

        let mut out_file = File::create(Path::new(p)).unwrap();
        let res = out_file.write(&out_buffer[..]);
        println!("Result was {:?}", res);
        println!("Buffer size: {}", out_buffer.len());
        println!("Address of buffer was: {:?}", out_buffer.as_ptr());
    }

    #[test]
    fn empty_ptr() {
        {
            let sdeq = SliceDeque::<i8>::new();
            let v = Vec::<i8>::new();
            assert_eq!(sdeq.as_ptr() as usize, mem::align_of::<i8>());
            assert_eq!(v.as_ptr() as usize, mem::align_of::<i8>());
        }
        {
            let sdeq = SliceDeque::<i16>::new();
            let v = Vec::<i16>::new();
            assert_eq!(sdeq.as_ptr() as usize, mem::align_of::<i16>());
            assert_eq!(v.as_ptr() as usize, mem::align_of::<i16>());
        }
        {
            let sdeq = SliceDeque::<i32>::new();
            let v = Vec::<i32>::new();
            assert_eq!(sdeq.as_ptr() as usize, mem::align_of::<i32>());
            assert_eq!(v.as_ptr() as usize, mem::align_of::<i32>());
        }
        {
            let sdeq = SliceDeque::<i64>::new();
            let v = Vec::<i64>::new();
            assert_eq!(sdeq.as_ptr() as usize, mem::align_of::<i64>());
            assert_eq!(v.as_ptr() as usize, mem::align_of::<i64>());
        }
        {
            #[repr(align(32))]
            struct Foo(i8);
            let sdeq = SliceDeque::<Foo>::new();
            let v = Vec::<Foo>::new();
            assert_eq!(sdeq.as_ptr() as usize, mem::align_of::<Foo>());
            assert_eq!(v.as_ptr() as usize, mem::align_of::<Foo>());
        }
    }

    #[test]
    fn issue_57() {
        const C: [i16; 3] = [42; 3];

        let mut deque = SliceDeque::new();

        for _ in 0..918 {
            deque.push_front(C);
        }

        for _ in 0..237 {
            assert_eq!(deque.pop_front(), Some(C));
            assert!(!deque.is_empty());
            assert_eq!(*deque.back().unwrap(), C);
        }
    }

    #[test]
    fn drop_count() {
        #[repr(C)]
        struct Foo(*mut usize);
        impl Drop for Foo {
            fn drop(&mut self) {
                unsafe {
                    *self.0 += 1;
                }
            }
        }

        let mut count = 0_usize;
        {
            let mut deque = SliceDeque::new();
            for _ in 0..10 {
                deque.push_back(Foo(&mut count as *mut _));
                deque.pop_front();
            }
        }
        assert_eq!(count, 10);
    }

    #[test]
    fn non_fitting_elem_type() {
        // This type does not perfectly fit in an "allocation granularity"
        // unit (e.g. a memory page). A new type has always (1, 2, 3),
        // while free-ing the type writes (4, 5, 6) to the memory.
        #[repr(C)]
        struct NonFitting(u8, u8, u8);
        impl NonFitting {
            fn new() -> Self {
                Self(1, 2, 3)
            }
            fn valid(&self) -> bool {
                //dbg!("valid", self as *const Self as usize);
                if self.0 == 1 && self.1 == 2 && self.2 == 3 {
                    true
                } else {
                    dbg!((self.0, self.1, self.2));
                    false
                }
            }
        }

        impl Drop for NonFitting {
            fn drop(&mut self) {
                //dbg!(("drop", self as *mut Self as usize));
                unsafe {
                    let ptr = self as *mut Self as *mut u8;
                    ptr.write_volatile(4);
                    ptr.add(1).write_volatile(5);
                    ptr.add(2).write_volatile(6);
                }
            }
        }

        let page_size = crate::mirrored::allocation_granularity();
        let elem_size = mem::size_of::<NonFitting>();

        assert!(elem_size % page_size != 0);
        let no_elements_that_fit = page_size / elem_size;

        // Create a deque, which has a single element
        // right at the end of the first page.
        //
        // We do that by shifting a single element till the end, which can be
        // achieved by pushing an element and while popping the previous one.
        let mut deque = SliceDeque::new();
        deque.push_back(NonFitting::new());

        for i in 0..no_elements_that_fit {
            if i > no_elements_that_fit - 2 {
                //dbg!((i, deque.len()));
            }
            //dbg!(("push_back", deque.len()));
            deque.push_back(NonFitting::new());
            //dbg!(("pop_front", deque.len()));
            deque.truncate_front(1);
            assert_eq!(deque.len(), 1);
            assert!(deque[0].valid());
        }
    }

    #[test]
    fn issue_57_2() {
        let mut deque = SliceDeque::new();
        for _ in 0..30_000 {
            deque.push_back(String::from("test"));
            if deque.len() == 8 {
                deque.pop_front();
            }
        }
    }

    #[test]
    fn zst() {
        struct A;
        let mut s = SliceDeque::<A>::new();
        assert_eq!(s.len(), 0);
        assert_eq!(s.capacity(), isize::max_value() as usize);

        for _ in 0..10 {
            s.push_back(A);
        }
        assert_eq!(s.len(), 10);
    }

    #[test]
    fn sync() {
        fn assert_sync<T: Sync>(_: T) {}

        struct S(*mut u8);
        unsafe impl Sync for S {}
        let x = SliceDeque::<S>::new();
        assert_sync(x);
    }

    #[test]
    fn send() {
        fn assert_send<T: Send>(_: T) {}

        struct S(*mut u8);
        unsafe impl Send for S {}
        let x = SliceDeque::<S>::new();
        assert_send(x);
    }
}
