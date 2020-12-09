#![deny(bare_trait_objects)]

mod bindings;
pub mod c_api;
pub mod ctx;
pub mod fdentry;
pub mod host;
pub mod hostcalls;
pub mod memory;
pub mod wasm32;

pub use bindings::bindings;
pub use ctx::{WasiCtx, WasiCtxBuilder};

#[macro_use]
extern crate lazy_static;