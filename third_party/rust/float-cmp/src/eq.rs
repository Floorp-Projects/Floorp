// Copyright 2014-2018 Optimal Computing (NZ) Ltd.
// Licensed under the MIT license.  See LICENSE for details.

use std::{f32,f64};
use super::Ulps;

/// ApproxEq is a trait for approximate equality comparisons.
pub trait ApproxEq: Sized {
    /// The Margin type defines a margin within which two values are to be
    /// considered approximately equal. It must implement Default so that
    /// approx_eq() can be called on unknown types.
    type Margin: Copy + Default;

    /// This method tests for `self` and `other` values to be approximately equal
    /// within `margin`.
    fn approx_eq<M: Into<Self::Margin>>(self, other: Self, margin: M) -> bool;

    /// This method tests for `self` and `other` values to be not approximately
    /// equal within `margin`.
    fn approx_ne<M: Into<Self::Margin>>(self, other: Self, margin: M) -> bool {
        !self.approx_eq(other, margin)
    }
}

/// This type defines a margin within two f32s might be considered equal
/// and is intended as the associated type for the `ApproxEq` trait.
///
/// Two methods are used to determine approximate equality.
///
/// First an epsilon method is used, considering them approximately equal if they
/// differ by <= `epsilon`.  This will only succeed for very small numbers.
/// Note that it may succeed even if the parameters are of differing signs straddling
/// zero.
///
/// The second method considers how many ULPs (units of least precision, units in
/// the last place, which is the integer number of floating point representations
/// that the parameters are separated by) different the parameters are and considers
/// them approximately equal if this is <= `ulps`. For large floating point numbers,
/// an ULP can be a rather large gap, but this kind of comparison is necessary
/// because floating point operations must round to the nearest representable value
/// and so larger floating point values accumulate larger errors.
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct F32Margin {
    pub epsilon: f32,
    pub ulps: i32
}
impl Default for F32Margin {
    #[inline]
    fn default() -> F32Margin {
        F32Margin {
            epsilon: f32::EPSILON,
            ulps: 4
        }
    }
}
impl F32Margin {
    #[inline]
    pub fn zero() -> F32Margin {
        F32Margin {
            epsilon: 0.0,
            ulps: 0
        }
    }
    pub fn epsilon(self, epsilon: f32) -> Self {
        F32Margin {
            epsilon: epsilon,
            ..self
        }
    }
    pub fn ulps(self, ulps: i32) -> Self {
        F32Margin {
            ulps: ulps,
            ..self
        }
    }
}
impl From<(f32, i32)> for F32Margin {
    fn from(m: (f32, i32)) -> F32Margin {
        F32Margin {
            epsilon: m.0,
            ulps: m.1
        }
    }
}

impl ApproxEq for f32 {
    type Margin = F32Margin;

    fn approx_eq<M: Into<Self::Margin>>(self, other: f32, margin: M) -> bool {
        let margin = margin.into();

        // Check for exact equality first. This is often true, and so we get the
        // performance benefit of only doing one compare in most cases.
        self==other ||

        // Perform epsilon comparison next
            ((self - other).abs() <= margin.epsilon) ||

        {
            // Perform ulps comparion last
            let diff: i32 = self.ulps(&other);
            saturating_abs_i32!(diff) <= margin.ulps
        }
    }
}

#[test]
fn f32_approx_eq_test1() {
    let f: f32 = 0.0_f32;
    let g: f32 = -0.0000000000000005551115123125783_f32;
    assert!(f != g); // Should not be directly equal
    assert!(f.approx_eq(g, (f32::EPSILON, 0)) == true);
}
#[test]
fn f32_approx_eq_test2() {
    let f: f32 = 0.0_f32;
    let g: f32 = -0.0_f32;
    assert!(f.approx_eq(g, (f32::EPSILON, 0)) == true);
}
#[test]
fn f32_approx_eq_test3() {
    let f: f32 = 0.0_f32;
    let g: f32 = 0.00000000000000001_f32;
    assert!(f.approx_eq(g, (f32::EPSILON, 0)) == true);
}
#[test]
fn f32_approx_eq_test4() {
    let f: f32 = 0.00001_f32;
    let g: f32 = 0.00000000000000001_f32;
    assert!(f.approx_eq(g, (f32::EPSILON, 0)) == false);
}
#[test]
fn f32_approx_eq_test5() {
    let f: f32 = 0.1_f32;
    let mut sum: f32 = 0.0_f32;
    for _ in 0_isize..10_isize { sum += f; }
    let product: f32 = f * 10.0_f32;
    assert!(sum != product); // Should not be directly equal:
    println!("Ulps Difference: {}",sum.ulps(&product));
    assert!(sum.approx_eq(product, (f32::EPSILON, 1)) == true);
    assert!(sum.approx_eq(product, F32Margin::zero()) == false);
}
#[test]
fn f32_approx_eq_test6() {
    let x: f32 = 1000000_f32;
    let y: f32 = 1000000.1_f32;
    assert!(x != y); // Should not be directly equal
    assert!(x.approx_eq(y, (0.0, 2)) == true); // 2 ulps does it
    // epsilon method no good here:
    assert!(x.approx_eq(y, (1000.0 * f32::EPSILON, 0)) == false);
}

/// This type defines a margin within two f32s might be considered equal
/// and is intended as the associated type for the `ApproxEq` trait.
///
/// Two methods are used to determine approximate equality.
///
/// First an epsilon method is used, considering them approximately equal if they
/// differ by <= `epsilon`.  This will only succeed for very small numbers.
/// Note that it may succeed even if the parameters are of differing signs straddling
/// zero.
///
/// The second method considers how many ULPs (units of least precision, units in
/// the last place, which is the integer number of floating point representations
/// that the parameters are separated by) different the parameters are and considers
/// them approximately equal if this <= `ulps`. For large floating point numbers,
/// an ULP can be a rather large gap, but this kind of comparison is necessary
/// because floating point operations must round to the nearest representable value
/// and so larger floating point values accumulate larger errors.
#[derive(Debug, Clone, Copy)]
pub struct F64Margin {
    pub epsilon: f64,
    pub ulps: i64
}
impl Default for F64Margin {
    #[inline]
    fn default() -> F64Margin {
        F64Margin {
            epsilon: f64::EPSILON,
            ulps: 4
        }
    }
}
impl F64Margin {
    #[inline]
    pub fn zero() -> F64Margin {
        F64Margin {
            epsilon: 0.0,
            ulps: 0
        }
    }
    pub fn epsilon(self, epsilon: f64) -> Self {
        F64Margin {
            epsilon: epsilon,
            ..self
        }
    }
    pub fn ulps(self, ulps: i64) -> Self {
        F64Margin {
            ulps: ulps,
            ..self
        }
    }
}
impl From<(f64, i64)> for F64Margin {
    fn from(m: (f64, i64)) -> F64Margin {
        F64Margin {
            epsilon: m.0,
            ulps: m.1
        }
    }
}

impl ApproxEq for f64 {
    type Margin = F64Margin;

    fn approx_eq<M: Into<Self::Margin>>(self, other: f64, margin: M) -> bool {
        let margin = margin.into();

        // Check for exact equality first. This is often true, and so we get the
        // performance benefit of only doing one compare in most cases.
        self==other ||

        // Perform epsilon comparison next
            ((self - other).abs() <= margin.epsilon) ||

        {
            // Perform ulps comparion last
            let diff: i64 = self.ulps(&other);
            saturating_abs_i64!(diff) <= margin.ulps
        }
    }
}

#[test]
fn f64_approx_eq_test1() {
    let f: f64 = 0.0_f64;
    let g: f64 = -0.0000000000000005551115123125783_f64;
    assert!(f != g); // Should not be directly equal
    assert!(f.approx_eq(g, (3.0 * f64::EPSILON, 0)) == true); // 3e is enough
    // ulps test wont ever call these equal
}
#[test]
fn f64_approx_eq_test2() {
    let f: f64 = 0.0_f64;
    let g: f64 = -0.0_f64;
    assert!(f.approx_eq(g, (f64::EPSILON, 0)) == true);
}
#[test]
fn f64_approx_eq_test3() {
    let f: f64 = 0.0_f64;
    let g: f64 = 1e-17_f64;
    assert!(f.approx_eq(g, (f64::EPSILON, 0)) == true);
}
#[test]
fn f64_approx_eq_test4() {
    let f: f64 = 0.00001_f64;
    let g: f64 = 0.00000000000000001_f64;
    assert!(f.approx_eq(g, (f64::EPSILON, 0)) == false);
}
#[test]
fn f64_approx_eq_test5() {
    let f: f64 = 0.1_f64;
    let mut sum: f64 = 0.0_f64;
    for _ in 0_isize..10_isize { sum += f; }
    let product: f64 = f * 10.0_f64;
    assert!(sum != product); // Should not be directly equal:
    println!("Ulps Difference: {}",sum.ulps(&product));
    assert!(sum.approx_eq(product, (f64::EPSILON, 0)) == true);
    assert!(sum.approx_eq(product, (0.0, 1)) == true);
}
#[test]
fn f64_approx_eq_test6() {
    let x: f64 = 1000000_f64;
    let y: f64 = 1000000.0000000003_f64;
    assert!(x != y); // Should not be directly equal
    println!("Ulps Difference: {}",x.ulps(&y));
    assert!(x.approx_eq(y, (0.0, 3)) == true);
}
#[test]
fn f64_code_triggering_issue_20() {
    assert_eq!((-25.0f64).approx_eq(25.0, (0.00390625, 1)), false);
}
