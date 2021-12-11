//! Fluent is a modern localization system designed to improve how software is translated.
//!
//! `fluent-syntax` is the lowest level component of the [Fluent Localization
//! System](https://www.projectfluent.org).
//!
//! It exposes components necessary for parsing and tooling operations on Fluent Translation Lists ("FTL").
//!
//! The crate provides a [`parser`] module which allows for parsing of an
//! input string to an Abstract Syntax Tree defined in the [`ast`] module.
//!
//! The [`unicode`] module exposes a set of helper functions used to decode
//! escaped unicode literals according to Fluent specification.
//!
//! # Example
//!
//! ```
//! use fluent_syntax::parser;
//! use fluent_syntax::ast;
//!
//! let ftl = r#"
//!
//! hello-world = Hello World!
//!
//! "#;
//!
//! let resource = parser::parse(ftl)
//!     .expect("Failed to parse an FTL resource.");
//!
//! assert_eq!(
//!     resource.body[0],
//!     ast::Entry::Message(
//!         ast::Message {
//!             id: ast::Identifier {
//!                 name: "hello-world"
//!             },
//!             value: Some(ast::Pattern {
//!                 elements: vec![
//!                     ast::PatternElement::TextElement {
//!                         value: "Hello World!"
//!                     },
//!                 ]
//!             }),
//!             attributes: vec![],
//!             comment: None,
//!         }
//!     ),
//! );
//! ```
pub mod ast;
pub mod parser;
pub mod unicode;
