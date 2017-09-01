//! Determining which types for which we can emit `#[derive(Copy)]`.

use super::{ConstrainResult, MonotoneFramework, generate_dependencies};
use ir::comp::CompKind;
use ir::comp::Field;
use ir::comp::FieldMethods;
use ir::context::{BindgenContext, ItemId};
use ir::derive::CanTriviallyDeriveCopy;
use ir::item::IsOpaque;
use ir::template::TemplateParameters;
use ir::traversal::EdgeKind;
use ir::ty::TypeKind;
use std::collections::HashMap;
use std::collections::HashSet;

/// An analysis that finds for each IR item whether copy cannot be derived.
///
/// We use the monotone constraint function `cannot_derive_copy`, defined as
/// follows:
///
/// * If T is Opaque and layout of the type is known, get this layout as opaque
///   type and check whether it can be derived using trivial checks.
/// * If T is Array type, copy cannot be derived if the length of the array is
///   larger than the limit or the type of data the array contains cannot derive
///   copy.
/// * If T is a type alias, a templated alias or an indirection to another type,
///   copy cannot be derived if the type T refers to cannot be derived copy.
/// * If T is a compound type, copy cannot be derived if any of its base member
///   or field cannot be derived copy.
/// * If T is an instantiation of an abstract template definition, T cannot be
///   derived copy if any of the template arguments or template definition
///   cannot derive copy.
#[derive(Debug, Clone)]
pub struct CannotDeriveCopy<'ctx, 'gen>
where
    'gen: 'ctx,
{
    ctx: &'ctx BindgenContext<'gen>,

    // The incremental result of this analysis's computation. Everything in this
    // set cannot derive copy.
    cannot_derive_copy: HashSet<ItemId>,

    // Dependencies saying that if a key ItemId has been inserted into the
    // `cannot_derive_copy` set, then each of
    // the ids in Vec<ItemId> need to be considered again.
    //
    // This is a subset of the natural IR graph with reversed edges, where we
    // only include the edges from the IR graph that can affect whether a type
    // can derive copy or not.
    dependencies: HashMap<ItemId, Vec<ItemId>>,
}

impl<'ctx, 'gen> CannotDeriveCopy<'ctx, 'gen> {
    fn consider_edge(kind: EdgeKind) -> bool {
        match kind {
            // These are the only edges that can affect whether a type can derive
            // copy or not.
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
        trace!("inserting {:?} into the cannot_derive_copy set", id);

        let was_not_already_in_set = self.cannot_derive_copy.insert(id);
        assert!(
            was_not_already_in_set,
            "We shouldn't try and insert {:?} twice because if it was \
             already in the set, `constrain` should have exited early.",
            id
        );

        ConstrainResult::Changed
    }
}

impl<'ctx, 'gen> MonotoneFramework for CannotDeriveCopy<'ctx, 'gen> {
    type Node = ItemId;
    type Extra = &'ctx BindgenContext<'gen>;
    type Output = HashSet<ItemId>;

    fn new(ctx: &'ctx BindgenContext<'gen>) -> CannotDeriveCopy<'ctx, 'gen> {
        let cannot_derive_copy = HashSet::new();
        let dependencies = generate_dependencies(ctx, Self::consider_edge);

        CannotDeriveCopy {
            ctx,
            cannot_derive_copy,
            dependencies,
        }
    }

    fn initial_worklist(&self) -> Vec<ItemId> {
        self.ctx.whitelisted_items().iter().cloned().collect()
    }

    fn constrain(&mut self, id: ItemId) -> ConstrainResult {
        trace!("constrain: {:?}", id);

        if self.cannot_derive_copy.contains(&id) {
            trace!("    already know it cannot derive Copy");
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
                l.opaque().can_trivially_derive_copy()
            });
            return if layout_can_derive {
                trace!("    we can trivially derive Copy for the layout");
                ConstrainResult::Same
            } else {
                trace!("    we cannot derive Copy for the layout");
                self.insert(id)
            };
        }

        match *ty.kind() {
            // Handle the simple cases. These can derive copy without further
            // information.
            TypeKind::Void |
            TypeKind::NullPtr |
            TypeKind::Int(..) |
            TypeKind::Float(..) |
            TypeKind::Complex(..) |
            TypeKind::Function(..) |
            TypeKind::Enum(..) |
            TypeKind::Reference(..) |
            TypeKind::TypeParam |
            TypeKind::BlockPointer |
            TypeKind::Pointer(..) |
            TypeKind::UnresolvedTypeRef(..) |
            TypeKind::ObjCInterface(..) |
            TypeKind::ObjCId |
            TypeKind::ObjCSel => {
                trace!("    simple type that can always derive Copy");
                ConstrainResult::Same
            }

            TypeKind::Array(t, len) => {
                let cant_derive_copy = self.cannot_derive_copy.contains(&t);
                if cant_derive_copy {
                    trace!(
                        "    arrays of T for which we cannot derive Copy \
                            also cannot derive Copy"
                    );
                    return self.insert(id);
                }

                if len > 0 {
                    trace!("    array can derive Copy with positive length");
                    ConstrainResult::Same
                } else {
                    trace!("    array cannot derive Copy with 0 length");
                    self.insert(id)
                }
            }

            TypeKind::ResolvedTypeRef(t) |
            TypeKind::TemplateAlias(t, _) |
            TypeKind::Alias(t) => {
                let cant_derive_copy = self.cannot_derive_copy.contains(&t);
                if cant_derive_copy {
                    trace!(
                        "    arrays of T for which we cannot derive Copy \
                            also cannot derive Copy"
                    );
                    return self.insert(id);
                }
                trace!(
                    "    aliases and type refs to T which can derive \
                        Copy can also derive Copy"
                );
                ConstrainResult::Same
            }

            TypeKind::Comp(ref info) => {
                assert!(
                    !info.has_non_type_template_params(),
                    "The early ty.is_opaque check should have handled this case"
                );

                // NOTE: Take into account that while unions in C and C++ are copied by
                // default, the may have an explicit destructor in C++, so we can't
                // defer this check just for the union case.
                if self.ctx.lookup_item_id_has_destructor(&id) {
                    trace!("    comp has destructor which cannot derive copy");
                    return self.insert(id);
                }

                if info.kind() == CompKind::Union {
                    if !self.ctx.options().rust_features().untagged_union() {
                        // NOTE: If there's no template parameters we can derive copy
                        // unconditionally, since arrays are magical for rustc, and
                        // __BindgenUnionField always implements copy.
                        trace!(
                            "    comp can always derive debug if it's a Union and no template parameters"
                        );
                        return ConstrainResult::Same;
                    }

                    // https://github.com/rust-lang/rust/issues/36640
                    if info.self_template_params(self.ctx).is_some() ||
                        item.used_template_params(self.ctx).is_some()
                    {
                        trace!(
                            "    comp cannot derive copy because issue 36640"
                        );
                        return self.insert(id);
                    }
                }

                let bases_cannot_derive =
                    info.base_members().iter().any(|base| {
                        self.cannot_derive_copy.contains(&base.ty)
                    });
                if bases_cannot_derive {
                    trace!(
                        "    base members cannot derive Copy, so we can't \
                            either"
                    );
                    return self.insert(id);
                }

                let fields_cannot_derive =
                    info.fields().iter().any(|f| match *f {
                        Field::DataMember(ref data) => {
                            self.cannot_derive_copy.contains(&data.ty())
                        }
                        Field::Bitfields(ref bfu) => {
                            bfu.bitfields().iter().any(|b| {
                                self.cannot_derive_copy.contains(&b.ty())
                            })
                        }
                    });
                if fields_cannot_derive {
                    trace!("    fields cannot derive Copy, so we can't either");
                    return self.insert(id);
                }

                trace!("    comp can derive Copy");
                ConstrainResult::Same
            }

            TypeKind::TemplateInstantiation(ref template) => {
                let args_cannot_derive =
                    template.template_arguments().iter().any(|arg| {
                        self.cannot_derive_copy.contains(&arg)
                    });
                if args_cannot_derive {
                    trace!(
                        "    template args cannot derive Copy, so \
                            insantiation can't either"
                    );
                    return self.insert(id);
                }

                assert!(
                    !template.template_definition().is_opaque(self.ctx, &()),
                    "The early ty.is_opaque check should have handled this case"
                );
                let def_cannot_derive = self.cannot_derive_copy.contains(
                    &template.template_definition(),
                );
                if def_cannot_derive {
                    trace!(
                        "    template definition cannot derive Copy, so \
                            insantiation can't either"
                    );
                    return self.insert(id);
                }

                trace!("    template instantiation can derive Copy");
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

impl<'ctx, 'gen> From<CannotDeriveCopy<'ctx, 'gen>> for HashSet<ItemId> {
    fn from(analysis: CannotDeriveCopy<'ctx, 'gen>) -> Self {
        analysis.cannot_derive_copy
    }
}
