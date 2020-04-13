//! Defines usage types for memory bocks.
//! See `Usage` and implementations for details.

use hal::memory as m;

/// Scenarios of how resources use memory.
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum MemoryUsage {
    /// Full speed GPU access.
    /// Optimal for render targets and persistent resources.
    /// Avoid memory with host access.
    Private,
    /// CPU to GPU data flow with update commands.
    /// Used for dynamic buffer data, typically constant buffers.
    /// Host access is guaranteed.
    /// Prefers memory with fast GPU access.
    Dynamic {
        /// Optimize for multiple disjoint small portions to be updated,
        /// as opposed to big linear chunks of memory.
        sparse_updates: bool,
    },
    /// CPU to GPU data flow with mapping.
    /// Used for staging data before copying to the `Data` memory.
    /// Host access is guaranteed.
    Staging {
        /// Optimize for reading back from Gpu.
        read_back: bool,
    },
}

impl MemoryUsage {
    /// Set of required memory properties for this usage.
    pub fn properties_required(&self) -> m::Properties {
        match *self {
            MemoryUsage::Private => m::Properties::DEVICE_LOCAL,
            MemoryUsage::Dynamic { .. } | MemoryUsage::Staging { .. } => m::Properties::CPU_VISIBLE,
        }
    }

    pub(crate) fn memory_fitness(&self, properties: m::Properties) -> u32 {
        match *self {
            MemoryUsage::Private => {
                assert!(properties.contains(m::Properties::DEVICE_LOCAL));
                0 | (!properties.contains(m::Properties::CPU_VISIBLE) as u32) << 3
                    | (!properties.contains(m::Properties::LAZILY_ALLOCATED) as u32) << 2
                    | (!properties.contains(m::Properties::CPU_CACHED) as u32) << 1
                    | (!properties.contains(m::Properties::COHERENT) as u32) << 0
            }
            MemoryUsage::Dynamic { sparse_updates } => {
                assert!(properties.contains(m::Properties::CPU_VISIBLE));
                assert!(!properties.contains(m::Properties::LAZILY_ALLOCATED));
                0 | (properties.contains(m::Properties::DEVICE_LOCAL) as u32) << 2
                    | ((properties.contains(m::Properties::COHERENT) == sparse_updates) as u32) << 1
                    | (!properties.contains(m::Properties::CPU_CACHED) as u32) << 0
            }
            MemoryUsage::Staging { read_back } => {
                assert!(properties.contains(m::Properties::CPU_VISIBLE));
                assert!(!properties.contains(m::Properties::LAZILY_ALLOCATED));
                0 | ((properties.contains(m::Properties::CPU_CACHED) == read_back) as u32) << 1
                    | (!properties.contains(m::Properties::DEVICE_LOCAL) as u32) << 0
            }
        }
    }
}
