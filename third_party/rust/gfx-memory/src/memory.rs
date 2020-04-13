use crate::{AtomSize, Size};

/// Memory object wrapper.
/// Contains size and properties of the memory.
#[derive(Debug)]
pub struct Memory<B: hal::Backend> {
    raw: B::Memory,
    size: Size,
    properties: hal::memory::Properties,
    pub(crate) non_coherent_atom_size: Option<AtomSize>,
}

impl<B: hal::Backend> Memory<B> {
    /// Get memory properties.
    pub fn properties(&self) -> hal::memory::Properties {
        self.properties
    }

    /// Get memory size.
    pub fn size(&self) -> Size {
        self.size
    }

    /// Get raw memory.
    pub fn raw(&self) -> &B::Memory {
        &self.raw
    }

    /// Unwrap raw memory.
    pub fn into_raw(self) -> B::Memory {
        self.raw
    }

    /// Create memory from raw object.
    ///
    /// # Safety
    ///
    /// TODO:
    pub unsafe fn from_raw(
        raw: B::Memory,
        size: Size,
        properties: hal::memory::Properties,
        non_coherent_atom_size: Option<AtomSize>,
    ) -> Self {
        debug_assert_eq!(
            non_coherent_atom_size.is_some(),
            crate::is_non_coherent_visible(properties),
        );
        Memory {
            properties,
            raw,
            size,
            non_coherent_atom_size,
        }
    }

    /// Check if this memory is host-visible and can be mapped.
    /// `Equivalent to `memory.properties().contains(Properties::CPU_VISIBLE)`.
    pub fn is_mappable(&self) -> bool {
        self.properties
            .contains(hal::memory::Properties::CPU_VISIBLE)
    }
}
