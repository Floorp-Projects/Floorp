#![no_std]
#![warn(missing_docs)]

//! This crate gives small utilities for casting between plain data types.
//!
//! ## Basics
//!
//! Data comes in five basic forms in Rust, so we have five basic casting
//! functions:
//!
//! * `T` uses [`cast`]
//! * `&T` uses [`cast_ref`]
//! * `&mut T` uses [`cast_mut`]
//! * `&[T]` uses [`cast_slice`]
//! * `&mut [T]` uses [`cast_slice_mut`]
//!
//! Some casts will never fail (eg: `cast::<u32, f32>` always works), other
//! casts might fail (eg: `cast_ref::<[u8; 4], u32>` will fail if the reference
//! isn't already aligned to 4). Each casting function has a "try" version which
//! will return a `Result`, and the "normal" version which will simply panic on
//! invalid input.
//!
//! ## Using Your Own Types
//!
//! All the functions here are guarded by the [`Pod`] trait, which is a
//! sub-trait of the [`Zeroable`] trait.
//!
//! If you're very sure that your type is eligible, you can implement those
//! traits for your type and then they'll have full casting support. However,
//! these traits are `unsafe`, and you should carefully read the requirements
//! before adding the them to your own types.
//!
//! ## Features
//!
//! * This crate is core only by default, but if you're using Rust 1.36 or later
//!   you can enable the `extern_crate_alloc` cargo feature for some additional
//!   methods related to `Box` and `Vec`. Note that the `docs.rs` documentation
//!   is always built with `extern_crate_alloc` cargo feature enabled.

#[cfg(target_arch = "x86")]
use core::arch::x86;
#[cfg(target_arch = "x86_64")]
use core::arch::x86_64;
//
use core::{marker::*, mem::*, num::*, ptr::*};

// Used from macros to ensure we aren't using some locally defined name and
// actually are referencing libcore. This also would allow pre-2018 edition
// crates to use our macros, but I'm not sure how important that is.
#[doc(hidden)]
pub use ::core as __core;

macro_rules! impl_unsafe_marker_for_array {
  ( $marker:ident , $( $n:expr ),* ) => {
    $(unsafe impl<T> $marker for [T; $n] where T: $marker {})*
  }
}

#[cfg(feature = "extern_crate_alloc")]
extern crate alloc;
#[cfg(feature = "extern_crate_alloc")]
pub mod allocation;
#[cfg(feature = "extern_crate_alloc")]
pub use allocation::*;

mod zeroable;
pub use zeroable::*;

mod pod;
pub use pod::*;

mod contiguous;
pub use contiguous::*;

mod offset_of;
pub use offset_of::*;

mod transparent;
pub use transparent::*;

/*

Note(Lokathor): We've switched all of the `unwrap` to `match` because there is
apparently a bug: https://github.com/rust-lang/rust/issues/68667
and it doesn't seem to show up in simple godbolt examples but has been reported
as having an impact when there's a cast mixed in with other more complicated
code around it. Rustc/LLVM ends up missing that the `Err` can't ever happen for
particular type combinations, and then it doesn't fully eliminated the panic
possibility code branch.

*/

/// Immediately panics.
#[cold]
#[inline(never)]
fn something_went_wrong(src: &str, err: PodCastError) -> ! {
  // Note(Lokathor): Keeping the panic here makes the panic _formatting_ go
  // here too, which helps assembly readability and also helps keep down
  // the inline pressure.
  panic!("{src}>{err:?}", src = src, err = err)
}

/// Re-interprets `&T` as `&[u8]`.
///
/// Any ZST becomes an empty slice, and in that case the pointer value of that
/// empty slice might not match the pointer value of the input reference.
#[inline]
pub fn bytes_of<T: Pod>(t: &T) -> &[u8] {
  match try_cast_slice::<T, u8>(core::slice::from_ref(t)) {
    Ok(s) => s,
    Err(_) => unreachable!(),
  }
}

/// Re-interprets `&mut T` as `&mut [u8]`.
///
/// Any ZST becomes an empty slice, and in that case the pointer value of that
/// empty slice might not match the pointer value of the input reference.
#[inline]
pub fn bytes_of_mut<T: Pod>(t: &mut T) -> &mut [u8] {
  match try_cast_slice_mut::<T, u8>(core::slice::from_mut(t)) {
    Ok(s) => s,
    Err(_) => unreachable!(),
  }
}

/// Re-interprets `&[u8]` as `&T`.
///
/// ## Panics
///
/// This is [`try_from_bytes`] but will panic on error.
#[inline]
pub fn from_bytes<T: Pod>(s: &[u8]) -> &T {
  match try_from_bytes(s) {
    Ok(t) => t,
    Err(e) => something_went_wrong("from_bytes", e),
  }
}

/// Re-interprets `&mut [u8]` as `&mut T`.
///
/// ## Panics
///
/// This is [`try_from_bytes_mut`] but will panic on error.
#[inline]
pub fn from_bytes_mut<T: Pod>(s: &mut [u8]) -> &mut T {
  match try_from_bytes_mut(s) {
    Ok(t) => t,
    Err(e) => something_went_wrong("from_bytes_mut", e),
  }
}

/// Re-interprets `&[u8]` as `&T`.
///
/// ## Failure
///
/// * If the slice isn't aligned for the new type
/// * If the slice's length isn’t exactly the size of the new type
#[inline]
pub fn try_from_bytes<T: Pod>(s: &[u8]) -> Result<&T, PodCastError> {
  if s.len() != size_of::<T>() {
    Err(PodCastError::SizeMismatch)
  } else if (s.as_ptr() as usize) % align_of::<T>() != 0 {
    Err(PodCastError::AlignmentMismatch)
  } else {
    Ok(unsafe { &*(s.as_ptr() as *const T) })
  }
}

/// Re-interprets `&mut [u8]` as `&mut T`.
///
/// ## Failure
///
/// * If the slice isn't aligned for the new type
/// * If the slice's length isn’t exactly the size of the new type
#[inline]
pub fn try_from_bytes_mut<T: Pod>(
  s: &mut [u8],
) -> Result<&mut T, PodCastError> {
  if s.len() != size_of::<T>() {
    Err(PodCastError::SizeMismatch)
  } else if (s.as_ptr() as usize) % align_of::<T>() != 0 {
    Err(PodCastError::AlignmentMismatch)
  } else {
    Ok(unsafe { &mut *(s.as_mut_ptr() as *mut T) })
  }
}

/// The things that can go wrong when casting between [`Pod`] data forms.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PodCastError {
  /// You tried to cast a slice to an element type with a higher alignment
  /// requirement but the slice wasn't aligned.
  TargetAlignmentGreaterAndInputNotAligned,
  /// If the element size changes then the output slice changes length
  /// accordingly. If the output slice wouldn't be a whole number of elements
  /// then the conversion fails.
  OutputSliceWouldHaveSlop,
  /// When casting a slice you can't convert between ZST elements and non-ZST
  /// elements. When casting an individual `T`, `&T`, or `&mut T` value the
  /// source size and destination size must be an exact match.
  SizeMismatch,
  /// For this type of cast the alignments must be exactly the same and they
  /// were not so now you're sad.
  AlignmentMismatch,
}

/// Cast `T` into `U`
///
/// ## Panics
///
/// This is [`try_cast`] but will panic on error.
#[inline]
pub fn cast<A: Pod, B: Pod>(a: A) -> B {
  if size_of::<A>() == size_of::<B>() {
    // Plz mr compiler, just notice that we can't ever hit Err in this case.
    match try_cast(a) {
      Ok(b) => b,
      Err(_) => unreachable!(),
    }
  } else {
    match try_cast(a) {
      Ok(b) => b,
      Err(e) => something_went_wrong("cast", e),
    }
  }
}

/// Cast `&mut T` into `&mut U`.
///
/// ## Panics
///
/// This is [`try_cast_mut`] but will panic on error.
#[inline]
pub fn cast_mut<A: Pod, B: Pod>(a: &mut A) -> &mut B {
  if size_of::<A>() == size_of::<B>() && align_of::<A>() >= align_of::<B>() {
    // Plz mr compiler, just notice that we can't ever hit Err in this case.
    match try_cast_mut(a) {
      Ok(b) => b,
      Err(_) => unreachable!(),
    }
  } else {
    match try_cast_mut(a) {
      Ok(b) => b,
      Err(e) => something_went_wrong("cast_mut", e),
    }
  }
}

/// Cast `&T` into `&U`.
///
/// ## Panics
///
/// This is [`try_cast_ref`] but will panic on error.
#[inline]
pub fn cast_ref<A: Pod, B: Pod>(a: &A) -> &B {
  if size_of::<A>() == size_of::<B>() && align_of::<A>() >= align_of::<B>() {
    // Plz mr compiler, just notice that we can't ever hit Err in this case.
    match try_cast_ref(a) {
      Ok(b) => b,
      Err(_) => unreachable!(),
    }
  } else {
    match try_cast_ref(a) {
      Ok(b) => b,
      Err(e) => something_went_wrong("cast_ref", e),
    }
  }
}

/// Cast `&[T]` into `&[U]`.
///
/// ## Panics
///
/// This is [`try_cast_slice`] but will panic on error.
#[inline]
pub fn cast_slice<A: Pod, B: Pod>(a: &[A]) -> &[B] {
  match try_cast_slice(a) {
    Ok(b) => b,
    Err(e) => something_went_wrong("cast_slice", e),
  }
}

/// Cast `&mut [T]` into `&mut [U]`.
///
/// ## Panics
///
/// This is [`try_cast_slice_mut`] but will panic on error.
#[inline]
pub fn cast_slice_mut<A: Pod, B: Pod>(a: &mut [A]) -> &mut [B] {
  match try_cast_slice_mut(a) {
    Ok(b) => b,
    Err(e) => something_went_wrong("cast_slice_mut", e),
  }
}

/// As `align_to`, but safe because of the [`Pod`] bound.
#[inline]
pub fn pod_align_to<T: Pod, U: Pod>(vals: &[T]) -> (&[T], &[U], &[T]) {
  unsafe { vals.align_to::<U>() }
}

/// As `align_to_mut`, but safe because of the [`Pod`] bound.
#[inline]
pub fn pod_align_to_mut<T: Pod, U: Pod>(
  vals: &mut [T],
) -> (&mut [T], &mut [U], &mut [T]) {
  unsafe { vals.align_to_mut::<U>() }
}

/// Try to cast `T` into `U`.
///
/// ## Failure
///
/// * If the types don't have the same size this fails.
#[inline]
pub fn try_cast<A: Pod, B: Pod>(a: A) -> Result<B, PodCastError> {
  if size_of::<A>() == size_of::<B>() {
    let mut b = B::zeroed();
    // Note(Lokathor): We copy in terms of `u8` because that allows us to bypass
    // any potential alignment difficulties.
    let ap = &a as *const A as *const u8;
    let bp = &mut b as *mut B as *mut u8;
    unsafe { ap.copy_to_nonoverlapping(bp, size_of::<A>()) };
    Ok(b)
  } else {
    Err(PodCastError::SizeMismatch)
  }
}

/// Try to convert a `&T` into `&U`.
///
/// ## Failure
///
/// * If the reference isn't aligned in the new type
/// * If the source type and target type aren't the same size.
#[inline]
pub fn try_cast_ref<A: Pod, B: Pod>(a: &A) -> Result<&B, PodCastError> {
  // Note(Lokathor): everything with `align_of` and `size_of` will optimize away
  // after monomorphization.
  if align_of::<B>() > align_of::<A>()
    && (a as *const A as usize) % align_of::<B>() != 0
  {
    Err(PodCastError::TargetAlignmentGreaterAndInputNotAligned)
  } else if size_of::<B>() == size_of::<A>() {
    Ok(unsafe { &*(a as *const A as *const B) })
  } else {
    Err(PodCastError::SizeMismatch)
  }
}

/// Try to convert a `&mut T` into `&mut U`.
///
/// As [`try_cast_ref`], but `mut`.
#[inline]
pub fn try_cast_mut<A: Pod, B: Pod>(a: &mut A) -> Result<&mut B, PodCastError> {
  // Note(Lokathor): everything with `align_of` and `size_of` will optimize away
  // after monomorphization.
  if align_of::<B>() > align_of::<A>()
    && (a as *mut A as usize) % align_of::<B>() != 0
  {
    Err(PodCastError::TargetAlignmentGreaterAndInputNotAligned)
  } else if size_of::<B>() == size_of::<A>() {
    Ok(unsafe { &mut *(a as *mut A as *mut B) })
  } else {
    Err(PodCastError::SizeMismatch)
  }
}

/// Try to convert `&[T]` into `&[U]` (possibly with a change in length).
///
/// * `input.as_ptr() as usize == output.as_ptr() as usize`
/// * `input.len() * size_of::<A>() == output.len() * size_of::<B>()`
///
/// ## Failure
///
/// * If the target type has a greater alignment requirement and the input slice
///   isn't aligned.
/// * If the target element type is a different size from the current element
///   type, and the output slice wouldn't be a whole number of elements when
///   accounting for the size change (eg: 3 `u16` values is 1.5 `u32` values, so
///   that's a failure).
/// * Similarly, you can't convert between a
///   [ZST](https://doc.rust-lang.org/nomicon/exotic-sizes.html#zero-sized-types-zsts)
///   and a non-ZST.
#[inline]
pub fn try_cast_slice<A: Pod, B: Pod>(a: &[A]) -> Result<&[B], PodCastError> {
  // Note(Lokathor): everything with `align_of` and `size_of` will optimize away
  // after monomorphization.
  if align_of::<B>() > align_of::<A>()
    && (a.as_ptr() as usize) % align_of::<B>() != 0
  {
    Err(PodCastError::TargetAlignmentGreaterAndInputNotAligned)
  } else if size_of::<B>() == size_of::<A>() {
    Ok(unsafe { core::slice::from_raw_parts(a.as_ptr() as *const B, a.len()) })
  } else if size_of::<A>() == 0 || size_of::<B>() == 0 {
    Err(PodCastError::SizeMismatch)
  } else if core::mem::size_of_val(a) % size_of::<B>() == 0 {
    let new_len = core::mem::size_of_val(a) / size_of::<B>();
    Ok(unsafe { core::slice::from_raw_parts(a.as_ptr() as *const B, new_len) })
  } else {
    Err(PodCastError::OutputSliceWouldHaveSlop)
  }
}

/// Try to convert `&mut [T]` into `&mut [U]` (possibly with a change in length).
///
/// As [`try_cast_slice`], but `&mut`.
#[inline]
pub fn try_cast_slice_mut<A: Pod, B: Pod>(
  a: &mut [A],
) -> Result<&mut [B], PodCastError> {
  // Note(Lokathor): everything with `align_of` and `size_of` will optimize away
  // after monomorphization.
  if align_of::<B>() > align_of::<A>()
    && (a.as_mut_ptr() as usize) % align_of::<B>() != 0
  {
    Err(PodCastError::TargetAlignmentGreaterAndInputNotAligned)
  } else if size_of::<B>() == size_of::<A>() {
    Ok(unsafe {
      core::slice::from_raw_parts_mut(a.as_mut_ptr() as *mut B, a.len())
    })
  } else if size_of::<A>() == 0 || size_of::<B>() == 0 {
    Err(PodCastError::SizeMismatch)
  } else if core::mem::size_of_val(a) % size_of::<B>() == 0 {
    let new_len = core::mem::size_of_val(a) / size_of::<B>();
    Ok(unsafe {
      core::slice::from_raw_parts_mut(a.as_mut_ptr() as *mut B, new_len)
    })
  } else {
    Err(PodCastError::OutputSliceWouldHaveSlop)
  }
}
