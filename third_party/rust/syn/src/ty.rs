use super::*;

/// The different kinds of types recognized by the compiler
#[derive(Debug, Clone, Eq, PartialEq)]
pub enum Ty {
    /// A variable-length array (`[T]`)
    Slice(Box<Ty>),
    /// A fixed length array (`[T; n]`)
    Array(Box<Ty>, ConstExpr),
    /// A raw pointer (`*const T` or `*mut T`)
    Ptr(Box<MutTy>),
    /// A reference (`&'a T` or `&'a mut T`)
    Rptr(Option<Lifetime>, Box<MutTy>),
    /// A bare function (e.g. `fn(usize) -> bool`)
    BareFn(Box<BareFnTy>),
    /// The never type (`!`)
    Never,
    /// A tuple (`(A, B, C, D, ...)`)
    Tup(Vec<Ty>),
    /// A path (`module::module::...::Type`), optionally
    /// "qualified", e.g. `<Vec<T> as SomeTrait>::SomeType`.
    ///
    /// Type parameters are stored in the Path itself
    Path(Option<QSelf>, Path),
    /// Something like `A+B`. Note that `B` must always be a path.
    ObjectSum(Box<Ty>, Vec<TyParamBound>),
    /// A type like `for<'a> Foo<&'a Bar>`
    PolyTraitRef(Vec<TyParamBound>),
    /// An `impl TraitA+TraitB` type.
    ImplTrait(Vec<TyParamBound>),
    /// No-op; kept solely so that we can pretty-print faithfully
    Paren(Box<Ty>),
    /// TyKind::Infer means the type should be inferred instead of it having been
    /// specified. This can appear anywhere in a type.
    Infer,
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct MutTy {
    pub ty: Ty,
    pub mutability: Mutability,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum Mutability {
    Mutable,
    Immutable,
}

/// A "Path" is essentially Rust's notion of a name.
///
/// It's represented as a sequence of identifiers,
/// along with a bunch of supporting information.
///
/// E.g. `std::cmp::PartialEq`
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct Path {
    pub global: bool,
    pub segments: Vec<PathSegment>,
}

impl<T> From<T> for Path
    where T: Into<PathSegment>
{
    fn from(segment: T) -> Self {
        Path {
            global: false,
            segments: vec![segment.into()],
        }
    }
}

/// A segment of a path: an identifier, an optional lifetime, and a set of types.
///
/// E.g. `std`, `String` or `Box<T>`
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct PathSegment {
    pub ident: Ident,
    pub parameters: PathParameters,
}

impl<T> From<T> for PathSegment
    where T: Into<Ident>
{
    fn from(ident: T) -> Self {
        PathSegment {
            ident: ident.into(),
            parameters: PathParameters::none(),
        }
    }
}

/// Parameters of a path segment.
///
/// E.g. `<A, B>` as in `Foo<A, B>` or `(A, B)` as in `Foo(A, B)`
#[derive(Debug, Clone, Eq, PartialEq)]
pub enum PathParameters {
    /// The `<'a, A, B, C>` in `foo::bar::baz::<'a, A, B, C>`
    AngleBracketed(AngleBracketedParameterData),
    /// The `(A, B)` and `C` in `Foo(A, B) -> C`
    Parenthesized(ParenthesizedParameterData),
}

impl PathParameters {
    pub fn none() -> Self {
        PathParameters::AngleBracketed(AngleBracketedParameterData::default())
    }
}

/// A path like `Foo<'a, T>`
#[derive(Debug, Clone, Eq, PartialEq, Default)]
pub struct AngleBracketedParameterData {
    /// The lifetime parameters for this path segment.
    pub lifetimes: Vec<Lifetime>,
    /// The type parameters for this path segment, if present.
    pub types: Vec<Ty>,
    /// Bindings (equality constraints) on associated types, if present.
    ///
    /// E.g., `Foo<A=Bar>`.
    pub bindings: Vec<TypeBinding>,
}

/// Bind a type to an associated type: `A=Foo`.
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct TypeBinding {
    pub ident: Ident,
    pub ty: Ty,
}

/// A path like `Foo(A,B) -> C`
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct ParenthesizedParameterData {
    /// `(A, B)`
    pub inputs: Vec<Ty>,
    /// `C`
    pub output: Option<Ty>,
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct PolyTraitRef {
    /// The `'a` in `<'a> Foo<&'a T>`
    pub bound_lifetimes: Vec<LifetimeDef>,
    /// The `Foo<&'a T>` in `<'a> Foo<&'a T>`
    pub trait_ref: Path,
}

/// The explicit Self type in a "qualified path". The actual
/// path, including the trait and the associated item, is stored
/// separately. `position` represents the index of the associated
/// item qualified with this Self type.
///
/// ```rust,ignore
/// <Vec<T> as a::b::Trait>::AssociatedItem
///  ^~~~~     ~~~~~~~~~~~~~~^
///  ty        position = 3
///
/// <Vec<T>>::AssociatedItem
///  ^~~~~    ^
///  ty       position = 0
/// ```
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct QSelf {
    pub ty: Box<Ty>,
    pub position: usize,
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct BareFnTy {
    pub lifetimes: Vec<LifetimeDef>,
    pub inputs: Vec<BareFnArg>,
    pub output: FunctionRetTy,
}

/// An argument in a function type.
///
/// E.g. `bar: usize` as in `fn foo(bar: usize)`
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct BareFnArg {
    pub name: Option<Ident>,
    pub ty: Ty,
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub enum FunctionRetTy {
    /// Return type is not specified.
    ///
    /// Functions default to `()` and
    /// closures default to inference. Span points to where return
    /// type would be inserted.
    Default,
    /// Everything else
    Ty(Ty),
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use {TraitBoundModifier, TyParamBound};
    use constant::parsing::const_expr;
    use generics::parsing::{lifetime, lifetime_def, ty_param_bound, bound_lifetimes};
    use ident::parsing::ident;
    use std::str;

    named!(pub ty -> Ty, alt!(
        ty_poly_trait_ref // must be before ty_path
        |
        ty_vec
        |
        ty_array
        |
        ty_ptr
        |
        ty_rptr
        |
        ty_bare_fn
        |
        ty_never
        |
        ty_tup
        |
        ty_path
        |
        ty_impl_trait
        |
        ty_paren
    ));

    named!(ty_vec -> Ty, do_parse!(
        punct!("[") >>
        elem: ty >>
        punct!("]") >>
        (Ty::Slice(Box::new(elem)))
    ));

    named!(ty_array -> Ty, do_parse!(
        punct!("[") >>
        elem: ty >>
        punct!(";") >>
        len: const_expr >>
        punct!("]") >>
        (Ty::Array(Box::new(elem), len))
    ));

    named!(ty_ptr -> Ty, do_parse!(
        punct!("*") >>
        mutability: alt!(
            keyword!("const") => { |_| Mutability::Immutable }
            |
            keyword!("mut") => { |_| Mutability::Mutable }
        ) >>
        target: ty >>
        (Ty::Ptr(Box::new(MutTy {
            ty: target,
            mutability: mutability,
        })))
    ));

    named!(ty_rptr -> Ty, do_parse!(
        punct!("&") >>
        life: option!(lifetime) >>
        mutability: mutability >>
        target: ty >>
        (Ty::Rptr(life, Box::new(MutTy {
            ty: target,
            mutability: mutability,
        })))
    ));

    named!(ty_bare_fn -> Ty, do_parse!(
        keyword!("fn") >>
        lifetimes: opt_vec!(delimited!(
            punct!("<"),
            terminated_list!(punct!(","), lifetime_def),
            punct!(">")
        )) >>
        punct!("(") >>
        inputs: terminated_list!(punct!(","), fn_arg) >>
        punct!(")") >>
        output: option!(preceded!(
            punct!("->"),
            ty
        )) >>
        (Ty::BareFn(Box::new(BareFnTy {
            lifetimes: lifetimes,
            inputs: inputs,
            output: match output {
                Some(ty) => FunctionRetTy::Ty(ty),
                None => FunctionRetTy::Default,
            },
        })))
    ));

    named!(ty_never -> Ty, map!(punct!("!"), |_| Ty::Never));

    named!(ty_tup -> Ty, do_parse!(
        punct!("(") >>
        elems: terminated_list!(punct!(","), ty) >>
        punct!(")") >>
        (Ty::Tup(elems))
    ));

    named!(ty_path -> Ty, do_parse!(
        qpath: qpath >>
        parenthesized: cond!(
            qpath.1.segments.last().unwrap().parameters == PathParameters::none(),
            option!(parenthesized_parameter_data)
        ) >>
        bounds: many0!(preceded!(punct!("+"), ty_param_bound)) >>
        ({
            let (qself, mut path) = qpath;
            if let Some(Some(parenthesized)) = parenthesized {
                path.segments.last_mut().unwrap().parameters = parenthesized;
            }
            let path = Ty::Path(qself, path);
            if bounds.is_empty() {
                path
            } else {
                Ty::ObjectSum(Box::new(path), bounds)
            }
        })
    ));

    named!(parenthesized_parameter_data -> PathParameters, do_parse!(
        punct!("(") >>
        inputs: terminated_list!(punct!(","), ty) >>
        punct!(")") >>
        output: option!(preceded!(
            punct!("->"),
            ty
        )) >>
        (PathParameters::Parenthesized(
            ParenthesizedParameterData {
                inputs: inputs,
                output: output,
            },
        ))
    ));

    named!(pub qpath -> (Option<QSelf>, Path), alt!(
        map!(path, |p| (None, p))
        |
        do_parse!(
            punct!("<") >>
            this: map!(ty, Box::new) >>
            path: option!(preceded!(
                keyword!("as"),
                path
            )) >>
            punct!(">") >>
            punct!("::") >>
            rest: separated_nonempty_list!(punct!("::"), path_segment) >>
            ({
                match path {
                    Some(mut path) => {
                        let pos = path.segments.len();
                        path.segments.extend(rest);
                        (Some(QSelf { ty: this, position: pos }), path)
                    }
                    None => {
                        (Some(QSelf { ty: this, position: 0 }), Path {
                            global: false,
                            segments: rest,
                        })
                    }
                }
            })
        )
    ));

    named!(ty_poly_trait_ref -> Ty, do_parse!(
        keyword!("for") >>
        punct!("<") >>
        lifetimes: terminated_list!(punct!(","), lifetime_def) >>
        punct!(">") >>
        trait_ref: path >>
        (Ty::PolyTraitRef(vec![
            TyParamBound::Trait(
                PolyTraitRef {
                    bound_lifetimes: lifetimes,
                    trait_ref: trait_ref,
                },
                TraitBoundModifier::None,
            ),
        ]))
    ));

    named!(ty_impl_trait -> Ty, do_parse!(
        keyword!("impl") >>
        elem: separated_nonempty_list!(punct!("+"), ty_param_bound) >>
        (Ty::ImplTrait(elem))
    ));

    named!(ty_paren -> Ty, do_parse!(
        punct!("(") >>
        elem: ty >>
        punct!(")") >>
        (Ty::Paren(Box::new(elem)))
    ));

    named!(pub mutability -> Mutability, alt!(
        keyword!("mut") => { |_| Mutability::Mutable }
        |
        epsilon!() => { |_| Mutability::Immutable }
    ));

    named!(pub path -> Path, do_parse!(
        global: option!(punct!("::")) >>
        segments: separated_nonempty_list!(punct!("::"), path_segment) >>
        (Path {
            global: global.is_some(),
            segments: segments,
        })
    ));

    named!(path_segment -> PathSegment, alt!(
        do_parse!(
            id: option!(ident) >>
            punct!("<") >>
            lifetimes: separated_list!(punct!(","), lifetime) >>
            types: opt_vec!(preceded!(
                cond!(!lifetimes.is_empty(), punct!(",")),
                separated_nonempty_list!(
                    punct!(","),
                    terminated!(ty, not!(peek!(punct!("="))))
                )
            )) >>
            bindings: opt_vec!(preceded!(
                cond!(!lifetimes.is_empty() || !types.is_empty(), punct!(",")),
                separated_nonempty_list!(punct!(","), type_binding)
            )) >>
            punct!(">") >>
            (PathSegment {
                ident: id.unwrap_or_else(|| "".into()),
                parameters: PathParameters::AngleBracketed(
                    AngleBracketedParameterData {
                        lifetimes: lifetimes,
                        types: types,
                        bindings: bindings,
                    }
                ),
            })
        )
        |
        map!(ident, Into::into)
    ));

    named!(type_binding -> TypeBinding, do_parse!(
        id: ident >>
        punct!("=") >>
        ty: ty >>
        (TypeBinding {
            ident: id,
            ty: ty,
        })
    ));

    named!(pub poly_trait_ref -> PolyTraitRef, do_parse!(
        bound_lifetimes: bound_lifetimes >>
        trait_ref: path >>
        parenthesized: cond!(
            trait_ref.segments.last().unwrap().parameters == PathParameters::none(),
            option!(parenthesized_parameter_data)
        ) >>
        ({
            let mut trait_ref = trait_ref;
            if let Some(Some(parenthesized)) = parenthesized {
                trait_ref.segments.last_mut().unwrap().parameters = parenthesized;
            }
            PolyTraitRef {
                bound_lifetimes: bound_lifetimes,
                trait_ref: trait_ref,
            }
        })
    ));

    named!(pub fn_arg -> BareFnArg, do_parse!(
        name: option!(terminated!(ident, punct!(":"))) >>
        ty: ty >>
        (BareFnArg {
            name: name,
            ty: ty,
        })
    ));
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use quote::{Tokens, ToTokens};

    impl ToTokens for Ty {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                Ty::Slice(ref inner) => {
                    tokens.append("[");
                    inner.to_tokens(tokens);
                    tokens.append("]");
                }
                Ty::Array(ref inner, ref len) => {
                    tokens.append("[");
                    inner.to_tokens(tokens);
                    tokens.append(";");
                    len.to_tokens(tokens);
                    tokens.append("]");
                }
                Ty::Ptr(ref target) => {
                    tokens.append("*");
                    match target.mutability {
                        Mutability::Mutable => tokens.append("mut"),
                        Mutability::Immutable => tokens.append("const"),
                    }
                    target.ty.to_tokens(tokens);
                }
                Ty::Rptr(ref lifetime, ref target) => {
                    tokens.append("&");
                    lifetime.to_tokens(tokens);
                    target.mutability.to_tokens(tokens);
                    target.ty.to_tokens(tokens);
                }
                Ty::BareFn(ref func) => {
                    func.to_tokens(tokens);
                }
                Ty::Never => {
                    tokens.append("!");
                }
                Ty::Tup(ref elems) => {
                    tokens.append("(");
                    tokens.append_separated(elems, ",");
                    if elems.len() == 1 {
                        tokens.append(",");
                    }
                    tokens.append(")");
                }
                Ty::Path(None, ref path) => {
                    path.to_tokens(tokens);
                }
                Ty::Path(Some(ref qself), ref path) => {
                    tokens.append("<");
                    qself.ty.to_tokens(tokens);
                    if qself.position > 0 {
                        tokens.append("as");
                        for (i, segment) in path.segments
                            .iter()
                            .take(qself.position)
                            .enumerate() {
                            if i > 0 || path.global {
                                tokens.append("::");
                            }
                            segment.to_tokens(tokens);
                        }
                    }
                    tokens.append(">");
                    for segment in path.segments.iter().skip(qself.position) {
                        tokens.append("::");
                        segment.to_tokens(tokens);
                    }
                }
                Ty::ObjectSum(ref ty, ref bounds) => {
                    ty.to_tokens(tokens);
                    for bound in bounds {
                        tokens.append("+");
                        bound.to_tokens(tokens);
                    }
                }
                Ty::PolyTraitRef(ref bounds) => {
                    tokens.append_separated(bounds, "+");
                }
                Ty::ImplTrait(ref bounds) => {
                    tokens.append("impl");
                    tokens.append_separated(bounds, "+");
                }
                Ty::Paren(ref inner) => {
                    tokens.append("(");
                    inner.to_tokens(tokens);
                    tokens.append(")");
                }
                Ty::Infer => {
                    tokens.append("_");
                }
            }
        }
    }

    impl ToTokens for Mutability {
        fn to_tokens(&self, tokens: &mut Tokens) {
            if let Mutability::Mutable = *self {
                tokens.append("mut");
            }
        }
    }

    impl ToTokens for Path {
        fn to_tokens(&self, tokens: &mut Tokens) {
            for (i, segment) in self.segments.iter().enumerate() {
                if i > 0 || self.global {
                    tokens.append("::");
                }
                segment.to_tokens(tokens);
            }
        }
    }

    impl ToTokens for PathSegment {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.ident.to_tokens(tokens);
            self.parameters.to_tokens(tokens);
        }
    }

    impl ToTokens for PathParameters {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                PathParameters::AngleBracketed(ref parameters) => {
                    parameters.to_tokens(tokens);
                }
                PathParameters::Parenthesized(ref parameters) => {
                    parameters.to_tokens(tokens);
                }
            }
        }
    }

    impl ToTokens for AngleBracketedParameterData {
        fn to_tokens(&self, tokens: &mut Tokens) {
            let has_lifetimes = !self.lifetimes.is_empty();
            let has_types = !self.types.is_empty();
            let has_bindings = !self.bindings.is_empty();
            if !has_lifetimes && !has_types && !has_bindings {
                return;
            }

            tokens.append("<");

            let mut first = true;
            for lifetime in &self.lifetimes {
                if !first {
                    tokens.append(",");
                }
                lifetime.to_tokens(tokens);
                first = false;
            }
            for ty in &self.types {
                if !first {
                    tokens.append(",");
                }
                ty.to_tokens(tokens);
                first = false;
            }
            for binding in &self.bindings {
                if !first {
                    tokens.append(",");
                }
                binding.to_tokens(tokens);
                first = false;
            }

            tokens.append(">");
        }
    }

    impl ToTokens for TypeBinding {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.ident.to_tokens(tokens);
            tokens.append("=");
            self.ty.to_tokens(tokens);
        }
    }

    impl ToTokens for ParenthesizedParameterData {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append("(");
            tokens.append_separated(&self.inputs, ",");
            tokens.append(")");
            if let Some(ref output) = self.output {
                tokens.append("->");
                output.to_tokens(tokens);
            }
        }
    }

    impl ToTokens for PolyTraitRef {
        fn to_tokens(&self, tokens: &mut Tokens) {
            if !self.bound_lifetimes.is_empty() {
                tokens.append("for");
                tokens.append("<");
                tokens.append_separated(&self.bound_lifetimes, ",");
                tokens.append(">");
            }
            self.trait_ref.to_tokens(tokens);
        }
    }

    impl ToTokens for BareFnTy {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append("fn");
            if !self.lifetimes.is_empty() {
                tokens.append("<");
                tokens.append_separated(&self.lifetimes, ",");
                tokens.append(">");
            }
            tokens.append("(");
            tokens.append_separated(&self.inputs, ",");
            tokens.append(")");
            if let FunctionRetTy::Ty(ref ty) = self.output {
                tokens.append("->");
                ty.to_tokens(tokens);
            }
        }
    }

    impl ToTokens for BareFnArg {
        fn to_tokens(&self, tokens: &mut Tokens) {
            if let Some(ref name) = self.name {
                name.to_tokens(tokens);
                tokens.append(":");
            }
            self.ty.to_tokens(tokens);
        }
    }
}
