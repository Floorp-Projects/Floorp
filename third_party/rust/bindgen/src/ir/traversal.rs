//! Traversal of the graph of IR items and types.

use super::context::{BindgenContext, ItemId};
use super::item::ItemSet;
use std::collections::{BTreeMap, VecDeque};

/// An outgoing edge in the IR graph is a reference from some item to another
/// item:
///
///   from --> to
///
/// The `from` is left implicit: it is the concrete `Trace` implementor which
/// yielded this outgoing edge.
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Edge {
    to: ItemId,
    kind: EdgeKind,
}

impl Edge {
    /// Construct a new edge whose referent is `to` and is of the given `kind`.
    pub fn new(to: ItemId, kind: EdgeKind) -> Edge {
        Edge {
            to: to,
            kind: kind,
        }
    }

    /// Get the item that this edge is pointing to.
    pub fn to(&self) -> ItemId {
        self.to
    }

    /// Get the kind of edge that this is.
    pub fn kind(&self) -> EdgeKind {
        self.kind
    }
}

impl Into<ItemId> for Edge {
    fn into(self) -> ItemId {
        self.to
    }
}

/// The kind of edge reference. This is useful when we wish to only consider
/// certain kinds of edges for a particular traversal.
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum EdgeKind {
    /// A generic, catch-all edge.
    Generic,

    /// An edge from a template declaration, to the definition of a named type
    /// parameter. For example, the edge Foo -> T in the following snippet:
    ///
    /// ```C++
    /// template<typename T>
    /// class Foo {
    ///     int x;
    /// };
    /// ```
    TemplateParameterDefinition,
}

/// A predicate to allow visiting only sub-sets of the whole IR graph by
/// excluding certain edges from being followed by the traversal.
pub trait TraversalPredicate {
    /// Should the traversal follow this edge, and visit everything that is
    /// reachable through it?
    fn should_follow(&self, edge: Edge) -> bool;
}

impl TraversalPredicate for fn(Edge) -> bool {
    fn should_follow(&self, edge: Edge) -> bool {
        (*self)(edge)
    }
}

/// A `TraversalPredicate` implementation that follows all edges, and therefore
/// traversals using this predicate will see the whole IR graph reachable from
/// the traversal's roots.
pub fn all_edges(_: Edge) -> bool {
    true
}

/// A `TraversalPredicate` implementation that never follows any edges, and
/// therefore traversals using this predicate will only visit the traversal's
/// roots.
pub fn no_edges(_: Edge) -> bool {
    false
}

/// The storage for the set of items that have been seen (although their
/// outgoing edges might not have been fully traversed yet) in an active
/// traversal.
pub trait TraversalStorage<'ctx, 'gen> {
    /// Construct a new instance of this TraversalStorage, for a new traversal.
    fn new(ctx: &'ctx BindgenContext<'gen>) -> Self;

    /// Add the given item to the storage. If the item has never been seen
    /// before, return `true`. Otherwise, return `false`.
    ///
    /// The `from` item is the item from which we discovered this item, or is
    /// `None` if this item is a root.
    fn add(&mut self, from: Option<ItemId>, item: ItemId) -> bool;
}

impl<'ctx, 'gen> TraversalStorage<'ctx, 'gen> for ItemSet {
    fn new(_: &'ctx BindgenContext<'gen>) -> Self {
        ItemSet::new()
    }

    fn add(&mut self, _: Option<ItemId>, item: ItemId) -> bool {
        self.insert(item)
    }
}

/// A `TraversalStorage` implementation that keeps track of how we first reached
/// each item. This is useful for providing debug assertions with meaningful
/// diagnostic messages about dangling items.
#[derive(Debug)]
pub struct Paths<'ctx, 'gen>(BTreeMap<ItemId, ItemId>,
                             &'ctx BindgenContext<'gen>)
    where 'gen: 'ctx;

impl<'ctx, 'gen> TraversalStorage<'ctx, 'gen> for Paths<'ctx, 'gen>
    where 'gen: 'ctx,
{
    fn new(ctx: &'ctx BindgenContext<'gen>) -> Self {
        Paths(BTreeMap::new(), ctx)
    }

    fn add(&mut self, from: Option<ItemId>, item: ItemId) -> bool {
        let newly_discovered =
            self.0.insert(item, from.unwrap_or(item)).is_none();

        if self.1.resolve_item_fallible(item).is_none() {
            let mut path = vec![];
            let mut current = item;
            loop {
                let predecessor = *self.0
                    .get(&current)
                    .expect("We know we found this item id, so it must have a \
                            predecessor");
                if predecessor == current {
                    break;
                }
                path.push(predecessor);
                current = predecessor;
            }
            path.reverse();
            panic!("Found reference to dangling id = {:?}\nvia path = {:?}",
                   item,
                   path);
        }

        newly_discovered
    }
}

/// The queue of seen-but-not-yet-traversed items.
///
/// Using a FIFO queue with a traversal will yield a breadth-first traversal,
/// while using a LIFO queue will result in a depth-first traversal of the IR
/// graph.
pub trait TraversalQueue: Default {
    /// Add a newly discovered item to the queue.
    fn push(&mut self, item: ItemId);

    /// Pop the next item to traverse, if any.
    fn next(&mut self) -> Option<ItemId>;
}

impl TraversalQueue for Vec<ItemId> {
    fn push(&mut self, item: ItemId) {
        self.push(item);
    }

    fn next(&mut self) -> Option<ItemId> {
        self.pop()
    }
}

impl TraversalQueue for VecDeque<ItemId> {
    fn push(&mut self, item: ItemId) {
        self.push_back(item);
    }

    fn next(&mut self) -> Option<ItemId> {
        self.pop_front()
    }
}

/// Something that can receive edges from a `Trace` implementation.
pub trait Tracer {
    /// Note an edge between items. Called from within a `Trace` implementation.
    fn visit_kind(&mut self, item: ItemId, kind: EdgeKind);

    /// A synonym for `tracer.visit_kind(item, EdgeKind::Generic)`.
    fn visit(&mut self, item: ItemId) {
        self.visit_kind(item, EdgeKind::Generic);
    }
}

impl<F> Tracer for F
    where F: FnMut(ItemId, EdgeKind)
{
    fn visit_kind(&mut self, item: ItemId, kind: EdgeKind) {
        (*self)(item, kind)
    }
}

/// Trace all of the outgoing edges to other items. Implementations should call
/// `tracer.visit(edge)` for each of their outgoing edges.
pub trait Trace {
    /// If a particular type needs extra information beyond what it has in
    /// `self` and `context` to find its referenced items, its implementation
    /// can define this associated type, forcing callers to pass the needed
    /// information through.
    type Extra;

    /// Trace all of this item's outgoing edges to other items.
    fn trace<T>(&self,
                context: &BindgenContext,
                tracer: &mut T,
                extra: &Self::Extra)
        where T: Tracer;
}

/// An graph traversal of the transitive closure of references between items.
///
/// See `BindgenContext::whitelisted_items` for more information.
pub struct ItemTraversal<'ctx, 'gen, Storage, Queue, Predicate>
    where 'gen: 'ctx,
          Storage: TraversalStorage<'ctx, 'gen>,
          Queue: TraversalQueue,
          Predicate: TraversalPredicate,
{
    ctx: &'ctx BindgenContext<'gen>,

    /// The set of items we have seen thus far in this traversal.
    seen: Storage,

    /// The set of items that we have seen, but have yet to traverse.
    queue: Queue,

    /// The predicate that determins which edges this traversal will follow.
    predicate: Predicate,

    /// The item we are currently traversing.
    currently_traversing: Option<ItemId>,
}

impl<'ctx, 'gen, Storage, Queue, Predicate> ItemTraversal<'ctx,
                                                          'gen,
                                                          Storage,
                                                          Queue,
                                                          Predicate>
    where 'gen: 'ctx,
          Storage: TraversalStorage<'ctx, 'gen>,
          Queue: TraversalQueue,
          Predicate: TraversalPredicate,
{
    /// Begin a new traversal, starting from the given roots.
    pub fn new<R>(ctx: &'ctx BindgenContext<'gen>,
                  roots: R,
                  predicate: Predicate)
                  -> ItemTraversal<'ctx, 'gen, Storage, Queue, Predicate>
        where R: IntoIterator<Item = ItemId>,
    {
        let mut seen = Storage::new(ctx);
        let mut queue = Queue::default();

        for id in roots {
            seen.add(None, id);
            queue.push(id);
        }

        ItemTraversal {
            ctx: ctx,
            seen: seen,
            queue: queue,
            predicate: predicate,
            currently_traversing: None,
        }
    }
}

impl<'ctx, 'gen, Storage, Queue, Predicate> Tracer
    for ItemTraversal<'ctx, 'gen, Storage, Queue, Predicate>
    where 'gen: 'ctx,
          Storage: TraversalStorage<'ctx, 'gen>,
          Queue: TraversalQueue,
          Predicate: TraversalPredicate,
{
    fn visit_kind(&mut self, item: ItemId, kind: EdgeKind) {
        let edge = Edge::new(item, kind);
        if !self.predicate.should_follow(edge) {
            return;
        }

        let is_newly_discovered = self.seen
            .add(self.currently_traversing, item);
        if is_newly_discovered {
            self.queue.push(item)
        }
    }
}

impl<'ctx, 'gen, Storage, Queue, Predicate> Iterator
    for ItemTraversal<'ctx, 'gen, Storage, Queue, Predicate>
    where 'gen: 'ctx,
          Storage: TraversalStorage<'ctx, 'gen>,
          Queue: TraversalQueue,
          Predicate: TraversalPredicate,
{
    type Item = ItemId;

    fn next(&mut self) -> Option<Self::Item> {
        let id = match self.queue.next() {
            None => return None,
            Some(id) => id,
        };

        let newly_discovered = self.seen.add(None, id);
        debug_assert!(!newly_discovered,
                      "should have already seen anything we get out of our queue");
        debug_assert!(self.ctx.resolve_item_fallible(id).is_some(),
                      "should only get IDs of actual items in our context during traversal");

        self.currently_traversing = Some(id);
        id.trace(self.ctx, self, &());
        self.currently_traversing = None;

        Some(id)
    }
}

/// An iterator to find any dangling items.
///
/// See `BindgenContext::assert_no_dangling_item_traversal` for more
/// information.
pub type AssertNoDanglingItemsTraversal<'ctx, 'gen> =
    ItemTraversal<'ctx,
                  'gen,
                  Paths<'ctx, 'gen>,
                  VecDeque<ItemId>,
                  fn(Edge) -> bool>;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[allow(dead_code)]
    fn traversal_predicate_is_object_safe() {
        // This should compile only if TraversalPredicate is object safe.
        fn takes_by_trait_object(_: &TraversalPredicate) {}
    }
}
