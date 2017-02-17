//! Bindgen's core intermediate representation type.

use super::annotations::Annotations;
use super::context::{BindgenContext, ItemId, PartialType};
use super::derive::{CanDeriveCopy, CanDeriveDebug, CanDeriveDefault};
use super::function::Function;
use super::item_kind::ItemKind;
use super::module::Module;
use super::traversal::{Trace, Tracer};
use super::ty::{TemplateDeclaration, Type, TypeKind};
use clang;
use clang_sys;
use parse::{ClangItemParser, ClangSubItemParser, ParseError, ParseResult};
use std::cell::{Cell, RefCell};
use std::collections::BTreeSet;
use std::fmt::Write;
use std::iter;

/// A trait to get the canonical name from an item.
///
/// This is the trait that will eventually isolate all the logic related to name
/// mangling and that kind of stuff.
///
/// This assumes no nested paths, at some point I'll have to make it a more
/// complex thing.
///
/// This name is required to be safe for Rust, that is, is not expected to
/// return any rust keyword from here.
pub trait ItemCanonicalName {
    /// Get the canonical name for this item.
    fn canonical_name(&self, ctx: &BindgenContext) -> String;
}

/// The same, but specifies the path that needs to be followed to reach an item.
///
/// To contrast with canonical_name, here's an example:
///
/// ```c++
/// namespace foo {
///     const BAR = 3;
/// }
/// ```
///
/// For bar, the canonical path is `vec!["foo", "BAR"]`, while the canonical
/// name is just `"BAR"`.
pub trait ItemCanonicalPath {
    /// Get the namespace-aware canonical path for this item. This means that if
    /// namespaces are disabled, you'll get a single item, and otherwise you get
    /// the whole path.
    fn namespace_aware_canonical_path(&self,
                                      ctx: &BindgenContext)
                                      -> Vec<String>;

    /// Get the canonical path for this item.
    fn canonical_path(&self, ctx: &BindgenContext) -> Vec<String>;
}

/// A trait for iterating over an item and its parents and up its ancestor chain
/// up to (but not including) the implicit root module.
pub trait ItemAncestors {
    /// Get an iterable over this item's ancestors.
    fn ancestors<'a, 'b>(&self,
                         ctx: &'a BindgenContext<'b>)
                         -> ItemAncestorsIter<'a, 'b>;
}

cfg_if! {
    if #[cfg(debug_assertions)] {
        type DebugOnlyItemSet = ItemSet;
    } else {
        struct DebugOnlyItemSet;

        impl DebugOnlyItemSet {
            fn new() -> Self {
                DebugOnlyItemSet
            }

            fn contains(&self,_id: &ItemId) -> bool {
                false
            }

            fn insert(&mut self, _id: ItemId) {}
        }
    }
}

/// An iterator over an item and its ancestors.
pub struct ItemAncestorsIter<'a, 'b>
    where 'b: 'a,
{
    item: ItemId,
    ctx: &'a BindgenContext<'b>,
    seen: DebugOnlyItemSet,
}

impl<'a, 'b> ItemAncestorsIter<'a, 'b>
    where 'b: 'a,
{
    fn new(ctx: &'a BindgenContext<'b>, item: ItemId) -> Self {
        ItemAncestorsIter {
            item: item,
            ctx: ctx,
            seen: DebugOnlyItemSet::new(),
        }
    }
}

impl<'a, 'b> Iterator for ItemAncestorsIter<'a, 'b>
    where 'b: 'a,
{
    type Item = ItemId;

    fn next(&mut self) -> Option<Self::Item> {
        let item = self.ctx.resolve_item(self.item);

        if item.parent_id() == self.item {
            None
        } else {
            self.item = item.parent_id();

            debug_assert!(!self.seen.contains(&item.id()));
            self.seen.insert(item.id());

            Some(item.id())
        }
    }
}

// Pure convenience
impl ItemCanonicalName for ItemId {
    fn canonical_name(&self, ctx: &BindgenContext) -> String {
        debug_assert!(ctx.in_codegen_phase(),
                      "You're not supposed to call this yet");
        ctx.resolve_item(*self).canonical_name(ctx)
    }
}

impl ItemCanonicalPath for ItemId {
    fn namespace_aware_canonical_path(&self,
                                      ctx: &BindgenContext)
                                      -> Vec<String> {
        debug_assert!(ctx.in_codegen_phase(),
                      "You're not supposed to call this yet");
        ctx.resolve_item(*self).namespace_aware_canonical_path(ctx)
    }

    fn canonical_path(&self, ctx: &BindgenContext) -> Vec<String> {
        debug_assert!(ctx.in_codegen_phase(),
                      "You're not supposed to call this yet");
        ctx.resolve_item(*self).canonical_path(ctx)
    }
}

impl ItemAncestors for ItemId {
    fn ancestors<'a, 'b>(&self,
                         ctx: &'a BindgenContext<'b>)
                         -> ItemAncestorsIter<'a, 'b> {
        ItemAncestorsIter::new(ctx, *self)
    }
}

impl ItemAncestors for Item {
    fn ancestors<'a, 'b>(&self,
                         ctx: &'a BindgenContext<'b>)
                         -> ItemAncestorsIter<'a, 'b> {
        self.id().ancestors(ctx)
    }
}

impl Trace for ItemId {
    type Extra = ();

    fn trace<T>(&self, ctx: &BindgenContext, tracer: &mut T, extra: &())
        where T: Tracer,
    {
        ctx.resolve_item(*self).trace(ctx, tracer, extra);
    }
}

impl Trace for Item {
    type Extra = ();

    fn trace<T>(&self, ctx: &BindgenContext, tracer: &mut T, _extra: &())
        where T: Tracer,
    {
        if self.is_hidden(ctx) {
            return;
        }

        match *self.kind() {
            ItemKind::Type(ref ty) => {
                // There are some types, like resolved type references, where we
                // don't want to stop collecting types even though they may be
                // opaque.
                if ty.should_be_traced_unconditionally() ||
                   !self.is_opaque(ctx) {
                    ty.trace(ctx, tracer, self);
                }
            }
            ItemKind::Function(ref fun) => {
                // Just the same way, it has not real meaning for a function to
                // be opaque, so we trace across it.
                tracer.visit(fun.signature());
            }
            ItemKind::Var(ref var) => {
                tracer.visit(var.ty());
            }
            ItemKind::Module(_) => {
                // Module -> children edges are "weak", and we do not want to
                // trace them. If we did, then whitelisting wouldn't work as
                // expected: everything in every module would end up
                // whitelisted.
                //
                // TODO: make a new edge kind for module -> children edges and
                // filter them during whitelisting traversals.
            }
        }
    }
}

impl CanDeriveDebug for Item {
    type Extra = ();

    fn can_derive_debug(&self, ctx: &BindgenContext, _: ()) -> bool {
        ctx.options().derive_debug &&
        match self.kind {
            ItemKind::Type(ref ty) => {
                if self.is_opaque(ctx) {
                    ty.layout(ctx)
                        .map_or(true, |l| l.opaque().can_derive_debug(ctx, ()))
                } else {
                    ty.can_derive_debug(ctx, ())
                }
            }
            _ => false,
        }
    }
}

impl CanDeriveDefault for Item {
    type Extra = ();

    fn can_derive_default(&self, ctx: &BindgenContext, _: ()) -> bool {
        ctx.options().derive_default &&
        match self.kind {
            ItemKind::Type(ref ty) => {
                if self.is_opaque(ctx) {
                    ty.layout(ctx)
                        .map_or(false,
                                |l| l.opaque().can_derive_default(ctx, ()))
                } else {
                    ty.can_derive_default(ctx, ())
                }
            }
            _ => false,
        }
    }
}

impl<'a> CanDeriveCopy<'a> for Item {
    type Extra = ();

    fn can_derive_copy(&self, ctx: &BindgenContext, _: ()) -> bool {
        match self.kind {
            ItemKind::Type(ref ty) => {
                if self.is_opaque(ctx) {
                    ty.layout(ctx)
                        .map_or(true, |l| l.opaque().can_derive_copy(ctx, ()))
                } else {
                    ty.can_derive_copy(ctx, self)
                }
            }
            _ => false,
        }
    }

    fn can_derive_copy_in_array(&self, ctx: &BindgenContext, _: ()) -> bool {
        match self.kind {
            ItemKind::Type(ref ty) => {
                if self.is_opaque(ctx) {
                    ty.layout(ctx)
                        .map_or(true, |l| {
                            l.opaque().can_derive_copy_in_array(ctx, ())
                        })
                } else {
                    ty.can_derive_copy_in_array(ctx, self)
                }
            }
            _ => false,
        }
    }
}

/// An item is the base of the bindgen representation, it can be either a
/// module, a type, a function, or a variable (see `ItemKind` for more
/// information).
///
/// Items refer to each other by `ItemId`. Every item has its parent's
/// id. Depending on the kind of item this is, it may also refer to other items,
/// such as a compound type item referring to other types. Collectively, these
/// references form a graph.
///
/// The entry-point to this graph is the "root module": a meta-item used to hold
/// all top-level items.
///
/// An item may have a comment, and annotations (see the `annotations` module).
///
/// Note that even though we parse all the types of annotations in comments, not
/// all of them apply to every item. Those rules are described in the
/// `annotations` module.
#[derive(Debug)]
pub struct Item {
    /// This item's id.
    id: ItemId,

    /// The item's local id, unique only amongst its siblings.  Only used for
    /// anonymous items.
    ///
    /// Lazily initialized in local_id().
    ///
    /// Note that only structs, unions, and enums get a local type id. In any
    /// case this is an implementation detail.
    local_id: Cell<Option<usize>>,

    /// The next local id to use for a child..
    next_child_local_id: Cell<usize>,

    /// A cached copy of the canonical name, as returned by `canonical_name`.
    ///
    /// This is a fairly used operation during codegen so this makes bindgen
    /// considerably faster in those cases.
    canonical_name_cache: RefCell<Option<String>>,

    /// A doc comment over the item, if any.
    comment: Option<String>,
    /// Annotations extracted from the doc comment, or the default ones
    /// otherwise.
    annotations: Annotations,
    /// An item's parent id. This will most likely be a class where this item
    /// was declared, or a module, etc.
    ///
    /// All the items have a parent, except the root module, in which case the
    /// parent id is its own id.
    parent_id: ItemId,
    /// The item kind.
    kind: ItemKind,
}

impl Item {
    /// Construct a new `Item`.
    pub fn new(id: ItemId,
               comment: Option<String>,
               annotations: Option<Annotations>,
               parent_id: ItemId,
               kind: ItemKind)
               -> Self {
        debug_assert!(id != parent_id || kind.is_module());
        Item {
            id: id,
            local_id: Cell::new(None),
            next_child_local_id: Cell::new(1),
            canonical_name_cache: RefCell::new(None),
            parent_id: parent_id,
            comment: comment,
            annotations: annotations.unwrap_or_default(),
            kind: kind,
        }
    }

    /// Get this `Item`'s identifier.
    pub fn id(&self) -> ItemId {
        self.id
    }

    /// Get this `Item`'s dot attributes.
    pub fn dot_attributes(&self, ctx: &BindgenContext) -> String {
        format!("[fontname=\"courier\", label=< \
                 <table border=\"0\"> \
                 <tr><td>ItemId({})</td></tr> \
                 <tr><td>name</td><td>{}</td></tr> \
                 <tr><td>kind</td><td>{}</td></tr> \
                 </table> \
                 >]",
                self.id.as_usize(),
                self.name(ctx).get(),
                self.kind.kind_name())
    }

    /// Get this `Item`'s parent's identifier.
    ///
    /// For the root module, the parent's ID is its own ID.
    pub fn parent_id(&self) -> ItemId {
        self.parent_id
    }

    /// Set this item's parent id.
    ///
    /// This is only used so replacements get generated in the proper module.
    pub fn set_parent_for_replacement(&mut self, id: ItemId) {
        self.parent_id = id;
    }

    /// Get this `Item`'s comment, if it has any.
    pub fn comment(&self) -> Option<&str> {
        self.comment.as_ref().map(|c| &**c)
    }

    /// What kind of item is this?
    pub fn kind(&self) -> &ItemKind {
        &self.kind
    }

    /// Get a mutable reference to this item's kind.
    pub fn kind_mut(&mut self) -> &mut ItemKind {
        &mut self.kind
    }

    /// Get an identifier that differentiates this item from its siblings.
    ///
    /// This should stay relatively stable in the face of code motion outside or
    /// below this item's lexical scope, meaning that this can be useful for
    /// generating relatively stable identifiers within a scope.
    pub fn local_id(&self, ctx: &BindgenContext) -> usize {
        if self.local_id.get().is_none() {
            let parent = ctx.resolve_item(self.parent_id);
            let local_id = parent.next_child_local_id.get();
            parent.next_child_local_id.set(local_id + 1);
            self.local_id.set(Some(local_id));
        }
        self.local_id.get().unwrap()
    }

    /// Returns whether this item is a top-level item, from the point of view of
    /// bindgen.
    ///
    /// This point of view changes depending on whether namespaces are enabled
    /// or not. That way, in the following example:
    ///
    /// ```c++
    /// namespace foo {
    ///     static int var;
    /// }
    /// ```
    ///
    /// `var` would be a toplevel item if namespaces are disabled, but won't if
    /// they aren't.
    ///
    /// This function is used to determine when the codegen phase should call
    /// `codegen` on an item, since any item that is not top-level will be
    /// generated by its parent.
    pub fn is_toplevel(&self, ctx: &BindgenContext) -> bool {
        // FIXME: Workaround for some types falling behind when parsing weird
        // stl classes, for example.
        if ctx.options().enable_cxx_namespaces && self.kind().is_module() &&
           self.id() != ctx.root_module() {
            return false;
        }

        let mut parent = self.parent_id;
        loop {
            let parent_item = match ctx.resolve_item_fallible(parent) {
                Some(item) => item,
                None => return false,
            };

            if parent_item.id() == ctx.root_module() {
                return true;
            } else if ctx.options().enable_cxx_namespaces ||
                      !parent_item.kind().is_module() {
                return false;
            }

            parent = parent_item.parent_id();
        }
    }

    /// Get a reference to this item's underlying `Type`. Panic if this is some
    /// other kind of item.
    pub fn expect_type(&self) -> &Type {
        self.kind().expect_type()
    }

    /// Get a reference to this item's underlying `Type`, or `None` if this is
    /// some other kind of item.
    pub fn as_type(&self) -> Option<&Type> {
        self.kind().as_type()
    }

    /// Is this item a named template type parameter?
    pub fn is_named(&self) -> bool {
        self.as_type()
            .map(|ty| ty.is_named())
            .unwrap_or(false)
    }

    /// Get a reference to this item's underlying `Function`. Panic if this is
    /// some other kind of item.
    pub fn expect_function(&self) -> &Function {
        self.kind().expect_function()
    }

    /// Checks whether an item contains in its "type signature" some named type.
    ///
    /// This function is used to avoid unused template parameter errors in Rust
    /// when generating typedef declarations, and also to know whether we need
    /// to generate a `PhantomData` member for a template parameter.
    ///
    /// For example, in code like the following:
    ///
    /// ```c++
    /// template<typename T, typename U>
    /// struct Foo {
    ///     T bar;
    ///
    ///     struct Baz {
    ///         U bas;
    ///     };
    /// };
    /// ```
    ///
    /// Both `Foo` and `Baz` contain both `T` and `U` template parameters in
    /// their signature:
    ///
    ///  * `Foo<T, U>`
    ///  * `Bar<T, U>`
    ///
    /// But the Rust structure for `Foo` would look like:
    ///
    /// ```rust
    /// struct Foo<T, U> {
    ///     bar: T,
    ///     _phantom0: ::std::marker::PhantomData<U>,
    /// }
    /// ```
    ///
    /// because none of its member fields contained the `U` type in the
    /// signature. Similarly, `Bar` would contain a `PhantomData<T>` type, for
    /// the same reason.
    ///
    /// Note that this is somewhat similar to `applicable_template_args`, but
    /// this also takes into account other kind of types, like arrays,
    /// (`[T; 40]`), pointers: `*mut T`, etc...
    ///
    /// Normally we could do this check just in the `Type` kind, but we also
    /// need to check the `applicable_template_args` more generally, since we
    /// could need a type transitively from our parent, see the test added in
    /// commit 2a3f93074dd2898669dbbce6e97e5cc4405d7cb1.
    ///
    /// It's kind of unfortunate (in the sense that it's a sort of complex
    /// process), but I think it should get all the cases.
    fn signature_contains_named_type(&self,
                                     ctx: &BindgenContext,
                                     ty: &Type)
                                     -> bool {
        debug_assert!(ty.is_named());
        self.expect_type().signature_contains_named_type(ctx, ty) ||
        self.applicable_template_args(ctx).iter().any(|template| {
            ctx.resolve_type(*template).signature_contains_named_type(ctx, ty)
        })
    }

    /// Returns the template arguments that apply to a struct. This is a concept
    /// needed because of type declarations inside templates, for example:
    ///
    /// ```c++
    /// template<typename T>
    /// class Foo {
    ///     typedef T element_type;
    ///     typedef int Bar;
    ///
    ///     template<typename U>
    ///     class Baz {
    ///     };
    /// };
    /// ```
    ///
    /// In this case, the applicable template arguments for the different types
    /// would be:
    ///
    ///  * `Foo`: [`T`]
    ///  * `Foo::element_type`: [`T`]
    ///  * `Foo::Bar`: [`T`]
    ///  * `Foo::Baz`: [`T`, `U`]
    ///
    /// You might notice that we can't generate something like:
    ///
    /// ```rust,ignore
    /// type Foo_Bar<T> = ::std::os::raw::c_int;
    /// ```
    ///
    /// since that would be invalid Rust. Still, conceptually, `Bar` *could* use
    /// the template parameter type `T`, and that's exactly what this method
    /// represents. The unused template parameters get stripped in the
    /// `signature_contains_named_type` check.
    pub fn applicable_template_args(&self,
                                    ctx: &BindgenContext)
                                    -> Vec<ItemId> {
        let ty = match *self.kind() {
            ItemKind::Type(ref ty) => ty,
            _ => return vec![],
        };

        fn parent_contains(ctx: &BindgenContext,
                           parent_template_args: &[ItemId],
                           item: ItemId)
                           -> bool {
            let item_ty = ctx.resolve_type(item);
            parent_template_args.iter().any(|parent_item| {
                let parent_ty = ctx.resolve_type(*parent_item);
                match (parent_ty.kind(), item_ty.kind()) {
                    (&TypeKind::Named, &TypeKind::Named) => {
                        parent_ty.name() == item_ty.name()
                    }
                    _ => false,
                }
            })
        }

        match *ty.kind() {
            TypeKind::Named => vec![self.id()],
            TypeKind::Array(inner, _) |
            TypeKind::Pointer(inner) |
            TypeKind::Reference(inner) |
            TypeKind::ResolvedTypeRef(inner) => {
                ctx.resolve_item(inner).applicable_template_args(ctx)
            }
            TypeKind::Alias(inner) => {
                let parent_args = ctx.resolve_item(self.parent_id())
                    .applicable_template_args(ctx);
                let inner = ctx.resolve_item(inner);

                // Avoid unused type parameters, sigh.
                parent_args.iter()
                    .cloned()
                    .filter(|arg| {
                        let arg = ctx.resolve_type(*arg);
                        arg.is_named() &&
                        inner.signature_contains_named_type(ctx, arg)
                    })
                    .collect()
            }
            // XXX Is this completely correct? Partial template specialization
            // is hard anyways, sigh...
            TypeKind::TemplateAlias(_, ref args) |
            TypeKind::TemplateInstantiation(_, ref args) => args.clone(),
            // In a template specialization we've got all we want.
            TypeKind::Comp(ref ci) if ci.is_template_specialization() => {
                ci.template_args().iter().cloned().collect()
            }
            TypeKind::Comp(ref ci) => {
                let mut parent_template_args =
                    ctx.resolve_item(self.parent_id())
                        .applicable_template_args(ctx);

                for ty in ci.template_args() {
                    if !parent_contains(ctx, &parent_template_args, *ty) {
                        parent_template_args.push(*ty);
                    }
                }

                parent_template_args
            }
            _ => vec![],
        }
    }

    /// Is this item a module?
    pub fn is_module(&self) -> bool {
        match self.kind {
            ItemKind::Module(..) => true,
            _ => false,
        }
    }

    /// Get this item's annotations.
    pub fn annotations(&self) -> &Annotations {
        &self.annotations
    }

    /// Whether this item should be hidden.
    ///
    /// This may be due to either annotations or to other kind of configuration.
    pub fn is_hidden(&self, ctx: &BindgenContext) -> bool {
        debug_assert!(ctx.in_codegen_phase(),
                      "You're not supposed to call this yet");
        self.annotations.hide() ||
        ctx.hidden_by_name(&self.canonical_path(ctx), self.id)
    }

    /// Is this item opaque?
    pub fn is_opaque(&self, ctx: &BindgenContext) -> bool {
        debug_assert!(ctx.in_codegen_phase(),
                      "You're not supposed to call this yet");
        self.annotations.opaque() ||
        ctx.opaque_by_name(&self.canonical_path(ctx))
    }

    /// Is this a reference to another type?
    pub fn is_type_ref(&self) -> bool {
        self.as_type().map_or(false, |ty| ty.is_type_ref())
    }

    /// Is this item a var type?
    pub fn is_var(&self) -> bool {
        match *self.kind() {
            ItemKind::Var(..) => true,
            _ => false,
        }
    }

    /// Take out item NameOptions
    pub fn name<'item, 'ctx>(&'item self,
                             ctx: &'item BindgenContext<'ctx>)
                             -> NameOptions<'item, 'ctx> {
        NameOptions::new(self, ctx)
    }

    /// Get the target item id for name generation.
    fn name_target(&self, ctx: &BindgenContext) -> ItemId {
        let mut targets_seen = DebugOnlyItemSet::new();
        let mut item = self;

        loop {
            debug_assert!(!targets_seen.contains(&item.id()));
            targets_seen.insert(item.id());

            if self.annotations().use_instead_of().is_some() {
                return self.id();
            }

            match *item.kind() {
                ItemKind::Type(ref ty) => {
                    match *ty.kind() {
                        // If we're a template specialization, our name is our
                        // parent's name.
                        TypeKind::Comp(ref ci)
                            if ci.is_template_specialization() => {
                            let specialized =
                                ci.specialized_template().unwrap();
                            item = ctx.resolve_item(specialized);
                        }
                        // Same as above.
                        TypeKind::ResolvedTypeRef(inner) |
                        TypeKind::TemplateInstantiation(inner, _) => {
                            item = ctx.resolve_item(inner);
                        }
                        _ => return item.id(),
                    }
                }
                _ => return item.id(),
            }
        }
    }

    /// Get this function item's name, or `None` if this item is not a function.
    fn func_name(&self) -> Option<&str> {
        match *self.kind() {
            ItemKind::Function(ref func) => Some(func.name()),
            _ => None,
        }
    }

    /// Get the overload index for this method. If this is not a method, return
    /// `None`.
    fn overload_index(&self, ctx: &BindgenContext) -> Option<usize> {
        self.func_name().and_then(|func_name| {
            let parent = ctx.resolve_item(self.parent_id());
            if let ItemKind::Type(ref ty) = *parent.kind() {
                if let TypeKind::Comp(ref ci) = *ty.kind() {
                    // All the constructors have the same name, so no need to
                    // resolve and check.
                    return ci.constructors()
                        .iter()
                        .position(|c| *c == self.id())
                        .or_else(|| {
                            ci.methods()
                                .iter()
                                .filter(|m| {
                                    let item = ctx.resolve_item(m.signature());
                                    let func = item.expect_function();
                                    func.name() == func_name
                                })
                                .position(|m| m.signature() == self.id())
                        });
                }
            }

            None
        })
    }

    /// Get this item's base name (aka non-namespaced name).
    fn base_name(&self, ctx: &BindgenContext) -> String {
        if let Some(path) = self.annotations().use_instead_of() {
            return path.last().unwrap().clone();
        }

        match *self.kind() {
            ItemKind::Var(ref var) => var.name().to_owned(),
            ItemKind::Module(ref module) => {
                module.name()
                    .map(ToOwned::to_owned)
                    .unwrap_or_else(|| {
                        format!("_bindgen_mod_{}", self.exposed_id(ctx))
                    })
            }
            ItemKind::Type(ref ty) => {
                let name = match *ty.kind() {
                    TypeKind::ResolvedTypeRef(..) => panic!("should have resolved this in name_target()"),
                    _ => ty.name(),
                };
                name.map(ToOwned::to_owned)
                    .unwrap_or_else(|| {
                        format!("_bindgen_ty_{}", self.exposed_id(ctx))
                    })
            }
            ItemKind::Function(ref fun) => {
                let mut name = fun.name().to_owned();

                if let Some(idx) = self.overload_index(ctx) {
                    if idx > 0 {
                        write!(&mut name, "{}", idx).unwrap();
                    }
                }

                name
            }
        }
    }

    /// Get the canonical name without taking into account the replaces
    /// annotation.
    ///
    /// This is the base logic used to implement hiding and replacing via
    /// annotations, and also to implement proper name mangling.
    ///
    /// The idea is that each generated type in the same "level" (read: module
    /// or namespace) has a unique canonical name.
    ///
    /// This name should be derived from the immutable state contained in the
    /// type and the parent chain, since it should be consistent.
    pub fn real_canonical_name(&self,
                               ctx: &BindgenContext,
                               opt: &NameOptions)
                               -> String {
        let target = ctx.resolve_item(self.name_target(ctx));

        // Short-circuit if the target has an override, and just use that.
        if let Some(path) = target.annotations.use_instead_of() {
            if ctx.options().enable_cxx_namespaces {
                return path.last().unwrap().clone();
            }
            return path.join("_").to_owned();
        }

        let base_name = target.base_name(ctx);

        // Named template type arguments are never namespaced, and never
        // mangled.
        if target.as_type().map_or(false, |ty| ty.is_named()) {
            return base_name;
        }

        // Concatenate this item's ancestors' names together.
        let mut names: Vec<_> = target.parent_id()
            .ancestors(ctx)
            .filter(|id| *id != ctx.root_module())
            .take_while(|id| {
                // Stop iterating ancestors once we reach a namespace.
                !opt.within_namespaces || !ctx.resolve_item(*id).is_module()
            })
            .map(|id| {
                let item = ctx.resolve_item(id);
                let target = ctx.resolve_item(item.name_target(ctx));
                target.base_name(ctx)
            })
            .filter(|name| !name.is_empty())
            .collect();

        names.reverse();

        if !base_name.is_empty() {
            names.push(base_name);
        }

        let name = names.join("_");

        ctx.rust_mangle(&name).into_owned()
    }

    fn exposed_id(&self, ctx: &BindgenContext) -> String {
        // Only use local ids for enums, classes, structs and union types.  All
        // other items use their global id.
        let ty_kind = self.kind().as_type().map(|t| t.kind());
        if let Some(ty_kind) = ty_kind {
            match *ty_kind {
                TypeKind::Comp(..) |
                TypeKind::Enum(..) => return self.local_id(ctx).to_string(),
                _ => {}
            }
        }

        // Note that this `id_` prefix prevents (really unlikely) collisions
        // between the global id and the local id of an item with the same
        // parent.
        format!("id_{}", self.id().as_usize())
    }

    /// Get a reference to this item's `Module`, or `None` if this is not a
    /// `Module` item.
    pub fn as_module(&self) -> Option<&Module> {
        match self.kind {
            ItemKind::Module(ref module) => Some(module),
            _ => None,
        }
    }

    /// Get a mutable reference to this item's `Module`, or `None` if this is
    /// not a `Module` item.
    pub fn as_module_mut(&mut self) -> Option<&mut Module> {
        match self.kind {
            ItemKind::Module(ref mut module) => Some(module),
            _ => None,
        }
    }
}

/// A set of items.
pub type ItemSet = BTreeSet<ItemId>;

impl TemplateDeclaration for ItemId {
    fn template_params(&self, ctx: &BindgenContext) -> Option<Vec<ItemId>> {
        ctx.resolve_item_fallible(*self)
            .and_then(|item| item.template_params(ctx))
    }
}

impl TemplateDeclaration for Item {
    fn template_params(&self, ctx: &BindgenContext) -> Option<Vec<ItemId>> {
        self.kind.template_params(ctx)
    }
}

impl TemplateDeclaration for ItemKind {
    fn template_params(&self, ctx: &BindgenContext) -> Option<Vec<ItemId>> {
        match *self {
            ItemKind::Type(ref ty) => ty.template_params(ctx),
            // If we start emitting bindings to explicitly instantiated
            // functions, then we'll need to check ItemKind::Function for
            // template params.
            ItemKind::Function(_) |
            ItemKind::Module(_) |
            ItemKind::Var(_) => None,
        }
    }
}

// An utility function to handle recursing inside nested types.
fn visit_child(cur: clang::Cursor,
               id: ItemId,
               ty: &clang::Type,
               parent_id: Option<ItemId>,
               ctx: &mut BindgenContext,
               result: &mut Result<ItemId, ParseError>)
               -> clang_sys::CXChildVisitResult {
    use clang_sys::*;
    if result.is_ok() {
        return CXChildVisit_Break;
    }

    *result = Item::from_ty_with_id(id, ty, Some(cur), parent_id, ctx);

    match *result {
        Ok(..) => CXChildVisit_Break,
        Err(ParseError::Recurse) => {
            cur.visit(|c| visit_child(c, id, ty, parent_id, ctx, result));
            CXChildVisit_Continue
        }
        Err(ParseError::Continue) => CXChildVisit_Continue,
    }
}

impl ClangItemParser for Item {
    fn builtin_type(kind: TypeKind,
                    is_const: bool,
                    ctx: &mut BindgenContext)
                    -> ItemId {
        // Feel free to add more here, I'm just lazy.
        match kind {
            TypeKind::Void |
            TypeKind::Int(..) |
            TypeKind::Pointer(..) |
            TypeKind::Float(..) => {}
            _ => panic!("Unsupported builtin type"),
        }

        let ty = Type::new(None, None, kind, is_const);
        let id = ctx.next_item_id();
        let module = ctx.root_module();
        ctx.add_item(Item::new(id, None, None, module, ItemKind::Type(ty)),
                     None,
                     None);
        id
    }


    fn parse(cursor: clang::Cursor,
             parent_id: Option<ItemId>,
             ctx: &mut BindgenContext)
             -> Result<ItemId, ParseError> {
        use ir::function::Function;
        use ir::module::Module;
        use ir::var::Var;
        use clang_sys::*;

        if !cursor.is_valid() {
            return Err(ParseError::Continue);
        }

        let comment = cursor.raw_comment();
        let annotations = Annotations::new(&cursor);

        let current_module = ctx.current_module();
        let relevant_parent_id = parent_id.unwrap_or(current_module);

        macro_rules! try_parse {
            ($what:ident) => {
                match $what::parse(cursor, ctx) {
                    Ok(ParseResult::New(item, declaration)) => {
                        let id = ctx.next_item_id();

                        ctx.add_item(Item::new(id, comment, annotations,
                                               relevant_parent_id,
                                               ItemKind::$what(item)),
                                         declaration,
                                         Some(cursor));
                        return Ok(id);
                    }
                    Ok(ParseResult::AlreadyResolved(id)) => {
                        return Ok(id);
                    }
                    Err(ParseError::Recurse) => return Err(ParseError::Recurse),
                    Err(ParseError::Continue) => {},
                }
            }
        }

        try_parse!(Module);

        // NOTE: Is extremely important to parse functions and vars **before**
        // types.  Otherwise we can parse a function declaration as a type
        // (which is legal), and lose functions to generate.
        //
        // In general, I'm not totally confident this split between
        // ItemKind::Function and TypeKind::FunctionSig is totally worth it, but
        // I guess we can try.
        try_parse!(Function);
        try_parse!(Var);

        // Types are sort of special, so to avoid parsing template classes
        // twice, handle them separately.
        {
            let applicable_cursor = cursor.definition().unwrap_or(cursor);
            match Self::from_ty(&applicable_cursor.cur_type(),
                                Some(applicable_cursor),
                                parent_id,
                                ctx) {
                Ok(ty) => return Ok(ty),
                Err(ParseError::Recurse) => return Err(ParseError::Recurse),
                Err(ParseError::Continue) => {}
            }
        }

        // Guess how does clang treat extern "C" blocks?
        if cursor.kind() == CXCursor_UnexposedDecl {
            Err(ParseError::Recurse)
        } else {
            // We whitelist cursors here known to be unhandled, to prevent being
            // too noisy about this.
            match cursor.kind() {
                CXCursor_MacroDefinition |
                CXCursor_MacroExpansion |
                CXCursor_UsingDeclaration |
                CXCursor_UsingDirective |
                CXCursor_StaticAssert |
                CXCursor_InclusionDirective => {
                    debug!("Unhandled cursor kind {:?}: {:?}",
                           cursor.kind(),
                           cursor);
                }
                _ => {
                    // ignore toplevel operator overloads
                    let spelling = cursor.spelling();
                    if !spelling.starts_with("operator") {
                        error!("Unhandled cursor kind {:?}: {:?}",
                               cursor.kind(),
                               cursor);
                    }
                }
            }

            Err(ParseError::Continue)
        }
    }

    fn from_ty_or_ref(ty: clang::Type,
                      location: Option<clang::Cursor>,
                      parent_id: Option<ItemId>,
                      ctx: &mut BindgenContext)
                      -> ItemId {
        let id = ctx.next_item_id();
        Self::from_ty_or_ref_with_id(id, ty, location, parent_id, ctx)
    }

    /// Parse a C++ type. If we find a reference to a type that has not been
    /// defined yet, use `UnresolvedTypeRef` as a placeholder.
    ///
    /// This logic is needed to avoid parsing items with the incorrect parent
    /// and it's sort of complex to explain, so I'll just point to
    /// `tests/headers/typeref.hpp` to see the kind of constructs that forced
    /// this.
    ///
    /// Typerefs are resolved once parsing is completely done, see
    /// `BindgenContext::resolve_typerefs`.
    fn from_ty_or_ref_with_id(potential_id: ItemId,
                              ty: clang::Type,
                              location: Option<clang::Cursor>,
                              parent_id: Option<ItemId>,
                              ctx: &mut BindgenContext)
                              -> ItemId {
        debug!("from_ty_or_ref_with_id: {:?} {:?}, {:?}, {:?}",
               potential_id,
               ty,
               location,
               parent_id);

        if ctx.collected_typerefs() {
            debug!("refs already collected, resolving directly");
            return Self::from_ty_with_id(potential_id,
                                         &ty,
                                         location,
                                         parent_id,
                                         ctx)
                .expect("Unable to resolve type");
        }

        if let Some(ty) =
            ctx.builtin_or_resolved_ty(potential_id, parent_id, &ty, location) {
            debug!("{:?} already resolved: {:?}", ty, location);
            return ty;
        }

        debug!("New unresolved type reference: {:?}, {:?}", ty, location);

        let is_const = ty.is_const();
        let kind = TypeKind::UnresolvedTypeRef(ty, location, parent_id);
        let current_module = ctx.current_module();
        ctx.add_item(Item::new(potential_id,
                               None,
                               None,
                               parent_id.unwrap_or(current_module),
                               ItemKind::Type(Type::new(None,
                                                        None,
                                                        kind,
                                                        is_const))),
                     Some(clang::Cursor::null()),
                     None);
        potential_id
    }


    fn from_ty(ty: &clang::Type,
               location: Option<clang::Cursor>,
               parent_id: Option<ItemId>,
               ctx: &mut BindgenContext)
               -> Result<ItemId, ParseError> {
        let id = ctx.next_item_id();
        Self::from_ty_with_id(id, ty, location, parent_id, ctx)
    }

    /// This is one of the trickiest methods you'll find (probably along with
    /// some of the ones that handle templates in `BindgenContext`).
    ///
    /// This method parses a type, given the potential id of that type (if
    /// parsing it was correct), an optional location we're scanning, which is
    /// critical some times to obtain information, an optional parent item id,
    /// that will, if it's `None`, become the current module id, and the
    /// context.
    fn from_ty_with_id(id: ItemId,
                       ty: &clang::Type,
                       location: Option<clang::Cursor>,
                       parent_id: Option<ItemId>,
                       ctx: &mut BindgenContext)
                       -> Result<ItemId, ParseError> {
        use clang_sys::*;

        let decl = {
            let decl = ty.declaration();
            decl.definition().unwrap_or(decl)
        };

        let comment = decl.raw_comment()
            .or_else(|| location.as_ref().and_then(|l| l.raw_comment()));
        let annotations = Annotations::new(&decl)
            .or_else(|| location.as_ref().and_then(|l| Annotations::new(l)));

        if let Some(ref annotations) = annotations {
            if let Some(ref replaced) = annotations.use_instead_of() {
                ctx.replace(replaced, id);
            }
        }

        if let Some(ty) =
            ctx.builtin_or_resolved_ty(id, parent_id, ty, location) {
            return Ok(ty);
        }

        // First, check we're not recursing.
        let mut valid_decl = decl.kind() != CXCursor_NoDeclFound;
        let declaration_to_look_for = if valid_decl {
            decl.canonical()
        } else if location.is_some() &&
                                                location.unwrap().kind() ==
                                                CXCursor_ClassTemplate {
            valid_decl = true;
            location.unwrap()
        } else {
            decl
        };

        if valid_decl {
            if let Some(partial) = ctx.currently_parsed_types()
                .iter()
                .find(|ty| *ty.decl() == declaration_to_look_for) {
                debug!("Avoiding recursion parsing type: {:?}", ty);
                return Ok(partial.id());
            }
        }

        let current_module = ctx.current_module();
        let partial_ty = PartialType::new(declaration_to_look_for, id);
        if valid_decl {
            ctx.begin_parsing(partial_ty);
        }

        let result = Type::from_clang_ty(id, ty, location, parent_id, ctx);
        let relevant_parent_id = parent_id.unwrap_or(current_module);
        let ret = match result {
            Ok(ParseResult::AlreadyResolved(ty)) => Ok(ty),
            Ok(ParseResult::New(item, declaration)) => {
                ctx.add_item(Item::new(id,
                                       comment,
                                       annotations,
                                       relevant_parent_id,
                                       ItemKind::Type(item)),
                             declaration,
                             location);
                Ok(id)
            }
            Err(ParseError::Continue) => Err(ParseError::Continue),
            Err(ParseError::Recurse) => {
                debug!("Item::from_ty recursing in the ast");
                let mut result = Err(ParseError::Recurse);
                if let Some(ref location) = location {
                    // Need to pop here, otherwise we'll get stuck.
                    //
                    // TODO: Find a nicer interface, really. Also, the
                    // declaration_to_look_for suspiciously shares a lot of
                    // logic with ir::context, so we should refactor that.
                    if valid_decl {
                        let finished = ctx.finish_parsing();
                        assert_eq!(*finished.decl(), declaration_to_look_for);
                    }

                    location.visit(|cur| {
                        visit_child(cur, id, ty, parent_id, ctx, &mut result)
                    });

                    if valid_decl {
                        let partial_ty =
                            PartialType::new(declaration_to_look_for, id);
                        ctx.begin_parsing(partial_ty);
                    }
                }
                // If we have recursed into the AST all we know, and we still
                // haven't found what we've got, let's just make a named type.
                //
                // This is what happens with some template members, for example.
                //
                // FIXME: Maybe we should restrict this to things with parent?
                // It's harmless, but if we restrict that, then
                // tests/headers/nsStyleAutoArray.hpp crashes.
                if let Err(ParseError::Recurse) = result {
                    warn!("Unknown type, assuming named template type: \
                          id = {:?}; spelling = {}",
                          id,
                          ty.spelling());
                    Ok(Self::named_type_with_id(id,
                                                ty.spelling(),
                                                relevant_parent_id,
                                                ctx))
                } else {
                    result
                }
            }
        };

        if valid_decl {
            let partial_ty = ctx.finish_parsing();
            assert_eq!(*partial_ty.decl(), declaration_to_look_for);
        }

        ret
    }

    /// A named type is a template parameter, e.g., the "T" in Foo<T>. They're
    /// always local so it's the only exception when there's no declaration for
    /// a type.
    ///
    /// It must have an id, and must not be the current module id. Ideally we
    /// could assert the parent id is a Comp(..) type, but that info isn't
    /// available yet.
    fn named_type_with_id<S>(id: ItemId,
                             name: S,
                             parent_id: ItemId,
                             ctx: &mut BindgenContext)
                             -> ItemId
        where S: Into<String>,
    {
        // see tests/headers/const_tparam.hpp
        // and tests/headers/variadic_tname.hpp
        let name = name.into().replace("const ", "").replace(".", "");

        ctx.add_item(Item::new(id,
                               None,
                               None,
                               parent_id,
                               ItemKind::Type(Type::named(name))),
                     None,
                     None);

        id
    }

    fn named_type<S>(name: S,
                     parent_id: ItemId,
                     ctx: &mut BindgenContext)
                     -> ItemId
        where S: Into<String>,
    {
        let id = ctx.next_item_id();
        Self::named_type_with_id(id, name, parent_id, ctx)
    }
}

impl ItemCanonicalName for Item {
    fn canonical_name(&self, ctx: &BindgenContext) -> String {
        debug_assert!(ctx.in_codegen_phase(),
                      "You're not supposed to call this yet");
        if self.canonical_name_cache.borrow().is_none() {
            let in_namespace = ctx.options().enable_cxx_namespaces ||
                               ctx.options().disable_name_namespacing;

            *self.canonical_name_cache.borrow_mut() = if in_namespace {
                Some(self.name(ctx).within_namespaces().get())
            } else {
                Some(self.name(ctx).get())
            };
        }
        return self.canonical_name_cache.borrow().as_ref().unwrap().clone();
    }
}

impl ItemCanonicalPath for Item {
    fn namespace_aware_canonical_path(&self,
                                      ctx: &BindgenContext)
                                      -> Vec<String> {
        let path = self.canonical_path(ctx);
        if ctx.options().enable_cxx_namespaces {
            return path;
        }
        if ctx.options().disable_name_namespacing {
            return vec![path.last().unwrap().clone()];
        }
        return vec![path[1..].join("_")];
    }

    fn canonical_path(&self, ctx: &BindgenContext) -> Vec<String> {
        if let Some(path) = self.annotations().use_instead_of() {
            let mut ret =
                vec![ctx.resolve_item(ctx.root_module()).name(ctx).get()];
            ret.extend_from_slice(path);
            return ret;
        }

        let target = ctx.resolve_item(self.name_target(ctx));
        let mut path: Vec<_> = target.ancestors(ctx)
            .chain(iter::once(ctx.root_module()))
            .map(|id| ctx.resolve_item(id))
            .filter(|item| {
                item.id() == target.id() ||
                item.as_module().map_or(false, |module| {
                    !module.is_inline() ||
                    ctx.options().conservative_inline_namespaces
                })
            })
            .map(|item| {
                ctx.resolve_item(item.name_target(ctx))
                    .name(ctx)
                    .within_namespaces()
                    .get()
            })
            .collect();
        path.reverse();
        path
    }
}

/// Builder struct for naming variations, which hold inside different
/// flags for naming options.
#[derive(Debug)]
pub struct NameOptions<'item, 'ctx>
    where 'ctx: 'item,
{
    item: &'item Item,
    ctx: &'item BindgenContext<'ctx>,
    within_namespaces: bool,
}

impl<'item, 'ctx> NameOptions<'item, 'ctx> {
    /// Construct a new `NameOptions`
    pub fn new(item: &'item Item, ctx: &'item BindgenContext<'ctx>) -> Self {
        NameOptions {
            item: item,
            ctx: ctx,
            within_namespaces: false,
        }
    }

    /// Construct the name without the item's containing C++ namespaces mangled
    /// into it. In other words, the item's name within the item's namespace.
    pub fn within_namespaces(&mut self) -> &mut Self {
        self.within_namespaces = true;
        self
    }

    /// Construct a name `String`
    pub fn get(&self) -> String {
        self.item.real_canonical_name(self.ctx, self)
    }
}
