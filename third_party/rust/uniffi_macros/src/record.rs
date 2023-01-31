use proc_macro2::{Ident, Span, TokenStream};
use quote::quote;
use syn::{Data, DeriveInput, Field, Fields};
use uniffi_meta::{FieldMetadata, RecordMetadata};

use crate::{
    export::metadata::convert::convert_type,
    util::{assert_type_eq, create_metadata_static_var, try_read_field},
};

pub fn expand_record(input: DeriveInput, module_path: Vec<String>) -> TokenStream {
    let fields = match input.data {
        Data::Struct(s) => Some(s.fields),
        _ => None,
    };

    let ident = &input.ident;

    let (write_impl, try_read_fields) = match &fields {
        Some(fields) => (
            fields.iter().map(write_field).collect(),
            fields.iter().map(try_read_field).collect(),
        ),
        None => {
            let unimplemented = quote! { ::std::unimplemented!() };
            (unimplemented.clone(), unimplemented)
        }
    };

    let meta_static_var = if let Some(fields) = fields {
        match record_metadata(ident, fields, module_path) {
            Ok(metadata) => create_metadata_static_var(ident, metadata.into()),
            Err(e) => e.into_compile_error(),
        }
    } else {
        syn::Error::new(
            Span::call_site(),
            "This derive must only be used on structs",
        )
        .into_compile_error()
    };

    let type_assertion = assert_type_eq(ident, quote! { crate::uniffi_types::#ident });

    quote! {
        #[automatically_derived]
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

fn record_metadata(
    ident: &Ident,
    fields: Fields,
    module_path: Vec<String>,
) -> syn::Result<RecordMetadata> {
    let name = ident.to_string();
    let fields = match fields {
        Fields::Named(fields) => fields.named,
        _ => {
            return Err(syn::Error::new(
                Span::call_site(),
                "UniFFI only supports structs with named fields",
            ));
        }
    };

    let fields = fields
        .iter()
        .map(field_metadata)
        .collect::<syn::Result<_>>()?;

    Ok(RecordMetadata {
        module_path,
        name,
        fields,
    })
}

fn field_metadata(f: &Field) -> syn::Result<FieldMetadata> {
    let name = f.ident.as_ref().unwrap().to_string();

    Ok(FieldMetadata {
        name,
        ty: convert_type(&f.ty)?,
    })
}

fn write_field(f: &Field) -> TokenStream {
    let ident = &f.ident;
    let ty = &f.ty;

    quote! {
        <#ty as ::uniffi::FfiConverter>::write(obj.#ident, buf);
    }
}
