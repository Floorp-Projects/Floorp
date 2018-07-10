// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::UnknownUnit;
use length::Length;
use scale::TypedScale;
use num::*;
use point::TypedPoint2D;
use vector::TypedVector2D;
use side_offsets::TypedSideOffsets2D;
use size::TypedSize2D;

use num_traits::NumCast;
#[cfg(feature = "serde")]
use serde::{Deserialize, Deserializer, Serialize, Serializer};

use core::borrow::Borrow;
use core::cmp::PartialOrd;
use core::fmt;
use core::hash::{Hash, Hasher};
use core::ops::{Add, Div, Mul, Sub};


/// A 2d Rectangle optionally tagged with a unit.
#[repr(C)]
pub struct TypedRect<T, U = UnknownUnit> {
    pub origin: TypedPoint2D<T, U>,
    pub size: TypedSize2D<T, U>,
}

/// The default rectangle type with no unit.
pub type Rect<T> = TypedRect<T, UnknownUnit>;

#[cfg(feature = "serde")]
impl<'de, T: Copy + Deserialize<'de>, U> Deserialize<'de> for TypedRect<T, U> {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let (origin, size) = try!(Deserialize::deserialize(deserializer));
        Ok(TypedRect::new(origin, size))
    }
}

#[cfg(feature = "serde")]
impl<T: Serialize, U> Serialize for TypedRect<T, U> {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        (&self.origin, &self.size).serialize(serializer)
    }
}

impl<T: Hash, U> Hash for TypedRect<T, U> {
    fn hash<H: Hasher>(&self, h: &mut H) {
        self.origin.hash(h);
        self.size.hash(h);
    }
}

impl<T: Copy, U> Copy for TypedRect<T, U> {}

impl<T: Copy, U> Clone for TypedRect<T, U> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<T: PartialEq, U> PartialEq<TypedRect<T, U>> for TypedRect<T, U> {
    fn eq(&self, other: &Self) -> bool {
        self.origin.eq(&other.origin) && self.size.eq(&other.size)
    }
}

impl<T: Eq, U> Eq for TypedRect<T, U> {}

impl<T: fmt::Debug, U> fmt::Debug for TypedRect<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "TypedRect({:?} at {:?})", self.size, self.origin)
    }
}

impl<T: fmt::Display, U> fmt::Display for TypedRect<T, U> {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "Rect({} at {})", self.size, self.origin)
    }
}

impl<T, U> TypedRect<T, U> {
    /// Constructor.
    pub fn new(origin: TypedPoint2D<T, U>, size: TypedSize2D<T, U>) -> Self {
        TypedRect {
            origin,
            size,
        }
    }
}

impl<T, U> TypedRect<T, U>
where
    T: Copy + Zero
{
    /// Creates a rect of the given size, at offset zero.
    pub fn from_size(size: TypedSize2D<T, U>) -> Self {
        TypedRect {
            origin: TypedPoint2D::zero(),
            size,
        }
    }
}

impl<T, U> TypedRect<T, U>
where
    T: Copy + Clone + Zero + PartialOrd + PartialEq + Add<T, Output = T> + Sub<T, Output = T>,
{
    #[inline]
    pub fn intersects(&self, other: &Self) -> bool {
        self.origin.x < other.origin.x + other.size.width
            && other.origin.x < self.origin.x + self.size.width
            && self.origin.y < other.origin.y + other.size.height
            && other.origin.y < self.origin.y + self.size.height
    }

    #[inline]
    pub fn max_x(&self) -> T {
        self.origin.x + self.size.width
    }

    #[inline]
    pub fn min_x(&self) -> T {
        self.origin.x
    }

    #[inline]
    pub fn max_y(&self) -> T {
        self.origin.y + self.size.height
    }

    #[inline]
    pub fn min_y(&self) -> T {
        self.origin.y
    }

    #[inline]
    pub fn max_x_typed(&self) -> Length<T, U> {
        Length::new(self.max_x())
    }

    #[inline]
    pub fn min_x_typed(&self) -> Length<T, U> {
        Length::new(self.min_x())
    }

    #[inline]
    pub fn max_y_typed(&self) -> Length<T, U> {
        Length::new(self.max_y())
    }

    #[inline]
    pub fn min_y_typed(&self) -> Length<T, U> {
        Length::new(self.min_y())
    }

    #[inline]
    pub fn intersection(&self, other: &Self) -> Option<Self> {
        if !self.intersects(other) {
            return None;
        }

        let upper_left = TypedPoint2D::new(
            max(self.min_x(), other.min_x()),
            max(self.min_y(), other.min_y()),
        );
        let lower_right_x = min(self.max_x(), other.max_x());
        let lower_right_y = min(self.max_y(), other.max_y());

        Some(TypedRect::new(
            upper_left,
            TypedSize2D::new(lower_right_x - upper_left.x, lower_right_y - upper_left.y),
        ))
    }

    /// Returns the same rectangle, translated by a vector.
    #[inline]
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn translate(&self, by: &TypedVector2D<T, U>) -> Self {
        Self::new(self.origin + *by, self.size)
    }

    /// Returns true if this rectangle contains the point. Points are considered
    /// in the rectangle if they are on the left or top edge, but outside if they
    /// are on the right or bottom edge.
    #[inline]
    pub fn contains(&self, other: &TypedPoint2D<T, U>) -> bool {
        self.origin.x <= other.x && other.x < self.origin.x + self.size.width
            && self.origin.y <= other.y && other.y < self.origin.y + self.size.height
    }

    /// Returns true if this rectangle contains the interior of rect. Always
    /// returns true if rect is empty, and always returns false if rect is
    /// nonempty but this rectangle is empty.
    #[inline]
    pub fn contains_rect(&self, rect: &Self) -> bool {
        rect.is_empty()
            || (self.min_x() <= rect.min_x() && rect.max_x() <= self.max_x()
                && self.min_y() <= rect.min_y() && rect.max_y() <= self.max_y())
    }

    #[inline]
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn inflate(&self, width: T, height: T) -> Self {
        TypedRect::new(
            TypedPoint2D::new(self.origin.x - width, self.origin.y - height),
            TypedSize2D::new(
                self.size.width + width + width,
                self.size.height + height + height,
            ),
        )
    }

    #[inline]
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn inflate_typed(&self, width: Length<T, U>, height: Length<T, U>) -> Self {
        self.inflate(width.get(), height.get())
    }

    #[inline]
    pub fn top_right(&self) -> TypedPoint2D<T, U> {
        TypedPoint2D::new(self.max_x(), self.origin.y)
    }

    #[inline]
    pub fn bottom_left(&self) -> TypedPoint2D<T, U> {
        TypedPoint2D::new(self.origin.x, self.max_y())
    }

    #[inline]
    pub fn bottom_right(&self) -> TypedPoint2D<T, U> {
        TypedPoint2D::new(self.max_x(), self.max_y())
    }

    #[inline]
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn translate_by_size(&self, size: &TypedSize2D<T, U>) -> Self {
        self.translate(&size.to_vector())
    }

    /// Calculate the size and position of an inner rectangle.
    ///
    /// Subtracts the side offsets from all sides. The horizontal and vertical
    /// offsets must not be larger than the original side length.
    pub fn inner_rect(&self, offsets: TypedSideOffsets2D<T, U>) -> Self {
        let rect = TypedRect::new(
            TypedPoint2D::new(
                self.origin.x + offsets.left,
                self.origin.y + offsets.top
            ),
            TypedSize2D::new(
                self.size.width - offsets.horizontal(),
                self.size.height - offsets.vertical()
            )
        );
        debug_assert!(rect.size.width >= Zero::zero());
        debug_assert!(rect.size.height >= Zero::zero());
        rect
    }

    /// Calculate the size and position of an outer rectangle.
    ///
    /// Add the offsets to all sides. The expanded rectangle is returned.
    pub fn outer_rect(&self, offsets: TypedSideOffsets2D<T, U>) -> Self {
        TypedRect::new(
            TypedPoint2D::new(
                self.origin.x - offsets.left,
                self.origin.y - offsets.top
            ),
            TypedSize2D::new(
                self.size.width + offsets.horizontal(),
                self.size.height + offsets.vertical()
            )
        )
    }

    /// Returns the smallest rectangle defined by the top/bottom/left/right-most
    /// points provided as parameter.
    ///
    /// Note: This function has a behavior that can be surprising because
    /// the right-most and bottom-most points are exactly on the edge
    /// of the rectangle while the `contains` function is has exclusive
    /// semantic on these edges. This means that the right-most and bottom-most
    /// points provided to `from_points` will count as not contained by the rect.
    /// This behavior may change in the future.
    pub fn from_points<I>(points: I) -> Self
    where
        I: IntoIterator,
        I::Item: Borrow<TypedPoint2D<T, U>>,
    {
        let mut points = points.into_iter();

        let (mut min_x, mut min_y) = match points.next() {
            Some(first) => (first.borrow().x, first.borrow().y),
            None => return TypedRect::zero(),
        };

        let (mut max_x, mut max_y) = (min_x, min_y);
        for point in points {
            let p = point.borrow();
            if p.x < min_x {
                min_x = p.x
            }
            if p.x > max_x {
                max_x = p.x
            }
            if p.y < min_y {
                min_y = p.y
            }
            if p.y > max_y {
                max_y = p.y
            }
        }
        TypedRect::new(
            TypedPoint2D::new(min_x, min_y),
            TypedSize2D::new(max_x - min_x, max_y - min_y),
        )
    }
}

impl<T, U> TypedRect<T, U>
where
    T: Copy + One + Add<Output = T> + Sub<Output = T> + Mul<Output = T>,
{
    /// Linearly interpolate between this rectangle and another rectangle.
    ///
    /// `t` is expected to be between zero and one.
    #[inline]
    pub fn lerp(&self, other: Self, t: T) -> Self {
        Self::new(
            self.origin.lerp(other.origin, t),
            self.size.lerp(other.size, t),
        )
    }
}

impl<T, U> TypedRect<T, U>
where
    T: Copy + Clone + PartialOrd + Add<T, Output = T> + Sub<T, Output = T> + Zero,
{
    #[inline]
    pub fn union(&self, other: &Self) -> Self {
        if self.size == Zero::zero() {
            return *other;
        }
        if other.size == Zero::zero() {
            return *self;
        }

        let upper_left = TypedPoint2D::new(
            min(self.min_x(), other.min_x()),
            min(self.min_y(), other.min_y()),
        );

        let lower_right_x = max(self.max_x(), other.max_x());
        let lower_right_y = max(self.max_y(), other.max_y());

        TypedRect::new(
            upper_left,
            TypedSize2D::new(lower_right_x - upper_left.x, lower_right_y - upper_left.y),
        )
    }
}

impl<T, U> TypedRect<T, U> {
    #[inline]
    pub fn scale<S: Copy>(&self, x: S, y: S) -> Self
    where
        T: Copy + Clone + Mul<S, Output = T>,
    {
        TypedRect::new(
            TypedPoint2D::new(self.origin.x * x, self.origin.y * y),
            TypedSize2D::new(self.size.width * x, self.size.height * y),
        )
    }
}

impl<T: Copy + PartialEq + Zero, U> TypedRect<T, U> {
    /// Constructor, setting all sides to zero.
    pub fn zero() -> Self {
        TypedRect::new(TypedPoint2D::origin(), TypedSize2D::zero())
    }

    /// Returns true if the size is zero, regardless of the origin's value.
    pub fn is_empty(&self) -> bool {
        self.size.width == Zero::zero() || self.size.height == Zero::zero()
    }
}

pub fn min<T: Clone + PartialOrd>(x: T, y: T) -> T {
    if x <= y {
        x
    } else {
        y
    }
}

pub fn max<T: Clone + PartialOrd>(x: T, y: T) -> T {
    if x >= y {
        x
    } else {
        y
    }
}

impl<T: Copy + Mul<T, Output = T>, U> Mul<T> for TypedRect<T, U> {
    type Output = Self;
    #[inline]
    fn mul(self, scale: T) -> Self {
        TypedRect::new(self.origin * scale, self.size * scale)
    }
}

impl<T: Copy + Div<T, Output = T>, U> Div<T> for TypedRect<T, U> {
    type Output = Self;
    #[inline]
    fn div(self, scale: T) -> Self {
        TypedRect::new(self.origin / scale, self.size / scale)
    }
}

impl<T: Copy + Mul<T, Output = T>, U1, U2> Mul<TypedScale<T, U1, U2>> for TypedRect<T, U1> {
    type Output = TypedRect<T, U2>;
    #[inline]
    fn mul(self, scale: TypedScale<T, U1, U2>) -> TypedRect<T, U2> {
        TypedRect::new(self.origin * scale, self.size * scale)
    }
}

impl<T: Copy + Div<T, Output = T>, U1, U2> Div<TypedScale<T, U1, U2>> for TypedRect<T, U2> {
    type Output = TypedRect<T, U1>;
    #[inline]
    fn div(self, scale: TypedScale<T, U1, U2>) -> TypedRect<T, U1> {
        TypedRect::new(self.origin / scale, self.size / scale)
    }
}

impl<T: Copy, Unit> TypedRect<T, Unit> {
    /// Drop the units, preserving only the numeric value.
    pub fn to_untyped(&self) -> Rect<T> {
        TypedRect::new(self.origin.to_untyped(), self.size.to_untyped())
    }

    /// Tag a unitless value with units.
    pub fn from_untyped(r: &Rect<T>) -> TypedRect<T, Unit> {
        TypedRect::new(
            TypedPoint2D::from_untyped(&r.origin),
            TypedSize2D::from_untyped(&r.size),
        )
    }
}

impl<T0: NumCast + Copy, Unit> TypedRect<T0, Unit> {
    /// Cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating point to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using round(), round_in or round_out() before casting.
    pub fn cast<T1: NumCast + Copy>(&self) -> TypedRect<T1, Unit> {
        TypedRect::new(
            self.origin.cast(),
            self.size.cast(),
        )
    }

    /// Fallible cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating point to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using round(), round_in or round_out() before casting.
    pub fn try_cast<T1: NumCast + Copy>(&self) -> Option<TypedRect<T1, Unit>> {
        match (self.origin.try_cast(), self.size.try_cast()) {
            (Some(origin), Some(size)) => Some(TypedRect::new(origin, size)),
            _ => None,
        }
    }
}

impl<T: Floor + Ceil + Round + Add<T, Output = T> + Sub<T, Output = T>, U> TypedRect<T, U> {
    /// Return a rectangle with edges rounded to integer coordinates, such that
    /// the returned rectangle has the same set of pixel centers as the original
    /// one.
    /// Edges at offset 0.5 round up.
    /// Suitable for most places where integral device coordinates
    /// are needed, but note that any translation should be applied first to
    /// avoid pixel rounding errors.
    /// Note that this is *not* rounding to nearest integer if the values are negative.
    /// They are always rounding as floor(n + 0.5).
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn round(&self) -> Self {
        let origin = self.origin.round();
        let size = self.origin.add_size(&self.size).round() - origin;
        TypedRect::new(origin, TypedSize2D::new(size.x, size.y))
    }

    /// Return a rectangle with edges rounded to integer coordinates, such that
    /// the original rectangle contains the resulting rectangle.
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn round_in(&self) -> Self {
        let origin = self.origin.ceil();
        let size = self.origin.add_size(&self.size).floor() - origin;
        TypedRect::new(origin, TypedSize2D::new(size.x, size.y))
    }

    /// Return a rectangle with edges rounded to integer coordinates, such that
    /// the original rectangle is contained in the resulting rectangle.
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn round_out(&self) -> Self {
        let origin = self.origin.floor();
        let size = self.origin.add_size(&self.size).ceil() - origin;
        TypedRect::new(origin, TypedSize2D::new(size.x, size.y))
    }
}

// Convenience functions for common casts
impl<T: NumCast + Copy, Unit> TypedRect<T, Unit> {
    /// Cast into an `f32` rectangle.
    pub fn to_f32(&self) -> TypedRect<f32, Unit> {
        self.cast()
    }

    /// Cast into an `f64` rectangle.
    pub fn to_f64(&self) -> TypedRect<f64, Unit> {
        self.cast()
    }

    /// Cast into an `usize` rectangle, truncating decimals if any.
    ///
    /// When casting from floating point rectangles, it is worth considering whether
    /// to `round()`, `round_in()` or `round_out()` before the cast in order to
    /// obtain the desired conversion behavior.
    pub fn to_usize(&self) -> TypedRect<usize, Unit> {
        self.cast()
    }

    /// Cast into an `u32` rectangle, truncating decimals if any.
    ///
    /// When casting from floating point rectangles, it is worth considering whether
    /// to `round()`, `round_in()` or `round_out()` before the cast in order to
    /// obtain the desired conversion behavior.
    pub fn to_u32(&self) -> TypedRect<u32, Unit> {
        self.cast()
    }

    /// Cast into an `i32` rectangle, truncating decimals if any.
    ///
    /// When casting from floating point rectangles, it is worth considering whether
    /// to `round()`, `round_in()` or `round_out()` before the cast in order to
    /// obtain the desired conversion behavior.
    pub fn to_i32(&self) -> TypedRect<i32, Unit> {
        self.cast()
    }

    /// Cast into an `i64` rectangle, truncating decimals if any.
    ///
    /// When casting from floating point rectangles, it is worth considering whether
    /// to `round()`, `round_in()` or `round_out()` before the cast in order to
    /// obtain the desired conversion behavior.
    pub fn to_i64(&self) -> TypedRect<i64, Unit> {
        self.cast()
    }
}

impl<T, U> From<TypedSize2D<T, U>> for TypedRect<T, U>
where T: Copy + Zero
{
    fn from(size: TypedSize2D<T, U>) -> Self {
        Self::from_size(size)
    }
}

/// Shorthand for `TypedRect::new(TypedPoint2D::new(x, y), TypedSize2D::new(w, h))`.
pub fn rect<T: Copy, U>(x: T, y: T, w: T, h: T) -> TypedRect<T, U> {
    TypedRect::new(TypedPoint2D::new(x, y), TypedSize2D::new(w, h))
}

#[cfg(test)]
mod tests {
    use point::Point2D;
    use vector::vec2;
    use side_offsets::SideOffsets2D;
    use size::Size2D;
    use super::*;

    #[test]
    fn test_min_max() {
        assert!(min(0u32, 1u32) == 0u32);
        assert!(min(-1.0f32, 0.0f32) == -1.0f32);

        assert!(max(0u32, 1u32) == 1u32);
        assert!(max(-1.0f32, 0.0f32) == 0.0f32);
    }

    #[test]
    fn test_translate() {
        let p = Rect::new(Point2D::new(0u32, 0u32), Size2D::new(50u32, 40u32));
        let pp = p.translate(&vec2(10, 15));

        assert!(pp.size.width == 50);
        assert!(pp.size.height == 40);
        assert!(pp.origin.x == 10);
        assert!(pp.origin.y == 15);

        let r = Rect::new(Point2D::new(-10, -5), Size2D::new(50, 40));
        let rr = r.translate(&vec2(0, -10));

        assert!(rr.size.width == 50);
        assert!(rr.size.height == 40);
        assert!(rr.origin.x == -10);
        assert!(rr.origin.y == -15);
    }

    #[test]
    fn test_translate_by_size() {
        let p = Rect::new(Point2D::new(0u32, 0u32), Size2D::new(50u32, 40u32));
        let pp = p.translate_by_size(&Size2D::new(10, 15));

        assert!(pp.size.width == 50);
        assert!(pp.size.height == 40);
        assert!(pp.origin.x == 10);
        assert!(pp.origin.y == 15);

        let r = Rect::new(Point2D::new(-10, -5), Size2D::new(50, 40));
        let rr = r.translate_by_size(&Size2D::new(0, -10));

        assert!(rr.size.width == 50);
        assert!(rr.size.height == 40);
        assert!(rr.origin.x == -10);
        assert!(rr.origin.y == -15);
    }

    #[test]
    fn test_union() {
        let p = Rect::new(Point2D::new(0, 0), Size2D::new(50, 40));
        let q = Rect::new(Point2D::new(20, 20), Size2D::new(5, 5));
        let r = Rect::new(Point2D::new(-15, -30), Size2D::new(200, 15));
        let s = Rect::new(Point2D::new(20, -15), Size2D::new(250, 200));

        let pq = p.union(&q);
        assert!(pq.origin == Point2D::new(0, 0));
        assert!(pq.size == Size2D::new(50, 40));

        let pr = p.union(&r);
        assert!(pr.origin == Point2D::new(-15, -30));
        assert!(pr.size == Size2D::new(200, 70));

        let ps = p.union(&s);
        assert!(ps.origin == Point2D::new(0, -15));
        assert!(ps.size == Size2D::new(270, 200));
    }

    #[test]
    fn test_intersection() {
        let p = Rect::new(Point2D::new(0, 0), Size2D::new(10, 20));
        let q = Rect::new(Point2D::new(5, 15), Size2D::new(10, 10));
        let r = Rect::new(Point2D::new(-5, -5), Size2D::new(8, 8));

        let pq = p.intersection(&q);
        assert!(pq.is_some());
        let pq = pq.unwrap();
        assert!(pq.origin == Point2D::new(5, 15));
        assert!(pq.size == Size2D::new(5, 5));

        let pr = p.intersection(&r);
        assert!(pr.is_some());
        let pr = pr.unwrap();
        assert!(pr.origin == Point2D::new(0, 0));
        assert!(pr.size == Size2D::new(3, 3));

        let qr = q.intersection(&r);
        assert!(qr.is_none());
    }

    #[test]
    fn test_contains() {
        let r = Rect::new(Point2D::new(-20, 15), Size2D::new(100, 200));

        assert!(r.contains(&Point2D::new(0, 50)));
        assert!(r.contains(&Point2D::new(-10, 200)));

        // The `contains` method is inclusive of the top/left edges, but not the
        // bottom/right edges.
        assert!(r.contains(&Point2D::new(-20, 15)));
        assert!(!r.contains(&Point2D::new(80, 15)));
        assert!(!r.contains(&Point2D::new(80, 215)));
        assert!(!r.contains(&Point2D::new(-20, 215)));

        // Points beyond the top-left corner.
        assert!(!r.contains(&Point2D::new(-25, 15)));
        assert!(!r.contains(&Point2D::new(-15, 10)));

        // Points beyond the top-right corner.
        assert!(!r.contains(&Point2D::new(85, 20)));
        assert!(!r.contains(&Point2D::new(75, 10)));

        // Points beyond the bottom-right corner.
        assert!(!r.contains(&Point2D::new(85, 210)));
        assert!(!r.contains(&Point2D::new(75, 220)));

        // Points beyond the bottom-left corner.
        assert!(!r.contains(&Point2D::new(-25, 210)));
        assert!(!r.contains(&Point2D::new(-15, 220)));

        let r = Rect::new(Point2D::new(-20.0, 15.0), Size2D::new(100.0, 200.0));
        assert!(r.contains_rect(&r));
        assert!(!r.contains_rect(&r.translate(&vec2(0.1, 0.0))));
        assert!(!r.contains_rect(&r.translate(&vec2(-0.1, 0.0))));
        assert!(!r.contains_rect(&r.translate(&vec2(0.0, 0.1))));
        assert!(!r.contains_rect(&r.translate(&vec2(0.0, -0.1))));
        // Empty rectangles are always considered as contained in other rectangles,
        // even if their origin is not.
        let p = Point2D::new(1.0, 1.0);
        assert!(!r.contains(&p));
        assert!(r.contains_rect(&Rect::new(p, Size2D::zero())));
    }

    #[test]
    fn test_scale() {
        let p = Rect::new(Point2D::new(0u32, 0u32), Size2D::new(50u32, 40u32));
        let pp = p.scale(10, 15);

        assert!(pp.size.width == 500);
        assert!(pp.size.height == 600);
        assert!(pp.origin.x == 0);
        assert!(pp.origin.y == 0);

        let r = Rect::new(Point2D::new(-10, -5), Size2D::new(50, 40));
        let rr = r.scale(1, 20);

        assert!(rr.size.width == 50);
        assert!(rr.size.height == 800);
        assert!(rr.origin.x == -10);
        assert!(rr.origin.y == -100);
    }

    #[test]
    fn test_inflate() {
        let p = Rect::new(Point2D::new(0, 0), Size2D::new(10, 10));
        let pp = p.inflate(10, 20);

        assert!(pp.size.width == 30);
        assert!(pp.size.height == 50);
        assert!(pp.origin.x == -10);
        assert!(pp.origin.y == -20);

        let r = Rect::new(Point2D::new(0, 0), Size2D::new(10, 20));
        let rr = r.inflate(-2, -5);

        assert!(rr.size.width == 6);
        assert!(rr.size.height == 10);
        assert!(rr.origin.x == 2);
        assert!(rr.origin.y == 5);
    }

    #[test]
    fn test_inner_outer_rect() {
        let inner_rect: Rect<i32> = Rect::new(Point2D::new(20, 40), Size2D::new(80, 100));
        let offsets = SideOffsets2D::new(20, 10, 10, 10);
        let outer_rect = inner_rect.outer_rect(offsets);
        assert_eq!(outer_rect.origin.x, 10);
        assert_eq!(outer_rect.origin.y, 20);
        assert_eq!(outer_rect.size.width, 100);
        assert_eq!(outer_rect.size.height, 130);
        assert_eq!(outer_rect.inner_rect(offsets), inner_rect);
    }

    #[test]
    fn test_min_max_x_y() {
        let p = Rect::new(Point2D::new(0u32, 0u32), Size2D::new(50u32, 40u32));
        assert!(p.max_y() == 40);
        assert!(p.min_y() == 0);
        assert!(p.max_x() == 50);
        assert!(p.min_x() == 0);

        let r = Rect::new(Point2D::new(-10, -5), Size2D::new(50, 40));
        assert!(r.max_y() == 35);
        assert!(r.min_y() == -5);
        assert!(r.max_x() == 40);
        assert!(r.min_x() == -10);
    }

    #[test]
    fn test_is_empty() {
        assert!(Rect::new(Point2D::new(0u32, 0u32), Size2D::new(0u32, 0u32)).is_empty());
        assert!(Rect::new(Point2D::new(0u32, 0u32), Size2D::new(10u32, 0u32)).is_empty());
        assert!(Rect::new(Point2D::new(0u32, 0u32), Size2D::new(0u32, 10u32)).is_empty());
        assert!(!Rect::new(Point2D::new(0u32, 0u32), Size2D::new(1u32, 1u32)).is_empty());
        assert!(Rect::new(Point2D::new(10u32, 10u32), Size2D::new(0u32, 0u32)).is_empty());
        assert!(Rect::new(Point2D::new(10u32, 10u32), Size2D::new(10u32, 0u32)).is_empty());
        assert!(Rect::new(Point2D::new(10u32, 10u32), Size2D::new(0u32, 10u32)).is_empty());
        assert!(!Rect::new(Point2D::new(10u32, 10u32), Size2D::new(1u32, 1u32)).is_empty());
    }

    #[test]
    fn test_round() {
        let mut x = -2.0;
        let mut y = -2.0;
        let mut w = -2.0;
        let mut h = -2.0;
        while x < 2.0 {
            while y < 2.0 {
                while w < 2.0 {
                    while h < 2.0 {
                        let rect = Rect::new(Point2D::new(x, y), Size2D::new(w, h));

                        assert!(rect.contains_rect(&rect.round_in()));
                        assert!(rect.round_in().inflate(1.0, 1.0).contains_rect(&rect));

                        assert!(rect.round_out().contains_rect(&rect));
                        assert!(rect.inflate(1.0, 1.0).contains_rect(&rect.round_out()));

                        assert!(rect.inflate(1.0, 1.0).contains_rect(&rect.round()));
                        assert!(rect.round().inflate(1.0, 1.0).contains_rect(&rect));

                        h += 0.1;
                    }
                    w += 0.1;
                }
                y += 0.1;
            }
            x += 0.1
        }
    }
}
