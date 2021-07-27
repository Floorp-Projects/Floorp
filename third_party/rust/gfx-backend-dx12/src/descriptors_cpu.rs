use bit_set::BitSet;
use native::{CpuDescriptor, DescriptorHeapFlags, DescriptorHeapType};
use std::fmt;

// Linear stack allocator for CPU descriptor heaps.
pub struct HeapLinear {
    handle_size: usize,
    num: usize,
    size: usize,
    start: CpuDescriptor,
    raw: native::DescriptorHeap,
}

impl fmt::Debug for HeapLinear {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("HeapLinear")
    }
}

impl HeapLinear {
    pub fn new(device: native::Device, ty: DescriptorHeapType, size: usize) -> Self {
        let (heap, _hr) =
            device.create_descriptor_heap(size as _, ty, DescriptorHeapFlags::empty(), 0);

        HeapLinear {
            handle_size: device.get_descriptor_increment_size(ty) as _,
            num: 0,
            size,
            start: heap.start_cpu_descriptor(),
            raw: heap,
        }
    }

    pub fn alloc_handle(&mut self) -> CpuDescriptor {
        assert!(!self.is_full());

        let slot = self.num;
        self.num += 1;

        CpuDescriptor {
            ptr: self.start.ptr + self.handle_size * slot,
        }
    }

    pub fn is_full(&self) -> bool {
        self.num >= self.size
    }

    pub fn clear(&mut self) {
        self.num = 0;
    }

    pub unsafe fn destroy(&self) {
        self.raw.destroy();
    }
}

const HEAP_SIZE_FIXED: usize = 64;

// Fixed-size free-list allocator for CPU descriptors.
struct Heap {
    // Bit flag representation of available handles in the heap.
    //
    //  0 - Occupied
    //  1 - free
    availability: u64,
    handle_size: usize,
    start: CpuDescriptor,
    raw: native::DescriptorHeap,
}

impl fmt::Debug for Heap {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("Heap")
    }
}

impl Heap {
    fn new(device: native::Device, ty: DescriptorHeapType) -> Self {
        let (heap, _hr) = device.create_descriptor_heap(
            HEAP_SIZE_FIXED as _,
            ty,
            DescriptorHeapFlags::empty(),
            0,
        );

        Heap {
            handle_size: device.get_descriptor_increment_size(ty) as _,
            availability: !0, // all free!
            start: heap.start_cpu_descriptor(),
            raw: heap,
        }
    }

    pub fn alloc_handle(&mut self) -> CpuDescriptor {
        // Find first free slot.
        let slot = self.availability.trailing_zeros() as usize;
        assert!(slot < HEAP_SIZE_FIXED);
        // Set the slot as occupied.
        self.availability ^= 1 << slot;

        CpuDescriptor {
            ptr: self.start.ptr + self.handle_size * slot,
        }
    }

    pub fn free_handle(&mut self, handle: CpuDescriptor) {
        let slot = (handle.ptr - self.start.ptr) / self.handle_size;
        assert!(slot < HEAP_SIZE_FIXED);
        assert_eq!(self.availability & (1 << slot), 0);
        self.availability ^= 1 << slot;
    }

    pub fn is_full(&self) -> bool {
        self.availability == 0
    }

    pub unsafe fn destroy(&self) {
        self.raw.destroy();
    }
}

#[derive(Clone, Copy)]
pub struct Handle {
    pub raw: CpuDescriptor,
    heap_index: usize,
}

pub struct DescriptorCpuPool {
    device: native::Device,
    ty: DescriptorHeapType,
    heaps: Vec<Heap>,
    avaliable_heap_indices: BitSet,
}

impl fmt::Debug for DescriptorCpuPool {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("DescriptorCpuPool")
    }
}

impl DescriptorCpuPool {
    pub fn new(device: native::Device, ty: DescriptorHeapType) -> Self {
        DescriptorCpuPool {
            device,
            ty,
            heaps: Vec::new(),
            avaliable_heap_indices: BitSet::new(),
        }
    }

    pub fn alloc_handle(&mut self) -> Handle {
        let heap_index = self
            .avaliable_heap_indices
            .iter()
            .next()
            .unwrap_or_else(|| {
                // Allocate a new heap
                let id = self.heaps.len();
                self.heaps.push(Heap::new(self.device, self.ty));
                self.avaliable_heap_indices.insert(id);
                id
            });

        let heap = &mut self.heaps[heap_index];
        let handle = Handle {
            raw: heap.alloc_handle(),
            heap_index,
        };
        if heap.is_full() {
            self.avaliable_heap_indices.remove(heap_index);
        }

        handle
    }

    pub fn free_handle(&mut self, handle: Handle) {
        self.heaps[handle.heap_index].free_handle(handle.raw);
        self.avaliable_heap_indices.insert(handle.heap_index);
    }

    pub unsafe fn destroy(&self) {
        for heap in &self.heaps {
            heap.destroy();
        }
    }
}

#[derive(Default)]
pub struct CopyAccumulator {
    starts: Vec<CpuDescriptor>,
    counts: Vec<u32>,
}

impl CopyAccumulator {
    pub fn add(&mut self, start: CpuDescriptor, count: u32) {
        self.starts.push(start);
        self.counts.push(count);
    }

    pub fn clear(&mut self) {
        self.starts.clear();
        self.counts.clear();
    }

    fn total(&self) -> u32 {
        self.counts.iter().sum()
    }
}

#[derive(Default)]
pub struct MultiCopyAccumulator {
    pub src_views: CopyAccumulator,
    pub src_samplers: CopyAccumulator,
    pub dst_views: CopyAccumulator,
    pub dst_samplers: CopyAccumulator,
}

impl MultiCopyAccumulator {
    pub unsafe fn flush(&self, device: native::Device) {
        use winapi::um::d3d12;
        assert_eq!(self.src_views.total(), self.dst_views.total());
        assert_eq!(self.src_samplers.total(), self.dst_samplers.total());

        if !self.src_views.starts.is_empty() {
            device.CopyDescriptors(
                self.dst_views.starts.len() as u32,
                self.dst_views.starts.as_ptr(),
                self.dst_views.counts.as_ptr(),
                self.src_views.starts.len() as u32,
                self.src_views.starts.as_ptr(),
                self.src_views.counts.as_ptr(),
                d3d12::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            );
        }
        if !self.src_samplers.starts.is_empty() {
            device.CopyDescriptors(
                self.dst_samplers.starts.len() as u32,
                self.dst_samplers.starts.as_ptr(),
                self.dst_samplers.counts.as_ptr(),
                self.src_samplers.starts.len() as u32,
                self.src_samplers.starts.as_ptr(),
                self.src_samplers.counts.as_ptr(),
                d3d12::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
            );
        }
    }
}

pub struct DescriptorUpdater {
    heaps: Vec<HeapLinear>,
    heap_index: usize,
    reset_heap_index: usize,
    avoid_overwrite: bool,
}

impl DescriptorUpdater {
    pub fn new(device: native::Device, avoid_overwrite: bool) -> Self {
        DescriptorUpdater {
            heaps: vec![Self::create_heap(device)],
            heap_index: 0,
            reset_heap_index: 0,
            avoid_overwrite,
        }
    }

    pub unsafe fn destroy(&mut self) {
        for heap in self.heaps.drain(..) {
            heap.destroy();
        }
    }

    pub fn reset(&mut self) {
        if self.avoid_overwrite {
            self.reset_heap_index = self.heap_index;
        } else {
            self.heap_index = 0;
            for heap in self.heaps.iter_mut() {
                heap.clear();
            }
        }
    }

    fn create_heap(device: native::Device) -> HeapLinear {
        let size = 1 << 12; //arbitrary
        HeapLinear::new(device, native::DescriptorHeapType::CbvSrvUav, size)
    }

    pub fn alloc_handle(&mut self, device: native::Device) -> CpuDescriptor {
        if self.heaps[self.heap_index].is_full() {
            self.heap_index += 1;
            if self.heap_index == self.heaps.len() {
                self.heap_index = 0;
            }
            if self.heap_index == self.reset_heap_index {
                let heap = Self::create_heap(device);
                self.heaps.insert(self.heap_index, heap);
                self.reset_heap_index += 1;
            } else {
                self.heaps[self.heap_index].clear();
            }
        }
        self.heaps[self.heap_index].alloc_handle()
    }
}
