//! Implementation of a [`From`] derive macro.

use std::iter;

use proc_macro2::{Span, TokenStream};
use quote::{format_ident, quote, ToTokens as _, TokenStreamExt as _};
use syn::{
    parse::{discouraged::Speculative as _, Parse, ParseStream},
    parse_quote,
    punctuated::Punctuated,
    spanned::Spanned as _,
    token, Ident,
};

use crate::{
    parsing::Type,
    utils::{polyfill, Either},
};

/// Expands a [`From`] derive macro.
pub fn expand(input: &syn::DeriveInput, _: &'static str) -> syn::Result<TokenStream> {
    match &input.data {
        syn::Data::Struct(data) => Expansion {
            attrs: StructAttribute::parse_attrs(&input.attrs, &data.fields)?
                .map(Into::into)
                .as_ref(),
            ident: &input.ident,
            variant: None,
            fields: &data.fields,
            generics: &input.generics,
            has_explicit_from: false,
        }
        .expand(),
        syn::Data::Enum(data) => {
            let mut has_explicit_from = false;
            let attrs = data
                .variants
                .iter()
                .map(|variant| {
                    let attrs =
                        VariantAttribute::parse_attrs(&variant.attrs, &variant.fields)?;
                    if matches!(
                        attrs,
                        Some(
                            VariantAttribute::From
                                | VariantAttribute::Types(_)
                                | VariantAttribute::Forward
                        ),
                    ) {
                        has_explicit_from = true;
                    }
                    Ok(attrs)
                })
                .collect::<syn::Result<Vec<_>>>()?;

            data.variants
                .iter()
                .zip(&attrs)
                .map(|(variant, attrs)| {
                    Expansion {
                        attrs: attrs.as_ref(),
                        ident: &input.ident,
                        variant: Some(&variant.ident),
                        fields: &variant.fields,
                        generics: &input.generics,
                        has_explicit_from,
                    }
                    .expand()
                })
                .collect()
        }
        syn::Data::Union(data) => Err(syn::Error::new(
            data.union_token.span(),
            "`From` cannot be derived for unions",
        )),
    }
}

/// Representation of a [`From`] derive macro struct container attribute.
///
/// ```rust,ignore
/// #[from(<types>)]
/// #[from(forward)]
/// ```
enum StructAttribute {
    /// [`Type`]s to derive [`From`].
    Types(Punctuated<Type, token::Comma>),

    /// Forward [`From`] implementation.
    Forward,
}

impl StructAttribute {
    /// Parses a [`StructAttribute`] from the provided [`syn::Attribute`]s.
    fn parse_attrs(
        attrs: impl AsRef<[syn::Attribute]>,
        fields: &syn::Fields,
    ) -> syn::Result<Option<Self>> {
        Ok(attrs
            .as_ref()
            .iter()
            .filter(|attr| attr.path().is_ident("from"))
            .try_fold(None, |attrs, attr| {
                let field_attr = attr.parse_args_with(|stream: ParseStream<'_>| {
                    Self::parse(stream, fields)
                })?;
                match (attrs, field_attr) {
                    (
                        Some((path, StructAttribute::Types(mut tys))),
                        StructAttribute::Types(more),
                    ) => {
                        tys.extend(more);
                        Ok(Some((path, StructAttribute::Types(tys))))
                    }
                    (None, field_attr) => Ok(Some((attr.path(), field_attr))),
                    _ => Err(syn::Error::new(
                        attr.path().span(),
                        "only single `#[from(...)]` attribute is allowed here",
                    )),
                }
            })?
            .map(|(_, attr)| attr))
    }

    /// Parses single [`StructAttribute`].
    fn parse(input: ParseStream<'_>, fields: &syn::Fields) -> syn::Result<Self> {
        let ahead = input.fork();
        match ahead.parse::<syn::Path>() {
            Ok(p) if p.is_ident("forward") => {
                input.advance_to(&ahead);
                Ok(Self::Forward)
            }
            Ok(p) if p.is_ident("types") => legacy_error(&ahead, input.span(), fields),
            _ => input
                .parse_terminated(Type::parse, token::Comma)
                .map(Self::Types),
        }
    }
}

/// Representation of a [`From`] derive macro enum variant attribute.
///
/// ```rust,ignore
/// #[from]
/// #[from(<types>)]
/// #[from(forward)]
/// #[from(skip)]
/// ```
enum VariantAttribute {
    /// Explicitly derive [`From`].
    From,

    /// [`Type`]s to derive [`From`].
    Types(Punctuated<Type, token::Comma>),

    /// Forward [`From`] implementation.
    Forward,

    /// Skip variant.
    Skip,
}

impl VariantAttribute {
    /// Parses a [`VariantAttribute`] from the provided [`syn::Attribute`]s.
    fn parse_attrs(
        attrs: impl AsRef<[syn::Attribute]>,
        fields: &syn::Fields,
    ) -> syn::Result<Option<Self>> {
        Ok(attrs
            .as_ref()
            .iter()
            .filter(|attr| attr.path().is_ident("from"))
            .try_fold(None, |mut attrs, attr| {
                let field_attr = Self::parse_attr(attr, fields)?;
                if let Some((path, _)) = attrs.replace((attr.path(), field_attr)) {
                    Err(syn::Error::new(
                        path.span(),
                        "only single `#[from(...)]` attribute is allowed here",
                    ))
                } else {
                    Ok(attrs)
                }
            })?
            .map(|(_, attr)| attr))
    }

    /// Parses a [`VariantAttribute`] from the single provided [`syn::Attribute`].
    fn parse_attr(attr: &syn::Attribute, fields: &syn::Fields) -> syn::Result<Self> {
        if matches!(attr.meta, syn::Meta::Path(_)) {
            return Ok(Self::From);
        }

        attr.parse_args_with(|input: ParseStream<'_>| {
            let ahead = input.fork();
            match ahead.parse::<syn::Path>() {
                Ok(p) if p.is_ident("forward") => {
                    input.advance_to(&ahead);
                    Ok(Self::Forward)
                }
                Ok(p) if p.is_ident("skip") || p.is_ident("ignore") => {
                    input.advance_to(&ahead);
                    Ok(Self::Skip)
                }
                Ok(p) if p.is_ident("types") => {
                    legacy_error(&ahead, input.span(), fields)
                }
                _ => input
                    .parse_terminated(Type::parse, token::Comma)
                    .map(Self::Types),
            }
        })
    }
}

impl From<StructAttribute> for VariantAttribute {
    fn from(value: StructAttribute) -> Self {
        match value {
            StructAttribute::Types(tys) => Self::Types(tys),
            StructAttribute::Forward => Self::Forward,
        }
    }
}

/// Expansion of a macro for generating [`From`] implementation of a struct or
/// enum.
struct Expansion<'a> {
    /// [`From`] attributes.
    ///
    /// As a [`VariantAttribute`] is superset of a [`StructAttribute`], we use
    /// it for both derives.
    attrs: Option<&'a VariantAttribute>,

    /// Struct or enum [`Ident`].
    ident: &'a Ident,

    /// Variant [`Ident`] in case of enum expansion.
    variant: Option<&'a Ident>,

    /// Struct or variant [`syn::Fields`].
    fields: &'a syn::Fields,

    /// Struct or enum [`syn::Generics`].
    generics: &'a syn::Generics,

    /// Indicator whether one of the enum variants has
    /// [`VariantAttribute::From`], [`VariantAttribute::Types`] or
    /// [`VariantAttribute::Forward`].
    ///
    /// Always [`false`] for structs.
    has_explicit_from: bool,
}

impl<'a> Expansion<'a> {
    /// Expands [`From`] implementations for a struct or an enum variant.
    fn expand(&self) -> syn::Result<TokenStream> {
        use crate::utils::FieldsExt as _;

        let ident = self.ident;
        let field_tys = self.fields.iter().map(|f| &f.ty).collect::<Vec<_>>();
        let (impl_gens, ty_gens, where_clause) = self.generics.split_for_impl();

        let skip_variant = self.has_explicit_from
            || (self.variant.is_some() && self.fields.is_empty());
        match (self.attrs, skip_variant) {
            (Some(VariantAttribute::Types(tys)), _) => {
                tys.iter().map(|ty| {
                    let variant = self.variant.iter();

                    let mut from_tys = self.fields.validate_type(ty)?;
                    let init = self.expand_fields(|ident, ty, index| {
                        let ident = ident.into_iter();
                        let index = index.into_iter();
                        let from_ty = from_tys.next().unwrap_or_else(|| unreachable!());
                        quote! {
                            #( #ident: )* <#ty as ::core::convert::From<#from_ty>>::from(
                                value #( .#index )*
                            ),
                        }
                    });

                    Ok(quote! {
                        #[automatically_derived]
                        impl #impl_gens ::core::convert::From<#ty>
                         for #ident #ty_gens #where_clause
                        {
                            #[inline]
                            fn from(value: #ty) -> Self {
                                #ident #( :: #variant )* #init
                            }
                        }
                    })
                })
                .collect()
            }
            (Some(VariantAttribute::From), _) | (None, false) => {
                let variant = self.variant.iter();
                let init = self.expand_fields(|ident, _, index| {
                    let ident = ident.into_iter();
                    let index = index.into_iter();
                    quote! { #( #ident: )* value #( . #index )*, }
                });

                Ok(quote! {
                    #[automatically_derived]
                    impl #impl_gens ::core::convert::From<(#( #field_tys ),*)>
                     for #ident #ty_gens #where_clause
                    {
                        #[inline]
                        fn from(value: (#( #field_tys ),*)) -> Self {
                            #ident #( :: #variant )* #init
                        }
                    }
                })
            }
            (Some(VariantAttribute::Forward), _) => {
                let mut i = 0;
                let mut gen_idents = Vec::with_capacity(self.fields.len());
                let init = self.expand_fields(|ident, ty, index| {
                    let ident = ident.into_iter();
                    let index = index.into_iter();
                    let gen_ident = format_ident!("__FromT{i}");
                    let out = quote! {
                        #( #ident: )* <#ty as ::core::convert::From<#gen_ident>>::from(
                            value #( .#index )*
                        ),
                    };
                    gen_idents.push(gen_ident);
                    i += 1;
                    out
                });

                let variant = self.variant.iter();
                let generics = {
                    let mut generics = self.generics.clone();
                    for (ty, ident) in field_tys.iter().zip(&gen_idents) {
                        generics.make_where_clause().predicates.push(
                            parse_quote! { #ty: ::core::convert::From<#ident> },
                        );
                        generics
                            .params
                            .push(syn::TypeParam::from(ident.clone()).into());
                    }
                    generics
                };
                let (impl_gens, _, where_clause) = generics.split_for_impl();

                Ok(quote! {
                    #[automatically_derived]
                    impl #impl_gens ::core::convert::From<(#( #gen_idents ),*)>
                     for #ident #ty_gens #where_clause
                    {
                        #[inline]
                        fn from(value: (#( #gen_idents ),*)) -> Self {
                            #ident #(:: #variant)* #init
                        }
                    }
                })
            }
            (Some(VariantAttribute::Skip), _) | (None, true) => {
                Ok(TokenStream::new())
            }
        }
    }

    /// Expands fields initialization wrapped into [`token::Brace`]s in case of
    /// [`syn::FieldsNamed`], or [`token::Paren`] in case of
    /// [`syn::FieldsUnnamed`].
    fn expand_fields(
        &self,
        mut wrap: impl FnMut(Option<&Ident>, &syn::Type, Option<syn::Index>) -> TokenStream,
    ) -> TokenStream {
        let surround = match self.fields {
            syn::Fields::Named(_) | syn::Fields::Unnamed(_) => {
                Some(|tokens| match self.fields {
                    syn::Fields::Named(named) => {
                        let mut out = TokenStream::new();
                        named
                            .brace_token
                            .surround(&mut out, |out| out.append_all(tokens));
                        out
                    }
                    syn::Fields::Unnamed(unnamed) => {
                        let mut out = TokenStream::new();
                        unnamed
                            .paren_token
                            .surround(&mut out, |out| out.append_all(tokens));
                        out
                    }
                    syn::Fields::Unit => unreachable!(),
                })
            }
            syn::Fields::Unit => None,
        };

        surround
            .map(|surround| {
                surround(if self.fields.len() == 1 {
                    let field = self
                        .fields
                        .iter()
                        .next()
                        .unwrap_or_else(|| unreachable!("self.fields.len() == 1"));
                    wrap(field.ident.as_ref(), &field.ty, None)
                } else {
                    self.fields
                        .iter()
                        .enumerate()
                        .map(|(i, field)| {
                            wrap(field.ident.as_ref(), &field.ty, Some(i.into()))
                        })
                        .collect()
                })
            })
            .unwrap_or_default()
    }
}

/// Constructs a [`syn::Error`] for legacy syntax: `#[from(types(i32, "&str"))]`.
fn legacy_error<T>(
    tokens: ParseStream<'_>,
    span: Span,
    fields: &syn::Fields,
) -> syn::Result<T> {
    let content;
    syn::parenthesized!(content in tokens);

    let types = content
        .parse_terminated(polyfill::NestedMeta::parse, token::Comma)?
        .into_iter()
        .map(|meta| {
            let value = match meta {
                polyfill::NestedMeta::Meta(meta) => {
                    meta.into_token_stream().to_string()
                }
                polyfill::NestedMeta::Lit(syn::Lit::Str(str)) => str.value(),
                polyfill::NestedMeta::Lit(_) => unreachable!(),
            };
            if fields.len() > 1 {
                format!(
                    "({})",
                    fields
                        .iter()
                        .map(|_| value.clone())
                        .collect::<Vec<_>>()
                        .join(", "),
                )
            } else {
                value
            }
        })
        .chain(match fields.len() {
            0 => Either::Left(iter::empty()),
            1 => Either::Right(iter::once(
                fields
                    .iter()
                    .next()
                    .unwrap_or_else(|| unreachable!("fields.len() == 1"))
                    .ty
                    .to_token_stream()
                    .to_string(),
            )),
            _ => Either::Right(iter::once(format!(
                "({})",
                fields
                    .iter()
                    .map(|f| f.ty.to_token_stream().to_string())
                    .collect::<Vec<_>>()
                    .join(", ")
            ))),
        })
        .collect::<Vec<_>>()
        .join(", ");

    Err(syn::Error::new(
        span,
        format!("legacy syntax, remove `types` and use `{types}` instead"),
    ))
}
