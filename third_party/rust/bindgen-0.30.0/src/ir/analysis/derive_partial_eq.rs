//! Determining which types for which we can emit `#[derive(PartialEq)]`.

use super::{ConstrainResult, MonotoneFramework, generate_dependencies};
use ir::comp::CompKind;
use ir::comp::Field;
use ir::comp::FieldMethods;
use ir::context::{BindgenContext, ItemId};
use ir::derive::CanTriviallyDerivePartialEq;
use ir::item::IsOpaque;
use ir::traversal::EdgeKind;
use ir::ty::RUST_DERIVE_IN_ARRAY_LIMIT;
use ir::ty::TypeKind;
use std::collections::HashMap;
use std::collections::HashSet;

/// An analysis that finds for each IR item whether partialeq cannot be derived.
///
/// We use the monotone constraint function `cannot_derive_partial_eq`, defined as
/// follows:
///
/// * If T is Opaque and layout of the type is known, get this layout as opaque
///   type and check whether it can be derived using trivial checks.
/// * If T is Array type, partialeq cannot be derived if the length of the array is
///   larger than the limit or the type of data the array contains cannot derive
///   partialeq.
/// * If T is a type alias, a templated alias or an indirection to another type,
///   partialeq cannot be derived if the type T refers to cannot be derived partialeq.
/// * If T is a compound type, partialeq cannot be derived if any of its base member
///   or field cannot be derived partialeq.
/// * If T is a pointer, T cannot be derived partialeq if T is a function pointer
///   and the function signature cannot be derived partialeq.
/// * If T is an instantiation of an abstract template definition, T cannot be
///   derived partialeq if any of the template arguments or template definition
///   cannot derive partialeq.
#[derive(Debug, Clone)]
pub struct CannotDerivePartialEq<'ctx, 'gen>
where
    'gen: 'ctx,
{
    ctx: &'ctx BindgenContext<'gen>,

    // The incremental result of this analysis's computation. Everything in this
    // set cannot derive partialeq.
    cannot_derive_partialeq: HashSet<ItemId>,

    // Dependencies saying that if a key ItemId has been inserted into the
    // `cannot_derive_partialeq` set, then each of the ids in Vec<ItemId> need to be
    // considered again.
    //
    // This is a subset of the natural IR graph with reversed edges, where we
    // only include the edges from the IR graph that can affect whether a type
    // can derive partialeq or not.
    dependencies: HashMap<ItemId, Vec<ItemId>>,
}

impl<'ctx, 'gen> CannotDerivePartialEq<'ctx, 'gen> {
    fn consider_edge(kind: EdgeKind) -> bool {
        match kind {
            // These are the only edges that can affect whether a type can derive
            // partialeq or not.
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

    fn insert(&mut self, id: ItemId) -> ConstrainResult {
        trace!("inserting {:?} into the cannot_derive_partialeq set", id);

        let was_not_already_in_set = self.cannot_derive_partialeq.insert(id);
        assert!(
            was_not_already_in_set,
            "We shouldn't try and insert {:?} twice because if it was \
             already in the set, `constrain` should have exited early.",
            id
        );

        ConstrainResult::Changed
    }
}

impl<'ctx, 'gen> MonotoneFramework for CannotDerivePartialEq<'ctx, 'gen> {
    type Node = ItemId;
    type Extra = &'ctx BindgenContext<'gen>;
    type Output = HashSet<ItemId>;

    fn new(
        ctx: &'ctx BindgenContext<'gen>,
    ) -> CannotDerivePartialEq<'ctx, 'gen> {
        let cannot_derive_partialeq = HashSet::new();
        let dependencies = generate_dependencies(ctx, Self::consider_edge);

        CannotDerivePartialEq {
            ctx,
            cannot_derive_partialeq,
            dependencies,
        }
    }

    fn initial_worklist(&self) -> Vec<ItemId> {
        self.ctx.whitelisted_items().iter().cloned().collect()
    }

    fn constrain(&mut self, id: ItemId) -> ConstrainResult {
        trace!("constrain: {:?}", id);

        if self.cannot_derive_partialeq.contains(&id) {
            trace!("    already know it cannot derive PartialEq");
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

        trace!("ty: {:?}", ty);
        if item.is_opaque(self.ctx, &()) {
            let layout_can_derive = ty.layout(self.ctx).map_or(true, |l| {
                l.opaque().can_trivially_derive_partialeq()
            });
            return if layout_can_derive {
                trace!("    we can trivially derive PartialEq for the layout");
                ConstrainResult::Same
            } else {
                trace!("    we cannot derive PartialEq for the layout");
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
                trace!("    simple type that can always derive PartialEq");
                ConstrainResult::Same
            }

            TypeKind::Array(t, len) => {
                if self.cannot_derive_partialeq.contains(&t) {
                    trace!(
                        "    arrays of T for which we cannot derive PartialEq \
                            also cannot derive PartialEq"
                    );
                    return self.insert(id);
                }

                if len <= RUST_DERIVE_IN_ARRAY_LIMIT {
                    trace!("    array is small enough to derive PartialEq");
                    ConstrainResult::Same
                } else {
                    trace!("    array is too large to derive PartialEq");
                    self.insert(id)
                }
            }

            TypeKind::Pointer(inner) => {
                let inner_type =
                    self.ctx.resolve_type(inner).canonical_type(self.ctx);
                if let TypeKind::Function(ref sig) = *inner_type.kind() {
                    if !sig.can_trivially_derive_partialeq() {
                        trace!(
                            "    function pointer that can't trivially derive PartialEq"
                        );
                        return self.insert(id);
                    }
                }
                trace!("    pointers can derive PartialEq");
                ConstrainResult::Same
            }

            TypeKind::Function(ref sig) => {
                if !sig.can_trivially_derive_partialeq() {
                    trace!(
                        "    function that can't trivially derive PartialEq"
                    );
                    return self.insert(id);
                }
                trace!("    function can derive PartialEq");
                ConstrainResult::Same
            }

            TypeKind::ResolvedTypeRef(t) |
            TypeKind::TemplateAlias(t, _) |
            TypeKind::Alias(t) => {
                if self.cannot_derive_partialeq.contains(&t) {
                    trace!(
                        "    aliases and type refs to T which cannot derive \
                            PartialEq also cannot derive PartialEq"
                    );
                    self.insert(id)
                } else {
                    trace!(
                        "    aliases and type refs to T which can derive \
                            PartialEq can also derive PartialEq"
                    );
                    ConstrainResult::Same
                }
            }

            TypeKind::Comp(ref info) => {
                assert!(
                    !info.has_non_type_template_params(),
                    "The early ty.is_opaque check should have handled this case"
                );

                if info.kind() == CompKind::Union {
                    if self.ctx.options().rust_features().untagged_union() {
                        trace!("    cannot derive PartialEq for Rust unions");
                        return self.insert(id);
                    }

                    if ty.layout(self.ctx).map_or(true, |l| {
                        l.opaque().can_trivially_derive_partialeq()
                    })
                    {
                        trace!(
                            "    union layout can trivially derive PartialEq"
                        );
                        return ConstrainResult::Same;
                    } else {
                        trace!("    union layout cannot derive PartialEq");
                        return self.insert(id);
                    }
                }

                let bases_cannot_derive =
                    info.base_members().iter().any(|base| {
                        !self.ctx.whitelisted_items().contains(&base.ty) ||
                            self.cannot_derive_partialeq.contains(&base.ty)
                    });
                if bases_cannot_derive {
                    trace!(
                        "    base members cannot derive PartialEq, so we can't \
                            either"
                    );
                    return self.insert(id);
                }

                let fields_cannot_derive =
                    info.fields().iter().any(|f| match *f {
                        Field::DataMember(ref data) => {
                            !self.ctx.whitelisted_items().contains(
                                &data.ty(),
                            ) ||
                                self.cannot_derive_partialeq.contains(
                                    &data.ty(),
                                )
                        }
                        Field::Bitfields(ref bfu) => {
                            bfu.bitfields().iter().any(|b| {
                                !self.ctx.whitelisted_items().contains(
                                    &b.ty(),
                                ) ||
                                    self.cannot_derive_partialeq.contains(
                                        &b.ty(),
                                    )
                            })
                        }
                    });
                if fields_cannot_derive {
                    trace!(
                        "    fields cannot derive PartialEq, so we can't either"
                    );
                    return self.insert(id);
                }

                trace!("    comp can derive PartialEq");
                ConstrainResult::Same
            }

            TypeKind::TemplateInstantiation(ref template) => {
                let args_cannot_derive =
                    template.template_arguments().iter().any(|arg| {
                        self.cannot_derive_partialeq.contains(&arg)
                    });
                if args_cannot_derive {
                    trace!(
                        "    template args cannot derive PartialEq, so \
                            insantiation can't either"
                    );
                    return self.insert(id);
                }

                assert!(
                    !template.template_definition().is_opaque(self.ctx, &()),
                    "The early ty.is_opaque check should have handled this case"
                );
                let def_cannot_derive = self.cannot_derive_partialeq.contains(
                    &template.template_definition(),
                );
                if def_cannot_derive {
                    trace!(
                        "    template definition cannot derive PartialEq, so \
                            insantiation can't either"
                    );
                    return self.insert(id);
                }

                trace!("    template instantiation can derive PartialEq");
                ConstrainResult::Same
            }

            TypeKind::Opaque => {
                unreachable!(
                    "The early ty.is_opaque check should have handled this case"
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

impl<'ctx, 'gen> From<CannotDerivePartialEq<'ctx, 'gen>> for HashSet<ItemId> {
    fn from(analysis: CannotDerivePartialEq<'ctx, 'gen>) -> Self {
        analysis.cannot_derive_partialeq
    }
}
