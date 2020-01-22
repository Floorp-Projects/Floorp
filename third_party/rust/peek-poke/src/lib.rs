//! Fast binary serialization and deserialization for types with a known maximum size.
//!
//! ## Binary Encoding Scheme
//!
//! ## Usage
//!
//! ## Comparison to bincode

#![no_std]

#[cfg(feature = "derive")]
pub use peek_poke_derive::*;

use core::{
    marker::PhantomData,
    mem::{size_of, transmute},
};

#[cfg(feature = "option_copy")]
use core::mem::uninitialized;

// Helper to copy a slice of bytes `bytes` into a buffer of bytes pointed to by
// `dest`.
#[inline(always)]
fn copy_bytes_to(bytes: &[u8], dest: *mut u8) -> *mut u8 {
    unsafe {
        bytes.as_ptr().copy_to_nonoverlapping(dest, bytes.len());
        dest.add(bytes.len())
    }
}

#[inline(always)]
fn copy_to_slice(src: *const u8, slice: &mut [u8]) -> *const u8 {
    unsafe {
        src.copy_to_nonoverlapping(slice.as_mut_ptr(), slice.len());
        src.add(slice.len())
    }
}

/// A trait for values that provide serialization into buffers of bytes.
///
/// # Example
///
/// ```no_run
/// use peek_poke::Poke;
///
/// struct Bar {
///     a: u32,
///     b: u8,
///     c: i16,
/// }
///
/// unsafe impl Poke for Bar {
///     fn max_size() -> usize {
///         <u32>::max_size() + <u8>::max_size() + <i16>::max_size()
///     }
///     unsafe fn poke_into(&self, bytes: *mut u8) -> *mut u8 {
///         let bytes = self.a.poke_into(bytes);
///         let bytes = self.b.poke_into(bytes);
///         self.c.poke_into(bytes)
///     }
/// }
/// ```
///
/// # Safety
///
/// The `Poke` trait is an `unsafe` trait for the reasons, and implementors must
/// ensure that they adhere to these contracts:
///
/// * `max_size()` query and calculations in general must be correct.  Callers
///    of this trait are expected to rely on the contract defined on each
///    method, and implementors must ensure such contracts remain true.
pub unsafe trait Poke {
    /// Return the maximum number of bytes that the serialized version of `Self`
    /// will occupy.
    ///
    /// # Safety
    ///
    /// Implementors of `Poke` guarantee to not write more than the result of
    /// calling `max_size()` into the buffer pointed to by `bytes` when
    /// `poke_into()` is called.
    fn max_size() -> usize;
    /// Serialize into the buffer pointed to by `bytes`.
    ///
    /// Returns a pointer to the next byte after the serialized representation of `Self`.
    ///
    /// # Safety
    ///
    /// This function is unsafe because undefined behavior can result if the
    /// caller does not ensure all of the following:
    ///
    /// * `bytes` must denote a valid pointer to a block of memory.
    ///
    /// * `bytes` must pointer to at least the number of bytes returned by
    ///   `max_size()`.
    unsafe fn poke_into(&self, bytes: *mut u8) -> *mut u8;
}

/// A trait for values that provide deserialization from buffers of bytes.
///
/// # Example
///
/// ```ignore
/// use peek_poke::Peek;
///
/// struct Bar {
///     a: u32,
///     b: u8,
///     c: i16,
/// }
///
/// ...
///
/// impl Peek for Bar {
///     unsafe fn peek_from(&mut self, bytes: *const u8) -> *const u8 {
///         let bytes = self.a.peek_from(bytes);
///         let bytes = self.b.peek_from(bytes);
///         self.c.peek_from(bytes)
///     }
/// }
/// ```
///
/// # Safety
///
/// The `Peek` trait contains unsafe methods for the following reasons, and
/// implementors must ensure that they adhere to these contracts:
///
/// * Callers of this trait are expected to rely on the contract defined on each
///   method, and implementors must ensure that `peek_from()` doesn't read more
///   bytes from `bytes` than is returned by `Peek::max_size()`.
pub trait Peek: Poke {
    /// Deserialize from the buffer pointed to by `bytes`.
    ///
    /// Returns a pointer to the next byte after the unconsumed bytes not used
    /// to deserialize the representation of `Self`.
    ///
    /// # Safety
    ///
    /// This function is unsafe because undefined behavior can result if the
    /// caller does not ensure all of the following:
    ///
    /// * `bytes` must denote a valid pointer to a block of memory.
    ///
    /// * `bytes` must pointer to at least the number of bytes returned by
    ///   `Poke::max_size()`.
    unsafe fn peek_from(&mut self, bytes: *const u8) -> *const u8;
}

macro_rules! impl_poke_for_deref {
    (<$($desc:tt)+) => {
        unsafe impl <$($desc)+ {
            #[inline(always)]
            fn max_size() -> usize {
                <T>::max_size()
            }
            unsafe fn poke_into(&self, bytes: *mut u8) -> *mut u8 {
                (**self).poke_into(bytes)
            }
        }
    }
}

impl_poke_for_deref!(<'a, T: Poke> Poke for &'a T);
impl_poke_for_deref!(<'a, T: Poke> Poke for &'a mut T);

impl<'a, T: Peek> Peek for &'a mut T {
    #[inline(always)]
    unsafe fn peek_from(&mut self, bytes: *const u8) -> *const u8 {
        (**self).peek_from(bytes)
    }
}

macro_rules! impl_for_primitive {
    ($($ty:ty)+) => {
        $(unsafe impl Poke for $ty {
            #[inline(always)]
            fn max_size() -> usize {
                size_of::<Self>()
            }
            #[inline(always)]
            unsafe fn poke_into(&self, bytes: *mut u8) -> *mut u8 {
                let int_bytes = transmute::<_, &[u8; size_of::<$ty>()]>(self);
                copy_bytes_to(int_bytes, bytes)
            }
        }
        impl Peek for $ty {
            #[inline(always)]
            unsafe fn peek_from(&mut self, bytes: *const u8) -> *const u8 {
                let int_bytes = transmute::<_, &mut [u8; size_of::<$ty>()]>(self);
                copy_to_slice(bytes, int_bytes)
            }
        })+
    };
}

impl_for_primitive! {
    i8 i16 i32 i64 isize
    u8 u16 u32 u64 usize
    f32 f64
}

unsafe impl Poke for bool {
    #[inline(always)]
    fn max_size() -> usize {
        <u8>::max_size()
    }
    #[inline]
    unsafe fn poke_into(&self, bytes: *mut u8) -> *mut u8 {
        (*self as u8).poke_into(bytes)
    }
}
impl Peek for bool {
    #[inline]
    unsafe fn peek_from(&mut self, bytes: *const u8) -> *const u8 {
        let mut int_bool = 0u8;
        let ptr = int_bool.peek_from(bytes);
        *self = int_bool != 0;
        ptr
    }
}

unsafe impl<T> Poke for PhantomData<T> {
    #[inline(always)]
    fn max_size() -> usize {
        0
    }
    #[inline(always)]
    unsafe fn poke_into(&self, bytes: *mut u8) -> *mut u8 {
        bytes
    }
}
impl<T> Peek for PhantomData<T> {
    #[inline(always)]
    unsafe fn peek_from(&mut self, bytes: *const u8) -> *const u8 {
        *self = PhantomData;
        bytes
    }
}

unsafe impl<T: Poke> Poke for Option<T> {
    #[inline(always)]
    fn max_size() -> usize {
        <u8>::max_size() + <T>::max_size()
    }
    #[inline]
    unsafe fn poke_into(&self, bytes: *mut u8) -> *mut u8 {
        match self {
            None => 0u8.poke_into(bytes),
            Some(ref v) => {
                let bytes = 1u8.poke_into(bytes);
                let bytes = v.poke_into(bytes);
                bytes
            }
        }
    }
}

#[cfg(feature = "option_copy")]
impl<T: Copy + Peek> Peek for Option<T> {
    #[inline]
    unsafe fn peek_from(&mut self, bytes: *const u8) -> *const u8 {
        let mut variant = 0u8;
        let bytes = variant.peek_from(bytes);
        match variant {
            0 => {
                *self = None;
                bytes
            }
            1 => {
                let mut __0: T = uninitialized();
                let bytes = __0.peek_from(bytes);
                *self = Some(__0);
                bytes
            }
            _ => unreachable!(),
        }
    }
}

#[cfg(feature = "option_default")]
impl<T: Default + Peek> Peek for Option<T> {
    #[inline]
    unsafe fn peek_from(&mut self, bytes: *const u8) -> *const u8 {
        let mut variant = 0u8;
        let bytes = variant.peek_from(bytes);
        match variant {
            0 => {
                *self = None;
                bytes
            }
            1 => {
                let mut __0 = T::default();
                let bytes = __0.peek_from(bytes);
                *self = Some(__0);
                bytes
            }
            _ => unreachable!(),
        }
    }
}

macro_rules! impl_for_arrays {
    ($($len:tt)+) => {
        $(unsafe impl<T: Poke> Poke for [T; $len] {
            fn max_size() -> usize {
                $len * <T>::max_size()
            }
            unsafe fn poke_into(&self, bytes: *mut u8) -> *mut u8 {
                self.iter().fold(bytes, |bytes, e| e.poke_into(bytes))
            }
        }
        impl<T: Peek> Peek for [T; $len] {
            unsafe fn peek_from(&mut self, bytes: *const u8) -> *const u8 {
                self.iter_mut().fold(bytes, |bytes, e| e.peek_from(bytes))
            }
        })+
    }
}

impl_for_arrays! {
    01 02 03 04 05 06 07 08 09 10
    11 12 13 14 15 16 17 18 19 20
    21 22 23 24 25 26 27 28 29 30
    31 32
}

unsafe impl Poke for () {
    fn max_size() -> usize {
        0
    }
    unsafe fn poke_into(&self, bytes: *mut u8) -> *mut u8 {
        bytes
    }
}
impl Peek for () {
    unsafe fn peek_from(&mut self, bytes: *const u8) -> *const u8 {
        *self = ();
        bytes
    }
}

macro_rules! impl_for_tuple {
    ($($n:tt: $ty:ident),+) => {
        unsafe impl<$($ty: Poke),+> Poke for ($($ty,)+) {
            #[inline(always)]
            fn max_size() -> usize {
                0 $(+ <$ty>::max_size())+
            }
            unsafe fn poke_into(&self, bytes: *mut u8) -> *mut u8 {
                $(let bytes = self.$n.poke_into(bytes);)+
                bytes
            }
        }
        impl<$($ty: Peek),+> Peek for ($($ty,)+) {
            unsafe fn peek_from(&mut self, bytes: *const u8) -> *const u8 {
                $(let bytes = self.$n.peek_from(bytes);)+
                bytes
            }
        }
    }
}

impl_for_tuple!(0: A);
impl_for_tuple!(0: A, 1: B);
impl_for_tuple!(0: A, 1: B, 2: C);
impl_for_tuple!(0: A, 1: B, 2: C, 3: D);
impl_for_tuple!(0: A, 1: B, 2: C, 3: D, 4: E);
