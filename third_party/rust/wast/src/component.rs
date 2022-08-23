//! Types and support for parsing the component model text format.

mod alias;
mod binary;
mod component;
mod custom;
mod expand;
mod export;
mod func;
mod import;
mod instance;
mod item_ref;
mod module;
mod resolve;
mod types;
mod wast;

pub use self::alias::*;
pub use self::component::*;
pub use self::custom::*;
pub use self::export::*;
pub use self::func::*;
pub use self::import::*;
pub use self::instance::*;
pub use self::item_ref::*;
pub use self::module::*;
pub use self::types::*;
pub use self::wast::*;
