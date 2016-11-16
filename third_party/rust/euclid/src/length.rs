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
use num_traits::NumCast;
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use std::cmp::Ordering;
use std::ops::{Add, Sub, Mul, Div, Neg};
use std::ops::{AddAssign, SubAssign};
use std::marker::PhantomData;
use std::fmt;

/// A one-dimensional distance, with value represented by `T` and unit of measurement `Unit`.
///
/// `T` can be any numeric type, for example a primitive type like u64 or f32.
///
/// `Unit` is not used in the representation of a Length value. It is used only at compile time
/// to ensure that a Length stored with one unit is converted explicitly before being used in an
/// expression that requires a different unit.  It may be a type without values, such as an empty
/// enum.
///
/// You can multiply a Length by a `scale_factor::ScaleFactor` to convert it from one unit to
/// another. See the `ScaleFactor` docs for an example.
// Uncomment the derive, and remove the macro call, once heapsize gets
// PhantomData<T> support.
#[derive(RustcDecodable, RustcEncodable)]
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
    fn deserialize<D>(deserializer: &mut D) -> Result<Length<T, Unit>,D::Error>
                      where D: Deserializer {
        Ok(Length(try!(Deserialize::deserialize(deserializer)), PhantomData))
    }
}

impl<T, Unit> Serialize for Length<T, Unit> where T: Serialize {
    fn serialize<S>(&self, serializer: &mut S) -> Result<(),S::Error> where S: Serializer {
        self.0.serialize(serializer)
    }
}

impl<T, Unit> Length<T, Unit> {
    pub fn new(x: T) -> Length<T, Unit> {
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
    fn eq(&self, other: &Length<T, Unit>) -> bool { self.get().eq(&other.get()) }
}

impl<Unit, T: Clone + PartialOrd> PartialOrd for Length<T, Unit> {
    fn partial_cmp(&self, other: &Length<T, Unit>) -> Option<Ordering> {
        self.get().partial_cmp(&other.get())
    }
}

impl<Unit, T: Clone + Eq> Eq for Length<T, Unit> {}

impl<Unit, T: Clone + Ord> Ord for Length<T, Unit> {
    fn cmp(&self, other: &Length<T, Unit>) -> Ordering { self.get().cmp(&other.get()) }
}

impl<Unit, T: Zero> Zero for Length<T, Unit> {
    fn zero() -> Length<T, Unit> {
        Length::new(Zero::zero())
    }
}

#[cfg(test)]
mod tests {
    use super::Length;
    use scale_factor::ScaleFactor;

    enum Inch {}
    enum Mm {}

    #[test]
    fn test_length() {
        let mm_per_inch: ScaleFactor<f32, Inch, Mm> = ScaleFactor::new(25.4);

        let one_foot: Length<f32, Inch> = Length::new(12.0);
        let two_feet = one_foot.clone() + one_foot.clone();
        let zero_feet = one_foot.clone() - one_foot.clone();

        assert_eq!(one_foot.get(), 12.0);
        assert_eq!(two_feet.get(), 24.0);
        assert_eq!(zero_feet.get(), 0.0);

        assert!(one_foot == one_foot);
        assert!(two_feet != one_foot);

        assert!(zero_feet <  one_foot);
        assert!(zero_feet <= one_foot);
        assert!(two_feet  >  one_foot);
        assert!(two_feet  >= one_foot);

        assert!(  two_feet <= two_feet);
        assert!(  two_feet >= two_feet);
        assert!(!(two_feet >  two_feet));
        assert!(!(two_feet <  two_feet));

        let one_foot_in_mm: Length<f32, Mm> = one_foot * mm_per_inch;

        assert_eq!(one_foot_in_mm, Length::new(304.8));
        assert_eq!(one_foot_in_mm / one_foot, mm_per_inch);

        let back_to_inches: Length<f32, Inch> = one_foot_in_mm / mm_per_inch;
        assert_eq!(one_foot, back_to_inches);

        let int_foot: Length<isize, Inch> = one_foot.cast().unwrap();
        assert_eq!(int_foot.get(), 12);

        let negative_one_foot = -one_foot;
        assert_eq!(negative_one_foot.get(), -12.0);

        let negative_two_feet = -two_feet;
        assert_eq!(negative_two_feet.get(), -24.0);

        let zero_feet: Length<f32, Inch> = Length::new(0.0);
        let negative_zero_feet = -zero_feet;
        assert_eq!(negative_zero_feet.get(), 0.0);
    }

    #[test]
    fn test_addassign() {
        let one_cm: Length<f32, Mm> = Length::new(10.0);
        let mut measurement: Length<f32, Mm> = Length::new(5.0);

        measurement += one_cm;

        assert_eq!(measurement.get(), 15.0);
    }

    #[test]
    fn test_subassign() {
        let one_cm: Length<f32, Mm> = Length::new(10.0);
        let mut measurement: Length<f32, Mm> = Length::new(5.0);

        measurement -= one_cm;

        assert_eq!(measurement.get(), -5.0);
    }
}
