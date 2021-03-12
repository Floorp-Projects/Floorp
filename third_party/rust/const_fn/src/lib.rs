//! An attribute for easy generation of const functions with conditional
//! compilations.
//!
//! # Examples
//!
//! ```rust
//! use const_fn::const_fn;
//!
//! // function is `const` on specified version and later compiler (including beta and nightly)
//! #[const_fn("1.36")]
//! pub const fn version() {
//!     /* ... */
//! }
//!
//! // function is `const` on nightly compiler (including dev build)
//! #[const_fn(nightly)]
//! pub const fn nightly() {
//!     /* ... */
//! }
//!
//! // function is `const` if `cfg(...)` is true
//! # #[cfg(any())]
//! #[const_fn(cfg(...))]
//! # pub fn _cfg() { unimplemented!() }
//! pub const fn cfg() {
//!     /* ... */
//! }
//!
//! // function is `const` if `cfg(feature = "...")` is true
//! #[const_fn(feature = "...")]
//! pub const fn feature() {
//!     /* ... */
//! }
//! ```
//!
//! # Alternatives
//!
//! This crate is proc-macro, but is very lightweight, and has no dependencies.
//!
//! You can manually define declarative macros with similar functionality (see [`if_rust_version`](https://github.com/ogoffart/if_rust_version#examples)), or [you can define the same function twice with different cfg](https://github.com/crossbeam-rs/crossbeam/blob/0b6ea5f69fde8768c1cfac0d3601e0b4325d7997/crossbeam-epoch/src/atomic.rs#L340-L372).
//! (Note: the former approach requires more macros to be defined depending on the number of version requirements, the latter approach requires more functions to be maintained manually)

#![doc(test(
    no_crate_inject,
    attr(
        deny(warnings, rust_2018_idioms, single_use_lifetimes),
        allow(dead_code, unused_variables)
    )
))]
#![forbid(unsafe_code)]
#![warn(future_incompatible, rust_2018_idioms, single_use_lifetimes, unreachable_pub)]
#![warn(clippy::all, clippy::default_trait_access)]

// older compilers require explicit `extern crate`.
#[allow(unused_extern_crates)]
extern crate proc_macro;

#[macro_use]
mod utils;

mod ast;
mod error;
mod iter;
mod to_tokens;

use proc_macro::{Delimiter, TokenStream, TokenTree};
use std::str::FromStr;

use crate::{
    ast::{Func, LitStr},
    error::Error,
    to_tokens::ToTokens,
    utils::{cfg_attrs, parse_as_empty, tt_span},
};

type Result<T, E = Error> = std::result::Result<T, E>;

/// An attribute for easy generation of const functions with conditional compilations.
///
/// See crate level documentation for details.
#[proc_macro_attribute]
pub fn const_fn(args: TokenStream, input: TokenStream) -> TokenStream {
    let arg = match parse_arg(args) {
        Ok(arg) => arg,
        Err(e) => return e.to_compile_error(),
    };
    let func = match ast::parse_input(input) {
        Ok(func) => func,
        Err(e) => return e.to_compile_error(),
    };

    expand(arg, func)
}

fn expand(arg: Arg, mut func: Func) -> TokenStream {
    match arg {
        Arg::Cfg(cfg) => {
            let (mut tokens, cfg_not) = cfg_attrs(cfg);
            tokens.extend(func.to_token_stream());
            tokens.extend(cfg_not);
            func.print_const = false;
            tokens.extend(func.to_token_stream());
            tokens
        }
        Arg::Feature(feat) => {
            let (mut tokens, cfg_not) = cfg_attrs(feat);
            tokens.extend(func.to_token_stream());
            tokens.extend(cfg_not);
            func.print_const = false;
            tokens.extend(func.to_token_stream());
            tokens
        }
        Arg::Version(req) => {
            if req.major > 1 || req.minor > VERSION.minor {
                func.print_const = false;
            }
            func.to_token_stream()
        }
        Arg::Nightly => {
            func.print_const = VERSION.nightly;
            func.to_token_stream()
        }
        Arg::Always => func.to_token_stream(),
    }
}

enum Arg {
    // `const_fn("...")`
    Version(VersionReq),
    // `const_fn(nightly)`
    Nightly,
    // `const_fn(cfg(...))`
    Cfg(TokenStream),
    // `const_fn(feature = "...")`
    Feature(TokenStream),
    // `const_fn`
    Always,
}

fn parse_arg(tokens: TokenStream) -> Result<Arg> {
    let mut iter = tokens.into_iter();

    let next = iter.next();
    let next_span = tt_span(next.as_ref());
    match next {
        None => return Ok(Arg::Always),
        Some(TokenTree::Ident(i)) => match &*i.to_string() {
            "nightly" => {
                parse_as_empty(iter)?;
                return Ok(Arg::Nightly);
            }
            "cfg" => {
                return match iter.next().as_ref() {
                    Some(TokenTree::Group(g)) if g.delimiter() == Delimiter::Parenthesis => {
                        parse_as_empty(iter)?;
                        Ok(Arg::Cfg(g.stream()))
                    }
                    tt => Err(error!(tt_span(tt), "expected `(`")),
                };
            }
            "feature" => {
                let next = iter.next();
                return match next.as_ref() {
                    Some(TokenTree::Punct(p)) if p.as_char() == '=' => match iter.next() {
                        Some(TokenTree::Literal(l)) => {
                            let l = LitStr::new(l)?;
                            parse_as_empty(iter)?;
                            Ok(Arg::Feature(
                                vec![TokenTree::Ident(i), next.unwrap(), l.token.into()]
                                    .into_iter()
                                    .collect(),
                            ))
                        }
                        tt => Err(error!(tt_span(tt.as_ref()), "expected string literal")),
                    },
                    tt => Err(error!(tt_span(tt), "expected `=`")),
                };
            }
            _ => {}
        },
        Some(TokenTree::Literal(l)) => {
            if let Ok(l) = LitStr::new(l) {
                parse_as_empty(iter)?;
                return match l.value().parse::<VersionReq>() {
                    Ok(req) => Ok(Arg::Version(req)),
                    Err(e) => Err(error!(l.span(), "{}", e)),
                };
            }
        }
        Some(_) => {}
    }

    Err(error!(next_span, "expected one of: `nightly`, `cfg`, `feature`, string literal"))
}

struct VersionReq {
    major: u32,
    minor: u32,
}

impl FromStr for VersionReq {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let mut pieces = s.split('.');
        let major = pieces
            .next()
            .ok_or("need to specify the major version")?
            .parse::<u32>()
            .map_err(|e| e.to_string())?;
        let minor = pieces
            .next()
            .ok_or("need to specify the minor version")?
            .parse::<u32>()
            .map_err(|e| e.to_string())?;
        if let Some(s) = pieces.next() {
            Err(format!("unexpected input: .{}", s))
        } else {
            Ok(Self { major, minor })
        }
    }
}

struct Version {
    minor: u32,
    nightly: bool,
}

#[cfg(const_fn_has_build_script)]
const VERSION: Version = include!(concat!(env!("OUT_DIR"), "/version"));
// If build script has not run or unable to determine version, it is considered as Rust 1.0.
#[cfg(not(const_fn_has_build_script))]
const VERSION: Version = Version { minor: 0, nightly: false };
