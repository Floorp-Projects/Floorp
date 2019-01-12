// Need this for rusty_peg
#![recursion_limit = "256"]
// I hate this lint.
#![allow(unused_parens)]
// The builtin tests don't cover the CLI and so forth, and it's just
// too darn annoying to try and make them do so.
#![cfg_attr(test, allow(dead_code))]

extern crate ascii_canvas;
extern crate atty;
extern crate bit_set;
extern crate diff;
extern crate ena;
extern crate itertools;
#[cfg_attr(any(feature = "test", test), macro_use)]
extern crate lalrpop_util;
extern crate petgraph;
extern crate regex;
extern crate regex_syntax;
extern crate sha2;
extern crate string_cache;
extern crate term;
extern crate unicode_xid;

#[cfg(test)]
extern crate rand;

// hoist the modules that define macros up earlier
#[macro_use]
mod rust;
#[macro_use]
mod log;

mod api;
mod build;
mod collections;
mod file_text;
mod grammar;
mod kernel_set;
mod lexer;
mod lr1;
mod message;
mod normalize;
mod parser;
mod session;
mod tls;
mod tok;
mod util;

#[cfg(test)]
mod generate;
#[cfg(test)]
mod test_util;

pub use api::process_root;
pub use api::process_root_unconditionally;
pub use api::Configuration;
use ascii_canvas::style;
