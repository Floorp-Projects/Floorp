// Copyright 2018 Syn Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use punctuated::Punctuated;
use super::*;

ast_struct! {
    /// A path at which a named item is exported: `std::collections::HashMap`.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    pub struct Path {
        pub leading_colon: Option<Token![::]>,
        pub segments: Punctuated<PathSegment, Token![::]>,
    }
}

impl Path {
    pub fn global(&self) -> bool {
        self.leading_colon.is_some()
    }
}

/// A helper for printing a self-type qualified path as tokens.
///
/// ```rust
/// extern crate syn;
/// extern crate quote;
///
/// use syn::{QSelf, Path, PathTokens};
/// use quote::{Tokens, ToTokens};
///
/// struct MyNode {
///     qself: Option<QSelf>,
///     path: Path,
/// }
///
/// impl ToTokens for MyNode {
///     fn to_tokens(&self, tokens: &mut Tokens) {
///         PathTokens(&self.qself, &self.path).to_tokens(tokens);
///     }
/// }
/// #
/// # fn main() {}
/// ```
///
/// *This type is available if Syn is built with the `"derive"` or `"full"`
/// feature and the `"printing"` feature.*
#[cfg(feature = "printing")]
#[cfg_attr(feature = "extra-traits", derive(Debug, Eq, PartialEq, Hash))]
#[cfg_attr(feature = "clone-impls", derive(Clone))]
pub struct PathTokens<'a>(pub &'a Option<QSelf>, pub &'a Path);

impl<T> From<T> for Path
where
    T: Into<PathSegment>,
{
    fn from(segment: T) -> Self {
        let mut path = Path {
            leading_colon: None,
            segments: Punctuated::new(),
        };
        path.segments.push_value(segment.into());
        path
    }
}

ast_struct! {
    /// A segment of a path together with any path arguments on that segment.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    pub struct PathSegment {
        pub ident: Ident,
        pub arguments: PathArguments,
    }
}

impl<T> From<T> for PathSegment
where
    T: Into<Ident>,
{
    fn from(ident: T) -> Self {
        PathSegment {
            ident: ident.into(),
            arguments: PathArguments::None,
        }
    }
}

ast_enum! {
    /// Angle bracketed or parenthesized arguments of a path segment.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    ///
    /// ## Angle bracketed
    ///
    /// The `<'a, T>` in `std::slice::iter<'a, T>`.
    ///
    /// ## Parenthesized
    ///
    /// The `(A, B) -> C` in `Fn(A, B) -> C`.
    pub enum PathArguments {
        None,
        /// The `<'a, T>` in `std::slice::iter<'a, T>`.
        AngleBracketed(AngleBracketedGenericArguments),
        /// The `(A, B) -> C` in `Fn(A, B) -> C`.
        Parenthesized(ParenthesizedGenericArguments),
    }
}

impl Default for PathArguments {
    fn default() -> Self {
        PathArguments::None
    }
}

impl PathArguments {
    pub fn is_empty(&self) -> bool {
        match *self {
            PathArguments::None => true,
            PathArguments::AngleBracketed(ref bracketed) => bracketed.args.is_empty(),
            PathArguments::Parenthesized(_) => false,
        }
    }
}

ast_enum! {
    /// An individual generic argument, like `'a`, `T`, or `Item = T`.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    pub enum GenericArgument {
        /// A lifetime argument.
        Lifetime(Lifetime),
        /// A type argument.
        Type(Type),
        /// A binding (equality constraint) on an associated type: the `Item =
        /// u8` in `Iterator<Item = u8>`.
        Binding(Binding),
        /// A const expression. Must be inside of a block.
        ///
        /// NOTE: Identity expressions are represented as Type arguments, as
        /// they are indistinguishable syntactically.
        Const(Expr),
    }
}

ast_struct! {
    /// Angle bracketed arguments of a path segment: the `<K, V>` in `HashMap<K,
    /// V>`.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    pub struct AngleBracketedGenericArguments {
        pub colon2_token: Option<Token![::]>,
        pub lt_token: Token![<],
        pub args: Punctuated<GenericArgument, Token![,]>,
        pub gt_token: Token![>],
    }
}

ast_struct! {
    /// A binding (equality constraint) on an associated type: `Item = u8`.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    pub struct Binding {
        pub ident: Ident,
        pub eq_token: Token![=],
        pub ty: Type,
    }
}

ast_struct! {
    /// Arguments of a function path segment: the `(A, B) -> C` in `Fn(A,B) ->
    /// C`.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    pub struct ParenthesizedGenericArguments {
        pub paren_token: token::Paren,
        /// `(A, B)`
        pub inputs: Punctuated<Type, Token![,]>,
        /// `C`
        pub output: ReturnType,
    }
}

ast_struct! {
    /// The explicit Self type in a qualified path: the `T` in `<T as
    /// Display>::fmt`.
    ///
    /// The actual path, including the trait and the associated item, is stored
    /// separately. The `position` field represents the index of the associated
    /// item qualified with this Self type.
    ///
    /// ```text
    /// <Vec<T> as a::b::Trait>::AssociatedItem
    ///  ^~~~~~    ~~~~~~~~~~~~~~^
    ///  ty        position = 3
    ///
    /// <Vec<T>>::AssociatedItem
    ///  ^~~~~~   ^
    ///  ty       position = 0
    /// ```
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    pub struct QSelf {
        pub lt_token: Token![<],
        pub ty: Box<Type>,
        pub position: usize,
        pub as_token: Option<Token![as]>,
        pub gt_token: Token![>],
    }
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use synom::Synom;

    impl Synom for Path {
        named!(parse -> Self, do_parse!(
            colon: option!(punct!(::)) >>
            segments: call!(Punctuated::<PathSegment, Token![::]>::parse_separated_nonempty) >>
            cond_reduce!(segments.first().map_or(true, |seg| seg.value().ident != "dyn")) >>
            (Path {
                leading_colon: colon,
                segments: segments,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("path")
        }
    }

    #[cfg(not(feature = "full"))]
    impl Synom for GenericArgument {
        named!(parse -> Self, alt!(
            call!(ty_no_eq_after) => { GenericArgument::Type }
            |
            syn!(Lifetime) => { GenericArgument::Lifetime }
            |
            syn!(Binding) => { GenericArgument::Binding }
        ));
    }

    #[cfg(feature = "full")]
    impl Synom for GenericArgument {
        named!(parse -> Self, alt!(
            call!(ty_no_eq_after) => { GenericArgument::Type }
            |
            syn!(Lifetime) => { GenericArgument::Lifetime }
            |
            syn!(Binding) => { GenericArgument::Binding }
            |
            syn!(ExprLit) => { |l| GenericArgument::Const(Expr::Lit(l)) }
            |
            syn!(ExprBlock) => { |b| GenericArgument::Const(Expr::Block(b)) }
        ));

        fn description() -> Option<&'static str> {
            Some("generic argument")
        }
    }

    impl Synom for AngleBracketedGenericArguments {
        named!(parse -> Self, do_parse!(
            colon2: option!(punct!(::)) >>
            lt: punct!(<) >>
            args: call!(Punctuated::parse_terminated) >>
            gt: punct!(>) >>
            (AngleBracketedGenericArguments {
                colon2_token: colon2,
                lt_token: lt,
                args: args,
                gt_token: gt,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("angle bracketed generic arguments")
        }
    }

    impl Synom for ParenthesizedGenericArguments {
        named!(parse -> Self, do_parse!(
            data: parens!(Punctuated::parse_terminated) >>
            output: syn!(ReturnType) >>
            (ParenthesizedGenericArguments {
                paren_token: data.0,
                inputs: data.1,
                output: output,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("parenthesized generic arguments: `Foo(A, B, ..) -> T`")
        }
    }

    impl Synom for PathSegment {
        named!(parse -> Self, alt!(
            do_parse!(
                ident: syn!(Ident) >>
                arguments: syn!(AngleBracketedGenericArguments) >>
                (PathSegment {
                    ident: ident,
                    arguments: PathArguments::AngleBracketed(arguments),
                })
            )
            |
            mod_style_path_segment
        ));

        fn description() -> Option<&'static str> {
            Some("path segment")
        }
    }

    impl Synom for Binding {
        named!(parse -> Self, do_parse!(
            id: syn!(Ident) >>
            eq: punct!(=) >>
            ty: syn!(Type) >>
            (Binding {
                ident: id,
                eq_token: eq,
                ty: ty,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("associated type binding")
        }
    }

    impl Path {
        named!(pub parse_mod_style -> Self, do_parse!(
            colon: option!(punct!(::)) >>
            segments: call!(Punctuated::parse_separated_nonempty_with,
                            mod_style_path_segment) >>
            (Path {
                leading_colon: colon,
                segments: segments,
            })
        ));
    }

    named!(mod_style_path_segment -> PathSegment, alt!(
        syn!(Ident) => { Into::into }
        |
        keyword!(super) => { Into::into }
        |
        keyword!(self) => { Into::into }
        |
        keyword!(Self) => { Into::into }
        |
        keyword!(crate) => { Into::into }
    ));

    named!(pub qpath -> (Option<QSelf>, Path), alt!(
        map!(syn!(Path), |p| (None, p))
        |
        do_parse!(
            lt: punct!(<) >>
            this: syn!(Type) >>
            path: option!(tuple!(keyword!(as), syn!(Path))) >>
            gt: punct!(>) >>
            colon2: punct!(::) >>
            rest: call!(Punctuated::parse_separated_nonempty) >>
            ({
                let (pos, as_, path) = match path {
                    Some((as_, mut path)) => {
                        let pos = path.segments.len();
                        path.segments.push_punct(colon2);
                        path.segments.extend(rest.into_pairs());
                        (pos, Some(as_), path)
                    }
                    None => {
                        (0, None, Path {
                            leading_colon: Some(colon2),
                            segments: rest,
                        })
                    }
                };
                (Some(QSelf {
                    lt_token: lt,
                    ty: Box::new(this),
                    position: pos,
                    as_token: as_,
                    gt_token: gt,
                }), path)
            })
        )
        |
        map!(keyword!(self), |s| (None, s.into()))
    ));

    named!(pub ty_no_eq_after -> Type, do_parse!(
        ty: syn!(Type) >>
        not!(punct!(=)) >>
        (ty)
    ));
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use quote::{ToTokens, Tokens};

    impl ToTokens for Path {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.leading_colon.to_tokens(tokens);
            self.segments.to_tokens(tokens);
        }
    }

    impl ToTokens for PathSegment {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.ident.to_tokens(tokens);
            self.arguments.to_tokens(tokens);
        }
    }

    impl ToTokens for PathArguments {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                PathArguments::None => {}
                PathArguments::AngleBracketed(ref arguments) => {
                    arguments.to_tokens(tokens);
                }
                PathArguments::Parenthesized(ref arguments) => {
                    arguments.to_tokens(tokens);
                }
            }
        }
    }

    impl ToTokens for GenericArgument {
        #[cfg_attr(feature = "cargo-clippy", allow(match_same_arms))]
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                GenericArgument::Lifetime(ref lt) => lt.to_tokens(tokens),
                GenericArgument::Type(ref ty) => ty.to_tokens(tokens),
                GenericArgument::Binding(ref tb) => tb.to_tokens(tokens),
                GenericArgument::Const(ref e) => match *e {
                    Expr::Lit(_) => e.to_tokens(tokens),

                    // NOTE: We should probably support parsing blocks with only
                    // expressions in them without the full feature for const
                    // generics.
                    #[cfg(feature = "full")]
                    Expr::Block(_) => e.to_tokens(tokens),

                    // ERROR CORRECTION: Add braces to make sure that the
                    // generated code is valid.
                    _ => token::Brace::default().surround(tokens, |tokens| {
                        e.to_tokens(tokens);
                    }),
                },
            }
        }
    }

    impl ToTokens for AngleBracketedGenericArguments {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.colon2_token.to_tokens(tokens);
            self.lt_token.to_tokens(tokens);

            // Print lifetimes before types and consts, all before bindings,
            // regardless of their order in self.args.
            //
            // TODO: ordering rules for const arguments vs type arguments have
            // not been settled yet. https://github.com/rust-lang/rust/issues/44580
            let mut trailing_or_empty = true;
            for param in self.args.pairs() {
                if let GenericArgument::Lifetime(_) = **param.value() {
                    param.to_tokens(tokens);
                    trailing_or_empty = param.punct().is_some();
                }
            }
            for param in self.args.pairs() {
                match **param.value() {
                    GenericArgument::Type(_) | GenericArgument::Const(_) => {
                        if !trailing_or_empty {
                            <Token![,]>::default().to_tokens(tokens);
                        }
                        param.to_tokens(tokens);
                        trailing_or_empty = param.punct().is_some();
                    }
                    GenericArgument::Lifetime(_) | GenericArgument::Binding(_) => {}
                }
            }
            for param in self.args.pairs() {
                if let GenericArgument::Binding(_) = **param.value() {
                    if !trailing_or_empty {
                        <Token![,]>::default().to_tokens(tokens);
                        trailing_or_empty = true;
                    }
                    param.to_tokens(tokens);
                }
            }

            self.gt_token.to_tokens(tokens);
        }
    }

    impl ToTokens for Binding {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.ident.to_tokens(tokens);
            self.eq_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
        }
    }

    impl ToTokens for ParenthesizedGenericArguments {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.paren_token.surround(tokens, |tokens| {
                self.inputs.to_tokens(tokens);
            });
            self.output.to_tokens(tokens);
        }
    }

    impl<'a> ToTokens for PathTokens<'a> {
        fn to_tokens(&self, tokens: &mut Tokens) {
            let qself = match *self.0 {
                Some(ref qself) => qself,
                None => return self.1.to_tokens(tokens),
            };
            qself.lt_token.to_tokens(tokens);
            qself.ty.to_tokens(tokens);

            // XXX: Gross.
            let pos = if qself.position > 0 && qself.position >= self.1.segments.len() {
                self.1.segments.len() - 1
            } else {
                qself.position
            };
            let mut segments = self.1.segments.pairs();
            if pos > 0 {
                TokensOrDefault(&qself.as_token).to_tokens(tokens);
                self.1.leading_colon.to_tokens(tokens);
                for (i, segment) in segments.by_ref().take(pos).enumerate() {
                    if i + 1 == pos {
                        segment.value().to_tokens(tokens);
                        qself.gt_token.to_tokens(tokens);
                        segment.punct().to_tokens(tokens);
                    } else {
                        segment.to_tokens(tokens);
                    }
                }
            } else {
                qself.gt_token.to_tokens(tokens);
                self.1.leading_colon.to_tokens(tokens);
            }
            for segment in segments {
                segment.to_tokens(tokens);
            }
        }
    }
}
