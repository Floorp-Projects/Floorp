use {Intersection, Plane, Polygon};

use euclid::{Trig, TypedRect, TypedScale, TypedTransform3D, TypedVector3D};
use euclid::approxeq::ApproxEq;
use num_traits::{Float, One, Zero};

use std::{fmt, mem, ops};


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

    /// Add a set of planes that define the frustum for a given transformation.
    pub fn add_frustum<V>(
        &mut self,
        t: &TypedTransform3D<T, U, V>,
        bounds: Option<TypedRect<T, V>>,
    ) {
        //Note: this is not the near plane, but the positive hemisphere
        // in homogeneous space.
        let mw = TypedVector3D::new(t.m14, t.m24, t.m34);
        self.clips.extend(Plane::from_unnormalized(mw, t.m44));

        if let Some(bounds) = bounds {
            let mx = TypedVector3D::new(t.m11, t.m21, t.m31);
            let left = bounds.origin.x;
            self.clips.extend(Plane::from_unnormalized(
                mx - mw * TypedScale::new(left),
                t.m41 - t.m44 * left,
            ));
            let right = bounds.origin.x + bounds.size.width;
            self.clips.extend(Plane::from_unnormalized(
                mw * TypedScale::new(right) - mx,
                t.m44 * right - t.m41,
            ));

            let my = TypedVector3D::new(t.m12, t.m22, t.m32);
            let top = bounds.origin.y;
            self.clips.extend(Plane::from_unnormalized(
                my - mw * TypedScale::new(top),
                t.m42 - t.m44 * top,
            ));
            let bottom = bounds.origin.y + bounds.size.height;
            self.clips.extend(Plane::from_unnormalized(
                mw * TypedScale::new(bottom) - my,
                t.m44 * bottom - t.m42,
            ));
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
                            .chain(res2)
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

    /// Clip the primitive with the frustum of the specified transformation,
    /// returning a sequence of polygons in the transformed space.
    pub fn clip_transformed<'a, V>(
        &'a mut self,
        polygon: Polygon<T, U>,
        transform: &'a TypedTransform3D<T, U, V>,
        bounds: Option<TypedRect<T, V>>,
    ) -> impl 'a + Iterator<Item = Polygon<T, V>>
    where
        T: Trig,
        V: 'a + fmt::Debug,
    {
        let num_planes = if bounds.is_some() {5} else {1};
        self.add_frustum(transform, bounds);
        self.clip(polygon);
        // remove the frustum planes
        for _ in 0 .. num_planes {
            self.clips.pop();
        }
        self.results
            .drain(..)
            .flat_map(move |poly| poly.transform(transform))
    }
}
