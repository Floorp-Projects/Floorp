//! This crate offers tools designed to aid with the implementation
//! of the JavaScript syntax and BinAST encoders/decoders/manipulators.

extern crate inflector;
extern crate itertools;

#[macro_use]
extern crate log;
extern crate webidl;


/// Generic tools for generating implementations of the Syntax.
pub mod export;

/// Import a specification of the Syntax.
pub mod import;

/// Manipulating the specifications of the language.
pub mod spec;

/// Misc. utilities.
pub mod util;
