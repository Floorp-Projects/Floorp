/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::util::{derive_all_ffi_traits, ident_to_string, mod_path, tagged_impl_header};
use proc_macro2::{Ident, TokenStream};
use quote::quote;
use syn::Path;

// Generate an FfiConverter impl based on the UniffiCustomTypeConverter
// implementation that the library supplies
pub(crate) fn expand_ffi_converter_custom_type(
    ident: &Ident,
    builtin: &Path,
    udl_mode: bool,
) -> syn::Result<TokenStream> {
    let impl_spec = tagged_impl_header("FfiConverter", ident, udl_mode);
    let derive_ffi_traits = derive_all_ffi_traits(ident, udl_mode);
    let name = ident_to_string(ident);
    let mod_path = mod_path()?;

    Ok(quote! {
        #[automatically_derived]
        unsafe #impl_spec {
            // Note: the builtin type needs to implement both `Lower` and `Lift'.  We use the
            // `Lower` trait to get the associated type `FfiType` and const `TYPE_ID_META`.  These
            // can't differ between `Lower` and `Lift`.
            type FfiType = <#builtin as ::uniffi::Lower<crate::UniFfiTag>>::FfiType;
            fn lower(obj: #ident ) -> Self::FfiType {
                <#builtin as ::uniffi::Lower<crate::UniFfiTag>>::lower(<#ident as crate::UniffiCustomTypeConverter>::from_custom(obj))
            }

            fn try_lift(v: Self::FfiType) -> uniffi::Result<#ident> {
                <#ident as crate::UniffiCustomTypeConverter>::into_custom(<#builtin as ::uniffi::Lift<crate::UniFfiTag>>::try_lift(v)?)
            }

            fn write(obj: #ident, buf: &mut Vec<u8>) {
                <#builtin as ::uniffi::Lower<crate::UniFfiTag>>::write(<#ident as crate::UniffiCustomTypeConverter>::from_custom(obj), buf);
            }

            fn try_read(buf: &mut &[u8]) -> uniffi::Result<#ident> {
                <#ident as crate::UniffiCustomTypeConverter>::into_custom(<#builtin as ::uniffi::Lift<crate::UniFfiTag>>::try_read(buf)?)
            }

            const TYPE_ID_META: ::uniffi::MetadataBuffer = ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::TYPE_CUSTOM)
                .concat_str(#mod_path)
                .concat_str(#name)
                .concat(<#builtin as ::uniffi::Lower<crate::UniFfiTag>>::TYPE_ID_META);
        }

        #derive_ffi_traits
    })
}

// Generate an FfiConverter impl *and* an UniffiCustomTypeConverter.
pub(crate) fn expand_ffi_converter_custom_newtype(
    ident: &Ident,
    builtin: &Path,
    udl_mode: bool,
) -> syn::Result<TokenStream> {
    let ffi_converter = expand_ffi_converter_custom_type(ident, builtin, udl_mode)?;
    let type_converter = custom_ffi_type_converter(ident, builtin)?;

    Ok(quote! {
        #ffi_converter

        #type_converter
    })
}

fn custom_ffi_type_converter(ident: &Ident, builtin: &Path) -> syn::Result<TokenStream> {
    Ok(quote! {
        impl crate::UniffiCustomTypeConverter for #ident {
            type Builtin = #builtin;

            fn into_custom(val: Self::Builtin) -> uniffi::Result<Self> {
                Ok(#ident(val))
            }

            fn from_custom(obj: Self) -> Self::Builtin {
                obj.0
            }
        }
    })
}
