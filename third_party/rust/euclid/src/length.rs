// Copyright 2014 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//! A one-dimensional length, tagged with its units.

use scale::Scale;
use num::Zero;

use num_traits::{NumCast, Saturating};
use num::One;
#[cfg(feature = "serde")]
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use core::cmp::Ordering;
use core::ops::{Add, Div, Mul, Neg, Sub};
use core::ops::{AddAssign, DivAssign, MulAssign, SubAssign};
use core::hash::{Hash, Hasher};
use core::marker::PhantomData;
use core::fmt;

/// A one-dimensional distance, with value represented by `T` and unit of measurement `Unit`.
///
/// `T` can be any numeric type, for example a primitive type like `u64` or `f32`.
///
/// `Unit` is not used in the representation of a `Length` value. It is used only at compile time
/// to ensure that a `Length` stored with one unit is converted explicitly before being used in an
/// expression that requires a different unit.  It may be a type without values, such as an empty
/// enum.
///
/// You can multiply a `Length` by a `scale::Scale` to convert it from one unit to
/// another. See the [`Scale`] docs for an example.
///
/// [`Scale`]: struct.Scale.html
#[repr(C)]
pub struct Length<T, Unit>(pub T, #[doc(hidden)] pub PhantomData<Unit>);

impl<T: Clone, U> Clone for Length<T, U> {
    fn clone(&self) -> Self {
        Length(self.0.clone(), PhantomData)
    }
}

impl<T: Copy, U> Copy for Length<T, U> {}

#[cfg(feature = "serde")]
impl<'de, T, U> Deserialize<'de> for Length<T, U>
where
    T: Deserialize<'de>,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        Ok(Length(
            Deserialize::deserialize(deserializer)?,
            PhantomData,
        ))
    }
}

#[cfg(feature = "serde")]
impl<T, U> Serialize for Length<T, U>
where
    T: Serialize,
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        self.0.serialize(serializer)
    }
}

impl<T, U> Length<T, U> {
    #[inline]
    pub const fn new(x: T) -> Self {
        Length(x, PhantomData)
    }
}

impl<T: Clone, U> Length<T, U> {
    pub fn get(&self) -> T {
        self.0.clone()
    }

    /// Cast the unit
    #[inline]
    pub fn cast_unit<V>(&self) -> Length<T, V> {
        Length::new(self.0.clone())
    }
}

impl<T: fmt::Debug, U> fmt::Debug for Length<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl<T: fmt::Display, U> fmt::Display for Length<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl<T: Default, U> Default for Length<T, U> {
    #[inline]
    fn default() -> Self {
        Length::new(Default::default())
    }
}

impl<T, U> Hash for Length<T, U>
    where T: Hash
{
    fn hash<H: Hasher>(&self, h: &mut H) {
        self.0.hash(h);
    }
}

// length + length
impl<U, T: Add<T, Output = T>> Add for Length<T, U> {
    type Output = Length<T, U>;
    fn add(self, other: Length<T, U>) -> Length<T, U> {
        Length::new(self.0 + other.0)
    }
}

// length += length
impl<U, T: AddAssign<T>> AddAssign for Length<T, U> {
    fn add_assign(&mut self, other: Length<T, U>) {
        self.0 += other.0;
    }
}

// length - length
impl<U, T: Sub<T, Output = T>> Sub<Length<T, U>> for Length<T, U> {
    type Output = Length<T, U>;
    fn sub(self, other: Length<T, U>) -> <Self as Sub>::Output {
        Length::new(self.0 - other.0)
    }
}

// length -= length
impl<U, T: SubAssign<T>> SubAssign for Length<T, U> {
    fn sub_assign(&mut self, other: Length<T, U>) {
        self.0 -= other.0;
    }
}

// Saturating length + length and length - length.
impl<U, T: Saturating> Saturating for Length<T, U> {
    fn saturating_add(self, other: Length<T, U>) -> Length<T, U> {
        Length::new(self.0.saturating_add(other.0))
    }

    fn saturating_sub(self, other: Length<T, U>) -> Length<T, U> {
        Length::new(self.0.saturating_sub(other.0))
    }
}

// length / length
impl<Src, Dst, T: Div<T, Output = T>> Div<Length<T, Src>> for Length<T, Dst> {
    type Output = Scale<T, Src, Dst>;
    #[inline]
    fn div(self, other: Length<T, Src>) -> Scale<T, Src, Dst> {
        Scale::new(self.0 / other.0)
    }
}

// length * scalar
impl<T: Mul<T, Output = T>, U> Mul<T> for Length<T, U> {
    type Output = Self;
    #[inline]
    fn mul(self, scale: T) -> Self {
        Length::new(self.0 * scale)
    }
}

// length *= scalar
impl<T: Copy + Mul<T, Output = T>, U> MulAssign<T> for Length<T, U> {
    #[inline]
    fn mul_assign(&mut self, scale: T) {
        *self = *self * scale
    }
}

// length / scalar
impl<T: Div<T, Output = T>, U> Div<T> for Length<T, U> {
    type Output = Self;
    #[inline]
    fn div(self, scale: T) -> Self {
        Length::new(self.0 / scale)
    }
}

// length /= scalar
impl<T: Copy + Div<T, Output = T>, U> DivAssign<T> for Length<T, U> {
    #[inline]
    fn div_assign(&mut self, scale: T) {
        *self = *self / scale
    }
}

// length * scaleFactor
impl<Src, Dst, T: Mul<T, Output = T>> Mul<Scale<T, Src, Dst>> for Length<T, Src> {
    type Output = Length<T, Dst>;
    #[inline]
    fn mul(self, scale: Scale<T, Src, Dst>) -> Length<T, Dst> {
        Length::new(self.0 * scale.0)
    }
}

// length / scaleFactor
impl<Src, Dst, T: Div<T, Output = T>> Div<Scale<T, Src, Dst>> for Length<T, Dst> {
    type Output = Length<T, Src>;
    #[inline]
    fn div(self, scale: Scale<T, Src, Dst>) -> Length<T, Src> {
        Length::new(self.0 / scale.0)
    }
}

// -length
impl<U, T: Neg<Output = T>> Neg for Length<T, U> {
    type Output = Length<T, U>;
    #[inline]
    fn neg(self) -> Length<T, U> {
        Length::new(-self.0)
    }
}

impl<T: NumCast + Clone, U> Length<T, U> {
    /// Cast from one numeric representation to another, preserving the units.
    #[inline]
    pub fn cast<NewT: NumCast>(&self) -> Length<NewT, U> {
        self.try_cast().unwrap()
    }

    /// Fallible cast from one numeric representation to another, preserving the units.
    pub fn try_cast<NewT: NumCast>(&self) -> Option<Length<NewT, U>> {
        NumCast::from(self.get()).map(Length::new)
    }
}

impl<T: PartialEq, U> PartialEq for Length<T, U> {
    fn eq(&self, other: &Self) -> bool {
        self.0.eq(&other.0)
    }
}

impl<T: PartialOrd, U> PartialOrd for Length<T, U> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        self.0.partial_cmp(&other.0)
    }
}

impl<T: Eq, U> Eq for Length<T, U> {}

impl<T: Ord, U> Ord for Length<T, U> {
    fn cmp(&self, other: &Self) -> Ordering {
        self.0.cmp(&other.0)
    }
}

impl<T: Zero, U> Zero for Length<T, U> {
    #[inline]
    fn zero() -> Self {
        Length::new(Zero::zero())
    }
}

impl<T, U> Length<T, U>
where
    T: Copy + One + Add<Output = T> + Sub<Output = T> + Mul<Output = T>,
{
    /// Linearly interpolate between this length and another length.
    ///
    /// When `t` is `One::one()`, returned value equals to `other`,
    /// otherwise equals to `self`.
    #[inline]
    pub fn lerp(&self, other: Self, t: T) -> Self {
        let one_t = T::one() - t;
        Length::new(one_t * self.get() + t * other.get())
    }
}

#[cfg(test)]
mod tests {
    use super::Length;
    use num::Zero;

    use num_traits::Saturating;
    use scale::Scale;
    use core::f32::INFINITY;

    enum Inch {}
    enum Mm {}
    enum Cm {}
    enum Second {}

    #[cfg(feature = "serde")]
    mod serde {
        use super::*;

        extern crate serde_test;
        use self::serde_test::Token;
        use self::serde_test::assert_tokens;

        #[test]
        fn test_length_serde() {
            let one_cm: Length<f32, Mm> = Length::new(10.0);

            assert_tokens(&one_cm, &[Token::F32(10.0)]);
        }
    }

    #[test]
    fn test_clone() {
        // A cloned Length is a separate length with the state matching the
        // original Length at the point it was cloned.
        let mut variable_length: Length<f32, Inch> = Length::new(12.0);

        let one_foot = variable_length.clone();
        variable_length.0 = 24.0;

        assert_eq!(one_foot.get(), 12.0);
        assert_eq!(variable_length.get(), 24.0);
    }

    #[test]
    fn test_get_clones_length_value() {
        // Calling get returns a clone of the Length's value.
        // To test this, we need something clone-able - hence a vector.
        let mut length: Length<Vec<i32>, Inch> = Length::new(vec![1, 2, 3]);

        let value = length.get();
        length.0.push(4);

        assert_eq!(value, vec![1, 2, 3]);
        assert_eq!(length.get(), vec![1, 2, 3, 4]);
    }

    #[test]
    fn test_add() {
        let length1: Length<u8, Mm> = Length::new(250);
        let length2: Length<u8, Mm> = Length::new(5);

        let result = length1 + length2;

        assert_eq!(result.get(), 255);
    }

    #[test]
    fn test_addassign() {
        let one_cm: Length<f32, Mm> = Length::new(10.0);
        let mut measurement: Length<f32, Mm> = Length::new(5.0);

        measurement += one_cm;

        assert_eq!(measurement.get(), 15.0);
    }

    #[test]
    fn test_sub() {
        let length1: Length<u8, Mm> = Length::new(250);
        let length2: Length<u8, Mm> = Length::new(5);

        let result = length1 - length2;

        assert_eq!(result.get(), 245);
    }

    #[test]
    fn test_subassign() {
        let one_cm: Length<f32, Mm> = Length::new(10.0);
        let mut measurement: Length<f32, Mm> = Length::new(5.0);

        measurement -= one_cm;

        assert_eq!(measurement.get(), -5.0);
    }

    #[test]
    fn test_saturating_add() {
        let length1: Length<u8, Mm> = Length::new(250);
        let length2: Length<u8, Mm> = Length::new(6);

        let result = length1.saturating_add(length2);

        assert_eq!(result.get(), 255);
    }

    #[test]
    fn test_saturating_sub() {
        let length1: Length<u8, Mm> = Length::new(5);
        let length2: Length<u8, Mm> = Length::new(10);

        let result = length1.saturating_sub(length2);

        assert_eq!(result.get(), 0);
    }

    #[test]
    fn test_division_by_length() {
        // Division results in a Scale from denominator units
        // to numerator units.
        let length: Length<f32, Cm> = Length::new(5.0);
        let duration: Length<f32, Second> = Length::new(10.0);

        let result = length / duration;

        let expected: Scale<f32, Second, Cm> = Scale::new(0.5);
        assert_eq!(result, expected);
    }

    #[test]
    fn test_multiplication() {
        let length_mm: Length<f32, Mm> = Length::new(10.0);
        let cm_per_mm: Scale<f32, Mm, Cm> = Scale::new(0.1);

        let result = length_mm * cm_per_mm;

        let expected: Length<f32, Cm> = Length::new(1.0);
        assert_eq!(result, expected);
    }

    #[test]
    fn test_multiplication_with_scalar() {
        let length_mm: Length<f32, Mm> = Length::new(10.0);

        let result = length_mm * 2.0;

        let expected: Length<f32, Mm> = Length::new(20.0);
        assert_eq!(result, expected);
    }

    #[test]
    fn test_multiplication_assignment() {
        let mut length: Length<f32, Mm> = Length::new(10.0);

        length *= 2.0;

        let expected: Length<f32, Mm> = Length::new(20.0);
        assert_eq!(length, expected);
    }

    #[test]
    fn test_division_by_scalefactor() {
        let length: Length<f32, Cm> = Length::new(5.0);
        let cm_per_second: Scale<f32, Second, Cm> = Scale::new(10.0);

        let result = length / cm_per_second;

        let expected: Length<f32, Second> = Length::new(0.5);
        assert_eq!(result, expected);
    }

    #[test]
    fn test_division_by_scalar() {
        let length: Length<f32, Cm> = Length::new(5.0);

        let result = length / 2.0;

        let expected: Length<f32, Cm> = Length::new(2.5);
        assert_eq!(result, expected);
    }

    #[test]
    fn test_division_assignment() {
        let mut length: Length<f32, Mm> = Length::new(10.0);

        length /= 2.0;

        let expected: Length<f32, Mm> = Length::new(5.0);
        assert_eq!(length, expected);
    }

    #[test]
    fn test_negation() {
        let length: Length<f32, Cm> = Length::new(5.0);

        let result = -length;

        let expected: Length<f32, Cm> = Length::new(-5.0);
        assert_eq!(result, expected);
    }

    #[test]
    fn test_cast() {
        let length_as_i32: Length<i32, Cm> = Length::new(5);

        let result: Length<f32, Cm> = length_as_i32.cast();

        let length_as_f32: Length<f32, Cm> = Length::new(5.0);
        assert_eq!(result, length_as_f32);
    }

    #[test]
    fn test_equality() {
        let length_5_point_0: Length<f32, Cm> = Length::new(5.0);
        let length_5_point_1: Length<f32, Cm> = Length::new(5.1);
        let length_0_point_1: Length<f32, Cm> = Length::new(0.1);

        assert!(length_5_point_0 == length_5_point_1 - length_0_point_1);
        assert!(length_5_point_0 != length_5_point_1);
    }

    #[test]
    fn test_order() {
        let length_5_point_0: Length<f32, Cm> = Length::new(5.0);
        let length_5_point_1: Length<f32, Cm> = Length::new(5.1);
        let length_0_point_1: Length<f32, Cm> = Length::new(0.1);

        assert!(length_5_point_0 < length_5_point_1);
        assert!(length_5_point_0 <= length_5_point_1);
        assert!(length_5_point_0 <= length_5_point_1 - length_0_point_1);
        assert!(length_5_point_1 > length_5_point_0);
        assert!(length_5_point_1 >= length_5_point_0);
        assert!(length_5_point_0 >= length_5_point_1 - length_0_point_1);
    }

    #[test]
    fn test_zero_add() {
        type LengthCm = Length<f32, Cm>;
        let length: LengthCm = Length::new(5.0);

        let result = length - LengthCm::zero();

        assert_eq!(result, length);
    }

    #[test]
    fn test_zero_division() {
        type LengthCm = Length<f32, Cm>;
        let length: LengthCm = Length::new(5.0);
        let length_zero: LengthCm = Length::zero();

        let result = length / length_zero;

        let expected: Scale<f32, Cm, Cm> = Scale::new(INFINITY);
        assert_eq!(result, expected);
    }
}
