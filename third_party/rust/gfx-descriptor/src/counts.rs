use std::{
    cmp::Ordering,
    iter::FromIterator,
    ops::{AddAssign, SubAssign},
};

pub use hal::pso::{
    BufferDescriptorFormat, BufferDescriptorType, DescriptorRangeDesc, DescriptorSetLayoutBinding,
    DescriptorType, ImageDescriptorType,
};

const DESCRIPTOR_TYPES_COUNT: usize = 15;

const DESCRIPTOR_TYPES: [DescriptorType; DESCRIPTOR_TYPES_COUNT] = [
    DescriptorType::Sampler,
    DescriptorType::Image {
        ty: ImageDescriptorType::Sampled { with_sampler: true },
    },
    DescriptorType::Image {
        ty: ImageDescriptorType::Sampled {
            with_sampler: false,
        },
    },
    DescriptorType::Image {
        ty: ImageDescriptorType::Storage { read_only: true },
    },
    DescriptorType::Image {
        ty: ImageDescriptorType::Storage { read_only: false },
    },
    DescriptorType::Buffer {
        ty: BufferDescriptorType::Storage { read_only: true },
        format: BufferDescriptorFormat::Structured {
            dynamic_offset: true,
        },
    },
    DescriptorType::Buffer {
        ty: BufferDescriptorType::Storage { read_only: true },
        format: BufferDescriptorFormat::Structured {
            dynamic_offset: false,
        },
    },
    DescriptorType::Buffer {
        ty: BufferDescriptorType::Storage { read_only: true },
        format: BufferDescriptorFormat::Texel,
    },
    DescriptorType::Buffer {
        ty: BufferDescriptorType::Storage { read_only: false },
        format: BufferDescriptorFormat::Structured {
            dynamic_offset: true,
        },
    },
    DescriptorType::Buffer {
        ty: BufferDescriptorType::Storage { read_only: false },
        format: BufferDescriptorFormat::Structured {
            dynamic_offset: false,
        },
    },
    DescriptorType::Buffer {
        ty: BufferDescriptorType::Storage { read_only: false },
        format: BufferDescriptorFormat::Texel,
    },
    DescriptorType::Buffer {
        ty: BufferDescriptorType::Uniform,
        format: BufferDescriptorFormat::Structured {
            dynamic_offset: true,
        },
    },
    DescriptorType::Buffer {
        ty: BufferDescriptorType::Uniform,
        format: BufferDescriptorFormat::Structured {
            dynamic_offset: false,
        },
    },
    DescriptorType::Buffer {
        ty: BufferDescriptorType::Uniform,
        format: BufferDescriptorFormat::Texel,
    },
    DescriptorType::InputAttachment,
];

fn descriptor_type_index(ty: &DescriptorType) -> usize {
    match ty {
         DescriptorType::Sampler => 0,
        DescriptorType::Image {
            ty: ImageDescriptorType::Sampled { with_sampler: true },
        } => 1,
        DescriptorType::Image {
            ty: ImageDescriptorType::Sampled {
                with_sampler: false,
            },
        } => 2,
        DescriptorType::Image {
            ty: ImageDescriptorType::Storage { read_only: true },
        } => 3,
        DescriptorType::Image {
            ty: ImageDescriptorType::Storage { read_only: false },
        } => 4,
        DescriptorType::Buffer {
            ty: BufferDescriptorType::Storage { read_only: true },
            format: BufferDescriptorFormat::Structured {
                dynamic_offset: true,
            },
        } => 5,
        DescriptorType::Buffer {
            ty: BufferDescriptorType::Storage { read_only: true },
            format: BufferDescriptorFormat::Structured {
                dynamic_offset: false,
            },
        } => 6,
        DescriptorType::Buffer {
            ty: BufferDescriptorType::Storage { read_only: true },
            format: BufferDescriptorFormat::Texel,
        } => 7,
        DescriptorType::Buffer {
            ty: BufferDescriptorType::Storage { read_only: false },
            format: BufferDescriptorFormat::Structured {
                dynamic_offset: true,
            },
        } => 8,
        DescriptorType::Buffer {
            ty: BufferDescriptorType::Storage { read_only: false },
            format: BufferDescriptorFormat::Structured {
                dynamic_offset: false,
            },
        } => 9,
        DescriptorType::Buffer {
            ty: BufferDescriptorType::Storage { read_only: false },
            format: BufferDescriptorFormat::Texel,
        } => 10,
        DescriptorType::Buffer {
            ty: BufferDescriptorType::Uniform,
            format: BufferDescriptorFormat::Structured {
                dynamic_offset: true,
            },
        } => 11,
        DescriptorType::Buffer {
            ty: BufferDescriptorType::Uniform,
            format: BufferDescriptorFormat::Structured {
                dynamic_offset: false,
            },
        } => 12,
        DescriptorType::Buffer {
            ty: BufferDescriptorType::Uniform,
            format: BufferDescriptorFormat::Texel,
        } => 13,
        DescriptorType::InputAttachment => 14,
    }
}

#[test]
fn test_descriptor_types() {
    for (index, ty) in DESCRIPTOR_TYPES.iter().enumerate() {
        assert_eq!(index, descriptor_type_index(ty));
    }
}

/// Number of descriptors per type.
#[derive(Clone, Debug, Default, PartialEq, Eq, Hash)]
pub struct DescriptorCounts {
    counts: [u32; DESCRIPTOR_TYPES_COUNT],
}

impl DescriptorCounts {
	/// Empty descriptor counts.
    pub const EMPTY: Self = DescriptorCounts {
        counts: [0; DESCRIPTOR_TYPES_COUNT],
    };

    /// Add a single layout binding.
    /// Useful when created with `DescriptorCounts::EMPTY`.
    pub fn add_binding(&mut self, binding: DescriptorSetLayoutBinding) {
        self.counts[descriptor_type_index(&binding.ty)] += binding.count as u32;
    }

    /// Iterate through counts yelding descriptor types and their amount.
    pub fn iter(&self) -> impl '_ + Iterator<Item = DescriptorRangeDesc> {
        self.counts
        	.iter()
        	.enumerate()
            .filter(|&(_, count)| *count != 0)
            .map(|(index, count)| DescriptorRangeDesc {
                count: *count as usize,
                ty: DESCRIPTOR_TYPES[index],
            })
    }

    /// Multiply all the counts by a value.
    pub fn multiply(&self, value: u32) -> Self {
        let mut descs = self.clone();
        for c in descs.counts.iter_mut() {
            *c *= value;
        }
        descs
    }
}

impl FromIterator<DescriptorSetLayoutBinding> for DescriptorCounts {
	fn from_iter<T>(iter: T) -> Self where
		T: IntoIterator<Item = DescriptorSetLayoutBinding>
	{
        let mut descs = Self::EMPTY;

        for binding in iter {
            descs.counts[descriptor_type_index(&binding.ty)] += binding.count as u32;
        }

        descs
	}
}

impl PartialOrd for DescriptorCounts {
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

impl AddAssign for DescriptorCounts {
    fn add_assign(&mut self, rhs: Self) {
        for i in 0..DESCRIPTOR_TYPES_COUNT {
            self.counts[i] += rhs.counts[i];
        }
    }
}

impl SubAssign for DescriptorCounts {
    fn sub_assign(&mut self, rhs: Self) {
        for i in 0..DESCRIPTOR_TYPES_COUNT {
            self.counts[i] -= rhs.counts[i];
        }
    }
}
