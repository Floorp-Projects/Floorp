//! Implementation of [`fmt::Display`]-like derive macros.

#[cfg(doc)]
use std::fmt;

use proc_macro2::{Ident, TokenStream};
use quote::{format_ident, quote};
use syn::{
    parse::{Parse, ParseStream},
    parse_quote,
    spanned::Spanned as _,
};

use super::{BoundsAttribute, FmtAttribute};

/// Expands a [`fmt::Display`]-like derive macro.
///
/// Available macros:
/// - [`Binary`](fmt::Binary)
/// - [`Display`](fmt::Display)
/// - [`LowerExp`](fmt::LowerExp)
/// - [`LowerHex`](fmt::LowerHex)
/// - [`Octal`](fmt::Octal)
/// - [`Pointer`](fmt::Pointer)
/// - [`UpperExp`](fmt::UpperExp)
/// - [`UpperHex`](fmt::UpperHex)
pub fn expand(input: &syn::DeriveInput, trait_name: &str) -> syn::Result<TokenStream> {
    let trait_name = normalize_trait_name(trait_name);

    let attrs = Attributes::parse_attrs(&input.attrs, trait_name)?;
    let trait_ident = format_ident!("{trait_name}");
    let ident = &input.ident;

    let ctx = (&attrs, ident, &trait_ident, trait_name);
    let (bounds, body) = match &input.data {
        syn::Data::Struct(s) => expand_struct(s, ctx),
        syn::Data::Enum(e) => expand_enum(e, ctx),
        syn::Data::Union(u) => expand_union(u, ctx),
    }?;

    let (impl_gens, ty_gens, where_clause) = {
        let (impl_gens, ty_gens, where_clause) = input.generics.split_for_impl();
        let mut where_clause = where_clause
            .cloned()
            .unwrap_or_else(|| parse_quote! { where });
        where_clause.predicates.extend(bounds);
        (impl_gens, ty_gens, where_clause)
    };

    Ok(quote! {
        #[automatically_derived]
        impl #impl_gens ::core::fmt::#trait_ident for #ident #ty_gens
             #where_clause
        {
            fn fmt(
                &self, __derive_more_f: &mut ::core::fmt::Formatter<'_>
            ) -> ::core::fmt::Result {
                #body
            }
        }
    })
}

/// Type alias for an expansion context:
/// - [`Attributes`].
/// - Struct/enum/union [`Ident`].
/// - Derived trait [`Ident`].
/// - Derived trait `&`[`str`].
type ExpansionCtx<'a> = (&'a Attributes, &'a Ident, &'a Ident, &'a str);

/// Expands a [`fmt::Display`]-like derive macro for the provided struct.
fn expand_struct(
    s: &syn::DataStruct,
    (attrs, ident, trait_ident, _): ExpansionCtx<'_>,
) -> syn::Result<(Vec<syn::WherePredicate>, TokenStream)> {
    let s = Expansion {
        attrs,
        fields: &s.fields,
        trait_ident,
        ident,
    };
    let bounds = s.generate_bounds();
    let body = s.generate_body()?;

    let vars = s.fields.iter().enumerate().map(|(i, f)| {
        let var = f.ident.clone().unwrap_or_else(|| format_ident!("_{i}"));
        let member = f
            .ident
            .clone()
            .map_or_else(|| syn::Member::Unnamed(i.into()), syn::Member::Named);
        quote! {
            let #var = &self.#member;
        }
    });

    let body = quote! {
        #( #vars )*
        #body
    };

    Ok((bounds, body))
}

/// Expands a [`fmt`]-like derive macro for the provided enum.
fn expand_enum(
    e: &syn::DataEnum,
    (attrs, _, trait_ident, trait_name): ExpansionCtx<'_>,
) -> syn::Result<(Vec<syn::WherePredicate>, TokenStream)> {
    if attrs.fmt.is_some() {
        todo!("https://github.com/JelteF/derive_more/issues/142");
    }

    let (bounds, match_arms) = e.variants.iter().try_fold(
        (Vec::new(), TokenStream::new()),
        |(mut bounds, mut arms), variant| {
            let attrs = Attributes::parse_attrs(&variant.attrs, trait_name)?;
            let ident = &variant.ident;

            if attrs.fmt.is_none()
                && variant.fields.is_empty()
                && trait_name != "Display"
            {
                return Err(syn::Error::new(
                    e.variants.span(),
                    format!(
                        "implicit formatting of unit enum variant is supported \
                         only for `Display` macro, use `#[{}(\"...\")]` to \
                         explicitly specify the formatting",
                        trait_name_to_attribute_name(trait_name),
                    ),
                ));
            }

            let v = Expansion {
                attrs: &attrs,
                fields: &variant.fields,
                trait_ident,
                ident,
            };
            let arm_body = v.generate_body()?;
            bounds.extend(v.generate_bounds());

            let fields_idents =
                variant.fields.iter().enumerate().map(|(i, f)| {
                    f.ident.clone().unwrap_or_else(|| format_ident!("_{i}"))
                });
            let matcher = match variant.fields {
                syn::Fields::Named(_) => {
                    quote! { Self::#ident { #( #fields_idents ),* } }
                }
                syn::Fields::Unnamed(_) => {
                    quote! { Self::#ident ( #( #fields_idents ),* ) }
                }
                syn::Fields::Unit => quote! { Self::#ident },
            };

            arms.extend([quote! { #matcher => { #arm_body }, }]);

            Ok::<_, syn::Error>((bounds, arms))
        },
    )?;

    let body = match_arms
        .is_empty()
        .then(|| quote! { match *self {} })
        .unwrap_or_else(|| quote! { match self { #match_arms } });

    Ok((bounds, body))
}

/// Expands a [`fmt::Display`]-like derive macro for the provided union.
fn expand_union(
    u: &syn::DataUnion,
    (attrs, _, _, trait_name): ExpansionCtx<'_>,
) -> syn::Result<(Vec<syn::WherePredicate>, TokenStream)> {
    let fmt = &attrs.fmt.as_ref().ok_or_else(|| {
        syn::Error::new(
            u.fields.span(),
            format!(
                "unions must have `#[{}(\"...\", ...)]` attribute",
                trait_name_to_attribute_name(trait_name),
            ),
        )
    })?;

    Ok((
        attrs.bounds.0.clone().into_iter().collect(),
        quote! { ::core::write!(__derive_more_f, #fmt) },
    ))
}

/// Representation of a [`fmt::Display`]-like derive macro attribute.
///
/// ```rust,ignore
/// #[<fmt_trait>("<fmt_literal>", <fmt_args>)]
/// #[bound(<bounds>)]
/// ```
///
/// `#[<fmt_trait>(...)]` can be specified only once, while multiple
/// `#[<fmt_trait>(bound(...))]` are allowed.
#[derive(Debug, Default)]
struct Attributes {
    /// Interpolation [`FmtAttribute`].
    fmt: Option<FmtAttribute>,

    /// Addition trait bounds.
    bounds: BoundsAttribute,
}

impl Attributes {
    /// Parses [`Attributes`] from the provided [`syn::Attribute`]s.
    fn parse_attrs(
        attrs: impl AsRef<[syn::Attribute]>,
        trait_name: &str,
    ) -> syn::Result<Self> {
        attrs
            .as_ref()
            .iter()
            .filter(|attr| attr.path().is_ident(trait_name_to_attribute_name(trait_name)))
            .try_fold(Attributes::default(), |mut attrs, attr| {
                let attr = attr.parse_args::<Attribute>()?;
                match attr {
                    Attribute::Bounds(more) => {
                        attrs.bounds.0.extend(more.0);
                    }
                    Attribute::Fmt(fmt) => {
                        attrs.fmt.replace(fmt).map_or(Ok(()), |dup| Err(syn::Error::new(
                            dup.span(),
                            format!(
                                "multiple `#[{}(\"...\", ...)]` attributes aren't allowed",
                                trait_name_to_attribute_name(trait_name),
                            ))))?;
                    }
                };
                Ok(attrs)
            })
    }
}

/// Representation of a single [`fmt::Display`]-like derive macro attribute.
#[derive(Debug)]
enum Attribute {
    /// [`fmt`] attribute.
    Fmt(FmtAttribute),

    /// Addition trait bounds.
    Bounds(BoundsAttribute),
}

impl Parse for Attribute {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        BoundsAttribute::check_legacy_fmt(input)?;
        FmtAttribute::check_legacy_fmt(input)?;

        if input.peek(syn::LitStr) {
            input.parse().map(Attribute::Fmt)
        } else {
            input.parse().map(Attribute::Bounds)
        }
    }
}

/// Helper struct to generate [`Display::fmt()`] implementation body and trait
/// bounds for a struct or an enum variant.
///
/// [`Display::fmt()`]: fmt::Display::fmt()
#[derive(Debug)]
struct Expansion<'a> {
    /// Derive macro [`Attributes`].
    attrs: &'a Attributes,

    /// Struct or enum [`Ident`].
    ident: &'a Ident,

    /// Struct or enum [`syn::Fields`].
    fields: &'a syn::Fields,

    /// [`fmt`] trait [`Ident`].
    trait_ident: &'a Ident,
}

impl<'a> Expansion<'a> {
    /// Generates [`Display::fmt()`] implementation for a struct or an enum variant.
    ///
    /// # Errors
    ///
    /// In case [`FmtAttribute`] is [`None`] and [`syn::Fields`] length is
    /// greater than 1.
    ///
    /// [`Display::fmt()`]: fmt::Display::fmt()
    fn generate_body(&self) -> syn::Result<TokenStream> {
        match &self.attrs.fmt {
            Some(fmt) => Ok(quote! { ::core::write!(__derive_more_f, #fmt) }),
            None if self.fields.is_empty() => {
                let ident_str = self.ident.to_string();
                Ok(quote! { ::core::write!(__derive_more_f, #ident_str) })
            }
            None if self.fields.len() == 1 => {
                let field = self
                    .fields
                    .iter()
                    .next()
                    .unwrap_or_else(|| unreachable!("count() == 1"));
                let ident = field.ident.clone().unwrap_or_else(|| format_ident!("_0"));
                let trait_ident = self.trait_ident;
                Ok(quote! {
                    ::core::fmt::#trait_ident::fmt(#ident, __derive_more_f)
                })
            }
            _ => Err(syn::Error::new(
                self.fields.span(),
                format!(
                    "struct or enum variant with more than 1 field must have \
                     `#[{}(\"...\", ...)]` attribute",
                    trait_name_to_attribute_name(self.trait_ident),
                ),
            )),
        }
    }

    /// Generates trait bounds for a struct or an enum variant.
    fn generate_bounds(&self) -> Vec<syn::WherePredicate> {
        let Some(fmt) = &self.attrs.fmt else {
            return self
                .fields
                .iter()
                .next()
                .map(|f| {
                    let ty = &f.ty;
                    let trait_ident = &self.trait_ident;
                    vec![parse_quote! { #ty: ::core::fmt::#trait_ident }]
                })
                .unwrap_or_default();
        };

        fmt.bounded_types(self.fields)
            .map(|(ty, trait_name)| {
                let tr = format_ident!("{}", trait_name);
                parse_quote! { #ty: ::core::fmt::#tr }
            })
            .chain(self.attrs.bounds.0.clone())
            .collect()
    }
}

/// Matches the provided `trait_name` to appropriate [`Attribute::Fmt`] argument name.
fn trait_name_to_attribute_name<T>(trait_name: T) -> &'static str
where
    T: for<'a> PartialEq<&'a str>,
{
    match () {
        _ if trait_name == "Binary" => "binary",
        _ if trait_name == "Display" => "display",
        _ if trait_name == "LowerExp" => "lower_exp",
        _ if trait_name == "LowerHex" => "lower_hex",
        _ if trait_name == "Octal" => "octal",
        _ if trait_name == "Pointer" => "pointer",
        _ if trait_name == "UpperExp" => "upper_exp",
        _ if trait_name == "UpperHex" => "upper_hex",
        _ => unimplemented!(),
    }
}

/// Matches the provided derive macro `name` to appropriate actual trait name.
fn normalize_trait_name(name: &str) -> &'static str {
    match name {
        "Binary" => "Binary",
        "Display" => "Display",
        "LowerExp" => "LowerExp",
        "LowerHex" => "LowerHex",
        "Octal" => "Octal",
        "Pointer" => "Pointer",
        "UpperExp" => "UpperExp",
        "UpperHex" => "UpperHex",
        _ => unimplemented!(),
    }
}
