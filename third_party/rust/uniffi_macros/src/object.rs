use proc_macro2::{Ident, Span, TokenStream};
use quote::quote;
use syn::DeriveInput;
use uniffi_meta::ObjectMetadata;

use crate::util::{assert_type_eq, create_metadata_static_var};

pub fn expand_object(input: DeriveInput, module_path: Vec<String>) -> TokenStream {
    let ident = &input.ident;
    let name = ident.to_string();
    let metadata = ObjectMetadata { module_path, name };
    let free_fn_ident = Ident::new(&metadata.free_ffi_symbol_name(), Span::call_site());
    let meta_static_var = create_metadata_static_var(ident, metadata.into());
    let type_assertion = assert_type_eq(ident, quote! { crate::uniffi_types::#ident });

    quote! {
        #[doc(hidden)]
        #[no_mangle]
        pub extern "C" fn #free_fn_ident(
            ptr: *const ::std::ffi::c_void,
            call_status: &mut ::uniffi::RustCallStatus
        ) {
            uniffi::call_with_output(call_status, || {
                assert!(!ptr.is_null());
                let ptr = ptr.cast::<#ident>();
                unsafe {
                    ::std::sync::Arc::decrement_strong_count(ptr);
                }
            });
        }

        #meta_static_var
        #type_assertion
    }
}
