/* Copyright 2017 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//! A simple event-driven library for parsing WebAssembly binary files
//! (or streams).
//!
//! The parser library reports events as they happend and only stores
//! parsing information for a brief period of time, making it very fast
//! and memory-efficient. The event-driven model, however, has some drawbacks.
//! If you need random access to the entire WebAssembly data-structure,
//! this is not the right library for you. You could however, build such
//! a data-structure using this library.

#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(not(feature = "std"), feature(alloc))]

#[cfg(not(feature = "std"))]
extern crate hashmap_core;

#[cfg(not(feature = "std"))]
#[macro_use]
extern crate alloc;

pub use binary_reader::BinaryReader;
pub use binary_reader::Range;
use binary_reader::SectionHeader;

pub use parser::LocalName;
pub use parser::NameEntry;
pub use parser::Parser;
pub use parser::ParserInput;
pub use parser::ParserState;
pub use parser::RelocEntry;
pub use parser::WasmDecoder;

pub use primitives::BinaryReaderError;
pub use primitives::BrTable;
pub use primitives::CustomSectionKind;
pub use primitives::ExternalKind;
pub use primitives::FuncType;
pub use primitives::GlobalType;
pub use primitives::Ieee32;
pub use primitives::Ieee64;
pub use primitives::ImportSectionEntryType;
pub use primitives::LinkingType;
pub use primitives::MemoryImmediate;
pub use primitives::MemoryType;
pub use primitives::NameType;
pub use primitives::Naming;
pub use primitives::Operator;
pub use primitives::RelocType;
pub use primitives::ResizableLimits;
pub use primitives::Result;
pub use primitives::SectionCode;
pub use primitives::TableType;
pub use primitives::Type;
pub use primitives::V128;

pub use validator::validate;
pub use validator::OperatorValidatorConfig;
pub use validator::ValidatingOperatorParser;
pub use validator::ValidatingParser;
pub use validator::ValidatingParserConfig;
pub use validator::WasmModuleResources;

pub use readers::CodeSectionReader;
pub use readers::Data;
pub use readers::DataKind;
pub use readers::DataSectionReader;
pub use readers::Element;
pub use readers::ElementItems;
pub use readers::ElementItemsReader;
pub use readers::ElementKind;
pub use readers::ElementSectionReader;
pub use readers::Export;
pub use readers::ExportSectionReader;
pub use readers::FunctionBody;
pub use readers::FunctionSectionReader;
pub use readers::Global;
pub use readers::GlobalSectionReader;
pub use readers::Import;
pub use readers::ImportSectionReader;
pub use readers::InitExpr;
pub use readers::LinkingSectionReader;
pub use readers::LocalsReader;
pub use readers::MemorySectionReader;
pub use readers::ModuleReader;
pub use readers::Name;
pub use readers::NameSectionReader;
pub use readers::NamingReader;
pub use readers::OperatorsReader;
pub use readers::ProducersField;
pub use readers::ProducersFieldValue;
pub use readers::ProducersSectionReader;
pub use readers::Reloc;
pub use readers::RelocSectionReader;
pub use readers::Section;
pub use readers::SectionIterator;
pub use readers::SectionIteratorLimited;
pub use readers::SectionReader;
pub use readers::SectionWithLimitedItems;
pub use readers::TableSectionReader;
pub use readers::TypeSectionReader;

mod binary_reader;
mod limits;
mod parser;
mod primitives;
mod readers;
mod tests;
mod validator;

#[cfg(not(feature = "std"))]
mod std {
    pub use alloc::{boxed, vec};
    pub use core::*;
    pub mod collections {
        pub use hashmap_core::HashSet;
    }
}
