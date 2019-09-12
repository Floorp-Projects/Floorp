use {Intersection, Plane, Polygon, Splitter};
use is_zero;

use binary_space_partition::{BspNode, Plane as BspPlane, PlaneCut};
use euclid::{Point3D, Vector3D};
use euclid::approxeq::ApproxEq;
use num_traits::{Float, One, Zero};

use std::{fmt, iter, ops};


impl<T, U, A> BspPlane for Polygon<T, U, A> where
    T: Copy + fmt::Debug + ApproxEq<T> +
        ops::Sub<T, Output=T> + ops::Add<T, Output=T> +
        ops::Mul<T, Output=T> + ops::Div<T, Output=T> +
        Zero + Float,
    U: fmt::Debug,
    A: Copy + fmt::Debug,
{
    fn cut(&self, mut poly: Self) -> PlaneCut<Self> {
        debug!("\tCutting anchor {:?} by {:?}", poly.anchor, self.anchor);
        trace!("\t\tbase {:?}", self.plane);

        //Note: we treat `self` as a plane, and `poly` as a concrete polygon here
        let (intersection, dist) = match self.plane.intersect(&poly.plane) {
            None => {
                let ndot = self.plane.normal.dot(poly.plane.normal);
                debug!("\t\tNormals are aligned with {:?}", ndot);
                let dist = self.plane.offset - ndot * poly.plane.offset;
                (Intersection::Coplanar, dist)
            }
            Some(_) if self.plane.are_outside(&poly.points) => {
                //Note: we can't start with `are_outside` because it's subject to FP precision
                let dist = self.plane.signed_distance_sum_to(&poly);
                (Intersection::Outside, dist)
            }
            Some(line) => {
                //Note: distance isn't relevant here
                (Intersection::Inside(line), T::zero())
            }
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
                let (res_add1, res_add2) = poly.split_with_normal(&line, &self.plane.normal);
                let mut front = Vec::new();
                let mut back = Vec::new();

                for sub in iter::once(poly)
                    .chain(res_add1)
                    .chain(res_add2)
                    .filter(|p| !p.is_empty())
                {
                    let dist = self.plane.signed_distance_sum_to(&sub);
                    if dist > T::zero() {
                        trace!("\t\t\tdist {:?} -> front: {:?}", dist, sub);
                        front.push(sub)
                    } else {
                        trace!("\t\t\tdist {:?} -> back: {:?}", dist, sub);
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
pub struct BspSplitter<T, U, A> {
    tree: BspNode<Polygon<T, U, A>>,
    result: Vec<Polygon<T, U, A>>,
}

impl<T, U, A> BspSplitter<T, U, A> {
    /// Create a new BSP splitter.
    pub fn new() -> Self {
        BspSplitter {
            tree: BspNode::new(),
            result: Vec::new(),
        }
    }
}

impl<T, U, A> Splitter<T, U, A> for BspSplitter<T, U, A> where
    T: Copy + fmt::Debug + ApproxEq<T> +
        ops::Sub<T, Output=T> + ops::Add<T, Output=T> +
        ops::Mul<T, Output=T> + ops::Div<T, Output=T> +
        Zero + One + Float,
    U: fmt::Debug,
    A: Copy + fmt::Debug + Default,
{
    fn reset(&mut self) {
        self.tree = BspNode::new();
    }

    fn add(&mut self, poly: Polygon<T, U, A>) {
        self.tree.insert(poly);
    }

    fn sort(&mut self, view: Vector3D<T, U>) -> &[Polygon<T, U, A>] {
        //debug!("\t\ttree before sorting {:?}", self.tree);
        let poly = Polygon {
            points: [Point3D::origin(); 4],
            plane: Plane {
                normal: -view, //Note: BSP `order()` is back to front
                offset: T::zero(),
            },
            anchor: A::default(),
        };
        self.result.clear();
        self.tree.order(&poly, &mut self.result);
        &self.result
    }
}
