use {Line, Plane, is_zero};

use std::{fmt, mem, ops};
use euclid::{Point2D, TypedTransform3D, TypedPoint3D, TypedVector3D, TypedRect};
use euclid::approxeq::ApproxEq;
use euclid::Trig;
use num_traits::{Float, One, Zero};


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


/// A convex polygon with 4 points lying on a plane.
#[derive(Debug, PartialEq)]
pub struct Polygon<T, U> {
    /// Points making the polygon.
    pub points: [TypedPoint3D<T, U>; 4],
    /// A plane describing polygon orientation.
    pub plane: Plane<T, U>,
    /// A simple anchoring index to allow association of the
    /// produced split polygons with the original one.
    pub anchor: usize,
}

impl<T: Clone, U> Clone for Polygon<T, U> {
    fn clone(&self) -> Self {
        Polygon {
            points: [
                self.points[0].clone(),
                 self.points[1].clone(),
                 self.points[2].clone(),
                 self.points[3].clone(),
            ],
            plane: self.plane.clone(),
            anchor: self.anchor,
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
    /// Construct a polygon from points that are already transformed.
    pub fn from_points(
        points: [TypedPoint3D<T, U>; 4],
        anchor: usize,
    ) -> Self {
        let normal = (points[1] - points[0])
            .cross(points[2] - points[0])
            .normalize();
        let offset = -points[0].to_vector()
            .dot(normal);

        Polygon {
            points,
            plane: Plane {
                normal,
                offset,
            },
            anchor,
        }
    }

    /// Construct a polygon from a rectangle with 3D transform.
    pub fn from_transformed_rect<V>(
        rect: TypedRect<T, V>,
        transform: TypedTransform3D<T, V, U>,
        anchor: usize,
    ) -> Option<Self>
    where
        T: Trig + ops::Neg<Output=T>,
    {
        let points = [
            transform.transform_point3d(&rect.origin.to_3d())?,
            transform.transform_point3d(&rect.top_right().to_3d())?,
            transform.transform_point3d(&rect.bottom_right().to_3d())?,
            transform.transform_point3d(&rect.bottom_left().to_3d())?,
        ];

        //Note: this code path could be more efficient if we had inverse-transpose
        //let n4 = transform.transform_point4d(&TypedPoint4D::new(T::zero(), T::zero(), T::one(), T::zero()));
        //let normal = TypedPoint3D::new(n4.x, n4.y, n4.z);
        Some(Self::from_points(points, anchor))
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

    /// Check if all the points are indeed placed on the plane defined by
    /// the normal and offset, and the winding order is consistent.
    pub fn is_valid(&self) -> bool {
        let is_planar = self.points
            .iter()
            .all(|p| is_zero(self.plane.signed_distance_to(p)));
        let edges = [
            self.points[1] - self.points[0],
            self.points[2] - self.points[1],
            self.points[3] - self.points[2],
            self.points[0] - self.points[3],
        ];
        let anchor = edges[3].cross(edges[0]);
        let is_winding = edges
            .iter()
            .zip(edges[1..].iter())
            .all(|(a, &b)| a.cross(b).dot(anchor) >= T::zero());
        is_planar && is_winding
    }

    /// Check if this polygon contains another one.
    pub fn contains(&self, other: &Self) -> bool {
        //TODO: actually check for inside/outside
        self.plane.contains(&other.plane)
    }


    /// Project this polygon onto a 3D vector, returning a line projection.
    /// Note: we can think of it as a projection to a ray placed at the origin.
    pub fn project_on(&self, vector: &TypedVector3D<T, U>) -> LineProjection<T> {
        LineProjection {
            markers: [
                vector.dot(self.points[0].to_vector()),
                vector.dot(self.points[1].to_vector()),
                vector.dot(self.points[2].to_vector()),
                vector.dot(self.points[3].to_vector()),
            ],
        }
    }

    /// Compute the line of intersection with an infinite plane.
    pub fn intersect_plane(&self, other: &Plane<T, U>) -> Intersection<Line<T, U>> {
        if other.are_outside(&self.points) {
            debug!("\t\tOutside of the plane");
            return Intersection::Outside
        }
        match self.plane.intersect(&other) {
            Some(line) => Intersection::Inside(line),
            None => {
                debug!("\t\tCoplanar");
                Intersection::Coplanar
            }
        }
    }

    /// Compute the line of intersection with another polygon.
    pub fn intersect(&self, other: &Self) -> Intersection<Line<T, U>> {
        if self.plane.are_outside(&other.points) || other.plane.are_outside(&self.points) {
            debug!("\t\tOne is completely outside of the other");
            return Intersection::Outside
        }
        match self.plane.intersect(&other.plane) {
            Some(line) => {
                let self_proj = self.project_on(&line.dir);
                let other_proj = other.project_on(&line.dir);
                if self_proj.intersect(&other_proj) {
                    Intersection::Inside(line)
                } else {
                    // projections on the line don't intersect
                    debug!("\t\tProjection is outside");
                    Intersection::Outside
                }
            }
            None => {
                debug!("\t\tCoplanar");
                Intersection::Coplanar
            }
        }
    }

    /// Split the polygon along the specified `Line`. Will do nothing if the line
    /// doesn't belong to the polygon plane.
    pub fn split(&mut self, line: &Line<T, U>) -> (Option<Self>, Option<Self>) {
        debug!("\tSplitting");
        // check if the cut is within the polygon plane first
        if !is_zero(self.plane.normal.dot(line.dir)) ||
           !is_zero(self.plane.signed_distance_to(&line.origin)) {
            debug!("\t\tDoes not belong to the plane, normal dot={:?}, origin distance={:?}",
                self.plane.normal.dot(line.dir), self.plane.signed_distance_to(&line.origin));
            return (None, None)
        }
        // compute the intersection points for each edge
        let mut cuts = [None; 4];
        for ((&b, &a), cut) in self.points
            .iter()
            .cycle()
            .skip(1)
            .zip(self.points.iter())
            .zip(cuts.iter_mut())
        {
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
