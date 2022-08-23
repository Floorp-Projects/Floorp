//! Types and support for parsing the core wasm text format.

mod custom;
mod export;
mod expr;
mod func;
mod global;
mod import;
mod memory;
mod module;
mod table;
mod tag;
mod types;
mod wast;
pub use self::custom::*;
pub use self::export::*;
pub use self::expr::*;
pub use self::func::*;
pub use self::global::*;
pub use self::import::*;
pub use self::memory::*;
pub use self::module::*;
pub use self::table::*;
pub use self::tag::*;
pub use self::types::*;
pub use self::wast::*;

pub(crate) mod binary;
pub(crate) mod resolve;
