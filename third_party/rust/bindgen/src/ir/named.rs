//! Discover which template type parameters are actually used.
//!
//! ### Why do we care?
//!
//! C++ allows ignoring template parameters, while Rust does not. Usually we can
//! blindly stick a `PhantomData<T>` inside a generic Rust struct to make up for
//! this. That doesn't work for templated type aliases, however:
//!
//! ```C++
//! template <typename T>
//! using Fml = int;
//! ```
//!
//! If we generate the naive Rust code for this alias, we get:
//!
//! ```ignore
//! pub type Fml<T> = ::std::os::raw::int;
//! ```
//!
//! And this is rejected by `rustc` due to the unused type parameter.
//!
//! (Aside: in these simple cases, `libclang` will often just give us the
//! aliased type directly, and we will never even know we were dealing with
//! aliases, let alone templated aliases. It's the more convoluted scenarios
//! where we get to have some fun...)
//!
//! For such problematic template aliases, we could generate a tuple whose
//! second member is a `PhantomData<T>`. Or, if we wanted to go the extra mile,
//! we could even generate some smarter wrapper that implements `Deref`,
//! `DerefMut`, `From`, `Into`, `AsRef`, and `AsMut` to the actually aliased
//! type. However, this is still lackluster:
//!
//! 1. Even with a billion conversion-trait implementations, using the generated
//!    bindings is rather un-ergonomic.
//! 2. With either of these solutions, we need to keep track of which aliases
//!    we've transformed like this in order to generate correct uses of the
//!    wrapped type.
//!
//! Given that we have to properly track which template parameters ended up used
//! for (2), we might as well leverage that information to make ergonomic
//! bindings that don't contain any unused type parameters at all, and
//! completely avoid the pain of (1).
//!
//! ### How do we determine which template parameters are used?
//!
//! Determining which template parameters are actually used is a trickier
//! problem than it might seem at a glance. On the one hand, trivial uses are
//! easy to detect:
//!
//! ```C++
//! template <typename T>
//! class Foo {
//!     T trivial_use_of_t;
//! };
//! ```
//!
//! It gets harder when determining if one template parameter is used depends on
//! determining if another template parameter is used. In this example, whether
//! `U` is used depends on whether `T` is used.
//!
//! ```C++
//! template <typename T>
//! class DoesntUseT {
//!     int x;
//! };
//!
//! template <typename U>
//! class Fml {
//!     DoesntUseT<U> lololol;
//! };
//! ```
//!
//! We can express the set of used template parameters as a constraint solving
//! problem (where the set of template parameters used by a given IR item is the
//! union of its sub-item's used template parameters) and iterate to a
//! fixed-point.
//!
//! We use the "monotone framework" for this fix-point analysis where our
//! lattice is the powerset of the template parameters that appear in the input
//! C++ header, our join function is set union, and we use the
//! `ir::traversal::Trace` trait to implement the work-list optimization so we
//! don't have to revisit every node in the graph when for every iteration
//! towards the fix-point.
//!
//! For a deeper introduction to the general form of this kind of analysis, see
//! [Static Program Analysis by Anders Møller and Michael I. Schwartzbach][spa].
//!
//! [spa]: https://cs.au.dk/~amoeller/spa/spa.pdf

use std::collections::HashMap;
use std::fmt;
use super::context::{BindgenContext, ItemId};
use super::item::ItemSet;
use super::traversal::{EdgeKind, Trace};
use super::ty::{TemplateDeclaration, TypeKind};

/// An analysis in the monotone framework.
///
/// Implementors of this trait must maintain the following two invariants:
///
/// 1. The concrete data must be a member of a finite-height lattice.
/// 2. The concrete `constrain` method must be monotone: that is,
///    if `x <= y`, then `constrain(x) <= constrain(y)`.
///
/// If these invariants do not hold, iteration to a fix-point might never
/// complete.
///
/// For a simple example analysis, see the `ReachableFrom` type in the `tests`
/// module below.
pub trait MonotoneFramework: Sized + fmt::Debug {
    /// The type of node in our dependency graph.
    ///
    /// This is just generic (and not `ItemId`) so that we can easily unit test
    /// without constructing real `Item`s and their `ItemId`s.
    type Node: Copy;

    /// Any extra data that is needed during computation.
    ///
    /// Again, this is just generic (and not `&BindgenContext`) so that we can
    /// easily unit test without constructing real `BindgenContext`s full of
    /// real `Item`s and real `ItemId`s.
    type Extra: Sized;

    /// The final output of this analysis. Once we have reached a fix-point, we
    /// convert `self` into this type, and return it as the final result of the
    /// analysis.
    type Output: From<Self>;

    /// Construct a new instance of this analysis.
    fn new(extra: Self::Extra) -> Self;

    /// Get the initial set of nodes from which to start the analysis. Unless
    /// you are sure of some domain-specific knowledge, this should be the
    /// complete set of nodes.
    fn initial_worklist(&self) -> Vec<Self::Node>;

    /// Update the analysis for the given node.
    ///
    /// If this results in changing our internal state (ie, we discovered that
    /// we have not reached a fix-point and iteration should continue), return
    /// `true`. Otherwise, return `false`. When `constrain` returns false for
    /// all nodes in the set, we have reached a fix-point and the analysis is
    /// complete.
    fn constrain(&mut self, node: Self::Node) -> bool;

    /// For each node `d` that depends on the given `node`'s current answer when
    /// running `constrain(d)`, call `f(d)`. This informs us which new nodes to
    /// queue up in the worklist when `constrain(node)` reports updated
    /// information.
    fn each_depending_on<F>(&self, node: Self::Node, f: F)
        where F: FnMut(Self::Node);
}

/// Run an analysis in the monotone framework.
// TODO: This allow(...) is just temporary until we replace
// `Item::signature_contains_named_type` with
// `analyze::<UsedTemplateParameters>`.
#[allow(dead_code)]
pub fn analyze<Analysis>(extra: Analysis::Extra) -> Analysis::Output
    where Analysis: MonotoneFramework
{
    let mut analysis = Analysis::new(extra);
    let mut worklist = analysis.initial_worklist();

    while let Some(node) = worklist.pop() {
        if analysis.constrain(node) {
            analysis.each_depending_on(node, |needs_work| {
                worklist.push(needs_work);
            });
        }
    }

    analysis.into()
}

/// An analysis that finds the set of template parameters that actually end up
/// used in our generated bindings.
#[derive(Debug, Clone)]
pub struct UsedTemplateParameters<'a> {
    ctx: &'a BindgenContext<'a>,
    used: ItemSet,
    dependencies: HashMap<ItemId, Vec<ItemId>>,
}

impl<'a> MonotoneFramework for UsedTemplateParameters<'a> {
    type Node = ItemId;
    type Extra = &'a BindgenContext<'a>;
    type Output = ItemSet;

    fn new(ctx: &'a BindgenContext<'a>) -> UsedTemplateParameters<'a> {
        let mut dependencies = HashMap::new();

        for item in ctx.whitelisted_items() {
            {
                // We reverse our natural IR graph edges to find dependencies
                // between nodes.
                let mut add_reverse_edge = |sub_item, _| {
                    dependencies.entry(sub_item).or_insert(vec![]).push(item);
                };
                item.trace(ctx, &mut add_reverse_edge, &());
            }

            // Additionally, whether a template instantiation's template
            // arguments are used depends on whether the template declaration's
            // generic template parameters are used.
            ctx.resolve_item_fallible(item)
                .and_then(|item| item.as_type())
                .map(|ty| match ty.kind() {
                    &TypeKind::TemplateInstantiation(decl, ref args) => {
                        let decl = ctx.resolve_type(decl);
                        let params = decl.template_params(ctx)
                            .expect("a template instantiation's referenced \
                                     template declaration should have template \
                                     parameters");
                        for (arg, param) in args.iter().zip(params.iter()) {
                            dependencies.entry(*arg).or_insert(vec![]).push(*param);
                        }
                    }
                    _ => {}
                });
        }

        UsedTemplateParameters {
            ctx: ctx,
            used: ItemSet::new(),
            dependencies: dependencies,
        }
    }

    fn initial_worklist(&self) -> Vec<Self::Node> {
        self.ctx.whitelisted_items().collect()
    }

    fn constrain(&mut self, item: ItemId) -> bool {
        let original_size = self.used.len();

        item.trace(self.ctx, &mut |item, edge_kind| {
            if edge_kind == EdgeKind::TemplateParameterDefinition {
                // The definition of a template parameter is not considered a
                // use of said template parameter. Ignore this edge.
                return;
            }

            let ty_kind = self.ctx.resolve_item(item)
                .as_type()
                .map(|ty| ty.kind());

            match ty_kind {
                Some(&TypeKind::Named) => {
                    // This is a "trivial" use of the template type parameter.
                    self.used.insert(item);
                },
                Some(&TypeKind::TemplateInstantiation(decl, ref args)) => {
                    // A template instantiation's concrete template argument is
                    // only used if the template declaration uses the
                    // corresponding template parameter.
                    let decl = self.ctx.resolve_type(decl);
                    let params = decl.template_params(self.ctx)
                        .expect("a template instantiation's referenced \
                                 template declaration should have template \
                                 parameters");
                    for (arg, param) in args.iter().zip(params.iter()) {
                        if self.used.contains(param) {
                            if self.ctx.resolve_item(*arg).is_named() {
                                self.used.insert(*arg);
                            }
                        }
                    }
                },
                _ => return,
            }
        }, &());

        let new_size = self.used.len();
        new_size != original_size
    }

    fn each_depending_on<F>(&self, item: ItemId, mut f: F)
        where F: FnMut(Self::Node)
    {
        if let Some(edges) = self.dependencies.get(&item) {
            for item in edges {
                f(*item);
            }
        }
    }
}

impl<'a> From<UsedTemplateParameters<'a>> for ItemSet {
    fn from(used_templ_params: UsedTemplateParameters) -> ItemSet {
        used_templ_params.used
    }
}

#[cfg(test)]
mod tests {
    use std::collections::{HashMap, HashSet};
    use super::*;

    // Here we find the set of nodes that are reachable from any given
    // node. This is a lattice mapping nodes to subsets of all nodes. Our join
    // function is set union.
    //
    // This is our test graph:
    //
    //     +---+                    +---+
    //     |   |                    |   |
    //     | 1 |               .----| 2 |
    //     |   |               |    |   |
    //     +---+               |    +---+
    //       |                 |      ^
    //       |                 |      |
    //       |      +---+      '------'
    //       '----->|   |
    //              | 3 |
    //       .------|   |------.
    //       |      +---+      |
    //       |        ^        |
    //       v        |        v
    //     +---+      |      +---+    +---+
    //     |   |      |      |   |    |   |
    //     | 4 |      |      | 5 |--->| 6 |
    //     |   |      |      |   |    |   |
    //     +---+      |      +---+    +---+
    //       |        |        |        |
    //       |        |        |        v
    //       |      +---+      |      +---+
    //       |      |   |      |      |   |
    //       '----->| 7 |<-----'      | 8 |
    //              |   |             |   |
    //              +---+             +---+
    //
    // And here is the mapping from a node to the set of nodes that are
    // reachable from it within the test graph:
    //
    //     1: {3,4,5,6,7,8}
    //     2: {2}
    //     3: {3,4,5,6,7,8}
    //     4: {3,4,5,6,7,8}
    //     5: {3,4,5,6,7,8}
    //     6: {8}
    //     7: {3,4,5,6,7,8}
    //     8: {}

    #[derive(Clone, Copy, Debug, Hash, PartialEq, Eq)]
    struct Node(usize);

    #[derive(Clone, Debug, Default, PartialEq, Eq)]
    struct Graph(HashMap<Node, Vec<Node>>);

    impl Graph {
        fn make_test_graph() -> Graph {
            let mut g = Graph::default();
            g.0.insert(Node(1), vec![Node(3)]);
            g.0.insert(Node(2), vec![Node(2)]);
            g.0.insert(Node(3), vec![Node(4), Node(5)]);
            g.0.insert(Node(4), vec![Node(7)]);
            g.0.insert(Node(5), vec![Node(6), Node(7)]);
            g.0.insert(Node(6), vec![Node(8)]);
            g.0.insert(Node(7), vec![Node(3)]);
            g.0.insert(Node(8), vec![]);
            g
        }
        
        fn reverse(&self) -> Graph {
            let mut reversed = Graph::default();
            for (node, edges) in self.0.iter() {
                reversed.0.entry(*node).or_insert(vec![]);
                for referent in edges.iter() {
                    reversed.0.entry(*referent).or_insert(vec![]).push(*node);
                }
            }
            reversed
        }
    }

    #[derive(Clone, Debug, PartialEq, Eq)]
    struct ReachableFrom<'a> {
        reachable: HashMap<Node, HashSet<Node>>,
        graph: &'a Graph,
        reversed: Graph,
    }

    impl<'a> MonotoneFramework for ReachableFrom<'a> {
        type Node = Node;
        type Extra = &'a Graph;
        type Output = HashMap<Node, HashSet<Node>>;

        fn new(graph: &'a Graph) -> ReachableFrom {
            let reversed = graph.reverse();
            ReachableFrom {
                reachable: Default::default(),
                graph: graph,
                reversed: reversed,
            }
        }

        fn initial_worklist(&self) -> Vec<Node> {
            self.graph.0.keys().cloned().collect()
        }

        fn constrain(&mut self, node: Node) -> bool {
            // The set of nodes reachable from a node `x` is
            //
            //     reachable(x) = s_0 U s_1 U ... U reachable(s_0) U reachable(s_1) U ...
            //
            // where there exist edges from `x` to each of `s_0, s_1, ...`.
            //
            // Yes, what follows is a **terribly** inefficient set union
            // implementation. Don't copy this code outside of this test!

            let original_size = self.reachable.entry(node).or_insert(HashSet::new()).len();
            
            for sub_node in self.graph.0[&node].iter() {
                self.reachable.get_mut(&node).unwrap().insert(*sub_node);

                let sub_reachable = self.reachable
                    .entry(*sub_node)
                    .or_insert(HashSet::new())
                    .clone();

                for transitive in sub_reachable {
                    self.reachable.get_mut(&node).unwrap().insert(transitive);
                }
            }

            let new_size = self.reachable[&node].len();
            original_size != new_size
        }

        fn each_depending_on<F>(&self, node: Node, mut f: F)
            where F: FnMut(Node)
        {
            for dep in self.reversed.0[&node].iter() {
                f(*dep);
            }
        }
    }

    impl<'a> From<ReachableFrom<'a>> for HashMap<Node, HashSet<Node>> {
        fn from(reachable: ReachableFrom<'a>) -> Self {
            reachable.reachable
        }
    }

    #[test]
    fn monotone() {
        let g = Graph::make_test_graph();
        let reachable = analyze::<ReachableFrom>(&g);
        println!("reachable = {:#?}", reachable);

        fn nodes<A>(nodes: A) -> HashSet<Node>
            where A: AsRef<[usize]>
        {
            nodes.as_ref().iter().cloned().map(Node).collect()
        }

        let mut expected = HashMap::new();
        expected.insert(Node(1), nodes([3,4,5,6,7,8]));
        expected.insert(Node(2), nodes([2]));
        expected.insert(Node(3), nodes([3,4,5,6,7,8]));
        expected.insert(Node(4), nodes([3,4,5,6,7,8]));
        expected.insert(Node(5), nodes([3,4,5,6,7,8]));
        expected.insert(Node(6), nodes([8]));
        expected.insert(Node(7), nodes([3,4,5,6,7,8]));
        expected.insert(Node(8), nodes([]));
        println!("expected = {:#?}", expected);

        assert_eq!(reachable, expected);
    }
}
