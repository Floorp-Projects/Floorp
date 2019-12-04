//! As of Rust 1.30, the language supports user-defined function-like procedural
//! macros. However these can only be invoked in item position, not in
//! statements or expressions.
//!
//! This crate implements an alternative type of procedural macro that can be
//! invoked in statement or expression position.
//!
//! # Defining procedural macros
//!
//! Two crates are required to define a procedural macro.
//!
//! ## The implementation crate
//!
//! This crate must contain nothing but procedural macros. Private helper
//! functions and private modules are fine but nothing can be public.
//!
//! [> example of an implementation crate][demo-hack-impl]
//!
//! Just like you would use a #\[proc_macro\] attribute to define a natively
//! supported procedural macro, use proc-macro-hack's #\[proc_macro_hack\]
//! attribute to define a procedural macro that works in expression position.
//! The function signature is the same as for ordinary function-like procedural
//! macros.
//!
//! ```
//! extern crate proc_macro;
//!
//! use proc_macro::TokenStream;
//! use proc_macro_hack::proc_macro_hack;
//! use quote::quote;
//! use syn::{parse_macro_input, Expr};
//!
//! # const IGNORE: &str = stringify! {
//! #[proc_macro_hack]
//! # };
//! pub fn add_one(input: TokenStream) -> TokenStream {
//!     let expr = parse_macro_input!(input as Expr);
//!     TokenStream::from(quote! {
//!         1 + (#expr)
//!     })
//! }
//! #
//! # fn main() {}
//! ```
//!
//! ## The declaration crate
//!
//! This crate is allowed to contain other public things if you need, for
//! example traits or functions or ordinary macros.
//!
//! [> example of a declaration crate][demo-hack]
//!
//! Within the declaration crate there needs to be a re-export of your
//! procedural macro from the implementation crate. The re-export also carries a
//! \#\[proc_macro_hack\] attribute.
//!
//! ```
//! use proc_macro_hack::proc_macro_hack;
//!
//! /// Add one to an expression.
//! ///
//! /// (Documentation goes here on the re-export, not in the other crate.)
//! #[proc_macro_hack]
//! pub use demo_hack_impl::add_one;
//! #
//! # fn main() {}
//! ```
//!
//! Both crates depend on `proc-macro-hack`:
//!
//! ```toml
//! [dependencies]
//! proc-macro-hack = "0.5"
//! ```
//!
//! Additionally, your implementation crate (but not your declaration crate) is
//! a proc macro crate:
//!
//! ```toml
//! [lib]
//! proc-macro = true
//! ```
//!
//! # Using procedural macros
//!
//! Users of your crate depend on your declaration crate (not your
//! implementation crate), then use your procedural macros as usual.
//!
//! [> example of a downstream crate][example]
//!
//! ```
//! use demo_hack::add_one;
//!
//! fn main() {
//!     let two = 2;
//!     let nine = add_one!(two) + add_one!(2 + 3);
//!     println!("nine = {}", nine);
//! }
//! ```
//!
//! [demo-hack-impl]: https://github.com/dtolnay/proc-macro-hack/tree/master/demo-hack-impl
//! [demo-hack]: https://github.com/dtolnay/proc-macro-hack/tree/master/demo-hack
//! [example]: https://github.com/dtolnay/proc-macro-hack/tree/master/example
//!
//! # Limitations
//!
//! - Only proc macros in expression position are supported. Proc macros in type
//!   position ([#10]) or pattern position ([#20]) are not supported.
//!
//! - By default, nested invocations are not supported i.e. the code emitted by
//!   a proc-macro-hack macro invocation cannot contain recursive calls to the
//!   same proc-macro-hack macro nor calls to any other proc-macro-hack macros.
//!   Use [`proc-macro-nested`] if you require support for nested invocations.
//!
//! - By default, hygiene is structured such that the expanded code can't refer
//!   to local variables other than those passed by name somewhere in the macro
//!   input. If your macro must refer to *local* variables that don't get named
//!   in the macro input, use `#[proc_macro_hack(fake_call_site)]` on the
//!   re-export in your declaration crate. *Most macros won't need this.*
//!
//! [#10]: https://github.com/dtolnay/proc-macro-hack/issues/10
//! [#20]: https://github.com/dtolnay/proc-macro-hack/issues/20
//! [`proc-macro-nested`]: https://docs.rs/proc-macro-nested

#![recursion_limit = "512"]
#![cfg_attr(feature = "cargo-clippy", allow(renamed_and_removed_lints))]
#![cfg_attr(feature = "cargo-clippy", allow(needless_pass_by_value))]

extern crate proc_macro;

use proc_macro2::{Span, TokenStream, TokenTree};
use quote::{format_ident, quote, ToTokens};
use std::fmt::Write;
use syn::ext::IdentExt;
use syn::parse::{Parse, ParseStream, Result};
use syn::{braced, bracketed, parenthesized, parse_macro_input, token, Ident, LitInt, Token};

type Visibility = Option<Token![pub]>;

enum Input {
    Export(Export),
    Define(Define),
}

// pub use demo_hack_impl::{m1, m2 as qrst};
struct Export {
    attrs: TokenStream,
    vis: Visibility,
    from: Ident,
    macros: Vec<Macro>,
}

// pub fn m1(input: TokenStream) -> TokenStream { ... }
struct Define {
    attrs: TokenStream,
    name: Ident,
    body: TokenStream,
}

struct Macro {
    name: Ident,
    export_as: Ident,
}

impl Parse for Input {
    fn parse(input: ParseStream) -> Result<Self> {
        let ahead = input.fork();
        parse_attributes(&ahead)?;
        ahead.parse::<Visibility>()?;

        if ahead.peek(Token![use]) {
            input.parse().map(Input::Export)
        } else if ahead.peek(Token![fn]) {
            input.parse().map(Input::Define)
        } else {
            Err(input.error("unexpected input to #[proc_macro_hack]"))
        }
    }
}

impl Parse for Export {
    fn parse(input: ParseStream) -> Result<Self> {
        let attrs = input.call(parse_attributes)?;
        let vis: Visibility = input.parse()?;
        input.parse::<Token![use]>()?;
        input.parse::<Option<Token![::]>>()?;
        let from: Ident = input.parse()?;
        input.parse::<Token![::]>()?;

        let mut macros = Vec::new();
        if input.peek(token::Brace) {
            let content;
            braced!(content in input);
            loop {
                macros.push(content.parse()?);
                if content.is_empty() {
                    break;
                }
                content.parse::<Token![,]>()?;
                if content.is_empty() {
                    break;
                }
            }
        } else {
            macros.push(input.parse()?);
        }

        input.parse::<Token![;]>()?;
        Ok(Export {
            attrs,
            vis,
            from,
            macros,
        })
    }
}

impl Parse for Define {
    fn parse(input: ParseStream) -> Result<Self> {
        let attrs = input.call(parse_attributes)?;
        let vis: Visibility = input.parse()?;
        if vis.is_none() {
            return Err(input.error("functions tagged with `#[proc_macro_hack]` must be `pub`"));
        }

        input.parse::<Token![fn]>()?;
        let name: Ident = input.parse()?;
        let body: TokenStream = input.parse()?;
        Ok(Define { attrs, name, body })
    }
}

impl Parse for Macro {
    fn parse(input: ParseStream) -> Result<Self> {
        let name: Ident = input.parse()?;
        let renamed: Option<Token![as]> = input.parse()?;
        let export_as = if renamed.is_some() {
            input.parse()?
        } else {
            name.clone()
        };
        Ok(Macro { name, export_as })
    }
}

fn parse_attributes(input: ParseStream) -> Result<TokenStream> {
    let mut attrs = TokenStream::new();
    while input.peek(Token![#]) {
        let pound: Token![#] = input.parse()?;
        pound.to_tokens(&mut attrs);
        let content;
        let bracket_token = bracketed!(content in input);
        let content: TokenStream = content.parse()?;
        bracket_token.surround(&mut attrs, |tokens| content.to_tokens(tokens));
    }
    Ok(attrs)
}

#[proc_macro_attribute]
pub fn proc_macro_hack(
    args: proc_macro::TokenStream,
    input: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    proc_macro::TokenStream::from(match parse_macro_input!(input) {
        Input::Export(export) => {
            let args = parse_macro_input!(args as ExportArgs);
            expand_export(export, args)
        }
        Input::Define(define) => {
            parse_macro_input!(args as DefineArgs);
            expand_define(define)
        }
    })
}

mod kw {
    syn::custom_keyword!(derive);
    syn::custom_keyword!(fake_call_site);
    syn::custom_keyword!(internal_macro_calls);
    syn::custom_keyword!(support_nested);
}

struct ExportArgs {
    support_nested: bool,
    internal_macro_calls: u16,
    fake_call_site: bool,
}

impl Parse for ExportArgs {
    fn parse(input: ParseStream) -> Result<Self> {
        let mut args = ExportArgs {
            support_nested: false,
            internal_macro_calls: 0,
            fake_call_site: false,
        };

        while !input.is_empty() {
            let ahead = input.lookahead1();
            if ahead.peek(kw::support_nested) {
                input.parse::<kw::support_nested>()?;
                args.support_nested = true;
            } else if ahead.peek(kw::internal_macro_calls) {
                input.parse::<kw::internal_macro_calls>()?;
                input.parse::<Token![=]>()?;
                let calls = input.parse::<LitInt>()?.base10_parse()?;
                args.internal_macro_calls = calls;
            } else if ahead.peek(kw::fake_call_site) {
                input.parse::<kw::fake_call_site>()?;
                args.fake_call_site = true;
            } else {
                return Err(ahead.error());
            }
            if input.is_empty() {
                break;
            }
            input.parse::<Token![,]>()?;
        }

        Ok(args)
    }
}

struct DefineArgs;

impl Parse for DefineArgs {
    fn parse(_input: ParseStream) -> Result<Self> {
        Ok(DefineArgs)
    }
}

struct EnumHack {
    token_stream: TokenStream,
}

impl Parse for EnumHack {
    fn parse(input: ParseStream) -> Result<Self> {
        input.parse::<Token![enum]>()?;
        input.parse::<Ident>()?;

        let braces;
        braced!(braces in input);
        braces.parse::<Ident>()?;
        braces.parse::<Token![=]>()?;

        let parens;
        parenthesized!(parens in braces);
        parens.parse::<Ident>()?;
        parens.parse::<Token![!]>()?;

        let inner;
        braced!(inner in parens);
        let token_stream: TokenStream = inner.parse()?;

        parens.parse::<Token![,]>()?;
        parens.parse::<TokenTree>()?;
        braces.parse::<Token![.]>()?;
        braces.parse::<TokenTree>()?;
        braces.parse::<Token![,]>()?;

        Ok(EnumHack { token_stream })
    }
}

#[doc(hidden)]
#[proc_macro_derive(ProcMacroHack)]
pub fn enum_hack(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let inner = parse_macro_input!(input as EnumHack);
    proc_macro::TokenStream::from(inner.token_stream)
}

struct FakeCallSite {
    derive: Ident,
    rest: TokenStream,
}

impl Parse for FakeCallSite {
    fn parse(input: ParseStream) -> Result<Self> {
        input.parse::<Token![#]>()?;
        let attr;
        bracketed!(attr in input);
        attr.parse::<kw::derive>()?;
        let path;
        parenthesized!(path in attr);
        Ok(FakeCallSite {
            derive: path.parse()?,
            rest: input.parse()?,
        })
    }
}

#[doc(hidden)]
#[proc_macro_attribute]
pub fn fake_call_site(
    args: proc_macro::TokenStream,
    input: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    let args = TokenStream::from(args);
    let span = match args.into_iter().next() {
        Some(token) => token.span(),
        None => return input,
    };

    let input = parse_macro_input!(input as FakeCallSite);
    let mut derive = input.derive;
    derive.set_span(span);
    let rest = input.rest;

    let expanded = quote! {
        #[derive(#derive)]
        #rest
    };

    proc_macro::TokenStream::from(expanded)
}

fn expand_export(export: Export, args: ExportArgs) -> TokenStream {
    let dummy = dummy_name_for_export(&export);

    let attrs = export.attrs;
    let vis = export.vis;
    let macro_export = match vis {
        Some(_) => quote!(#[macro_export]),
        None => quote!(),
    };
    let crate_prefix = vis.map(|_| quote!($crate::));
    let enum_variant = if args.support_nested {
        if args.internal_macro_calls == 0 {
            quote!(Nested)
        } else {
            format_ident!("Nested{}", args.internal_macro_calls).to_token_stream()
        }
    } else {
        quote!(Value)
    };

    let from = export.from;
    let rules = export
        .macros
        .into_iter()
        .map(|Macro { name, export_as }| {
            let actual_name = actual_proc_macro_name(&name);
            let dispatch = dispatch_macro_name(&name);
            let call_site = call_site_macro_name(&name);

            let export_dispatch = if args.support_nested {
                quote! {
                    #[doc(hidden)]
                    #vis use proc_macro_nested::dispatch as #dispatch;
                }
            } else {
                quote!()
            };

            let proc_macro_call = if args.support_nested {
                let extra_bangs = (0..args.internal_macro_calls).map(|_| quote!(!));
                quote! {
                    #crate_prefix #dispatch! { ($($proc_macro)*) #(#extra_bangs)* }
                }
            } else {
                quote! {
                    proc_macro_call!()
                }
            };

            let export_call_site = if args.fake_call_site {
                quote! {
                    #[doc(hidden)]
                    #vis use proc_macro_hack::fake_call_site as #call_site;
                }
            } else {
                quote!()
            };

            let do_derive = if !args.fake_call_site {
                quote! {
                    #[derive(#crate_prefix #actual_name)]
                }
            } else if crate_prefix.is_some() {
                quote! {
                    use #crate_prefix #actual_name;
                    #[#crate_prefix #call_site ($($proc_macro)*)]
                    #[derive(#actual_name)]
                }
            } else {
                quote! {
                    #[#call_site ($($proc_macro)*)]
                    #[derive(#actual_name)]
                }
            };

            quote! {
                #[doc(hidden)]
                #vis use #from::#actual_name;

                #export_dispatch
                #export_call_site

                #attrs
                #macro_export
                macro_rules! #export_as {
                    ($($proc_macro:tt)*) => {{
                        #do_derive
                        enum ProcMacroHack {
                            #enum_variant = (stringify! { $($proc_macro)* }, 0).1,
                        }
                        #proc_macro_call
                    }};
                }
            }
        })
        .collect();

    wrap_in_enum_hack(dummy, rules)
}

fn expand_define(define: Define) -> TokenStream {
    let attrs = define.attrs;
    let name = define.name;
    let dummy = actual_proc_macro_name(&name);
    let body = define.body;

    quote! {
        mod #dummy {
            extern crate proc_macro;
            pub use self::proc_macro::*;
        }

        #attrs
        #[proc_macro_derive(#dummy)]
        pub fn #dummy(input: #dummy::TokenStream) -> #dummy::TokenStream {
            use std::iter::FromIterator;

            let mut iter = input.into_iter();
            iter.next().unwrap(); // `enum`
            iter.next().unwrap(); // `ProcMacroHack`

            let mut braces = match iter.next().unwrap() {
                #dummy::TokenTree::Group(group) => group.stream().into_iter(),
                _ => unimplemented!(),
            };
            let variant = braces.next().unwrap(); // `Value` or `Nested`
            let varname = variant.to_string();
            let support_nested = varname.starts_with("Nested");
            braces.next().unwrap(); // `=`

            let mut parens = match braces.next().unwrap() {
                #dummy::TokenTree::Group(group) => group.stream().into_iter(),
                _ => unimplemented!(),
            };
            parens.next().unwrap(); // `stringify`
            parens.next().unwrap(); // `!`

            let inner = match parens.next().unwrap() {
                #dummy::TokenTree::Group(group) => group.stream(),
                _ => unimplemented!(),
            };

            let output: #dummy::TokenStream = #name(inner.clone());

            fn count_bangs(input: #dummy::TokenStream) -> usize {
                let mut count = 0;
                for token in input {
                    match token {
                        #dummy::TokenTree::Punct(punct) => {
                            if punct.as_char() == '!' {
                                count += 1;
                            }
                        }
                        #dummy::TokenTree::Group(group) => {
                            count += count_bangs(group.stream());
                        }
                        _ => {}
                    }
                }
                count
            }

            // macro_rules! proc_macro_call {
            //     () => { #output }
            // }
            #dummy::TokenStream::from_iter(vec![
                #dummy::TokenTree::Ident(
                    #dummy::Ident::new("macro_rules", #dummy::Span::call_site()),
                ),
                #dummy::TokenTree::Punct(
                    #dummy::Punct::new('!', #dummy::Spacing::Alone),
                ),
                #dummy::TokenTree::Ident(
                    #dummy::Ident::new(
                        &if support_nested {
                            let extra_bangs = if varname == "Nested" {
                                0
                            } else {
                                varname["Nested".len()..].parse().unwrap()
                            };
                            format!("proc_macro_call_{}", extra_bangs + count_bangs(inner))
                        } else {
                            String::from("proc_macro_call")
                        },
                        #dummy::Span::call_site(),
                    ),
                ),
                #dummy::TokenTree::Group(
                    #dummy::Group::new(#dummy::Delimiter::Brace, #dummy::TokenStream::from_iter(vec![
                        #dummy::TokenTree::Group(
                            #dummy::Group::new(#dummy::Delimiter::Parenthesis, #dummy::TokenStream::new()),
                        ),
                        #dummy::TokenTree::Punct(
                            #dummy::Punct::new('=', #dummy::Spacing::Joint),
                        ),
                        #dummy::TokenTree::Punct(
                            #dummy::Punct::new('>', #dummy::Spacing::Alone),
                        ),
                        #dummy::TokenTree::Group(
                            #dummy::Group::new(#dummy::Delimiter::Brace, output),
                        ),
                    ])),
                ),
            ])
        }

        fn #name #body
    }
}

fn actual_proc_macro_name(conceptual: &Ident) -> Ident {
    format_ident!("proc_macro_hack_{}", conceptual)
}

fn dispatch_macro_name(conceptual: &Ident) -> Ident {
    format_ident!("proc_macro_call_{}", conceptual)
}

fn call_site_macro_name(conceptual: &Ident) -> Ident {
    format_ident!("proc_macro_fake_call_site_{}", conceptual)
}

fn dummy_name_for_export(export: &Export) -> String {
    let mut dummy = String::new();
    let from = export.from.unraw().to_string();
    write!(dummy, "_{}{}", from.len(), from).unwrap();
    for m in &export.macros {
        let name = m.name.unraw().to_string();
        write!(dummy, "_{}{}", name.len(), name).unwrap();
    }
    dummy
}

fn wrap_in_enum_hack(dummy: String, inner: TokenStream) -> TokenStream {
    let dummy = Ident::new(&dummy, Span::call_site());
    quote! {
        #[derive(proc_macro_hack::ProcMacroHack)]
        enum #dummy {
            Value = (stringify! { #inner }, 0).1,
        }
    }
}
