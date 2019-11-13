use std::{
    cmp::Ordering,
    ops::{Add, AddAssign, Mul, MulAssign, Sub, SubAssign},
};

pub use gfx_hal::pso::{DescriptorRangeDesc, DescriptorSetLayoutBinding, DescriptorType};

const DESCRIPTOR_TYPES_COUNT: usize = 11;

const DESCRIPTOR_TYPES: [DescriptorType; DESCRIPTOR_TYPES_COUNT] = [
    DescriptorType::Sampler,
    DescriptorType::CombinedImageSampler,
    DescriptorType::SampledImage,
    DescriptorType::StorageImage,
    DescriptorType::UniformTexelBuffer,
    DescriptorType::StorageTexelBuffer,
    DescriptorType::UniformBuffer,
    DescriptorType::StorageBuffer,
    DescriptorType::UniformBufferDynamic,
    DescriptorType::StorageBufferDynamic,
    DescriptorType::InputAttachment,
];

/// Number of descriptors per type.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct DescriptorRanges {
    counts: [u32; DESCRIPTOR_TYPES_COUNT],
}

impl DescriptorRanges {
    /// Create new instance without descriptors.
    pub fn zero() -> Self {
        DescriptorRanges {
            counts: [0; DESCRIPTOR_TYPES_COUNT],
        }
    }

    /// Add a single layout binding.
    /// Useful when created with `DescriptorRanges::zero()`.
    pub fn add_binding(&mut self, binding: DescriptorSetLayoutBinding) {
        self.counts[binding.ty as usize] += binding.count as u32;
    }

    /// Iterate through ranges yelding
    /// descriptor types and their amount.
    pub fn iter(&self) -> DescriptorRangesIter<'_> {
        DescriptorRangesIter {
            counts: &self.counts,
            index: 0,
        }
    }

    /// Read as slice.
    pub fn counts(&self) -> &[u32] {
        &self.counts
    }

    /// Read or write as slice.
    pub fn counts_mut(&mut self) -> &mut [u32] {
        &mut self.counts
    }

    /// Calculate ranges from bindings.
    pub fn from_bindings(bindings: &[DescriptorSetLayoutBinding]) -> Self {
        let mut descs = Self::zero();

        for binding in bindings {
            descs.counts[binding.ty as usize] += binding.count as u32;
        }

        descs
    }

    /// Calculate ranges from bindings, specified with an iterator.
    pub fn from_binding_iter<I>(bindings: I) -> Self
    where
        I: Iterator<Item = DescriptorSetLayoutBinding>,
    {
        let mut descs = Self::zero();

        for binding in bindings {
            descs.counts[binding.ty as usize] += binding.count as u32;
        }

        descs
    }
}

impl PartialOrd for DescriptorRanges {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        let mut ord = self.counts[0].partial_cmp(&other.counts[0])?;
        for i in 1..DESCRIPTOR_TYPES_COUNT {
            match (ord, self.counts[i].partial_cmp(&other.counts[i])?) {
                (Ordering::Less, Ordering::Greater) | (Ordering::Greater, Ordering::Less) => {
                    return None;
                }
                (Ordering::Equal, new) => ord = new,
                _ => (),
            }
        }
        Some(ord)
    }
}

impl Add for DescriptorRanges {
    type Output = Self;
    fn add(mut self, rhs: Self) -> Self {
        self += rhs;
        self
    }
}

impl AddAssign for DescriptorRanges {
    fn add_assign(&mut self, rhs: Self) {
        for i in 0..DESCRIPTOR_TYPES_COUNT {
            self.counts[i] += rhs.counts[i];
        }
    }
}

impl Sub for DescriptorRanges {
    type Output = Self;
    fn sub(mut self, rhs: Self) -> Self {
        self -= rhs;
        self
    }
}

impl SubAssign for DescriptorRanges {
    fn sub_assign(&mut self, rhs: Self) {
        for i in 0..DESCRIPTOR_TYPES_COUNT {
            self.counts[i] -= rhs.counts[i];
        }
    }
}

impl Mul<u32> for DescriptorRanges {
    type Output = Self;
    fn mul(mut self, rhs: u32) -> Self {
        self *= rhs;
        self
    }
}

impl MulAssign<u32> for DescriptorRanges {
    fn mul_assign(&mut self, rhs: u32) {
        for i in 0..DESCRIPTOR_TYPES_COUNT {
            self.counts[i] *= rhs;
        }
    }
}

impl<'a> IntoIterator for &'a DescriptorRanges {
    type Item = DescriptorRangeDesc;
    type IntoIter = DescriptorRangesIter<'a>;

    fn into_iter(self) -> DescriptorRangesIter<'a> {
        self.iter()
    }
}

/// Iterator over descriptor ranges.
pub struct DescriptorRangesIter<'a> {
    counts: &'a [u32; DESCRIPTOR_TYPES_COUNT],
    index: u8,
}

impl<'a> Iterator for DescriptorRangesIter<'a> {
    type Item = DescriptorRangeDesc;

    fn next(&mut self) -> Option<DescriptorRangeDesc> {
        loop {
            let index = self.index as usize;
            if index >= DESCRIPTOR_TYPES_COUNT {
                return None;
            } else {
                self.index += 1;
                if self.counts[index] > 0 {
                    return Some(DescriptorRangeDesc {
                        count: self.counts[index] as usize,
                        ty: DESCRIPTOR_TYPES[index],
                    });
                }
            }
        }
    }
}
