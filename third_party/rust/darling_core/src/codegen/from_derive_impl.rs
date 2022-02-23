use proc_macro2::TokenStream;
use quote::ToTokens;
use syn::{self, Ident};

use ast::Data;
use codegen::{ExtractAttribute, OuterFromImpl, TraitImpl};
use options::{ForwardAttrs, Shape};
use util::PathList;

pub struct FromDeriveInputImpl<'a> {
    pub ident: Option<&'a Ident>,
    pub generics: Option<&'a Ident>,
    pub vis: Option<&'a Ident>,
    pub attrs: Option<&'a Ident>,
    pub data: Option<&'a Ident>,
    pub base: TraitImpl<'a>,
    pub attr_names: &'a PathList,
    pub forward_attrs: Option<&'a ForwardAttrs>,
    pub from_ident: bool,
    pub supports: Option<&'a Shape>,
}

impl<'a> ToTokens for FromDeriveInputImpl<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let ty_ident = self.base.ident;
        let input = self.param_name();
        let map = self.base.map_fn();

        if let Data::Struct(ref data) = self.base.data {
            if data.is_newtype() {
                self.wrap(
                    quote!{
                        fn from_derive_input(#input: &::syn::DeriveInput) -> ::darling::Result<Self> {
                            ::darling::export::Ok(
                                #ty_ident(::darling::FromDeriveInput::from_derive_input(#input)?)
                            ) #map
                        }
                    },
                    tokens,
                );

                return;
            }
        }

        let passed_ident = self.ident
            .as_ref()
            .map(|i| quote!(#i: #input.ident.clone(),));
        let passed_vis = self.vis.as_ref().map(|i| quote!(#i: #input.vis.clone(),));
        let passed_generics = self.generics
            .as_ref()
            .map(|i| quote!(#i: ::darling::FromGenerics::from_generics(&#input.generics)?,));
        let passed_attrs = self.attrs.as_ref().map(|i| quote!(#i: __fwd_attrs,));
        let passed_body = self.data
            .as_ref()
            .map(|i| quote!(#i: ::darling::ast::Data::try_from(&#input.data)?,));

        let supports = self.supports.map(|i| {
            quote!{
                #i
                __validate_body(&#input.data)?;
            }
        });

        let inits = self.base.initializers();
        let default = if self.from_ident {
            quote!(let __default: Self = ::darling::export::From::from(#input.ident.clone());)
        } else {
            self.base.fallback_decl()
        };

        let grab_attrs = self.extractor();

        let declare_errors = self.base.declare_errors();
        let require_fields = self.base.require_fields();
        let check_errors = self.base.check_errors();

        self.wrap(
            quote! {
                fn from_derive_input(#input: &::syn::DeriveInput) -> ::darling::Result<Self> {
                    #declare_errors

                    #grab_attrs

                    #supports

                    #require_fields

                    #check_errors

                    #default

                    ::darling::export::Ok(#ty_ident {
                        #passed_ident
                        #passed_generics
                        #passed_vis
                        #passed_attrs
                        #passed_body
                        #inits
                    }) #map
                }
            },
            tokens,
        );
    }
}

impl<'a> ExtractAttribute for FromDeriveInputImpl<'a> {
    fn attr_names(&self) -> &PathList {
        &self.attr_names
    }

    fn forwarded_attrs(&self) -> Option<&ForwardAttrs> {
        self.forward_attrs
    }

    fn param_name(&self) -> TokenStream {
        quote!(__di)
    }

    fn core_loop(&self) -> TokenStream {
        self.base.core_loop()
    }

    fn local_declarations(&self) -> TokenStream {
        self.base.local_declarations()
    }

    fn immutable_declarations(&self) -> TokenStream {
        self.base.immutable_declarations()
    }
}

impl<'a> OuterFromImpl<'a> for FromDeriveInputImpl<'a> {
    fn trait_path(&self) -> syn::Path {
        path!(::darling::FromDeriveInput)
    }

    fn trait_bound(&self) -> syn::Path {
        path!(::darling::FromMeta)
    }

    fn base(&'a self) -> &'a TraitImpl<'a> {
        &self.base
    }
}
