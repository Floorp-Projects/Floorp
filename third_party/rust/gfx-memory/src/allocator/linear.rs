use crate::{
    allocator::{Allocator, Kind},
    block::Block,
    mapping::MappedRange,
    memory::Memory,
    AtomSize, Size,
};
use hal::{device::Device as _, Backend};
use std::{collections::VecDeque, ops::Range, ptr::NonNull, sync::Arc};

type LineCount = u32;

/// Memory block allocated from `LinearAllocator`.
#[derive(Debug)]
pub struct LinearBlock<B: Backend> {
    memory: Arc<Memory<B>>,
    line_index: LineCount,
    ptr: Option<NonNull<u8>>,
    range: Range<Size>,
}

unsafe impl<B: Backend> Send for LinearBlock<B> {}
unsafe impl<B: Backend> Sync for LinearBlock<B> {}

impl<B: Backend> LinearBlock<B> {
    /// Get the size of this block.
    pub fn size(&self) -> Size {
        self.range.end - self.range.start
    }
}

impl<B: Backend> Block<B> for LinearBlock<B> {
    fn properties(&self) -> hal::memory::Properties {
        self.memory.properties()
    }

    fn memory(&self) -> &B::Memory {
        self.memory.raw()
    }

    fn segment(&self) -> hal::memory::Segment {
        hal::memory::Segment {
            offset: self.range.start,
            size: Some(self.range.end - self.range.start),
        }
    }

    fn map<'a>(
        &'a mut self,
        _device: &B::Device,
        segment: hal::memory::Segment,
    ) -> Result<MappedRange<'a, B>, hal::device::MapError> {
        let requested_range = crate::segment_to_sub_range(segment, &self.range)?;

        let mapping_range = match self.memory.non_coherent_atom_size {
            Some(atom) => crate::align_range(&requested_range, atom),
            None => requested_range.clone(),
        };

        Ok(unsafe {
            MappedRange::from_raw(
                &self.memory,
                self.ptr
                    //TODO: https://github.com/gfx-rs/gfx/issues/3182
                    .ok_or(hal::device::MapError::MappingFailed)?
                    .as_ptr()
                    .offset((mapping_range.start - self.range.start) as isize),
                mapping_range,
                requested_range,
            )
        })
    }
}

/// Config for [`LinearAllocator`].
/// Refer to documentation on [`LinearAllocator`] to better understand what the configuration options mean.
#[derive(Clone, Copy, Debug)]
pub struct LinearConfig {
    /// Size in bytes of each `Line`.
    /// If you try to create an allocation larger then this your allocation will fall back to the general allocator.
    pub line_size: Size,
}

/// The `LinearAllocator` is best suited for short-lived allocations.
/// The allocation strategy has minimal overhead and the implementation is fast.
/// But holding a single block will completely stop memory recycling.
///
/// The linear allocator will internally create multiple lines.
/// Each line is a `gfx_hal::Backend::Memory` from which multiple [`LinearBlock`]s are linearly allocated.
///
/// A new line is created if there is insufficient space to create a [`LinearBlock`] from the current line.
/// New lines are created from scratch or taken from a pool of previously used lines.
/// When lines have no allocated [`LinearBlock`]s remaining they are moved to a pool to be reused.
/// This pool of unused lines is freed only when [`LinearAllocator::clear`] is called.
#[derive(Debug)]
pub struct LinearAllocator<B: Backend> {
    memory_type: hal::MemoryTypeId,
    memory_properties: hal::memory::Properties,
    line_size: Size,
    finished_lines_count: LineCount,
    lines: VecDeque<Line<B>>,
    non_coherent_atom_size: Option<AtomSize>,
    /// Previously used lines that have been replaced, kept around to use next time a new line is needed.
    unused_lines: Vec<Line<B>>,
}

/// If freed >= allocated it is safe to free the line.
#[derive(Debug)]
struct Line<B: Backend> {
    /// Points to the last allocated byte in the line. Only ever increases.
    allocated: Size,
    /// Points to the last freed byte in the line. Only ever increases.
    freed: Size,
    memory: Arc<Memory<B>>,
    ptr: Option<NonNull<u8>>,
}

impl<B: Backend> Line<B> {
    unsafe fn free_memory(self, device: &B::Device) -> Size {
        match Arc::try_unwrap(self.memory) {
            Ok(memory) => {
                log::trace!("Freed `Line` of size {}", memory.size());
                if memory.is_mappable() {
                    device.unmap_memory(memory.raw());
                }

                let freed = memory.size();
                device.free_memory(memory.into_raw());
                freed
            }
            Err(_) => {
                log::error!("Allocated `Line` was freed, but memory is still shared.");
                0
            }
        }
    }
}

unsafe impl<B: Backend> Send for Line<B> {}
unsafe impl<B: Backend> Sync for Line<B> {}

impl<B: Backend> LinearAllocator<B> {
    /// Create new `LinearAllocator`
    /// for `memory_type` with `memory_properties` specified,
    /// with `config`.
    pub fn new(
        memory_type: hal::MemoryTypeId,
        memory_properties: hal::memory::Properties,
        config: LinearConfig,
        non_coherent_atom_size: Size,
    ) -> Self {
        log::trace!(
            "Create new 'linear' allocator: type: '{:?}', properties: '{:#?}' config: '{:#?}'",
            memory_type,
            memory_properties,
            config
        );
        let (line_size, non_coherent_atom_size) =
            if crate::is_non_coherent_visible(memory_properties) {
                let atom = AtomSize::new(non_coherent_atom_size);
                (crate::align_size(config.line_size, atom.unwrap()), atom)
            } else {
                (config.line_size, None)
            };

        LinearAllocator {
            memory_type,
            memory_properties,
            line_size,
            finished_lines_count: 0,
            lines: VecDeque::new(),
            unused_lines: Vec::new(),
            non_coherent_atom_size,
        }
    }

    /// Maximum allocation size.
    pub fn max_allocation(&self) -> Size {
        self.line_size / 2
    }

    fn cleanup(&mut self, device: &B::Device, free_memory: bool) -> Size {
        let mut freed = 0;
        while !self.lines.is_empty() {
            if self.lines[0].allocated > self.lines[0].freed {
                break;
            }

            let line = self.lines.pop_front().unwrap();
            self.finished_lines_count += 1;

            if free_memory {
                unsafe {
                    freed += line.free_memory(device);
                }
            } else if Arc::strong_count(&line.memory) == 1 {
                self.unused_lines.push(line);
            } else {
                log::error!("Allocated `Line` was freed, but memory is still shared.");
            }
        }
        freed
    }

    /// Perform full cleanup of the allocated memory.
    pub fn clear(&mut self, device: &B::Device) -> Size {
        let mut freed = self.cleanup(device, true);

        for line in self.unused_lines.drain(..) {
            freed += self.line_size;
            unsafe {
                line.free_memory(device);
            }
        }

        freed
    }
}

impl<B: Backend> Allocator<B> for LinearAllocator<B> {
    type Block = LinearBlock<B>;

    const KIND: Kind = Kind::Linear;

    fn alloc(
        &mut self,
        device: &B::Device,
        size: Size,
        align: Size,
    ) -> Result<(LinearBlock<B>, Size), hal::device::AllocationError> {
        let (size, align) = match self.non_coherent_atom_size {
            Some(atom) => (
                crate::align_size(size, atom),
                crate::align_size(align, atom),
            ),
            None => (size, align),
        };

        if size > self.line_size || align > self.line_size {
            //TODO: better error here?
            return Err(hal::device::AllocationError::TooManyObjects);
        }

        let lines_count = self.lines.len() as LineCount;
        if let Some(line) = self.lines.back_mut() {
            let aligned_offset =
                crate::align_offset(line.allocated, unsafe { AtomSize::new_unchecked(align) });
            if aligned_offset + size <= self.line_size {
                line.freed += aligned_offset - line.allocated;
                line.allocated = aligned_offset + size;

                let block = LinearBlock {
                    line_index: self.finished_lines_count + lines_count - 1,
                    memory: Arc::clone(&line.memory),
                    ptr: line.ptr.map(|ptr| unsafe {
                        NonNull::new_unchecked(ptr.as_ptr().offset(aligned_offset as isize))
                    }),
                    range: aligned_offset..aligned_offset + size,
                };

                return Ok((block, 0));
            }
        }

        let (line, new_allocation_size) = match self.unused_lines.pop() {
            Some(mut line) => {
                line.allocated = size;
                line.freed = 0;
                (line, 0)
            }
            None => {
                log::trace!("Allocated `Line` of size {}", self.line_size);
                let (memory, ptr) = unsafe {
                    super::allocate_memory_helper(
                        device,
                        self.memory_type,
                        self.line_size,
                        self.memory_properties,
                        self.non_coherent_atom_size,
                    )?
                };

                (
                    Line {
                        allocated: size,
                        freed: 0,
                        ptr,
                        memory: Arc::new(memory),
                    },
                    self.line_size,
                )
            }
        };

        let block = LinearBlock {
            line_index: self.finished_lines_count + lines_count,
            memory: Arc::clone(&line.memory),
            ptr: line.ptr,
            range: 0..size,
        };

        self.lines.push_back(line);
        Ok((block, new_allocation_size))
    }

    fn free(&mut self, device: &B::Device, block: Self::Block) -> Size {
        let index = (block.line_index - self.finished_lines_count) as usize;
        self.lines[index].freed += block.size();
        drop(block);
        self.cleanup(device, false)
    }
}

impl<B: Backend> Drop for LinearAllocator<B> {
    fn drop(&mut self) {
        if !self.lines.is_empty() {
            log::error!("Not all allocations from LinearAllocator were freed");
        }
    }
}
