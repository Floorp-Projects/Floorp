//! Docopt for Rust. This implementation conforms to the
//! [official description of Docopt](http://docopt.org/) and
//! [passes its test suite](https://github.com/docopt/docopt/pull/201).
//!
//! This library is [on GitHub](https://github.com/docopt/docopt.rs).
//!
//! Fundamentally, Docopt is a command line argument parser. The detail that
//! distinguishes it from most parsers is that the parser is derived from the
//! usage string. Here's a simple example:
//!
//! ```rust
//! use docopt::Docopt;
//!
//! // Write the Docopt usage string.
//! const USAGE: &'static str = "
//! Usage: cp [-a] <source> <dest>
//!        cp [-a] <source>... <dir>
//!
//! Options:
//!     -a, --archive  Copy everything.
//! ";
//!
//! // The argv. Normally you'd just use `parse` which will automatically
//! // use `std::env::args()`.
//! let argv = || vec!["cp", "-a", "file1", "file2", "dest/"];
//!
//! // Parse argv and exit the program with an error message if it fails.
//! let args = Docopt::new(USAGE)
//!                   .and_then(|d| d.argv(argv().into_iter()).parse())
//!                   .unwrap_or_else(|e| e.exit());
//!
//! // Now access your argv values. Synonyms work just fine!
//! assert!(args.get_bool("-a") && args.get_bool("--archive"));
//! assert_eq!(args.get_vec("<source>"), vec!["file1", "file2"]);
//! assert_eq!(args.get_str("<dir>"), "dest/");
//! assert_eq!(args.get_str("<dest>"), "");
//! ```
//!
//! # Type based decoding
//!
//! Often, command line values aren't just strings or booleans---sometimes
//! they are integers, or enums, or something more elaborate. Using the
//! standard Docopt interface can be inconvenient for this purpose, because
//! you'll need to convert all of the values explicitly. Instead, this crate
//! provides a `Decoder` that converts an `ArgvMap` to your custom struct.
//! Here is the same example as above using type based decoding:
//!
//! ```rust
//! # extern crate docopt;
//! #[macro_use]
//! extern crate serde_derive;
//! # fn main() {
//! use docopt::Docopt;
//!
//! // Write the Docopt usage string.
//! const USAGE: &'static str = "
//! Usage: cp [-a] <source> <dest>
//!        cp [-a] <source>... <dir>
//!
//! Options:
//!     -a, --archive  Copy everything.
//! ";
//!
//! #[derive(Deserialize)]
//! struct Args {
//!     arg_source: Vec<String>,
//!     arg_dest: String,
//!     arg_dir: String,
//!     flag_archive: bool,
//! }
//!
//! let argv = || vec!["cp", "-a", "file1", "file2", "dest/"];
//! let args: Args = Docopt::new(USAGE)
//!                         .and_then(|d| d.argv(argv().into_iter()).deserialize())
//!                         .unwrap_or_else(|e| e.exit());
//!
//! // Now access your argv values.
//! fn s(x: &str) -> String { x.to_string() }
//! assert!(args.flag_archive);
//! assert_eq!(args.arg_source, vec![s("file1"), s("file2")]);
//! assert_eq!(args.arg_dir, s("dest/"));
//! assert_eq!(args.arg_dest, s(""));
//! # }
//! ```
//!
//! # Command line arguments for `rustc`
//!
//! Here's an example with a subset of `rustc`'s command line arguments that
//! shows more of Docopt and some of the benefits of type based decoding.
//!
//! ```rust
//! # extern crate docopt;
//! #[macro_use]
//! extern crate serde_derive;
//! extern crate serde;
//! # fn main() {
//! # #![allow(non_snake_case)]
//! use docopt::Docopt;
//! use std::fmt;
//!
//! // Write the Docopt usage string.
//! const USAGE: &'static str = "
//! Usage: rustc [options] [--cfg SPEC... -L PATH...] INPUT
//!        rustc (--help | --version)
//!
//! Options:
//!     -h, --help         Show this message.
//!     --version          Show the version of rustc.
//!     --cfg SPEC         Configure the compilation environment.
//!     -L PATH            Add a directory to the library search path.
//!     --emit TYPE        Configure the output that rustc will produce.
//!                        Valid values: asm, ir, bc, obj, link.
//!     --opt-level LEVEL  Optimize with possible levels 0-3.
//! ";
//!
//! #[derive(Deserialize)]
//! struct Args {
//!     arg_INPUT: String,
//!     flag_emit: Option<Emit>,
//!     flag_opt_level: Option<OptLevel>,
//!     flag_cfg: Vec<String>,
//!     flag_L: Vec<String>,
//!     flag_help: bool,
//!     flag_version: bool,
//! }
//!
//! // This is easy. The decoder will automatically restrict values to
//! // strings that match one of the enum variants.
//! #[derive(Deserialize)]
//! # #[derive(Debug, PartialEq)]
//! enum Emit { Asm, Ir, Bc, Obj, Link }
//!
//! // This one is harder because we want the user to specify an integer,
//! // but restrict it to a specific range. So we implement `Deserialize`
//! // ourselves.
//! # #[derive(Debug, PartialEq)]
//! enum OptLevel { Zero, One, Two, Three }
//! struct OptLevelVisitor;
//!
//! impl<'de> serde::de::Visitor<'de> for OptLevelVisitor {
//!     type Value = OptLevel;
//!
//!     fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
//!         formatter.write_str("a number from range 0..3")
//!     }
//!
//!     fn visit_u8<E>(self, n: u8) -> Result<Self::Value, E>
//!         where E: serde::de::Error
//!     {
//!         Ok(match n {
//!             0 => OptLevel::Zero, 1 => OptLevel::One,
//!             2 => OptLevel::Two, 3 => OptLevel::Three,
//!             n => {
//!                 let err = format!(
//!                     "Could not deserialize '{}' as opt-level.", n);
//!                 return Err(E::custom(err));
//!             }
//!         })
//!     }
//! }
//!
//! impl<'de> serde::de::Deserialize<'de> for OptLevel {
//!     fn deserialize<D>(d: D) -> Result<OptLevel, D::Error>
//!         where D: serde::de::Deserializer<'de>
//!     {
//!         d.deserialize_u8(OptLevelVisitor)
//!     }
//! }
//!
//! let argv = || vec!["rustc", "-L", ".", "-L", "..", "--cfg", "a",
//!                             "--opt-level", "2", "--emit=ir", "docopt.rs"];
//! let args: Args = Docopt::new(USAGE)
//!                         .and_then(|d| d.argv(argv().into_iter()).deserialize())
//!                         .unwrap_or_else(|e| e.exit());
//!
//! // Now access your argv values.
//! fn s(x: &str) -> String { x.to_string() }
//! assert_eq!(args.arg_INPUT, "docopt.rs".to_string());
//! assert_eq!(args.flag_L, vec![s("."), s("..")]);
//! assert_eq!(args.flag_cfg, vec![s("a")]);
//! assert_eq!(args.flag_opt_level, Some(OptLevel::Two));
//! assert_eq!(args.flag_emit, Some(Emit::Ir));
//! # }
//! ```

#![crate_name = "docopt"]
#![doc(html_root_url = "http://burntsushi.net/rustdoc/docopt")]

#![deny(missing_docs)]

#[macro_use]
extern crate lazy_static;
extern crate regex;
extern crate strsim;
#[allow(unused_imports)]
#[macro_use]
extern crate serde_derive;
extern crate serde;

pub use dopt::{ArgvMap, Deserializer, Docopt, Error, Value};

macro_rules! werr(
    ($($arg:tt)*) => ({
        use std::io::{Write, stderr};
        write!(&mut stderr(), $($arg)*).unwrap();
    })
);

macro_rules! regex(
    ($s:expr) => (::regex::Regex::new($s).unwrap());
);

fn cap_or_empty<'t>(caps: &regex::Captures<'t>, name: &str) -> &'t str {
    caps.name(name).map_or("", |m| m.as_str())
}

mod dopt;
#[doc(hidden)]
pub mod parse;
mod synonym;
#[cfg(test)]
mod test;
