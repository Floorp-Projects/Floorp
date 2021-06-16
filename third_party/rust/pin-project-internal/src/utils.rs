use std::mem;

use proc_macro2::{Group, TokenStream, TokenTree};
use quote::{format_ident, quote_spanned};
use syn::{
    parse::{ParseBuffer, ParseStream},
    punctuated::Punctuated,
    token::{self, Comma},
    visit_mut::{self, VisitMut},
    *,
};

pub(crate) const DEFAULT_LIFETIME_NAME: &str = "'pin";
pub(crate) const CURRENT_PRIVATE_MODULE: &str = "__private";

pub(crate) type Variants = Punctuated<Variant, token::Comma>;

pub(crate) use Mutability::{Immutable, Mutable};

macro_rules! error {
    ($span:expr, $msg:expr) => {
        syn::Error::new_spanned(&$span, $msg)
    };
    ($span:expr, $($tt:tt)*) => {
        error!($span, format!($($tt)*))
    };
}

#[derive(Clone, Copy, Eq, PartialEq)]
pub(crate) enum Mutability {
    Mutable,
    Immutable,
}

/// Creates the ident of projected type from the ident of the original type.
pub(crate) fn proj_ident(ident: &Ident, mutability: Mutability) -> Ident {
    if mutability == Mutable {
        format_ident!("__{}Projection", ident)
    } else {
        format_ident!("__{}ProjectionRef", ident)
    }
}

/// Determines the lifetime names. Ensure it doesn't overlap with any existing lifetime names.
pub(crate) fn determine_lifetime_name(
    lifetime_name: &mut String,
    generics: &Punctuated<GenericParam, Comma>,
) {
    let existing_lifetimes: Vec<String> = generics
        .iter()
        .filter_map(|param| {
            if let GenericParam::Lifetime(LifetimeDef { lifetime, .. }) = param {
                Some(lifetime.to_string())
            } else {
                None
            }
        })
        .collect();
    while existing_lifetimes.iter().any(|name| name.starts_with(&**lifetime_name)) {
        lifetime_name.push('_');
    }
}

/// Like `insert_lifetime`, but also generates a bound of the form
/// `OriginalType<A, B>: 'lifetime`. Used when generating the definition
/// of a projection type
pub(crate) fn insert_lifetime_and_bound(
    generics: &mut Generics,
    lifetime: Lifetime,
    orig_generics: &Generics,
    orig_ident: Ident,
) -> WherePredicate {
    insert_lifetime(generics, lifetime.clone());

    let orig_type: syn::Type = syn::parse_quote!(#orig_ident #orig_generics);
    let mut punct = Punctuated::new();
    punct.push(TypeParamBound::Lifetime(lifetime));

    WherePredicate::Type(PredicateType {
        lifetimes: None,
        bounded_ty: orig_type,
        colon_token: syn::token::Colon::default(),
        bounds: punct,
    })
}

/// Inserts a `lifetime` at position `0` of `generics.params`.
pub(crate) fn insert_lifetime(generics: &mut Generics, lifetime: Lifetime) {
    if generics.lt_token.is_none() {
        generics.lt_token = Some(token::Lt::default())
    }
    if generics.gt_token.is_none() {
        generics.gt_token = Some(token::Gt::default())
    }

    generics.params.insert(
        0,
        GenericParam::Lifetime(LifetimeDef {
            attrs: Vec::new(),
            lifetime,
            colon_token: None,
            bounds: Punctuated::new(),
        }),
    );
}

/// Determines the visibility of the projected type and projection method.
pub(crate) fn determine_visibility(vis: &Visibility) -> Visibility {
    if let Visibility::Public(token) = vis {
        syn::parse2(quote_spanned! { token.pub_token.span =>
            pub(crate)
        })
        .unwrap()
    } else {
        vis.clone()
    }
}

/// Check if `tokens` is an empty `TokenStream`.
/// This is almost equivalent to `syn::parse2::<Nothing>()`,
/// but produces a better error message and does not require ownership of `tokens`.
pub(crate) fn parse_as_empty(tokens: &TokenStream) -> Result<()> {
    if tokens.is_empty() { Ok(()) } else { Err(error!(tokens, "unexpected token: {}", tokens)) }
}

// =================================================================================================
// extension traits

pub(crate) trait SliceExt {
    fn position_exact(&self, ident: &str) -> Result<Option<usize>>;
    fn find(&self, ident: &str) -> Option<&Attribute>;
    fn find_exact(&self, ident: &str) -> Result<Option<&Attribute>>;
}

pub(crate) trait VecExt {
    fn find_remove(&mut self, ident: &str) -> Result<Option<Attribute>>;
}

impl SliceExt for [Attribute] {
    fn position_exact(&self, ident: &str) -> Result<Option<usize>> {
        self.iter()
            .try_fold((0, None), |(i, mut prev), attr| {
                if attr.path.is_ident(ident) {
                    if prev.is_some() {
                        return Err(error!(attr, "duplicate #[{}] attribute", ident));
                    }
                    parse_as_empty(&attr.tokens)?;
                    prev = Some(i);
                }
                Ok((i + 1, prev))
            })
            .map(|(_, pos)| pos)
    }

    fn find(&self, ident: &str) -> Option<&Attribute> {
        self.iter().position(|attr| attr.path.is_ident(ident)).and_then(|i| self.get(i))
    }

    fn find_exact(&self, ident: &str) -> Result<Option<&Attribute>> {
        self.position_exact(ident).map(|pos| pos.and_then(|i| self.get(i)))
    }
}

impl VecExt for Vec<Attribute> {
    fn find_remove(&mut self, ident: &str) -> Result<Option<Attribute>> {
        self.position_exact(ident).map(|pos| pos.map(|i| self.remove(i)))
    }
}

pub(crate) trait ParseBufferExt<'a> {
    fn parenthesized(self) -> Result<ParseBuffer<'a>>;
}

impl<'a> ParseBufferExt<'a> for ParseStream<'a> {
    fn parenthesized(self) -> Result<ParseBuffer<'a>> {
        let content;
        let _: token::Paren = syn::parenthesized!(content in self);
        Ok(content)
    }
}

impl<'a> ParseBufferExt<'a> for ParseBuffer<'a> {
    fn parenthesized(self) -> Result<ParseBuffer<'a>> {
        let content;
        let _: token::Paren = syn::parenthesized!(content in self);
        Ok(content)
    }
}

// =================================================================================================
// visitors

// Replace `self`/`Self` with `__self`/`self_ty`.
// Based on https://github.com/dtolnay/async-trait/blob/0.1.22/src/receiver.rs

pub(crate) struct ReplaceReceiver<'a> {
    self_ty: &'a Type,
}

impl<'a> ReplaceReceiver<'a> {
    pub(crate) fn new(self_ty: &'a Type) -> Self {
        Self { self_ty }
    }

    fn self_to_qself(&mut self, qself: &mut Option<QSelf>, path: &mut Path) {
        if path.leading_colon.is_some() {
            return;
        }

        let first = &path.segments[0];
        if first.ident != "Self" || !first.arguments.is_empty() {
            return;
        }

        if path.segments.len() == 1 {
            self.self_to_expr_path(path);
            return;
        }

        *qself = Some(QSelf {
            lt_token: token::Lt::default(),
            ty: Box::new(self.self_ty.clone()),
            position: 0,
            as_token: None,
            gt_token: token::Gt::default(),
        });

        match path.segments.pairs().next().unwrap().punct() {
            Some(&&colon) => path.leading_colon = Some(colon),
            None => return,
        }

        let segments = mem::replace(&mut path.segments, Punctuated::new());
        path.segments = segments.into_pairs().skip(1).collect();
    }

    fn self_to_expr_path(&self, path: &mut Path) {
        if let Type::Path(self_ty) = &self.self_ty {
            *path = self_ty.path.clone();
            for segment in &mut path.segments {
                if let PathArguments::AngleBracketed(bracketed) = &mut segment.arguments {
                    if bracketed.colon2_token.is_none() && !bracketed.args.is_empty() {
                        bracketed.colon2_token = Some(token::Colon2::default());
                    }
                }
            }
        } else {
            let span = path.segments[0].ident.span();
            let msg = "Self type of this impl is unsupported in expression position";
            let error = Error::new(span, msg).to_compile_error();
            *path = parse_quote!(::core::marker::PhantomData::<#error>);
        }
    }
}

impl VisitMut for ReplaceReceiver<'_> {
    // `Self` -> `Receiver`
    fn visit_type_mut(&mut self, ty: &mut Type) {
        if let Type::Path(node) = ty {
            if node.qself.is_none() && node.path.is_ident("Self") {
                *ty = self.self_ty.clone();
            } else {
                self.visit_type_path_mut(node);
            }
        } else {
            visit_mut::visit_type_mut(self, ty);
        }
    }

    // `Self::Assoc` -> `<Receiver>::Assoc`
    fn visit_type_path_mut(&mut self, ty: &mut TypePath) {
        if ty.qself.is_none() {
            self.self_to_qself(&mut ty.qself, &mut ty.path);
        }
        visit_mut::visit_type_path_mut(self, ty);
    }

    // `Self::method` -> `<Receiver>::method`
    fn visit_expr_path_mut(&mut self, expr: &mut ExprPath) {
        if expr.qself.is_none() {
            prepend_underscore_to_self(&mut expr.path.segments[0].ident);
            self.self_to_qself(&mut expr.qself, &mut expr.path);
        }
        visit_mut::visit_expr_path_mut(self, expr);
    }

    fn visit_expr_struct_mut(&mut self, expr: &mut ExprStruct) {
        if expr.path.is_ident("Self") {
            self.self_to_expr_path(&mut expr.path);
        }
        visit_mut::visit_expr_struct_mut(self, expr);
    }

    fn visit_macro_mut(&mut self, node: &mut Macro) {
        // We can't tell in general whether `self` inside a macro invocation
        // refers to the self in the argument list or a different self
        // introduced within the macro. Heuristic: if the macro input contains
        // `fn`, then `self` is more likely to refer to something other than the
        // outer function's self argument.
        if !contains_fn(node.tokens.clone()) {
            node.tokens = fold_token_stream(node.tokens.clone());
        }
    }

    fn visit_item_mut(&mut self, _: &mut Item) {
        // Do not recurse into nested items.
    }
}

fn contains_fn(tokens: TokenStream) -> bool {
    tokens.into_iter().any(|tt| match tt {
        TokenTree::Ident(ident) => ident == "fn",
        TokenTree::Group(group) => contains_fn(group.stream()),
        _ => false,
    })
}

fn fold_token_stream(tokens: TokenStream) -> TokenStream {
    tokens
        .into_iter()
        .map(|tt| match tt {
            TokenTree::Ident(mut ident) => {
                prepend_underscore_to_self(&mut ident);
                TokenTree::Ident(ident)
            }
            TokenTree::Group(group) => {
                let content = fold_token_stream(group.stream());
                TokenTree::Group(Group::new(group.delimiter(), content))
            }
            other => other,
        })
        .collect()
}

pub(crate) fn prepend_underscore_to_self(ident: &mut Ident) {
    if ident == "self" {
        *ident = Ident::new("__self", ident.span());
    }
}
