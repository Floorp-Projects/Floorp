// Adapted from libsyntax.

//! A Folder represents an AST->AST fold; it accepts an AST piece,
//! and returns a piece of the same type.

use super::*;
#[cfg(not(feature = "full"))]
use constant;

/// AST->AST fold.
///
/// Each method of the Folder trait is a hook to be potentially overridden. Each
/// method's default implementation recursively visits the substructure of the
/// input via the `noop_fold` methods, which perform an "identity fold", that
/// is, they return the same structure that they are given (for example the
/// `fold_crate` method by default calls `fold::noop_fold_crate`).
///
/// If you want to ensure that your code handles every variant explicitly, you
/// need to override each method and monitor future changes to `Folder` in case
/// a new method with a new default implementation gets introduced.
pub trait Folder {
    // Any additions to this trait should happen in form
    // of a call to a public `noop_*` function that only calls
    // out to the folder again, not other `noop_*` functions.
    //
    // This is a necessary API workaround to the problem of not
    // being able to call out to the super default method
    // in an overridden default method.

    fn fold_ident(&mut self, _ident: Ident) -> Ident {
        noop_fold_ident(self, _ident)
    }
    fn fold_derive_input(&mut self, derive_input: DeriveInput) -> DeriveInput {
        noop_fold_derive_input(self, derive_input)
    }
    fn fold_ty(&mut self, ty: Ty) -> Ty {
        noop_fold_ty(self, ty)
    }
    fn fold_generics(&mut self, generics: Generics) -> Generics {
        noop_fold_generics(self, generics)
    }
    fn fold_ty_param_bound(&mut self, bound: TyParamBound) -> TyParamBound {
        noop_fold_ty_param_bound(self, bound)
    }
    fn fold_poly_trait_ref(&mut self, trait_ref: PolyTraitRef) -> PolyTraitRef {
        noop_fold_poly_trait_ref(self, trait_ref)
    }
    fn fold_variant_data(&mut self, data: VariantData) -> VariantData {
        noop_fold_variant_data(self, data)
    }
    fn fold_field(&mut self, field: Field) -> Field {
        noop_fold_field(self, field)
    }
    fn fold_variant(&mut self, variant: Variant) -> Variant {
        noop_fold_variant(self, variant)
    }
    fn fold_lifetime(&mut self, _lifetime: Lifetime) -> Lifetime {
        noop_fold_lifetime(self, _lifetime)
    }
    fn fold_lifetime_def(&mut self, lifetime: LifetimeDef) -> LifetimeDef {
        noop_fold_lifetime_def(self, lifetime)
    }
    fn fold_path(&mut self, path: Path) -> Path {
        noop_fold_path(self, path)
    }
    fn fold_path_segment(&mut self, path_segment: PathSegment) -> PathSegment {
        noop_fold_path_segment(self, path_segment)
    }
    fn fold_path_parameters(&mut self, path_parameters: PathParameters) -> PathParameters {
        noop_fold_path_parameters(self, path_parameters)
    }
    fn fold_assoc_type_binding(&mut self, type_binding: TypeBinding) -> TypeBinding {
        noop_fold_assoc_type_binding(self, type_binding)
    }
    fn fold_attribute(&mut self, _attr: Attribute) -> Attribute {
        noop_fold_attribute(self, _attr)
    }
    fn fold_fn_ret_ty(&mut self, ret_ty: FunctionRetTy) -> FunctionRetTy {
        noop_fold_fn_ret_ty(self, ret_ty)
    }
    fn fold_const_expr(&mut self, expr: ConstExpr) -> ConstExpr {
        noop_fold_const_expr(self, expr)
    }
    fn fold_lit(&mut self, _lit: Lit) -> Lit {
        noop_fold_lit(self, _lit)
    }

    fn fold_mac(&mut self, mac: Mac) -> Mac {
        noop_fold_mac(self, mac)
    }

    #[cfg(feature = "full")]
    fn fold_crate(&mut self, _crate: Crate) -> Crate {
        noop_fold_crate(self, _crate)
    }
    #[cfg(feature = "full")]
    fn fold_item(&mut self, item: Item) -> Item {
        noop_fold_item(self, item)
    }
    #[cfg(feature = "full")]
    fn fold_expr(&mut self, expr: Expr) -> Expr {
        noop_fold_expr(self, expr)
    }
    #[cfg(feature = "full")]
    fn fold_foreign_item(&mut self, foreign_item: ForeignItem) -> ForeignItem {
        noop_fold_foreign_item(self, foreign_item)
    }
    #[cfg(feature = "full")]
    fn fold_pat(&mut self, pat: Pat) -> Pat {
        noop_fold_pat(self, pat)
    }
    #[cfg(feature = "full")]
    fn fold_fn_decl(&mut self, fn_decl: FnDecl) -> FnDecl {
        noop_fold_fn_decl(self, fn_decl)
    }
    #[cfg(feature = "full")]
    fn fold_trait_item(&mut self, trait_item: TraitItem) -> TraitItem {
        noop_fold_trait_item(self, trait_item)
    }
    #[cfg(feature = "full")]
    fn fold_impl_item(&mut self, impl_item: ImplItem) -> ImplItem {
        noop_fold_impl_item(self, impl_item)
    }
    #[cfg(feature = "full")]
    fn fold_method_sig(&mut self, method_sig: MethodSig) -> MethodSig {
        noop_fold_method_sig(self, method_sig)
    }
    #[cfg(feature = "full")]
    fn fold_stmt(&mut self, stmt: Stmt) -> Stmt {
        noop_fold_stmt(self, stmt)
    }
    #[cfg(feature = "full")]
    fn fold_block(&mut self, block: Block) -> Block {
        noop_fold_block(self, block)
    }
    #[cfg(feature = "full")]
    fn fold_local(&mut self, local: Local) -> Local {
        noop_fold_local(self, local)
    }
    #[cfg(feature = "full")]
    fn fold_view_path(&mut self, view_path: ViewPath) -> ViewPath {
        noop_fold_view_path(self, view_path)
    }
}

trait LiftOnce<T, U> {
    type Output;
    fn lift<F>(self, f: F) -> Self::Output where F: FnOnce(T) -> U;
}

impl<T, U> LiftOnce<T, U> for Box<T> {
    type Output = Box<U>;
    // Clippy false positive
    // https://github.com/Manishearth/rust-clippy/issues/1478
    #[cfg_attr(feature = "cargo-clippy", allow(boxed_local))]
    fn lift<F>(self, f: F) -> Box<U>
        where F: FnOnce(T) -> U
    {
        Box::new(f(*self))
    }
}

trait LiftMut<T, U> {
    type Output;
    fn lift<F>(self, f: F) -> Self::Output where F: FnMut(T) -> U;
}

impl<T, U> LiftMut<T, U> for Vec<T> {
    type Output = Vec<U>;
    fn lift<F>(self, f: F) -> Vec<U>
        where F: FnMut(T) -> U
    {
        self.into_iter().map(f).collect()
    }
}

pub fn noop_fold_ident<F: ?Sized + Folder>(_: &mut F, _ident: Ident) -> Ident {
    _ident
}

pub fn noop_fold_derive_input<F: ?Sized + Folder>(folder: &mut F,
                                         DeriveInput{ ident,
                                                      vis,
                                                      attrs,
                                                      generics,
body }: DeriveInput) -> DeriveInput{
    use Body::*;
    DeriveInput {
        ident: folder.fold_ident(ident),
        vis: noop_fold_vis(folder, vis),
        attrs: attrs.lift(|a| folder.fold_attribute(a)),
        generics: folder.fold_generics(generics),
        body: match body {
            Enum(variants) => Enum(variants.lift(move |v| folder.fold_variant(v))),
            Struct(variant_data) => Struct(folder.fold_variant_data(variant_data)),
        },
    }
}

pub fn noop_fold_ty<F: ?Sized + Folder>(folder: &mut F, ty: Ty) -> Ty {
    use Ty::*;
    match ty {
        Slice(inner) => Slice(inner.lift(|v| folder.fold_ty(v))),
        Paren(inner) => Paren(inner.lift(|v| folder.fold_ty(v))),
        Ptr(mutable_type) => {
            let mutable_type_ = *mutable_type;
            let MutTy { ty, mutability }: MutTy = mutable_type_;
            Ptr(Box::new(MutTy {
                             ty: folder.fold_ty(ty),
                             mutability: mutability,
                         }))
        }
        Rptr(opt_lifetime, mutable_type) => {
            let mutable_type_ = *mutable_type;
            let MutTy { ty, mutability }: MutTy = mutable_type_;
            Rptr(opt_lifetime.map(|l| folder.fold_lifetime(l)),
                 Box::new(MutTy {
                              ty: folder.fold_ty(ty),
                              mutability: mutability,
                          }))
        }
        Never => Never,
        Infer => Infer,
        Tup(tuple_element_types) => Tup(tuple_element_types.lift(|x| folder.fold_ty(x))),
        BareFn(bare_fn) => {
            let bf_ = *bare_fn;
            let BareFnTy { unsafety, abi, lifetimes, inputs, output, variadic } = bf_;
            BareFn(Box::new(BareFnTy {
                                unsafety: unsafety,
                                abi: abi,
                                lifetimes: lifetimes.lift(|l| folder.fold_lifetime_def(l)),
                                inputs: inputs.lift(|v| {
                BareFnArg {
                    name: v.name.map(|n| folder.fold_ident(n)),
                    ty: folder.fold_ty(v.ty),
                }
            }),
                                output: folder.fold_fn_ret_ty(output),
                                variadic: variadic,
                            }))
        }
        Path(maybe_qself, path) => {
            Path(maybe_qself.map(|v| noop_fold_qself(folder, v)),
                 folder.fold_path(path))
        }
        Array(inner, len) => {
            Array({
                      inner.lift(|v| folder.fold_ty(v))
                  },
                  folder.fold_const_expr(len))
        }
        TraitObject(bounds) => TraitObject(bounds.lift(|v| folder.fold_ty_param_bound(v))),
        ImplTrait(bounds) => ImplTrait(bounds.lift(|v| folder.fold_ty_param_bound(v))),
        Mac(mac) => Mac(folder.fold_mac(mac)),
    }
}

fn noop_fold_qself<F: ?Sized + Folder>(folder: &mut F, QSelf { ty, position }: QSelf) -> QSelf {
    QSelf {
        ty: Box::new(folder.fold_ty(*(ty))),
        position: position,
    }
}

pub fn noop_fold_generics<F: ?Sized + Folder>(folder: &mut F,
                                     Generics { lifetimes, ty_params, where_clause }: Generics)
-> Generics{
    use WherePredicate::*;
    Generics {
        lifetimes: lifetimes.lift(|l| folder.fold_lifetime_def(l)),
        ty_params: ty_params.lift(|ty| {
            TyParam {
                attrs: ty.attrs.lift(|a| folder.fold_attribute(a)),
                ident: folder.fold_ident(ty.ident),
                bounds: ty.bounds.lift(|ty_pb| folder.fold_ty_param_bound(ty_pb)),
                default: ty.default.map(|v| folder.fold_ty(v)),
            }
        }),
        where_clause: WhereClause {
            predicates: where_clause.predicates.lift(|p| match p {
                                                         BoundPredicate(bound_predicate) => {
                        BoundPredicate(WhereBoundPredicate {
                            bound_lifetimes: bound_predicate.bound_lifetimes
                                .lift(|l| folder.fold_lifetime_def(l)),
                            bounded_ty: folder.fold_ty(bound_predicate.bounded_ty),
                            bounds: bound_predicate.bounds
                                .lift(|ty_pb| folder.fold_ty_param_bound(ty_pb)),
                        })
                    }
                                                         RegionPredicate(region_predicate) => {
                        RegionPredicate(WhereRegionPredicate {
                            lifetime: folder.fold_lifetime(region_predicate.lifetime),
                            bounds: region_predicate.bounds
                                .lift(|b| folder.fold_lifetime(b)),
                        })
                    }
                                                         EqPredicate(eq_predicate) => {
                        EqPredicate(WhereEqPredicate {
                            lhs_ty: folder.fold_ty(eq_predicate.lhs_ty),
                            rhs_ty: folder.fold_ty(eq_predicate.rhs_ty),
                        })
                    }
                                                     }),
        },
    }
}

pub fn noop_fold_ty_param_bound<F: ?Sized + Folder>(folder: &mut F,
                                                    bound: TyParamBound)
                                                    -> TyParamBound {
    use TyParamBound::*;
    match bound {
        Trait(ty, modifier) => Trait(folder.fold_poly_trait_ref(ty), modifier),
        Region(lifetime) => Region(folder.fold_lifetime(lifetime)),
    }
}

pub fn noop_fold_poly_trait_ref<F: ?Sized + Folder>(folder: &mut F,
                                                    trait_ref: PolyTraitRef)
                                                    -> PolyTraitRef {
    PolyTraitRef {
        bound_lifetimes: trait_ref.bound_lifetimes.lift(|bl| folder.fold_lifetime_def(bl)),
        trait_ref: folder.fold_path(trait_ref.trait_ref),
    }
}

pub fn noop_fold_variant_data<F: ?Sized + Folder>(folder: &mut F,
                                                  data: VariantData)
                                                  -> VariantData {
    use VariantData::*;
    match data {
        Struct(fields) => Struct(fields.lift(|f| folder.fold_field(f))),
        Tuple(fields) => Tuple(fields.lift(|f| folder.fold_field(f))),
        Unit => Unit,
    }
}

pub fn noop_fold_field<F: ?Sized + Folder>(folder: &mut F, field: Field) -> Field {
    Field {
        ident: field.ident.map(|i| folder.fold_ident(i)),
        vis: noop_fold_vis(folder, field.vis),
        attrs: field.attrs.lift(|a| folder.fold_attribute(a)),
        ty: folder.fold_ty(field.ty),
    }
}

pub fn noop_fold_variant<F: ?Sized + Folder>(folder: &mut F,
                                    Variant { ident, attrs, data, discriminant }: Variant)
-> Variant{
    Variant {
        ident: folder.fold_ident(ident),
        attrs: attrs.lift(|v| folder.fold_attribute(v)),
        data: folder.fold_variant_data(data),
        discriminant: discriminant.map(|ce| folder.fold_const_expr(ce)),
    }
}

pub fn noop_fold_lifetime<F: ?Sized + Folder>(folder: &mut F, _lifetime: Lifetime) -> Lifetime {
    Lifetime { ident: folder.fold_ident(_lifetime.ident) }
}

pub fn noop_fold_lifetime_def<F: ?Sized + Folder>(folder: &mut F,
                                         LifetimeDef { attrs, lifetime, bounds }: LifetimeDef)
-> LifetimeDef{
    LifetimeDef {
        attrs: attrs.lift(|x| folder.fold_attribute(x)),
        lifetime: folder.fold_lifetime(lifetime),
        bounds: bounds.lift(|l| folder.fold_lifetime(l)),
    }
}

pub fn noop_fold_path<F: ?Sized + Folder>(folder: &mut F, Path { global, segments }: Path) -> Path {
    Path {
        global: global,
        segments: segments.lift(|s| folder.fold_path_segment(s)),
    }
}

pub fn noop_fold_path_segment<F: ?Sized + Folder>(folder: &mut F,
                                                  PathSegment { ident, parameters }: PathSegment)
                                                  -> PathSegment {
    PathSegment {
        ident: folder.fold_ident(ident),
        parameters: folder.fold_path_parameters(parameters),
    }
}

pub fn noop_fold_path_parameters<F: ?Sized + Folder>(folder: &mut F,
                                                     path_parameters: PathParameters)
                                                     -> PathParameters {
    use PathParameters::*;
    match path_parameters {
        AngleBracketed(d) => {
            let AngleBracketedParameterData { lifetimes, types, bindings } = d;
            AngleBracketed(AngleBracketedParameterData {
                               lifetimes: lifetimes.into_iter()
                                   .map(|l| folder.fold_lifetime(l))
                                   .collect(),
                               types: types.lift(|ty| folder.fold_ty(ty)),
                               bindings: bindings.lift(|tb| folder.fold_assoc_type_binding(tb)),
                           })
        }
        Parenthesized(d) => {
            let ParenthesizedParameterData { inputs, output } = d;
            Parenthesized(ParenthesizedParameterData {
                              inputs: inputs.lift(|i| folder.fold_ty(i)),
                              output: output.map(|v| folder.fold_ty(v)),
                          })
        }
    }
}

pub fn noop_fold_assoc_type_binding<F: ?Sized + Folder>(folder: &mut F,
                                                        TypeBinding { ident, ty }: TypeBinding)
                                                        -> TypeBinding {
    TypeBinding {
        ident: folder.fold_ident(ident),
        ty: folder.fold_ty(ty),
    }

}

pub fn noop_fold_attribute<F: ?Sized + Folder>(_: &mut F, _attr: Attribute) -> Attribute {
    _attr
}

pub fn noop_fold_fn_ret_ty<F: ?Sized + Folder>(folder: &mut F,
                                               ret_ty: FunctionRetTy)
                                               -> FunctionRetTy {
    use FunctionRetTy::*;
    match ret_ty {
        Default => Default,
        Ty(ty) => Ty(folder.fold_ty(ty)),
    }
}

pub fn noop_fold_const_expr<F: ?Sized + Folder>(folder: &mut F, expr: ConstExpr) -> ConstExpr {
    use ConstExpr::*;
    match expr {
        Call(f, args) => {
            Call(f.lift(|e| folder.fold_const_expr(e)),
                 args.lift(|v| folder.fold_const_expr(v)))
        }
        Binary(op, lhs, rhs) => {
            Binary(op,
                   lhs.lift(|e| folder.fold_const_expr(e)),
                   rhs.lift(|e| folder.fold_const_expr(e)))
        }
        Unary(op, e) => Unary(op, e.lift(|e| folder.fold_const_expr(e))),
        Lit(l) => Lit(folder.fold_lit(l)),
        Cast(e, ty) => {
            Cast(e.lift(|e| folder.fold_const_expr(e)),
                 ty.lift(|v| folder.fold_ty(v)))
        }
        Path(p) => Path(folder.fold_path(p)),
        Index(o, i) => {
            Index(o.lift(|e| folder.fold_const_expr(e)),
                  i.lift(|e| folder.fold_const_expr(e)))
        }
        Paren(no_op) => Paren(no_op.lift(|e| folder.fold_const_expr(e))),
        Other(e) => Other(noop_fold_other_const_expr(folder, e)),
    }
}

#[cfg(feature = "full")]
fn noop_fold_other_const_expr<F: ?Sized + Folder>(folder: &mut F, e: Expr) -> Expr {
    folder.fold_expr(e)
}

#[cfg(not(feature = "full"))]
fn noop_fold_other_const_expr<F: ?Sized + Folder>(_: &mut F,
                                                  e: constant::Other)
                                                  -> constant::Other {
    e
}

pub fn noop_fold_lit<F: ?Sized + Folder>(_: &mut F, _lit: Lit) -> Lit {
    _lit
}

pub fn noop_fold_tt<F: ?Sized + Folder>(folder: &mut F, tt: TokenTree) -> TokenTree {
    use TokenTree::*;
    use Token::*;
    match tt {
        Token(token) => {
            Token(match token {
                      Literal(lit) => Literal(folder.fold_lit(lit)),
                      Ident(ident) => Ident(folder.fold_ident(ident)),
                      Lifetime(ident) => Lifetime(folder.fold_ident(ident)),
                      x => x,
                  })
        }
        Delimited(super::Delimited { delim, tts }) => {
            Delimited(super::Delimited {
                          delim: delim,
                          tts: tts.lift(|v| noop_fold_tt(folder, v)),
                      })
        }
    }
}

pub fn noop_fold_mac<F: ?Sized + Folder>(folder: &mut F, Mac { path, tts }: Mac) -> Mac {
    Mac {
        path: folder.fold_path(path),
        tts: tts.lift(|tt| noop_fold_tt(folder, tt)),
    }
}

#[cfg(feature = "full")]
pub fn noop_fold_crate<F: ?Sized + Folder>(folder: &mut F,
                                           Crate { shebang, attrs, items }: Crate)
                                           -> Crate {
    Crate {
        shebang: shebang,
        attrs: attrs.lift(|a| folder.fold_attribute(a)),
        items: items.lift(|i| folder.fold_item(i)),
    }

}

#[cfg(feature = "full")]
pub fn noop_fold_block<F: ?Sized + Folder>(folder: &mut F, block: Block) -> Block {
    Block { stmts: block.stmts.lift(|s| folder.fold_stmt(s)) }
}

fn noop_fold_vis<F: ?Sized + Folder>(folder: &mut F, vis: Visibility) -> Visibility {
    use Visibility::*;
    match vis {
        Crate => Crate,
        Inherited => Inherited,
        Public => Public,
        Restricted(path) => Restricted(path.lift(|p| folder.fold_path(p))),
    }
}

#[cfg(feature = "full")]
pub fn noop_fold_item<F: ?Sized + Folder>(folder: &mut F,
                                          Item { ident, vis, attrs, node }: Item)
                                          -> Item {
    use ItemKind::*;
    Item {
        ident: folder.fold_ident(ident.clone()),
        vis: noop_fold_vis(folder, vis),
        attrs: attrs.lift(|a| folder.fold_attribute(a)),
        node: match node {
            ExternCrate(name) => ExternCrate(name.map(|i| folder.fold_ident(i))),
            Use(view_path) => Use(Box::new(folder.fold_view_path(*view_path))),
            Static(ty, mutability, expr) => {
                Static(Box::new(folder.fold_ty(*ty)),
                       mutability,
                       expr.lift(|e| folder.fold_expr(e)))
            }
            Const(ty, expr) => {
                Const(ty.lift(|ty| folder.fold_ty(ty)),
                      expr.lift(|e| folder.fold_expr(e)))
            }
            Fn(fn_decl, unsafety, constness, abi, generics, block) => {
                Fn(fn_decl.lift(|v| folder.fold_fn_decl(v)),
                   unsafety,
                   constness,
                   abi,
                   folder.fold_generics(generics),
                   block.lift(|v| folder.fold_block(v)))
            }
            Mod(items) => Mod(items.map(|items| items.lift(|i| folder.fold_item(i)))),
            ForeignMod(super::ForeignMod { abi, items }) => {
                ForeignMod(super::ForeignMod {
                               abi: abi,
                               items: items.lift(|foreign_item| {
                                                     folder.fold_foreign_item(foreign_item)
                                                 }),
                           })
            }
            Ty(ty, generics) => {
                Ty(ty.lift(|ty| folder.fold_ty(ty)),
                   folder.fold_generics(generics))
            }
            Enum(variants, generics) => {
                Enum(variants.lift(|v| folder.fold_variant(v)),
                     folder.fold_generics(generics))
            }
            Struct(variant_data, generics) => {
                Struct(folder.fold_variant_data(variant_data),
                       folder.fold_generics(generics))
            }
            Union(variant_data, generics) => {
                Union(folder.fold_variant_data(variant_data),
                      folder.fold_generics(generics))
            }
            Trait(unsafety, generics, typbs, trait_items) => {
                Trait(unsafety,
                      folder.fold_generics(generics),
                      typbs.lift(|typb| folder.fold_ty_param_bound(typb)),
                      trait_items.lift(|ti| folder.fold_trait_item(ti)))
            }
            DefaultImpl(unsafety, path) => DefaultImpl(unsafety, folder.fold_path(path)),
            Impl(unsafety, impl_polarity, generics, path, ty, impl_items) => {
                Impl(unsafety,
                     impl_polarity,
                     folder.fold_generics(generics),
                     path.map(|p| folder.fold_path(p)),
                     ty.lift(|ty| folder.fold_ty(ty)),
                     impl_items.lift(|i| folder.fold_impl_item(i)))
            }
            Mac(mac) => Mac(folder.fold_mac(mac)),
        },
    }
}

#[cfg(feature = "full")]
pub fn noop_fold_expr<F: ?Sized + Folder>(folder: &mut F, Expr { node, attrs }: Expr) -> Expr {
    use ExprKind::*;
    Expr {
        node: match node {
            ExprKind::Box(e) => ExprKind::Box(e.lift(|e| folder.fold_expr(e))),
            InPlace(place, value) => {
                InPlace(place.lift(|e| folder.fold_expr(e)),
                        value.lift(|e| folder.fold_expr(e)))
            }
            Array(array) => Array(array.lift(|e| folder.fold_expr(e))),
            Call(function, args) => {
                Call(function.lift(|e| folder.fold_expr(e)),
                     args.lift(|e| folder.fold_expr(e)))
            }
            MethodCall(method, tys, args) => {
                MethodCall(folder.fold_ident(method),
                           tys.lift(|t| folder.fold_ty(t)),
                           args.lift(|e| folder.fold_expr(e)))
            }
            Tup(args) => Tup(args.lift(|e| folder.fold_expr(e))),
            Binary(bop, lhs, rhs) => {
                Binary(bop,
                       lhs.lift(|e| folder.fold_expr(e)),
                       rhs.lift(|e| folder.fold_expr(e)))
            }
            Unary(uop, e) => Unary(uop, e.lift(|e| folder.fold_expr(e))),
            Lit(lit) => Lit(folder.fold_lit(lit)),
            Cast(e, ty) => {
                Cast(e.lift(|e| folder.fold_expr(e)),
                     ty.lift(|t| folder.fold_ty(t)))
            }
            Type(e, ty) => {
                Type(e.lift(|e| folder.fold_expr(e)),
                     ty.lift(|t| folder.fold_ty(t)))
            }
            If(e, if_block, else_block) => {
                If(e.lift(|e| folder.fold_expr(e)),
                   folder.fold_block(if_block),
                   else_block.map(|v| v.lift(|e| folder.fold_expr(e))))
            }
            IfLet(pat, expr, block, else_block) => {
                IfLet(pat.lift(|p| folder.fold_pat(p)),
                      expr.lift(|e| folder.fold_expr(e)),
                      folder.fold_block(block),
                      else_block.map(|v| v.lift(|e| folder.fold_expr(e))))
            }
            While(e, block, label) => {
                While(e.lift(|e| folder.fold_expr(e)),
                      folder.fold_block(block),
                      label.map(|i| folder.fold_ident(i)))
            }
            WhileLet(pat, expr, block, label) => {
                WhileLet(pat.lift(|p| folder.fold_pat(p)),
                         expr.lift(|e| folder.fold_expr(e)),
                         folder.fold_block(block),
                         label.map(|i| folder.fold_ident(i)))
            }
            ForLoop(pat, expr, block, label) => {
                ForLoop(pat.lift(|p| folder.fold_pat(p)),
                        expr.lift(|e| folder.fold_expr(e)),
                        folder.fold_block(block),
                        label.map(|i| folder.fold_ident(i)))
            }
            Loop(block, label) => {
                Loop(folder.fold_block(block),
                     label.map(|i| folder.fold_ident(i)))
            }
            Match(e, arms) => {
                Match(e.lift(|e| folder.fold_expr(e)),
                      arms.lift(|Arm { attrs, pats, guard, body }: Arm| {
                    Arm {
                        attrs: attrs.lift(|a| folder.fold_attribute(a)),
                        pats: pats.lift(|p| folder.fold_pat(p)),
                        guard: guard.map(|v| v.lift(|e| folder.fold_expr(e))),
                        body: body.lift(|e| folder.fold_expr(e)),
                    }
                }))
            }
            Closure(capture_by, fn_decl, expr) => {
                Closure(capture_by,
                        fn_decl.lift(|v| folder.fold_fn_decl(v)),
                        expr.lift(|e| folder.fold_expr(e)))
            }
            Block(unsafety, block) => Block(unsafety, folder.fold_block(block)),
            Assign(lhs, rhs) => {
                Assign(lhs.lift(|e| folder.fold_expr(e)),
                       rhs.lift(|e| folder.fold_expr(e)))
            }
            AssignOp(bop, lhs, rhs) => {
                AssignOp(bop,
                         lhs.lift(|e| folder.fold_expr(e)),
                         rhs.lift(|e| folder.fold_expr(e)))
            }
            Field(expr, name) => Field(expr.lift(|e| folder.fold_expr(e)), folder.fold_ident(name)),
            TupField(expr, index) => TupField(expr.lift(|e| folder.fold_expr(e)), index),
            Index(expr, index) => {
                Index(expr.lift(|e| folder.fold_expr(e)),
                      index.lift(|e| folder.fold_expr(e)))
            }
            Range(lhs, rhs, limits) => {
                Range(lhs.map(|v| v.lift(|e| folder.fold_expr(e))),
                      rhs.map(|v| v.lift(|e| folder.fold_expr(e))),
                      limits)
            }
            Path(qself, path) => {
                Path(qself.map(|v| noop_fold_qself(folder, v)),
                     folder.fold_path(path))
            }
            AddrOf(mutability, expr) => AddrOf(mutability, expr.lift(|e| folder.fold_expr(e))),
            Break(label, expr) => {
                Break(label.map(|i| folder.fold_ident(i)),
                      expr.map(|v| v.lift(|e| folder.fold_expr(e))))
            }
            Continue(label) => Continue(label.map(|i| folder.fold_ident(i))),
            Ret(expr) => Ret(expr.map(|v| v.lift(|e| folder.fold_expr(e)))),
            ExprKind::Mac(mac) => ExprKind::Mac(folder.fold_mac(mac)),
            Struct(path, fields, expr) => {
                Struct(folder.fold_path(path),
                       fields.lift(|FieldValue { ident, expr, is_shorthand, attrs }: FieldValue| {
                    FieldValue {
                        ident: folder.fold_ident(ident),
                        expr: folder.fold_expr(expr),
                        is_shorthand: is_shorthand,
                        attrs: attrs.lift(|v| folder.fold_attribute(v)),
                    }
                }),
                       expr.map(|v| v.lift(|e| folder.fold_expr(e))))
            }
            Repeat(element, number) => {
                Repeat(element.lift(|e| folder.fold_expr(e)),
                       number.lift(|e| folder.fold_expr(e)))
            }
            Paren(expr) => Paren(expr.lift(|e| folder.fold_expr(e))),
            Try(expr) => Try(expr.lift(|e| folder.fold_expr(e))),
        },
        attrs: attrs.into_iter().map(|a| folder.fold_attribute(a)).collect(),
    }
}

#[cfg(feature = "full")]
pub fn noop_fold_foreign_item<F: ?Sized + Folder>(folder: &mut F,
                                         ForeignItem { ident, attrs, node, vis }: ForeignItem)
-> ForeignItem{
    ForeignItem {
        ident: folder.fold_ident(ident),
        attrs: attrs.into_iter().map(|a| folder.fold_attribute(a)).collect(),
        node: match node {
            ForeignItemKind::Fn(fn_dcl, generics) => {
                ForeignItemKind::Fn(fn_dcl.lift(|v| folder.fold_fn_decl(v)),
                                    folder.fold_generics(generics))
            }
            ForeignItemKind::Static(ty, mutability) => {
                ForeignItemKind::Static(ty.lift(|v| folder.fold_ty(v)), mutability)
            }
        },
        vis: noop_fold_vis(folder, vis),
    }
}

#[cfg(feature = "full")]
pub fn noop_fold_pat<F: ?Sized + Folder>(folder: &mut F, pat: Pat) -> Pat {
    use Pat::*;
    match pat {
        Wild => Wild,
        Ident(binding_mode, ident, pat) => {
            Ident(binding_mode,
                  folder.fold_ident(ident),
                  pat.map(|p| p.lift(|p| folder.fold_pat(p))))
        }
        Struct(path, field_patterns, dots) => {
            Struct(folder.fold_path(path),
                   field_patterns.lift(|FieldPat { ident, pat, is_shorthand, attrs }: FieldPat| {
                    FieldPat {
                        ident: folder.fold_ident(ident),
                        pat: pat.lift(|p| folder.fold_pat(p)),
                        is_shorthand: is_shorthand,
                        attrs: attrs.lift(|a| folder.fold_attribute(a)),
                    }
                }),
                   dots)
        }
        TupleStruct(path, pats, len) => {
            TupleStruct(folder.fold_path(path),
                        pats.lift(|p| folder.fold_pat(p)),
                        len)
        }
        Path(qself, path) => {
            Path(qself.map(|v| noop_fold_qself(folder, v)),
                 folder.fold_path(path))
        }
        Tuple(pats, len) => Tuple(pats.lift(|p| folder.fold_pat(p)), len),
        Box(b) => Box(b.lift(|p| folder.fold_pat(p))),
        Ref(b, mutability) => Ref(b.lift(|p| folder.fold_pat(p)), mutability),
        Lit(expr) => Lit(expr.lift(|e| folder.fold_expr(e))),
        Range(l, r) => {
            Range(l.lift(|e| folder.fold_expr(e)),
                  r.lift(|e| folder.fold_expr(e)))
        }
        Slice(lefts, pat, rights) => {
            Slice(lefts.lift(|p| folder.fold_pat(p)),
                  pat.map(|v| v.lift(|p| folder.fold_pat(p))),
                  rights.lift(|p| folder.fold_pat(p)))
        }
        Mac(mac) => Mac(folder.fold_mac(mac)),
    }
}

#[cfg(feature = "full")]
pub fn noop_fold_fn_decl<F: ?Sized + Folder>(folder: &mut F,
                                             FnDecl { inputs, output, variadic }: FnDecl)
                                             -> FnDecl {

    FnDecl {
        inputs: inputs.lift(|a| {
            use FnArg::*;
            match a {
                SelfRef(lifetime, mutability) => {
                    SelfRef(lifetime.map(|v| folder.fold_lifetime(v)), mutability)
                }
                SelfValue(mutability) => SelfValue(mutability),
                Captured(pat, ty) => Captured(folder.fold_pat(pat), folder.fold_ty(ty)),
                Ignored(ty) => Ignored(folder.fold_ty(ty)),
            }
        }),
        output: folder.fold_fn_ret_ty(output),
        variadic: variadic,
    }

}

#[cfg(feature = "full")]
pub fn noop_fold_trait_item<F: ?Sized + Folder>(folder: &mut F,
                                                TraitItem { ident, attrs, node }: TraitItem)
                                                -> TraitItem {
    use TraitItemKind::*;
    TraitItem {
        ident: folder.fold_ident(ident),
        attrs: attrs.lift(|v| folder.fold_attribute(v)),
        node: match node {
            Const(ty, expr) => Const(folder.fold_ty(ty), expr.map(|v| folder.fold_expr(v))),
            Method(sig, block) => {
                Method(folder.fold_method_sig(sig),
                       block.map(|v| folder.fold_block(v)))
            }
            Type(ty_pbs, ty) => {
                Type(ty_pbs.lift(|v| folder.fold_ty_param_bound(v)),
                     ty.map(|v| folder.fold_ty(v)))
            }
            Macro(mac) => Macro(folder.fold_mac(mac)),
        },
    }
}

#[cfg(feature = "full")]
pub fn noop_fold_impl_item<F: ?Sized + Folder>(folder: &mut F,
                                      ImplItem { ident, vis, defaultness, attrs, node }: ImplItem)
-> ImplItem{
    use ImplItemKind::*;
    ImplItem {
        ident: folder.fold_ident(ident),
        vis: noop_fold_vis(folder, vis),
        defaultness: defaultness,
        attrs: attrs.lift(|v| folder.fold_attribute(v)),
        node: match node {
            Const(ty, expr) => Const(folder.fold_ty(ty), folder.fold_expr(expr)),
            Method(sig, block) => Method(folder.fold_method_sig(sig), folder.fold_block(block)),
            Type(ty) => Type(folder.fold_ty(ty)),
            Macro(mac) => Macro(folder.fold_mac(mac)),
        },
    }
}

#[cfg(feature = "full")]
pub fn noop_fold_method_sig<F: ?Sized + Folder>(folder: &mut F, MethodSig{unsafety, constness, abi, decl, generics}:MethodSig) -> MethodSig{
    MethodSig {
        unsafety: unsafety,
        constness: constness,
        abi: abi,
        decl: folder.fold_fn_decl(decl),
        generics: folder.fold_generics(generics),
    }

}

#[cfg(feature = "full")]
pub fn noop_fold_stmt<F: ?Sized + Folder>(folder: &mut F, stmt: Stmt) -> Stmt {
    use Stmt::*;
    match stmt {
        Local(local) => Local(local.lift(|l| folder.fold_local(l))),
        Item(item) => Item(item.lift(|v| folder.fold_item(v))),
        Expr(expr) => Expr(expr.lift(|v| folder.fold_expr(v))),
        Semi(expr) => Semi(expr.lift(|v| folder.fold_expr(v))),
        Mac(mac_stmt) => {
            Mac(mac_stmt.lift(|(mac, style, attrs)| {
                                  (folder.fold_mac(mac),
                                   style,
                                   attrs.lift(|a| folder.fold_attribute(a)))
                              }))
        }
    }

}

#[cfg(feature = "full")]
pub fn noop_fold_local<F: ?Sized + Folder>(folder: &mut F,
                                           Local { pat, ty, init, attrs }: Local)
                                           -> Local {
    Local {
        pat: pat.lift(|v| folder.fold_pat(v)),
        ty: ty.map(|v| v.lift(|t| folder.fold_ty(t))),
        init: init.map(|v| v.lift(|e| folder.fold_expr(e))),
        attrs: attrs.lift(|a| folder.fold_attribute(a)),
    }
}

#[cfg(feature = "full")]
pub fn noop_fold_view_path<F: ?Sized + Folder>(folder: &mut F, view_path: ViewPath) -> ViewPath {
    use ViewPath::*;
    match view_path {
        Simple(path, ident) => Simple(folder.fold_path(path), ident.map(|i| folder.fold_ident(i))),
        Glob(path) => Glob(folder.fold_path(path)),
        List(path, items) => {
            List(folder.fold_path(path),
                 items.lift(|PathListItem { name, rename }: PathListItem| {
                                PathListItem {
                                    name: folder.fold_ident(name),
                                    rename: rename.map(|i| folder.fold_ident(i)),
                                }
                            }))
        }
    }
}
