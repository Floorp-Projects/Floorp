//! Determining which types for which we cannot emit `#[derive(PartialEq,
//! PartialOrd)]`.

use super::{ConstrainResult, MonotoneFramework, generate_dependencies};
use ir::comp::CompKind;
use ir::context::{BindgenContext, ItemId};
use ir::derive::{CanTriviallyDerivePartialEqOrPartialOrd, CanDerive};
use ir::item::{Item, IsOpaque};
use ir::traversal::{EdgeKind, Trace};
use ir::ty::RUST_DERIVE_IN_ARRAY_LIMIT;
use ir::ty::{TypeKind, Type};
use std::collections::HashMap;
use std::collections::hash_map::Entry;

/// An analysis that finds for each IR item whether `PartialEq`/`PartialOrd`
/// cannot be derived.
///
/// We use the monotone constraint function
/// `cannot_derive_partialeq_or_partialord`, defined as follows:
///
/// * If T is Opaque and layout of the type is known, get this layout as opaque
///   type and check whether it can be derived using trivial checks.
///
/// * If T is Array type, `PartialEq` or partialord cannot be derived if the array is incomplete, if the length of
///   the array is larger than the limit, or the type of data the array contains cannot derive
///   `PartialEq`/`PartialOrd`.
///
/// * If T is a type alias, a templated alias or an indirection to another type,
///   `PartialEq`/`PartialOrd` cannot be derived if the type T refers to cannot be
///   derived `PartialEq`/`PartialOrd`.
///
/// * If T is a compound type, `PartialEq`/`PartialOrd` cannot be derived if any
///   of its base member or field cannot be derived `PartialEq`/`PartialOrd`.
///
/// * If T is a pointer, T cannot be derived `PartialEq`/`PartialOrd` if T is a
///   function pointer and the function signature cannot be derived
///   `PartialEq`/`PartialOrd`.
///
/// * If T is an instantiation of an abstract template definition, T cannot be
///   derived `PartialEq`/`PartialOrd` if any of the template arguments or
///   template definition cannot derive `PartialEq`/`PartialOrd`.
#[derive(Debug, Clone)]
pub struct CannotDerivePartialEqOrPartialOrd<'ctx> {
    ctx: &'ctx BindgenContext,

    // The incremental result of this analysis's computation. 
    // Contains information whether particular item can derive `PartialEq`/`PartialOrd`.
    can_derive_partialeq_or_partialord: HashMap<ItemId, CanDerive>,

    // Dependencies saying that if a key ItemId has been inserted into the
    // `cannot_derive_partialeq_or_partialord` set, then each of the ids
    // in Vec<ItemId> need to be considered again.
    //
    // This is a subset of the natural IR graph with reversed edges, where we
    // only include the edges from the IR graph that can affect whether a type
    // can derive `PartialEq`/`PartialOrd`.
    dependencies: HashMap<ItemId, Vec<ItemId>>,
}

impl<'ctx> CannotDerivePartialEqOrPartialOrd<'ctx> {
    fn consider_edge(kind: EdgeKind) -> bool {
        match kind {
            // These are the only edges that can affect whether a type can derive
            // `PartialEq`/`PartialOrd`.
            EdgeKind::BaseMember |
            EdgeKind::Field |
            EdgeKind::TypeReference |
            EdgeKind::VarType |
            EdgeKind::TemplateArgument |
            EdgeKind::TemplateDeclaration |
            EdgeKind::TemplateParameterDefinition => true,

            EdgeKind::Constructor |
            EdgeKind::Destructor |
            EdgeKind::FunctionReturn |
            EdgeKind::FunctionParameter |
            EdgeKind::InnerType |
            EdgeKind::InnerVar |
            EdgeKind::Method => false,
            EdgeKind::Generic => false,
        }
    }

    fn insert<Id: Into<ItemId>>(
        &mut self,
        id: Id,
        can_derive: CanDerive,
    ) -> ConstrainResult {
        let id = id.into();
        trace!("inserting {:?} can_derive={:?}", id, can_derive);

        if let CanDerive::Yes = can_derive {
            return ConstrainResult::Same;
        }

        match self.can_derive_partialeq_or_partialord.entry(id) {
            Entry::Occupied(mut entry) => if *entry.get() < can_derive {
                entry.insert(can_derive);
                ConstrainResult::Changed
            } else {
                ConstrainResult::Same
            },
            Entry::Vacant(entry) => {
                entry.insert(can_derive);
                ConstrainResult::Changed
            }
        }
    }

    fn constrain_type(&mut self, item: &Item, ty: &Type) -> CanDerive {
        if !self.ctx.whitelisted_items().contains(&item.id()) {
            return CanDerive::No;
        }

        if self.ctx.no_partialeq_by_name(&item) {
            return CanDerive::No;
        }

        trace!("ty: {:?}", ty);
        if item.is_opaque(self.ctx, &()) {
            if ty.is_union()
                && self.ctx.options().rust_features().untagged_union
            {
                trace!(
                    "    cannot derive `PartialEq`/`PartialOrd` for Rust unions"
                );
                return CanDerive::No;
            }

            let layout_can_derive = ty.layout(self.ctx)
                .map_or(CanDerive::Yes, |l| {
                    l.opaque().can_trivially_derive_partialeq_or_partialord()
                });

            match layout_can_derive {
                CanDerive::Yes => {
                    trace!(
                        "    we can trivially derive `PartialEq`/`PartialOrd` for the layout"
                    );
                }
                _ => {
                    trace!(
                        "    we cannot derive `PartialEq`/`PartialOrd` for the layout"
                    );
                }
            };
            return layout_can_derive;
        }

        match *ty.kind() {
            // Handle the simple cases. These can derive partialeq without further
            // information.
            TypeKind::Void |
            TypeKind::NullPtr |
            TypeKind::Int(..) |
            TypeKind::Complex(..) |
            TypeKind::Float(..) |
            TypeKind::Enum(..) |
            TypeKind::TypeParam |
            TypeKind::UnresolvedTypeRef(..) |
            TypeKind::BlockPointer |
            TypeKind::Reference(..) |
            TypeKind::ObjCInterface(..) |
            TypeKind::ObjCId |
            TypeKind::ObjCSel => {
                trace!(
                    "    simple type that can always derive `PartialEq`/`PartialOrd`"
                );
                return CanDerive::Yes;
            }

            TypeKind::Array(t, len) => {
                let inner_type = self.can_derive_partialeq_or_partialord
                    .get(&t.into())
                    .cloned()
                    .unwrap_or(CanDerive::Yes);
                if inner_type != CanDerive::Yes {
                    trace!(
                        "    arrays of T for which we cannot derive `PartialEq`/`PartialOrd` \
                         also cannot derive `PartialEq`/`PartialOrd`"
                    );
                    return CanDerive::No;
                }

                if len == 0 {
                    trace!(
                        "    cannot derive `PartialEq`/`PartialOrd` for incomplete arrays"
                    );
                    return CanDerive::No;
                } else if len <= RUST_DERIVE_IN_ARRAY_LIMIT {
                    trace!(
                        "    array is small enough to derive `PartialEq`/`PartialOrd`"
                    );
                    return CanDerive::Yes;
                } else {
                    trace!(
                        "    array is too large to derive `PartialEq`/`PartialOrd`"
                    );
                    return CanDerive::ArrayTooLarge;
                }
            }

            TypeKind::Pointer(inner) => {
                let inner_type =
                    self.ctx.resolve_type(inner).canonical_type(self.ctx);
                if let TypeKind::Function(ref sig) = *inner_type.kind() {
                    if sig.can_trivially_derive_partialeq_or_partialord()
                        != CanDerive::Yes
                    {
                        trace!(
                            "    function pointer that can't trivially derive `PartialEq`/`PartialOrd`"
                        );
                        return CanDerive::No;
                    }
                }
                trace!("    pointers can derive `PartialEq`/`PartialOrd`");
                return CanDerive::Yes;
            }

            TypeKind::Function(ref sig) => {
                if sig.can_trivially_derive_partialeq_or_partialord()
                    != CanDerive::Yes
                {
                    trace!(
                        "    function that can't trivially derive `PartialEq`/`PartialOrd`"
                    );
                    return CanDerive::No;
                }
                trace!("    function can derive `PartialEq`/`PartialOrd`");
                return CanDerive::Yes;
            }

            TypeKind::Comp(ref info) => {
                assert!(
                    !info.has_non_type_template_params(),
                    "The early ty.is_opaque check should have handled this case"
                );

                if info.is_forward_declaration() {
                    trace!("    cannot derive for forward decls");
                    return CanDerive::No;
                }

                if info.kind() == CompKind::Union {
                    if self.ctx.options().rust_features().untagged_union {
                        trace!(
                            "    cannot derive `PartialEq`/`PartialOrd` for Rust unions"
                        );
                        return CanDerive::No;
                    }

                    let layout_can_derive =
                        ty.layout(self.ctx).map_or(CanDerive::Yes, |l| {
                            l.opaque()
                                .can_trivially_derive_partialeq_or_partialord()
                        });
                    match layout_can_derive {
                        CanDerive::Yes => {
                            trace!(
                                "    union layout can trivially derive `PartialEq`/`PartialOrd`"
                            );
                        }
                        _ => {
                            trace!(
                                "    union layout cannot derive `PartialEq`/`PartialOrd`"
                            );
                        }
                    };
                    return layout_can_derive;
                }
                return self.constrain_join(item);
            }

            TypeKind::ResolvedTypeRef(..) |
            TypeKind::TemplateAlias(..) |
            TypeKind::Alias(..) |
            TypeKind::TemplateInstantiation(..) => {
                return self.constrain_join(item);
            }

            TypeKind::Opaque => unreachable!(
                "The early ty.is_opaque check should have handled this case"
            ),
        }
    }

    fn constrain_join(&mut self, item: &Item) -> CanDerive {
        let mut candidate = CanDerive::Yes;

        item.trace(
            self.ctx,
            &mut |sub_id, edge_kind| {
                // Ignore ourselves, since union with ourself is a
                // no-op. Ignore edges that aren't relevant to the
                // analysis.
                if sub_id == item.id() || !Self::consider_edge(edge_kind) {
                    return;
                }

                let can_derive = self.can_derive_partialeq_or_partialord
                    .get(&sub_id)
                    .cloned()
                    .unwrap_or_default();

                candidate |= can_derive;
            },
            &(),
        );

        candidate
    }
}

impl<'ctx> MonotoneFramework for CannotDerivePartialEqOrPartialOrd<'ctx> {
    type Node = ItemId;
    type Extra = &'ctx BindgenContext;
    type Output = HashMap<ItemId, CanDerive>;

    fn new(
        ctx: &'ctx BindgenContext,
    ) -> CannotDerivePartialEqOrPartialOrd<'ctx> {
        let can_derive_partialeq_or_partialord = HashMap::new();
        let dependencies = generate_dependencies(ctx, Self::consider_edge);

        CannotDerivePartialEqOrPartialOrd {
            ctx,
            can_derive_partialeq_or_partialord,
            dependencies,
        }
    }

    fn initial_worklist(&self) -> Vec<ItemId> {
        // The transitive closure of all whitelisted items, including explicitly
        // blacklisted items.
        self.ctx
            .whitelisted_items()
            .iter()
            .cloned()
            .flat_map(|i| {
                let mut reachable = vec![i];
                i.trace(
                    self.ctx,
                    &mut |s, _| {
                        reachable.push(s);
                    },
                    &(),
                );
                reachable
            })
            .collect()
    }

    fn constrain(&mut self, id: ItemId) -> ConstrainResult {
        trace!("constrain: {:?}", id);

        if let Some(CanDerive::No) =
            self.can_derive_partialeq_or_partialord.get(&id).cloned()
        {
            trace!(
                "    already know it cannot derive `PartialEq`/`PartialOrd`"
            );
            return ConstrainResult::Same;
        }

        let item = self.ctx.resolve_item(id);
        let can_derive = match item.as_type() {
            Some(ty) => {
                let mut can_derive = self.constrain_type(item, ty);
                if let CanDerive::Yes = can_derive {
                    if ty.layout(self.ctx)
                        .map_or(false, |l| l.align > RUST_DERIVE_IN_ARRAY_LIMIT)
                    {
                        // We have to be conservative: the struct *could* have enough
                        // padding that we emit an array that is longer than
                        // `RUST_DERIVE_IN_ARRAY_LIMIT`. If we moved padding calculations
                        // into the IR and computed them before this analysis, then we could
                        // be precise rather than conservative here.
                        can_derive = CanDerive::ArrayTooLarge;
                    }
                }
                can_derive
            }
            None => self.constrain_join(item),
        };

        self.insert(id, can_derive)
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

impl<'ctx> From<CannotDerivePartialEqOrPartialOrd<'ctx>>
    for HashMap<ItemId, CanDerive> {
    fn from(analysis: CannotDerivePartialEqOrPartialOrd<'ctx>) -> Self {
        extra_assert!(
            analysis
                .can_derive_partialeq_or_partialord
                .values()
                .all(|v| { *v != CanDerive::Yes })
        );

        analysis.can_derive_partialeq_or_partialord
    }
}
