#![cfg_attr(feature = "clippy", feature(plugin))]
#![cfg_attr(feature = "clippy", plugin(clippy))]
#![deny(missing_copy_implementations, missing_debug_implementations, missing_docs, trivial_casts,
        trivial_numeric_casts, unsafe_code, unused_import_braces, unused_qualifications,
        variant_size_differences)]

//! Contains a lexer and parser for the WebIDL grammar.

extern crate lalrpop_util;

/// Contains lexer related structures and functions for lexing the WebIDL grammar.
mod lexer;

/// Contains parser related structures and functions for parsing the WebIDL grammar.
mod parser;

pub use lexer::*;
pub use parser::*;
