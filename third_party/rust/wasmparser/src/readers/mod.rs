/* Copyright 2018 Mozilla Foundation
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

use super::{
    BinaryReader, BinaryReaderError, CustomSectionKind, ExternalKind, FuncType, GlobalType,
    ImportSectionEntryType, LinkingType, MemoryType, NameType, Naming, Operator, Range, RelocType,
    Result, SectionCode, TableType, Type,
};

use super::SectionHeader;

pub use self::code_section::CodeSectionReader;
pub use self::code_section::FunctionBody;
pub use self::code_section::LocalsReader;
use self::data_count_section::read_data_count_section_content;
pub use self::data_section::Data;
pub use self::data_section::DataKind;
pub use self::data_section::DataSectionReader;
pub use self::element_section::Element;
pub use self::element_section::ElementItem;
pub use self::element_section::ElementItems;
pub use self::element_section::ElementItemsReader;
pub use self::element_section::ElementKind;
pub use self::element_section::ElementSectionReader;
pub use self::export_section::Export;
pub use self::export_section::ExportSectionReader;
pub use self::function_section::FunctionSectionReader;
pub use self::global_section::Global;
pub use self::global_section::GlobalSectionReader;
pub use self::import_section::Import;
pub use self::import_section::ImportSectionReader;
pub use self::init_expr::InitExpr;
pub use self::memory_section::MemorySectionReader;
pub use self::module::CustomSectionContent;
pub use self::module::ModuleReader;
pub use self::module::Section;
pub use self::module::SectionContent;
use self::start_section::read_start_section_content;
pub use self::table_section::TableSectionReader;
pub use self::type_section::TypeSectionReader;

pub use self::section_reader::SectionIterator;
pub use self::section_reader::SectionIteratorLimited;
pub use self::section_reader::SectionReader;
pub use self::section_reader::SectionWithLimitedItems;

pub use self::name_section::FunctionName;
pub use self::name_section::LocalName;
pub use self::name_section::ModuleName;
pub use self::name_section::Name;
pub use self::name_section::NameSectionReader;
pub use self::name_section::NamingReader;

pub use self::producers_section::ProducersField;
pub use self::producers_section::ProducersFieldValue;
pub use self::producers_section::ProducersFieldValuesReader;
pub use self::producers_section::ProducersSectionReader;

pub use self::linking_section::LinkingSectionReader;

pub use self::reloc_section::Reloc;
pub use self::reloc_section::RelocSectionReader;

use self::sourcemappingurl_section::read_sourcemappingurl_section_content;

pub use self::operators::OperatorsReader;

mod code_section;
mod data_count_section;
mod data_section;
mod element_section;
mod export_section;
mod function_section;
mod global_section;
mod import_section;
mod init_expr;
mod linking_section;
mod memory_section;
mod module;
mod name_section;
mod operators;
mod producers_section;
mod reloc_section;
mod section_reader;
mod sourcemappingurl_section;
mod start_section;
mod table_section;
mod type_section;
