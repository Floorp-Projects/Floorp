//! The ir module defines bindgen's intermediate representation.
//!
//! Parsing C/C++ generates the IR, while code generation outputs Rust code from
//! the IR.

pub mod annotations;
pub mod comp;
pub mod context;
pub mod derive;
pub mod enum_ty;
pub mod function;
pub mod int;
pub mod item;
pub mod item_kind;
pub mod layout;
pub mod module;
pub mod ty;
pub mod type_collector;
pub mod var;
pub mod objc;
