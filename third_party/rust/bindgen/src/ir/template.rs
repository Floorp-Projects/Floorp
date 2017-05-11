//! Template declaration and instantiation related things.
//!
//! The nomenclature surrounding templates is often confusing, so here are a few
//! brief definitions:
//!
//! * "Template definition": a class/struct/alias/function definition that takes
//! generic template parameters. For example:
//!
//! ```c++
//! template<typename T>
//! class List<T> {
//!     // ...
//! };
//! ```
//!
//! * "Template instantiation": an instantiation is a use of a template with
//! concrete template arguments. For example, `List<int>`.
//!
//! * "Template specialization": an alternative template definition providing a
//! custom definition for instantiations with the matching template
//! arguments. This C++ feature is unsupported by bindgen. For example:
//!
//! ```c++
//! template<>
//! class List<int> {
//!     // Special layout for int lists...
//! };
//! ```

use super::context::{BindgenContext, ItemId};
use super::derive::{CanDeriveCopy, CanDeriveDebug};
use super::item::{Item, ItemAncestors};
use super::layout::Layout;
use super::traversal::{EdgeKind, Trace, Tracer};
use clang;
use parse::ClangItemParser;

/// Template declaration (and such declaration's template parameters) related
/// methods.
///
/// This trait's methods distinguish between `None` and `Some([])` for
/// declarations that are not templates and template declarations with zero
/// parameters, in general.
///
/// Consider this example:
///
/// ```c++
/// template <typename T, typename U>
/// class Foo {
///     T use_of_t;
///     U use_of_u;
///
///     template <typename V>
///     using Bar = V*;
///
///     class Inner {
///         T        x;
///         U        y;
///         Bar<int> z;
///     };
///
///     template <typename W>
///     class Lol {
///         // No use of W, but here's a use of T.
///         T t;
///     };
///
///     template <typename X>
///     class Wtf {
///         // X is not used because W is not used.
///         Lol<X> lololol;
///     };
/// };
///
/// class Qux {
///     int y;
/// };
/// ```
///
/// The following table depicts the results of each trait method when invoked on
/// each of the declarations above:
///
/// +------+----------------------+--------------------------+------------------------+----
/// |Decl. | self_template_params | num_self_template_params | all_template_parameters| ...
/// +------+----------------------+--------------------------+------------------------+----
/// |Foo   | Some([T, U])         | Some(2)                  | Some([T, U])           | ...
/// |Bar   | Some([V])            | Some(1)                  | Some([T, U, V])        | ...
/// |Inner | None                 | None                     | Some([T, U])           | ...
/// |Lol   | Some([W])            | Some(1)                  | Some([T, U, W])        | ...
/// |Wtf   | Some([X])            | Some(1)                  | Some([T, U, X])        | ...
/// |Qux   | None                 | None                     | None                   | ...
/// +------+----------------------+--------------------------+------------------------+----
///
/// ----+------+-----+----------------------+
/// ... |Decl. | ... | used_template_params |
/// ----+------+-----+----------------------+
/// ... |Foo   | ... | Some([T, U])         |
/// ... |Bar   | ... | Some([V])            |
/// ... |Inner | ... | None                 |
/// ... |Lol   | ... | Some([T])            |
/// ... |Wtf   | ... | Some([T])            |
/// ... |Qux   | ... | None                 |
/// ----+------+-----+----------------------+
pub trait TemplateParameters {
    /// Get the set of `ItemId`s that make up this template declaration's free
    /// template parameters.
    ///
    /// Note that these might *not* all be named types: C++ allows
    /// constant-value template parameters as well as template-template
    /// parameters. Of course, Rust does not allow generic parameters to be
    /// anything but types, so we must treat them as opaque, and avoid
    /// instantiating them.
    fn self_template_params(&self,
                            ctx: &BindgenContext)
                            -> Option<Vec<ItemId>>;

    /// Get the number of free template parameters this template declaration
    /// has.
    ///
    /// Implementations *may* return `Some` from this method when
    /// `template_params` returns `None`. This is useful when we only have
    /// partial information about the template declaration, such as when we are
    /// in the middle of parsing it.
    fn num_self_template_params(&self, ctx: &BindgenContext) -> Option<usize> {
        self.self_template_params(ctx).map(|params| params.len())
    }

    /// Get the complete set of template parameters that can affect this
    /// declaration.
    ///
    /// Note that this item doesn't need to be a template declaration itself for
    /// `Some` to be returned here (in contrast to `self_template_params`). If
    /// this item is a member of a template declaration, then the parent's
    /// template parameters are included here.
    ///
    /// In the example above, `Inner` depends on both of the `T` and `U` type
    /// parameters, even though it is not itself a template declaration and
    /// therefore has no type parameters itself. Perhaps it helps to think about
    /// how we would fully reference such a member type in C++:
    /// `Foo<int,char>::Inner`. `Foo` *must* be instantiated with template
    /// arguments before we can gain access to the `Inner` member type.
    fn all_template_params(&self, ctx: &BindgenContext) -> Option<Vec<ItemId>>
        where Self: ItemAncestors,
    {
        let each_self_params: Vec<Vec<_>> = self.ancestors(ctx)
            .filter_map(|id| id.self_template_params(ctx))
            .collect();
        if each_self_params.is_empty() {
            None
        } else {
            Some(each_self_params.into_iter()
                .rev()
                .flat_map(|params| params)
                .collect())
        }
    }

    /// Get only the set of template parameters that this item uses. This is a
    /// subset of `all_template_params` and does not necessarily contain any of
    /// `self_template_params`.
    fn used_template_params(&self, ctx: &BindgenContext) -> Option<Vec<ItemId>>
        where Self: AsRef<ItemId>,
    {
        assert!(ctx.in_codegen_phase(),
                "template parameter usage is not computed until codegen");

        let id = *self.as_ref();
        ctx.resolve_item(id)
            .all_template_params(ctx)
            .map(|all_params| {
                all_params.into_iter()
                    .filter(|p| ctx.uses_template_parameter(id, *p))
                    .collect()
            })
    }
}

/// A trait for things which may or may not be a named template type parameter.
pub trait AsNamed {
    /// Any extra information the implementor might need to make this decision.
    type Extra;

    /// Convert this thing to the item id of a named template type parameter.
    fn as_named(&self,
                ctx: &BindgenContext,
                extra: &Self::Extra)
                -> Option<ItemId>;

    /// Is this a named template type parameter?
    fn is_named(&self, ctx: &BindgenContext, extra: &Self::Extra) -> bool {
        self.as_named(ctx, extra).is_some()
    }
}

/// A concrete instantiation of a generic template.
#[derive(Clone, Debug)]
pub struct TemplateInstantiation {
    /// The template definition which this is instantiating.
    definition: ItemId,
    /// The concrete template arguments, which will be substituted in the
    /// definition for the generic template parameters.
    args: Vec<ItemId>,
}

impl TemplateInstantiation {
    /// Construct a new template instantiation from the given parts.
    pub fn new<I>(template_definition: ItemId,
                  template_args: I)
                  -> TemplateInstantiation
        where I: IntoIterator<Item = ItemId>,
    {
        TemplateInstantiation {
            definition: template_definition,
            args: template_args.into_iter().collect(),
        }
    }

    /// Get the template definition for this instantiation.
    pub fn template_definition(&self) -> ItemId {
        self.definition
    }

    /// Get the concrete template arguments used in this instantiation.
    pub fn template_arguments(&self) -> &[ItemId] {
        &self.args[..]
    }

    /// Parse a `TemplateInstantiation` from a clang `Type`.
    pub fn from_ty(ty: &clang::Type,
                   ctx: &mut BindgenContext)
                   -> Option<TemplateInstantiation> {
        use clang_sys::*;

        let template_args = ty.template_args()
            .map_or(vec![], |args| {
                args.filter(|t| t.kind() != CXType_Invalid)
                    .map(|t| {
                        Item::from_ty_or_ref(t, t.declaration(), None, ctx)
                    })
                    .collect()
            });

        let definition = ty.declaration()
            .specialized()
            .or_else(|| {
                let mut template_ref = None;
                ty.declaration().visit(|child| {
                    if child.kind() == CXCursor_TemplateRef {
                        template_ref = Some(child);
                        return CXVisit_Break;
                    }

                    // Instantiations of template aliases might have the
                    // TemplateRef to the template alias definition arbitrarily
                    // deep, so we need to recurse here and not only visit
                    // direct children.
                    CXChildVisit_Recurse
                });

                template_ref.and_then(|cur| cur.referenced())
            });

        let definition = match definition {
            Some(def) => def,
            None => {
                if !ty.declaration().is_builtin() {
                    warn!("Could not find template definition for template \
                           instantiation");
                }
                return None
            }
        };

        let template_definition =
            Item::from_ty_or_ref(definition.cur_type(), definition, None, ctx);

        Some(TemplateInstantiation::new(template_definition, template_args))
    }

    /// Does this instantiation have a vtable?
    pub fn has_vtable(&self, ctx: &BindgenContext) -> bool {
        ctx.resolve_type(self.definition).has_vtable(ctx)
    }

    /// Does this instantiation have a destructor?
    pub fn has_destructor(&self, ctx: &BindgenContext) -> bool {
        ctx.resolve_type(self.definition).has_destructor(ctx) ||
        self.args.iter().any(|arg| ctx.resolve_type(*arg).has_destructor(ctx))
    }
}

impl<'a> CanDeriveCopy<'a> for TemplateInstantiation {
    type Extra = ();

    fn can_derive_copy(&self, ctx: &BindgenContext, _: ()) -> bool {
        self.definition.can_derive_copy(ctx, ()) &&
        self.args.iter().all(|arg| arg.can_derive_copy(ctx, ()))
    }

    fn can_derive_copy_in_array(&self, ctx: &BindgenContext, _: ()) -> bool {
        self.definition.can_derive_copy_in_array(ctx, ()) &&
        self.args.iter().all(|arg| arg.can_derive_copy_in_array(ctx, ()))
    }
}

impl CanDeriveDebug for TemplateInstantiation {
    type Extra = Option<Layout>;

    fn can_derive_debug(&self,
                        ctx: &BindgenContext,
                        layout: Option<Layout>)
                        -> bool {
        self.args.iter().all(|arg| arg.can_derive_debug(ctx, ())) &&
        ctx.resolve_type(self.definition)
            .as_comp()
            .and_then(|c| {
                // For non-type template parameters, we generate an opaque
                // blob, and in this case the instantiation has a better
                // idea of the layout than the definition does.
                if c.has_non_type_template_params() {
                    let opaque = layout.unwrap_or(Layout::zero()).opaque();
                    Some(opaque.can_derive_debug(ctx, ()))
                } else {
                    None
                }
            })
            .unwrap_or_else(|| self.definition.can_derive_debug(ctx, ()))
    }
}

impl Trace for TemplateInstantiation {
    type Extra = ();

    fn trace<T>(&self, _ctx: &BindgenContext, tracer: &mut T, _: &())
        where T: Tracer,
    {
        tracer.visit_kind(self.definition, EdgeKind::TemplateDeclaration);
        for &item in self.template_arguments() {
            tracer.visit_kind(item, EdgeKind::TemplateArgument);
        }
    }
}
