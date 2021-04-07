use {
    crate::{align_down, align_up, error::MapError},
    alloc::sync::Arc,
    core::{
        convert::TryFrom as _,
        ptr::{copy_nonoverlapping, NonNull},
        // sync::atomic::{AtomicU8, Ordering::*},
    },
    gpu_alloc_types::{MappedMemoryRange, MemoryDevice, MemoryPropertyFlags},
};

#[derive(Debug)]
struct Relevant;

impl Drop for Relevant {
    #[cfg(feature = "tracing")]
    fn drop(&mut self) {
        #[cfg(feature = "std")]
        {
            if std::thread::panicking() {
                return;
            }
        }
        tracing::error!("Memory block wasn't deallocated");
    }

    #[cfg(all(not(feature = "tracing"), feature = "std"))]
    fn drop(&mut self) {
        if std::thread::panicking() {
            return;
        }
        eprintln!("Memory block wasn't deallocated")
    }

    #[cfg(all(not(feature = "tracing"), not(feature = "std")))]
    fn drop(&mut self) {
        panic!("Memory block wasn't deallocated")
    }
}

/// Memory block allocated by `GpuAllocator`.
#[derive(Debug)]
pub struct MemoryBlock<M> {
    memory_type: u32,
    props: MemoryPropertyFlags,
    offset: u64,
    size: u64,
    atom_mask: u64,
    mapped: bool,
    flavor: MemoryBlockFlavor<M>,
    relevant: Relevant,
}

impl<M> MemoryBlock<M> {
    pub(crate) fn new(
        memory_type: u32,
        props: MemoryPropertyFlags,
        offset: u64,
        size: u64,
        atom_mask: u64,
        flavor: MemoryBlockFlavor<M>,
    ) -> Self {
        isize::try_from(atom_mask).expect("`atom_mask` is too large");
        MemoryBlock {
            memory_type,
            props,
            offset,
            size,
            atom_mask,
            flavor,
            mapped: false,
            relevant: Relevant,
        }
    }

    pub(crate) fn deallocate(self) -> MemoryBlockFlavor<M> {
        core::mem::forget(self.relevant);
        self.flavor
    }
}

unsafe impl<M> Sync for MemoryBlock<M> where M: Sync {}
unsafe impl<M> Send for MemoryBlock<M> where M: Send {}

#[derive(Debug)]
pub(crate) enum MemoryBlockFlavor<M> {
    Dedicated {
        memory: M,
    },
    Linear {
        chunk: u64,
        ptr: Option<NonNull<u8>>,
        memory: Arc<M>,
    },
    Buddy {
        chunk: usize,
        index: usize,
        ptr: Option<NonNull<u8>>,
        memory: Arc<M>,
    },
    #[cfg(feature = "freelist")]
    FreeList {
        chunk: u64,
        ptr: Option<NonNull<u8>>,
        memory: Arc<M>,
    },
}

impl<M> MemoryBlock<M> {
    /// Returns reference to parent memory object.
    #[inline(always)]
    pub fn memory(&self) -> &M {
        match &self.flavor {
            MemoryBlockFlavor::Dedicated { memory } => memory,
            MemoryBlockFlavor::Buddy { memory, .. } => &**memory,
            MemoryBlockFlavor::Linear { memory, .. } => &**memory,

            #[cfg(feature = "freelist")]
            MemoryBlockFlavor::FreeList { memory, .. } => &**memory,
        }
    }

    /// Returns offset in bytes from start of memory object to start of this block.
    #[inline(always)]
    pub fn offset(&self) -> u64 {
        self.offset
    }

    /// Returns size of this memory block.
    #[inline(always)]
    pub fn size(&self) -> u64 {
        self.size
    }

    /// Returns memory property flags for parent memory object.
    #[inline(always)]
    pub fn props(&self) -> MemoryPropertyFlags {
        self.props
    }

    /// Returns index of type of parent memory object.
    #[inline(always)]
    pub fn memory_type(&self) -> u32 {
        self.memory_type
    }

    /// Returns pointer to mapped memory range of this block.
    /// This blocks becomes mapped.
    ///
    /// The user of returned pointer must guarantee that any previously submitted command that writes to this range has completed
    /// before the host reads from or writes to that range,
    /// and that any previously submitted command that reads from that range has completed
    /// before the host writes to that region.
    /// If the device memory was allocated without the `HOST_COHERENT` property flag set,
    /// these guarantees must be made for an extended range:
    /// the user must round down the start of the range to the nearest multiple of `non_coherent_atom_size`,
    /// and round the end of the range up to the nearest multiple of `non_coherent_atom_size`.
    ///
    /// # Panics
    ///
    /// This function panics if block is currently mapped.
    ///
    /// # Safety
    ///
    /// `block` must have been allocated from specified `device`.
    #[inline(always)]
    pub unsafe fn map(
        &mut self,
        device: &impl MemoryDevice<M>,
        offset: u64,
        size: usize,
    ) -> Result<NonNull<u8>, MapError> {
        let size_u64 = u64::try_from(size).expect("`size` doesn't fit device address space");
        assert!(offset < self.size, "`offset` is out of memory block bounds");
        assert!(
            size_u64 <= self.size - offset,
            "`offset + size` is out of memory block bounds"
        );

        let ptr = match &mut self.flavor {
            MemoryBlockFlavor::Dedicated { memory } => {
                let end = align_up(offset + size_u64, self.atom_mask)
                    .expect("mapping end doesn't fit device address space");
                let aligned_offset = align_down(offset, self.atom_mask);

                if !acquire_mapping(&mut self.mapped) {
                    return Err(MapError::AlreadyMapped);
                }
                let result =
                    device.map_memory(memory, self.offset + aligned_offset, end - aligned_offset);

                match result {
                    // the overflow is checked in `Self::new()`
                    Ok(ptr) => {
                        let ptr_offset = (offset - aligned_offset) as isize;
                        ptr.as_ptr().offset(ptr_offset)
                    }
                    Err(err) => {
                        release_mapping(&mut self.mapped);
                        return Err(err.into());
                    }
                }
            }

            MemoryBlockFlavor::Linear { ptr: Some(ptr), .. }
            | MemoryBlockFlavor::Buddy { ptr: Some(ptr), .. } => {
                if !acquire_mapping(&mut self.mapped) {
                    return Err(MapError::AlreadyMapped);
                }
                let offset_isize = isize::try_from(offset)
                    .expect("Buddy and linear block should fit host address space");
                ptr.as_ptr().offset(offset_isize)
            }
            #[cfg(feature = "freelist")]
            MemoryBlockFlavor::FreeList { ptr: Some(ptr), .. } => {
                if !acquire_mapping(&mut self.mapped) {
                    return Err(MapError::AlreadyMapped);
                }
                let offset_isize = isize::try_from(offset)
                    .expect("Buddy and linear block should fit host address space");
                ptr.as_ptr().offset(offset_isize)
            }
            _ => return Err(MapError::NonHostVisible),
        };

        Ok(NonNull::new_unchecked(ptr))
    }

    /// Unmaps memory range of this block that was previously mapped with `Block::map`.
    /// This block becomes unmapped.
    ///
    /// # Panics
    ///
    /// This function panics if this block is not currently mapped.
    ///
    /// # Safety
    ///
    /// `block` must have been allocated from specified `device`.
    #[inline(always)]
    pub unsafe fn unmap(&mut self, device: &impl MemoryDevice<M>) -> bool {
        if !release_mapping(&mut self.mapped) {
            return false;
        }
        match &mut self.flavor {
            MemoryBlockFlavor::Dedicated { memory } => {
                device.unmap_memory(memory);
            }
            MemoryBlockFlavor::Linear { .. } => {}
            MemoryBlockFlavor::Buddy { .. } => {}

            #[cfg(feature = "freelist")]
            MemoryBlockFlavor::FreeList { .. } => {}
        }
        true
    }

    /// Transiently maps block memory range and copies specified data
    /// to the mapped memory range.
    ///
    /// # Panics
    ///
    /// This function panics if block is currently mapped.
    ///
    /// # Safety
    ///
    /// `block` must have been allocated from specified `device`.
    /// The caller must guarantee that any previously submitted command that reads or writes to this range has completed.
    #[inline(always)]
    pub unsafe fn write_bytes(
        &mut self,
        device: &impl MemoryDevice<M>,
        offset: u64,
        data: &[u8],
    ) -> Result<(), MapError> {
        let size = data.len();
        let ptr = self.map(device, offset, size)?;

        copy_nonoverlapping(data.as_ptr(), ptr.as_ptr(), size);
        let result = if !self.coherent() {
            let aligned_offset = align_down(offset, self.atom_mask);
            let end = align_up(offset + data.len() as u64, self.atom_mask).unwrap();

            device.flush_memory_ranges(&[MappedMemoryRange {
                memory: self.memory(),
                offset: self.offset + aligned_offset,
                size: end - aligned_offset,
            }])
        } else {
            Ok(())
        };

        self.unmap(device);
        result.map_err(Into::into)
    }

    /// Transiently maps block memory range and copies specified data
    /// from the mapped memory range.
    ///
    /// # Panics
    ///
    /// This function panics if block is currently mapped.
    ///
    /// # Safety
    ///
    /// `block` must have been allocated from specified `device`.
    /// The caller must guarantee that any previously submitted command that reads to this range has completed.
    #[inline(always)]
    pub unsafe fn read_bytes(
        &mut self,
        device: &impl MemoryDevice<M>,
        offset: u64,
        data: &mut [u8],
    ) -> Result<(), MapError> {
        #[cfg(feature = "tracing")]
        {
            if !self.cached() {
                tracing::warn!("Reading from non-cached memory may be slow. Consider allocating HOST_CACHED memory block for host reads.")
            }
        }

        let size = data.len();
        let ptr = self.map(device, offset, size)?;
        let result = if !self.coherent() {
            let aligned_offset = align_down(offset, self.atom_mask);
            let end = align_up(offset + data.len() as u64, self.atom_mask).unwrap();

            device.invalidate_memory_ranges(&[MappedMemoryRange {
                memory: self.memory(),
                offset: self.offset + aligned_offset,
                size: end - aligned_offset,
            }])
        } else {
            Ok(())
        };
        if result.is_ok() {
            copy_nonoverlapping(ptr.as_ptr(), data.as_mut_ptr(), size);
        }

        self.unmap(device);
        result.map_err(Into::into)
    }

    fn coherent(&self) -> bool {
        self.props.contains(MemoryPropertyFlags::HOST_COHERENT)
    }

    #[cfg(feature = "tracing")]
    fn cached(&self) -> bool {
        self.props.contains(MemoryPropertyFlags::HOST_CACHED)
    }
}

fn acquire_mapping(mapped: &mut bool) -> bool {
    if *mapped {
        false
    } else {
        *mapped = true;
        true
    }
}

fn release_mapping(mapped: &mut bool) -> bool {
    if *mapped {
        *mapped = false;
        true
    } else {
        false
    }
}
