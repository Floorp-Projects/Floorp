use core::iter::{FromIterator, Iterator};
use core::mem::{self, ManuallyDrop};
use core::ops::{Deref, DerefMut};
use core::ptr::{self, NonNull};
use core::{cmp, fmt, hash, isize, slice, usize};

use alloc::{
    borrow::{Borrow, BorrowMut},
    boxed::Box,
    string::String,
    vec::Vec,
};

use crate::buf::IntoIter;
use crate::bytes::Vtable;
#[allow(unused)]
use crate::loom::sync::atomic::AtomicMut;
use crate::loom::sync::atomic::{self, AtomicPtr, AtomicUsize, Ordering};
use crate::{Buf, BufMut, Bytes};

/// A unique reference to a contiguous slice of memory.
///
/// `BytesMut` represents a unique view into a potentially shared memory region.
/// Given the uniqueness guarantee, owners of `BytesMut` handles are able to
/// mutate the memory.
///
/// `BytesMut` can be thought of as containing a `buf: Arc<Vec<u8>>`, an offset
/// into `buf`, a slice length, and a guarantee that no other `BytesMut` for the
/// same `buf` overlaps with its slice. That guarantee means that a write lock
/// is not required.
///
/// # Growth
///
/// `BytesMut`'s `BufMut` implementation will implicitly grow its buffer as
/// necessary. However, explicitly reserving the required space up-front before
/// a series of inserts will be more efficient.
///
/// # Examples
///
/// ```
/// use bytes::{BytesMut, BufMut};
///
/// let mut buf = BytesMut::with_capacity(64);
///
/// buf.put_u8(b'h');
/// buf.put_u8(b'e');
/// buf.put(&b"llo"[..]);
///
/// assert_eq!(&buf[..], b"hello");
///
/// // Freeze the buffer so that it can be shared
/// let a = buf.freeze();
///
/// // This does not allocate, instead `b` points to the same memory.
/// let b = a.clone();
///
/// assert_eq!(&a[..], b"hello");
/// assert_eq!(&b[..], b"hello");
/// ```
pub struct BytesMut {
    ptr: NonNull<u8>,
    len: usize,
    cap: usize,
    data: *mut Shared,
}

// Thread-safe reference-counted container for the shared storage. This mostly
// the same as `core::sync::Arc` but without the weak counter. The ref counting
// fns are based on the ones found in `std`.
//
// The main reason to use `Shared` instead of `core::sync::Arc` is that it ends
// up making the overall code simpler and easier to reason about. This is due to
// some of the logic around setting `Inner::arc` and other ways the `arc` field
// is used. Using `Arc` ended up requiring a number of funky transmutes and
// other shenanigans to make it work.
struct Shared {
    vec: Vec<u8>,
    original_capacity_repr: usize,
    ref_count: AtomicUsize,
}

// Buffer storage strategy flags.
const KIND_ARC: usize = 0b0;
const KIND_VEC: usize = 0b1;
const KIND_MASK: usize = 0b1;

// The max original capacity value. Any `Bytes` allocated with a greater initial
// capacity will default to this.
const MAX_ORIGINAL_CAPACITY_WIDTH: usize = 17;
// The original capacity algorithm will not take effect unless the originally
// allocated capacity was at least 1kb in size.
const MIN_ORIGINAL_CAPACITY_WIDTH: usize = 10;
// The original capacity is stored in powers of 2 starting at 1kb to a max of
// 64kb. Representing it as such requires only 3 bits of storage.
const ORIGINAL_CAPACITY_MASK: usize = 0b11100;
const ORIGINAL_CAPACITY_OFFSET: usize = 2;

// When the storage is in the `Vec` representation, the pointer can be advanced
// at most this value. This is due to the amount of storage available to track
// the offset is usize - number of KIND bits and number of ORIGINAL_CAPACITY
// bits.
const VEC_POS_OFFSET: usize = 5;
const MAX_VEC_POS: usize = usize::MAX >> VEC_POS_OFFSET;
const NOT_VEC_POS_MASK: usize = 0b11111;

#[cfg(target_pointer_width = "64")]
const PTR_WIDTH: usize = 64;
#[cfg(target_pointer_width = "32")]
const PTR_WIDTH: usize = 32;

/*
 *
 * ===== BytesMut =====
 *
 */

impl BytesMut {
    /// Creates a new `BytesMut` with the specified capacity.
    ///
    /// The returned `BytesMut` will be able to hold at least `capacity` bytes
    /// without reallocating.
    ///
    /// It is important to note that this function does not specify the length
    /// of the returned `BytesMut`, but only the capacity.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::{BytesMut, BufMut};
    ///
    /// let mut bytes = BytesMut::with_capacity(64);
    ///
    /// // `bytes` contains no data, even though there is capacity
    /// assert_eq!(bytes.len(), 0);
    ///
    /// bytes.put(&b"hello world"[..]);
    ///
    /// assert_eq!(&bytes[..], b"hello world");
    /// ```
    #[inline]
    pub fn with_capacity(capacity: usize) -> BytesMut {
        BytesMut::from_vec(Vec::with_capacity(capacity))
    }

    /// Creates a new `BytesMut` with default capacity.
    ///
    /// Resulting object has length 0 and unspecified capacity.
    /// This function does not allocate.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::{BytesMut, BufMut};
    ///
    /// let mut bytes = BytesMut::new();
    ///
    /// assert_eq!(0, bytes.len());
    ///
    /// bytes.reserve(2);
    /// bytes.put_slice(b"xy");
    ///
    /// assert_eq!(&b"xy"[..], &bytes[..]);
    /// ```
    #[inline]
    pub fn new() -> BytesMut {
        BytesMut::with_capacity(0)
    }

    /// Returns the number of bytes contained in this `BytesMut`.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::BytesMut;
    ///
    /// let b = BytesMut::from(&b"hello"[..]);
    /// assert_eq!(b.len(), 5);
    /// ```
    #[inline]
    pub fn len(&self) -> usize {
        self.len
    }

    /// Returns true if the `BytesMut` has a length of 0.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::BytesMut;
    ///
    /// let b = BytesMut::with_capacity(64);
    /// assert!(b.is_empty());
    /// ```
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    /// Returns the number of bytes the `BytesMut` can hold without reallocating.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::BytesMut;
    ///
    /// let b = BytesMut::with_capacity(64);
    /// assert_eq!(b.capacity(), 64);
    /// ```
    #[inline]
    pub fn capacity(&self) -> usize {
        self.cap
    }

    /// Converts `self` into an immutable `Bytes`.
    ///
    /// The conversion is zero cost and is used to indicate that the slice
    /// referenced by the handle will no longer be mutated. Once the conversion
    /// is done, the handle can be cloned and shared across threads.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::{BytesMut, BufMut};
    /// use std::thread;
    ///
    /// let mut b = BytesMut::with_capacity(64);
    /// b.put(&b"hello world"[..]);
    /// let b1 = b.freeze();
    /// let b2 = b1.clone();
    ///
    /// let th = thread::spawn(move || {
    ///     assert_eq!(&b1[..], b"hello world");
    /// });
    ///
    /// assert_eq!(&b2[..], b"hello world");
    /// th.join().unwrap();
    /// ```
    #[inline]
    pub fn freeze(mut self) -> Bytes {
        if self.kind() == KIND_VEC {
            // Just re-use `Bytes` internal Vec vtable
            unsafe {
                let (off, _) = self.get_vec_pos();
                let vec = rebuild_vec(self.ptr.as_ptr(), self.len, self.cap, off);
                mem::forget(self);
                let mut b: Bytes = vec.into();
                b.advance(off);
                b
            }
        } else {
            debug_assert_eq!(self.kind(), KIND_ARC);

            let ptr = self.ptr.as_ptr();
            let len = self.len;
            let data = AtomicPtr::new(self.data as _);
            mem::forget(self);
            unsafe { Bytes::with_vtable(ptr, len, data, &SHARED_VTABLE) }
        }
    }

    /// Splits the bytes into two at the given index.
    ///
    /// Afterwards `self` contains elements `[0, at)`, and the returned
    /// `BytesMut` contains elements `[at, capacity)`.
    ///
    /// This is an `O(1)` operation that just increases the reference count
    /// and sets a few indices.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::BytesMut;
    ///
    /// let mut a = BytesMut::from(&b"hello world"[..]);
    /// let mut b = a.split_off(5);
    ///
    /// a[0] = b'j';
    /// b[0] = b'!';
    ///
    /// assert_eq!(&a[..], b"jello");
    /// assert_eq!(&b[..], b"!world");
    /// ```
    ///
    /// # Panics
    ///
    /// Panics if `at > capacity`.
    #[must_use = "consider BytesMut::truncate if you don't need the other half"]
    pub fn split_off(&mut self, at: usize) -> BytesMut {
        assert!(
            at <= self.capacity(),
            "split_off out of bounds: {:?} <= {:?}",
            at,
            self.capacity(),
        );
        unsafe {
            let mut other = self.shallow_clone();
            other.set_start(at);
            self.set_end(at);
            other
        }
    }

    /// Removes the bytes from the current view, returning them in a new
    /// `BytesMut` handle.
    ///
    /// Afterwards, `self` will be empty, but will retain any additional
    /// capacity that it had before the operation. This is identical to
    /// `self.split_to(self.len())`.
    ///
    /// This is an `O(1)` operation that just increases the reference count and
    /// sets a few indices.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::{BytesMut, BufMut};
    ///
    /// let mut buf = BytesMut::with_capacity(1024);
    /// buf.put(&b"hello world"[..]);
    ///
    /// let other = buf.split();
    ///
    /// assert!(buf.is_empty());
    /// assert_eq!(1013, buf.capacity());
    ///
    /// assert_eq!(other, b"hello world"[..]);
    /// ```
    #[must_use = "consider BytesMut::advance(len()) if you don't need the other half"]
    pub fn split(&mut self) -> BytesMut {
        let len = self.len();
        self.split_to(len)
    }

    /// Splits the buffer into two at the given index.
    ///
    /// Afterwards `self` contains elements `[at, len)`, and the returned `BytesMut`
    /// contains elements `[0, at)`.
    ///
    /// This is an `O(1)` operation that just increases the reference count and
    /// sets a few indices.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::BytesMut;
    ///
    /// let mut a = BytesMut::from(&b"hello world"[..]);
    /// let mut b = a.split_to(5);
    ///
    /// a[0] = b'!';
    /// b[0] = b'j';
    ///
    /// assert_eq!(&a[..], b"!world");
    /// assert_eq!(&b[..], b"jello");
    /// ```
    ///
    /// # Panics
    ///
    /// Panics if `at > len`.
    #[must_use = "consider BytesMut::advance if you don't need the other half"]
    pub fn split_to(&mut self, at: usize) -> BytesMut {
        assert!(
            at <= self.len(),
            "split_to out of bounds: {:?} <= {:?}",
            at,
            self.len(),
        );

        unsafe {
            let mut other = self.shallow_clone();
            other.set_end(at);
            self.set_start(at);
            other
        }
    }

    /// Shortens the buffer, keeping the first `len` bytes and dropping the
    /// rest.
    ///
    /// If `len` is greater than the buffer's current length, this has no
    /// effect.
    ///
    /// The [`split_off`] method can emulate `truncate`, but this causes the
    /// excess bytes to be returned instead of dropped.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::BytesMut;
    ///
    /// let mut buf = BytesMut::from(&b"hello world"[..]);
    /// buf.truncate(5);
    /// assert_eq!(buf, b"hello"[..]);
    /// ```
    ///
    /// [`split_off`]: #method.split_off
    pub fn truncate(&mut self, len: usize) {
        if len <= self.len() {
            unsafe {
                self.set_len(len);
            }
        }
    }

    /// Clears the buffer, removing all data.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::BytesMut;
    ///
    /// let mut buf = BytesMut::from(&b"hello world"[..]);
    /// buf.clear();
    /// assert!(buf.is_empty());
    /// ```
    pub fn clear(&mut self) {
        self.truncate(0);
    }

    /// Resizes the buffer so that `len` is equal to `new_len`.
    ///
    /// If `new_len` is greater than `len`, the buffer is extended by the
    /// difference with each additional byte set to `value`. If `new_len` is
    /// less than `len`, the buffer is simply truncated.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::BytesMut;
    ///
    /// let mut buf = BytesMut::new();
    ///
    /// buf.resize(3, 0x1);
    /// assert_eq!(&buf[..], &[0x1, 0x1, 0x1]);
    ///
    /// buf.resize(2, 0x2);
    /// assert_eq!(&buf[..], &[0x1, 0x1]);
    ///
    /// buf.resize(4, 0x3);
    /// assert_eq!(&buf[..], &[0x1, 0x1, 0x3, 0x3]);
    /// ```
    pub fn resize(&mut self, new_len: usize, value: u8) {
        let len = self.len();
        if new_len > len {
            let additional = new_len - len;
            self.reserve(additional);
            unsafe {
                let dst = self.bytes_mut().as_mut_ptr();
                ptr::write_bytes(dst, value, additional);
                self.set_len(new_len);
            }
        } else {
            self.truncate(new_len);
        }
    }

    /// Sets the length of the buffer.
    ///
    /// This will explicitly set the size of the buffer without actually
    /// modifying the data, so it is up to the caller to ensure that the data
    /// has been initialized.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::BytesMut;
    ///
    /// let mut b = BytesMut::from(&b"hello world"[..]);
    ///
    /// unsafe {
    ///     b.set_len(5);
    /// }
    ///
    /// assert_eq!(&b[..], b"hello");
    ///
    /// unsafe {
    ///     b.set_len(11);
    /// }
    ///
    /// assert_eq!(&b[..], b"hello world");
    /// ```
    #[inline]
    pub unsafe fn set_len(&mut self, len: usize) {
        debug_assert!(len <= self.cap, "set_len out of bounds");
        self.len = len;
    }

    /// Reserves capacity for at least `additional` more bytes to be inserted
    /// into the given `BytesMut`.
    ///
    /// More than `additional` bytes may be reserved in order to avoid frequent
    /// reallocations. A call to `reserve` may result in an allocation.
    ///
    /// Before allocating new buffer space, the function will attempt to reclaim
    /// space in the existing buffer. If the current handle references a small
    /// view in the original buffer and all other handles have been dropped,
    /// and the requested capacity is less than or equal to the existing
    /// buffer's capacity, then the current view will be copied to the front of
    /// the buffer and the handle will take ownership of the full buffer.
    ///
    /// # Examples
    ///
    /// In the following example, a new buffer is allocated.
    ///
    /// ```
    /// use bytes::BytesMut;
    ///
    /// let mut buf = BytesMut::from(&b"hello"[..]);
    /// buf.reserve(64);
    /// assert!(buf.capacity() >= 69);
    /// ```
    ///
    /// In the following example, the existing buffer is reclaimed.
    ///
    /// ```
    /// use bytes::{BytesMut, BufMut};
    ///
    /// let mut buf = BytesMut::with_capacity(128);
    /// buf.put(&[0; 64][..]);
    ///
    /// let ptr = buf.as_ptr();
    /// let other = buf.split();
    ///
    /// assert!(buf.is_empty());
    /// assert_eq!(buf.capacity(), 64);
    ///
    /// drop(other);
    /// buf.reserve(128);
    ///
    /// assert_eq!(buf.capacity(), 128);
    /// assert_eq!(buf.as_ptr(), ptr);
    /// ```
    ///
    /// # Panics
    ///
    /// Panics if the new capacity overflows `usize`.
    #[inline]
    pub fn reserve(&mut self, additional: usize) {
        let len = self.len();
        let rem = self.capacity() - len;

        if additional <= rem {
            // The handle can already store at least `additional` more bytes, so
            // there is no further work needed to be done.
            return;
        }

        self.reserve_inner(additional);
    }

    // In separate function to allow the short-circuits in `reserve` to
    // be inline-able. Significant helps performance.
    fn reserve_inner(&mut self, additional: usize) {
        let len = self.len();
        let kind = self.kind();

        if kind == KIND_VEC {
            // If there's enough free space before the start of the buffer, then
            // just copy the data backwards and reuse the already-allocated
            // space.
            //
            // Otherwise, since backed by a vector, use `Vec::reserve`
            unsafe {
                let (off, prev) = self.get_vec_pos();

                // Only reuse space if we can satisfy the requested additional space.
                if self.capacity() - self.len() + off >= additional {
                    // There's space - reuse it
                    //
                    // Just move the pointer back to the start after copying
                    // data back.
                    let base_ptr = self.ptr.as_ptr().offset(-(off as isize));
                    ptr::copy(self.ptr.as_ptr(), base_ptr, self.len);
                    self.ptr = vptr(base_ptr);
                    self.set_vec_pos(0, prev);

                    // Length stays constant, but since we moved backwards we
                    // can gain capacity back.
                    self.cap += off;
                } else {
                    // No space - allocate more
                    let mut v =
                        ManuallyDrop::new(rebuild_vec(self.ptr.as_ptr(), self.len, self.cap, off));
                    v.reserve(additional);

                    // Update the info
                    self.ptr = vptr(v.as_mut_ptr().offset(off as isize));
                    self.len = v.len() - off;
                    self.cap = v.capacity() - off;
                }

                return;
            }
        }

        debug_assert_eq!(kind, KIND_ARC);
        let shared: *mut Shared = self.data as _;

        // Reserving involves abandoning the currently shared buffer and
        // allocating a new vector with the requested capacity.
        //
        // Compute the new capacity
        let mut new_cap = len.checked_add(additional).expect("overflow");

        let original_capacity;
        let original_capacity_repr;

        unsafe {
            original_capacity_repr = (*shared).original_capacity_repr;
            original_capacity = original_capacity_from_repr(original_capacity_repr);

            // First, try to reclaim the buffer. This is possible if the current
            // handle is the only outstanding handle pointing to the buffer.
            if (*shared).is_unique() {
                // This is the only handle to the buffer. It can be reclaimed.
                // However, before doing the work of copying data, check to make
                // sure that the vector has enough capacity.
                let v = &mut (*shared).vec;

                if v.capacity() >= new_cap {
                    // The capacity is sufficient, reclaim the buffer
                    let ptr = v.as_mut_ptr();

                    ptr::copy(self.ptr.as_ptr(), ptr, len);

                    self.ptr = vptr(ptr);
                    self.cap = v.capacity();

                    return;
                }

                // The vector capacity is not sufficient. The reserve request is
                // asking for more than the initial buffer capacity. Allocate more
                // than requested if `new_cap` is not much bigger than the current
                // capacity.
                //
                // There are some situations, using `reserve_exact` that the
                // buffer capacity could be below `original_capacity`, so do a
                // check.
                let double = v.capacity().checked_shl(1).unwrap_or(new_cap);

                new_cap = cmp::max(cmp::max(double, new_cap), original_capacity);
            } else {
                new_cap = cmp::max(new_cap, original_capacity);
            }
        }

        // Create a new vector to store the data
        let mut v = ManuallyDrop::new(Vec::with_capacity(new_cap));

        // Copy the bytes
        v.extend_from_slice(self.as_ref());

        // Release the shared handle. This must be done *after* the bytes are
        // copied.
        unsafe { release_shared(shared) };

        // Update self
        let data = (original_capacity_repr << ORIGINAL_CAPACITY_OFFSET) | KIND_VEC;
        self.data = data as _;
        self.ptr = vptr(v.as_mut_ptr());
        self.len = v.len();
        self.cap = v.capacity();
    }

    /// Appends given bytes to this `BytesMut`.
    ///
    /// If this `BytesMut` object does not have enough capacity, it is resized
    /// first.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::BytesMut;
    ///
    /// let mut buf = BytesMut::with_capacity(0);
    /// buf.extend_from_slice(b"aaabbb");
    /// buf.extend_from_slice(b"cccddd");
    ///
    /// assert_eq!(b"aaabbbcccddd", &buf[..]);
    /// ```
    pub fn extend_from_slice(&mut self, extend: &[u8]) {
        let cnt = extend.len();
        self.reserve(cnt);

        unsafe {
            let dst = self.maybe_uninit_bytes();
            // Reserved above
            debug_assert!(dst.len() >= cnt);

            ptr::copy_nonoverlapping(extend.as_ptr(), dst.as_mut_ptr() as *mut u8, cnt);
        }

        unsafe {
            self.advance_mut(cnt);
        }
    }

    /// Absorbs a `BytesMut` that was previously split off.
    ///
    /// If the two `BytesMut` objects were previously contiguous, i.e., if
    /// `other` was created by calling `split_off` on this `BytesMut`, then
    /// this is an `O(1)` operation that just decreases a reference
    /// count and sets a few indices. Otherwise this method degenerates to
    /// `self.extend_from_slice(other.as_ref())`.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes::BytesMut;
    ///
    /// let mut buf = BytesMut::with_capacity(64);
    /// buf.extend_from_slice(b"aaabbbcccddd");
    ///
    /// let split = buf.split_off(6);
    /// assert_eq!(b"aaabbb", &buf[..]);
    /// assert_eq!(b"cccddd", &split[..]);
    ///
    /// buf.unsplit(split);
    /// assert_eq!(b"aaabbbcccddd", &buf[..]);
    /// ```
    pub fn unsplit(&mut self, other: BytesMut) {
        if self.is_empty() {
            *self = other;
            return;
        }

        if let Err(other) = self.try_unsplit(other) {
            self.extend_from_slice(other.as_ref());
        }
    }

    // private

    // For now, use a `Vec` to manage the memory for us, but we may want to
    // change that in the future to some alternate allocator strategy.
    //
    // Thus, we don't expose an easy way to construct from a `Vec` since an
    // internal change could make a simple pattern (`BytesMut::from(vec)`)
    // suddenly a lot more expensive.
    #[inline]
    pub(crate) fn from_vec(mut vec: Vec<u8>) -> BytesMut {
        let ptr = vptr(vec.as_mut_ptr());
        let len = vec.len();
        let cap = vec.capacity();
        mem::forget(vec);

        let original_capacity_repr = original_capacity_to_repr(cap);
        let data = (original_capacity_repr << ORIGINAL_CAPACITY_OFFSET) | KIND_VEC;

        BytesMut {
            ptr,
            len,
            cap,
            data: data as *mut _,
        }
    }

    #[inline]
    fn as_slice(&self) -> &[u8] {
        unsafe { slice::from_raw_parts(self.ptr.as_ptr(), self.len) }
    }

    #[inline]
    fn as_slice_mut(&mut self) -> &mut [u8] {
        unsafe { slice::from_raw_parts_mut(self.ptr.as_ptr(), self.len) }
    }

    unsafe fn set_start(&mut self, start: usize) {
        // Setting the start to 0 is a no-op, so return early if this is the
        // case.
        if start == 0 {
            return;
        }

        debug_assert!(start <= self.cap, "internal: set_start out of bounds");

        let kind = self.kind();

        if kind == KIND_VEC {
            // Setting the start when in vec representation is a little more
            // complicated. First, we have to track how far ahead the
            // "start" of the byte buffer from the beginning of the vec. We
            // also have to ensure that we don't exceed the maximum shift.
            let (mut pos, prev) = self.get_vec_pos();
            pos += start;

            if pos <= MAX_VEC_POS {
                self.set_vec_pos(pos, prev);
            } else {
                // The repr must be upgraded to ARC. This will never happen
                // on 64 bit systems and will only happen on 32 bit systems
                // when shifting past 134,217,727 bytes. As such, we don't
                // worry too much about performance here.
                self.promote_to_shared(/*ref_count = */ 1);
            }
        }

        // Updating the start of the view is setting `ptr` to point to the
        // new start and updating the `len` field to reflect the new length
        // of the view.
        self.ptr = vptr(self.ptr.as_ptr().offset(start as isize));

        if self.len >= start {
            self.len -= start;
        } else {
            self.len = 0;
        }

        self.cap -= start;
    }

    unsafe fn set_end(&mut self, end: usize) {
        debug_assert_eq!(self.kind(), KIND_ARC);
        assert!(end <= self.cap, "set_end out of bounds");

        self.cap = end;
        self.len = cmp::min(self.len, end);
    }

    fn try_unsplit(&mut self, other: BytesMut) -> Result<(), BytesMut> {
        if other.is_empty() {
            return Ok(());
        }

        let ptr = unsafe { self.ptr.as_ptr().offset(self.len as isize) };
        if ptr == other.ptr.as_ptr()
            && self.kind() == KIND_ARC
            && other.kind() == KIND_ARC
            && self.data == other.data
        {
            // Contiguous blocks, just combine directly
            self.len += other.len;
            self.cap += other.cap;
            Ok(())
        } else {
            Err(other)
        }
    }

    #[inline]
    fn kind(&self) -> usize {
        self.data as usize & KIND_MASK
    }

    unsafe fn promote_to_shared(&mut self, ref_cnt: usize) {
        debug_assert_eq!(self.kind(), KIND_VEC);
        debug_assert!(ref_cnt == 1 || ref_cnt == 2);

        let original_capacity_repr =
            (self.data as usize & ORIGINAL_CAPACITY_MASK) >> ORIGINAL_CAPACITY_OFFSET;

        // The vec offset cannot be concurrently mutated, so there
        // should be no danger reading it.
        let off = (self.data as usize) >> VEC_POS_OFFSET;

        // First, allocate a new `Shared` instance containing the
        // `Vec` fields. It's important to note that `ptr`, `len`,
        // and `cap` cannot be mutated without having `&mut self`.
        // This means that these fields will not be concurrently
        // updated and since the buffer hasn't been promoted to an
        // `Arc`, those three fields still are the components of the
        // vector.
        let shared = Box::new(Shared {
            vec: rebuild_vec(self.ptr.as_ptr(), self.len, self.cap, off),
            original_capacity_repr,
            ref_count: AtomicUsize::new(ref_cnt),
        });

        let shared = Box::into_raw(shared);

        // The pointer should be aligned, so this assert should
        // always succeed.
        debug_assert_eq!(shared as usize & KIND_MASK, KIND_ARC);

        self.data = shared as _;
    }

    /// Makes an exact shallow clone of `self`.
    ///
    /// The kind of `self` doesn't matter, but this is unsafe
    /// because the clone will have the same offsets. You must
    /// be sure the returned value to the user doesn't allow
    /// two views into the same range.
    #[inline]
    unsafe fn shallow_clone(&mut self) -> BytesMut {
        if self.kind() == KIND_ARC {
            increment_shared(self.data);
            ptr::read(self)
        } else {
            self.promote_to_shared(/*ref_count = */ 2);
            ptr::read(self)
        }
    }

    #[inline]
    unsafe fn get_vec_pos(&mut self) -> (usize, usize) {
        debug_assert_eq!(self.kind(), KIND_VEC);

        let prev = self.data as usize;
        (prev >> VEC_POS_OFFSET, prev)
    }

    #[inline]
    unsafe fn set_vec_pos(&mut self, pos: usize, prev: usize) {
        debug_assert_eq!(self.kind(), KIND_VEC);
        debug_assert!(pos <= MAX_VEC_POS);

        self.data = ((pos << VEC_POS_OFFSET) | (prev & NOT_VEC_POS_MASK)) as *mut _;
    }

    #[inline]
    fn maybe_uninit_bytes(&mut self) -> &mut [mem::MaybeUninit<u8>] {
        unsafe {
            let ptr = self.ptr.as_ptr().offset(self.len as isize);
            let len = self.cap - self.len;

            slice::from_raw_parts_mut(ptr as *mut mem::MaybeUninit<u8>, len)
        }
    }
}

impl Drop for BytesMut {
    fn drop(&mut self) {
        let kind = self.kind();

        if kind == KIND_VEC {
            unsafe {
                let (off, _) = self.get_vec_pos();

                // Vector storage, free the vector
                let _ = rebuild_vec(self.ptr.as_ptr(), self.len, self.cap, off);
            }
        } else if kind == KIND_ARC {
            unsafe { release_shared(self.data as _) };
        }
    }
}

impl Buf for BytesMut {
    #[inline]
    fn remaining(&self) -> usize {
        self.len()
    }

    #[inline]
    fn bytes(&self) -> &[u8] {
        self.as_slice()
    }

    #[inline]
    fn advance(&mut self, cnt: usize) {
        assert!(
            cnt <= self.remaining(),
            "cannot advance past `remaining`: {:?} <= {:?}",
            cnt,
            self.remaining(),
        );
        unsafe {
            self.set_start(cnt);
        }
    }

    fn to_bytes(&mut self) -> crate::Bytes {
        self.split().freeze()
    }
}

impl BufMut for BytesMut {
    #[inline]
    fn remaining_mut(&self) -> usize {
        usize::MAX - self.len()
    }

    #[inline]
    unsafe fn advance_mut(&mut self, cnt: usize) {
        let new_len = self.len() + cnt;
        assert!(
            new_len <= self.cap,
            "new_len = {}; capacity = {}",
            new_len,
            self.cap
        );
        self.len = new_len;
    }

    #[inline]
    fn bytes_mut(&mut self) -> &mut [mem::MaybeUninit<u8>] {
        if self.capacity() == self.len() {
            self.reserve(64);
        }
        self.maybe_uninit_bytes()
    }

    // Specialize these methods so they can skip checking `remaining_mut`
    // and `advance_mut`.

    fn put<T: crate::Buf>(&mut self, mut src: T)
    where
        Self: Sized,
    {
        while src.has_remaining() {
            let s = src.bytes();
            let l = s.len();
            self.extend_from_slice(s);
            src.advance(l);
        }
    }

    fn put_slice(&mut self, src: &[u8]) {
        self.extend_from_slice(src);
    }
}

impl AsRef<[u8]> for BytesMut {
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.as_slice()
    }
}

impl Deref for BytesMut {
    type Target = [u8];

    #[inline]
    fn deref(&self) -> &[u8] {
        self.as_ref()
    }
}

impl AsMut<[u8]> for BytesMut {
    #[inline]
    fn as_mut(&mut self) -> &mut [u8] {
        self.as_slice_mut()
    }
}

impl DerefMut for BytesMut {
    #[inline]
    fn deref_mut(&mut self) -> &mut [u8] {
        self.as_mut()
    }
}

impl<'a> From<&'a [u8]> for BytesMut {
    fn from(src: &'a [u8]) -> BytesMut {
        BytesMut::from_vec(src.to_vec())
    }
}

impl<'a> From<&'a str> for BytesMut {
    fn from(src: &'a str) -> BytesMut {
        BytesMut::from(src.as_bytes())
    }
}

impl From<BytesMut> for Bytes {
    fn from(src: BytesMut) -> Bytes {
        src.freeze()
    }
}

impl PartialEq for BytesMut {
    fn eq(&self, other: &BytesMut) -> bool {
        self.as_slice() == other.as_slice()
    }
}

impl PartialOrd for BytesMut {
    fn partial_cmp(&self, other: &BytesMut) -> Option<cmp::Ordering> {
        self.as_slice().partial_cmp(other.as_slice())
    }
}

impl Ord for BytesMut {
    fn cmp(&self, other: &BytesMut) -> cmp::Ordering {
        self.as_slice().cmp(other.as_slice())
    }
}

impl Eq for BytesMut {}

impl Default for BytesMut {
    #[inline]
    fn default() -> BytesMut {
        BytesMut::new()
    }
}

impl hash::Hash for BytesMut {
    fn hash<H>(&self, state: &mut H)
    where
        H: hash::Hasher,
    {
        let s: &[u8] = self.as_ref();
        s.hash(state);
    }
}

impl Borrow<[u8]> for BytesMut {
    fn borrow(&self) -> &[u8] {
        self.as_ref()
    }
}

impl BorrowMut<[u8]> for BytesMut {
    fn borrow_mut(&mut self) -> &mut [u8] {
        self.as_mut()
    }
}

impl fmt::Write for BytesMut {
    #[inline]
    fn write_str(&mut self, s: &str) -> fmt::Result {
        if self.remaining_mut() >= s.len() {
            self.put_slice(s.as_bytes());
            Ok(())
        } else {
            Err(fmt::Error)
        }
    }

    #[inline]
    fn write_fmt(&mut self, args: fmt::Arguments<'_>) -> fmt::Result {
        fmt::write(self, args)
    }
}

impl Clone for BytesMut {
    fn clone(&self) -> BytesMut {
        BytesMut::from(&self[..])
    }
}

impl IntoIterator for BytesMut {
    type Item = u8;
    type IntoIter = IntoIter<BytesMut>;

    fn into_iter(self) -> Self::IntoIter {
        IntoIter::new(self)
    }
}

impl<'a> IntoIterator for &'a BytesMut {
    type Item = &'a u8;
    type IntoIter = core::slice::Iter<'a, u8>;

    fn into_iter(self) -> Self::IntoIter {
        self.as_ref().into_iter()
    }
}

impl Extend<u8> for BytesMut {
    fn extend<T>(&mut self, iter: T)
    where
        T: IntoIterator<Item = u8>,
    {
        let iter = iter.into_iter();

        let (lower, _) = iter.size_hint();
        self.reserve(lower);

        // TODO: optimize
        // 1. If self.kind() == KIND_VEC, use Vec::extend
        // 2. Make `reserve` inline-able
        for b in iter {
            self.reserve(1);
            self.put_u8(b);
        }
    }
}

impl<'a> Extend<&'a u8> for BytesMut {
    fn extend<T>(&mut self, iter: T)
    where
        T: IntoIterator<Item = &'a u8>,
    {
        self.extend(iter.into_iter().map(|b| *b))
    }
}

impl FromIterator<u8> for BytesMut {
    fn from_iter<T: IntoIterator<Item = u8>>(into_iter: T) -> Self {
        BytesMut::from_vec(Vec::from_iter(into_iter))
    }
}

impl<'a> FromIterator<&'a u8> for BytesMut {
    fn from_iter<T: IntoIterator<Item = &'a u8>>(into_iter: T) -> Self {
        BytesMut::from_iter(into_iter.into_iter().map(|b| *b))
    }
}

/*
 *
 * ===== Inner =====
 *
 */

unsafe fn increment_shared(ptr: *mut Shared) {
    let old_size = (*ptr).ref_count.fetch_add(1, Ordering::Relaxed);

    if old_size > isize::MAX as usize {
        crate::abort();
    }
}

unsafe fn release_shared(ptr: *mut Shared) {
    // `Shared` storage... follow the drop steps from Arc.
    if (*ptr).ref_count.fetch_sub(1, Ordering::Release) != 1 {
        return;
    }

    // This fence is needed to prevent reordering of use of the data and
    // deletion of the data.  Because it is marked `Release`, the decreasing
    // of the reference count synchronizes with this `Acquire` fence. This
    // means that use of the data happens before decreasing the reference
    // count, which happens before this fence, which happens before the
    // deletion of the data.
    //
    // As explained in the [Boost documentation][1],
    //
    // > It is important to enforce any possible access to the object in one
    // > thread (through an existing reference) to *happen before* deleting
    // > the object in a different thread. This is achieved by a "release"
    // > operation after dropping a reference (any access to the object
    // > through this reference must obviously happened before), and an
    // > "acquire" operation before deleting the object.
    //
    // [1]: (www.boost.org/doc/libs/1_55_0/doc/html/atomic/usage_examples.html)
    atomic::fence(Ordering::Acquire);

    // Drop the data
    Box::from_raw(ptr);
}

impl Shared {
    fn is_unique(&self) -> bool {
        // The goal is to check if the current handle is the only handle
        // that currently has access to the buffer. This is done by
        // checking if the `ref_count` is currently 1.
        //
        // The `Acquire` ordering synchronizes with the `Release` as
        // part of the `fetch_sub` in `release_shared`. The `fetch_sub`
        // operation guarantees that any mutations done in other threads
        // are ordered before the `ref_count` is decremented. As such,
        // this `Acquire` will guarantee that those mutations are
        // visible to the current thread.
        self.ref_count.load(Ordering::Acquire) == 1
    }
}

fn original_capacity_to_repr(cap: usize) -> usize {
    let width = PTR_WIDTH - ((cap >> MIN_ORIGINAL_CAPACITY_WIDTH).leading_zeros() as usize);
    cmp::min(
        width,
        MAX_ORIGINAL_CAPACITY_WIDTH - MIN_ORIGINAL_CAPACITY_WIDTH,
    )
}

fn original_capacity_from_repr(repr: usize) -> usize {
    if repr == 0 {
        return 0;
    }

    1 << (repr + (MIN_ORIGINAL_CAPACITY_WIDTH - 1))
}

/*
#[test]
fn test_original_capacity_to_repr() {
    assert_eq!(original_capacity_to_repr(0), 0);

    let max_width = 32;

    for width in 1..(max_width + 1) {
        let cap = 1 << width - 1;

        let expected = if width < MIN_ORIGINAL_CAPACITY_WIDTH {
            0
        } else if width < MAX_ORIGINAL_CAPACITY_WIDTH {
            width - MIN_ORIGINAL_CAPACITY_WIDTH
        } else {
            MAX_ORIGINAL_CAPACITY_WIDTH - MIN_ORIGINAL_CAPACITY_WIDTH
        };

        assert_eq!(original_capacity_to_repr(cap), expected);

        if width > 1 {
            assert_eq!(original_capacity_to_repr(cap + 1), expected);
        }

        //  MIN_ORIGINAL_CAPACITY_WIDTH must be bigger than 7 to pass tests below
        if width == MIN_ORIGINAL_CAPACITY_WIDTH + 1 {
            assert_eq!(original_capacity_to_repr(cap - 24), expected - 1);
            assert_eq!(original_capacity_to_repr(cap + 76), expected);
        } else if width == MIN_ORIGINAL_CAPACITY_WIDTH + 2 {
            assert_eq!(original_capacity_to_repr(cap - 1), expected - 1);
            assert_eq!(original_capacity_to_repr(cap - 48), expected - 1);
        }
    }
}

#[test]
fn test_original_capacity_from_repr() {
    assert_eq!(0, original_capacity_from_repr(0));

    let min_cap = 1 << MIN_ORIGINAL_CAPACITY_WIDTH;

    assert_eq!(min_cap, original_capacity_from_repr(1));
    assert_eq!(min_cap * 2, original_capacity_from_repr(2));
    assert_eq!(min_cap * 4, original_capacity_from_repr(3));
    assert_eq!(min_cap * 8, original_capacity_from_repr(4));
    assert_eq!(min_cap * 16, original_capacity_from_repr(5));
    assert_eq!(min_cap * 32, original_capacity_from_repr(6));
    assert_eq!(min_cap * 64, original_capacity_from_repr(7));
}
*/

unsafe impl Send for BytesMut {}
unsafe impl Sync for BytesMut {}

/*
 *
 * ===== PartialEq / PartialOrd =====
 *
 */

impl PartialEq<[u8]> for BytesMut {
    fn eq(&self, other: &[u8]) -> bool {
        &**self == other
    }
}

impl PartialOrd<[u8]> for BytesMut {
    fn partial_cmp(&self, other: &[u8]) -> Option<cmp::Ordering> {
        (**self).partial_cmp(other)
    }
}

impl PartialEq<BytesMut> for [u8] {
    fn eq(&self, other: &BytesMut) -> bool {
        *other == *self
    }
}

impl PartialOrd<BytesMut> for [u8] {
    fn partial_cmp(&self, other: &BytesMut) -> Option<cmp::Ordering> {
        <[u8] as PartialOrd<[u8]>>::partial_cmp(self, other)
    }
}

impl PartialEq<str> for BytesMut {
    fn eq(&self, other: &str) -> bool {
        &**self == other.as_bytes()
    }
}

impl PartialOrd<str> for BytesMut {
    fn partial_cmp(&self, other: &str) -> Option<cmp::Ordering> {
        (**self).partial_cmp(other.as_bytes())
    }
}

impl PartialEq<BytesMut> for str {
    fn eq(&self, other: &BytesMut) -> bool {
        *other == *self
    }
}

impl PartialOrd<BytesMut> for str {
    fn partial_cmp(&self, other: &BytesMut) -> Option<cmp::Ordering> {
        <[u8] as PartialOrd<[u8]>>::partial_cmp(self.as_bytes(), other)
    }
}

impl PartialEq<Vec<u8>> for BytesMut {
    fn eq(&self, other: &Vec<u8>) -> bool {
        *self == &other[..]
    }
}

impl PartialOrd<Vec<u8>> for BytesMut {
    fn partial_cmp(&self, other: &Vec<u8>) -> Option<cmp::Ordering> {
        (**self).partial_cmp(&other[..])
    }
}

impl PartialEq<BytesMut> for Vec<u8> {
    fn eq(&self, other: &BytesMut) -> bool {
        *other == *self
    }
}

impl PartialOrd<BytesMut> for Vec<u8> {
    fn partial_cmp(&self, other: &BytesMut) -> Option<cmp::Ordering> {
        other.partial_cmp(self)
    }
}

impl PartialEq<String> for BytesMut {
    fn eq(&self, other: &String) -> bool {
        *self == &other[..]
    }
}

impl PartialOrd<String> for BytesMut {
    fn partial_cmp(&self, other: &String) -> Option<cmp::Ordering> {
        (**self).partial_cmp(other.as_bytes())
    }
}

impl PartialEq<BytesMut> for String {
    fn eq(&self, other: &BytesMut) -> bool {
        *other == *self
    }
}

impl PartialOrd<BytesMut> for String {
    fn partial_cmp(&self, other: &BytesMut) -> Option<cmp::Ordering> {
        <[u8] as PartialOrd<[u8]>>::partial_cmp(self.as_bytes(), other)
    }
}

impl<'a, T: ?Sized> PartialEq<&'a T> for BytesMut
where
    BytesMut: PartialEq<T>,
{
    fn eq(&self, other: &&'a T) -> bool {
        *self == **other
    }
}

impl<'a, T: ?Sized> PartialOrd<&'a T> for BytesMut
where
    BytesMut: PartialOrd<T>,
{
    fn partial_cmp(&self, other: &&'a T) -> Option<cmp::Ordering> {
        self.partial_cmp(*other)
    }
}

impl PartialEq<BytesMut> for &[u8] {
    fn eq(&self, other: &BytesMut) -> bool {
        *other == *self
    }
}

impl PartialOrd<BytesMut> for &[u8] {
    fn partial_cmp(&self, other: &BytesMut) -> Option<cmp::Ordering> {
        <[u8] as PartialOrd<[u8]>>::partial_cmp(self, other)
    }
}

impl PartialEq<BytesMut> for &str {
    fn eq(&self, other: &BytesMut) -> bool {
        *other == *self
    }
}

impl PartialOrd<BytesMut> for &str {
    fn partial_cmp(&self, other: &BytesMut) -> Option<cmp::Ordering> {
        other.partial_cmp(self)
    }
}

impl PartialEq<BytesMut> for Bytes {
    fn eq(&self, other: &BytesMut) -> bool {
        &other[..] == &self[..]
    }
}

impl PartialEq<Bytes> for BytesMut {
    fn eq(&self, other: &Bytes) -> bool {
        &other[..] == &self[..]
    }
}

fn vptr(ptr: *mut u8) -> NonNull<u8> {
    if cfg!(debug_assertions) {
        NonNull::new(ptr).expect("Vec pointer should be non-null")
    } else {
        unsafe { NonNull::new_unchecked(ptr) }
    }
}

unsafe fn rebuild_vec(ptr: *mut u8, mut len: usize, mut cap: usize, off: usize) -> Vec<u8> {
    let ptr = ptr.offset(-(off as isize));
    len += off;
    cap += off;

    Vec::from_raw_parts(ptr, len, cap)
}

// ===== impl SharedVtable =====

static SHARED_VTABLE: Vtable = Vtable {
    clone: shared_v_clone,
    drop: shared_v_drop,
};

unsafe fn shared_v_clone(data: &AtomicPtr<()>, ptr: *const u8, len: usize) -> Bytes {
    let shared = data.load(Ordering::Relaxed) as *mut Shared;
    increment_shared(shared);

    let data = AtomicPtr::new(shared as _);
    Bytes::with_vtable(ptr, len, data, &SHARED_VTABLE)
}

unsafe fn shared_v_drop(data: &mut AtomicPtr<()>, _ptr: *const u8, _len: usize) {
    data.with_mut(|shared| {
        release_shared(*shared as *mut Shared);
    });
}

// compile-fails

/// ```compile_fail
/// use bytes::BytesMut;
/// #[deny(unused_must_use)]
/// {
///     let mut b1 = BytesMut::from("hello world");
///     b1.split_to(6);
/// }
/// ```
fn _split_to_must_use() {}

/// ```compile_fail
/// use bytes::BytesMut;
/// #[deny(unused_must_use)]
/// {
///     let mut b1 = BytesMut::from("hello world");
///     b1.split_off(6);
/// }
/// ```
fn _split_off_must_use() {}

/// ```compile_fail
/// use bytes::BytesMut;
/// #[deny(unused_must_use)]
/// {
///     let mut b1 = BytesMut::from("hello world");
///     b1.split();
/// }
/// ```
fn _split_must_use() {}

// fuzz tests
#[cfg(all(test, loom))]
mod fuzz {
    use loom::sync::Arc;
    use loom::thread;

    use super::BytesMut;
    use crate::Bytes;

    #[test]
    fn bytes_mut_cloning_frozen() {
        loom::model(|| {
            let a = BytesMut::from(&b"abcdefgh"[..]).split().freeze();
            let addr = a.as_ptr() as usize;

            // test the Bytes::clone is Sync by putting it in an Arc
            let a1 = Arc::new(a);
            let a2 = a1.clone();

            let t1 = thread::spawn(move || {
                let b: Bytes = (*a1).clone();
                assert_eq!(b.as_ptr() as usize, addr);
            });

            let t2 = thread::spawn(move || {
                let b: Bytes = (*a2).clone();
                assert_eq!(b.as_ptr() as usize, addr);
            });

            t1.join().unwrap();
            t2.join().unwrap();
        });
    }
}
