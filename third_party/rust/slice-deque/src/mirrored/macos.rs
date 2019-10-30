//! Implements the allocator hooks on top of mach.

#![allow(clippy::shadow_unrelated)]

use super::mem;

use mach;
use mach::boolean::boolean_t;
use mach::kern_return::*;
use mach::mach_types::mem_entry_name_port_t;
use mach::memory_object_types::{
    memory_object_offset_t, memory_object_size_t,
};
use mach::traps::mach_task_self;
use mach::vm::{
    mach_make_memory_entry_64, mach_vm_allocate, mach_vm_deallocate,
    mach_vm_remap,
};
use mach::vm_inherit::VM_INHERIT_NONE;
use mach::vm_prot::{vm_prot_t, VM_PROT_READ, VM_PROT_WRITE};
use mach::vm_statistics::{VM_FLAGS_ANYWHERE, VM_FLAGS_FIXED};
use mach::vm_types::mach_vm_address_t;

use super::AllocError;

/// TODO: not exposed by the mach crate
const VM_FLAGS_OVERWRITE: ::libc::c_int = 0x4000_i32;

/// Returns the size of an allocation unit.
///
/// In `MacOSX` this equals the page size.
pub fn allocation_granularity() -> usize {
    unsafe { mach::vm_page_size::vm_page_size as usize }
}

/// Allocates an uninitialzied buffer that holds `size` bytes, where
/// the bytes in range `[0, size / 2)` are mirrored into the bytes in
/// range `[size / 2, size)`.
///
/// On Macos X the algorithm is as follows:
///
/// * 1. Allocate twice the memory (`size` bytes)
/// * 2. Deallocate the second half (bytes in range `[size / 2, 0)`)
/// * 3. Race condition: mirror bytes of the first half into the second
/// half.
///
/// If we get a race (e.g. because some other process allocates to the
/// second half) we release all the resources (we need to deallocate the
/// memory) and try again (up to a maximum of `MAX_NO_ALLOC_ITERS` times).
///
/// # Panics
///
/// If `size` is zero or `size / 2` is not a multiple of the
/// allocation granularity.
pub fn allocate_mirrored(size: usize) -> Result<*mut u8, AllocError> {
    unsafe {
        assert!(size != 0);
        let half_size = size / 2;
        assert!(half_size % allocation_granularity() == 0);

        let task = mach_task_self();

        // Allocate memory to hold the whole buffer:
        let mut addr: mach_vm_address_t = 0;
        let r: kern_return_t = mach_vm_allocate(
            task,
            &mut addr as *mut mach_vm_address_t,
            size as u64,
            VM_FLAGS_ANYWHERE,
        );
        if r != KERN_SUCCESS {
            // If the first allocation fails, there is nothing to
            // deallocate and we can just fail to allocate:
            print_error("initial alloc", r);
            return Err(AllocError::Oom);
        }
        debug_assert!(addr != 0);

        // Set the size of the first half to size/2:
        let r: kern_return_t = mach_vm_allocate(
            task,
            &mut addr as *mut mach_vm_address_t,
            half_size as u64,
            VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
        );
        if r != KERN_SUCCESS {
            // If the first allocation fails, there is nothing to
            // deallocate and we can just fail to allocate:
            print_error("first half alloc", r);
            return Err(AllocError::Other);
        }

        // Get an object handle to the first memory region:
        let mut memory_object_size = half_size as memory_object_size_t;
        let mut object_handle: mem_entry_name_port_t = mem::uninitialized();
        let parent_handle: mem_entry_name_port_t = 0;
        let r: kern_return_t = mach_make_memory_entry_64(
            task,
            &mut memory_object_size as *mut memory_object_size_t,
            addr as memory_object_offset_t,
            VM_PROT_READ | VM_PROT_WRITE,
            &mut object_handle as *mut mem_entry_name_port_t,
            parent_handle,
        );

        if r != KERN_SUCCESS {
            // If making the memory entry fails we should deallocate the first
            // allocation:
            print_error("make memory entry", r);
            if dealloc(addr as *mut u8, size).is_err() {
                panic!("failed to deallocate after error");
            }
            return Err(AllocError::Other);
        }

        // Map the first half to the second half using the object handle:
        let mut to = (addr as *mut u8).add(half_size) as mach_vm_address_t;
        let mut current_prot: vm_prot_t = mem::uninitialized();
        let mut out_prot: vm_prot_t = mem::uninitialized();
        let r: kern_return_t = mach_vm_remap(
            task,
            &mut to as *mut mach_vm_address_t,
            half_size as u64,
            /* mask: */ 0,
            VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
            task,
            addr,
            /* copy: */ 0 as boolean_t,
            &mut current_prot as *mut vm_prot_t,
            &mut out_prot as *mut vm_prot_t,
            VM_INHERIT_NONE,
        );

        if r != KERN_SUCCESS {
            print_error("map first to second half", r);
            // If making the memory entry fails we deallocate all the memory
            if dealloc(addr as *mut u8, size).is_err() {
                panic!("failed to deallocate after error");
            }
            return Err(AllocError::Other);
        }

        // TODO: object_handle is leaked here. Investigate whether this is ok.

        Ok(addr as *mut u8)
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
    dealloc(ptr, size).expect("deallocating mirrored buffer failed");
}

/// Tries to deallocates `size` bytes of memory starting at `ptr`.
///
/// # Unsafety
///
/// The `ptr` must have been obtained from a previous call to `alloc` and point
/// to a memory region containing at least `size` bytes.
///
/// # Panics
///
/// If `size` is zero or not a multiple of the `allocation_granularity`, or if
/// `ptr` is null.
unsafe fn dealloc(ptr: *mut u8, size: usize) -> Result<(), ()> {
    assert!(size != 0);
    assert!(size % allocation_granularity() == 0);
    assert!(!ptr.is_null());
    let addr = ptr as mach_vm_address_t;
    let r: kern_return_t =
        mach_vm_deallocate(mach_task_self(), addr, size as u64);
    if r != KERN_SUCCESS {
        print_error("dealloc", r);
        return Err(());
    }
    Ok(())
}

/// Prints last os error at `location`.
#[cfg(not(all(debug_assertions, feature = "use_std")))]
fn print_error(_msg: &str, _code: kern_return_t) {}

/// Prints last os error at `location`.
#[cfg(all(debug_assertions, feature = "use_std"))]
fn print_error(msg: &str, code: kern_return_t) {
    eprintln!("ERROR at \"{}\": {}", msg, report_error(code));
}

/// Maps a vm `kern_return_t` to an error string.
#[cfg(all(debug_assertions, feature = "use_std"))]
fn report_error(error: kern_return_t) -> &'static str {
    use mach::kern_return::*;
    match error {
        KERN_ABORTED => "KERN_ABORTED",
        KERN_ALREADY_IN_SET => "KERN_ALREADY_IN_SET",
        KERN_ALREADY_WAITING => "KERN_ALREADY_WAITING",
        KERN_CODESIGN_ERROR => "KERN_CODESIGN_ERROR",
        KERN_DEFAULT_SET => "KERN_DEFAULT_SET",
        KERN_EXCEPTION_PROTECTED => "KERN_EXCEPTION_PROTECTED",
        KERN_FAILURE => "KERN_FAILURE",
        KERN_INVALID_ADDRESS => "KERN_INVALID_ADDRESS",
        KERN_INVALID_ARGUMENT => "KERN_INVALID_ARGUMENT",
        KERN_INVALID_CAPABILITY => "KERN_INVALID_CAPABILITY",
        KERN_INVALID_HOST => "KERN_INVALID_HOST",
        KERN_INVALID_LEDGER => "KERN_INVALID_LEDGER",
        KERN_INVALID_MEMORY_CONTROL => "KERN_INVALID_MEMORY_CONTROL",
        KERN_INVALID_NAME => "KERN_INVALID_NAME",
        KERN_INVALID_OBJECT => "KERN_INVALID_OBJECT",
        KERN_INVALID_POLICY => "KERN_INVALID_POLICY",
        KERN_INVALID_PROCESSOR_SET => "KERN_INVALID_PROCESSOR_SET",
        KERN_INVALID_RIGHT => "KERN_INVALID_RIGHT",
        KERN_INVALID_SECURITY => "KERN_INVALID_SECURITY",
        KERN_INVALID_TASK => "KERN_INVALID_TASK",
        KERN_INVALID_VALUE => "KERN_INVALID_VALUE",
        KERN_LOCK_OWNED => "KERN_LOCK_OWNED",
        KERN_LOCK_OWNED_SELF => "KERN_LOCK_OWNED_SELF",
        KERN_LOCK_SET_DESTROYED => "KERN_LOCK_SET_DESTROYED",
        KERN_LOCK_UNSTABLE => "KERN_LOCK_UNSTABLE",
        KERN_MEMORY_DATA_MOVED => "KERN_MEMORY_DATA_MOVED",
        KERN_MEMORY_ERROR => "KERN_MEMORY_ERROR",
        KERN_MEMORY_FAILURE => "KERN_MEMORY_FAILURE",
        KERN_MEMORY_PRESENT => "KERN_MEMORY_PRESENT",
        KERN_MEMORY_RESTART_COPY => "KERN_MEMORY_RESTART_COPY",
        KERN_NAME_EXISTS => "KERN_NAME_EXISTS",
        KERN_NODE_DOWN => "KERN_NODE_DOWN",
        KERN_NOT_DEPRESSED => "KERN_NOT_DEPRESSED",
        KERN_NOT_IN_SET => "KERN_NOT_IN_SET",
        KERN_NOT_RECEIVER => "KERN_NOT_RECEIVER",
        KERN_NOT_SUPPORTED => "KERN_NOT_SUPPORTED",
        KERN_NOT_WAITING => "KERN_NOT_WAITING",
        KERN_NO_ACCESS => "KERN_NO_ACCESS",
        KERN_NO_SPACE => "KERN_NO_SPACE",
        KERN_OPERATION_TIMED_OUT => "KERN_OPERATION_TIMED_OUT",
        KERN_POLICY_LIMIT => "KERN_POLICY_LIMIT",
        KERN_POLICY_STATIC => "KERN_POLICY_STATIC",
        KERN_PROTECTION_FAILURE => "KERN_PROTECTION_FAILURE",
        KERN_RESOURCE_SHORTAGE => "KERN_RESOURCE_SHORTAGE",
        KERN_RETURN_MAX => "KERN_RETURN_MAX",
        KERN_RIGHT_EXISTS => "KERN_RIGHT_EXISTS",
        KERN_RPC_CONTINUE_ORPHAN => "KERN_RPC_CONTINUE_ORPHAN",
        KERN_RPC_SERVER_TERMINATED => "KERN_RPC_SERVER_TERMINATED",
        KERN_RPC_TERMINATE_ORPHAN => "KERN_RPC_TERMINATE_ORPHAN",
        KERN_SEMAPHORE_DESTROYED => "KERN_SEMAPHORE_DESTROYED",
        KERN_SUCCESS => "KERN_SUCCESS",
        KERN_TERMINATED => "KERN_TERMINATED",
        KERN_UREFS_OVERFLOW => "KERN_UREFS_OVERFLOW",
        v => {
            eprintln!("unknown kernel error: {}", v);
            "UNKNOWN_KERN_ERROR"
        }
    }
}
