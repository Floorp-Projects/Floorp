//! Interface for reading object files.

use crate::alloc::vec::Vec;
use crate::common::{RelocationEncoding, RelocationKind, SectionKind, SymbolKind, SymbolScope};

mod any;
pub use any::*;

mod coff;
pub use coff::*;

mod elf;
pub use elf::*;

mod macho;
pub use macho::*;

mod pe;
pub use pe::*;

mod traits;
pub use traits::*;

#[cfg(feature = "wasm")]
mod wasm;
#[cfg(feature = "wasm")]
pub use wasm::*;

/// The native object file for the target platform.
#[cfg(target_os = "linux")]
pub type NativeFile<'data> = ElfFile<'data>;

/// The native object file for the target platform.
#[cfg(target_os = "macos")]
pub type NativeFile<'data> = MachOFile<'data>;

/// The native object file for the target platform.
#[cfg(target_os = "windows")]
pub type NativeFile<'data> = PeFile<'data>;

/// The native object file for the target platform.
#[cfg(all(feature = "wasm", target_arch = "wasm32"))]
pub type NativeFile<'data> = WasmFile<'data>;

/// The index used to identify a section of a file.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct SectionIndex(pub usize);

/// The index used to identify a symbol of a file.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct SymbolIndex(pub usize);

/// A symbol table entry.
#[derive(Debug)]
pub struct Symbol<'data> {
    name: Option<&'data str>,
    address: u64,
    size: u64,
    kind: SymbolKind,
    section_index: Option<SectionIndex>,
    undefined: bool,
    weak: bool,
    scope: SymbolScope,
}

impl<'data> Symbol<'data> {
    /// Return the kind of this symbol.
    #[inline]
    pub fn kind(&self) -> SymbolKind {
        self.kind
    }

    /// Returns the section index for the section containing this symbol.
    ///
    /// May return `None` if the section is unknown or the symbol is undefined.
    #[inline]
    pub fn section_index(&self) -> Option<SectionIndex> {
        self.section_index
    }

    /// Return true if the symbol is undefined.
    #[inline]
    pub fn is_undefined(&self) -> bool {
        self.undefined
    }

    /// Return true if the symbol is weak.
    #[inline]
    pub fn is_weak(&self) -> bool {
        self.weak
    }

    /// Return true if the symbol visible outside of the compilation unit.
    ///
    /// This treats `SymbolScope::Unknown` as global.
    #[inline]
    pub fn is_global(&self) -> bool {
        !self.is_local()
    }

    /// Return true if the symbol is only visible within the compilation unit.
    #[inline]
    pub fn is_local(&self) -> bool {
        self.scope == SymbolScope::Compilation
    }

    /// Returns the symbol scope.
    #[inline]
    pub fn scope(&self) -> SymbolScope {
        self.scope
    }

    /// The name of the symbol.
    #[inline]
    pub fn name(&self) -> Option<&'data str> {
        self.name
    }

    /// The address of the symbol. May be zero if the address is unknown.
    #[inline]
    pub fn address(&self) -> u64 {
        self.address
    }

    /// The size of the symbol. May be zero if the size is unknown.
    #[inline]
    pub fn size(&self) -> u64 {
        self.size
    }
}

/// A map from addresses to symbols.
#[derive(Debug)]
pub struct SymbolMap<'data> {
    symbols: Vec<Symbol<'data>>,
}

impl<'data> SymbolMap<'data> {
    /// Get the symbol containing the given address.
    pub fn get(&self, address: u64) -> Option<&Symbol<'data>> {
        self.symbols
            .binary_search_by(|symbol| {
                if address < symbol.address {
                    std::cmp::Ordering::Greater
                } else if address < symbol.address + symbol.size {
                    std::cmp::Ordering::Equal
                } else {
                    std::cmp::Ordering::Less
                }
            })
            .ok()
            .and_then(|index| self.symbols.get(index))
    }

    /// Get all symbols in the map.
    pub fn symbols(&self) -> &[Symbol<'data>] {
        &self.symbols
    }

    /// Return true for symbols that should be included in the map.
    fn filter(symbol: &Symbol<'_>) -> bool {
        match symbol.kind() {
            SymbolKind::Unknown | SymbolKind::Text | SymbolKind::Data => {}
            SymbolKind::Null
            | SymbolKind::Section
            | SymbolKind::File
            | SymbolKind::Label
            | SymbolKind::Common
            | SymbolKind::Tls => {
                return false;
            }
        }
        !symbol.is_undefined() && symbol.size() > 0
    }
}

/// The target referenced by a relocation.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum RelocationTarget {
    /// The target is a symbol.
    Symbol(SymbolIndex),
    /// The target is a section.
    Section(SectionIndex),
}

/// A relocation entry.
#[derive(Debug)]
pub struct Relocation {
    kind: RelocationKind,
    encoding: RelocationEncoding,
    size: u8,
    target: RelocationTarget,
    addend: i64,
    implicit_addend: bool,
}

impl Relocation {
    /// The operation used to calculate the result of the relocation.
    #[inline]
    pub fn kind(&self) -> RelocationKind {
        self.kind
    }

    /// Information about how the result of the relocation operation is encoded in the place.
    #[inline]
    pub fn encoding(&self) -> RelocationEncoding {
        self.encoding
    }

    /// The size in bits of the place of the relocation.
    ///
    /// If 0, then the size is determined by the relocation kind.
    #[inline]
    pub fn size(&self) -> u8 {
        self.size
    }

    /// The target of the relocation.
    #[inline]
    pub fn target(&self) -> RelocationTarget {
        self.target
    }

    /// The addend to use in the relocation calculation.
    pub fn addend(&self) -> i64 {
        self.addend
    }

    /// Set the addend to use in the relocation calculation.
    pub fn set_addend(&mut self, addend: i64) {
        self.addend = addend
    }

    /// Returns true if there is an implicit addend stored in the data at the offset
    /// to be relocated.
    pub fn has_implicit_addend(&self) -> bool {
        self.implicit_addend
    }
}

fn data_range(data: &[u8], data_address: u64, range_address: u64, size: u64) -> Option<&[u8]> {
    if range_address >= data_address {
        let start_offset = (range_address - data_address) as usize;
        let end_offset = start_offset + size as usize;
        if end_offset <= data.len() {
            return Some(&data[start_offset..end_offset]);
        }
    }
    None
}
