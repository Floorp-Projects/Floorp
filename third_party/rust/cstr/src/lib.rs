//! A macro for getting `&'static CStr` from literal or identifier.
//!
//! This macro checks whether the given literal is valid for `CStr`
//! at compile time, and returns a static reference of `CStr`.
//!
//! This macro can be used to to initialize constants on Rust 1.46 and above.
//!
//! ## Example
//!
//! ```
//! use cstr::cstr;
//! use std::ffi::CStr;
//!
//! let test = cstr!(b"hello\xff");
//! assert_eq!(test, CStr::from_bytes_with_nul(b"hello\xff\0").unwrap());
//! let test = cstr!("hello");
//! assert_eq!(test, CStr::from_bytes_with_nul(b"hello\0").unwrap());
//! let test = cstr!(hello);
//! assert_eq!(test, CStr::from_bytes_with_nul(b"hello\0").unwrap());
//! ```

// While this isn't necessary when using Cargo >= 1.42, omitting it actually requires path-less
// `--extern proc_macro` to be passed to `rustc` when building this crate. Some tools may not do
// this correctly. So it's added as a precaution.
extern crate proc_macro;

use crate::parse::parse_input;
use proc_macro::TokenStream as RawTokenStream;
use proc_macro2::{Literal, Span, TokenStream};
use quote::{quote, quote_spanned};
use std::ffi::CString;

mod parse;

struct Error(Span, &'static str);

#[proc_macro]
pub fn cstr(input: RawTokenStream) -> RawTokenStream {
    let tokens = match build_byte_str(input.into()) {
        // We can't use `&*ptr` to convert the raw pointer to reference, because as of Rust 1.46,
        // dereferencing raw pointer in constants is unstable.
        // This is being tracked in https://github.com/rust-lang/rust/issues/51911
        // So we explicitly disable the clippy lint for this expression.
        Ok(s) => quote!(unsafe {
            #[allow(clippy::transmute_ptr_to_ref)]
            ::std::mem::transmute::<_, &::std::ffi::CStr>(
                #s as *const [u8] as *const ::std::ffi::CStr
            )
        }),
        Err(Error(span, msg)) => quote_spanned!(span => compile_error!(#msg)),
    };
    tokens.into()
}

fn build_byte_str(input: TokenStream) -> Result<Literal, Error> {
    let (bytes, span) = parse_input(input)?;
    match CString::new(bytes) {
        Ok(s) => {
            let mut lit = Literal::byte_string(s.as_bytes_with_nul());
            lit.set_span(span);
            Ok(lit)
        }
        Err(_) => Err(Error(span, "nul byte found in the literal")),
    }
}
