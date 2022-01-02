/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! This module provides rust bindings for the XPCOM string types.
//!
//! # TL;DR (what types should I use)
//!
//! Use `&{mut,} nsA[C]String` for functions in rust which wish to take or
//! mutate XPCOM strings. The other string types `Deref` to this type.
//!
//! Use `ns[C]String` (`ns[C]String` in C++) for string struct members, and as
//! an intermediate between rust string data structures (such as `String` or
//! `Vec<u16>`) and `&{mut,} nsA[C]String` (using `ns[C]String::from(value)`).
//! These conversions will attempt to re-use the passed-in buffer, appending a
//! null.
//!
//! Use `ns[C]Str` (`nsDependent[C]String` in C++) as an intermediate between
//! borrowed rust data structures (such as `&str` and `&[u16]`) and `&{mut,}
//! nsA[C]String` (using `ns[C]Str::from(value)`). These conversions should not
//! perform any allocations. This type is not safe to share with `C++` as a
//! struct field, but passing the borrowed `&{mut,} nsA[C]String` over FFI is
//! safe.
//!
//! Use `*{const,mut} nsA[C]String` (`{const,} nsA[C]String*` in C++) for
//! function arguments passed across the rust/C++ language boundary.
//!
//! There is currently no Rust equivalent to nsAuto[C]String. Implementing a
//! type that contains a pointer to an inline buffer is difficult in Rust due
//! to its move semantics, which require that it be safe to move a value by
//! copying its bits. If such a type is genuinely needed at some point,
//! https://bugzilla.mozilla.org/show_bug.cgi?id=1403506#c6 has a sketch of how
//! to emulate it via macros.
//!
//! # String Types
//!
//! ## `nsA[C]String`
//!
//! The core types in this module are `nsAString` and `nsACString`. These types
//! are zero-sized as far as rust is concerned, and are safe to pass around
//! behind both references (in rust code), and pointers (in C++ code). They
//! represent a handle to a XPCOM string which holds either `u16` or `u8`
//! characters respectively. The backing character buffer is guaranteed to live
//! as long as the reference to the `nsAString` or `nsACString`.
//!
//! These types in rust are simply used as dummy types. References to them
//! represent a pointer to the beginning of a variable-sized `#[repr(C)]` struct
//! which is common between both C++ and Rust implementations. In C++, their
//! corresponding types are also named `nsAString` or `nsACString`, and they are
//! defined within the `nsTSubstring.{cpp,h}` file.
//!
//! ### Valid Operations
//!
//! An `&nsA[C]String` acts like rust's `&str`, in that it is a borrowed
//! reference to the backing data. When used as an argument to other functions
//! on `&mut nsA[C]String`, optimizations can be performed to avoid copying
//! buffers, as information about the backing storage is preserved.
//!
//! An `&mut nsA[C]String` acts like rust's `&mut Cow<str>`, in that it is a
//! mutable reference to a potentially borrowed string, which when modified will
//! ensure that it owns its own backing storage. This type can be appended to
//! with the methods `.append`, `.append_utf{8,16}`, and with the `write!`
//! macro, and can be assigned to with `.assign`.
//!
//! ## `ns[C]Str<'a>`
//!
//! This type is an maybe-owned string type. It acts similarially to a
//! `Cow<[{u8,u16}]>`. This type provides `Deref` and `DerefMut` implementations
//! to `nsA[C]String`, which provides the methods for manipulating this type.
//! This type's lifetime parameter, `'a`, represents the lifetime of the backing
//! storage. When modified this type may re-allocate in order to ensure that it
//! does not mutate its backing storage.
//!
//! `ns[C]Str`s can be constructed either with `ns[C]Str::new()`, which creates
//! an empty `ns[C]Str<'static>`, or through one of the provided `From`
//! implementations. Only `nsCStr` can be constructed `From<'a str>`, as
//! constructing a `nsStr` would require transcoding. Use `ns[C]String` instead.
//!
//! When passing this type by reference, prefer passing a `&nsA[C]String` or
//! `&mut nsA[C]String`. to passing this type.
//!
//! When passing this type across the language boundary, pass it as `*const
//! nsA[C]String` for an immutable reference, or `*mut nsA[C]String` for a
//! mutable reference.
//!
//! ## `ns[C]String`
//!
//! This type is an owned, null-terminated string type. This type provides
//! `Deref` and `DerefMut` implementations to `nsA[C]String`, which provides the
//! methods for manipulating this type.
//!
//! `ns[C]String`s can be constructed either with `ns[C]String::new()`, which
//! creates an empty `ns[C]String`, or through one of the provided `From`
//! implementations, which will try to avoid reallocating when possible,
//! although a terminating `null` will be added.
//!
//! When passing this type by reference, prefer passing a `&nsA[C]String` or
//! `&mut nsA[C]String`. to passing this type.
//!
//! When passing this type across the language boundary, pass it as `*const
//! nsA[C]String` for an immutable reference, or `*mut nsA[C]String` for a
//! mutable reference. This struct may also be included in `#[repr(C)]` structs
//! shared with C++.
//!
//! ## `ns[C]StringRepr`
//!
//! This crate also provides the type `ns[C]StringRepr` which acts conceptually
//! similar to an `ns[C]String`, however, it does not have a `Drop`
//! implementation.
//!
//! If this type is dropped in rust, it will not free its backing storage. This
//! can be useful when implementing FFI types which contain `ns[C]String` members
//! which invoke their member's destructors through C++ code.

#![allow(non_camel_case_types)]
#![allow(clippy::missing_safety_doc)]
#![allow(clippy::new_without_default)]
#![allow(clippy::result_unit_err)]

use bitflags::bitflags;
use std::borrow;
use std::cmp;
use std::fmt;
use std::marker::PhantomData;
use std::mem;
use std::ops::{Deref, DerefMut};
use std::os::raw::c_void;
use std::ptr;
use std::slice;
use std::str;

mod conversions;

pub use self::conversions::nscstring_fallible_append_latin1_to_utf8_check;
pub use self::conversions::nscstring_fallible_append_utf16_to_latin1_lossy_impl;
pub use self::conversions::nscstring_fallible_append_utf16_to_utf8_impl;
pub use self::conversions::nscstring_fallible_append_utf8_to_latin1_lossy_check;
pub use self::conversions::nsstring_fallible_append_latin1_impl;
pub use self::conversions::nsstring_fallible_append_utf8_impl;

/// A type for showing that `finish()` was called on a `BulkWriteHandle`.
/// Instantiating this type from elsewhere is basically an assertion that
/// there is no `BulkWriteHandle` around, so be very careful with instantiating
/// this type!
pub struct BulkWriteOk;

/// Semi-arbitrary threshold below which we don't care about shrinking
/// buffers to size. Currently matches `CACHE_LINE` in the `conversions`
/// module.
const SHRINKING_THRESHOLD: usize = 64;

///////////////////////////////////
// Internal Implementation Flags //
///////////////////////////////////

bitflags! {
    // While this has the same layout as u16, it cannot be passed
    // over FFI safely as a u16.
    #[repr(C)]
    struct DataFlags: u16 {
        const TERMINATED = 1 << 0; // IsTerminated returns true
        const VOIDED = 1 << 1; // IsVoid returns true
        const REFCOUNTED = 1 << 2; // mData points to a heap-allocated, shareable, refcounted
                                    // buffer
        const OWNED = 1 << 3; // mData points to a heap-allocated, raw buffer
        const INLINE = 1 << 4; // mData points to a writable, inline buffer
        const LITERAL = 1 << 5; // mData points to a string literal; TERMINATED will also be set
    }
}

bitflags! {
    // While this has the same layout as u16, it cannot be passed
    // over FFI safely as a u16.
    #[repr(C)]
    struct ClassFlags: u16 {
        const INLINE = 1 << 0; // |this|'s buffer is inline
        const NULL_TERMINATED = 1 << 1; // |this| requires its buffer is null-terminated
    }
}

////////////////////////////////////
// Generic String Bindings Macros //
////////////////////////////////////

macro_rules! string_like {
    {
        char_t = $char_t: ty;

        AString = $AString: ident;
        String = $String: ident;
        Str = $Str: ident;

        StringLike = $StringLike: ident;
        StringAdapter = $StringAdapter: ident;
    } => {
        /// This trait is implemented on types which are `ns[C]String`-like, in
        /// that they can at very low cost be converted to a borrowed
        /// `&nsA[C]String`. Unfortunately, the intermediate type
        /// `ns[C]StringAdapter` is required as well due to types like `&[u8]`
        /// needing to be (cheaply) wrapped in a `nsCString` on the stack to
        /// create the `&nsACString`.
        ///
        /// This trait is used to DWIM when calling the methods on
        /// `nsA[C]String`.
        pub trait $StringLike {
            fn adapt(&self) -> $StringAdapter;
        }

        impl<'a, T: $StringLike + ?Sized> $StringLike for &'a T {
            fn adapt(&self) -> $StringAdapter {
                <T as $StringLike>::adapt(*self)
            }
        }

        impl<'a, T> $StringLike for borrow::Cow<'a, T>
            where T: $StringLike + borrow::ToOwned + ?Sized {
            fn adapt(&self) -> $StringAdapter {
                <T as $StringLike>::adapt(self.as_ref())
            }
        }

        impl $StringLike for $AString {
            fn adapt(&self) -> $StringAdapter {
                $StringAdapter::Abstract(self)
            }
        }

        impl<'a> $StringLike for $Str<'a> {
            fn adapt(&self) -> $StringAdapter {
                $StringAdapter::Abstract(self)
            }
        }

        impl $StringLike for $String {
            fn adapt(&self) -> $StringAdapter {
                $StringAdapter::Abstract(self)
            }
        }

        impl $StringLike for [$char_t] {
            fn adapt(&self) -> $StringAdapter {
                $StringAdapter::Borrowed($Str::from(self))
            }
        }

        impl $StringLike for Vec<$char_t> {
            fn adapt(&self) -> $StringAdapter {
                $StringAdapter::Borrowed($Str::from(&self[..]))
            }
        }

        impl $StringLike for Box<[$char_t]> {
            fn adapt(&self) -> $StringAdapter {
                $StringAdapter::Borrowed($Str::from(&self[..]))
            }
        }
    }
}

impl<'a> Drop for nsAStringBulkWriteHandle<'a> {
    /// This only runs in error cases. In success cases, `finish()`
    /// calls `forget(self)`.
    fn drop(&mut self) {
        if self.capacity == 0 {
            // If capacity is 0, the string is a zero-length
            // string, so we have nothing to do.
            return;
        }
        // The old zero terminator may be gone by now, so we need
        // to write a new one somewhere and make length match.
        // We can use a length between 1 and self.capacity.
        // Seems prudent to overwrite the uninitialized memory.
        // Using the length 1 leaves the shortest memory to overwrite.
        // U+FFFD is the safest placeholder. Merely truncating the
        // string to a zero-length string might be dangerous in some
        // scenarios. See
        // https://www.unicode.org/reports/tr36/#Substituting_for_Ill_Formed_Subsequences
        // for closely related scenario.
        unsafe {
            let mut this = self.string.as_repr_mut();
            this.as_mut().length = 1u32;
            *(this.as_mut().data.as_mut()) = 0xFFFDu16;
            *(this.as_mut().data.as_ptr().add(1)) = 0;
        }
    }
}

impl<'a> Drop for nsACStringBulkWriteHandle<'a> {
    /// This only runs in error cases. In success cases, `finish()`
    /// calls `forget(self)`.
    fn drop(&mut self) {
        if self.capacity == 0 {
            // If capacity is 0, the string is a zero-length
            // string, so we have nothing to do.
            return;
        }
        // The old zero terminator may be gone by now, so we need
        // to write a new one somewhere and make length match.
        // We can use a length between 1 and self.capacity.
        // Seems prudent to overwrite the uninitialized memory.
        // Using the length 1 leaves the shortest memory to overwrite.
        // U+FFFD is the safest placeholder, but when it doesn't fit,
        // let's use ASCII substitute. Merely truncating the
        // string to a zero-length string might be dangerous in some
        // scenarios. See
        // https://www.unicode.org/reports/tr36/#Substituting_for_Ill_Formed_Subsequences
        // for closely related scenario.
        unsafe {
            let mut this = self.string.as_repr_mut();
            if self.capacity >= 3 {
                this.as_mut().length = 3u32;
                *(this.as_mut().data.as_mut()) = 0xEFu8;
                *(this.as_mut().data.as_ptr().add(1)) = 0xBFu8;
                *(this.as_mut().data.as_ptr().add(2)) = 0xBDu8;
                *(this.as_mut().data.as_ptr().add(3)) = 0;
            } else {
                this.as_mut().length = 1u32;
                *(this.as_mut().data.as_mut()) = 0x1Au8; // U+FFFD doesn't fit
                *(this.as_mut().data.as_ptr().add(1)) = 0;
            }
        }
    }
}

macro_rules! define_string_types {
    {
        char_t = $char_t: ty;

        AString = $AString: ident;
        String = $String: ident;
        Str = $Str: ident;

        StringLike = $StringLike: ident;
        StringAdapter = $StringAdapter: ident;

        StringRepr = $StringRepr: ident;
        AutoStringRepr = $AutoStringRepr: ident;

        BulkWriteHandle = $BulkWriteHandle: ident;

        drop = $drop: ident;
        assign = $assign: ident, $fallible_assign: ident;
        take_from = $take_from: ident, $fallible_take_from: ident;
        append = $append: ident, $fallible_append: ident;
        set_length = $set_length: ident, $fallible_set_length: ident;
        begin_writing = $begin_writing: ident, $fallible_begin_writing: ident;
        start_bulk_write = $start_bulk_write: ident;
    } => {
        /// The representation of a ns[C]String type in C++. This type is
        /// used internally by our definition of ns[C]String to ensure layout
        /// compatibility with the C++ ns[C]String type.
        ///
        /// This type may also be used in place of a C++ ns[C]String inside of
        /// struct definitions which are shared with C++, as it has identical
        /// layout to our ns[C]String type.
        ///
        /// This struct will leak its data if dropped from rust. See the module
        /// documentation for more information on this type.
        #[repr(C)]
        #[derive(Debug)]
        pub struct $StringRepr {
            data: ptr::NonNull<$char_t>,
            length: u32,
            dataflags: DataFlags,
            classflags: ClassFlags,
        }

        impl $StringRepr {
            fn new(classflags: ClassFlags) -> $StringRepr {
                static NUL: $char_t = 0;
                $StringRepr {
                    data: unsafe { ptr::NonNull::new_unchecked(&NUL as *const _ as *mut _) },
                    length: 0,
                    dataflags: DataFlags::TERMINATED | DataFlags::LITERAL,
                    classflags,
                }
            }
        }

        impl Deref for $StringRepr {
            type Target = $AString;
            fn deref(&self) -> &$AString {
                unsafe {
                    &*(self as *const _ as *const $AString)
                }
            }
        }

        impl DerefMut for $StringRepr {
            fn deref_mut(&mut self) -> &mut $AString {
                unsafe {
                    &mut *(self as *mut _ as *mut $AString)
                }
            }
        }

        #[repr(C)]
        #[derive(Debug)]
        pub struct $AutoStringRepr {
            super_repr: $StringRepr,
            inline_capacity: u32,
        }

        pub struct $BulkWriteHandle<'a> {
            string: &'a mut $AString,
            capacity: usize,
        }

        impl<'a> $BulkWriteHandle<'a> {
            fn new(string: &'a mut $AString, capacity: usize) -> Self {
                $BulkWriteHandle{ string, capacity }
            }

            pub unsafe fn restart_bulk_write(&mut self,
                                             capacity: usize,
                                             units_to_preserve: usize,
                                             allow_shrinking: bool) -> Result<(), ()> {
                self.capacity =
                    self.string.start_bulk_write_impl(capacity,
                                                      units_to_preserve,
                                                      allow_shrinking)?;
                Ok(())
            }

            pub fn finish(mut self, length: usize, allow_shrinking: bool) -> BulkWriteOk {
                // NOTE: Drop is implemented outside the macro earlier in this file,
                // because it needs to deal with different code unit representations
                // for the REPLACEMENT CHARACTER in the UTF-16 and UTF-8 cases and
                // needs to deal with a REPLACEMENT CHARACTER not fitting in the
                // buffer in the UTF-8 case.
                assert!(length <= self.capacity);
                if length == 0 {
                    // `truncate()` is OK even when the string
                    // is in invalid state.
                    self.string.truncate();
                    mem::forget(self); // Don't run the failure path in drop()
                    return BulkWriteOk{};
                }
                if allow_shrinking && length > SHRINKING_THRESHOLD {
                    unsafe {
                        let _ = self.restart_bulk_write(length, length, true);
                    }
                }
                unsafe {
                    let mut this = self.string.as_repr_mut();
                    this.as_mut().length = length as u32;
                    *(this.as_mut().data.as_ptr().add(length)) = 0;
                    if cfg!(debug_assertions) {
                        // Overwrite the unused part in debug builds. Note
                        // that capacity doesn't include space for the zero
                        // terminator, so starting after the zero-terminator
                        // we wrote ends up overwriting the terminator space
                        // not reflected in the capacity number.
                        // write_bytes() takes care of multiplying the length
                        // by the size of T.
                        ptr::write_bytes(this.as_mut().data.as_ptr().add(length + 1),
                                         0xE4u8,
                                         self.capacity - length);
                    }
                    // We don't have a Rust interface for mozilla/MemoryChecking.h,
                    // so let's just not communicate with MSan/Valgrind here.
                }
                mem::forget(self); // Don't run the failure path in drop()
                BulkWriteOk{}
            }

            pub fn as_mut_slice(&mut self) -> &mut [$char_t] {
                unsafe {
                    let mut this = self.string.as_repr_mut();
                    slice::from_raw_parts_mut(this.as_mut().data.as_ptr(), self.capacity)
                }
            }
        }

        /// This type is the abstract type which is used for interacting with
        /// strings in rust. Each string type can derefence to an instance of
        /// this type, which provides the useful operations on strings.
        ///
        /// NOTE: Rust thinks this type has a size of 0, because the data
        /// associated with it is not necessarially safe to move. It is not safe
        /// to construct a nsAString yourself, unless it is received by
        /// dereferencing one of these types.
        ///
        /// NOTE: The `[u8; 0]` member is zero sized, and only exists to prevent
        /// the construction by code outside of this module. It is used instead
        /// of a private `()` member because the `improper_ctypes` lint complains
        /// about some ZST members in `extern "C"` function declarations.
        #[repr(C)]
        pub struct $AString {
            _prohibit_constructor: [u8; 0],
        }

        impl $AString {
            /// Assign the value of `other` into self, overwriting any value
            /// currently stored. Performs an optimized assignment when possible
            /// if `other` is a `nsA[C]String`.
            pub fn assign<T: $StringLike + ?Sized>(&mut self, other: &T) {
                unsafe { $assign(self, other.adapt().as_ptr()) };
            }

            /// Assign the value of `other` into self, overwriting any value
            /// currently stored. Performs an optimized assignment when possible
            /// if `other` is a `nsA[C]String`.
            ///
            /// Returns Ok(()) on success, and Err(()) if the allocation failed.
            pub fn fallible_assign<T: $StringLike + ?Sized>(&mut self, other: &T) -> Result<(), ()> {
                if unsafe { $fallible_assign(self, other.adapt().as_ptr()) } {
                    Ok(())
                } else {
                    Err(())
                }
            }

            /// Take the value of `other` and set `self`, overwriting any value
            /// currently stored. The passed-in string will be truncated.
            pub fn take_from(&mut self, other: &mut $AString) {
                unsafe { $take_from(self, other) };
            }

            /// Take the value of `other` and set `self`, overwriting any value
            /// currently stored. If this function fails, the source string will
            /// be left untouched, otherwise it will be truncated.
            ///
            /// Returns Ok(()) on success, and Err(()) if the allocation failed.
            pub fn fallible_take_from(&mut self, other: &mut $AString) -> Result<(), ()> {
                if unsafe { $fallible_take_from(self, other) } {
                    Ok(())
                } else {
                    Err(())
                }
            }

            /// Append the value of `other` into self.
            pub fn append<T: $StringLike + ?Sized>(&mut self, other: &T) {
                unsafe { $append(self, other.adapt().as_ptr()) };
            }

            /// Append the value of `other` into self.
            ///
            /// Returns Ok(()) on success, and Err(()) if the allocation failed.
            pub fn fallible_append<T: $StringLike + ?Sized>(&mut self, other: &T) -> Result<(), ()> {
                if unsafe { $fallible_append(self, other.adapt().as_ptr()) } {
                    Ok(())
                } else {
                    Err(())
                }
            }

            /// Mark the string's data as void. If `true`, the string will be truncated.
            ///
            /// A void string is generally converted to a `null` JS value by bindings code.
            pub fn set_is_void(&mut self, is_void: bool) {
                if is_void {
                    self.truncate();
                }
                unsafe {
                    self.as_repr_mut().as_mut().dataflags.set(DataFlags::VOIDED, is_void);
                }
            }

            /// Returns whether the string's data is voided.
            pub fn is_void(&self) -> bool {
                self.as_repr().dataflags.contains(DataFlags::VOIDED)
            }

            /// Set the length of the string to the passed-in length, and expand
            /// the backing capacity to match. This method is unsafe as it can
            /// expose uninitialized memory when len is greater than the current
            /// length of the string.
            pub unsafe fn set_length(&mut self, len: u32) {
                $set_length(self, len);
            }

            /// Set the length of the string to the passed-in length, and expand
            /// the backing capacity to match. This method is unsafe as it can
            /// expose uninitialized memory when len is greater than the current
            /// length of the string.
            ///
            /// Returns Ok(()) on success, and Err(()) if the allocation failed.
            pub unsafe fn fallible_set_length(&mut self, len: u32) -> Result<(), ()> {
                if $fallible_set_length(self, len) {
                    Ok(())
                } else {
                    Err(())
                }
            }

            pub fn truncate(&mut self) {
                unsafe {
                    self.set_length(0);
                }
            }

            /// Get a `&mut` reference to the backing data for this string.
            /// This method will allocate and copy if the current backing buffer
            /// is immutable or shared.
            pub fn to_mut(&mut self) -> &mut [$char_t] {
                unsafe {
                    let len = self.len();
                    if len == 0 {
                        // Use an arbitrary but aligned non-null value as the pointer
                        slice::from_raw_parts_mut(ptr::NonNull::<$char_t>::dangling().as_ptr(), 0)
                    } else {
                        slice::from_raw_parts_mut($begin_writing(self), len)
                    }
                }
            }

            /// Get a `&mut` reference to the backing data for this string.
            /// This method will allocate and copy if the current backing buffer
            /// is immutable or shared.
            ///
            /// Returns `Ok(&mut [T])` on success, and `Err(())` if the
            /// allocation failed.
            pub fn fallible_to_mut(&mut self) -> Result<&mut [$char_t], ()> {
                unsafe {
                    let len = self.len();
                    if len == 0 {
                        // Use an arbitrary but aligned non-null value as the pointer
                        Ok(slice::from_raw_parts_mut(
                            ptr::NonNull::<$char_t>::dangling().as_ptr() as *mut $char_t, 0))
                    } else {
                        let ptr = $fallible_begin_writing(self);
                        if ptr.is_null() {
                            Err(())
                        } else {
                            Ok(slice::from_raw_parts_mut(ptr, len))
                        }
                    }
                }
            }

            /// Unshares the buffer of the string and returns a handle
            /// from which a writable slice whose length is the rounded-up
            /// capacity can be obtained.
            ///
            /// Fails also if the new length doesn't fit in 32 bits.
            ///
            /// # Safety
            ///
            /// Unsafe because of exposure of uninitialized memory.
            pub unsafe fn bulk_write(&mut self,
                                     capacity: usize,
                                     units_to_preserve: usize,
                                     allow_shrinking: bool) -> Result<$BulkWriteHandle, ()> {
                let capacity =
                    self.start_bulk_write_impl(capacity, units_to_preserve, allow_shrinking)?;
                Ok($BulkWriteHandle::new(self, capacity))
            }

            unsafe fn start_bulk_write_impl(&mut self,
                                            capacity: usize,
                                            units_to_preserve: usize,
                                            allow_shrinking: bool) -> Result<usize, ()> {
                if capacity > u32::MAX as usize {
                    Err(())
                } else {
                    let capacity32 = capacity as u32;
                    let rounded = $start_bulk_write(self,
                                                    capacity32,
                                                    units_to_preserve as u32,
                                                    allow_shrinking && capacity > SHRINKING_THRESHOLD);
                    if rounded == u32::MAX {
                        return Err(())
                    }
                    Ok(rounded as usize)
                }
            }

            fn as_repr(&self) -> &$StringRepr {
                // All $AString values point to a struct prefix which is
                // identical to $StringRepr, thus we can cast `self`
                // into *const $StringRepr to get the reference to the
                // underlying data.
                unsafe {
                    &*(self as *const _ as *const $StringRepr)
                }
            }

            fn as_repr_mut(&mut self) -> ptr::NonNull<$StringRepr> {
                unsafe { ptr::NonNull::new_unchecked(self as *mut _ as *mut $StringRepr)}
            }

            fn as_auto_string_repr(&self) -> Option<&$AutoStringRepr> {
                if !self.as_repr().classflags.contains(ClassFlags::INLINE) {
                    return None;
                }

                unsafe {
                    Some(&*(self as *const _ as *const $AutoStringRepr))
                }
            }

            /// If this is an autostring, returns the capacity (excluding the
            /// zero terminator) of the inline buffer within `Some()`. Otherwise
            /// returns `None`.
            pub fn inline_capacity(&self) -> Option<usize> {
                Some(self.as_auto_string_repr()?.inline_capacity as usize)
            }
        }

        impl Deref for $AString {
            type Target = [$char_t];
            fn deref(&self) -> &[$char_t] {
                unsafe {
                    // All $AString values point to a struct prefix which is
                    // identical to $StringRepr, thus we can cast `self`
                    // into *const $StringRepr to get the reference to the
                    // underlying data.
                    let this = &*(self as *const _ as *const $StringRepr);
                    slice::from_raw_parts(this.data.as_ptr(), this.length as usize)
                }
            }
        }

        impl AsRef<[$char_t]> for $AString {
            fn as_ref(&self) -> &[$char_t] {
                self
            }
        }

        impl cmp::PartialEq for $AString {
            fn eq(&self, other: &$AString) -> bool {
                &self[..] == &other[..]
            }
        }

        impl cmp::PartialEq<[$char_t]> for $AString {
            fn eq(&self, other: &[$char_t]) -> bool {
                &self[..] == other
            }
        }

        impl cmp::PartialEq<$String> for $AString {
            fn eq(&self, other: &$String) -> bool {
                self.eq(&**other)
            }
        }

        impl<'a> cmp::PartialEq<$Str<'a>> for $AString {
            fn eq(&self, other: &$Str<'a>) -> bool {
                self.eq(&**other)
            }
        }

        #[repr(C)]
        pub struct $Str<'a> {
            hdr: $StringRepr,
            _marker: PhantomData<&'a [$char_t]>,
        }

        impl $Str<'static> {
            pub fn new() -> $Str<'static> {
                $Str {
                    hdr: $StringRepr::new(ClassFlags::empty()),
                    _marker: PhantomData,
                }
            }
        }

        impl<'a> Drop for $Str<'a> {
            fn drop(&mut self) {
                unsafe {
                    $drop(&mut **self);
                }
            }
        }

        impl<'a> Deref for $Str<'a> {
            type Target = $AString;
            fn deref(&self) -> &$AString {
                &self.hdr
            }
        }

        impl<'a> DerefMut for $Str<'a> {
            fn deref_mut(&mut self) -> &mut $AString {
                &mut self.hdr
            }
        }

        impl<'a> AsRef<[$char_t]> for $Str<'a> {
            fn as_ref(&self) -> &[$char_t] {
                &self
            }
        }

        impl<'a> From<&'a [$char_t]> for $Str<'a> {
            fn from(s: &'a [$char_t]) -> $Str<'a> {
                assert!(s.len() < (u32::MAX as usize));
                if s.is_empty() {
                    return $Str::new();
                }
                $Str {
                    hdr: $StringRepr {
                        data: unsafe { ptr::NonNull::new_unchecked(s.as_ptr() as *mut _) },
                        length: s.len() as u32,
                        dataflags: DataFlags::empty(),
                        classflags: ClassFlags::empty(),
                    },
                    _marker: PhantomData,
                }
            }
        }

        impl<'a> From<&'a Vec<$char_t>> for $Str<'a> {
            fn from(s: &'a Vec<$char_t>) -> $Str<'a> {
                $Str::from(&s[..])
            }
        }

        impl<'a> From<&'a $AString> for $Str<'a> {
            fn from(s: &'a $AString) -> $Str<'a> {
                $Str::from(&s[..])
            }
        }

        impl<'a> fmt::Write for $Str<'a> {
            fn write_str(&mut self, s: &str) -> Result<(), fmt::Error> {
                $AString::write_str(self, s)
            }
        }

        impl<'a> fmt::Display for $Str<'a> {
            fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
                <$AString as fmt::Display>::fmt(self, f)
            }
        }

        impl<'a> fmt::Debug for $Str<'a> {
            fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
                <$AString as fmt::Debug>::fmt(self, f)
            }
        }

        impl<'a> cmp::PartialEq for $Str<'a> {
            fn eq(&self, other: &$Str<'a>) -> bool {
                $AString::eq(self, other)
            }
        }

        impl<'a> cmp::PartialEq<[$char_t]> for $Str<'a> {
            fn eq(&self, other: &[$char_t]) -> bool {
                $AString::eq(self, other)
            }
        }

        impl<'a, 'b> cmp::PartialEq<&'b [$char_t]> for $Str<'a> {
            fn eq(&self, other: &&'b [$char_t]) -> bool {
                $AString::eq(self, *other)
            }
        }

        impl<'a> cmp::PartialEq<str> for $Str<'a> {
            fn eq(&self, other: &str) -> bool {
                $AString::eq(self, other)
            }
        }

        impl<'a, 'b> cmp::PartialEq<&'b str> for $Str<'a> {
            fn eq(&self, other: &&'b str) -> bool {
                $AString::eq(self, *other)
            }
        }

        #[repr(C)]
        pub struct $String {
            hdr: $StringRepr,
        }

        unsafe impl Send for $String {}
        unsafe impl Sync for $String {}

        impl $String {
            pub fn new() -> $String {
                $String {
                    hdr: $StringRepr::new(ClassFlags::NULL_TERMINATED),
                }
            }

            /// Converts this String into a StringRepr, which will leak if the
            /// repr is not passed to something that knows how to free it.
            pub fn into_repr(mut self) -> $StringRepr {
                mem::replace(&mut self.hdr, $StringRepr::new(ClassFlags::NULL_TERMINATED))
            }
        }

        impl Drop for $String {
            fn drop(&mut self) {
                unsafe {
                    $drop(&mut **self);
                }
            }
        }

        impl Deref for $String {
            type Target = $AString;
            fn deref(&self) -> &$AString {
                &self.hdr
            }
        }

        impl DerefMut for $String {
            fn deref_mut(&mut self) -> &mut $AString {
                &mut self.hdr
            }
        }

        impl Clone for $String {
            fn clone(&self) -> Self {
                let mut copy = $String::new();
                copy.assign(self);
                copy
            }
        }

        impl AsRef<[$char_t]> for $String {
            fn as_ref(&self) -> &[$char_t] {
                &self
            }
        }

        impl<'a> From<&'a [$char_t]> for $String {
            fn from(s: &'a [$char_t]) -> $String {
                let mut res = $String::new();
                res.assign(&$Str::from(&s[..]));
                res
            }
        }

        impl<'a> From<&'a Vec<$char_t>> for $String {
            fn from(s: &'a Vec<$char_t>) -> $String {
                $String::from(&s[..])
            }
        }

        impl<'a> From<&'a $AString> for $String {
            fn from(s: &'a $AString) -> $String {
                $String::from(&s[..])
            }
        }

        impl From<Box<[$char_t]>> for $String {
            fn from(s: Box<[$char_t]>) -> $String {
                s.into_vec().into()
            }
        }

        impl From<Vec<$char_t>> for $String {
            fn from(mut s: Vec<$char_t>) -> $String {
                assert!(s.len() < (u32::MAX as usize));
                if s.is_empty() {
                    return $String::new();
                }

                let length = s.len() as u32;
                s.push(0); // null terminator

                // SAFETY NOTE: This method produces an data_flags::OWNED
                // ns[C]String from a Box<[$char_t]>. this is only safe
                // because in the Gecko tree, we use the same allocator for
                // Rust code as for C++ code, meaning that our box can be
                // legally freed with libc::free().
                let ptr = s.as_mut_ptr();
                mem::forget(s);
                unsafe {
                    Gecko_IncrementStringAdoptCount(ptr as *mut _);
                }
                $String {
                    hdr: $StringRepr {
                        data: unsafe { ptr::NonNull::new_unchecked(ptr) },
                        length,
                        dataflags: DataFlags::OWNED | DataFlags::TERMINATED,
                        classflags: ClassFlags::NULL_TERMINATED,
                    }
                }
            }
        }

        impl fmt::Write for $String {
            fn write_str(&mut self, s: &str) -> Result<(), fmt::Error> {
                $AString::write_str(self, s)
            }
        }

        impl fmt::Display for $String {
            fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
                <$AString as fmt::Display>::fmt(self, f)
            }
        }

        impl fmt::Debug for $String {
            fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
                <$AString as fmt::Debug>::fmt(self, f)
            }
        }

        impl cmp::PartialEq for $String {
            fn eq(&self, other: &$String) -> bool {
                $AString::eq(self, other)
            }
        }

        impl cmp::PartialEq<[$char_t]> for $String {
            fn eq(&self, other: &[$char_t]) -> bool {
                $AString::eq(self, other)
            }
        }

        impl<'a> cmp::PartialEq<&'a [$char_t]> for $String {
            fn eq(&self, other: &&'a [$char_t]) -> bool {
                $AString::eq(self, *other)
            }
        }

        impl cmp::PartialEq<str> for $String {
            fn eq(&self, other: &str) -> bool {
                $AString::eq(self, other)
            }
        }

        impl<'a> cmp::PartialEq<&'a str> for $String {
            fn eq(&self, other: &&'a str) -> bool {
                $AString::eq(self, *other)
            }
        }

        /// An adapter type to allow for passing both types which coerce to
        /// &[$char_type], and &$AString to a function, while still performing
        /// optimized operations when passed the $AString.
        pub enum $StringAdapter<'a> {
            Borrowed($Str<'a>),
            Abstract(&'a $AString),
        }

        impl<'a> $StringAdapter<'a> {
            fn as_ptr(&self) -> *const $AString {
                &**self
            }
        }

        impl<'a> Deref for $StringAdapter<'a> {
            type Target = $AString;

            fn deref(&self) -> &$AString {
                match *self {
                    $StringAdapter::Borrowed(ref s) => s,
                    $StringAdapter::Abstract(ref s) => s,
                }
            }
        }

        impl<'a> $StringAdapter<'a> {
            #[allow(dead_code)]
            fn is_abstract(&self) -> bool {
                match *self {
                    $StringAdapter::Borrowed(_) => false,
                    $StringAdapter::Abstract(_) => true,
                }
            }
        }

        string_like! {
            char_t = $char_t;

            AString = $AString;
            String = $String;
            Str = $Str;

            StringLike = $StringLike;
            StringAdapter = $StringAdapter;
        }
    }
}

///////////////////////////////////////////
// Bindings for nsCString (u8 char type) //
///////////////////////////////////////////

define_string_types! {
    char_t = u8;

    AString = nsACString;
    String = nsCString;
    Str = nsCStr;

    StringLike = nsCStringLike;
    StringAdapter = nsCStringAdapter;

    StringRepr = nsCStringRepr;
    AutoStringRepr = nsAutoCStringRepr;

    BulkWriteHandle = nsACStringBulkWriteHandle;

    drop = Gecko_FinalizeCString;
    assign = Gecko_AssignCString, Gecko_FallibleAssignCString;
    take_from = Gecko_TakeFromCString, Gecko_FallibleTakeFromCString;
    append = Gecko_AppendCString, Gecko_FallibleAppendCString;
    set_length = Gecko_SetLengthCString, Gecko_FallibleSetLengthCString;
    begin_writing = Gecko_BeginWritingCString, Gecko_FallibleBeginWritingCString;
    start_bulk_write = Gecko_StartBulkWriteCString;
}

impl nsACString {
    /// Gets a CString as an utf-8 str or a String, trying to avoid copies, and
    /// replacing invalid unicode sequences with replacement characters.
    #[inline]
    pub fn to_utf8(&self) -> borrow::Cow<str> {
        String::from_utf8_lossy(&self[..])
    }

    #[inline]
    pub unsafe fn as_str_unchecked(&self) -> &str {
        if cfg!(debug_assertions) {
            str::from_utf8(self).expect("Should be utf-8")
        } else {
            str::from_utf8_unchecked(self)
        }
    }
}

impl<'a> From<&'a str> for nsCStr<'a> {
    fn from(s: &'a str) -> nsCStr<'a> {
        s.as_bytes().into()
    }
}

impl<'a> From<&'a String> for nsCStr<'a> {
    fn from(s: &'a String) -> nsCStr<'a> {
        nsCStr::from(&s[..])
    }
}

impl<'a> From<&'a str> for nsCString {
    fn from(s: &'a str) -> nsCString {
        s.as_bytes().into()
    }
}

impl<'a> From<&'a String> for nsCString {
    fn from(s: &'a String) -> nsCString {
        nsCString::from(&s[..])
    }
}

impl From<Box<str>> for nsCString {
    fn from(s: Box<str>) -> nsCString {
        s.into_string().into()
    }
}

impl From<String> for nsCString {
    fn from(s: String) -> nsCString {
        s.into_bytes().into()
    }
}

// Support for the write!() macro for appending to nsACStrings
impl fmt::Write for nsACString {
    fn write_str(&mut self, s: &str) -> Result<(), fmt::Error> {
        self.append(s);
        Ok(())
    }
}

impl fmt::Display for nsACString {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        fmt::Display::fmt(&self.to_utf8(), f)
    }
}

impl fmt::Debug for nsACString {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        fmt::Debug::fmt(&self.to_utf8(), f)
    }
}

impl cmp::PartialEq<str> for nsACString {
    fn eq(&self, other: &str) -> bool {
        &self[..] == other.as_bytes()
    }
}

impl nsCStringLike for str {
    fn adapt(&self) -> nsCStringAdapter {
        nsCStringAdapter::Borrowed(nsCStr::from(self))
    }
}

impl nsCStringLike for String {
    fn adapt(&self) -> nsCStringAdapter {
        nsCStringAdapter::Borrowed(nsCStr::from(&self[..]))
    }
}

impl nsCStringLike for Box<str> {
    fn adapt(&self) -> nsCStringAdapter {
        nsCStringAdapter::Borrowed(nsCStr::from(&self[..]))
    }
}

// This trait is implemented on types which are Latin1 `nsCString`-like,
// in that they can at very low cost be converted to a borrowed
// `&nsACString` and do not denote UTF-8ness in the Rust type system.
//
// This trait is used to DWIM when calling the methods on
// `nsACString`.
string_like! {
    char_t = u8;

    AString = nsACString;
    String = nsCString;
    Str = nsCStr;

    StringLike = Latin1StringLike;
    StringAdapter = nsCStringAdapter;
}

///////////////////////////////////////////
// Bindings for nsString (u16 char type) //
///////////////////////////////////////////

define_string_types! {
    char_t = u16;

    AString = nsAString;
    String = nsString;
    Str = nsStr;

    StringLike = nsStringLike;
    StringAdapter = nsStringAdapter;

    StringRepr = nsStringRepr;
    AutoStringRepr = nsAutoStringRepr;

    BulkWriteHandle = nsAStringBulkWriteHandle;

    drop = Gecko_FinalizeString;
    assign = Gecko_AssignString, Gecko_FallibleAssignString;
    take_from = Gecko_TakeFromString, Gecko_FallibleTakeFromString;
    append = Gecko_AppendString, Gecko_FallibleAppendString;
    set_length = Gecko_SetLengthString, Gecko_FallibleSetLengthString;
    begin_writing = Gecko_BeginWritingString, Gecko_FallibleBeginWritingString;
    start_bulk_write = Gecko_StartBulkWriteString;
}

// NOTE: The From impl for a string slice for nsString produces a <'static>
// lifetime, as it allocates.
impl<'a> From<&'a str> for nsString {
    fn from(s: &'a str) -> nsString {
        s.encode_utf16().collect::<Vec<u16>>().into()
    }
}

impl<'a> From<&'a String> for nsString {
    fn from(s: &'a String) -> nsString {
        nsString::from(&s[..])
    }
}

// Support for the write!() macro for writing to nsStrings
impl fmt::Write for nsAString {
    fn write_str(&mut self, s: &str) -> Result<(), fmt::Error> {
        // Directly invoke gecko's routines for appending utf8 strings to
        // nsAString values, to avoid as much overhead as possible
        self.append_str(s);
        Ok(())
    }
}

impl nsAString {
    /// Turns this utf-16 string into a string, replacing invalid unicode
    /// sequences with replacement characters.
    ///
    /// This is needed because the default ToString implementation goes through
    /// fmt::Display, and thus allocates the string twice.
    #[allow(clippy::inherent_to_string_shadow_display)]
    pub fn to_string(&self) -> String {
        String::from_utf16_lossy(&self[..])
    }
}

impl fmt::Display for nsAString {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        fmt::Display::fmt(&self.to_string(), f)
    }
}

impl fmt::Debug for nsAString {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        fmt::Debug::fmt(&self.to_string(), f)
    }
}

impl cmp::PartialEq<str> for nsAString {
    fn eq(&self, other: &str) -> bool {
        other.encode_utf16().eq(self.iter().cloned())
    }
}

#[cfg(not(feature = "gecko_debug"))]
#[allow(non_snake_case)]
unsafe fn Gecko_IncrementStringAdoptCount(_: *mut c_void) {}

extern "C" {
    #[cfg(feature = "gecko_debug")]
    fn Gecko_IncrementStringAdoptCount(data: *mut c_void);

    // Gecko implementation in nsSubstring.cpp
    fn Gecko_FinalizeCString(this: *mut nsACString);

    fn Gecko_AssignCString(this: *mut nsACString, other: *const nsACString);
    fn Gecko_TakeFromCString(this: *mut nsACString, other: *mut nsACString);
    fn Gecko_AppendCString(this: *mut nsACString, other: *const nsACString);
    fn Gecko_SetLengthCString(this: *mut nsACString, length: u32);
    fn Gecko_BeginWritingCString(this: *mut nsACString) -> *mut u8;
    fn Gecko_FallibleAssignCString(this: *mut nsACString, other: *const nsACString) -> bool;
    fn Gecko_FallibleTakeFromCString(this: *mut nsACString, other: *mut nsACString) -> bool;
    fn Gecko_FallibleAppendCString(this: *mut nsACString, other: *const nsACString) -> bool;
    fn Gecko_FallibleSetLengthCString(this: *mut nsACString, length: u32) -> bool;
    fn Gecko_FallibleBeginWritingCString(this: *mut nsACString) -> *mut u8;
    fn Gecko_StartBulkWriteCString(
        this: *mut nsACString,
        capacity: u32,
        units_to_preserve: u32,
        allow_shrinking: bool,
    ) -> u32;

    fn Gecko_FinalizeString(this: *mut nsAString);

    fn Gecko_AssignString(this: *mut nsAString, other: *const nsAString);
    fn Gecko_TakeFromString(this: *mut nsAString, other: *mut nsAString);
    fn Gecko_AppendString(this: *mut nsAString, other: *const nsAString);
    fn Gecko_SetLengthString(this: *mut nsAString, length: u32);
    fn Gecko_BeginWritingString(this: *mut nsAString) -> *mut u16;
    fn Gecko_FallibleAssignString(this: *mut nsAString, other: *const nsAString) -> bool;
    fn Gecko_FallibleTakeFromString(this: *mut nsAString, other: *mut nsAString) -> bool;
    fn Gecko_FallibleAppendString(this: *mut nsAString, other: *const nsAString) -> bool;
    fn Gecko_FallibleSetLengthString(this: *mut nsAString, length: u32) -> bool;
    fn Gecko_FallibleBeginWritingString(this: *mut nsAString) -> *mut u16;
    fn Gecko_StartBulkWriteString(
        this: *mut nsAString,
        capacity: u32,
        units_to_preserve: u32,
        allow_shrinking: bool,
    ) -> u32;
}

//////////////////////////////////////
// Repr Validation Helper Functions //
//////////////////////////////////////

pub mod test_helpers {
    //! This module only exists to help with ensuring that the layout of the
    //! structs inside of rust and C++ are identical.
    //!
    //! It is public to ensure that these testing functions are avaliable to
    //! gtest code.

    use super::{nsACString, nsAString};
    use super::{nsCStr, nsCString, nsCStringRepr};
    use super::{nsStr, nsString, nsStringRepr};
    use super::{ClassFlags, DataFlags};
    use std::mem;

    /// Generates an #[no_mangle] extern "C" function which returns the size and
    /// alignment of the given type with the given name.
    macro_rules! size_align_check {
        ($T:ty, $fname:ident) => {
            #[no_mangle]
            #[allow(non_snake_case)]
            pub unsafe extern "C" fn $fname(size: *mut usize, align: *mut usize) {
                *size = mem::size_of::<$T>();
                *align = mem::align_of::<$T>();
            }
        };
        ($T:ty, $U:ty, $V:ty, $fname:ident) => {
            #[no_mangle]
            #[allow(non_snake_case)]
            pub unsafe extern "C" fn $fname(size: *mut usize, align: *mut usize) {
                *size = mem::size_of::<$T>();
                *align = mem::align_of::<$T>();

                assert_eq!(*size, mem::size_of::<$U>());
                assert_eq!(*align, mem::align_of::<$U>());
                assert_eq!(*size, mem::size_of::<$V>());
                assert_eq!(*align, mem::align_of::<$V>());
            }
        };
    }

    size_align_check!(
        nsStringRepr,
        nsString,
        nsStr<'static>,
        Rust_Test_ReprSizeAlign_nsString
    );
    size_align_check!(
        nsCStringRepr,
        nsCString,
        nsCStr<'static>,
        Rust_Test_ReprSizeAlign_nsCString
    );

    /// Generates a $[no_mangle] extern "C" function which returns the size,
    /// alignment and offset in the parent struct of a given member, with the
    /// given name.
    ///
    /// This method can trigger Undefined Behavior if the accessing the member
    /// $member on a given type would use that type's `Deref` implementation.
    macro_rules! member_check {
        ($T:ty, $U:ty, $V:ty, $member:ident, $method:ident) => {
            #[no_mangle]
            #[allow(non_snake_case)]
            pub unsafe extern "C" fn $method(
                size: *mut usize,
                align: *mut usize,
                offset: *mut usize,
            ) {
                // Create a temporary value of type T to get offsets, sizes
                // and alignments from.
                let tmp: mem::MaybeUninit<$T> = mem::MaybeUninit::uninit();
                // FIXME: This should use &raw references when available,
                // this is technically UB as it creates a reference to
                // uninitialized memory, but there's no better way to do
                // this right now.
                let tmp = &*tmp.as_ptr();
                *size = mem::size_of_val(&tmp.$member);
                *align = mem::align_of_val(&tmp.$member);
                *offset = (&tmp.$member as *const _ as usize) - (tmp as *const $T as usize);

                let tmp: mem::MaybeUninit<$U> = mem::MaybeUninit::uninit();
                let tmp = &*tmp.as_ptr();
                assert_eq!(*size, mem::size_of_val(&tmp.hdr.$member));
                assert_eq!(*align, mem::align_of_val(&tmp.hdr.$member));
                assert_eq!(
                    *offset,
                    (&tmp.hdr.$member as *const _ as usize) - (tmp as *const $U as usize)
                );

                let tmp: mem::MaybeUninit<$V> = mem::MaybeUninit::uninit();
                let tmp = &*tmp.as_ptr();
                assert_eq!(*size, mem::size_of_val(&tmp.hdr.$member));
                assert_eq!(*align, mem::align_of_val(&tmp.hdr.$member));
                assert_eq!(
                    *offset,
                    (&tmp.hdr.$member as *const _ as usize) - (tmp as *const $V as usize)
                );
            }
        };
    }

    member_check!(
        nsStringRepr,
        nsString,
        nsStr<'static>,
        data,
        Rust_Test_Member_nsString_mData
    );
    member_check!(
        nsStringRepr,
        nsString,
        nsStr<'static>,
        length,
        Rust_Test_Member_nsString_mLength
    );
    member_check!(
        nsStringRepr,
        nsString,
        nsStr<'static>,
        dataflags,
        Rust_Test_Member_nsString_mDataFlags
    );
    member_check!(
        nsStringRepr,
        nsString,
        nsStr<'static>,
        classflags,
        Rust_Test_Member_nsString_mClassFlags
    );
    member_check!(
        nsCStringRepr,
        nsCString,
        nsCStr<'static>,
        data,
        Rust_Test_Member_nsCString_mData
    );
    member_check!(
        nsCStringRepr,
        nsCString,
        nsCStr<'static>,
        length,
        Rust_Test_Member_nsCString_mLength
    );
    member_check!(
        nsCStringRepr,
        nsCString,
        nsCStr<'static>,
        dataflags,
        Rust_Test_Member_nsCString_mDataFlags
    );
    member_check!(
        nsCStringRepr,
        nsCString,
        nsCStr<'static>,
        classflags,
        Rust_Test_Member_nsCString_mClassFlags
    );

    #[no_mangle]
    #[allow(non_snake_case)]
    pub unsafe extern "C" fn Rust_Test_NsStringFlags(
        f_terminated: *mut u16,
        f_voided: *mut u16,
        f_refcounted: *mut u16,
        f_owned: *mut u16,
        f_inline: *mut u16,
        f_literal: *mut u16,
        f_class_inline: *mut u16,
        f_class_null_terminated: *mut u16,
    ) {
        *f_terminated = DataFlags::TERMINATED.bits();
        *f_voided = DataFlags::VOIDED.bits();
        *f_refcounted = DataFlags::REFCOUNTED.bits();
        *f_owned = DataFlags::OWNED.bits();
        *f_inline = DataFlags::INLINE.bits();
        *f_literal = DataFlags::LITERAL.bits();
        *f_class_inline = ClassFlags::INLINE.bits();
        *f_class_null_terminated = ClassFlags::NULL_TERMINATED.bits();
    }

    #[no_mangle]
    #[allow(non_snake_case)]
    pub unsafe extern "C" fn Rust_InlineCapacityFromRust(
        cstring: *const nsACString,
        string: *const nsAString,
        cstring_capacity: *mut usize,
        string_capacity: *mut usize,
    ) {
        *cstring_capacity = (*cstring).inline_capacity().unwrap();
        *string_capacity = (*string).inline_capacity().unwrap();
    }
}
