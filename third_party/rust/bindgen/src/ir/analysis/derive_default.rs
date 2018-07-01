//! Determining which types for which we can emit `#[derive(Default)]`.

use super::{ConstrainResult, HasVtable, MonotoneFramework};
use ir::comp::CompKind;
use ir::comp::Field;
use ir::comp::FieldMethods;
use ir::context::{BindgenContext, ItemId};
use ir::derive::CanTriviallyDeriveDefault;
use ir::item::IsOpaque;
use ir::item::ItemSet;
use ir::traversal::EdgeKind;
use ir::traversal::Trace;
use ir::ty::RUST_DERIVE_IN_ARRAY_LIMIT;
use ir::ty::TypeKind;
use std::collections::HashMap;
use std::collections::HashSet;

/// An analysis that finds for each IR item whether default cannot be derived.
///
/// We use the monotone constraint function `cannot_derive_default`, defined as
/// follows:
///
/// * If T is Opaque and layout of the type is known, get this layout as opaque
///   type and check whether it can be derived using trivial checks.
/// * If T is Array type, default cannot be derived if the length of the array is
///   larger than the limit or the type of data the array contains cannot derive
///   default.
/// * If T is a type alias, a templated alias or an indirection to another type,
///   default cannot be derived if the type T refers to cannot be derived default.
/// * If T is a compound type, default cannot be derived if any of its base member
///   or field cannot be derived default.
#[derive(Debug, Clone)]
pub struct CannotDeriveDefault<'ctx> {
    ctx: &'ctx BindgenContext,

    // The incremental result of this analysis's computation. Everything in this
    // set cannot derive default.
    cannot_derive_default: HashSet<ItemId>,

    // Dependencies saying that if a key ItemId has been inserted into the
    // `cannot_derive_default` set, then each of the ids in Vec<ItemId> need to be
    // considered again.
    //
    // This is a subset of the natural IR graph with reversed edges, where we
    // only include the edges from the IR graph that can affect whether a type
    // can derive default or not.
    dependencies: HashMap<ItemId, Vec<ItemId>>,
}

impl<'ctx> CannotDeriveDefault<'ctx> {
    fn consider_edge(kind: EdgeKind) -> bool {
        match kind {
            // These are the only edges that can affect whether a type can derive
            // default or not.
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

    fn insert<Id: Into<ItemId>>(&mut self, id: Id) -> ConstrainResult {
        let id = id.into();
        trace!("inserting {:?} into the cannot_derive_default set", id);

        let was_not_already_in_set = self.cannot_derive_default.insert(id);
        assert!(
            was_not_already_in_set,
            "We shouldn't try and insert {:?} twice because if it was \
             already in the set, `constrain` should have exited early.",
            id
        );

        ConstrainResult::Changed
    }

    fn is_not_default<Id: Into<ItemId>>(&self, id: Id) -> bool {
        let id = id.into();
        self.cannot_derive_default.contains(&id) ||
            !self.ctx.whitelisted_items().contains(&id)
    }
}

impl<'ctx> MonotoneFramework for CannotDeriveDefault<'ctx> {
    type Node = ItemId;
    type Extra = &'ctx BindgenContext;
    type Output = HashSet<ItemId>;

    fn new(ctx: &'ctx BindgenContext) -> CannotDeriveDefault<'ctx> {
        let mut dependencies = HashMap::new();
        let cannot_derive_default = HashSet::new();

        let whitelisted_items: HashSet<_> =
            ctx.whitelisted_items().iter().cloned().collect();

        let whitelisted_and_blacklisted_items: ItemSet = whitelisted_items
            .iter()
            .cloned()
            .flat_map(|i| {
                let mut reachable = vec![i];
                i.trace(ctx, &mut |s, _| { reachable.push(s); }, &());
                reachable
            })
            .collect();

        for item in whitelisted_and_blacklisted_items {
            dependencies.entry(item).or_insert(vec![]);

            {
                // We reverse our natural IR graph edges to find dependencies
                // between nodes.
                item.trace(
                    ctx,
                    &mut |sub_item: ItemId, edge_kind| {
                        if ctx.whitelisted_items().contains(&sub_item) &&
                            Self::consider_edge(edge_kind)
                        {
                            dependencies
                                .entry(sub_item)
                                .or_insert(vec![])
                                .push(item);
                        }
                    },
                    &(),
                );
            }
        }

        CannotDeriveDefault {
            ctx,
            cannot_derive_default,
            dependencies,
        }
    }

    fn initial_worklist(&self) -> Vec<ItemId> {
        self.ctx.whitelisted_items().iter().cloned().collect()
    }

    fn constrain(&mut self, id: ItemId) -> ConstrainResult {
        trace!("constrain: {:?}", id);

        if self.cannot_derive_default.contains(&id) {
            trace!("    already know it cannot derive Default");
            return ConstrainResult::Same;
        }

        if !self.ctx.whitelisted_items().contains(&id) {
            trace!("    blacklisted items pessimistically cannot derive Default");
            return ConstrainResult::Same;
        }

        let item = self.ctx.resolve_item(id);
        let ty = match item.as_type() {
            Some(ty) => ty,
            None => {
                trace!("    not a type; ignoring");
                return ConstrainResult::Same;
            }
        };

        if item.is_opaque(self.ctx, &()) {
            let layout_can_derive = ty.layout(self.ctx).map_or(true, |l| {
                l.opaque().can_trivially_derive_default()
            });
            return if layout_can_derive &&
                      !(ty.is_union() &&
                        self.ctx.options().rust_features().untagged_union) {
                trace!("    we can trivially derive Default for the layout");
                ConstrainResult::Same
            } else {
                trace!("    we cannot derive Default for the layout");
                self.insert(id)
            };
        }

        if ty.layout(self.ctx).map_or(false, |l| {
            l.align > RUST_DERIVE_IN_ARRAY_LIMIT
        })
        {
            // We have to be conservative: the struct *could* have enough
            // padding that we emit an array that is longer than
            // `RUST_DERIVE_IN_ARRAY_LIMIT`. If we moved padding calculations
            // into the IR and computed them before this analysis, then we could
            // be precise rather than conservative here.
            return self.insert(id);
        }

        match *ty.kind() {
            // Handle the simple cases. These can derive Default without further
            // information.
            TypeKind::Function(..) |
            TypeKind::Int(..) |
            TypeKind::Float(..) |
            TypeKind::Complex(..) => {
                trace!("    simple type that can always derive Default");
                ConstrainResult::Same
            }

            TypeKind::Void |
            TypeKind::TypeParam |
            TypeKind::Reference(..) |
            TypeKind::NullPtr |
            TypeKind::Pointer(..) |
            TypeKind::BlockPointer |
            TypeKind::ObjCId |
            TypeKind::ObjCSel |
            TypeKind::ObjCInterface(..) |
            TypeKind::Enum(..) => {
                trace!("    types that always cannot derive Default");
                self.insert(id)
            }

            TypeKind::Array(t, len) => {
                if self.is_not_default(t) {
                    trace!(
                        "    arrays of T for which we cannot derive Default \
                            also cannot derive Default"
                    );
                    return self.insert(id);
                }

                if len <= RUST_DERIVE_IN_ARRAY_LIMIT {
                    trace!("    array is small enough to derive Default");
                    ConstrainResult::Same
                } else {
                    trace!("    array is too large to derive Default");
                    self.insert(id)
                }
            }

            TypeKind::ResolvedTypeRef(t) |
            TypeKind::TemplateAlias(t, _) |
            TypeKind::Alias(t) => {
                if self.is_not_default(t) {
                    trace!(
                        "    aliases and type refs to T which cannot derive \
                            Default also cannot derive Default"
                    );
                    self.insert(id)
                } else {
                    trace!(
                        "    aliases and type refs to T which can derive \
                            Default can also derive Default"
                    );
                    ConstrainResult::Same
                }
            }

            TypeKind::Comp(ref info) => {
                assert!(
                    !info.has_non_type_template_params(),
                    "The early ty.is_opaque check should have handled this case"
                );

                if info.is_forward_declaration() {
                    trace!("    cannot derive Default for forward decls");
                    return self.insert(id);
                }

                if info.kind() == CompKind::Union {
                    if self.ctx.options().rust_features().untagged_union {
                        trace!("    cannot derive Default for Rust unions");
                        return self.insert(id);
                    }

                    if ty.layout(self.ctx).map_or(true, |l| {
                        l.opaque().can_trivially_derive_default()
                    })
                    {
                        trace!("    union layout can trivially derive Default");
                        return ConstrainResult::Same;
                    } else {
                        trace!("    union layout cannot derive Default");
                        return self.insert(id);
                    }
                }

                if item.has_vtable(self.ctx) {
                    trace!("    comp with vtable cannot derive Default");
                    return self.insert(id);
                }

                let bases_cannot_derive =
                    info.base_members().iter().any(|base| {
                        !self.ctx.whitelisted_items().contains(&base.ty.into()) ||
                            self.is_not_default(base.ty)
                    });
                if bases_cannot_derive {
                    trace!(
                        "    base members cannot derive Default, so we can't \
                            either"
                    );
                    return self.insert(id);
                }

                let fields_cannot_derive =
                    info.fields().iter().any(|f| match *f {
                        Field::DataMember(ref data) => {
                            !self.ctx.whitelisted_items().contains(
                                &data.ty().into(),
                            ) ||
                                self.is_not_default(data.ty())
                        }
                        Field::Bitfields(ref bfu) => {
                            if bfu.layout().align > RUST_DERIVE_IN_ARRAY_LIMIT {
                                trace!(
                                    "   we cannot derive Default for a bitfield larger then \
                                        the limit"
                                );
                                return true;
                            }

                            bfu.bitfields().iter().any(|b| {
                                !self.ctx.whitelisted_items().contains(
                                    &b.ty().into(),
                                ) ||
                                    self.is_not_default(b.ty())
                            })
                        }
                    });
                if fields_cannot_derive {
                    trace!(
                        "    fields cannot derive Default, so we can't either"
                    );
                    return self.insert(id);
                }

                trace!("    comp can derive Default");
                ConstrainResult::Same
            }

            TypeKind::TemplateInstantiation(ref template) => {
                let args_cannot_derive =
                    template.template_arguments().iter().any(|arg| {
                        self.is_not_default(*arg)
                    });
                if args_cannot_derive {
                    trace!(
                        "    template args cannot derive Default, so \
                         insantiation can't either"
                    );
                    return self.insert(id);
                }

                assert!(
                    !template.template_definition().is_opaque(self.ctx, &()),
                    "The early ty.is_opaque check should have handled this case"
                );
                let def_cannot_derive =
                    self.is_not_default(template.template_definition());
                if def_cannot_derive {
                    trace!(
                        "    template definition cannot derive Default, so \
                         insantiation can't either"
                    );
                    return self.insert(id);
                }

                trace!("    template instantiation can derive Default");
                ConstrainResult::Same
            }

            TypeKind::Opaque => {
                unreachable!(
                    "The early ty.is_opaque check should have handled this case"
                )
            }

            TypeKind::UnresolvedTypeRef(..) => {
                unreachable!(
                    "Type with unresolved type ref can't reach derive default"
                )
            }
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

impl<'ctx> From<CannotDeriveDefault<'ctx>> for HashSet<ItemId> {
    fn from(analysis: CannotDeriveDefault<'ctx>) -> Self {
        analysis.cannot_derive_default
    }
}
