use {Intersection, NegativeHemisphereError, Plane, Polygon};

use euclid::{Trig, Rect, Scale, Transform3D, Vector3D};
use euclid::approxeq::ApproxEq;
use num_traits::{Float, One, Zero};

use std::{fmt, iter, mem, ops};


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

    /// Reset the clipper internals, but preserve the allocation.
    pub fn reset(&mut self) {
        self.clips.clear();
    }

    /// Extract the clipping planes that define the frustum for a given transformation.
    pub fn frustum_planes<V>(
        t: &Transform3D<T, U, V>,
        bounds: Option<Rect<T, V>>,
    ) -> Result<impl Iterator<Item = Plane<T, U>>, NegativeHemisphereError> {
        let mw = Vector3D::new(t.m14, t.m24, t.m34);
        let plane_positive = Plane::from_unnormalized(mw, t.m44)?;

        let bounds_iter_maybe = match bounds {
            Some(bounds) => {
                let mx = Vector3D::new(t.m11, t.m21, t.m31);
                let left = bounds.origin.x;
                let plane_left = Plane::from_unnormalized(
                    mx - mw * Scale::new(left),
                    t.m41 - t.m44 * left,
                )?;
                let right = bounds.origin.x + bounds.size.width;
                let plane_right = Plane::from_unnormalized(
                    mw * Scale::new(right) - mx,
                    t.m44 * right - t.m41,
                )?;

                let my = Vector3D::new(t.m12, t.m22, t.m32);
                let top = bounds.origin.y;
                let plane_top = Plane::from_unnormalized(
                    my - mw * Scale::new(top),
                    t.m42 - t.m44 * top,
                )?;
                let bottom = bounds.origin.y + bounds.size.height;
                let plane_bottom = Plane::from_unnormalized(
                    mw * Scale::new(bottom) - my,
                    t.m44 * bottom - t.m42,
                )?;

                Some(plane_left
                    .into_iter()
                    .chain(plane_right)
                    .chain(plane_top)
                    .chain(plane_bottom)
                )
            }
            None => None,
        };

        Ok(bounds_iter_maybe
            .into_iter()
            .flat_map(|pi| pi)
            .chain(plane_positive)
        )
    }

    /// Add a clipping plane to the list. The plane will clip everything behind it,
    /// where the direction is set by the plane normal.
    pub fn add(&mut self, plane: Plane<T, U>) {
        self.clips.push(plane);
    }

    /// Clip specified polygon by the contained planes, return the fragmented polygons.
    pub fn clip(&mut self, polygon: Polygon<T, U>) -> &[Polygon<T, U>] {
        debug!("\tClipping {:?}", polygon);
        self.results.clear();
        self.results.push(polygon);

        for clip in &self.clips {
            self.temp.clear();
            mem::swap(&mut self.results, &mut self.temp);

            for mut poly in self.temp.drain(..) {
                let dist = match poly.intersect_plane(clip) {
                    Intersection::Inside(line) => {
                        let (res1, res2) = poly.split_with_normal(&line, &clip.normal);
                        self.results.extend(
                            iter::once(poly)
                                .chain(res1)
                                .chain(res2)
                                .filter(|p| clip.signed_distance_sum_to(p) > T::zero())
                        );
                        continue
                    }
                    Intersection::Coplanar => {
                        let ndot = poly.plane.normal.dot(clip.normal);
                        clip.offset - ndot * poly.plane.offset
                    }
                    Intersection::Outside => {
                        clip.signed_distance_sum_to(&poly)
                    }
                };

                if dist > T::zero() {
                    self.results.push(poly);
                }
            }
        }

        &self.results
    }

    /// Clip the primitive with the frustum of the specified transformation,
    /// returning a sequence of polygons in the transformed space.
    /// Returns None if the transformation can't be frustum clipped.
    pub fn clip_transformed<'a, V>(
        &'a mut self,
        polygon: Polygon<T, U>,
        transform: &'a Transform3D<T, U, V>,
        bounds: Option<Rect<T, V>>,
    ) -> Result<impl 'a + Iterator<Item = Polygon<T, V>>, NegativeHemisphereError>
    where
        T: Trig,
        V: 'a + fmt::Debug,
    {
        let planes = Self::frustum_planes(transform, bounds)?;

        let old_count = self.clips.len();
        self.clips.extend(planes);
        self.clip(polygon);
        // remove the frustum planes
        while self.clips.len() > old_count {
            self.clips.pop();
        }

        let polys = self.results
            .drain(..)
            .flat_map(move |poly| poly.transform(transform));
        Ok(polys)
    }
}
