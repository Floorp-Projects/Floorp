//! The Diplomat core module contains common logic between
//! the macro expansion and code generation. Right now, this
//! is primarily the AST types that Diplomat generates while
//! extracting APIs.

#![allow(clippy::needless_lifetimes)] // we use named lifetimes for clarity

pub mod ast;
#[cfg(feature = "hir")]
pub mod hir;

mod environment;

pub use environment::{Env, ModuleEnv};
