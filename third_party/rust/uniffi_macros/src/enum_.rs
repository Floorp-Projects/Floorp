use proc_macro2::{Ident, Span, TokenStream};
use quote::quote;
use syn::{Data, DataEnum, DeriveInput, Field, Index};

use crate::util::{
    create_metadata_items, derive_all_ffi_traits, ident_to_string, mod_path, tagged_impl_header,
    try_metadata_value_from_usize, try_read_field,
};

pub fn expand_enum(input: DeriveInput, udl_mode: bool) -> syn::Result<TokenStream> {
    let enum_ = match input.data {
        Data::Enum(e) => e,
        _ => {
            return Err(syn::Error::new(
                Span::call_site(),
                "This derive must only be used on enums",
            ))
        }
    };
    let ident = &input.ident;
    let ffi_converter_impl = enum_ffi_converter_impl(ident, &enum_, udl_mode);

    let meta_static_var = (!udl_mode).then(|| {
        enum_meta_static_var(ident, &enum_).unwrap_or_else(syn::Error::into_compile_error)
    });

    Ok(quote! {
        #ffi_converter_impl
        #meta_static_var
    })
}

pub(crate) fn enum_ffi_converter_impl(
    ident: &Ident,
    enum_: &DataEnum,
    udl_mode: bool,
) -> TokenStream {
    enum_or_error_ffi_converter_impl(
        ident,
        enum_,
        udl_mode,
        quote! { ::uniffi::metadata::codes::TYPE_ENUM },
    )
}

pub(crate) fn rich_error_ffi_converter_impl(
    ident: &Ident,
    enum_: &DataEnum,
    udl_mode: bool,
) -> TokenStream {
    enum_or_error_ffi_converter_impl(
        ident,
        enum_,
        udl_mode,
        quote! { ::uniffi::metadata::codes::TYPE_ENUM },
    )
}

fn enum_or_error_ffi_converter_impl(
    ident: &Ident,
    enum_: &DataEnum,
    udl_mode: bool,
    metadata_type_code: TokenStream,
) -> TokenStream {
    let name = ident_to_string(ident);
    let impl_spec = tagged_impl_header("FfiConverter", ident, udl_mode);
    let derive_ffi_traits = derive_all_ffi_traits(ident, udl_mode);
    let mod_path = match mod_path() {
        Ok(p) => p,
        Err(e) => return e.into_compile_error(),
    };
    let write_match_arms = enum_.variants.iter().enumerate().map(|(i, v)| {
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

    let try_read_match_arms = enum_.variants.iter().enumerate().map(|(i, v)| {
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

    quote! {
        #[automatically_derived]
        unsafe #impl_spec {
            ::uniffi::ffi_converter_rust_buffer_lift_and_lower!(crate::UniFfiTag);

            fn write(obj: Self, buf: &mut ::std::vec::Vec<u8>) {
                #write_impl
            }

            fn try_read(buf: &mut &[::std::primitive::u8]) -> ::uniffi::deps::anyhow::Result<Self> {
                #try_read_impl
            }

            const TYPE_ID_META: ::uniffi::MetadataBuffer = ::uniffi::MetadataBuffer::from_code(#metadata_type_code)
                .concat_str(#mod_path)
                .concat_str(#name);
        }

        #derive_ffi_traits
    }
}

fn write_field(f: &Field) -> TokenStream {
    let ident = &f.ident;
    let ty = &f.ty;

    quote! {
        <#ty as ::uniffi::Lower<crate::UniFfiTag>>::write(#ident, buf);
    }
}

pub(crate) fn enum_meta_static_var(ident: &Ident, enum_: &DataEnum) -> syn::Result<TokenStream> {
    let name = ident_to_string(ident);
    let module_path = mod_path()?;

    let mut metadata_expr = quote! {
        ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::ENUM)
            .concat_str(#module_path)
            .concat_str(#name)
    };
    metadata_expr.extend(variant_metadata(enum_)?);
    Ok(create_metadata_items("enum", &name, metadata_expr, None))
}

pub fn variant_metadata(enum_: &DataEnum) -> syn::Result<Vec<TokenStream>> {
    let variants_len =
        try_metadata_value_from_usize(enum_.variants.len(), "UniFFI limits enums to 256 variants")?;
    std::iter::once(Ok(quote! { .concat_value(#variants_len) }))
        .chain(
            enum_.variants
                .iter()
                .map(|v| {
                    let fields_len = try_metadata_value_from_usize(
                        v.fields.len(),
                        "UniFFI limits enum variants to 256 fields",
                    )?;

                    let field_names = v.fields
                        .iter()
                        .map(|f| {
                            f.ident
                                .as_ref()
                                .ok_or_else(||
                                    syn::Error::new_spanned(
                                        v,
                                        "UniFFI only supports enum variants with named fields (or no fields at all)",
                                    )
                                )
                                .map(ident_to_string)
                        })
                    .collect::<syn::Result<Vec<_>>>()?;

                    let name = ident_to_string(&v.ident);
                    let field_types = v.fields.iter().map(|f| &f.ty);
                    Ok(quote! {
                        .concat_str(#name)
                        .concat_value(#fields_len)
                            #(
                                .concat_str(#field_names)
                                .concat(<#field_types as ::uniffi::Lower<crate::UniFfiTag>>::TYPE_ID_META)
                                // field defaults not yet supported for enums
                                .concat_bool(false)
                            )*
                    })
                })
        )
        .collect()
}
