use crate::alloc::alloc::{alloc, dealloc, handle_alloc_error};
use crate::scopeguard::guard;
use crate::TryReserveError;
use core::alloc::Layout;
use core::hint;
use core::iter::FusedIterator;
use core::marker::PhantomData;
use core::mem;
use core::mem::ManuallyDrop;
use core::ptr::NonNull;

cfg_if! {
    // Use the SSE2 implementation if possible: it allows us to scan 16 buckets
    // at once instead of 8. We don't bother with AVX since it would require
    // runtime dispatch and wouldn't gain us much anyways: the probability of
    // finding a match drops off drastically after the first few buckets.
    //
    // I attempted an implementation on ARM using NEON instructions, but it
    // turns out that most NEON instructions have multi-cycle latency, which in
    // the end outweighs any gains over the generic implementation.
    if #[cfg(all(
        target_feature = "sse2",
        any(target_arch = "x86", target_arch = "x86_64"),
        not(miri)
    ))] {
        mod sse2;
        use sse2 as imp;
    } else {
        #[path = "generic.rs"]
        mod generic;
        use generic as imp;
    }
}

mod bitmask;

use self::bitmask::{BitMask, BitMaskIter};
use self::imp::Group;

// Branch prediction hint. This is currently only available on nightly but it
// consistently improves performance by 10-15%.
#[cfg(feature = "nightly")]
use core::intrinsics::{likely, unlikely};
#[cfg(not(feature = "nightly"))]
#[inline]
fn likely(b: bool) -> bool {
    b
}
#[cfg(not(feature = "nightly"))]
#[inline]
fn unlikely(b: bool) -> bool {
    b
}

#[cfg(feature = "nightly")]
#[cfg_attr(feature = "inline-more", inline)]
unsafe fn offset_from<T>(to: *const T, from: *const T) -> usize {
    to.offset_from(from) as usize
}
#[cfg(not(feature = "nightly"))]
#[cfg_attr(feature = "inline-more", inline)]
unsafe fn offset_from<T>(to: *const T, from: *const T) -> usize {
    (to as usize - from as usize) / mem::size_of::<T>()
}

/// Whether memory allocation errors should return an error or abort.
#[derive(Copy, Clone)]
enum Fallibility {
    Fallible,
    Infallible,
}

impl Fallibility {
    /// Error to return on capacity overflow.
    #[cfg_attr(feature = "inline-more", inline)]
    fn capacity_overflow(self) -> TryReserveError {
        match self {
            Fallibility::Fallible => TryReserveError::CapacityOverflow,
            Fallibility::Infallible => panic!("Hash table capacity overflow"),
        }
    }

    /// Error to return on allocation error.
    #[cfg_attr(feature = "inline-more", inline)]
    fn alloc_err(self, layout: Layout) -> TryReserveError {
        match self {
            Fallibility::Fallible => TryReserveError::AllocError { layout },
            Fallibility::Infallible => handle_alloc_error(layout),
        }
    }
}

/// Control byte value for an empty bucket.
const EMPTY: u8 = 0b1111_1111;

/// Control byte value for a deleted bucket.
const DELETED: u8 = 0b1000_0000;

/// Checks whether a control byte represents a full bucket (top bit is clear).
#[inline]
fn is_full(ctrl: u8) -> bool {
    ctrl & 0x80 == 0
}

/// Checks whether a control byte represents a special value (top bit is set).
#[inline]
fn is_special(ctrl: u8) -> bool {
    ctrl & 0x80 != 0
}

/// Checks whether a special control value is EMPTY (just check 1 bit).
#[inline]
fn special_is_empty(ctrl: u8) -> bool {
    debug_assert!(is_special(ctrl));
    ctrl & 0x01 != 0
}

/// Primary hash function, used to select the initial bucket to probe from.
#[inline]
#[allow(clippy::cast_possible_truncation)]
fn h1(hash: u64) -> usize {
    // On 32-bit platforms we simply ignore the higher hash bits.
    hash as usize
}

/// Secondary hash function, saved in the low 7 bits of the control byte.
#[inline]
#[allow(clippy::cast_possible_truncation)]
fn h2(hash: u64) -> u8 {
    // Grab the top 7 bits of the hash. While the hash is normally a full 64-bit
    // value, some hash functions (such as FxHash) produce a usize result
    // instead, which means that the top 32 bits are 0 on 32-bit platforms.
    let hash_len = usize::min(mem::size_of::<usize>(), mem::size_of::<u64>());
    let top7 = hash >> (hash_len * 8 - 7);
    (top7 & 0x7f) as u8 // truncation
}

/// Probe sequence based on triangular numbers, which is guaranteed (since our
/// table size is a power of two) to visit every group of elements exactly once.
///
/// A triangular probe has us jump by 1 more group every time. So first we
/// jump by 1 group (meaning we just continue our linear scan), then 2 groups
/// (skipping over 1 group), then 3 groups (skipping over 2 groups), and so on.
///
/// Proof that the probe will visit every group in the table:
/// <https://fgiesen.wordpress.com/2015/02/22/triangular-numbers-mod-2n/>
struct ProbeSeq {
    bucket_mask: usize,
    pos: usize,
    stride: usize,
}

impl Iterator for ProbeSeq {
    type Item = usize;

    #[inline]
    fn next(&mut self) -> Option<usize> {
        // We should have found an empty bucket by now and ended the probe.
        debug_assert!(
            self.stride <= self.bucket_mask,
            "Went past end of probe sequence"
        );

        let result = self.pos;
        self.stride += Group::WIDTH;
        self.pos += self.stride;
        self.pos &= self.bucket_mask;
        Some(result)
    }
}

/// Returns the number of buckets needed to hold the given number of items,
/// taking the maximum load factor into account.
///
/// Returns `None` if an overflow occurs.
// Workaround for emscripten bug emscripten-core/emscripten-fastcomp#258
#[cfg_attr(target_os = "emscripten", inline(never))]
#[cfg_attr(not(target_os = "emscripten"), inline)]
fn capacity_to_buckets(cap: usize) -> Option<usize> {
    debug_assert_ne!(cap, 0);

    // For small tables we require at least 1 empty bucket so that lookups are
    // guaranteed to terminate if an element doesn't exist in the table.
    if cap < 8 {
        // We don't bother with a table size of 2 buckets since that can only
        // hold a single element. Instead we skip directly to a 4 bucket table
        // which can hold 3 elements.
        return Some(if cap < 4 { 4 } else { 8 });
    }

    // Otherwise require 1/8 buckets to be empty (87.5% load)
    //
    // Be careful when modifying this, calculate_layout relies on the
    // overflow check here.
    let adjusted_cap = cap.checked_mul(8)? / 7;

    // Any overflows will have been caught by the checked_mul. Also, any
    // rounding errors from the division above will be cleaned up by
    // next_power_of_two (which can't overflow because of the previous divison).
    Some(adjusted_cap.next_power_of_two())
}

/// Returns the maximum effective capacity for the given bucket mask, taking
/// the maximum load factor into account.
#[inline]
fn bucket_mask_to_capacity(bucket_mask: usize) -> usize {
    if bucket_mask < 8 {
        // For tables with 1/2/4/8 buckets, we always reserve one empty slot.
        // Keep in mind that the bucket mask is one less than the bucket count.
        bucket_mask
    } else {
        // For larger tables we reserve 12.5% of the slots as empty.
        ((bucket_mask + 1) / 8) * 7
    }
}

/// Returns a Layout which describes the allocation required for a hash table,
/// and the offset of the control bytes in the allocation.
/// (the offset is also one past last element of buckets)
///
/// Returns `None` if an overflow occurs.
#[cfg_attr(feature = "inline-more", inline)]
#[cfg(feature = "nightly")]
fn calculate_layout<T>(buckets: usize) -> Option<(Layout, usize)> {
    debug_assert!(buckets.is_power_of_two());

    // Array of buckets
    let data = Layout::array::<T>(buckets).ok()?;

    // Array of control bytes. This must be aligned to the group size.
    //
    // We add `Group::WIDTH` control bytes at the end of the array which
    // replicate the bytes at the start of the array and thus avoids the need to
    // perform bounds-checking while probing.
    //
    // There is no possible overflow here since buckets is a power of two and
    // Group::WIDTH is a small number.
    let ctrl = unsafe { Layout::from_size_align_unchecked(buckets + Group::WIDTH, Group::WIDTH) };

    data.extend(ctrl).ok()
}

/// Returns a Layout which describes the allocation required for a hash table,
/// and the offset of the control bytes in the allocation.
/// (the offset is also one past last element of buckets)
///
/// Returns `None` if an overflow occurs.
#[cfg_attr(feature = "inline-more", inline)]
#[cfg(not(feature = "nightly"))]
fn calculate_layout<T>(buckets: usize) -> Option<(Layout, usize)> {
    debug_assert!(buckets.is_power_of_two());

    // Manual layout calculation since Layout methods are not yet stable.
    let ctrl_align = usize::max(mem::align_of::<T>(), Group::WIDTH);
    let ctrl_offset = mem::size_of::<T>()
        .checked_mul(buckets)?
        .checked_add(ctrl_align - 1)?
        & !(ctrl_align - 1);
    let len = ctrl_offset.checked_add(buckets + Group::WIDTH)?;

    Some((
        unsafe { Layout::from_size_align_unchecked(len, ctrl_align) },
        ctrl_offset,
    ))
}

/// A reference to a hash table bucket containing a `T`.
///
/// This is usually just a pointer to the element itself. However if the element
/// is a ZST, then we instead track the index of the element in the table so
/// that `erase` works properly.
pub struct Bucket<T> {
    // Actually it is pointer to next element than element itself
    // this is needed to maintain pointer arithmetic invariants
    // keeping direct pointer to element introduces difficulty.
    // Using `NonNull` for variance and niche layout
    ptr: NonNull<T>,
}

// This Send impl is needed for rayon support. This is safe since Bucket is
// never exposed in a public API.
unsafe impl<T> Send for Bucket<T> {}

impl<T> Clone for Bucket<T> {
    #[cfg_attr(feature = "inline-more", inline)]
    fn clone(&self) -> Self {
        Self { ptr: self.ptr }
    }
}

impl<T> Bucket<T> {
    #[cfg_attr(feature = "inline-more", inline)]
    unsafe fn from_base_index(base: NonNull<T>, index: usize) -> Self {
        let ptr = if mem::size_of::<T>() == 0 {
            // won't overflow because index must be less than length
            (index + 1) as *mut T
        } else {
            base.as_ptr().sub(index)
        };
        Self {
            ptr: NonNull::new_unchecked(ptr),
        }
    }
    #[cfg_attr(feature = "inline-more", inline)]
    unsafe fn to_base_index(&self, base: NonNull<T>) -> usize {
        if mem::size_of::<T>() == 0 {
            self.ptr.as_ptr() as usize - 1
        } else {
            offset_from(base.as_ptr(), self.ptr.as_ptr())
        }
    }
    #[cfg_attr(feature = "inline-more", inline)]
    pub unsafe fn as_ptr(&self) -> *mut T {
        if mem::size_of::<T>() == 0 {
            // Just return an arbitrary ZST pointer which is properly aligned
            mem::align_of::<T>() as *mut T
        } else {
            self.ptr.as_ptr().sub(1)
        }
    }
    #[cfg_attr(feature = "inline-more", inline)]
    unsafe fn next_n(&self, offset: usize) -> Self {
        let ptr = if mem::size_of::<T>() == 0 {
            (self.ptr.as_ptr() as usize + offset) as *mut T
        } else {
            self.ptr.as_ptr().sub(offset)
        };
        Self {
            ptr: NonNull::new_unchecked(ptr),
        }
    }
    #[cfg_attr(feature = "inline-more", inline)]
    pub unsafe fn drop(&self) {
        self.as_ptr().drop_in_place();
    }
    #[cfg_attr(feature = "inline-more", inline)]
    pub unsafe fn read(&self) -> T {
        self.as_ptr().read()
    }
    #[cfg_attr(feature = "inline-more", inline)]
    pub unsafe fn write(&self, val: T) {
        self.as_ptr().write(val);
    }
    #[cfg_attr(feature = "inline-more", inline)]
    pub unsafe fn as_ref<'a>(&self) -> &'a T {
        &*self.as_ptr()
    }
    #[cfg_attr(feature = "inline-more", inline)]
    pub unsafe fn as_mut<'a>(&self) -> &'a mut T {
        &mut *self.as_ptr()
    }
    #[cfg_attr(feature = "inline-more", inline)]
    pub unsafe fn copy_from_nonoverlapping(&self, other: &Self) {
        self.as_ptr().copy_from_nonoverlapping(other.as_ptr(), 1);
    }
}

/// A raw hash table with an unsafe API.
pub struct RawTable<T> {
    // Mask to get an index from a hash value. The value is one less than the
    // number of buckets in the table.
    bucket_mask: usize,

    // [Padding], T1, T2, ..., Tlast, C1, C2, ...
    //                                ^ points here
    ctrl: NonNull<u8>,

    // Number of elements that can be inserted before we need to grow the table
    growth_left: usize,

    // Number of elements in the table, only really used by len()
    items: usize,

    // Tell dropck that we own instances of T.
    marker: PhantomData<T>,
}

impl<T> RawTable<T> {
    /// Creates a new empty hash table without allocating any memory.
    ///
    /// In effect this returns a table with exactly 1 bucket. However we can
    /// leave the data pointer dangling since that bucket is never written to
    /// due to our load factor forcing us to always have at least 1 free bucket.
    #[cfg_attr(feature = "inline-more", inline)]
    pub const fn new() -> Self {
        Self {
            // Be careful to cast the entire slice to a raw pointer.
            ctrl: unsafe { NonNull::new_unchecked(Group::static_empty() as *const _ as *mut u8) },
            bucket_mask: 0,
            items: 0,
            growth_left: 0,
            marker: PhantomData,
        }
    }

    /// Allocates a new hash table with the given number of buckets.
    ///
    /// The control bytes are left uninitialized.
    #[cfg_attr(feature = "inline-more", inline)]
    unsafe fn new_uninitialized(
        buckets: usize,
        fallability: Fallibility,
    ) -> Result<Self, TryReserveError> {
        debug_assert!(buckets.is_power_of_two());

        // Avoid `Option::ok_or_else` because it bloats LLVM IR.
        let (layout, ctrl_offset) = match calculate_layout::<T>(buckets) {
            Some(lco) => lco,
            None => return Err(fallability.capacity_overflow()),
        };
        let ptr = match NonNull::new(alloc(layout)) {
            Some(ptr) => ptr,
            None => return Err(fallability.alloc_err(layout)),
        };
        let ctrl = NonNull::new_unchecked(ptr.as_ptr().add(ctrl_offset));
        Ok(Self {
            ctrl,
            bucket_mask: buckets - 1,
            items: 0,
            growth_left: bucket_mask_to_capacity(buckets - 1),
            marker: PhantomData,
        })
    }

    /// Attempts to allocate a new hash table with at least enough capacity
    /// for inserting the given number of elements without reallocating.
    fn fallible_with_capacity(
        capacity: usize,
        fallability: Fallibility,
    ) -> Result<Self, TryReserveError> {
        if capacity == 0 {
            Ok(Self::new())
        } else {
            unsafe {
                // Avoid `Option::ok_or_else` because it bloats LLVM IR.
                let buckets = match capacity_to_buckets(capacity) {
                    Some(buckets) => buckets,
                    None => return Err(fallability.capacity_overflow()),
                };
                let result = Self::new_uninitialized(buckets, fallability)?;
                result.ctrl(0).write_bytes(EMPTY, result.num_ctrl_bytes());

                Ok(result)
            }
        }
    }

    /// Attempts to allocate a new hash table with at least enough capacity
    /// for inserting the given number of elements without reallocating.
    #[cfg(feature = "raw")]
    pub fn try_with_capacity(capacity: usize) -> Result<Self, TryReserveError> {
        Self::fallible_with_capacity(capacity, Fallibility::Fallible)
    }

    /// Allocates a new hash table with at least enough capacity for inserting
    /// the given number of elements without reallocating.
    pub fn with_capacity(capacity: usize) -> Self {
        // Avoid `Result::unwrap_or_else` because it bloats LLVM IR.
        match Self::fallible_with_capacity(capacity, Fallibility::Infallible) {
            Ok(capacity) => capacity,
            Err(_) => unsafe { hint::unreachable_unchecked() },
        }
    }

    /// Deallocates the table without dropping any entries.
    #[cfg_attr(feature = "inline-more", inline)]
    unsafe fn free_buckets(&mut self) {
        // Avoid `Option::unwrap_or_else` because it bloats LLVM IR.
        let (layout, ctrl_offset) = match calculate_layout::<T>(self.buckets()) {
            Some(lco) => lco,
            None => hint::unreachable_unchecked(),
        };
        dealloc(self.ctrl.as_ptr().sub(ctrl_offset), layout);
    }

    /// Returns pointer to one past last element of data table.
    #[cfg_attr(feature = "inline-more", inline)]
    pub unsafe fn data_end(&self) -> NonNull<T> {
        NonNull::new_unchecked(self.ctrl.as_ptr() as *mut T)
    }

    /// Returns pointer to start of data table.
    #[cfg_attr(feature = "inline-more", inline)]
    #[cfg(feature = "nightly")]
    pub unsafe fn data_start(&self) -> *mut T {
        self.data_end().as_ptr().wrapping_sub(self.buckets())
    }

    /// Returns the index of a bucket from a `Bucket`.
    #[cfg_attr(feature = "inline-more", inline)]
    pub unsafe fn bucket_index(&self, bucket: &Bucket<T>) -> usize {
        bucket.to_base_index(self.data_end())
    }

    /// Returns a pointer to a control byte.
    #[cfg_attr(feature = "inline-more", inline)]
    unsafe fn ctrl(&self, index: usize) -> *mut u8 {
        debug_assert!(index < self.num_ctrl_bytes());
        self.ctrl.as_ptr().add(index)
    }

    /// Returns a pointer to an element in the table.
    #[cfg_attr(feature = "inline-more", inline)]
    pub unsafe fn bucket(&self, index: usize) -> Bucket<T> {
        debug_assert_ne!(self.bucket_mask, 0);
        debug_assert!(index < self.buckets());
        Bucket::from_base_index(self.data_end(), index)
    }

    /// Erases an element from the table without dropping it.
    #[cfg_attr(feature = "inline-more", inline)]
    #[deprecated(since = "0.8.1", note = "use erase or remove instead")]
    pub unsafe fn erase_no_drop(&mut self, item: &Bucket<T>) {
        let index = self.bucket_index(item);
        debug_assert!(is_full(*self.ctrl(index)));
        let index_before = index.wrapping_sub(Group::WIDTH) & self.bucket_mask;
        let empty_before = Group::load(self.ctrl(index_before)).match_empty();
        let empty_after = Group::load(self.ctrl(index)).match_empty();

        // If we are inside a continuous block of Group::WIDTH full or deleted
        // cells then a probe window may have seen a full block when trying to
        // insert. We therefore need to keep that block non-empty so that
        // lookups will continue searching to the next probe window.
        //
        // Note that in this context `leading_zeros` refers to the bytes at the
        // end of a group, while `trailing_zeros` refers to the bytes at the
        // begining of a group.
        let ctrl = if empty_before.leading_zeros() + empty_after.trailing_zeros() >= Group::WIDTH {
            DELETED
        } else {
            self.growth_left += 1;
            EMPTY
        };
        self.set_ctrl(index, ctrl);
        self.items -= 1;
    }

    /// Erases an element from the table, dropping it in place.
    #[cfg_attr(feature = "inline-more", inline)]
    #[allow(clippy::needless_pass_by_value)]
    #[allow(deprecated)]
    pub unsafe fn erase(&mut self, item: Bucket<T>) {
        // Erase the element from the table first since drop might panic.
        self.erase_no_drop(&item);
        item.drop();
    }

    /// Finds and erases an element from the table, dropping it in place.
    /// Returns true if an element was found.
    #[cfg(feature = "raw")]
    #[cfg_attr(feature = "inline-more", inline)]
    pub fn erase_entry(&mut self, hash: u64, eq: impl FnMut(&T) -> bool) -> bool {
        // Avoid `Option::map` because it bloats LLVM IR.
        if let Some(bucket) = self.find(hash, eq) {
            unsafe { self.erase(bucket) };
            true
        } else {
            false
        }
    }

    /// Removes an element from the table, returning it.
    #[cfg_attr(feature = "inline-more", inline)]
    #[allow(clippy::needless_pass_by_value)]
    #[allow(deprecated)]
    pub unsafe fn remove(&mut self, item: Bucket<T>) -> T {
        self.erase_no_drop(&item);
        item.read()
    }

    /// Finds and removes an element from the table, returning it.
    #[cfg_attr(feature = "inline-more", inline)]
    pub fn remove_entry(&mut self, hash: u64, eq: impl FnMut(&T) -> bool) -> Option<T> {
        // Avoid `Option::map` because it bloats LLVM IR.
        match self.find(hash, eq) {
            Some(bucket) => Some(unsafe { self.remove(bucket) }),
            None => None,
        }
    }

    /// Returns an iterator for a probe sequence on the table.
    ///
    /// This iterator never terminates, but is guaranteed to visit each bucket
    /// group exactly once. The loop using `probe_seq` must terminate upon
    /// reaching a group containing an empty bucket.
    #[cfg_attr(feature = "inline-more", inline)]
    fn probe_seq(&self, hash: u64) -> ProbeSeq {
        ProbeSeq {
            bucket_mask: self.bucket_mask,
            pos: h1(hash) & self.bucket_mask,
            stride: 0,
        }
    }

    /// Sets a control byte, and possibly also the replicated control byte at
    /// the end of the array.
    #[cfg_attr(feature = "inline-more", inline)]
    unsafe fn set_ctrl(&self, index: usize, ctrl: u8) {
        // Replicate the first Group::WIDTH control bytes at the end of
        // the array without using a branch:
        // - If index >= Group::WIDTH then index == index2.
        // - Otherwise index2 == self.bucket_mask + 1 + index.
        //
        // The very last replicated control byte is never actually read because
        // we mask the initial index for unaligned loads, but we write it
        // anyways because it makes the set_ctrl implementation simpler.
        //
        // If there are fewer buckets than Group::WIDTH then this code will
        // replicate the buckets at the end of the trailing group. For example
        // with 2 buckets and a group size of 4, the control bytes will look
        // like this:
        //
        //     Real    |             Replicated
        // ---------------------------------------------
        // | [A] | [B] | [EMPTY] | [EMPTY] | [A] | [B] |
        // ---------------------------------------------
        let index2 = ((index.wrapping_sub(Group::WIDTH)) & self.bucket_mask) + Group::WIDTH;

        *self.ctrl(index) = ctrl;
        *self.ctrl(index2) = ctrl;
    }

    /// Searches for an empty or deleted bucket which is suitable for inserting
    /// a new element.
    ///
    /// There must be at least 1 empty bucket in the table.
    #[cfg_attr(feature = "inline-more", inline)]
    fn find_insert_slot(&self, hash: u64) -> usize {
        for pos in self.probe_seq(hash) {
            unsafe {
                let group = Group::load(self.ctrl(pos));
                if let Some(bit) = group.match_empty_or_deleted().lowest_set_bit() {
                    let result = (pos + bit) & self.bucket_mask;

                    // In tables smaller than the group width, trailing control
                    // bytes outside the range of the table are filled with
                    // EMPTY entries. These will unfortunately trigger a
                    // match, but once masked may point to a full bucket that
                    // is already occupied. We detect this situation here and
                    // perform a second scan starting at the begining of the
                    // table. This second scan is guaranteed to find an empty
                    // slot (due to the load factor) before hitting the trailing
                    // control bytes (containing EMPTY).
                    if unlikely(is_full(*self.ctrl(result))) {
                        debug_assert!(self.bucket_mask < Group::WIDTH);
                        debug_assert_ne!(pos, 0);
                        return Group::load_aligned(self.ctrl(0))
                            .match_empty_or_deleted()
                            .lowest_set_bit_nonzero();
                    } else {
                        return result;
                    }
                }
            }
        }

        // probe_seq never returns.
        unreachable!();
    }

    /// Marks all table buckets as empty without dropping their contents.
    #[cfg_attr(feature = "inline-more", inline)]
    pub fn clear_no_drop(&mut self) {
        if !self.is_empty_singleton() {
            unsafe {
                self.ctrl(0).write_bytes(EMPTY, self.num_ctrl_bytes());
            }
        }
        self.items = 0;
        self.growth_left = bucket_mask_to_capacity(self.bucket_mask);
    }

    /// Removes all elements from the table without freeing the backing memory.
    #[cfg_attr(feature = "inline-more", inline)]
    pub fn clear(&mut self) {
        // Ensure that the table is reset even if one of the drops panic
        let self_ = guard(self, |self_| self_.clear_no_drop());

        if mem::needs_drop::<T>() && self_.len() != 0 {
            unsafe {
                for item in self_.iter() {
                    item.drop();
                }
            }
        }
    }

    /// Shrinks the table to fit `max(self.len(), min_size)` elements.
    #[cfg_attr(feature = "inline-more", inline)]
    pub fn shrink_to(&mut self, min_size: usize, hasher: impl Fn(&T) -> u64) {
        // Calculate the minimal number of elements that we need to reserve
        // space for.
        let min_size = usize::max(self.items, min_size);
        if min_size == 0 {
            *self = Self::new();
            return;
        }

        // Calculate the number of buckets that we need for this number of
        // elements. If the calculation overflows then the requested bucket
        // count must be larger than what we have right and nothing needs to be
        // done.
        let min_buckets = match capacity_to_buckets(min_size) {
            Some(buckets) => buckets,
            None => return,
        };

        // If we have more buckets than we need, shrink the table.
        if min_buckets < self.buckets() {
            // Fast path if the table is empty
            if self.items == 0 {
                *self = Self::with_capacity(min_size)
            } else {
                // Avoid `Result::unwrap_or_else` because it bloats LLVM IR.
                if self
                    .resize(min_size, hasher, Fallibility::Infallible)
                    .is_err()
                {
                    unsafe { hint::unreachable_unchecked() }
                }
            }
        }
    }

    /// Ensures that at least `additional` items can be inserted into the table
    /// without reallocation.
    #[cfg_attr(feature = "inline-more", inline)]
    pub fn reserve(&mut self, additional: usize, hasher: impl Fn(&T) -> u64) {
        if additional > self.growth_left {
            // Avoid `Result::unwrap_or_else` because it bloats LLVM IR.
            if self
                .reserve_rehash(additional, hasher, Fallibility::Infallible)
                .is_err()
            {
                unsafe { hint::unreachable_unchecked() }
            }
        }
    }

    /// Tries to ensure that at least `additional` items can be inserted into
    /// the table without reallocation.
    #[cfg_attr(feature = "inline-more", inline)]
    pub fn try_reserve(
        &mut self,
        additional: usize,
        hasher: impl Fn(&T) -> u64,
    ) -> Result<(), TryReserveError> {
        if additional > self.growth_left {
            self.reserve_rehash(additional, hasher, Fallibility::Fallible)
        } else {
            Ok(())
        }
    }

    /// Out-of-line slow path for `reserve` and `try_reserve`.
    #[cold]
    #[inline(never)]
    fn reserve_rehash(
        &mut self,
        additional: usize,
        hasher: impl Fn(&T) -> u64,
        fallability: Fallibility,
    ) -> Result<(), TryReserveError> {
        // Avoid `Option::ok_or_else` because it bloats LLVM IR.
        let new_items = match self.items.checked_add(additional) {
            Some(new_items) => new_items,
            None => return Err(fallability.capacity_overflow()),
        };
        let full_capacity = bucket_mask_to_capacity(self.bucket_mask);
        if new_items <= full_capacity / 2 {
            // Rehash in-place without re-allocating if we have plenty of spare
            // capacity that is locked up due to DELETED entries.
            self.rehash_in_place(hasher);
            Ok(())
        } else {
            // Otherwise, conservatively resize to at least the next size up
            // to avoid churning deletes into frequent rehashes.
            self.resize(
                usize::max(new_items, full_capacity + 1),
                hasher,
                fallability,
            )
        }
    }

    /// Rehashes the contents of the table in place (i.e. without changing the
    /// allocation).
    ///
    /// If `hasher` panics then some the table's contents may be lost.
    fn rehash_in_place(&mut self, hasher: impl Fn(&T) -> u64) {
        unsafe {
            // Bulk convert all full control bytes to DELETED, and all DELETED
            // control bytes to EMPTY. This effectively frees up all buckets
            // containing a DELETED entry.
            for i in (0..self.buckets()).step_by(Group::WIDTH) {
                let group = Group::load_aligned(self.ctrl(i));
                let group = group.convert_special_to_empty_and_full_to_deleted();
                group.store_aligned(self.ctrl(i));
            }

            // Fix up the trailing control bytes. See the comments in set_ctrl
            // for the handling of tables smaller than the group width.
            if self.buckets() < Group::WIDTH {
                self.ctrl(0)
                    .copy_to(self.ctrl(Group::WIDTH), self.buckets());
            } else {
                self.ctrl(0)
                    .copy_to(self.ctrl(self.buckets()), Group::WIDTH);
            }

            // If the hash function panics then properly clean up any elements
            // that we haven't rehashed yet. We unfortunately can't preserve the
            // element since we lost their hash and have no way of recovering it
            // without risking another panic.
            let mut guard = guard(self, |self_| {
                if mem::needs_drop::<T>() {
                    for i in 0..self_.buckets() {
                        if *self_.ctrl(i) == DELETED {
                            self_.set_ctrl(i, EMPTY);
                            self_.bucket(i).drop();
                            self_.items -= 1;
                        }
                    }
                }
                self_.growth_left = bucket_mask_to_capacity(self_.bucket_mask) - self_.items;
            });

            // At this point, DELETED elements are elements that we haven't
            // rehashed yet. Find them and re-insert them at their ideal
            // position.
            'outer: for i in 0..guard.buckets() {
                if *guard.ctrl(i) != DELETED {
                    continue;
                }
                'inner: loop {
                    // Hash the current item
                    let item = guard.bucket(i);
                    let hash = hasher(item.as_ref());

                    // Search for a suitable place to put it
                    let new_i = guard.find_insert_slot(hash);

                    // Probing works by scanning through all of the control
                    // bytes in groups, which may not be aligned to the group
                    // size. If both the new and old position fall within the
                    // same unaligned group, then there is no benefit in moving
                    // it and we can just continue to the next item.
                    let probe_index = |pos: usize| {
                        (pos.wrapping_sub(guard.probe_seq(hash).pos) & guard.bucket_mask)
                            / Group::WIDTH
                    };
                    if likely(probe_index(i) == probe_index(new_i)) {
                        guard.set_ctrl(i, h2(hash));
                        continue 'outer;
                    }

                    // We are moving the current item to a new position. Write
                    // our H2 to the control byte of the new position.
                    let prev_ctrl = *guard.ctrl(new_i);
                    guard.set_ctrl(new_i, h2(hash));

                    if prev_ctrl == EMPTY {
                        // If the target slot is empty, simply move the current
                        // element into the new slot and clear the old control
                        // byte.
                        guard.set_ctrl(i, EMPTY);
                        guard.bucket(new_i).copy_from_nonoverlapping(&item);
                        continue 'outer;
                    } else {
                        // If the target slot is occupied, swap the two elements
                        // and then continue processing the element that we just
                        // swapped into the old slot.
                        debug_assert_eq!(prev_ctrl, DELETED);
                        mem::swap(guard.bucket(new_i).as_mut(), item.as_mut());
                        continue 'inner;
                    }
                }
            }

            guard.growth_left = bucket_mask_to_capacity(guard.bucket_mask) - guard.items;
            mem::forget(guard);
        }
    }

    /// Allocates a new table of a different size and moves the contents of the
    /// current table into it.
    fn resize(
        &mut self,
        capacity: usize,
        hasher: impl Fn(&T) -> u64,
        fallability: Fallibility,
    ) -> Result<(), TryReserveError> {
        unsafe {
            debug_assert!(self.items <= capacity);

            // Allocate and initialize the new table.
            let mut new_table = Self::fallible_with_capacity(capacity, fallability)?;
            new_table.growth_left -= self.items;
            new_table.items = self.items;

            // The hash function may panic, in which case we simply free the new
            // table without dropping any elements that may have been copied into
            // it.
            //
            // This guard is also used to free the old table on success, see
            // the comment at the bottom of this function.
            let mut new_table = guard(ManuallyDrop::new(new_table), |new_table| {
                if !new_table.is_empty_singleton() {
                    new_table.free_buckets();
                }
            });

            // Copy all elements to the new table.
            for item in self.iter() {
                // This may panic.
                let hash = hasher(item.as_ref());

                // We can use a simpler version of insert() here since:
                // - there are no DELETED entries.
                // - we know there is enough space in the table.
                // - all elements are unique.
                let index = new_table.find_insert_slot(hash);
                new_table.set_ctrl(index, h2(hash));
                new_table.bucket(index).copy_from_nonoverlapping(&item);
            }

            // We successfully copied all elements without panicking. Now replace
            // self with the new table. The old table will have its memory freed but
            // the items will not be dropped (since they have been moved into the
            // new table).
            mem::swap(self, &mut new_table);

            Ok(())
        }
    }

    /// Inserts a new element into the table, and returns its raw bucket.
    ///
    /// This does not check if the given element already exists in the table.
    #[cfg_attr(feature = "inline-more", inline)]
    pub fn insert(&mut self, hash: u64, value: T, hasher: impl Fn(&T) -> u64) -> Bucket<T> {
        unsafe {
            let mut index = self.find_insert_slot(hash);

            // We can avoid growing the table once we have reached our load
            // factor if we are replacing a tombstone. This works since the
            // number of EMPTY slots does not change in this case.
            let old_ctrl = *self.ctrl(index);
            if unlikely(self.growth_left == 0 && special_is_empty(old_ctrl)) {
                self.reserve(1, hasher);
                index = self.find_insert_slot(hash);
            }

            let bucket = self.bucket(index);
            self.growth_left -= special_is_empty(old_ctrl) as usize;
            self.set_ctrl(index, h2(hash));
            bucket.write(value);
            self.items += 1;
            bucket
        }
    }

    /// Inserts a new element into the table, and returns a mutable reference to it.
    ///
    /// This does not check if the given element already exists in the table.
    #[cfg_attr(feature = "inline-more", inline)]
    pub fn insert_entry(&mut self, hash: u64, value: T, hasher: impl Fn(&T) -> u64) -> &mut T {
        unsafe { self.insert(hash, value, hasher).as_mut() }
    }

    /// Inserts a new element into the table, without growing the table.
    ///
    /// There must be enough space in the table to insert the new element.
    ///
    /// This does not check if the given element already exists in the table.
    #[cfg_attr(feature = "inline-more", inline)]
    #[cfg(any(feature = "raw", feature = "rustc-internal-api"))]
    pub fn insert_no_grow(&mut self, hash: u64, value: T) -> Bucket<T> {
        unsafe {
            let index = self.find_insert_slot(hash);
            let bucket = self.bucket(index);

            // If we are replacing a DELETED entry then we don't need to update
            // the load counter.
            let old_ctrl = *self.ctrl(index);
            self.growth_left -= special_is_empty(old_ctrl) as usize;

            self.set_ctrl(index, h2(hash));
            bucket.write(value);
            self.items += 1;
            bucket
        }
    }

    /// Temporary removes a bucket, applying the given function to the removed
    /// element and optionally put back the returned value in the same bucket.
    ///
    /// Returns `true` if the bucket still contains an element
    ///
    /// This does not check if the given bucket is actually occupied.
    #[cfg_attr(feature = "inline-more", inline)]
    pub unsafe fn replace_bucket_with<F>(&mut self, bucket: Bucket<T>, f: F) -> bool
    where
        F: FnOnce(T) -> Option<T>,
    {
        let index = self.bucket_index(&bucket);
        let old_ctrl = *self.ctrl(index);
        debug_assert!(is_full(old_ctrl));
        let old_growth_left = self.growth_left;
        let item = self.remove(bucket);
        if let Some(new_item) = f(item) {
            self.growth_left = old_growth_left;
            self.set_ctrl(index, old_ctrl);
            self.items += 1;
            self.bucket(index).write(new_item);
            true
        } else {
            false
        }
    }

    /// Searches for an element in the table.
    #[inline]
    pub fn find(&self, hash: u64, mut eq: impl FnMut(&T) -> bool) -> Option<Bucket<T>> {
        unsafe {
            for bucket in self.iter_hash(hash) {
                let elm = bucket.as_ref();
                if likely(eq(elm)) {
                    return Some(bucket);
                }
            }
            None
        }
    }

    /// Gets a reference to an element in the table.
    #[inline]
    pub fn get(&self, hash: u64, eq: impl FnMut(&T) -> bool) -> Option<&T> {
        // Avoid `Option::map` because it bloats LLVM IR.
        match self.find(hash, eq) {
            Some(bucket) => Some(unsafe { bucket.as_ref() }),
            None => None,
        }
    }

    /// Gets a mutable reference to an element in the table.
    #[inline]
    pub fn get_mut(&mut self, hash: u64, eq: impl FnMut(&T) -> bool) -> Option<&mut T> {
        // Avoid `Option::map` because it bloats LLVM IR.
        match self.find(hash, eq) {
            Some(bucket) => Some(unsafe { bucket.as_mut() }),
            None => None,
        }
    }

    /// Returns the number of elements the map can hold without reallocating.
    ///
    /// This number is a lower bound; the table might be able to hold
    /// more, but is guaranteed to be able to hold at least this many.
    #[cfg_attr(feature = "inline-more", inline)]
    pub fn capacity(&self) -> usize {
        self.items + self.growth_left
    }

    /// Returns the number of elements in the table.
    #[cfg_attr(feature = "inline-more", inline)]
    pub fn len(&self) -> usize {
        self.items
    }

    /// Returns the number of buckets in the table.
    #[cfg_attr(feature = "inline-more", inline)]
    pub fn buckets(&self) -> usize {
        self.bucket_mask + 1
    }

    /// Returns the number of control bytes in the table.
    #[cfg_attr(feature = "inline-more", inline)]
    fn num_ctrl_bytes(&self) -> usize {
        self.bucket_mask + 1 + Group::WIDTH
    }

    /// Returns whether this table points to the empty singleton with a capacity
    /// of 0.
    #[cfg_attr(feature = "inline-more", inline)]
    fn is_empty_singleton(&self) -> bool {
        self.bucket_mask == 0
    }

    /// Returns an iterator over every element in the table. It is up to
    /// the caller to ensure that the `RawTable` outlives the `RawIter`.
    /// Because we cannot make the `next` method unsafe on the `RawIter`
    /// struct, we have to make the `iter` method unsafe.
    #[cfg_attr(feature = "inline-more", inline)]
    pub unsafe fn iter(&self) -> RawIter<T> {
        let data = Bucket::from_base_index(self.data_end(), 0);
        RawIter {
            iter: RawIterRange::new(self.ctrl.as_ptr(), data, self.buckets()),
            items: self.items,
        }
    }

    /// Returns an iterator over occupied buckets that could match a given hash.
    ///
    /// In rare cases, the iterator may return a bucket with a different hash.
    ///
    /// It is up to the caller to ensure that the `RawTable` outlives the
    /// `RawIterHash`. Because we cannot make the `next` method unsafe on the
    /// `RawIterHash` struct, we have to make the `iter_hash` method unsafe.
    #[cfg_attr(feature = "inline-more", inline)]
    pub unsafe fn iter_hash(&self, hash: u64) -> RawIterHash<'_, T> {
        RawIterHash::new(self, hash)
    }

    /// Returns an iterator which removes all elements from the table without
    /// freeing the memory.
    #[cfg_attr(feature = "inline-more", inline)]
    pub fn drain(&mut self) -> RawDrain<'_, T> {
        unsafe {
            let iter = self.iter();
            self.drain_iter_from(iter)
        }
    }

    /// Returns an iterator which removes all elements from the table without
    /// freeing the memory.
    ///
    /// Iteration starts at the provided iterator's current location.
    ///
    /// It is up to the caller to ensure that the iterator is valid for this
    /// `RawTable` and covers all items that remain in the table.
    #[cfg_attr(feature = "inline-more", inline)]
    pub unsafe fn drain_iter_from(&mut self, iter: RawIter<T>) -> RawDrain<'_, T> {
        debug_assert_eq!(iter.len(), self.len());
        RawDrain {
            iter,
            table: ManuallyDrop::new(mem::replace(self, Self::new())),
            orig_table: NonNull::from(self),
            marker: PhantomData,
        }
    }

    /// Returns an iterator which consumes all elements from the table.
    ///
    /// Iteration starts at the provided iterator's current location.
    ///
    /// It is up to the caller to ensure that the iterator is valid for this
    /// `RawTable` and covers all items that remain in the table.
    pub unsafe fn into_iter_from(self, iter: RawIter<T>) -> RawIntoIter<T> {
        debug_assert_eq!(iter.len(), self.len());

        let alloc = self.into_alloc();
        RawIntoIter {
            iter,
            alloc,
            marker: PhantomData,
        }
    }

    /// Converts the table into a raw allocation. The contents of the table
    /// should be dropped using a `RawIter` before freeing the allocation.
    #[cfg_attr(feature = "inline-more", inline)]
    pub(crate) fn into_alloc(self) -> Option<(NonNull<u8>, Layout)> {
        let alloc = if self.is_empty_singleton() {
            None
        } else {
            // Avoid `Option::unwrap_or_else` because it bloats LLVM IR.
            let (layout, ctrl_offset) = match calculate_layout::<T>(self.buckets()) {
                Some(lco) => lco,
                None => unsafe { hint::unreachable_unchecked() },
            };
            Some((
                unsafe { NonNull::new_unchecked(self.ctrl.as_ptr().sub(ctrl_offset)) },
                layout,
            ))
        };
        mem::forget(self);
        alloc
    }
}

unsafe impl<T> Send for RawTable<T> where T: Send {}
unsafe impl<T> Sync for RawTable<T> where T: Sync {}

impl<T: Clone> Clone for RawTable<T> {
    fn clone(&self) -> Self {
        if self.is_empty_singleton() {
            Self::new()
        } else {
            unsafe {
                let mut new_table = ManuallyDrop::new(
                    // Avoid `Result::ok_or_else` because it bloats LLVM IR.
                    match Self::new_uninitialized(self.buckets(), Fallibility::Infallible) {
                        Ok(table) => table,
                        Err(_) => hint::unreachable_unchecked(),
                    },
                );

                new_table.clone_from_spec(self, |new_table| {
                    // We need to free the memory allocated for the new table.
                    new_table.free_buckets();
                });

                // Return the newly created table.
                ManuallyDrop::into_inner(new_table)
            }
        }
    }

    fn clone_from(&mut self, source: &Self) {
        if source.is_empty_singleton() {
            *self = Self::new();
        } else {
            unsafe {
                // First, drop all our elements without clearing the control bytes.
                if mem::needs_drop::<T>() && self.len() != 0 {
                    for item in self.iter() {
                        item.drop();
                    }
                }

                // If necessary, resize our table to match the source.
                if self.buckets() != source.buckets() {
                    // Skip our drop by using ptr::write.
                    if !self.is_empty_singleton() {
                        self.free_buckets();
                    }
                    (self as *mut Self).write(
                        // Avoid `Result::unwrap_or_else` because it bloats LLVM IR.
                        match Self::new_uninitialized(source.buckets(), Fallibility::Infallible) {
                            Ok(table) => table,
                            Err(_) => hint::unreachable_unchecked(),
                        },
                    );
                }

                self.clone_from_spec(source, |self_| {
                    // We need to leave the table in an empty state.
                    self_.clear_no_drop()
                });
            }
        }
    }
}

/// Specialization of `clone_from` for `Copy` types
trait RawTableClone {
    unsafe fn clone_from_spec(&mut self, source: &Self, on_panic: impl FnMut(&mut Self));
}
impl<T: Clone> RawTableClone for RawTable<T> {
    #[cfg_attr(feature = "inline-more", inline)]
    default_fn! {
        unsafe fn clone_from_spec(&mut self, source: &Self, on_panic: impl FnMut(&mut Self)) {
            self.clone_from_impl(source, on_panic);
        }
    }
}
#[cfg(feature = "nightly")]
impl<T: Copy> RawTableClone for RawTable<T> {
    #[cfg_attr(feature = "inline-more", inline)]
    unsafe fn clone_from_spec(&mut self, source: &Self, _on_panic: impl FnMut(&mut Self)) {
        source
            .ctrl(0)
            .copy_to_nonoverlapping(self.ctrl(0), self.num_ctrl_bytes());
        source
            .data_start()
            .copy_to_nonoverlapping(self.data_start(), self.buckets());

        self.items = source.items;
        self.growth_left = source.growth_left;
    }
}

impl<T: Clone> RawTable<T> {
    /// Common code for clone and clone_from. Assumes `self.buckets() == source.buckets()`.
    #[cfg_attr(feature = "inline-more", inline)]
    unsafe fn clone_from_impl(&mut self, source: &Self, mut on_panic: impl FnMut(&mut Self)) {
        // Copy the control bytes unchanged. We do this in a single pass
        source
            .ctrl(0)
            .copy_to_nonoverlapping(self.ctrl(0), self.num_ctrl_bytes());

        // The cloning of elements may panic, in which case we need
        // to make sure we drop only the elements that have been
        // cloned so far.
        let mut guard = guard((0, &mut *self), |(index, self_)| {
            if mem::needs_drop::<T>() && self_.len() != 0 {
                for i in 0..=*index {
                    if is_full(*self_.ctrl(i)) {
                        self_.bucket(i).drop();
                    }
                }
            }

            // Depending on whether we were called from clone or clone_from, we
            // either need to free the memory for the destination table or just
            // clear the control bytes.
            on_panic(self_);
        });

        for from in source.iter() {
            let index = source.bucket_index(&from);
            let to = guard.1.bucket(index);
            to.write(from.as_ref().clone());

            // Update the index in case we need to unwind.
            guard.0 = index;
        }

        // Successfully cloned all items, no need to clean up.
        mem::forget(guard);

        self.items = source.items;
        self.growth_left = source.growth_left;
    }

    /// Variant of `clone_from` to use when a hasher is available.
    #[cfg(feature = "raw")]
    pub fn clone_from_with_hasher(&mut self, source: &Self, hasher: impl Fn(&T) -> u64) {
        // If we have enough capacity in the table, just clear it and insert
        // elements one by one. We don't do this if we have the same number of
        // buckets as the source since we can just copy the contents directly
        // in that case.
        if self.buckets() != source.buckets()
            && bucket_mask_to_capacity(self.bucket_mask) >= source.len()
        {
            self.clear();

            let guard_self = guard(&mut *self, |self_| {
                // Clear the partially copied table if a panic occurs, otherwise
                // items and growth_left will be out of sync with the contents
                // of the table.
                self_.clear();
            });

            unsafe {
                for item in source.iter() {
                    // This may panic.
                    let item = item.as_ref().clone();
                    let hash = hasher(&item);

                    // We can use a simpler version of insert() here since:
                    // - there are no DELETED entries.
                    // - we know there is enough space in the table.
                    // - all elements are unique.
                    let index = guard_self.find_insert_slot(hash);
                    guard_self.set_ctrl(index, h2(hash));
                    guard_self.bucket(index).write(item);
                }
            }

            // Successfully cloned all items, no need to clean up.
            mem::forget(guard_self);

            self.items = source.items;
            self.growth_left -= source.items;
        } else {
            self.clone_from(source);
        }
    }
}

#[cfg(feature = "nightly")]
unsafe impl<#[may_dangle] T> Drop for RawTable<T> {
    #[cfg_attr(feature = "inline-more", inline)]
    fn drop(&mut self) {
        if !self.is_empty_singleton() {
            unsafe {
                if mem::needs_drop::<T>() && self.len() != 0 {
                    for item in self.iter() {
                        item.drop();
                    }
                }
                self.free_buckets();
            }
        }
    }
}
#[cfg(not(feature = "nightly"))]
impl<T> Drop for RawTable<T> {
    #[cfg_attr(feature = "inline-more", inline)]
    fn drop(&mut self) {
        if !self.is_empty_singleton() {
            unsafe {
                if mem::needs_drop::<T>() && self.len() != 0 {
                    for item in self.iter() {
                        item.drop();
                    }
                }
                self.free_buckets();
            }
        }
    }
}

impl<T> IntoIterator for RawTable<T> {
    type Item = T;
    type IntoIter = RawIntoIter<T>;

    #[cfg_attr(feature = "inline-more", inline)]
    fn into_iter(self) -> RawIntoIter<T> {
        unsafe {
            let iter = self.iter();
            self.into_iter_from(iter)
        }
    }
}

/// Iterator over a sub-range of a table. Unlike `RawIter` this iterator does
/// not track an item count.
pub(crate) struct RawIterRange<T> {
    // Mask of full buckets in the current group. Bits are cleared from this
    // mask as each element is processed.
    current_group: BitMask,

    // Pointer to the buckets for the current group.
    data: Bucket<T>,

    // Pointer to the next group of control bytes,
    // Must be aligned to the group size.
    next_ctrl: *const u8,

    // Pointer one past the last control byte of this range.
    end: *const u8,
}

impl<T> RawIterRange<T> {
    /// Returns a `RawIterRange` covering a subset of a table.
    ///
    /// The control byte address must be aligned to the group size.
    #[cfg_attr(feature = "inline-more", inline)]
    unsafe fn new(ctrl: *const u8, data: Bucket<T>, len: usize) -> Self {
        debug_assert_ne!(len, 0);
        debug_assert_eq!(ctrl as usize % Group::WIDTH, 0);
        let end = ctrl.add(len);

        // Load the first group and advance ctrl to point to the next group
        let current_group = Group::load_aligned(ctrl).match_full();
        let next_ctrl = ctrl.add(Group::WIDTH);

        Self {
            current_group,
            data,
            next_ctrl,
            end,
        }
    }

    /// Splits a `RawIterRange` into two halves.
    ///
    /// Returns `None` if the remaining range is smaller than or equal to the
    /// group width.
    #[cfg_attr(feature = "inline-more", inline)]
    #[cfg(feature = "rayon")]
    pub(crate) fn split(mut self) -> (Self, Option<RawIterRange<T>>) {
        unsafe {
            if self.end <= self.next_ctrl {
                // Nothing to split if the group that we are current processing
                // is the last one.
                (self, None)
            } else {
                // len is the remaining number of elements after the group that
                // we are currently processing. It must be a multiple of the
                // group size (small tables are caught by the check above).
                let len = offset_from(self.end, self.next_ctrl);
                debug_assert_eq!(len % Group::WIDTH, 0);

                // Split the remaining elements into two halves, but round the
                // midpoint down in case there is an odd number of groups
                // remaining. This ensures that:
                // - The tail is at least 1 group long.
                // - The split is roughly even considering we still have the
                //   current group to process.
                let mid = (len / 2) & !(Group::WIDTH - 1);

                let tail = Self::new(
                    self.next_ctrl.add(mid),
                    self.data.next_n(Group::WIDTH).next_n(mid),
                    len - mid,
                );
                debug_assert_eq!(
                    self.data.next_n(Group::WIDTH).next_n(mid).ptr,
                    tail.data.ptr
                );
                debug_assert_eq!(self.end, tail.end);
                self.end = self.next_ctrl.add(mid);
                debug_assert_eq!(self.end.add(Group::WIDTH), tail.next_ctrl);
                (self, Some(tail))
            }
        }
    }
}

// We make raw iterators unconditionally Send and Sync, and let the PhantomData
// in the actual iterator implementations determine the real Send/Sync bounds.
unsafe impl<T> Send for RawIterRange<T> {}
unsafe impl<T> Sync for RawIterRange<T> {}

impl<T> Clone for RawIterRange<T> {
    #[cfg_attr(feature = "inline-more", inline)]
    fn clone(&self) -> Self {
        Self {
            data: self.data.clone(),
            next_ctrl: self.next_ctrl,
            current_group: self.current_group,
            end: self.end,
        }
    }
}

impl<T> Iterator for RawIterRange<T> {
    type Item = Bucket<T>;

    #[cfg_attr(feature = "inline-more", inline)]
    fn next(&mut self) -> Option<Bucket<T>> {
        unsafe {
            loop {
                if let Some(index) = self.current_group.lowest_set_bit() {
                    self.current_group = self.current_group.remove_lowest_bit();
                    return Some(self.data.next_n(index));
                }

                if self.next_ctrl >= self.end {
                    return None;
                }

                // We might read past self.end up to the next group boundary,
                // but this is fine because it only occurs on tables smaller
                // than the group size where the trailing control bytes are all
                // EMPTY. On larger tables self.end is guaranteed to be aligned
                // to the group size (since tables are power-of-two sized).
                self.current_group = Group::load_aligned(self.next_ctrl).match_full();
                self.data = self.data.next_n(Group::WIDTH);
                self.next_ctrl = self.next_ctrl.add(Group::WIDTH);
            }
        }
    }

    #[cfg_attr(feature = "inline-more", inline)]
    fn size_hint(&self) -> (usize, Option<usize>) {
        // We don't have an item count, so just guess based on the range size.
        (
            0,
            Some(unsafe { offset_from(self.end, self.next_ctrl) + Group::WIDTH }),
        )
    }
}

impl<T> FusedIterator for RawIterRange<T> {}

/// Iterator which returns a raw pointer to every full bucket in the table.
///
/// For maximum flexibility this iterator is not bound by a lifetime, but you
/// must observe several rules when using it:
/// - You must not free the hash table while iterating (including via growing/shrinking).
/// - It is fine to erase a bucket that has been yielded by the iterator.
/// - Erasing a bucket that has not yet been yielded by the iterator may still
///   result in the iterator yielding that bucket (unless `reflect_remove` is called).
/// - It is unspecified whether an element inserted after the iterator was
///   created will be yielded by that iterator (unless `reflect_insert` is called).
/// - The order in which the iterator yields bucket is unspecified and may
///   change in the future.
pub struct RawIter<T> {
    pub(crate) iter: RawIterRange<T>,
    items: usize,
}

impl<T> RawIter<T> {
    /// Refresh the iterator so that it reflects a removal from the given bucket.
    ///
    /// For the iterator to remain valid, this method must be called once
    /// for each removed bucket before `next` is called again.
    ///
    /// This method should be called _before_ the removal is made. It is not necessary to call this
    /// method if you are removing an item that this iterator yielded in the past.
    #[cfg(feature = "raw")]
    pub fn reflect_remove(&mut self, b: &Bucket<T>) {
        self.reflect_toggle_full(b, false);
    }

    /// Refresh the iterator so that it reflects an insertion into the given bucket.
    ///
    /// For the iterator to remain valid, this method must be called once
    /// for each insert before `next` is called again.
    ///
    /// This method does not guarantee that an insertion of a bucket witha greater
    /// index than the last one yielded will be reflected in the iterator.
    ///
    /// This method should be called _after_ the given insert is made.
    #[cfg(feature = "raw")]
    pub fn reflect_insert(&mut self, b: &Bucket<T>) {
        self.reflect_toggle_full(b, true);
    }

    /// Refresh the iterator so that it reflects a change to the state of the given bucket.
    #[cfg(feature = "raw")]
    fn reflect_toggle_full(&mut self, b: &Bucket<T>, is_insert: bool) {
        unsafe {
            if b.as_ptr() > self.iter.data.as_ptr() {
                // The iterator has already passed the bucket's group.
                // So the toggle isn't relevant to this iterator.
                return;
            }

            if self.iter.next_ctrl < self.iter.end
                && b.as_ptr() <= self.iter.data.next_n(Group::WIDTH).as_ptr()
            {
                // The iterator has not yet reached the bucket's group.
                // We don't need to reload anything, but we do need to adjust the item count.

                if cfg!(debug_assertions) {
                    // Double-check that the user isn't lying to us by checking the bucket state.
                    // To do that, we need to find its control byte. We know that self.iter.data is
                    // at self.iter.next_ctrl - Group::WIDTH, so we work from there:
                    let offset = offset_from(self.iter.data.as_ptr(), b.as_ptr());
                    let ctrl = self.iter.next_ctrl.sub(Group::WIDTH).add(offset);
                    // This method should be called _before_ a removal, or _after_ an insert,
                    // so in both cases the ctrl byte should indicate that the bucket is full.
                    assert!(is_full(*ctrl));
                }

                if is_insert {
                    self.items += 1;
                } else {
                    self.items -= 1;
                }

                return;
            }

            // The iterator is at the bucket group that the toggled bucket is in.
            // We need to do two things:
            //
            //  - Determine if the iterator already yielded the toggled bucket.
            //    If it did, we're done.
            //  - Otherwise, update the iterator cached group so that it won't
            //    yield a to-be-removed bucket, or _will_ yield a to-be-added bucket.
            //    We'll also need ot update the item count accordingly.
            if let Some(index) = self.iter.current_group.lowest_set_bit() {
                let next_bucket = self.iter.data.next_n(index);
                if b.as_ptr() > next_bucket.as_ptr() {
                    // The toggled bucket is "before" the bucket the iterator would yield next. We
                    // therefore don't need to do anything --- the iterator has already passed the
                    // bucket in question.
                    //
                    // The item count must already be correct, since a removal or insert "prior" to
                    // the iterator's position wouldn't affect the item count.
                } else {
                    // The removed bucket is an upcoming bucket. We need to make sure it does _not_
                    // get yielded, and also that it's no longer included in the item count.
                    //
                    // NOTE: We can't just reload the group here, both since that might reflect
                    // inserts we've already passed, and because that might inadvertently unset the
                    // bits for _other_ removals. If we do that, we'd have to also decrement the
                    // item count for those other bits that we unset. But the presumably subsequent
                    // call to reflect for those buckets might _also_ decrement the item count.
                    // Instead, we _just_ flip the bit for the particular bucket the caller asked
                    // us to reflect.
                    let our_bit = offset_from(self.iter.data.as_ptr(), b.as_ptr());
                    let was_full = self.iter.current_group.flip(our_bit);
                    debug_assert_ne!(was_full, is_insert);

                    if is_insert {
                        self.items += 1;
                    } else {
                        self.items -= 1;
                    }

                    if cfg!(debug_assertions) {
                        if b.as_ptr() == next_bucket.as_ptr() {
                            // The removed bucket should no longer be next
                            debug_assert_ne!(self.iter.current_group.lowest_set_bit(), Some(index));
                        } else {
                            // We should not have changed what bucket comes next.
                            debug_assert_eq!(self.iter.current_group.lowest_set_bit(), Some(index));
                        }
                    }
                }
            } else {
                // We must have already iterated past the removed item.
            }
        }
    }
}

impl<T> Clone for RawIter<T> {
    #[cfg_attr(feature = "inline-more", inline)]
    fn clone(&self) -> Self {
        Self {
            iter: self.iter.clone(),
            items: self.items,
        }
    }
}

impl<T> Iterator for RawIter<T> {
    type Item = Bucket<T>;

    #[cfg_attr(feature = "inline-more", inline)]
    fn next(&mut self) -> Option<Bucket<T>> {
        if let Some(b) = self.iter.next() {
            self.items -= 1;
            Some(b)
        } else {
            // We don't check against items == 0 here to allow the
            // compiler to optimize away the item count entirely if the
            // iterator length is never queried.
            debug_assert_eq!(self.items, 0);
            None
        }
    }

    #[cfg_attr(feature = "inline-more", inline)]
    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.items, Some(self.items))
    }
}

impl<T> ExactSizeIterator for RawIter<T> {}
impl<T> FusedIterator for RawIter<T> {}

/// Iterator which consumes a table and returns elements.
pub struct RawIntoIter<T> {
    iter: RawIter<T>,
    alloc: Option<(NonNull<u8>, Layout)>,
    marker: PhantomData<T>,
}

impl<T> RawIntoIter<T> {
    #[cfg_attr(feature = "inline-more", inline)]
    pub fn iter(&self) -> RawIter<T> {
        self.iter.clone()
    }
}

unsafe impl<T> Send for RawIntoIter<T> where T: Send {}
unsafe impl<T> Sync for RawIntoIter<T> where T: Sync {}

#[cfg(feature = "nightly")]
unsafe impl<#[may_dangle] T> Drop for RawIntoIter<T> {
    #[cfg_attr(feature = "inline-more", inline)]
    fn drop(&mut self) {
        unsafe {
            // Drop all remaining elements
            if mem::needs_drop::<T>() && self.iter.len() != 0 {
                while let Some(item) = self.iter.next() {
                    item.drop();
                }
            }

            // Free the table
            if let Some((ptr, layout)) = self.alloc {
                dealloc(ptr.as_ptr(), layout);
            }
        }
    }
}
#[cfg(not(feature = "nightly"))]
impl<T> Drop for RawIntoIter<T> {
    #[cfg_attr(feature = "inline-more", inline)]
    fn drop(&mut self) {
        unsafe {
            // Drop all remaining elements
            if mem::needs_drop::<T>() && self.iter.len() != 0 {
                while let Some(item) = self.iter.next() {
                    item.drop();
                }
            }

            // Free the table
            if let Some((ptr, layout)) = self.alloc {
                dealloc(ptr.as_ptr(), layout);
            }
        }
    }
}

impl<T> Iterator for RawIntoIter<T> {
    type Item = T;

    #[cfg_attr(feature = "inline-more", inline)]
    fn next(&mut self) -> Option<T> {
        unsafe { Some(self.iter.next()?.read()) }
    }

    #[cfg_attr(feature = "inline-more", inline)]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<T> ExactSizeIterator for RawIntoIter<T> {}
impl<T> FusedIterator for RawIntoIter<T> {}

/// Iterator which consumes elements without freeing the table storage.
pub struct RawDrain<'a, T> {
    iter: RawIter<T>,

    // The table is moved into the iterator for the duration of the drain. This
    // ensures that an empty table is left if the drain iterator is leaked
    // without dropping.
    table: ManuallyDrop<RawTable<T>>,
    orig_table: NonNull<RawTable<T>>,

    // We don't use a &'a mut RawTable<T> because we want RawDrain to be
    // covariant over T.
    marker: PhantomData<&'a RawTable<T>>,
}

impl<T> RawDrain<'_, T> {
    #[cfg_attr(feature = "inline-more", inline)]
    pub fn iter(&self) -> RawIter<T> {
        self.iter.clone()
    }
}

unsafe impl<T> Send for RawDrain<'_, T> where T: Send {}
unsafe impl<T> Sync for RawDrain<'_, T> where T: Sync {}

impl<T> Drop for RawDrain<'_, T> {
    #[cfg_attr(feature = "inline-more", inline)]
    fn drop(&mut self) {
        unsafe {
            // Drop all remaining elements. Note that this may panic.
            if mem::needs_drop::<T>() && self.iter.len() != 0 {
                while let Some(item) = self.iter.next() {
                    item.drop();
                }
            }

            // Reset the contents of the table now that all elements have been
            // dropped.
            self.table.clear_no_drop();

            // Move the now empty table back to its original location.
            self.orig_table
                .as_ptr()
                .copy_from_nonoverlapping(&*self.table, 1);
        }
    }
}

impl<T> Iterator for RawDrain<'_, T> {
    type Item = T;

    #[cfg_attr(feature = "inline-more", inline)]
    fn next(&mut self) -> Option<T> {
        unsafe {
            let item = self.iter.next()?;
            Some(item.read())
        }
    }

    #[cfg_attr(feature = "inline-more", inline)]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<T> ExactSizeIterator for RawDrain<'_, T> {}
impl<T> FusedIterator for RawDrain<'_, T> {}

/// Iterator over occupied buckets that could match a given hash.
///
/// In rare cases, the iterator may return a bucket with a different hash.
pub struct RawIterHash<'a, T> {
    table: &'a RawTable<T>,

    // The top 7 bits of the hash.
    h2_hash: u8,

    // The sequence of groups to probe in the search.
    probe_seq: ProbeSeq,

    // The current group and its position.
    pos: usize,
    group: Group,

    // The elements within the group with a matching h2-hash.
    bitmask: BitMaskIter,
}

impl<'a, T> RawIterHash<'a, T> {
    fn new(table: &'a RawTable<T>, hash: u64) -> Self {
        unsafe {
            let h2_hash = h2(hash);
            let mut probe_seq = table.probe_seq(hash);
            let pos = probe_seq.next().unwrap();
            let group = Group::load(table.ctrl(pos));
            let bitmask = group.match_byte(h2_hash).into_iter();

            RawIterHash {
                table,
                h2_hash,
                probe_seq,
                pos,
                group,
                bitmask,
            }
        }
    }
}

impl<'a, T> Iterator for RawIterHash<'a, T> {
    type Item = Bucket<T>;

    fn next(&mut self) -> Option<Bucket<T>> {
        unsafe {
            loop {
                if let Some(bit) = self.bitmask.next() {
                    let index = (self.pos + bit) & self.table.bucket_mask;
                    let bucket = self.table.bucket(index);
                    return Some(bucket);
                }
                if likely(self.group.match_empty().any_bit_set()) {
                    return None;
                }
                self.pos = self.probe_seq.next().unwrap();
                self.group = Group::load(self.table.ctrl(self.pos));
                self.bitmask = self.group.match_byte(self.h2_hash).into_iter();
            }
        }
    }
}
