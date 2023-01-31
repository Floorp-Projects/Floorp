use proc_macro2::{Ident, Span, TokenStream};
use quote::quote;
use syn::{punctuated::Punctuated, Data, DeriveInput, Field, Index, Token, Variant};
use uniffi_meta::{EnumMetadata, FieldMetadata, VariantMetadata};

use crate::{
    export::metadata::convert::convert_type,
    util::{assert_type_eq, create_metadata_static_var, try_read_field},
};

pub fn expand_enum(input: DeriveInput, module_path: Vec<String>) -> TokenStream {
    let variants = match input.data {
        Data::Enum(e) => Some(e.variants),
        _ => None,
    };

    let ident = &input.ident;

    let ffi_converter_impl = enum_ffi_converter_impl(variants.as_ref(), ident);

    let meta_static_var = if let Some(variants) = variants {
        match enum_metadata(ident, variants, module_path) {
            Ok(metadata) => create_metadata_static_var(ident, metadata.into()),
            Err(e) => e.into_compile_error(),
        }
    } else {
        syn::Error::new(Span::call_site(), "This derive must only be used on enums")
            .into_compile_error()
    };

    let type_assertion = assert_type_eq(ident, quote! { crate::uniffi_types::#ident });

    quote! {
        #ffi_converter_impl
        #meta_static_var
        #type_assertion
    }
}

pub(crate) fn enum_ffi_converter_impl(
    variants: Option<&Punctuated<Variant, Token![,]>>,
    ident: &Ident,
) -> TokenStream {
    let (write_impl, try_read_impl) = match variants {
        Some(variants) => {
            let write_match_arms = variants.iter().enumerate().map(|(i, v)| {
                let v_ident = &v.ident;
                let fields = v.fields.iter().map(|f| &f.ident);
                let idx = Index::from(i + 1);
                let write_fields = v.fields.iter().map(write_field);

                quote! {
                    Self::#v_ident { #(#fields),* } => {
                        ::uniffi::deps::bytes::BufMut::put_i32(buf, #idx);
                        #(#write_fields)*
                    }
                }
            });
            let write_impl = quote! {
                match obj { #(#write_match_arms)* }
            };

            let try_read_match_arms = variants.iter().enumerate().map(|(i, v)| {
                let idx = Index::from(i + 1);
                let v_ident = &v.ident;
                let try_read_fields = v.fields.iter().map(try_read_field);

                quote! {
                    #idx => Self::#v_ident { #(#try_read_fields)* },
                }
            });
            let error_format_string = format!("Invalid {ident} enum value: {{}}");
            let try_read_impl = quote! {
                ::uniffi::check_remaining(buf, 4)?;

                Ok(match ::uniffi::deps::bytes::Buf::get_i32(buf) {
                    #(#try_read_match_arms)*
                    v => ::uniffi::deps::anyhow::bail!(#error_format_string, v),
                })
            };

            (write_impl, try_read_impl)
        }
        None => {
            let unimplemented = quote! { ::std::unimplemented!() };
            (unimplemented.clone(), unimplemented)
        }
    };

    quote! {
        #[automatically_derived]
        impl ::uniffi::RustBufferFfiConverter for #ident {
            type RustType = Self;

            fn write(obj: Self, buf: &mut ::std::vec::Vec<u8>) {
                #write_impl
            }

            fn try_read(buf: &mut &[::std::primitive::u8]) -> ::uniffi::deps::anyhow::Result<Self> {
                #try_read_impl
            }
        }
    }
}

fn enum_metadata(
    ident: &Ident,
    variants: Punctuated<Variant, Token![,]>,
    module_path: Vec<String>,
) -> syn::Result<EnumMetadata> {
    let name = ident.to_string();
    let variants = variants
        .iter()
        .map(variant_metadata)
        .collect::<syn::Result<_>>()?;

    Ok(EnumMetadata {
        module_path,
        name,
        variants,
    })
}

pub(crate) fn variant_metadata(v: &Variant) -> syn::Result<VariantMetadata> {
    let name = v.ident.to_string();
    let fields = v
        .fields
        .iter()
        .map(|f| field_metadata(f, v))
        .collect::<syn::Result<_>>()?;

    Ok(VariantMetadata { name, fields })
}

fn field_metadata(f: &Field, v: &Variant) -> syn::Result<FieldMetadata> {
    let name = f
        .ident
        .as_ref()
        .ok_or_else(|| {
            syn::Error::new_spanned(
                v,
                "UniFFI only supports enum variants with named fields (or no fields at all)",
            )
        })?
        .to_string();

    Ok(FieldMetadata {
        name,
        ty: convert_type(&f.ty)?,
    })
}

fn write_field(f: &Field) -> TokenStream {
    let ident = &f.ident;
    let ty = &f.ty;

    quote! {
        <#ty as ::uniffi::FfiConverter>::write(#ident, buf);
    }
}
