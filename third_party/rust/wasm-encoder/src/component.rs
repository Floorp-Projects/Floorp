mod aliases;
mod canonicals;
mod components;
mod exports;
mod imports;
mod instances;
mod modules;
mod start;
mod types;

pub use self::aliases::*;
pub use self::canonicals::*;
pub use self::components::*;
pub use self::exports::*;
pub use self::imports::*;
pub use self::instances::*;
pub use self::modules::*;
pub use self::start::*;
pub use self::types::*;

use crate::{CustomSection, Encode};

// Core sorts extended by the component model
const CORE_TYPE_SORT: u8 = 0x10;
const CORE_MODULE_SORT: u8 = 0x11;
const CORE_INSTANCE_SORT: u8 = 0x12;

const CORE_SORT: u8 = 0x00;
const FUNCTION_SORT: u8 = 0x01;
const VALUE_SORT: u8 = 0x02;
const TYPE_SORT: u8 = 0x03;
const COMPONENT_SORT: u8 = 0x04;
const INSTANCE_SORT: u8 = 0x05;

/// A WebAssembly component section.
///
/// Various builders defined in this crate already implement this trait, but you
/// can also implement it yourself for your own custom section builders, or use
/// `RawSection` to use a bunch of raw bytes as a section.
pub trait ComponentSection: Encode {
    /// Gets the section identifier for this section.
    fn id(&self) -> u8;
}

/// Known section identifiers of WebAssembly components.
///
/// These sections are supported by the component model proposal.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]
#[repr(u8)]
pub enum ComponentSectionId {
    /// The section is a core custom section.
    CoreCustom = 0,
    /// The section is a core module section.
    CoreModule = 1,
    /// The section is a core instance section.
    CoreInstance = 2,
    /// The section is a core alias section.
    CoreAlias = 3,
    /// The section is a core type section.
    CoreType = 4,
    /// The section is a component section.
    Component = 5,
    /// The section is an instance section.
    Instance = 6,
    /// The section is an alias section.
    Alias = 7,
    /// The section is a type section.
    Type = 8,
    /// The section is a canonical function section.
    CanonicalFunction = 9,
    /// The section is a start section.
    Start = 10,
    /// The section is an import section.
    Import = 11,
    /// The section is an export section.
    Export = 12,
}

impl From<ComponentSectionId> for u8 {
    #[inline]
    fn from(id: ComponentSectionId) -> u8 {
        id as u8
    }
}

impl Encode for ComponentSectionId {
    fn encode(&self, sink: &mut Vec<u8>) {
        sink.push(*self as u8);
    }
}

/// Represents a WebAssembly component that is being encoded.
///
/// Unlike core WebAssembly modules, the sections of a component
/// may appear in any order and may be repeated.
///
/// Components may also added as a section to other components.
#[derive(Clone, Debug)]
pub struct Component {
    bytes: Vec<u8>,
}

impl Component {
    /// Begin writing a new `Component`.
    pub fn new() -> Self {
        Self {
            bytes: vec![
                0x00, 0x61, 0x73, 0x6D, // magic (`\0asm`)
                0x0a, 0x00, 0x01, 0x00, // version
            ],
        }
    }

    /// Finish writing this component and extract ownership of the encoded bytes.
    pub fn finish(self) -> Vec<u8> {
        self.bytes
    }

    /// Write a section to this component.
    pub fn section(&mut self, section: &impl ComponentSection) -> &mut Self {
        self.bytes.push(section.id());
        section.encode(&mut self.bytes);
        self
    }
}

impl Default for Component {
    fn default() -> Self {
        Self::new()
    }
}

impl ComponentSection for CustomSection<'_> {
    fn id(&self) -> u8 {
        ComponentSectionId::CoreCustom.into()
    }
}
