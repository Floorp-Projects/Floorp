use binary_space_partition::{BspNode, Plane as BspPlane, PlaneCut};
use euclid::{TypedPoint3D, TypedVector3D};
use euclid::approxeq::ApproxEq;
use num_traits::{Float, One, Zero};
use std::{fmt, ops};
use {Intersection, Plane, Polygon, Splitter};


impl<T, U> BspPlane for Polygon<T, U> where
    T: Copy + fmt::Debug + ApproxEq<T> +
        ops::Sub<T, Output=T> + ops::Add<T, Output=T> +
        ops::Mul<T, Output=T> + ops::Div<T, Output=T> +
        Zero + One + Float,
    U: fmt::Debug,
{
    fn cut(&self, mut plane: Self) -> PlaneCut<Self> {
        debug!("\tCutting anchor {}", plane.anchor);
        let dist = self.plane.signed_distance_sum_to(&plane);

        match self.intersect(&plane) {
            Intersection::Coplanar if dist.approx_eq(&T::zero()) => {
                debug!("\t\tCoplanar and matching");
                PlaneCut::Sibling(plane)
            }
            Intersection::Coplanar | Intersection::Outside => {
                debug!("\t\tCoplanar at {:?}", dist);
                if dist > T::zero() {
                    PlaneCut::Cut {
                        front: vec![plane],
                        back: vec![],
                    }
                } else {
                    PlaneCut::Cut {
                        front: vec![],
                        back: vec![plane],
                    }
                }
            }
            Intersection::Inside(line) => {
                let (res_add1, res_add2) = plane.split(&line);
                let mut front = Vec::new();
                let mut back = Vec::new();

                for sub in Some(plane).into_iter().chain(res_add1).chain(res_add2) {
                    if self.plane.signed_distance_sum_to(&sub) > T::zero() {
                        front.push(sub)
                    } else {
                        back.push(sub)
                    }
                }
                debug!("\t\tCut across {:?} by {} in front and {} in back",
                    line, front.len(), back.len());

                PlaneCut::Cut {
                    front: front,
                    back: back,
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
