use {Rect, Box2D, Box3D, Vector2D, Vector3D, size2, point2, point3};
use approxord::{min, max};
use num::Zero;
use core::ops::Deref;
use core::ops::{Add, Sub};
use core::cmp::{PartialEq};


#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(transparent))]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub struct NonEmpty<T>(pub(crate) T);

impl<T> Deref for NonEmpty<T> {
    type Target = T;
    fn deref(&self) -> &T {
        &self.0
    }
}

impl<T> NonEmpty<T> {
    #[inline]
    pub fn get(&self) -> &T {
        &self.0
    }
}

impl<T, U> NonEmpty<Rect<T, U>>
where
    T: Copy + Clone + Zero + PartialOrd + PartialEq + Add<T, Output = T> + Sub<T, Output = T>,
{
    #[inline]
    pub fn union(&self, other: &NonEmpty<Rect<T, U>>) -> NonEmpty<Rect<T, U>> {
        let origin = point2(
            min(self.min_x(), other.min_x()),
            min(self.min_y(), other.min_y()),
        );

        let lower_right_x = max(self.max_x(), other.max_x());
        let lower_right_y = max(self.max_y(), other.max_y());

        NonEmpty(Rect {
            origin,
            size: size2(
                lower_right_x - origin.x,
                lower_right_y - origin.y,
            ),
        })
    }

    #[inline]
    pub fn contains_rect(&self, rect: &Self) -> bool {
        self.min_x() <= rect.min_x()
            && rect.max_x() <= self.max_x()
            && self.min_y() <= rect.min_y()
            && rect.max_y() <= self.max_y()
    }

    #[inline]
    pub fn translate(&self, by: Vector2D<T, U>) -> Self {
        NonEmpty(self.0.translate(by))
    }
}

impl<T, U> NonEmpty<Box2D<T, U>>
where
    T: Copy + PartialOrd,
{
    #[inline]
    pub fn union(&self, other: &NonEmpty<Box2D<T, U>>) -> NonEmpty<Box2D<T, U>> {
        NonEmpty(Box2D {
            min: point2(
                min(self.min.x, other.min.x),
                min(self.min.y, other.min.y),
            ),
            max: point2(
                max(self.max.x, other.max.x),
                max(self.max.y, other.max.y),
            ),
        })
    }

    /// Returns true if this box contains the interior of the other box.
    #[inline]
    pub fn contains_box(&self, other: &Self) -> bool {
        self.min.x <= other.min.x
            && other.max.x <= self.max.x
            && self.min.y <= other.min.y
            && other.max.y <= self.max.y
    }
}

impl<T, U> NonEmpty<Box2D<T, U>>
where
    T: Copy + Add<T, Output = T>,
{
    #[inline]
    pub fn translate(&self, by: Vector2D<T, U>) -> Self {
        NonEmpty(self.0.translate(by))
    }
}

impl<T, U> NonEmpty<Box3D<T, U>>
where
    T: Copy + PartialOrd,
{
    #[inline]
    pub fn union(&self, other: &NonEmpty<Box3D<T, U>>) -> NonEmpty<Box3D<T, U>> {
        NonEmpty(Box3D {
            min: point3(
                min(self.min.x, other.min.x),
                min(self.min.y, other.min.y),
                min(self.min.z, other.min.z),
            ),
            max: point3(
                max(self.max.x, other.max.x),
                max(self.max.y, other.max.y),
                max(self.max.z, other.max.z),
            ),
        })
    }

    /// Returns true if this box contains the interior of the other box.
    #[inline]
    pub fn contains_box(&self, other: &Self) -> bool {
        self.min.x <= other.min.x
            && other.max.x <= self.max.x
            && self.min.y <= other.min.y
            && other.max.y <= self.max.y
            && self.min.z <= other.min.z
            && other.max.z <= self.max.z
    }
}

impl<T, U> NonEmpty<Box3D<T, U>>
where
    T: Copy + Add<T, Output = T>,
{
    #[inline]
    pub fn translate(&self, by: Vector3D<T, U>) -> Self {
        NonEmpty(self.0.translate(by))
    }
}

#[test]
fn empty_nonempty() {
    use default;

    // zero-width
    let box1: default::Box2D<i32> = Box2D {
        min: point2(-10, 2),
        max: point2(-10, 12),
    };
    // zero-height
    let box2: default::Box2D<i32> = Box2D {
        min: point2(0, 11),
        max: point2(2, 11),
    };
    // negative width
    let box3: default::Box2D<i32> = Box2D {
        min: point2(1, 11),
        max: point2(0, 12),
    };
    // negative height
    let box4: default::Box2D<i32> = Box2D {
        min: point2(0, 11),
        max: point2(5, 10),
    };

    assert!(box1.to_non_empty().is_none());
    assert!(box2.to_non_empty().is_none());
    assert!(box3.to_non_empty().is_none());
    assert!(box4.to_non_empty().is_none());
}

#[test]
fn nonempty_union() {
    use default;

    let box1: default::Box2D<i32> = Box2D {
        min: point2(-10, 2),
        max: point2(15, 12),
    };
    let box2 = Box2D {
        min: point2(-2, -5),
        max: point2(10, 5),
    };

    assert_eq!(box1.union(&box2), *box1.to_non_empty().unwrap().union(&box2.to_non_empty().unwrap()));

    let box3: default::Box3D<i32> = Box3D {
        min: point3(1, -10, 2),
        max: point3(6, 15, 12),
    };
    let box4 = Box3D {
        min: point3(0, -2, -5),
        max: point3(7, 10, 5),
    };

    assert_eq!(box3.union(&box4), *box3.to_non_empty().unwrap().union(&box4.to_non_empty().unwrap()));

    let rect1: default::Rect<i32> = Rect {
        origin: point2(1, 2),
        size: size2(3, 4),
    };
    let rect2 = Rect {
        origin: point2(-1, 5),
        size: size2(1, 10),
    };

    assert_eq!(rect1.union(&rect2), *rect1.to_non_empty().unwrap().union(&rect2.to_non_empty().unwrap()));
}

#[test]
fn nonempty_contains() {
    use default;
    use {vec2, vec3};

    let r: NonEmpty<default::Rect<i32>> = Rect {
        origin: point2(-20, 15),
        size: size2(100, 200),
    }.to_non_empty().unwrap();

    assert!(r.contains_rect(&r));
    assert!(!r.contains_rect(&r.translate(vec2(1, 0))));
    assert!(!r.contains_rect(&r.translate(vec2(-1, 0))));
    assert!(!r.contains_rect(&r.translate(vec2(0, 1))));
    assert!(!r.contains_rect(&r.translate(vec2(0, -1))));

    let b: NonEmpty<default::Box2D<i32>> = Box2D {
        min: point2(-10, 5),
        max: point2(30, 100),
    }.to_non_empty().unwrap();

    assert!(b.contains_box(&b));
    assert!(!b.contains_box(&b.translate(vec2(1, 0))));
    assert!(!b.contains_box(&b.translate(vec2(-1, 0))));
    assert!(!b.contains_box(&b.translate(vec2(0, 1))));
    assert!(!b.contains_box(&b.translate(vec2(0, -1))));

    let b: NonEmpty<default::Box3D<i32>> = Box3D {
        min: point3(-1, -10, 5),
        max: point3(10, 30, 100),
    }.to_non_empty().unwrap();

    assert!(b.contains_box(&b));
    assert!(!b.contains_box(&b.translate(vec3(0, 1, 0))));
    assert!(!b.contains_box(&b.translate(vec3(0, -1, 0))));
    assert!(!b.contains_box(&b.translate(vec3(0, 0, 1))));
    assert!(!b.contains_box(&b.translate(vec3(0, 0, -1))));
    assert!(!b.contains_box(&b.translate(vec3(1, 1, 0))));
    assert!(!b.contains_box(&b.translate(vec3(1, -1, 0))));
    assert!(!b.contains_box(&b.translate(vec3(1, 0, 1))));
    assert!(!b.contains_box(&b.translate(vec3(1, 0, -1))));
    assert!(!b.contains_box(&b.translate(vec3(-1, 1, 0))));
    assert!(!b.contains_box(&b.translate(vec3(-1, -1, 0))));
    assert!(!b.contains_box(&b.translate(vec3(-1, 0, 1))));
    assert!(!b.contains_box(&b.translate(vec3(-1, 0, -1))));
}
