//! Types and support for parsing the component model text format.

mod alias;
mod component;
mod deftype;
mod export;
mod func;
mod import;
mod instance;
mod intertype;
mod item_ref;
mod module;
mod types;
pub use self::alias::*;
pub use self::component::*;
pub use self::deftype::*;
pub use self::export::*;
pub use self::func::*;
pub use self::import::*;
pub use self::instance::*;
pub use self::intertype::*;
pub use self::item_ref::*;
pub use self::module::*;
pub use self::types::*;

mod binary;
mod expand;
mod resolve;
