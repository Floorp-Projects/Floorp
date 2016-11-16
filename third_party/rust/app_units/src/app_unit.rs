/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use heapsize::HeapSizeOf;
use num_traits::Zero;
use rustc_serialize::{Encodable, Encoder};
use serde::de::{Deserialize, Deserializer};
use serde::ser::{Serialize, Serializer};
use std::default::Default;
use std::fmt;
use std::i32;
use std::ops::{Add, AddAssign, Div, DivAssign, Mul, MulAssign, Neg, Rem, Sub, SubAssign};

/// The number of app units in a pixel.
pub const AU_PER_PX: i32 = 60;

#[derive(Clone, Copy, Hash, PartialEq, PartialOrd, Eq, Ord)]
pub struct Au(pub i32);

impl HeapSizeOf for Au {
    fn heap_size_of_children(&self) -> usize { 0 }
}

impl Deserialize for Au {
    fn deserialize<D: Deserializer>(deserializer: &mut D) -> Result<Au, D::Error> {
        Ok(Au(try!(i32::deserialize(deserializer))))
    }
}

impl Serialize for Au {
    fn serialize<S: Serializer>(&self, serializer: &mut S) -> Result<(), S::Error> {
        self.0.serialize(serializer)
    }
}

impl Default for Au {
    #[inline]
    fn default() -> Au {
        Au(0)
    }
}

impl Zero for Au {
    #[inline]
    fn zero() -> Au {
        Au(0)
    }

    #[inline]
    fn is_zero(&self) -> bool {
        self.0 == 0
    }
}

pub const MIN_AU: Au = Au(i32::MIN);
pub const MAX_AU: Au = Au(i32::MAX);

impl Encodable for Au {
    fn encode<S: Encoder>(&self, e: &mut S) -> Result<(), S::Error> {
        e.emit_f64(self.to_f64_px())
    }
}

impl fmt::Debug for Au {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}px", self.to_f64_px())
    }
}

impl Add for Au {
    type Output = Au;

    #[inline]
    fn add(self, other: Au) -> Au {
        Au(self.0.wrapping_add(other.0))
    }
}

impl Sub for Au {
    type Output = Au;

    #[inline]
    fn sub(self, other: Au) -> Au {
        Au(self.0.wrapping_sub(other.0))
    }

}

impl Mul<i32> for Au {
    type Output = Au;

    #[inline]
    fn mul(self, other: i32) -> Au {
        Au(self.0.wrapping_mul(other))
    }
}

impl Div<i32> for Au {
    type Output = Au;

    #[inline]
    fn div(self, other: i32) -> Au {
        Au(self.0 / other)
    }
}

impl Rem<i32> for Au {
    type Output = Au;

    #[inline]
    fn rem(self, other: i32) -> Au {
        Au(self.0 % other)
    }
}

impl Neg for Au {
    type Output = Au;

    #[inline]
    fn neg(self) -> Au {
        Au(-self.0)
    }
}

impl AddAssign for Au {
    #[inline]
    fn add_assign(&mut self, other: Au) {
        *self = *self + other;
    }
}

impl SubAssign for Au {
    #[inline]
    fn sub_assign(&mut self, other: Au) {
        *self = *self - other;
    }
}

impl MulAssign<i32> for Au {
    #[inline]
    fn mul_assign(&mut self, other: i32) {
        *self = *self * other;
    }
}

impl DivAssign<i32> for Au {
    #[inline]
    fn div_assign(&mut self, other: i32) {
        *self = *self / other;
    }
}

impl Au {
    /// FIXME(pcwalton): Workaround for lack of cross crate inlining of newtype structs!
    #[inline]
    pub fn new(value: i32) -> Au {
        Au(value)
    }

    #[inline]
    pub fn scale_by(self, factor: f32) -> Au {
        Au(((self.0 as f32) * factor) as i32)
    }

    #[inline]
    pub fn from_px(px: i32) -> Au {
        Au((px * AU_PER_PX) as i32)
    }

    /// Rounds this app unit down to the pixel towards zero and returns it.
    #[inline]
    pub fn to_px(self) -> i32 {
        self.0 / AU_PER_PX
    }

    /// Ceil this app unit to the appropriate pixel boundary and return it.
    #[inline]
    pub fn ceil_to_px(self) -> i32 {
        ((self.0 as f64) / (AU_PER_PX as f64)).ceil() as i32
    }

    #[inline]
    pub fn to_nearest_px(self) -> i32 {
        ((self.0 as f64) / (AU_PER_PX as f64)).round() as i32
    }

    #[inline]
    pub fn to_nearest_pixel(self, pixels_per_px: f32) -> f32 {
        ((self.0 as f32) / (AU_PER_PX as f32) * pixels_per_px).round() / pixels_per_px
    }

    #[inline]
    pub fn to_f32_px(self) -> f32 {
        (self.0 as f32) / (AU_PER_PX as f32)
    }

    #[inline]
    pub fn to_f64_px(self) -> f64 {
        (self.0 as f64) / (AU_PER_PX as f64)
    }

    #[inline]
    pub fn from_f32_px(px: f32) -> Au {
        Au((px * (AU_PER_PX as f32)) as i32)
    }

    #[inline]
    pub fn from_f64_px(px: f64) -> Au {
        Au((px * (AU_PER_PX as f64)) as i32)
    }
}

#[test]
fn create() {
    assert_eq!(Au::zero(), Au(0));
    assert_eq!(Au::default(), Au(0));
    assert_eq!(Au::new(7), Au(7));
}

#[test]
fn operations() {
    assert_eq!(Au(7) + Au(5), Au(12));
    assert_eq!(MAX_AU + Au(1), MIN_AU);

    assert_eq!(Au(7) - Au(5), Au(2));
    assert_eq!(MIN_AU - Au(1), MAX_AU);

    assert_eq!(Au(7) * 5, Au(35));
    assert_eq!(MAX_AU * -1, MIN_AU + Au(1));
    assert_eq!(MIN_AU * -1, MIN_AU);

    assert_eq!(Au(35) / 5, Au(7));
    assert_eq!(Au(35) % 6, Au(5));

    assert_eq!(-Au(7), Au(-7));
}

#[test]
#[should_panic]
fn overflowing_div() {
    MIN_AU / -1;
}

#[test]
#[should_panic]
fn overflowing_rem() {
    MIN_AU % -1;
}

#[test]
fn scale() {
    assert_eq!(Au(12).scale_by(1.5), Au(18));
}

#[test]
fn convert() {
    assert_eq!(Au::from_px(5), Au(300));

    assert_eq!(Au(300).to_px(), 5);
    assert_eq!(Au(330).to_px(), 5);
    assert_eq!(Au(350).to_px(), 5);
    assert_eq!(Au(360).to_px(), 6);

    assert_eq!(Au(300).ceil_to_px(), 5);
    assert_eq!(Au(310).ceil_to_px(), 6);
    assert_eq!(Au(330).ceil_to_px(), 6);
    assert_eq!(Au(350).ceil_to_px(), 6);
    assert_eq!(Au(360).ceil_to_px(), 6);

    assert_eq!(Au(300).to_nearest_px(), 5);
    assert_eq!(Au(310).to_nearest_px(), 5);
    assert_eq!(Au(330).to_nearest_px(), 6);
    assert_eq!(Au(350).to_nearest_px(), 6);
    assert_eq!(Au(360).to_nearest_px(), 6);

    assert_eq!(Au(60).to_nearest_pixel(2.), 1.);
    assert_eq!(Au(70).to_nearest_pixel(2.), 1.);
    assert_eq!(Au(80).to_nearest_pixel(2.), 1.5);
    assert_eq!(Au(90).to_nearest_pixel(2.), 1.5);
    assert_eq!(Au(100).to_nearest_pixel(2.), 1.5);
    assert_eq!(Au(110).to_nearest_pixel(2.), 2.);
    assert_eq!(Au(120).to_nearest_pixel(2.), 2.);

    assert_eq!(Au(300).to_f32_px(), 5.);
    assert_eq!(Au(312).to_f32_px(), 5.2);
    assert_eq!(Au(330).to_f32_px(), 5.5);
    assert_eq!(Au(348).to_f32_px(), 5.8);
    assert_eq!(Au(360).to_f32_px(), 6.);

    assert_eq!(Au(300).to_f64_px(), 5.);
    assert_eq!(Au(312).to_f64_px(), 5.2);
    assert_eq!(Au(330).to_f64_px(), 5.5);
    assert_eq!(Au(348).to_f64_px(), 5.8);
    assert_eq!(Au(360).to_f64_px(), 6.);

    assert_eq!(Au::from_f32_px(5.), Au(300));
    assert_eq!(Au::from_f32_px(5.2), Au(312));
    assert_eq!(Au::from_f32_px(5.5), Au(330));
    assert_eq!(Au::from_f32_px(5.8), Au(348));
    assert_eq!(Au::from_f32_px(6.), Au(360));

    assert_eq!(Au::from_f64_px(5.), Au(300));
    assert_eq!(Au::from_f64_px(5.2), Au(312));
    assert_eq!(Au::from_f64_px(5.5), Au(330));
    assert_eq!(Au::from_f64_px(5.8), Au(348));
    assert_eq!(Au::from_f64_px(6.), Au(360));
}

#[test]
fn heapsize() {
    use heapsize::HeapSizeOf;
    fn f<T: HeapSizeOf>(_: T) {}
    f(Au::new(0));
}
