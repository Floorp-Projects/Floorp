// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! A group of side offsets, which correspond to top/left/bottom/right for borders, padding,
//! and margins in CSS.

use super::UnknownUnit;
use length::Length;
use num::Zero;
use std::fmt;
use std::ops::Add;
use std::marker::PhantomData;

#[cfg(feature = "unstable")]
use heapsize::HeapSizeOf;

/// A group of side offsets, which correspond to top/left/bottom/right for borders, padding,
/// and margins in CSS, optionally tagged with a unit.
define_matrix! {
    pub struct TypedSideOffsets2D<T, U> {
        pub top: T,
        pub right: T,
        pub bottom: T,
        pub left: T,
    }
}

impl<T: fmt::Debug, U> fmt::Debug for TypedSideOffsets2D<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "({:?},{:?},{:?},{:?})",
               self.top, self.right, self.bottom, self.left)
    }
}

/// The default side offset type with no unit.
pub type SideOffsets2D<T> = TypedSideOffsets2D<T, UnknownUnit>;

impl<T: Copy, U> TypedSideOffsets2D<T, U> {
    /// Constructor taking a scalar for each side.
    pub fn new(top: T, right: T, bottom: T, left: T) -> Self {
        TypedSideOffsets2D {
            top: top,
            right: right,
            bottom: bottom,
            left: left,
            _unit: PhantomData,
        }
    }

    /// Constructor taking a typed Length for each side.
    pub fn from_lengths(top: Length<T, U>,
                        right: Length<T, U>,
                        bottom: Length<T, U>,
                        left: Length<T, U>) -> Self {
        TypedSideOffsets2D::new(top.0, right.0, bottom.0, left.0)
    }

    /// Access self.top as a typed Length instead of a scalar value.
    pub fn top_typed(&self) -> Length<T, U> { Length::new(self.top) }

    /// Access self.right as a typed Length instead of a scalar value.
    pub fn right_typed(&self) -> Length<T, U> { Length::new(self.right) }

    /// Access self.bottom as a typed Length instead of a scalar value.
    pub fn bottom_typed(&self) -> Length<T, U> { Length::new(self.bottom) }

    /// Access self.left as a typed Length instead of a scalar value.
    pub fn left_typed(&self) -> Length<T, U> { Length::new(self.left) }

    /// Constructor setting the same value to all sides, taking a scalar value directly.
    pub fn new_all_same(all: T) -> Self {
        TypedSideOffsets2D::new(all, all, all, all)
    }

    /// Constructor setting the same value to all sides, taking a typed Length.
    pub fn from_length_all_same(all: Length<T, U>) -> Self {
        TypedSideOffsets2D::new_all_same(all.0)
    }
}

impl<T, U> TypedSideOffsets2D<T, U> where T: Add<T, Output=T> + Copy {
    pub fn horizontal(&self) -> T {
        self.left + self.right
    }

    pub fn vertical(&self) -> T {
        self.top + self.bottom
    }

    pub fn horizontal_typed(&self) -> Length<T, U> {
        Length::new(self.horizontal())
    }

    pub fn vertical_typed(&self) -> Length<T, U> {
        Length::new(self.vertical())
    }
}

impl<T, U> Add for TypedSideOffsets2D<T, U> where T : Copy + Add<T, Output=T> {
    type Output = Self;
    fn add(self, other: Self) -> Self {
        TypedSideOffsets2D::new(
            self.top + other.top,
            self.right + other.right,
            self.bottom + other.bottom,
            self.left + other.left,
        )
    }
}

impl<T: Copy + Zero, U> TypedSideOffsets2D<T, U> {
    /// Constructor, setting all sides to zero.
    pub fn zero() -> Self {
        TypedSideOffsets2D::new(
            Zero::zero(),
            Zero::zero(),
            Zero::zero(),
            Zero::zero(),
        )
    }
}

/// A SIMD enabled version of TypedSideOffsets2D specialized for i32.
#[cfg(feature = "unstable")]
#[derive(Clone, Copy, PartialEq)]
#[repr(simd)]
pub struct SideOffsets2DSimdI32 {
    pub top: i32,
    pub bottom: i32,
    pub right: i32,
    pub left: i32,
}

#[cfg(feature = "unstable")]
impl HeapSizeOf for SideOffsets2DSimdI32 {
    fn heap_size_of_children(&self) -> usize { 0 }
}

#[cfg(feature = "unstable")]
impl SideOffsets2DSimdI32 {
    #[inline]
    pub fn new(top: i32, right: i32, bottom: i32, left: i32) -> SideOffsets2DSimdI32 {
        SideOffsets2DSimdI32 {
            top: top,
            bottom: bottom,
            right: right,
            left: left,
        }
    }
}

#[cfg(feature = "unstable")]
impl SideOffsets2DSimdI32 {
    #[inline]
    pub fn new_all_same(all: i32) -> SideOffsets2DSimdI32 {
        SideOffsets2DSimdI32::new(all.clone(), all.clone(), all.clone(), all.clone())
    }
}

#[cfg(feature = "unstable")]
impl SideOffsets2DSimdI32 {
    #[inline]
    pub fn horizontal(&self) -> i32 {
        self.left + self.right
    }

    #[inline]
    pub fn vertical(&self) -> i32 {
        self.top + self.bottom
    }
}

/*impl Add for SideOffsets2DSimdI32 {
    type Output = SideOffsets2DSimdI32;
    #[inline]
    fn add(self, other: SideOffsets2DSimdI32) -> SideOffsets2DSimdI32 {
        self + other // Use SIMD addition
    }
}*/

#[cfg(feature = "unstable")]
impl SideOffsets2DSimdI32 {
    #[inline]
    pub fn zero() -> SideOffsets2DSimdI32 {
        SideOffsets2DSimdI32 {
            top: 0,
            bottom: 0,
            right: 0,
            left: 0,
        }
    }

    #[cfg(not(target_arch = "x86_64"))]
    #[inline]
    pub fn is_zero(&self) -> bool {
        self.top == 0 && self.right == 0 && self.bottom == 0 && self.left == 0
    }

    #[cfg(target_arch = "x86_64")]
    #[inline]
    pub fn is_zero(&self) -> bool {
        let is_zero: bool;
        unsafe {
            asm! {
                "ptest $1, $1
                 setz $0"
                : "=r"(is_zero)
                : "x"(*self)
                :
                : "intel"
            };
        }
        is_zero
    }
}

#[cfg(feature = "unstable")]
#[cfg(test)]
mod tests {
    use super::SideOffsets2DSimdI32;

    #[test]
    fn test_is_zero() {
        assert!(SideOffsets2DSimdI32::new_all_same(0).is_zero());
        assert!(!SideOffsets2DSimdI32::new_all_same(1).is_zero());
        assert!(!SideOffsets2DSimdI32::new(1, 0, 0, 0).is_zero());
        assert!(!SideOffsets2DSimdI32::new(0, 1, 0, 0).is_zero());
        assert!(!SideOffsets2DSimdI32::new(0, 0, 1, 0).is_zero());
        assert!(!SideOffsets2DSimdI32::new(0, 0, 0, 1).is_zero());
    }
}

#[cfg(feature = "unstable")]
#[cfg(bench)]
mod bench {
    use test::BenchHarness;
    use std::num::Zero;
    use rand::{XorShiftRng, Rng};
    use super::SideOffsets2DSimdI32;

    #[cfg(target_arch = "x86")]
    #[cfg(target_arch = "x86_64")]
    #[bench]
    fn bench_naive_is_zero(bh: &mut BenchHarness) {
        fn is_zero(x: &SideOffsets2DSimdI32) -> bool {
            x.top.is_zero() && x.right.is_zero() && x.bottom.is_zero() && x.left.is_zero()
        }
        let mut rng = XorShiftRng::new().unwrap();
        bh.iter(|| is_zero(&rng.gen::<SideOffsets2DSimdI32>()))
    }

    #[bench]
    fn bench_is_zero(bh: &mut BenchHarness) {
        let mut rng = XorShiftRng::new().unwrap();
        bh.iter(|| rng.gen::<SideOffsets2DSimdI32>().is_zero())
    }

    #[bench]
    fn bench_naive_add(bh: &mut BenchHarness) {
        fn add(x: &SideOffsets2DSimdI32, y: &SideOffsets2DSimdI32) -> SideOffsets2DSimdI32 {
            SideOffsets2DSimdI32 {
                top: x.top + y.top,
                right: x.right + y.right,
                bottom: x.bottom + y.bottom,
                left: x.left + y.left,
            }
        }
        let mut rng = XorShiftRng::new().unwrap();
        bh.iter(|| add(&rng.gen::<SideOffsets2DSimdI32>(), &rng.gen::<SideOffsets2DSimdI32>()))
    }

    #[bench]
    fn bench_add(bh: &mut BenchHarness) {
        let mut rng = XorShiftRng::new().unwrap();
        bh.iter(|| rng.gen::<SideOffsets2DSimdI32>() + rng.gen::<SideOffsets2DSimdI32>())
    }
}
