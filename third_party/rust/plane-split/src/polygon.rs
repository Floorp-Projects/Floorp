use {Line, Plane, is_zero};

use euclid::{Point2D, TypedTransform3D, TypedPoint3D, TypedVector3D, TypedRect};
use euclid::approxeq::ApproxEq;
use euclid::Trig;
use num_traits::{Float, One, Zero};

use std::{fmt, iter, mem, ops};


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
    /// Return None if the polygon doesn't contain any space.
    pub fn from_points(
        points: [TypedPoint3D<T, U>; 4],
        anchor: usize,
    ) -> Option<Self> {
        let edge1 = points[1] - points[0];
        let edge2 = points[2] - points[0];
        let edge3 = points[3] - points[0];
        let edge4 = points[3] - points[1];

        if edge2.square_length() < T::epsilon() || edge4.square_length() < T::epsilon() {
            return None
        }

        // one of them can be zero for redundant polygons produced by plane splitting
        //Note: this would be nicer if we used triangles instead of quads in the first place...
        // see https://github.com/servo/plane-split/issues/17
        let normal_rough1 = edge1.cross(edge2);
        let normal_rough2 = edge2.cross(edge3);
        let square_length1 = normal_rough1.square_length();
        let square_length2 = normal_rough2.square_length();
        let normal = if square_length1 > square_length2 {
            normal_rough1 / square_length1.sqrt()
        } else {
            normal_rough2 / square_length2.sqrt()
        };

        let offset = -points[0].to_vector()
            .dot(normal);

        Some(Polygon {
            points,
            plane: Plane {
                normal,
                offset,
            },
            anchor,
        })
    }

    /// Construct a polygon from a non-transformed rectangle.
    pub fn from_rect(rect: TypedRect<T, U>, anchor: usize) -> Self {
        Polygon {
            points: [
                rect.origin.to_3d(),
                rect.top_right().to_3d(),
                rect.bottom_right().to_3d(),
                rect.bottom_left().to_3d(),
            ],
            plane: Plane {
                normal: TypedVector3D::new(T::zero(), T::zero(), T::one()),
                offset: T::zero(),
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
        Self::from_points(points, anchor)
    }

    /// Construct a polygon from a rectangle with an invertible 3D transform.
    pub fn from_transformed_rect_with_inverse<V>(
        rect: TypedRect<T, V>,
        transform: &TypedTransform3D<T, V, U>,
        inv_transform: &TypedTransform3D<T, U, V>,
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

        // Compute the normal directly from the transformation. This guarantees consistent polygons
        // generated from various local rectanges on the same geometry plane.
        let normal_raw = TypedVector3D::new(inv_transform.m13, inv_transform.m23, inv_transform.m33);
        let normal_sql = normal_raw.square_length();
        if normal_sql.approx_eq(&T::zero()) || transform.m44.approx_eq(&T::zero()) {
            None
        } else {
            let normal = normal_raw / normal_sql.sqrt();
            let offset = -TypedVector3D::new(transform.m41, transform.m42, transform.m43)
                .dot(normal) / transform.m44;

            Some(Polygon {
                points,
                plane: Plane {
                    normal,
                    offset,
                },
                anchor,
            })
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

    /// Transform a polygon by an affine transform (preserving straight lines).
    pub fn transform<V>(
        &self, transform: &TypedTransform3D<T, U, V>
    ) -> Option<Polygon<T, V>>
    where
        T: Trig,
        V: fmt::Debug,
    {
        let mut points = [TypedPoint3D::origin(); 4];
        for (out, point) in points.iter_mut().zip(self.points.iter()) {
            let mut homo = transform.transform_point3d_homogeneous(point);
            homo.w = homo.w.max(T::approx_epsilon());
            *out = homo.to_point3d()?;
        }

        //Note: this code path could be more efficient if we had inverse-transpose
        //let n4 = transform.transform_point4d(&TypedPoint4D::new(T::zero(), T::zero(), T::one(), T::zero()));
        //let normal = TypedPoint3D::new(n4.x, n4.y, n4.z);
        Polygon::from_points(points, self.anchor)
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

    /// Check if the polygon doesn't contain any space. This may happen
    /// after a sequence of splits, and such polygons should be discarded.
    pub fn is_empty(&self) -> bool {
        (self.points[0] - self.points[2]).square_length() < T::epsilon() ||
        (self.points[1] - self.points[3]).square_length() < T::epsilon()
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

    fn split_impl(
        &mut self,
        first: (usize, TypedPoint3D<T, U>),
        second: (usize, TypedPoint3D<T, U>),
    ) -> (Option<Self>, Option<Self>) {
        //TODO: can be optimized for when the polygon has a redundant 4th vertex
        //TODO: can be simplified greatly if only working with triangles
        debug!("\t\tReached complex case [{}, {}]", first.0, second.0);
        let base = first.0;
        assert!(base < self.points.len());
        match second.0 - first.0 {
            1 => {
                // rect between the cut at the diagonal
                let other1 = Polygon {
                    points: [
                        first.1,
                        second.1,
                        self.points[(base + 2) & 3],
                        self.points[base],
                    ],
                    .. self.clone()
                };
                // triangle on the near side of the diagonal
                let other2 = Polygon {
                    points: [
                        self.points[(base + 2) & 3],
                        self.points[(base + 3) & 3],
                        self.points[base],
                        self.points[base],
                    ],
                    .. self.clone()
                };
                // triangle being cut out
                self.points = [
                    first.1,
                    self.points[(base + 1) & 3],
                    second.1,
                    second.1,
                ];
                (Some(other1), Some(other2))
            }
            2 => {
                // rect on the far side
                let other = Polygon {
                    points: [
                        first.1,
                        self.points[(base + 1) & 3],
                        self.points[(base + 2) & 3],
                        second.1,
                    ],
                    .. self.clone()
                };
                // rect on the near side
                self.points = [
                    first.1,
                    second.1,
                    self.points[(base + 3) & 3],
                    self.points[base],
                ];
                (Some(other), None)
            }
            3 => {
                // rect between the cut at the diagonal
                let other1 = Polygon {
                    points: [
                        first.1,
                        self.points[(base + 1) & 3],
                        self.points[(base + 3) & 3],
                        second.1,
                    ],
                    .. self.clone()
                };
                // triangle on the far side of the diagonal
                let other2 = Polygon {
                    points: [
                        self.points[(base + 1) & 3],
                        self.points[(base + 2) & 3],
                        self.points[(base + 3) & 3],
                        self.points[(base + 3) & 3],
                    ],
                    .. self.clone()
                };
                // triangle being cut out
                self.points = [
                    first.1,
                    second.1,
                    self.points[base],
                    self.points[base],
                ];
                (Some(other1), Some(other2))
            }
            _ => panic!("Unexpected indices {} {}", first.0, second.0),
        }
    }

    /// Split the polygon along the specified `Line`.
    /// Will do nothing if the line doesn't belong to the polygon plane.
    #[deprecated(note = "Use split_with_normal instead")]
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
            if let Some(t) = line.intersect_edge(a .. b) {
                if t >= T::zero() && t < T::one() {
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
        self.split_impl(
            (first, cuts[first].unwrap()),
            (second, cuts[second].unwrap()),
        )
    }

    /// Split the polygon along the specified `Line`, with a normal to the split line provided.
    /// This is useful when called by the plane splitter, since the other plane's normal
    /// forms the side direction here, and figuring out the actual line of split isn't needed.
    /// Will do nothing if the line doesn't belong to the polygon plane.
    pub fn split_with_normal(
        &mut self, line: &Line<T, U>, normal: &TypedVector3D<T, U>,
    ) -> (Option<Self>, Option<Self>) {
        debug!("\tSplitting with normal");
        // figure out which side of the split does each point belong to
        let mut sides = [T::zero(); 4];
        let (mut cut_positive, mut cut_negative) = (None, None);
        for (side, point) in sides.iter_mut().zip(&self.points) {
            *side = normal.dot(*point - line.origin);
        }
        // compute the edge intersection points
        for (i, ((&side1, point1), (&side0, point0))) in sides[1..]
            .iter()
            .chain(iter::once(&sides[0]))
            .zip(self.points[1..].iter().chain(iter::once(&self.points[0])))
            .zip(sides.iter().zip(&self.points))
            .enumerate()
        {
            // figure out if an edge between 0 and 1 needs to be cut
            let cut = if side0 < T::zero() && side1 >= T::zero() {
                &mut cut_positive
            } else if side0 > T::zero() && side1 <= T::zero() {
                &mut cut_negative
            } else {
                continue;
            };
            // compute the cut point by weighting the opposite distances
            //
            // Note: this algorithm is designed to not favor one end of the edge over the other.
            // The previous approach of calling `intersect_edge` sometimes ended up with "t" ever
            // slightly outside of [0, 1] range, since it was computing it relative to the first point only.
            //
            // Given that we are intersecting two straight lines, the triangles on both
            // sides of intersection are alike, so distances along the [point0, point1] line
            // are proportional to the side vector lengths we just computed: (side0, side1).
            let point = (*point0 * side1.abs() + point1.to_vector() * side0.abs()) / (side0 - side1).abs();
            debug_assert_eq!(*cut, None);
            *cut = Some((i, point));
        }
        // form new polygons
        if let (Some(first), Some(mut second)) = (cut_positive, cut_negative) {
            if second.0 < first.0 {
                second.0 += 4;
            }
            self.split_impl(first, second)
        } else {
            (None, None)
        }
    }
}
