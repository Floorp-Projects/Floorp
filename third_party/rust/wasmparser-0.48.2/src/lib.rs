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

pub use crate::binary_reader::BinaryReader;
pub use crate::binary_reader::Range;
use crate::binary_reader::SectionHeader;

pub use crate::parser::ElemSectionEntryTable;
pub use crate::parser::LocalName;
pub use crate::parser::NameEntry;
pub use crate::parser::Parser;
pub use crate::parser::ParserInput;
pub use crate::parser::ParserState;
pub use crate::parser::RelocEntry;
pub use crate::parser::WasmDecoder;

pub use crate::primitives::BinaryReaderError;
pub use crate::primitives::BrTable;
pub use crate::primitives::CustomSectionKind;
pub use crate::primitives::ExternalKind;
pub use crate::primitives::FuncType;
pub use crate::primitives::GlobalType;
pub use crate::primitives::Ieee32;
pub use crate::primitives::Ieee64;
pub use crate::primitives::ImportSectionEntryType;
pub use crate::primitives::LinkingType;
pub use crate::primitives::MemoryImmediate;
pub use crate::primitives::MemoryType;
pub use crate::primitives::NameType;
pub use crate::primitives::Naming;
pub use crate::primitives::Operator;
pub use crate::primitives::RelocType;
pub use crate::primitives::ResizableLimits;
pub use crate::primitives::Result;
pub use crate::primitives::SectionCode;
pub use crate::primitives::TableType;
pub use crate::primitives::Type;
pub use crate::primitives::TypeOrFuncType;
pub use crate::primitives::V128;

pub use crate::validator::validate;
pub use crate::validator::validate_function_body;
pub use crate::validator::ValidatingOperatorParser;
pub use crate::validator::ValidatingParser;
pub use crate::validator::ValidatingParserConfig;

pub use crate::module_resources::WasmFuncType;
pub use crate::module_resources::WasmGlobalType;
pub use crate::module_resources::WasmMemoryType;
pub use crate::module_resources::WasmModuleResources;
pub use crate::module_resources::WasmTableType;
pub use crate::module_resources::WasmType;

pub(crate) use crate::module_resources::{wasm_func_type_inputs, wasm_func_type_outputs};

pub use crate::operators_validator::OperatorValidatorConfig;

pub use crate::readers::CodeSectionReader;
pub use crate::readers::CustomSectionContent;
pub use crate::readers::Data;
pub use crate::readers::DataKind;
pub use crate::readers::DataSectionReader;
pub use crate::readers::Element;
pub use crate::readers::ElementItem;
pub use crate::readers::ElementItems;
pub use crate::readers::ElementItemsReader;
pub use crate::readers::ElementKind;
pub use crate::readers::ElementSectionReader;
pub use crate::readers::Export;
pub use crate::readers::ExportSectionReader;
pub use crate::readers::FunctionBody;
pub use crate::readers::FunctionSectionReader;
pub use crate::readers::Global;
pub use crate::readers::GlobalSectionReader;
pub use crate::readers::Import;
pub use crate::readers::ImportSectionReader;
pub use crate::readers::InitExpr;
pub use crate::readers::LinkingSectionReader;
pub use crate::readers::LocalsReader;
pub use crate::readers::MemorySectionReader;
pub use crate::readers::ModuleReader;
pub use crate::readers::Name;
pub use crate::readers::NameSectionReader;
pub use crate::readers::NamingReader;
pub use crate::readers::OperatorsReader;
pub use crate::readers::ProducersField;
pub use crate::readers::ProducersFieldValue;
pub use crate::readers::ProducersFieldValuesReader;
pub use crate::readers::ProducersSectionReader;
pub use crate::readers::Reloc;
pub use crate::readers::RelocSectionReader;
pub use crate::readers::Section;
pub use crate::readers::SectionContent;
pub use crate::readers::SectionIterator;
pub use crate::readers::SectionIteratorLimited;
pub use crate::readers::SectionReader;
pub use crate::readers::SectionWithLimitedItems;
pub use crate::readers::TableSectionReader;
pub use crate::readers::TypeSectionReader;

mod binary_reader;
mod limits;
mod module_resources;
mod operators_validator;
mod parser;
mod primitives;
mod readers;
mod tests;
mod validator;
