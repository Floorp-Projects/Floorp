use crate::error::Error;
use crate::module::Module;
use crate::region::RegionInternal;
use libc::c_void;
use lucet_module::GlobalValue;
use nix::unistd::{sysconf, SysconfVar};
use std::sync::{Arc, Once, Weak};

pub const HOST_PAGE_SIZE_EXPECTED: usize = 4096;
static mut HOST_PAGE_SIZE: usize = 0;
static HOST_PAGE_SIZE_INIT: Once = Once::new();

/// Our host is Linux x86_64, which should always use a 4K page.
///
/// We double check the expected value using `sysconf` at runtime.
pub fn host_page_size() -> usize {
    unsafe {
        HOST_PAGE_SIZE_INIT.call_once(|| match sysconf(SysconfVar::PAGE_SIZE) {
            Ok(Some(sz)) => {
                if sz as usize == HOST_PAGE_SIZE_EXPECTED {
                    HOST_PAGE_SIZE = HOST_PAGE_SIZE_EXPECTED;
                } else {
                    panic!(
                        "host page size was {}; expected {}",
                        sz, HOST_PAGE_SIZE_EXPECTED
                    );
                }
            }
            _ => panic!("could not get host page size from sysconf"),
        });
        HOST_PAGE_SIZE
    }
}

pub fn instance_heap_offset() -> usize {
    1 * host_page_size()
}

/// A set of pointers into virtual memory that can be allocated into an `Alloc`.
///
/// The `'r` lifetime parameter represents the lifetime of the region that backs this virtual
/// address space.
///
/// The memory layout in a `Slot` is meant to be reused in order to reduce overhead on region
/// implementations. To back the layout with real memory, use `Region::allocate_runtime`.
///
/// To ensure a `Slot` can only be backed by one allocation at a time, it contains a mutex, but
/// otherwise can be freely copied.
#[repr(C)]
pub struct Slot {
    /// The beginning of the contiguous virtual memory chunk managed by this `Alloc`.
    ///
    /// The first part of this memory, pointed to by `start`, is always backed by real memory, and
    /// is used to store the lucet_instance structure.
    pub start: *mut c_void,

    /// The next part of memory contains the heap and its guard pages.
    ///
    /// The heap is backed by real memory according to the `HeapSpec`. Guard pages trigger a sigsegv
    /// when accessed.
    pub heap: *mut c_void,

    /// The stack comes after the heap.
    ///
    /// Because the stack grows downwards, we get the added safety of ensuring that stack overflows
    /// go into the guard pages, if the `Limits` specify guard pages. The stack is always the size
    /// given by `Limits.stack_pages`.
    pub stack: *mut c_void,

    /// The WebAssembly Globals follow the stack and a single guard page.
    pub globals: *mut c_void,

    /// The signal handler stack follows the globals.
    ///
    /// Having a separate signal handler stack allows the signal handler to run in situations where
    /// the normal stack has grown into the guard page.
    pub sigstack: *mut c_void,

    /// Limits of the memory.
    ///
    /// Should not change through the lifetime of the `Alloc`.
    pub limits: Limits,

    pub region: Weak<dyn RegionInternal>,
}

// raw pointers require unsafe impl
unsafe impl Send for Slot {}
unsafe impl Sync for Slot {}

impl Slot {
    pub fn stack_top(&self) -> *mut c_void {
        (self.stack as usize + self.limits.stack_size) as *mut c_void
    }
}

/// The structure that manages the allocations backing an `Instance`.
///
/// `Alloc`s are not to be created directly, but rather are created by `Region`s during instance
/// creation.
pub struct Alloc {
    pub heap_accessible_size: usize,
    pub heap_inaccessible_size: usize,
    pub slot: Option<Slot>,
    pub region: Arc<dyn RegionInternal>,
}

impl Drop for Alloc {
    fn drop(&mut self) {
        // eprintln!("Alloc::drop()");
        self.region.clone().drop_alloc(self);
    }
}

impl Alloc {
    pub fn addr_in_guard_page(&self, addr: *const c_void) -> bool {
        let addr = addr as usize;
        let heap = self.slot().heap as usize;
        let guard_start = heap + self.heap_accessible_size;
        let guard_end = heap + self.slot().limits.heap_address_space_size;
        // eprintln!(
        //     "addr = {:p}, guard_start = {:p}, guard_end = {:p}",
        //     addr, guard_start as *mut c_void, guard_end as *mut c_void
        // );
        let stack_guard_end = self.slot().stack as usize;
        let stack_guard_start = stack_guard_end - host_page_size();
        // eprintln!(
        //     "addr = {:p}, stack_guard_start = {:p}, stack_guard_end = {:p}",
        //     addr, stack_guard_start as *mut c_void, stack_guard_end as *mut c_void
        // );
        let in_heap_guard = (addr >= guard_start) && (addr < guard_end);
        let in_stack_guard = (addr >= stack_guard_start) && (addr < stack_guard_end);

        in_heap_guard || in_stack_guard
    }

    pub fn expand_heap(&mut self, expand_bytes: u32, module: &dyn Module) -> Result<u32, Error> {
        let slot = self.slot();

        if expand_bytes == 0 {
            // no expansion takes place, which is not an error
            return Ok(self.heap_accessible_size as u32);
        }

        let host_page_size = host_page_size() as u32;

        if self.heap_accessible_size as u32 % host_page_size != 0 {
            lucet_bail!("heap is not page-aligned; this is a bug");
        }

        if expand_bytes > std::u32::MAX - host_page_size - 1 {
            bail_limits_exceeded!("expanded heap would overflow address space");
        }

        // round the expansion up to a page boundary
        let expand_pagealigned =
            ((expand_bytes + host_page_size - 1) / host_page_size) * host_page_size;

        // `heap_inaccessible_size` tracks the size of the allocation that is addressible but not
        // accessible. We cannot perform an expansion larger than this size.
        if expand_pagealigned as usize > self.heap_inaccessible_size {
            bail_limits_exceeded!("expanded heap would overflow addressable memory");
        }

        // the above makes sure this expression does not underflow
        let guard_remaining = self.heap_inaccessible_size - expand_pagealigned as usize;

        if let Some(heap_spec) = module.heap_spec() {
            // The compiler specifies how much guard (memory which traps on access) must be beyond the
            // end of the accessible memory. We cannot perform an expansion that would make this region
            // smaller than the compiler expected it to be.
            if guard_remaining < heap_spec.guard_size as usize {
                bail_limits_exceeded!("expansion would leave guard memory too small");
            }

            // The compiler indicates that the module has specified a maximum memory size. Don't let
            // the heap expand beyond that:
            if let Some(max_size) = heap_spec.max_size {
                if self.heap_accessible_size + expand_pagealigned as usize > max_size as usize {
                    bail_limits_exceeded!(
                        "expansion would exceed module-specified heap limit: {:?}",
                        max_size
                    );
                }
            }
        } else {
            return Err(Error::NoLinearMemory("cannot expand heap".to_owned()));
        }
        // The runtime sets a limit on how much of the heap can be backed by real memory. Don't let
        // the heap expand beyond that:
        if self.heap_accessible_size + expand_pagealigned as usize > slot.limits.heap_memory_size {
            bail_limits_exceeded!(
                "expansion would exceed runtime-specified heap limit: {:?}",
                slot.limits
            );
        }

        let newly_accessible = self.heap_accessible_size;

        self.region
            .clone()
            .expand_heap(slot, newly_accessible as u32, expand_pagealigned)?;

        self.heap_accessible_size += expand_pagealigned as usize;
        self.heap_inaccessible_size -= expand_pagealigned as usize;

        Ok(newly_accessible as u32)
    }

    pub fn reset_heap(&mut self, module: &dyn Module) -> Result<(), Error> {
        self.region.clone().reset_heap(self, module)
    }

    pub fn heap_len(&self) -> usize {
        self.heap_accessible_size
    }

    pub fn slot(&self) -> &Slot {
        self.slot
            .as_ref()
            .expect("alloc missing its slot before drop")
    }

    /// Return the heap as a byte slice.
    pub unsafe fn heap(&self) -> &[u8] {
        std::slice::from_raw_parts(self.slot().heap as *mut u8, self.heap_accessible_size)
    }

    /// Return the heap as a mutable byte slice.
    pub unsafe fn heap_mut(&mut self) -> &mut [u8] {
        std::slice::from_raw_parts_mut(self.slot().heap as *mut u8, self.heap_accessible_size)
    }

    /// Return the heap as a slice of 32-bit words.
    pub unsafe fn heap_u32(&self) -> &[u32] {
        assert!(self.slot().heap as usize % 4 == 0, "heap is 4-byte aligned");
        assert!(
            self.heap_accessible_size % 4 == 0,
            "heap size is multiple of 4-bytes"
        );
        std::slice::from_raw_parts(self.slot().heap as *mut u32, self.heap_accessible_size / 4)
    }

    /// Return the heap as a mutable slice of 32-bit words.
    pub unsafe fn heap_u32_mut(&self) -> &mut [u32] {
        assert!(self.slot().heap as usize % 4 == 0, "heap is 4-byte aligned");
        assert!(
            self.heap_accessible_size % 4 == 0,
            "heap size is multiple of 4-bytes"
        );
        std::slice::from_raw_parts_mut(self.slot().heap as *mut u32, self.heap_accessible_size / 4)
    }

    /// Return the heap as a slice of 64-bit words.
    pub unsafe fn heap_u64(&self) -> &[u64] {
        assert!(self.slot().heap as usize % 8 == 0, "heap is 8-byte aligned");
        assert!(
            self.heap_accessible_size % 8 == 0,
            "heap size is multiple of 8-bytes"
        );
        std::slice::from_raw_parts(self.slot().heap as *mut u64, self.heap_accessible_size / 8)
    }

    /// Return the heap as a mutable slice of 64-bit words.
    pub unsafe fn heap_u64_mut(&mut self) -> &mut [u64] {
        assert!(self.slot().heap as usize % 8 == 0, "heap is 8-byte aligned");
        assert!(
            self.heap_accessible_size % 8 == 0,
            "heap size is multiple of 8-bytes"
        );
        std::slice::from_raw_parts_mut(self.slot().heap as *mut u64, self.heap_accessible_size / 8)
    }

    /// Return the stack as a mutable byte slice.
    ///
    /// Since the stack grows down, `alloc.stack_mut()[0]` is the top of the stack, and
    /// `alloc.stack_mut()[alloc.limits.stack_size - 1]` is the last byte at the bottom of the
    /// stack.
    pub unsafe fn stack_mut(&mut self) -> &mut [u8] {
        std::slice::from_raw_parts_mut(self.slot().stack as *mut u8, self.slot().limits.stack_size)
    }

    /// Return the stack as a mutable slice of 64-bit words.
    ///
    /// Since the stack grows down, `alloc.stack_mut()[0]` is the top of the stack, and
    /// `alloc.stack_mut()[alloc.limits.stack_size - 1]` is the last word at the bottom of the
    /// stack.
    pub unsafe fn stack_u64_mut(&mut self) -> &mut [u64] {
        assert!(
            self.slot().stack as usize % 8 == 0,
            "stack is 8-byte aligned"
        );
        assert!(
            self.slot().limits.stack_size % 8 == 0,
            "stack size is multiple of 8-bytes"
        );
        std::slice::from_raw_parts_mut(
            self.slot().stack as *mut u64,
            self.slot().limits.stack_size / 8,
        )
    }

    /// Return the globals as a slice.
    pub unsafe fn globals(&self) -> &[GlobalValue] {
        std::slice::from_raw_parts(
            self.slot().globals as *const GlobalValue,
            self.slot().limits.globals_size / std::mem::size_of::<GlobalValue>(),
        )
    }

    /// Return the globals as a mutable slice.
    pub unsafe fn globals_mut(&mut self) -> &mut [GlobalValue] {
        std::slice::from_raw_parts_mut(
            self.slot().globals as *mut GlobalValue,
            self.slot().limits.globals_size / std::mem::size_of::<GlobalValue>(),
        )
    }

    /// Return the sigstack as a mutable byte slice.
    pub unsafe fn sigstack_mut(&mut self) -> &mut [u8] {
        std::slice::from_raw_parts_mut(
            self.slot().sigstack as *mut u8,
            self.slot().limits.signal_stack_size,
        )
    }

    pub fn mem_in_heap<T>(&self, ptr: *const T, len: usize) -> bool {
        let start = ptr as usize;
        let end = start + len;

        let heap_start = self.slot().heap as usize;
        let heap_end = heap_start + self.heap_accessible_size;

        // TODO: check for off-by-ones
        start <= end
            && start >= heap_start
            && start < heap_end
            && end >= heap_start
            && end <= heap_end
    }
}

/// Runtime limits for the various memories that back a Lucet instance.
///
/// Each value is specified in bytes, and must be evenly divisible by the host page size (4K).
#[derive(Clone, Debug)]
#[repr(C)]
pub struct Limits {
    /// Max size of the heap, which can be backed by real memory. (default 1M)
    pub heap_memory_size: usize,
    /// Size of total virtual memory. (default 8G)
    pub heap_address_space_size: usize,
    /// Size of the guest stack. (default 128K)
    pub stack_size: usize,
    /// Size of the globals region in bytes; each global uses 8 bytes. (default 4K)
    pub globals_size: usize,
    /// Size of the signal stack in bytes. (default SIGSTKSZ for release builds, 12K for debug builds)
    ///
    /// This difference is to account for the greatly increased stack size usage in the signal
    /// handler when running without optimizations.
    ///
    /// Note that debug vs. release mode is determined by `cfg(debug_assertions)`, so if you are
    /// specifically enabling debug assertions in your release builds, the default signal stack will
    /// be larger.
    pub signal_stack_size: usize,
}

#[cfg(debug_assertions)]
pub const DEFAULT_SIGNAL_STACK_SIZE: usize = 12 * 1024;
#[cfg(not(debug_assertions))]
pub const DEFAULT_SIGNAL_STACK_SIZE: usize = libc::SIGSTKSZ;

impl Limits {
    pub const fn default() -> Limits {
        Limits {
            heap_memory_size: 16 * 64 * 1024,
            heap_address_space_size: 0x200000000,
            stack_size: 128 * 1024,
            globals_size: 4096,
            signal_stack_size: DEFAULT_SIGNAL_STACK_SIZE,
        }
    }
}

impl Limits {
    pub fn total_memory_size(&self) -> usize {
        // Memory is laid out as follows:
        // * the instance (up to instance_heap_offset)
        // * the heap, followed by guard pages
        // * the stack (grows towards heap guard pages)
        // * globals
        // * one guard page (to catch signal stack overflow)
        // * the signal stack

        [
            instance_heap_offset(),
            self.heap_address_space_size,
            host_page_size(),
            self.stack_size,
            self.globals_size,
            host_page_size(),
            self.signal_stack_size,
        ]
        .iter()
        .try_fold(0usize, |acc, &x| acc.checked_add(x))
        .expect("total_memory_size doesn't overflow")
    }

    /// Validate that the limits are aligned to page sizes, and that the stack is not empty.
    pub fn validate(&self) -> Result<(), Error> {
        if self.heap_memory_size % host_page_size() != 0 {
            return Err(Error::InvalidArgument(
                "memory size must be a multiple of host page size",
            ));
        }
        if self.heap_address_space_size % host_page_size() != 0 {
            return Err(Error::InvalidArgument(
                "address space size must be a multiple of host page size",
            ));
        }
        if self.heap_memory_size > self.heap_address_space_size {
            return Err(Error::InvalidArgument(
                "address space size must be at least as large as memory size",
            ));
        }
        if self.stack_size % host_page_size() != 0 {
            return Err(Error::InvalidArgument(
                "stack size must be a multiple of host page size",
            ));
        }
        if self.globals_size % host_page_size() != 0 {
            return Err(Error::InvalidArgument(
                "globals size must be a multiple of host page size",
            ));
        }
        if self.stack_size <= 0 {
            return Err(Error::InvalidArgument("stack size must be greater than 0"));
        }
        if self.signal_stack_size % host_page_size() != 0 {
            return Err(Error::InvalidArgument(
                "signal stack size must be a multiple of host page size",
            ));
        }
        Ok(())
    }
}

pub mod tests;
