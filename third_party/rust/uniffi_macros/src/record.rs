use proc_macro2::{Ident, Span, TokenStream};
use quote::quote;
use syn::{parse::ParseStream, Data, DataStruct, DeriveInput, Field, Token};

use crate::{
    default::{default_value_metadata_calls, DefaultValue},
    util::{
        create_metadata_items, derive_all_ffi_traits, either_attribute_arg, extract_docstring,
        ident_to_string, kw, mod_path, tagged_impl_header, try_metadata_value_from_usize,
        try_read_field, AttributeSliceExt, UniffiAttributeArgs,
    },
};

pub fn expand_record(input: DeriveInput, udl_mode: bool) -> syn::Result<TokenStream> {
    if let Some(e) = input.attrs.uniffi_attr_args_not_allowed_here() {
        return Err(e);
    }
    let record = match input.data {
        Data::Struct(s) => s,
        _ => {
            return Err(syn::Error::new(
                Span::call_site(),
                "This derive must only be used on structs",
            ));
        }
    };

    let ident = &input.ident;
    let docstring = extract_docstring(&input.attrs)?;
    let ffi_converter = record_ffi_converter_impl(ident, &record, udl_mode)
        .unwrap_or_else(syn::Error::into_compile_error);
    let meta_static_var = (!udl_mode).then(|| {
        record_meta_static_var(ident, docstring, &record)
            .unwrap_or_else(syn::Error::into_compile_error)
    });

    Ok(quote! {
        #ffi_converter
        #meta_static_var
    })
}

pub(crate) fn record_ffi_converter_impl(
    ident: &Ident,
    record: &DataStruct,
    udl_mode: bool,
) -> syn::Result<TokenStream> {
    let impl_spec = tagged_impl_header("FfiConverter", ident, udl_mode);
    let derive_ffi_traits = derive_all_ffi_traits(ident, udl_mode);
    let name = ident_to_string(ident);
    let mod_path = mod_path()?;
    let write_impl: TokenStream = record.fields.iter().map(write_field).collect();
    let try_read_fields: TokenStream = record.fields.iter().map(try_read_field).collect();

    Ok(quote! {
        #[automatically_derived]
        unsafe #impl_spec {
            ::uniffi::ffi_converter_rust_buffer_lift_and_lower!(crate::UniFfiTag);

            fn write(obj: Self, buf: &mut ::std::vec::Vec<u8>) {
                #write_impl
            }

            fn try_read(buf: &mut &[::std::primitive::u8]) -> ::uniffi::deps::anyhow::Result<Self> {
                Ok(Self { #try_read_fields })
            }

            const TYPE_ID_META: ::uniffi::MetadataBuffer = ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::TYPE_RECORD)
                .concat_str(#mod_path)
                .concat_str(#name);
        }

        #derive_ffi_traits
    })
}

fn write_field(f: &Field) -> TokenStream {
    let ident = &f.ident;
    let ty = &f.ty;

    quote! {
        <#ty as ::uniffi::Lower<crate::UniFfiTag>>::write(obj.#ident, buf);
    }
}

#[derive(Default)]
pub struct FieldAttributeArguments {
    pub(crate) default: Option<DefaultValue>,
}

impl UniffiAttributeArgs for FieldAttributeArguments {
    fn parse_one(input: ParseStream<'_>) -> syn::Result<Self> {
        let _: kw::default = input.parse()?;
        let _: Token![=] = input.parse()?;
        let default = input.parse()?;
        Ok(Self {
            default: Some(default),
        })
    }

    fn merge(self, other: Self) -> syn::Result<Self> {
        Ok(Self {
            default: either_attribute_arg(self.default, other.default)?,
        })
    }
}

pub(crate) fn record_meta_static_var(
    ident: &Ident,
    docstring: String,
    record: &DataStruct,
) -> syn::Result<TokenStream> {
    let name = ident_to_string(ident);
    let module_path = mod_path()?;
    let fields_len =
        try_metadata_value_from_usize(record.fields.len(), "UniFFI limits structs to 256 fields")?;

    let concat_fields: TokenStream = record
        .fields
        .iter()
        .map(|f| {
            let attrs = f
                .attrs
                .parse_uniffi_attr_args::<FieldAttributeArguments>()?;

            let name = ident_to_string(f.ident.as_ref().unwrap());
            let docstring = extract_docstring(&f.attrs)?;
            let ty = &f.ty;
            let default = default_value_metadata_calls(&attrs.default)?;

            // Note: fields need to implement both `Lower` and `Lift` to be used in a record.  The
            // TYPE_ID_META should be the same for both traits.
            Ok(quote! {
                .concat_str(#name)
                .concat(<#ty as ::uniffi::Lower<crate::UniFfiTag>>::TYPE_ID_META)
                #default
                .concat_long_str(#docstring)
            })
        })
        .collect::<syn::Result<_>>()?;

    Ok(create_metadata_items(
        "record",
        &name,
        quote! {
            ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::RECORD)
                .concat_str(#module_path)
                .concat_str(#name)
                .concat_value(#fields_len)
                #concat_fields
                .concat_long_str(#docstring)
        },
        None,
    ))
}
