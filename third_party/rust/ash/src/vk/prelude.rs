/// Holds 24 bits in the least significant bits of memory,
/// and 8 bytes in the most significant bits of that memory,
/// occupying a single [`u32`] in total. This is commonly used in
/// [acceleration structure instances] such as
/// [`vk::AccelerationStructureInstanceKHR`][super::AccelerationStructureInstanceKHR],
/// [`vk::AccelerationStructureSRTMotionInstanceNV`][super::AccelerationStructureSRTMotionInstanceNV] and
/// [`vk::AccelerationStructureMatrixMotionInstanceNV`][super::AccelerationStructureMatrixMotionInstanceNV].
///
/// [acceleration structure instances]: https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkAccelerationStructureInstanceKHR.html#_description
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug)]
#[repr(transparent)]
pub struct Packed24_8(u32);

impl Packed24_8 {
    pub fn new(low_24: u32, high_8: u8) -> Self {
        Self((low_24 & 0x00ff_ffff) | (u32::from(high_8) << 24))
    }

    /// Extracts the least-significant 24 bits (3 bytes) of this integer
    pub fn low_24(&self) -> u32 {
        self.0 & 0xffffff
    }

    /// Extracts the most significant 8 bits (single byte) of this integer
    pub fn high_8(&self) -> u8 {
        (self.0 >> 24) as u8
    }
}
