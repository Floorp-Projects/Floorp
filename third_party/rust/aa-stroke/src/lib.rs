
use std::default::Default;

use bezierflattener::CBezierFlattener;

use crate::{bezierflattener::{CFlatteningSink, GpPointR, HRESULT, S_OK, CBezier}};

mod bezierflattener;
pub mod tri_rasterize;
#[cfg(feature = "c_bindings")]
pub mod c_bindings;

#[derive(Clone, Copy, PartialEq, Debug)]
pub enum Winding {
    EvenOdd,
    NonZero,
}

#[derive(Clone, Copy, Debug)]
pub enum PathOp {
    MoveTo(Point),
    LineTo(Point),
    QuadTo(Point, Point),
    CubicTo(Point, Point, Point),
    Close,
}


/// Represents a complete path usable for filling or stroking.
#[derive(Clone, Debug)]
pub struct Path {
    pub ops: Vec<PathOp>,
    pub winding: Winding,
}

pub type Point = euclid::default::Point2D<f32>;
pub type Transform = euclid::default::Transform2D<f32>;
pub type Vector = euclid::default::Vector2D<f32>;


#[derive(Clone, Copy, PartialEq, Debug)]
#[repr(C)]
pub enum LineCap {
    Round,
    Square,
    Butt,
}

#[derive(Clone, Copy, PartialEq, Debug)]
#[repr(C)]
pub enum LineJoin {
    Round,
    Miter,
    Bevel,
}

#[derive(Clone, PartialEq, Debug)]
#[repr(C)]
pub struct StrokeStyle {
    pub width: f32,
    pub cap: LineCap,
    pub join: LineJoin,
    pub miter_limit: f32,
}

impl Default for StrokeStyle {
    fn default() -> Self {
        StrokeStyle {
            width: 1.,
            cap: LineCap::Butt,
            join: LineJoin::Miter,
            miter_limit: 10.,
        }
    }
}
#[derive(Debug)]
pub struct Vertex {
    pub x: f32,
    pub y: f32,
    pub coverage: f32
}

/// A helper struct used for constructing a `Path`.
pub struct PathBuilder<'z> {
    output_buffer: Option<&'z mut [Vertex]>,
    output_buffer_offset: usize,
    vertices: Vec<Vertex>,
    coverage: f32,
    aa: bool
}



impl<'z> PathBuilder<'z> {
    pub fn new(coverage: f32) -> PathBuilder<'z> {
        PathBuilder {
            output_buffer: None,
            output_buffer_offset: 0,
            vertices: Vec::new(),
            coverage,
            aa: true
        }
    }

    pub fn set_output_buffer(&mut self, output_buffer: &'z mut [Vertex]) {
        assert!(self.output_buffer.is_none());
        self.output_buffer = Some(output_buffer);
    }

    pub fn push_tri_with_coverage(&mut self, x1: f32, y1: f32, c1: f32, x2: f32, y2: f32, c2: f32, x3: f32, y3: f32, c3: f32) {
        let v1 = Vertex { x: x1, y: y1, coverage: c1 };
        let v2 = Vertex { x: x2, y: y2, coverage: c2 };
        let v3 = Vertex { x: x3, y: y3, coverage: c3 };
        if let Some(output_buffer) = &mut self.output_buffer {
            let offset = self.output_buffer_offset;
            if offset + 3 <= output_buffer.len() {
                output_buffer[offset] = v1;
                output_buffer[offset + 1] = v2;
                output_buffer[offset + 2] = v3;
            }
            self.output_buffer_offset = offset + 3;
        } else {
            self.vertices.push(v1);
            self.vertices.push(v2);
            self.vertices.push(v3);
        }
    }

    pub fn push_tri(&mut self, x1: f32, y1: f32, x2: f32, y2: f32, x3: f32, y3: f32) {
        self.push_tri_with_coverage(x1, y1, self.coverage, x2, y2, self.coverage, x3, y3, self.coverage);
    }


    // x3, y3 is the full coverage vert
    pub fn tri_ramp(&mut self, x1: f32, y1: f32, x2: f32, y2: f32, x3: f32, y3: f32) {
        self.push_tri_with_coverage(x1, y1, 0., x2, y2, 0., x3, y3, self.coverage);
    }

    pub fn quad(&mut self, x1: f32, y1: f32, x2: f32, y2: f32, x3: f32, y3: f32, x4: f32, y4: f32) {
        self.push_tri(x1, y1, x2, y2, x3, y3);
        self.push_tri(x3, y3, x4, y4, x1, y1);
    }

    pub fn ramp(&mut self, x1: f32, y1: f32, x2: f32, y2: f32, x3: f32, y3: f32, x4: f32, y4: f32) {
        self.push_tri_with_coverage(x1, y1, self.coverage, x2, y2, 0., x3, y3, 0.);
        self.push_tri_with_coverage(x3, y3, 0., x4, y4, self.coverage, x1, y1, self.coverage);
    }

    // first edge is outside
    pub fn tri(&mut self, x1: f32, y1: f32, x2: f32, y2: f32, x3: f32, y3: f32) {
        self.push_tri(x1, y1, x2, y2, x3, y3);
    }

    pub fn arc_wedge(&mut self, c: Point, radius: f32, a: Vector, b: Vector) {
        arc(self, c.x, c.y, radius, a, b);
    }

    /// Completes the current path
    pub fn finish(self) -> Vec<Vertex> {
        self.vertices
    }

    pub fn get_output_buffer_size(&self) -> Option<usize> {
        if self.output_buffer.is_some() {
            Some(self.output_buffer_offset)
        } else {
            None
        }
    }
}



fn compute_normal(p0: Point, p1: Point) -> Option<Vector> {
    let ux = p1.x - p0.x;
    let uy = p1.y - p0.y;

    // this could overflow f32. Skia checks for this and
    // uses a double in that situation
    let ulen = ux.hypot(uy);
    if ulen == 0. {
        return None;
    }
    // the normal is perpendicular to the *unit* vector
    Some(Vector::new(-uy / ulen, ux / ulen))
}

fn flip(v: Vector) -> Vector {
    Vector::new(-v.x, -v.y)
}

/* Compute a spline approximation of the arc
centered at xc, yc from the angle a to the angle b

The angle between a and b should not be more than a
quarter circle (pi/2)

The approximation is similar to an approximation given in:
"Approximation of a cubic bezier curve by circular arcs and vice versa"
by Alekas Riškus. However that approximation becomes unstable when the
angle of the arc approaches 0.

This approximation is inspired by a discusion with Boris Zbarsky
and essentially just computes:

  h = 4.0/3.0 * tan ((angle_B - angle_A) / 4.0);

without converting to polar coordinates.

A different way to do this is covered in "Approximation of a cubic bezier
curve by circular arcs and vice versa" by Alekas Riškus. However, the method
presented there doesn't handle arcs with angles close to 0 because it
divides by the perp dot product of the two angle vectors.
*/

fn arc_segment_tri(path: &mut PathBuilder, xc: f32, yc: f32, radius: f32, a: Vector, b: Vector) {
    let r_sin_a = radius * a.y;
    let r_cos_a = radius * a.x;
    let r_sin_b = radius * b.y;
    let r_cos_b = radius * b.x;


    /* bisect the angle between 'a' and 'b' with 'mid' */
    let mut mid = a + b;
    mid /= mid.length();

    /* bisect the angle between 'a' and 'mid' with 'mid2' this is parallel to a
     * line with angle (B - A)/4 */
    let mid2 = a + mid;

    let h = (4. / 3.) * dot(perp(a), mid2) / dot(a, mid2);

    let last_point = GpPointR { x: (xc + r_cos_a) as f64, y: (yc + r_sin_a) as f64 };
    let initial_normal = GpPointR { x: a.x as f64, y: a.y as f64 };


    struct Target<'a, 'b> { last_point: GpPointR, last_normal: GpPointR, xc: f32, yc: f32, path: &'a mut PathBuilder<'b> }
    impl<'a, 'b> CFlatteningSink for Target<'a, 'b> {
        fn AcceptPointAndTangent(&mut self,
            pt: &GpPointR,
                // The point
            vec: &GpPointR,
                // The tangent there
            _last: bool
                // Is this the last point on the curve?
            ) -> HRESULT {
                if self.path.aa {
                    let len = vec.Norm();
                    let normal = *vec/len;
                    let normal = GpPointR { x: -normal.y, y: normal.x };
                    // FIXME: we probably need more width here because
                    // the normals are not perpendicular with the edge
                    let width = 0.5; 

                    self.path.ramp(
                    (pt.x - normal.x * width) as f32, 
                    (pt.y - normal.y * width) as f32,
                    (pt.x + normal.x * width) as f32, 
                    (pt.y + normal.y * width) as f32,
                    (self.last_point.x + self.last_normal.x * width) as f32,
                    (self.last_point.y + self.last_normal.y * width) as f32, 
                    (self.last_point.x - self.last_normal.x * width) as f32,
                    (self.last_point.y - self.last_normal.y * width) as f32, );
                    self.path.push_tri(
                        (self.last_point.x - self.last_normal.x * 0.5) as f32,
                        (self.last_point.y - self.last_normal.y * 0.5) as f32, 
                        (pt.x - normal.x * 0.5) as f32, 
                        (pt.y - normal.y * 0.5) as f32,
                         self.xc, self.yc);
                    self.last_normal = normal;

                } else {
                    self.path.push_tri(self.last_point.x as f32, self.last_point.y as f32, pt.x as f32, pt.y as f32, self.xc, self.yc);
                }
                self.last_point = pt.clone();
                return S_OK;
        }

        fn AcceptPoint(&mut self,
            pt: &GpPointR,
                // The point
            _t: f64,
                // Parameter we're at
            _aborted: &mut bool,
            _last_point: bool) -> HRESULT {
            self.path.push_tri(self.last_point.x as f32, self.last_point.y as f32, pt.x as f32, pt.y as f32, self.xc, self.yc);
            self.last_point = pt.clone();
            return S_OK;
        }
    }
    let bezier = CBezier::new([GpPointR { x: (xc + r_cos_a) as f64, y: (yc + r_sin_a) as f64,  },
        GpPointR { x: (xc + r_cos_a - h * r_sin_a) as f64, y: (yc + r_sin_a + h * r_cos_a) as f64, },
        GpPointR { x: (xc + r_cos_b + h * r_sin_b) as f64, y: (yc + r_sin_b - h * r_cos_b) as f64, },
        GpPointR { x: (xc + r_cos_b) as f64, y: (yc + r_sin_b) as f64, }]);
    if bezier.is_degenerate() {
        return;
    }
    let mut t = Target{ last_point, last_normal: initial_normal, xc, yc, path };
    let mut f = CBezierFlattener::new(&bezier, &mut t, 0.25);
    f.Flatten(true);

}

/* The angle between the vectors must be <= pi */
fn bisect(a: Vector, b: Vector) -> Vector {
    let mut mid;
    if dot(a, b) >= 0. {
        /* if the angle between a and b is accute, then we can
         * just add the vectors and normalize */
        mid = a + b;
    } else {
        /* otherwise, we can flip a, add it
         * and then use the perpendicular of the result */
        mid = flip(a) + b;
        mid = perp(mid);
    }

    /* normalize */
    /* because we assume that 'a' and 'b' are normalized, we can use
     * sqrt instead of hypot because the range of mid is limited */
    let mid_len = mid.x * mid.x + mid.y * mid.y;
    let len = mid_len.sqrt();
    return mid / len;
}

/* The angle between the vectors must be <= 180 degrees */
fn arc(path: &mut PathBuilder, xc: f32, yc: f32, radius: f32, a: Vector, b: Vector) {
    // Depending on the magnitude of the angle use 0, 1 or 2 arc segments.
    if dot(a, b) == 1.0 {
        // the angle is 0 degrees, do nothing
    } else if dot(a, b) >= 0. {
        // the angle is less than 90 degrees
        arc_segment_tri(path, xc, yc, radius, a, b);
    } else {
        /* find a vector that bisects the angle between a and b */
        let mid_v = bisect(a, b);

        /* construct the arc using two curve segments */
        arc_segment_tri(path, xc, yc, radius, a, mid_v);
        arc_segment_tri(path, xc, yc, radius, mid_v, b);
    }
}

/* 
fn join_round(path: &mut PathBuilder, center: Point, a: Vector, b: Vector, radius: f32) {
    /*
    int ccw = dot (perp (b), a) >= 0; // XXX: is this always true?
    yes, otherwise we have an interior angle.
    assert (ccw);
    */
    arc(path, center.x, center.y, radius, a, b);
}*/

fn cap_line(dest: &mut PathBuilder, style: &StrokeStyle, pt: Point, normal: Vector) {
    let offset = style.width / 2.;
    match style.cap {
        LineCap::Butt => {
            if dest.aa {
                let half_width = offset;
                let end = pt;
                let v = Vector::new(normal.y, -normal.x);
                // end
                dest.ramp(
                    end.x - normal.x * (half_width - 0.5),
                    end.y - normal.y * (half_width - 0.5),
                    end.x + v.x - normal.x * (half_width - 0.5),
                    end.y + v.y - normal.y * (half_width - 0.5),
                    end.x + v.x + normal.x * (half_width - 0.5),
                    end.y + v.y + normal.y * (half_width - 0.5),
                    end.x + normal.x * (half_width - 0.5), 
                    end.y + normal.y * (half_width - 0.5),
                );
                dest.tri_ramp(
                    end.x + v.x - normal.x * (half_width - 0.5),
                end.y + v.y - normal.y * (half_width - 0.5),
                end.x - normal.x * (half_width + 0.5),
                end.y - normal.y * (half_width + 0.5),
                end.x - normal.x * (half_width - 0.5),
                end.y - normal.y * (half_width - 0.5));
                dest.tri_ramp(
                    end.x + v.x + normal.x * (half_width - 0.5),
                end.y + v.y + normal.y * (half_width - 0.5),
                end.x + normal.x * (half_width + 0.5),
                end.y + normal.y * (half_width + 0.5),
                end.x + normal.x * (half_width - 0.5),
                end.y + normal.y * (half_width - 0.5));
            }
         }
        LineCap::Round => {
            dest.arc_wedge(pt, offset, normal, flip(normal));
        }
        LineCap::Square => {
            // parallel vector
            let v = Vector::new(normal.y, -normal.x);
            let end = pt + v * offset;
            if dest.aa {
                let half_width = offset;
                let offset = offset - 0.5;
                dest.ramp(                        
                    end.x + normal.x * (half_width - 0.5), 
                    end.y + normal.y * (half_width - 0.5),
                    end.x + normal.x * (half_width + 0.5),
                    end.y + normal.y * (half_width + 0.5),
                    pt.x + normal.x * (half_width + 0.5),
                    pt.y + normal.y * (half_width + 0.5),
                    pt.x + normal.x * (half_width - 0.5),
                    pt.y + normal.y * (half_width - 0.5),
                );
                dest.quad(pt.x + normal.x * offset, pt.y + normal.y * offset,
                    end.x + normal.x * offset, end.y + normal.y * offset,
                    end.x + -normal.x * offset, end.y + -normal.y * offset,
                    pt.x - normal.x * offset, pt.y - normal.y * offset);

                dest.ramp(                        
                    pt.x - normal.x * (half_width - 0.5),
                    pt.y - normal.y * (half_width - 0.5),
                    pt.x - normal.x * (half_width + 0.5),
                    pt.y - normal.y * (half_width + 0.5),
                    end.x - normal.x * (half_width + 0.5),
                    end.y - normal.y * (half_width + 0.5),
                    end.x - normal.x * (half_width - 0.5), 
                    end.y - normal.y * (half_width - 0.5));

                // end
                dest.ramp(                        
                    end.x - normal.x * (half_width - 0.5),
                    end.y - normal.y * (half_width - 0.5),
                    end.x + v.x - normal.x * (half_width - 0.5),
                    end.y + v.y - normal.y * (half_width - 0.5),
                    end.x + v.x + normal.x * (half_width - 0.5),
                    end.y + v.y + normal.y * (half_width - 0.5),
                    end.x + normal.x * (half_width - 0.5), 
                    end.y + normal.y * (half_width - 0.5),
                );

                // corners
                dest.tri_ramp(
                    end.x + v.x - normal.x * (half_width - 0.5),
                end.y + v.y - normal.y * (half_width - 0.5),
                end.x - normal.x * (half_width + 0.5),
                end.y - normal.y * (half_width + 0.5),
                end.x - normal.x * (half_width - 0.5),
                end.y - normal.y * (half_width - 0.5));
                dest.tri_ramp(
                    end.x + v.x + normal.x * (half_width - 0.5),
                end.y + v.y + normal.y * (half_width - 0.5),
                end.x + normal.x * (half_width + 0.5),
                end.y + normal.y * (half_width + 0.5),
                end.x + normal.x * (half_width - 0.5),
                end.y + normal.y * (half_width - 0.5));
            } else {
                dest.quad(pt.x + normal.x * offset, pt.y + normal.y * offset,
                end.x + normal.x * offset, end.y + normal.y * offset,
                end.x + -normal.x * offset, end.y + -normal.y * offset,
                pt.x - normal.x * offset, pt.y - normal.y * offset);
            }
        }
    }
}

fn bevel(
    dest: &mut PathBuilder,
    style: &StrokeStyle,
    pt: Point,
    s1_normal: Vector,
    s2_normal: Vector,
) {
    let offset = style.width / 2.;
    if dest.aa {
        let width = 1.;
        let offset = offset - width / 2.;
        //XXX: we should be able to just bisect the two norms to get this
        let diff = match (s2_normal - s1_normal).try_normalize() {
            Some(diff) => diff,
            None => return,
        };
        let edge_normal = perp(diff);

        dest.tri(pt.x + s1_normal.x * offset, pt.y + s1_normal.y * offset,
            pt.x + s2_normal.x * offset, pt.y + s2_normal.y * offset,
            pt.x, pt.y);
        
        dest.tri_ramp(pt.x + s1_normal.x * (offset + width), pt.y + s1_normal.y * (offset + width),
                  pt.x + s1_normal.x * offset + edge_normal.x, pt.y + s1_normal.y * offset + edge_normal.y,
                  pt.x + s1_normal.x * offset, pt.y + s1_normal.y * offset);
        dest.ramp(
            pt.x + s2_normal.x * offset, pt.y + s2_normal.y * offset,
            pt.x + s2_normal.x * offset + edge_normal.x, pt.y + s2_normal.y * offset + edge_normal.y,
            pt.x + s1_normal.x * offset + edge_normal.x, pt.y + s1_normal.y * offset + edge_normal.y,
            pt.x + s1_normal.x * offset, pt.y + s1_normal.y * offset,
                );
        dest.tri_ramp(pt.x + s2_normal.x * (offset + width), pt.y + s2_normal.y * (offset + width),
                pt.x + s2_normal.x * offset + edge_normal.x, pt.y + s2_normal.y * offset + edge_normal.y,
                pt.x + s2_normal.x * offset, pt.y + s2_normal.y * offset);
    } else {
        dest.tri(pt.x + s1_normal.x * offset, pt.y + s1_normal.y * offset,
            pt.x + s2_normal.x * offset, pt.y + s2_normal.y * offset,
            pt.x, pt.y);
    }
}

/* given a normal rotate the vector 90 degrees to the right clockwise
 * This function has a period of 4. e.g. swap(swap(swap(swap(x) == x */
fn swap(a: Vector) -> Vector {
    /* one of these needs to be negative. We choose a.x so that we rotate to the right instead of negating */
    Vector::new(a.y, -a.x)
}

fn unperp(a: Vector) -> Vector {
    swap(a)
}

/* rotate a vector 90 degrees to the left */
fn perp(v: Vector) -> Vector {
    Vector::new(-v.y, v.x)
}

fn dot(a: Vector, b: Vector) -> f32 {
    a.x * b.x + a.y * b.y
}

/* Finds the intersection of two lines each defined by a point and a normal.
From "Example 2: Find the intersection of two lines" of
"The Pleasures of "Perp Dot" Products"
F. S. Hill, Jr. */
fn line_intersection(a: Point, a_perp: Vector, b: Point, b_perp: Vector) -> Option<Point> {
    let a_parallel = unperp(a_perp);
    let c = b - a;
    let denom = dot(b_perp, a_parallel);
    if denom == 0.0 {
        return None;
    }

    let t = dot(b_perp, c) / denom;

    let intersection = Point::new(a.x + t * (a_parallel.x), a.y + t * (a_parallel.y));

    Some(intersection)
}

fn is_interior_angle(a: Vector, b: Vector) -> bool {
    /* angles of 180 and 0 degress will evaluate to 0, however
     * we to treat 180 as an interior angle and 180 as an exterior angle */
    dot(perp(a), b) > 0. || a == b /* 0 degrees is interior */
}

fn join_line(
    dest: &mut PathBuilder,
    style: &StrokeStyle,
    pt: Point,
    mut s1_normal: Vector,
    mut s2_normal: Vector,
) {
    if is_interior_angle(s1_normal, s2_normal) {
        s2_normal = flip(s2_normal);
        s1_normal = flip(s1_normal);
        std::mem::swap(&mut s1_normal, &mut s2_normal);
    }

    // XXX: joining uses `pt` which can cause seams because it lies halfway on a line and the
    // rasterizer may not find exactly the same spot
    let mut offset = style.width / 2.;

    match style.join {
        LineJoin::Round => {
            dest.arc_wedge(pt, offset, s1_normal, s2_normal);
        }
        LineJoin::Miter => {
            if dest.aa {
                offset -= 0.5;
            }
            let in_dot_out = -s1_normal.x * s2_normal.x + -s1_normal.y * s2_normal.y;
            if 2. <= style.miter_limit * style.miter_limit * (1. - in_dot_out) {
                let start = pt + s1_normal * offset;
                let end = pt + s2_normal * offset;
                if let Some(intersection) = line_intersection(start, s1_normal, end, s2_normal) {
                    // We won't have an intersection if the segments are parallel
                    if dest.aa {
                        let ramp_start = pt + s1_normal * (offset + 1.);
                        let ramp_end = pt + s2_normal * (offset + 1.);
                        let mid = bisect(s1_normal, s2_normal);
                        let ramp_intersection = intersection + mid;

                        let ramp_s1 = line_intersection(ramp_start, s1_normal, ramp_intersection, flip(mid));
                        let ramp_s2 = line_intersection(ramp_end, s2_normal, ramp_intersection, flip(mid));

                        if let Some(ramp_s1) = ramp_s1 {
                            dest.ramp(intersection.x, intersection.y,
                                ramp_s1.x, ramp_s1.y,
                                ramp_start.x, ramp_start.y,
                                pt.x + s1_normal.x * offset, pt.y + s1_normal.y * offset,
                            );
                        }
                        if let Some(ramp_s2) = ramp_s2 {
                            dest.ramp(pt.x + s2_normal.x * offset, pt.y + s2_normal.y * offset,
                                ramp_end.x, ramp_end.y,
                                ramp_s2.x, ramp_s2.y,
                                intersection.x, intersection.y);
                            if let Some(ramp_s1) = ramp_s1 {
                                dest.tri_ramp(ramp_s1.x, ramp_s1.y, ramp_s2.x, ramp_s2.y, intersection.x, intersection.y);
                            }
                        }

                        // we'll want to intersect the ramps and put a flat cap on the end
                        dest.quad(pt.x + s1_normal.x * offset, pt.y + s1_normal.y * offset,
                            intersection.x, intersection.y,
                            pt.x + s2_normal.x * offset, pt.y + s2_normal.y * offset,
                            pt.x, pt.y);
                    } else {
                        dest.quad(pt.x + s1_normal.x * offset, pt.y + s1_normal.y * offset,
                            intersection.x, intersection.y,
                            pt.x + s2_normal.x * offset, pt.y + s2_normal.y * offset,
                            pt.x, pt.y);
                    }
                }
            } else {
                bevel(dest, style, pt, s1_normal, s2_normal);
            }
        }
        LineJoin::Bevel => {
            bevel(dest, style, pt, s1_normal, s2_normal);
        }
    }
}

pub struct Stroker<'z> {
    stroked_path: PathBuilder<'z>,
    cur_pt: Option<Point>,
    last_normal: Vector,
    half_width: f32,
    start_point: Option<(Point, Vector)>,
    style: StrokeStyle,
    closed_subpath: bool
}

impl<'z> Stroker<'z> {
    pub fn new(style: &StrokeStyle) -> Self {
        let mut style = style.clone();
        let mut coverage = 1.;
        if style.width < 1. {
            coverage = style.width;
            style.width = 1.;
        }
        Stroker {
            stroked_path: PathBuilder::new(coverage),
            cur_pt: None,
            last_normal: Vector::zero(),
            half_width: style.width / 2.,
            start_point: None,
            style,
            closed_subpath: false,
        }
    }

    pub fn set_output_buffer(&mut self, output_buffer: &'z mut [Vertex]) {
        self.stroked_path.set_output_buffer(output_buffer);
    }

    pub fn line_to_capped(&mut self, pt: Point) {
        if let Some(cur_pt) = self.cur_pt {
            let normal = compute_normal(cur_pt, pt).unwrap_or(self.last_normal);
            self.line_to(if self.stroked_path.aa && self.style.cap == LineCap::Butt { pt - flip(normal) * 0.5} else { pt });
            if let (Some(cur_pt), Some((_point, _normal))) = (self.cur_pt, self.start_point) {
                // cap end
                cap_line(&mut self.stroked_path, &self.style, cur_pt, self.last_normal);
            }
        }
        self.start_point = None;
    }

    pub fn move_to(&mut self, pt: Point, closed_subpath: bool) {
        self.start_point = None;
        self.cur_pt = Some(pt);
        self.closed_subpath = closed_subpath;
    }

    pub fn line_to(&mut self, pt: Point) {
        let cur_pt = self.cur_pt;
        let stroked_path = &mut self.stroked_path;
        let half_width = self.half_width;

        if cur_pt.is_none() {
            self.start_point = None;
        } else if let Some(cur_pt) = cur_pt {
            if let Some(normal) = compute_normal(cur_pt, pt) {
                if self.start_point.is_none() {
                    if !self.closed_subpath {
                        // cap beginning
                        cap_line(stroked_path, &self.style, cur_pt, flip(normal));
                        if stroked_path.aa && self.style.cap == LineCap::Butt {
                            
                        }
                    }
                    self.start_point = Some((cur_pt, normal));
                } else {
                    join_line(stroked_path, &self.style, cur_pt, self.last_normal, normal);
                }
                if stroked_path.aa { 
                    stroked_path.ramp(                        
                        pt.x + normal.x * (half_width - 0.5), 
                        pt.y + normal.y * (half_width - 0.5),
                        pt.x + normal.x * (half_width + 0.5),
                        pt.y + normal.y * (half_width + 0.5),
                        cur_pt.x + normal.x * (half_width + 0.5),
                        cur_pt.y + normal.y * (half_width + 0.5),
                        cur_pt.x + normal.x * (half_width - 0.5),
                        cur_pt.y + normal.y * (half_width - 0.5),
                    );
                    stroked_path.quad(
                        cur_pt.x + normal.x * (half_width - 0.5),
                        cur_pt.y + normal.y * (half_width - 0.5),
                        pt.x + normal.x * (half_width - 0.5), pt.y + normal.y * (half_width - 0.5),
                        pt.x + -normal.x * (half_width - 0.5), pt.y + -normal.y * (half_width - 0.5),
                        cur_pt.x - normal.x * (half_width - 0.5),
                        cur_pt.y - normal.y * (half_width - 0.5),
                    );
                    stroked_path.ramp(                        
                        cur_pt.x - normal.x * (half_width - 0.5),
                        cur_pt.y - normal.y * (half_width - 0.5),
                        cur_pt.x - normal.x * (half_width + 0.5),
                        cur_pt.y - normal.y * (half_width + 0.5),
                        pt.x - normal.x * (half_width + 0.5),
                        pt.y - normal.y * (half_width + 0.5),
                        pt.x - normal.x * (half_width - 0.5), 
                        pt.y - normal.y * (half_width - 0.5),
                    );
                } else {
                    stroked_path.quad(
                        cur_pt.x + normal.x * half_width,
                        cur_pt.y + normal.y * half_width,
                        pt.x + normal.x * half_width, pt.y + normal.y * half_width,
                        pt.x + -normal.x * half_width, pt.y + -normal.y * half_width,
                        cur_pt.x - normal.x * half_width,
                        cur_pt.y - normal.y * half_width,
                    );
                }

                self.last_normal = normal;

            }
        }
        self.cur_pt = Some(pt);
    }

    pub fn curve_to(&mut self, cx1: Point, cx2: Point, pt: Point) {
        self.curve_to_internal(cx1, cx2, pt, false);
    }

    pub fn curve_to_capped(&mut self, cx1: Point, cx2: Point, pt: Point) {
        self.curve_to_internal(cx1, cx2, pt, true);
    }

    pub fn curve_to_internal(&mut self, cx1: Point, cx2: Point, pt: Point, end: bool) {
        struct Target<'a, 'b> { stroker: &'a mut Stroker<'b>, end: bool }
        impl<'a, 'b> CFlatteningSink for Target<'a, 'b> {
            fn AcceptPointAndTangent(&mut self, _: &GpPointR, _: &GpPointR, _: bool ) -> HRESULT {
                panic!()
            }

            fn AcceptPoint(&mut self,
                pt: &GpPointR,
                    // The point
                _t: f64,
                    // Parameter we're at
                _aborted: &mut bool,
                last_point: bool) -> HRESULT {
                if last_point && self.end  {
                    self.stroker.line_to_capped(Point::new(pt.x as f32, pt.y as f32));
                } else {
                    self.stroker.line_to(Point::new(pt.x as f32, pt.y as f32));
                }
                return S_OK;
            }
        }
        let cur_pt = self.cur_pt.unwrap_or(cx1);
        let bezier = CBezier::new([GpPointR { x: cur_pt.x as f64, y: cur_pt.y as f64,  },
            GpPointR { x: cx1.x as f64, y: cx1.y as f64, },
            GpPointR { x: cx2.x as f64, y: cx2.y as f64, },
            GpPointR { x: pt.x as f64, y: pt.y as f64, }]);
        let mut t = Target{ stroker: self, end };
        let mut f = CBezierFlattener::new(&bezier, &mut t, 0.25);
        f.Flatten(false);
    }

    pub fn close(&mut self) {
        let stroked_path = &mut self.stroked_path;
        let half_width = self.half_width;
        if let (Some(cur_pt), Some((end_point, start_normal))) = (self.cur_pt, self.start_point) {
            if let Some(normal) = compute_normal(cur_pt, end_point) {
                join_line(stroked_path, &self.style, cur_pt, self.last_normal, normal);
                if stroked_path.aa { 
                    stroked_path.ramp(                        
                        end_point.x + normal.x * (half_width - 0.5), 
                        end_point.y + normal.y * (half_width - 0.5),
                        end_point.x + normal.x * (half_width + 0.5),
                        end_point.y + normal.y * (half_width + 0.5),
                        cur_pt.x + normal.x * (half_width + 0.5),
                        cur_pt.y + normal.y * (half_width + 0.5),
                        cur_pt.x + normal.x * (half_width - 0.5),
                        cur_pt.y + normal.y * (half_width - 0.5),
                    );
                    stroked_path.quad(
                        cur_pt.x + normal.x * (half_width - 0.5),
                        cur_pt.y + normal.y * (half_width - 0.5),
                        end_point.x + normal.x * (half_width - 0.5), end_point.y + normal.y * (half_width - 0.5),
                        end_point.x + -normal.x * (half_width - 0.5), end_point.y + -normal.y * (half_width - 0.5),
                        cur_pt.x - normal.x * (half_width - 0.5),
                        cur_pt.y - normal.y * (half_width - 0.5),
                    );
                    stroked_path.ramp(                        
                        cur_pt.x - normal.x * (half_width - 0.5),
                        cur_pt.y - normal.y * (half_width - 0.5),
                        cur_pt.x - normal.x * (half_width + 0.5),
                        cur_pt.y - normal.y * (half_width + 0.5),
                        end_point.x - normal.x * (half_width + 0.5),
                        end_point.y - normal.y * (half_width + 0.5),
                        end_point.x - normal.x * (half_width - 0.5), 
                        end_point.y - normal.y * (half_width - 0.5),
                    );
                } else {
                    stroked_path.quad(
                        cur_pt.x + normal.x * half_width,
                        cur_pt.y + normal.y * half_width,
                        end_point.x + normal.x * half_width, end_point.y + normal.y * half_width,
                        end_point.x + -normal.x * half_width, end_point.y + -normal.y * half_width,
                        cur_pt.x - normal.x * half_width,
                        cur_pt.y - normal.y * half_width,
                    );
                }
                join_line(stroked_path, &self.style, end_point, normal, start_normal);
            } else {
                join_line(stroked_path, &self.style, end_point, self.last_normal, start_normal);
            }
        }
        self.cur_pt = self.start_point.map(|x| x.0);
        self.start_point = None;
    }

    pub fn get_stroked_path(&mut self) -> PathBuilder<'z> {
        let mut stroked_path = std::mem::replace(&mut self.stroked_path, PathBuilder::new(1.));

        if let (Some(cur_pt), Some((point, normal))) = (self.cur_pt, self.start_point) {
            // cap end
            cap_line(&mut stroked_path, &self.style, cur_pt, self.last_normal);
            // cap beginning
            cap_line(&mut stroked_path, &self.style, point, flip(normal));
        }

        stroked_path
    }

    pub fn finish(&mut self) -> Vec<Vertex> {
        self.get_stroked_path().finish()
    }
}

#[test]
fn simple() {
    let mut stroker = Stroker::new(&StrokeStyle{
        cap: LineCap::Round, 
        join: LineJoin::Bevel, 
        width: 20.,
        ..Default::default()});
    stroker.move_to(Point::new(20., 20.), false);
    stroker.line_to(Point::new(100., 100.));
    stroker.line_to_capped(Point::new(110., 20.));

    stroker.move_to(Point::new(120., 20.), true);
    stroker.line_to(Point::new(120., 50.));
    stroker.line_to(Point::new(140., 50.));
    stroker.close();

    let stroked = stroker.finish();
    assert_eq!(stroked.len(), 330);
}

#[test]
fn curve() {
    let mut stroker = Stroker::new(&StrokeStyle{
        cap: LineCap::Round,
        join: LineJoin::Bevel,
        width: 20.,
        ..Default::default()});
        stroker.move_to(Point::new(20., 160.), true);
        stroker.curve_to(Point::new(100., 160.), Point::new(100., 180.), Point::new(20., 180.));
        stroker.close();
    let stroked = stroker.finish();
    assert_eq!(stroked.len(), 1089);
}

#[test]
fn width_one_radius_arc() {
    // previously this caused us to try to flatten an arc with radius 0
    let mut stroker = Stroker::new(&StrokeStyle{
        cap: LineCap::Round,
        join: LineJoin::Round,
        width: 1.,
        ..Default::default()});
    stroker.move_to(Point::new(20., 20.), false);
    stroker.line_to(Point::new(30., 160.));
    stroker.line_to_capped(Point::new(40., 20.));
    stroker.finish();
}

#[test]
fn round_join_less_than_90deg() {
    let mut stroker = Stroker::new(&StrokeStyle{
        cap: LineCap::Round,
        join: LineJoin::Round,
        width: 1.,
        ..Default::default()});
    stroker.move_to(Point::new(20., 20.), false);
    stroker.line_to(Point::new(20., 40.));
    stroker.line_to_capped(Point::new(30., 50.));
    assert_eq!(stroker.finish().len(), 81);
}

#[test]
fn parallel_line_join() {
    // ensure line joins of almost parallel lines don't cause math to fail
    for join in [LineJoin::Bevel, LineJoin::Round, LineJoin::Miter] {
        let mut stroker = Stroker::new(&StrokeStyle{
            cap: LineCap::Butt,
            join,
            width: 1.0,
            ..Default::default()});
        stroker.move_to(Point::new(19.812500, 71.625000), true);
        stroker.line_to(Point::new(19.250000, 72.000000));
        stroker.line_to(Point::new(19.062500, 72.125000));
        stroker.close();
        stroker.finish();
    }
}

