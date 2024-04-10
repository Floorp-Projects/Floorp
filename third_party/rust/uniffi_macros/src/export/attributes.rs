use std::collections::{HashMap, HashSet};

use crate::{
    default::DefaultValue,
    util::{either_attribute_arg, kw, parse_comma_separated, UniffiAttributeArgs},
};

use proc_macro2::TokenStream;
use quote::ToTokens;
use syn::{
    parenthesized,
    parse::{Parse, ParseStream},
    Attribute, Ident, LitStr, Meta, PathArguments, PathSegment, Token,
};
use uniffi_meta::UniffiTraitDiscriminants;

#[derive(Default)]
pub struct ExportTraitArgs {
    pub(crate) async_runtime: Option<AsyncRuntime>,
    pub(crate) callback_interface: Option<kw::callback_interface>,
    pub(crate) with_foreign: Option<kw::with_foreign>,
}

impl Parse for ExportTraitArgs {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        parse_comma_separated(input)
    }
}

impl UniffiAttributeArgs for ExportTraitArgs {
    fn parse_one(input: ParseStream<'_>) -> syn::Result<Self> {
        let lookahead = input.lookahead1();
        if lookahead.peek(kw::async_runtime) {
            let _: kw::async_runtime = input.parse()?;
            let _: Token![=] = input.parse()?;
            Ok(Self {
                async_runtime: Some(input.parse()?),
                ..Self::default()
            })
        } else if lookahead.peek(kw::callback_interface) {
            Ok(Self {
                callback_interface: input.parse()?,
                ..Self::default()
            })
        } else if lookahead.peek(kw::with_foreign) {
            Ok(Self {
                with_foreign: input.parse()?,
                ..Self::default()
            })
        } else {
            Ok(Self::default())
        }
    }

    fn merge(self, other: Self) -> syn::Result<Self> {
        let merged = Self {
            async_runtime: either_attribute_arg(self.async_runtime, other.async_runtime)?,
            callback_interface: either_attribute_arg(
                self.callback_interface,
                other.callback_interface,
            )?,
            with_foreign: either_attribute_arg(self.with_foreign, other.with_foreign)?,
        };
        if merged.callback_interface.is_some() && merged.with_foreign.is_some() {
            return Err(syn::Error::new(
                merged.callback_interface.unwrap().span,
                "`callback_interface` and `with_foreign` are mutually exclusive",
            ));
        }
        Ok(merged)
    }
}

#[derive(Clone, Default)]
pub struct ExportFnArgs {
    pub(crate) async_runtime: Option<AsyncRuntime>,
    pub(crate) name: Option<String>,
    pub(crate) defaults: DefaultMap,
}

impl Parse for ExportFnArgs {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        parse_comma_separated(input)
    }
}

impl UniffiAttributeArgs for ExportFnArgs {
    fn parse_one(input: ParseStream<'_>) -> syn::Result<Self> {
        let lookahead = input.lookahead1();
        if lookahead.peek(kw::async_runtime) {
            let _: kw::async_runtime = input.parse()?;
            let _: Token![=] = input.parse()?;
            Ok(Self {
                async_runtime: Some(input.parse()?),
                ..Self::default()
            })
        } else if lookahead.peek(kw::name) {
            let _: kw::name = input.parse()?;
            let _: Token![=] = input.parse()?;
            let name = Some(input.parse::<LitStr>()?.value());
            Ok(Self {
                name,
                ..Self::default()
            })
        } else if lookahead.peek(kw::default) {
            Ok(Self {
                defaults: DefaultMap::parse(input)?,
                ..Self::default()
            })
        } else {
            Err(syn::Error::new(
                input.span(),
                format!("uniffi::export attribute `{input}` is not supported here."),
            ))
        }
    }

    fn merge(self, other: Self) -> syn::Result<Self> {
        Ok(Self {
            async_runtime: either_attribute_arg(self.async_runtime, other.async_runtime)?,
            name: either_attribute_arg(self.name, other.name)?,
            defaults: self.defaults.merge(other.defaults),
        })
    }
}

#[derive(Default)]
pub struct ExportImplArgs {
    pub(crate) async_runtime: Option<AsyncRuntime>,
}

impl Parse for ExportImplArgs {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        parse_comma_separated(input)
    }
}

impl UniffiAttributeArgs for ExportImplArgs {
    fn parse_one(input: ParseStream<'_>) -> syn::Result<Self> {
        let lookahead = input.lookahead1();
        if lookahead.peek(kw::async_runtime) {
            let _: kw::async_runtime = input.parse()?;
            let _: Token![=] = input.parse()?;
            Ok(Self {
                async_runtime: Some(input.parse()?),
            })
        } else {
            Err(syn::Error::new(
                input.span(),
                format!("uniffi::export attribute `{input}` is not supported here."),
            ))
        }
    }

    fn merge(self, other: Self) -> syn::Result<Self> {
        Ok(Self {
            async_runtime: either_attribute_arg(self.async_runtime, other.async_runtime)?,
        })
    }
}

#[derive(Default)]
pub struct ExportStructArgs {
    pub(crate) traits: HashSet<UniffiTraitDiscriminants>,
}

impl Parse for ExportStructArgs {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        parse_comma_separated(input)
    }
}

impl UniffiAttributeArgs for ExportStructArgs {
    fn parse_one(input: ParseStream<'_>) -> syn::Result<Self> {
        let lookahead = input.lookahead1();
        if lookahead.peek(kw::Debug) {
            input.parse::<Option<kw::Debug>>()?;
            Ok(Self {
                traits: HashSet::from([UniffiTraitDiscriminants::Debug]),
            })
        } else if lookahead.peek(kw::Display) {
            input.parse::<Option<kw::Display>>()?;
            Ok(Self {
                traits: HashSet::from([UniffiTraitDiscriminants::Display]),
            })
        } else if lookahead.peek(kw::Hash) {
            input.parse::<Option<kw::Hash>>()?;
            Ok(Self {
                traits: HashSet::from([UniffiTraitDiscriminants::Hash]),
            })
        } else if lookahead.peek(kw::Eq) {
            input.parse::<Option<kw::Eq>>()?;
            Ok(Self {
                traits: HashSet::from([UniffiTraitDiscriminants::Eq]),
            })
        } else {
            Err(syn::Error::new(
                input.span(),
                format!(
                    "uniffi::export struct attributes must be builtin trait names; `{input}` is invalid"
                ),
            ))
        }
    }

    fn merge(self, other: Self) -> syn::Result<Self> {
        let mut traits = self.traits;
        traits.extend(other.traits);
        Ok(Self { traits })
    }
}

#[derive(Clone)]
pub(crate) enum AsyncRuntime {
    Tokio(LitStr),
}

impl Parse for AsyncRuntime {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        let lit: LitStr = input.parse()?;
        match lit.value().as_str() {
            "tokio" => Ok(Self::Tokio(lit)),
            _ => Err(syn::Error::new_spanned(
                lit,
                "unknown async runtime, currently only `tokio` is supported",
            )),
        }
    }
}

impl ToTokens for AsyncRuntime {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        match self {
            AsyncRuntime::Tokio(lit) => lit.to_tokens(tokens),
        }
    }
}

/// Arguments for function inside an impl block
///
/// This stores the parsed arguments for `uniffi::constructor` and `uniffi::method`
#[derive(Clone, Default)]
pub struct ExportedImplFnArgs {
    pub(crate) name: Option<String>,
    pub(crate) defaults: DefaultMap,
}

impl Parse for ExportedImplFnArgs {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        parse_comma_separated(input)
    }
}

impl UniffiAttributeArgs for ExportedImplFnArgs {
    fn parse_one(input: ParseStream<'_>) -> syn::Result<Self> {
        let lookahead = input.lookahead1();
        if lookahead.peek(kw::name) {
            let _: kw::name = input.parse()?;
            let _: Token![=] = input.parse()?;
            let name = Some(input.parse::<LitStr>()?.value());
            Ok(Self {
                name,
                ..Self::default()
            })
        } else if lookahead.peek(kw::default) {
            Ok(Self {
                defaults: DefaultMap::parse(input)?,
                ..Self::default()
            })
        } else {
            Err(syn::Error::new(
                input.span(),
                format!("uniffi::constructor/method attribute `{input}` is not supported here."),
            ))
        }
    }

    fn merge(self, other: Self) -> syn::Result<Self> {
        Ok(Self {
            name: either_attribute_arg(self.name, other.name)?,
            defaults: self.defaults.merge(other.defaults),
        })
    }
}

#[derive(Default)]
pub(super) struct ExportedImplFnAttributes {
    pub constructor: bool,
    pub args: ExportedImplFnArgs,
}

impl ExportedImplFnAttributes {
    pub fn new(attrs: &[Attribute]) -> syn::Result<Self> {
        let mut this = Self::default();
        for attr in attrs {
            let segs = &attr.path().segments;

            let fst = segs
                .first()
                .expect("attributes have at least one path segment");
            if fst.ident != "uniffi" {
                continue;
            }
            ensure_no_path_args(fst)?;

            let args = match &attr.meta {
                Meta::List(_) => attr.parse_args::<ExportedImplFnArgs>()?,
                _ => Default::default(),
            };
            this.args = args;

            if segs.len() != 2 {
                return Err(syn::Error::new_spanned(
                    segs,
                    "unsupported uniffi attribute",
                ));
            }
            let snd = &segs[1];
            ensure_no_path_args(snd)?;

            match snd.ident.to_string().as_str() {
                "constructor" => {
                    if this.constructor {
                        return Err(syn::Error::new_spanned(
                            attr,
                            "duplicate constructor attribute",
                        ));
                    }
                    this.constructor = true;
                }
                "method" => {
                    if this.constructor {
                        return Err(syn::Error::new_spanned(
                            attr,
                            "confused constructor/method attributes",
                        ));
                    }
                }
                _ => return Err(syn::Error::new_spanned(snd, "unknown uniffi attribute")),
            }
        }

        Ok(this)
    }
}

fn ensure_no_path_args(seg: &PathSegment) -> syn::Result<()> {
    if matches!(seg.arguments, PathArguments::None) {
        Ok(())
    } else {
        Err(syn::Error::new_spanned(&seg.arguments, "unexpected syntax"))
    }
}

/// Maps arguments to defaults for functions
#[derive(Clone, Default)]
pub struct DefaultMap {
    map: HashMap<Ident, DefaultValue>,
}

impl DefaultMap {
    pub fn merge(self, other: Self) -> Self {
        let mut map = self.map;
        map.extend(other.map);
        Self { map }
    }

    pub fn remove(&mut self, ident: &Ident) -> Option<DefaultValue> {
        self.map.remove(ident)
    }

    pub fn idents(&self) -> Vec<&Ident> {
        self.map.keys().collect()
    }
}

impl Parse for DefaultMap {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        let _: kw::default = input.parse()?;
        let content;
        let _ = parenthesized!(content in input);
        let pairs = content.parse_terminated(DefaultPair::parse, Token![,])?;
        Ok(Self {
            map: pairs.into_iter().map(|p| (p.name, p.value)).collect(),
        })
    }
}

pub struct DefaultPair {
    pub name: Ident,
    pub eq_token: Token![=],
    pub value: DefaultValue,
}

impl Parse for DefaultPair {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        Ok(Self {
            name: input.parse()?,
            eq_token: input.parse()?,
            value: input.parse()?,
        })
    }
}
