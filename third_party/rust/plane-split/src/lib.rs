/*!
Plane splitting.

Uses [euclid](https://crates.io/crates/euclid) for the math basis.
Introduces new geometrical primitives and associated logic.

Automatically splits a given set of 4-point polygons into sub-polygons
that don't intersect each other. This is useful for WebRender, to sort
the resulting sub-polygons by depth and avoid transparency blending issues.
*/
#![warn(missing_docs)]

mod bsp;
mod clip;
mod polygon;

pub use polygon::PlaneCut;

use euclid::{
    approxeq::ApproxEq,
    default::{Point3D, Scale, Vector3D},
};

use std::ops;

pub use self::bsp::BspSplitter;
pub use self::clip::Clipper;
pub use self::polygon::{Intersection, LineProjection, Polygon};

fn is_zero(value: f64) -> bool {
    //HACK: this is rough, but the original Epsilon is too strict
    (value * value).approx_eq(&0.0)
}

fn is_zero_vec(vec: Vector3D<f64>) -> bool {
    vec.dot(vec).approx_eq(&0.0)
}

/// A generic line.
#[derive(Debug)]
pub struct Line {
    /// Arbitrary point on the line.
    pub origin: Point3D<f64>,
    /// Normalized direction of the line.
    pub dir: Vector3D<f64>,
}

impl Line {
    /// Check if the line has consistent parameters.
    pub fn is_valid(&self) -> bool {
        is_zero(self.dir.dot(self.dir) - 1.0)
    }
    /// Check if two lines match each other.
    pub fn matches(&self, other: &Self) -> bool {
        let diff = self.origin - other.origin;
        is_zero_vec(self.dir.cross(other.dir)) && is_zero_vec(self.dir.cross(diff))
    }

    /// Intersect an edge given by the end points.
    /// Returns the fraction of the edge where the intersection occurs.
    fn intersect_edge(&self, edge: ops::Range<Point3D<f64>>) -> Option<f64> {
        let edge_vec = edge.end - edge.start;
        let origin_vec = self.origin - edge.start;
        // edge.start + edge_vec * t = r + k * d
        // (edge.start, d) + t * (edge_vec, d) - (r, d) = k
        // edge.start + t * edge_vec = r + t * (edge_vec, d) * d + (start-r, d) * d
        // t * (edge_vec - (edge_vec, d)*d) = origin_vec - (origin_vec, d) * d
        let pr = origin_vec - self.dir * self.dir.dot(origin_vec);
        let pb = edge_vec - self.dir * self.dir.dot(edge_vec);
        let denom = pb.dot(pb);
        if denom.approx_eq(&0.0) {
            None
        } else {
            Some(pr.dot(pb) / denom)
        }
    }
}

/// An infinite plane in 3D space, defined by equation:
/// dot(v, normal) + offset = 0
/// When used for plane splitting, it's defining a hemisphere
/// with equation "dot(v, normal) + offset > 0".
#[derive(Debug, PartialEq)]
pub struct Plane {
    /// Normalized vector perpendicular to the plane.
    pub normal: Vector3D<f64>,
    /// Constant offset from the normal plane, specified in the
    /// direction opposite to the normal.
    pub offset: f64,
}

impl Clone for Plane {
    fn clone(&self) -> Self {
        Plane {
            normal: self.normal.clone(),
            offset: self.offset.clone(),
        }
    }
}

/// An error returned when everything would end up projected
/// to the negative hemisphere (W <= 0.0);
#[derive(Clone, Debug, Hash, PartialEq, PartialOrd)]
pub struct NegativeHemisphereError;

impl Plane {
    /// Construct a new plane from unnormalized equation.
    pub fn from_unnormalized(
        normal: Vector3D<f64>,
        offset: f64,
    ) -> Result<Option<Self>, NegativeHemisphereError> {
        let square_len = normal.square_length();
        if square_len < f64::approx_epsilon() * f64::approx_epsilon() {
            if offset > 0.0 {
                Ok(None)
            } else {
                Err(NegativeHemisphereError)
            }
        } else {
            let kf = 1.0 / square_len.sqrt();
            Ok(Some(Plane {
                normal: normal * Scale::new(kf),
                offset: offset * kf,
            }))
        }
    }

    /// Check if this plane contains another one.
    pub fn contains(&self, other: &Self) -> bool {
        //TODO: actually check for inside/outside
        self.normal == other.normal && self.offset == other.offset
    }

    /// Return the signed distance from this plane to a point.
    /// The distance is negative if the point is on the other side of the plane
    /// from the direction of the normal.
    pub fn signed_distance_to(&self, point: &Point3D<f64>) -> f64 {
        point.to_vector().dot(self.normal) + self.offset
    }

    /// Compute the distance across the line to the plane plane,
    /// starting from the line origin.
    pub fn distance_to_line(&self, line: &Line) -> f64 {
        self.signed_distance_to(&line.origin) / -self.normal.dot(line.dir)
    }

    /// Compute the sum of signed distances to each of the points
    /// of another plane. Useful to know the relation of a plane that
    /// is a product of a split, and we know it doesn't intersect `self`.
    pub fn signed_distance_sum_to<A>(&self, poly: &Polygon<A>) -> f64 {
        poly.points
            .iter()
            .fold(0.0, |u, p| u + self.signed_distance_to(p))
    }

    /// Check if a convex shape defined by a set of points is completely
    /// outside of this plane. Merely touching the surface is not
    /// considered an intersection.
    pub fn are_outside(&self, points: &[Point3D<f64>]) -> bool {
        let d0 = self.signed_distance_to(&points[0]);
        points[1..]
            .iter()
            .all(|p| self.signed_distance_to(p) * d0 > 0.0)
    }

    //TODO(breaking): turn this into Result<Line, DotProduct>
    /// Compute the line of intersection with another plane.
    pub fn intersect(&self, other: &Self) -> Option<Line> {
        // compute any point on the intersection between planes
        // (n1, v) + d1 = 0
        // (n2, v) + d2 = 0
        // v = a*n1/w + b*n2/w; w = (n1, n2)
        // v = (d2*w - d1) / (1 - w*w) * n1 - (d2 - d1*w) / (1 - w*w) * n2
        let w = self.normal.dot(other.normal);
        let divisor = 1.0 - w * w;
        if divisor < f64::approx_epsilon() * f64::approx_epsilon() {
            return None;
        }
        let origin = Point3D::origin() + self.normal * ((other.offset * w - self.offset) / divisor)
            - other.normal * ((other.offset - self.offset * w) / divisor);

        let cross_dir = self.normal.cross(other.normal);
        // note: the cross product isn't too close to zero
        // due to the previous check

        Some(Line {
            origin,
            dir: cross_dir.normalize(),
        })
    }
}

/// Helper method used for benchmarks and tests.
/// Constructs a 3D grid of polygons.
#[doc(hidden)]
pub fn make_grid(count: usize) -> Vec<Polygon<usize>> {
    let mut polys: Vec<Polygon<usize>> = Vec::with_capacity(count * 3);
    let len = count as f64;
    polys.extend((0..count).map(|i| Polygon {
        points: [
            Point3D::new(0.0, i as f64, 0.0),
            Point3D::new(len, i as f64, 0.0),
            Point3D::new(len, i as f64, len),
            Point3D::new(0.0, i as f64, len),
        ],
        plane: Plane {
            normal: Vector3D::new(0.0, 1.0, 0.0),
            offset: -(i as f64),
        },
        anchor: 0,
    }));
    polys.extend((0..count).map(|i| Polygon {
        points: [
            Point3D::new(i as f64, 0.0, 0.0),
            Point3D::new(i as f64, len, 0.0),
            Point3D::new(i as f64, len, len),
            Point3D::new(i as f64, 0.0, len),
        ],
        plane: Plane {
            normal: Vector3D::new(1.0, 0.0, 0.0),
            offset: -(i as f64),
        },
        anchor: 0,
    }));
    polys.extend((0..count).map(|i| Polygon {
        points: [
            Point3D::new(0.0, 0.0, i as f64),
            Point3D::new(len, 0.0, i as f64),
            Point3D::new(len, len, i as f64),
            Point3D::new(0.0, len, i as f64),
        ],
        plane: Plane {
            normal: Vector3D::new(0.0, 0.0, 1.0),
            offset: -(i as f64),
        },
        anchor: 0,
    }));
    polys
}
