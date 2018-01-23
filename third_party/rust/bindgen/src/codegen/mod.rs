mod impl_debug;
mod impl_partialeq;
mod error;
mod helpers;
pub mod struct_layout;

#[cfg(test)]
#[allow(warnings)]
pub(crate) mod bitfield_unit;
#[cfg(test)]
mod bitfield_unit_tests;

use self::helpers::attributes;
use self::struct_layout::StructLayoutTracker;

use super::BindgenOptions;

use ir::analysis::{HasVtable, Sizedness};
use ir::annotations::FieldAccessorKind;
use ir::comment;
use ir::comp::{Base, Bitfield, BitfieldUnit, CompInfo, CompKind, Field,
               FieldData, FieldMethods, Method, MethodKind};
use ir::context::{BindgenContext, ItemId};
use ir::derive::{CanDeriveCopy, CanDeriveDebug, CanDeriveDefault,
                 CanDeriveHash, CanDerivePartialOrd, CanDeriveOrd,
                 CanDerivePartialEq, CanDeriveEq, CanDerive};
use ir::dot;
use ir::enum_ty::{Enum, EnumVariant, EnumVariantValue};
use ir::function::{Abi, Function, FunctionKind, FunctionSig, Linkage};
use ir::int::IntKind;
use ir::item::{IsOpaque, Item, ItemCanonicalName, ItemCanonicalPath};
use ir::item_kind::ItemKind;
use ir::layout::Layout;
use ir::module::Module;
use ir::objc::{ObjCInterface, ObjCMethod};
use ir::template::{AsTemplateParam, TemplateInstantiation, TemplateParameters};
use ir::ty::{Type, TypeKind};
use ir::var::Var;

use quote;

use std::borrow::Cow;
use std::cell::Cell;
use std::collections::{HashSet, VecDeque};
use std::collections::hash_map::{Entry, HashMap};
use std::fmt::Write;
use std::iter;
use std::mem;
use std::ops;

// Name of type defined in constified enum module
pub static CONSTIFIED_ENUM_MODULE_REPR_NAME: &'static str = "Type";

fn top_level_path(ctx: &BindgenContext, item: &Item) -> Vec<quote::Tokens> {
    let mut path = vec![quote! { self }];

    if ctx.options().enable_cxx_namespaces {
        for _ in 0..item.codegen_depth(ctx) {
            path.push(quote! { super });
        }
    }

    path
}

fn root_import(ctx: &BindgenContext, module: &Item) -> quote::Tokens {
    assert!(ctx.options().enable_cxx_namespaces, "Somebody messed it up");
    assert!(module.is_module());

    let mut path = top_level_path(ctx, module);

    let root = ctx.root_module().canonical_name(ctx);
    let root_ident = ctx.rust_ident(&root);
    path.push(quote! { #root_ident });


    let mut tokens = quote! {};
    tokens.append_separated(path, "::");

    quote! {
        #[allow(unused_imports)]
        use #tokens ;
    }
}

struct CodegenResult<'a> {
    items: Vec<quote::Tokens>,

    /// A monotonic counter used to add stable unique id's to stuff that doesn't
    /// need to be referenced by anything.
    codegen_id: &'a Cell<usize>,

    /// Whether a bindgen union has been generated at least once.
    saw_bindgen_union: bool,

    /// Whether an union has been generated at least once.
    saw_union: bool,

    /// Whether an incomplete array has been generated at least once.
    saw_incomplete_array: bool,

    /// Whether Objective C types have been seen at least once.
    saw_objc: bool,

    /// Whether a bitfield allocation unit has been seen at least once.
    saw_bitfield_unit: bool,

    items_seen: HashSet<ItemId>,
    /// The set of generated function/var names, needed because in C/C++ is
    /// legal to do something like:
    ///
    /// ```c++
    /// extern "C" {
    ///   void foo();
    ///   extern int bar;
    /// }
    ///
    /// extern "C" {
    ///   void foo();
    ///   extern int bar;
    /// }
    /// ```
    ///
    /// Being these two different declarations.
    functions_seen: HashSet<String>,
    vars_seen: HashSet<String>,

    /// Used for making bindings to overloaded functions. Maps from a canonical
    /// function name to the number of overloads we have already codegen'd for
    /// that name. This lets us give each overload a unique suffix.
    overload_counters: HashMap<String, u32>,
}

impl<'a> CodegenResult<'a> {
    fn new(codegen_id: &'a Cell<usize>) -> Self {
        CodegenResult {
            items: vec![],
            saw_union: false,
            saw_bindgen_union: false,
            saw_incomplete_array: false,
            saw_objc: false,
            saw_bitfield_unit: false,
            codegen_id: codegen_id,
            items_seen: Default::default(),
            functions_seen: Default::default(),
            vars_seen: Default::default(),
            overload_counters: Default::default(),
        }
    }

    fn saw_union(&mut self) {
        self.saw_union = true;
    }

    fn saw_bindgen_union(&mut self) {
        self.saw_union();
        self.saw_bindgen_union = true;
    }

    fn saw_incomplete_array(&mut self) {
        self.saw_incomplete_array = true;
    }

    fn saw_objc(&mut self) {
        self.saw_objc = true;
    }

    fn saw_bitfield_unit(&mut self) {
        self.saw_bitfield_unit = true;
    }

    fn seen<Id: Into<ItemId>>(&self, item: Id) -> bool {
        self.items_seen.contains(&item.into())
    }

    fn set_seen<Id: Into<ItemId>>(&mut self, item: Id) {
        self.items_seen.insert(item.into());
    }

    fn seen_function(&self, name: &str) -> bool {
        self.functions_seen.contains(name)
    }

    fn saw_function(&mut self, name: &str) {
        self.functions_seen.insert(name.into());
    }

    /// Get the overload number for the given function name. Increments the
    /// counter internally so the next time we ask for the overload for this
    /// name, we get the incremented value, and so on.
    fn overload_number(&mut self, name: &str) -> u32 {
        let counter = self.overload_counters.entry(name.into()).or_insert(0);
        let number = *counter;
        *counter += 1;
        number
    }

    fn seen_var(&self, name: &str) -> bool {
        self.vars_seen.contains(name)
    }

    fn saw_var(&mut self, name: &str) {
        self.vars_seen.insert(name.into());
    }

    fn inner<F>(&mut self, cb: F) -> Vec<quote::Tokens>
    where
        F: FnOnce(&mut Self),
    {
        let mut new = Self::new(self.codegen_id);

        cb(&mut new);

        self.saw_union |= new.saw_union;
        self.saw_incomplete_array |= new.saw_incomplete_array;
        self.saw_objc |= new.saw_objc;
        self.saw_bitfield_unit |= new.saw_bitfield_unit;

        new.items
    }
}

impl<'a> ops::Deref for CodegenResult<'a> {
    type Target = Vec<quote::Tokens>;

    fn deref(&self) -> &Self::Target {
        &self.items
    }
}

impl<'a> ops::DerefMut for CodegenResult<'a> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.items
    }
}

/// A trait to convert a rust type into a pointer, optionally const, to the same
/// type.
trait ToPtr {
    fn to_ptr(self, is_const: bool) -> quote::Tokens;
}

impl ToPtr for quote::Tokens {
    fn to_ptr(self, is_const: bool) -> quote::Tokens {
        if is_const {
            quote! { *const #self }
        } else {
            quote! { *mut #self }
        }
    }
}

/// An extension trait for `quote::Tokens` that lets us append any implicit
/// template parameters that exist for some type, if necessary.
trait AppendImplicitTemplateParams {
    fn append_implicit_template_params(
        &mut self,
        ctx: &BindgenContext,
        item: &Item,
    );
}

impl AppendImplicitTemplateParams for quote::Tokens {
    fn append_implicit_template_params(
        &mut self,
        ctx: &BindgenContext,
        item: &Item,
    ) {
        let item = item.id()
            .into_resolver()
            .through_type_refs()
            .resolve(ctx);

        match *item.expect_type().kind() {
            TypeKind::UnresolvedTypeRef(..) => {
                unreachable!("already resolved unresolved type refs")
            }
            TypeKind::ResolvedTypeRef(..) => {
                unreachable!("we resolved item through type refs")
            }

            // None of these types ever have implicit template parameters.
            TypeKind::Void |
            TypeKind::NullPtr |
            TypeKind::Pointer(..) |
            TypeKind::Reference(..) |
            TypeKind::Int(..) |
            TypeKind::Float(..) |
            TypeKind::Complex(..) |
            TypeKind::Array(..) |
            TypeKind::TypeParam |
            TypeKind::Opaque |
            TypeKind::Function(..) |
            TypeKind::Enum(..) |
            TypeKind::BlockPointer |
            TypeKind::ObjCId |
            TypeKind::ObjCSel |
            TypeKind::TemplateInstantiation(..) => return,
            _ => {},
        }

        if let Some(params) = item.used_template_params(ctx) {
            if params.is_empty() {
                return;
            }

            let params = params.into_iter().map(|p| {
                p.try_to_rust_ty(ctx, &())
                    .expect("template params cannot fail to be a rust type")
            });

            self.append(quote! {
                < #( #params ),* >
            });
        }
    }
}

trait CodeGenerator {
    /// Extra information from the caller.
    type Extra;

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        extra: &Self::Extra,
    );
}

impl CodeGenerator for Item {
    type Extra = ();

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        _extra: &(),
    ) {
        if !self.is_enabled_for_codegen(ctx) {
            return;
        }

        if self.is_blacklisted(ctx) || result.seen(self.id()) {
            debug!(
                "<Item as CodeGenerator>::codegen: Ignoring hidden or seen: \
                 self = {:?}",
                self
            );
            return;
        }

        debug!("<Item as CodeGenerator>::codegen: self = {:?}", self);
        if !ctx.codegen_items().contains(&self.id()) {
            // TODO(emilio, #453): Figure out what to do when this happens
            // legitimately, we could track the opaque stuff and disable the
            // assertion there I guess.
            error!("Found non-whitelisted item in code generation: {:?}", self);
        }

        result.set_seen(self.id());

        match *self.kind() {
            ItemKind::Module(ref module) => {
                module.codegen(ctx, result, self);
            }
            ItemKind::Function(ref fun) => {
                fun.codegen(ctx, result, self);
            }
            ItemKind::Var(ref var) => {
                var.codegen(ctx, result, self);
            }
            ItemKind::Type(ref ty) => {
                ty.codegen(ctx, result, self);
            }
        }
    }
}

impl CodeGenerator for Module {
    type Extra = Item;

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        item: &Item,
    ) {
        debug!("<Module as CodeGenerator>::codegen: item = {:?}", item);

        let codegen_self = |result: &mut CodegenResult,
                            found_any: &mut bool| {
            for child in self.children() {
                if ctx.codegen_items().contains(child) {
                    *found_any = true;
                    ctx.resolve_item(*child).codegen(ctx, result, &());
                }
            }

            if item.id() == ctx.root_module() {
                if result.saw_bindgen_union {
                    utils::prepend_union_types(ctx, &mut *result);
                }
                if result.saw_incomplete_array {
                    utils::prepend_incomplete_array_types(ctx, &mut *result);
                }
                if ctx.need_bindegen_complex_type() {
                    utils::prepend_complex_type(&mut *result);
                }
                if result.saw_objc {
                    utils::prepend_objc_header(ctx, &mut *result);
                }
                if result.saw_bitfield_unit {
                    utils::prepend_bitfield_unit_type(&mut *result);
                }
            }
        };

        if !ctx.options().enable_cxx_namespaces ||
            (self.is_inline() &&
                !ctx.options().conservative_inline_namespaces)
        {
            codegen_self(result, &mut false);
            return;
        }

        let mut found_any = false;
        let inner_items = result.inner(|result| {
            result.push(root_import(ctx, item));
            codegen_self(result, &mut found_any);
        });

        // Don't bother creating an empty module.
        if !found_any {
            return;
        }

        let name = item.canonical_name(ctx);

        result.push(if name == "root" {
            quote! {
                #[allow(non_snake_case, non_camel_case_types, non_upper_case_globals)]
                pub mod root {
                    #( #inner_items )*
                }
            }
        } else {
            let ident = ctx.rust_ident(name);
            quote! {
                pub mod #ident {
                    #( #inner_items )*
                }
            }
        });
    }
}

impl CodeGenerator for Var {
    type Extra = Item;
    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        item: &Item,
    ) {
        use ir::var::VarType;
        debug!("<Var as CodeGenerator>::codegen: item = {:?}", item);
        debug_assert!(item.is_enabled_for_codegen(ctx));

        let canonical_name = item.canonical_name(ctx);

        if result.seen_var(&canonical_name) {
            return;
        }
        result.saw_var(&canonical_name);

        let canonical_ident = ctx.rust_ident(&canonical_name);

        // We can't generate bindings to static variables of templates. The
        // number of actual variables for a single declaration are open ended
        // and we don't know what instantiations do or don't exist.
        let type_params = item.all_template_params(ctx);
        if let Some(params) = type_params {
            if !params.is_empty() {
                return;
            }
        }

        let ty = self.ty().to_rust_ty_or_opaque(ctx, &());

        if let Some(val) = self.val() {
            match *val {
                VarType::Bool(val) => {
                    result.push(quote! {
                        pub const #canonical_ident : #ty = #val ;
                    });
                }
                VarType::Int(val) => {
                    let int_kind = self.ty()
                        .into_resolver()
                        .through_type_aliases()
                        .through_type_refs()
                        .resolve(ctx)
                        .expect_type()
                        .as_integer()
                        .unwrap();
                    let val = if int_kind.is_signed() {
                        helpers::ast_ty::int_expr(val)
                    } else {
                        helpers::ast_ty::uint_expr(val as _)
                    };
                    result.push(quote! {
                        pub const #canonical_ident : #ty = #val ;
                    });
                }
                VarType::String(ref bytes) => {
                    // Account the trailing zero.
                    //
                    // TODO: Here we ignore the type we just made up, probably
                    // we should refactor how the variable type and ty id work.
                    let len = bytes.len() + 1;
                    let ty = quote! {
                        [u8; #len]
                    };

                    match String::from_utf8(bytes.clone()) {
                        Ok(string) => {
                            let cstr = helpers::ast_ty::cstr_expr(string);
                            result.push(quote! {
                                pub const #canonical_ident : &'static #ty = #cstr ;
                            });
                        }
                        Err(..) => {
                            let bytes = helpers::ast_ty::byte_array_expr(bytes);
                            result.push(quote! {
                                pub const #canonical_ident : #ty = #bytes ;
                            });
                        }
                    }
                }
                VarType::Float(f) => {
                    match helpers::ast_ty::float_expr(ctx, f) {
                        Ok(expr) => result.push(quote! {
                            pub const #canonical_ident : #ty = #expr ;
                        }),
                        Err(..) => return,
                    }
                }
                VarType::Char(c) => {
                    result.push(quote! {
                        pub const #canonical_ident : #ty = #c ;
                    });
                }
            }
        } else {
            let mut attrs = vec![];
            if let Some(mangled) = self.mangled_name() {
                attrs.push(attributes::link_name(mangled));
            } else if canonical_name != self.name() {
                attrs.push(attributes::link_name(self.name()));
            }

            let mut tokens = quote! {
                extern "C"
            };
            tokens.append("{\n");
            if !attrs.is_empty() {
                tokens.append_separated(attrs, "\n");
                tokens.append("\n");
            }
            tokens.append("pub static mut ");
            tokens.append(quote! { #canonical_ident });
            tokens.append(" : ");
            tokens.append(quote! { #ty });
            tokens.append(";\n}");

            result.push(tokens);
        }
    }
}

impl CodeGenerator for Type {
    type Extra = Item;

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        item: &Item,
    ) {
        debug!("<Type as CodeGenerator>::codegen: item = {:?}", item);
        debug_assert!(item.is_enabled_for_codegen(ctx));

        match *self.kind() {
            TypeKind::Void |
            TypeKind::NullPtr |
            TypeKind::Int(..) |
            TypeKind::Float(..) |
            TypeKind::Complex(..) |
            TypeKind::Array(..) |
            TypeKind::Pointer(..) |
            TypeKind::BlockPointer |
            TypeKind::Reference(..) |
            TypeKind::Function(..) |
            TypeKind::ResolvedTypeRef(..) |
            TypeKind::Opaque |
            TypeKind::TypeParam => {
                // These items don't need code generation, they only need to be
                // converted to rust types in fields, arguments, and such.
                return;
            }
            TypeKind::TemplateInstantiation(ref inst) => {
                inst.codegen(ctx, result, item)
            }
            TypeKind::Comp(ref ci) => ci.codegen(ctx, result, item),
            TypeKind::TemplateAlias(inner, _) |
            TypeKind::Alias(inner) => {
                let inner_item = inner.into_resolver()
                    .through_type_refs()
                    .resolve(ctx);
                let name = item.canonical_name(ctx);

                // Try to catch the common pattern:
                //
                // typedef struct foo { ... } foo;
                //
                // here.
                //
                if inner_item.canonical_name(ctx) == name {
                    return;
                }

                // If this is a known named type, disallow generating anything
                // for it too.
                let spelling = self.name().expect("Unnamed alias?");
                if utils::type_from_named(ctx, spelling).is_some() {
                    return;
                }

                let mut outer_params = item.used_template_params(ctx)
                    .and_then(|ps| if ps.is_empty() {
                        None
                    } else {
                        Some(ps)
                    });

                let inner_rust_type = if item.is_opaque(ctx, &()) {
                    outer_params = None;
                    self.to_opaque(ctx, item)
                } else {
                    // Its possible that we have better layout information than
                    // the inner type does, so fall back to an opaque blob based
                    // on our layout if converting the inner item fails.
                    let mut inner_ty = inner_item
                        .try_to_rust_ty_or_opaque(ctx, &())
                        .unwrap_or_else(|_| self.to_opaque(ctx, item));
                    inner_ty.append_implicit_template_params(ctx, inner_item);
                    inner_ty
                };

                {
                    // FIXME(emilio): This is a workaround to avoid generating
                    // incorrect type aliases because of types that we haven't
                    // been able to resolve (because, eg, they depend on a
                    // template parameter).
                    //
                    // It's kind of a shame not generating them even when they
                    // could be referenced, but we already do the same for items
                    // with invalid template parameters, and at least this way
                    // they can be replaced, instead of generating plain invalid
                    // code.
                    let inner_canon_type =
                        inner_item.expect_type().canonical_type(ctx);
                    if inner_canon_type.is_invalid_type_param() {
                        warn!(
                            "Item contained invalid named type, skipping: \
                             {:?}, {:?}",
                            item,
                            inner_item
                        );
                        return;
                    }
                }

                let rust_name = ctx.rust_ident(&name);

                let mut tokens = if let Some(comment) = item.comment(ctx) {
                    attributes::doc(comment)
                } else {
                    quote! {}
                };

                // We prefer using `pub use` over `pub type` because of:
                // https://github.com/rust-lang/rust/issues/26264
                if inner_rust_type.as_str()
                    .chars()
                    .all(|c| match c {
                        // These are the only characters allowed in simple
                        // paths, eg `good::dogs::Bront`.
                        'A'...'Z' | 'a'...'z' | '0'...'9' | ':' | '_' | ' ' => true,
                        _ => false,
                    }) &&
                    outer_params.is_none() &&
                    inner_item.expect_type().canonical_type(ctx).is_enum()
                {
                    tokens.append(quote! {
                        pub use
                    });
                    let path = top_level_path(ctx, item);
                    tokens.append_separated(path, "::");
                    tokens.append(quote! {
                        :: #inner_rust_type  as #rust_name ;
                    });
                    result.push(tokens);
                    return;
                }

                tokens.append(quote! {
                    pub type #rust_name
                });

                if let Some(params) = outer_params {
                    let params: Vec<_> = params.into_iter()
                        .filter_map(|p| p.as_template_param(ctx, &()))
                        .collect();
                    if params.iter().any(|p| ctx.resolve_type(*p).is_invalid_type_param()) {
                        warn!(
                            "Item contained invalid template \
                             parameter: {:?}",
                            item
                        );
                        return;
                    }

                    let params = params.iter()
                        .map(|p| {
                            p.try_to_rust_ty(ctx, &())
                                .expect("type parameters can always convert to rust ty OK")
                        });

                    tokens.append(quote! {
                        < #( #params ),* >
                    });
                }

                tokens.append(quote! {
                    = #inner_rust_type ;
                });

                result.push(tokens);
            }
            TypeKind::Enum(ref ei) => ei.codegen(ctx, result, item),
            TypeKind::ObjCId | TypeKind::ObjCSel => {
                result.saw_objc();
            }
            TypeKind::ObjCInterface(ref interface) => {
                interface.codegen(ctx, result, item)
            }
            ref u @ TypeKind::UnresolvedTypeRef(..) => {
                unreachable!("Should have been resolved after parsing {:?}!", u)
            }
        }
    }
}

struct Vtable<'a> {
    item_id: ItemId,
    #[allow(dead_code)]
    methods: &'a [Method],
    #[allow(dead_code)]
    base_classes: &'a [Base],
}

impl<'a> Vtable<'a> {
    fn new(
        item_id: ItemId,
        methods: &'a [Method],
        base_classes: &'a [Base],
    ) -> Self {
        Vtable {
            item_id: item_id,
            methods: methods,
            base_classes: base_classes,
        }
    }
}

impl<'a> CodeGenerator for Vtable<'a> {
    type Extra = Item;

    fn codegen<'b>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'b>,
        item: &Item,
    ) {
        assert_eq!(item.id(), self.item_id);
        debug_assert!(item.is_enabled_for_codegen(ctx));

        // For now, generate an empty struct, later we should generate function
        // pointers and whatnot.
        let name = ctx.rust_ident(&self.canonical_name(ctx));
        let void = helpers::ast_ty::raw_type(ctx, "c_void");
        result.push(quote! {
            #[repr(C)]
            pub struct #name ( #void );
        });
    }
}

impl<'a> ItemCanonicalName for Vtable<'a> {
    fn canonical_name(&self, ctx: &BindgenContext) -> String {
        format!("{}__bindgen_vtable", self.item_id.canonical_name(ctx))
    }
}

impl<'a> TryToRustTy for Vtable<'a> {
    type Extra = ();

    fn try_to_rust_ty(
        &self,
        ctx: &BindgenContext,
        _: &(),
    ) -> error::Result<quote::Tokens> {
        let name = ctx.rust_ident(self.canonical_name(ctx));
        Ok(quote! {
            #name
        })
    }
}

impl CodeGenerator for TemplateInstantiation {
    type Extra = Item;

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        item: &Item,
    ) {
        debug_assert!(item.is_enabled_for_codegen(ctx));

        // Although uses of instantiations don't need code generation, and are
        // just converted to rust types in fields, vars, etc, we take this
        // opportunity to generate tests for their layout here. If the
        // instantiation is opaque, then its presumably because we don't
        // properly understand it (maybe because of specializations), and so we
        // shouldn't emit layout tests either.
        if !ctx.options().layout_tests || self.is_opaque(ctx, item) {
            return;
        }

        // If there are any unbound type parameters, then we can't generate a
        // layout test because we aren't dealing with a concrete type with a
        // concrete size and alignment.
        if ctx.uses_any_template_parameters(item.id()) {
            return;
        }

        let layout = item.kind().expect_type().layout(ctx);

        if let Some(layout) = layout {
            let size = layout.size;
            let align = layout.align;

            let name = item.full_disambiguated_name(ctx);
            let mut fn_name =
                format!("__bindgen_test_layout_{}_instantiation", name);
            let times_seen = result.overload_number(&fn_name);
            if times_seen > 0 {
                write!(&mut fn_name, "_{}", times_seen).unwrap();
            }

            let fn_name = ctx.rust_ident_raw(fn_name);

            let prefix = ctx.trait_prefix();
            let ident = item.to_rust_ty_or_opaque(ctx, &());
            let size_of_expr = quote! {
                ::#prefix::mem::size_of::<#ident>()
            };
            let align_of_expr = quote! {
                ::#prefix::mem::align_of::<#ident>()
            };

            let item = quote! {
                #[test]
                fn #fn_name() {
                    assert_eq!(#size_of_expr, #size,
                               concat!("Size of template specialization: ",
                                       stringify!(#ident)));
                    assert_eq!(#align_of_expr, #align,
                               concat!("Alignment of template specialization: ",
                                       stringify!(#ident)));
                }
            };

            result.push(item);
        }
    }
}

/// Trait for implementing the code generation of a struct or union field.
trait FieldCodegen<'a> {
    type Extra;

    fn codegen<F, M>(
        &self,
        ctx: &BindgenContext,
        fields_should_be_private: bool,
        codegen_depth: usize,
        accessor_kind: FieldAccessorKind,
        parent: &CompInfo,
        result: &mut CodegenResult,
        struct_layout: &mut StructLayoutTracker,
        fields: &mut F,
        methods: &mut M,
        extra: Self::Extra,
    ) where
        F: Extend<quote::Tokens>,
        M: Extend<quote::Tokens>;
}

impl<'a> FieldCodegen<'a> for Field {
    type Extra = ();

    fn codegen<F, M>(
        &self,
        ctx: &BindgenContext,
        fields_should_be_private: bool,
        codegen_depth: usize,
        accessor_kind: FieldAccessorKind,
        parent: &CompInfo,
        result: &mut CodegenResult,
        struct_layout: &mut StructLayoutTracker,
        fields: &mut F,
        methods: &mut M,
        _: (),
    ) where
        F: Extend<quote::Tokens>,
        M: Extend<quote::Tokens>,
    {
        match *self {
            Field::DataMember(ref data) => {
                data.codegen(
                    ctx,
                    fields_should_be_private,
                    codegen_depth,
                    accessor_kind,
                    parent,
                    result,
                    struct_layout,
                    fields,
                    methods,
                    (),
                );
            }
            Field::Bitfields(ref unit) => {
                unit.codegen(
                    ctx,
                    fields_should_be_private,
                    codegen_depth,
                    accessor_kind,
                    parent,
                    result,
                    struct_layout,
                    fields,
                    methods,
                    (),
                );
            }
        }
    }
}

impl<'a> FieldCodegen<'a> for FieldData {
    type Extra = ();

    fn codegen<F, M>(
        &self,
        ctx: &BindgenContext,
        fields_should_be_private: bool,
        codegen_depth: usize,
        accessor_kind: FieldAccessorKind,
        parent: &CompInfo,
        result: &mut CodegenResult,
        struct_layout: &mut StructLayoutTracker,
        fields: &mut F,
        methods: &mut M,
        _: (),
    ) where
        F: Extend<quote::Tokens>,
        M: Extend<quote::Tokens>,
    {
        // Bitfields are handled by `FieldCodegen` implementations for
        // `BitfieldUnit` and `Bitfield`.
        assert!(self.bitfield_width().is_none());

        let field_item = self.ty().into_resolver().through_type_refs().resolve(ctx);
        let field_ty = field_item.expect_type();
        let mut ty = self.ty().to_rust_ty_or_opaque(ctx, &());

        // NB: If supported, we use proper `union` types.
        let ty = if parent.is_union() && !parent.can_be_rust_union(ctx) {
            if ctx.options().enable_cxx_namespaces {
                quote! {
                    root::__BindgenUnionField<#ty>
                }
            } else {
                quote! {
                    __BindgenUnionField<#ty>
                }
            }
        } else if let Some(item) = field_ty.is_incomplete_array(ctx) {
            result.saw_incomplete_array();

            let inner = item.to_rust_ty_or_opaque(ctx, &());

            if ctx.options().enable_cxx_namespaces {
                quote! {
                    root::__IncompleteArrayField<#inner>
                }
            } else {
                quote! {
                    __IncompleteArrayField<#inner>
                }
            }
        } else {
            ty.append_implicit_template_params(ctx, field_item);
            ty
        };

        let mut field = quote! {};
        if ctx.options().generate_comments {
            if let Some(raw_comment) = self.comment() {
                let comment =
                    comment::preprocess(raw_comment, codegen_depth + 1);
                field = attributes::doc(comment);
            }
        }

        let field_name =
            self.name()
                .map(|name| ctx.rust_mangle(name).into_owned())
                .expect("Each field should have a name in codegen!");
        let field_ident = ctx.rust_ident_raw(field_name.as_str());

        if !parent.is_union() {
            if let Some(padding_field) =
                struct_layout.pad_field(&field_name, field_ty, self.offset())
            {
                fields.extend(Some(padding_field));
            }
        }

        let is_private = self.annotations().private_fields().unwrap_or(
            fields_should_be_private,
        );

        let accessor_kind =
            self.annotations().accessor_kind().unwrap_or(accessor_kind);

        if is_private {
            field.append(quote! {
                #field_ident : #ty ,
            });
        } else {
            field.append(quote! {
                pub #field_ident : #ty ,
            });
        }

        fields.extend(Some(field));

        // TODO: Factor the following code out, please!
        if accessor_kind == FieldAccessorKind::None {
            return;
        }

        let getter_name = ctx.rust_ident_raw(format!("get_{}", field_name));
        let mutable_getter_name =
            ctx.rust_ident_raw(format!("get_{}_mut", field_name));
        let field_name = ctx.rust_ident_raw(field_name);

        methods.extend(Some(match accessor_kind {
            FieldAccessorKind::None => unreachable!(),
            FieldAccessorKind::Regular => {
                quote! {
                    #[inline]
                    pub fn #getter_name(&self) -> & #ty {
                        &self.#field_name
                    }

                    #[inline]
                    pub fn #mutable_getter_name(&mut self) -> &mut #ty {
                        &mut self.#field_name
                    }
                }
            }
            FieldAccessorKind::Unsafe => {
                quote! {
                    #[inline]
                    pub unsafe fn #getter_name(&self) -> & #ty {
                        &self.#field_name
                    }

                    #[inline]
                    pub unsafe fn #mutable_getter_name(&mut self) -> &mut #ty {
                        &mut self.#field_name
                    }
                }
            }
            FieldAccessorKind::Immutable => {
                quote! {
                    #[inline]
                    pub fn #getter_name(&self) -> & #ty {
                        &self.#field_name
                    }
                }
            }
        }));
    }
}

impl BitfieldUnit {
    /// Get the constructor name for this bitfield unit.
    fn ctor_name(&self) -> quote::Tokens {
        let ctor_name = quote::Ident::new(format!("new_bitfield_{}", self.nth()));
        quote! {
            #ctor_name
        }
    }
}

impl Bitfield {
    /// Extend an under construction bitfield unit constructor with this
    /// bitfield. This involves two things:
    ///
    /// 1. Adding a parameter with this bitfield's name and its type.
    ///
    /// 2. Setting the relevant bits on the `__bindgen_bitfield_unit` variable
    ///    that's being constructed.
    fn extend_ctor_impl(
        &self,
        ctx: &BindgenContext,
        param_name: quote::Tokens,
        mut ctor_impl: quote::Tokens,
    ) -> quote::Tokens {
        let bitfield_ty = ctx.resolve_type(self.ty());
        let bitfield_ty_layout = bitfield_ty.layout(ctx).expect(
            "Bitfield without layout? Gah!",
        );
        let bitfield_int_ty = helpers::blob(bitfield_ty_layout);

        let offset = self.offset_into_unit();
        let width = self.width() as u8;
        let prefix = ctx.trait_prefix();

        ctor_impl.append(quote! {
            __bindgen_bitfield_unit.set(
                #offset,
                #width,
                {
                    let #param_name: #bitfield_int_ty = unsafe {
                        ::#prefix::mem::transmute(#param_name)
                    };
                    #param_name as u64
                }
            );
        });

        ctor_impl
    }
}

impl<'a> FieldCodegen<'a> for BitfieldUnit {
    type Extra = ();

    fn codegen<F, M>(
        &self,
        ctx: &BindgenContext,
        fields_should_be_private: bool,
        codegen_depth: usize,
        accessor_kind: FieldAccessorKind,
        parent: &CompInfo,
        result: &mut CodegenResult,
        struct_layout: &mut StructLayoutTracker,
        fields: &mut F,
        methods: &mut M,
        _: (),
    ) where
        F: Extend<quote::Tokens>,
        M: Extend<quote::Tokens>,
    {
        result.saw_bitfield_unit();

        let field_ty = {
            let ty = helpers::bitfield_unit(ctx, self.layout());
            if parent.is_union() && !parent.can_be_rust_union(ctx) {
                if ctx.options().enable_cxx_namespaces {
                    quote! {
                        root::__BindgenUnionField<#ty>
                    }
                } else {
                    quote! {
                        __BindgenUnionField<#ty>
                    }
                }
            } else {
                ty
            }
        };

        let unit_field_name = format!("_bitfield_{}", self.nth());
        let unit_field_ident = ctx.rust_ident(&unit_field_name);

        let field = quote! {
            pub #unit_field_ident : #field_ty ,
        };
        fields.extend(Some(field));

        let unit_field_ty = helpers::bitfield_unit(ctx, self.layout());

        let ctor_name = self.ctor_name();
        let mut ctor_params = vec![];
        let mut ctor_impl = quote! {};
        let mut generate_ctor = true;

        for bf in self.bitfields() {
            // Codegen not allowed for anonymous bitfields
            if bf.name().is_none() {
                continue;
            }

            let mut bitfield_representable_as_int = true;

            bf.codegen(
                ctx,
                fields_should_be_private,
                codegen_depth,
                accessor_kind,
                parent,
                result,
                struct_layout,
                fields,
                methods,
                (&unit_field_name, &mut bitfield_representable_as_int),
            );

            // Generating a constructor requires the bitfield to be representable as an integer.
            if !bitfield_representable_as_int {
                generate_ctor = false;
                continue;
            }

            let param_name = bitfield_getter_name(ctx, bf);
            let bitfield_ty_item = ctx.resolve_item(bf.ty());
            let bitfield_ty = bitfield_ty_item.expect_type();
            let bitfield_ty =
                bitfield_ty.to_rust_ty_or_opaque(ctx, bitfield_ty_item);

            ctor_params.push(quote! {
                #param_name : #bitfield_ty
            });
            ctor_impl = bf.extend_ctor_impl(
                ctx,
                param_name,
                ctor_impl,
            );
        }

        if generate_ctor {
            methods.extend(Some(quote! {
                #[inline]
                pub fn #ctor_name ( #( #ctor_params ),* ) -> #unit_field_ty {
                    let mut __bindgen_bitfield_unit: #unit_field_ty = Default::default();
                    #ctor_impl
                    __bindgen_bitfield_unit
                }
            }));
        }

        struct_layout.saw_bitfield_unit(self.layout());
    }
}

fn bitfield_getter_name(
    ctx: &BindgenContext,
    bitfield: &Bitfield,
) -> quote::Tokens {
    let name = bitfield.getter_name();
    let name = ctx.rust_ident_raw(name);
    quote! { #name }
}

fn bitfield_setter_name(
    ctx: &BindgenContext,
    bitfield: &Bitfield,
) -> quote::Tokens {
    let setter = bitfield.setter_name();
    let setter = ctx.rust_ident_raw(setter);
    quote! { #setter }
}

impl<'a> FieldCodegen<'a> for Bitfield {
    type Extra = (&'a str, &'a mut bool);

    fn codegen<F, M>(
        &self,
        ctx: &BindgenContext,
        _fields_should_be_private: bool,
        _codegen_depth: usize,
        _accessor_kind: FieldAccessorKind,
        parent: &CompInfo,
        _result: &mut CodegenResult,
        _struct_layout: &mut StructLayoutTracker,
        _fields: &mut F,
        methods: &mut M,
        (unit_field_name, bitfield_representable_as_int): (&'a str, &mut bool),
    ) where
        F: Extend<quote::Tokens>,
        M: Extend<quote::Tokens>,
    {
        let prefix = ctx.trait_prefix();
        let getter_name = bitfield_getter_name(ctx, self);
        let setter_name = bitfield_setter_name(ctx, self);
        let unit_field_ident = quote::Ident::new(unit_field_name);

        let bitfield_ty_item = ctx.resolve_item(self.ty());
        let bitfield_ty = bitfield_ty_item.expect_type();

        let bitfield_ty_layout = bitfield_ty.layout(ctx).expect(
            "Bitfield without layout? Gah!",
        );
        let bitfield_int_ty = match helpers::integer_type(bitfield_ty_layout) {
            Some(int_ty) => {
                *bitfield_representable_as_int = true;
                int_ty
            }
            None => {
                *bitfield_representable_as_int = false;
                return;
            }
        };

        let bitfield_ty =
            bitfield_ty.to_rust_ty_or_opaque(ctx, bitfield_ty_item);

        let offset = self.offset_into_unit();

        let width = self.width() as u8;

        if parent.is_union() && !parent.can_be_rust_union(ctx) {
            methods.extend(Some(quote! {
                #[inline]
                pub fn #getter_name(&self) -> #bitfield_ty {
                    unsafe {
                        ::#prefix::mem::transmute(
                            self.#unit_field_ident.as_ref().get(#offset, #width)
                                as #bitfield_int_ty
                        )
                    }
                }

                #[inline]
                pub fn #setter_name(&mut self, val: #bitfield_ty) {
                    unsafe {
                        let val: #bitfield_int_ty = ::#prefix::mem::transmute(val);
                        self.#unit_field_ident.as_mut().set(
                            #offset,
                            #width,
                            val as u64
                        )
                    }
                }
            }));
        } else {
            methods.extend(Some(quote! {
                #[inline]
                pub fn #getter_name(&self) -> #bitfield_ty {
                    unsafe {
                        ::#prefix::mem::transmute(
                            self.#unit_field_ident.get(#offset, #width)
                                as #bitfield_int_ty
                        )
                    }
                }

                #[inline]
                pub fn #setter_name(&mut self, val: #bitfield_ty) {
                    unsafe {
                        let val: #bitfield_int_ty = ::#prefix::mem::transmute(val);
                        self.#unit_field_ident.set(
                            #offset,
                            #width,
                            val as u64
                        )
                    }
                }
            }));
        }
    }
}

impl CodeGenerator for CompInfo {
    type Extra = Item;

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        item: &Item,
    ) {
        debug!("<CompInfo as CodeGenerator>::codegen: item = {:?}", item);
        debug_assert!(item.is_enabled_for_codegen(ctx));

        // Don't output classes with template parameters that aren't types, and
        // also don't output template specializations, neither total or partial.
        if self.has_non_type_template_params() {
            return;
        }

        let used_template_params = item.used_template_params(ctx);

        let ty = item.expect_type();
        let layout = ty.layout(ctx);
        let mut packed = self.is_packed(ctx, &layout);

        // generate tuple struct if struct or union is a forward declaration,
        // skip for now if template parameters are needed.
        //
        // NB: We generate a proper struct to avoid struct/function name
        // collisions.
        if self.is_forward_declaration() && used_template_params.is_none() {
            let struct_name = item.canonical_name(ctx);
            let struct_name = ctx.rust_ident_raw(struct_name);
            let tuple_struct = quote! {
                #[repr(C)]
                #[derive(Debug, Copy, Clone)]
                pub struct #struct_name { _unused: [u8; 0] }
            };
            result.push(tuple_struct);
            return;
        }

        let canonical_name = item.canonical_name(ctx);
        let canonical_ident = ctx.rust_ident(&canonical_name);

        // Generate the vtable from the method list if appropriate.
        //
        // TODO: I don't know how this could play with virtual methods that are
        // not in the list of methods found by us, we'll see. Also, could the
        // order of the vtable pointers vary?
        //
        // FIXME: Once we generate proper vtables, we need to codegen the
        // vtable, but *not* generate a field for it in the case that
        // HasVtable::has_vtable_ptr is false but HasVtable::has_vtable is true.
        //
        // Also, we need to generate the vtable in such a way it "inherits" from
        // the parent too.
        let is_opaque = item.is_opaque(ctx, &());
        let mut fields = vec![];
        let mut struct_layout =
            StructLayoutTracker::new(ctx, self, ty, &canonical_name);

        if !is_opaque {
            if item.has_vtable_ptr(ctx) {
                let vtable =
                    Vtable::new(item.id(), self.methods(), self.base_members());
                vtable.codegen(ctx, result, item);

                let vtable_type = vtable
                    .try_to_rust_ty(ctx, &())
                    .expect("vtable to Rust type conversion is infallible")
                    .to_ptr(true);

                fields.push(quote! {
                    pub vtable_: #vtable_type ,
                });

                struct_layout.saw_vtable();
            }

            for base in self.base_members() {
                if !base.requires_storage(ctx) {
                    continue;
                }

                let inner = base.ty.to_rust_ty_or_opaque(ctx, &());
                let field_name = ctx.rust_ident(&base.field_name);

                let base_ty = ctx.resolve_type(base.ty);
                struct_layout.saw_base(base_ty);

                fields.push(quote! {
                    pub #field_name : #inner ,
                });
            }
        }

        let is_union = self.kind() == CompKind::Union;
        if is_union {
            result.saw_union();
            if !self.can_be_rust_union(ctx) {
                result.saw_bindgen_union();
            }
        }

        let mut methods = vec![];
        if !is_opaque {
            let codegen_depth = item.codegen_depth(ctx);
            let fields_should_be_private =
                item.annotations().private_fields().unwrap_or(false);
            let struct_accessor_kind = item.annotations()
                .accessor_kind()
                .unwrap_or(FieldAccessorKind::None);
            for field in self.fields() {
                field.codegen(
                    ctx,
                    fields_should_be_private,
                    codegen_depth,
                    struct_accessor_kind,
                    self,
                    result,
                    &mut struct_layout,
                    &mut fields,
                    &mut methods,
                    (),
                );
            }
        }

        let layout = item.kind().expect_type().layout(ctx);
        if is_union && !is_opaque {
            let layout = layout.expect("Unable to get layout information?");
            let ty = helpers::blob(layout);

            fields.push(if self.can_be_rust_union(ctx) {
                quote! {
                    _bindgen_union_align: #ty ,
                }
            } else {
                struct_layout.saw_union(layout);

                quote! {
                    pub bindgen_union_field: #ty ,
                }
            });
        }

        if is_opaque {
            // Opaque item should not have generated methods, fields.
            debug_assert!(fields.is_empty());
            debug_assert!(methods.is_empty());

            match layout {
                Some(l) => {
                    let ty = helpers::blob(l);
                    fields.push(quote! {
                        pub _bindgen_opaque_blob: #ty ,
                    });
                }
                None => {
                    warn!("Opaque type without layout! Expect dragons!");
                }
            }
        } else if !is_union && !item.is_zero_sized(ctx) {
            if let Some(padding_field) =
                layout.and_then(|layout| struct_layout.pad_struct(layout))
            {
                fields.push(padding_field);
            }

            if let Some(layout) = layout {
                if struct_layout.requires_explicit_align(layout) {
                    if layout.align == 1 {
                        packed = true;
                    } else {
                        let ty = helpers::blob(Layout::new(0, layout.align));
                        fields.push(quote! {
                            pub __bindgen_align: #ty ,
                        });
                    }
                }
            }
        }

        // C++ requires every struct to be addressable, so what C++ compilers do
        // is making the struct 1-byte sized.
        //
        // This is apparently not the case for C, see:
        // https://github.com/rust-lang-nursery/rust-bindgen/issues/551
        //
        // Just get the layout, and assume C++ if not.
        //
        // NOTE: This check is conveniently here to avoid the dummy fields we
        // may add for unused template parameters.
        if item.is_zero_sized(ctx) {
            let has_address = if is_opaque {
                // Generate the address field if it's an opaque type and
                // couldn't determine the layout of the blob.
                layout.is_none()
            } else {
                layout.map_or(true, |l| l.size != 0)
            };

            if has_address {
                let ty = helpers::blob(Layout::new(1, 1));
                fields.push(quote! {
                    pub _address: #ty,
                });
            }
        }

        let mut generic_param_names = vec![];

        if let Some(ref params) = used_template_params {
            for (idx, ty) in params.iter().enumerate() {
                let param = ctx.resolve_type(*ty);
                let name = param.name().unwrap();
                let ident = ctx.rust_ident(name);
                generic_param_names.push(ident.clone());

                let prefix = ctx.trait_prefix();
                let field_name = ctx.rust_ident(format!("_phantom_{}", idx));
                fields.push(quote! {
                    pub #field_name : ::#prefix::marker::PhantomData<
                        ::#prefix::cell::UnsafeCell<#ident>
                    > ,
                });
            }
        }

        let generics = if !generic_param_names.is_empty() {
            let generic_param_names = generic_param_names.clone();
            quote! {
                < #( #generic_param_names ),* >
            }
        } else {
            quote! { }
        };

        let mut attributes = vec![];
        let mut needs_clone_impl = false;
        let mut needs_default_impl = false;
        let mut needs_debug_impl = false;
        let mut needs_partialeq_impl = false;
        if let Some(comment) = item.comment(ctx) {
            attributes.push(attributes::doc(comment));
        }
        if packed && !is_opaque {
            attributes.push(attributes::repr_list(&["C", "packed"]));
        } else {
            attributes.push(attributes::repr("C"));
        }

        let mut derives = vec![];
        if item.can_derive_debug(ctx) {
            derives.push("Debug");
        } else {
            needs_debug_impl = ctx.options().derive_debug &&
                ctx.options().impl_debug
        }

        if item.can_derive_default(ctx) {
            derives.push("Default");
        } else {
            needs_default_impl = ctx.options().derive_default;
        }

        if item.can_derive_copy(ctx) && !item.annotations().disallow_copy() &&
            ctx.options().derive_copy
        {
            derives.push("Copy");

            if ctx.options().rust_features().builtin_clone_impls() ||
                used_template_params.is_some()
            {
                // FIXME: This requires extra logic if you have a big array in a
                // templated struct. The reason for this is that the magic:
                //     fn clone(&self) -> Self { *self }
                // doesn't work for templates.
                //
                // It's not hard to fix though.
                derives.push("Clone");
            } else {
                needs_clone_impl = true;
            }
        }

        if item.can_derive_hash(ctx) {
            derives.push("Hash");
        }

        if item.can_derive_partialord(ctx) {
            derives.push("PartialOrd");
        }

        if item.can_derive_ord(ctx) {
            derives.push("Ord");
        }

        if item.can_derive_partialeq(ctx) {
            derives.push("PartialEq");
        } else {
            needs_partialeq_impl =
                ctx.options().derive_partialeq &&
                ctx.options().impl_partialeq &&
                ctx.lookup_can_derive_partialeq_or_partialord(item.id()) == CanDerive::ArrayTooLarge;
        }

        if item.can_derive_eq(ctx) {
            derives.push("Eq");
        }

        if !derives.is_empty() {
            attributes.push(attributes::derives(&derives))
        }

        let mut tokens = if is_union && self.can_be_rust_union(ctx) {
            quote! {
                #( #attributes )*
                pub union #canonical_ident
            }
        } else {
            quote! {
                #( #attributes )*
                pub struct #canonical_ident
            }
        };

        tokens.append(quote! {
            #generics {
                #( #fields )*
            }
        });
        result.push(tokens);

        // Generate the inner types and all that stuff.
        //
        // TODO: In the future we might want to be smart, and use nested
        // modules, and whatnot.
        for ty in self.inner_types() {
            let child_item = ctx.resolve_item(*ty);
            // assert_eq!(child_item.parent_id(), item.id());
            child_item.codegen(ctx, result, &());
        }

        // NOTE: Some unexposed attributes (like alignment attributes) may
        // affect layout, so we're bad and pray to the gods for avoid sending
        // all the tests to shit when parsing things like max_align_t.
        if self.found_unknown_attr() {
            warn!(
                "Type {} has an unkown attribute that may affect layout",
                canonical_ident
            );
        }

        if used_template_params.is_none() {
            if !is_opaque {
                for var in self.inner_vars() {
                    ctx.resolve_item(*var).codegen(ctx, result, &());
                }
            }

            if ctx.options().layout_tests {
                if let Some(layout) = layout {
                    let fn_name =
                        format!("bindgen_test_layout_{}", canonical_ident);
                    let fn_name = ctx.rust_ident_raw(fn_name);
                    let prefix = ctx.trait_prefix();
                    let size_of_expr = quote! {
                        ::#prefix::mem::size_of::<#canonical_ident>()
                    };
                    let align_of_expr = quote! {
                        ::#prefix::mem::align_of::<#canonical_ident>()
                    };
                    let size = layout.size;
                    let align = layout.align;

                    let check_struct_align =
                        if align > mem::size_of::<*mut ()>() {
                            // FIXME when [RFC 1358](https://github.com/rust-lang/rust/issues/33626) ready
                            None
                        } else {
                            Some(quote! {
                                assert_eq!(#align_of_expr,
                                       #align,
                                       concat!("Alignment of ", stringify!(#canonical_ident)));

                            })
                        };

                    // FIXME when [issue #465](https://github.com/rust-lang-nursery/rust-bindgen/issues/465) ready
                    let too_many_base_vtables = self.base_members()
                        .iter()
                        .filter(|base| base.ty.has_vtable(ctx))
                        .count() > 1;

                    let should_skip_field_offset_checks = is_opaque ||
                        too_many_base_vtables;

                    let check_field_offset =
                        if should_skip_field_offset_checks {
                            vec![]
                        } else {
                            let asserts = self.fields()
                                .iter()
                                .filter_map(|field| match *field {
                                    Field::DataMember(ref f) if f.name().is_some() => Some(f),
                                    _ => None,
                                })
                                .flat_map(|field| {
                                    let name = field.name().unwrap();
                                    field.offset().and_then(|offset| {
                                        let field_offset = offset / 8;
                                        let field_name = ctx.rust_ident(name);

                                        Some(quote! {
                                            assert_eq!(
                                                unsafe {
                                                    &(*(::#prefix::ptr::null::<#canonical_ident>())).#field_name as *const _ as usize
                                                },
                                                #field_offset,
                                                concat!("Offset of field: ", stringify!(#canonical_ident), "::", stringify!(#field_name))
                                            );
                                        })
                                    })
                                })
                                .collect::<Vec<quote::Tokens>>();

                            asserts
                        };

                    let item = quote! {
                        #[test]
                        fn #fn_name() {
                            assert_eq!(#size_of_expr,
                                       #size,
                                       concat!("Size of: ", stringify!(#canonical_ident)));

                            #check_struct_align
                            #( #check_field_offset )*
                        }
                    };
                    result.push(item);
                }
            }

            let mut method_names = Default::default();
            if ctx.options().codegen_config.methods {
                for method in self.methods() {
                    assert!(method.kind() != MethodKind::Constructor);
                    method.codegen_method(
                        ctx,
                        &mut methods,
                        &mut method_names,
                        result,
                        self,
                    );
                }
            }

            if ctx.options().codegen_config.constructors {
                for sig in self.constructors() {
                    Method::new(
                        MethodKind::Constructor,
                        *sig,
                        /* const */
                        false,
                    ).codegen_method(
                        ctx,
                        &mut methods,
                        &mut method_names,
                        result,
                        self,
                    );
                }
            }

            if ctx.options().codegen_config.destructors {
                if let Some((kind, destructor)) = self.destructor() {
                    debug_assert!(kind.is_destructor());
                    Method::new(kind, destructor, false).codegen_method(
                        ctx,
                        &mut methods,
                        &mut method_names,
                        result,
                        self,
                    );
                }
            }
        }

        // NB: We can't use to_rust_ty here since for opaque types this tries to
        // use the specialization knowledge to generate a blob field.
        let ty_for_impl = quote! {
            #canonical_ident #generics
        };

        if needs_clone_impl {
            result.push(quote! {
                impl #generics Clone for #ty_for_impl {
                    fn clone(&self) -> Self { *self }
                }
            });
        }

        if needs_default_impl {
            let prefix = ctx.trait_prefix();
            result.push(quote! {
                impl #generics Default for #ty_for_impl {
                    fn default() -> Self { unsafe { ::#prefix::mem::zeroed() } }
                }
            });
        }

        if needs_debug_impl {
            let impl_ = impl_debug::gen_debug_impl(
                ctx,
                self.fields(),
                item,
                self.kind(),
            );

            result.push(quote! {
                impl #generics ::std::fmt::Debug for #ty_for_impl {
                    #impl_
                }
            });
        }

        if needs_partialeq_impl {
            if let Some(impl_) = impl_partialeq::gen_partialeq_impl(ctx, self, item, &ty_for_impl) {

                let partialeq_bounds = if !generic_param_names.is_empty() {
                    let bounds = generic_param_names.iter().map(|t| {
                        quote! { #t: PartialEq }
                    });
                    quote! { where #( #bounds ),* }
                } else {
                    quote! { }
                };

                let prefix = ctx.trait_prefix();
                result.push(quote! {
                    impl #generics ::#prefix::cmp::PartialEq for #ty_for_impl #partialeq_bounds {
                        #impl_
                    }
                });
            }
        }

        if !methods.is_empty() {
            result.push(quote! {
                impl #generics #ty_for_impl {
                    #( #methods )*
                }
            });
        }
    }
}

trait MethodCodegen {
    fn codegen_method<'a>(
        &self,
        ctx: &BindgenContext,
        methods: &mut Vec<quote::Tokens>,
        method_names: &mut HashMap<String, usize>,
        result: &mut CodegenResult<'a>,
        parent: &CompInfo,
    );
}

impl MethodCodegen for Method {
    fn codegen_method<'a>(
        &self,
        ctx: &BindgenContext,
        methods: &mut Vec<quote::Tokens>,
        method_names: &mut HashMap<String, usize>,
        result: &mut CodegenResult<'a>,
        _parent: &CompInfo,
    ) {
        assert!({
            let cc = &ctx.options().codegen_config;
            match self.kind() {
                MethodKind::Constructor => cc.constructors,
                MethodKind::Destructor => cc.destructors,
                MethodKind::VirtualDestructor { .. } => cc.destructors,
                MethodKind::Static | MethodKind::Normal |
                MethodKind::Virtual { .. } => cc.methods,
            }
        });

        if self.is_virtual() {
            return; // FIXME
        }

        // First of all, output the actual function.
        let function_item = ctx.resolve_item(self.signature());
        function_item.codegen(ctx, result, &());

        let function = function_item.expect_function();
        let signature_item = ctx.resolve_item(function.signature());
        let mut name = match self.kind() {
            MethodKind::Constructor => "new".into(),
            MethodKind::Destructor => "destruct".into(),
            _ => function.name().to_owned(),
        };

        let signature = match *signature_item.expect_type().kind() {
            TypeKind::Function(ref sig) => sig,
            _ => panic!("How in the world?"),
        };

        if let (Abi::ThisCall, false) = (signature.abi(), ctx.options().rust_features().thiscall_abi()) {
            return;
        }

        // Do not generate variadic methods, since rust does not allow
        // implementing them, and we don't do a good job at it anyway.
        if signature.is_variadic() {
            return;
        }

        let count = {
            let count = method_names.entry(name.clone()).or_insert(0);
            *count += 1;
            *count - 1
        };

        if count != 0 {
            name.push_str(&count.to_string());
        }

        let function_name = ctx.rust_ident(function_item.canonical_name(ctx));
        let mut args = utils::fnsig_arguments(ctx, signature);
        let mut ret = utils::fnsig_return_ty(ctx, signature);

        if !self.is_static() && !self.is_constructor() {
            args[0] = if self.is_const() {
                quote! { &self }
            } else {
                quote! { &mut self }
            };
        }

        // If it's a constructor, we always return `Self`, and we inject the
        // "this" parameter, so there's no need to ask the user for it.
        //
        // Note that constructors in Clang are represented as functions with
        // return-type = void.
        if self.is_constructor() {
            args.remove(0);
            ret = quote! { -> Self };
        }

        let mut exprs =
            helpers::ast_ty::arguments_from_signature(&signature, ctx);

        let mut stmts = vec![];

        // If it's a constructor, we need to insert an extra parameter with a
        // variable called `__bindgen_tmp` we're going to create.
        if self.is_constructor() {
            let prefix = ctx.trait_prefix();
            let tmp_variable_decl =
                quote! {
                    let mut __bindgen_tmp = ::#prefix::mem::uninitialized()
                };
            stmts.push(tmp_variable_decl);
            exprs[0] = quote! {
                &mut __bindgen_tmp
            };
        } else if !self.is_static() {
            assert!(!exprs.is_empty());
            exprs[0] = quote! {
                self
            };
        };

        let call = quote! {
            #function_name (#( #exprs ),* )
        };

        stmts.push(call);

        if self.is_constructor() {
            stmts.push(quote! {
                __bindgen_tmp
            });
        }

        let block = quote! {
            #( #stmts );*
        };

        let mut attrs = vec![];
        attrs.push(attributes::inline());

        let name = ctx.rust_ident(&name);
        methods.push(quote! {
            #[inline]
            pub unsafe fn #name ( #( #args ),* ) #ret {
                #block
            }
        });
    }
}

/// A helper type that represents different enum variations.
#[derive(Copy, Clone)]
enum EnumVariation {
    Rust,
    Bitfield,
    Consts,
    ModuleConsts
}

impl EnumVariation {
    fn is_rust(&self) -> bool {
        match *self {
            EnumVariation::Rust => true,
            _ => false
        }
    }

    fn is_bitfield(&self) -> bool {
        match *self {
            EnumVariation::Bitfield => true,
            _ => false
        }
    }

    /// Both the `Const` and `ModuleConsts` variants will cause this to return
    /// true.
    fn is_const(&self) -> bool {
        match *self {
            EnumVariation::Consts | EnumVariation::ModuleConsts => true,
            _ => false
        }
    }
}

/// A helper type to construct different enum variations.
enum EnumBuilder<'a> {
    Rust {
        tokens: quote::Tokens,
        emitted_any_variants: bool,
    },
    Bitfield {
        canonical_name: &'a str,
        tokens: quote::Tokens,
    },
    Consts(Vec<quote::Tokens>),
    ModuleConsts {
        module_name: &'a str,
        module_items: Vec<quote::Tokens>,
    },
}

impl<'a> EnumBuilder<'a> {
    /// Create a new enum given an item builder, a canonical name, a name for
    /// the representation, and which variation it should be generated as.
    fn new(
        name: &'a str,
        attrs: Vec<quote::Tokens>,
        repr: quote::Tokens,
        enum_variation: EnumVariation
    ) -> Self {
        let ident = quote::Ident::new(name);

        match enum_variation {
            EnumVariation::Bitfield => {
                EnumBuilder::Bitfield {
                    canonical_name: name,
                    tokens: quote! {
                        #( #attrs )*
                        pub struct #ident (pub #repr);
                    },
                }
            }

            EnumVariation::Rust => {
                let mut tokens = quote! {
                    #( #attrs )*
                    pub enum #ident
                };
                tokens.append("{");
                EnumBuilder::Rust {
                    tokens,
                    emitted_any_variants: false,
                }
            }

            EnumVariation::Consts => {
                EnumBuilder::Consts(vec![
                    quote! {
                        pub type #ident = #repr;
                    }
                ])
            }

            EnumVariation::ModuleConsts => {
                let ident = quote::Ident::new(CONSTIFIED_ENUM_MODULE_REPR_NAME);
                let type_definition = quote! {
                    pub type #ident = #repr;
                };

                EnumBuilder::ModuleConsts {
                    module_name: name,
                    module_items: vec![type_definition],
                }
            }
        }
    }

    /// Add a variant to this enum.
    fn with_variant<'b>(
        self,
        ctx: &BindgenContext,
        variant: &EnumVariant,
        mangling_prefix: Option<&String>,
        rust_ty: quote::Tokens,
        result: &mut CodegenResult<'b>,
    ) -> Self {
        let variant_name = ctx.rust_mangle(variant.name());
        let expr = match variant.val() {
            EnumVariantValue::Signed(v) => helpers::ast_ty::int_expr(v),
            EnumVariantValue::Unsigned(v) => helpers::ast_ty::uint_expr(v),
        };

        match self {
            EnumBuilder::Rust { tokens, emitted_any_variants: _ } => {
                let name = ctx.rust_ident(variant_name);
                EnumBuilder::Rust {
                    tokens: quote! {
                        #tokens
                        #name = #expr,
                    },
                    emitted_any_variants: true,
                }
            }

            EnumBuilder::Bitfield { .. } => {
                let constant_name = match mangling_prefix {
                    Some(prefix) => {
                        Cow::Owned(format!("{}_{}", prefix, variant_name))
                    }
                    None => variant_name,
                };

                let ident = ctx.rust_ident(constant_name);
                result.push(quote! {
                    pub const #ident : #rust_ty = #rust_ty ( #expr );
                });

                self
            }

            EnumBuilder::Consts {
                ..
            } => {
                let constant_name = match mangling_prefix {
                    Some(prefix) => {
                        Cow::Owned(format!("{}_{}", prefix, variant_name))
                    }
                    None => variant_name,
                };

                let ident = ctx.rust_ident(constant_name);
                result.push(quote! {
                    pub const #ident : #rust_ty = #expr ;
                });

                self
            }
            EnumBuilder::ModuleConsts {
                module_name,
                mut module_items,
            } => {
                let name = ctx.rust_ident(variant_name);
                let ty = ctx.rust_ident(CONSTIFIED_ENUM_MODULE_REPR_NAME);
                module_items.push(quote! {
                    pub const #name : #ty = #expr ;
                });

                EnumBuilder::ModuleConsts {
                    module_name,
                    module_items,
                }
            }
        }
    }

    fn build<'b>(
        self,
        ctx: &BindgenContext,
        rust_ty: quote::Tokens,
        result: &mut CodegenResult<'b>,
    ) -> quote::Tokens {
        match self {
            EnumBuilder::Rust { mut tokens, emitted_any_variants } => {
                if !emitted_any_variants {
                    tokens.append(quote! { __bindgen_cannot_repr_c_on_empty_enum = 0 });
                }
                tokens.append("}");
                tokens
            }
            EnumBuilder::Bitfield {
                canonical_name,
                tokens,
            } => {
                let rust_ty_name = ctx.rust_ident_raw(canonical_name);
                let prefix = ctx.trait_prefix();

                result.push(quote! {
                    impl ::#prefix::ops::BitOr<#rust_ty> for #rust_ty {
                        type Output = Self;

                        #[inline]
                        fn bitor(self, other: Self) -> Self {
                            #rust_ty_name(self.0 | other.0)
                        }
                    }
                });

                result.push(quote! {
                    impl ::#prefix::ops::BitOrAssign for #rust_ty {
                        #[inline]
                        fn bitor_assign(&mut self, rhs: #rust_ty) {
                            self.0 |= rhs.0;
                        }
                    }
                });

                result.push(quote! {
                    impl ::#prefix::ops::BitAnd<#rust_ty> for #rust_ty {
                        type Output = Self;

                        #[inline]
                        fn bitand(self, other: Self) -> Self {
                            #rust_ty_name(self.0 & other.0)
                        }
                    }
                });

                result.push(quote! {
                    impl ::#prefix::ops::BitAndAssign for #rust_ty {
                        #[inline]
                        fn bitand_assign(&mut self, rhs: #rust_ty) {
                            self.0 &= rhs.0;
                        }
                    }
                });

                tokens
            }
            EnumBuilder::Consts(tokens) => quote! { #( #tokens )* },
            EnumBuilder::ModuleConsts {
                module_items,
                module_name,
            } => {
                let ident = ctx.rust_ident(module_name);
                quote! {
                    pub mod #ident {
                        #( #module_items )*
                    }
                }
            }
        }
    }
}

impl CodeGenerator for Enum {
    type Extra = Item;

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        item: &Item,
    ) {
        debug!("<Enum as CodeGenerator>::codegen: item = {:?}", item);
        debug_assert!(item.is_enabled_for_codegen(ctx));

        let name = item.canonical_name(ctx);
        let ident = ctx.rust_ident(&name);
        let enum_ty = item.expect_type();
        let layout = enum_ty.layout(ctx);

        let repr = self.repr().map(|repr| ctx.resolve_type(repr));
        let repr = match repr {
            Some(repr) => {
                match *repr.canonical_type(ctx).kind() {
                    TypeKind::Int(int_kind) => int_kind,
                    _ => panic!("Unexpected type as enum repr"),
                }
            }
            None => {
                warn!(
                    "Guessing type of enum! Forward declarations of enums \
                     shouldn't be legal!"
                );
                IntKind::Int
            }
        };

        let signed = repr.is_signed();
        let size = layout
            .map(|l| l.size)
            .or_else(|| repr.known_size())
            .unwrap_or(0);

        let repr_name = match (signed, size) {
            (true, 1) => "i8",
            (false, 1) => "u8",
            (true, 2) => "i16",
            (false, 2) => "u16",
            (true, 4) => "i32",
            (false, 4) => "u32",
            (true, 8) => "i64",
            (false, 8) => "u64",
            _ => {
                warn!("invalid enum decl: signed: {}, size: {}", signed, size);
                "i32"
            }
        };

        let variation = if self.is_bitfield(ctx, item) {
            EnumVariation::Bitfield
        } else if self.is_rustified_enum(ctx, item) {
            EnumVariation::Rust
        } else if self.is_constified_enum_module(ctx, item) {
            EnumVariation::ModuleConsts
        } else {
            // We generate consts by default
            EnumVariation::Consts
        };

        let mut attrs = vec![];

        // TODO(emilio): Delegate this to the builders?
        if variation.is_rust() {
            attrs.push(attributes::repr(repr_name));
        } else if variation.is_bitfield() {
            attrs.push(attributes::repr("C"));
        }

        if let Some(comment) = item.comment(ctx) {
            attrs.push(attributes::doc(comment));
        }

        if !variation.is_const() {
            attrs.push(attributes::derives(
                &["Debug", "Copy", "Clone", "PartialEq", "Eq", "Hash"],
            ));
        }

        fn add_constant<'a>(
            ctx: &BindgenContext,
            enum_: &Type,
            // Only to avoid recomputing every time.
            enum_canonical_name: &quote::Ident,
            // May be the same as "variant" if it's because the
            // enum is unnamed and we still haven't seen the
            // value.
            variant_name: &str,
            referenced_name: &quote::Ident,
            enum_rust_ty: quote::Tokens,
            result: &mut CodegenResult<'a>,
        ) {
            let constant_name = if enum_.name().is_some() {
                if ctx.options().prepend_enum_name {
                    format!("{}_{}", enum_canonical_name, variant_name)
                } else {
                    variant_name.into()
                }
            } else {
                variant_name.into()
            };
            let constant_name = ctx.rust_ident(constant_name);

            result.push(quote! {
                pub const #constant_name : #enum_rust_ty =
                    #enum_canonical_name :: #referenced_name ;
            });
        }

        let repr = {
            let repr_name = ctx.rust_ident_raw(repr_name);
            quote! { #repr_name }
        };

        let mut builder = EnumBuilder::new(
            &name,
            attrs,
            repr,
            variation
        );

        // A map where we keep a value -> variant relation.
        let mut seen_values = HashMap::<_, quote::Ident>::new();
        let enum_rust_ty = item.to_rust_ty_or_opaque(ctx, &());
        let is_toplevel = item.is_toplevel(ctx);

        // Used to mangle the constants we generate in the unnamed-enum case.
        let parent_canonical_name = if is_toplevel {
            None
        } else {
            Some(item.parent_id().canonical_name(ctx))
        };

        let constant_mangling_prefix = if ctx.options().prepend_enum_name {
            if enum_ty.name().is_none() {
                parent_canonical_name.as_ref().map(|n| &*n)
            } else {
                Some(&name)
            }
        } else {
            None
        };

        // NB: We defer the creation of constified variants, in case we find
        // another variant with the same value (which is the common thing to
        // do).
        let mut constified_variants = VecDeque::new();

        let mut iter = self.variants().iter().peekable();
        while let Some(variant) = iter.next().or_else(|| {
            constified_variants.pop_front()
        })
        {
            if variant.hidden() {
                continue;
            }

            if variant.force_constification() && iter.peek().is_some() {
                constified_variants.push_back(variant);
                continue;
            }

            match seen_values.entry(variant.val()) {
                Entry::Occupied(ref entry) => {
                    if variation.is_rust() {
                        let variant_name = ctx.rust_mangle(variant.name());
                        let mangled_name =
                            if is_toplevel || enum_ty.name().is_some() {
                                variant_name
                            } else {
                                let parent_name =
                                    parent_canonical_name.as_ref().unwrap();

                                Cow::Owned(
                                    format!("{}_{}", parent_name, variant_name),
                                )
                            };

                        let existing_variant_name = entry.get();
                        add_constant(
                            ctx,
                            enum_ty,
                            &ident,
                            &*mangled_name,
                            existing_variant_name,
                            enum_rust_ty.clone(),
                            result,
                        );
                    } else {
                        builder = builder.with_variant(
                            ctx,
                            variant,
                            constant_mangling_prefix,
                            enum_rust_ty.clone(),
                            result,
                        );
                    }
                }
                Entry::Vacant(entry) => {
                    builder = builder.with_variant(
                        ctx,
                        variant,
                        constant_mangling_prefix,
                        enum_rust_ty.clone(),
                        result,
                    );

                    let variant_name = ctx.rust_ident(variant.name());

                    // If it's an unnamed enum, or constification is enforced,
                    // we also generate a constant so it can be properly
                    // accessed.
                    if (variation.is_rust() && enum_ty.name().is_none()) ||
                        variant.force_constification()
                    {
                        let mangled_name = if is_toplevel {
                            variant_name.clone()
                        } else {
                            let parent_name =
                                parent_canonical_name.as_ref().unwrap();

                            quote::Ident::new(
                                format!(
                                    "{}_{}",
                                    parent_name,
                                    variant_name
                                )
                            )
                        };

                        add_constant(
                            ctx,
                            enum_ty,
                            &ident,
                            mangled_name.as_ref(),
                            &variant_name,
                            enum_rust_ty.clone(),
                            result,
                        );
                    }

                    entry.insert(quote::Ident::new(variant_name));
                }
            }
        }

        let item = builder.build(ctx, enum_rust_ty, result);
        result.push(item);
    }
}

/// Fallible conversion to an opaque blob.
///
/// Implementors of this trait should provide the `try_get_layout` method to
/// fallibly get this thing's layout, which the provided `try_to_opaque` trait
/// method will use to convert the `Layout` into an opaque blob Rust type.
trait TryToOpaque {
    type Extra;

    /// Get the layout for this thing, if one is available.
    fn try_get_layout(
        &self,
        ctx: &BindgenContext,
        extra: &Self::Extra,
    ) -> error::Result<Layout>;

    /// Do not override this provided trait method.
    fn try_to_opaque(
        &self,
        ctx: &BindgenContext,
        extra: &Self::Extra,
    ) -> error::Result<quote::Tokens> {
        self.try_get_layout(ctx, extra).map(|layout| {
            helpers::blob(layout)
        })
    }
}

/// Infallible conversion of an IR thing to an opaque blob.
///
/// The resulting layout is best effort, and is unfortunately not guaranteed to
/// be correct. When all else fails, we fall back to a single byte layout as a
/// last resort, because C++ does not permit zero-sized types. See the note in
/// the `ToRustTyOrOpaque` doc comment about fallible versus infallible traits
/// and when each is appropriate.
///
/// Don't implement this directly. Instead implement `TryToOpaque`, and then
/// leverage the blanket impl for this trait.
trait ToOpaque: TryToOpaque {
    fn get_layout(&self, ctx: &BindgenContext, extra: &Self::Extra) -> Layout {
        self.try_get_layout(ctx, extra).unwrap_or_else(
            |_| Layout::for_size(1),
        )
    }

    fn to_opaque(
        &self,
        ctx: &BindgenContext,
        extra: &Self::Extra,
    ) -> quote::Tokens {
        let layout = self.get_layout(ctx, extra);
        helpers::blob(layout)
    }
}

impl<T> ToOpaque for T
where
    T: TryToOpaque,
{
}

/// Fallible conversion from an IR thing to an *equivalent* Rust type.
///
/// If the C/C++ construct represented by the IR thing cannot (currently) be
/// represented in Rust (for example, instantiations of templates with
/// const-value generic parameters) then the impl should return an `Err`. It
/// should *not* attempt to return an opaque blob with the correct size and
/// alignment. That is the responsibility of the `TryToOpaque` trait.
trait TryToRustTy {
    type Extra;

    fn try_to_rust_ty(
        &self,
        ctx: &BindgenContext,
        extra: &Self::Extra,
    ) -> error::Result<quote::Tokens>;
}

/// Fallible conversion to a Rust type or an opaque blob with the correct size
/// and alignment.
///
/// Don't implement this directly. Instead implement `TryToRustTy` and
/// `TryToOpaque`, and then leverage the blanket impl for this trait below.
trait TryToRustTyOrOpaque: TryToRustTy + TryToOpaque {
    type Extra;

    fn try_to_rust_ty_or_opaque(
        &self,
        ctx: &BindgenContext,
        extra: &<Self as TryToRustTyOrOpaque>::Extra,
    ) -> error::Result<quote::Tokens>;
}

impl<E, T> TryToRustTyOrOpaque for T
where
    T: TryToRustTy<Extra = E>
        + TryToOpaque<Extra = E>,
{
    type Extra = E;

    fn try_to_rust_ty_or_opaque(
        &self,
        ctx: &BindgenContext,
        extra: &E,
    ) -> error::Result<quote::Tokens> {
        self.try_to_rust_ty(ctx, extra).or_else(
            |_| if let Ok(layout) =
                self.try_get_layout(ctx, extra)
            {
                Ok(helpers::blob(layout))
            } else {
                Err(error::Error::NoLayoutForOpaqueBlob)
            },
        )
    }
}

/// Infallible conversion to a Rust type, or an opaque blob with a best effort
/// of correct size and alignment.
///
/// Don't implement this directly. Instead implement `TryToRustTy` and
/// `TryToOpaque`, and then leverage the blanket impl for this trait below.
///
/// ### Fallible vs. Infallible Conversions to Rust Types
///
/// When should one use this infallible `ToRustTyOrOpaque` trait versus the
/// fallible `TryTo{RustTy, Opaque, RustTyOrOpaque}` triats? All fallible trait
/// implementations that need to convert another thing into a Rust type or
/// opaque blob in a nested manner should also use fallible trait methods and
/// propagate failure up the stack. Only infallible functions and methods like
/// CodeGenerator implementations should use the infallible
/// `ToRustTyOrOpaque`. The further out we push error recovery, the more likely
/// we are to get a usable `Layout` even if we can't generate an equivalent Rust
/// type for a C++ construct.
trait ToRustTyOrOpaque: TryToRustTy + ToOpaque {
    type Extra;

    fn to_rust_ty_or_opaque(
        &self,
        ctx: &BindgenContext,
        extra: &<Self as ToRustTyOrOpaque>::Extra,
    ) -> quote::Tokens;
}

impl<E, T> ToRustTyOrOpaque for T
where
    T: TryToRustTy<Extra = E> + ToOpaque<Extra = E>,
{
    type Extra = E;

    fn to_rust_ty_or_opaque(
        &self,
        ctx: &BindgenContext,
        extra: &E,
    ) -> quote::Tokens {
        self.try_to_rust_ty(ctx, extra).unwrap_or_else(|_| {
            self.to_opaque(ctx, extra)
        })
    }
}

impl<T> TryToOpaque for T
where
    T: Copy + Into<ItemId>
{
    type Extra = ();

    fn try_get_layout(
        &self,
        ctx: &BindgenContext,
        _: &(),
    ) -> error::Result<Layout> {
        ctx.resolve_item((*self).into()).try_get_layout(ctx, &())
    }
}

impl<T> TryToRustTy for T
where
    T: Copy + Into<ItemId>
{
    type Extra = ();

    fn try_to_rust_ty(
        &self,
        ctx: &BindgenContext,
        _: &(),
    ) -> error::Result<quote::Tokens> {
        ctx.resolve_item((*self).into()).try_to_rust_ty(ctx, &())
    }
}

impl TryToOpaque for Item {
    type Extra = ();

    fn try_get_layout(
        &self,
        ctx: &BindgenContext,
        _: &(),
    ) -> error::Result<Layout> {
        self.kind().expect_type().try_get_layout(ctx, self)
    }
}

impl TryToRustTy for Item {
    type Extra = ();

    fn try_to_rust_ty(
        &self,
        ctx: &BindgenContext,
        _: &(),
    ) -> error::Result<quote::Tokens> {
        self.kind().expect_type().try_to_rust_ty(ctx, self)
    }
}

impl TryToOpaque for Type {
    type Extra = Item;

    fn try_get_layout(
        &self,
        ctx: &BindgenContext,
        _: &Item,
    ) -> error::Result<Layout> {
        self.layout(ctx).ok_or(error::Error::NoLayoutForOpaqueBlob)
    }
}

impl TryToRustTy for Type {
    type Extra = Item;

    fn try_to_rust_ty(
        &self,
        ctx: &BindgenContext,
        item: &Item,
    ) -> error::Result<quote::Tokens> {
        use self::helpers::ast_ty::*;

        match *self.kind() {
            TypeKind::Void => Ok(raw_type(ctx, "c_void")),
            // TODO: we should do something smart with nullptr, or maybe *const
            // c_void is enough?
            TypeKind::NullPtr => {
                Ok(raw_type(ctx, "c_void").to_ptr(true))
            }
            TypeKind::Int(ik) => {
                match ik {
                    IntKind::Bool => Ok(quote! { bool }),
                    IntKind::Char {
                        ..
                    } => Ok(raw_type(ctx, "c_char")),
                    IntKind::SChar => Ok(raw_type(ctx, "c_schar")),
                    IntKind::UChar => Ok(raw_type(ctx, "c_uchar")),
                    IntKind::Short => Ok(raw_type(ctx, "c_short")),
                    IntKind::UShort => Ok(raw_type(ctx, "c_ushort")),
                    IntKind::Int => Ok(raw_type(ctx, "c_int")),
                    IntKind::UInt => Ok(raw_type(ctx, "c_uint")),
                    IntKind::Long => Ok(raw_type(ctx, "c_long")),
                    IntKind::ULong => Ok(raw_type(ctx, "c_ulong")),
                    IntKind::LongLong => Ok(raw_type(ctx, "c_longlong")),
                    IntKind::ULongLong => Ok(raw_type(ctx, "c_ulonglong")),

                    IntKind::I8 => Ok(quote! { i8 }),
                    IntKind::U8 => Ok(quote! { u8 }),
                    IntKind::I16 => Ok(quote! { i16 }),
                    IntKind::U16 => Ok(quote! { u16 }),
                    IntKind::I32 => Ok(quote! { i32 }),
                    IntKind::U32 => Ok(quote! { u32 }),
                    IntKind::I64 => Ok(quote! { i64 }),
                    IntKind::U64 => Ok(quote! { u64 }),
                    IntKind::Custom {
                        name, ..
                    } => {
                        let ident = ctx.rust_ident_raw(name);
                        Ok(quote! {
                            #ident
                        })
                    }
                    // FIXME: This doesn't generate the proper alignment, but we
                    // can't do better right now. We should be able to use
                    // i128/u128 when they're available.
                    IntKind::U128 | IntKind::I128 => {
                        Ok(quote! { [u64; 2] })
                    }
                }
            }
            TypeKind::Float(fk) => Ok(float_kind_rust_type(ctx, fk)),
            TypeKind::Complex(fk) => {
                let float_path = float_kind_rust_type(ctx, fk);

                ctx.generated_bindegen_complex();
                Ok(if ctx.options().enable_cxx_namespaces {
                    quote! {
                        root::__BindgenComplex<#float_path>
                    }
                } else {
                    quote! {
                        __BindgenComplex<#float_path>
                    }
                })
            }
            TypeKind::Function(ref fs) => {
                // We can't rely on the sizeof(Option<NonZero<_>>) ==
                // sizeof(NonZero<_>) optimization with opaque blobs (because
                // they aren't NonZero), so don't *ever* use an or_opaque
                // variant here.
                let ty = fs.try_to_rust_ty(ctx, &())?;

                let prefix = ctx.trait_prefix();
                Ok(quote! {
                    ::#prefix::option::Option<#ty>
                })
            }
            TypeKind::Array(item, len) => {
                let ty = item.try_to_rust_ty(ctx, &())?;
                Ok(quote! {
                    [ #ty ; #len ]
                })
            }
            TypeKind::Enum(..) => {
                let mut tokens = quote! {};
                let path = item.namespace_aware_canonical_path(ctx);
                tokens.append_separated(path.into_iter().map(quote::Ident::new), "::");
                Ok(tokens)
            }
            TypeKind::TemplateInstantiation(ref inst) => {
                inst.try_to_rust_ty(ctx, item)
            }
            TypeKind::ResolvedTypeRef(inner) => inner.try_to_rust_ty(ctx, &()),
            TypeKind::TemplateAlias(..) |
            TypeKind::Alias(..) => {
                let template_params = item.used_template_params(ctx)
                    .unwrap_or(vec![])
                    .into_iter()
                    .filter(|param| param.is_template_param(ctx, &()))
                    .collect::<Vec<_>>();

                let spelling = self.name().expect("Unnamed alias?");
                if item.is_opaque(ctx, &()) && !template_params.is_empty() {
                    self.try_to_opaque(ctx, item)
                } else if let Some(ty) = utils::type_from_named(
                    ctx,
                    spelling,
                )
                {
                    Ok(ty)
                } else {
                    utils::build_path(item, ctx)
                }
            }
            TypeKind::Comp(ref info) => {
                let template_params = item.used_template_params(ctx);
                if info.has_non_type_template_params() ||
                    (item.is_opaque(ctx, &()) && template_params.is_some())
                {
                    return self.try_to_opaque(ctx, item);
                }

                utils::build_path(item, ctx)
            }
            TypeKind::Opaque => self.try_to_opaque(ctx, item),
            TypeKind::BlockPointer => {
                let void = raw_type(ctx, "c_void");
                Ok(void.to_ptr(
                    /* is_const = */
                    false
                ))
            }
            TypeKind::Pointer(inner) |
            TypeKind::Reference(inner) => {
                let is_const = self.is_const() || ctx.resolve_type(inner).is_const();

                let inner = inner.into_resolver().through_type_refs().resolve(ctx);
                let inner_ty = inner.expect_type();

                // Regardless if we can properly represent the inner type, we
                // should always generate a proper pointer here, so use
                // infallible conversion of the inner type.
                let mut ty = inner.to_rust_ty_or_opaque(ctx, &());
                ty.append_implicit_template_params(ctx, inner);

                // Avoid the first function pointer level, since it's already
                // represented in Rust.
                if inner_ty.canonical_type(ctx).is_function() {
                    Ok(ty)
                } else {
                    Ok(ty.to_ptr(is_const))
                }
            }
            TypeKind::TypeParam => {
                let name = item.canonical_name(ctx);
                let ident = ctx.rust_ident(&name);
                Ok(quote! {
                    #ident
                })
            }
            TypeKind::ObjCSel => {
                Ok(quote! {
                    objc::runtime::Sel
                })
            }
            TypeKind::ObjCId |
            TypeKind::ObjCInterface(..) => Ok(quote! {
                id
            }),
            ref u @ TypeKind::UnresolvedTypeRef(..) => {
                unreachable!("Should have been resolved after parsing {:?}!", u)
            }
        }
    }
}

impl TryToOpaque for TemplateInstantiation {
    type Extra = Item;

    fn try_get_layout(
        &self,
        ctx: &BindgenContext,
        item: &Item,
    ) -> error::Result<Layout> {
        item.expect_type().layout(ctx).ok_or(
            error::Error::NoLayoutForOpaqueBlob,
        )
    }
}

impl TryToRustTy for TemplateInstantiation {
    type Extra = Item;

    fn try_to_rust_ty(
        &self,
        ctx: &BindgenContext,
        item: &Item,
    ) -> error::Result<quote::Tokens> {
        if self.is_opaque(ctx, item) {
            return Err(error::Error::InstantiationOfOpaqueType);
        }

        let def = self.template_definition()
            .into_resolver()
            .through_type_refs()
            .resolve(ctx);

        let mut ty = quote! {};
        let def_path = def.namespace_aware_canonical_path(ctx);
        ty.append_separated(def_path.into_iter().map(|p| ctx.rust_ident(p)), "::");

        let def_params = match def.self_template_params(ctx) {
            Some(params) => params,
            None => {
                // This can happen if we generated an opaque type for a partial
                // template specialization, and we've hit an instantiation of
                // that partial specialization.
                extra_assert!(
                    def.is_opaque(ctx, &())
                );
                return Err(error::Error::InstantiationOfOpaqueType);
            }
        };

        // TODO: If the definition type is a template class/struct
        // definition's member template definition, it could rely on
        // generic template parameters from its outer template
        // class/struct. When we emit bindings for it, it could require
        // *more* type arguments than we have here, and we will need to
        // reconstruct them somehow. We don't have any means of doing
        // that reconstruction at this time.

        let template_args = self.template_arguments()
            .iter()
            .zip(def_params.iter())
        // Only pass type arguments for the type parameters that
        // the def uses.
            .filter(|&(_, param)| ctx.uses_template_parameter(def.id(), *param))
            .map(|(arg, _)| {
                let arg = arg.into_resolver().through_type_refs().resolve(ctx);
                let mut ty = arg.try_to_rust_ty(ctx, &())?;
                ty.append_implicit_template_params(ctx, arg);
                Ok(ty)
            })
            .collect::<error::Result<Vec<_>>>()?;

        if template_args.is_empty() {
            return Ok(ty);
        }

        Ok(quote! {
            #ty < #( #template_args ),* >
        })
    }
}

impl TryToRustTy for FunctionSig {
    type Extra = ();

    fn try_to_rust_ty(
        &self,
        ctx: &BindgenContext,
        _: &(),
    ) -> error::Result<quote::Tokens> {
        // TODO: we might want to consider ignoring the reference return value.
        let ret = utils::fnsig_return_ty(ctx, &self);
        let arguments = utils::fnsig_arguments(ctx, &self);
        let abi = self.abi();

        match abi {
            Abi::ThisCall if !ctx.options().rust_features().thiscall_abi() => {
                warn!("Skipping function with thiscall ABI that isn't supported by the configured Rust target");
                Ok(quote::Tokens::new())
            }
            _ => {
                Ok(quote! {
                    unsafe extern #abi fn ( #( #arguments ),* ) #ret
                })
            }
        }
    }
}

impl CodeGenerator for Function {
    type Extra = Item;

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        item: &Item,
    ) {
        debug!("<Function as CodeGenerator>::codegen: item = {:?}", item);
        debug_assert!(item.is_enabled_for_codegen(ctx));

        // We can't currently do anything with Internal functions so just
        // avoid generating anything for them.
        match self.linkage() {
            Linkage::Internal => return,
            Linkage::External => {}
        }

        // Pure virtual methods have no actual symbol, so we can't generate
        // something meaningful for them.
        match self.kind() {
            FunctionKind::Method(ref method_kind) if method_kind.is_pure_virtual() => {
                return;
            }
            _ => {},
        }

        // Similar to static member variables in a class template, we can't
        // generate bindings to template functions, because the set of
        // instantiations is open ended and we have no way of knowing which
        // monomorphizations actually exist.
        let type_params = item.all_template_params(ctx);
        if let Some(params) = type_params {
            if !params.is_empty() {
                return;
            }
        }

        let name = self.name();
        let mut canonical_name = item.canonical_name(ctx);
        let mangled_name = self.mangled_name();

        {
            let seen_symbol_name = mangled_name.unwrap_or(&canonical_name);

            // TODO: Maybe warn here if there's a type/argument mismatch, or
            // something?
            if result.seen_function(seen_symbol_name) {
                return;
            }
            result.saw_function(seen_symbol_name);
        }

        let signature_item = ctx.resolve_item(self.signature());
        let signature = signature_item.kind().expect_type().canonical_type(ctx);
        let signature = match *signature.kind() {
            TypeKind::Function(ref sig) => sig,
            _ => panic!("Signature kind is not a Function: {:?}", signature),
        };

        let args = utils::fnsig_arguments(ctx, signature);
        let ret = utils::fnsig_return_ty(ctx, signature);

        let mut attributes = vec![];

        if let Some(comment) = item.comment(ctx) {
            attributes.push(attributes::doc(comment));
        }

        if let Some(mangled) = mangled_name {
            attributes.push(attributes::link_name(mangled));
        } else if name != canonical_name {
            attributes.push(attributes::link_name(name));
        }

        // Handle overloaded functions by giving each overload its own unique
        // suffix.
        let times_seen = result.overload_number(&canonical_name);
        if times_seen > 0 {
            write!(&mut canonical_name, "{}", times_seen).unwrap();
        }

        let abi = match signature.abi() {
            Abi::ThisCall if !ctx.options().rust_features().thiscall_abi() => {
                warn!("Skipping function with thiscall ABI that isn't supported by the configured Rust target");
                return;
            }
            Abi::Unknown(unknown_abi) => {
                panic!(
                    "Invalid or unknown abi {:?} for function {:?} ({:?})",
                    unknown_abi,
                    canonical_name,
                    self
                );
            }
            abi => abi,
        };

        let ident = ctx.rust_ident(canonical_name);
        let mut tokens = quote! { extern #abi };
        tokens.append("{\n");
        if !attributes.is_empty() {
            tokens.append_separated(attributes, "\n");
            tokens.append("\n");
        }
        tokens.append(quote! {
            pub fn #ident ( #( #args ),* ) #ret;
        });
        tokens.append("\n}");
        result.push(tokens);
    }
}


fn objc_method_codegen(
    ctx: &BindgenContext,
    method: &ObjCMethod,
    class_name: Option<&str>,
    prefix: &str,
) -> (quote::Tokens, quote::Tokens) {
    let signature = method.signature();
    let fn_args = utils::fnsig_arguments(ctx, signature);
    let fn_ret = utils::fnsig_return_ty(ctx, signature);

    let sig = if method.is_class_method() {
        let fn_args = fn_args.clone();
        quote! {
            ( #( #fn_args ),* ) #fn_ret
        }
    } else {
        let fn_args = fn_args.clone();
        let args = iter::once(quote! { self })
            .chain(fn_args.into_iter());
        quote! {
            ( #( #args ),* ) #fn_ret
        }
    };

    let methods_and_args = method.format_method_call(&fn_args);

    let body = if method.is_class_method() {
        let class_name = class_name
            .expect("Generating a class method without class name?")
            .to_owned();
        let expect_msg = format!("Couldn't find {}", class_name);
        quote! {
            msg_send!(objc::runtime::Class::get(#class_name).expect(#expect_msg), #methods_and_args)
        }
    } else {
        quote! {
            msg_send!(self, #methods_and_args)
        }
    };

    let method_name = ctx.rust_ident(format!("{}{}", prefix, method.rust_name()));

    (
        quote! {
            unsafe fn #method_name #sig {
                #body
            }
        },
        quote! {
            unsafe fn #method_name #sig ;
        }
    )
}

impl CodeGenerator for ObjCInterface {
    type Extra = Item;

    fn codegen<'a>(
        &self,
        ctx: &BindgenContext,
        result: &mut CodegenResult<'a>,
        item: &Item,
    ) {
        debug_assert!(item.is_enabled_for_codegen(ctx));

        let mut impl_items = vec![];
        let mut trait_items = vec![];

        for method in self.methods() {
            let (impl_item, trait_item) =
                objc_method_codegen(ctx, method, None, "");
            impl_items.push(impl_item);
            trait_items.push(trait_item)
        }

        let instance_method_names: Vec<_> = self.methods()
            .iter()
            .map({
                |m| m.rust_name()
            })
            .collect();

        for class_method in self.class_methods() {
            let ambiquity =
                instance_method_names.contains(&class_method.rust_name());
            let prefix = if ambiquity { "class_" } else { "" };
            let (impl_item, trait_item) = objc_method_codegen(
                ctx,
                class_method,
                Some(self.name()),
                prefix,
            );
            impl_items.push(impl_item);
            trait_items.push(trait_item)
        }

        let trait_name = ctx.rust_ident(self.rust_name());

        let trait_block = quote! {
            pub trait #trait_name {
                #( #trait_items )*
            }
        };

        let ty_for_impl = quote! {
            id
        };
        let impl_block = quote! {
            impl #trait_name for #ty_for_impl {
                #( #impl_items )*
            }
        };

        result.push(trait_block);
        result.push(impl_block);
        result.saw_objc();
    }
}

pub(crate) fn codegen(context: BindgenContext) -> (Vec<quote::Tokens>, BindgenOptions) {
    context.gen(|context| {
        let _t = context.timer("codegen");
        let counter = Cell::new(0);
        let mut result = CodegenResult::new(&counter);

        debug!("codegen: {:?}", context.options());

        let codegen_items = context.codegen_items();
        if context.options().emit_ir {
            for &id in codegen_items {
                let item = context.resolve_item(id);
                println!("ir: {:?} = {:#?}", id, item);
            }
        }

        if let Some(path) = context.options().emit_ir_graphviz.as_ref() {
            match dot::write_dot_file(context, path) {
                Ok(()) => info!("Your dot file was generated successfully into: {}", path),
                Err(e) => error!("{}", e),
            }
        }

        context.resolve_item(context.root_module())
            .codegen(context, &mut result, &());

        result.items
    })
}

mod utils {
    use super::{ToRustTyOrOpaque, error};
    use ir::context::BindgenContext;
    use ir::function::FunctionSig;
    use ir::item::{Item, ItemCanonicalPath};
    use ir::ty::TypeKind;
    use quote;
    use std::mem;

    pub fn prepend_bitfield_unit_type(result: &mut Vec<quote::Tokens>) {
        let mut bitfield_unit_type = quote! {};
        bitfield_unit_type.append(include_str!("./bitfield_unit.rs"));

        let items = vec![bitfield_unit_type];
        let old_items = mem::replace(result, items);
        result.extend(old_items);
    }

    pub fn prepend_objc_header(
        ctx: &BindgenContext,
        result: &mut Vec<quote::Tokens>,
    ) {
        let use_objc = if ctx.options().objc_extern_crate {
            quote! {
                #[macro_use]
                extern crate objc;
            }
        } else {
            quote! {
                use objc;
            }
        };

        let id_type = quote! {
            #[allow(non_camel_case_types)]
            pub type id = *mut objc::runtime::Object;
        };

        let items = vec![use_objc, id_type];
        let old_items = mem::replace(result, items);
        result.extend(old_items.into_iter());
    }

    pub fn prepend_union_types(
        ctx: &BindgenContext,
        result: &mut Vec<quote::Tokens>,
    ) {
        let prefix = ctx.trait_prefix();

        // TODO(emilio): The fmt::Debug impl could be way nicer with
        // std::intrinsics::type_name, but...
        let union_field_decl = quote! {
            #[repr(C)]
            pub struct __BindgenUnionField<T>(::#prefix::marker::PhantomData<T>);
        };

        let union_field_impl = quote! {
            impl<T> __BindgenUnionField<T> {
                #[inline]
                pub fn new() -> Self {
                    __BindgenUnionField(::#prefix::marker::PhantomData)
                }

                #[inline]
                pub unsafe fn as_ref(&self) -> &T {
                    ::#prefix::mem::transmute(self)
                }

                #[inline]
                pub unsafe fn as_mut(&mut self) -> &mut T {
                    ::#prefix::mem::transmute(self)
                }
            }
        };

        let union_field_default_impl = quote! {
            impl<T> ::#prefix::default::Default for __BindgenUnionField<T> {
                #[inline]
                fn default() -> Self {
                    Self::new()
                }
            }
        };

        let union_field_clone_impl = quote! {
            impl<T> ::#prefix::clone::Clone for __BindgenUnionField<T> {
                #[inline]
                fn clone(&self) -> Self {
                    Self::new()
                }
            }
        };

        let union_field_copy_impl = quote! {
            impl<T> ::#prefix::marker::Copy for __BindgenUnionField<T> {}
        };

        let union_field_debug_impl = quote! {
            impl<T> ::#prefix::fmt::Debug for __BindgenUnionField<T> {
                fn fmt(&self, fmt: &mut ::#prefix::fmt::Formatter)
                       -> ::#prefix::fmt::Result {
                    fmt.write_str("__BindgenUnionField")
                }
            }
        };

        // The actual memory of the filed will be hashed, so that's why these
        // field doesn't do anything with the hash.
        let union_field_hash_impl = quote! {
            impl<T> ::#prefix::hash::Hash for __BindgenUnionField<T> {
                fn hash<H: ::#prefix::hash::Hasher>(&self, _state: &mut H) {
                }
            }
        };

        let union_field_partialeq_impl = quote! {
            impl<T> ::#prefix::cmp::PartialEq for __BindgenUnionField<T> {
               fn eq(&self, _other: &__BindgenUnionField<T>) -> bool {
                   true
               }
           }
        };

        let union_field_eq_impl = quote! {
           impl<T> ::#prefix::cmp::Eq for __BindgenUnionField<T> {
           }
        };

        let items = vec![union_field_decl,
                         union_field_impl,
                         union_field_default_impl,
                         union_field_clone_impl,
                         union_field_copy_impl,
                         union_field_debug_impl,
                         union_field_hash_impl,
                         union_field_partialeq_impl,
                         union_field_eq_impl];

        let old_items = mem::replace(result, items);
        result.extend(old_items.into_iter());
    }

    pub fn prepend_incomplete_array_types(
        ctx: &BindgenContext,
        result: &mut Vec<quote::Tokens>,
    ) {
        let prefix = ctx.trait_prefix();

        let incomplete_array_decl = quote! {
            #[repr(C)]
            #[derive(Default)]
            pub struct __IncompleteArrayField<T>(
                ::#prefix::marker::PhantomData<T>);
        };

        let incomplete_array_impl = quote! {
            impl<T> __IncompleteArrayField<T> {
                #[inline]
                pub fn new() -> Self {
                    __IncompleteArrayField(::#prefix::marker::PhantomData)
                }

                #[inline]
                pub unsafe fn as_ptr(&self) -> *const T {
                    ::#prefix::mem::transmute(self)
                }

                #[inline]
                pub unsafe fn as_mut_ptr(&mut self) -> *mut T {
                    ::#prefix::mem::transmute(self)
                }

                #[inline]
                pub unsafe fn as_slice(&self, len: usize) -> &[T] {
                    ::#prefix::slice::from_raw_parts(self.as_ptr(), len)
                }

                #[inline]
                pub unsafe fn as_mut_slice(&mut self, len: usize) -> &mut [T] {
                    ::#prefix::slice::from_raw_parts_mut(self.as_mut_ptr(), len)
                }
            }
        };

        let incomplete_array_debug_impl = quote! {
            impl<T> ::#prefix::fmt::Debug for __IncompleteArrayField<T> {
                fn fmt(&self, fmt: &mut ::#prefix::fmt::Formatter)
                       -> ::#prefix::fmt::Result {
                    fmt.write_str("__IncompleteArrayField")
                }
            }
        };

        let incomplete_array_clone_impl = quote! {
            impl<T> ::#prefix::clone::Clone for __IncompleteArrayField<T> {
                #[inline]
                fn clone(&self) -> Self {
                    Self::new()
                }
            }
        };

        let incomplete_array_copy_impl = quote! {
            impl<T> ::#prefix::marker::Copy for __IncompleteArrayField<T> {}
        };

        let items = vec![incomplete_array_decl,
                         incomplete_array_impl,
                         incomplete_array_debug_impl,
                         incomplete_array_clone_impl,
                         incomplete_array_copy_impl];

        let old_items = mem::replace(result, items);
        result.extend(old_items.into_iter());
    }

    pub fn prepend_complex_type(
        result: &mut Vec<quote::Tokens>,
    ) {
        let complex_type = quote! {
            #[derive(PartialEq, Copy, Clone, Hash, Debug, Default)]
            #[repr(C)]
            pub struct __BindgenComplex<T> {
                pub re: T,
                pub im: T
            }
        };

        let items = vec![complex_type];
        let old_items = mem::replace(result, items);
        result.extend(old_items.into_iter());
    }

    pub fn build_path(
        item: &Item,
        ctx: &BindgenContext,
    ) -> error::Result<quote::Tokens> {
        let path = item.namespace_aware_canonical_path(ctx);

        let mut tokens = quote! {};
        tokens.append_separated(path.into_iter().map(quote::Ident::new), "::");

        Ok(tokens)
    }

    fn primitive_ty(ctx: &BindgenContext, name: &str) -> quote::Tokens {
        let ident = ctx.rust_ident_raw(name);
        quote! {
            #ident
        }
    }

    pub fn type_from_named(
        ctx: &BindgenContext,
        name: &str,
    ) -> Option<quote::Tokens> {
        // FIXME: We could use the inner item to check this is really a
        // primitive type but, who the heck overrides these anyway?
        Some(match name {
            "int8_t" => primitive_ty(ctx, "i8"),
            "uint8_t" => primitive_ty(ctx, "u8"),
            "int16_t" => primitive_ty(ctx, "i16"),
            "uint16_t" => primitive_ty(ctx, "u16"),
            "int32_t" => primitive_ty(ctx, "i32"),
            "uint32_t" => primitive_ty(ctx, "u32"),
            "int64_t" => primitive_ty(ctx, "i64"),
            "uint64_t" => primitive_ty(ctx, "u64"),

            "uintptr_t" | "size_t" => primitive_ty(ctx, "usize"),

            "intptr_t" | "ptrdiff_t" | "ssize_t" => primitive_ty(ctx, "isize"),
            _ => return None,
        })
    }

    pub fn fnsig_return_ty(
        ctx: &BindgenContext,
        sig: &FunctionSig,
    ) -> quote::Tokens {
        let return_item = ctx.resolve_item(sig.return_type());
        if let TypeKind::Void = *return_item.kind().expect_type().kind() {
            quote! { }
        } else {
            let ret_ty = return_item.to_rust_ty_or_opaque(ctx, &());
            quote! {
                -> #ret_ty
            }
        }
    }

    pub fn fnsig_arguments(
        ctx: &BindgenContext,
        sig: &FunctionSig,
    ) -> Vec<quote::Tokens> {
        use super::ToPtr;

        let mut unnamed_arguments = 0;
        let mut args = sig.argument_types().iter().map(|&(ref name, ty)| {
            let arg_item = ctx.resolve_item(ty);
            let arg_ty = arg_item.kind().expect_type();

            // From the C90 standard[1]:
            //
            //     A declaration of a parameter as "array of type" shall be
            //     adjusted to "qualified pointer to type", where the type
            //     qualifiers (if any) are those specified within the [ and ] of
            //     the array type derivation.
            //
            // [1]: http://c0x.coding-guidelines.com/6.7.5.3.html
            let arg_ty = match *arg_ty.canonical_type(ctx).kind() {
                TypeKind::Array(t, _) => {
                    t.to_rust_ty_or_opaque(ctx, &())
                        .to_ptr(ctx.resolve_type(t).is_const())
                },
                TypeKind::Pointer(inner) => {
                    let inner = ctx.resolve_item(inner);
                    let inner_ty = inner.expect_type();
                    if let TypeKind::ObjCInterface(_) = *inner_ty.canonical_type(ctx).kind() {
                        quote! {
                            id
                        }
                    } else {
                        arg_item.to_rust_ty_or_opaque(ctx, &())
                    }
                },
                _ => {
                    arg_item.to_rust_ty_or_opaque(ctx, &())
                }
            };

            let arg_name = match *name {
                Some(ref name) => ctx.rust_mangle(name).into_owned(),
                None => {
                    unnamed_arguments += 1;
                    format!("arg{}", unnamed_arguments)
                }
            };

            assert!(!arg_name.is_empty());
            let arg_name = ctx.rust_ident(arg_name);

            quote! {
                #arg_name : #arg_ty
            }
        }).collect::<Vec<_>>();

        if sig.is_variadic() {
            args.push(quote! { ... })
        }

        args
    }
}
