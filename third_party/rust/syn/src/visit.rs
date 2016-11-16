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
    fn visit_macro_input(&mut self, macro_input: &MacroInput) {
        walk_macro_input(self, macro_input)
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
}

#[macro_export]
macro_rules! walk_list {
    ($visitor: expr, $method: ident, $list: expr) => {
        for elem in $list {
            $visitor.$method(elem)
        }
    };
    ($visitor: expr, $method: ident, $list: expr, $($extra_args: expr),*) => {
        for elem in $list {
            $visitor.$method(elem, $($extra_args,)*)
        }
    }
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

pub fn walk_macro_input<V: Visitor>(visitor: &mut V, macro_input: &MacroInput) {
    visitor.visit_ident(&macro_input.ident);
    visitor.visit_generics(&macro_input.generics);
    match macro_input.body {
        Body::Enum(ref variants) => {
            walk_list!(visitor, visit_variant, variants, &macro_input.generics);
        }
        Body::Struct(ref variant_data) => {
            visitor.visit_variant_data(variant_data, &macro_input.ident, &macro_input.generics);
        }
    }
    walk_list!(visitor, visit_attribute, &macro_input.attrs);
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
        Ty::Never => {}
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
        Ty::ObjectSum(ref inner, ref bounds) => {
            visitor.visit_ty(inner);
            walk_list!(visitor, visit_ty_param_bound, bounds);
        }
        Ty::Array(ref inner, ref len) => {
            visitor.visit_ty(inner);
            visitor.visit_const_expr(len);
        }
        Ty::PolyTraitRef(ref bounds) => {
            walk_list!(visitor, visit_ty_param_bound, bounds);
        }
        Ty::ImplTrait(ref bounds) => {
            walk_list!(visitor, visit_ty_param_bound, bounds);
        }
        Ty::Infer => {}
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
            visitor.visit_const_expr(&function);
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
    }
}
