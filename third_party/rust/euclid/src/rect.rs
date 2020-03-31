// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::UnknownUnit;
use scale::Scale;
use num::*;
use box2d::Box2D;
use point::Point2D;
use vector::Vector2D;
use side_offsets::SideOffsets2D;
use size::Size2D;
use approxord::{min, max};
use nonempty::NonEmpty;

use num_traits::NumCast;
#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};

use core::borrow::Borrow;
use core::cmp::PartialOrd;
use core::fmt;
use core::hash::{Hash, Hasher};
use core::ops::{Add, Div, Mul, Sub, Range};


/// A 2d Rectangle optionally tagged with a unit.
#[repr(C)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(bound(serialize = "T: Serialize", deserialize = "T: Deserialize<'de>")))]
pub struct Rect<T, U> {
    pub origin: Point2D<T, U>,
    pub size: Size2D<T, U>,
}

impl<T: Hash, U> Hash for Rect<T, U> {
    fn hash<H: Hasher>(&self, h: &mut H) {
        self.origin.hash(h);
        self.size.hash(h);
    }
}

impl<T: Copy, U> Copy for Rect<T, U> {}

impl<T: Copy, U> Clone for Rect<T, U> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<T: PartialEq, U> PartialEq<Rect<T, U>> for Rect<T, U> {
    fn eq(&self, other: &Self) -> bool {
        self.origin.eq(&other.origin) && self.size.eq(&other.size)
    }
}

impl<T: Eq, U> Eq for Rect<T, U> {}

impl<T: fmt::Debug, U> fmt::Debug for Rect<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Rect({:?} at {:?})", self.size, self.origin)
    }
}

impl<T: fmt::Display, U> fmt::Display for Rect<T, U> {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "Rect({} at {})", self.size, self.origin)
    }
}

impl<T: Default, U> Default for Rect<T, U> {
    fn default() -> Self {
        Rect::new(Default::default(), Default::default())
    }
}

impl<T, U> Rect<T, U> {
    /// Constructor.
    pub fn new(origin: Point2D<T, U>, size: Size2D<T, U>) -> Self {
        Rect {
            origin,
            size,
        }
    }
}

impl<T, U> Rect<T, U>
where
    T: Copy + Zero
{
    /// Creates a rect of the given size, at offset zero.
    pub fn from_size(size: Size2D<T, U>) -> Self {
        Rect {
            origin: Point2D::zero(),
            size,
        }
    }
}

impl<T, U> Rect<T, U>
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
    pub fn min(&self) -> Point2D<T, U> {
        self.origin
    }

    #[inline]
    pub fn max(&self) -> Point2D<T, U> {
        self.origin + self.size
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
    pub fn x_range(&self) -> Range<T> {
        self.min_x()..self.max_x()
    }

    #[inline]
    pub fn y_range(&self) -> Range<T> {
        self.min_y()..self.max_y()
    }

    #[inline]
    pub fn intersection(&self, other: &Self) -> Option<Self> {
        if !self.intersects(other) {
            return None;
        }

        let upper_left = Point2D::new(
            max(self.min_x(), other.min_x()),
            max(self.min_y(), other.min_y()),
        );
        let lower_right_x = min(self.max_x(), other.max_x());
        let lower_right_y = min(self.max_y(), other.max_y());

        Some(Rect::new(
            upper_left,
            Size2D::new(lower_right_x - upper_left.x, lower_right_y - upper_left.y),
        ))
    }

    /// Returns the same rectangle, translated by a vector.
    #[inline]
    #[must_use]
    pub fn translate(&self, by: Vector2D<T, U>) -> Self {
        Self::new(self.origin + by, self.size)
    }

    /// Returns true if this rectangle contains the point. Points are considered
    /// in the rectangle if they are on the left or top edge, but outside if they
    /// are on the right or bottom edge.
    #[inline]
    pub fn contains(&self, other: Point2D<T, U>) -> bool {
        self.origin.x <= other.x && other.x < self.origin.x + self.size.width
            && self.origin.y <= other.y && other.y < self.origin.y + self.size.height
    }

    /// Returns true if this rectangle contains the interior of rect. Always
    /// returns true if rect is empty, and always returns false if rect is
    /// nonempty but this rectangle is empty.
    #[inline]
    pub fn contains_rect(&self, rect: &Self) -> bool {
        rect.is_empty_or_negative()
            || (self.min_x() <= rect.min_x() && rect.max_x() <= self.max_x()
                && self.min_y() <= rect.min_y() && rect.max_y() <= self.max_y())
    }

    #[inline]
    #[must_use]
    pub fn inflate(&self, width: T, height: T) -> Self {
        Rect::new(
            Point2D::new(self.origin.x - width, self.origin.y - height),
            Size2D::new(
                self.size.width + width + width,
                self.size.height + height + height,
            ),
        )
    }

    #[inline]
    pub fn to_box2d(&self) -> Box2D<T, U> {
        Box2D {
            min: self.min(),
            max: self.max(),
        }
    }

    /// Calculate the size and position of an inner rectangle.
    ///
    /// Subtracts the side offsets from all sides. The horizontal and vertical
    /// offsets must not be larger than the original side length.
    /// This method assumes y oriented downward.
    pub fn inner_rect(&self, offsets: SideOffsets2D<T, U>) -> Self {
        let rect = Rect::new(
            Point2D::new(
                self.origin.x + offsets.left,
                self.origin.y + offsets.top
            ),
            Size2D::new(
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
    /// This method assumes y oriented downward.
    pub fn outer_rect(&self, offsets: SideOffsets2D<T, U>) -> Self {
        Rect::new(
            Point2D::new(
                self.origin.x - offsets.left,
                self.origin.y - offsets.top
            ),
            Size2D::new(
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
        I::Item: Borrow<Point2D<T, U>>,
    {
        let mut points = points.into_iter();

        let (mut min_x, mut min_y) = match points.next() {
            Some(first) => (first.borrow().x, first.borrow().y),
            None => return Rect::zero(),
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
        Rect::new(
            Point2D::new(min_x, min_y),
            Size2D::new(max_x - min_x, max_y - min_y),
        )
    }
}

impl<T, U> Rect<T, U>
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

impl<T, U> Rect<T, U>
where
    T: Copy + One + Add<Output = T> + Div<Output = T>,
{
    pub fn center(&self) -> Point2D<T, U> {
        let two = T::one() + T::one();
        self.origin + self.size.to_vector() / two
    }
}

impl<T, U> Rect<T, U>
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

        let upper_left = Point2D::new(
            min(self.min_x(), other.min_x()),
            min(self.min_y(), other.min_y()),
        );

        let lower_right_x = max(self.max_x(), other.max_x());
        let lower_right_y = max(self.max_y(), other.max_y());

        Rect::new(
            upper_left,
            Size2D::new(lower_right_x - upper_left.x, lower_right_y - upper_left.y),
        )
    }
}

impl<T, U> Rect<T, U> {
    #[inline]
    pub fn scale<S: Copy>(&self, x: S, y: S) -> Self
    where
        T: Copy + Clone + Mul<S, Output = T>,
    {
        Rect::new(
            Point2D::new(self.origin.x * x, self.origin.y * y),
            Size2D::new(self.size.width * x, self.size.height * y),
        )
    }
}

impl<T: Copy + Clone + Mul<T, Output = T>, U> Rect<T, U> {
    #[inline]
    pub fn area(&self) -> T {
        self.size.area()
    }
}

impl<T: Copy + PartialEq + Zero, U> Rect<T, U> {
    /// Constructor, setting all sides to zero.
    pub fn zero() -> Self {
        Rect::new(Point2D::origin(), Size2D::zero())
    }

    /// Returns true if the size is zero, regardless of the origin's value.
    pub fn is_empty(&self) -> bool {
        self.size.width == Zero::zero() || self.size.height == Zero::zero()
    }
}

impl<T: Copy + Zero + PartialOrd, U> Rect<T, U> {

    #[inline]
    pub fn is_empty_or_negative(&self) -> bool {
        self.size.is_empty_or_negative()
    }

    #[inline]
    pub fn to_non_empty(&self) -> Option<NonEmpty<Self>> {
        if self.is_empty_or_negative() {
            return None;
        }

        Some(NonEmpty(*self))
    }
}

impl<T: Copy + Mul<T, Output = T>, U> Mul<T> for Rect<T, U> {
    type Output = Self;
    #[inline]
    fn mul(self, scale: T) -> Self {
        Rect::new(self.origin * scale, self.size * scale)
    }
}

impl<T: Copy + Div<T, Output = T>, U> Div<T> for Rect<T, U> {
    type Output = Self;
    #[inline]
    fn div(self, scale: T) -> Self {
        Rect::new(self.origin / scale, self.size / scale)
    }
}

impl<T: Copy + Mul<T, Output = T>, U1, U2> Mul<Scale<T, U1, U2>> for Rect<T, U1> {
    type Output = Rect<T, U2>;
    #[inline]
    fn mul(self, scale: Scale<T, U1, U2>) -> Rect<T, U2> {
        Rect::new(self.origin * scale, self.size * scale)
    }
}

impl<T: Copy + Div<T, Output = T>, U1, U2> Div<Scale<T, U1, U2>> for Rect<T, U2> {
    type Output = Rect<T, U1>;
    #[inline]
    fn div(self, scale: Scale<T, U1, U2>) -> Rect<T, U1> {
        Rect::new(self.origin / scale, self.size / scale)
    }
}

impl<T: Copy, Unit> Rect<T, Unit> {
    /// Drop the units, preserving only the numeric value.
    #[inline]
    pub fn to_untyped(&self) -> Rect<T, UnknownUnit> {
        Rect::new(self.origin.to_untyped(), self.size.to_untyped())
    }

    /// Tag a unitless value with units.
    #[inline]
    pub fn from_untyped(r: &Rect<T, UnknownUnit>) -> Rect<T, Unit> {
        Rect::new(
            Point2D::from_untyped(r.origin),
            Size2D::from_untyped(r.size),
        )
    }
}

impl<T0: NumCast + Copy, Unit> Rect<T0, Unit> {
    /// Cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating point to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using round(), round_in or round_out() before casting.
    pub fn cast<T1: NumCast + Copy>(&self) -> Rect<T1, Unit> {
        Rect::new(
            self.origin.cast(),
            self.size.cast(),
        )
    }

    /// Fallible cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating point to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using round(), round_in or round_out() before casting.
    pub fn try_cast<T1: NumCast + Copy>(&self) -> Option<Rect<T1, Unit>> {
        match (self.origin.try_cast(), self.size.try_cast()) {
            (Some(origin), Some(size)) => Some(Rect::new(origin, size)),
            _ => None,
        }
    }
}

impl<T: Floor + Ceil + Round + Add<T, Output = T> + Sub<T, Output = T>, U> Rect<T, U> {
    /// Return a rectangle with edges rounded to integer coordinates, such that
    /// the returned rectangle has the same set of pixel centers as the original
    /// one.
    /// Edges at offset 0.5 round up.
    /// Suitable for most places where integral device coordinates
    /// are needed, but note that any translation should be applied first to
    /// avoid pixel rounding errors.
    /// Note that this is *not* rounding to nearest integer if the values are negative.
    /// They are always rounding as floor(n + 0.5).
    #[must_use]
    pub fn round(&self) -> Self {
        let origin = self.origin.round();
        let size = self.origin.add_size(&self.size).round() - origin;
        Rect::new(origin, Size2D::new(size.x, size.y))
    }

    /// Return a rectangle with edges rounded to integer coordinates, such that
    /// the original rectangle contains the resulting rectangle.
    #[must_use]
    pub fn round_in(&self) -> Self {
        let origin = self.origin.ceil();
        let size = self.origin.add_size(&self.size).floor() - origin;
        Rect::new(origin, Size2D::new(size.x, size.y))
    }

    /// Return a rectangle with edges rounded to integer coordinates, such that
    /// the original rectangle is contained in the resulting rectangle.
    #[must_use]
    pub fn round_out(&self) -> Self {
        let origin = self.origin.floor();
        let size = self.origin.add_size(&self.size).ceil() - origin;
        Rect::new(origin, Size2D::new(size.x, size.y))
    }
}

// Convenience functions for common casts
impl<T: NumCast + Copy, Unit> Rect<T, Unit> {
    /// Cast into an `f32` rectangle.
    pub fn to_f32(&self) -> Rect<f32, Unit> {
        self.cast()
    }

    /// Cast into an `f64` rectangle.
    pub fn to_f64(&self) -> Rect<f64, Unit> {
        self.cast()
    }

    /// Cast into an `usize` rectangle, truncating decimals if any.
    ///
    /// When casting from floating point rectangles, it is worth considering whether
    /// to `round()`, `round_in()` or `round_out()` before the cast in order to
    /// obtain the desired conversion behavior.
    pub fn to_usize(&self) -> Rect<usize, Unit> {
        self.cast()
    }

    /// Cast into an `u32` rectangle, truncating decimals if any.
    ///
    /// When casting from floating point rectangles, it is worth considering whether
    /// to `round()`, `round_in()` or `round_out()` before the cast in order to
    /// obtain the desired conversion behavior.
    pub fn to_u32(&self) -> Rect<u32, Unit> {
        self.cast()
    }

    /// Cast into an `i32` rectangle, truncating decimals if any.
    ///
    /// When casting from floating point rectangles, it is worth considering whether
    /// to `round()`, `round_in()` or `round_out()` before the cast in order to
    /// obtain the desired conversion behavior.
    pub fn to_i32(&self) -> Rect<i32, Unit> {
        self.cast()
    }

    /// Cast into an `i64` rectangle, truncating decimals if any.
    ///
    /// When casting from floating point rectangles, it is worth considering whether
    /// to `round()`, `round_in()` or `round_out()` before the cast in order to
    /// obtain the desired conversion behavior.
    pub fn to_i64(&self) -> Rect<i64, Unit> {
        self.cast()
    }
}

impl<T, U> From<Size2D<T, U>> for Rect<T, U>
where T: Copy + Zero
{
    fn from(size: Size2D<T, U>) -> Self {
        Self::from_size(size)
    }
}

/// Shorthand for `Rect::new(Point2D::new(x, y), Size2D::new(w, h))`.
pub fn rect<T: Copy, U>(x: T, y: T, w: T, h: T) -> Rect<T, U> {
    Rect::new(Point2D::new(x, y), Size2D::new(w, h))
}

#[cfg(test)]
mod tests {
    use default::{Point2D, Rect, Size2D};
    use {point2, vec2, rect, size2};
    use side_offsets::SideOffsets2D;

    #[test]
    fn test_translate() {
        let p = Rect::new(Point2D::new(0u32, 0u32), Size2D::new(50u32, 40u32));
        let pp = p.translate(vec2(10, 15));

        assert!(pp.size.width == 50);
        assert!(pp.size.height == 40);
        assert!(pp.origin.x == 10);
        assert!(pp.origin.y == 15);

        let r = Rect::new(Point2D::new(-10, -5), Size2D::new(50, 40));
        let rr = r.translate(vec2(0, -10));

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

        assert!(r.contains(Point2D::new(0, 50)));
        assert!(r.contains(Point2D::new(-10, 200)));

        // The `contains` method is inclusive of the top/left edges, but not the
        // bottom/right edges.
        assert!(r.contains(Point2D::new(-20, 15)));
        assert!(!r.contains(Point2D::new(80, 15)));
        assert!(!r.contains(Point2D::new(80, 215)));
        assert!(!r.contains(Point2D::new(-20, 215)));

        // Points beyond the top-left corner.
        assert!(!r.contains(Point2D::new(-25, 15)));
        assert!(!r.contains(Point2D::new(-15, 10)));

        // Points beyond the top-right corner.
        assert!(!r.contains(Point2D::new(85, 20)));
        assert!(!r.contains(Point2D::new(75, 10)));

        // Points beyond the bottom-right corner.
        assert!(!r.contains(Point2D::new(85, 210)));
        assert!(!r.contains(Point2D::new(75, 220)));

        // Points beyond the bottom-left corner.
        assert!(!r.contains(Point2D::new(-25, 210)));
        assert!(!r.contains(Point2D::new(-15, 220)));

        let r = Rect::new(Point2D::new(-20.0, 15.0), Size2D::new(100.0, 200.0));
        assert!(r.contains_rect(&r));
        assert!(!r.contains_rect(&r.translate(vec2(0.1, 0.0))));
        assert!(!r.contains_rect(&r.translate(vec2(-0.1, 0.0))));
        assert!(!r.contains_rect(&r.translate(vec2(0.0, 0.1))));
        assert!(!r.contains_rect(&r.translate(vec2(0.0, -0.1))));
        // Empty rectangles are always considered as contained in other rectangles,
        // even if their origin is not.
        let p = Point2D::new(1.0, 1.0);
        assert!(!r.contains(p));
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
        let inner_rect = Rect::new(point2(20, 40), size2(80, 100));
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

    #[test]
    fn test_center() {
        let r: Rect<i32> = rect(-2, 5, 4, 10);
        assert_eq!(r.center(), point2(0, 10));

        let r: Rect<f32> = rect(1.0, 2.0, 3.0, 4.0);
        assert_eq!(r.center(), point2(2.5, 4.0));
    }
}
