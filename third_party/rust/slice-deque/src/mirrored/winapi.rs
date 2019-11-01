//! Implements the allocator hooks on top of window's virtual alloc.

use super::mem;

use winapi::shared::basetsd::SIZE_T;
use winapi::shared::minwindef::{BOOL, DWORD, LPCVOID, LPVOID};
use winapi::shared::ntdef::LPCWSTR;
use winapi::um::memoryapi::{
    CreateFileMappingW, MapViewOfFileEx, UnmapViewOfFile, VirtualAlloc,
    VirtualFree, FILE_MAP_ALL_ACCESS,
};
use winapi::um::winnt::{
    MEM_RELEASE, MEM_RESERVE, PAGE_NOACCESS, PAGE_READWRITE, SEC_COMMIT,
};

use winapi::um::handleapi::{CloseHandle, INVALID_HANDLE_VALUE};
use winapi::um::minwinbase::LPSECURITY_ATTRIBUTES;
use winapi::um::sysinfoapi::{GetSystemInfo, LPSYSTEM_INFO, SYSTEM_INFO};

pub use winapi::shared::ntdef::HANDLE;

use super::AllocError;

/// Returns the size of an allocation unit in bytes.
///
/// In Windows calls to `VirtualAlloc` must specify a multiple of
/// `SYSTEM_INFO::dwAllocationGranularity` bytes.
///
/// FIXME: the allocation granularity should always be larger than the page
/// size (64k vs 4k), so determining the page size here is not necessary.
pub fn allocation_granularity() -> usize {
    unsafe {
        let mut system_info: SYSTEM_INFO = mem::uninitialized();
        GetSystemInfo(&mut system_info as LPSYSTEM_INFO);
        let allocation_granularity =
            system_info.dwAllocationGranularity as usize;
        let page_size = system_info.dwPageSize as usize;
        page_size.max(allocation_granularity)
    }
}

/// Allocates an uninitialzied buffer that holds `size` bytes, where
/// the bytes in range `[0, size / 2)` are mirrored into the bytes in
/// range `[size / 2, size)`.
///
/// On Windows the algorithm is as follows:
///
/// * 1. Allocate physical memory to hold `size / 2` bytes using a   memory
///   mapped file.
/// * 2. Find a region of virtual memory large enough to hold `size`
/// bytes (by allocating memory with `VirtualAlloc` and immediately
/// freeing   it with `VirtualFree`).
/// * 3. Race condition: map the physical memory to the two halves of the
///   virtual memory region.
///
/// If we get a race (e.g. because some other process obtains memory in the
/// memory region where we wanted to map our physical memory) we release
/// the first portion of virtual memory if mapping succeeded and try
/// again (up to a maximum of `MAX_NO_ALLOC_ITERS` times).
///
/// # Panics
///
/// If `size` is zero or `size / 2` is not a multiple of the
/// allocation granularity.
pub fn allocate_mirrored(size: usize) -> Result<*mut u8, AllocError> {
    /// Maximum number of attempts to allocate in case of a race condition.
    const MAX_NO_ALLOC_ITERS: usize = 10;
    unsafe {
        let half_size = size / 2;
        assert!(size != 0);
        assert!(half_size % allocation_granularity() == 0);

        let file_mapping = create_file_mapping(half_size)?;

        let mut no_iters = 0;
        let virt_ptr = loop {
            if no_iters > MAX_NO_ALLOC_ITERS {
                // If we exceeded the number of iterations try to close the
                // handle and error:
                close_file_mapping(file_mapping)
                    .expect("freeing physical memory failed");
                return Err(AllocError::Other);
            }

            // Find large enough virtual memory region (if this fails we are
            // done):
            let virt_ptr = reserve_virtual_memory(size)?;

            // Map the physical memory to the first half:
            if map_view_of_file(file_mapping, half_size, virt_ptr).is_err() {
                // If this fails, there is nothing to free and we try again:
                no_iters += 1;
                continue;
            }

            // Map physical memory to the second half:
            if map_view_of_file(
                file_mapping,
                half_size,
                virt_ptr.offset(half_size as isize),
            )
            .is_err()
            {
                // If this fails, we release the map of the first half and try
                // again:
                no_iters += 1;
                if unmap_view_of_file(virt_ptr).is_err() {
                    // If unmapping fails try to close the handle and
                    // panic:
                    close_file_mapping(file_mapping)
                        .expect("freeing physical memory failed");
                    panic!("unmapping first half of memory failed")
                }
                continue;
            }

            // We are done
            break virt_ptr;
        };

        // Close the file handle, it will be released when all the memory is
        // unmapped:
        close_file_mapping(file_mapping).expect("closing file handle failed");

        Ok(virt_ptr)
    }
}

/// Deallocates the mirrored memory region at `ptr` of `size` bytes.
///
/// # Unsafe
///
/// `ptr` must have been obtained from a call to `allocate_mirrored(size)`,
/// otherwise the behavior is undefined.
///
/// # Panics
///
/// If `size` is zero or `size / 2` is not a multiple of the
/// allocation granularity, or `ptr` is null.
pub unsafe fn deallocate_mirrored(ptr: *mut u8, size: usize) {
    assert!(!ptr.is_null());
    assert!(size != 0);
    assert!(size % allocation_granularity() == 0);
    // On "windows" we unmap the memory.
    let half_size = size / 2;
    unmap_view_of_file(ptr).expect("unmapping first buffer half failed");
    let second_half_ptr = ptr.offset(half_size as isize);
    unmap_view_of_file(second_half_ptr)
        .expect("unmapping second buffer half failed");
}

/// Creates a file mapping able to hold `size` bytes.
///
/// # Panics
///
/// If `size` is zero or not a multiple of the `allocation_granularity`.
fn create_file_mapping(size: usize) -> Result<HANDLE, AllocError> {
    unsafe {
        assert!(size != 0);
        assert!(size % allocation_granularity() == 0);
        let dw_maximum_size_low: DWORD = size as DWORD;
        let dw_maximum_size_high: DWORD =
            match (mem::size_of::<DWORD>(), mem::size_of::<usize>()) {
                // If both sizes are equal, the size is passed in the lower
                // half, so the higher 32-bits are zero
                (4, 4) | (8, 8) => 0,
                // If DWORD is 32 bit but usize is 64-bit, we pass the higher
                // 32-bit of size:
                (4, 8) => (size >> 32) as DWORD,
                _ => unimplemented!(),
            };

        let h: HANDLE = CreateFileMappingW(
            /* hFile: */ INVALID_HANDLE_VALUE as HANDLE,
            /* lpAttributes: */ 0 as LPSECURITY_ATTRIBUTES,
            /* flProtect: */ PAGE_READWRITE | SEC_COMMIT as DWORD,
            /* dwMaximumSizeHigh: */ dw_maximum_size_high,
            /* dwMaximumSizeLow: */ dw_maximum_size_low,
            /* lpName: */ 0 as LPCWSTR,
        );

        if h.is_null() {
            let s = tiny_str!("create_file_mapping (with size: {})", size);
            print_error(s.as_str());
            return Err(AllocError::Oom);
        }
        Ok(h)
    }
}

/// Closes a file mapping.
///
/// # Unsafety
///
/// `file_mapping` must point to a valid file mapping created with
/// `create_file_mapping`.
///
/// # Panics
///
/// If `file_mapping` is null.
unsafe fn close_file_mapping(file_mapping: HANDLE) -> Result<(), ()> {
    assert!(!file_mapping.is_null());

    let r: BOOL = CloseHandle(file_mapping);
    if r == 0 {
        print_error("close_file_mapping");
        return Err(());
    }
    Ok(())
}

/// Reserves a virtual memory region able to hold `size` bytes.
///
/// The Windows API has no way to do this, so... we allocate a `size`-ed region
/// with `VirtualAlloc`, immediately free it afterwards with `VirtualFree` and
/// hope that the region is still available when we try to map into it.
///
/// # Panics
///
/// If `size` is not a multiple of the `allocation_granularity`.
fn reserve_virtual_memory(size: usize) -> Result<(*mut u8), AllocError> {
    unsafe {
        assert!(size != 0);
        assert!(size % allocation_granularity() == 0);

        let r: LPVOID = VirtualAlloc(
            /* lpAddress: */ 0 as LPVOID,
            /* dwSize: */ size as SIZE_T,
            /* flAllocationType: */ MEM_RESERVE,
            /* flProtect: */ PAGE_NOACCESS,
        );

        if r.is_null() {
            print_error("reserve_virtual_memory(alloc failed)");
            return Err(AllocError::Oom);
        }

        let fr = VirtualFree(
            /* lpAddress: */ r,
            /* dwSize: */ 0 as SIZE_T,
            /* dwFreeType: */ MEM_RELEASE as DWORD,
        );
        if fr == 0 {
            print_error("reserve_virtual_memory(free failed)");
            return Err(AllocError::Other);
        }

        Ok(r as *mut u8)
    }
}

/// Maps `size` bytes of `file_mapping` to `address`.
///
/// # Unsafety
///
/// `file_mapping` must point to a valid file-mapping created with
/// `create_file_mapping`.
///
/// # Panics
///
/// If `file_mapping` or `address` are null, or if `size` is zero or not a
/// multiple of the allocation granularity of the system.
unsafe fn map_view_of_file(
    file_mapping: HANDLE, size: usize, address: *mut u8,
) -> Result<(), ()> {
    assert!(!file_mapping.is_null());
    assert!(!address.is_null());
    assert!(size != 0);
    assert!(size % allocation_granularity() == 0);

    let r: LPVOID = MapViewOfFileEx(
        /* hFileMappingObject: */ file_mapping,
        /* dwDesiredAccess: */ FILE_MAP_ALL_ACCESS,
        /* dwFileOffsetHigh: */ 0 as DWORD,
        /* dwFileOffsetLow: */ 0 as DWORD,
        /* dwNumberOfBytesToMap: */ size as SIZE_T,
        /* lpBaseAddress: */ address as LPVOID,
    );
    if r.is_null() {
        print_error("map_view_of_file");
        return Err(());
    }
    debug_assert!(r == address as LPVOID);
    Ok(())
}

/// Unmaps the memory at `address`.
///
/// # Unsafety
///
/// If address does not point to a valid memory address previously mapped with
/// `map_view_of_file`.
///
/// # Panics
///
/// If `address` is null.
unsafe fn unmap_view_of_file(address: *mut u8) -> Result<(), ()> {
    assert!(!address.is_null());

    let r = UnmapViewOfFile(/* lpBaseAddress: */ address as LPCVOID);
    if r == 0 {
        print_error("unmap_view_of_file");
        return Err(());
    }
    Ok(())
}

/// Prints last os error at `location`.
#[cfg(all(debug_assertions, feature = "use_std"))]
fn print_error(location: &str) {
    eprintln!(
        "Error at {}: {}",
        location,
        ::std::io::Error::last_os_error()
    );
}

/// Prints last os error at `location`.
#[cfg(not(all(debug_assertions, feature = "use_std")))]
fn print_error(_location: &str) {}
