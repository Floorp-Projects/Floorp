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
/// An App Unit, the fundamental unit of length in Servo. Usually
/// 1/60th of a pixel (see AU_PER_PX)
///
/// Please ensure that the values are between MIN_AU and MAX_AU.
/// It is safe to construct invalid Au values, but it may lead to
/// panics and overflows.
pub struct Au(pub i32);

impl HeapSizeOf for Au {
    fn heap_size_of_children(&self) -> usize { 0 }
}

impl Deserialize for Au {
    fn deserialize<D: Deserializer>(deserializer: D) -> Result<Au, D::Error> {
        Ok(Au(try!(i32::deserialize(deserializer))).clamp())
    }
}

impl Serialize for Au {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
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

// 1 << 30 lets us add/subtract two Au and check for overflow
// after the operation. Gecko uses the same min/max values
pub const MAX_AU: Au = Au(1 << 30);
pub const MIN_AU: Au = Au(- (1 << 30));

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
        Au(self.0 + other.0).clamp()
    }
}

impl Sub for Au {
    type Output = Au;

    #[inline]
    fn sub(self, other: Au) -> Au {
        Au(self.0 - other.0).clamp()
    }

}

impl Mul<i32> for Au {
    type Output = Au;

    #[inline]
    fn mul(self, other: i32) -> Au {
        if let Some(new) = self.0.checked_mul(other) {
            Au(new).clamp()
        } else if (self.0 > 0) ^ (other > 0) {
            MIN_AU
        } else {
            MAX_AU
        }
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
        self.clamp_self();
    }
}

impl SubAssign for Au {
    #[inline]
    fn sub_assign(&mut self, other: Au) {
        *self = *self - other;
        self.clamp_self();
    }
}

impl MulAssign<i32> for Au {
    #[inline]
    fn mul_assign(&mut self, other: i32) {
        *self = *self * other;
        self.clamp_self();
    }
}

impl DivAssign<i32> for Au {
    #[inline]
    fn div_assign(&mut self, other: i32) {
        *self = *self / other;
        self.clamp_self();
    }
}

impl Au {
    /// FIXME(pcwalton): Workaround for lack of cross crate inlining of newtype structs!
    #[inline]
    pub fn new(value: i32) -> Au {
        Au(value).clamp()
    }

    #[inline]
    fn clamp(self) -> Self {
        if self.0 > MAX_AU.0 {
            MAX_AU
        } else if self.0 < MIN_AU.0 {
            MIN_AU
        } else {
            self
        }
    }

    #[inline]
    fn clamp_self(&mut self) {
        *self = self.clamp()
    }

    #[inline]
    pub fn scale_by(self, factor: f32) -> Au {
        let new_float = ((self.0 as f32) * factor).round();
        if new_float > MAX_AU.0 as f32 {
            MAX_AU
        } else if new_float < MIN_AU.0 as f32 {
            MIN_AU
        } else {
            Au(new_float as i32)
        }
    }

    #[inline]
    pub fn from_px(px: i32) -> Au {
        Au(px) * AU_PER_PX
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
        let float = (px * AU_PER_PX as f32).round();
        if float > MAX_AU.0 as f32 {
            MAX_AU
        } else if float < MIN_AU.0 as f32 {
            MIN_AU
        } else {
            Au(float as i32)
        }
    }

    #[inline]
    pub fn from_f64_px(px: f64) -> Au {
        let float = (px * AU_PER_PX as f64).round();
        if float > MAX_AU.0 as f64 {
            MAX_AU
        } else if float < MIN_AU.0 as f64 {
            MIN_AU
        } else {
            Au(float as i32)
        }
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
    assert_eq!(MAX_AU + Au(1), MAX_AU);

    assert_eq!(Au(7) - Au(5), Au(2));
    assert_eq!(MIN_AU - Au(1), MIN_AU);

    assert_eq!(Au(7) * 5, Au(35));
    assert_eq!(MAX_AU * -1, MIN_AU);
    assert_eq!(MIN_AU * -1, MAX_AU);

    assert_eq!(Au(35) / 5, Au(7));
    assert_eq!(Au(35) % 6, Au(5));

    assert_eq!(-Au(7), Au(-7));
}

#[test]
fn saturate() {
    let half = MAX_AU / 2;
    assert_eq!(half + half + half + half + half, MAX_AU);
    assert_eq!(-half - half - half - half - half, MIN_AU);
    assert_eq!(half * -10, MIN_AU);
    assert_eq!(-half * 10, MIN_AU);
    assert_eq!(half * 10, MAX_AU);
    assert_eq!(-half * -10, MAX_AU);
}

#[test]
fn scale() {
    assert_eq!(Au(12).scale_by(1.5), Au(18));
    assert_eq!(Au(12).scale_by(1.7), Au(20));
    assert_eq!(Au(12).scale_by(1.8), Au(22));
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
    assert_eq!((Au(367).to_f32_px() * 1000.).round(), 6_117.);
    assert_eq!((Au(368).to_f32_px() * 1000.).round(), 6_133.);

    assert_eq!(Au(300).to_f64_px(), 5.);
    assert_eq!(Au(312).to_f64_px(), 5.2);
    assert_eq!(Au(330).to_f64_px(), 5.5);
    assert_eq!(Au(348).to_f64_px(), 5.8);
    assert_eq!(Au(360).to_f64_px(), 6.);
    assert_eq!((Au(367).to_f64_px() * 1000.).round(), 6_117.);
    assert_eq!((Au(368).to_f64_px() * 1000.).round(), 6_133.);

    assert_eq!(Au::from_f32_px(5.), Au(300));
    assert_eq!(Au::from_f32_px(5.2), Au(312));
    assert_eq!(Au::from_f32_px(5.5), Au(330));
    assert_eq!(Au::from_f32_px(5.8), Au(348));
    assert_eq!(Au::from_f32_px(6.), Au(360));
    assert_eq!(Au::from_f32_px(6.12), Au(367));
    assert_eq!(Au::from_f32_px(6.13), Au(368));

    assert_eq!(Au::from_f64_px(5.), Au(300));
    assert_eq!(Au::from_f64_px(5.2), Au(312));
    assert_eq!(Au::from_f64_px(5.5), Au(330));
    assert_eq!(Au::from_f64_px(5.8), Au(348));
    assert_eq!(Au::from_f64_px(6.), Au(360));
    assert_eq!(Au::from_f64_px(6.12), Au(367));
    assert_eq!(Au::from_f64_px(6.13), Au(368));
}

#[test]
fn heapsize() {
    use heapsize::HeapSizeOf;
    fn f<T: HeapSizeOf>(_: T) {}
    f(Au::new(0));
}
