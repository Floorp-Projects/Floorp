// Adapted from libsyntax.

//! AST walker. Each overridden visit method has full control over what
//! happens with its node, it can do its own traversal of the node's children,
//! call `visit::walk_*` to apply the default traversal algorithm, or prevent
//! deeper traversal by doing nothing.
//!
//! Note: it is an important invariant that the default visitor walks the body
//! of a function in "execution order" (more concretely, reverse post-order
//! with respect to the CFG implied by the AST), meaning that if AST node A may
//! execute before AST node B, then A is visited first.  The borrow checker in
//! particular relies on this property.
//!
//! Note: walking an AST before macro expansion is probably a bad idea. For
//! instance, a walker looking for item names in a module will miss all of
//! those that are created by the expansion of a macro.

use super::*;

/// Each method of the Visitor trait is a hook to be potentially
/// overridden.  Each method's default implementation recursively visits
/// the substructure of the input via the corresponding `walk` method;
/// e.g. the `visit_mod` method by default calls `visit::walk_mod`.
///
/// If you want to ensure that your code handles every variant
/// explicitly, you need to override each method.  (And you also need
/// to monitor future changes to `Visitor` in case a new method with a
/// new default implementation gets introduced.)
pub trait Visitor: Sized {
    fn visit_ident(&mut self, _ident: &Ident) {}
    fn visit_derive_input(&mut self, derive_input: &DeriveInput) {
        walk_derive_input(self, derive_input)
    }
    fn visit_ty(&mut self, ty: &Ty) {
        walk_ty(self, ty)
    }
    fn visit_generics(&mut self, generics: &Generics) {
        walk_generics(self, generics)
    }
    fn visit_ty_param_bound(&mut self, bound: &TyParamBound) {
        walk_ty_param_bound(self, bound)
    }
    fn visit_poly_trait_ref(&mut self, trait_ref: &PolyTraitRef, modifier: &TraitBoundModifier) {
        walk_poly_trait_ref(self, trait_ref, modifier)
    }
    fn visit_variant_data(&mut self, data: &VariantData, _ident: &Ident, _generics: &Generics) {
        walk_variant_data(self, data)
    }
    fn visit_field(&mut self, field: &Field) {
        walk_field(self, field)
    }
    fn visit_variant(&mut self, variant: &Variant, generics: &Generics) {
        walk_variant(self, variant, generics)
    }
    fn visit_lifetime(&mut self, _lifetime: &Lifetime) {}
    fn visit_lifetime_def(&mut self, lifetime: &LifetimeDef) {
        walk_lifetime_def(self, lifetime)
    }
    fn visit_path(&mut self, path: &Path) {
        walk_path(self, path)
    }
    fn visit_path_segment(&mut self, path_segment: &PathSegment) {
        walk_path_segment(self, path_segment)
    }
    fn visit_path_parameters(&mut self, path_parameters: &PathParameters) {
        walk_path_parameters(self, path_parameters)
    }
    fn visit_assoc_type_binding(&mut self, type_binding: &TypeBinding) {
        walk_assoc_type_binding(self, type_binding)
    }
    fn visit_attribute(&mut self, _attr: &Attribute) {}
    fn visit_fn_ret_ty(&mut self, ret_ty: &FunctionRetTy) {
        walk_fn_ret_ty(self, ret_ty)
    }
    fn visit_const_expr(&mut self, expr: &ConstExpr) {
        walk_const_expr(self, expr)
    }
    fn visit_lit(&mut self, _lit: &Lit) {}

    fn visit_mac(&mut self, mac: &Mac) {
        walk_mac(self, mac);
    }

    #[cfg(feature = "full")]
    fn visit_crate(&mut self, _crate: &Crate) {
        walk_crate(self, _crate);
    }
    #[cfg(feature = "full")]
    fn visit_item(&mut self, item: &Item) {
        walk_item(self, item);
    }
    #[cfg(feature = "full")]
    fn visit_expr(&mut self, expr: &Expr) {
        walk_expr(self, expr);
    }
    #[cfg(feature = "full")]
    fn visit_foreign_item(&mut self, foreign_item: &ForeignItem) {
        walk_foreign_item(self, foreign_item);
    }
    #[cfg(feature = "full")]
    fn visit_pat(&mut self, pat: &Pat) {
        walk_pat(self, pat);
    }
    #[cfg(feature = "full")]
    fn visit_fn_decl(&mut self, fn_decl: &FnDecl) {
        walk_fn_decl(self, fn_decl);
    }
    #[cfg(feature = "full")]
    fn visit_trait_item(&mut self, trait_item: &TraitItem) {
        walk_trait_item(self, trait_item);
    }
    #[cfg(feature = "full")]
    fn visit_impl_item(&mut self, impl_item: &ImplItem) {
        walk_impl_item(self, impl_item);
    }
    #[cfg(feature = "full")]
    fn visit_method_sig(&mut self, method_sig: &MethodSig) {
        walk_method_sig(self, method_sig);
    }
    #[cfg(feature = "full")]
    fn visit_stmt(&mut self, stmt: &Stmt) {
        walk_stmt(self, stmt);
    }
    #[cfg(feature = "full")]
    fn visit_local(&mut self, local: &Local) {
        walk_local(self, local);
    }
    #[cfg(feature = "full")]
    fn visit_view_path(&mut self, view_path: &ViewPath) {
        walk_view_path(self, view_path);
    }
}

macro_rules! walk_list {
    ($visitor:expr, $method:ident, $list:expr $(, $extra_args:expr)*) => {
        for elem in $list {
            $visitor.$method(elem $(, $extra_args)*)
        }
    };
}

pub fn walk_opt_ident<V: Visitor>(visitor: &mut V, opt_ident: &Option<Ident>) {
    if let Some(ref ident) = *opt_ident {
        visitor.visit_ident(ident);
    }
}

pub fn walk_lifetime_def<V: Visitor>(visitor: &mut V, lifetime_def: &LifetimeDef) {
    visitor.visit_lifetime(&lifetime_def.lifetime);
    walk_list!(visitor, visit_lifetime, &lifetime_def.bounds);
}

pub fn walk_poly_trait_ref<V>(visitor: &mut V, trait_ref: &PolyTraitRef, _: &TraitBoundModifier)
    where V: Visitor
{
    walk_list!(visitor, visit_lifetime_def, &trait_ref.bound_lifetimes);
    visitor.visit_path(&trait_ref.trait_ref);
}

pub fn walk_derive_input<V: Visitor>(visitor: &mut V, derive_input: &DeriveInput) {
    visitor.visit_ident(&derive_input.ident);
    visitor.visit_generics(&derive_input.generics);
    match derive_input.body {
        Body::Enum(ref variants) => {
            walk_list!(visitor, visit_variant, variants, &derive_input.generics);
        }
        Body::Struct(ref variant_data) => {
            visitor.visit_variant_data(variant_data, &derive_input.ident, &derive_input.generics);
        }
    }
    walk_list!(visitor, visit_attribute, &derive_input.attrs);
}

pub fn walk_variant<V>(visitor: &mut V, variant: &Variant, generics: &Generics)
    where V: Visitor
{
    visitor.visit_ident(&variant.ident);
    visitor.visit_variant_data(&variant.data, &variant.ident, generics);
    walk_list!(visitor, visit_attribute, &variant.attrs);
}

pub fn walk_ty<V: Visitor>(visitor: &mut V, ty: &Ty) {
    match *ty {
        Ty::Slice(ref inner) |
        Ty::Paren(ref inner) => visitor.visit_ty(inner),
        Ty::Ptr(ref mutable_type) => visitor.visit_ty(&mutable_type.ty),
        Ty::Rptr(ref opt_lifetime, ref mutable_type) => {
            walk_list!(visitor, visit_lifetime, opt_lifetime);
            visitor.visit_ty(&mutable_type.ty)
        }
        Ty::Never | Ty::Infer => {}
        Ty::Tup(ref tuple_element_types) => {
            walk_list!(visitor, visit_ty, tuple_element_types);
        }
        Ty::BareFn(ref bare_fn) => {
            walk_list!(visitor, visit_lifetime_def, &bare_fn.lifetimes);
            for argument in &bare_fn.inputs {
                walk_opt_ident(visitor, &argument.name);
                visitor.visit_ty(&argument.ty)
            }
            visitor.visit_fn_ret_ty(&bare_fn.output)
        }
        Ty::Path(ref maybe_qself, ref path) => {
            if let Some(ref qself) = *maybe_qself {
                visitor.visit_ty(&qself.ty);
            }
            visitor.visit_path(path);
        }
        Ty::Array(ref inner, ref len) => {
            visitor.visit_ty(inner);
            visitor.visit_const_expr(len);
        }
        Ty::TraitObject(ref bounds) |
        Ty::ImplTrait(ref bounds) => {
            walk_list!(visitor, visit_ty_param_bound, bounds);
        }
        Ty::Mac(ref mac) => {
            visitor.visit_mac(mac);
        }
    }
}

pub fn walk_path<V: Visitor>(visitor: &mut V, path: &Path) {
    for segment in &path.segments {
        visitor.visit_path_segment(segment);
    }
}

pub fn walk_path_segment<V: Visitor>(visitor: &mut V, segment: &PathSegment) {
    visitor.visit_ident(&segment.ident);
    visitor.visit_path_parameters(&segment.parameters);
}

pub fn walk_path_parameters<V>(visitor: &mut V, path_parameters: &PathParameters)
    where V: Visitor
{
    match *path_parameters {
        PathParameters::AngleBracketed(ref data) => {
            walk_list!(visitor, visit_ty, &data.types);
            walk_list!(visitor, visit_lifetime, &data.lifetimes);
            walk_list!(visitor, visit_assoc_type_binding, &data.bindings);
        }
        PathParameters::Parenthesized(ref data) => {
            walk_list!(visitor, visit_ty, &data.inputs);
            walk_list!(visitor, visit_ty, &data.output);
        }
    }
}

pub fn walk_assoc_type_binding<V: Visitor>(visitor: &mut V, type_binding: &TypeBinding) {
    visitor.visit_ident(&type_binding.ident);
    visitor.visit_ty(&type_binding.ty);
}

pub fn walk_ty_param_bound<V: Visitor>(visitor: &mut V, bound: &TyParamBound) {
    match *bound {
        TyParamBound::Trait(ref ty, ref modifier) => {
            visitor.visit_poly_trait_ref(ty, modifier);
        }
        TyParamBound::Region(ref lifetime) => {
            visitor.visit_lifetime(lifetime);
        }
    }
}

pub fn walk_generics<V: Visitor>(visitor: &mut V, generics: &Generics) {
    for param in &generics.ty_params {
        visitor.visit_ident(&param.ident);
        walk_list!(visitor, visit_ty_param_bound, &param.bounds);
        walk_list!(visitor, visit_ty, &param.default);
    }
    walk_list!(visitor, visit_lifetime_def, &generics.lifetimes);
    for predicate in &generics.where_clause.predicates {
        match *predicate {
            WherePredicate::BoundPredicate(WhereBoundPredicate { ref bounded_ty,
                                                                 ref bounds,
                                                                 ref bound_lifetimes,
                                                                 .. }) => {
                visitor.visit_ty(bounded_ty);
                walk_list!(visitor, visit_ty_param_bound, bounds);
                walk_list!(visitor, visit_lifetime_def, bound_lifetimes);
            }
            WherePredicate::RegionPredicate(WhereRegionPredicate { ref lifetime,
                                                                   ref bounds,
                                                                   .. }) => {
                visitor.visit_lifetime(lifetime);
                walk_list!(visitor, visit_lifetime, bounds);
            }
            WherePredicate::EqPredicate(WhereEqPredicate { ref lhs_ty, ref rhs_ty, .. }) => {
                visitor.visit_ty(lhs_ty);
                visitor.visit_ty(rhs_ty);
            }
        }
    }
}

pub fn walk_fn_ret_ty<V: Visitor>(visitor: &mut V, ret_ty: &FunctionRetTy) {
    if let FunctionRetTy::Ty(ref output_ty) = *ret_ty {
        visitor.visit_ty(output_ty)
    }
}

pub fn walk_variant_data<V: Visitor>(visitor: &mut V, data: &VariantData) {
    walk_list!(visitor, visit_field, data.fields());
}

pub fn walk_field<V: Visitor>(visitor: &mut V, field: &Field) {
    walk_opt_ident(visitor, &field.ident);
    visitor.visit_ty(&field.ty);
    walk_list!(visitor, visit_attribute, &field.attrs);
}

pub fn walk_const_expr<V: Visitor>(visitor: &mut V, len: &ConstExpr) {
    match *len {
        ConstExpr::Call(ref function, ref args) => {
            visitor.visit_const_expr(function);
            walk_list!(visitor, visit_const_expr, args);
        }
        ConstExpr::Binary(_op, ref left, ref right) => {
            visitor.visit_const_expr(left);
            visitor.visit_const_expr(right);
        }
        ConstExpr::Unary(_op, ref v) => {
            visitor.visit_const_expr(v);
        }
        ConstExpr::Lit(ref lit) => {
            visitor.visit_lit(lit);
        }
        ConstExpr::Cast(ref expr, ref ty) => {
            visitor.visit_const_expr(expr);
            visitor.visit_ty(ty);
        }
        ConstExpr::Path(ref path) => {
            visitor.visit_path(path);
        }
        ConstExpr::Index(ref expr, ref index) => {
            visitor.visit_const_expr(expr);
            visitor.visit_const_expr(index);
        }
        ConstExpr::Paren(ref expr) => {
            visitor.visit_const_expr(expr);
        }
        ConstExpr::Other(ref other) => {
            #[cfg(feature = "full")]
            fn walk_other<V: Visitor>(visitor: &mut V, other: &Expr) {
                visitor.visit_expr(other);
            }
            #[cfg(not(feature = "full"))]
            fn walk_other<V: Visitor>(_: &mut V, _: &super::constant::Other) {}
            walk_other(visitor, other);
        }
    }
}

pub fn walk_mac<V: Visitor>(visitor: &mut V, mac: &Mac) {
    visitor.visit_path(&mac.path);
}

#[cfg(feature = "full")]
pub fn walk_crate<V: Visitor>(visitor: &mut V, _crate: &Crate) {
    walk_list!(visitor, visit_attribute, &_crate.attrs);
    walk_list!(visitor, visit_item, &_crate.items);
}

#[cfg(feature = "full")]
pub fn walk_item<V: Visitor>(visitor: &mut V, item: &Item) {
    visitor.visit_ident(&item.ident);
    walk_list!(visitor, visit_attribute, &item.attrs);
    match item.node {
        ItemKind::ExternCrate(ref ident) => {
            walk_opt_ident(visitor, ident);
        }
        ItemKind::Use(ref view_path) => {
            visitor.visit_view_path(view_path);
        }
        ItemKind::Static(ref ty, _, ref expr) |
        ItemKind::Const(ref ty, ref expr) => {
            visitor.visit_ty(ty);
            visitor.visit_expr(expr);
        }
        ItemKind::Fn(ref decl, _, _, _, ref generics, ref body) => {
            visitor.visit_fn_decl(decl);
            visitor.visit_generics(generics);
            walk_list!(visitor, visit_stmt, &body.stmts);
        }
        ItemKind::Mod(ref maybe_items) => {
            if let Some(ref items) = *maybe_items {
                walk_list!(visitor, visit_item, items);
            }
        }
        ItemKind::ForeignMod(ref foreign_mod) => {
            walk_list!(visitor, visit_foreign_item, &foreign_mod.items);
        }
        ItemKind::Ty(ref ty, ref generics) => {
            visitor.visit_ty(ty);
            visitor.visit_generics(generics);
        }
        ItemKind::Enum(ref variant, ref generics) => {
            walk_list!(visitor, visit_variant, variant, generics);
        }
        ItemKind::Struct(ref variant_data, ref generics) |
        ItemKind::Union(ref variant_data, ref generics) => {
            visitor.visit_variant_data(variant_data, &item.ident, generics);
        }
        ItemKind::Trait(_, ref generics, ref bounds, ref trait_items) => {
            visitor.visit_generics(generics);
            walk_list!(visitor, visit_ty_param_bound, bounds);
            walk_list!(visitor, visit_trait_item, trait_items);
        }
        ItemKind::DefaultImpl(_, ref path) => {
            visitor.visit_path(path);
        }
        ItemKind::Impl(_, _, ref generics, ref maybe_path, ref ty, ref impl_items) => {
            visitor.visit_generics(generics);
            if let Some(ref path) = *maybe_path {
                visitor.visit_path(path);
            }
            visitor.visit_ty(ty);
            walk_list!(visitor, visit_impl_item, impl_items);
        }
        ItemKind::Mac(ref mac) => visitor.visit_mac(mac),
    }
}

#[cfg(feature = "full")]
#[cfg_attr(feature = "cargo-clippy", allow(cyclomatic_complexity))]
pub fn walk_expr<V: Visitor>(visitor: &mut V, expr: &Expr) {
    walk_list!(visitor, visit_attribute, &expr.attrs);
    match expr.node {
        ExprKind::InPlace(ref place, ref value) => {
            visitor.visit_expr(place);
            visitor.visit_expr(value);
        }
        ExprKind::Call(ref callee, ref args) => {
            visitor.visit_expr(callee);
            walk_list!(visitor, visit_expr, args);
        }
        ExprKind::MethodCall(ref name, ref ty_args, ref args) => {
            visitor.visit_ident(name);
            walk_list!(visitor, visit_ty, ty_args);
            walk_list!(visitor, visit_expr, args);
        }
        ExprKind::Array(ref exprs) |
        ExprKind::Tup(ref exprs) => {
            walk_list!(visitor, visit_expr, exprs);
        }
        ExprKind::Unary(_, ref operand) => {
            visitor.visit_expr(operand);
        }
        ExprKind::Lit(ref lit) => {
            visitor.visit_lit(lit);
        }
        ExprKind::Cast(ref expr, ref ty) |
        ExprKind::Type(ref expr, ref ty) => {
            visitor.visit_expr(expr);
            visitor.visit_ty(ty);
        }
        ExprKind::If(ref cond, ref cons, ref maybe_alt) => {
            visitor.visit_expr(cond);
            walk_list!(visitor, visit_stmt, &cons.stmts);
            if let Some(ref alt) = *maybe_alt {
                visitor.visit_expr(alt);
            }
        }
        ExprKind::IfLet(ref pat, ref cond, ref cons, ref maybe_alt) => {
            visitor.visit_pat(pat);
            visitor.visit_expr(cond);
            walk_list!(visitor, visit_stmt, &cons.stmts);
            if let Some(ref alt) = *maybe_alt {
                visitor.visit_expr(alt);
            }
        }
        ExprKind::While(ref cond, ref body, ref label) => {
            visitor.visit_expr(cond);
            walk_list!(visitor, visit_stmt, &body.stmts);
            walk_opt_ident(visitor, label);
        }
        ExprKind::WhileLet(ref pat, ref cond, ref body, ref label) => {
            visitor.visit_pat(pat);
            visitor.visit_expr(cond);
            walk_list!(visitor, visit_stmt, &body.stmts);
            walk_opt_ident(visitor, label);
        }
        ExprKind::ForLoop(ref pat, ref expr, ref body, ref label) => {
            visitor.visit_pat(pat);
            visitor.visit_expr(expr);
            walk_list!(visitor, visit_stmt, &body.stmts);
            walk_opt_ident(visitor, label);
        }
        ExprKind::Loop(ref body, ref label) => {
            walk_list!(visitor, visit_stmt, &body.stmts);
            walk_opt_ident(visitor, label);
        }
        ExprKind::Match(ref expr, ref arms) => {
            visitor.visit_expr(expr);
            for &Arm { ref attrs, ref pats, ref guard, ref body } in arms {
                walk_list!(visitor, visit_attribute, attrs);
                walk_list!(visitor, visit_pat, pats);
                if let Some(ref guard) = *guard {
                    visitor.visit_expr(guard);
                }
                visitor.visit_expr(body);
            }
        }
        ExprKind::Closure(_, ref decl, ref expr) => {
            visitor.visit_fn_decl(decl);
            visitor.visit_expr(expr);
        }
        ExprKind::Block(_, ref block) => {
            walk_list!(visitor, visit_stmt, &block.stmts);
        }
        ExprKind::Binary(_, ref lhs, ref rhs) |
        ExprKind::Assign(ref lhs, ref rhs) |
        ExprKind::AssignOp(_, ref lhs, ref rhs) => {
            visitor.visit_expr(lhs);
            visitor.visit_expr(rhs);
        }
        ExprKind::Field(ref obj, ref field) => {
            visitor.visit_expr(obj);
            visitor.visit_ident(field);
        }
        ExprKind::TupField(ref obj, _) => {
            visitor.visit_expr(obj);
        }
        ExprKind::Index(ref obj, ref idx) => {
            visitor.visit_expr(obj);
            visitor.visit_expr(idx);
        }
        ExprKind::Range(ref maybe_start, ref maybe_end, _) => {
            if let Some(ref start) = *maybe_start {
                visitor.visit_expr(start);
            }
            if let Some(ref end) = *maybe_end {
                visitor.visit_expr(end);
            }
        }
        ExprKind::Path(ref maybe_qself, ref path) => {
            if let Some(ref qself) = *maybe_qself {
                visitor.visit_ty(&qself.ty);
            }
            visitor.visit_path(path);
        }
        ExprKind::Break(ref maybe_label, ref maybe_expr) => {
            walk_opt_ident(visitor, maybe_label);
            if let Some(ref expr) = *maybe_expr {
                visitor.visit_expr(expr);
            }
        }
        ExprKind::Continue(ref maybe_label) => {
            walk_opt_ident(visitor, maybe_label);
        }
        ExprKind::Ret(ref maybe_expr) => {
            if let Some(ref expr) = *maybe_expr {
                visitor.visit_expr(expr);
            }
        }
        ExprKind::Mac(ref mac) => {
            visitor.visit_mac(mac);
        }
        ExprKind::Struct(ref path, ref fields, ref maybe_base) => {
            visitor.visit_path(path);
            for &FieldValue { ref ident, ref expr, .. } in fields {
                visitor.visit_ident(ident);
                visitor.visit_expr(expr);
            }
            if let Some(ref base) = *maybe_base {
                visitor.visit_expr(base);
            }
        }
        ExprKind::Repeat(ref value, ref times) => {
            visitor.visit_expr(value);
            visitor.visit_expr(times);
        }
        ExprKind::Box(ref expr) |
        ExprKind::AddrOf(_, ref expr) |
        ExprKind::Paren(ref expr) |
        ExprKind::Try(ref expr) => {
            visitor.visit_expr(expr);
        }
    }
}

#[cfg(feature = "full")]
pub fn walk_foreign_item<V: Visitor>(visitor: &mut V, foreign_item: &ForeignItem) {
    visitor.visit_ident(&foreign_item.ident);
    walk_list!(visitor, visit_attribute, &foreign_item.attrs);
    match foreign_item.node {
        ForeignItemKind::Fn(ref decl, ref generics) => {
            visitor.visit_fn_decl(decl);
            visitor.visit_generics(generics);
        }
        ForeignItemKind::Static(ref ty, _) => {
            visitor.visit_ty(ty);
        }
    }
}

#[cfg(feature = "full")]
pub fn walk_pat<V: Visitor>(visitor: &mut V, pat: &Pat) {
    match *pat {
        Pat::Wild => {}
        Pat::Ident(_, ref ident, ref maybe_pat) => {
            visitor.visit_ident(ident);
            if let Some(ref pat) = *maybe_pat {
                visitor.visit_pat(pat);
            }
        }
        Pat::Struct(ref path, ref field_pats, _) => {
            visitor.visit_path(path);
            for &FieldPat { ref ident, ref pat, .. } in field_pats {
                visitor.visit_ident(ident);
                visitor.visit_pat(pat);
            }
        }
        Pat::TupleStruct(ref path, ref pats, _) => {
            visitor.visit_path(path);
            walk_list!(visitor, visit_pat, pats);
        }
        Pat::Path(ref maybe_qself, ref path) => {
            if let Some(ref qself) = *maybe_qself {
                visitor.visit_ty(&qself.ty);
            }
            visitor.visit_path(path);
        }
        Pat::Tuple(ref pats, _) => {
            walk_list!(visitor, visit_pat, pats);
        }
        Pat::Box(ref pat) |
        Pat::Ref(ref pat, _) => {
            visitor.visit_pat(pat);
        }
        Pat::Lit(ref expr) => {
            visitor.visit_expr(expr);
        }
        Pat::Range(ref start, ref end) => {
            visitor.visit_expr(start);
            visitor.visit_expr(end);
        }
        Pat::Slice(ref start, ref maybe_mid, ref end) => {
            walk_list!(visitor, visit_pat, start);
            if let Some(ref mid) = *maybe_mid {
                visitor.visit_pat(mid);
            }
            walk_list!(visitor, visit_pat, end);
        }
        Pat::Mac(ref mac) => {
            visitor.visit_mac(mac);
        }
    }
}

#[cfg(feature = "full")]
pub fn walk_fn_decl<V: Visitor>(visitor: &mut V, fn_decl: &FnDecl) {
    for input in &fn_decl.inputs {
        match *input {
            FnArg::SelfRef(_, _) |
            FnArg::SelfValue(_) => {}
            FnArg::Captured(ref pat, ref ty) => {
                visitor.visit_pat(pat);
                visitor.visit_ty(ty);
            }
            FnArg::Ignored(ref ty) => {
                visitor.visit_ty(ty);
            }
        }
    }
    visitor.visit_fn_ret_ty(&fn_decl.output);
}

#[cfg(feature = "full")]
pub fn walk_trait_item<V: Visitor>(visitor: &mut V, trait_item: &TraitItem) {
    visitor.visit_ident(&trait_item.ident);
    walk_list!(visitor, visit_attribute, &trait_item.attrs);
    match trait_item.node {
        TraitItemKind::Const(ref ty, ref maybe_expr) => {
            visitor.visit_ty(ty);
            if let Some(ref expr) = *maybe_expr {
                visitor.visit_expr(expr);
            }
        }
        TraitItemKind::Method(ref method_sig, ref maybe_block) => {
            visitor.visit_method_sig(method_sig);
            if let Some(ref block) = *maybe_block {
                walk_list!(visitor, visit_stmt, &block.stmts);
            }
        }
        TraitItemKind::Type(ref bounds, ref maybe_ty) => {
            walk_list!(visitor, visit_ty_param_bound, bounds);
            if let Some(ref ty) = *maybe_ty {
                visitor.visit_ty(ty);
            }
        }
        TraitItemKind::Macro(ref mac) => {
            visitor.visit_mac(mac);
        }
    }
}

#[cfg(feature = "full")]
pub fn walk_impl_item<V: Visitor>(visitor: &mut V, impl_item: &ImplItem) {
    visitor.visit_ident(&impl_item.ident);
    walk_list!(visitor, visit_attribute, &impl_item.attrs);
    match impl_item.node {
        ImplItemKind::Const(ref ty, ref expr) => {
            visitor.visit_ty(ty);
            visitor.visit_expr(expr);
        }
        ImplItemKind::Method(ref method_sig, ref block) => {
            visitor.visit_method_sig(method_sig);
            walk_list!(visitor, visit_stmt, &block.stmts);
        }
        ImplItemKind::Type(ref ty) => {
            visitor.visit_ty(ty);
        }
        ImplItemKind::Macro(ref mac) => {
            visitor.visit_mac(mac);
        }
    }
}

#[cfg(feature = "full")]
pub fn walk_method_sig<V: Visitor>(visitor: &mut V, method_sig: &MethodSig) {
    visitor.visit_fn_decl(&method_sig.decl);
    visitor.visit_generics(&method_sig.generics);
}

#[cfg(feature = "full")]
pub fn walk_stmt<V: Visitor>(visitor: &mut V, stmt: &Stmt) {
    match *stmt {
        Stmt::Local(ref local) => {
            visitor.visit_local(local);
        }
        Stmt::Item(ref item) => {
            visitor.visit_item(item);
        }
        Stmt::Expr(ref expr) |
        Stmt::Semi(ref expr) => {
            visitor.visit_expr(expr);
        }
        Stmt::Mac(ref details) => {
            let (ref mac, _, ref attrs) = **details;
            visitor.visit_mac(mac);
            walk_list!(visitor, visit_attribute, attrs);
        }
    }
}

#[cfg(feature = "full")]
pub fn walk_local<V: Visitor>(visitor: &mut V, local: &Local) {
    visitor.visit_pat(&local.pat);
    if let Some(ref ty) = local.ty {
        visitor.visit_ty(ty);
    }
    if let Some(ref init) = local.init {
        visitor.visit_expr(init);
    }
    walk_list!(visitor, visit_attribute, &local.attrs);
}

#[cfg(feature = "full")]
pub fn walk_view_path<V: Visitor>(visitor: &mut V, view_path: &ViewPath) {
    match *view_path {
        ViewPath::Simple(ref path, ref maybe_ident) => {
            visitor.visit_path(path);
            walk_opt_ident(visitor, maybe_ident);
        }
        ViewPath::Glob(ref path) => {
            visitor.visit_path(path);
        }
        ViewPath::List(ref path, ref items) => {
            visitor.visit_path(path);
            for &PathListItem { ref name, ref rename } in items {
                visitor.visit_ident(name);
                walk_opt_ident(visitor, rename);
            }
        }
    }
}
