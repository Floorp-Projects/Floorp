use proc_macro2::TokenStream;
use quote::ToTokens;
use syn;

use ast::{Data, Fields, Style};
use codegen::{Field, OuterFromImpl, TraitImpl, Variant};

pub struct FromMetaImpl<'a> {
    pub base: TraitImpl<'a>,
}

impl<'a> ToTokens for FromMetaImpl<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let base = &self.base;

        let impl_block = match base.data {
            // Unit structs allow empty bodies only.
            Data::Struct(ref vd) if vd.style.is_unit() => {
                let ty_ident = base.ident;
                quote!(
                    fn from_word() -> ::darling::Result<Self> {
                        ::darling::export::Ok(#ty_ident)
                    }
                )
            }

            // Newtype structs proxy to the sole value they contain.
            Data::Struct(Fields {
                ref fields,
                style: Style::Tuple,
                ..
            }) if fields.len() == 1 => {
                let ty_ident = base.ident;
                quote!(
                    fn from_meta(__item: &::syn::Meta) -> ::darling::Result<Self> {
                        ::darling::FromMeta::from_meta(__item)
                            .map_err(|e| e.with_span(&__item))
                            .map(#ty_ident)
                    }
                )
            }
            Data::Struct(Fields {
                style: Style::Tuple,
                ..
            }) => {
                panic!("Multi-field tuples are not supported");
            }
            Data::Struct(ref data) => {
                let inits = data.fields.iter().map(Field::as_initializer);
                let declare_errors = base.declare_errors();
                let require_fields = base.require_fields();
                let check_errors = base.check_errors();
                let decls = base.local_declarations();
                let core_loop = base.core_loop();
                let default = base.fallback_decl();
                let map = base.map_fn();

                quote!(
                    fn from_list(__items: &[::syn::NestedMeta]) -> ::darling::Result<Self> {

                        #decls

                        #declare_errors

                        #core_loop

                        #require_fields

                        #check_errors

                        #default

                        ::darling::export::Ok(Self {
                            #(#inits),*
                        }) #map
                    }
                )
            }
            Data::Enum(ref variants) => {
                let unit_arms = variants.iter().map(Variant::as_unit_match_arm);
                let struct_arms = variants.iter().map(Variant::as_data_match_arm);

                let unknown_variant_err = if !variants.is_empty() {
                    let names = variants.iter().map(Variant::as_name);
                    quote! {
                        unknown_field_with_alts(__other, &[#(#names),*])
                    }
                } else {
                    quote! {
                        unknown_field(__other)
                    }
                };

                quote!(
                    fn from_list(__outer: &[::syn::NestedMeta]) -> ::darling::Result<Self> {
                        // An enum must have exactly one value inside the parentheses if it's not a unit
                        // match arm
                        match __outer.len() {
                            0 => ::darling::export::Err(::darling::Error::too_few_items(1)),
                            1 => {
                                if let ::syn::NestedMeta::Meta(ref __nested) = __outer[0] {
                                    match __nested.path().segments.iter().map(|s| s.ident.to_string()).collect::<Vec<String>>().join("::").as_ref() {
                                        #(#struct_arms)*
                                        __other => ::darling::export::Err(::darling::Error::#unknown_variant_err.with_span(__nested))
                                    }
                                } else {
                                    ::darling::export::Err(::darling::Error::unsupported_format("literal"))
                                }
                            }
                            _ => ::darling::export::Err(::darling::Error::too_many_items(1)),
                        }
                    }

                    fn from_string(lit: &str) -> ::darling::Result<Self> {
                        match lit {
                            #(#unit_arms)*
                            __other => ::darling::export::Err(::darling::Error::unknown_value(__other))
                        }
                    }
                )
            }
        };

        self.wrap(impl_block, tokens);
    }
}

impl<'a> OuterFromImpl<'a> for FromMetaImpl<'a> {
    fn trait_path(&self) -> syn::Path {
        path!(::darling::FromMeta)
    }

    fn base(&'a self) -> &'a TraitImpl<'a> {
        &self.base
    }
}
