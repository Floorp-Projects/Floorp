use proc_macro2::{Span, TokenStream};
use quote::quote;
use syn::{Data, DeriveInput};
use uniffi_meta::{FieldMetadata, RecordMetadata};

use crate::{
    export::metadata::convert::convert_type,
    util::{assert_type_eq, create_metadata_static_var},
};

pub fn expand_record(input: DeriveInput, module_path: Vec<String>) -> TokenStream {
    let fields = match input.data {
        Data::Struct(s) => Some(s.fields),
        _ => None,
    };

    let ident = &input.ident;

    let (write_impl, try_read_fields) = match &fields {
        Some(fields) => fields
            .iter()
            .map(|f| {
                let ident = &f.ident;
                let ty = &f.ty;

                let write_field = quote! {
                    <#ty as ::uniffi::FfiConverter>::write(obj.#ident, buf);
                };
                let try_read_field = quote! {
                    #ident: <#ty as ::uniffi::FfiConverter>::try_read(buf)?,
                };

                (write_field, try_read_field)
            })
            .unzip(),
        None => {
            let unimplemented = quote! { ::std::unimplemented!() };
            (unimplemented.clone(), unimplemented)
        }
    };

    let meta_static_var = fields
        .map(|fields| {
            let name = ident.to_string();
            let fields_res: syn::Result<_> = fields
                .iter()
                .map(|f| {
                    let name = f
                        .ident
                        .as_ref()
                        .expect("We only allow record structs")
                        .to_string();

                    Ok(FieldMetadata {
                        name,
                        ty: convert_type(&f.ty)?,
                    })
                })
                .collect();

            match fields_res {
                Ok(fields) => {
                    let metadata = RecordMetadata {
                        module_path,
                        name,
                        fields,
                    };

                    create_metadata_static_var(ident, metadata.into())
                }
                Err(e) => e.into_compile_error(),
            }
        })
        .unwrap_or_else(|| {
            syn::Error::new(
                Span::call_site(),
                "This derive must only be used on structs",
            )
            .into_compile_error()
        });

    let type_assertion = assert_type_eq(ident, quote! { crate::uniffi_types::#ident });

    quote! {
        impl ::uniffi::RustBufferFfiConverter for #ident {
            type RustType = Self;

            fn write(obj: Self, buf: &mut ::std::vec::Vec<u8>) {
                #write_impl
            }

            fn try_read(buf: &mut &[::std::primitive::u8]) -> ::uniffi::deps::anyhow::Result<Self> {
                Ok(Self { #try_read_fields })
            }
        }

        #meta_static_var
        #type_assertion
    }
}
