#![deny(missing_docs)]

//! `ThinVec` is exactly the same as `Vec`, except that it stores its `len` and `capacity` in the buffer
//! it allocates.
//!
//! This makes the memory footprint of ThinVecs lower; notably in cases where space is reserved for
//! a non-existence `ThinVec<T>`. So `Vec<ThinVec<T>>` and `Option<ThinVec<T>>::None` will waste less
//! space. Being pointer-sized also means it can be passed/stored in registers.
//!
//! Of course, any actually constructed `ThinVec` will theoretically have a bigger allocation, but
//! the fuzzy nature of allocators means that might not actually be the case.
//!
//! Properties of `Vec` that are preserved:
//! * `ThinVec::new()` doesn't allocate (it points to a statically allocated singleton)
//! * reallocation can be done in place
//! * `size_of::<ThinVec<T>>()` == `size_of::<Option<ThinVec<T>>>()`
//!
//! Properties of `Vec` that aren't preserved:
//! * `ThinVec<T>` can't ever be zero-cost roundtripped to a `Box<[T]>`, `String`, or `*mut T`
//! * `from_raw_parts` doesn't exist
//! * `ThinVec` currently doesn't bother to not-allocate for Zero Sized Types (e.g. `ThinVec<()>`),
//!   but it could be done if someone cared enough to implement it.
//!
//!
//!
//! # Gecko FFI
//!
//! If you enable the gecko-ffi feature, `ThinVec` will verbatim bridge with the nsTArray type in
//! Gecko (Firefox). That is, `ThinVec` and nsTArray have identical layouts *but not ABIs*,
//! so nsTArrays/ThinVecs an be natively manipulated by C++ and Rust, and ownership can be
//! transferred across the FFI boundary (**IF YOU ARE CAREFUL, SEE BELOW!!**).
//!
//! While this feature is handy, it is also inherently dangerous to use because Rust and C++ do not
//! know about each other. Specifically, this can be an issue with non-POD types (types which
//! have destructors, move constructors, or are `!Copy`).
//!
//! ## Do Not Pass By Value
//!
//! The biggest thing to keep in mind is that **FFI functions cannot pass ThinVec/nsTArray
//! by-value**. That is, these are busted APIs:
//!
//! ```rust,ignore
//! // BAD WRONG
//! extern fn process_data(data: ThinVec<u32>) { ... }
//! // BAD WRONG
//! extern fn get_data() -> ThinVec<u32> { ... }
//! ```
//!
//! You must instead pass by-reference:
//!
//! ```rust
//! # use thin_vec::*;
//! # use std::mem;
//!
//! // Read-only access, ok!
//! extern fn process_data(data: &ThinVec<u32>) {
//!     for val in data {
//!         println!("{}", val);
//!     }
//! }
//!
//! // Replace with empty instance to take ownership, ok!
//! extern fn consume_data(data: &mut ThinVec<u32>) {
//!     let owned = mem::replace(data, ThinVec::new());
//!     mem::drop(owned);
//! }
//!
//! // Mutate input, ok!
//! extern fn add_data(dataset: &mut ThinVec<u32>) {
//!     dataset.push(37);
//!     dataset.push(12);
//! }
//!
//! // Return via out-param, usually ok!
//! //
//! // WARNING: output must be initialized! (Empty nsTArrays are free, so just do it!)
//! extern fn get_data(output: &mut ThinVec<u32>) {
//!     *output = thin_vec![1, 2, 3, 4, 5];
//! }
//! ```
//!
//! Ignorable Explanation For Those Who Really Want To Know Why:
//!
//! > The fundamental issue is that Rust and C++ can't currently communicate about destructors, and
//! > the semantics of C++ require destructors of function arguments to be run when the function
//! > returns. Whether the callee or caller is responsible for this is also platform-specific, so
//! > trying to hack around it manually would be messy.
//! >
//! > Also a type having a destructor changes its C++ ABI, because that type must actually exist
//! > in memory (unlike a trivial struct, which is often passed in registers). We don't currently
//! > have a way to communicate to Rust that this is happening, so even if we worked out the
//! > destructor issue with say, MaybeUninit, it would still be a non-starter without some RFCs
//! > to add explicit rustc support.
//! >
//! > Realistically, the best answer here is to have a "heavier" bindgen that can secretly
//! > generate FFI glue so we can pass things "by value" and have it generate by-reference code
//! > behind our back (like the cxx crate does). This would muddy up debugging/searchfox though.
//!
//! ## Types Should Be Trivially Relocatable
//!
//! Types in Rust are always trivially relocatable (unless suitably borrowed/[pinned][]/hidden).
//! This means all Rust types are legal to relocate with a bitwise copy, you cannot provide
//! copy or move constructors to execute when this happens, and the old location won't have its
//! destructor run. This will cause problems for types which have a significant location
//! (types that intrusively point into themselves or have their location registered with a service).
//!
//! While relocations are generally predictable if you're very careful, **you should avoid using
//! types with significant locations with Rust FFI**.
//!
//! Specifically, `ThinVec` will trivially relocate its contents whenever it needs to reallocate its
//! buffer to change its capacity. This is the default reallocation strategy for nsTArray, and is
//! suitable for the vast majority of types. Just be aware of this limitation!
//!
//! ## Auto Arrays Are Dangerous
//!
//! `ThinVec` has *some* support for handling auto arrays which store their buffer on the stack,
//! but this isn't well tested.
//!
//! Regardless of how much support we provide, Rust won't be aware of the buffer's limited lifetime,
//! so standard auto array safety caveats apply about returning/storing them! `ThinVec` won't ever
//! produce an auto array on its own, so this is only an issue for transferring an nsTArray into
//! Rust.
//!
//! ## Other Issues
//!
//! Standard FFI caveats also apply:
//!
//!  * Rust is more strict about POD types being initialized (use MaybeUninit if you must)
//!  * `ThinVec<T>` has no idea if the C++ version of `T` has move/copy/assign/delete overloads
//!  * `nsTArray<T>` has no idea if the Rust version of `T` has a Drop/Clone impl
//!  * C++ can do all sorts of unsound things that Rust can't catch
//!  * C++ and Rust don't agree on how zero-sized/empty types should be handled
//!
//! The gecko-ffi feature will not work if you aren't linking with code that has nsTArray
//! defined. Specifically, we must share the symbol for nsTArray's empty singleton. You will get
//! linking errors if that isn't defined.
//!
//! The gecko-ffi feature also limits `ThinVec` to the legacy behaviors of nsTArray. Most notably,
//! nsTArray has a maximum capacity of i32::MAX (~2.1 billion items). Probably not an issue.
//! Probably.
//!
//! [pinned]: https://doc.rust-lang.org/std/pin/index.html

#![allow(clippy::comparison_chain, clippy::missing_safety_doc)]

use std::alloc::*;
use std::borrow::*;
use std::cmp::*;
use std::convert::TryFrom;
use std::convert::TryInto;
use std::hash::*;
use std::iter::FromIterator;
use std::marker::PhantomData;
use std::ops::Bound;
use std::ops::{Deref, DerefMut, RangeBounds};
use std::ptr::NonNull;
use std::slice::IterMut;
use std::{fmt, io, mem, ptr, slice};

use impl_details::*;

// modules: a simple way to cfg a whole bunch of impl details at once

#[cfg(not(feature = "gecko-ffi"))]
mod impl_details {
    pub type SizeType = usize;
    pub const MAX_CAP: usize = !0;

    #[inline(always)]
    pub fn assert_size(x: usize) -> SizeType {
        x
    }
}

#[cfg(feature = "gecko-ffi")]
mod impl_details {
    // Support for briding a gecko nsTArray verbatim into a ThinVec.
    //
    // `ThinVec` can't see copy/move/delete implementations
    // from C++
    //
    // The actual layout of an nsTArray is:
    //
    // ```cpp
    // struct {
    //   uint32_t mLength;
    //   uint32_t mCapacity: 31;
    //   uint32_t mIsAutoArray: 1;
    // }
    // ```
    //
    // Rust doesn't natively support bit-fields, so we manually mask
    // and shift the bit. When the "auto" bit is set, the header and buffer
    // are actually on the stack, meaning the `ThinVec` pointer-to-header
    // is essentially an "owned borrow", and therefore dangerous to handle.
    // There are no safety guards for this situation.
    //
    // On little-endian platforms, the auto bit will be the high-bit of
    // our capacity u32. On big-endian platforms, it will be the low bit.
    // Hence we need some platform-specific CFGs for the necessary masking/shifting.
    //
    // `ThinVec` won't ever construct an auto array. They only happen when
    // bridging from C++. This means we don't need to ever set/preserve the bit.
    // We just need to be able to read and handle it if it happens to be there.
    //
    // Handling the auto bit mostly just means not freeing/reallocating the buffer.

    pub type SizeType = u32;

    pub const MAX_CAP: usize = i32::max_value() as usize;

    // Little endian: the auto bit is the high bit, and the capacity is
    // verbatim. So we just need to mask off the high bit. Note that
    // this masking is unnecessary when packing, because assert_size
    // guards against the high bit being set.
    #[cfg(target_endian = "little")]
    pub fn pack_capacity(cap: SizeType) -> SizeType {
        cap as SizeType
    }
    #[cfg(target_endian = "little")]
    pub fn unpack_capacity(cap: SizeType) -> usize {
        (cap as usize) & !(1 << 31)
    }
    #[cfg(target_endian = "little")]
    pub fn is_auto(cap: SizeType) -> bool {
        (cap & (1 << 31)) != 0
    }

    // Big endian: the auto bit is the low bit, and the capacity is
    // shifted up one bit. Masking out the auto bit is unnecessary,
    // as rust shifts always shift in 0's for unsigned integers.
    #[cfg(target_endian = "big")]
    pub fn pack_capacity(cap: SizeType) -> SizeType {
        (cap as SizeType) << 1
    }
    #[cfg(target_endian = "big")]
    pub fn unpack_capacity(cap: SizeType) -> usize {
        (cap >> 1) as usize
    }
    #[cfg(target_endian = "big")]
    pub fn is_auto(cap: SizeType) -> bool {
        (cap & 1) != 0
    }

    #[inline]
    pub fn assert_size(x: usize) -> SizeType {
        if x > MAX_CAP as usize {
            panic!("nsTArray size may not exceed the capacity of a 32-bit sized int");
        }
        x as SizeType
    }
}

// The header of a ThinVec.
//
// The _cap can be a bitfield, so use accessors to avoid trouble.
//
// In "real" gecko-ffi mode, the empty singleton will be aligned
// to 8 by gecko. But in tests we have to provide the singleton
// ourselves, and Rust makes it hard to "just" align a static.
// To avoid messing around with a wrapper type around the
// singleton *just* for tests, we just force all headers to be
// aligned to 8 in this weird "zombie" gecko mode.
//
// This shouldn't affect runtime layout (padding), but it will
// result in us asking the allocator to needlessly overalign
// non-empty ThinVecs containing align < 8 types in
// zombie-mode, but not in "real" geck-ffi mode. Minor.
#[cfg_attr(all(feature = "gecko-ffi", any(test, miri)), repr(align(8)))]
#[repr(C)]
struct Header {
    _len: SizeType,
    _cap: SizeType,
}

impl Header {
    #[inline]
    #[allow(clippy::unnecessary_cast)]
    fn len(&self) -> usize {
        self._len as usize
    }

    #[inline]
    fn set_len(&mut self, len: usize) {
        self._len = assert_size(len);
    }
}

#[cfg(feature = "gecko-ffi")]
impl Header {
    fn cap(&self) -> usize {
        unpack_capacity(self._cap)
    }

    fn set_cap(&mut self, cap: usize) {
        // debug check that our packing is working
        debug_assert_eq!(unpack_capacity(pack_capacity(cap as SizeType)), cap);
        // FIXME: this assert is busted because it reads uninit memory
        // debug_assert!(!self.uses_stack_allocated_buffer());

        // NOTE: this always stores a cleared auto bit, because set_cap
        // is only invoked by Rust, and Rust doesn't create auto arrays.
        self._cap = pack_capacity(assert_size(cap));
    }

    fn uses_stack_allocated_buffer(&self) -> bool {
        is_auto(self._cap)
    }
}

#[cfg(not(feature = "gecko-ffi"))]
impl Header {
    #[allow(clippy::unnecessary_cast)]
    fn cap(&self) -> usize {
        self._cap as usize
    }

    fn set_cap(&mut self, cap: usize) {
        self._cap = assert_size(cap);
    }
}

/// Singleton that all empty collections share.
/// Note: can't store non-zero ZSTs, we allocate in that case. We could
/// optimize everything to not do that (basically, make ptr == len and branch
/// on size == 0 in every method), but it's a bunch of work for something that
/// doesn't matter much.
#[cfg(any(not(feature = "gecko-ffi"), test, miri))]
static EMPTY_HEADER: Header = Header { _len: 0, _cap: 0 };

#[cfg(all(feature = "gecko-ffi", not(test), not(miri)))]
extern "C" {
    #[link_name = "sEmptyTArrayHeader"]
    static EMPTY_HEADER: Header;
}

// Utils for computing layouts of allocations

/// Gets the size necessary to allocate a `ThinVec<T>` with the give capacity.
///
/// # Panics
///
/// This will panic if isize::MAX is overflowed at any point.
fn alloc_size<T>(cap: usize) -> usize {
    // Compute "real" header size with pointer math
    //
    // We turn everything into isizes here so that we can catch isize::MAX overflow,
    // we never want to allow allocations larger than that!
    let header_size = mem::size_of::<Header>() as isize;
    let padding = padding::<T>() as isize;

    let data_size = if mem::size_of::<T>() == 0 {
        // If we're allocating an array for ZSTs we need a header/padding but no actual
        // space for items, so we don't care about the capacity that was requested!
        0
    } else {
        let cap: isize = cap.try_into().expect("capacity overflow");
        let elem_size = mem::size_of::<T>() as isize;
        elem_size.checked_mul(cap).expect("capacity overflow")
    };

    let final_size = data_size
        .checked_add(header_size + padding)
        .expect("capacity overflow");

    // Ok now we can turn it back into a usize (don't need to worry about negatives)
    final_size as usize
}

/// Gets the padding necessary for the array of a `ThinVec<T>`
fn padding<T>() -> usize {
    let alloc_align = alloc_align::<T>();
    let header_size = mem::size_of::<Header>();

    if alloc_align > header_size {
        if cfg!(feature = "gecko-ffi") {
            panic!(
                "nsTArray does not handle alignment above > {} correctly",
                header_size
            );
        }
        alloc_align - header_size
    } else {
        0
    }
}

/// Gets the align necessary to allocate a `ThinVec<T>`
fn alloc_align<T>() -> usize {
    max(mem::align_of::<T>(), mem::align_of::<Header>())
}

/// Gets the layout necessary to allocate a `ThinVec<T>`
///
/// # Panics
///
/// Panics if the required size overflows `isize::MAX`.
fn layout<T>(cap: usize) -> Layout {
    unsafe { Layout::from_size_align_unchecked(alloc_size::<T>(cap), alloc_align::<T>()) }
}

/// Allocates a header (and array) for a `ThinVec<T>` with the given capacity.
///
/// # Panics
///
/// Panics if the required size overflows `isize::MAX`.
fn header_with_capacity<T>(cap: usize) -> NonNull<Header> {
    debug_assert!(cap > 0);
    unsafe {
        let layout = layout::<T>(cap);
        let header = alloc(layout) as *mut Header;

        if header.is_null() {
            handle_alloc_error(layout)
        }

        // "Infinite" capacity for zero-sized types:
        (*header).set_cap(if mem::size_of::<T>() == 0 {
            MAX_CAP
        } else {
            cap
        });
        (*header).set_len(0);

        NonNull::new_unchecked(header)
    }
}

/// See the crate's top level documentation for a description of this type.
#[repr(C)]
pub struct ThinVec<T> {
    ptr: NonNull<Header>,
    boo: PhantomData<T>,
}

unsafe impl<T: Sync> Sync for ThinVec<T> {}
unsafe impl<T: Send> Send for ThinVec<T> {}

/// Creates a `ThinVec` containing the arguments.
///
// A hack to avoid linking problems with `cargo test --features=gecko-ffi`.
#[cfg_attr(not(feature = "gecko-ffi"), doc = "```")]
#[cfg_attr(feature = "gecko-ffi", doc = "```ignore")]
/// #[macro_use] extern crate thin_vec;
///
/// fn main() {
///     let v = thin_vec![1, 2, 3];
///     assert_eq!(v.len(), 3);
///     assert_eq!(v[0], 1);
///     assert_eq!(v[1], 2);
///     assert_eq!(v[2], 3);
///
///     let v = thin_vec![1; 3];
///     assert_eq!(v, [1, 1, 1]);
/// }
/// ```
#[macro_export]
macro_rules! thin_vec {
    (@UNIT $($t:tt)*) => (());

    ($elem:expr; $n:expr) => ({
        let mut vec = $crate::ThinVec::new();
        vec.resize($n, $elem);
        vec
    });
    () => {$crate::ThinVec::new()};
    ($($x:expr),*) => ({
        let len = [$(thin_vec!(@UNIT $x)),*].len();
        let mut vec = $crate::ThinVec::with_capacity(len);
        $(vec.push($x);)*
        vec
    });
    ($($x:expr,)*) => (thin_vec![$($x),*]);
}

impl<T> ThinVec<T> {
    /// Creates a new empty ThinVec.
    ///
    /// This will not allocate.
    pub fn new() -> ThinVec<T> {
        ThinVec::with_capacity(0)
    }

    /// Constructs a new, empty `ThinVec<T>` with at least the specified capacity.
    ///
    /// The vector will be able to hold at least `capacity` elements without
    /// reallocating. This method is allowed to allocate for more elements than
    /// `capacity`. If `capacity` is 0, the vector will not allocate.
    ///
    /// It is important to note that although the returned vector has the
    /// minimum *capacity* specified, the vector will have a zero *length*.
    ///
    /// If it is important to know the exact allocated capacity of a `ThinVec`,
    /// always use the [`capacity`] method after construction.
    ///
    /// **NOTE**: unlike `Vec`, `ThinVec` **MUST** allocate once to keep track of non-zero
    /// lengths. As such, we cannot provide the same guarantees about ThinVecs
    /// of ZSTs not allocating. However the allocation never needs to be resized
    /// to add more ZSTs, since the underlying array is still length 0.
    ///
    /// [Capacity and reallocation]: #capacity-and-reallocation
    /// [`capacity`]: Vec::capacity
    ///
    /// # Panics
    ///
    /// Panics if the new capacity exceeds `isize::MAX` bytes.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::ThinVec;
    ///
    /// let mut vec = ThinVec::with_capacity(10);
    ///
    /// // The vector contains no items, even though it has capacity for more
    /// assert_eq!(vec.len(), 0);
    /// assert!(vec.capacity() >= 10);
    ///
    /// // These are all done without reallocating...
    /// for i in 0..10 {
    ///     vec.push(i);
    /// }
    /// assert_eq!(vec.len(), 10);
    /// assert!(vec.capacity() >= 10);
    ///
    /// // ...but this may make the vector reallocate
    /// vec.push(11);
    /// assert_eq!(vec.len(), 11);
    /// assert!(vec.capacity() >= 11);
    ///
    /// // A vector of a zero-sized type will always over-allocate, since no
    /// // space is needed to store the actual elements.
    /// let vec_units = ThinVec::<()>::with_capacity(10);
    ///
    /// // Only true **without** the gecko-ffi feature!
    /// // assert_eq!(vec_units.capacity(), usize::MAX);
    /// ```
    pub fn with_capacity(cap: usize) -> ThinVec<T> {
        // `padding` contains ~static assertions against types that are
        // incompatible with the current feature flags. We also call it to
        // invoke these assertions when getting a pointer to the `ThinVec`
        // contents, but since we also get a pointer to the contents in the
        // `Drop` impl, trippng an assertion along that code path causes a
        // double panic. We duplicate the assertion here so that it is
        // testable,
        let _ = padding::<T>();

        if cap == 0 {
            unsafe {
                ThinVec {
                    ptr: NonNull::new_unchecked(&EMPTY_HEADER as *const Header as *mut Header),
                    boo: PhantomData,
                }
            }
        } else {
            ThinVec {
                ptr: header_with_capacity::<T>(cap),
                boo: PhantomData,
            }
        }
    }

    // Accessor conveniences

    fn ptr(&self) -> *mut Header {
        self.ptr.as_ptr()
    }
    fn header(&self) -> &Header {
        unsafe { self.ptr.as_ref() }
    }
    fn data_raw(&self) -> *mut T {
        // `padding` contains ~static assertions against types that are
        // incompatible with the current feature flags. Even if we don't
        // care about its result, we should always call it before getting
        // a data pointer to guard against invalid types!
        let padding = padding::<T>();

        // Although we ensure the data array is aligned when we allocate,
        // we can't do that with the empty singleton. So when it might not
        // be properly aligned, we substitute in the NonNull::dangling
        // which *is* aligned.
        //
        // To minimize dynamic branches on `cap` for all accesses
        // to the data, we include this guard which should only involve
        // compile-time constants. Ideally this should result in the branch
        // only be included for types with excessive alignment.
        let empty_header_is_aligned = if cfg!(feature = "gecko-ffi") {
            // in gecko-ffi mode `padding` will ensure this under
            // the assumption that the header has size 8 and the
            // static empty singleton is aligned to 8.
            true
        } else {
            // In non-gecko-ffi mode, the empty singleton is just
            // naturally aligned to the Header. If the Header is at
            // least as aligned as T *and* the padding would have
            // been 0, then one-past-the-end of the empty singleton
            // *is* a valid data pointer and we can remove the
            // `dangling` special case.
            mem::align_of::<Header>() >= mem::align_of::<T>() && padding == 0
        };

        unsafe {
            if !empty_header_is_aligned && self.header().cap() == 0 {
                NonNull::dangling().as_ptr()
            } else {
                // This could technically result in overflow, but padding
                // would have to be absurdly large for this to occur.
                let header_size = mem::size_of::<Header>();
                let ptr = self.ptr.as_ptr() as *mut u8;
                ptr.add(header_size + padding) as *mut T
            }
        }
    }

    // This is unsafe when the header is EMPTY_HEADER.
    unsafe fn header_mut(&mut self) -> &mut Header {
        &mut *self.ptr()
    }

    /// Returns the number of elements in the vector, also referred to
    /// as its 'length'.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::thin_vec;
    ///
    /// let a = thin_vec![1, 2, 3];
    /// assert_eq!(a.len(), 3);
    /// ```
    pub fn len(&self) -> usize {
        self.header().len()
    }

    /// Returns `true` if the vector contains no elements.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::ThinVec;
    ///
    /// let mut v = ThinVec::new();
    /// assert!(v.is_empty());
    ///
    /// v.push(1);
    /// assert!(!v.is_empty());
    /// ```
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Returns the number of elements the vector can hold without
    /// reallocating.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::ThinVec;
    ///
    /// let vec: ThinVec<i32> = ThinVec::with_capacity(10);
    /// assert_eq!(vec.capacity(), 10);
    /// ```
    pub fn capacity(&self) -> usize {
        self.header().cap()
    }

    /// Forces the length of the vector to `new_len`.
    ///
    /// This is a low-level operation that maintains none of the normal
    /// invariants of the type. Normally changing the length of a vector
    /// is done using one of the safe operations instead, such as
    /// [`truncate`], [`resize`], [`extend`], or [`clear`].
    ///
    /// [`truncate`]: ThinVec::truncate
    /// [`resize`]: ThinVec::resize
    /// [`extend`]: ThinVec::extend
    /// [`clear`]: ThinVec::clear
    ///
    /// # Safety
    ///
    /// - `new_len` must be less than or equal to [`capacity()`].
    /// - The elements at `old_len..new_len` must be initialized.
    ///
    /// [`capacity()`]: ThinVec::capacity
    ///
    /// # Examples
    ///
    /// This method can be useful for situations in which the vector
    /// is serving as a buffer for other code, particularly over FFI:
    ///
    /// ```no_run
    /// use thin_vec::ThinVec;
    ///
    /// # // This is just a minimal skeleton for the doc example;
    /// # // don't use this as a starting point for a real library.
    /// # pub struct StreamWrapper { strm: *mut std::ffi::c_void }
    /// # const Z_OK: i32 = 0;
    /// # extern "C" {
    /// #     fn deflateGetDictionary(
    /// #         strm: *mut std::ffi::c_void,
    /// #         dictionary: *mut u8,
    /// #         dictLength: *mut usize,
    /// #     ) -> i32;
    /// # }
    /// # impl StreamWrapper {
    /// pub fn get_dictionary(&self) -> Option<ThinVec<u8>> {
    ///     // Per the FFI method's docs, "32768 bytes is always enough".
    ///     let mut dict = ThinVec::with_capacity(32_768);
    ///     let mut dict_length = 0;
    ///     // SAFETY: When `deflateGetDictionary` returns `Z_OK`, it holds that:
    ///     // 1. `dict_length` elements were initialized.
    ///     // 2. `dict_length` <= the capacity (32_768)
    ///     // which makes `set_len` safe to call.
    ///     unsafe {
    ///         // Make the FFI call...
    ///         let r = deflateGetDictionary(self.strm, dict.as_mut_ptr(), &mut dict_length);
    ///         if r == Z_OK {
    ///             // ...and update the length to what was initialized.
    ///             dict.set_len(dict_length);
    ///             Some(dict)
    ///         } else {
    ///             None
    ///         }
    ///     }
    /// }
    /// # }
    /// ```
    ///
    /// While the following example is sound, there is a memory leak since
    /// the inner vectors were not freed prior to the `set_len` call:
    ///
    /// ```no_run
    /// use thin_vec::thin_vec;
    ///
    /// let mut vec = thin_vec![thin_vec![1, 0, 0],
    ///                    thin_vec![0, 1, 0],
    ///                    thin_vec![0, 0, 1]];
    /// // SAFETY:
    /// // 1. `old_len..0` is empty so no elements need to be initialized.
    /// // 2. `0 <= capacity` always holds whatever `capacity` is.
    /// unsafe {
    ///     vec.set_len(0);
    /// }
    /// ```
    ///
    /// Normally, here, one would use [`clear`] instead to correctly drop
    /// the contents and thus not leak memory.
    pub unsafe fn set_len(&mut self, len: usize) {
        if self.is_singleton() {
            // A prerequisite of `Vec::set_len` is that `new_len` must be
            // less than or equal to capacity(). The same applies here.
            assert!(len == 0, "invalid set_len({}) on empty ThinVec", len);
        } else {
            self.header_mut().set_len(len)
        }
    }

    // For internal use only, when setting the length and it's known to be the non-singleton.
    unsafe fn set_len_non_singleton(&mut self, len: usize) {
        self.header_mut().set_len(len)
    }

    /// Appends an element to the back of a collection.
    ///
    /// # Panics
    ///
    /// Panics if the new capacity exceeds `isize::MAX` bytes.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::thin_vec;
    ///
    /// let mut vec = thin_vec![1, 2];
    /// vec.push(3);
    /// assert_eq!(vec, [1, 2, 3]);
    /// ```
    pub fn push(&mut self, val: T) {
        let old_len = self.len();
        if old_len == self.capacity() {
            self.reserve(1);
        }
        unsafe {
            ptr::write(self.data_raw().add(old_len), val);
            self.set_len_non_singleton(old_len + 1);
        }
    }

    /// Removes the last element from a vector and returns it, or [`None`] if it
    /// is empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::thin_vec;
    ///
    /// let mut vec = thin_vec![1, 2, 3];
    /// assert_eq!(vec.pop(), Some(3));
    /// assert_eq!(vec, [1, 2]);
    /// ```
    pub fn pop(&mut self) -> Option<T> {
        let old_len = self.len();
        if old_len == 0 {
            return None;
        }

        unsafe {
            self.set_len_non_singleton(old_len - 1);
            Some(ptr::read(self.data_raw().add(old_len - 1)))
        }
    }

    /// Inserts an element at position `index` within the vector, shifting all
    /// elements after it to the right.
    ///
    /// # Panics
    ///
    /// Panics if `index > len`.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::thin_vec;
    ///
    /// let mut vec = thin_vec![1, 2, 3];
    /// vec.insert(1, 4);
    /// assert_eq!(vec, [1, 4, 2, 3]);
    /// vec.insert(4, 5);
    /// assert_eq!(vec, [1, 4, 2, 3, 5]);
    /// ```
    pub fn insert(&mut self, idx: usize, elem: T) {
        let old_len = self.len();

        assert!(idx <= old_len, "Index out of bounds");
        if old_len == self.capacity() {
            self.reserve(1);
        }
        unsafe {
            let ptr = self.data_raw();
            ptr::copy(ptr.add(idx), ptr.add(idx + 1), old_len - idx);
            ptr::write(ptr.add(idx), elem);
            self.set_len_non_singleton(old_len + 1);
        }
    }

    /// Removes and returns the element at position `index` within the vector,
    /// shifting all elements after it to the left.
    ///
    /// Note: Because this shifts over the remaining elements, it has a
    /// worst-case performance of *O*(*n*). If you don't need the order of elements
    /// to be preserved, use [`swap_remove`] instead. If you'd like to remove
    /// elements from the beginning of the `ThinVec`, consider using `std::collections::VecDeque`.
    ///
    /// [`swap_remove`]: ThinVec::swap_remove
    ///
    /// # Panics
    ///
    /// Panics if `index` is out of bounds.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::thin_vec;
    ///
    /// let mut v = thin_vec![1, 2, 3];
    /// assert_eq!(v.remove(1), 2);
    /// assert_eq!(v, [1, 3]);
    /// ```
    pub fn remove(&mut self, idx: usize) -> T {
        let old_len = self.len();

        assert!(idx < old_len, "Index out of bounds");

        unsafe {
            self.set_len_non_singleton(old_len - 1);
            let ptr = self.data_raw();
            let val = ptr::read(self.data_raw().add(idx));
            ptr::copy(ptr.add(idx + 1), ptr.add(idx), old_len - idx - 1);
            val
        }
    }

    /// Removes an element from the vector and returns it.
    ///
    /// The removed element is replaced by the last element of the vector.
    ///
    /// This does not preserve ordering, but is *O*(1).
    /// If you need to preserve the element order, use [`remove`] instead.
    ///
    /// [`remove`]: ThinVec::remove
    ///
    /// # Panics
    ///
    /// Panics if `index` is out of bounds.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::thin_vec;
    ///
    /// let mut v = thin_vec!["foo", "bar", "baz", "qux"];
    ///
    /// assert_eq!(v.swap_remove(1), "bar");
    /// assert_eq!(v, ["foo", "qux", "baz"]);
    ///
    /// assert_eq!(v.swap_remove(0), "foo");
    /// assert_eq!(v, ["baz", "qux"]);
    /// ```
    pub fn swap_remove(&mut self, idx: usize) -> T {
        let old_len = self.len();

        assert!(idx < old_len, "Index out of bounds");

        unsafe {
            let ptr = self.data_raw();
            ptr::swap(ptr.add(idx), ptr.add(old_len - 1));
            self.set_len_non_singleton(old_len - 1);
            ptr::read(ptr.add(old_len - 1))
        }
    }

    /// Shortens the vector, keeping the first `len` elements and dropping
    /// the rest.
    ///
    /// If `len` is greater than the vector's current length, this has no
    /// effect.
    ///
    /// The [`drain`] method can emulate `truncate`, but causes the excess
    /// elements to be returned instead of dropped.
    ///
    /// Note that this method has no effect on the allocated capacity
    /// of the vector.
    ///
    /// # Examples
    ///
    /// Truncating a five element vector to two elements:
    ///
    /// ```
    /// use thin_vec::thin_vec;
    ///
    /// let mut vec = thin_vec![1, 2, 3, 4, 5];
    /// vec.truncate(2);
    /// assert_eq!(vec, [1, 2]);
    /// ```
    ///
    /// No truncation occurs when `len` is greater than the vector's current
    /// length:
    ///
    /// ```
    /// use thin_vec::thin_vec;
    ///
    /// let mut vec = thin_vec![1, 2, 3];
    /// vec.truncate(8);
    /// assert_eq!(vec, [1, 2, 3]);
    /// ```
    ///
    /// Truncating when `len == 0` is equivalent to calling the [`clear`]
    /// method.
    ///
    /// ```
    /// use thin_vec::thin_vec;
    ///
    /// let mut vec = thin_vec![1, 2, 3];
    /// vec.truncate(0);
    /// assert_eq!(vec, []);
    /// ```
    ///
    /// [`clear`]: ThinVec::clear
    /// [`drain`]: ThinVec::drain
    pub fn truncate(&mut self, len: usize) {
        unsafe {
            // drop any extra elements
            while len < self.len() {
                // decrement len before the drop_in_place(), so a panic on Drop
                // doesn't re-drop the just-failed value.
                let new_len = self.len() - 1;
                self.set_len_non_singleton(new_len);
                ptr::drop_in_place(self.data_raw().add(new_len));
            }
        }
    }

    /// Clears the vector, removing all values.
    ///
    /// Note that this method has no effect on the allocated capacity
    /// of the vector.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::thin_vec;
    ///
    /// let mut v = thin_vec![1, 2, 3];
    /// v.clear();
    /// assert!(v.is_empty());
    /// ```
    pub fn clear(&mut self) {
        unsafe {
            ptr::drop_in_place(&mut self[..]);
            self.set_len(0); // could be the singleton
        }
    }

    /// Extracts a slice containing the entire vector.
    ///
    /// Equivalent to `&s[..]`.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::thin_vec;
    /// use std::io::{self, Write};
    /// let buffer = thin_vec![1, 2, 3, 5, 8];
    /// io::sink().write(buffer.as_slice()).unwrap();
    /// ```
    pub fn as_slice(&self) -> &[T] {
        unsafe { slice::from_raw_parts(self.data_raw(), self.len()) }
    }

    /// Extracts a mutable slice of the entire vector.
    ///
    /// Equivalent to `&mut s[..]`.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::thin_vec;
    /// use std::io::{self, Read};
    /// let mut buffer = vec![0; 3];
    /// io::repeat(0b101).read_exact(buffer.as_mut_slice()).unwrap();
    /// ```
    pub fn as_mut_slice(&mut self) -> &mut [T] {
        unsafe { slice::from_raw_parts_mut(self.data_raw(), self.len()) }
    }

    /// Reserve capacity for at least `additional` more elements to be inserted.
    ///
    /// May reserve more space than requested, to avoid frequent reallocations.
    ///
    /// Panics if the new capacity overflows `usize`.
    ///
    /// Re-allocates only if `self.capacity() < self.len() + additional`.
    #[cfg(not(feature = "gecko-ffi"))]
    pub fn reserve(&mut self, additional: usize) {
        let len = self.len();
        let old_cap = self.capacity();
        let min_cap = len.checked_add(additional).expect("capacity overflow");
        if min_cap <= old_cap {
            return;
        }
        // Ensure the new capacity is at least double, to guarantee exponential growth.
        let double_cap = if old_cap == 0 {
            // skip to 4 because tiny ThinVecs are dumb; but not if that would cause overflow
            if mem::size_of::<T>() > (!0) / 8 {
                1
            } else {
                4
            }
        } else {
            old_cap.saturating_mul(2)
        };
        let new_cap = max(min_cap, double_cap);
        unsafe {
            self.reallocate(new_cap);
        }
    }

    /// Reserve capacity for at least `additional` more elements to be inserted.
    ///
    /// This method mimics the growth algorithm used by the C++ implementation
    /// of nsTArray.
    #[cfg(feature = "gecko-ffi")]
    pub fn reserve(&mut self, additional: usize) {
        let elem_size = mem::size_of::<T>();

        let len = self.len();
        let old_cap = self.capacity();
        let min_cap = len.checked_add(additional).expect("capacity overflow");
        if min_cap <= old_cap {
            return;
        }

        // The growth logic can't handle zero-sized types, so we have to exit
        // early here.
        if elem_size == 0 {
            unsafe {
                self.reallocate(min_cap);
            }
            return;
        }

        let min_cap_bytes = assert_size(min_cap)
            .checked_mul(assert_size(elem_size))
            .and_then(|x| x.checked_add(assert_size(mem::size_of::<Header>())))
            .unwrap();

        // Perform some checked arithmetic to ensure all of the numbers we
        // compute will end up in range.
        let will_fit = min_cap_bytes.checked_mul(2).is_some();
        if !will_fit {
            panic!("Exceeded maximum nsTArray size");
        }

        const SLOW_GROWTH_THRESHOLD: usize = 8 * 1024 * 1024;

        let bytes = if min_cap > SLOW_GROWTH_THRESHOLD {
            // Grow by a minimum of 1.125x
            let old_cap_bytes = old_cap * elem_size + mem::size_of::<Header>();
            let min_growth = old_cap_bytes + (old_cap_bytes >> 3);
            let growth = max(min_growth, min_cap_bytes as usize);

            // Round up to the next megabyte.
            const MB: usize = 1 << 20;
            MB * ((growth + MB - 1) / MB)
        } else {
            // Try to allocate backing buffers in powers of two.
            min_cap_bytes.next_power_of_two() as usize
        };

        let cap = (bytes - std::mem::size_of::<Header>()) / elem_size;
        unsafe {
            self.reallocate(cap);
        }
    }

    /// Reserves the minimum capacity for `additional` more elements to be inserted.
    ///
    /// Panics if the new capacity overflows `usize`.
    ///
    /// Re-allocates only if `self.capacity() < self.len() + additional`.
    pub fn reserve_exact(&mut self, additional: usize) {
        let new_cap = self
            .len()
            .checked_add(additional)
            .expect("capacity overflow");
        let old_cap = self.capacity();
        if new_cap > old_cap {
            unsafe {
                self.reallocate(new_cap);
            }
        }
    }

    /// Shrinks the capacity of the vector as much as possible.
    ///
    /// It will drop down as close as possible to the length but the allocator
    /// may still inform the vector that there is space for a few more elements.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::ThinVec;
    ///
    /// let mut vec = ThinVec::with_capacity(10);
    /// vec.extend([1, 2, 3]);
    /// assert_eq!(vec.capacity(), 10);
    /// vec.shrink_to_fit();
    /// assert!(vec.capacity() >= 3);
    /// ```
    pub fn shrink_to_fit(&mut self) {
        let old_cap = self.capacity();
        let new_cap = self.len();
        if new_cap < old_cap {
            if new_cap == 0 {
                *self = ThinVec::new();
            } else {
                unsafe {
                    self.reallocate(new_cap);
                }
            }
        }
    }

    /// Retains only the elements specified by the predicate.
    ///
    /// In other words, remove all elements `e` such that `f(&e)` returns `false`.
    /// This method operates in place and preserves the order of the retained
    /// elements.
    ///
    /// # Examples
    ///
    // A hack to avoid linking problems with `cargo test --features=gecko-ffi`.
    #[cfg_attr(not(feature = "gecko-ffi"), doc = "```")]
    #[cfg_attr(feature = "gecko-ffi", doc = "```ignore")]
    /// # #[macro_use] extern crate thin_vec;
    /// # fn main() {
    /// let mut vec = thin_vec![1, 2, 3, 4];
    /// vec.retain(|&x| x%2 == 0);
    /// assert_eq!(vec, [2, 4]);
    /// # }
    /// ```
    pub fn retain<F>(&mut self, mut f: F)
    where
        F: FnMut(&T) -> bool,
    {
        self.retain_mut(|x| f(&*x));
    }

    /// Retains only the elements specified by the predicate, passing a mutable reference to it.
    ///
    /// In other words, remove all elements `e` such that `f(&mut e)` returns `false`.
    /// This method operates in place and preserves the order of the retained
    /// elements.
    ///
    /// # Examples
    ///
    // A hack to avoid linking problems with `cargo test --features=gecko-ffi`.
    #[cfg_attr(not(feature = "gecko-ffi"), doc = "```")]
    #[cfg_attr(feature = "gecko-ffi", doc = "```ignore")]
    /// # #[macro_use] extern crate thin_vec;
    /// # fn main() {
    /// let mut vec = thin_vec![1, 2, 3, 4, 5];
    /// vec.retain_mut(|x| {
    ///     *x += 1;
    ///     (*x)%2 == 0
    /// });
    /// assert_eq!(vec, [2, 4, 6]);
    /// # }
    /// ```
    pub fn retain_mut<F>(&mut self, mut f: F)
    where
        F: FnMut(&mut T) -> bool,
    {
        let len = self.len();
        let mut del = 0;
        {
            let v = &mut self[..];

            for i in 0..len {
                if !f(&mut v[i]) {
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

    /// Removes consecutive elements in the vector that resolve to the same key.
    ///
    /// If the vector is sorted, this removes all duplicates.
    ///
    /// # Examples
    ///
    // A hack to avoid linking problems with `cargo test --features=gecko-ffi`.
    #[cfg_attr(not(feature = "gecko-ffi"), doc = "```")]
    #[cfg_attr(feature = "gecko-ffi", doc = "```ignore")]
    /// # #[macro_use] extern crate thin_vec;
    /// # fn main() {
    /// let mut vec = thin_vec![10, 20, 21, 30, 20];
    ///
    /// vec.dedup_by_key(|i| *i / 10);
    ///
    /// assert_eq!(vec, [10, 20, 30, 20]);
    /// # }
    /// ```
    pub fn dedup_by_key<F, K>(&mut self, mut key: F)
    where
        F: FnMut(&mut T) -> K,
        K: PartialEq<K>,
    {
        self.dedup_by(|a, b| key(a) == key(b))
    }

    /// Removes consecutive elements in the vector according to a predicate.
    ///
    /// The `same_bucket` function is passed references to two elements from the vector, and
    /// returns `true` if the elements compare equal, or `false` if they do not. Only the first
    /// of adjacent equal items is kept.
    ///
    /// If the vector is sorted, this removes all duplicates.
    ///
    /// # Examples
    ///
    // A hack to avoid linking problems with `cargo test --features=gecko-ffi`.
    #[cfg_attr(not(feature = "gecko-ffi"), doc = "```")]
    #[cfg_attr(feature = "gecko-ffi", doc = "```ignore")]
    /// # #[macro_use] extern crate thin_vec;
    /// # fn main() {
    /// let mut vec = thin_vec!["foo", "bar", "Bar", "baz", "bar"];
    ///
    /// vec.dedup_by(|a, b| a.eq_ignore_ascii_case(b));
    ///
    /// assert_eq!(vec, ["foo", "bar", "baz", "bar"]);
    /// # }
    /// ```
    #[allow(clippy::swap_ptr_to_ref)]
    pub fn dedup_by<F>(&mut self, mut same_bucket: F)
    where
        F: FnMut(&mut T, &mut T) -> bool,
    {
        // See the comments in `Vec::dedup` for a detailed explanation of this code.
        unsafe {
            let ln = self.len();
            if ln <= 1 {
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

    /// Splits the collection into two at the given index.
    ///
    /// Returns a newly allocated vector containing the elements in the range
    /// `[at, len)`. After the call, the original vector will be left containing
    /// the elements `[0, at)` with its previous capacity unchanged.
    ///
    /// # Panics
    ///
    /// Panics if `at > len`.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::thin_vec;
    ///
    /// let mut vec = thin_vec![1, 2, 3];
    /// let vec2 = vec.split_off(1);
    /// assert_eq!(vec, [1]);
    /// assert_eq!(vec2, [2, 3]);
    /// ```
    pub fn split_off(&mut self, at: usize) -> ThinVec<T> {
        let old_len = self.len();
        let new_vec_len = old_len - at;

        assert!(at <= old_len, "Index out of bounds");

        unsafe {
            let mut new_vec = ThinVec::with_capacity(new_vec_len);

            ptr::copy_nonoverlapping(self.data_raw().add(at), new_vec.data_raw(), new_vec_len);

            new_vec.set_len(new_vec_len); // could be the singleton
            self.set_len(at); // could be the singleton

            new_vec
        }
    }

    /// Moves all the elements of `other` into `self`, leaving `other` empty.
    ///
    /// # Panics
    ///
    /// Panics if the new capacity exceeds `isize::MAX` bytes.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::thin_vec;
    ///
    /// let mut vec = thin_vec![1, 2, 3];
    /// let mut vec2 = thin_vec![4, 5, 6];
    /// vec.append(&mut vec2);
    /// assert_eq!(vec, [1, 2, 3, 4, 5, 6]);
    /// assert_eq!(vec2, []);
    /// ```
    pub fn append(&mut self, other: &mut ThinVec<T>) {
        self.extend(other.drain(..))
    }

    /// Removes the specified range from the vector in bulk, returning all
    /// removed elements as an iterator. If the iterator is dropped before
    /// being fully consumed, it drops the remaining removed elements.
    ///
    /// The returned iterator keeps a mutable borrow on the vector to optimize
    /// its implementation.
    ///
    /// # Panics
    ///
    /// Panics if the starting point is greater than the end point or if
    /// the end point is greater than the length of the vector.
    ///
    /// # Leaking
    ///
    /// If the returned iterator goes out of scope without being dropped (due to
    /// [`mem::forget`], for example), the vector may have lost and leaked
    /// elements arbitrarily, including elements outside the range.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::{ThinVec, thin_vec};
    ///
    /// let mut v = thin_vec![1, 2, 3];
    /// let u: ThinVec<_> = v.drain(1..).collect();
    /// assert_eq!(v, &[1]);
    /// assert_eq!(u, &[2, 3]);
    ///
    /// // A full range clears the vector, like `clear()` does
    /// v.drain(..);
    /// assert_eq!(v, &[]);
    /// ```
    pub fn drain<R>(&mut self, range: R) -> Drain<'_, T>
    where
        R: RangeBounds<usize>,
    {
        // See comments in the Drain struct itself for details on this
        let len = self.len();
        let start = match range.start_bound() {
            Bound::Included(&n) => n,
            Bound::Excluded(&n) => n + 1,
            Bound::Unbounded => 0,
        };
        let end = match range.end_bound() {
            Bound::Included(&n) => n + 1,
            Bound::Excluded(&n) => n,
            Bound::Unbounded => len,
        };
        assert!(start <= end);
        assert!(end <= len);

        unsafe {
            // Set our length to the start bound
            self.set_len(start); // could be the singleton

            let iter =
                slice::from_raw_parts_mut(self.data_raw().add(start), end - start).iter_mut();

            Drain {
                iter,
                vec: self,
                end,
                tail: len - end,
            }
        }
    }

    /// Creates a splicing iterator that replaces the specified range in the vector
    /// with the given `replace_with` iterator and yields the removed items.
    /// `replace_with` does not need to be the same length as `range`.
    ///
    /// `range` is removed even if the iterator is not consumed until the end.
    ///
    /// It is unspecified how many elements are removed from the vector
    /// if the `Splice` value is leaked.
    ///
    /// The input iterator `replace_with` is only consumed when the `Splice` value is dropped.
    ///
    /// This is optimal if:
    ///
    /// * The tail (elements in the vector after `range`) is empty,
    /// * or `replace_with` yields fewer or equal elements than `range`’s length
    /// * or the lower bound of its `size_hint()` is exact.
    ///
    /// Otherwise, a temporary vector is allocated and the tail is moved twice.
    ///
    /// # Panics
    ///
    /// Panics if the starting point is greater than the end point or if
    /// the end point is greater than the length of the vector.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::{ThinVec, thin_vec};
    ///
    /// let mut v = thin_vec![1, 2, 3, 4];
    /// let new = [7, 8, 9];
    /// let u: ThinVec<_> = v.splice(1..3, new).collect();
    /// assert_eq!(v, &[1, 7, 8, 9, 4]);
    /// assert_eq!(u, &[2, 3]);
    /// ```
    #[inline]
    pub fn splice<R, I>(&mut self, range: R, replace_with: I) -> Splice<'_, I::IntoIter>
    where
        R: RangeBounds<usize>,
        I: IntoIterator<Item = T>,
    {
        Splice {
            drain: self.drain(range),
            replace_with: replace_with.into_iter(),
        }
    }

    /// Resize the buffer and update its capacity, without changing the length.
    /// Unsafe because it can cause length to be greater than capacity.
    unsafe fn reallocate(&mut self, new_cap: usize) {
        debug_assert!(new_cap > 0);
        if self.has_allocation() {
            let old_cap = self.capacity();
            let ptr = realloc(
                self.ptr() as *mut u8,
                layout::<T>(old_cap),
                alloc_size::<T>(new_cap),
            ) as *mut Header;

            if ptr.is_null() {
                handle_alloc_error(layout::<T>(new_cap))
            }
            (*ptr).set_cap(new_cap);
            self.ptr = NonNull::new_unchecked(ptr);
        } else {
            let new_header = header_with_capacity::<T>(new_cap);

            // If we get here and have a non-zero len, then we must be handling
            // a gecko auto array, and we have items in a stack buffer. We shouldn't
            // free it, but we should memcopy the contents out of it and mark it as empty.
            //
            // T is assumed to be trivially relocatable, as this is ~required
            // for Rust compatibility anyway. Furthermore, we assume C++ won't try
            // to unconditionally destroy the contents of the stack allocated buffer
            // (i.e. it's obfuscated behind a union).
            //
            // In effect, we are partially reimplementing the auto array move constructor
            // by leaving behind a valid empty instance.
            let len = self.len();
            if cfg!(feature = "gecko-ffi") && len > 0 {
                new_header
                    .as_ptr()
                    .add(1)
                    .cast::<T>()
                    .copy_from_nonoverlapping(self.data_raw(), len);
                self.set_len_non_singleton(0);
            }

            self.ptr = new_header;
        }
    }

    #[cfg(feature = "gecko-ffi")]
    #[inline]
    #[allow(unused_unsafe)]
    fn is_singleton(&self) -> bool {
        // NOTE: the tests will complain that this "unsafe" isn't needed, but it *IS*!
        // In production this refers to an *extern static* which *is* unsafe to reference.
        // In tests this refers to a local static because we don't have Firefox's codebase
        // providing the symbol!
        unsafe { self.ptr.as_ptr() as *const Header == &EMPTY_HEADER }
    }

    #[cfg(not(feature = "gecko-ffi"))]
    #[inline]
    fn is_singleton(&self) -> bool {
        self.ptr.as_ptr() as *const Header == &EMPTY_HEADER
    }

    #[cfg(feature = "gecko-ffi")]
    #[inline]
    fn has_allocation(&self) -> bool {
        unsafe { !self.is_singleton() && !self.ptr.as_ref().uses_stack_allocated_buffer() }
    }

    #[cfg(not(feature = "gecko-ffi"))]
    #[inline]
    fn has_allocation(&self) -> bool {
        !self.is_singleton()
    }
}

impl<T: Clone> ThinVec<T> {
    /// Resizes the `Vec` in-place so that `len()` is equal to `new_len`.
    ///
    /// If `new_len` is greater than `len()`, the `Vec` is extended by the
    /// difference, with each additional slot filled with `value`.
    /// If `new_len` is less than `len()`, the `Vec` is simply truncated.
    ///
    /// # Examples
    ///
    // A hack to avoid linking problems with `cargo test --features=gecko-ffi`.
    #[cfg_attr(not(feature = "gecko-ffi"), doc = "```")]
    #[cfg_attr(feature = "gecko-ffi", doc = "```ignore")]
    /// # #[macro_use] extern crate thin_vec;
    /// # fn main() {
    /// let mut vec = thin_vec!["hello"];
    /// vec.resize(3, "world");
    /// assert_eq!(vec, ["hello", "world", "world"]);
    ///
    /// let mut vec = thin_vec![1, 2, 3, 4];
    /// vec.resize(2, 0);
    /// assert_eq!(vec, [1, 2]);
    /// # }
    /// ```
    pub fn resize(&mut self, new_len: usize, value: T) {
        let old_len = self.len();

        if new_len > old_len {
            let additional = new_len - old_len;
            self.reserve(additional);
            for _ in 1..additional {
                self.push(value.clone());
            }
            // We can write the last element directly without cloning needlessly
            if additional > 0 {
                self.push(value);
            }
        } else if new_len < old_len {
            self.truncate(new_len);
        }
    }

    /// Clones and appends all elements in a slice to the `ThinVec`.
    ///
    /// Iterates over the slice `other`, clones each element, and then appends
    /// it to this `ThinVec`. The `other` slice is traversed in-order.
    ///
    /// Note that this function is same as [`extend`] except that it is
    /// specialized to work with slices instead. If and when Rust gets
    /// specialization this function will likely be deprecated (but still
    /// available).
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::thin_vec;
    ///
    /// let mut vec = thin_vec![1];
    /// vec.extend_from_slice(&[2, 3, 4]);
    /// assert_eq!(vec, [1, 2, 3, 4]);
    /// ```
    ///
    /// [`extend`]: ThinVec::extend
    pub fn extend_from_slice(&mut self, other: &[T]) {
        self.extend(other.iter().cloned())
    }
}

impl<T: PartialEq> ThinVec<T> {
    /// Removes consecutive repeated elements in the vector.
    ///
    /// If the vector is sorted, this removes all duplicates.
    ///
    /// # Examples
    ///
    // A hack to avoid linking problems with `cargo test --features=gecko-ffi`.
    #[cfg_attr(not(feature = "gecko-ffi"), doc = "```")]
    #[cfg_attr(feature = "gecko-ffi", doc = "```ignore")]
    /// # #[macro_use] extern crate thin_vec;
    /// # fn main() {
    /// let mut vec = thin_vec![1, 2, 2, 3, 2];
    ///
    /// vec.dedup();
    ///
    /// assert_eq!(vec, [1, 2, 3, 2]);
    /// # }
    /// ```
    pub fn dedup(&mut self) {
        self.dedup_by(|a, b| a == b)
    }
}

impl<T> Drop for ThinVec<T> {
    #[inline]
    fn drop(&mut self) {
        #[cold]
        #[inline(never)]
        fn drop_non_singleton<T>(this: &mut ThinVec<T>) {
            unsafe {
                ptr::drop_in_place(&mut this[..]);

                #[cfg(feature = "gecko-ffi")]
                if this.ptr.as_ref().uses_stack_allocated_buffer() {
                    return;
                }

                dealloc(this.ptr() as *mut u8, layout::<T>(this.capacity()))
            }
        }

        if !self.is_singleton() {
            drop_non_singleton(self);
        }
    }
}

impl<T> Deref for ThinVec<T> {
    type Target = [T];

    fn deref(&self) -> &[T] {
        self.as_slice()
    }
}

impl<T> DerefMut for ThinVec<T> {
    fn deref_mut(&mut self) -> &mut [T] {
        self.as_mut_slice()
    }
}

impl<T> Borrow<[T]> for ThinVec<T> {
    fn borrow(&self) -> &[T] {
        self.as_slice()
    }
}

impl<T> BorrowMut<[T]> for ThinVec<T> {
    fn borrow_mut(&mut self) -> &mut [T] {
        self.as_mut_slice()
    }
}

impl<T> AsRef<[T]> for ThinVec<T> {
    fn as_ref(&self) -> &[T] {
        self.as_slice()
    }
}

impl<T> Extend<T> for ThinVec<T> {
    #[inline]
    fn extend<I>(&mut self, iter: I)
    where
        I: IntoIterator<Item = T>,
    {
        let iter = iter.into_iter();
        let hint = iter.size_hint().0;
        if hint > 0 {
            self.reserve(hint);
        }
        for x in iter {
            self.push(x);
        }
    }
}

impl<T: fmt::Debug> fmt::Debug for ThinVec<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(&**self, f)
    }
}

impl<T> Hash for ThinVec<T>
where
    T: Hash,
{
    fn hash<H>(&self, state: &mut H)
    where
        H: Hasher,
    {
        self[..].hash(state);
    }
}

impl<T> PartialOrd for ThinVec<T>
where
    T: PartialOrd,
{
    #[inline]
    fn partial_cmp(&self, other: &ThinVec<T>) -> Option<Ordering> {
        self[..].partial_cmp(&other[..])
    }
}

impl<T> Ord for ThinVec<T>
where
    T: Ord,
{
    #[inline]
    fn cmp(&self, other: &ThinVec<T>) -> Ordering {
        self[..].cmp(&other[..])
    }
}

impl<A, B> PartialEq<ThinVec<B>> for ThinVec<A>
where
    A: PartialEq<B>,
{
    #[inline]
    fn eq(&self, other: &ThinVec<B>) -> bool {
        self[..] == other[..]
    }
}

impl<A, B> PartialEq<Vec<B>> for ThinVec<A>
where
    A: PartialEq<B>,
{
    #[inline]
    fn eq(&self, other: &Vec<B>) -> bool {
        self[..] == other[..]
    }
}

impl<A, B> PartialEq<[B]> for ThinVec<A>
where
    A: PartialEq<B>,
{
    #[inline]
    fn eq(&self, other: &[B]) -> bool {
        self[..] == other[..]
    }
}

impl<'a, A, B> PartialEq<&'a [B]> for ThinVec<A>
where
    A: PartialEq<B>,
{
    #[inline]
    fn eq(&self, other: &&'a [B]) -> bool {
        self[..] == other[..]
    }
}

// Serde impls based on
// https://github.com/bluss/arrayvec/blob/67ec907a98c0f40c4b76066fed3c1af59d35cf6a/src/arrayvec.rs#L1222-L1267
#[cfg(feature = "serde")]
impl<T: serde::Serialize> serde::Serialize for ThinVec<T> {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.collect_seq(self.as_slice())
    }
}

#[cfg(feature = "serde")]
impl<'de, T: serde::Deserialize<'de>> serde::Deserialize<'de> for ThinVec<T> {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        use serde::de::{SeqAccess, Visitor};
        use serde::Deserialize;

        struct ThinVecVisitor<T>(PhantomData<T>);

        impl<'de, T: Deserialize<'de>> Visitor<'de> for ThinVecVisitor<T> {
            type Value = ThinVec<T>;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                write!(formatter, "a sequence")
            }

            fn visit_seq<SA>(self, mut seq: SA) -> Result<Self::Value, SA::Error>
            where
                SA: SeqAccess<'de>,
            {
                // Same policy as
                // https://github.com/serde-rs/serde/blob/ce0844b9ecc32377b5e4545d759d385a8c46bc6a/serde/src/private/size_hint.rs#L13
                let initial_capacity = seq.size_hint().unwrap_or_default().min(4096);
                let mut values = ThinVec::<T>::with_capacity(initial_capacity);

                while let Some(value) = seq.next_element()? {
                    values.push(value);
                }

                Ok(values)
            }
        }

        deserializer.deserialize_seq(ThinVecVisitor::<T>(PhantomData))
    }
}

macro_rules! array_impls {
    ($($N:expr)*) => {$(
        impl<A, B> PartialEq<[B; $N]> for ThinVec<A> where A: PartialEq<B> {
            #[inline]
            fn eq(&self, other: &[B; $N]) -> bool { self[..] == other[..] }
        }

        impl<'a, A, B> PartialEq<&'a [B; $N]> for ThinVec<A> where A: PartialEq<B> {
            #[inline]
            fn eq(&self, other: &&'a [B; $N]) -> bool { self[..] == other[..] }
        }
    )*}
}

array_impls! {
    0  1  2  3  4  5  6  7  8  9
    10 11 12 13 14 15 16 17 18 19
    20 21 22 23 24 25 26 27 28 29
    30 31 32
}

impl<T> Eq for ThinVec<T> where T: Eq {}

impl<T> IntoIterator for ThinVec<T> {
    type Item = T;
    type IntoIter = IntoIter<T>;

    fn into_iter(self) -> IntoIter<T> {
        IntoIter {
            vec: self,
            start: 0,
        }
    }
}

impl<'a, T> IntoIterator for &'a ThinVec<T> {
    type Item = &'a T;
    type IntoIter = slice::Iter<'a, T>;

    fn into_iter(self) -> slice::Iter<'a, T> {
        self.iter()
    }
}

impl<'a, T> IntoIterator for &'a mut ThinVec<T> {
    type Item = &'a mut T;
    type IntoIter = slice::IterMut<'a, T>;

    fn into_iter(self) -> slice::IterMut<'a, T> {
        self.iter_mut()
    }
}

impl<T> Clone for ThinVec<T>
where
    T: Clone,
{
    #[inline]
    fn clone(&self) -> ThinVec<T> {
        #[cold]
        #[inline(never)]
        fn clone_non_singleton<T: Clone>(this: &ThinVec<T>) -> ThinVec<T> {
            let len = this.len();
            let mut new_vec = ThinVec::<T>::with_capacity(len);
            let mut data_raw = new_vec.data_raw();
            for x in this.iter() {
                unsafe {
                    ptr::write(data_raw, x.clone());
                    data_raw = data_raw.add(1);
                }
            }
            unsafe {
                // `this` is not the singleton, but `new_vec` will be if
                // `this` is empty.
                new_vec.set_len(len); // could be the singleton
            }
            new_vec
        }

        if self.is_singleton() {
            ThinVec::new()
        } else {
            clone_non_singleton(self)
        }
    }
}

impl<T> Default for ThinVec<T> {
    fn default() -> ThinVec<T> {
        ThinVec::new()
    }
}

impl<T> FromIterator<T> for ThinVec<T> {
    #[inline]
    fn from_iter<I: IntoIterator<Item = T>>(iter: I) -> ThinVec<T> {
        let mut vec = ThinVec::new();
        vec.extend(iter.into_iter());
        vec
    }
}

impl<T: Clone> From<&[T]> for ThinVec<T> {
    /// Allocate a `ThinVec<T>` and fill it by cloning `s`'s items.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::{ThinVec, thin_vec};
    ///
    /// assert_eq!(ThinVec::from(&[1, 2, 3][..]), thin_vec![1, 2, 3]);
    /// ```
    fn from(s: &[T]) -> ThinVec<T> {
        s.iter().cloned().collect()
    }
}

#[cfg(not(no_global_oom_handling))]
impl<T: Clone> From<&mut [T]> for ThinVec<T> {
    /// Allocate a `ThinVec<T>` and fill it by cloning `s`'s items.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::{ThinVec, thin_vec};
    ///
    /// assert_eq!(ThinVec::from(&mut [1, 2, 3][..]), thin_vec![1, 2, 3]);
    /// ```
    fn from(s: &mut [T]) -> ThinVec<T> {
        s.iter().cloned().collect()
    }
}

impl<T, const N: usize> From<[T; N]> for ThinVec<T> {
    /// Allocate a `ThinVec<T>` and move `s`'s items into it.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::{ThinVec, thin_vec};
    ///
    /// assert_eq!(ThinVec::from([1, 2, 3]), thin_vec![1, 2, 3]);
    /// ```
    fn from(s: [T; N]) -> ThinVec<T> {
        std::iter::IntoIterator::into_iter(s).collect()
    }
}

impl<T> From<Box<[T]>> for ThinVec<T> {
    /// Convert a boxed slice into a vector by transferring ownership of
    /// the existing heap allocation.
    ///
    /// **NOTE:** unlike `std`, this must reallocate to change the layout!
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::{ThinVec, thin_vec};
    ///
    /// let b: Box<[i32]> = thin_vec![1, 2, 3].into_iter().collect();
    /// assert_eq!(ThinVec::from(b), thin_vec![1, 2, 3]);
    /// ```
    fn from(s: Box<[T]>) -> Self {
        // Can just lean on the fact that `Box<[T]>` -> `Vec<T>` is Free.
        Vec::from(s).into_iter().collect()
    }
}

impl<T> From<Vec<T>> for ThinVec<T> {
    /// Convert a `std::Vec` into a `ThinVec`.
    ///
    /// **NOTE:** this must reallocate to change the layout!
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::{ThinVec, thin_vec};
    ///
    /// let b: Vec<i32> = vec![1, 2, 3];
    /// assert_eq!(ThinVec::from(b), thin_vec![1, 2, 3]);
    /// ```
    fn from(s: Vec<T>) -> Self {
        s.into_iter().collect()
    }
}

impl<T> From<ThinVec<T>> for Vec<T> {
    /// Convert a `ThinVec` into a `std::Vec`.
    ///
    /// **NOTE:** this must reallocate to change the layout!
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::{ThinVec, thin_vec};
    ///
    /// let b: ThinVec<i32> = thin_vec![1, 2, 3];
    /// assert_eq!(Vec::from(b), vec![1, 2, 3]);
    /// ```
    fn from(s: ThinVec<T>) -> Self {
        s.into_iter().collect()
    }
}

impl<T> From<ThinVec<T>> for Box<[T]> {
    /// Convert a vector into a boxed slice.
    ///
    /// If `v` has excess capacity, its items will be moved into a
    /// newly-allocated buffer with exactly the right capacity.
    ///
    /// **NOTE:** unlike `std`, this must reallocate to change the layout!
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::{ThinVec, thin_vec};
    /// assert_eq!(Box::from(thin_vec![1, 2, 3]), thin_vec![1, 2, 3].into_iter().collect());
    /// ```
    fn from(v: ThinVec<T>) -> Self {
        v.into_iter().collect()
    }
}

impl From<&str> for ThinVec<u8> {
    /// Allocate a `ThinVec<u8>` and fill it with a UTF-8 string.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::{ThinVec, thin_vec};
    ///
    /// assert_eq!(ThinVec::from("123"), thin_vec![b'1', b'2', b'3']);
    /// ```
    fn from(s: &str) -> ThinVec<u8> {
        From::from(s.as_bytes())
    }
}

impl<T, const N: usize> TryFrom<ThinVec<T>> for [T; N] {
    type Error = ThinVec<T>;

    /// Gets the entire contents of the `ThinVec<T>` as an array,
    /// if its size exactly matches that of the requested array.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::{ThinVec, thin_vec};
    /// use std::convert::TryInto;
    ///
    /// assert_eq!(thin_vec![1, 2, 3].try_into(), Ok([1, 2, 3]));
    /// assert_eq!(<ThinVec<i32>>::new().try_into(), Ok([]));
    /// ```
    ///
    /// If the length doesn't match, the input comes back in `Err`:
    /// ```
    /// use thin_vec::{ThinVec, thin_vec};
    /// use std::convert::TryInto;
    ///
    /// let r: Result<[i32; 4], _> = (0..10).collect::<ThinVec<_>>().try_into();
    /// assert_eq!(r, Err(thin_vec![0, 1, 2, 3, 4, 5, 6, 7, 8, 9]));
    /// ```
    ///
    /// If you're fine with just getting a prefix of the `ThinVec<T>`,
    /// you can call [`.truncate(N)`](ThinVec::truncate) first.
    /// ```
    /// use thin_vec::{ThinVec, thin_vec};
    /// use std::convert::TryInto;
    ///
    /// let mut v = ThinVec::from("hello world");
    /// v.sort();
    /// v.truncate(2);
    /// let [a, b]: [_; 2] = v.try_into().unwrap();
    /// assert_eq!(a, b' ');
    /// assert_eq!(b, b'd');
    /// ```
    fn try_from(mut vec: ThinVec<T>) -> Result<[T; N], ThinVec<T>> {
        if vec.len() != N {
            return Err(vec);
        }

        // SAFETY: `.set_len(0)` is always sound.
        unsafe { vec.set_len(0) };

        // SAFETY: A `ThinVec`'s pointer is always aligned properly, and
        // the alignment the array needs is the same as the items.
        // We checked earlier that we have sufficient items.
        // The items will not double-drop as the `set_len`
        // tells the `ThinVec` not to also drop them.
        let array = unsafe { ptr::read(vec.data_raw() as *const [T; N]) };
        Ok(array)
    }
}

/// An iterator that moves out of a vector.
///
/// This `struct` is created by the [`ThinVec::into_iter`][]
/// (provided by the [`IntoIterator`] trait).
///
/// # Example
///
/// ```
/// use thin_vec::thin_vec;
///
/// let v = thin_vec![0, 1, 2];
/// let iter: thin_vec::IntoIter<_> = v.into_iter();
/// ```
pub struct IntoIter<T> {
    vec: ThinVec<T>,
    start: usize,
}

impl<T> IntoIter<T> {
    /// Returns the remaining items of this iterator as a slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::thin_vec;
    ///
    /// let vec = thin_vec!['a', 'b', 'c'];
    /// let mut into_iter = vec.into_iter();
    /// assert_eq!(into_iter.as_slice(), &['a', 'b', 'c']);
    /// let _ = into_iter.next().unwrap();
    /// assert_eq!(into_iter.as_slice(), &['b', 'c']);
    /// ```
    pub fn as_slice(&self) -> &[T] {
        unsafe { slice::from_raw_parts(self.vec.data_raw().add(self.start), self.len()) }
    }

    /// Returns the remaining items of this iterator as a mutable slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::thin_vec;
    ///
    /// let vec = thin_vec!['a', 'b', 'c'];
    /// let mut into_iter = vec.into_iter();
    /// assert_eq!(into_iter.as_slice(), &['a', 'b', 'c']);
    /// into_iter.as_mut_slice()[2] = 'z';
    /// assert_eq!(into_iter.next().unwrap(), 'a');
    /// assert_eq!(into_iter.next().unwrap(), 'b');
    /// assert_eq!(into_iter.next().unwrap(), 'z');
    /// ```
    pub fn as_mut_slice(&mut self) -> &mut [T] {
        unsafe { &mut *self.as_raw_mut_slice() }
    }

    fn as_raw_mut_slice(&mut self) -> *mut [T] {
        unsafe { ptr::slice_from_raw_parts_mut(self.vec.data_raw().add(self.start), self.len()) }
    }
}

impl<T> Iterator for IntoIter<T> {
    type Item = T;
    fn next(&mut self) -> Option<T> {
        if self.start == self.vec.len() {
            None
        } else {
            unsafe {
                let old_start = self.start;
                self.start += 1;
                Some(ptr::read(self.vec.data_raw().add(old_start)))
            }
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = self.vec.len() - self.start;
        (len, Some(len))
    }
}

impl<T> DoubleEndedIterator for IntoIter<T> {
    fn next_back(&mut self) -> Option<T> {
        if self.start == self.vec.len() {
            None
        } else {
            self.vec.pop()
        }
    }
}

impl<T> ExactSizeIterator for IntoIter<T> {}

impl<T> std::iter::FusedIterator for IntoIter<T> {}

// SAFETY: the length calculation is trivial, we're an array! And if it's wrong we're So Screwed.
#[cfg(feature = "unstable")]
unsafe impl<T> std::iter::TrustedLen for IntoIter<T> {}

impl<T> Drop for IntoIter<T> {
    #[inline]
    fn drop(&mut self) {
        #[cold]
        #[inline(never)]
        fn drop_non_singleton<T>(this: &mut IntoIter<T>) {
            unsafe {
                let mut vec = mem::replace(&mut this.vec, ThinVec::new());
                ptr::drop_in_place(&mut vec[this.start..]);
                vec.set_len_non_singleton(0)
            }
        }

        if !self.vec.is_singleton() {
            drop_non_singleton(self);
        }
    }
}

impl<T: fmt::Debug> fmt::Debug for IntoIter<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_tuple("IntoIter").field(&self.as_slice()).finish()
    }
}

impl<T> AsRef<[T]> for IntoIter<T> {
    fn as_ref(&self) -> &[T] {
        self.as_slice()
    }
}

impl<T: Clone> Clone for IntoIter<T> {
    #[allow(clippy::into_iter_on_ref)]
    fn clone(&self) -> Self {
        // Just create a new `ThinVec` from the remaining elements and IntoIter it
        self.as_slice()
            .into_iter()
            .cloned()
            .collect::<ThinVec<_>>()
            .into_iter()
    }
}

/// A draining iterator for `ThinVec<T>`.
///
/// This `struct` is created by [`ThinVec::drain`].
/// See its documentation for more.
///
/// # Example
///
/// ```
/// use thin_vec::thin_vec;
///
/// let mut v = thin_vec![0, 1, 2];
/// let iter: thin_vec::Drain<_> = v.drain(..);
/// ```
pub struct Drain<'a, T> {
    // Ok so ThinVec::drain takes a range of the ThinVec and yields the contents by-value,
    // then backshifts the array. During iteration the array is in an unsound state
    // (big deinitialized hole in it), and this is very dangerous.
    //
    // Our first line of defense is the borrow checker: we have a mutable borrow, so nothing
    // can access the ThinVec while we exist. As long as we make sure the ThinVec is in a valid
    // state again before we release the borrow, everything should be A-OK! We do this cleanup
    // in our Drop impl.
    //
    // Unfortunately, that's unsound, because mem::forget exists and The Leakpocalypse Is Real.
    // So we can't actually guarantee our destructor runs before our borrow expires. Thankfully
    // this isn't fatal: we can just set the ThinVec's len to 0 at the start, so if anyone
    // leaks the Drain, we just leak everything the ThinVec contained out of spite! If they
    // *don't* leak us then we can properly repair the len in our Drop impl. This is known
    // as "leak amplification", and is the same approach std uses.
    //
    // But we can do slightly better than setting the len to 0! The drain breaks us up into
    // these parts:
    //
    // ```text
    //
    // [A, B, C, D, E, F, G, H, _, _]
    //  ____  __________  ____  ____
    //   |         |        |     |
    // prefix    drain     tail  spare-cap
    // ```
    //
    // As the drain iterator is consumed from both ends (DoubleEnded!), we'll start to look
    // like this:
    //
    // ```text
    // [A, B, _, _, E, _, G, H, _, _]
    //  ____  __________  ____  ____
    //   |         |        |     |
    // prefix    drain     tail   spare-cap
    // ```
    //
    // Note that the prefix is always valid and untouched, as such we can set the len
    // to the prefix when doing leak-amplification. As a bonus, we can use this value
    // to remember where the drain range starts. At the end we'll look like this
    // (we exhaust ourselves in our Drop impl):
    //
    // ```text
    // [A, B, _, _, _, _, G, H, _, _]
    // _____  __________  _____ ____
    //   |         |        |     |
    //  len      drain     tail  spare-cap
    // ```
    //
    // And need to become this:
    //
    // ```text
    // [A, B, G, H, _, _, _, _, _, _]
    // ___________  ________________
    //     |               |
    //    len          spare-cap
    // ```
    //
    // All this requires is moving the tail back to the prefix (stored in `len`)
    // and setting `len` to `len + tail_len` to undo the leak amplification.
    /// An iterator over the elements we're removing.
    ///
    /// As we go we'll be `read`ing out of the mutable refs yielded by this.
    /// It's ok to use IterMut here because it promises to only take mutable
    /// refs to the parts we haven't yielded yet.
    ///
    /// A downside of this (and the *mut below) is that it makes this type invariant, when
    /// technically it could be covariant?
    iter: IterMut<'a, T>,
    /// The actual ThinVec, which we need to hold onto to undo the leak amplification
    /// and backshift the tail into place. This should only be accessed when we're
    /// completely done with the IterMut in the `drop` impl of this type (or miri will get mad).
    ///
    /// Since we set the `len` of this to be before `IterMut`, we can use that `len`
    /// to retrieve the index of the start of the drain range later.
    vec: *mut ThinVec<T>,
    /// The one-past-the-end index of the drain range, or equivalently the start of the tail.
    end: usize,
    /// The length of the tail.
    tail: usize,
}

impl<'a, T> Iterator for Drain<'a, T> {
    type Item = T;
    fn next(&mut self) -> Option<T> {
        self.iter.next().map(|x| unsafe { ptr::read(x) })
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<'a, T> DoubleEndedIterator for Drain<'a, T> {
    fn next_back(&mut self) -> Option<T> {
        self.iter.next_back().map(|x| unsafe { ptr::read(x) })
    }
}

impl<'a, T> ExactSizeIterator for Drain<'a, T> {}

// SAFETY: we need to keep track of this perfectly Or Else anyway!
#[cfg(feature = "unstable")]
unsafe impl<T> std::iter::TrustedLen for Drain<'_, T> {}

impl<T> std::iter::FusedIterator for Drain<'_, T> {}

impl<'a, T> Drop for Drain<'a, T> {
    fn drop(&mut self) {
        // Consume the rest of the iterator.
        for _ in self.by_ref() {}

        // Move the tail over the drained items, and update the length.
        unsafe {
            let vec = &mut *self.vec;

            // Don't mutate the empty singleton!
            if !vec.is_singleton() {
                let old_len = vec.len();
                let start = vec.data_raw().add(old_len);
                let end = vec.data_raw().add(self.end);
                ptr::copy(end, start, self.tail);
                vec.set_len_non_singleton(old_len + self.tail);
            }
        }
    }
}

impl<T: fmt::Debug> fmt::Debug for Drain<'_, T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_tuple("Drain").field(&self.iter.as_slice()).finish()
    }
}

impl<'a, T> Drain<'a, T> {
    /// Returns the remaining items of this iterator as a slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use thin_vec::thin_vec;
    ///
    /// let mut vec = thin_vec!['a', 'b', 'c'];
    /// let mut drain = vec.drain(..);
    /// assert_eq!(drain.as_slice(), &['a', 'b', 'c']);
    /// let _ = drain.next().unwrap();
    /// assert_eq!(drain.as_slice(), &['b', 'c']);
    /// ```
    #[must_use]
    pub fn as_slice(&self) -> &[T] {
        // SAFETY: this is A-OK because the elements that the underlying
        // iterator still points at are still logically initialized and contiguous.
        self.iter.as_slice()
    }
}

impl<'a, T> AsRef<[T]> for Drain<'a, T> {
    fn as_ref(&self) -> &[T] {
        self.as_slice()
    }
}

/// A splicing iterator for `ThinVec`.
///
/// This struct is created by [`ThinVec::splice`][].
/// See its documentation for more.
///
/// # Example
///
/// ```
/// use thin_vec::thin_vec;
///
/// let mut v = thin_vec![0, 1, 2];
/// let new = [7, 8];
/// let iter: thin_vec::Splice<_> = v.splice(1.., new);
/// ```
#[derive(Debug)]
pub struct Splice<'a, I: Iterator + 'a> {
    drain: Drain<'a, I::Item>,
    replace_with: I,
}

impl<I: Iterator> Iterator for Splice<'_, I> {
    type Item = I::Item;

    fn next(&mut self) -> Option<Self::Item> {
        self.drain.next()
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.drain.size_hint()
    }
}

impl<I: Iterator> DoubleEndedIterator for Splice<'_, I> {
    fn next_back(&mut self) -> Option<Self::Item> {
        self.drain.next_back()
    }
}

impl<I: Iterator> ExactSizeIterator for Splice<'_, I> {}

impl<I: Iterator> Drop for Splice<'_, I> {
    fn drop(&mut self) {
        // Ensure we've fully drained out the range
        self.drain.by_ref().for_each(drop);

        unsafe {
            // If there's no tail elements, then the inner ThinVec is already
            // correct and we can just extend it like normal.
            if self.drain.tail == 0 {
                (*self.drain.vec).extend(self.replace_with.by_ref());
                return;
            }

            // First fill the range left by drain().
            if !self.drain.fill(&mut self.replace_with) {
                return;
            }

            // There may be more elements. Use the lower bound as an estimate.
            let (lower_bound, _upper_bound) = self.replace_with.size_hint();
            if lower_bound > 0 {
                self.drain.move_tail(lower_bound);
                if !self.drain.fill(&mut self.replace_with) {
                    return;
                }
            }

            // Collect any remaining elements.
            // This is a zero-length vector which does not allocate if `lower_bound` was exact.
            let mut collected = self
                .replace_with
                .by_ref()
                .collect::<Vec<I::Item>>()
                .into_iter();
            // Now we have an exact count.
            if collected.len() > 0 {
                self.drain.move_tail(collected.len());
                let filled = self.drain.fill(&mut collected);
                debug_assert!(filled);
                debug_assert_eq!(collected.len(), 0);
            }
        }
        // Let `Drain::drop` move the tail back if necessary and restore `vec.len`.
    }
}

/// Private helper methods for `Splice::drop`
impl<T> Drain<'_, T> {
    /// The range from `self.vec.len` to `self.tail_start` contains elements
    /// that have been moved out.
    /// Fill that range as much as possible with new elements from the `replace_with` iterator.
    /// Returns `true` if we filled the entire range. (`replace_with.next()` didn’t return `None`.)
    unsafe fn fill<I: Iterator<Item = T>>(&mut self, replace_with: &mut I) -> bool {
        let vec = unsafe { &mut *self.vec };
        let range_start = vec.len();
        let range_end = self.end;
        let range_slice = unsafe {
            slice::from_raw_parts_mut(vec.data_raw().add(range_start), range_end - range_start)
        };

        for place in range_slice {
            if let Some(new_item) = replace_with.next() {
                unsafe { ptr::write(place, new_item) };
                vec.set_len(vec.len() + 1);
            } else {
                return false;
            }
        }
        true
    }

    /// Makes room for inserting more elements before the tail.
    unsafe fn move_tail(&mut self, additional: usize) {
        let vec = unsafe { &mut *self.vec };
        let len = self.end + self.tail;
        vec.reserve(len.checked_add(additional).expect("capacity overflow"));

        let new_tail_start = self.end + additional;
        unsafe {
            let src = vec.data_raw().add(self.end);
            let dst = vec.data_raw().add(new_tail_start);
            ptr::copy(src, dst, self.tail);
        }
        self.end = new_tail_start;
    }
}

/// Write is implemented for `ThinVec<u8>` by appending to the vector.
/// The vector will grow as needed.
/// This implementation is identical to the one for `Vec<u8>`.
impl io::Write for ThinVec<u8> {
    #[inline]
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.extend_from_slice(buf);
        Ok(buf.len())
    }

    #[inline]
    fn write_all(&mut self, buf: &[u8]) -> io::Result<()> {
        self.extend_from_slice(buf);
        Ok(())
    }

    #[inline]
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

// TODO: a million Index impls

#[cfg(test)]
mod tests {
    use super::{ThinVec, MAX_CAP};

    #[test]
    fn test_size_of() {
        use std::mem::size_of;
        assert_eq!(size_of::<ThinVec<u8>>(), size_of::<&u8>());

        assert_eq!(size_of::<Option<ThinVec<u8>>>(), size_of::<&u8>());
    }

    #[test]
    fn test_drop_empty() {
        ThinVec::<u8>::new();
    }

    #[test]
    fn test_data_ptr_alignment() {
        let v = ThinVec::<u16>::new();
        assert!(v.data_raw() as usize % 2 == 0);

        let v = ThinVec::<u32>::new();
        assert!(v.data_raw() as usize % 4 == 0);

        let v = ThinVec::<u64>::new();
        assert!(v.data_raw() as usize % 8 == 0);
    }

    #[test]
    #[cfg_attr(feature = "gecko-ffi", should_panic)]
    fn test_overaligned_type_is_rejected_for_gecko_ffi_mode() {
        #[repr(align(16))]
        struct Align16(u8);

        let v = ThinVec::<Align16>::new();
        assert!(v.data_raw() as usize % 16 == 0);
    }

    #[test]
    fn test_partial_eq() {
        assert_eq!(thin_vec![0], thin_vec![0]);
        assert_ne!(thin_vec![0], thin_vec![1]);
        assert_eq!(thin_vec![1, 2, 3], vec![1, 2, 3]);
    }

    #[test]
    fn test_alloc() {
        let mut v = ThinVec::new();
        assert!(!v.has_allocation());
        v.push(1);
        assert!(v.has_allocation());
        v.pop();
        assert!(v.has_allocation());
        v.shrink_to_fit();
        assert!(!v.has_allocation());
        v.reserve(64);
        assert!(v.has_allocation());
        v = ThinVec::with_capacity(64);
        assert!(v.has_allocation());
        v = ThinVec::with_capacity(0);
        assert!(!v.has_allocation());
    }

    #[test]
    fn test_drain_items() {
        let mut vec = thin_vec![1, 2, 3];
        let mut vec2 = thin_vec![];
        for i in vec.drain(..) {
            vec2.push(i);
        }
        assert_eq!(vec, []);
        assert_eq!(vec2, [1, 2, 3]);
    }

    #[test]
    fn test_drain_items_reverse() {
        let mut vec = thin_vec![1, 2, 3];
        let mut vec2 = thin_vec![];
        for i in vec.drain(..).rev() {
            vec2.push(i);
        }
        assert_eq!(vec, []);
        assert_eq!(vec2, [3, 2, 1]);
    }

    #[test]
    fn test_drain_items_zero_sized() {
        let mut vec = thin_vec![(), (), ()];
        let mut vec2 = thin_vec![];
        for i in vec.drain(..) {
            vec2.push(i);
        }
        assert_eq!(vec, []);
        assert_eq!(vec2, [(), (), ()]);
    }

    #[test]
    #[should_panic]
    fn test_drain_out_of_bounds() {
        let mut v = thin_vec![1, 2, 3, 4, 5];
        v.drain(5..6);
    }

    #[test]
    fn test_drain_range() {
        let mut v = thin_vec![1, 2, 3, 4, 5];
        for _ in v.drain(4..) {}
        assert_eq!(v, &[1, 2, 3, 4]);

        let mut v: ThinVec<_> = (1..6).map(|x| x.to_string()).collect();
        for _ in v.drain(1..4) {}
        assert_eq!(v, &[1.to_string(), 5.to_string()]);

        let mut v: ThinVec<_> = (1..6).map(|x| x.to_string()).collect();
        for _ in v.drain(1..4).rev() {}
        assert_eq!(v, &[1.to_string(), 5.to_string()]);

        let mut v: ThinVec<_> = thin_vec![(); 5];
        for _ in v.drain(1..4).rev() {}
        assert_eq!(v, &[(), ()]);
    }

    #[test]
    fn test_drain_max_vec_size() {
        let mut v = ThinVec::<()>::with_capacity(MAX_CAP);
        unsafe {
            v.set_len(MAX_CAP);
        }
        for _ in v.drain(MAX_CAP - 1..) {}
        assert_eq!(v.len(), MAX_CAP - 1);
    }

    #[test]
    fn test_clear() {
        let mut v = ThinVec::<i32>::new();
        assert_eq!(v.len(), 0);
        assert_eq!(v.capacity(), 0);
        assert_eq!(&v[..], &[]);

        v.clear();
        assert_eq!(v.len(), 0);
        assert_eq!(v.capacity(), 0);
        assert_eq!(&v[..], &[]);

        v.push(1);
        v.push(2);
        assert_eq!(v.len(), 2);
        assert!(v.capacity() >= 2);
        assert_eq!(&v[..], &[1, 2]);

        v.clear();
        assert_eq!(v.len(), 0);
        assert!(v.capacity() >= 2);
        assert_eq!(&v[..], &[]);

        v.push(3);
        v.push(4);
        assert_eq!(v.len(), 2);
        assert!(v.capacity() >= 2);
        assert_eq!(&v[..], &[3, 4]);

        v.clear();
        assert_eq!(v.len(), 0);
        assert!(v.capacity() >= 2);
        assert_eq!(&v[..], &[]);

        v.clear();
        assert_eq!(v.len(), 0);
        assert!(v.capacity() >= 2);
        assert_eq!(&v[..], &[]);
    }

    #[test]
    fn test_empty_singleton_torture() {
        {
            let mut v = ThinVec::<i32>::new();
            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert!(v.is_empty());
            assert_eq!(&v[..], &[]);
            assert_eq!(&mut v[..], &mut []);

            assert_eq!(v.pop(), None);
            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);
        }

        {
            let v = ThinVec::<i32>::new();
            assert_eq!(v.into_iter().count(), 0);

            let v = ThinVec::<i32>::new();
            for _ in v.into_iter() {
                unreachable!();
            }
        }

        {
            let mut v = ThinVec::<i32>::new();
            assert_eq!(v.drain(..).len(), 0);

            for _ in v.drain(..) {
                unreachable!()
            }

            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);
        }

        {
            let mut v = ThinVec::<i32>::new();
            assert_eq!(v.splice(.., []).len(), 0);

            for _ in v.splice(.., []) {
                unreachable!()
            }

            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);
        }

        {
            let mut v = ThinVec::<i32>::new();
            v.truncate(1);
            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);

            v.truncate(0);
            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);
        }

        {
            let mut v = ThinVec::<i32>::new();
            v.shrink_to_fit();
            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);
        }

        {
            let mut v = ThinVec::<i32>::new();
            let new = v.split_off(0);
            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);

            assert_eq!(new.len(), 0);
            assert_eq!(new.capacity(), 0);
            assert_eq!(&new[..], &[]);
        }

        {
            let mut v = ThinVec::<i32>::new();
            let mut other = ThinVec::<i32>::new();
            v.append(&mut other);

            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);

            assert_eq!(other.len(), 0);
            assert_eq!(other.capacity(), 0);
            assert_eq!(&other[..], &[]);
        }

        {
            let mut v = ThinVec::<i32>::new();
            v.reserve(0);

            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);
        }

        {
            let mut v = ThinVec::<i32>::new();
            v.reserve_exact(0);

            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);
        }

        {
            let mut v = ThinVec::<i32>::new();
            v.reserve(0);

            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);
        }

        {
            let v = ThinVec::<i32>::with_capacity(0);

            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);
        }

        {
            let v = ThinVec::<i32>::default();

            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);
        }

        {
            let mut v = ThinVec::<i32>::new();
            v.retain(|_| unreachable!());

            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);
        }

        {
            let mut v = ThinVec::<i32>::new();
            v.retain_mut(|_| unreachable!());

            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);
        }

        {
            let mut v = ThinVec::<i32>::new();
            v.dedup_by_key(|x| *x);

            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);
        }

        {
            let mut v = ThinVec::<i32>::new();
            v.dedup_by(|_, _| unreachable!());

            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);
        }

        {
            let v = ThinVec::<i32>::new();
            let v = v.clone();

            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), 0);
            assert_eq!(&v[..], &[]);
        }
    }

    #[test]
    fn test_clone() {
        let mut v = ThinVec::<i32>::new();
        assert!(v.is_singleton());
        v.push(0);
        v.pop();
        assert!(!v.is_singleton());

        let v2 = v.clone();
        assert!(v2.is_singleton());
    }
}

#[cfg(test)]
mod std_tests {
    #![allow(clippy::reversed_empty_ranges)]

    use super::*;
    use std::mem::size_of;
    use std::usize;

    struct DropCounter<'a> {
        count: &'a mut u32,
    }

    impl<'a> Drop for DropCounter<'a> {
        fn drop(&mut self) {
            *self.count += 1;
        }
    }

    #[test]
    fn test_small_vec_struct() {
        assert!(size_of::<ThinVec<u8>>() == size_of::<usize>());
    }

    #[test]
    fn test_double_drop() {
        struct TwoVec<T> {
            x: ThinVec<T>,
            y: ThinVec<T>,
        }

        let (mut count_x, mut count_y) = (0, 0);
        {
            let mut tv = TwoVec {
                x: ThinVec::new(),
                y: ThinVec::new(),
            };
            tv.x.push(DropCounter {
                count: &mut count_x,
            });
            tv.y.push(DropCounter {
                count: &mut count_y,
            });

            // If ThinVec had a drop flag, here is where it would be zeroed.
            // Instead, it should rely on its internal state to prevent
            // doing anything significant when dropped multiple times.
            drop(tv.x);

            // Here tv goes out of scope, tv.y should be dropped, but not tv.x.
        }

        assert_eq!(count_x, 1);
        assert_eq!(count_y, 1);
    }

    #[test]
    fn test_reserve() {
        let mut v = ThinVec::new();
        assert_eq!(v.capacity(), 0);

        v.reserve(2);
        assert!(v.capacity() >= 2);

        for i in 0..16 {
            v.push(i);
        }

        assert!(v.capacity() >= 16);
        v.reserve(16);
        assert!(v.capacity() >= 32);

        v.push(16);

        v.reserve(16);
        assert!(v.capacity() >= 33)
    }

    #[test]
    fn test_extend() {
        let mut v = ThinVec::<usize>::new();
        let mut w = ThinVec::new();
        v.extend(w.clone());
        assert_eq!(v, &[]);

        v.extend(0..3);
        for i in 0..3 {
            w.push(i)
        }

        assert_eq!(v, w);

        v.extend(3..10);
        for i in 3..10 {
            w.push(i)
        }

        assert_eq!(v, w);

        v.extend(w.clone()); // specializes to `append`
        assert!(v.iter().eq(w.iter().chain(w.iter())));

        // Zero sized types
        #[derive(PartialEq, Debug)]
        struct Foo;

        let mut a = ThinVec::new();
        let b = thin_vec![Foo, Foo];

        a.extend(b);
        assert_eq!(a, &[Foo, Foo]);

        // Double drop
        let mut count_x = 0;
        {
            let mut x = ThinVec::new();
            let y = thin_vec![DropCounter {
                count: &mut count_x
            }];
            x.extend(y);
        }

        assert_eq!(count_x, 1);
    }

    /* TODO: implement extend for Iter<&Copy>
        #[test]
        fn test_extend_ref() {
            let mut v = thin_vec![1, 2];
            v.extend(&[3, 4, 5]);

            assert_eq!(v.len(), 5);
            assert_eq!(v, [1, 2, 3, 4, 5]);

            let w = thin_vec![6, 7];
            v.extend(&w);

            assert_eq!(v.len(), 7);
            assert_eq!(v, [1, 2, 3, 4, 5, 6, 7]);
        }
    */

    #[test]
    fn test_slice_from_mut() {
        let mut values = thin_vec![1, 2, 3, 4, 5];
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
    fn test_slice_to_mut() {
        let mut values = thin_vec![1, 2, 3, 4, 5];
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
    fn test_split_at_mut() {
        let mut values = thin_vec![1, 2, 3, 4, 5];
        {
            let (left, right) = values.split_at_mut(2);
            {
                let left: &[_] = left;
                assert!(left[..left.len()] == [1, 2]);
            }
            for p in left {
                *p += 1;
            }

            {
                let right: &[_] = right;
                assert!(right[..right.len()] == [3, 4, 5]);
            }
            for p in right {
                *p += 2;
            }
        }

        assert_eq!(values, [2, 3, 5, 6, 7]);
    }

    #[test]
    fn test_clone() {
        let v: ThinVec<i32> = thin_vec![];
        let w = thin_vec![1, 2, 3];

        assert_eq!(v, v.clone());

        let z = w.clone();
        assert_eq!(w, z);
        // they should be disjoint in memory.
        assert!(w.as_ptr() != z.as_ptr())
    }

    #[test]
    fn test_clone_from() {
        let mut v = thin_vec![];
        let three: ThinVec<Box<_>> = thin_vec![Box::new(1), Box::new(2), Box::new(3)];
        let two: ThinVec<Box<_>> = thin_vec![Box::new(4), Box::new(5)];
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
    fn test_retain() {
        let mut vec = thin_vec![1, 2, 3, 4];
        vec.retain(|&x| x % 2 == 0);
        assert_eq!(vec, [2, 4]);
    }

    #[test]
    fn test_retain_mut() {
        let mut vec = thin_vec![9, 9, 9, 9];
        let mut i = 0;
        vec.retain_mut(|x| {
            i += 1;
            *x = i;
            i != 4
        });
        assert_eq!(vec, [1, 2, 3]);
    }

    #[test]
    fn test_dedup() {
        fn case(a: ThinVec<i32>, b: ThinVec<i32>) {
            let mut v = a;
            v.dedup();
            assert_eq!(v, b);
        }
        case(thin_vec![], thin_vec![]);
        case(thin_vec![1], thin_vec![1]);
        case(thin_vec![1, 1], thin_vec![1]);
        case(thin_vec![1, 2, 3], thin_vec![1, 2, 3]);
        case(thin_vec![1, 1, 2, 3], thin_vec![1, 2, 3]);
        case(thin_vec![1, 2, 2, 3], thin_vec![1, 2, 3]);
        case(thin_vec![1, 2, 3, 3], thin_vec![1, 2, 3]);
        case(thin_vec![1, 1, 2, 2, 2, 3, 3], thin_vec![1, 2, 3]);
    }

    #[test]
    fn test_dedup_by_key() {
        fn case(a: ThinVec<i32>, b: ThinVec<i32>) {
            let mut v = a;
            v.dedup_by_key(|i| *i / 10);
            assert_eq!(v, b);
        }
        case(thin_vec![], thin_vec![]);
        case(thin_vec![10], thin_vec![10]);
        case(thin_vec![10, 11], thin_vec![10]);
        case(thin_vec![10, 20, 30], thin_vec![10, 20, 30]);
        case(thin_vec![10, 11, 20, 30], thin_vec![10, 20, 30]);
        case(thin_vec![10, 20, 21, 30], thin_vec![10, 20, 30]);
        case(thin_vec![10, 20, 30, 31], thin_vec![10, 20, 30]);
        case(thin_vec![10, 11, 20, 21, 22, 30, 31], thin_vec![10, 20, 30]);
    }

    #[test]
    fn test_dedup_by() {
        let mut vec = thin_vec!["foo", "bar", "Bar", "baz", "bar"];
        vec.dedup_by(|a, b| a.eq_ignore_ascii_case(b));

        assert_eq!(vec, ["foo", "bar", "baz", "bar"]);

        let mut vec = thin_vec![("foo", 1), ("foo", 2), ("bar", 3), ("bar", 4), ("bar", 5)];
        vec.dedup_by(|a, b| {
            a.0 == b.0 && {
                b.1 += a.1;
                true
            }
        });

        assert_eq!(vec, [("foo", 3), ("bar", 12)]);
    }

    #[test]
    fn test_dedup_unique() {
        let mut v0: ThinVec<Box<_>> = thin_vec![Box::new(1), Box::new(1), Box::new(2), Box::new(3)];
        v0.dedup();
        let mut v1: ThinVec<Box<_>> = thin_vec![Box::new(1), Box::new(2), Box::new(2), Box::new(3)];
        v1.dedup();
        let mut v2: ThinVec<Box<_>> = thin_vec![Box::new(1), Box::new(2), Box::new(3), Box::new(3)];
        v2.dedup();
        // If the boxed pointers were leaked or otherwise misused, valgrind
        // and/or rt should raise errors.
    }

    #[test]
    fn zero_sized_values() {
        let mut v = ThinVec::new();
        assert_eq!(v.len(), 0);
        v.push(());
        assert_eq!(v.len(), 1);
        v.push(());
        assert_eq!(v.len(), 2);
        assert_eq!(v.pop(), Some(()));
        assert_eq!(v.pop(), Some(()));
        assert_eq!(v.pop(), None);

        assert_eq!(v.iter().count(), 0);
        v.push(());
        assert_eq!(v.iter().count(), 1);
        v.push(());
        assert_eq!(v.iter().count(), 2);

        for &() in &v {}

        assert_eq!(v.iter_mut().count(), 2);
        v.push(());
        assert_eq!(v.iter_mut().count(), 3);
        v.push(());
        assert_eq!(v.iter_mut().count(), 4);

        for &mut () in &mut v {}
        unsafe {
            v.set_len(0);
        }
        assert_eq!(v.iter_mut().count(), 0);
    }

    #[test]
    fn test_partition() {
        assert_eq!(
            thin_vec![].into_iter().partition(|x: &i32| *x < 3),
            (thin_vec![], thin_vec![])
        );
        assert_eq!(
            thin_vec![1, 2, 3].into_iter().partition(|x| *x < 4),
            (thin_vec![1, 2, 3], thin_vec![])
        );
        assert_eq!(
            thin_vec![1, 2, 3].into_iter().partition(|x| *x < 2),
            (thin_vec![1], thin_vec![2, 3])
        );
        assert_eq!(
            thin_vec![1, 2, 3].into_iter().partition(|x| *x < 0),
            (thin_vec![], thin_vec![1, 2, 3])
        );
    }

    #[test]
    fn test_zip_unzip() {
        let z1 = thin_vec![(1, 4), (2, 5), (3, 6)];

        let (left, right): (ThinVec<_>, ThinVec<_>) = z1.iter().cloned().unzip();

        assert_eq!((1, 4), (left[0], right[0]));
        assert_eq!((2, 5), (left[1], right[1]));
        assert_eq!((3, 6), (left[2], right[2]));
    }

    #[test]
    fn test_vec_truncate_drop() {
        static mut DROPS: u32 = 0;
        struct Elem(i32);
        impl Drop for Elem {
            fn drop(&mut self) {
                unsafe {
                    DROPS += 1;
                }
            }
        }

        let mut v = thin_vec![Elem(1), Elem(2), Elem(3), Elem(4), Elem(5)];
        assert_eq!(unsafe { DROPS }, 0);
        v.truncate(3);
        assert_eq!(unsafe { DROPS }, 2);
        v.truncate(0);
        assert_eq!(unsafe { DROPS }, 5);
    }

    #[test]
    #[should_panic]
    fn test_vec_truncate_fail() {
        struct BadElem(i32);
        impl Drop for BadElem {
            fn drop(&mut self) {
                let BadElem(ref mut x) = *self;
                if *x == 0xbadbeef {
                    panic!("BadElem panic: 0xbadbeef")
                }
            }
        }

        let mut v = thin_vec![BadElem(1), BadElem(2), BadElem(0xbadbeef), BadElem(4)];
        v.truncate(0);
    }

    #[test]
    fn test_index() {
        let vec = thin_vec![1, 2, 3];
        assert!(vec[1] == 2);
    }

    #[test]
    #[should_panic]
    fn test_index_out_of_bounds() {
        let vec = thin_vec![1, 2, 3];
        let _ = vec[3];
    }

    #[test]
    #[should_panic]
    fn test_slice_out_of_bounds_1() {
        let x = thin_vec![1, 2, 3, 4, 5];
        let _ = &x[!0..];
    }

    #[test]
    #[should_panic]
    fn test_slice_out_of_bounds_2() {
        let x = thin_vec![1, 2, 3, 4, 5];
        let _ = &x[..6];
    }

    #[test]
    #[should_panic]
    fn test_slice_out_of_bounds_3() {
        let x = thin_vec![1, 2, 3, 4, 5];
        let _ = &x[!0..4];
    }

    #[test]
    #[should_panic]
    fn test_slice_out_of_bounds_4() {
        let x = thin_vec![1, 2, 3, 4, 5];
        let _ = &x[1..6];
    }

    #[test]
    #[should_panic]
    fn test_slice_out_of_bounds_5() {
        let x = thin_vec![1, 2, 3, 4, 5];
        let _ = &x[3..2];
    }

    #[test]
    #[should_panic]
    fn test_swap_remove_empty() {
        let mut vec = ThinVec::<i32>::new();
        vec.swap_remove(0);
    }

    #[test]
    fn test_move_items() {
        let vec = thin_vec![1, 2, 3];
        let mut vec2 = thin_vec![];
        for i in vec {
            vec2.push(i);
        }
        assert_eq!(vec2, [1, 2, 3]);
    }

    #[test]
    fn test_move_items_reverse() {
        let vec = thin_vec![1, 2, 3];
        let mut vec2 = thin_vec![];
        for i in vec.into_iter().rev() {
            vec2.push(i);
        }
        assert_eq!(vec2, [3, 2, 1]);
    }

    #[test]
    fn test_move_items_zero_sized() {
        let vec = thin_vec![(), (), ()];
        let mut vec2 = thin_vec![];
        for i in vec {
            vec2.push(i);
        }
        assert_eq!(vec2, [(), (), ()]);
    }

    #[test]
    fn test_drain_items() {
        let mut vec = thin_vec![1, 2, 3];
        let mut vec2 = thin_vec![];
        for i in vec.drain(..) {
            vec2.push(i);
        }
        assert_eq!(vec, []);
        assert_eq!(vec2, [1, 2, 3]);
    }

    #[test]
    fn test_drain_items_reverse() {
        let mut vec = thin_vec![1, 2, 3];
        let mut vec2 = thin_vec![];
        for i in vec.drain(..).rev() {
            vec2.push(i);
        }
        assert_eq!(vec, []);
        assert_eq!(vec2, [3, 2, 1]);
    }

    #[test]
    fn test_drain_items_zero_sized() {
        let mut vec = thin_vec![(), (), ()];
        let mut vec2 = thin_vec![];
        for i in vec.drain(..) {
            vec2.push(i);
        }
        assert_eq!(vec, []);
        assert_eq!(vec2, [(), (), ()]);
    }

    #[test]
    #[should_panic]
    fn test_drain_out_of_bounds() {
        let mut v = thin_vec![1, 2, 3, 4, 5];
        v.drain(5..6);
    }

    #[test]
    fn test_drain_range() {
        let mut v = thin_vec![1, 2, 3, 4, 5];
        for _ in v.drain(4..) {}
        assert_eq!(v, &[1, 2, 3, 4]);

        let mut v: ThinVec<_> = (1..6).map(|x| x.to_string()).collect();
        for _ in v.drain(1..4) {}
        assert_eq!(v, &[1.to_string(), 5.to_string()]);

        let mut v: ThinVec<_> = (1..6).map(|x| x.to_string()).collect();
        for _ in v.drain(1..4).rev() {}
        assert_eq!(v, &[1.to_string(), 5.to_string()]);

        let mut v: ThinVec<_> = thin_vec![(); 5];
        for _ in v.drain(1..4).rev() {}
        assert_eq!(v, &[(), ()]);
    }

    #[test]
    fn test_drain_inclusive_range() {
        let mut v = thin_vec!['a', 'b', 'c', 'd', 'e'];
        for _ in v.drain(1..=3) {}
        assert_eq!(v, &['a', 'e']);

        let mut v: ThinVec<_> = (0..=5).map(|x| x.to_string()).collect();
        for _ in v.drain(1..=5) {}
        assert_eq!(v, &["0".to_string()]);

        let mut v: ThinVec<String> = (0..=5).map(|x| x.to_string()).collect();
        for _ in v.drain(0..=5) {}
        assert_eq!(v, ThinVec::<String>::new());

        let mut v: ThinVec<_> = (0..=5).map(|x| x.to_string()).collect();
        for _ in v.drain(0..=3) {}
        assert_eq!(v, &["4".to_string(), "5".to_string()]);

        let mut v: ThinVec<_> = (0..=1).map(|x| x.to_string()).collect();
        for _ in v.drain(..=0) {}
        assert_eq!(v, &["1".to_string()]);
    }

    #[test]
    #[cfg(not(feature = "gecko-ffi"))]
    fn test_drain_max_vec_size() {
        let mut v = ThinVec::<()>::with_capacity(usize::max_value());
        unsafe {
            v.set_len(usize::max_value());
        }
        for _ in v.drain(usize::max_value() - 1..) {}
        assert_eq!(v.len(), usize::max_value() - 1);

        let mut v = ThinVec::<()>::with_capacity(usize::max_value());
        unsafe {
            v.set_len(usize::max_value());
        }
        for _ in v.drain(usize::max_value() - 1..=usize::max_value() - 1) {}
        assert_eq!(v.len(), usize::max_value() - 1);
    }

    #[test]
    #[should_panic]
    fn test_drain_inclusive_out_of_bounds() {
        let mut v = thin_vec![1, 2, 3, 4, 5];
        v.drain(5..=5);
    }

    #[test]
    fn test_splice() {
        let mut v = thin_vec![1, 2, 3, 4, 5];
        let a = [10, 11, 12];
        v.splice(2..4, a.iter().cloned());
        assert_eq!(v, &[1, 2, 10, 11, 12, 5]);
        v.splice(1..3, Some(20));
        assert_eq!(v, &[1, 20, 11, 12, 5]);
    }

    #[test]
    fn test_splice_inclusive_range() {
        let mut v = thin_vec![1, 2, 3, 4, 5];
        let a = [10, 11, 12];
        let t1: ThinVec<_> = v.splice(2..=3, a.iter().cloned()).collect();
        assert_eq!(v, &[1, 2, 10, 11, 12, 5]);
        assert_eq!(t1, &[3, 4]);
        let t2: ThinVec<_> = v.splice(1..=2, Some(20)).collect();
        assert_eq!(v, &[1, 20, 11, 12, 5]);
        assert_eq!(t2, &[2, 10]);
    }

    #[test]
    #[should_panic]
    fn test_splice_out_of_bounds() {
        let mut v = thin_vec![1, 2, 3, 4, 5];
        let a = [10, 11, 12];
        v.splice(5..6, a.iter().cloned());
    }

    #[test]
    #[should_panic]
    fn test_splice_inclusive_out_of_bounds() {
        let mut v = thin_vec![1, 2, 3, 4, 5];
        let a = [10, 11, 12];
        v.splice(5..=5, a.iter().cloned());
    }

    #[test]
    fn test_splice_items_zero_sized() {
        let mut vec = thin_vec![(), (), ()];
        let vec2 = thin_vec![];
        let t: ThinVec<_> = vec.splice(1..2, vec2.iter().cloned()).collect();
        assert_eq!(vec, &[(), ()]);
        assert_eq!(t, &[()]);
    }

    #[test]
    fn test_splice_unbounded() {
        let mut vec = thin_vec![1, 2, 3, 4, 5];
        let t: ThinVec<_> = vec.splice(.., None).collect();
        assert_eq!(vec, &[]);
        assert_eq!(t, &[1, 2, 3, 4, 5]);
    }

    #[test]
    fn test_splice_forget() {
        let mut v = thin_vec![1, 2, 3, 4, 5];
        let a = [10, 11, 12];
        ::std::mem::forget(v.splice(2..4, a.iter().cloned()));
        assert_eq!(v, &[1, 2]);
    }

    #[test]
    fn test_splice_from_empty() {
        let mut v = thin_vec![];
        let a = [10, 11, 12];
        v.splice(.., a.iter().cloned());
        assert_eq!(v, &[10, 11, 12]);
    }

    /* probs won't ever impl this
        #[test]
        fn test_into_boxed_slice() {
            let xs = thin_vec![1, 2, 3];
            let ys = xs.into_boxed_slice();
            assert_eq!(&*ys, [1, 2, 3]);
        }
    */

    #[test]
    fn test_append() {
        let mut vec = thin_vec![1, 2, 3];
        let mut vec2 = thin_vec![4, 5, 6];
        vec.append(&mut vec2);
        assert_eq!(vec, [1, 2, 3, 4, 5, 6]);
        assert_eq!(vec2, []);
    }

    #[test]
    fn test_split_off() {
        let mut vec = thin_vec![1, 2, 3, 4, 5, 6];
        let vec2 = vec.split_off(4);
        assert_eq!(vec, [1, 2, 3, 4]);
        assert_eq!(vec2, [5, 6]);
    }

    #[test]
    fn test_into_iter_as_slice() {
        let vec = thin_vec!['a', 'b', 'c'];
        let mut into_iter = vec.into_iter();
        assert_eq!(into_iter.as_slice(), &['a', 'b', 'c']);
        let _ = into_iter.next().unwrap();
        assert_eq!(into_iter.as_slice(), &['b', 'c']);
        let _ = into_iter.next().unwrap();
        let _ = into_iter.next().unwrap();
        assert_eq!(into_iter.as_slice(), &[]);
    }

    #[test]
    fn test_into_iter_as_mut_slice() {
        let vec = thin_vec!['a', 'b', 'c'];
        let mut into_iter = vec.into_iter();
        assert_eq!(into_iter.as_slice(), &['a', 'b', 'c']);
        into_iter.as_mut_slice()[0] = 'x';
        into_iter.as_mut_slice()[1] = 'y';
        assert_eq!(into_iter.next().unwrap(), 'x');
        assert_eq!(into_iter.as_slice(), &['y', 'c']);
    }

    #[test]
    fn test_into_iter_debug() {
        let vec = thin_vec!['a', 'b', 'c'];
        let into_iter = vec.into_iter();
        let debug = format!("{:?}", into_iter);
        assert_eq!(debug, "IntoIter(['a', 'b', 'c'])");
    }

    #[test]
    fn test_into_iter_count() {
        assert_eq!(thin_vec![1, 2, 3].into_iter().count(), 3);
    }

    #[test]
    fn test_into_iter_clone() {
        fn iter_equal<I: Iterator<Item = i32>>(it: I, slice: &[i32]) {
            let v: ThinVec<i32> = it.collect();
            assert_eq!(&v[..], slice);
        }
        let mut it = thin_vec![1, 2, 3].into_iter();
        iter_equal(it.clone(), &[1, 2, 3]);
        assert_eq!(it.next(), Some(1));
        let mut it = it.rev();
        iter_equal(it.clone(), &[3, 2]);
        assert_eq!(it.next(), Some(3));
        iter_equal(it.clone(), &[2]);
        assert_eq!(it.next(), Some(2));
        iter_equal(it.clone(), &[]);
        assert_eq!(it.next(), None);
    }

    /* TODO: make drain covariant
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

    /* TODO: specialize vec.into_iter().collect::<ThinVec<_>>();
        #[test]
        fn from_into_inner() {
            let vec = thin_vec![1, 2, 3];
            let ptr = vec.as_ptr();
            let vec = vec.into_iter().collect::<ThinVec<_>>();
            assert_eq!(vec, [1, 2, 3]);
            assert_eq!(vec.as_ptr(), ptr);

            let ptr = &vec[1] as *const _;
            let mut it = vec.into_iter();
            it.next().unwrap();
            let vec = it.collect::<ThinVec<_>>();
            assert_eq!(vec, [2, 3]);
            assert!(ptr != vec.as_ptr());
        }
    */

    #[test]
    #[cfg_attr(feature = "gecko-ffi", ignore)]
    fn overaligned_allocations() {
        #[repr(align(256))]
        struct Foo(usize);
        let mut v = thin_vec![Foo(273)];
        for i in 0..0x1000 {
            v.reserve_exact(i);
            assert!(v[0].0 == 273);
            assert!(v.as_ptr() as usize & 0xff == 0);
            v.shrink_to_fit();
            assert!(v[0].0 == 273);
            assert!(v.as_ptr() as usize & 0xff == 0);
        }
    }

    /* TODO: implement drain_filter?
        #[test]
        fn drain_filter_empty() {
            let mut vec: ThinVec<i32> = thin_vec![];

            {
                let mut iter = vec.drain_filter(|_| true);
                assert_eq!(iter.size_hint(), (0, Some(0)));
                assert_eq!(iter.next(), None);
                assert_eq!(iter.size_hint(), (0, Some(0)));
                assert_eq!(iter.next(), None);
                assert_eq!(iter.size_hint(), (0, Some(0)));
            }
            assert_eq!(vec.len(), 0);
            assert_eq!(vec, thin_vec![]);
        }

        #[test]
        fn drain_filter_zst() {
            let mut vec = thin_vec![(), (), (), (), ()];
            let initial_len = vec.len();
            let mut count = 0;
            {
                let mut iter = vec.drain_filter(|_| true);
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
            assert_eq!(vec.len(), 0);
            assert_eq!(vec, thin_vec![]);
        }

        #[test]
        fn drain_filter_false() {
            let mut vec = thin_vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

            let initial_len = vec.len();
            let mut count = 0;
            {
                let mut iter = vec.drain_filter(|_| false);
                assert_eq!(iter.size_hint(), (0, Some(initial_len)));
                for _ in iter.by_ref() {
                    count += 1;
                }
                assert_eq!(iter.size_hint(), (0, Some(0)));
                assert_eq!(iter.next(), None);
                assert_eq!(iter.size_hint(), (0, Some(0)));
            }

            assert_eq!(count, 0);
            assert_eq!(vec.len(), initial_len);
            assert_eq!(vec, thin_vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
        }

        #[test]
        fn drain_filter_true() {
            let mut vec = thin_vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

            let initial_len = vec.len();
            let mut count = 0;
            {
                let mut iter = vec.drain_filter(|_| true);
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
            assert_eq!(vec.len(), 0);
            assert_eq!(vec, thin_vec![]);
        }

        #[test]
        fn drain_filter_complex() {

            {   //                [+xxx++++++xxxxx++++x+x++]
                let mut vec = thin_vec![1,
                                   2, 4, 6,
                                   7, 9, 11, 13, 15, 17,
                                   18, 20, 22, 24, 26,
                                   27, 29, 31, 33,
                                   34,
                                   35,
                                   36,
                                   37, 39];

                let removed = vec.drain_filter(|x| *x % 2 == 0).collect::<ThinVec<_>>();
                assert_eq!(removed.len(), 10);
                assert_eq!(removed, thin_vec![2, 4, 6, 18, 20, 22, 24, 26, 34, 36]);

                assert_eq!(vec.len(), 14);
                assert_eq!(vec, thin_vec![1, 7, 9, 11, 13, 15, 17, 27, 29, 31, 33, 35, 37, 39]);
            }

            {   //                [xxx++++++xxxxx++++x+x++]
                let mut vec = thin_vec![2, 4, 6,
                                   7, 9, 11, 13, 15, 17,
                                   18, 20, 22, 24, 26,
                                   27, 29, 31, 33,
                                   34,
                                   35,
                                   36,
                                   37, 39];

                let removed = vec.drain_filter(|x| *x % 2 == 0).collect::<ThinVec<_>>();
                assert_eq!(removed.len(), 10);
                assert_eq!(removed, thin_vec![2, 4, 6, 18, 20, 22, 24, 26, 34, 36]);

                assert_eq!(vec.len(), 13);
                assert_eq!(vec, thin_vec![7, 9, 11, 13, 15, 17, 27, 29, 31, 33, 35, 37, 39]);
            }

            {   //                [xxx++++++xxxxx++++x+x]
                let mut vec = thin_vec![2, 4, 6,
                                   7, 9, 11, 13, 15, 17,
                                   18, 20, 22, 24, 26,
                                   27, 29, 31, 33,
                                   34,
                                   35,
                                   36];

                let removed = vec.drain_filter(|x| *x % 2 == 0).collect::<ThinVec<_>>();
                assert_eq!(removed.len(), 10);
                assert_eq!(removed, thin_vec![2, 4, 6, 18, 20, 22, 24, 26, 34, 36]);

                assert_eq!(vec.len(), 11);
                assert_eq!(vec, thin_vec![7, 9, 11, 13, 15, 17, 27, 29, 31, 33, 35]);
            }

            {   //                [xxxxxxxxxx+++++++++++]
                let mut vec = thin_vec![2, 4, 6, 8, 10, 12, 14, 16, 18, 20,
                                   1, 3, 5, 7, 9, 11, 13, 15, 17, 19];

                let removed = vec.drain_filter(|x| *x % 2 == 0).collect::<ThinVec<_>>();
                assert_eq!(removed.len(), 10);
                assert_eq!(removed, thin_vec![2, 4, 6, 8, 10, 12, 14, 16, 18, 20]);

                assert_eq!(vec.len(), 10);
                assert_eq!(vec, thin_vec![1, 3, 5, 7, 9, 11, 13, 15, 17, 19]);
            }

            {   //                [+++++++++++xxxxxxxxxx]
                let mut vec = thin_vec![1, 3, 5, 7, 9, 11, 13, 15, 17, 19,
                                   2, 4, 6, 8, 10, 12, 14, 16, 18, 20];

                let removed = vec.drain_filter(|x| *x % 2 == 0).collect::<ThinVec<_>>();
                assert_eq!(removed.len(), 10);
                assert_eq!(removed, thin_vec![2, 4, 6, 8, 10, 12, 14, 16, 18, 20]);

                assert_eq!(vec.len(), 10);
                assert_eq!(vec, thin_vec![1, 3, 5, 7, 9, 11, 13, 15, 17, 19]);
            }
        }
    */
    #[test]
    fn test_reserve_exact() {
        // This is all the same as test_reserve

        let mut v = ThinVec::new();
        assert_eq!(v.capacity(), 0);

        v.reserve_exact(2);
        assert!(v.capacity() >= 2);

        for i in 0..16 {
            v.push(i);
        }

        assert!(v.capacity() >= 16);
        v.reserve_exact(16);
        assert!(v.capacity() >= 32);

        v.push(16);

        v.reserve_exact(16);
        assert!(v.capacity() >= 33)
    }

    /* TODO: implement try_reserve
        #[test]
        fn test_try_reserve() {

            // These are the interesting cases:
            // * exactly isize::MAX should never trigger a CapacityOverflow (can be OOM)
            // * > isize::MAX should always fail
            //    * On 16/32-bit should CapacityOverflow
            //    * On 64-bit should OOM
            // * overflow may trigger when adding `len` to `cap` (in number of elements)
            // * overflow may trigger when multiplying `new_cap` by size_of::<T> (to get bytes)

            const MAX_CAP: usize = isize::MAX as usize;
            const MAX_USIZE: usize = usize::MAX;

            // On 16/32-bit, we check that allocations don't exceed isize::MAX,
            // on 64-bit, we assume the OS will give an OOM for such a ridiculous size.
            // Any platform that succeeds for these requests is technically broken with
            // ptr::offset because LLVM is the worst.
            let guards_against_isize = size_of::<usize>() < 8;

            {
                // Note: basic stuff is checked by test_reserve
                let mut empty_bytes: ThinVec<u8> = ThinVec::new();

                // Check isize::MAX doesn't count as an overflow
                if let Err(CapacityOverflow) = empty_bytes.try_reserve(MAX_CAP) {
                    panic!("isize::MAX shouldn't trigger an overflow!");
                }
                // Play it again, frank! (just to be sure)
                if let Err(CapacityOverflow) = empty_bytes.try_reserve(MAX_CAP) {
                    panic!("isize::MAX shouldn't trigger an overflow!");
                }

                if guards_against_isize {
                    // Check isize::MAX + 1 does count as overflow
                    if let Err(CapacityOverflow) = empty_bytes.try_reserve(MAX_CAP + 1) {
                    } else { panic!("isize::MAX + 1 should trigger an overflow!") }

                    // Check usize::MAX does count as overflow
                    if let Err(CapacityOverflow) = empty_bytes.try_reserve(MAX_USIZE) {
                    } else { panic!("usize::MAX should trigger an overflow!") }
                } else {
                    // Check isize::MAX + 1 is an OOM
                    if let Err(AllocErr) = empty_bytes.try_reserve(MAX_CAP + 1) {
                    } else { panic!("isize::MAX + 1 should trigger an OOM!") }

                    // Check usize::MAX is an OOM
                    if let Err(AllocErr) = empty_bytes.try_reserve(MAX_USIZE) {
                    } else { panic!("usize::MAX should trigger an OOM!") }
                }
            }


            {
                // Same basic idea, but with non-zero len
                let mut ten_bytes: ThinVec<u8> = thin_vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

                if let Err(CapacityOverflow) = ten_bytes.try_reserve(MAX_CAP - 10) {
                    panic!("isize::MAX shouldn't trigger an overflow!");
                }
                if let Err(CapacityOverflow) = ten_bytes.try_reserve(MAX_CAP - 10) {
                    panic!("isize::MAX shouldn't trigger an overflow!");
                }
                if guards_against_isize {
                    if let Err(CapacityOverflow) = ten_bytes.try_reserve(MAX_CAP - 9) {
                    } else { panic!("isize::MAX + 1 should trigger an overflow!"); }
                } else {
                    if let Err(AllocErr) = ten_bytes.try_reserve(MAX_CAP - 9) {
                    } else { panic!("isize::MAX + 1 should trigger an OOM!") }
                }
                // Should always overflow in the add-to-len
                if let Err(CapacityOverflow) = ten_bytes.try_reserve(MAX_USIZE) {
                } else { panic!("usize::MAX should trigger an overflow!") }
            }


            {
                // Same basic idea, but with interesting type size
                let mut ten_u32s: ThinVec<u32> = thin_vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

                if let Err(CapacityOverflow) = ten_u32s.try_reserve(MAX_CAP/4 - 10) {
                    panic!("isize::MAX shouldn't trigger an overflow!");
                }
                if let Err(CapacityOverflow) = ten_u32s.try_reserve(MAX_CAP/4 - 10) {
                    panic!("isize::MAX shouldn't trigger an overflow!");
                }
                if guards_against_isize {
                    if let Err(CapacityOverflow) = ten_u32s.try_reserve(MAX_CAP/4 - 9) {
                    } else { panic!("isize::MAX + 1 should trigger an overflow!"); }
                } else {
                    if let Err(AllocErr) = ten_u32s.try_reserve(MAX_CAP/4 - 9) {
                    } else { panic!("isize::MAX + 1 should trigger an OOM!") }
                }
                // Should fail in the mul-by-size
                if let Err(CapacityOverflow) = ten_u32s.try_reserve(MAX_USIZE - 20) {
                } else {
                    panic!("usize::MAX should trigger an overflow!");
                }
            }

        }

        #[test]
        fn test_try_reserve_exact() {

            // This is exactly the same as test_try_reserve with the method changed.
            // See that test for comments.

            const MAX_CAP: usize = isize::MAX as usize;
            const MAX_USIZE: usize = usize::MAX;

            let guards_against_isize = size_of::<usize>() < 8;

            {
                let mut empty_bytes: ThinVec<u8> = ThinVec::new();

                if let Err(CapacityOverflow) = empty_bytes.try_reserve_exact(MAX_CAP) {
                    panic!("isize::MAX shouldn't trigger an overflow!");
                }
                if let Err(CapacityOverflow) = empty_bytes.try_reserve_exact(MAX_CAP) {
                    panic!("isize::MAX shouldn't trigger an overflow!");
                }

                if guards_against_isize {
                    if let Err(CapacityOverflow) = empty_bytes.try_reserve_exact(MAX_CAP + 1) {
                    } else { panic!("isize::MAX + 1 should trigger an overflow!") }

                    if let Err(CapacityOverflow) = empty_bytes.try_reserve_exact(MAX_USIZE) {
                    } else { panic!("usize::MAX should trigger an overflow!") }
                } else {
                    if let Err(AllocErr) = empty_bytes.try_reserve_exact(MAX_CAP + 1) {
                    } else { panic!("isize::MAX + 1 should trigger an OOM!") }

                    if let Err(AllocErr) = empty_bytes.try_reserve_exact(MAX_USIZE) {
                    } else { panic!("usize::MAX should trigger an OOM!") }
                }
            }


            {
                let mut ten_bytes: ThinVec<u8> = thin_vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

                if let Err(CapacityOverflow) = ten_bytes.try_reserve_exact(MAX_CAP - 10) {
                    panic!("isize::MAX shouldn't trigger an overflow!");
                }
                if let Err(CapacityOverflow) = ten_bytes.try_reserve_exact(MAX_CAP - 10) {
                    panic!("isize::MAX shouldn't trigger an overflow!");
                }
                if guards_against_isize {
                    if let Err(CapacityOverflow) = ten_bytes.try_reserve_exact(MAX_CAP - 9) {
                    } else { panic!("isize::MAX + 1 should trigger an overflow!"); }
                } else {
                    if let Err(AllocErr) = ten_bytes.try_reserve_exact(MAX_CAP - 9) {
                    } else { panic!("isize::MAX + 1 should trigger an OOM!") }
                }
                if let Err(CapacityOverflow) = ten_bytes.try_reserve_exact(MAX_USIZE) {
                } else { panic!("usize::MAX should trigger an overflow!") }
            }


            {
                let mut ten_u32s: ThinVec<u32> = thin_vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

                if let Err(CapacityOverflow) = ten_u32s.try_reserve_exact(MAX_CAP/4 - 10) {
                    panic!("isize::MAX shouldn't trigger an overflow!");
                }
                if let Err(CapacityOverflow) = ten_u32s.try_reserve_exact(MAX_CAP/4 - 10) {
                    panic!("isize::MAX shouldn't trigger an overflow!");
                }
                if guards_against_isize {
                    if let Err(CapacityOverflow) = ten_u32s.try_reserve_exact(MAX_CAP/4 - 9) {
                    } else { panic!("isize::MAX + 1 should trigger an overflow!"); }
                } else {
                    if let Err(AllocErr) = ten_u32s.try_reserve_exact(MAX_CAP/4 - 9) {
                    } else { panic!("isize::MAX + 1 should trigger an OOM!") }
                }
                if let Err(CapacityOverflow) = ten_u32s.try_reserve_exact(MAX_USIZE - 20) {
                } else { panic!("usize::MAX should trigger an overflow!") }
            }
        }
    */

    #[test]
    #[cfg_attr(feature = "gecko-ffi", ignore)]
    fn test_header_data() {
        macro_rules! assert_aligned_head_ptr {
            ($typename:ty) => {{
                let v: ThinVec<$typename> = ThinVec::with_capacity(1 /* ensure allocation */);
                let head_ptr: *mut $typename = v.data_raw();
                assert_eq!(
                    head_ptr as usize % std::mem::align_of::<$typename>(),
                    0,
                    "expected Header::data<{}> to be aligned",
                    stringify!($typename)
                );
            }};
        }

        const HEADER_SIZE: usize = std::mem::size_of::<Header>();
        assert_eq!(2 * std::mem::size_of::<usize>(), HEADER_SIZE);

        #[repr(C, align(128))]
        struct Funky<T>(T);
        assert_eq!(padding::<Funky<()>>(), 128 - HEADER_SIZE);
        assert_aligned_head_ptr!(Funky<()>);

        assert_eq!(padding::<Funky<u8>>(), 128 - HEADER_SIZE);
        assert_aligned_head_ptr!(Funky<u8>);

        assert_eq!(padding::<Funky<[(); 1024]>>(), 128 - HEADER_SIZE);
        assert_aligned_head_ptr!(Funky<[(); 1024]>);

        assert_eq!(padding::<Funky<[*mut usize; 1024]>>(), 128 - HEADER_SIZE);
        assert_aligned_head_ptr!(Funky<[*mut usize; 1024]>);
    }

    #[cfg(feature = "serde")]
    use serde_test::{assert_tokens, Token};

    #[test]
    #[cfg(feature = "serde")]
    fn test_ser_de_empty() {
        let vec = ThinVec::<u32>::new();

        assert_tokens(&vec, &[Token::Seq { len: Some(0) }, Token::SeqEnd]);
    }

    #[test]
    #[cfg(feature = "serde")]
    fn test_ser_de() {
        let mut vec = ThinVec::<u32>::new();
        vec.push(20);
        vec.push(55);
        vec.push(123);

        assert_tokens(
            &vec,
            &[
                Token::Seq { len: Some(3) },
                Token::U32(20),
                Token::U32(55),
                Token::U32(123),
                Token::SeqEnd,
            ],
        );
    }

    #[test]
    fn test_set_len() {
        let mut vec: ThinVec<u32> = thin_vec![];
        unsafe {
            vec.set_len(0); // at one point this caused a crash
        }
    }

    #[test]
    #[should_panic(expected = "invalid set_len(1) on empty ThinVec")]
    fn test_set_len_invalid() {
        let mut vec: ThinVec<u32> = thin_vec![];
        unsafe {
            vec.set_len(1);
        }
    }

    #[test]
    #[should_panic(expected = "capacity overflow")]
    fn test_capacity_overflow_header_too_big() {
        let vec: ThinVec<u8> = ThinVec::with_capacity(isize::MAX as usize - 2);
        assert!(vec.capacity() > 0);
    }
    #[test]
    #[should_panic(expected = "capacity overflow")]
    fn test_capacity_overflow_cap_too_big() {
        let vec: ThinVec<u8> = ThinVec::with_capacity(isize::MAX as usize + 1);
        assert!(vec.capacity() > 0);
    }
    #[test]
    #[should_panic(expected = "capacity overflow")]
    fn test_capacity_overflow_size_mul1() {
        let vec: ThinVec<u16> = ThinVec::with_capacity(isize::MAX as usize + 1);
        assert!(vec.capacity() > 0);
    }
    #[test]
    #[should_panic(expected = "capacity overflow")]
    fn test_capacity_overflow_size_mul2() {
        let vec: ThinVec<u16> = ThinVec::with_capacity(isize::MAX as usize / 2 + 1);
        assert!(vec.capacity() > 0);
    }
    #[test]
    #[should_panic(expected = "capacity overflow")]
    fn test_capacity_overflow_cap_really_isnt_isize() {
        let vec: ThinVec<u8> = ThinVec::with_capacity(isize::MAX as usize);
        assert!(vec.capacity() > 0);
    }
}
