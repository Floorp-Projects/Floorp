//! # Portable packed SIMD vectors
//!
//! This crate is proposed for stabilization as `std::packed_simd` in [RFC2366:
//! `std::simd`](https://github.com/rust-lang/rfcs/pull/2366) .
//!
//! The examples available in the
//! [`examples/`](https://github.com/rust-lang-nursery/packed_simd/tree/master/examples)
//! sub-directory of the crate showcase how to use the library in practice.
//!
//! ## Table of contents
//!
//! - [Introduction](#introduction)
//! - [Vector types](#vector-types)
//! - [Conditional operations](#conditional-operations)
//! - [Conversions](#conversions)
//! - [Hardware Features](#hardware-features)
//! - [Performance guide](https://rust-lang-nursery.github.io/packed_simd/perf-guide/)
//!
//! ## Introduction
//!
//! This crate exports [`Simd<[T; N]>`][`Simd`]: a packed vector of `N`
//! elements of type `T` as well as many type aliases for this type: for
//! example, [`f32x4`], which is just an alias for `Simd<[f32; 4]>`.
//!
//! The operations on packed vectors are, by default, "vertical", that is, they
//! are applied to each vector lane in isolation of the others:
//!
//! ```
//! # use packed_simd_2::*;
//! let a = i32x4::new(1, 2, 3, 4);
//! let b = i32x4::new(5, 6, 7, 8);
//! assert_eq!(a + b, i32x4::new(6, 8, 10, 12));
//! ```
//!
//! Many "horizontal" operations are also provided:
//!
//! ```
//! # use packed_simd_2::*;
//! # let a = i32x4::new(1, 2, 3, 4);
//! assert_eq!(a.wrapping_sum(), 10);
//! ```
//!
//! In virtually all architectures vertical operations are fast, while
//! horizontal operations are, by comparison, much slower. That is, the
//! most portably-efficient way of performing a reduction over a slice
//! is to collect the results into a vector using vertical operations,
//! and performing a single horizontal operation at the end:
//!
//! ```
//! # use packed_simd_2::*;
//! fn reduce(x: &[i32]) -> i32 {
//!     assert_eq!(x.len() % 4, 0);
//!     let mut sum = i32x4::splat(0); // [0, 0, 0, 0]
//!     for i in (0..x.len()).step_by(4) {
//!         sum += i32x4::from_slice_unaligned(&x[i..]);
//!     }
//!     sum.wrapping_sum()
//! }
//!
//! let x = [0, 1, 2, 3, 4, 5, 6, 7];
//! assert_eq!(reduce(&x), 28);
//! ```
//!
//! ## Vector types
//!
//! The vector type aliases are named according to the following scheme:
//!
//! > `{element_type}x{number_of_lanes} == Simd<[element_type;
//! number_of_lanes]>`
//!
//! where the following element types are supported:
//!
//! * `i{element_width}`: signed integer
//! * `u{element_width}`: unsigned integer
//! * `f{element_width}`: float
//! * `m{element_width}`: mask (see below)
//! * `*{const,mut} T`: `const` and `mut` pointers
//!
//! ## Basic operations
//!
//! ```
//! # use packed_simd_2::*;
//! // Sets all elements to `0`:
//! let a = i32x4::splat(0);
//!
//! // Reads a vector from a slice:
//! let mut arr = [0, 0, 0, 1, 2, 3, 4, 5];
//! let b = i32x4::from_slice_unaligned(&arr);
//!
//! // Reads the 4-th element of a vector:
//! assert_eq!(b.extract(3), 1);
//!
//! // Returns a new vector where the 4-th element is replaced with `1`:
//! let a = a.replace(3, 1);
//! assert_eq!(a, b);
//!
//! // Writes a vector to a slice:
//! let a = a.replace(2, 1);
//! a.write_to_slice_unaligned(&mut arr[4..]);
//! assert_eq!(arr, [0, 0, 0, 1, 0, 0, 1, 1]);
//! ```
//!
//! ## Conditional operations
//!
//! One often needs to perform an operation on some lanes of the vector. Vector
//! masks, like `m32x4`, allow selecting on which vector lanes an operation is
//! to be performed:
//!
//! ```
//! # use packed_simd_2::*;
//! let a = i32x4::new(1, 1, 2, 2);
//!
//! // Add `1` to the first two lanes of the vector.
//! let m = m16x4::new(true, true, false, false);
//! let a = m.select(a + 1, a);
//! assert_eq!(a, i32x4::splat(2));
//! ```
//!
//! The elements of a vector mask are either `true` or `false`. Here `true`
//! means that a lane is "selected", while `false` means that a lane is not
//! selected.
//!
//! All vector masks implement a `mask.select(a: T, b: T) -> T` method that
//! works on all vectors that have the same number of lanes as the mask. The
//! resulting vector contains the elements of `a` for those lanes for which the
//! mask is `true`, and the elements of `b` otherwise.
//!
//! The example constructs a mask with the first two lanes set to `true` and
//! the last two lanes set to `false`. This selects the first two lanes of `a +
//! 1` and the last two lanes of `a`, producing a vector where the first two
//! lanes have been incremented by `1`.
//!
//! > note: mask `select` can be used on vector types that have the same number
//! > of lanes as the mask. The example shows this by using [`m16x4`] instead
//! > of [`m32x4`]. It is _typically_ more performant to use a mask element
//! > width equal to the element width of the vectors being operated upon.
//! > This is, however, not true for 512-bit wide vectors when targeting
//! > AVX-512, where the most efficient masks use only 1-bit per element.
//!
//! All vertical comparison operations returns masks:
//!
//! ```
//! # use packed_simd_2::*;
//! let a = i32x4::new(1, 1, 3, 3);
//! let b = i32x4::new(2, 2, 0, 0);
//!
//! // ge: >= (Greater Eequal; see also lt, le, gt, eq, ne).
//! let m = a.ge(i32x4::splat(2));
//!
//! if m.any() {
//!     // all / any / none allow coherent control flow
//!     let d = m.select(a, b);
//!     assert_eq!(d, i32x4::new(2, 2, 3, 3));
//! }
//! ```
//!
//! ## Conversions
//!
//! * **lossless widening conversions**: [`From`]/[`Into`] are implemented for
//!   vectors with the same number of lanes when the conversion is value
//! preserving   (same as in `std`).
//!
//! * **safe bitwise conversions**: The cargo feature `into_bits` provides the
//!   `IntoBits/FromBits` traits (`x.into_bits()`). These perform safe bitwise
//!   `transmute`s when all bit patterns of the source type are valid bit
//!   patterns of the target type and are also implemented for the
//!   architecture-specific vector types of `std::arch`. For example, `let x:
//!   u8x8 = m8x8::splat(true).into_bits();` is provided because all `m8x8` bit
//!   patterns are valid `u8x8` bit patterns. However, the opposite is not
//! true,   not all `u8x8` bit patterns are valid `m8x8` bit-patterns, so this
//!   operation cannot be performed safely using `x.into_bits()`; one needs to
//!   use `unsafe { crate::mem::transmute(x) }` for that, making sure that the
//!   value in the `u8x8` is a valid bit-pattern of `m8x8`.
//!
//! * **numeric casts** (`as`): are performed using [`FromCast`]/[`Cast`]
//! (`x.cast()`), just like `as`:
//!
//!   * casting integer vectors whose lane types have the same size (e.g.
//! `i32xN`     -> `u32xN`) is a **no-op**,
//!
//!   * casting from a larger integer to a smaller integer (e.g. `u32xN` ->
//! `u8xN`)     will **truncate**,
//!
//!   * casting from a smaller integer to a larger integer     (e.g. `u8xN` ->
//!     `u32xN`) will:
//!        * **zero-extend** if the source is unsigned, or
//!        * **sign-extend** if the source is signed,
//!
//!   * casting from a float to an integer will **round the float towards
//! zero**,
//!
//!   * casting from an integer to float will produce the floating point
//!     representation of the integer, **rounding to nearest, ties to even**,
//!
//!   * casting from an `f32` to an `f64` is perfect and lossless,
//!
//!   * casting from an `f64` to an `f32` **rounds to nearest, ties to even**.
//!
//!   Numeric casts are not very "precise": sometimes lossy, sometimes value
//!   preserving, etc.
//!
//! ## Hardware Features
//!
//! This crate can use different hardware features based on your configured
//! `RUSTFLAGS`. For example, with no configured `RUSTFLAGS`, `u64x8` on
//! x86_64 will use SSE2 operations like `PCMPEQD`. If you configure
//! `RUSTFLAGS='-C target-feature=+avx2,+avx'` on supported x86_64 hardware
//! the same `u64x8` may use wider AVX2 operations like `VPCMPEQQ`. It is
//! important for performance and for hardware support requirements that
//! you choose an appropriate set of `target-feature` and `target-cpu`
//! options during builds. For more information, see the [Performance
//! guide](https://rust-lang-nursery.github.io/packed_simd/perf-guide/)

#![feature(
    adt_const_params,
    repr_simd,
    rustc_attrs,
    platform_intrinsics,
    stdsimd,
    arm_target_feature,
    link_llvm_intrinsics,
    core_intrinsics,
    stmt_expr_attributes,
    custom_inner_attributes,
)]
#![allow(non_camel_case_types, non_snake_case,
        // FIXME: these types are unsound in C FFI already
        // See https://github.com/rust-lang/rust/issues/53346
        improper_ctypes_definitions,
        incomplete_features,
        clippy::cast_possible_truncation,
        clippy::cast_lossless,
        clippy::cast_possible_wrap,
        clippy::cast_precision_loss,
        // TODO: manually add the `#[must_use]` attribute where appropriate
        clippy::must_use_candidate,
        // This lint is currently broken for generic code
        // See https://github.com/rust-lang/rust-clippy/issues/3410
        clippy::use_self,
        clippy::wrong_self_convention,
        clippy::from_over_into,
)]
#![cfg_attr(test, feature(hashmap_internals))]
#![deny(rust_2018_idioms, clippy::missing_inline_in_public_items)]
#![no_std]

use cfg_if::cfg_if;

cfg_if! {
    if #[cfg(feature = "core_arch")] {
        #[allow(unused_imports)]
        use core_arch as arch;
    } else {
        #[allow(unused_imports)]
        use core::arch;
    }
}

#[cfg(all(target_arch = "wasm32", test))]
use wasm_bindgen_test::*;

#[allow(unused_imports)]
use core::{
    /* arch (handled above), */ cmp, f32, f64, fmt, hash, hint, i128, i16, i32, i64, i8, intrinsics,
    isize, iter, marker, mem, ops, ptr, slice, u128, u16, u32, u64, u8, usize,
};

#[macro_use]
mod testing;
#[macro_use]
mod api;
mod codegen;
mod sealed;

pub use crate::sealed::{Mask, Shuffle, Simd as SimdVector, SimdArray};

/// Packed SIMD vector type.
///
/// # Examples
///
/// ```
/// # use packed_simd_2::Simd;
/// let v = Simd::<[i32; 4]>::new(0, 1, 2, 3);
/// assert_eq!(v.extract(2), 2);
/// ```
#[repr(transparent)]
#[derive(Copy, Clone)]
pub struct Simd<A: sealed::SimdArray>(
    // FIXME: this type should be private,
    // but it currently must be public for the
    // `shuffle!` macro to work: it needs to
    // access the internal `repr(simd)` type
    // to call the shuffle intrinsics.
    #[doc(hidden)] pub <A as sealed::SimdArray>::Tuple,
);

impl<A: sealed::SimdArray> sealed::Seal for Simd<A> {}

/// Wrapper over `T` implementing a lexicoraphical order via the `PartialOrd`
/// and/or `Ord` traits.
#[repr(transparent)]
#[derive(Copy, Clone, Debug)]
#[allow(clippy::missing_inline_in_public_items)]
pub struct LexicographicallyOrdered<T>(T);

mod masks;
pub use self::masks::*;

mod v16;
pub use self::v16::*;

mod v32;
pub use self::v32::*;

mod v64;
pub use self::v64::*;

mod v128;
pub use self::v128::*;

mod v256;
pub use self::v256::*;

mod v512;
pub use self::v512::*;

mod vSize;
pub use self::vSize::*;

mod vPtr;
pub use self::vPtr::*;

pub use self::api::cast::*;

#[cfg(feature = "into_bits")]
pub use self::api::into_bits::*;

// Re-export the shuffle intrinsics required by the `shuffle!` macro.
#[doc(hidden)]
pub use self::codegen::llvm::{
    __shuffle_vector16, __shuffle_vector2, __shuffle_vector32, __shuffle_vector4, __shuffle_vector64,
    __shuffle_vector8,
};

pub(crate) mod llvm {
    pub(crate) use crate::codegen::llvm::*;
}
