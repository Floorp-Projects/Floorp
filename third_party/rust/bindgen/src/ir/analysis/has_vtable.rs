//! Determining which types has vtable

use super::{ConstrainResult, MonotoneFramework, generate_dependencies};
use ir::context::{BindgenContext, ItemId};
use ir::traversal::EdgeKind;
use ir::ty::TypeKind;
use std::collections::HashMap;
use std::collections::HashSet;

/// An analysis that finds for each IR item whether it has vtable or not
///
/// We use the monotone function `has vtable`, defined as follows:
///
/// * If T is a type alias, a templated alias, an indirection to another type,
///   or a reference of a type, T has vtable if the type T refers to has vtable.
/// * If T is a compound type, T has vtable if we saw a virtual function when
///   parsing it or any of its base member has vtable.
/// * If T is an instantiation of an abstract template definition, T has
///   vtable if template definition has vtable
#[derive(Debug, Clone)]
pub struct HasVtableAnalysis<'ctx, 'gen>
where
    'gen: 'ctx,
{
    ctx: &'ctx BindgenContext<'gen>,

    // The incremental result of this analysis's computation. Everything in this
    // set definitely has a vtable.
    have_vtable: HashSet<ItemId>,

    // Dependencies saying that if a key ItemId has been inserted into the
    // `have_vtable` set, then each of the ids in Vec<ItemId> need to be
    // considered again.
    //
    // This is a subset of the natural IR graph with reversed edges, where we
    // only include the edges from the IR graph that can affect whether a type
    // has a vtable or not.
    dependencies: HashMap<ItemId, Vec<ItemId>>,
}

impl<'ctx, 'gen> HasVtableAnalysis<'ctx, 'gen> {
    fn consider_edge(kind: EdgeKind) -> bool {
        match kind {
            // These are the only edges that can affect whether a type has a
            // vtable or not.
            EdgeKind::TypeReference |
            EdgeKind::BaseMember |
            EdgeKind::TemplateDeclaration => true,
            _ => false,
        }
    }

    fn insert(&mut self, id: ItemId) -> ConstrainResult {
        let was_not_already_in_set = self.have_vtable.insert(id);
        assert!(
            was_not_already_in_set,
            "We shouldn't try and insert {:?} twice because if it was \
             already in the set, `constrain` should have exited early.",
            id
        );
        ConstrainResult::Changed
    }
}

impl<'ctx, 'gen> MonotoneFramework for HasVtableAnalysis<'ctx, 'gen> {
    type Node = ItemId;
    type Extra = &'ctx BindgenContext<'gen>;
    type Output = HashSet<ItemId>;

    fn new(ctx: &'ctx BindgenContext<'gen>) -> HasVtableAnalysis<'ctx, 'gen> {
        let have_vtable = HashSet::new();
        let dependencies = generate_dependencies(ctx, Self::consider_edge);

        HasVtableAnalysis {
            ctx,
            have_vtable,
            dependencies,
        }
    }

    fn initial_worklist(&self) -> Vec<ItemId> {
        self.ctx.whitelisted_items().iter().cloned().collect()
    }

    fn constrain(&mut self, id: ItemId) -> ConstrainResult {
        if self.have_vtable.contains(&id) {
            // We've already computed that this type has a vtable and that can't
            // change.
            return ConstrainResult::Same;
        }

        let item = self.ctx.resolve_item(id);
        let ty = match item.as_type() {
            None => return ConstrainResult::Same,
            Some(ty) => ty,
        };

        // TODO #851: figure out a way to handle deriving from template type parameters.
        match *ty.kind() {
            TypeKind::TemplateAlias(t, _) |
            TypeKind::Alias(t) |
            TypeKind::ResolvedTypeRef(t) |
            TypeKind::Reference(t) => {
                if self.have_vtable.contains(&t) {
                    self.insert(id)
                } else {
                    ConstrainResult::Same
                }
            }

            TypeKind::Comp(ref info) => {
                if info.has_own_virtual_method() {
                    return self.insert(id);
                }
                let bases_has_vtable = info.base_members().iter().any(|base| {
                    self.have_vtable.contains(&base.ty)
                });
                if bases_has_vtable {
                    self.insert(id)
                } else {
                    ConstrainResult::Same
                }
            }

            TypeKind::TemplateInstantiation(ref inst) => {
                if self.have_vtable.contains(&inst.template_definition()) {
                    self.insert(id)
                } else {
                    ConstrainResult::Same
                }
            }

            _ => ConstrainResult::Same,
        }
    }

    fn each_depending_on<F>(&self, id: ItemId, mut f: F)
    where
        F: FnMut(ItemId),
    {
        if let Some(edges) = self.dependencies.get(&id) {
            for item in edges {
                trace!("enqueue {:?} into worklist", item);
                f(*item);
            }
        }
    }
}

impl<'ctx, 'gen> From<HasVtableAnalysis<'ctx, 'gen>> for HashSet<ItemId> {
    fn from(analysis: HasVtableAnalysis<'ctx, 'gen>) -> Self {
        analysis.have_vtable
    }
}

/// A convenience trait for the things for which we might wonder if they have a
/// vtable during codegen.
///
/// This is not for _computing_ whether the thing has a vtable, it is for
/// looking up the results of the HasVtableAnalysis's computations for a
/// specific thing.
pub trait HasVtable {
    /// Return `true` if this thing has vtable, `false` otherwise.
    fn has_vtable(&self, ctx: &BindgenContext) -> bool;
}
