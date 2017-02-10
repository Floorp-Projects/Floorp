#![cfg_attr(test, deny(warnings))]
#![deny(missing_docs)]

//! Wrappers for total order on Floats.

extern crate num_traits;
extern crate unreachable;

use std::cmp::Ordering;
use std::error::Error;
use std::ops::{Add, AddAssign, Deref, DerefMut, Div, DivAssign, Mul, MulAssign, Neg, Rem, RemAssign, Sub, SubAssign};
use std::hash::{Hash, Hasher};
use std::fmt;
use std::io;
use unreachable::unreachable;
use num_traits::Float;

/// A wrapper around Floats providing an implementation of Ord and Hash.
///
/// NaN is sorted as *greater* than all other values and *equal*
/// to itself, in contradiction with the IEEE standard.
#[derive(PartialOrd, Debug, Default, Clone, Copy)]
pub struct OrderedFloat<T: Float>(pub T);

impl<T: Float> OrderedFloat<T> {
    /// Get the value out.
    pub fn into_inner(self) -> T {
        let OrderedFloat(val) = self;
        val
    }
}

impl<T: Float> AsRef<T> for OrderedFloat<T> {
    fn as_ref(&self) -> &T {
        let OrderedFloat(ref val) = *self;
        val
    }
}

impl<T: Float> AsMut<T> for OrderedFloat<T> {
    fn as_mut(&mut self) -> &mut T {
        let OrderedFloat(ref mut val) = *self;
        val
    }
}

impl<T: Float + PartialOrd> Ord for OrderedFloat<T> {
    fn cmp(&self, other: &OrderedFloat<T>) -> Ordering {
        match self.partial_cmp(&other) {
            Some(ordering) => ordering,
            None => {
                if self.as_ref().is_nan() {
                    if other.as_ref().is_nan() {
                        Ordering::Equal
                    } else {
                        Ordering::Greater
                    }
                } else {
                    Ordering::Less
                }
            }
        }
    }
}

impl<T: Float + PartialEq> PartialEq for OrderedFloat<T> {
    fn eq(&self, other: &OrderedFloat<T>) -> bool {
        if self.as_ref().is_nan() {
            if other.as_ref().is_nan() {
                true
            } else {
                false
            }
        } else if other.as_ref().is_nan() {
            false
        } else {
            self.as_ref() == other.as_ref()
        }
    }
}

impl<T: Float> Hash for OrderedFloat<T> {
    fn hash<H: Hasher>(&self, state: &mut H) {
        if self.is_nan() {
            // normalize to one representation of NaN
            hash_float(&T::nan(), state)
        } else {
            hash_float(self.as_ref(), state)
        }
    }
}

impl<T: Float + fmt::Display> fmt::Display for OrderedFloat<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.as_ref().fmt(f)
    }
}

impl Into<f32> for OrderedFloat<f32> {
    fn into(self) -> f32 {
        self.into_inner()
    }
}

impl Into<f64> for OrderedFloat<f64> {
    fn into(self) -> f64 {
        self.into_inner()
    }
}

impl<T: Float> From<T> for OrderedFloat<T> {
    fn from(val: T) -> Self {
        OrderedFloat(val)
    }
}

impl<T: Float> Deref for OrderedFloat<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.as_ref()
    }
}

impl<T: Float> DerefMut for OrderedFloat<T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.as_mut()
    }
}

impl<T: Float + PartialEq> Eq for OrderedFloat<T> { }

/// A wrapper around Floats providing an implementation of Ord and Hash.
///
/// A NaN value cannot be stored in this type.
#[derive(PartialOrd, PartialEq, Debug, Default, Clone, Copy)]
pub struct NotNaN<T: Float>(T);

impl<T: Float> NotNaN<T> {
    /// Create a NotNaN value.
    ///
    /// Returns Err if val is NaN
    pub fn new(val: T) -> Result<Self, FloatIsNaN> {
        match val {
            ref val if val.is_nan() => Err(FloatIsNaN),
            val => Ok(NotNaN(val)),
        }
    }

    /// Create a NotNaN value from a value that is guaranteed to not be NaN
    ///
    /// Behaviour is undefined if `val` is NaN
    pub unsafe fn unchecked_new(val: T) -> Self {
        debug_assert!(!val.is_nan());
        NotNaN(val)
    }

    /// Get the value out.
    pub fn into_inner(self) -> T {
        let NotNaN(val) = self;
        val
    }
}

impl<T: Float> AsRef<T> for NotNaN<T> {
    fn as_ref(&self) -> &T {
        let NotNaN(ref val) = *self;
        val
    }
}

impl<T: Float + PartialOrd> Ord for NotNaN<T> {
    fn cmp(&self, other: &NotNaN<T>) -> Ordering {
        match self.partial_cmp(&other) {
            Some(ord) => ord,
            None => unsafe { unreachable() },
        }
    }
}

impl<T: Float> Hash for NotNaN<T> {
    fn hash<H: Hasher>(&self, state: &mut H) {
        hash_float(self.as_ref(), state)
    }
}

impl<T: Float + fmt::Display> fmt::Display for NotNaN<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.as_ref().fmt(f)
    }
}

impl Into<f32> for NotNaN<f32> {
    fn into(self) -> f32 {
        self.into_inner()
    }
}

impl Into<f64> for NotNaN<f64> {
    fn into(self) -> f64 {
        self.into_inner()
    }
}

/// Creates a NotNaN value from a Float.
///
/// Panics if the provided value is NaN.
impl<T: Float> From<T> for NotNaN<T> {
    fn from(v: T) -> Self {
        assert!(!v.is_nan());
        NotNaN(v)
    }
}

impl<T: Float> Deref for NotNaN<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.as_ref()
    }
}

impl<T: Float + PartialEq> Eq for NotNaN<T> {}

impl<T: Float> Add for NotNaN<T> {
    type Output = Self;

    fn add(self, other: Self) -> Self {
        NotNaN(self.0 + other.0)
    }
}

/// Adds a float directly.
///
/// Panics if the provided value is NaN.
impl<T: Float> Add<T> for NotNaN<T> {
    type Output = Self;

    fn add(self, other: T) -> Self {
        assert!(!other.is_nan());
        NotNaN(self.0 + other)
    }
}

impl AddAssign for NotNaN<f64> {
    fn add_assign(&mut self, other: Self) {
        self.0 += other.0;
    }
}

impl AddAssign for NotNaN<f32> {
    fn add_assign(&mut self, other: Self) {
        self.0 += other.0;
    }
}

/// Adds a float directly.
///
/// Panics if the provided value is NaN.
impl AddAssign<f64> for NotNaN<f64> {
    fn add_assign(&mut self, other: f64) {
        assert!(!other.is_nan());
        self.0 += other;
    }
}

/// Adds a float directly.
///
/// Panics if the provided value is NaN.
impl AddAssign<f32> for NotNaN<f32> {
    fn add_assign(&mut self, other: f32) {
        assert!(!other.is_nan());
        self.0 += other;
    }
}

impl<T: Float> Sub for NotNaN<T> {
    type Output = Self;

    fn sub(self, other: Self) -> Self {
        NotNaN(self.0 - other.0)
    }
}

/// Subtracts a float directly.
///
/// Panics if the provided value is NaN.
impl<T: Float> Sub<T> for NotNaN<T> {
    type Output = Self;

    fn sub(self, other: T) -> Self {
        assert!(!other.is_nan());
        NotNaN(self.0 - other)
    }
}

impl SubAssign for NotNaN<f64> {
    fn sub_assign(&mut self, other: Self) {
        self.0 -= other.0;
    }
}

impl SubAssign for NotNaN<f32> {
    fn sub_assign(&mut self, other: Self) {
        self.0 -= other.0;
    }
}

/// Subtracts a float directly.
///
/// Panics if the provided value is NaN.
impl SubAssign<f64> for NotNaN<f64> {
    fn sub_assign(&mut self, other: f64) {
        assert!(!other.is_nan());
        self.0 -= other;
    }
}

/// Subtracts a float directly.
///
/// Panics if the provided value is NaN.
impl SubAssign<f32> for NotNaN<f32> {
    fn sub_assign(&mut self, other: f32) {
        assert!(!other.is_nan());
        self.0 -= other;
    }
}

impl<T: Float> Mul for NotNaN<T> {
    type Output = Self;

    fn mul(self, other: Self) -> Self {
        NotNaN(self.0 * other.0)
    }
}

/// Multiplies a float directly.
///
/// Panics if the provided value is NaN.
impl<T: Float> Mul<T> for NotNaN<T> {
    type Output = Self;

    fn mul(self, other: T) -> Self {
        assert!(!other.is_nan());
        NotNaN(self.0 * other)
    }
}

impl MulAssign for NotNaN<f64> {
    fn mul_assign(&mut self, other: Self) {
        self.0 *= other.0;
    }
}

impl MulAssign for NotNaN<f32> {
    fn mul_assign(&mut self, other: Self) {
        self.0 *= other.0;
    }
}

/// Multiplies a float directly.
///
/// Panics if the provided value is NaN.
impl MulAssign<f64> for NotNaN<f64> {
    fn mul_assign(&mut self, other: f64) {
        assert!(!other.is_nan());
        self.0 *= other;
    }
}

/// Multiplies a float directly.
///
/// Panics if the provided value is NaN.
impl MulAssign<f32> for NotNaN<f32> {
    fn mul_assign(&mut self, other: f32) {
        assert!(!other.is_nan());
        self.0 *= other;
    }
}

impl<T: Float> Div for NotNaN<T> {
    type Output = Self;

    fn div(self, other: Self) -> Self {
        NotNaN(self.0 / other.0)
    }
}

/// Divides a float directly.
///
/// Panics if the provided value is NaN.
impl<T: Float> Div<T> for NotNaN<T> {
    type Output = Self;

    fn div(self, other: T) -> Self {
        assert!(!other.is_nan());
        NotNaN(self.0 / other)
    }
}

impl DivAssign for NotNaN<f64> {
    fn div_assign(&mut self, other: Self) {
        self.0 /= other.0;
    }
}

impl DivAssign for NotNaN<f32> {
    fn div_assign(&mut self, other: Self) {
        self.0 /= other.0;
    }
}

/// Divides a float directly.
///
/// Panics if the provided value is NaN.
impl DivAssign<f64> for NotNaN<f64> {
    fn div_assign(&mut self, other: f64) {
        assert!(!other.is_nan());
        self.0 /= other;
    }
}

/// Divides a float directly.
///
/// Panics if the provided value is NaN.
impl DivAssign<f32> for NotNaN<f32> {
    fn div_assign(&mut self, other: f32) {
        assert!(!other.is_nan());
        self.0 /= other;
    }
}

impl<T: Float> Rem for NotNaN<T> {
    type Output = Self;

    fn rem(self, other: Self) -> Self {
        NotNaN(self.0 % other.0)
    }
}

/// Calculates `%` with a float directly.
///
/// Panics if the provided value is NaN.
impl<T: Float> Rem<T> for NotNaN<T> {
    type Output = Self;

    fn rem(self, other: T) -> Self {
        assert!(!other.is_nan());
        NotNaN(self.0 % other)
    }
}

impl RemAssign for NotNaN<f64> {
    fn rem_assign(&mut self, other: Self) {
        self.0 %= other.0;
    }
}

impl RemAssign for NotNaN<f32> {
    fn rem_assign(&mut self, other: Self) {
        self.0 %= other.0;
    }
}

/// Calculates `%=` with a float directly.
///
/// Panics if the provided value is NaN.
impl RemAssign<f64> for NotNaN<f64> {
    fn rem_assign(&mut self, other: f64) {
        assert!(!other.is_nan());
        self.0 %= other;
    }
}

/// Calculates `%=` with a float directly.
///
/// Panics if the provided value is NaN.
impl RemAssign<f32> for NotNaN<f32> {
    fn rem_assign(&mut self, other: f32) {
        assert!(!other.is_nan());
        self.0 %= other;
    }
}

impl<T: Float> Neg for NotNaN<T> {
    type Output = Self;

    fn neg(self) -> Self {
        NotNaN(-self.0)
    }
}

/// An error indicating an attempt to construct NotNaN from a NaN
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub struct FloatIsNaN;

impl Error for FloatIsNaN {
    fn description(&self) -> &str {
        return "NotNaN constructed with NaN"
    }
}

impl fmt::Display for FloatIsNaN {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        <Self as fmt::Debug>::fmt(self, f)
    }
}

impl Into<io::Error> for FloatIsNaN {
    fn into(self) -> io::Error {
        io::Error::new(io::ErrorKind::InvalidInput, self)
    }
}

#[inline]
fn hash_float<F: Float, H: Hasher>(f: &F, state: &mut H) {
    let (man, exp, sign) = f.integer_decode();
    if man == 0 {
        // Consolidate the representation of zero, whether signed or not
        // The IEEE standard considers positive and negative zero to be equal
        0
    } else {
        (man ^ ((exp as u64) << 48) ^ sign as u64)
    }.hash(state)
}

#[cfg(feature = "rustc-serialize")]
mod impl_rustc {
    extern crate rustc_serialize;
    use self::rustc_serialize::{Encodable, Encoder, Decodable, Decoder};
    use super::{OrderedFloat, NotNaN};
    use std::error::Error;
    use num_traits::Float;

    impl<T: Float + Encodable> Encodable for OrderedFloat<T> {
        fn encode<S: Encoder>(&self, s: &mut S) -> Result<(), S::Error> {
            self.0.encode(s)
        }
    }

    impl<T: Float + Decodable> Decodable for OrderedFloat<T> {
        fn decode<D: Decoder>(d: &mut D) -> Result<Self, D::Error> {
            T::decode(d).map(OrderedFloat)
        }
    }

    impl<T: Float + Encodable> Encodable for NotNaN<T> {
        fn encode<S: Encoder>(&self, s: &mut S) -> Result<(), S::Error> {
            self.0.encode(s)
        }
    }

    impl<T: Float + Decodable> Decodable for NotNaN<T> {
        fn decode<D: Decoder>(d: &mut D) -> Result<Self, D::Error> {
            T::decode(d).and_then(|v| NotNaN::new(v).map_err(|e| d.error(e.description())))
        }
    }
}

#[cfg(feature = "serde")]
mod impl_serde {
    extern crate serde;
    use self::serde::{Serialize, Serializer, Deserialize, Deserializer};
    use self::serde::de::Error;
    use super::{OrderedFloat, NotNaN};
    use num_traits::Float;

    impl<T: Float + Serialize> Serialize for OrderedFloat<T> {
        fn serialize<S: Serializer>(&self, s: &mut S) -> Result<(), S::Error> {
            self.0.serialize(s)
        }
    }

    impl<T: Float + Deserialize> Deserialize for OrderedFloat<T> {
        fn deserialize<D: Deserializer>(d: &mut D) -> Result<Self, D::Error> {
            T::deserialize(d).map(OrderedFloat)
        }
    }

    impl<T: Float + Serialize> Serialize for NotNaN<T> {
        fn serialize<S: Serializer>(&self, s: &mut S) -> Result<(), S::Error> {
            self.0.serialize(s)
        }
    }

    impl<T: Float + Deserialize> Deserialize for NotNaN<T> {
        fn deserialize<D: Deserializer>(d: &mut D) -> Result<Self, D::Error> {
            T::deserialize(d).and_then(|v| NotNaN::new(v).map_err(|_| <D::Error as Error>::invalid_value("value cannot be NaN")))
        }
    }
}
