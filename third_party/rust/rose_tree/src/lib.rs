//!
//! **rose_tree** is a rose tree (aka multi-way tree) data structure library.
//!
//! The most prominent type is [**RoseTree**](./struct.RoseTree.html) - a wrapper around [petgraph]
//! (http://bluss.github.io/petulant-avenger-graphlibrary/doc/petgraph/index.html)'s [**Graph**]
//! (http://bluss.github.io/petulant-avenger-graphlibrary/doc/petgraph/graph/struct.Graph.html)
//! data structure, exposing a refined API targeted towards rose tree related functionality.
//!

#![forbid(unsafe_code)]
#![warn(missing_docs)]

pub extern crate petgraph;
use petgraph as pg;

use petgraph::graph::IndexType;
pub use petgraph::graph::NodeIndex;

type DefIndex = u32;

/// The PetGraph to be used internally within the RoseTree for storing/managing nodes and edges.
pub type PetGraph<N, Ix> = pg::Graph<N, (), pg::Directed, Ix>;

/// An indexable tree data structure with a variable and unbounded number of branches per node.
///
/// Also known as a multi-way tree.
///
/// See the [wikipedia article on the "Rose tree" data
/// structure](https://en.wikipedia.org/wiki/Rose_tree).
///
/// Note: The following documentation is adapted from petgraph's [**Graph** documentation]
/// (http://bluss.github.io/petulant-avenger-graphlibrary/doc/petgraph/graph/struct.Graph.html).
///
/// **RoseTree** is parameterized over the node weight **N** and **Ix** which is the index type
/// used.
///
/// A wrapper around petgraph's **Graph** data structure with a refined API specifically targeted
/// towards use as a **RoseTree**.
///
/// **NodeIndex** is a type that acts as a reference to nodes, but these are only stable across
/// certain operations. **Removing nodes may shift other indices.** Adding kids to the **RoseTree**
/// keeps all indices stable, but removing a node will force the last node to shift its index to
/// take its place.
///
/// The fact that the node indices in the **RoseTree** are numbered in a compact interval from 0 to
/// *n*-1 simplifies some graph algorithms.
///
/// The **Ix** parameter is u32 by default. The goal is that you can ignore this parameter
/// completely unless you need a very large **RoseTree** -- then you can use usize.
///
/// The **RoseTree** also offers methods for accessing the underlying **Graph**, which can be useful
/// for taking advantage of petgraph's various graph-related algorithms.
#[derive(Clone, Debug)]
pub struct RoseTree<N, Ix: IndexType = DefIndex> {
    /// A graph for storing all nodes and edges between them that represent the tree.
    graph: PetGraph<N, Ix>,
}

/// An iterator that yeilds an index to the parent of the current child before the setting the
/// parent as the new current child. This occurs recursively until the root index is yeilded.
pub struct ParentRecursion<'a, N: 'a, Ix: IndexType> {
    rose_tree: &'a RoseTree<N, Ix>,
    child: NodeIndex<Ix>,
}

/// An iterator yielding indices to the children of some node.
pub type Children<'a, Ix> = pg::graph::Neighbors<'a, (), Ix>;

/// An iterator yielding indices to the siblings of some child node.
pub struct Siblings<'a, Ix: IndexType> {
    child: NodeIndex<Ix>,
    maybe_siblings: Option<Children<'a, Ix>>,
}

/// A "walker" object that can be used to step through the children of some parent node.
pub struct WalkChildren<Ix: IndexType> {
    walk_edges: pg::graph::WalkNeighbors<Ix>,
}

/// A "walker" object that can be used to step through the siblings of some child node.
pub struct WalkSiblings<Ix: IndexType> {
    child: NodeIndex<Ix>,
    maybe_walk_children: Option<WalkChildren<Ix>>,
}

/// `RoseTree`'s API ensures that it always has a "root" node and that its index is always 0.
pub const ROOT: usize = 0;

impl<N, Ix> RoseTree<N, Ix>
where
    Ix: IndexType,
{
    /// Create a new `RoseTree` along with some root node.
    /// Returns both the `RoseTree` and an index into the root node in a tuple.
    pub fn new(root: N) -> (Self, NodeIndex<Ix>) {
        Self::with_capacity(1, root)
    }

    /// Create a new `RoseTree` with estimated capacity and some root node.
    /// Returns both the `RoseTree` and an index into the root node in a tuple.
    pub fn with_capacity(nodes: usize, root: N) -> (Self, NodeIndex<Ix>) {
        let mut graph = PetGraph::with_capacity(nodes, nodes);
        let root = graph.add_node(root);
        (RoseTree { graph }, root)
    }

    /// The total number of nodes in the RoseTree.
    pub fn node_count(&self) -> usize {
        self.graph.node_count()
    }

    /// Borrow the `RoseTree`'s underlying `PetGraph<N, Ix>`.
    /// All existing `NodeIndex`s may be used to index into this graph the same way they may be
    /// used to index into the `RoseTree`.
    pub fn graph(&self) -> &PetGraph<N, Ix> {
        &self.graph
    }

    /// Take ownership of the RoseTree and return the internal PetGraph<N, Ix>.
    /// All existing `NodeIndex`s may be used to index into this graph the same way they may be
    /// used to index into the `RoseTree`.
    pub fn into_graph(self) -> PetGraph<N, Ix> {
        let RoseTree { graph } = self;
        graph
    }

    /// Add a child node to the node at the given NodeIndex.
    /// Returns an index into the child's position within the tree.
    ///
    /// Computes in **O(1)** time.
    ///
    /// **Panics** if the given parent node doesn't exist.
    ///
    /// **Panics** if the Graph is at the maximum number of edges for its index.
    pub fn add_child(&mut self, parent: NodeIndex<Ix>, kid: N) -> NodeIndex<Ix> {
        let kid = self.graph.add_node(kid);
        self.graph.add_edge(parent, kid, ());
        kid
    }

    /// Borrow the weight from the node at the given index.
    pub fn node_weight(&self, node: NodeIndex<Ix>) -> Option<&N> {
        self.graph.node_weight(node)
    }

    /// Mutably borrow the weight from the node at the given index.
    pub fn node_weight_mut(&mut self, node: NodeIndex<Ix>) -> Option<&mut N> {
        self.graph.node_weight_mut(node)
    }

    /// Index the `RoseTree` by two node indices.
    ///
    /// **Panics** if the indices are equal or if they are out of bounds.
    pub fn index_twice_mut(&mut self, a: NodeIndex<Ix>, b: NodeIndex<Ix>) -> (&mut N, &mut N) {
        self.graph.index_twice_mut(a, b)
    }

    /// Remove all nodes in the `RoseTree` except for the root.
    pub fn remove_all_but_root(&mut self) {
        // We can assume that the `root`'s index is zero, as it is always the first node to be
        // added to the RoseTree.
        if let Some(root) = self.graph.remove_node(NodeIndex::new(0)) {
            self.graph.clear();
            self.graph.add_node(root);
        }
    }

    /// Removes and returns the node at the given index.
    ///
    /// The parent of `node` will become the new parent for all of its children.
    ///
    /// The root node cannot be removed. If its index is given, `None` will be returned.
    ///
    /// Note: this method may shift other node indices, invalidating previously returned indices!
    pub fn remove_node(&mut self, node: NodeIndex<Ix>) -> Option<N> {
        // Check if an attempt to remove the root node has been made.
        if node.index() == ROOT || self.graph.node_weight(node).is_none() {
            return None;
        }

        // Now that we know we're not the root node, we know that we **must** have some parent.
        let parent = self.parent(node).expect("No parent node found");

        // For each of `node`'s children, set their parent to `node`'s parent.
        let mut children = self.graph.neighbors_directed(node, pg::Outgoing).detach();
        while let Some((child_edge, child_node)) = children.next(&self.graph) {
            self.graph.remove_edge(child_edge);
            self.graph.add_edge(parent, child_node, ());
        }

        // Finally, remove our node and return it.
        self.graph.remove_node(node)
    }

    /// Removes the node at the given index along with all their children, returning them as a new
    /// RoseTree.
    ///
    /// If there was no node at the given index, `None` will be returned.
    pub fn remove_node_with_children(&mut self, _node: NodeIndex<Ix>) -> Option<RoseTree<N, Ix>> {
        unimplemented!();
    }

    /// An index to the parent of the node at the given index if there is one.
    pub fn parent(&self, child: NodeIndex<Ix>) -> Option<NodeIndex<Ix>> {
        self.graph.neighbors_directed(child, pg::Incoming).next()
    }

    /// An iterator over the given child's parent, that parent's parent and so forth.
    ///
    /// The returned iterator yields `NodeIndex<Ix>`s.
    pub fn parent_recursion(&self, child: NodeIndex<Ix>) -> ParentRecursion<N, Ix> {
        ParentRecursion {
            rose_tree: self,
            child,
        }
    }

    /// An iterator over all nodes that are children to the node at the given index.
    ///
    /// The returned iterator yields `NodeIndex<Ix>`s.
    pub fn children(&self, parent: NodeIndex<Ix>) -> Children<Ix> {
        self.graph.neighbors_directed(parent, pg::Outgoing)
    }

    /// A "walker" object that may be used to step through the children of the given parent node.
    ///
    /// Unlike the `Children` type, `WalkChildren` does not borrow the `RoseTree`.
    pub fn walk_children(&self, parent: NodeIndex<Ix>) -> WalkChildren<Ix> {
        let walk_edges = self.graph.neighbors_directed(parent, pg::Outgoing).detach();
        WalkChildren { walk_edges }
    }

    /// An iterator over all nodes that are siblings to the node at the given index.
    ///
    /// The returned iterator yields `NodeIndex<Ix>`s.
    pub fn siblings(&self, child: NodeIndex<Ix>) -> Siblings<Ix> {
        let maybe_siblings = self.parent(child).map(|parent| self.children(parent));
        Siblings {
            child,
            maybe_siblings,
        }
    }

    /// A "walker" object that may be used to step through the siblings of the given child node.
    ///
    /// Unlike the `Siblings` type, `WalkSiblings` does not borrow the `RoseTree`.
    pub fn walk_siblings(&self, child: NodeIndex<Ix>) -> WalkSiblings<Ix> {
        let maybe_walk_children = self.parent(child).map(|parent| self.walk_children(parent));
        WalkSiblings {
            child,
            maybe_walk_children,
        }
    }
}

impl<N, Ix> ::std::ops::Index<NodeIndex<Ix>> for RoseTree<N, Ix>
where
    Ix: IndexType,
{
    type Output = N;
    fn index(&self, index: NodeIndex<Ix>) -> &N {
        &self.graph[index]
    }
}

impl<N, Ix> ::std::ops::IndexMut<NodeIndex<Ix>> for RoseTree<N, Ix>
where
    Ix: IndexType,
{
    fn index_mut(&mut self, index: NodeIndex<Ix>) -> &mut N {
        &mut self.graph[index]
    }
}

impl<'a, Ix> Iterator for Siblings<'a, Ix>
where
    Ix: IndexType,
{
    type Item = NodeIndex<Ix>;
    fn next(&mut self) -> Option<NodeIndex<Ix>> {
        let Siblings {
            child,
            ref mut maybe_siblings,
        } = *self;
        maybe_siblings.as_mut().and_then(|siblings| {
            siblings.next().and_then(|sibling| {
                if sibling != child {
                    Some(sibling)
                } else {
                    siblings.next()
                }
            })
        })
    }
}

impl<'a, N, Ix> Iterator for ParentRecursion<'a, N, Ix>
where
    Ix: IndexType,
{
    type Item = NodeIndex<Ix>;
    fn next(&mut self) -> Option<NodeIndex<Ix>> {
        let ParentRecursion {
            ref mut child,
            ref rose_tree,
        } = *self;
        rose_tree.parent(*child).map(|parent| {
            *child = parent;
            parent
        })
    }
}

impl<Ix> WalkChildren<Ix>
where
    Ix: IndexType,
{
    /// Fetch the next child index in the walk for the given `RoseTree`.
    pub fn next<N>(&mut self, tree: &RoseTree<N, Ix>) -> Option<NodeIndex<Ix>> {
        self.walk_edges.next(&tree.graph).map(|(_, n)| n)
    }
}

impl<Ix> WalkSiblings<Ix>
where
    Ix: IndexType,
{
    /// Fetch the next sibling index in the walk for the given `RoseTree`.
    pub fn next<N>(&mut self, tree: &RoseTree<N, Ix>) -> Option<NodeIndex<Ix>> {
        let WalkSiblings {
            child,
            ref mut maybe_walk_children,
        } = *self;
        maybe_walk_children.as_mut().and_then(|walk_children| {
            walk_children.next(tree).and_then(|sibling| {
                if child != sibling {
                    Some(sibling)
                } else {
                    walk_children.next(tree)
                }
            })
        })
    }
}
