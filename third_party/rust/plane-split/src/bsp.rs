use {Intersection, Plane, Polygon, Splitter};
use is_zero;

use binary_space_partition::{BspNode, Plane as BspPlane, PlaneCut};
use euclid::{TypedPoint3D, TypedVector3D};
use euclid::approxeq::ApproxEq;
use num_traits::{Float, One, Zero};

use std::{fmt, iter, ops};


impl<T, U> BspPlane for Polygon<T, U> where
    T: Copy + fmt::Debug + ApproxEq<T> +
        ops::Sub<T, Output=T> + ops::Add<T, Output=T> +
        ops::Mul<T, Output=T> + ops::Div<T, Output=T> +
        Zero + One + Float,
    U: fmt::Debug,
{
    fn cut(&self, mut poly: Self) -> PlaneCut<Self> {
        debug!("\tCutting anchor {} by {}", poly.anchor, self.anchor);
        trace!("\t\tbase {:?}", self.plane);

        let (intersection, dist) = if self.plane.normal
            .dot(poly.plane.normal)
            .approx_eq(&T::one())
        {
            debug!("\t\tNormals roughly match");
            (Intersection::Coplanar, self.plane.offset - poly.plane.offset)
        } else {
            let is = self.intersect(&poly);
            let dist = self.plane.signed_distance_sum_to(&poly);
            (is, dist)
        };

        match intersection {
            //Note: we deliberately make the comparison wider than just with T::epsilon().
            // This is done to avoid mistakenly ordering items that should be on the same
            // plane but end up slightly different due to the floating point precision.
            Intersection::Coplanar if is_zero(dist) => {
                debug!("\t\tCoplanar at {:?}", dist);
                PlaneCut::Sibling(poly)
            }
            Intersection::Coplanar | Intersection::Outside => {
                debug!("\t\tOutside at {:?}", dist);
                if dist > T::zero() {
                    PlaneCut::Cut {
                        front: vec![poly],
                        back: vec![],
                    }
                } else {
                    PlaneCut::Cut {
                        front: vec![],
                        back: vec![poly],
                    }
                }
            }
            Intersection::Inside(line) => {
                debug!("\t\tCut across {:?}", line);
                let (res_add1, res_add2) = poly.split(&line);
                let mut front = Vec::new();
                let mut back = Vec::new();

                for sub in iter::once(poly)
                    .chain(res_add1)
                    .chain(res_add2)
                    .filter(|p| !p.is_empty())
                {
                    if self.plane.signed_distance_sum_to(&sub) > T::zero() {
                        trace!("\t\t\tfront: {:?}", sub);
                        front.push(sub)
                    } else {
                        trace!("\t\t\tback: {:?}", sub);
                        back.push(sub)
                    }
                }

                PlaneCut::Cut {
                    front,
                    back,
                }
            },
        }
    }

    fn is_aligned(&self, other: &Self) -> bool {
        self.plane.normal.dot(other.plane.normal) > T::zero()
    }
}


/// Binary Space Partitioning splitter, uses a BSP tree.
pub struct BspSplitter<T, U> {
    tree: BspNode<Polygon<T, U>>,
    result: Vec<Polygon<T, U>>,
}

impl<T, U> BspSplitter<T, U> {
    /// Create a new BSP splitter.
    pub fn new() -> Self {
        BspSplitter {
            tree: BspNode::new(),
            result: Vec::new(),
        }
    }
}

impl<T, U> Splitter<T, U> for BspSplitter<T, U> where
    T: Copy + fmt::Debug + ApproxEq<T> +
        ops::Sub<T, Output=T> + ops::Add<T, Output=T> +
        ops::Mul<T, Output=T> + ops::Div<T, Output=T> +
        Zero + One + Float,
    U: fmt::Debug,
{
    fn reset(&mut self) {
        self.tree = BspNode::new();
    }

    fn add(&mut self, poly: Polygon<T, U>) {
        self.tree.insert(poly);
    }

    fn sort(&mut self, view: TypedVector3D<T, U>) -> &[Polygon<T, U>] {
        //debug!("\t\ttree before sorting {:?}", self.tree);
        let poly = Polygon {
            points: [TypedPoint3D::origin(); 4],
            plane: Plane {
                normal: -view, //Note: BSP `order()` is back to front
                offset: T::zero(),
            },
            anchor: 0,
        };
        self.result.clear();
        self.tree.order(&poly, &mut self.result);
        &self.result
    }
}
