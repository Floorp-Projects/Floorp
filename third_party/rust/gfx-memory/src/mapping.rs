use {
    crate::{memory::Memory, Size},
    hal::{device::Device as _, Backend},
    std::{iter, ops::Range, ptr::NonNull, slice},
};

#[derive(Debug)]
struct Flush<'a, B: Backend> {
    device: &'a B::Device,
    memory: &'a B::Memory,
    segment: hal::memory::Segment,
}

/// Wrapper structure for a mutable slice with deferred
/// flushing for non-coherent memory.
#[derive(Debug)]
pub struct Writer<'a, 'b, T, B: Backend> {
    /// Wrapped slice.
    pub slice: &'a mut [T],
    flush: Option<Flush<'b, B>>,
}

impl<T, B: Backend> Writer<'_, '_, T, B> {
    /// Dispose of the wrapper and return a bare mapping pointer.
    ///
    /// The segment to flush is returned. The user is responsible
    /// to flush this segment manually.
    pub fn forget(mut self) -> (*mut T, Option<hal::memory::Segment>) {
        (self.slice.as_mut_ptr(), self.flush.take().map(|f| f.segment))
    }
}

impl<'a, 'b, T, B: Backend> Drop for Writer<'a, 'b, T, B> {
    fn drop(&mut self) {
        if let Some(f) = self.flush.take() {
            unsafe {
                f.device
                    .flush_mapped_memory_ranges(iter::once((f.memory, f.segment)))
                    .expect("Should flush successfully")
            };
        }
    }
}

/// Represents range of the memory mapped to the host.
/// Provides methods for safer host access to the memory.
#[derive(Debug)]
pub struct MappedRange<'a, B: Backend> {
    /// Memory object that is mapped.
    memory: &'a Memory<B>,

    /// Pointer to range mapped memory.
    ptr: NonNull<u8>,

    /// Range of mapped memory.
    mapping_range: Range<Size>,

    /// Mapping range requested by caller.
    /// Must be subrange of `mapping_range`.
    requested_range: Range<Size>,
}

impl<'a, B: Backend> MappedRange<'a, B> {
    /// Construct mapped range from raw mapping
    ///
    /// # Safety
    ///
    /// `memory` `range` must be mapped to host memory region pointer by `ptr`.
    /// `range` is in memory object space.
    /// `ptr` points to the `range.start` offset from memory origin.
    pub(crate) unsafe fn from_raw(
        memory: &'a Memory<B>,
        ptr: *mut u8,
        mapping_range: Range<Size>,
        requested_range: Range<Size>,
    ) -> Self {
        debug_assert!(
            mapping_range.start < mapping_range.end,
            "Memory mapping region must have valid size"
        );

        debug_assert!(
            requested_range.start < requested_range.end,
            "Memory mapping region must have valid size"
        );

        match memory.non_coherent_atom_size {
            Some(atom) => {
                debug_assert_eq!((mapping_range.start % atom.get(), mapping_range.end % atom.get()), (0, 0),
                    "Bounds of non-coherent memory mapping ranges must be multiple of `Limits::non_coherent_atom_size`",
                );
                debug_assert!(
                    crate::is_sub_range(&requested_range, &mapping_range),
                    "Requested {:?} must be sub-range of mapping {:?}",
                    requested_range,
                    mapping_range,
                );
            }
            None => {
                debug_assert_eq!(mapping_range, requested_range);
            }
        };

        MappedRange {
            ptr: NonNull::new_unchecked(ptr),
            mapping_range,
            requested_range,
            memory,
        }
    }

    /// Get pointer to beginning of memory region.
    /// i.e. to `range().start` offset from memory origin.
    pub fn ptr(&self) -> NonNull<u8> {
        let offset = (self.requested_range.start - self.mapping_range.start) as isize;
        unsafe { NonNull::new_unchecked(self.ptr.as_ptr().offset(offset)) }
    }

    /// Get mapped range.
    pub fn range(&self) -> Range<Size> {
        self.requested_range.clone()
    }

    /// Return true if the mapped memory is coherent.
    pub fn is_coherent(&self) -> bool {
        self.memory.non_coherent_atom_size.is_none()
    }

    /// Fetch readable slice of sub-range to be read.
    /// Invalidating range if memory is not coherent.
    ///
    /// # Safety
    ///
    /// * Caller must ensure that device won't write to the memory region until the borrowing ends.
    /// * `T` Must be plain-old-data type compatible with data in mapped region.
    pub unsafe fn read<'b, T>(
        &'b mut self,
        device: &B::Device,
        segment: hal::memory::Segment,
    ) -> Result<&'b [T], hal::device::MapError>
    where
        'a: 'b,
        T: Copy,
    {
        let sub_range = crate::segment_to_sub_range(segment, &self.requested_range)?;

        if let Some(atom) = self.memory.non_coherent_atom_size {
            let aligned_range = crate::align_range(&sub_range, atom);
            let segment = hal::memory::Segment {
                offset: aligned_range.start,
                size: Some(aligned_range.end - aligned_range.start),
            };
            device.invalidate_mapped_memory_ranges(iter::once((self.memory.raw(), segment)))?;
        }

        let ptr = self
            .ptr
            .as_ptr()
            .offset((sub_range.start - self.mapping_range.start) as isize);
        let size = (sub_range.end - sub_range.start) as usize;

        let (_pre, slice, _post) = slice::from_raw_parts(ptr, size).align_to();
        Ok(slice)
    }

    /// Fetch writer to the sub-region.
    /// This writer will flush data on drop if written at least once.
    ///
    /// # Safety
    ///
    /// * Caller must ensure that device won't write to or read from the memory region.
    pub unsafe fn write<'b, T: 'b>(
        &'b mut self,
        device: &'b B::Device,
        segment: hal::memory::Segment,
    ) -> Result<Writer<'a, 'b, T, B>, hal::device::MapError>
    where
        'a: 'b,
        T: Copy,
    {
        let sub_range = crate::segment_to_sub_range(segment, &self.requested_range)?;
        let ptr = self
            .ptr
            .as_ptr()
            .offset((sub_range.start - self.mapping_range.start) as isize);
        let size = (sub_range.end - sub_range.start) as usize;

        let (_pre, slice, _post) = slice::from_raw_parts_mut(ptr, size).align_to_mut();
        let memory = self.memory.raw();
        let flush = self.memory.non_coherent_atom_size.map(|atom| Flush {
            device,
            memory,
            segment: {
                let range = crate::align_range(&sub_range, atom);
                hal::memory::Segment {
                    offset: range.start,
                    size: Some(range.end - range.start),
                }
            },
        });
        Ok(Writer { slice, flush })
    }
}
