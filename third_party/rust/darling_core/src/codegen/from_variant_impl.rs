use proc_macro2::TokenStream;
use quote::ToTokens;
use syn::{self, Ident};

use codegen::{ExtractAttribute, OuterFromImpl, TraitImpl};
use options::{DataShape, ForwardAttrs};
use util::PathList;

pub struct FromVariantImpl<'a> {
    pub base: TraitImpl<'a>,
    pub ident: Option<&'a Ident>,
    pub fields: Option<&'a Ident>,
    pub attrs: Option<&'a Ident>,
    pub attr_names: &'a PathList,
    pub forward_attrs: Option<&'a ForwardAttrs>,
    pub from_ident: bool,
    pub supports: Option<&'a DataShape>,
}

impl<'a> ToTokens for FromVariantImpl<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let input = self.param_name();
        let extractor = self.extractor();
        let passed_ident = self.ident
            .as_ref()
            .map(|i| quote!(#i: #input.ident.clone(),));
        let passed_attrs = self.attrs.as_ref().map(|i| quote!(#i: __fwd_attrs,));
        let passed_fields = self.fields
            .as_ref()
            .map(|i| quote!(#i: ::darling::ast::Fields::try_from(&#input.fields)?,));

        let inits = self.base.initializers();
        let map = self.base.map_fn();

        let default = if self.from_ident {
            quote!(let __default: Self = ::darling::export::From::from(#input.ident.clone());)
        } else {
            self.base.fallback_decl()
        };

        let supports = self.supports.map(|i| {
            quote! {
                #i
                __validate_data(&#input.fields)?;
            }
        });

        let error_declaration = self.base.declare_errors();
        let require_fields = self.base.require_fields();
        let error_check = self.base.check_errors();

        self.wrap(
            quote!(
            fn from_variant(#input: &::syn::Variant) -> ::darling::Result<Self> {
                #error_declaration

                #extractor

                #supports

                #require_fields

                #error_check

                #default

                ::darling::export::Ok(Self {
                    #passed_ident
                    #passed_attrs
                    #passed_fields
                    #inits
                }) #map
            }
        ),
            tokens,
        );
    }
}

impl<'a> ExtractAttribute for FromVariantImpl<'a> {
    fn local_declarations(&self) -> TokenStream {
        self.base.local_declarations()
    }

    fn immutable_declarations(&self) -> TokenStream {
        self.base.immutable_declarations()
    }

    fn attr_names(&self) -> &PathList {
        &self.attr_names
    }

    fn forwarded_attrs(&self) -> Option<&ForwardAttrs> {
        self.forward_attrs
    }

    fn param_name(&self) -> TokenStream {
        quote!(__variant)
    }

    fn core_loop(&self) -> TokenStream {
        self.base.core_loop()
    }
}

impl<'a> OuterFromImpl<'a> for FromVariantImpl<'a> {
    fn trait_path(&self) -> syn::Path {
        path!(::darling::FromVariant)
    }

    fn trait_bound(&self) -> syn::Path {
        path!(::darling::FromMeta)
    }

    fn base(&'a self) -> &'a TraitImpl<'a> {
        &self.base
    }
}
