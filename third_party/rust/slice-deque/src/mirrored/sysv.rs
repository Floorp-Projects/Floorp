//! Racy System V mirrored memory allocation.
use super::{mem, AllocError};
use libc::{
    c_int, c_void, mmap, munmap, shmat, shmctl, shmdt, shmget, shmid_ds,
    sysconf, IPC_CREAT, IPC_PRIVATE, IPC_RMID, MAP_FAILED, MAP_PRIVATE,
    PROT_NONE, _SC_PAGESIZE,
};

#[cfg(not(target_os = "macos"))]
use libc::MAP_ANONYMOUS;

#[cfg(target_os = "macos")]
use libc::MAP_ANON as MAP_ANONYMOUS;

/// Returns the size of an allocation unit.
///
/// System V shared memory has the page size as its allocation unit.
pub fn allocation_granularity() -> usize {
    unsafe { sysconf(_SC_PAGESIZE) as usize }
}

/// System V Shared Memory handle.
struct SharedMemory {
    id: c_int,
}

/// Map of System V Shared Memory to an address in the process address space.
struct MemoryMap(*mut c_void);

impl SharedMemory {
    /// Allocates `size` bytes of inter-process shared memory.
    ///
    /// Return the handle to this shared memory.
    ///
    /// # Panics
    ///
    /// If `size` is zero or not a multiple of the allocation granularity.
    pub fn allocate(size: usize) -> Result<SharedMemory, AllocError> {
        assert!(size != 0);
        assert!(size % allocation_granularity() == 0);
        unsafe {
            let id = shmget(IPC_PRIVATE, size, IPC_CREAT | 448);
            if id == -1 {
                print_error("shmget");
                return Err(AllocError::Oom);
            }
            Ok(SharedMemory { id })
        }
    }

    /// Attaches System V shared memory to the memory address at `ptr` in the
    /// address space of the current process.
    ///
    /// # Panics
    ///
    /// If `ptr` is null.
    pub fn attach(&self, ptr: *mut c_void) -> Result<MemoryMap, AllocError> {
        unsafe {
            // note: the success of allocate guarantees `shm_id != -1`.
            assert!(!ptr.is_null());
            let r = shmat(self.id, ptr, 0);
            if r as isize == -1 {
                print_error("shmat");
                return Err(AllocError::Other);
            }
            let map = MemoryMap(ptr);
            if r != ptr {
                print_error("shmat2");
                // map is dropped here, freeing the memory.
                return Err(AllocError::Other);
            }
            Ok(map)
        }
    }
}

impl Drop for SharedMemory {
    /// Deallocates the inter-process shared memory..
    fn drop(&mut self) {
        unsafe {
            // note: the success of allocate guarantees `shm_id != -1`.
            let r = shmctl(self.id, IPC_RMID, 0 as *mut shmid_ds);
            if r == -1 {
                // TODO: unlikely
                // This should never happen, but just in case:
                print_error("shmctl");
                panic!("freeing system V shared-memory failed");
            }
        }
    }
}

impl MemoryMap {
    /// Initializes a MemoryMap to `ptr`.
    ///
    /// # Panics
    ///
    /// If `ptr` is null.
    ///
    /// # Unsafety
    ///
    /// If `ptr` does not point to a memory map created using
    /// `SharedMemory::attach` that has not been dropped yet..
    unsafe fn from_raw(ptr: *mut c_void) -> MemoryMap {
        assert!(!ptr.is_null());
        MemoryMap(ptr)
    }
}

impl Drop for MemoryMap {
    fn drop(&mut self) {
        unsafe {
            // note: the success of SharedMemory::attach and MemoryMap::new
            // guarantee `!self.0.is_null()`.
            let r = shmdt(self.0);
            if r == -1 {
                // TODO: unlikely
                print_error("shmdt");
                panic!("freeing system V memory map failed");
            }
        }
    }
}

/// Allocates `size` bytes of uninitialized memory, where the bytes in range
/// `[0, size / 2)` are mirrored into the bytes in range `[size / 2, size)`.
///
/// The algorithm using System V interprocess shared-memory is:
///
/// * 1. Allocate `size / 2` of interprocess shared memory.
/// * 2. Reserve `size` bytes of virtual memory using `mmap + munmap`.
/// * 3. Attach the shared memory to the first and second half.
///
/// There is a race between steps 2 and 3 because after unmapping the memory
/// and before attaching the shared memory to it another process might use that
/// memory.
pub fn allocate_mirrored(size: usize) -> Result<*mut u8, AllocError> {
    unsafe {
        assert!(size != 0);
        let half_size = size / 2;
        assert!(half_size % allocation_granularity() == 0);

        // 1. Allocate interprocess shared memory
        let shm = SharedMemory::allocate(half_size)?;

        const MAX_NO_ITERS: i32 = 10;
        let mut counter = 0;
        let ptr = loop {
            counter += 1;
            if counter > MAX_NO_ITERS {
                return Err(AllocError::Other);
            }

            // 2. Reserve virtual memory:
            let ptr = mmap(
                0 as *mut c_void,
                size,
                PROT_NONE,
                MAP_ANONYMOUS | MAP_PRIVATE,
                -1,
                0,
            );
            if ptr == MAP_FAILED {
                print_error("mmap initial");
                return Err(AllocError::Oom);
            }

            let ptr2 =
                (ptr as *mut u8).offset(half_size as isize) as *mut c_void;

            unmap(ptr, size).expect("unmap initial failed");

            // 3. Attach shared memory to virtual memory:
            let map0 = shm.attach(ptr);
            if map0.is_err() {
                print_error("attach_shm first failed");
                continue;
            }
            let map1 = shm.attach(ptr2);
            if map1.is_err() {
                print_error("attach_shm second failed");
                continue;
            }
            // On success we leak the maps to keep them alive.
            // On drop we rebuild the maps from ptr and ptr + half_size
            // to deallocate them.
            mem::forget(map0);
            mem::forget(map1);
            break ptr;
        };

        Ok(ptr as *mut u8)
    }
}

/// Deallocates the mirrored memory region at `ptr` of `size` bytes.
///
/// # Unsafe
///
/// `ptr` must have been obtained from a call to `allocate_mirrored(size)` and
/// not have been previously deallocated. Otherwise the behavior is undefined.
///
/// # Panics
///
/// If `size` is zero or `size / 2` is not a multiple of the
/// allocation granularity, or `ptr` is null.
pub unsafe fn deallocate_mirrored(ptr: *mut u8, size: usize) {
    let ptr2 = ptr.offset(size as isize / 2);
    MemoryMap::from_raw(ptr as *mut c_void);
    MemoryMap::from_raw(ptr2 as *mut c_void);
}

/// Unmaps the memory region at `[ptr, ptr+size)`.
unsafe fn unmap(ptr: *mut c_void, size: usize) -> Result<(), ()> {
    let r = munmap(ptr, size);
    if r == -1 {
        print_error("unmap");
        return Err(());
    }
    Ok(())
}

#[cfg(not(all(debug_assertions, feature = "use_std")))]
fn print_error(_location: &str) {}

#[cfg(all(debug_assertions, feature = "use_std"))]
fn print_error(location: &str) {
    eprintln!(
        "Error at {}: {}",
        location,
        ::std::io::Error::last_os_error()
    );
}
