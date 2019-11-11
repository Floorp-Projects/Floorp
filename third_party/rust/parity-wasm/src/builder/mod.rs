//! Various builders to generate/alter wasm components

mod invoke;
mod module;
mod code;
mod misc;
mod import;
mod memory;
mod table;
mod export;
mod global;
mod data;

pub use self::code::{
	signatures, signature, function, SignatureBuilder, SignaturesBuilder,
	FunctionBuilder, TypeRefBuilder, FuncBodyBuilder, FunctionDefinition,
};
pub use self::data::DataSegmentBuilder;
pub use self::export::{export, ExportBuilder, ExportInternalBuilder};
pub use self::global::{global, GlobalBuilder};
pub use self::import::{import, ImportBuilder};
pub use self::invoke::Identity;
pub use self::memory::MemoryBuilder;
pub use self::module::{module, from_module, ModuleBuilder};
pub use self::table::{TableBuilder, TableDefinition, TableEntryDefinition};
