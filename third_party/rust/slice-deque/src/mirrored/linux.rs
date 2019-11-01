//! Non-racy linux-specific mirrored memory allocation.
use libc::{
    c_char, c_int, c_long, c_uint, c_void, close, ftruncate, mkstemp, mmap,
    munmap, off_t, size_t, syscall, sysconf, SYS_memfd_create, ENOSYS,
    MAP_FAILED, MAP_FIXED, MAP_SHARED, PROT_READ, PROT_WRITE, _SC_PAGESIZE,
};

#[cfg(target_os = "android")]
use libc::__errno;
#[cfg(not(target_os = "android"))]
use libc::__errno_location;

use super::{ptr, AllocError};

/// [`memfd_create`] - create an anonymous file
///
/// [`memfd_create`]: http://man7.org/linux/man-pages/man2/memfd_create.2.html
fn memfd_create(name: *const c_char, flags: c_uint) -> c_long {
    unsafe { syscall(SYS_memfd_create, name, flags) }
}

/// Returns the size of a memory allocation unit.
///
/// In Linux-like systems this equals the page-size.
pub fn allocation_granularity() -> usize {
    unsafe { sysconf(_SC_PAGESIZE) as usize }
}

/// Reads `errno`.
fn errno() -> c_int {
    #[cfg(not(target_os = "android"))]
    unsafe {
        *__errno_location()
    }
    #[cfg(target_os = "android")]
    unsafe {
        *__errno()
    }
}

/// Allocates an uninitialzied buffer that holds `size` bytes, where
/// the bytes in range `[0, size / 2)` are mirrored into the bytes in
/// range `[size / 2, size)`.
///
/// On Linux the algorithm is as follows:
///
/// * 1. Allocate a memory-mapped file containing `size / 2` bytes.
/// * 2. Map the file into `size` bytes of virtual memory.
/// * 3. Map the file into the last `size / 2` bytes of the virtual memory
/// region      obtained in step 2.
///
/// This algorithm doesn't have any races.
///
/// # Panics
///
/// If `size` is zero or `size / 2` is not a multiple of the
/// allocation granularity.
pub fn allocate_mirrored(size: usize) -> Result<*mut u8, AllocError> {
    unsafe {
        let half_size = size / 2;
        assert!(size != 0);
        assert!(half_size % allocation_granularity() == 0);

        // create temporary file
        let mut fname = *b"/tmp/slice_deque_fileXXXXXX\0";
        let mut fd: c_long =
            memfd_create(fname.as_mut_ptr() as *mut c_char, 0);
        if fd == -1 && errno() == ENOSYS {
            // memfd_create is not implemented, use mkstemp instead:
            fd = c_long::from(mkstemp(fname.as_mut_ptr() as *mut c_char));
        }
        if fd == -1 {
            print_error("memfd_create failed");
            return Err(AllocError::Other);
        }
        let fd = fd as c_int;
        if ftruncate(fd, half_size as off_t) == -1 {
            print_error("ftruncate failed");
            if close(fd) == -1 {
                print_error("@ftruncate: close failed");
            }
            return Err(AllocError::Oom);
        };

        // mmap memory
        let ptr = mmap(
            ptr::null_mut(),
            size,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            fd,
            0,
        );
        if ptr == MAP_FAILED {
            print_error("@first: mmap failed");
            if close(fd) == -1 {
                print_error("@first: close failed");
            }
            return Err(AllocError::Oom);
        }

        let ptr2 = mmap(
            (ptr as *mut u8).offset(half_size as isize) as *mut c_void,
            half_size,
            PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_FIXED,
            fd,
            0,
        );
        if ptr2 == MAP_FAILED {
            print_error("@second: mmap failed");
            if munmap(ptr, size as size_t) == -1 {
                print_error("@second: munmap failed");
            }
            if close(fd) == -1 {
                print_error("@second: close failed");
            }
            return Err(AllocError::Other);
        }

        if close(fd) == -1 {
            print_error("@success: close failed");
        }
        Ok(ptr as *mut u8)
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
    if munmap(ptr as *mut c_void, size as size_t) == -1 {
        print_error("deallocate munmap failed");
    }
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
