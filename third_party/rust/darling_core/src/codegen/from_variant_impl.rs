use quote::{Tokens, ToTokens};
use syn::{self, Ident};

use codegen::{ExtractAttribute, OuterFromImpl, TraitImpl};
use options::{DataShape, ForwardAttrs};

pub struct FromVariantImpl<'a> {
    pub base: TraitImpl<'a>,
    pub ident: Option<&'a Ident>,
    pub data: Option<&'a Ident>,
    pub attrs: Option<&'a Ident>,
    pub attr_names: Vec<&'a str>,
    pub forward_attrs: Option<&'a ForwardAttrs>,
    pub from_ident: Option<bool>,
    pub supports: Option<&'a DataShape>,
}

impl<'a> ToTokens for FromVariantImpl<'a> {
    fn to_tokens(&self, tokens: &mut Tokens) {
        let input = self.param_name();
        let extractor = self.extractor();
        let passed_ident = self.ident.as_ref().map(|i| quote!(#i: #input.ident.clone(),));
        let passed_attrs = self.attrs.as_ref().map(|i| quote!(#i: __fwd_attrs,));
        let passed_data = self.data.as_ref().map(|i| quote!(#i: ::darling::ast::VariantData::try_from(&#input.data)?,));

        let inits = self.base.initializers();
        let map = self.base.map_fn();

        let default = if let Some(true) = self.from_ident {
            quote!(let __default: Self = ::darling::export::From::from(#input.ident.clone());)
        } else {
            self.base.fallback_decl()
        };

        let supports = self.supports.map(|i| quote! {
            #i
            __validate_data(&#input.data)?;
        });

        let error_declaration = self.base.declare_errors();
        let require_fields = self.base.require_fields();
        let error_check = self.base.check_errors();

        self.wrap(quote!(
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
                    #passed_data
                    #inits
                }) #map
            }
        ), tokens);
    }
}

impl<'a> ExtractAttribute for FromVariantImpl<'a> {
    fn local_declarations(&self) -> Tokens {
        self.base.local_declarations()
    }

    fn immutable_declarations(&self) -> Tokens {
        self.base.immutable_declarations()
    }

    fn attr_names(&self) -> &[&str] {
        self.attr_names.as_slice()
    }

    fn forwarded_attrs(&self) -> Option<&ForwardAttrs> {
        self.forward_attrs
    }

    fn param_name(&self) -> Tokens {
        quote!(__variant)
    }

    fn core_loop(&self) -> Tokens {
        self.base.core_loop()
    }
}


impl<'a> OuterFromImpl<'a> for FromVariantImpl<'a> {
    fn trait_path(&self) -> syn::Path {
        path!(::darling::FromVariant)
    }

    fn trait_bound(&self) -> syn::Path {
        path!(::darling::FromMetaItem)
    }

    fn base(&'a self) -> &'a TraitImpl<'a> {
        &self.base
    }
}