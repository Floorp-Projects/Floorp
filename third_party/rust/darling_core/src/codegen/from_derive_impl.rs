use quote::{Tokens, ToTokens};
use syn::{self, Ident};

use ast::Body;
use codegen::{TraitImpl, ExtractAttribute, OuterFromImpl};
use options::{ForwardAttrs, Shape};

pub struct FromDeriveInputImpl<'a> {
    pub ident: Option<&'a Ident>,
    pub generics: Option<&'a Ident>,
    pub vis: Option<&'a Ident>,
    pub attrs: Option<&'a Ident>,
    pub body: Option<&'a Ident>,
    pub base: TraitImpl<'a>,
    pub attr_names: Vec<&'a str>,
    pub forward_attrs: Option<&'a ForwardAttrs>,
    pub from_ident: Option<bool>,
    pub supports: Option<&'a Shape>,
}

impl<'a> ToTokens for FromDeriveInputImpl<'a> {
    fn to_tokens(&self, tokens: &mut Tokens) {
        let ty_ident = self.base.ident;
        let input = self.param_name();
        let map = self.base.map_fn();
        
        if let Body::Struct(ref data) = self.base.body {
            if data.is_newtype() {
                self.wrap(quote!{
                    fn from_derive_input(#input: &::syn::DeriveInput) -> ::darling::Result<Self> {
                        ::darling::export::Ok(
                            #ty_ident(::darling::FromDeriveInput::from_derive_input(#input)?)
                        ) #map
                    }
                }, tokens);

                return;
            }
        }

        let passed_ident = self.ident.as_ref().map(|i| quote!(#i: #input.ident.clone(),));
        let passed_vis = self.vis.as_ref().map(|i| quote!(#i: #input.vis.clone(),));
        let passed_generics = self.generics.as_ref().map(|i| quote!(#i: #input.generics.clone(),));
        let passed_attrs = self.attrs.as_ref().map(|i| quote!(#i: __fwd_attrs,));
        let passed_body = self.body.as_ref().map(|i| quote!(#i: ::darling::ast::Body::try_from(&#input.body)?,));

        let supports = self.supports.map(|i| quote!{
            #i
            __validate_body(&#input.body)?;
        });

        let inits = self.base.initializers();
        let default = if let Some(true) = self.from_ident {
            quote!(let __default: Self = ::darling::export::From::from(#input.ident.clone());)
        } else {
            self.base.fallback_decl()
        };

        let grab_attrs = self.extractor();

        let declare_errors = self.base.declare_errors();
        let require_fields = self.base.require_fields();
        let check_errors = self.base.check_errors();

        self.wrap(quote! {
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
        }, tokens);
    }
}

impl<'a> ExtractAttribute for FromDeriveInputImpl<'a> {
    fn attr_names(&self) -> &[&str] {
        self.attr_names.as_slice()
    }

    fn forwarded_attrs(&self) -> Option<&ForwardAttrs> {
        self.forward_attrs
    }

    fn param_name(&self) -> Tokens {
        quote!(__di)
    }

    fn core_loop(&self) -> Tokens {
        self.base.core_loop()
    }

    fn local_declarations(&self) -> Tokens {
        self.base.local_declarations()
    }

    fn immutable_declarations(&self) -> Tokens {
        self.base.immutable_declarations()
    }
}

impl<'a> OuterFromImpl<'a> for FromDeriveInputImpl<'a> {
    fn trait_path(&self) -> syn::Path {
        path!(::darling::FromDeriveInput)
    }

    fn trait_bound(&self) -> syn::Path {
        path!(::darling::FromMetaItem)
    }

    fn base(&'a self) -> &'a TraitImpl<'a> {
        &self.base
    }
}