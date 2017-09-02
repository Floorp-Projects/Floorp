//! Determining which types for which we can emit `#[derive(Debug)]`.

use super::{ConstrainResult, MonotoneFramework, generate_dependencies};
use std::collections::HashSet;
use std::collections::HashMap;
use ir::context::{BindgenContext, ItemId};
use ir::item::IsOpaque;
use ir::traversal::EdgeKind;
use ir::ty::RUST_DERIVE_IN_ARRAY_LIMIT;
use ir::ty::TypeKind;
use ir::comp::Field;
use ir::comp::FieldMethods;
use ir::derive::CanTriviallyDeriveDebug;
use ir::comp::CompKind;

/// An analysis that finds for each IR item whether debug cannot be derived.
///
/// We use the monotone constraint function `cannot_derive_debug`, defined as
/// follows:
///
/// * If T is Opaque and layout of the type is known, get this layout as opaque
///   type and check whether it can be derived using trivial checks.
/// * If T is Array type, debug cannot be derived if the length of the array is
///   larger than the limit or the type of data the array contains cannot derive
///   debug.
/// * If T is a type alias, a templated alias or an indirection to another type,
///   debug cannot be derived if the type T refers to cannot be derived debug.
/// * If T is a compound type, debug cannot be derived if any of its base member
///   or field cannot be derived debug.
/// * If T is a pointer, T cannot be derived debug if T is a function pointer
///   and the function signature cannot be derived debug.
/// * If T is an instantiation of an abstract template definition, T cannot be
///   derived debug if any of the template arguments or template definition
///   cannot derive debug.
#[derive(Debug, Clone)]
pub struct CannotDeriveDebug<'ctx, 'gen>
    where 'gen: 'ctx
{
    ctx: &'ctx BindgenContext<'gen>,

    // The incremental result of this analysis's computation. Everything in this
    // set cannot derive debug.
    cannot_derive_debug: HashSet<ItemId>,

    // Dependencies saying that if a key ItemId has been inserted into the
    // `cannot_derive_debug` set, then each of the ids in Vec<ItemId> need to be
    // considered again.
    //
    // This is a subset of the natural IR graph with reversed edges, where we
    // only include the edges from the IR graph that can affect whether a type
    // can derive debug or not.
    dependencies: HashMap<ItemId, Vec<ItemId>>,
}

impl<'ctx, 'gen> CannotDeriveDebug<'ctx, 'gen> {
    fn consider_edge(kind: EdgeKind) -> bool {
        match kind {
            // These are the only edges that can affect whether a type can derive
            // debug or not.
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
        trace!("inserting {:?} into the cannot_derive_debug set", id);

        let was_not_already_in_set = self.cannot_derive_debug.insert(id);
        assert!(
            was_not_already_in_set,
            "We shouldn't try and insert {:?} twice because if it was \
             already in the set, `constrain` should have exited early.",
            id
        );

        ConstrainResult::Changed
    }
}

impl<'ctx, 'gen> MonotoneFramework for CannotDeriveDebug<'ctx, 'gen> {
    type Node = ItemId;
    type Extra = &'ctx BindgenContext<'gen>;
    type Output = HashSet<ItemId>;

    fn new(ctx: &'ctx BindgenContext<'gen>) -> CannotDeriveDebug<'ctx, 'gen> {
        let cannot_derive_debug = HashSet::new();
        let dependencies = generate_dependencies(ctx, Self::consider_edge);

        CannotDeriveDebug {
            ctx,
            cannot_derive_debug,
            dependencies,
        }
    }

    fn initial_worklist(&self) -> Vec<ItemId> {
        self.ctx.whitelisted_items().iter().cloned().collect()
    }

    fn constrain(&mut self, id: ItemId) -> ConstrainResult {
        trace!("constrain: {:?}", id);

        if self.cannot_derive_debug.contains(&id) {
            trace!("    already know it cannot derive Debug");
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

        if ty.is_opaque(self.ctx, item) {
            let layout_can_derive = ty.layout(self.ctx).map_or(true, |l| {
                l.opaque().can_trivially_derive_debug(self.ctx, ())
            });
            return if layout_can_derive {
                trace!("    we can trivially derive Debug for the layout");
                ConstrainResult::Same
            } else {
                trace!("    we cannot derive Debug for the layout");
                self.insert(id)
            };
        }

        if ty.layout(self.ctx).map_or(false, |l| l.align > RUST_DERIVE_IN_ARRAY_LIMIT) {
            // We have to be conservative: the struct *could* have enough
            // padding that we emit an array that is longer than
            // `RUST_DERIVE_IN_ARRAY_LIMIT`. If we moved padding calculations
            // into the IR and computed them before this analysis, then we could
            // be precise rather than conservative here.
            return self.insert(id);
        }

        match *ty.kind() {
            // Handle the simple cases. These can derive debug without further
            // information.
            TypeKind::Void |
            TypeKind::NullPtr |
            TypeKind::Int(..) |
            TypeKind::Float(..) |
            TypeKind::Complex(..) |
            TypeKind::Function(..) |
            TypeKind::Enum(..) |
            TypeKind::Reference(..) |
            TypeKind::BlockPointer |
            TypeKind::Named |
            TypeKind::UnresolvedTypeRef(..) |
            TypeKind::ObjCInterface(..) |
            TypeKind::ObjCId |
            TypeKind::ObjCSel => {
                trace!("    simple type that can always derive Debug");
                ConstrainResult::Same
            }

            TypeKind::Array(t, len) => {
                if self.cannot_derive_debug.contains(&t) {
                    trace!("    arrays of T for which we cannot derive Debug \
                            also cannot derive Debug");
                    return self.insert(id);
                }

                if len <= RUST_DERIVE_IN_ARRAY_LIMIT {
                    trace!("    array is small enough to derive Debug");
                    ConstrainResult::Same
                } else {
                    trace!("    array is too large to derive Debug");
                    self.insert(id)
                }
            }

            TypeKind::ResolvedTypeRef(t) |
            TypeKind::TemplateAlias(t, _) |
            TypeKind::Alias(t) => {
                if self.cannot_derive_debug.contains(&t) {
                    trace!("    aliases and type refs to T which cannot derive \
                            Debug also cannot derive Debug");
                    self.insert(id)
                } else {
                    trace!("    aliases and type refs to T which can derive \
                            Debug can also derive Debug");
                    ConstrainResult::Same
                }
            }

            TypeKind::Comp(ref info) => {
                assert!(
                    !info.has_non_type_template_params(),
                    "The early ty.is_opaque check should have handled this case"
                );

                if info.kind() == CompKind::Union {
                    if self.ctx.options().unstable_rust {
                        trace!("    cannot derive Debug for Rust unions");
                        return self.insert(id);
                    }

                    if ty.layout(self.ctx)
                        .map_or(true,
                                |l| l.opaque().can_trivially_derive_debug(self.ctx, ())) {
                        trace!("    union layout can trivially derive Debug");
                        return ConstrainResult::Same;
                    } else {
                        trace!("    union layout cannot derive Debug");
                        return self.insert(id);
                    }
                }

                let bases_cannot_derive = info.base_members()
                    .iter()
                    .any(|base| self.cannot_derive_debug.contains(&base.ty));
                if bases_cannot_derive {
                    trace!("    base members cannot derive Debug, so we can't \
                            either");
                    return self.insert(id);
                }

                let fields_cannot_derive = info.fields()
                    .iter()
                    .any(|f| {
                        match *f {
                            Field::DataMember(ref data) => {
                                self.cannot_derive_debug.contains(&data.ty())
                            }
                            Field::Bitfields(ref bfu) => {
                                bfu.bitfields()
                                    .iter().any(|b| {
                                        self.cannot_derive_debug.contains(&b.ty())
                                    })
                            }
                        }
                    });
                if fields_cannot_derive {
                    trace!("    fields cannot derive Debug, so we can't either");
                    return self.insert(id);
                }

                trace!("    comp can derive Debug");
                ConstrainResult::Same
            }

            TypeKind::Pointer(inner) => {
                let inner_type = self.ctx.resolve_type(inner).canonical_type(self.ctx);
                if let TypeKind::Function(ref sig) = *inner_type.kind() {
                    if !sig.can_trivially_derive_debug(&self.ctx, ()) {
                        trace!("    function pointer that can't trivially derive Debug");
                        return self.insert(id);
                    }
                }
                trace!("    pointers can derive Debug");
                ConstrainResult::Same
            }

            TypeKind::TemplateInstantiation(ref template) => {
                let args_cannot_derive = template.template_arguments()
                    .iter()
                    .any(|arg| self.cannot_derive_debug.contains(&arg));
                if args_cannot_derive {
                    trace!("    template args cannot derive Debug, so \
                            insantiation can't either");
                    return self.insert(id);
                }

                assert!(
                    !template.template_definition().is_opaque(self.ctx, &()),
                    "The early ty.is_opaque check should have handled this case"
                );
                let def_cannot_derive = self.cannot_derive_debug
                    .contains(&template.template_definition());
                if def_cannot_derive {
                    trace!("    template definition cannot derive Debug, so \
                            insantiation can't either");
                    return self.insert(id);
                }

                trace!("    template instantiation can derive Debug");
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
        where F: FnMut(ItemId),
    {
        if let Some(edges) = self.dependencies.get(&id) {
            for item in edges {
                trace!("enqueue {:?} into worklist", item);
                f(*item);
            }
        }
    }
}

impl<'ctx, 'gen> From<CannotDeriveDebug<'ctx, 'gen>> for HashSet<ItemId> {
    fn from(analysis: CannotDeriveDebug<'ctx, 'gen>) -> Self {
        analysis.cannot_derive_debug
    }
}
