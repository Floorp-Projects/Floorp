// SPDX-FileCopyrightText: 2021 - 2022 Kamila Borowska <kamila@borowska.pw>
// SPDX-FileCopyrightText: 2021 Bruno Corrêa Zimmermann <brunoczim@gmail.com>
//
// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::type_length;
use proc_macro2::TokenStream;
use quote::quote;
use syn::{DataStruct, Fields, FieldsNamed, FieldsUnnamed, Ident, Index};

pub fn generate(name: Ident, data_struct: DataStruct) -> TokenStream {
    StructGenerator::from_fields(&data_struct.fields).finish(&name)
}

/// Total length is the product of each member's length. To represent a struct, one can
/// think of this as representing a little-endian number. First member is simply added, but
/// next members are multiplied before being added.
#[derive(Debug)]
struct StructGenerator {
    length: TokenStream,
    from_usize: TokenStream,
    into_usize: TokenStream,
}

impl StructGenerator {
    fn from_fields(fields: &Fields) -> Self {
        match fields {
            Fields::Unit => Self::from_unit_fields(),
            Fields::Unnamed(fields_data) => Self::from_unnamed_fields(fields_data),
            Fields::Named(fields_data) => Self::from_named_fields(fields_data),
        }
    }

    fn from_unit_fields() -> Self {
        Self {
            length: quote! { 1usize },
            from_usize: quote! { Self },
            into_usize: quote! { 0usize },
        }
    }

    fn from_unnamed_fields(fields: &FieldsUnnamed) -> Self {
        let mut params_from = quote! {};
        let mut into_usize = quote! { 0usize };
        let mut length = quote! { 1usize };
        for (i, field) in fields.unnamed.iter().enumerate() {
            let ty = &field.ty;
            let index_ident = Index::from(i);
            let field_length = type_length(ty);

            into_usize = quote! {
                (#into_usize + #length * ::enum_map::Enum::into_usize(self.#index_ident))
            };

            params_from = quote! {
                #params_from <#ty as ::enum_map::Enum>::from_usize(
                    value / #length % #field_length
                ),
            };

            length = quote! { (#length * #field_length) };
        }

        let from_usize = quote! { Self(#params_from) };
        Self {
            length,
            from_usize,
            into_usize,
        }
    }

    fn from_named_fields(fields: &FieldsNamed) -> Self {
        let mut params_from = quote! {};
        let mut into_usize = quote! { 0usize };
        let mut length = quote! { 1usize };
        for field in fields.named.iter() {
            let ty = &field.ty;
            let ident = field.ident.as_ref().unwrap();
            let field_length = type_length(ty);

            into_usize = quote! {
                (#into_usize + #length * ::enum_map::Enum::into_usize(self.#ident))
            };

            params_from = quote! {
                #params_from #ident: <#ty as ::enum_map::Enum>::from_usize(
                    value / #length % #field_length
                ),
            };

            length = quote! { (#field_length * #length) };
        }

        let from_usize = quote! { Self { #params_from } };
        Self {
            length,
            from_usize,
            into_usize,
        }
    }

    fn finish(&self, name: &Ident) -> TokenStream {
        let length = &self.length;
        let from_usize = &self.from_usize;
        let into_usize = &self.into_usize;

        quote! {
            #[automatically_derived]
            impl ::enum_map::Enum for #name {
                const LENGTH: ::enum_map::usize = #length;

                #[inline]
                fn from_usize(value: ::enum_map::usize) -> Self {
                    #from_usize
                }

                #[inline]
                fn into_usize(self) -> ::enum_map::usize {
                    #into_usize
                }
            }

            #[automatically_derived]
            impl<V> ::enum_map::EnumArray<V> for #name {
                type Array = [V; #length];
            }
        }
    }
}
