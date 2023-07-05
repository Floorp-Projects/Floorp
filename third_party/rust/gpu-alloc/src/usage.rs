use {
    core::fmt::{self, Debug},
    gpu_alloc_types::{MemoryPropertyFlags, MemoryType},
};

bitflags::bitflags! {
    /// Memory usage type.
    /// Bits set define intended usage for requested memory.
    #[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
    #[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
    pub struct UsageFlags: u8 {
        /// Hints for allocator to find memory with faster device access.
        /// If no flags is specified than `FAST_DEVICE_ACCESS` is implied.
        const FAST_DEVICE_ACCESS = 0x01;

        /// Memory will be accessed from host.
        /// This flags guarantees that host memory operations will be available.
        /// Otherwise implementation is encouraged to use non-host-accessible memory.
        const HOST_ACCESS = 0x02;

        /// Hints allocator that memory will be used for data downloading.
        /// Allocator will strongly prefer host-cached memory.
        /// Implies `HOST_ACCESS` flag.
        const DOWNLOAD = 0x04;

        /// Hints allocator that memory will be used for data uploading.
        /// If `DOWNLOAD` flag is not set then allocator will assume that
        /// host will access memory in write-only manner and may
        /// pick not host-cached.
        /// Implies `HOST_ACCESS` flag.
        const UPLOAD = 0x08;

        /// Hints allocator that memory will be used for short duration
        /// allowing to use faster algorithm with less memory overhead.
        /// If use holds returned memory block for too long then
        /// effective memory overhead increases instead.
        /// Best use case is for staging buffer for single batch of operations.
        const TRANSIENT = 0x10;

        /// Requests memory that can be addressed with `u64`.
        /// Allows fetching device address for resources bound to that memory.
        const DEVICE_ADDRESS = 0x20;
    }
}

#[derive(Clone, Copy, Debug)]
struct MemoryForOneUsage {
    mask: u32,
    types: [u32; 32],
    types_count: u32,
}

pub(crate) struct MemoryForUsage {
    usages: [MemoryForOneUsage; 64],
}

impl Debug for MemoryForUsage {
    fn fmt(&self, fmt: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt.debug_struct("MemoryForUsage")
            .field("usages", &&self.usages[..])
            .finish()
    }
}

impl MemoryForUsage {
    pub fn new(memory_types: &[MemoryType]) -> Self {
        assert!(
            memory_types.len() <= 32,
            "Only up to 32 memory types supported"
        );

        let mut mfu = MemoryForUsage {
            usages: [MemoryForOneUsage {
                mask: 0,
                types: [0; 32],
                types_count: 0,
            }; 64],
        };

        for usage in 0..64 {
            mfu.usages[usage as usize] =
                one_usage(UsageFlags::from_bits_truncate(usage), memory_types);
        }

        mfu
    }

    /// Returns mask with bits set for memory type indices that support the
    /// usage.
    pub fn mask(&self, usage: UsageFlags) -> u32 {
        self.usages[usage.bits() as usize].mask
    }

    /// Returns slice of memory type indices that support the usage.
    /// Earlier memory type has priority over later.
    pub fn types(&self, usage: UsageFlags) -> &[u32] {
        let usage = &self.usages[usage.bits() as usize];
        &usage.types[..usage.types_count as usize]
    }
}

fn one_usage(usage: UsageFlags, memory_types: &[MemoryType]) -> MemoryForOneUsage {
    let mut types = [0; 32];
    let mut types_count = 0;

    for (index, mt) in memory_types.iter().enumerate() {
        if compatible(usage, mt.props) {
            types[types_count as usize] = index as u32;
            types_count += 1;
        }
    }

    types[..types_count as usize]
        .sort_unstable_by_key(|&index| reverse_priority(usage, memory_types[index as usize].props));

    let mask = types[..types_count as usize]
        .iter()
        .fold(0u32, |mask, index| mask | 1u32 << index);

    MemoryForOneUsage {
        mask,
        types,
        types_count,
    }
}

fn compatible(usage: UsageFlags, flags: MemoryPropertyFlags) -> bool {
    type Flags = MemoryPropertyFlags;
    if flags.contains(Flags::LAZILY_ALLOCATED) || flags.contains(Flags::PROTECTED) {
        // Unsupported
        false
    } else if usage.intersects(UsageFlags::HOST_ACCESS | UsageFlags::UPLOAD | UsageFlags::DOWNLOAD)
    {
        // Requires HOST_VISIBLE
        flags.contains(Flags::HOST_VISIBLE)
    } else {
        true
    }
}

/// Returns reversed priority of memory with specified flags for specified usage.
/// Lesser value returned = more prioritized.
fn reverse_priority(usage: UsageFlags, flags: MemoryPropertyFlags) -> u32 {
    type Flags = MemoryPropertyFlags;

    // Highly prefer device local memory when `FAST_DEVICE_ACCESS` usage is specified
    // or usage is empty.
    let device_local: bool = flags.contains(Flags::DEVICE_LOCAL)
        ^ (usage.is_empty() || usage.contains(UsageFlags::FAST_DEVICE_ACCESS));

    assert!(
        flags.contains(Flags::HOST_VISIBLE)
            || !usage
                .intersects(UsageFlags::HOST_ACCESS | UsageFlags::UPLOAD | UsageFlags::DOWNLOAD)
    );

    // Prefer non-host-visible memory when host access is not required.
    let host_visible: bool = flags.contains(Flags::HOST_VISIBLE)
        ^ usage.intersects(UsageFlags::HOST_ACCESS | UsageFlags::UPLOAD | UsageFlags::DOWNLOAD);

    // Prefer cached memory for downloads.
    // Or non-cached if downloads are not expected.
    let host_cached: bool =
        flags.contains(Flags::HOST_CACHED) ^ usage.contains(UsageFlags::DOWNLOAD);

    // Prefer coherent for both uploads and downloads.
    // Prefer non-coherent if neither flags is set.
    let host_coherent: bool = flags.contains(Flags::HOST_COHERENT)
        ^ (usage.intersects(UsageFlags::UPLOAD | UsageFlags::DOWNLOAD));

    // Each boolean is false if flags are preferred.
    device_local as u32 * 8
        + host_visible as u32 * 4
        + host_cached as u32 * 2
        + host_coherent as u32
}
