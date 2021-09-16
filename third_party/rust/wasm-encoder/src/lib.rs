//! A WebAssembly encoder.
//!
//! The main builder is the [`Module`]. You can build a section with a
//! section-specific builder, like [`TypeSection`] or [`ImportSection`], and
//! then add it to the module with [`Module::section`]. When you are finished
//! building the module, call either [`Module::as_slice`] or [`Module::finish`]
//! to get the encoded bytes. The former gives a shared reference to the
//! underlying bytes as a slice, while the latter gives you ownership of them as
//! a vector.
//!
//! # Example
//!
//! If we wanted to build this module:
//!
//! ```wasm
//! (module
//!   (type (func (param i32 i32) (result i32)))
//!   (func (type 0)
//!     local.get 0
//!     local.get 1
//!     i32.add)
//!   (export "f" (func 0)))
//! ```
//!
//! then we would do this:
//!
//! ```
//! use wasm_encoder::{
//!     CodeSection, Export, ExportSection, Function, FunctionSection, Instruction,
//!     Module, TypeSection, ValType,
//! };
//!
//! let mut module = Module::new();
//!
//! // Encode the type section.
//! let mut types = TypeSection::new();
//! let params = vec![ValType::I32, ValType::I32];
//! let results = vec![ValType::I32];
//! types.function(params, results);
//! module.section(&types);
//!
//! // Encode the function section.
//! let mut functions = FunctionSection::new();
//! let type_index = 0;
//! functions.function(type_index);
//! module.section(&functions);
//!
//! // Encode the export section.
//! let mut exports = ExportSection::new();
//! exports.export("f", Export::Function(0));
//! module.section(&exports);
//!
//! // Encode the code section.
//! let mut codes = CodeSection::new();
//! let locals = vec![];
//! let mut f = Function::new(locals);
//! f.instruction(Instruction::LocalGet(0));
//! f.instruction(Instruction::LocalGet(1));
//! f.instruction(Instruction::I32Add);
//! f.instruction(Instruction::End);
//! codes.function(&f);
//! module.section(&codes);
//!
//! // Extract the encoded Wasm bytes for this module.
//! let wasm_bytes = module.finish();
//!
//! // We generated a valid Wasm module!
//! assert!(wasmparser::validate(&wasm_bytes).is_ok());
//! ```

#![deny(missing_docs, missing_debug_implementations)]

mod aliases;
mod code;
mod custom;
mod data;
mod elements;
mod exports;
mod functions;
mod globals;
mod imports;
mod instances;
mod linking;
mod memories;
mod modules;
mod start;
mod tables;
mod types;

pub use aliases::*;
pub use code::*;
pub use custom::*;
pub use data::*;
pub use elements::*;
pub use exports::*;
pub use functions::*;
pub use globals::*;
pub use imports::*;
pub use instances::*;
pub use linking::*;
pub use memories::*;
pub use modules::*;
pub use start::*;
pub use tables::*;
pub use types::*;

pub mod encoders;

use std::convert::TryFrom;

/// A Wasm module that is being encoded.
#[derive(Clone, Debug)]
pub struct Module {
    bytes: Vec<u8>,
}

/// A WebAssembly section.
///
/// Various builders defined in this crate already implement this trait, but you
/// can also implement it yourself for your own custom section builders, or use
/// `RawSection` to use a bunch of raw bytes as a section.
pub trait Section {
    /// This section's id.
    ///
    /// See `SectionId` for known section ids.
    fn id(&self) -> u8;

    /// Write this section's data and data length prefix into the given sink.
    fn encode<S>(&self, sink: &mut S)
    where
        S: Extend<u8>;
}

/// A section made up of uninterpreted, raw bytes.
///
/// Allows you to splat any data into a Wasm section.
#[derive(Clone, Copy, Debug)]
pub struct RawSection<'a> {
    /// The id for this section.
    pub id: u8,
    /// The raw data for this section.
    pub data: &'a [u8],
}

impl Section for RawSection<'_> {
    fn id(&self) -> u8 {
        self.id
    }

    fn encode<S>(&self, sink: &mut S)
    where
        S: Extend<u8>,
    {
        sink.extend(
            encoders::u32(u32::try_from(self.data.len()).unwrap()).chain(self.data.iter().copied()),
        );
    }
}

impl Module {
    /// Begin writing a new `Module`.
    #[rustfmt::skip]
    pub fn new() -> Self {
        Module {
            bytes: vec![
                // Magic
                0x00, 0x61, 0x73, 0x6D,
                // Version
                0x01, 0x00, 0x00, 0x00,
            ],
        }
    }

    /// Write a section into this module.
    ///
    /// It is your responsibility to define the sections in the [proper
    /// order](https://webassembly.github.io/spec/core/binary/modules.html#binary-module),
    /// and to ensure that each kind of section (other than custom sections) is
    /// only defined once. While this is a potential footgun, it also allows you
    /// to use this crate to easily construct test cases for bad Wasm module
    /// encodings.
    pub fn section(&mut self, section: &impl Section) -> &mut Self {
        self.bytes.push(section.id());
        section.encode(&mut self.bytes);
        self
    }

    /// Get the encoded Wasm module as a slice.
    pub fn as_slice(&self) -> &[u8] {
        &self.bytes
    }

    /// Finish writing this Wasm module and extract ownership of the encoded
    /// bytes.
    pub fn finish(self) -> Vec<u8> {
        self.bytes
    }
}

/// Known section IDs.
///
/// Useful for implementing the `Section` trait, or for setting
/// `RawSection::id`.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]
#[repr(u8)]
#[allow(missing_docs)]
pub enum SectionId {
    Custom = 0,
    Type = 1,
    Import = 2,
    Function = 3,
    Table = 4,
    Memory = 5,
    Global = 6,
    Export = 7,
    Start = 8,
    Element = 9,
    Code = 10,
    Data = 11,
    DataCount = 12,
    Module = 14,
    Instance = 15,
    Alias = 16,
}

impl From<SectionId> for u8 {
    #[inline]
    fn from(id: SectionId) -> u8 {
        id as u8
    }
}

/// The type of a value.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum ValType {
    /// The `i32` type.
    I32 = 0x7F,
    /// The `i64` type.
    I64 = 0x7E,
    /// The `f32` type.
    F32 = 0x7D,
    /// The `f64` type.
    F64 = 0x7C,
    /// The `v128` type.
    ///
    /// Part of the SIMD proposal.
    V128 = 0x7B,
    /// The `funcref` type.
    ///
    /// Part of the reference types proposal when used anywhere other than a
    /// table's element type.
    FuncRef = 0x70,
    /// The `externref` type.
    ///
    /// Part of the reference types proposal.
    ExternRef = 0x6F,
}

impl From<ValType> for u8 {
    #[inline]
    fn from(t: ValType) -> u8 {
        t as u8
    }
}
