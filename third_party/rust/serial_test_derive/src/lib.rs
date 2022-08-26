//! # serial_test_derive
//! Helper crate for [serial_test](../serial_test/index.html)

#![cfg_attr(docsrs, feature(doc_cfg))]

extern crate proc_macro;

use proc_macro::TokenStream;
use proc_macro2::TokenTree;
use proc_macro_error::{abort_call_site, proc_macro_error};
use quote::{format_ident, quote, ToTokens, TokenStreamExt};
use std::ops::Deref;

/// Allows for the creation of serialised Rust tests
/// ````
/// #[test]
/// #[serial]
/// fn test_serial_one() {
///   // Do things
/// }
///
/// #[test]
/// #[serial]
/// fn test_serial_another() {
///   // Do things
/// }
/// ````
/// Multiple tests with the [serial](macro@serial) attribute are guaranteed to be executed in serial. Ordering
/// of the tests is not guaranteed however. If you want different subsets of tests to be serialised with each
/// other, but not depend on other subsets, you can add an argument to [serial](macro@serial), and all calls
/// with identical arguments will be called in serial. e.g.
/// ````
/// #[test]
/// #[serial(something)]
/// fn test_serial_one() {
///   // Do things
/// }
///
/// #[test]
/// #[serial(something)]
/// fn test_serial_another() {
///   // Do things
/// }
///
/// #[test]
/// #[serial(other)]
/// fn test_serial_third() {
///   // Do things
/// }
///
/// #[test]
/// #[serial(other)]
/// fn test_serial_fourth() {
///   // Do things
/// }
/// ````
/// `test_serial_one` and `test_serial_another` will be executed in serial, as will `test_serial_third` and `test_serial_fourth`
/// but neither sequence will be blocked by the other
///
/// Nested serialised tests (i.e. a [serial](macro@serial) tagged test calling another) is supported
#[proc_macro_attribute]
#[proc_macro_error]
pub fn serial(attr: TokenStream, input: TokenStream) -> TokenStream {
    local_serial_core(attr.into(), input.into()).into()
}

/// Allows for the creation of file-serialised Rust tests
/// ````
/// #[test]
/// #[file_serial]
/// fn test_serial_one() {
///   // Do things
/// }
///
/// #[test]
/// #[file_serial]
/// fn test_serial_another() {
///   // Do things
/// }
/// ````
///
/// Multiple tests with the [file_serial](macro@file_serial) attribute are guaranteed to run in serial, as per the [serial](macro@serial)
/// attribute. Note that there are no guarantees about one test with [serial](macro@serial) and another with [file_serial](macro@file_serial)
/// as they lock using different methods, and [file_serial](macro@file_serial) does not support nested serialised tests, but otherwise acts
/// like [serial](macro@serial).
///
/// It also supports an optional `path` arg e.g
/// ````
/// #[test]
/// #[file_serial(key, "/tmp/foo")]
/// fn test_serial_one() {
///   // Do things
/// }
///
/// #[test]
/// #[file_serial(key, "/tmp/foo")]
/// fn test_serial_another() {
///   // Do things
/// }
/// ````
/// Note that in this case you need to specify the `name` arg as well (as per [serial](macro@serial)). The path defaults to a reasonable temp directory for the OS if not specified.
#[proc_macro_attribute]
#[proc_macro_error]
#[cfg_attr(docsrs, doc(cfg(feature = "file_locks")))]
pub fn file_serial(attr: TokenStream, input: TokenStream) -> TokenStream {
    fs_serial_core(attr.into(), input.into()).into()
}

// Based off of https://github.com/dtolnay/quote/issues/20#issuecomment-437341743
struct QuoteOption<T>(Option<T>);

impl<T: ToTokens> ToTokens for QuoteOption<T> {
    fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
        tokens.append_all(match self.0 {
            Some(ref t) => quote! { ::std::option::Option::Some(#t) },
            None => quote! { ::std::option::Option::None },
        });
    }
}

fn get_raw_args(attr: proc_macro2::TokenStream) -> Vec<String> {
    let mut attrs = attr.into_iter().collect::<Vec<TokenTree>>();
    let mut raw_args: Vec<String> = Vec::new();
    while !attrs.is_empty() {
        match attrs.remove(0) {
            TokenTree::Ident(id) => {
                raw_args.push(id.to_string());
            }
            TokenTree::Literal(literal) => {
                let string_literal = literal.to_string();
                if !string_literal.starts_with('\"') || !string_literal.ends_with('\"') {
                    panic!("Expected a string literal, got '{}'", string_literal);
                }
                // Hacky way of getting a string without the enclosing quotes
                raw_args.push(string_literal[1..string_literal.len() - 1].to_string());
            }
            x => {
                panic!("Expected either strings or literals as args, not {}", x);
            }
        }
        if !attrs.is_empty() {
            match attrs.remove(0) {
                TokenTree::Punct(p) if p.as_char() == ',' => {}
                x => {
                    panic!("Expected , between args, not {}", x);
                }
            }
        }
    }
    raw_args
}

fn local_serial_core(
    attr: proc_macro2::TokenStream,
    input: proc_macro2::TokenStream,
) -> proc_macro2::TokenStream {
    let mut raw_args = get_raw_args(attr);
    let key = match raw_args.len() {
        0 => "".to_string(),
        1 => raw_args.pop().unwrap(),
        n => {
            panic!(
                "Expected either 0 or 1 arguments, got {}: {:?}",
                n, raw_args
            );
        }
    };
    serial_setup(input, vec![Box::new(key)], "local")
}

fn fs_serial_core(
    attr: proc_macro2::TokenStream,
    input: proc_macro2::TokenStream,
) -> proc_macro2::TokenStream {
    let none_ident = Box::new(format_ident!("None"));
    let mut args: Vec<Box<dyn quote::ToTokens>> = Vec::new();
    let mut raw_args = get_raw_args(attr);
    match raw_args.len() {
        0 => {
            args.push(Box::new("".to_string()));
            args.push(none_ident);
        }
        1 => {
            args.push(Box::new(raw_args.pop().unwrap()));
            args.push(none_ident);
        }
        2 => {
            let key = raw_args.remove(0);
            let path = raw_args.remove(0);
            args.push(Box::new(key));
            args.push(Box::new(QuoteOption(Some(path))));
        }
        n => {
            panic!("Expected 0-2 arguments, got {}: {:?}", n, raw_args);
        }
    }
    serial_setup(input, args, "fs")
}

fn serial_setup<T>(
    input: proc_macro2::TokenStream,
    args: Vec<Box<T>>,
    prefix: &str,
) -> proc_macro2::TokenStream
where
    T: quote::ToTokens + ?Sized,
{
    let ast: syn::ItemFn = syn::parse2(input).unwrap();
    let asyncness = ast.sig.asyncness;
    let name = ast.sig.ident;
    let return_type = match ast.sig.output {
        syn::ReturnType::Default => None,
        syn::ReturnType::Type(_rarrow, ref box_type) => Some(box_type.deref()),
    };
    let block = ast.block;
    let attrs: Vec<syn::Attribute> = ast
        .attrs
        .into_iter()
        .filter(|at| {
            if let Ok(m) = at.parse_meta() {
                let path = m.path();
                if asyncness.is_some()
                    && path.segments.len() == 2
                    && path.segments[1].ident == "test"
                {
                    // We assume that any 2-part attribute with the second part as "test" on an async function
                    // is the "do this test with reactor" wrapper. This is true for actix, tokio and async_std.
                    abort_call_site!("Found async test attribute after serial, which will break");
                }

                // we skip ignore/should_panic because the test framework already deals with it
                !(path.is_ident("ignore") || path.is_ident("should_panic"))
            } else {
                true
            }
        })
        .collect();
    if let Some(ret) = return_type {
        match asyncness {
            Some(_) => {
                let fnname = format_ident!("{}_async_serial_core_with_return", prefix);
                quote! {
                    #(#attrs)
                    *
                    async fn #name () -> #ret {
                        serial_test::#fnname(#(#args ),*, || async #block ).await;
                    }
                }
            }
            None => {
                let fnname = format_ident!("{}_serial_core_with_return", prefix);
                quote! {
                    #(#attrs)
                    *
                    fn #name () -> #ret {
                        serial_test::#fnname(#(#args ),*, || #block )
                    }
                }
            }
        }
    } else {
        match asyncness {
            Some(_) => {
                let fnname = format_ident!("{}_async_serial_core", prefix);
                quote! {
                    #(#attrs)
                    *
                    async fn #name () {
                        serial_test::#fnname(#(#args ),*, || async #block ).await;
                    }
                }
            }
            None => {
                let fnname = format_ident!("{}_serial_core", prefix);
                quote! {
                    #(#attrs)
                    *
                    fn #name () {
                        serial_test::#fnname(#(#args ),*, || #block );
                    }
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use proc_macro2::{Literal, Punct, Spacing};

    use super::{format_ident, fs_serial_core, local_serial_core, quote, TokenTree};
    use std::iter::FromIterator;

    #[test]
    fn test_serial() {
        let attrs = proc_macro2::TokenStream::new();
        let input = quote! {
            #[test]
            fn foo() {}
        };
        let stream = local_serial_core(attrs.into(), input);
        let compare = quote! {
            #[test]
            fn foo () {
                serial_test::local_serial_core("", || {} );
            }
        };
        assert_eq!(format!("{}", compare), format!("{}", stream));
    }

    #[test]
    fn test_stripped_attributes() {
        let _ = env_logger::builder().is_test(true).try_init();
        let attrs = proc_macro2::TokenStream::new();
        let input = quote! {
            #[test]
            #[ignore]
            #[should_panic(expected = "Testing panic")]
            #[something_else]
            fn foo() {}
        };
        let stream = local_serial_core(attrs.into(), input);
        let compare = quote! {
            #[test]
            #[something_else]
            fn foo () {
                serial_test::local_serial_core("", || {} );
            }
        };
        assert_eq!(format!("{}", compare), format!("{}", stream));
    }

    #[test]
    fn test_serial_async() {
        let attrs = proc_macro2::TokenStream::new();
        let input = quote! {
            async fn foo() {}
        };
        let stream = local_serial_core(attrs.into(), input);
        let compare = quote! {
            async fn foo () {
                serial_test::local_async_serial_core("", || async {} ).await;
            }
        };
        assert_eq!(format!("{}", compare), format!("{}", stream));
    }

    #[test]
    fn test_serial_async_return() {
        let attrs = proc_macro2::TokenStream::new();
        let input = quote! {
            async fn foo() -> Result<(), ()> { Ok(()) }
        };
        let stream = local_serial_core(attrs.into(), input);
        let compare = quote! {
            async fn foo () -> Result<(), ()> {
                serial_test::local_async_serial_core_with_return("", || async { Ok(()) } ).await;
            }
        };
        assert_eq!(format!("{}", compare), format!("{}", stream));
    }

    // 1.54 needed for https://github.com/rust-lang/rust/commit/9daf546b77dbeab7754a80d7336cd8d00c6746e4 change in note message
    #[rustversion::since(1.54)]
    #[test]
    fn test_serial_async_before_wrapper() {
        let t = trybuild::TestCases::new();
        t.compile_fail("tests/broken/test_serial_async_before_wrapper.rs");
    }

    #[test]
    fn test_file_serial() {
        let attrs = vec![TokenTree::Ident(format_ident!("foo"))];
        let input = quote! {
            #[test]
            fn foo() {}
        };
        let stream = fs_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn foo () {
                serial_test::fs_serial_core("foo", None, || {} );
            }
        };
        assert_eq!(format!("{}", compare), format!("{}", stream));
    }

    #[test]
    fn test_file_serial_with_path() {
        let attrs = vec![
            TokenTree::Ident(format_ident!("foo")),
            TokenTree::Punct(Punct::new(',', Spacing::Alone)),
            TokenTree::Literal(Literal::string("bar_path")),
        ];
        let input = quote! {
            #[test]
            fn foo() {}
        };
        let stream = fs_serial_core(
            proc_macro2::TokenStream::from_iter(attrs.into_iter()),
            input,
        );
        let compare = quote! {
            #[test]
            fn foo () {
                serial_test::fs_serial_core("foo", ::std::option::Option::Some("bar_path"), || {} );
            }
        };
        assert_eq!(format!("{}", compare), format!("{}", stream));
    }
}
