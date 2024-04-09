/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::fnsig::FnSignature;
use proc_macro::TokenStream;
use proc_macro2::{Ident, Span};
use quote::ToTokens;

use super::attributes::{
    ExportFnArgs, ExportImplArgs, ExportStructArgs, ExportTraitArgs, ExportedImplFnArgs,
    ExportedImplFnAttributes,
};
use crate::util::extract_docstring;
use uniffi_meta::UniffiTraitDiscriminants;

pub(super) enum ExportItem {
    Function {
        sig: FnSignature,
        args: ExportFnArgs,
    },
    Impl {
        self_ident: Ident,
        items: Vec<ImplItem>,
        args: ExportImplArgs,
    },
    Trait {
        self_ident: Ident,
        items: Vec<ImplItem>,
        with_foreign: bool,
        callback_interface_only: bool,
        docstring: String,
        args: ExportTraitArgs,
    },
    Struct {
        self_ident: Ident,
        uniffi_traits: Vec<UniffiTraitDiscriminants>,
    },
}

impl ExportItem {
    pub fn new(item: syn::Item, attr_args: TokenStream) -> syn::Result<Self> {
        match item {
            syn::Item::Fn(item) => {
                let args: ExportFnArgs = syn::parse(attr_args)?;
                let docstring = extract_docstring(&item.attrs)?;
                let sig = FnSignature::new_function(item.sig, args.clone(), docstring)?;
                Ok(Self::Function { sig, args })
            }
            syn::Item::Impl(item) => Self::from_impl(item, attr_args),
            syn::Item::Trait(item) => Self::from_trait(item, attr_args),
            syn::Item::Struct(item) => Self::from_struct(item, attr_args),
            // FIXME: Support const / static?
            _ => Err(syn::Error::new(
                Span::call_site(),
                "unsupported item: only functions and impl \
                 blocks may be annotated with this attribute",
            )),
        }
    }

    pub fn from_impl(item: syn::ItemImpl, attr_args: TokenStream) -> syn::Result<Self> {
        let args: ExportImplArgs = syn::parse(attr_args)?;
        if !item.generics.params.is_empty() || item.generics.where_clause.is_some() {
            return Err(syn::Error::new_spanned(
                &item.generics,
                "generic impls are not currently supported by uniffi::export",
            ));
        }

        let type_path = type_as_type_path(&item.self_ty)?;

        if type_path.qself.is_some() {
            return Err(syn::Error::new_spanned(
                type_path,
                "qualified self types are not currently supported by uniffi::export",
            ));
        }

        let self_ident = match type_path.path.get_ident() {
            Some(id) => id,
            None => {
                return Err(syn::Error::new_spanned(
                    type_path,
                    "qualified paths in self-types are not currently supported by uniffi::export",
                ));
            }
        };

        let items = item
            .items
            .into_iter()
            .map(|item| {
                let impl_fn = match item {
                    syn::ImplItem::Fn(m) => m,
                    _ => {
                        return Err(syn::Error::new_spanned(
                            item,
                            "only fn's are supported in impl blocks annotated with uniffi::export",
                        ));
                    }
                };

                let docstring = extract_docstring(&impl_fn.attrs)?;
                let attrs = ExportedImplFnAttributes::new(&impl_fn.attrs)?;
                let item = if attrs.constructor {
                    ImplItem::Constructor(FnSignature::new_constructor(
                        self_ident.clone(),
                        impl_fn.sig,
                        attrs.args,
                        docstring,
                    )?)
                } else {
                    ImplItem::Method(FnSignature::new_method(
                        self_ident.clone(),
                        impl_fn.sig,
                        attrs.args,
                        docstring,
                    )?)
                };

                Ok(item)
            })
            .collect::<syn::Result<_>>()?;

        Ok(Self::Impl {
            items,
            self_ident: self_ident.to_owned(),
            args,
        })
    }

    fn from_trait(item: syn::ItemTrait, attr_args: TokenStream) -> syn::Result<Self> {
        let args: ExportTraitArgs = syn::parse(attr_args)?;
        let with_foreign = args.callback_interface.is_some() || args.with_foreign.is_some();
        let callback_interface_only = args.callback_interface.is_some();

        if !item.generics.params.is_empty() || item.generics.where_clause.is_some() {
            return Err(syn::Error::new_spanned(
                &item.generics,
                "generic impls are not currently supported by uniffi::export",
            ));
        }

        let self_ident = item.ident.to_owned();
        let docstring = extract_docstring(&item.attrs)?;
        let items = item
            .items
            .into_iter()
            .enumerate()
            .map(|(i, item)| {
                let tim = match item {
                    syn::TraitItem::Fn(tim) => tim,
                    _ => {
                        return Err(syn::Error::new_spanned(
                            item,
                            "only fn's are supported in traits annotated with uniffi::export",
                        ));
                    }
                };

                let docstring = extract_docstring(&tim.attrs)?;
                let attrs = ExportedImplFnAttributes::new(&tim.attrs)?;
                let item = if attrs.constructor {
                    return Err(syn::Error::new_spanned(
                        tim,
                        "exported traits can not have constructors",
                    ));
                } else {
                    ImplItem::Method(FnSignature::new_trait_method(
                        self_ident.clone(),
                        tim.sig,
                        ExportedImplFnArgs::default(),
                        i as u32,
                        docstring,
                    )?)
                };

                Ok(item)
            })
            .collect::<syn::Result<_>>()?;

        Ok(Self::Trait {
            items,
            self_ident,
            with_foreign,
            callback_interface_only,
            docstring,
            args,
        })
    }

    fn from_struct(item: syn::ItemStruct, attr_args: TokenStream) -> syn::Result<Self> {
        let args: ExportStructArgs = syn::parse(attr_args)?;
        let uniffi_traits: Vec<UniffiTraitDiscriminants> = args.traits.into_iter().collect();
        if uniffi_traits.is_empty() {
            Err(syn::Error::new(Span::call_site(),
                "uniffi::export on a struct must supply a builtin trait name. Did you mean `#[derive(uniffi::Object)]`?"
            ))
        } else {
            Ok(Self::Struct {
                self_ident: item.ident,
                uniffi_traits,
            })
        }
    }
}

pub(super) enum ImplItem {
    Constructor(FnSignature),
    Method(FnSignature),
}

fn type_as_type_path(ty: &syn::Type) -> syn::Result<&syn::TypePath> {
    match ty {
        syn::Type::Group(g) => type_as_type_path(&g.elem),
        syn::Type::Paren(p) => type_as_type_path(&p.elem),
        syn::Type::Path(p) => Ok(p),
        _ => Err(type_not_supported(ty)),
    }
}

fn type_not_supported(ty: &impl ToTokens) -> syn::Error {
    syn::Error::new_spanned(
        ty,
        "this type is not currently supported by uniffi::export in this position",
    )
}
