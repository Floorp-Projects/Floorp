use crate::{Plane, PlaneCut, Polygon};

use euclid::default::{Point3D, Vector3D};
use smallvec::SmallVec;

use std::fmt;

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct PolygonIdx(usize);

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct NodeIdx(usize);

/// Binary Space Partitioning splitter, uses a BSP tree.
pub struct BspSplitter<A: Copy> {
    result: Vec<Polygon<A>>,
    nodes: Vec<BspNode>,
    polygons: Vec<Polygon<A>>,
}

impl<A: Copy> BspSplitter<A> {
    /// Create a new BSP splitter.
    pub fn new() -> Self {
        BspSplitter {
            result: Vec::new(),
            nodes: vec![BspNode::new()],
            polygons: Vec::new(),
        }
    }
}

impl<A> BspSplitter<A>
where
    A: Copy + fmt::Debug + Default,
{
    /// Put the splitter back in it initial state.
    ///
    /// Call this at the beginning of every frame when reusing the splitter.
    pub fn reset(&mut self) {
        self.polygons.clear();
        self.nodes.clear();
        self.nodes.push(BspNode::new());
    }

    /// Add a polygon to the plane splitter.
    ///
    /// This is where most of the expensive computation happens.
    pub fn add(&mut self, poly: Polygon<A>) {
        let root = NodeIdx(0);
        self.insert(root, &poly);
    }

    /// Sort the added and split polygons against the view vector.
    ///
    /// Call this towards the end of the frame after having added all polygons.
    pub fn sort(&mut self, view: Vector3D<f64>) -> &[Polygon<A>] {
        //debug!("\t\ttree before sorting {:?}", self.tree);
        let poly = Polygon {
            points: [Point3D::origin(); 4],
            plane: Plane {
                normal: -view, //Note: BSP `order()` is back to front
                offset: 0.0,
            },
            anchor: A::default(),
        };

        let root = NodeIdx(0);
        let mut result = std::mem::take(&mut self.result);
        result.clear();
        self.order(root, &poly, &mut result);
        self.result = result;

        &self.result
    }

    /// Process a set of polygons at once.
    pub fn solve(&mut self, input: &[Polygon<A>], view: Vector3D<f64>) -> &[Polygon<A>]
    where
        A: Copy,
    {
        self.reset();
        for p in input {
            self.add(p.clone());
        }
        self.sort(view)
    }

    /// Insert a value into the sub-tree starting with this node.
    /// This operation may spawn additional leafs/branches of the tree.
    fn insert(&mut self, node_idx: NodeIdx, value: &Polygon<A>) {
        let node = &mut self.nodes[node_idx.0];
        if node.values.is_empty() {
            node.values.push(add_polygon(&mut self.polygons, value));
            return;
        }

        let mut front: SmallVec<[Polygon<A>; 2]> = SmallVec::new();
        let mut back: SmallVec<[Polygon<A>; 2]> = SmallVec::new();
        let first = node.values[0].0;
        match self.polygons[first].cut(value, &mut front, &mut back) {
            PlaneCut::Sibling => {
                node.values.push(add_polygon(&mut self.polygons, value));
            }
            PlaneCut::Cut => {
                if front.len() != 0 {
                    if self.nodes[node_idx.0].front.is_none() {
                        self.nodes[node_idx.0].front = Some(add_node(&mut self.nodes));
                    }
                    let node_front = self.nodes[node_idx.0].front.unwrap();
                    for p in &front {
                        self.insert(node_front, p)
                    }
                }
                if back.len() != 0 {
                    if self.nodes[node_idx.0].back.is_none() {
                        self.nodes[node_idx.0].back = Some(add_node(&mut self.nodes));
                    }
                    let node_back = self.nodes[node_idx.0].back.unwrap();
                    for p in &back {
                        self.insert(node_back, p)
                    }
                }
            }
        }
    }

    /// Build the draw order of this sub-tree into an `out` vector,
    /// so that the contained planes are sorted back to front according
    /// to the view vector defined as the `base` plane front direction.
    pub fn order(&self, node: NodeIdx, base: &Polygon<A>, out: &mut Vec<Polygon<A>>) {
        let node = &self.nodes[node.0];
        let (former, latter) = match node.values.first() {
            None => return,
            Some(first) => {
                if base.is_aligned(&self.polygons[first.0]) {
                    (node.front, node.back)
                } else {
                    (node.back, node.front)
                }
            }
        };

        if let Some(node) = former {
            self.order(node, base, out);
        }

        out.reserve(node.values.len());
        for poly_idx in &node.values {
            out.push(self.polygons[poly_idx.0].clone());
        }

        if let Some(node) = latter {
            self.order(node, base, out);
        }
    }
}

pub fn add_polygon<A: Copy>(polygons: &mut Vec<Polygon<A>>, poly: &Polygon<A>) -> PolygonIdx {
    let index = PolygonIdx(polygons.len());
    polygons.push(poly.clone());
    index
}

pub fn add_node(nodes: &mut Vec<BspNode>) -> NodeIdx {
    let index = NodeIdx(nodes.len());
    nodes.push(BspNode::new());
    index
}

/// A node in the `BspTree`, which can be considered a tree itself.
#[derive(Clone, Debug)]
pub struct BspNode {
    values: SmallVec<[PolygonIdx; 4]>,
    front: Option<NodeIdx>,
    back: Option<NodeIdx>,
}

impl BspNode {
    /// Create a new node.
    pub fn new() -> Self {
        BspNode {
            values: SmallVec::new(),
            front: None,
            back: None,
        }
    }
}
