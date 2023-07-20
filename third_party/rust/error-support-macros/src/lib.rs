/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use proc_macro::TokenStream;
use quote::quote;
use syn::{parse_quote, spanned::Spanned};

mod argument;

/// A procedural macro that exposes internal errors to external errors the
/// consuming applications should handle. It requires that the internal error
/// implements [`error_support::ErrorHandling`].
///
/// Additionally, this procedural macro has side effects, including:
/// * It would log the error based on a pre-defined log level. The log level is defined
///  in the [`error_support::ErrorHandling`] implementation.
/// * It would report some errors using an external error reporter, in practice, this
///   is implemented using Sentry in the app.
///
/// # Example
/// ```ignore
/// use error_support::{handle_error, GetErrorHandling, ErrorHandling};
/// use std::fmt::Display
///#[derive(Debug, thiserror::Error)]
/// struct Error {}
/// type Result<T, E = Error> = std::result::Result<T, E>;

/// impl Display for Error {
///     fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
///         write!(f, "Internal Error!")
///     }
/// }
///
/// #[derive(Debug, thiserror::Error)]
/// struct ExternalError {}
///
/// impl Display for ExternalError {
///     fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
///         write!(f, "External Error!")
///     }
/// }
///
/// impl GetErrorHandling for Error {
///    type ExternalError = ExternalError;
///
///    fn get_error_handling(&self) -> ErrorHandling<Self::ExternalError> {
///        ErrorHandling::convert(ExternalError {})
///    }
/// }
///
/// // The `handle_error` macro maps from the error supplied in the mandatory argument
/// // (ie, `Error` in this example) to the error returned by the function (`ExternalError`
/// // in this example)
/// #[handle_error(Error)]
/// fn do_something() -> std::result::Result<String, ExternalError> {
///    Err(Error{})
/// }
///
/// // The error here is an `ExternalError`
/// let _: ExternalError = do_something().unwrap_err();
/// ```
#[proc_macro_attribute]
pub fn handle_error(args: TokenStream, input: TokenStream) -> TokenStream {
    let args = syn::parse_macro_input!(args as syn::AttributeArgs);
    let parsed = syn::parse_macro_input!(input as syn::Item);
    TokenStream::from(match impl_handle_error(&parsed, &args) {
        Ok(res) => res,
        Err(e) => e.to_compile_error(),
    })
}

fn impl_handle_error(
    input: &syn::Item,
    arguments: &syn::AttributeArgs,
) -> syn::Result<proc_macro2::TokenStream> {
    if let syn::Item::Fn(item_fn) = input {
        let err_path = argument::validate(arguments)?;
        let original_body = &item_fn.block;

        let mut new_fn = item_fn.clone();
        new_fn.block = parse_quote! {
            {
                (|| -> ::std::result::Result<_, #err_path> {
                    #original_body
                })().map_err(::error_support::convert_log_report_error)
            }
        };

        Ok(quote! {
            #new_fn
        })
    } else {
        Err(syn::Error::new(
            input.span(),
            "#[handle_error(..)] can only be used on functions",
        ))
    }
}
