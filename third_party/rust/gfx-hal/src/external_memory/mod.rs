//! Structures related to the import external memory functionality.

mod errors;
pub use errors::*;

pub use external_memory::*;

bitflags!(
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    /// External memory properties.
    pub struct ExternalMemoryProperties: u32 {
        /// The memory can be exported using [Device::export_memory][Device::export_memory].
        const EXPORTABLE = (1 << 0);
        /// The memory can be imported using [Device::import_external_image][Device::import_external_image] and [Device::import_external_buffer][Device::import_external_buffer].
        const IMPORTABLE = (1 << 1);
        /// The memory created using [Device::import_external_image][Device::import_external_image] and [Device::import_external_buffer][Device::import_external_buffer] can be exported using [Device::export_memory][Device::export_memory].
        const EXPORTABLE_FROM_IMPORTED = (1 << 2);
    }
);
