//! Lifetime information for types.
#![allow(dead_code)]

use super::IdentBuf;
use crate::ast;
use smallvec::{smallvec, SmallVec};

/// Convenience const representing the number of lifetimes a [`LifetimeEnv`]
/// can hold inline before needing to dynamically allocate.
const INLINE_NUM_LIFETIMES: usize = 4;

// TODO(Quinn): This type is going to mainly be recycled from `ast::LifetimeEnv`.
// Not fully sure how that will look like yet, but the ideas of what this will do
// is basically the same.
#[derive(Debug)]
pub struct LifetimeEnv {
    /// List of named lifetimes in scope of the method, in the form of an
    /// adjacency matrix.
    nodes: SmallVec<[Lifetime; INLINE_NUM_LIFETIMES]>,

    /// The number of named _and_ anonymous lifetimes in the method.
    /// We store the sum since it represents the upper bound on what indices
    /// are in range of the graph. If we make a [`MethodLfetimes`] with
    /// `num_lifetimes` entries, then `TypeLifetime`s that convert into
    /// `MethodLifetime`s will fall into this range, and we'll know that it's
    /// a named lifetime if it's < `nodes.len()`, or that it's an anonymous
    /// lifetime if it's < `num_lifetimes`. Otherwise, we'd have to make a
    /// distinction in `TypeLifetime` about which kind it refers to.
    num_lifetimes: usize,
}

/// A lifetime in a [`LifetimeEnv`], which keeps track of which lifetimes it's
/// longer and shorter than.
#[derive(Debug)]
pub(super) struct Lifetime {
    ident: IdentBuf,
    longer: SmallVec<[MethodLifetime; 2]>,
    shorter: SmallVec<[MethodLifetime; 2]>,
}

impl Lifetime {
    /// Returns a new [`Lifetime`].
    pub(super) fn new(
        ident: IdentBuf,
        longer: SmallVec<[MethodLifetime; 2]>,
        shorter: SmallVec<[MethodLifetime; 2]>,
    ) -> Self {
        Self {
            ident,
            longer,
            shorter,
        }
    }
}

/// Visit subtype lifetimes recursively, keeping track of which have already
/// been visited.
pub struct SubtypeLifetimeVisitor<'lt, F> {
    lifetime_env: &'lt LifetimeEnv,
    visited: SmallVec<[bool; INLINE_NUM_LIFETIMES]>,
    visit_fn: F,
}

impl<'lt, F> SubtypeLifetimeVisitor<'lt, F>
where
    F: FnMut(MethodLifetime),
{
    fn new(lifetime_env: &'lt LifetimeEnv, visit_fn: F) -> Self {
        Self {
            lifetime_env,
            visited: smallvec![false; lifetime_env.nodes.len()],
            visit_fn,
        }
    }

    /// Visit more sublifetimes. This method tracks which lifetimes have already
    /// been visited, and uses this to not visit the same lifetime twice.
    pub fn visit_subtypes(&mut self, method_lifetime: MethodLifetime) {
        if let Some(visited @ false) = self.visited.get_mut(method_lifetime.0) {
            *visited = true;

            (self.visit_fn)(method_lifetime);

            for longer in self.lifetime_env.nodes[method_lifetime.0].longer.iter() {
                self.visit_subtypes(*longer)
            }
        } else {
            debug_assert!(
                method_lifetime.0 > self.lifetime_env.num_lifetimes,
                "method lifetime has an internal index that's not in range of the lifetime env"
            );
        }
    }
}

/// Wrapper type for `TypeLifetime` and `MethodLifetime`, indicating that it may
/// be the `'static` lifetime.
#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub enum MaybeStatic<T> {
    Static,
    NonStatic(T),
}

impl<T> MaybeStatic<T> {
    /// Maps the lifetime, if it's not the `'static` lifetime, to another
    /// non-static lifetime.
    pub(super) fn map_nonstatic<F, R>(self, f: F) -> MaybeStatic<R>
    where
        F: FnOnce(T) -> R,
    {
        match self {
            MaybeStatic::Static => MaybeStatic::Static,
            MaybeStatic::NonStatic(lifetime) => MaybeStatic::NonStatic(f(lifetime)),
        }
    }

    /// Maps the lifetime, if it's not the `'static` lifetime, to a potentially
    /// static lifetime.
    pub(super) fn flat_map_nonstatic<R, F>(self, f: F) -> MaybeStatic<R>
    where
        F: FnOnce(T) -> MaybeStatic<R>,
    {
        match self {
            MaybeStatic::Static => MaybeStatic::Static,
            MaybeStatic::NonStatic(lifetime) => f(lifetime),
        }
    }
}

/// A lifetime that exists as part of a type signature.
///
/// This type can be mapped to a [`MethodLifetime`] by using the
/// [`TypeLifetime::as_method_lifetime`] method.
#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub struct TypeLifetime(usize);

/// A set of lifetimes that exist as generic arguments on [`StructPath`]s,
/// [`OutStructPath`]s, and [`OpaquePath`]s.
///
/// By itself, `TypeLifetimes` isn't very useful. However, it can be combined with
/// a [`MethodLifetimes`] using [`TypeLifetimes::as_method_lifetimes`] to get the lifetimes
/// in the scope of a method it appears in.
///
/// [`StructPath`]: super::StructPath
/// [`OutStructPath`]: super::OutStructPath
/// [`OpaquePath`]: super::OpaquePath
#[derive(Clone, Debug)]
pub struct TypeLifetimes {
    indices: SmallVec<[MaybeStatic<TypeLifetime>; 2]>,
}

/// A lifetime that exists as part of a method signature, e.g. `'a` or an
/// anonymous lifetime.
///
/// This type is intended to be used as a key into a map to keep track of which
/// borrowed fields depend on which method lifetimes.
#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub struct MethodLifetime(usize);

/// Map a lifetime in a nested struct to the original lifetime defined
/// in the method that it refers to.
pub struct MethodLifetimes {
    indices: SmallVec<[MaybeStatic<MethodLifetime>; 2]>,
}

impl LifetimeEnv {
    /// Returns a new [`LifetimeEnv`].
    pub(super) fn new(
        nodes: SmallVec<[Lifetime; INLINE_NUM_LIFETIMES]>,
        num_lifetimes: usize,
    ) -> Self {
        Self {
            nodes,
            num_lifetimes,
        }
    }

    /// Returns a fresh [`MethodLifetimes`] corresponding to `self`.
    pub fn method_lifetimes(&self) -> MethodLifetimes {
        let indices = (0..self.num_lifetimes)
            .map(|index| MaybeStatic::NonStatic(MethodLifetime(index)))
            .collect();

        MethodLifetimes { indices }
    }

    /// Returns a new [`SubtypeLifetimeVisitor`], which can visit all reachable
    /// lifetimes
    pub fn subtype_lifetimes_visitor<F>(&self, visit_fn: F) -> SubtypeLifetimeVisitor<'_, F>
    where
        F: FnMut(MethodLifetime),
    {
        SubtypeLifetimeVisitor::new(self, visit_fn)
    }
}

impl TypeLifetime {
    /// Returns a [`TypeLifetime`] from its AST counterparts.
    pub(super) fn from_ast(named: &ast::NamedLifetime, lifetime_env: &ast::LifetimeEnv) -> Self {
        let index = lifetime_env
            .id(named)
            .unwrap_or_else(|| panic!("lifetime `{named}` not found in lifetime env"));
        Self(index)
    }

    pub(super) fn new(index: usize) -> Self {
        Self(index)
    }

    /// Returns a new [`MaybeStatic<MethodLifetime>`] representing `self` in the
    /// scope of the method that it appears in.
    ///
    /// For example, if we have some `Foo<'a>` type with a field `&'a Bar`, then
    /// we can call this on the `'a` on the field. If `Foo` was `Foo<'static>`
    /// in the method, then this will return `MaybeStatic::Static`. But if it
    /// was `Foo<'b>`, then this will return `MaybeStatic::NonStatic` containing
    /// the `MethodLifetime` corresponding to `'b`.
    pub fn as_method_lifetime(
        self,
        method_lifetimes: &MethodLifetimes,
    ) -> MaybeStatic<MethodLifetime> {
        method_lifetimes.indices[self.0]
    }
}

impl TypeLifetimes {
    pub(super) fn from_fn<F>(lifetimes: &[ast::Lifetime], lower_fn: F) -> Self
    where
        F: FnMut(&ast::Lifetime) -> MaybeStatic<TypeLifetime>,
    {
        Self {
            indices: lifetimes.iter().map(lower_fn).collect(),
        }
    }

    /// Returns a new [`MethodLifetimes`] representing the lifetimes in the scope
    /// of the method this type appears in.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # struct Alice<'a>(&'a ());
    /// # struct Bob<'b>(&'b ());
    /// struct Foo<'a, 'b> {
    ///     alice: Alice<'a>,
    ///     bob: Bob<'b>,
    /// }
    ///
    /// fn bar<'x, 'y>(arg: Foo<'x, 'y>) {}
    /// ```
    /// Here, `Foo` will have a [`TypeLifetimes`] containing `['a, 'b]`,
    /// and `bar` will have a [`MethodLifetimes`] containing `{'x: 'x, 'y: 'y}`.
    /// When we enter the scope of `Foo` as a type, we use this method to combine
    /// the two to get a new [`MethodLifetimes`] representing the mapping from
    /// lifetimes in `Foo`'s scope to lifetimes in `bar`s scope: `{'a: 'x, 'b: 'y}`.
    ///
    /// This tells us that `arg.alice` has lifetime `'x` in the method, and
    /// that `arg.bob` has lifetime `'y`.
    pub fn as_method_lifetimes(&self, method_lifetimes: &MethodLifetimes) -> MethodLifetimes {
        let indices = self
            .indices
            .iter()
            .map(|maybe_static_lt| {
                maybe_static_lt.flat_map_nonstatic(|lt| lt.as_method_lifetime(method_lifetimes))
            })
            .collect();

        MethodLifetimes { indices }
    }
}

impl MethodLifetime {
    /// Returns a new `MethodLifetime` from an index into a `LifetimeEnv`.
    pub(super) fn new(index: usize) -> Self {
        Self(index)
    }
}

impl MethodLifetimes {
    /// Returns an iterator over the contained [`MethodLifetime`]s.
    pub(super) fn lifetimes(&self) -> impl Iterator<Item = MaybeStatic<MethodLifetime>> + '_ {
        self.indices.iter().copied()
    }
}
