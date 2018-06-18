#![recursion_limit = "256"]

#[macro_use]
extern crate quote;

#[macro_use]
extern crate syn;
extern crate proc_macro2;

extern crate ident_case;

#[macro_use]
mod macros;

pub mod ast;
pub mod codegen;
pub mod error;
mod from_field;
mod from_derive_input;
mod from_meta_item;
mod from_variant;
pub mod options;
pub mod util;

pub use error::{Result, Error};
pub use from_derive_input::FromDeriveInput;
pub use from_field::FromField;
pub use from_meta_item::{FromMetaItem};
pub use from_variant::FromVariant;

#[cfg(test)]
mod tests {
    
}
