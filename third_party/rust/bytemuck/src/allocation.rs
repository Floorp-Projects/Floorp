//! Stuff to boost things in the `alloc` crate.
//!
//! * You must enable the `extern_crate_alloc` feature of `bytemuck` or you will
//!   not be able to use this module!

use super::*;
use alloc::{
  alloc::{alloc_zeroed, Layout},
  boxed::Box,
  vec::Vec,
};

/// As [`try_cast_box`](try_cast_box), but unwraps for you.
#[inline]
pub fn cast_box<A: Pod, B: Pod>(input: Box<A>) -> Box<B> {
  try_cast_box(input).map_err(|(e, _v)| e).unwrap()
}

/// Attempts to cast the content type of a [`Box`](alloc::boxed::Box).
///
/// On failure you get back an error along with the starting `Box`.
///
/// ## Failure
///
/// * The start and end content type of the `Box` must have the exact same
///   alignment.
/// * The start and end size of the `Box` must have the exact same size.
#[inline]
pub fn try_cast_box<A: Pod, B: Pod>(
  input: Box<A>,
) -> Result<Box<B>, (PodCastError, Box<A>)> {
  if align_of::<A>() != align_of::<B>() {
    Err((PodCastError::AlignmentMismatch, input))
  } else if size_of::<A>() != size_of::<B>() {
    Err((PodCastError::SizeMismatch, input))
  } else {
    // Note(Lokathor): This is much simpler than with the Vec casting!
    let ptr: *mut B = Box::into_raw(input) as *mut B;
    Ok(unsafe { Box::from_raw(ptr) })
  }
}

/// Allocates a `Box<T>` with all of the contents being zeroed out.
///
/// This uses the global allocator to create a zeroed allocation and _then_
/// turns it into a Box. In other words, it's 100% assured that the zeroed data
/// won't be put temporarily on the stack. You can make a box of any size
/// without fear of a stack overflow.
///
/// ## Failure
///
/// This fails if the allocation fails.
#[inline]
pub fn try_zeroed_box<T: Zeroable>() -> Result<Box<T>, ()> {
  if size_of::<T>() == 0 {
    return Ok(Box::new(T::zeroed()));
  }
  let layout =
    Layout::from_size_align(size_of::<T>(), align_of::<T>()).unwrap();
  let ptr = unsafe { alloc_zeroed(layout) };
  if ptr.is_null() {
    // we don't know what the error is because `alloc_zeroed` is a dumb API
    Err(())
  } else {
    Ok(unsafe { Box::<T>::from_raw(ptr as *mut T) })
  }
}

/// As [`try_zeroed_box`], but unwraps for you.
#[inline]
pub fn zeroed_box<T: Zeroable>() -> Box<T> {
  try_zeroed_box().unwrap()
}

/// As [`try_cast_vec`](try_cast_vec), but unwraps for you.
#[inline]
pub fn cast_vec<A: Pod, B: Pod>(input: Vec<A>) -> Vec<B> {
  try_cast_vec(input).map_err(|(e, _v)| e).unwrap()
}

/// Attempts to cast the content type of a [`Vec`](alloc::vec::Vec).
///
/// On failure you get back an error along with the starting `Vec`.
///
/// ## Failure
///
/// * The start and end content type of the `Vec` must have the exact same
///   alignment.
/// * The start and end size of the `Vec` must have the exact same size.
/// * In the future this second restriction might be lessened by having the
///   capacity and length get adjusted during transmutation, but for now it's
///   absolute.
#[inline]
pub fn try_cast_vec<A: Pod, B: Pod>(
  input: Vec<A>,
) -> Result<Vec<B>, (PodCastError, Vec<A>)> {
  if align_of::<A>() != align_of::<B>() {
    Err((PodCastError::AlignmentMismatch, input))
  } else if size_of::<A>() != size_of::<B>() {
    // Note(Lokathor): Under some conditions it would be possible to cast
    // between Vec content types of the same alignment but different sizes by
    // changing the capacity and len values in the output Vec. However, we will
    // not attempt that for now.
    Err((PodCastError::SizeMismatch, input))
  } else {
    // Note(Lokathor): First we record the length and capacity, which don't have
    // any secret provenance metadata.
    let length: usize = input.len();
    let capacity: usize = input.capacity();
    // Note(Lokathor): Next we "pre-forget" the old Vec by wrapping with
    // ManuallyDrop, because if we used `core::mem::forget` after taking the
    // pointer then that would invalidate our pointer. In nightly there's a
    // "into raw parts" method, which we can switch this too eventually.
    let mut manual_drop_vec = ManuallyDrop::new(input);
    let vec_ptr: *mut A = manual_drop_vec.as_mut_ptr();
    let ptr: *mut B = vec_ptr as *mut B;
    Ok(unsafe { Vec::from_raw_parts(ptr, length, capacity) })
  }
}
