//! Implementation of an [`Into`] derive macro.

use std::{borrow::Cow, iter};

use proc_macro2::{Span, TokenStream};
use quote::{quote, ToTokens as _};
use syn::{
    ext::IdentExt as _,
    parse::{discouraged::Speculative as _, Parse, ParseStream},
    punctuated::Punctuated,
    spanned::Spanned as _,
    token, Ident,
};

use crate::{
    parsing::Type,
    utils::{polyfill, Either, FieldsExt as _},
};

/// Expands an [`Into`] derive macro.
pub fn expand(input: &syn::DeriveInput, _: &'static str) -> syn::Result<TokenStream> {
    let data = match &input.data {
        syn::Data::Struct(data) => Ok(data),
        syn::Data::Enum(e) => Err(syn::Error::new(
            e.enum_token.span(),
            "`Into` cannot be derived for enums",
        )),
        syn::Data::Union(u) => Err(syn::Error::new(
            u.union_token.span(),
            "`Into` cannot be derived for unions",
        )),
    }?;

    let attr = StructAttribute::parse_attrs(&input.attrs, &data.fields)?
        .unwrap_or_else(|| StructAttribute {
            owned: Some(Punctuated::new()),
            r#ref: None,
            ref_mut: None,
        });
    let ident = &input.ident;
    let fields = data
        .fields
        .iter()
        .enumerate()
        .filter_map(|(i, f)| match SkipFieldAttribute::parse_attrs(&f.attrs) {
            Ok(None) => Some(Ok((
                &f.ty,
                f.ident
                    .as_ref()
                    .map_or_else(|| Either::Right(syn::Index::from(i)), Either::Left),
            ))),
            Ok(Some(_)) => None,
            Err(e) => Some(Err(e)),
        })
        .collect::<syn::Result<Vec<_>>>()?;
    let (fields_tys, fields_idents): (Vec<_>, Vec<_>) = fields.into_iter().unzip();
    let (fields_tys, fields_idents) = (&fields_tys, &fields_idents);

    let expand = |tys: Option<Punctuated<_, _>>, r: bool, m: bool| {
        let Some(tys) = tys else {
            return Either::Left(iter::empty());
        };

        let lf =
            r.then(|| syn::Lifetime::new("'__derive_more_into", Span::call_site()));
        let r = r.then(token::And::default);
        let m = m.then(token::Mut::default);

        let gens = if let Some(lf) = lf.clone() {
            let mut gens = input.generics.clone();
            gens.params.push(syn::LifetimeParam::new(lf).into());
            Cow::Owned(gens)
        } else {
            Cow::Borrowed(&input.generics)
        };

        Either::Right(
            if tys.is_empty() {
                Either::Left(iter::once(Type::tuple(fields_tys.clone())))
            } else {
                Either::Right(tys.into_iter())
            }
            .map(move |ty| {
                let tys = fields_tys.validate_type(&ty)?.collect::<Vec<_>>();
                let (impl_gens, _, where_clause) = gens.split_for_impl();
                let (_, ty_gens, _) = input.generics.split_for_impl();

                Ok(quote! {
                    #[automatically_derived]
                    impl #impl_gens ::core::convert::From<#r #lf #m #ident #ty_gens>
                     for ( #( #r #lf #m #tys ),* ) #where_clause
                    {
                        #[inline]
                        fn from(value: #r #lf #m #ident #ty_gens) -> Self {
                            (#(
                                <#r #m #tys as ::core::convert::From<_>>::from(
                                    #r #m value. #fields_idents
                                )
                            ),*)
                        }
                    }
                })
            }),
        )
    };

    [
        expand(attr.owned, false, false),
        expand(attr.r#ref, true, false),
        expand(attr.ref_mut, true, true),
    ]
    .into_iter()
    .flatten()
    .collect()
}

/// Representation of an [`Into`] derive macro struct container attribute.
///
/// ```rust,ignore
/// #[into(<types>)]
/// #[into(owned(<types>), ref(<types>), ref_mut(<types>))]
/// ```
#[derive(Debug, Default)]
struct StructAttribute {
    /// [`Type`]s wrapped into `owned(...)` or simply `#[into(...)]`.
    owned: Option<Punctuated<Type, token::Comma>>,

    /// [`Type`]s wrapped into `ref(...)`.
    r#ref: Option<Punctuated<Type, token::Comma>>,

    /// [`Type`]s wrapped into `ref_mut(...)`.
    ref_mut: Option<Punctuated<Type, token::Comma>>,
}

impl StructAttribute {
    /// Parses a [`StructAttribute`] from the provided [`syn::Attribute`]s.
    fn parse_attrs(
        attrs: impl AsRef<[syn::Attribute]>,
        fields: &syn::Fields,
    ) -> syn::Result<Option<Self>> {
        fn infer<T>(v: T) -> T
        where
            T: for<'a> FnOnce(ParseStream<'a>) -> syn::Result<StructAttribute>,
        {
            v
        }

        attrs
            .as_ref()
            .iter()
            .filter(|attr| attr.path().is_ident("into"))
            .try_fold(None, |mut attrs, attr| {
                let merge = |out: &mut Option<_>, tys| match (out.as_mut(), tys) {
                    (None, Some(tys)) => {
                        *out = Some::<Punctuated<_, _>>(tys);
                    }
                    (Some(out), Some(tys)) => out.extend(tys),
                    (Some(_), None) | (None, None) => {}
                };

                let field_attr =
                    attr.parse_args_with(infer(|stream| Self::parse(stream, fields)))?;
                let out = attrs.get_or_insert_with(Self::default);
                merge(&mut out.owned, field_attr.owned);
                merge(&mut out.r#ref, field_attr.r#ref);
                merge(&mut out.ref_mut, field_attr.ref_mut);

                Ok(attrs)
            })
    }

    /// Parses a single [`StructAttribute`].
    fn parse(content: ParseStream<'_>, fields: &syn::Fields) -> syn::Result<Self> {
        check_legacy_syntax(content, fields)?;

        let mut out = Self::default();

        let parse_inner = |ahead, types: &mut Option<_>| {
            content.advance_to(&ahead);

            let types = types.get_or_insert_with(Punctuated::new);
            if content.peek(token::Paren) {
                let inner;
                syn::parenthesized!(inner in content);

                types.extend(
                    inner
                        .parse_terminated(Type::parse, token::Comma)?
                        .into_pairs(),
                );
            }
            if content.peek(token::Comma) {
                let comma = content.parse::<token::Comma>()?;
                if !types.empty_or_trailing() {
                    types.push_punct(comma);
                }
            }

            Ok(())
        };

        let mut has_wrapped_type = false;
        let mut top_level_type = None;

        while !content.is_empty() {
            let ahead = content.fork();
            let res = if ahead.peek(Ident::peek_any) {
                ahead.call(Ident::parse_any).map(Into::into)
            } else {
                ahead.parse::<syn::Path>()
            };
            match res {
                Ok(p) if p.is_ident("owned") => {
                    has_wrapped_type = true;
                    parse_inner(ahead, &mut out.owned)?;
                }
                Ok(p) if p.is_ident("ref") => {
                    has_wrapped_type = true;
                    parse_inner(ahead, &mut out.r#ref)?;
                }
                Ok(p) if p.is_ident("ref_mut") => {
                    has_wrapped_type = true;
                    parse_inner(ahead, &mut out.ref_mut)?;
                }
                _ => {
                    let ty = content.parse::<Type>()?;
                    let _ = top_level_type.get_or_insert_with(|| ty.clone());
                    out.owned.get_or_insert_with(Punctuated::new).push_value(ty);

                    if content.peek(token::Comma) {
                        out.owned
                            .get_or_insert_with(Punctuated::new)
                            .push_punct(content.parse::<token::Comma>()?)
                    }
                }
            }
        }

        if let Some(ty) = top_level_type.filter(|_| has_wrapped_type) {
            Err(syn::Error::new(
                ty.span(),
                format!(
                    "mixing regular types with wrapped into \
                     `owned`/`ref`/`ref_mut` is not allowed, try wrapping \
                     this type into `owned({ty}), ref({ty}), ref_mut({ty})`",
                    ty = ty.into_token_stream(),
                ),
            ))
        } else {
            Ok(out)
        }
    }
}

/// `#[into(skip)]` field attribute.
struct SkipFieldAttribute;

impl SkipFieldAttribute {
    /// Parses a [`SkipFieldAttribute`] from the provided [`syn::Attribute`]s.
    fn parse_attrs(attrs: impl AsRef<[syn::Attribute]>) -> syn::Result<Option<Self>> {
        Ok(attrs
            .as_ref()
            .iter()
            .filter(|attr| attr.path().is_ident("into"))
            .try_fold(None, |mut attrs, attr| {
                let field_attr = attr.parse_args::<SkipFieldAttribute>()?;
                if let Some((path, _)) = attrs.replace((attr.path(), field_attr)) {
                    Err(syn::Error::new(
                        path.span(),
                        "only single `#[into(...)]` attribute is allowed here",
                    ))
                } else {
                    Ok(attrs)
                }
            })?
            .map(|(_, attr)| attr))
    }
}

impl Parse for SkipFieldAttribute {
    fn parse(content: ParseStream) -> syn::Result<Self> {
        match content.parse::<syn::Path>()? {
            p if p.is_ident("skip") | p.is_ident("ignore") => Ok(Self),
            p => Err(syn::Error::new(
                p.span(),
                format!("expected `skip`, found: `{}`", p.into_token_stream()),
            )),
        }
    }
}

/// [`Error`]ors for legacy syntax: `#[into(types(i32, "&str"))]`.
fn check_legacy_syntax(
    tokens: ParseStream<'_>,
    fields: &syn::Fields,
) -> syn::Result<()> {
    let span = tokens.span();
    let tokens = tokens.fork();

    let map_ty = |s: String| {
        if fields.len() > 1 {
            format!(
                "({})",
                (0..fields.len())
                    .map(|_| s.as_str())
                    .collect::<Vec<_>>()
                    .join(", ")
            )
        } else {
            s
        }
    };
    let field = match fields.len() {
        0 => None,
        1 => Some(
            fields
                .iter()
                .next()
                .unwrap_or_else(|| unreachable!("fields.len() == 1"))
                .ty
                .to_token_stream()
                .to_string(),
        ),
        _ => Some(format!(
            "({})",
            fields
                .iter()
                .map(|f| f.ty.to_token_stream().to_string())
                .collect::<Vec<_>>()
                .join(", ")
        )),
    };

    let Ok(metas) = tokens.parse_terminated(polyfill::Meta::parse, token::Comma) else {
        return Ok(());
    };

    let parse_list = |list: polyfill::MetaList, attrs: &mut Option<Vec<_>>| {
        if !list.path.is_ident("types") {
            return None;
        }
        for meta in list
            .parse_args_with(Punctuated::<_, token::Comma>::parse_terminated)
            .ok()?
        {
            attrs.get_or_insert_with(Vec::new).push(match meta {
                polyfill::NestedMeta::Lit(syn::Lit::Str(str)) => str.value(),
                polyfill::NestedMeta::Meta(polyfill::Meta::Path(path)) => {
                    path.into_token_stream().to_string()
                }
                _ => return None,
            })
        }
        Some(())
    };

    let Some((top_level, owned, ref_, ref_mut)) = metas
            .into_iter()
            .try_fold(
                (None, None, None, None),
                |(mut top_level, mut owned, mut ref_, mut ref_mut), meta| {
                    let is = |name| {
                        matches!(&meta, polyfill::Meta::Path(p) if p.is_ident(name))
                            || matches!(&meta, polyfill::Meta::List(list) if list.path.is_ident(name))
                    };
                    let parse_inner = |meta, attrs: &mut Option<_>| {
                        match meta {
                            polyfill::Meta::Path(_) => {
                                let _ = attrs.get_or_insert_with(Vec::new);
                                Some(())
                            }
                            polyfill::Meta::List(list) => {
                                if let polyfill::NestedMeta::Meta(polyfill::Meta::List(list)) = list
                                    .parse_args_with(Punctuated::<_, token::Comma>::parse_terminated)
                                    .ok()?
                                    .pop()?
                                    .into_value()
                                {
                                    parse_list(list, attrs)
                                } else {
                                    None
                                }
                            }
                        }
                    };

                    match meta {
                        meta if is("owned") => parse_inner(meta, &mut owned),
                        meta if is("ref") => parse_inner(meta, &mut ref_),
                        meta if is("ref_mut") => parse_inner(meta, &mut ref_mut),
                        polyfill::Meta::List(list) => parse_list(list, &mut top_level),
                        _ => None,
                    }
                    .map(|_| (top_level, owned, ref_, ref_mut))
                },
            )
            .filter(|(top_level, owned, ref_, ref_mut)| {
                [top_level, owned, ref_, ref_mut]
                    .into_iter()
                    .any(|l| l.as_ref().map_or(false, |l| !l.is_empty()))
            })
        else {
            return Ok(());
        };

    if [&owned, &ref_, &ref_mut].into_iter().any(Option::is_some) {
        let format = |list: Option<Vec<_>>, name: &str| match list {
            Some(l)
                if top_level.as_ref().map_or(true, Vec::is_empty) && l.is_empty() =>
            {
                Some(name.to_owned())
            }
            Some(l) => Some(format!(
                "{}({})",
                name,
                l.into_iter()
                    .chain(top_level.clone().into_iter().flatten())
                    .map(map_ty)
                    .chain(field.clone())
                    .collect::<Vec<_>>()
                    .join(", "),
            )),
            None => None,
        };
        let format = [
            format(owned, "owned"),
            format(ref_, "ref"),
            format(ref_mut, "ref_mut"),
        ]
        .into_iter()
        .flatten()
        .collect::<Vec<_>>()
        .join(", ");

        Err(syn::Error::new(
            span,
            format!("legacy syntax, use `{format}` instead"),
        ))
    } else {
        Err(syn::Error::new(
            span,
            format!(
                "legacy syntax, remove `types` and use `{}` instead",
                top_level.unwrap_or_else(|| unreachable!()).join(", "),
            ),
        ))
    }
}
