// Copyright 2014 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//! A one-dimensional length, tagged with its units.

use scale_factor::ScaleFactor;
use num::Zero;

use heapsize::HeapSizeOf;
use num_traits::{NumCast, Saturating};
use num::One;
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use std::cmp::Ordering;
use std::ops::{Add, Sub, Mul, Div, Neg};
use std::ops::{AddAssign, SubAssign};
use std::marker::PhantomData;
use std::fmt;

/// A one-dimensional distance, with value represented by `T` and unit of measurement `Unit`.
///
/// `T` can be any numeric type, for example a primitive type like `u64` or `f32`.
///
/// `Unit` is not used in the representation of a `Length` value. It is used only at compile time
/// to ensure that a `Length` stored with one unit is converted explicitly before being used in an
/// expression that requires a different unit.  It may be a type without values, such as an empty
/// enum.
///
/// You can multiply a `Length` by a `scale_factor::ScaleFactor` to convert it from one unit to
/// another. See the `ScaleFactor` docs for an example.
// Uncomment the derive, and remove the macro call, once heapsize gets
// PhantomData<T> support.
#[repr(C)]
pub struct Length<T, Unit>(pub T, PhantomData<Unit>);

impl<T: Clone, Unit> Clone for Length<T, Unit> {
    fn clone(&self) -> Self {
        Length(self.0.clone(), PhantomData)
    }
}

impl<T: Copy, Unit> Copy for Length<T, Unit> {}

impl<Unit, T: HeapSizeOf> HeapSizeOf for Length<T, Unit> {
    fn heap_size_of_children(&self) -> usize {
        self.0.heap_size_of_children()
    }
}

impl<Unit, T> Deserialize for Length<T, Unit> where T: Deserialize {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
                      where D: Deserializer {
        Ok(Length(try!(Deserialize::deserialize(deserializer)), PhantomData))
    }
}

impl<T, Unit> Serialize for Length<T, Unit> where T: Serialize {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: Serializer {
        self.0.serialize(serializer)
    }
}

impl<T, Unit> Length<T, Unit> {
    pub fn new(x: T) -> Self {
        Length(x, PhantomData)
    }
}

impl<Unit, T: Clone> Length<T, Unit> {
    pub fn get(&self) -> T {
        self.0.clone()
    }
}

impl<T: fmt::Debug + Clone, U> fmt::Debug for Length<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.get().fmt(f)
    }
}

impl<T: fmt::Display + Clone, U> fmt::Display for Length<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.get().fmt(f)
    }
}

// length + length
impl<U, T: Clone + Add<T, Output=T>> Add for Length<T, U> {
    type Output = Length<T, U>;
    fn add(self, other: Length<T, U>) -> Length<T, U> {
        Length::new(self.get() + other.get())
    }
}

// length += length
impl<U, T: Clone + AddAssign<T>> AddAssign for Length<T, U> {
    fn add_assign(&mut self, other: Length<T, U>) {
        self.0 += other.get();
    }
}

// length - length
impl<U, T: Clone + Sub<T, Output=T>> Sub<Length<T, U>> for Length<T, U> {
    type Output = Length<T, U>;
    fn sub(self, other: Length<T, U>) -> <Self as Sub>::Output {
        Length::new(self.get() - other.get())
    }
}

// length -= length
impl<U, T: Clone + SubAssign<T>> SubAssign for Length<T, U> {
    fn sub_assign(&mut self, other: Length<T, U>) {
        self.0 -= other.get();
    }
}

// Saturating length + length and length - length.
impl<U, T: Clone + Saturating> Saturating for Length<T, U> {
    fn saturating_add(self, other: Length<T, U>) -> Length<T, U> {
        Length::new(self.get().saturating_add(other.get()))
    }

    fn saturating_sub(self, other: Length<T, U>) -> Length<T, U> {
        Length::new(self.get().saturating_sub(other.get()))
    }
}

// length / length
impl<Src, Dst, T: Clone + Div<T, Output=T>> Div<Length<T, Src>> for Length<T, Dst> {
    type Output = ScaleFactor<T, Src, Dst>;
    #[inline]
    fn div(self, other: Length<T, Src>) -> ScaleFactor<T, Src, Dst> {
        ScaleFactor::new(self.get() / other.get())
    }
}

// length * scaleFactor
impl<Src, Dst, T: Clone + Mul<T, Output=T>> Mul<ScaleFactor<T, Src, Dst>> for Length<T, Src> {
    type Output = Length<T, Dst>;
    #[inline]
    fn mul(self, scale: ScaleFactor<T, Src, Dst>) -> Length<T, Dst> {
        Length::new(self.get() * scale.get())
    }
}

// length / scaleFactor
impl<Src, Dst, T: Clone + Div<T, Output=T>> Div<ScaleFactor<T, Src, Dst>> for Length<T, Dst> {
    type Output = Length<T, Src>;
    #[inline]
    fn div(self, scale: ScaleFactor<T, Src, Dst>) -> Length<T, Src> {
        Length::new(self.get() / scale.get())
    }
}

// -length
impl <U, T:Clone + Neg<Output=T>> Neg for Length<T, U> {
    type Output = Length<T, U>;
    #[inline]
    fn neg(self) -> Length<T, U> {
        Length::new(-self.get())
    }
}

impl<Unit, T0: NumCast + Clone> Length<T0, Unit> {
    /// Cast from one numeric representation to another, preserving the units.
    pub fn cast<T1: NumCast + Clone>(&self) -> Option<Length<T1, Unit>> {
        NumCast::from(self.get()).map(Length::new)
    }
}

impl<Unit, T: Clone + PartialEq> PartialEq for Length<T, Unit> {
    fn eq(&self, other: &Self) -> bool { self.get().eq(&other.get()) }
}

impl<Unit, T: Clone + PartialOrd> PartialOrd for Length<T, Unit> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        self.get().partial_cmp(&other.get())
    }
}

impl<Unit, T: Clone + Eq> Eq for Length<T, Unit> {}

impl<Unit, T: Clone + Ord> Ord for Length<T, Unit> {
    fn cmp(&self, other: &Self) -> Ordering { self.get().cmp(&other.get()) }
}

impl<Unit, T: Zero> Zero for Length<T, Unit> {
    fn zero() -> Self {
        Length::new(Zero::zero())
    }
}

impl<T, U> Length<T, U>
where T: Copy + One + Add<Output=T> + Sub<Output=T> + Mul<Output=T> {
    /// Linearly interpolate between this length and another length.
    ///
    /// `t` is expected to be between zero and one.
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

    use heapsize::HeapSizeOf;
    use num_traits::Saturating;
    use scale_factor::ScaleFactor;
    use std::f32::INFINITY;

    extern crate serde_test;
    use self::serde_test::Token;
    use self::serde_test::assert_tokens;

    enum Inch {}
    enum Mm {}
    enum Cm {}
    enum Second {}

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
    fn test_heapsizeof_builtins() {
        // Heap size of built-ins is zero by default.
        let one_foot: Length<f32, Inch> = Length::new(12.0);

        let heap_size_length_f32 = one_foot.heap_size_of_children();

        assert_eq!(heap_size_length_f32, 0);
    }

    #[test]
    fn test_heapsizeof_length_vector() {
        // Heap size of any Length is just the heap size of the length value.
        for n in 0..5 {
            let length: Length<Vec<f32>, Inch> = Length::new(Vec::with_capacity(n));

            assert_eq!(length.heap_size_of_children(), length.0.heap_size_of_children());
        }
    }

    #[test]
    fn test_length_serde() {
        let one_cm: Length<f32, Mm> = Length::new(10.0);

        assert_tokens(&one_cm, &[Token::F32(10.0)]);
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
    fn test_fmt_debug() {
        // Debug and display format the value only.
        let one_cm: Length<f32, Mm> = Length::new(10.0);

        let result = format!("{:?}", one_cm);

        assert_eq!(result, "10");
    }

    #[test]
    fn test_fmt_display() {
        // Debug and display format the value only.
        let one_cm: Length<f32, Mm> = Length::new(10.0);

        let result = format!("{}", one_cm);

        assert_eq!(result, "10");
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
        // Division results in a ScaleFactor from denominator units
        // to numerator units.
        let length: Length<f32, Cm> = Length::new(5.0);
        let duration: Length<f32, Second> = Length::new(10.0);

        let result = length / duration;

        let expected: ScaleFactor<f32, Second, Cm> = ScaleFactor::new(0.5);
        assert_eq!(result, expected);
    }

    #[test]
    fn test_multiplication() {
        let length_mm: Length<f32, Mm> = Length::new(10.0);
        let cm_per_mm: ScaleFactor<f32, Mm, Cm> = ScaleFactor::new(0.1);

        let result = length_mm * cm_per_mm;

        let expected: Length<f32, Cm> = Length::new(1.0);
        assert_eq!(result, expected);
    }

    #[test]
    fn test_division_by_scalefactor() {
        let length: Length<f32, Cm> = Length::new(5.0);
        let cm_per_second: ScaleFactor<f32, Second, Cm> = ScaleFactor::new(10.0);

        let result = length / cm_per_second;

        let expected: Length<f32, Second> = Length::new(0.5);
        assert_eq!(result, expected);
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

        let result: Length<f32, Cm> = length_as_i32.cast().unwrap();

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

        let expected: ScaleFactor<f32, Cm, Cm> = ScaleFactor::new(INFINITY);
        assert_eq!(result, expected);
    }
}
