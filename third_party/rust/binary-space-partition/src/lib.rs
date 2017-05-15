/*!
Binary Space Partitioning (BSP)

Provides an abstract `BspNode` structure, which can be seen as a tree.
Useful for quickly ordering polygons along a particular view vector.
Is not tied to a particular math library.
*/
#![warn(missing_docs)]

use std::cmp;


/// The result of one plane being cut by another one.
/// The "cut" here is an attempt to classify a plane as being
/// in front or in the back of another one.
#[derive(Debug)]
pub enum PlaneCut<T> {
    /// The planes are one the same geometrical plane.
    Sibling(T),
    /// Planes are different, thus we can either determine that
    /// our plane is completely in front/back of another one,
    /// or split it into these sub-groups.
    Cut {
        /// Sub-planes in front of the base plane.
        front: Vec<T>,
        /// Sub-planes in the back of the base plane.
        back: Vec<T>,
    },
}

/// A plane abstracted to the matter of partitioning.
pub trait Plane: Sized + Clone {
    /// Try to cut a different plane by this one.
    fn cut(&self, Self) -> PlaneCut<Self>;
    /// Check if a different plane is aligned in the same direction
    /// as this one.
    fn is_aligned(&self, &Self) -> bool;
}

/// Add a list of planes to a particular front/end branch of some root node.
fn add_side<T: Plane>(side: &mut Option<Box<BspNode<T>>>, mut planes: Vec<T>) {
    if planes.len() != 0 {
        if side.is_none() {
            *side = Some(Box::new(BspNode::new()));
        }
        let mut node = side.as_mut().unwrap();
        for p in planes.drain(..) {
            node.insert(p)
        }
    }
}


/// A node in the `BspTree`, which can be considered a tree itself.
#[derive(Clone, Debug)]
pub struct BspNode<T> {
    values: Vec<T>,
    front: Option<Box<BspNode<T>>>,
    back: Option<Box<BspNode<T>>>,
}

impl<T> BspNode<T> {
    /// Create a new node.
    pub fn new() -> Self {
        BspNode {
            values: Vec::new(),
            front: None,
            back: None,
        }
    }

    /// Check if this node is a leaf of the tree.
    pub fn is_leaf(&self) -> bool {
        self.front.is_none() && self.back.is_none()
    }

    /// Get the tree depth starting with this node.
    pub fn get_depth(&self) -> usize {
        if self.values.is_empty() {
            return 0
        }
        let df = match self.front {
            Some(ref node) => node.get_depth(),
            None => 0,
        };
        let db = match self.back {
            Some(ref node) => node.get_depth(),
            None => 0,
        };
        1 + cmp::max(df, db)
    }
}

impl<T: Plane> BspNode<T> {
    /// Insert a value into the sub-tree starting with this node.
    /// This operation may spawn additional leafs/branches of the tree.
    pub fn insert(&mut self, value: T) {
        if self.values.is_empty() {
            self.values.push(value);
            return
        }
        match self.values[0].cut(value) {
            PlaneCut::Sibling(value) => self.values.push(value),
            PlaneCut::Cut { front, back } => {
                add_side(&mut self.front, front);
                add_side(&mut self.back, back);
            }
        }
    }

    /// Build the draw order of this sub-tree into an `out` vector,
    /// so that the contained planes are sorted back to front according
    /// to the view vector defines as the `base` plane front direction.
    pub fn order(&self, base: &T, out: &mut Vec<T>) {
        let (former, latter) = match self.values.first() {
            None => return,
            Some(ref first) if base.is_aligned(first) => (&self.front, &self.back),
            Some(_) => (&self.back, &self.front),
        };

        if let Some(ref node) = *former {
            node.order(base, out);
        }

        out.extend_from_slice(&self.values);

        if let Some(ref node) = *latter {
            node.order(base, out);
        }
    }
}


#[cfg(test)]
mod tests {
    extern crate rand;
    use super::*;
    use self::rand::Rng;

    #[derive(Clone, Debug, PartialEq)]
    struct Plane1D(i32, bool);

    impl Plane for Plane1D {
        fn cut(&self, plane: Self) -> PlaneCut<Self> {
            if self.0 == plane.0 {
                PlaneCut::Sibling(plane)
            } else if (plane.0 > self.0) == self.1 {
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

        fn is_aligned(&self, plane: &Self) -> bool {
            self.1 == plane.1
        }
    }


    #[test]
    fn test_add_side() {
        let mut node_opt = None;
        let p0: Vec<Plane1D> = Vec::new();
        add_side(&mut node_opt, p0);
        assert!(node_opt.is_none());

        let p1 = Plane1D(1, true);
        add_side(&mut node_opt, vec![p1.clone()]);
        assert_eq!(node_opt.as_ref().unwrap().values, vec![p1.clone()]);
        assert!(node_opt.as_ref().unwrap().is_leaf());

        let p23 = vec![Plane1D(0, false), Plane1D(2, false)];
        add_side(&mut node_opt, p23);
        let node = node_opt.unwrap();
        assert_eq!(node.values, vec![p1.clone()]);
        assert!(node.front.is_some() && node.back.is_some());
    }

    #[test]
    fn test_insert_depth() {
        let mut node = BspNode::new();
        assert_eq!(node.get_depth(), 0);
        node.insert(Plane1D(0, true));
        assert_eq!(node.get_depth(), 1);
        node.insert(Plane1D(6, true));
        assert_eq!(node.get_depth(), 2);
        node.insert(Plane1D(8, true));
        assert_eq!(node.get_depth(), 3);
        node.insert(Plane1D(6, true));
        assert_eq!(node.get_depth(), 3);
        node.insert(Plane1D(-5, false));
        assert_eq!(node.get_depth(), 3);
    }

    #[test]
    fn test_order() {
        let mut rng = rand::thread_rng();
        let mut node = BspNode::new();
        let mut out = Vec::new();

        node.order(&Plane1D(0, true), &mut out);
        assert!(out.is_empty());

        for _ in 0 .. 100 {
            let plane = Plane1D(rng.gen(), rng.gen());
            node.insert(plane);
        }

        node.order(&Plane1D(0, true), &mut out);
        let mut out2 = out.clone();
        out2.sort_by_key(|p| -p.0);
        assert_eq!(out, out2);
    }
}
