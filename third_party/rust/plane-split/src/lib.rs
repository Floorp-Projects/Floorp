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
mod naive;

use std::{fmt, mem, ops};
use euclid::{Point2D, TypedMatrix4D, TypedPoint3D, TypedRect};
use euclid::approxeq::ApproxEq;
use euclid::trig::Trig;
use num_traits::{Float, One, Zero};

pub use self::bsp::BspSplitter;
pub use self::naive::NaiveSplitter;


fn is_zero<T>(value: T) -> bool where
    T: Copy + Zero + ApproxEq<T> + ops::Mul<T, Output=T> {
    //HACK: this is rough, but the original Epsilon is too strict
    (value * value).approx_eq(&T::zero())
}

fn is_zero_vec<T, U>(vec: TypedPoint3D<T, U>) -> bool where
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
    pub dir: TypedPoint3D<T, U>,
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

/// A convex flat polygon with 4 points, defined by equation:
/// dot(v, normal) + offset = 0
#[derive(Debug, PartialEq)]
pub struct Polygon<T, U> {
    /// Points making the polygon.
    pub points: [TypedPoint3D<T, U>; 4],
    /// Normalized vector perpendicular to the polygon plane.
    pub normal: TypedPoint3D<T, U>,
    /// Constant offset from the normal plane, specified in the
    /// direction opposite to the normal.
    pub offset: T,
    /// A simple anchoring index to allow association of the
    /// produced split polygons with the original one.
    pub anchor: usize,
}

impl<T: Clone, U> Clone for Polygon<T, U> {
    fn clone(&self) -> Self {
        Polygon {
            points: [self.points[0].clone(),
                     self.points[1].clone(),
                     self.points[2].clone(),
                     self.points[3].clone()],
            normal: self.normal.clone(),
            offset: self.offset.clone(),
            anchor: self.anchor,
        }
    }
}

/// The projection of a `Polygon` on a line.
pub struct LineProjection<T> {
    /// Projected value of each point in the polygon.
    pub markers: [T; 4],
}

impl<T> LineProjection<T> where
    T : Copy + PartialOrd + ops::Sub<T, Output=T> + ops::Add<T, Output=T>
{
    /// Get the min/max of the line projection markers.
    pub fn get_bounds(&self) -> (T, T) {
        let (mut a, mut b, mut c, mut d) = (self.markers[0], self.markers[1], self.markers[2], self.markers[3]);
        // bitonic sort of 4 elements
        // we could not just use `min/max` since they require `Ord` bound
        //TODO: make it nicer
        if a > c {
            mem::swap(&mut a, &mut c);
        }
        if b > d {
            mem::swap(&mut b, &mut d);
        }
        if a > b {
            mem::swap(&mut a, &mut b);
        }
        if c > d {
            mem::swap(&mut c, &mut d);
        }
        if b > c {
            mem::swap(&mut b, &mut c);
        }
        debug_assert!(a <= b && b <= c && c <= d);
        (a, d)
    }

    /// Check intersection with another line projection.
    pub fn intersect(&self, other: &Self) -> bool {
        // compute the bounds of both line projections
        let span = self.get_bounds();
        let other_span = other.get_bounds();
        // compute the total footprint
        let left = if span.0 < other_span.0 { span.0 } else { other_span.0 };
        let right = if span.1 > other_span.1 { span.1 } else { other_span.1 };
        // they intersect if the footprint is smaller than the sum
        right - left < span.1 - span.0 + other_span.1 - other_span.0
    }
}

/// Polygon intersection results.
pub enum Intersection<T> {
    /// Polygons are coplanar, including the case of being on the same plane.
    Coplanar,
    /// Polygon planes are intersecting, but polygons are not.
    Outside,
    /// Polygons are actually intersecting.
    Inside(T),
}

impl<T> Intersection<T> {
    /// Return true if the intersection is completely outside.
    pub fn is_outside(&self) -> bool {
        match *self {
            Intersection::Outside => true,
            _ => false,
        }
    }
    /// Return true if the intersection cuts the source polygon.
    pub fn is_inside(&self) -> bool {
        match *self {
            Intersection::Inside(_) => true,
            _ => false,
        }
    }
}

impl<T, U> Polygon<T, U> where
    T: Copy + fmt::Debug + ApproxEq<T> +
        ops::Sub<T, Output=T> + ops::Add<T, Output=T> +
        ops::Mul<T, Output=T> + ops::Div<T, Output=T> +
        Zero + One + Float,
    U: fmt::Debug,
{
    /// Construct a polygon from a transformed rectangle.
    pub fn from_transformed_rect<V>(rect: TypedRect<T, V>,
                                    transform: TypedMatrix4D<T, V, U>,
                                    anchor: usize)
                                    -> Polygon<T, U>
    where T: Trig + ops::Neg<Output=T> {
        let points = [
            transform.transform_point3d(&rect.origin.to_3d()),
            transform.transform_point3d(&rect.top_right().to_3d()),
            transform.transform_point3d(&rect.bottom_right().to_3d()),
            transform.transform_point3d(&rect.bottom_left().to_3d()),
        ];

        //Note: this code path could be more efficient if we had inverse-transpose
        //let n4 = transform.transform_point4d(&TypedPoint4D::new(T::zero(), T::zero(), T::one(), T::zero()));
        //let normal = TypedPoint3D::new(n4.x, n4.y, n4.z);

        let normal = (points[1] - points[0]).cross(points[2] - points[0])
                                            .normalize();
        let offset = -TypedPoint3D::new(transform.m41, transform.m42, transform.m43).dot(normal);

        Polygon {
            points: points,
            normal: normal,
            offset: offset,
            anchor: anchor,
        }
    }

    /// Bring a point into the local coordinate space, returning
    /// the 2D normalized coordinates.
    pub fn untransform_point(&self, point: TypedPoint3D<T, U>) -> Point2D<T> {
        //debug_assert!(self.contains(point));
        // get axises and target vector
        let a = self.points[1] - self.points[0];
        let b = self.points[3] - self.points[0];
        let c = point - self.points[0];
        // get pair-wise dot products
        let a2 = a.dot(a);
        let ab = a.dot(b);
        let b2 = b.dot(b);
        let ca = c.dot(a);
        let cb = c.dot(b);
        // compute the final coordinates
        let denom = ab * ab - a2 * b2;
        let x = ab * cb - b2 * ca;
        let y = ab * ca - a2 * cb;
        Point2D::new(x, y) / denom
    }

    /// Return the signed distance from this polygon to a point.
    /// The distance is negative if the point is on the other side of the polygon
    /// from the direction of the normal.
    pub fn signed_distance_to(&self, point: &TypedPoint3D<T, U>) -> T {
        point.dot(self.normal) + self.offset
    }

    /// Compute the distance across the line to the polygon plane,
    /// starting from the line origin.
    pub fn distance_to_line(&self, line: &Line<T, U>) -> T
    where T: ops::Neg<Output=T> {
        self.signed_distance_to(&line.origin) / -self.normal.dot(line.dir)
    }

    /// Compute the sum of signed distances to each of the points
    /// of another polygon. Useful to know the relation of a polygon that
    /// is a product of a split, and we know it doesn't intersect `self`.
    pub fn signed_distance_sum_to(&self, other: &Self) -> T {
        other.points.iter().fold(T::zero(), |sum, p| {
            sum + self.signed_distance_to(p)
        })
    }

    /// Check if all the points are indeed placed on the plane defined by
    /// the normal and offset, and the winding order is consistent.
    pub fn is_valid(&self) -> bool {
        let is_planar = self.points.iter()
                                   .all(|p| is_zero(self.signed_distance_to(p)));
        let edges = [self.points[1] - self.points[0],
                     self.points[2] - self.points[1],
                     self.points[3] - self.points[2],
                     self.points[0] - self.points[3]];
        let anchor = edges[3].cross(edges[0]);
        let is_winding = edges.iter()
                              .zip(edges[1..].iter())
                              .all(|(a, &b)| a.cross(b).dot(anchor) >= T::zero());
        is_planar && is_winding
    }

    /// Check if a convex shape defined by a set of points is completely
    /// outside of this polygon. Merely touching the surface is not
    /// considered an intersection.
    pub fn are_outside(&self, points: &[TypedPoint3D<T, U>]) -> bool {
        let d0 = self.signed_distance_to(&points[0]);
        points[1..].iter()
                   .all(|p| self.signed_distance_to(p) * d0 > T::zero())
    }

    /// Check if this polygon contains another one.
    pub fn contains(&self, other: &Self) -> bool {
        //TODO: actually check for inside/outside
        self.normal == other.normal && self.offset == other.offset
    }

    /// Project this polygon onto a 3D vector, returning a line projection.
    /// Note: we can think of it as a projection to a ray placed at the origin.
    pub fn project_on(&self, vector: &TypedPoint3D<T, U>) -> LineProjection<T> {
        LineProjection {
            markers: [
                vector.dot(self.points[0]),
                vector.dot(self.points[1]),
                vector.dot(self.points[2]),
                vector.dot(self.points[3]),
            ],
        }
    }

    /// Compute the line of intersection with another polygon.
    pub fn intersect(&self, other: &Self) -> Intersection<Line<T, U>> {
        if self.are_outside(&other.points) || other.are_outside(&self.points) {
            // one is completely outside the other
            debug!("\t\tOutside");
            return Intersection::Outside
        }
        let cross_dir = self.normal.cross(other.normal);
        if cross_dir.dot(cross_dir) < T::approx_epsilon() {
            // polygons are co-planar
            debug!("\t\tCoplanar");
            return Intersection::Coplanar
        }
        let self_proj = self.project_on(&cross_dir);
        let other_proj = other.project_on(&cross_dir);
        if !self_proj.intersect(&other_proj) {
            // projections on the line don't intersect
            debug!("\t\tProjection outside");
            return Intersection::Outside
        }
        // compute any point on the intersection between planes
        // (n1, v) + d1 = 0
        // (n2, v) + d2 = 0
        // v = a*n1/w + b*n2/w; w = (n1, n2)
        // v = (d2*w - d1) / (1 - w*w) * n1 - (d2 - d1*w) / (1 - w*w) * n2
        let w = self.normal.dot(other.normal);
        let factor = T::one() / (T::one() - w * w);
        let center = self.normal * ((other.offset * w - self.offset) * factor) -
                     other.normal* ((other.offset - self.offset * w) * factor);
        Intersection::Inside(Line {
            origin: center,
            dir: cross_dir.normalize(),
        })
    }

    /// Split the polygon along the specified `Line`. Will do nothing if the line
    /// doesn't belong to the polygon plane.
    pub fn split(&mut self, line: &Line<T, U>)
                 -> (Option<Polygon<T, U>>, Option<Polygon<T, U>>) {
        debug!("\tSplitting");
        // check if the cut is within the polygon plane first
        if !is_zero(self.normal.dot(line.dir)) ||
           !is_zero(self.signed_distance_to(&line.origin)) {
            debug!("\t\tDoes not belong to the plane, normal dot={:?}, origin distance={:?}",
                self.normal.dot(line.dir), self.signed_distance_to(&line.origin));
            return (None, None)
        }
        // compute the intersection points for each edge
        let mut cuts = [None; 4];
        for ((&b, &a), cut) in self.points.iter()
                                          .cycle()
                                          .skip(1)
                                          .zip(self.points.iter())
                                          .zip(cuts.iter_mut()) {
            // intersecting line segment [a, b] with `line`
            //a + (b-a) * t = r + k * d
            //(a, d) + t * (b-a, d) - (r, d) = k
            // a + t * (b-a) = r + t * (b-a, d) * d + (a-r, d) * d
            // t * ((b-a) - (b-a, d)*d) = (r-a) - (r-a, d) * d
            let pr = line.origin - a - line.dir * line.dir.dot(line.origin - a);
            let pb = b - a - line.dir * line.dir.dot(b - a);
            let denom = pb.dot(pb);
            if !denom.approx_eq(&T::zero()) {
                let t = pr.dot(pb) / denom;
                if t > T::zero() && t < T::one() {
                    *cut = Some(a + (b - a) * t);
                }
            }
        }

        let first = match cuts.iter().position(|c| c.is_some()) {
            Some(pos) => pos,
            None => return (None, None),
        };
        let second = match cuts[first+1 ..].iter().position(|c| c.is_some()) {
            Some(pos) => first + 1 + pos,
            None => return (None, None),
        };
        debug!("\t\tReached complex case [{}, {}]", first, second);
        //TODO: can be optimized for when the polygon has a redundant 4th vertex
        //TODO: can be simplified greatly if only working with triangles
        let (a, b) = (cuts[first].unwrap(), cuts[second].unwrap());
        match second-first {
            2 => {
                let mut other_points = self.points;
                other_points[first] = a;
                other_points[(first+3) % 4] = b;
                self.points[first+1] = a;
                self.points[first+2] = b;
                let poly = Polygon {
                    points: other_points,
                    .. self.clone()
                };
                (Some(poly), None)
            }
            3 => {
                let xpoints = [
                    self.points[first+1],
                    self.points[first+2],
                    self.points[first+3],
                    b];
                let ypoints = [a, self.points[first+1], b, b];
                self.points = [self.points[first], a, b, b];
                let poly1 = Polygon {
                    points: xpoints,
                    .. self.clone()
                };
                let poly2 = Polygon {
                    points: ypoints,
                    .. self.clone()
                };
                (Some(poly1), Some(poly2))
            }
            1 => {
                let xpoints = [
                    b,
                    self.points[(first+2) % 4],
                    self.points[(first+3) % 4],
                    self.points[first]
                    ];
                let ypoints = [self.points[first], a, b, b];
                self.points = [a, self.points[first+1], b, b];
                let poly1 = Polygon {
                    points: xpoints,
                    .. self.clone()
                };
                let poly2 = Polygon {
                    points: ypoints,
                    .. self.clone()
                };
                (Some(poly1), Some(poly2))
            }
            _ => panic!("Unexpected indices {} {}", first, second),
        }
    }
}


/// Generic plane splitter interface
pub trait Splitter<T, U> {
    /// Reset the splitter results.
    fn reset(&mut self);

    /// Add a new polygon and return a slice of the subdivisions
    /// that avoid collision with any of the previously added polygons.
    fn add(&mut self, Polygon<T, U>);

    /// Sort the produced polygon set by the ascending distance across
    /// the specified view vector. Return the sorted slice.
    fn sort(&mut self, TypedPoint3D<T, U>) -> &[Polygon<T, U>];

    /// Process a set of polygons at once.
    fn solve(&mut self, input: &[Polygon<T, U>], view: TypedPoint3D<T, U>)
             -> &[Polygon<T, U>]
    where T: Clone, U: Clone {
        self.reset();
        for p in input.iter() {
            self.add(p.clone());
        }
        self.sort(view)
    }
}


/// Helper method used for benchmarks and tests.
/// Constructs a 3D grid of polygons.
pub fn _make_grid(count: usize) -> Vec<Polygon<f32, ()>> {
    let mut polys: Vec<Polygon<f32, ()>> = Vec::with_capacity(count*3);
    let len = count as f32;
    polys.extend((0 .. count).map(|i| Polygon {
        points: [
            TypedPoint3D::new(0.0, i as f32, 0.0),
            TypedPoint3D::new(len, i as f32, 0.0),
            TypedPoint3D::new(len, i as f32, len),
            TypedPoint3D::new(0.0, i as f32, len),
        ],
        normal: TypedPoint3D::new(0.0, 1.0, 0.0),
        offset: -(i as f32),
        anchor: 0,
    }));
    polys.extend((0 .. count).map(|i| Polygon {
        points: [
            TypedPoint3D::new(i as f32, 0.0, 0.0),
            TypedPoint3D::new(i as f32, len, 0.0),
            TypedPoint3D::new(i as f32, len, len),
            TypedPoint3D::new(i as f32, 0.0, len),
        ],
        normal: TypedPoint3D::new(1.0, 0.0, 0.0),
        offset: -(i as f32),
        anchor: 0,
    }));
    polys.extend((0 .. count).map(|i| Polygon {
        points: [
            TypedPoint3D::new(0.0, 0.0, i as f32),
            TypedPoint3D::new(len, 0.0, i as f32),
            TypedPoint3D::new(len, len, i as f32),
            TypedPoint3D::new(0.0, len, i as f32),
        ],
        normal: TypedPoint3D::new(0.0, 0.0, 1.0),
        offset: -(i as f32),
        anchor: 0,
    }));
    polys
}
