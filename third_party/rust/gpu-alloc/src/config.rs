/// Configuration for [`GpuAllocator`]
///
/// [`GpuAllocator`]: type.GpuAllocator
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct Config {
    /// Size in bytes of request that will be served by dedicated memory object.
    /// This value should be large enough to not exhaust memory object limit
    /// and not use slow memory object allocation when it is not necessary.
    pub dedicated_threshold: u64,

    /// Size in bytes of request that will be served by dedicated memory object if preferred.
    /// This value should be large enough to not exhaust memory object limit
    /// and not use slow memory object allocation when it is not necessary.
    ///
    /// This won't make much sense if this value is larger than `dedicated_threshold`.
    pub preferred_dedicated_threshold: u64,

    /// Size in bytes of transient memory request that will be served by dedicated memory object.
    /// This value should be large enough to not exhaust memory object limit
    /// and not use slow memory object allocation when it is not necessary.
    ///
    /// This won't make much sense if this value is lesser than `dedicated_threshold`.
    pub transient_dedicated_threshold: u64,

    /// Size in bytes for chunks for linear allocator.
    pub linear_chunk: u64,

    /// Minimal size for buddy allocator.
    pub minimal_buddy_size: u64,

    /// Initial memory object size for buddy allocator.
    /// If less than `minimal_buddy_size` then `minimal_buddy_size` is used instead.
    pub initial_buddy_dedicated_size: u64,
}

impl Config {
    /// Returns default configuration.
    ///
    /// This is not `Default` implementation to discourage usage outside of
    /// prototyping.
    ///
    /// Proper configuration should depend on hardware and intended usage.\
    /// But those values can be used as starting point.\
    /// Note that they can simply not work for some platforms with lesser
    /// memory capacity than today's "modern" GPU (year 2020).
    pub fn i_am_prototyping() -> Self {
        // Assume that today's modern GPU is made of 1024 potatoes.
        let potato = Config::i_am_potato();

        Config {
            dedicated_threshold: potato.dedicated_threshold * 1024,
            preferred_dedicated_threshold: potato.preferred_dedicated_threshold * 1024,
            transient_dedicated_threshold: potato.transient_dedicated_threshold * 1024,
            linear_chunk: potato.linear_chunk * 1024,
            minimal_buddy_size: potato.minimal_buddy_size * 1024,
            initial_buddy_dedicated_size: potato.initial_buddy_dedicated_size * 1024,
        }
    }

    /// Returns default configuration for average sized potato.
    pub fn i_am_potato() -> Self {
        Config {
            dedicated_threshold: 32 * 1024,
            preferred_dedicated_threshold: 1024,
            transient_dedicated_threshold: 128 * 1024,
            linear_chunk: 128 * 1024,
            minimal_buddy_size: 1,
            initial_buddy_dedicated_size: 8 * 1024,
        }
    }
}
