use proc_macro2::{Ident, Span, TokenStream};
use quote::quote;
use syn::{
    parse::{Parse, ParseStream},
    punctuated::Punctuated,
    Data, DeriveInput, Index, Token, Variant,
};
use uniffi_meta::{ErrorMetadata, VariantMetadata};

use crate::{
    enum_::{enum_ffi_converter_impl, variant_metadata},
    util::{
        assert_type_eq, chain, create_metadata_static_var, either_attribute_arg, AttributeSliceExt,
        UniffiAttribute,
    },
};

pub fn expand_error(input: DeriveInput, module_path: Vec<String>) -> TokenStream {
    let variants = match input.data {
        Data::Enum(e) => Ok(e.variants),
        _ => Err(syn::Error::new(
            Span::call_site(),
            "This derive currently only supports enums",
        )),
    };

    let ident = &input.ident;
    let attr = input.attrs.parse_uniffi_attributes::<ErrorAttr>();
    let ffi_converter_impl = match &attr {
        Ok(a) if a.flat.is_some() => flat_error_ffi_converter_impl(variants.as_ref().ok(), ident),
        _ => enum_ffi_converter_impl(variants.as_ref().ok(), ident),
    };

    let meta_static_var = match (&variants, &attr) {
        (Ok(vs), Ok(a)) => Some(match error_metadata(ident, vs, module_path, a) {
            Ok(metadata) => create_metadata_static_var(ident, metadata.into()),
            Err(e) => e.into_compile_error(),
        }),
        _ => None,
    };

    let type_assertion = assert_type_eq(ident, quote! { crate::uniffi_types::#ident });
    let variant_errors: TokenStream = match variants {
        Ok(vs) => vs
            .iter()
            .flat_map(|variant| {
                chain(
                    variant.attrs.attributes_not_allowed_here(),
                    variant
                        .fields
                        .iter()
                        .flat_map(|field| field.attrs.attributes_not_allowed_here()),
                )
            })
            .map(syn::Error::into_compile_error)
            .collect(),
        Err(e) => e.into_compile_error(),
    };
    let attr_error = attr.err().map(syn::Error::into_compile_error);

    quote! {
        #ffi_converter_impl

        #[automatically_derived]
        impl ::uniffi::FfiError for #ident {}

        #meta_static_var
        #type_assertion
        #variant_errors
        #attr_error
    }
}

pub(crate) fn flat_error_ffi_converter_impl(
    variants: Option<&Punctuated<Variant, Token![,]>>,
    ident: &Ident,
) -> TokenStream {
    let write_impl = match variants {
        Some(variants) => {
            let write_match_arms = variants.iter().enumerate().map(|(i, v)| {
                let v_ident = &v.ident;
                let idx = Index::from(i + 1);

                quote! {
                    Self::#v_ident { .. } => {
                        ::uniffi::deps::bytes::BufMut::put_i32(buf, #idx);
                        <::std::string::String as ::uniffi::FfiConverter>::write(error_msg, buf);
                    }
                }
            });
            let write_impl = quote! {
                let error_msg = ::std::string::ToString::to_string(&obj);
                match obj { #(#write_match_arms)* }
            };

            write_impl
        }
        None => quote! { ::std::unimplemented!() },
    };

    quote! {
        #[automatically_derived]
        impl ::uniffi::RustBufferFfiConverter for #ident {
            type RustType = Self;

            fn write(obj: Self, buf: &mut ::std::vec::Vec<u8>) {
                #write_impl
            }

            fn try_read(buf: &mut &[::std::primitive::u8]) -> ::uniffi::deps::anyhow::Result<Self> {
                ::std::panic!("try_read not supported for flat errors");
            }
        }
    }
}

fn error_metadata(
    ident: &Ident,
    variants: &Punctuated<Variant, Token![,]>,
    module_path: Vec<String>,
    attr: &ErrorAttr,
) -> syn::Result<ErrorMetadata> {
    let name = ident.to_string();
    let flat = attr.flat.is_some();
    let variants = if flat {
        variants
            .iter()
            .map(|v| VariantMetadata {
                name: v.ident.to_string(),
                fields: vec![],
            })
            .collect()
    } else {
        variants
            .iter()
            .map(variant_metadata)
            .collect::<syn::Result<_>>()?
    };

    Ok(ErrorMetadata {
        module_path,
        name,
        variants,
        flat,
    })
}

mod kw {
    syn::custom_keyword!(flat_error);
}

#[derive(Default)]
struct ErrorAttr {
    flat: Option<kw::flat_error>,
}

impl Parse for ErrorAttr {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        let flat = input.parse()?;
        Ok(ErrorAttr { flat })
    }
}

impl UniffiAttribute for ErrorAttr {
    fn merge(self, other: Self) -> syn::Result<Self> {
        Ok(Self {
            flat: either_attribute_arg(self.flat, other.flat)?,
        })
    }
}
