/*!
Plane splitting.

Uses [euclid](https://crates.io/crates/euclid) for the math basis.
Introduces new geometrical primitives and associated logic.

Automatically splits a given set of 4-point polygons into sub-polygons
that don't intersect each other. This is useful for WebRender, to sort
the resulting sub-polygons by depth and avoid transparency blending issues.
*/
#![warn(missing_docs)]

extern crate binary_space_partition;
extern crate euclid;
#[macro_use]
extern crate log;
extern crate num_traits;

mod bsp;
mod polygon;

use std::{fmt, mem, ops};
use euclid::{TypedPoint3D, TypedVector3D};
use euclid::approxeq::ApproxEq;
use num_traits::{Float, One, Zero};

pub use self::bsp::BspSplitter;
pub use self::polygon::{Intersection, LineProjection, Polygon};


fn is_zero<T>(value: T) -> bool where
    T: Copy + Zero + ApproxEq<T> + ops::Mul<T, Output=T> {
    //HACK: this is rough, but the original Epsilon is too strict
    (value * value).approx_eq(&T::zero())
}

fn is_zero_vec<T, U>(vec: TypedVector3D<T, U>) -> bool where
   T: Copy + Zero + ApproxEq<T> +
      ops::Add<T, Output=T> + ops::Sub<T, Output=T> + ops::Mul<T, Output=T> {
    vec.dot(vec).approx_eq(&T::zero())
}


/// A generic line.
#[derive(Debug)]
pub struct Line<T, U> {
    /// Arbitrary point on the line.
    pub origin: TypedPoint3D<T, U>,
    /// Normalized direction of the line.
    pub dir: TypedVector3D<T, U>,
}

impl<T, U> Line<T, U> where
    T: Copy + One + Zero + ApproxEq<T> +
       ops::Add<T, Output=T> + ops::Sub<T, Output=T> + ops::Mul<T, Output=T>
{
    /// Check if the line has consistent parameters.
    pub fn is_valid(&self) -> bool {
        is_zero(self.dir.dot(self.dir) - T::one())
    }
    /// Check if two lines match each other.
    pub fn matches(&self, other: &Self) -> bool {
        let diff = self.origin - other.origin;
        is_zero_vec(self.dir.cross(other.dir)) &&
        is_zero_vec(self.dir.cross(diff))
    }
}


/// An infinite plane in 3D space, defined by equation:
/// dot(v, normal) + offset = 0
/// When used for plane splitting, it's defining a hemisphere
/// with equation "dot(v, normal) + offset > 0".
#[derive(Debug, PartialEq)]
pub struct Plane<T, U> {
    /// Normalized vector perpendicular to the plane.
    pub normal: TypedVector3D<T, U>,
    /// Constant offset from the normal plane, specified in the
    /// direction opposite to the normal.
    pub offset: T,
}

impl<T: Clone, U> Clone for Plane<T, U> {
    fn clone(&self) -> Self {
        Plane {
            normal: self.normal.clone(),
            offset: self.offset.clone(),
        }
    }
}

impl<
    T: Copy + Zero + One + Float + ApproxEq<T> +
        ops::Sub<T, Output=T> + ops::Add<T, Output=T> +
        ops::Mul<T, Output=T> + ops::Div<T, Output=T>,
    U,
> Plane<T, U> {
    /// Check if this plane contains another one.
    pub fn contains(&self, other: &Self) -> bool {
        //TODO: actually check for inside/outside
        self.normal == other.normal && self.offset == other.offset
    }

    /// Return the signed distance from this plane to a point.
    /// The distance is negative if the point is on the other side of the plane
    /// from the direction of the normal.
    pub fn signed_distance_to(&self, point: &TypedPoint3D<T, U>) -> T {
        point.to_vector().dot(self.normal) + self.offset
    }

    /// Compute the distance across the line to the plane plane,
    /// starting from the line origin.
    pub fn distance_to_line(&self, line: &Line<T, U>) -> T
    where T: ops::Neg<Output=T> {
        self.signed_distance_to(&line.origin) / -self.normal.dot(line.dir)
    }

    /// Compute the sum of signed distances to each of the points
    /// of another plane. Useful to know the relation of a plane that
    /// is a product of a split, and we know it doesn't intersect `self`.
    pub fn signed_distance_sum_to(&self, poly: &Polygon<T, U>) -> T {
        poly.points
            .iter()
            .fold(T::zero(), |u, p| u + self.signed_distance_to(p))
    }

    /// Check if a convex shape defined by a set of points is completely
    /// outside of this plane. Merely touching the surface is not
    /// considered an intersection.
    pub fn are_outside(&self, points: &[TypedPoint3D<T, U>]) -> bool {
        let d0 = self.signed_distance_to(&points[0]);
        points[1..]
            .iter()
            .all(|p| self.signed_distance_to(p) * d0 > T::zero())
    }

    /// Compute the line of intersection with another plane.
    pub fn intersect(&self, other: &Self) -> Option<Line<T, U>> {
        let cross_dir = self.normal.cross(other.normal);
        if cross_dir.dot(cross_dir) < T::approx_epsilon() {
            return None
        }

        // compute any point on the intersection between planes
        // (n1, v) + d1 = 0
        // (n2, v) + d2 = 0
        // v = a*n1/w + b*n2/w; w = (n1, n2)
        // v = (d2*w - d1) / (1 - w*w) * n1 - (d2 - d1*w) / (1 - w*w) * n2
        let w = self.normal.dot(other.normal);
        let factor = T::one() / (T::one() - w * w);
        let origin = TypedPoint3D::origin() +
            self.normal * ((other.offset * w - self.offset) * factor) -
            other.normal* ((other.offset - self.offset * w) * factor);

        Some(Line {
            origin,
            dir: cross_dir.normalize(),
        })
    }
}



/// Generic plane splitter interface
pub trait Splitter<T, U> {
    /// Reset the splitter results.
    fn reset(&mut self);

    /// Add a new polygon and return a slice of the subdivisions
    /// that avoid collision with any of the previously added polygons.
    fn add(&mut self, polygon: Polygon<T, U>);

    /// Sort the produced polygon set by the ascending distance across
    /// the specified view vector. Return the sorted slice.
    fn sort(&mut self, view: TypedVector3D<T, U>) -> &[Polygon<T, U>];

    /// Process a set of polygons at once.
    fn solve(
        &mut self,
        input: &[Polygon<T, U>],
        view: TypedVector3D<T, U>,
    ) -> &[Polygon<T, U>]
    where
        T: Clone,
        U: Clone,
    {
        self.reset();
        for p in input {
            self.add(p.clone());
        }
        self.sort(view)
    }
}


/// A helper object to clip polygons by a number of planes.
#[derive(Debug)]
pub struct Clipper<T, U> {
    clips: Vec<Plane<T, U>>,
    results: Vec<Polygon<T, U>>,
    temp: Vec<Polygon<T, U>>,
}

impl<
    T: Copy + fmt::Debug + ApproxEq<T> +
        ops::Sub<T, Output=T> + ops::Add<T, Output=T> +
        ops::Mul<T, Output=T> + ops::Div<T, Output=T> +
        Zero + One + Float,
    U: fmt::Debug,
> Clipper<T, U> {
    /// Create a new clipper object.
    pub fn new() -> Self {
        Clipper {
            clips: Vec::new(),
            results: Vec::new(),
            temp: Vec::new(),
        }
    }

    /// Add a clipping plane to the list. The plane will clip everything behind it,
    /// where the direction is set by the plane normal.
    pub fn add(&mut self, plane: Plane<T, U>) {
        self.clips.push(plane);
    }

    /// Clip specified polygon by the contained planes, return the fragmented polygons.
    pub fn clip(&mut self, polygon: Polygon<T, U>) -> &[Polygon<T, U>] {
        self.results.clear();
        self.results.push(polygon);
        for clip in &self.clips {
            self.temp.clear();
            mem::swap(&mut self.results, &mut self.temp);

            for mut poly in self.temp.drain(..) {
                if let Intersection::Inside(line) = poly.intersect_plane(clip) {
                    let (res1, res2) = poly.split(&line);
                    self.results.extend(
                        res1
                            .into_iter()
                            .chain(res2.into_iter())
                            .filter(|p| clip.signed_distance_sum_to(p) > T::zero())
                    );
                }
                // Note: if the intersection has happened, the `poly` will now
                // contain the remainder of the original polygon.
                if clip.signed_distance_sum_to(&poly) > T::zero() {
                    self.results.push(poly);
                }
            }
        }

        &self.results
    }
}


/// Helper method used for benchmarks and tests.
/// Constructs a 3D grid of polygons.
#[doc(hidden)]
pub fn make_grid(count: usize) -> Vec<Polygon<f32, ()>> {
    let mut polys: Vec<Polygon<f32, ()>> = Vec::with_capacity(count*3);
    let len = count as f32;
    polys.extend((0 .. count).map(|i| Polygon {
        points: [
            TypedPoint3D::new(0.0, i as f32, 0.0),
            TypedPoint3D::new(len, i as f32, 0.0),
            TypedPoint3D::new(len, i as f32, len),
            TypedPoint3D::new(0.0, i as f32, len),
        ],
        plane: Plane {
            normal: TypedVector3D::new(0.0, 1.0, 0.0),
            offset: -(i as f32),
        },
        anchor: 0,
    }));
    polys.extend((0 .. count).map(|i| Polygon {
        points: [
            TypedPoint3D::new(i as f32, 0.0, 0.0),
            TypedPoint3D::new(i as f32, len, 0.0),
            TypedPoint3D::new(i as f32, len, len),
            TypedPoint3D::new(i as f32, 0.0, len),
        ],
        plane: Plane {
            normal: TypedVector3D::new(1.0, 0.0, 0.0),
            offset: -(i as f32),
        },
        anchor: 0,
    }));
    polys.extend((0 .. count).map(|i| Polygon {
        points: [
            TypedPoint3D::new(0.0, 0.0, i as f32),
            TypedPoint3D::new(len, 0.0, i as f32),
            TypedPoint3D::new(len, len, i as f32),
            TypedPoint3D::new(0.0, len, i as f32),
        ],
        plane: Plane {
            normal: TypedVector3D::new(0.0, 0.0, 1.0),
            offset: -(i as f32),
        },
        anchor: 0,
    }));
    polys
}
