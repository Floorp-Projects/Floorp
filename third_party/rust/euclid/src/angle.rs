// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use num_traits::{Float, FloatConst, Zero};
use core::ops::{Add, AddAssign, Div, DivAssign, Mul, MulAssign, Neg, Rem, Sub, SubAssign};
use core::cmp::{Eq, PartialEq};
use core::hash::{Hash};
use trig::Trig;
#[cfg(feature = "serde")]
use serde;

/// An angle in radians
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Angle<T> {
    pub radians: T,
}

impl<T> Angle<T> {
    #[inline]
    pub fn radians(radians: T) -> Self {
        Angle { radians }
    }

    #[inline]
    pub fn get(self) -> T {
        self.radians
    }
}

impl<T> Angle<T>
where
    T: Trig,
{
    #[inline]
    pub fn degrees(deg: T) -> Self {
        Angle {
            radians: T::degrees_to_radians(deg),
        }
    }

    #[inline]
    pub fn to_degrees(self) -> T {
        T::radians_to_degrees(self.radians)
    }
}

impl<T> Angle<T>
where
    T: Rem<Output = T> + Sub<Output = T> + Add<Output = T> + Zero + FloatConst + PartialOrd + Copy,
{
    /// Returns this angle in the [0..2*PI[ range.
    pub fn positive(&self) -> Self {
        let two_pi = T::PI() + T::PI();
        let mut a = self.radians % two_pi;
        if a < T::zero() {
            a = a + two_pi;
        }
        Angle::radians(a)
    }

    /// Returns this angle in the ]-PI..PI] range.
    pub fn signed(&self) -> Self {
        Angle::pi() - (Angle::pi() - *self).positive()
    }
}

impl<T> Angle<T>
where
    T: Float,
{
    /// Returns (sin(self), cos(self)).
    pub fn sin_cos(self) -> (T, T) {
        self.radians.sin_cos()
    }
}

impl<T> Angle<T>
where
    T: Zero,
{
    pub fn zero() -> Self {
        Angle::radians(T::zero())
    }
}

impl<T> Angle<T>
where
    T: FloatConst + Add<Output = T>,
{
    pub fn pi() -> Self {
        Angle::radians(T::PI())
    }

    pub fn two_pi() -> Self {
        Angle::radians(T::PI() + T::PI())
    }

    pub fn frac_pi_2() -> Self {
        Angle::radians(T::FRAC_PI_2())
    }

    pub fn frac_pi_3() -> Self {
        Angle::radians(T::FRAC_PI_3())
    }

    pub fn frac_pi_4() -> Self {
        Angle::radians(T::FRAC_PI_4())
    }
}

impl<T: Add<T, Output = T>> Add for Angle<T> {
    type Output = Angle<T>;
    fn add(self, other: Angle<T>) -> Angle<T> {
        Angle::radians(self.radians + other.radians)
    }
}

impl<T: AddAssign<T>> AddAssign for Angle<T> {
    fn add_assign(&mut self, other: Angle<T>) {
        self.radians += other.radians;
    }
}

impl<T: Sub<T, Output = T>> Sub<Angle<T>> for Angle<T> {
    type Output = Angle<T>;
    fn sub(self, other: Angle<T>) -> <Self as Sub>::Output {
        Angle::radians(self.radians - other.radians)
    }
}

impl<T: SubAssign<T>> SubAssign for Angle<T> {
    fn sub_assign(&mut self, other: Angle<T>) {
        self.radians -= other.radians;
    }
}

impl<T: Div<T, Output = T>> Div<Angle<T>> for Angle<T> {
    type Output = T;
    #[inline]
    fn div(self, other: Angle<T>) -> T {
        self.radians / other.radians
    }
}

impl<T: Div<T, Output = T>> Div<T> for Angle<T> {
    type Output = Angle<T>;
    #[inline]
    fn div(self, factor: T) -> Angle<T> {
        Angle::radians(self.radians / factor)
    }
}

impl<T: DivAssign<T>> DivAssign<T> for Angle<T> {
    fn div_assign(&mut self, factor: T) {
        self.radians /= factor;
    }
}

impl<T: Mul<T, Output = T>> Mul<T> for Angle<T> {
    type Output = Angle<T>;
    #[inline]
    fn mul(self, factor: T) -> Angle<T> {
        Angle::radians(self.radians * factor)
    }
}

impl<T: MulAssign<T>> MulAssign<T> for Angle<T> {
    fn mul_assign(&mut self, factor: T) {
        self.radians *= factor;
    }
}

impl<T: Neg<Output = T>> Neg for Angle<T> {
    type Output = Self;
    fn neg(self) -> Self {
        Angle::radians(-self.radians)
    }
}

#[test]
fn wrap_angles() {
    use approxeq::ApproxEq;
    use core::f32::consts::{FRAC_PI_2, PI};

    assert!(Angle::radians(0.0).positive().radians.approx_eq(&0.0));
    assert!(
        Angle::radians(FRAC_PI_2)
            .positive()
            .radians
            .approx_eq(&FRAC_PI_2)
    );
    assert!(
        Angle::radians(-FRAC_PI_2)
            .positive()
            .radians
            .approx_eq(&(3.0 * FRAC_PI_2))
    );
    assert!(
        Angle::radians(3.0 * FRAC_PI_2)
            .positive()
            .radians
            .approx_eq(&(3.0 * FRAC_PI_2))
    );
    assert!(
        Angle::radians(5.0 * FRAC_PI_2)
            .positive()
            .radians
            .approx_eq(&FRAC_PI_2)
    );
    assert!(Angle::radians(2.0 * PI).positive().radians.approx_eq(&0.0));
    assert!(Angle::radians(-2.0 * PI).positive().radians.approx_eq(&0.0));
    assert!(Angle::radians(PI).positive().radians.approx_eq(&PI));
    assert!(Angle::radians(-PI).positive().radians.approx_eq(&PI));

    assert!(
        Angle::radians(FRAC_PI_2)
            .signed()
            .radians
            .approx_eq(&FRAC_PI_2)
    );
    assert!(
        Angle::radians(3.0 * FRAC_PI_2)
            .signed()
            .radians
            .approx_eq(&-FRAC_PI_2)
    );
    assert!(
        Angle::radians(5.0 * FRAC_PI_2)
            .signed()
            .radians
            .approx_eq(&FRAC_PI_2)
    );
    assert!(Angle::radians(2.0 * PI).signed().radians.approx_eq(&0.0));
    assert!(Angle::radians(-2.0 * PI).signed().radians.approx_eq(&0.0));
    assert!(Angle::radians(-PI).signed().radians.approx_eq(&PI));
    assert!(Angle::radians(PI).signed().radians.approx_eq(&PI));
}
